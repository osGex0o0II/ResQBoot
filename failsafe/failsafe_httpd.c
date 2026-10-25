#include <common.h>
#include <net.h>
#include "../net/httpd.h"
#include <malloc.h>
#include <asm/byteorder.h>
#ifdef CONFIG_CMD_NAND
#include <nand.h>
#endif
#include <ipq_api.h>
#include <asm/io.h>
#include <spi_flash.h>
#include <asm-generic/global_data.h>
#include <asm/arch-qca-common/smem.h>
#include <asm/arch-qca-common/qca_common.h>
#include "../board/qca/arm/common/macaddr_modify.h"
#ifdef CONFIG_IPQ40XX
#include "../board/qca/arm/common/fdt_info.h"
#endif
#if defined(CONFIG_IPQ40XX) || defined(CONFIG_IPQ6018) \
	|| defined(CONFIG_IPQ5018) || defined(CONFIG_IPQ5332) \
	|| defined(CONFIG_IPQ9574) || defined(CONFIG_IPQ806X)
#include <asm/arch-qca-common/qpic_nand.h>
#endif
#include <command.h>
#include <webterm.h>
#include <version.h>
#include <fdtdec.h>
#if defined(CONFIG_IPQ40XX) || defined(CONFIG_IPQ806X)
#include <miiphy.h>
#include <linux/mdio.h>
#endif
#ifdef CONFIG_QCA_MMC
#include <mmc.h>
#include <sdhci.h>
#include <part.h>
#ifndef CONFIG_SDHCI_SUPPORT
extern qca_mmc mmc_host;
#else
extern struct sdhci_host mmc_host;
#endif
extern void emmc_cache_invalidate(void);
#endif
extern unsigned int get_spi_flash_size(void);
extern struct spi_flash *spi_flash_ptr[MAX_SF_BUS_NUM][MAX_SF_CS_NUM];
extern int flashread_partition_chunk(const char *part_name, ulong addr, u64 user_offset, ulong user_size, int raw, int flash_type, ulong *out_offset, ulong *out_size, char *out_detail);
extern void (*flashread_yield_fn)(void);
#ifdef CONFIG_DHCPD
#include "../net/dhcpd.h"
#endif

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "netif/ethernet.h"
#include "lwip/ip4_addr.h"

#include "failsafe_httpd.h"
#include "failsafe_httpd_types.h"
#include "ethernetif.h"
#include "fs_wrapper.h"

DECLARE_GLOBAL_DATA_PTR;

#define WEBFAILSAFE_UPLOAD_CDT_MIN_SIZE_IN_BYTES 184

#define ISO_space	0x20
#define ISO_nl		0x0a
#define ISO_cr		0x0d
#define ISO_tab		0x09

#define is_digit(c) ((c) >= '0' && (c) <= '9')
#define is_http_whitespace(c) ((c) == ISO_space || (c) == ISO_cr || (c) == ISO_nl || (c) == ISO_tab)
#define is_http_method_separator(c) ((c) == ISO_space || (c) == ISO_tab)

#define WEBFAILSAFE_PROGRESS_START			0
#define WEBFAILSAFE_PROGRESS_UPLOADING		1
#define WEBFAILSAFE_PROGRESS_UPLOAD_READY	2
#define WEBFAILSAFE_PROGRESS_UPGRADING		3
#define WEBFAILSAFE_PROGRESS_UPGRADE_READY	4
#define WEBFAILSAFE_PROGRESS_UPGRADE_FAILED	5

#define PART_JSON_BUF_SIZE 4096
#define GPT_PRIMARY_SIZE 34
#define GPT_BACKUP_SIZE 33

static char *boundary_value;
static struct {
	u32_t ram_end;
	int data_start_found;
	u8_t packet_counter;
	u32_t led_counter;
	u32_t start_time;
	int file_too_big;
	int failed;
	int done;
} upload = { .packet_counter = 255 };

static struct {
	u32_t data_size;
	u32_t data_addr;
	int sending_header;
	u64 total_remaining;
	u64 total_sent;
	u64 total_size;
	u64 chunk_offset;
	int chunked;
	int chunk_busy;
	int chunk_num;
	int total_chunks;
	int raw;
	int flash_type;
	char part_name[64];
} backup;

int upgrade_status = 0;
static char part_json_buf[PART_JSON_BUF_SIZE];
static struct failsafe_httpd_state *hs_global;
static int upload_pending;
static struct failsafe_httpd_state *upload_hs;

static void httpd_poll_wait(int count);
static int mac_fmt(char *buf, const uchar *m, int first);

static void flashread_yield(void) {
	/*
	 * This runs inside a lwIP tcp_recv callback. Calling eth_rx() here
	 * re-enters lwIP and can free the connection state under us.
	 * Only yield the CPU/watchdog; receive processing resumes when the
	 * callback returns to failsafe_httpd_poll().
	 */
	mdelay(1);
}

#define mib_int(b) ((b) / (1024 * 1024))
#define mib_frac(b) (((b) % (1024 * 1024)) * 100 / (1024 * 1024))

static void print_error(const char *msg) {
	printf("\n## Error: %s\n", msg);
}

static void print_file_size_error(u64 max_size) {
	printf("## Error: size too large, max size <= %llu bytes\n", max_size);
}

static void httpd_upload_progress(struct failsafe_httpd_state *hs) {
	enum { bar_width = 25 };
	u32_t data_written, elapsed, speed, percent, filled, i;
	char bar[bar_width + 1];

	if (hs->upload_total == 0)
		return;

	data_written = (u32_t)(webfailsafe_data_pointer - (u8_t *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS);
	percent = (u32_t)((u64_t)data_written * 100 / hs->upload_total);
	if (percent > 100)
		percent = 100;

	if (percent / 25 != upload.packet_counter / 25) {
		filled = (percent * bar_width) / 100;
		for (i = 0; i < bar_width; i++)
			bar[i] = (i < filled) ? '#' : '.';
		bar[bar_width] = '\0';
		elapsed = (u32_t)get_timer(upload.start_time);
		speed = (elapsed > 0) ? (u32_t)((u64_t)data_written * 1000 / elapsed) : 0;
		printf("\rUploading: [%s] %3u%% %u.%02u MiB/s", bar, percent, (u32)mib_int(speed), (u32)mib_frac(speed));
		upload.packet_counter = (u8_t)percent;
	}

	upload.led_counter++;
	if (upload.led_counter >= 10000) {
		upload.led_counter = 0;
		do_http_progress(WEBFAILSAFE_PROGRESS_UPLOADING);
	}
}

static void httpd_state_reset(struct failsafe_httpd_state *hs) {
	hs->state = STATE_NONE;
	hs->last_activity = (u32_t)get_timer(0);
	hs->dataptr = 0;
	hs->upload = 0;
	hs->upload_total = 0;
	if (hs->owns_global) {
		int done = upload.done, failed = upload.failed;
		hs->owns_global = 0;
		hs_global = NULL;
		memset(&upload, 0, sizeof(upload));
		upload.done = done;
		upload.failed = failed;
		upload.packet_counter = 255;
		memset(&backup, 0, sizeof(backup));
		flashread_yield_fn = NULL;
#ifdef CONFIG_QCA_MMC
		emmc_cache_invalidate();
#endif
		led_on("blink_led");
		if (boundary_value) {
			free(boundary_value);
			boundary_value = NULL;
		}
	}
}

static void httpd_reset_upload_state(void) {
	memset(&upload, 0, sizeof(upload));
	upload.packet_counter = 255;
	upload_pending = 0;
	upload_hs = NULL;
}

static void httpd_discard_pending_upload(void) {
	upload_pending = 0;
	upload.done = 0;
	upload.failed = 0;
	upload_hs = NULL;
	upgrade_status = 0;
}

static void httpd_detach_pcb(struct tcp_pcb *pcb) {
	tcp_arg(pcb, NULL);
	tcp_err(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_poll(pcb, NULL, 0);
}

static void httpd_close_conn(struct failsafe_httpd_state *hs, struct tcp_pcb *pcb) {
	if (pcb) {
		tcp_setprio(pcb, TCP_PRIO_MIN);
		httpd_detach_pcb(pcb);
	}
	httpd_state_reset(hs);
	if (pcb)
		tcp_close(pcb);
	free(hs);
}

static void httpd_conn_abort(struct failsafe_httpd_state *hs, struct tcp_pcb *pcb) {
	httpd_detach_pcb(pcb);
	httpd_state_reset(hs);
	free(hs);
	tcp_abort(pcb);
}

static err_t httpd_recv_abort(struct failsafe_httpd_state *hs, struct tcp_pcb *pcb, char *data, int need_free, struct pbuf *p) {
	httpd_conn_abort(hs, pcb);
	if (need_free) free(data);
	pbuf_free(p);
	return ERR_ABRT;
}

typedef u64 (*get_max_size_fn)(void);

static const struct { const char *name; int type; const char *label; get_max_size_fn get_max_size; } upload_types[] = {
	{"name=\"firmware\"",	WEBFAILSAFE_UPGRADE_TYPE_FIRMWARE,	"FIRMWARE",	get_firmware_upgrade_max_size},
	{"name=\"uboot\"",		WEBFAILSAFE_UPGRADE_TYPE_UBOOT,		"U-Boot",	get_uboot_size},
	{"name=\"art\"",		WEBFAILSAFE_UPGRADE_TYPE_ART,		"ART",		get_art_size},
	{"name=\"img\"",		WEBFAILSAFE_UPGRADE_TYPE_IMG,		"IMG",		NULL},
	{"name=\"cdt\"",		WEBFAILSAFE_UPGRADE_TYPE_CDT,		"CDT",		get_cdt_size},
	{"name=\"mibib\"",		WEBFAILSAFE_UPGRADE_TYPE_MIBIB,		"MIBIB",	get_mibib_size},
	{"name=\"ptable\"",		WEBFAILSAFE_UPGRADE_TYPE_PTABLE,	"PTABLE",	NULL},
	{"name=\"initramfs\"",	WEBFAILSAFE_UPGRADE_TYPE_INITRAMFS,	"INITRAMFS",get_initramfs_max_size},
};

static int httpd_findandstore_firstchunk(struct failsafe_httpd_state *hs, char *data, int data_len) {
	char *start = NULL, *end = NULL;
	u32_t i;

	if (!boundary_value)
		return 0;

	start = strstr(data, boundary_value);
	if (!start)
		return 0;

	for (i = 0; i < ARRAY_SIZE(upload_types); i++) {
		if (strstr(start, upload_types[i].name)) {
			printf("Upgrade type: %s\n", upload_types[i].label);
			webfailsafe_upgrade_type = upload_types[i].type;
			if (upload_types[i].type == WEBFAILSAFE_UPGRADE_TYPE_IMG) {
				webfailsafe_img_flash = strstr(start, "img_nand_raw") ? IMG_FLASH_NAND_RAW :
					strstr(start, "img_nand") ? IMG_FLASH_NAND :
					strstr(start, "img_emmc") ? IMG_FLASH_EMMC :
					strstr(start, "img_nor") ? IMG_FLASH_NOR : 0;
			}
			webfailsafe_backup_avail_enabled = strstr(start, "flash_backup") ? 1 : 0;
			if (strstr(start, "auto_reboot")) {
				char *ar_key = strstr(start, "auto_reboot");
				char *ar_body = strstr(ar_key, "\r\n\r\n");
				if (ar_body && ar_body >= ar_key && ar_body < ar_key + 100) {
					webfailsafe_auto_reboot_enabled = (*(ar_body + 4) == '0') ? 0 : 1;
				} else {
					webfailsafe_auto_reboot_enabled = 1;
				}
			} else {
				webfailsafe_auto_reboot_enabled = 1;
			}
			break;
		}
	}

	if (i == ARRAY_SIZE(upload_types)) {
		print_error("input name not found!");
		return 0;
	}

	end = strstr(strstr(start, upload_types[i].name), "\r\n\r\n");
	if (!end) {
		print_error("couldn't find start of data!");
		return 0;
	}

	if ((end - data) >= data_len)
		return 0;

	end += 4;
	{
		u64 overhead = (u64)(end - start) + strlen(boundary_value) + 6;

		if (overhead > hs->upload_total) {
			print_error("invalid Content-Length");
			upload.failed = 1;
			return 0;
		}
		hs->upload_total -= (u32_t)overhead;
	}
	printf("Upload size: %u.%02u MiB [%u bytes | 0x%x]\n", (u32)mib_int(hs->upload_total), (u32)mib_frac(hs->upload_total), hs->upload_total, hs->upload_total);

	if (upload_types[i].get_max_size) {
		u64 max_size = upload_types[i].get_max_size();
		if ((u64)hs->upload_total > max_size) {
			print_file_size_error(max_size);
			upload.failed = 1;
			upload.file_too_big = 1;
		}
	}

	hs->upload = (u32_t)(data_len - (end - data));
	if (upload.file_too_big)
		return 1;

	{
		/*
		 * hs->upload counts everything received (data + trailing
		 * boundary) for completion detection, but only the declared
		 * data length may be copied into the upload buffer. Without
		 * this cap a first packet larger than upload_total makes the
		 * next httpd_handle_upload_data() subtraction underflow.
		 */
		u32_t first_data = hs->upload;

		if (first_data > hs->upload_total)
			first_data = hs->upload_total;

		if (webfailsafe_data_pointer + first_data > (u8_t *)upload.ram_end) {
			print_error("data larger than available RAM space!");
			upload.failed = 1;
			upload.file_too_big = 1;
			return 1;
		}

		memcpy((void *)webfailsafe_data_pointer, (void *)end, first_data);
		webfailsafe_data_pointer += first_data;
	}
	upload.start_time = (u32_t)get_timer(0);
	httpd_upload_progress(hs);
	return 1;
}

static int httpd_parse_content_length(struct failsafe_httpd_state *hs, char *data) {
	char *start = strstr(data, "Content-Length:"), *end;
	if (start) {
		u64 content_length;
		char *num_end;

		start += strlen("Content-Length:");
		while (*start == ' ' || *start == '\t')
			start++;
		end = strstr(start, "\r\n");
		if (end) {
			content_length = simple_strtoull(start, &num_end, 10);
			while (*num_end == ' ' || *num_end == '\t')
				num_end++;
			if (num_end == start ||
			    (*num_end != '\r' && *num_end != '\n') ||
			    content_length == 0 ||
			    content_length > 0xFFFFFFFFULL) {
				print_error("invalid Content-Length");
				return -1;
			}
			hs->upload_total = (u32_t)content_length;
			return 0;
		}
	}
	print_error("couldn't find \"Content-Length\"!");
	return -1;
}

static int httpd_parse_boundary(char *data) {
	char *start = strstr(data, "boundary="), *end;
	if (start) {
		start += 9;
		end = strstr(start, "\r\n");
		if (end) {
			boundary_value = malloc(end - start + 3);
			if (boundary_value) {
				memcpy(&boundary_value[2], start, end - start);
				boundary_value[0] = '-';
				boundary_value[1] = '-';
				boundary_value[end - start + 2] = 0;
				return 0;
			}
			print_error("couldn't allocate memory for boundary!");
			return -1;
		}
	}
	print_error("couldn't find boundary!");
	return -1;
}

static int httpd_init_upload_ram(void) {
	u32_t memset_len;
	webfailsafe_data_pointer = (u8_t *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;
	upload.ram_end = (u32_t)CONFIG_SYS_SDRAM_END;
	if (!webfailsafe_data_pointer) {
		print_error("couldn't allocate RAM for data!");
		return -1;
	}
	printf("Upload RAM address: 0x%x\n", (u32_t)WEBFAILSAFE_UPLOAD_RAM_ADDRESS);
	printf("Available RAM space: %u.%02u MiB\n",
		(u32)mib_int(upload.ram_end - (u32_t)webfailsafe_data_pointer),
		(u32)mib_frac(upload.ram_end - (u32_t)webfailsafe_data_pointer));
	memset_len = WEBFAILSAFE_UPLOAD_UBOOT_SIZE_IN_BYTES;
	if (webfailsafe_data_pointer + memset_len > (u8_t *)upload.ram_end)
		memset_len = upload.ram_end - (u32_t)webfailsafe_data_pointer;
	if (memset_len > 0)
		memset((void *)webfailsafe_data_pointer, 0xFF, memset_len);
	return 0;
}

static int httpd_check_upload_size(struct failsafe_httpd_state *hs) {
	if (hs->upload_total < 10240 && webfailsafe_upgrade_type != WEBFAILSAFE_UPGRADE_TYPE_CDT) {
		print_error("request for upload < 10 KB data!");
		return -1;
	}
	if (webfailsafe_upgrade_type == WEBFAILSAFE_UPGRADE_TYPE_CDT &&
		hs->upload_total < WEBFAILSAFE_UPLOAD_CDT_MIN_SIZE_IN_BYTES) {
		printf("## Error: CDT data too small, minimum %d bytes!\n", WEBFAILSAFE_UPLOAD_CDT_MIN_SIZE_IN_BYTES);
		return -1;
	}
	return 0;
}

static void httpd_upload_complete(struct failsafe_httpd_state *hs) {
	if (upload.failed) {
		printf("\nfailed!\n");
	} else {
		printf("  Done!\n");
	}
	led_on("blink_led");
	upload.done = 1;
	if (!upload.failed) {
		upload_pending = 1;
		upload_hs = hs;
	}
	upgrade_status = 1;
	net_boot_file_size = (ulong)hs->upload_total;
}

static int httpd_check_upload_complete(struct failsafe_httpd_state *hs) {
	if ((u64)hs->upload >= (u64)hs->upload_total + strlen(boundary_value) + 6) {
		httpd_upload_complete(hs);
		static const char resp_ok[] = "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n";
		static const char resp_err[] = "HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\n";
		httpd_state_reset(hs);
		hs->state = STATE_FILE_REQUEST;
		if (!upload.failed) {
			hs->dataptr = (u8_t *)resp_ok;
			hs->upload = sizeof(resp_ok) - 1;
		} else {
			hs->dataptr = (u8_t *)resp_err;
			hs->upload = sizeof(resp_err) - 1;
		}
		httpd_send_data(hs);
		return 1;
	}
	return 0;
}

static void httpd_handle_upload_data(struct failsafe_httpd_state *hs, char *data, int data_len) {
	u32_t bytes_to_write = (u32_t)data_len, data_written = (u32_t)(webfailsafe_data_pointer - (u8_t *)WEBFAILSAFE_UPLOAD_RAM_ADDRESS);

	if (data_written > hs->upload_total) {
		/* Never let the subtraction below wrap. */
		upload.failed = 1;
		return;
	}
	if (data_written == hs->upload_total) {
		/* Remaining bytes are the multipart trailer, not image data. */
		httpd_upload_progress(hs);
		return;
	}

	if ((u64_t)data_written + bytes_to_write > hs->upload_total)
		bytes_to_write = hs->upload_total - data_written;

	if (bytes_to_write > 0 && webfailsafe_data_pointer + bytes_to_write > (u8_t *)upload.ram_end) {
		print_error("data larger than available RAM space!");
		upload.failed = 1;
		upload.file_too_big = 1;
	} else if (bytes_to_write > 0) {
		memcpy((void *)webfailsafe_data_pointer, (void *)data, bytes_to_write);
		webfailsafe_data_pointer += bytes_to_write;
	}
	httpd_upload_progress(hs);
}

static void str_trim_crlf(char *s) {
	char *p;
	if ((p = strchr(s, ' ')))  *p = '\0';
	if ((p = strchr(s, '\r')))  *p = '\0';
	if ((p = strchr(s, '\n')))  *p = '\0';
}

static int hexval(char c) {
	return (c >= '0' && c <= '9') ? c - '0' :
		(c >= 'a' && c <= 'f') ? c - 'a' + 10 :
		(c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0;
}

static void url_decode(char *s) {
	char *src = s, *dst = s;
	while (*src) {
		if (*src == '%' && src[1] && src[2]) {
			*dst++ = (char)(hexval(src[1]) * 16 + hexval(src[2]));
			src += 3;
		} else if (*src == '+') {
			*dst++ = ' ';
			src++;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
}

/*
 * Extract an application/x-www-form-urlencoded field.  Matching only at the
 * start of a field avoids treating e.g. "xiface=" as "iface=".  The value is
 * URL-decoded before it is returned.
 */
static int mac_string_valid(const char *s) {
	int i;

	if (!s || strlen(s) != 17)
		return 0;
	for (i = 0; i < 17; i++) {
		if (i % 3 == 2) {
			if (s[i] != ':')
				return 0;
		} else if (!((s[i] >= '0' && s[i] <= '9') ||
			     (s[i] >= 'a' && s[i] <= 'f') ||
			     (s[i] >= 'A' && s[i] <= 'F'))) {
			return 0;
		}
	}
	return s[17] == '\0';
}

static int httpd_form_value(const char *body, const char *key, char *out,
			    size_t out_size) {
	size_t klen;
	const char *p;

	if (!body || !key || !out || !out_size)
		return -1;

	klen = strlen(key);
	for (p = body; *p; ) {
		const char *amp = strchr(p, '&');
		size_t len = amp ? (size_t)(amp - p) : strlen(p);

		if (len > klen && !strncmp(p, key, klen) && p[klen] == '=') {
			size_t vlen = len - klen - 1;

			if (vlen >= out_size)
				return -1;
			memcpy(out, p + klen + 1, vlen);
			out[vlen] = '\0';
			url_decode(out);
			return 0;
		}
		p = amp ? amp + 1 : NULL;
	}
	return -1;
}

static void httpd_send_static(struct failsafe_httpd_state *hs, const char *data, int len) {
	hs->state = STATE_FILE_REQUEST;
	hs->dataptr = (u8_t *)data;
	hs->upload = len;
	httpd_send_data(hs);
}

static void httpd_handle_upgrade_status(struct failsafe_httpd_state *hs) {
	static const char *status_text[] = {"idle", "verifying", "flashing", "type_mismatch", "rebooting", "upgrade_done"};
	static char resp[128];
	int len = sprintf(resp, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s", status_text[upgrade_status]);
	httpd_send_static(hs, resp, len);
}

#define ABOUT_BUF_SIZE 4096
static char about_json_buf[ABOUT_BUF_SIZE];

struct phy_id_name {
	u32 id;
	const char *name;
};

static const struct phy_id_name phy_c22_qca[] = {
	{ 0x004DD0B0, "QCA8075 V1.0 5P" },
	{ 0x004DD0B1, "QCA8075 V1.1 5P" },
	{ 0x004DD0B2, "QCA8075 V1.1 2P" },
	{ 0x004DD036, "QCA8337"         },
	{ 0x004DD074, "QCA8033"         },
};

#if defined(CONFIG_IPQ6018) || defined(CONFIG_IPQ807X) || defined(CONFIG_IPQ9574) || defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ5018)
static const struct phy_id_name phy_c22_ext[] = {
	{ 0x004DD0C0, "GEPHY"        },
	{ 0x004DD100, "QCA8081 V1.0" },
	{ 0x004DD101, "QCA8081 V1.1" },
	{ 0x001CC849, "RTL8221"      },
	{ 0x004DD180, "QCA8084"      },
	{ 0x00837270, "RTL8372N"     },
};

static const struct phy_id_name phy_c45_aq[] = {
	{ 0x03a1b4e2, "AQR107"        },
	{ 0x03a1b502, "AQR109"        },
	{ 0x03a1b610, "AQR111"        },
	{ 0x03a1b612, "AQR111B0"      },
	{ 0x03a1b660, "AQR112"        },
	{ 0x03a1b792, "AQR112C"       },
	{ 0x31c31C10, "AQR113C A0"    },
	{ 0x31c31C11, "AQR113C A1"    },
	{ 0x31c31C12, "AQR113C B0"    },
	{ 0x31c31C13, "AQR113C B1"    },
	{ 0x31c31DD3, "Marvell X3410" },
};
#endif

static const char *phy_lookup(u32 id, const struct phy_id_name *tbl, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++)
		if (id == tbl[i].id)
			return tbl[i].name;
	return NULL;
}

static int phy_emit(int pos, int *first, const char *name, int addr, u16 id1, u16 id2, int link, int speed)
{
	/*
	 * Leave room for the MAC list, GPIO array and JSON trailer that follow.
	 * Once the buffer is this full, omit further PHY entries instead of
	 * writing past about_json_buf.
	 */
	if (pos < 0 || pos > ABOUT_BUF_SIZE - 256)
		return pos;

	if (!*first)
		pos += sprintf(about_json_buf + pos, ", ");
	if (speed > 0)
		pos += sprintf(about_json_buf + pos,
			"%s@%d (ID %04x:%04x) %dMbps [%s]", name, addr, id1, id2, speed, link ? "UP" : "DOWN");
	else
		pos += sprintf(about_json_buf + pos,
			"%s@%d (ID %04x:%04x) [%s]", name, addr, id1, id2, link ? "UP" : "DOWN");
	*first = 0;
	return pos;
}

#if defined(CONFIG_IPQ6018) || defined(CONFIG_IPQ807X) || defined(CONFIG_IPQ9574) || defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ5018)
typedef int (*mdio_read_fn)(int, int, ushort *);
static int phy_scan_mdio(int pos, int *first, mdio_read_fn mdio_read) {
	int phy_addr;
	for (phy_addr = 0; phy_addr <= 31; phy_addr++) {
		int ret;
		u16 id1, id2;
		u32 phy_id;
		const char *name;
		ret = mdio_read(phy_addr, 2, NULL);
		if (ret < 0)
			continue;
		id1 = (u16)ret;
		ret = mdio_read(phy_addr, 3, NULL);
		if (ret < 0)
			continue;
		id2 = (u16)ret;
		phy_id = ((u32)id1 << 16) | id2;
		if (!phy_id || phy_id == 0xFFFFFFFF)
			continue;
		if (phy_id == 0x001CC849 && phy_addr == 0)
			continue;
		name = phy_lookup(phy_id, phy_c22_qca,
			ARRAY_SIZE(phy_c22_qca));
		int is_ext = 0;
		if (!name) {
			name = phy_lookup(phy_id, phy_c22_ext,
				ARRAY_SIZE(phy_c22_ext));
			is_ext = 1;
		}
		if (name) {
			u16 bmsr = (u16)mdio_read(phy_addr, 1, NULL);
			int link = (bmsr >> 2) & 1;
			int spd = 0;
			if (link) {
				if (phy_id == 0x001CC849) {
					u16 s = (u16)mdio_read(phy_addr,
						(1 << 30) | (31 << 16) | 0xA434, NULL);
					int i = ((s >> 9) & 3) * 4 + ((s >> 4) & 3);
					spd = i == 0 ? 10 : i == 1 ? 100 : i == 2 ? 1000 : i == 5 ? 2500 : i == 7 ? 1000 : 0;
				} else {
					u16 ssr = (u16)mdio_read(phy_addr, 17, NULL);
					if (is_ext)
						spd = (ssr & 0x800) ? 5000 : (ssr & 0x400) ? 10000 : (ssr & 0x200) ? 2500 : (ssr & 0x100) ? 1000 : (ssr & 0x080) ? 100 : 10;
					else {
						int i = (ssr >> 14) & 3;
						spd = i == 2 ? 1000 : i == 1 ? 100 : 10;
					}
				}
			}
			pos = phy_emit(pos, first, name, phy_addr, id1, id2, link, spd);
			continue;
		}
		{
			u16 c45_id1, c45_id2;
			u32 c45_phy_id;
			c45_id1 = mdio_read(phy_addr,
				(1 << 30) | (1 << 16) | 2, NULL);
			c45_id2 = mdio_read(phy_addr,
				(1 << 30) | (1 << 16) | 3, NULL);
			c45_phy_id = ((u32)c45_id1 << 16) | c45_id2;
			if (!c45_phy_id || c45_phy_id == 0xFFFFFFFF)
				continue;
			name = phy_lookup(c45_phy_id, phy_c45_aq,
				ARRAY_SIZE(phy_c45_aq));
			if (name) {
				u16 c45_bmsr = (u16)mdio_read(phy_addr,
					(1 << 30) | (1 << 16) | 1, NULL);
				int link = (c45_bmsr >> 2) & 1;
				int spd = 0;
				if (link) {
					u16 c45_spd = (u16)mdio_read(phy_addr,
						(1 << 30) | (0x1e << 16) | 0xc800, NULL);
					int i = (c45_spd >> 1) & 7;
					spd = i == 3 ? 10000 : i == 5 ? 5000 : i == 4 ? 2500 : i == 2 ? 1000 : i == 1 ? 100 : 10;
				}
				pos = phy_emit(pos, first, name, phy_addr, c45_id1, c45_id2, link, spd);
			}
		}
	}
	return pos;
}
#endif

#if defined(CONFIG_IPQ40XX) || defined(CONFIG_IPQ806X)
static int phy_scan_mii_bus(int pos, int *first, struct mii_dev *bus, int max_addr) {
	int phy_addr;
	for (phy_addr = 0; phy_addr <= max_addr; phy_addr++) {
		int ret;
		u16 id1, id2;
		u32 phy_id;
		const char *name;
		ret = bus->read(bus, phy_addr, MDIO_DEVAD_NONE, 2);
		if (ret < 0)
			continue;
		id1 = (u16)ret;
		ret = bus->read(bus, phy_addr, MDIO_DEVAD_NONE, 3);
		if (ret < 0)
			continue;
		id2 = (u16)ret;
		phy_id = ((u32)id1 << 16) | id2;
		if (!phy_id || phy_id == 0xFFFFFFFF)
			continue;
		name = phy_lookup(phy_id, phy_c22_qca, ARRAY_SIZE(phy_c22_qca));
		if (name) {
			u16 bmsr = (u16)bus->read(bus, phy_addr, MDIO_DEVAD_NONE, 1);
			int link = (bmsr >> 2) & 1;
			int spd = 0;
			if (link) {
				u16 ssr = (u16)bus->read(bus, phy_addr, MDIO_DEVAD_NONE, 17);
				int i = (ssr >> 14) & 3;
				spd = i == 2 ? 1000 : i == 1 ? 100 : 10;
			}
			pos = phy_emit(pos, first, name, phy_addr, id1, id2, link, spd);
		}
	}
	return pos;
}
#endif

struct about_gpio_ctx {
	char *buf;
	int *pos;
	int *first;
	int env_rk_gpio;
};

static const char *fdt_find_alias_for_path(const char *path) {
	int aliases = fdt_path_offset(gd->fdt_blob, "/aliases");
	if (aliases < 0)
		return NULL;
	int prop_len;
	const struct fdt_property *prop;
	for (int offset = fdt_first_property_offset(gd->fdt_blob, aliases);
		 offset >= 0;
		 offset = fdt_next_property_offset(gd->fdt_blob, offset)) {
		prop = fdt_get_property_by_offset(gd->fdt_blob, offset, &prop_len);
		if (!prop)
			break;
		if (strcmp(prop->data, path) == 0)
			return fdt_string(gd->fdt_blob, fdt32_to_cpu(prop->nameoff));
	}
	return NULL;
}

static void about_gpio_cb(int gpio, const char *name, const char *dir, int value, const char *parent, void *ctx) {
	struct about_gpio_ctx *c = ctx;
	const char *src = getenv(name) ? "env" : "fdt";
	const char *override = "";
	if (c->env_rk_gpio >= 0 && strcmp(parent, "key_gpio") == 0 && src[0] == 'f')
		override = "reset_key";
	char path[80];
	int remaining;
	int n;

	snprintf(path, sizeof(path), "/tlmm-gpio/%s/%s", parent, name);
	const char *alias = fdt_find_alias_for_path(path);
	remaining = ABOUT_BUF_SIZE - *c->pos;
	if (remaining < 256)
		return;
	n = snprintf(c->buf + *c->pos, remaining,
		"%s{\"gpio\":%d,\"name\":\"%s\",\"alias\":\"%s\",\"dir\":\"%s\",\"value\":%d,\"parent\":\"%s\",\"source\":\"%s\",\"override\":\"%s\"}",
		*c->first ? "" : ",", gpio, name, alias ? alias : "", dir, value, parent, src, override);
	if (n < 0 || n >= remaining)
		return;
	*c->pos += n;
	*c->first = 0;
}

static int json_escape(char *dst, size_t dstsz, const char *src, size_t maxlen)
{
	size_t o = 0;

	if (!dst || dstsz == 0)
		return -1;

	while (src && *src && maxlen) {
		unsigned char c = (unsigned char)*src++;
		char tmp[8];
		const char *rep = tmp;
		size_t l;

		maxlen--;
		if (c == '"' || c == '\\') {
			tmp[0] = '\\';
			tmp[1] = c;
			tmp[2] = '\0';
		} else if (c == '\n') {
			strcpy(tmp, "\\n");
		} else if (c == '\r') {
			strcpy(tmp, "\\r");
		} else if (c == '\t') {
			strcpy(tmp, "\\t");
		} else if (c < 0x20) {
			snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned int)c);
		} else {
			tmp[0] = c;
			tmp[1] = '\0';
		}
		l = strlen(rep);
		if (o + l >= dstsz) {
			dst[o] = '\0';
			return -1;
		}
		memcpy(dst + o, rep, l);
		o += l;
	}
	dst[o] = '\0';
	return (int)o;
}

static void httpd_handle_about(struct failsafe_httpd_state *hs) {
	int pos = 0, hdr_len;
	char hdr[128];
	pos += sprintf(about_json_buf + pos, "{\"version\":\"%s\",", version_string);
	pos += sprintf(about_json_buf + pos, "\"machid\":\"0x%lx\",", gd->bd->bi_arch_number);
	pos += sprintf(about_json_buf + pos, "\"ram_size\":%lu,", (ulong)gd->ram_size);

	{
		const char *fdt_model = fdt_getprop(gd->fdt_blob, 0, "model", NULL);
		char model_esc[384];

		if (json_escape(model_esc, sizeof(model_esc),
				fdt_model ? fdt_model : "", 128) < 0)
			model_esc[0] = '\0';
#ifdef CONFIG_BOARD_DISPLAY_NAME
		pos += sprintf(about_json_buf + pos, "\"model\":\"%s%s%s%s\",", CONFIG_BOARD_DISPLAY_NAME, fdt_model ? " (" : "", model_esc, fdt_model ? ")" : "");
#else
		pos += sprintf(about_json_buf + pos, "\"model\":\"%s\",", model_esc);
#endif
	}

	{
		const char *cn = fdt_getprop(gd->fdt_blob, 0, "config_name", NULL);
		const char *cn_env = getenv("config_name");
		if (cn_env) {
			char cn_esc[384];

			if (json_escape(cn_esc, sizeof(cn_esc), cn_env, 128) < 0)
				cn_esc[0] = '\0';
			pos += sprintf(about_json_buf + pos, "\"config_name\":\"%s\"," "\"config_name_source\":\"env\",", cn_esc);
		} else if (cn) {
			int cnt = fdt_count_strings(gd->fdt_blob, 0, "config_name");
			pos += sprintf(about_json_buf + pos, "\"config_name\":\"");
			int first = 1;
			for (int i = 0; i < cnt && i < 8; i++) {
				const char *s;
				if (fdt_get_string_index(gd->fdt_blob, 0, "config_name", i, &s) == 0 && s && strchr(s, '@')) {
					char s_esc[192];

					if (json_escape(s_esc, sizeof(s_esc), s, 64) < 0)
						continue;
					if (!first) pos += sprintf(about_json_buf + pos, ", ");
					pos += sprintf(about_json_buf + pos, "%s", s_esc);
					first = 0;
				}
			}
			pos += sprintf(about_json_buf + pos, "\",\"config_name_source\":\"fdt\",");
		} else {
			pos += sprintf(about_json_buf + pos, "\"config_name\":\"\"," "\"config_name_source\":\"\",");
		}
	}

	{
		static const char * const flash_type_names[] = {
			[SMEM_BOOT_NO_FLASH]        = "NONE",
			[SMEM_BOOT_NOR_FLASH]       = "NOR",
			[SMEM_BOOT_NAND_FLASH]      = "NAND",
			[SMEM_BOOT_ONENAND_FLASH]   = "ONENAND",
			[SMEM_BOOT_SDC_FLASH]       = "SDC",
			[SMEM_BOOT_MMC_FLASH]       = "EMMC",
			[SMEM_BOOT_SPI_FLASH]       = "SPI",
			[SMEM_BOOT_NORPLUSNAND]     = "NORPLUSNAND",
			[SMEM_BOOT_NORPLUSEMMC]     = "NORPLUSEMMC",
			[SMEM_BOOT_QSPI_NAND_FLASH] = "QSPI-NAND",
		};
		uint32_t ft;
		const char *ft_name = "UNKNOWN";
		if (get_current_flash_type(&ft) == 0 &&
			ft < ARRAY_SIZE(flash_type_names) && flash_type_names[ft])
			ft_name = flash_type_names[ft];
		pos += sprintf(about_json_buf + pos, "\"flash_type\":\"%s\",", ft_name);
	}

	{
		int fc_first = 1;
		pos += sprintf(about_json_buf + pos, "\"flash_chips\":\"");
		{
			int b, c;
			for (b = 0; b < MAX_SF_BUS_NUM; b++)
				for (c = 0; c < MAX_SF_CS_NUM; c++) {
					struct spi_flash *sf = spi_flash_ptr[b][c];
					if (sf && sf->name && sf->size) {
						if (!fc_first)
							pos += sprintf(about_json_buf + pos, ", ");
						pos += sprintf(about_json_buf + pos, "%s %luMiB",
							sf->name, (ulong)(sf->size >> 20));
						if (sf->jedec)
							pos += sprintf(about_json_buf + pos,
								" [ID %02x:%02x:%02x",
								sf->jedec >> 16,
								(sf->jedec >> 8) & 0xFF,
								sf->jedec & 0xFF);
						if (sf->jedec && sf->ext_jedec)
							pos += sprintf(about_json_buf + pos, ":%02x", sf->ext_jedec & 0xFF);
						if (sf->jedec)
							pos += sprintf(about_json_buf + pos, "]");
						fc_first = 0;
					}
				}
		}
#ifdef CONFIG_CMD_NAND
		{
			int i;
			for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++) {
				struct mtd_info *mtd = &nand_info[i];
#ifdef CONFIG_IPQ_SPI_NAND_INFO_IDX
				if (i == CONFIG_IPQ_SPI_NAND_INFO_IDX)
					continue;
#endif
#ifdef CONFIG_IPQ_SPI_NOR_INFO_IDX
				if (i == CONFIG_IPQ_SPI_NOR_INFO_IDX)
					continue;
#endif
				if (mtd->size > 0 && mtd->writesize > 0 && mtd->writesize != 1) {
					struct nand_chip *chip = mtd->priv;
					if (!fc_first)
						pos += sprintf(about_json_buf + pos, ", ");
					pos += sprintf(about_json_buf + pos, "%s %luMiB", mtd->name ? mtd->name : "nand", (ulong)(mtd->size >> 20));
					if (chip && chip->onfi_version) {
						int ml = 20;
						while (ml > 0 && chip->onfi_params.model[ml-1] == ' ')
							ml--;
						if (ml > 0)
							pos += sprintf(about_json_buf + pos, " %.*s", ml, chip->onfi_params.model);
#ifdef CONFIG_QPIC_NAND
						if (chip->priv) {
							struct { unsigned id; } *qd = chip->priv;
							if (qd->id)
								pos += sprintf(about_json_buf + pos, " [ONFI v%d ID %02x:%02x:%02x:%02x]", chip->onfi_version,
									qd->id & 0xff, (qd->id >> 8) & 0xff, (qd->id >> 16) & 0xff, (qd->id >> 24) & 0xff);
						} else
#endif
						pos += sprintf(about_json_buf + pos, " [ONFI v%d ID %02x]", chip->onfi_version, chip->onfi_params.jedec_id);
					}
					else if (chip && chip->jedec_version)
						pos += sprintf(about_json_buf + pos, " [JEDEC v%d ID:%02x:%02x:%02x]", chip->jedec_version,
							chip->jedec_params.jedec_id[0], chip->jedec_params.jedec_id[1], chip->jedec_params.jedec_id[2]);
					else if (chip && chip->cmdfunc && chip->read_byte) {
						u8 mid, did;
						chip->select_chip(mtd, 0);
						chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
						mid = chip->read_byte(mtd);
						did = chip->read_byte(mtd);
						if (mid || did)
							pos += sprintf(about_json_buf + pos, " [ID %02x:%02x]", mid, did);
					}
#ifdef CONFIG_QPIC_NAND
					else if (chip && chip->priv) {
						struct { unsigned id; unsigned type;
							unsigned vendor; unsigned device; } *qd = chip->priv;
						if (qd->id) {
							extern struct nand_manufacturers nand_manuf_ids[];
							extern struct nand_flash_dev nand_flash_ids[];
							struct nand_manufacturers *mfr;
							struct nand_flash_dev *fdev;
							const char *mfr_name = NULL;
							const char *dev_name = NULL;
							u8 vid = qd->id & 0xff;
							u8 did = (qd->id >> 8) & 0xff;
							for (mfr = nand_manuf_ids; mfr->id; mfr++) {
								if (mfr->id == vid) {
									mfr_name = mfr->name;
									break;
								}
							}
							for (fdev = nand_flash_ids; fdev->name; fdev++) {
								if (fdev->id[0] == vid &&
									fdev->id[1] == did &&
									fdev->id_len >= 4) {
									dev_name = fdev->name;
									break;
								}
							}
#ifdef CONFIG_QPIC_SERIAL
							extern struct qpic_serial_nand_params *serial_params;
							if (serial_params && serial_params->name)
								pos += sprintf(about_json_buf + pos, " %s", serial_params->name);
							else
#endif
							if (dev_name)
								pos += sprintf(about_json_buf + pos, " %s", dev_name);
							else if (mfr_name)
								pos += sprintf(about_json_buf + pos, " %s", mfr_name);
							pos += sprintf(about_json_buf + pos, " [ID %02x:%02x:%02x:%02x]", qd->id & 0xff,
								(qd->id >> 8) & 0xff, (qd->id >> 16) & 0xff, (qd->id >> 24) & 0xff);
						}
					}
#endif
#ifdef CONFIG_QPIC_SERIAL
					else {
						extern struct qpic_serial_nand_params *serial_params;
						if (serial_params)
							pos += sprintf(about_json_buf + pos, " [ID %02x:%02x:%02x:%02x]",
								serial_params->id[0], serial_params->id[1], serial_params->id[2], serial_params->id[3]);
					}
#endif
					fc_first = 0;
				}
			}
		}
#endif
#ifdef CONFIG_QCA_MMC
		{
			int i;
			for (i = 0; i < get_mmc_num(); i++) {
				struct mmc *m = find_mmc_device(i);
				if (m && m->has_init && m->capacity > 0) {
					if (!fc_first)
						pos += sprintf(about_json_buf + pos, ", ");
					pos += sprintf(about_json_buf + pos, "eMMC %s%s%s %s %luMiB",
						m->block_dev.product[0] ? m->block_dev.product : "unknown",
						m->block_dev.revision[0] ? " rev" : "",
						m->block_dev.revision[0] ? m->block_dev.revision : "",
						m->block_dev.vendor[0] ? m->block_dev.vendor : "",
						(ulong)(m->capacity >> 20));
					fc_first = 0;
				}
			}
		}
#endif
		pos += sprintf(about_json_buf + pos, "\",");
	}

	{
		int ps_first = 1;
		pos += sprintf(about_json_buf + pos, "\"phy_switch_info\":\"");
#if defined(CONFIG_IPQ40XX)
		{
			struct mii_dev *bus = mdio_get_current_dev();
			if (bus)
				pos = phy_scan_mii_bus(pos, &ps_first, bus, 6);
		}
#elif defined(CONFIG_IPQ6018) || defined(CONFIG_IPQ807X) || defined(CONFIG_IPQ9574) || defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ5018)
		{
#ifdef CONFIG_IPQ5018
			extern int ipq5018_mdio_read(int mii_id, int regnum, ushort *data);
			pos = phy_scan_mdio(pos, &ps_first, ipq5018_mdio_read);
#elif defined(CONFIG_RTL8372N_SWITCH)
			extern int rtl8372n_c22_read(int mdio_addr, int regnum, ushort *data);
			pos = phy_scan_mdio(pos, &ps_first, rtl8372n_c22_read);
#else
			extern int ipq_mdio_read(int mii_id, int regnum, ushort *data);
			pos = phy_scan_mdio(pos, &ps_first, ipq_mdio_read);
#endif
		}
#elif defined(CONFIG_IPQ806X)
		{
			struct mii_dev *bus = mdio_get_current_dev();
			if (bus)
				pos = phy_scan_mii_bus(pos, &ps_first, bus, 4);
		}
#endif
		pos += sprintf(about_json_buf + pos, "\",");
	}

	{
		uchar mb[CONFIG_IPQ_NO_MACS * 6];
		int i, r = get_eth_mac_address(mb, CONFIG_IPQ_NO_MACS);
		pos += sprintf(about_json_buf + pos, "\"mac_addr\":\"");
		for (i = 0; r >= 0 && i < CONFIG_IPQ_NO_MACS; i++)
			pos += mac_fmt(about_json_buf + pos, &mb[i*6], i == 0);
		pos += sprintf(about_json_buf + pos, "\",");
	}

	pos += sprintf(about_json_buf + pos, "\"gpios\":[");
	{
		int rk_gpio = env_reset_gpio();
		struct about_gpio_ctx ctx = { about_json_buf, &pos, &(int){1}, rk_gpio };
		fdt_list_gpio("/tlmm-gpio", "", about_gpio_cb, &ctx);
		if (rk_gpio >= 0) {
			int rk_value = gpio_get_value(rk_gpio);
			about_gpio_cb(rk_gpio, "reset_key", "in", rk_value, "key_gpio", &ctx);
		}
	}
	if (pos > ABOUT_BUF_SIZE - 256) {
		static const char err[] =
			"HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\nabout too large";
		httpd_send_static(hs, err, sizeof(err) - 1);
		return;
	}
	pos += sprintf(about_json_buf + pos, "]}");

	hdr_len = sprintf(hdr, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", pos);
	if (hdr_len + pos > (int)sizeof(about_json_buf)) {
		static const char err[] =
			"HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\nabout too large";
		httpd_send_static(hs, err, sizeof(err) - 1);
		return;
	}
	memmove(about_json_buf + hdr_len, about_json_buf, pos);
	memcpy(about_json_buf, hdr, hdr_len);

	hs->state = STATE_FILE_REQUEST;
	hs->dataptr = (u8_t *)about_json_buf;
	hs->upload = hdr_len + pos;
	httpd_send_data(hs);
}

static void httpd_handle_led(struct failsafe_httpd_state *hs, char *data) {
	char *q = strchr(&data[4], '?'), name[64] = "", action[16] = "";
	static char resp[128];
	int len, alen;
	int bad = 0;

	if (q) {
		char *p = q + 1, *amp, *sp;
		if (strncmp(p, "name=", 5) == 0) {
			const char *start = p + 5;
			size_t nlen;

			p = (char *)start;
			amp = strchr(p, '&');
			sp = strchr(p, ' ');
			if (amp && (!sp || amp < sp)) {
				nlen = (size_t)(amp - p);
				p = amp + 1;
			} else if (sp) {
				nlen = (size_t)(sp - p);
				p = sp;
			} else {
				nlen = strlen(p);
				p += nlen;
			}
			if (nlen >= sizeof(name)) {
				bad = 1;
			} else {
				memcpy(name, start, nlen);
				name[nlen] = '\0';
			}
		}
		if (!bad && strncmp(p, "action=", 7) == 0) {
			p += 7;
			sp = strchr(p, ' ');
			amp = strchr(p, '&');
			char *end = sp;
			if (amp && (!end || amp < end)) end = amp;
			if (end) alen = (int)(end - p);
			else { for (alen = 0; p[alen] && p[alen] != ' ' && p[alen] != '\r' && p[alen] != '\n'; alen++); }
			if (alen > (int)sizeof(action) - 1) alen = sizeof(action) - 1;
			memcpy(action, p, alen);
			action[alen] = '\0';
		}
	}
	if (!bad && name[0] && action[0]) {
		if (strcmp(action, "on") == 0) led_on(name);
		else if (strcmp(action, "off") == 0) led_off(name);
		else if (strcmp(action, "toggle") == 0) led_toggle(name);
	}
	len = sprintf(resp, "HTTP/1.0 %s\r\nCache-Control: no-cache\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s",
		      bad ? "400 Bad Request" : "200 OK", bad ? "bad request" : "ok");
	httpd_send_static(hs, resp, len);
}

static void httpd_handle_btn_detect(struct failsafe_httpd_state *hs) {
	static char resp[4096];
	int pos = 0, hdr_len, len, first = 1, i, resp_size = sizeof(resp) - 256;
	char hdr[128];

	pos += sprintf(resp + pos, "[");
	{
		for (i = 0; i < GPIO_MAX; i++) {
			unsigned int cfg;
			if (pos + 32 > resp_size)
				break;
#if defined(CONFIG_IPQ6018) && defined(CONFIG_QCA_MMC)
			if (i == 20)
				continue;
#endif
			cfg = readl(GPIO_CONFIG_ADDR(i));
			if ((cfg & 0x1C) || (cfg & (1 << 9)))
				continue;
			if (!first)
				pos += sprintf(resp + pos, ", ");
			pos += sprintf(resp + pos, "{\"gpio\":%d,\"value\":%d}", i, gpio_get_value(i));
			first = 0;
		}
	}
	pos += sprintf(resp + pos, "]");

	hdr_len = sprintf(hdr, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", pos);
	len = hdr_len + pos;
	memmove(resp + hdr_len, resp, pos);
	memcpy(resp, hdr, hdr_len);

	httpd_send_static(hs, resp, len);
}

static void httpd_handle_env_set(struct failsafe_httpd_state *hs, char *data, int data_len) {
	static char resp[128];
	int len, ok = 0, bad = 0;
	char *body = strstr(data, "\r\n\r\n");

	if (body) {
		char *amp, *eq;
		char name[64] = "", value[128] = "";
		size_t nlen, vlen;

		body += 4;
		amp = strchr(body, '&');
		eq = strchr(body, '=');
		if (eq && eq < (amp ? amp : body + strlen(body))) {
			nlen = (size_t)(eq - body);
			vlen = amp ? (size_t)(amp - (eq + 1)) : strlen(eq + 1);

			if (nlen >= sizeof(name) || vlen >= sizeof(value)) {
				bad = 1;
			} else {
				memcpy(name, body, nlen);
				name[nlen] = '\0';
				memcpy(value, eq + 1, vlen);
				value[vlen] = '\0';
				/*
				 * The Web UI submits this as application/x-www-form-
				 * urlencoded, so decode before validating/storing.
				 */
				url_decode(name);
				url_decode(value);

				if (strcmp(name, "reset_key") == 0) {
					if (value[0]) {
						unsigned long gpio = 0;
						int i, valid = 1;

						for (i = 0; value[i]; i++) {
							if (value[i] < '0' || value[i] > '9' ||
							    gpio > (GPIO_MAX - 1) / 10) {
								valid = 0;
								break;
							}
							gpio = gpio * 10 + (value[i] - '0');
							if (gpio >= GPIO_MAX) {
								valid = 0;
								break;
							}
						}
						if (!valid) {
							bad = 1;
						} else {
							setenv(name, value);
							saveenv();
							env_reset_gpio_invalidate();
							ok = 1;
						}
					} else {
						setenv(name, NULL);
						saveenv();
						env_reset_gpio_invalidate();
						ok = 1;
					}
				} else if (strcmp(name, "config_name") == 0) {
					int i, valid = 1;

					/* This value is emitted verbatim in /about JSON. */
					for (i = 0; value[i]; i++) {
						if (value[i] == '"' || value[i] == '\\' ||
						    (unsigned char)value[i] < 0x20) {
							valid = 0;
							break;
						}
					}
					if (!valid) {
						bad = 1;
					} else {
						setenv(name, value[0] ? value : NULL);
						saveenv();
						ok = 1;
					}
				}
			}
		} else {
			bad = 1;
		}
	} else {
		bad = 1;
	}

	len = sprintf(resp, "HTTP/1.0 %s\r\nCache-Control: no-cache\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s",
		      bad ? "400 Bad Request" : "200 OK", bad ? "bad request" : (ok ? "ok" : "error"));
	httpd_send_static(hs, resp, len);
}

static int mac_fmt(char *buf, const uchar *m, int first) {
	return sprintf(buf, "%s%02X:%02X:%02X:%02X:%02X:%02X", first ? "" : ", ",
		m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void httpd_handle_mac_info(struct failsafe_httpd_state *hs) {
	static char buf[768];
	char hdr[128];
	int pos = 0, hdr_len, len, i, n, f;
	u32 wifi_valid = 0;
	uchar wifi_macs[MACADDR_WIFI_MAX][6];
	static const char *wf_names[] = {"wifi_off", "wifi_cs", "wifi_cs_off", "wifi_cs_ok"};

	for (i = 0; i < MACADDR_WIFI_MAX; i++)
		if (macaddr_read_wifi(wifi_macs[i], i) >= 0 && is_valid_ethaddr(wifi_macs[i]))
			wifi_valid |= (1 << i);

	pos += sprintf(buf + pos, "{\"mac_addr\":\"");
	{
		uchar mb[CONFIG_IPQ_NO_MACS * 6];
		int r = get_eth_mac_address(mb, CONFIG_IPQ_NO_MACS);
		for (i = 0; r >= 0 && i < CONFIG_IPQ_NO_MACS; i++)
			pos += mac_fmt(buf + pos, &mb[i*6], i == 0);
	}
	pos += sprintf(buf + pos, "\",\"mac_off\":\"");
	for (i = 0; i < CONFIG_IPQ_NO_MACS; i++)
		pos += sprintf(buf + pos, "%s0x%X", i ? ", " : "", i * 6);
	pos += sprintf(buf + pos, "\",\"wifi_mac\":\"");
	for (i = 0, n = 0; i < MACADDR_WIFI_MAX; i++)
		if (wifi_valid & (1 << i))
			pos += mac_fmt(buf + pos, wifi_macs[i], n++ == 0);
	for (f = 0; f < 4; f++) {
		pos += sprintf(buf + pos, "\",\"%s\":\"", wf_names[f]);
		for (i = 0, n = 0; i < MACADDR_WIFI_MAX; i++) {
			u32 val;
			if (!(wifi_valid & (1 << i))) continue;
			switch (f) {
			case 0: val = macaddr_wifi_offset(i); break;
			case 1: val = macaddr_wifi_checksum(i); break;
			case 2: val = macaddr_wifi_cs_offset(i); break;
			default: val = macaddr_wifi_cs_valid(i); break;
			}
			pos += sprintf(buf + pos, f == 1 ? "%s%X" : f == 3 ? "%s%d" : "%s0x%X", n++ ? ", " : "", val);
		}
	}
	pos += sprintf(buf + pos, "\"}");

	hdr_len = sprintf(hdr, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", pos);
	len = hdr_len + pos;
	memmove(buf + hdr_len, buf, pos);
	memcpy(buf, hdr, hdr_len);

	httpd_send_static(hs, buf, len);
}

static void httpd_handle_mac_set(struct failsafe_httpd_state *hs, char *data, int data_len) {
	static char resp[128];
	int len, ok = 0, type = -1, index = -1;
	char *body = strstr(data, "\r\n\r\n");
	char iface[16] = "", mac_str[32] = "";
	u8 mac[6];

	if (body) {
		body += 4;
		if (httpd_form_value(body, "iface", iface, sizeof(iface)) == 0 &&
		    httpd_form_value(body, "mac", mac_str, sizeof(mac_str)) == 0) {
			char *end = NULL;
			unsigned long idx = 0;

			if (!strncmp(iface, "eth", 3)) {
				idx = simple_strtoul(iface + 3, &end, 10);
				if (*end == '\0') {
					type = 0;
					index = (int)idx;
				}
			} else if (!strncmp(iface, "wifi", 4)) {
				idx = simple_strtoul(iface + 4, &end, 10);
				if (*end == '\0') {
					type = 1;
					index = (int)idx;
				}
			}

			/*
			 * eth_parse_enetaddr() is deliberately forgiving and
			 * turns "zz" into 0, which would let a malformed API
			 * request overwrite ART.  Require the canonical
			 * XX:XX:XX:XX:XX:XX form first.
			 */
			if (type >= 0 && mac_string_valid(mac_str)) {
				eth_parse_enetaddr(mac_str, mac);
				if (is_valid_ethaddr(mac))
					ok = (type ? macaddr_modify_wifi(mac, index) :
						     macaddr_modify_base(mac, index)) >= 0;
			}
		}
	}
	len = sprintf(resp, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n%s", ok ? "ok" : "error");
	httpd_send_static(hs, resp, len);
}

#ifdef CONFIG_QCA_MMC
static int append_emmc_gpt_json(int pos, int *count, block_dev_desc_t *blk_dev) {
	int gpt_count, i, n, remaining;
	disk_partition_t disk_info;

	remaining = PART_JSON_BUF_SIZE - pos;
	if (remaining < 340)
		return pos;
	n = snprintf(part_json_buf + pos, remaining,
		"%s{\"name\":\"0:GPT\",\"start\":0,\"size\":%llu,\"flash\":\"emmc\",\"blk_start\":0,\"blk_size\":34,\"blksz\":%lu}",
		(*count > 0 ? "," : ""), (u64)34 * (u64)blk_dev->blksz, (ulong)blk_dev->blksz);
	if (n < 0 || n >= remaining)
		return pos;
	pos += n;
	(*count)++;

	gpt_count = get_partition_count_efi(blk_dev);
	for (i = 1; i <= gpt_count && pos < PART_JSON_BUF_SIZE - 340; i++) {
		if (get_partition_info_efi(blk_dev, i, &disk_info) == 0) {
			remaining = PART_JSON_BUF_SIZE - pos;
			n = snprintf(part_json_buf + pos, remaining,
				",{\"name\":\"%s\",\"start\":%llu,\"size\":%llu,\"flash\":\"emmc\",\"blk_start\":%llu,\"blk_size\":%llu,\"blksz\":%lu}",
				disk_info.name, (u64)disk_info.start * (u64)blk_dev->blksz, (u64)disk_info.size * (u64)blk_dev->blksz,
				(u64)disk_info.start, (u64)disk_info.size, (ulong)blk_dev->blksz);
			if (n < 0 || n >= remaining)
				break;
			pos += n;
			(*count)++;
		}
	}

	remaining = PART_JSON_BUF_SIZE - pos;
	if (remaining >= 340) {
		n = snprintf(part_json_buf + pos, remaining,
			",{\"name\":\"0:GPTBACKUP\",\"start\":%llu,\"size\":%llu,\"flash\":\"emmc\",\"blk_start\":%llu,\"blk_size\":33,\"blksz\":%lu}",
			(u64)(blk_dev->lba - 33) * (u64)blk_dev->blksz, (u64)33 * (u64)blk_dev->blksz, (u64)(blk_dev->lba - 33), (ulong)blk_dev->blksz);
		if (n > 0 && n < remaining) {
			pos += n;
			(*count)++;
		}
	}

	return pos;
}
#endif

static void httpd_handle_partitions(struct failsafe_httpd_state *hs) {
	int i, pos = 0, hdr_len, count = 0, smem_count;
	char name[SMEM_PTN_NAME_MAX], hdr[128];
	uint32_t start, size, flash_type = SMEM_BOOT_NO_FLASH, bsize;
	qca_smem_flash_info_t *sfi = &qca_smem_flash_info;
#ifdef CONFIG_QCA_MMC
	block_dev_desc_t *blk_dev = NULL;
#endif

	if (get_current_flash_type(&flash_type) != 0)
		flash_type = sfi->flash_type;
	bsize = sfi->flash_block_size;

	pos += sprintf(part_json_buf + pos, "{\"parts\":[");

#ifdef CONFIG_QCA_MMC
	if (flash_type == SMEM_BOOT_MMC_FLASH) {
		blk_dev = mmc_get_dev(mmc_host.dev_num);
		if (blk_dev != NULL)
			pos = append_emmc_gpt_json(pos, &count, blk_dev);
	} else
#endif
	{
		smem_count = smem_getpart_count();
		for (i = 0; i < smem_count && pos < PART_JSON_BUF_SIZE - 200; i++) {
			if (smem_getpart_by_index(i, name, sizeof(name), &start, &size) == 0) {
				const char *pflash = "nor";
#ifdef CONFIG_CMD_NAND
				if (flash_type == SMEM_BOOT_NAND_FLASH || flash_type == SMEM_BOOT_QSPI_NAND_FLASH || get_which_flash_param(name))
					pflash = "nand";
#endif
				pos += sprintf(part_json_buf + pos,
					"%s{\"name\":\"%s\",\"start\":%lu,\"size\":%lu,\"flash\":\"%s\"}",
					(count > 0 ? "," : ""), name, (ulong)start * (ulong)bsize, (ulong)size, pflash);
				count++;
			}
		}
#ifdef CONFIG_QCA_MMC
		if ((sfi->flash_type == SMEM_BOOT_SPI_FLASH || flash_type == SMEM_BOOT_NORPLUSEMMC) &&
			(sfi->flash_secondary_type == SMEM_BOOT_MMC_FLASH ||
			 sfi->rootfs.offset == 0xBAD0FF5E || flash_type == SMEM_BOOT_NORPLUSEMMC)) {
			blk_dev = mmc_get_dev(mmc_host.dev_num);
			if (blk_dev != NULL)
				pos = append_emmc_gpt_json(pos, &count, blk_dev);
		}
#endif
	}

	if (pos < PART_JSON_BUF_SIZE - 320) {
		pos += sprintf(part_json_buf + pos, "],\"has_spi\":%s,\"spi_size\":%lu,\"has_nand\":%s,\"nand_size\":%lu,\"nand_raw_size\":%lu,\"ram_available\":%lu,\"has_emmc\":%s,\"emmc_size\":%llu,\"nand_type\":\"%s\"}",
			(sfi->flash_type == SMEM_BOOT_SPI_FLASH ? "true" : "false")
			,(ulong)(sfi->flash_type == SMEM_BOOT_SPI_FLASH ? get_spi_flash_size() : 0)
	#ifdef CONFIG_CMD_NAND
			,(nand_info[0].size > 0 || (CONFIG_SYS_MAX_NAND_DEVICE > 1 && nand_info[1].size > 0) ? "true" : "false")
			,(ulong)(nand_info[0].size > 0 ? nand_info[0].size : (CONFIG_SYS_MAX_NAND_DEVICE > 1 ? nand_info[1].size : 0))
			,(ulong)(nand_info[0].size > 0 && nand_info[0].writesize > 0 ?
				(nand_info[0].size / nand_info[0].writesize * (nand_info[0].writesize + nand_info[0].oobsize)) :
				(CONFIG_SYS_MAX_NAND_DEVICE > 1 && nand_info[1].size > 0 && nand_info[1].writesize > 0 ?
					(nand_info[1].size / nand_info[1].writesize * (nand_info[1].writesize + nand_info[1].oobsize)) : 0UL))
	#else
			,"false",0UL,0UL
	#endif
			,(ulong)(CONFIG_SYS_SDRAM_END - WEBFAILSAFE_UPLOAD_RAM_ADDRESS)
	#ifdef CONFIG_QCA_MMC
			,(blk_dev ? "true" : "false")
			,(u64)(blk_dev ? (u64)blk_dev->lba * (u64)blk_dev->blksz : 0ULL)
	#else
			,"false",0ULL
	#endif
	#ifdef CONFIG_CMD_NAND
			,(nand_info[0].size > 0 ? "parallel" : (CONFIG_SYS_MAX_NAND_DEVICE > 1 && nand_info[1].size > 0 ? "spi" : "parallel"))
	#else
			,"none"
	#endif
		);
	} else if (pos <= (int)sizeof(part_json_buf) - 3) {
		pos += sprintf(part_json_buf + pos, "]}");
	} else {
		static const char err[] =
			"HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\npartitions too large";
		httpd_send_static(hs, err, sizeof(err) - 1);
		return;
	}

	hdr_len = sprintf(hdr, "HTTP/1.0 200 OK\r\n" "Content-Type: application/json\r\n" "Content-Length: %d\r\n" "Connection: close\r\n\r\n", pos);
	if (hdr_len + pos > (int)sizeof(part_json_buf)) {
		static const char err[] =
			"HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\npartitions too large";
		httpd_send_static(hs, err, sizeof(err) - 1);
		return;
	}

	memmove(part_json_buf + hdr_len, part_json_buf, pos);
	memcpy(part_json_buf, hdr, hdr_len);

	hs->state = STATE_FILE_REQUEST;
	hs->dataptr = (u8_t *)part_json_buf;
	hs->upload = hdr_len + pos;
	httpd_send_data(hs);
}

static u64 backup_flash_capacity(const char *part_name, int raw, int flash_type_override)
{
	u64 capacity = 0;
	uint32_t flash_type;

	if (get_current_flash_type(&flash_type) != 0) {
		if (flash_type_override == -1)
			return 0;
		flash_type = flash_type_override;
	}

	if (!strcmp(part_name, "nor_full"))
		return get_spi_flash_size();

#ifdef CONFIG_CMD_NAND
	if (!strcmp(part_name, "nand_full") ||
	    flash_type_override == SMEM_BOOT_NAND_FLASH ||
	    (flash_type_override == -1 &&
	     (flash_type == SMEM_BOOT_NAND_FLASH ||
	      flash_type == SMEM_BOOT_QSPI_NAND_FLASH ||
	      flash_type == SMEM_BOOT_NORPLUSNAND ||
	      get_which_flash_param((char *)part_name) > 0))) {
#ifdef CONFIG_IPQ40XX
		int nd = is_spi_nand_available();
#else
		int nd = CONFIG_NAND_FLASH_INFO_IDX;
#endif
		if (!strcmp(part_name, "nand_full")) {
			if (nd >= 0 && nand_info[nd].size > 0) {
				capacity = nand_info[nd].size;
				if (raw && nand_info[nd].writesize > 0)
					capacity = capacity / nand_info[nd].writesize *
						(nand_info[nd].writesize + nand_info[nd].oobsize);
			}
			return capacity;
		}

		{
			u32 offset, part_size;

			if (getpart_offset_size((char *)part_name, &offset, &part_size) == 0 &&
			    part_size)
				return part_size;
		}

		if (nd >= 0 && nand_info[nd].size > 0)
			capacity = nand_info[nd].size;
		return capacity;
	}
#endif

#ifdef CONFIG_QCA_MMC
	if (flash_type_override == SMEM_BOOT_MMC_FLASH ||
	    (flash_type_override == -1 &&
	     (flash_type == SMEM_BOOT_MMC_FLASH ||
	      flash_type == SMEM_BOOT_NO_FLASH ||
	      flash_type == SMEM_BOOT_NORPLUSEMMC))) {
		block_dev_desc_t *blk_dev = mmc_get_dev(mmc_host.dev_num);

		if (blk_dev) {
			disk_partition_t disk_info;

			if (!strcmp(part_name, "0:GPT"))
				return (u64)GPT_PRIMARY_SIZE * blk_dev->blksz;
			if (!strcmp(part_name, "0:GPTBACKUP"))
				return (u64)GPT_BACKUP_SIZE * blk_dev->blksz;
			if (get_partition_info_efi_by_name(blk_dev, (char *)part_name,
							   &disk_info) == 0)
				return (u64)disk_info.size * blk_dev->blksz;
			if (flash_type_override == SMEM_BOOT_MMC_FLASH)
				return 0;
			if (flash_type == SMEM_BOOT_NORPLUSEMMC) {
				u32 offset, part_size;

				/* A NOR partition on a NOR+eMMC board. */
				if (getpart_offset_size((char *)part_name, &offset,
							&part_size) == 0 && part_size)
					return part_size;
				return get_spi_flash_size();
			}
			return (u64)blk_dev->lba * blk_dev->blksz;
		}
	}
#endif

	if (flash_type_override == SMEM_BOOT_SPI_FLASH ||
	    (flash_type_override == -1 &&
	     (flash_type == SMEM_BOOT_SPI_FLASH ||
	      flash_type == SMEM_BOOT_NORPLUSNAND ||
	      flash_type == SMEM_BOOT_NORPLUSEMMC))) {
#ifdef CONFIG_CMD_NAND
		if (flash_type_override == -1 &&
		    get_which_flash_param((char *)part_name) > 0) {
			u32 offset, part_size;

			if (getpart_offset_size((char *)part_name, &offset, &part_size) == 0 &&
			    part_size)
				return part_size;
		}
#endif
		if (flash_type_override == SMEM_BOOT_SPI_FLASH) {
			u32 offset, part_size;

			if (getpart_offset_size((char *)part_name, &offset, &part_size) == 0 &&
			    part_size)
				return part_size;
		}
		return get_spi_flash_size();
	}

	return capacity;
}

static void httpd_handle_backup(struct failsafe_httpd_state *hs, char *data, int data_len) {
	char *query = strchr(&data[4], '?'), part_name[64], filename[96], *size_param, *amp;
	ulong size;
	int hdr_len, raw = 0, flash_type_override = -1, flash_invalid = 0;
	u32_t ram_avail;
	u64 total_size = 0;

	if (upload_pending || (hs_global && hs_global != hs)) {
		static const char *busy =
			"HTTP/1.0 503 Service Unavailable\r\nConnection: close\r\n\r\nBusy";
		httpd_send_static(hs, busy, strlen(busy));
		return;
	}

	if (!query || strncmp(query + 1, "part=", 5) != 0) {
		static const char *err = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\nMissing partition";
		httpd_send_static(hs, err, strlen(err));
		return;
	}

	strncpy(part_name, query + 6, sizeof(part_name) - 1);
	part_name[sizeof(part_name) - 1] = '\0';
	str_trim_crlf(part_name);
	url_decode(part_name);

	amp = strchr(part_name, '&');
	if (amp) *amp = '\0';

	if (strstr(query, "raw=1"))
		raw = 1;

	{
		char *flash_param = query;

		/*
		 * Only accept an exact field name.  Substring matching would make
		 * "notflash=nor" select NOR and would silently ignore typoed
		 * values instead of rejecting them.
		 */
		while ((flash_param = strstr(flash_param, "flash=")) != NULL) {
			if (flash_param == query || flash_param[-1] == '?' ||
			    flash_param[-1] == '&')
				break;
			flash_param++;
		}

		if (flash_param) {
			char *fp = flash_param + 6;
			int flen = 0;

			while (fp[flen] && fp[flen] != '&' && fp[flen] != '\r' &&
			       fp[flen] != '\n' && fp[flen] != ' ')
				flen++;

			if (flen == 4 && !strncmp(fp, "emmc", 4))
				flash_type_override = SMEM_BOOT_MMC_FLASH;
			else if (flen == 4 && !strncmp(fp, "nand", 4))
				flash_type_override = SMEM_BOOT_NAND_FLASH;
			else if (flen == 3 && !strncmp(fp, "nor", 3))
				flash_type_override = SMEM_BOOT_SPI_FLASH;
			else
				flash_invalid = 1;
		}
	}

	if (flash_invalid) {
		static const char *err = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\nInvalid flash type";
		httpd_send_static(hs, err, strlen(err));
		return;
	}

	size_param = strstr(query, "size=");
	if (size_param)
		total_size = simple_strtoull(size_param + 5, NULL, 10);

	printf("Backup request: %s%s size [%llu.%02llu MiB | %llu bytes]\n",
		part_name, raw ? " (raw)" : "",
		mib_int(total_size), mib_frac(total_size),
		total_size);

	if (total_size == 0) {
		static const char *err = "HTTP/1.0 400 Bad Request\r\nConnection: close\r\n\r\nMissing size";
		httpd_send_static(hs, err, strlen(err));
		return;
	}

	{
		u64 capacity = backup_flash_capacity(part_name, raw, flash_type_override);

		if (capacity && total_size > capacity) {
			printf("Backup: requested %llu exceeds capacity %llu, capped\n",
			       total_size, capacity);
			total_size = capacity;
		}
	}

	ram_avail = (u32_t)CONFIG_SYS_SDRAM_END - (u32_t)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;
	backup.raw = raw;
	backup.flash_type = flash_type_override;
	backup.total_size = total_size;
	strncpy(backup.part_name, part_name, sizeof(backup.part_name) - 1);
	backup.part_name[sizeof(backup.part_name) - 1] = '\0';
	flashread_yield_fn = flashread_yield;

	{
		u32 first_size = (total_size > ram_avail) ? ram_avail : (u32)total_size;
		char chunk_detail[64] = "";

		if (flashread_partition_chunk(part_name, WEBFAILSAFE_UPLOAD_RAM_ADDRESS, 0,
					      first_size, raw, flash_type_override,
					      NULL, &size, chunk_detail) != CMD_RET_SUCCESS ||
		    size == 0) {
			snprintf(part_json_buf, sizeof(part_json_buf),
				 "HTTP/1.0 500 Internal Server Error\r\nConnection: close\r\n\r\nRead failed%s%s",
				 chunk_detail[0] ? ": " : "", chunk_detail);
			httpd_send_static(hs, part_json_buf, strlen(part_json_buf));
			return;
		}
		if (size > total_size)
			size = total_size;

		backup.data_addr = (u32_t)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;
		backup.data_size = (u32_t)size;

		if (total_size > ram_avail) {
			backup.chunked = 1;
			backup.total_chunks = (int)((total_size + ram_avail - 1) / ram_avail);
			backup.chunk_num = 1;
			backup.chunk_offset = (u64)size;
			backup.total_sent = (u64)size;
			backup.total_remaining = total_size - (u64)size;
			printf("Backup: %llu.%02llu MiB > RAM %u.%02u MiB %d chunks transfer\n",
				mib_int(total_size), mib_frac(total_size),
				(u32)mib_int(ram_avail), (u32)mib_frac(ram_avail),
				backup.total_chunks);
			printf("Backup: %d/%d %llu.%02llu/%llu.%02llu MiB\n",
				backup.chunk_num, backup.total_chunks,
				mib_int(backup.total_sent), mib_frac(backup.total_sent),
				mib_int(backup.total_size), mib_frac(backup.total_size));
			backup.chunk_num++;
		} else {
			total_size = size;
			backup.total_size = total_size;
			backup.chunked = 0;
			backup.total_remaining = 0;
			backup.total_sent = 0;
			backup.chunk_offset = 0;
		}
	}

	/* Do not re-enter lwIP from this tcp_recv callback. */
	mdelay(1);

	sprintf(filename, "%s%s.bin", part_name, raw ? "_oob" : "");
	hdr_len = sprintf(part_json_buf, "HTTP/1.0 200 OK\r\n" "Content-Type: application/octet-stream\r\n"
		"Content-Disposition: attachment; filename=\"%s\"\r\n" "Content-Length: %llu\r\n" "Connection: close\r\n\r\n", filename, total_size);

	hs->state = STATE_FILE_REQUEST;
	hs->owns_global = 1;
	hs_global = hs;
	tcp_setprio(hs->pcb, TCP_PRIO_NORMAL);
	hs->dataptr = (u8_t *)part_json_buf;
	hs->upload = hdr_len;
	httpd_send_data(hs);

	backup.sending_header = 1;
}

static int httpd_handle_file_request(struct failsafe_httpd_state *hs, char *data, int data_len) {
	struct fs_file fsfile;
	u32_t i;

	if (data_len < 6)
		return -1;

	if (data_len >= 13 && memcmp((const void *)&data[4], "/cgi-bin/", 9) == 0) {
		httpd_send_static(hs, "HTTP/1.0 302 Found\r\nLocation: /index.html\r\n\r\n", 44);
		return 0;
	}

	for (i = 4; i < 30 && (int)i < data_len; i++) {
		if (is_http_whitespace(data[i])) {
			data[i] = 0;
			i = 0;
			break;
		}
	}
	if (i != 0) {
		print_error("request file name too long!");
		return -1;
	}

	{
		char *q = strstr((char *)&data[4], "?");
		if (q) *q = 0;
	}

	if (!strstr(&data[4], ".css") && !strstr(&data[4], ".js") && !strstr(&data[4], ".svg") && !strstr(&data[4], ".ico"))
		printf("GET: %s\n", &data[4]);

	if (data[4] == ISO_slash && data[5] == 0) {
		fs_open(file_index_html[0].name, &fsfile);
	} else {
		{
			static const struct { const char *url; const char *hash; } redirects[] = {
				{"/firmware.html", "firmware"},
				{"/uboot.html", "uboot"},
				{"/art.html", "art"},
				{"/cdt.html", "cdt"},
				{"/ptable.html", "ptable"},
				{"/initramfs.html", "initramfs"},
				{"/mibib.html", "ptable"},
				{"/gpt.html", "ptable"},
			};
			static char redirect_buf[128];
			int i;
			for (i = 0; i < sizeof(redirects) / sizeof(redirects[0]); i++) {
				if (strcmp(&data[4], redirects[i].url) == 0) {
					int len = sprintf(redirect_buf,
						"HTTP/1.0 302 Found\r\nLocation: /update.html#%s\r\n\r\n",
						redirects[i].hash);
					httpd_send_static(hs, redirect_buf, len);
					return 0;
				}
			}
		}
		if (!fs_open((const char *)&data[4], &fsfile)) {
			if (!strstr(&data[4], "favicon.ico"))
				print_error("file not found!");
			fs_open(file_404_html[0].name, &fsfile);
		}
	}

	httpd_send_static(hs, fsfile.data, fsfile.len);
	return 0;
}

void httpd_send_data(struct failsafe_httpd_state *hs) {
	while (hs->upload > 0) {
		u16_t snd_buf = tcp_sndbuf(hs->pcb);
		if (snd_buf == 0)
			break;

		u16_t send_len = (hs->upload > snd_buf) ? snd_buf : (u16_t)hs->upload;
		err_t wr_err = tcp_write(hs->pcb, hs->dataptr, send_len, TCP_WRITE_FLAG_COPY);
		if (wr_err == ERR_OK) {
			hs->dataptr += send_len;
			hs->upload -= send_len;
			continue;
		}

		if (send_len > TCP_MSS) {
			send_len = TCP_MSS;
			if (hs->upload < send_len)
				send_len = (u16_t)hs->upload;
			wr_err = tcp_write(hs->pcb, hs->dataptr, send_len, TCP_WRITE_FLAG_COPY);
			if (wr_err == ERR_OK) {
				hs->dataptr += send_len;
				hs->upload -= send_len;
				continue;
			}
		}
		break;
	}
	tcp_output(hs->pcb);
}

static void backup_chunk_next(void) {
	u32_t ram_avail = (u32_t)CONFIG_SYS_SDRAM_END - (u32_t)WEBFAILSAFE_UPLOAD_RAM_ADDRESS,
		  chunk_size = (backup.total_remaining > ram_avail) ? ram_avail : (u32)backup.total_remaining;
	ulong rd_size;
	char chunk_detail[64] = "";

	if (flashread_partition_chunk(backup.part_name, WEBFAILSAFE_UPLOAD_RAM_ADDRESS, backup.chunk_offset, chunk_size, backup.raw, backup.flash_type, NULL, &rd_size, chunk_detail) == CMD_RET_SUCCESS && rd_size > 0) {
		backup.data_addr = (u32_t)WEBFAILSAFE_UPLOAD_RAM_ADDRESS;
		backup.data_size = (u32_t)rd_size;
		backup.chunk_offset += backup.data_size;
		backup.total_sent += backup.data_size;
		if (backup.total_sent > backup.total_size) {
			backup.data_size -= (u32_t)(backup.total_sent - backup.total_size);
			backup.total_sent = backup.total_size;
		}
		backup.total_remaining = backup.total_size - backup.total_sent;
		printf("Backup: %d/%d %llu.%02llu/%llu.%02llu MiB\n", backup.chunk_num, backup.total_chunks, mib_int(backup.total_sent), mib_frac(backup.total_sent), mib_int(backup.total_size), mib_frac(backup.total_size));
		backup.chunk_num++;
		httpd_poll_wait(2);
		if (hs_global) {
			hs_global->dataptr = (u8_t *)(uintptr_t)backup.data_addr;
			hs_global->upload = backup.data_size;
			httpd_send_data(hs_global);
			backup.chunk_busy = 0;
		} else {
			backup.chunked = 0;
			backup.total_remaining = 0;
			backup.chunk_busy = 0;
		}
	} else {
		printf("Backup: chunk failed at offset %llu.%02llu MiB%s%s\n",
			mib_int(backup.chunk_offset), mib_frac(backup.chunk_offset),
			chunk_detail[0] ? ": " : "", chunk_detail);
		backup.chunk_busy = 0;
		backup.chunked = 0;
		backup.total_remaining = 0;
		/*
		 * The client already has the original Content-Length. Closing
		 * now makes the short transfer fail immediately instead of
		 * leaving the connection idle until the 5-minute poll timeout.
		 */
		if (hs_global)
			httpd_close_conn(hs_global, hs_global->pcb);
	}
}

static void httpd_poll_wait(int count) {
	int i;
	for (i = 0; i < count; i++) {
		mdelay(1);
		eth_rx();
		sys_check_timeouts();
	}
}

static err_t httpd_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
	struct failsafe_httpd_state *hs = (struct failsafe_httpd_state *)arg;

	if (hs == NULL)
		return ERR_OK;

	hs->last_activity = (u32_t)get_timer(0);

	{
		int owner = hs->owns_global && hs == hs_global;

		if (owner && backup.sending_header && hs->upload <= 0) {
			backup.sending_header = 0;
			hs->state = STATE_FILE_REQUEST;
			hs->dataptr = (u8_t *)(uintptr_t)backup.data_addr;
			hs->upload = backup.data_size;
		}

		if (hs->upload <= 0) {
			if (owner && backup.chunked && backup.total_remaining > 0 &&
			    !backup.chunk_busy) {
				backup.chunk_busy = 1;
				return ERR_OK;
			}
			if (owner && backup.chunk_busy)
				return ERR_OK;
		}
	}

	if (hs->upload <= 0) {
		if (upload.done && (!upload_hs || hs == upload_hs)) {
			if (!upload.failed)
				webfailsafe_ready_for_upgrade = 1;
			upload.done = 0;
			upload.failed = 0;
			upload_hs = NULL;
		}
		httpd_close_conn(hs, pcb);
		return ERR_OK;
	}

	httpd_send_data(hs);
	return ERR_OK;
}

static err_t httpd_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
	struct failsafe_httpd_state *hs = (struct failsafe_httpd_state *)arg;
	char *data;
	int data_len, need_free = 0, offset;
	struct pbuf *q;

	if (hs == NULL) {
		if (p) pbuf_free(p);
		return ERR_OK;
	}

	if (p == NULL) {
		/*
		 * A half-close can arrive before httpd_sent() consumes
		 * upload.done. Arm the upgrade here as well; the response was
		 * already queued by httpd_check_upload_complete().
		 */
		if (upload.done && (!upload_hs || hs == upload_hs)) {
			if (!upload.failed)
				webfailsafe_ready_for_upgrade = 1;
			upload.done = 0;
			upload.failed = 0;
			upload_hs = NULL;
		}
		httpd_close_conn(hs, pcb);
		return ERR_OK;
	}

	hs->last_activity = (u32_t)get_timer(0);

	{
		int alloc_len = p->tot_len + 1;

		/* Keep short requests from reading/writing past the buffer. */
		if (alloc_len < 64)
			alloc_len = 64;
		data = malloc(alloc_len);
		if (!data) {
			pbuf_free(p);
			httpd_conn_abort(hs, pcb);
			return ERR_ABRT;
		}
		memset(data, 0, alloc_len);
	}
	offset = 0;
	for (q = p; q; q = q->next) {
		memcpy(data + offset, q->payload, q->len);
		offset += q->len;
	}
	data_len = p->tot_len;
	need_free = 1;

	switch (hs->state) {
	case STATE_NONE:
		if (strncmp(data, "GET", 3) == 0 && is_http_method_separator(data[3])) {
			if (strncmp(&data[4], "/webterm", 8) == 0) {
				webterm_http_handler(hs, data, data_len);
				break;
			}
			if (strncmp(&data[4], "/upgrade_status", 15) == 0) {
				httpd_handle_upgrade_status(hs);
				break;
			}
			if (strncmp(&data[4], "/partitions", 11) == 0 &&
				data[15] == ISO_space) {
				httpd_handle_partitions(hs);
				break;
			}
			if (strncmp(&data[4], "/backup?", 8) == 0) {
				httpd_handle_backup(hs, data, data_len);
				break;
			}
			if (strncmp(&data[4], "/about", 6) == 0 && data[10] == ISO_space) {
				httpd_handle_about(hs);
				break;
			}
			if (strncmp(&data[4], "/mac_info", 9) == 0 && data[13] == ISO_space) {
				httpd_handle_mac_info(hs);
				break;
			}
			if (strncmp(&data[4], "/led?", 5) == 0) {
				httpd_handle_led(hs, data);
				break;
			}
			if (strncmp(&data[4], "/btn_detect", 11) == 0 && data[15] == ISO_space) {
				httpd_handle_btn_detect(hs);
				break;
			}
			if (httpd_handle_file_request(hs, data, data_len) < 0)
				return httpd_recv_abort(hs, pcb, data, need_free, p);
		} else if (strncmp(data, "POST", 4) == 0 && is_http_method_separator(data[4])) {
			if (strncmp(&data[5], "/webterm", 8) == 0) {
				webterm_http_handler(hs, data, data_len);
				break;
			}
			if (strncmp(&data[5], "/env_set", 8) == 0) {
				httpd_handle_env_set(hs, data, data_len);
				break;
			}
			if (strncmp(&data[5], "/mac_set", 8) == 0) {
				httpd_handle_mac_set(hs, data, data_len);
				break;
			}
			data[data_len] = '\0';
			if (upload_pending || (hs_global && hs_global != hs)) {
				static const char busy[] =
					"HTTP/1.0 503 Service Unavailable\r\nConnection: close\r\n\r\nBusy";
				httpd_send_static(hs, busy, sizeof(busy) - 1);
				break;
			}
			httpd_reset_upload_state();
			if (httpd_parse_content_length(hs, data) < 0)
				return httpd_recv_abort(hs, pcb, data, need_free, p);
			hs->state = STATE_UPLOAD_REQUEST;
			hs->owns_global = 1;
			hs_global = hs;
			tcp_setprio(pcb, TCP_PRIO_NORMAL);
			led_off("blink_led");
			if (httpd_parse_boundary(data) < 0 || httpd_init_upload_ram() < 0)
				return httpd_recv_abort(hs, pcb, data, need_free, p);
			if (httpd_findandstore_firstchunk(hs, data, data_len)) {
				upload.data_start_found = 1;
				if (httpd_check_upload_size(hs) < 0)
					return httpd_recv_abort(hs, pcb, data, need_free, p);
				httpd_check_upload_complete(hs);
			} else {
				upload.data_start_found = 0;
			}
		} else {
			return httpd_recv_abort(hs, pcb, data, need_free, p);
		}
		break;

	case STATE_UPLOAD_REQUEST:
		if (!upload.data_start_found) {
			data[data_len] = '\0';
			if (!httpd_findandstore_firstchunk(hs, data, data_len)) {
				print_error("couldn't find start of data in next packet!");
				return httpd_recv_abort(hs, pcb, data, need_free, p);
			}
			upload.data_start_found = 1;
			if (httpd_check_upload_size(hs) < 0)
				return httpd_recv_abort(hs, pcb, data, need_free, p);
			httpd_check_upload_complete(hs);
		} else {
			hs->upload += data_len;
			if (!upload.failed)
				httpd_handle_upload_data(hs, data, data_len);
			httpd_check_upload_complete(hs);
		}
		break;

	case STATE_FILE_REQUEST:
		break;
	}

	tcp_recved(pcb, p->tot_len);
	if (need_free) free(data);
	pbuf_free(p);
	return ERR_OK;
}

static void httpd_err(void *arg, err_t err) {
	struct failsafe_httpd_state *hs = (struct failsafe_httpd_state *)arg;
	if (hs) {
		if (hs == hs_global)
			hs_global = NULL;
		if (hs == upload_hs)
			httpd_discard_pending_upload();
		httpd_state_reset(hs);
		free(hs);
	}
}

static err_t httpd_poll_cb(void *arg, struct tcp_pcb *pcb) {
	struct failsafe_httpd_state *hs = (struct failsafe_httpd_state *)arg;
	if (hs == NULL)
		return ERR_OK;

	if (get_timer(hs->last_activity) >= 300000) {
		if (hs == upload_hs)
			httpd_discard_pending_upload();
		if (hs == hs_global)
			hs_global = NULL;
		httpd_conn_abort(hs, pcb);
		return ERR_ABRT;
	}

	if (hs->upload > 0)
		httpd_send_data(hs);

	return ERR_OK;
}

static err_t httpd_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
	struct failsafe_httpd_state *hs;

	hs = malloc(sizeof(struct failsafe_httpd_state));
	if (hs == NULL) {
		return ERR_MEM;
	}
	memset(hs, 0, sizeof(struct failsafe_httpd_state));
	hs->pcb = pcb;
	hs->state = STATE_NONE;
	hs->last_activity = get_timer(0);
	hs->owns_global = 0;

	tcp_arg(pcb, hs);
	tcp_recv(pcb, httpd_recv);
	tcp_sent(pcb, httpd_sent);
	tcp_err(pcb, httpd_err);
	tcp_poll(pcb, httpd_poll_cb, 1);
	tcp_setprio(pcb, TCP_PRIO_MIN);

	return ERR_OK;
}

static struct tcp_pcb *listen_pcb;

void failsafe_httpd_init(void) {
	struct tcp_pcb *pcb;

	fs_init();

	pcb = tcp_new();
	if (pcb == NULL)
		return;

	if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
		tcp_close(pcb);
		return;
	}

	listen_pcb = tcp_listen(pcb);
	if (listen_pcb == NULL) {
		tcp_close(pcb);
		return;
	}

	tcp_accept(listen_pcb, httpd_accept);
}

static struct netif failsafe_netif;

static int httpd_progress_start_done = 0;
static ulong periodic_timer = 0;
static ulong eth_retry_timer = 0;

static void abort_port_pcb(struct tcp_pcb **list) {
	struct tcp_pcb *pcb, *next;
	for (pcb = *list; pcb != NULL; pcb = next) {
		next = pcb->next;
		if (pcb->local_port == 80) {
			struct failsafe_httpd_state *hs =
				(struct failsafe_httpd_state *)pcb->callback_arg;

			httpd_detach_pcb(pcb);
			if (hs) {
				httpd_state_reset(hs);
				free(hs);
			}
			tcp_abort(pcb);
		}
	}
}

void failsafe_httpd_stop(void) {
	if (listen_pcb != NULL) {
		tcp_close(listen_pcb);
		listen_pcb = NULL;
	}
	abort_port_pcb(&tcp_active_pcbs);
	abort_port_pcb(&tcp_tw_pcbs);
	abort_port_pcb(&tcp_bound_pcbs);
	hs_global = NULL;
	/* Drop any upload/backup state left over from the stopped session. */
	memset(&upload, 0, sizeof(upload));
	memset(&backup, 0, sizeof(backup));
	upload_pending = 0;
	upload_hs = NULL;
	if (boundary_value) {
		free(boundary_value);
		boundary_value = NULL;
	}
	flashread_yield_fn = NULL;
#ifdef CONFIG_QCA_MMC
	emmc_cache_invalidate();
#endif
	netif_remove(&failsafe_netif);
	httpd_progress_start_done = 0;
	eth_retry_timer = 0;
	periodic_timer = 0;
}

static int lwip_initialized = 0;



void failsafe_lwip_init(struct ip4_addr *ipaddr, struct ip4_addr *netmask, struct ip4_addr *gw) {
	if (!lwip_initialized) {
		lwip_init();
		lwip_initialized = 1;
	}

	netif_add(&failsafe_netif, ipaddr, netmask, gw, NULL,
		  ethernetif_init, ethernet_input);
	netif_set_default(&failsafe_netif);
	netif_set_up(&failsafe_netif);
	netif_set_link_up(&failsafe_netif);

	failsafe_httpd_init();

#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
	ppe_arp_kickstart();
#endif
}

void failsafe_httpd_poll(void) {
	ulong now = get_timer(0);
#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
	int link_changed = 0;
#endif

	if (!webfailsafe_is_running)
		return;

	if (webfailsafe_ready_for_upgrade) {
		webfailsafe_ready_for_upgrade = 0;
		upgrade_status = 1;
		setenv_hex("filesize", net_boot_file_size);
		setenv_hex("filesize_128k", (net_boot_file_size/131072+(net_boot_file_size%131072!=0))*131072);
		setenv_hex("fileaddr", WEBFAILSAFE_UPLOAD_RAM_ADDRESS);
		do_http_progress(WEBFAILSAFE_PROGRESS_UPLOAD_READY);

		httpd_poll_wait(1000);

		upgrade_status = 2;
		httpd_poll_wait(1500);

		{
			int upgrade_ret = do_http_upgrade(net_boot_file_size,
							  webfailsafe_upgrade_type);

			upload_pending = 0;
			if (upgrade_ret != 0) {
				do_http_progress(WEBFAILSAFE_PROGRESS_UPGRADE_FAILED);
				upgrade_status = 3;
				httpd_poll_wait(2000);
				return;
			}
		}
		upgrade_status = webfailsafe_auto_reboot_enabled ? 4 : 5;

		httpd_poll_wait(3000);
		if (webfailsafe_auto_reboot_enabled) {
			HttpdDone();
			do_reset(NULL, 0, 0, NULL);
			printf("reboot fail\n");
		}
		return;
	}

	/*
	 * Execute web-terminal commands from the main poll loop, never from a
	 * lwIP callback. A command may halt/reinit Ethernet.
	 */
	if (webterm_run_pending_command()) {
		if (!eth_is_active(eth_get_dev()))
			eth_retry_timer = 0;
		/* A web-terminal command may have stopped the server. */
		if (!webfailsafe_is_running)
			return;
	}

#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
	link_changed = eth_check_link_change();
#else
	eth_check_link_change();
#endif

	if (!eth_is_active(eth_get_dev())) {
		if (get_timer(eth_retry_timer) >= 2000) {
			eth_retry_timer = get_timer(0);
			eth_halt();
			eth_set_current();
			eth_init();
#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
			ppe_arp_kickstart();
#endif
		}
	} else {
		eth_retry_timer = get_timer(0);
#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
		if (link_changed > 0)
			ppe_arp_kickstart();
#endif
	}

	if (!httpd_progress_start_done && eth_is_active(eth_get_dev())) {
		do_http_progress(WEBFAILSAFE_PROGRESS_START);
		httpd_progress_start_done = 1;
	}

	if (backup.chunked && backup.chunk_busy && backup.total_remaining > 0)
		backup_chunk_next();

	if (eth_rx() > 0) {
#ifdef CONFIG_DHCPD
		dhcpd_poll_server();
#endif
	}

	sys_check_timeouts();

	if (get_timer(periodic_timer) >= 500) {
		periodic_timer = now;
		sys_check_timeouts();
	}
}

#if defined(CONFIG_IPQ5332) || defined(CONFIG_IPQ9574)
#include "lwip/prot/ethernet.h"

struct ppe_arp_hdr {
	struct eth_hdr ethhdr;
	u16_t hwtype;
	u16_t protocol;
	u8_t hwlen;
	u8_t protolen;
	u16_t opcode;
	struct eth_addr shwaddr;
	u16_t sipaddr[2];
	struct eth_addr dhwaddr;
	u16_t dipaddr[2];
};

void ppe_arp_kickstart(void) {
	uchar pkt[60];
	struct ppe_arp_hdr *arp = (struct ppe_arp_hdr *)pkt;
	u16_t *hostaddr = (u16_t *)netif_ip4_addr(&failsafe_netif);

	memset(pkt, 0, sizeof(pkt));
	memset(arp->ethhdr.dest.addr, 0xff, 6);
	arp->ethhdr.src = *(struct eth_addr *)net_ethaddr;
	arp->ethhdr.type = lwip_htons(ETHTYPE_ARP);

	arp->hwtype = lwip_htons(1);
	arp->protocol = lwip_htons(ETHTYPE_IP);
	arp->hwlen = 6;
	arp->protolen = 4;
	arp->opcode = lwip_htons(1);

	arp->shwaddr = *(struct eth_addr *)net_ethaddr;
	arp->sipaddr[0] = hostaddr[0];
	arp->sipaddr[1] = hostaddr[1];

	arp->dhwaddr = *(struct eth_addr *)net_ethaddr;
	arp->dipaddr[0] = hostaddr[0];
	arp->dipaddr[1] = (hostaddr[1] & lwip_htons(0xFF00)) | lwip_htons(0x00FE);

	eth_send(pkt, sizeof(pkt));
}
#endif

void httpd_poll(void) {
	failsafe_httpd_poll();
}

void httpd_stop(void) {
	webfailsafe_is_running = 0;
}

int httpd_is_running(void) {
	return webfailsafe_is_running;
}
