#include <common.h>
#include <command.h>
#include <sysupgrade_parser.h>
#include <ipq_api.h>
#include <asm/arch-qca-common/smem.h>
#ifdef CONFIG_CMD_NAND
#include <nand.h>
#endif
#ifdef CONFIG_CMD_UBI
extern int ubi_volume_can_replace(const char *name, size_t size);
#endif
#ifdef CONFIG_IPQ40XX
#include <../board/qca/arm/common/fdt_info.h>
#endif

static u64 tar_parse_size(const char *s, int len) {
	u64 val = 0;
	int i, seen = 0;

	for (i = 0; i < len; i++) {
		if (s[i] == ' ' || s[i] == '\0') {
			if (seen)
				break;
			continue;
		}
		if (s[i] < '0' || s[i] > '7')
			return 0xFFFFFFFFFFFFFFFFULL;
		if (val > (0xFFFFFFFFFFFFFFFFULL - 7) / 8)
			return 0xFFFFFFFFFFFFFFFFULL;
		val = val * 8 + (s[i] - '0');
		seen = 1;
	}
	return val;
}

static int tar_is_kernel_name(const char *name) {
	return strstr(name, "kernel") != NULL ||
		strstr(name, "hlos") != NULL ||
		strstr(name, "fit") != NULL;
}

static int tar_is_rootfs_name(const char *name) {
	return strstr(name, "root") != NULL ||
		strstr(name, "squashfs") != NULL;
}

static sysupgrade_fw_parts parse_sysupgrade(void *address, size_t size) {
	sysupgrade_fw_parts parts;
	uintptr_t base = (uintptr_t)address, pos, data, end;
	int header_off, max_entries = 32, entry, i, j;
	char *hdr;
	u64 fsize, padded;
	const uint32_t MAGIC_HSQS = 0x73717368;
	uint8_t *sb;
	u64 bytes_used;
	int invalid = 0;

	memset(&parts, 0, sizeof(parts));

	if (!address || size < 512) {
		printf("parse failed\n");
		return parts;
	}

	end = base + size;
	header_off = (size >= 262 && memcmp((void *)(base + 257), "ustar", 5) == 0) ? 0 : 10;

	if ((uintptr_t)size < (uintptr_t)header_off + 512) {
		printf("parse failed\n");
		return parts;
	}

	if (memcmp((void *)(base + header_off + 257), "ustar", 5) == 0 ||
	    memcmp((void *)(base + 257), "ustar", 5) == 0) {
		pos = base + header_off;

		for (entry = 0; entry < max_entries; entry++) {
			char name[101];

			if (pos >= end || end - pos < 512)
				break;

			hdr = (char *)pos;

			if (hdr[0] == 0)
				break;

			if (memcmp(hdr + 257, "ustar", 5) != 0)
				break;

			memcpy(name, hdr, 100);
			name[100] = '\0';

			fsize = tar_parse_size(hdr + 124, 12);
			data = pos + 512;
			if (fsize == 0xFFFFFFFFFFFFFFFFULL ||
			    data > end || fsize > (u64)(end - data)) {
				invalid = 1;
				break;
			}

			padded = (fsize + 511) & ~511ULL;
			if (padded > (u64)(end - data)) {
				invalid = 1;
				break;
			}

			if (fsize > 0 && !parts.kernel_data && tar_is_kernel_name(name)) {
				parts.kernel_data = (const void *)data;
				parts.kernel_size = fsize;
			} else if (fsize > 0 && !parts.rootfs_data && tar_is_rootfs_name(name)) {
				parts.rootfs_data = (const void *)data;
				parts.rootfs_size = fsize;
			}

			pos = data + (uintptr_t)padded;
		}
	} else {
		for (i = header_off; (size_t)i + 4 <= size; i += 4) {
			if (*(uint32_t *)(base + i) == MAGIC_HSQS) {
				if ((size_t)i + 48 > size) {
					invalid = 1;
					break;
				}

				parts.kernel_data = (const void *)(base + header_off);
				parts.kernel_size = i - header_off;
				parts.rootfs_data = (const void *)(base + i);
				sb = (uint8_t *)parts.rootfs_data;
				bytes_used = 0;
				for (j = 0; j < 8; j++)
					bytes_used |= (u64)sb[40 + j] << (j * 8);
				if (bytes_used == 0 || bytes_used > size - i) {
					invalid = 1;
					break;
				}
				parts.rootfs_size = (size_t)bytes_used;
				break;
			}
		}
	}

	if (invalid ||
	    !parts.kernel_data || !parts.kernel_size ||
	    !parts.rootfs_data || !parts.rootfs_size)
		memset(&parts, 0, sizeof(parts));

	sysupgrade_parts_valid(&parts) ?
		printf("kernel %lu, rootfs %lu\n",
				(unsigned long)parts.kernel_size, (unsigned long)parts.rootfs_size) :
		printf("parse failed\n");

	return parts;
}

sysupgrade_fw_parts parse_sysupgrade_firmware(void *address, size_t size) {
	return parse_sysupgrade(address, size);
}

#ifdef CONFIG_CMD_UBI
int sysupgrade_ubi_init(void) {
	qca_smem_flash_info_t *sfi = &qca_smem_flash_info;

	if (sfi->rootfs.offset == 0xBAD0FF5E || !sfi->rootfs.size)
		return -1;

#ifdef IPQ_UBI_VOL_WRITE_SUPPORT
	if (sfi->flash_type == SMEM_BOOT_NAND_FLASH ||
		sfi->flash_type == SMEM_BOOT_QSPI_NAND_FLASH ||
		(sfi->flash_type == SMEM_BOOT_SPI_FLASH &&
			get_which_flash_param("rootfs") > 0)) {
		if (ubi_set_rootfs_part() == 0)
			return 0;
	}
#endif

	int nand_dev;
#ifdef CONFIG_IPQ40XX
	nand_dev = is_spi_nand_available();
#else
	nand_dev = CONFIG_NAND_FLASH_INFO_IDX;
#endif

	char cmd[256];
	char *msmparts = getenv("msmparts");
	snprintf(cmd, sizeof(cmd),
		 "nand device %d && "
		 "setenv mtdids nand%d=nand%d && "
		 "setenv mtdparts mtdparts=nand%d:0x%llx@0x%llx(fs)%s%s",
		 nand_dev, nand_dev, nand_dev, nand_dev, sfi->rootfs.size,
		 sfi->rootfs.offset, msmparts ? "," : "", msmparts ? msmparts : "");

	return run_command(cmd, 0);
}

static int ubi_write_volume(const char *name, const void *data, size_t size, int required) {
	char buf[256];
	char rm_cmd[64];

	/*
	 * Never remove the old volume until we know the replacement can be
	 * allocated. Otherwise an ENOSPC create leaves the device without a
	 * kernel/rootfs volume.
	 */
	if (ubi_volume_can_replace(name, size) != 0) {
		if (required)
			printf("%s not updated: not enough free UBI space\n", name);
		return -1;
	}

	snprintf(rm_cmd, sizeof(rm_cmd), "ubi remove %s", name);
	run_command(rm_cmd, 0);

	snprintf(buf, sizeof(buf),
		 "ubi create %s 0x%llx && "
		 "ubi write 0x%lx %s 0x%llx",
		 name, (u64)size,
		 (unsigned long)data, name,
		 (u64)size);

	if (run_command(buf, 0) != 0) {
		required ? printf("%s write failed\n", name) :
			   printf("%s skip\n", name);
		return -1;
	}
	return 0;
}

int sysupgrade_write_ubi_volumes(sysupgrade_fw_parts *parts, int backup_enabled) {
	int ret = 0;

	if (sysupgrade_ubi_init() != 0) {
		printf("UBI init failed\n");
		return -1;
	}

	if (run_command("ubi part fs", 0) != 0) {
		printf("UBI attach failed\n");
		return -1;
	}

	/*
	 * A missing rootfs_data volume is expected on first boot. The
	 * authoritative check is that it can be recreated below.
	 */
	if (run_command("ubi remove rootfs_data", 0) != 0)
		printf("rootfs_data absent, recreating\n");

	if (parts->kernel_data && parts->kernel_size > 0 &&
	    ubi_write_volume("kernel", parts->kernel_data, parts->kernel_size, 1) != 0) {
		ret = -1;
		goto restore_config;
	}

	if (parts->rootfs_data && parts->rootfs_size > 0 &&
	    ubi_write_volume("rootfs", parts->rootfs_data, parts->rootfs_size, 1) != 0) {
		ret = -1;
		goto restore_config;
	}

	if (backup_enabled) {
		/* Backup volumes are optional; never fail a good primary update. */
		if (parts->kernel_data && parts->kernel_size > 0 &&
		    ubi_write_volume("kernel_1", parts->kernel_data, parts->kernel_size, 0) != 0)
			printf("kernel_1 not updated\n");
		if (parts->rootfs_data && parts->rootfs_size > 0 &&
		    ubi_write_volume("rootfs_1", parts->rootfs_data, parts->rootfs_size, 0) != 0)
			printf("rootfs_1 not updated\n");
	}

restore_config:
	/*
	 * The user chose to clear configuration on upgrade. Recreate the
	 * volume on every exit path so a failure does not leave it missing.
	 * A full UBI may reject the create; the new system can recreate it
	 * on first boot, so this is a warning rather than an upgrade failure.
	 */
	if (run_command("ubi create rootfs_data", 0) != 0)
		printf("rootfs_data recreate failed (will be created on boot)\n");

	return ret;
}
#endif