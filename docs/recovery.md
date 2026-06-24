# Web Recovery Guide

This document covers the default network access and web recovery workflow.

## Default Network

The IPQ builds use these default U-Boot network values:

| Item | Value |
| --- | --- |
| Device IP | `192.168.1.1` |
| Netmask | `255.255.255.0` |
| Server IP / host side | `192.168.1.2` |
| Web recovery URL | `http://192.168.1.1/` |

Connect your computer directly to the device LAN port. In many cases the built-in DHCP server will assign an address automatically.

If DHCP does not work, set a static address on your computer:

```text
IP address: 192.168.1.2
Netmask:    255.255.255.0
Gateway:    leave empty or use 192.168.1.1
```

Then open:

```text
http://192.168.1.1/
```

## Browser Compatibility

The recovery page is compressed with gzip/zopfli to reduce U-Boot image size and device resource usage.

Use a modern browser:

- Chrome 60+
- Firefox 55+
- Safari 11+
- Edge 15+

Internet Explorer is not supported.

## Recovery Functions

The web recovery page can be used for:

- U-Boot update,
- firmware update,
- CDT update,
- MIBIB update,
- GPT update,
- ART update,
- initramfs boot for debugging and recovery.

Only upload images that match your device and partition layout.

## Environment Overrides

### Reset Key

Use `reset_key=<GPIO_NUM>` to override the reset button GPIO.

TTL console:

```text
setenv reset_key x
saveenv
```

Web environment page:

- variable name: `reset_key`
- variable value: `x`

Replace `x` with the known GPIO number.

### Config Name

Use `config_name=<config@xxx>` to override the device config name for cross-model or firmware testing.

TTL console:

```text
setenv config_name config@xxx
saveenv
```

Web environment page:

- variable name: `config_name`
- variable value: `config@xxx`

### Clear Overrides

Clear an override from the TTL console:

```text
setenv reset_key
saveenv
```

```text
setenv config_name
saveenv
```

Reboot after clearing the override so U-Boot falls back to the default DTB entry.

## Troubleshooting

- Make sure your computer is on the `192.168.1.0/24` network.
- Disable other active network adapters if the browser opens the wrong route.
- Try a direct Ethernet connection instead of a switch.
- Use `http://192.168.1.1/`, not `https://`.
- Try another modern browser if the page does not render.
- Check that you flashed the correct asset for your actual device model.
