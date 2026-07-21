#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=0.8.3
ARCHIVE=zeroclaw-armv7-unknown-linux-gnueabihf.tar.gz
EXPECTED=c86c9a164412828d06134ac92901646631f8c7eee6f2a4de3d14980ec53d346c
URL="https://github.com/zeroclaw-labs/zeroclaw/releases/download/v$VERSION/$ARCHIVE"
VENDOR="$ROOT/vendor/zeroclaw-$VERSION-armv7"

mkdir -p "$VENDOR"
if [ ! -f "$VENDOR/$ARCHIVE" ]; then
    curl -fL --retry 3 --connect-timeout 15 "$URL" -o "$VENDOR/$ARCHIVE.part"
    mv "$VENDOR/$ARCHIVE.part" "$VENDOR/$ARCHIVE"
fi

actual=$(sha256sum "$VENDOR/$ARCHIVE" | awk '{print $1}')
if [ "$actual" != "$EXPECTED" ]; then
    echo "ZeroClaw archive SHA256 mismatch: $actual" >&2
    exit 1
fi

tar -xzf "$VENDOR/$ARCHIVE" -C "$VENDOR" zeroclaw
chmod 0755 "$VENDOR/zeroclaw"
printf '%s\n' "$VENDOR/zeroclaw"
