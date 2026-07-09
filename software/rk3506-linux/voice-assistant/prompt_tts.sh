#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
PROMPT_DIR="${PROMPT_AUDIO_DIR:-$DIR/prompts}"
FALLBACK_CODE="${PROMPT_FALLBACK_CODE:-done}"

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 text-or-text-file [output.wav]" >&2
    exit 2
fi

INPUT="$1"
OUT="${2:-$DIR/reply.wav}"

if [ -f "$INPUT" ]; then
    TEXT="$(cat "$INPUT" 2>/dev/null || true)"
else
    TEXT="$INPUT"
fi

choose_code() {
    text="$1"
    compact="$(printf '%s' "$text" | tr -d '[:space:]')"

    if [ -z "$compact" ]; then
        printf '%s\n' not_heard
        return
    fi

    case "$compact" in
        *我在*|*小电在*) printf '%s\n' awake ;;
        *没有听清*|*没听清*|*请再说*) printf '%s\n' not_heard ;;
        *拒绝*|*不允许*|*不能执行*|*暂不支持*) printf '%s\n' denied ;;
        *状态：*|*kernel=*|*uptime=*|*load=*) printf '%s\n' status_ok ;;
        *MQTT*|*mqtt*) printf '%s\n' mqtt_status ;;
        *音频*|*喇叭*|*咪头*) printf '%s\n' audio_test ;;
        *网络*已连接*|*联网*) printf '%s\n' network_ok ;;
        *错误*|*失败*|*error*|*Error*) printf '%s\n' error ;;
        *你好*|*介绍*|*能做什么*) printf '%s\n' intro ;;
        *) printf '%s\n' "$FALLBACK_CODE" ;;
    esac
}

CODE="$(choose_code "$TEXT")"
SRC="$PROMPT_DIR/$CODE.wav"
if [ ! -s "$SRC" ]; then
    CODE="$FALLBACK_CODE"
    SRC="$PROMPT_DIR/$CODE.wav"
fi

if [ ! -s "$SRC" ]; then
    echo "prompt_tts: missing prompt wav: $SRC" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUT")"
cp "$SRC" "$OUT"
printf '%s\n' "$CODE" >"$DIR/last_prompt_code.txt"
printf '%s\n' "$OUT"
