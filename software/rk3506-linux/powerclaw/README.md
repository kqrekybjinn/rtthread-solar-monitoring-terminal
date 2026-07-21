# PowerClaw MVP

PowerClaw is the RK3506 edge-agent distribution for the solar-energy terminal.
This first version is based on ZeroClaw 0.8.3 and keeps its Agent Runtime,
Feishu/Lark long-connection channel, model providers, memory, and approval flow.

The device boundary is intentionally narrower than upstream:

```text
Feishu -> PowerClaw Agent Runtime -> power_device MCP -> device-core -> Linux/RT-Thread
```

The PowerClaw agent receives only four MCP tools:

- `power_device__system_status`
- `power_device__mqtt_status`
- `power_device__amp_status`
- `power_device__capabilities`

The MCP process has no shell execution and accepts no host, URL, path, RPMsg,
GPIO, or device-node arguments. Its only network destination is the compiled-in
`127.0.0.1:18080` device-core service. Version 1 exposes no write tool.

## Build

Ubuntu/WSL prerequisites:

```sh
sudo apt-get install gcc-arm-linux-gnueabihf make python3 curl
make test
make arm
make package
```

`make package` downloads and verifies the pinned upstream ARMv7 archive, then
creates `out/powerclaw-rk3506-v0.1.0.tar.gz`.

## Board layout

```text
/userdata/powerclaw/
  bin/powerclaw-agent
  bin/powerclaw-device-mcp
  config/config.toml
  home/.zeroclaw/config.toml -> ../../config/config.toml
  workspace/SOUL.md
  logs/
  powerclaw.env              # board-only, mode 0600
```

Install the package and init script with `scripts/deploy-board.py`. The service
does not start unless `/userdata/powerclaw/powerclaw.env` contains
`POWERCLAW_ENABLED=1` and the required model/Feishu secrets.

## Feishu acceptance setup

1. Create an internal application in the Feishu Open Platform and enable its
   bot capability.
2. Select long-connection event delivery. Subscribe to
   `im.message.receive_v1`; add `card.action.trigger` when approval cards are
   needed. Grant the bot only the message receive/send and message-resource
   permissions required by those functions, then publish the app to the test
   tenant.
3. Add approved Feishu user `open_id` values to
   `peer_groups.feishu_users.external_peers` in the board config. The public
   template intentionally uses an empty list, which denies every inbound user.
   A temporary `"*"` may be used only during controlled first-user enrollment
   and must be replaced immediately with the observed `open_id`.
4. On the board, fill the model key, Feishu `app_id`, and `app_secret` in
   `/userdata/powerclaw/powerclaw.env`. Keep the file at mode `0600`.
5. Set `POWERCLAW_ENABLED=1`, run `/etc/init.d/S97powerclaw restart`, and check
   `/userdata/powerclaw/logs/powerclaw.log` for the Lark WebSocket connection
   and `power_device` MCP registration.

The public template uses an OpenAI-compatible DeepSeek endpoint. A different
model service requires changing the non-secret provider URI/model fields in
`config/config.toml`; its key still belongs only in the board environment file.
Before credentials are available, keep the service disabled and use
`powerclaw-agent self-test --quick` for offline acceptance.

## Security boundary

- Gateway binds loopback only.
- Feishu uses the outbound WebSocket mode, so no public callback port is needed.
- Shell, process, browser, generic HTTP, filesystem mutation, scheduler, plugins,
  native peripherals, GPIO, I2C, and SPI are disabled or excluded.
- The model cannot choose the device-core URL or action name.
- device-core independently applies its own deny-by-default policy and audit log.
- Credentials are supplied by a mode-0600 board environment file and never enter
  the repository, package, prompt, or audit record.

## Current acceptance boundary

The ARM runtime and local device tools can be verified without credentials.
Live Feishu login, attachment ingestion, model response, and approval-card flow
require the user's Feishu application and model credentials and are a separate
hardware acceptance step.
