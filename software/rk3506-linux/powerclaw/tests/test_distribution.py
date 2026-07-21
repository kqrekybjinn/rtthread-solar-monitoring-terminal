#!/usr/bin/env python3
import pathlib
import re
import subprocess
import sys


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def section(text, name, array=False):
    brackets = r"\[\[" if array else r"\["
    closing = r"\]\]" if array else r"\]"
    match = re.search(
        rf"(?ms)^{brackets}{re.escape(name)}{closing}\s*$\n(.*?)(?=^\[|\Z)", text
    )
    check(match is not None, f"missing section {name}")
    return match.group(1)


def scalar(body, key):
    match = re.search(rf'(?m)^{re.escape(key)}\s*=\s*(.+?)\s*$', body)
    check(match is not None, f"missing key {key}")
    value = match.group(1).strip()
    if value in ("true", "false"):
        return value == "true"
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    return value


def string_array(body, key):
    match = re.search(rf'(?ms)^{re.escape(key)}\s*=\s*\[(.*?)\]', body)
    check(match is not None, f"missing array {key}")
    return re.findall(r'"([^"\\]*)"', match.group(1))


def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    config_path = root / "config" / "config.toml"
    config_text = config_path.read_text(encoding="ascii")

    check(re.search(r'(?m)^schema_version\s*=\s*3\s*$', config_text) is not None,
          "configuration must use schema v3")
    gateway = section(config_text, "gateway")
    lark = section(config_text, "channels.lark.main")
    hardware = section(config_text, "hardware")
    peripherals = section(config_text, "peripherals")
    http_request = section(config_text, "http_request")
    browser = section(config_text, "browser")
    plugins = section(config_text, "plugins")
    memory = section(config_text, "memory")
    check(scalar(gateway, "host") == "127.0.0.1", "gateway is not loopback-only")
    check(scalar(gateway, "allow_public_bind") is False, "public gateway bind enabled")
    check(scalar(lark, "enabled") is False,
          "Feishu must be disabled in the public template")
    check(scalar(lark, "receive_mode") == "websocket",
          "Feishu must use outbound WebSocket mode")
    check(scalar(hardware, "enabled") is False, "native hardware is enabled")
    check(scalar(peripherals, "enabled") is False, "native peripherals are enabled")
    check(scalar(http_request, "enabled") is False, "generic HTTP is enabled")
    check(scalar(browser, "enabled") is False, "browser is enabled")
    check(scalar(plugins, "enabled") is False, "plugins are enabled")
    check(scalar(memory, "embedding_provider") == "none",
          "edge configuration must not call an embedding provider")
    check(scalar(memory, "search_mode") == "bm25",
          "edge memory must use keyword search without embeddings")
    check(re.search(r'(?m)^\[cron\]\s*$', config_text) is None,
          "cron is an alias map; disable scheduling through [scheduler]")

    risk = section(config_text, "risk_profiles.power_readonly")
    expected_tools = {
        "power_device__system_status",
        "power_device__mqtt_status",
        "power_device__amp_status",
        "power_device__capabilities",
    }
    check(scalar(risk, "level") == "readonly", "agent is not read-only")
    check(set(string_array(risk, "allowed_tools")) == expected_tools,
          "allowed tool set changed")
    check(set(string_array(risk, "auto_approve")) == expected_tools,
          "auto-approved tool set changed")
    excluded = string_array(risk, "excluded_tools")
    for dangerous in ("shell", "process", "file_write", "http_request", "i2c", "spi"):
        check(dangerous in excluded, f"{dangerous} is not explicitly excluded")

    server = section(config_text, "mcp.servers", array=True)
    bundle = section(config_text, "mcp_bundles.device_core")
    check(scalar(server, "name") == "power_device", "unexpected MCP server")
    check(scalar(server, "command") == "/userdata/powerclaw/bin/powerclaw-device-mcp",
          "MCP command path changed")
    check(string_array(bundle, "servers") == ["power_device"],
          "agent MCP bundle changed")

    check("cli_" not in config_text and "sk-" not in config_text,
          "credential-like content present")
    provider = section(config_text, "providers.models.custom.power")
    check(scalar(provider, "api_key") == "",
          "model API key is committed")
    check(scalar(lark, "app_secret") == "",
          "Feishu secret is committed")

    for script in ("S97powerclaw", "scripts/fetch-zeroclaw.sh",
                   "scripts/package-release.sh", "scripts/install-board.sh"):
        subprocess.run(["sh", "-n", str(root / script)], check=True)
    init_text = (root / "S97powerclaw").read_text(encoding="ascii")
    check("stat -c" not in init_text,
          "init script depends on GNU stat, which is absent on the board")
    check('"-rw-------"' in init_text,
          "init script must enforce mode 0600 for the credential file")
    print("powerclaw distribution tests: PASS")


if __name__ == "__main__":
    main()
