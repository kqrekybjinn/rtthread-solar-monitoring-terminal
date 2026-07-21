#!/bin/sh
set -eu

SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TARGET=/userdata/powerclaw

mkdir -p "$TARGET/bin" "$TARGET/config" "$TARGET/workspace" "$TARGET/logs" "$TARGET/home"
chmod 700 "$TARGET" "$TARGET/config" "$TARGET/logs" "$TARGET/home"
install -m 0755 "$SOURCE/bin/powerclaw-agent" "$TARGET/bin/powerclaw-agent"
install -m 0755 "$SOURCE/bin/powerclaw-device-mcp" "$TARGET/bin/powerclaw-device-mcp"
install -m 0644 "$SOURCE/config/config.toml.dist" "$TARGET/config/config.toml.dist"
if [ ! -f "$TARGET/config/config.toml" ]; then
    install -m 0600 "$SOURCE/config/config.toml.dist" "$TARGET/config/config.toml"
fi
install -m 0644 "$SOURCE/workspace/SOUL.md" "$TARGET/workspace/SOUL.md"
install -m 0644 "$SOURCE/LICENSE-APACHE" "$TARGET/LICENSE-APACHE"
install -m 0644 "$SOURCE/LICENSE-MIT" "$TARGET/LICENSE-MIT"
install -m 0644 "$SOURCE/NOTICE-ZEROCLAW" "$TARGET/NOTICE-ZEROCLAW"
if [ ! -f "$TARGET/powerclaw.env" ]; then
    install -m 0600 "$SOURCE/powerclaw.env.example" "$TARGET/powerclaw.env"
fi
install -m 0755 "$SOURCE/S97powerclaw" /etc/init.d/S97powerclaw

mkdir -p "$TARGET/home/.zeroclaw"
chmod 700 "$TARGET/home/.zeroclaw"
ln -sf ../../config/config.toml "$TARGET/home/.zeroclaw/config.toml"
echo "PowerClaw installed disabled. Configure $TARGET/powerclaw.env, then set POWERCLAW_ENABLED=1."
