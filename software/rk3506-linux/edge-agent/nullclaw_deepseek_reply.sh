#!/bin/sh
set -eu

AGENT_ROOT="${AGENT_ROOT:-/userdata/agent}"
INPUT_FILE="$AGENT_ROOT/input.txt"
POLICY_FILE="$AGENT_ROOT/agent_policy.md"
PROMPT_FILE="$AGENT_ROOT/deepseek_prompt.txt"
RAW_FILE="$AGENT_ROOT/deepseek_raw.txt"
BIN="${DEEPSEEK_CHAT_BIN:-$AGENT_ROOT/deepseek_chat}"
CONFIG="${DEEPSEEK_CONFIG:-$AGENT_ROOT/deepseek.env}"

mkdir -p "$AGENT_ROOT"

USER_TEXT=""
if [ -r "$INPUT_FILE" ]; then
  USER_TEXT="$(tr -d '\r' < "$INPUT_FILE" | head -c 512)"
fi

POLICY_TEXT=""
if [ -r "$POLICY_FILE" ]; then
  POLICY_TEXT="$(sed -n '1,180p' "$POLICY_FILE")"
fi

cat >"$PROMPT_FILE" <<EOF
你是 RK3506 板端受限智能体。你的任务是把用户语音识别文本转换为“回复文字”和“白名单动作”。

必须严格遵守以下策略：
$POLICY_TEXT

只允许输出一行 JSON，不要 Markdown，不要解释，不要代码块。
JSON schema:
{"reply":"给用户朗读的中文短回复","action":"status|mqtt_status_placeholder|audio_test_placeholder|none"}

动作选择：
- 用户请求系统/设备状态：status
- 用户请求 MQTT 状态：mqtt_status_placeholder
- 用户请求音频/喇叭/麦克风测试：audio_test_placeholder
- 闲聊、问候、无法确定、危险请求：none

用户文本：
$USER_TEXT
EOF

"$BIN" "$PROMPT_FILE" "$CONFIG" >"$RAW_FILE"
cat "$RAW_FILE"
