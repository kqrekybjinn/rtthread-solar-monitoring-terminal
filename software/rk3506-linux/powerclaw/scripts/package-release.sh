#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=0.1.0
STAGE="$ROOT/out/package/powerclaw"
UPSTREAM="$ROOT/vendor/zeroclaw-0.8.3-armv7/zeroclaw"
ARCHIVE="$ROOT/out/powerclaw-rk3506-v$VERSION.tar.gz"

[ -x "$UPSTREAM" ] || "$ROOT/scripts/fetch-zeroclaw.sh" >/dev/null
[ -x "$ROOT/out/powerclaw-device-mcp.arm" ] || {
    echo "Run make arm before packaging" >&2
    exit 1
}

rm -rf "$ROOT/out/package"
mkdir -p "$STAGE/bin" "$STAGE/config" "$STAGE/workspace" "$STAGE/logs"
cp "$UPSTREAM" "$STAGE/bin/powerclaw-agent"
cp "$ROOT/out/powerclaw-device-mcp.arm" "$STAGE/bin/powerclaw-device-mcp"
cp "$ROOT/config/config.toml" "$STAGE/config/config.toml.dist"
cp "$ROOT/config/powerclaw.env.example" "$STAGE/powerclaw.env.example"
cp "$ROOT/workspace/SOUL.md" "$STAGE/workspace/SOUL.md"
cp "$ROOT/S97powerclaw" "$STAGE/S97powerclaw"
cp "$ROOT/scripts/install-board.sh" "$STAGE/install-board.sh"
cp "$ROOT/README.md" "$ROOT/UPSTREAM.md" "$STAGE/"

curl -fsSL "https://raw.githubusercontent.com/zeroclaw-labs/zeroclaw/v0.8.3/LICENSE-APACHE" \
    -o "$STAGE/LICENSE-APACHE"
curl -fsSL "https://raw.githubusercontent.com/zeroclaw-labs/zeroclaw/v0.8.3/LICENSE-MIT" \
    -o "$STAGE/LICENSE-MIT"
curl -fsSL "https://raw.githubusercontent.com/zeroclaw-labs/zeroclaw/v0.8.3/NOTICE" \
    -o "$STAGE/NOTICE-ZEROCLAW"
chmod 0755 "$STAGE/bin/"* "$STAGE/S97powerclaw" "$STAGE/install-board.sh"
tar -czf "$ARCHIVE" -C "$ROOT/out/package" powerclaw
sha256sum "$ARCHIVE" >"$ARCHIVE.sha256"
printf '%s\n' "$ARCHIVE"
