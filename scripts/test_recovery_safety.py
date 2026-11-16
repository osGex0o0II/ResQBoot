#!/usr/bin/env python3
"""Regression checks for Web recovery access, content and command parsing."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


terminal = source("common/webterm.c")
httpd = source("net/httpd.c")
dhcp = source("net/dhcpd.c")
env = source("failsafe/failsafe_httpd.c")
about = source("failsafe/html/pig/about.html")
upload = source("failsafe/html/pig/submit.js")
doc = source("docs/recovery.md")

require("static int webterm_command_enabled = 0;" in terminal, "Web terminal must be locked by default")
require("webterm_set_command_enabled(allow_terminal)" in httpd and '"t"' in httpd, "Serial opt-in missing")
require("webterm_post_is_from_ui(data, data_len)" in terminal, "Command POST anti-CSRF check missing")
require('strcmp(cmd, "reset") == 0' in terminal, "Limited locked-mode reboot path missing")
require("webterm_set_command_enabled(0)" in httpd, "Terminal lock reset on stop missing")

network_section = httpd.split("static void httpd_set_recovery_network(", 1)[1].split("\n}\n", 1)[0]
require("setenv(" not in network_section and "net_httpd_ip = httpd_ip" in network_section,
        "HTTP recovery must not mutate persistent network variables")
require("if (net_httpd_ip.s_addr)" in dhcp, "DHCP must honor runtime recovery IP")
require("ret = saveenv();" in env and "setenv(name, saved);" in env and "persist failed" in env,
        "Environment save failure handling missing")

require("isBackupPartition(" not in about and "sessionStorage.getItem('flash_backup')" in about,
        "Unsafe inferred backup partition detection remains")
require("sessionStorage.getItem('flash_backup')" in upload and "每次" not in upload or
        "getFlashBackupPref()" in upload, "Manual backup opt-in mismatch")
require("getFlashBackupPref() === '1'" in upload and "confirm(" in upload,
        "Backup write must require per-upload confirmation")
require("FW_TYPE_FIT" in httpd, "FIT-only firmware check missing")
selector = source("failsafe/html/pig/update.html")
require(".zImage" not in selector and ".uImage" not in selector,
        "Unsupported FIT file picker formats still advertised")
require("httpd t" in doc and "Web 命令终端" in doc,
        "Serial opt-in must be documented")

# Parse each inline script and external JS module with the CI-provided Node.
require(shutil.which("node") is not None, "Node.js is required for browser syntax checks")
with tempfile.TemporaryDirectory(prefix="resqboot-ui-") as directory:
    script_path = Path(directory) / "ui-check.js"
    browser_files = (
        "failsafe/html/pig/index.html",
        "failsafe/html/pig/update.html",
        "failsafe/html/pig/img.html",
        "failsafe/html/pig/backup.html",
        "failsafe/html/pig/term.html",
        "failsafe/html/pig/about.html",
    )
    checked = 0
    for file in browser_files:
        for body in re.findall(r"<script\b[^>]*>(.*?)</script\s*>", source(file),
                               re.IGNORECASE | re.DOTALL):
            if not body.strip():
                continue
            script_path.write_text(body, encoding="utf-8")
            subprocess.run(["node", "--check", str(script_path)], check=True)
            checked += 1
    for file in ("failsafe/html/pig/common.js", "failsafe/html/pig/submit.js"):
        subprocess.run(["node", "--check", str(ROOT / file)], check=True)
        checked += 1
    require(checked >= 9, "Too few browser scripts checked")

# Compile and exercise the exact C HTTP header parser, including casing and
# a fake header after the request-body separator.
start = terminal.index("static int webterm_header_matches(")
end = terminal.index("void webterm_http_handler(", start)
parser = terminal[start:end]
test_c = (
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    + parser
    + r"""
int main(void) {
    const char *requests[] = {
        "POST /webterm/cmd HTTP/1.1\r\nX-ResQBoot-Request: 1\r\n\r\nreset",
        "POST /webterm/cmd HTTP/1.1\r\nx-resqboot-request: 1\r\n\r\nreset",
        "POST /webterm/cmd HTTP/1.1\r\nContent-Type: text/plain\r\n\r\n"
        "X-ResQBoot-Request: 1\r\nreset",
        "POST /webterm/cmd HTTP/1.1\r\nX-ResQBoot-Request: 0\r\n\r\nreset",
        "POST /webterm/cmd HTTP/1.1\r\n\r\nreset",
    };
    const int expected[] = {1, 1, 0, 0, 0};
    int i;

    for (i = 0; i < 5; ++i) {
        if (webterm_post_is_from_ui(requests[i], strlen(requests[i])) != expected[i]) {
            printf("Request header regression in case %d\n", i);
            return 1;
        }
    }
    return 0;
}
"""
)
with tempfile.TemporaryDirectory(prefix="resqboot-http-") as directory:
    test_file = Path(directory) / "test.c"
    executable = Path(directory) / "test"
    test_file.write_text(test_c, encoding="utf-8")
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(test_file),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)

print("Recovery security and browser checks passed")
