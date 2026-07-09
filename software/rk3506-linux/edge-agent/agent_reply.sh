#!/bin/sh
set -eu

AGENT_ROOT="${AGENT_ROOT:-/userdata/agent}"
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INPUT_FILE="$AGENT_ROOT/input.txt"
REPLY_FILE="$AGENT_ROOT/reply.txt"
ACTION_FILE="$AGENT_ROOT/action.json"
RESULT_FILE="$AGENT_ROOT/action_result.json"
LLM_REPLY="${AGENT_LLM_REPLY:-$AGENT_ROOT/nullclaw_deepseek_reply.sh}"
USE_LLM="${AGENT_USE_LLM:-1}"

mkdir -p "$AGENT_ROOT"

json_escape() {
  printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

write_action() {
  action="$1"
  printf '{"action":"%s","source":"agent_reply.sh","allowed_actions":["status","mqtt_status_placeholder","audio_test_placeholder","none"]}\n' \
    "$(json_escape "$action")" > "$ACTION_FILE"
}

extract_json_field() {
  field="$1"
  file="$2"
  sed -n 's/.*"'"$field"'"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$file" | head -n 1
}

fallback_decide() {
  case "$input_text" in
    "")
      action="none"
      fallback_reply="我没有听清，请再说一遍。"
      ;;
    *DTB*|*dtb*|*网络*|*路由*|*分区*|*密钥*|*密码*|*kill*|*Kill*|*KILL*|*/dev/mem*|*shell*|*Shell*|*SHELL*|*启动项*|*重启*|*reboot*|*Reboot*|*REBOOT*)
      action="none"
      fallback_reply="拒绝：当前板端智能体只允许白名单动作，不修改系统、网络、分区、密钥、启动项，也不执行 shell。"
      ;;
    *MQTT*|*mqtt*)
      action="mqtt_status_placeholder"
      fallback_reply="MQTT 状态功能目前是占位实现。"
      ;;
    *音频*|*喇叭*|*麦克风*|*audio*|*Audio*|*AUDIO*)
      action="audio_test_placeholder"
      fallback_reply="音频测试功能目前是占位实现。"
      ;;
    *状态*|*系统*|*status*|*Status*|*STATUS*)
      action="status"
      fallback_reply="正在查看状态。"
      ;;
    *)
      action="none"
      fallback_reply="收到：我可以和你对话，也可以处理少量白名单设备动作。"
      ;;
  esac
}

if [ -r "$INPUT_FILE" ]; then
  input_text="$(tr -d '\r' < "$INPUT_FILE" | head -c 512)"
else
  input_text=""
fi

fallback_decide

case "$input_text" in
  ""|*DTB*|*dtb*|*网络*|*路由*|*分区*|*密钥*|*密码*|*kill*|*Kill*|*KILL*|*/dev/mem*|*shell*|*Shell*|*SHELL*|*启动项*|*重启*|*reboot*|*Reboot*|*REBOOT*)
    ;;
  *)
    if [ "$USE_LLM" = "1" ] && [ -x "$LLM_REPLY" ]; then
      if llm_json="$("$LLM_REPLY" 2>/tmp/agent_llm_error.log)"; then
        llm_action="$(printf '%s\n' "$llm_json" | sed -n 's/.*"action"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n 1)"
        llm_reply="$(printf '%s\n' "$llm_json" | sed -n 's/.*"reply"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n 1)"
        case "$llm_action" in
          status|mqtt_status_placeholder|audio_test_placeholder|none)
            action="$llm_action"
            [ -n "$llm_reply" ] && fallback_reply="$llm_reply"
            ;;
        esac
      fi
    fi
    ;;
esac

write_action "$action"

if "$SCRIPT_DIR/agent_guard.sh" "$ACTION_FILE"; then
  guard_message="$(extract_json_field message "$RESULT_FILE" 2>/dev/null || true)"
  if [ -n "$guard_message" ] && [ "$action" != "none" ]; then
    printf '%s\n' "$guard_message" > "$REPLY_FILE"
  else
    printf '%s\n' "$fallback_reply" > "$REPLY_FILE"
  fi
else
  printf '%s\n' "拒绝执行：动作未通过白名单检查。" > "$REPLY_FILE"
  exit 2
fi
