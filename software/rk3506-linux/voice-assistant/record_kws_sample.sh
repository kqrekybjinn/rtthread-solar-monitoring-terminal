#!/bin/sh
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
. "$DIR/config.env"

SECONDS_ARG="${1:-4}"
OUT="${2:-$VOICE_DIR/wake_sample.wav}"

echo "请在接下来的 ${SECONDS_ARG} 秒内说：小电小电小电"
"$DIR/record_once.sh" "$SECONDS_ARG" "$OUT" >/dev/null
echo "sample=$OUT"
echo "开始本地 KWS 检测..."

if "$DIR/test_kws_sample.sh" "$OUT" "$KWS_THRESHOLD" "$KWS_SCORE"; then
    exit 0
fi

echo "默认阈值未命中，尝试更低阈值..."
for th in 0.30 0.25 0.20 0.15 0.10; do
    if "$DIR/test_kws_sample.sh" "$OUT" "$th" "$KWS_SCORE"; then
        echo "建议把 KWS_THRESHOLD 调成 $th"
        exit 0
    fi
done

echo "所有 KWS 阈值都未命中。下一步应使用该样本做模板匹配唤醒。"
exit 1
