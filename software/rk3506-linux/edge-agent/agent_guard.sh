#!/bin/sh
set -eu

AGENT_ROOT="${AGENT_ROOT:-/userdata/agent}"
ACTION_FILE="${1:-$AGENT_ROOT/action.json}"
RESULT_FILE="$AGENT_ROOT/action_result.json"

mkdir -p "$AGENT_ROOT"

json_escape() {
  printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

write_result() {
  allowed="$1"
  action="$2"
  message="$3"
  reason="$4"
  printf '{"allowed":%s,"action":"%s","message":"%s","reason":"%s"}\n' \
    "$allowed" \
    "$(json_escape "$action")" \
    "$(json_escape "$message")" \
    "$(json_escape "$reason")" > "$RESULT_FILE"
}

extract_action() {
  if [ -f "$ACTION_FILE" ]; then
    sed -n 's/.*"action"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ACTION_FILE" | head -n 1
  else
    printf '%s' "$ACTION_FILE"
  fi
}

action="$(extract_action)"
if [ -z "$action" ]; then
  write_result false "" "拒绝：动作为空。" "empty action"
  exit 2
fi

case "$action" in
  status|mqtt_status_placeholder|audio_test_placeholder|none)
    ;;
  *)
    write_result false "$action" "拒绝：动作不在白名单内。" "not whitelisted"
    exit 2
    ;;
esac

case "$action" in
  status)
    uptime_value="unknown"
    if [ -r /proc/uptime ]; then
      set -- $(cat /proc/uptime)
      uptime_value="${1:-unknown}"
    fi
    load_value="unknown"
    if [ -r /proc/loadavg ]; then
      set -- $(cat /proc/loadavg)
      load_value="${1:-?},${2:-?},${3:-?}"
    fi
    kernel_value="$(uname -sr 2>/dev/null || printf 'unknown')"
    write_result true "$action" "状态：kernel=$kernel_value uptime=${uptime_value}s load=$load_value" ""
    ;;
  mqtt_status_placeholder)
    write_result true "$action" "MQTT 状态占位：第一版未连接真实 MQTT 控制。" ""
    ;;
  audio_test_placeholder)
    write_result true "$action" "音频测试占位：音频/TTS/ASR 由独立模块处理，本执行器不调用音频设备。" ""
    ;;
  none)
    write_result true "$action" "无动作。" ""
    ;;
esac
