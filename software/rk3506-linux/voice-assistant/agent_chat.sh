#!/bin/sh
set -eu

INPUT="${1:?usage: agent_chat.sh user_text.txt}"
AGENT_ROOT="${AGENT_ROOT:-/userdata/agent}"
AGENT_REPLY="${AGENT_REPLY:-$AGENT_ROOT/agent_reply.sh}"

mkdir -p "$AGENT_ROOT"
cp "$INPUT" "$AGENT_ROOT/input.txt"
"$AGENT_REPLY" >/dev/null
cat "$AGENT_ROOT/reply.txt"
