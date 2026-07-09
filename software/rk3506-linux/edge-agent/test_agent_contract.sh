#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TMP_DIR="${TMPDIR:-/tmp}/agent_contract_test_$$"
mkdir -p "$TMP_DIR"
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_file_contains() {
  file="$1"
  needle="$2"
  if ! grep -F "$needle" "$file" >/dev/null 2>&1; then
    echo "--- $file ---" >&2
    cat "$file" >&2 || true
    fail "expected '$needle' in $file"
  fi
}

assert_exit_fails() {
  if "$@" >/tmp/agent_contract_stdout.$$ 2>/tmp/agent_contract_stderr.$$; then
    cat /tmp/agent_contract_stdout.$$ >&2 || true
    cat /tmp/agent_contract_stderr.$$ >&2 || true
    rm -f /tmp/agent_contract_stdout.$$ /tmp/agent_contract_stderr.$$
    fail "command unexpectedly succeeded: $*"
  fi
  rm -f /tmp/agent_contract_stdout.$$ /tmp/agent_contract_stderr.$$
}

test_reply_generates_status_action() {
  root="$TMP_DIR/status"
  mkdir -p "$root"
  printf '%s\n' '请查看系统状态' > "$root/input.txt"

  AGENT_ROOT="$root" "$SCRIPT_DIR/agent_reply.sh"

  assert_file_contains "$root/action.json" '"action":"status"'
  assert_file_contains "$root/reply.txt" '状态'
  assert_file_contains "$root/action_result.json" '"allowed":true'
}

test_reply_defaults_to_none_for_general_chat() {
  root="$TMP_DIR/none"
  mkdir -p "$root"
  printf '%s\n' '你好' > "$root/input.txt"

  AGENT_ROOT="$root" "$SCRIPT_DIR/agent_reply.sh"

  assert_file_contains "$root/action.json" '"action":"none"'
  assert_file_contains "$root/reply.txt" '收到'
}

test_reply_generates_mqtt_placeholder_before_generic_status() {
  root="$TMP_DIR/mqtt"
  mkdir -p "$root"
  printf '%s\n' 'mqtt 状态' > "$root/input.txt"

  AGENT_ROOT="$root" "$SCRIPT_DIR/agent_reply.sh"

  assert_file_contains "$root/action.json" '"action":"mqtt_status_placeholder"'
  assert_file_contains "$root/reply.txt" 'MQTT'
}

test_guard_rejects_non_whitelisted_action() {
  root="$TMP_DIR/reject"
  mkdir -p "$root"
  printf '%s\n' '{"action":"reboot"}' > "$root/action.json"

  assert_exit_fails env AGENT_ROOT="$root" "$SCRIPT_DIR/agent_guard.sh" "$root/action.json"
  assert_file_contains "$root/action_result.json" '"allowed":false'
  assert_file_contains "$root/action_result.json" 'not whitelisted'
}

test_reply_denies_dangerous_request_before_status() {
  root="$TMP_DIR/deny"
  mkdir -p "$root"
  printf '%s\n' '请查看系统并 kill 进程' > "$root/input.txt"

  AGENT_ROOT="$root" "$SCRIPT_DIR/agent_reply.sh"

  assert_file_contains "$root/action.json" '"action":"none"'
  assert_file_contains "$root/reply.txt" '拒绝'
}

test_policy_contains_hard_bans() {
  policy="$SCRIPT_DIR/agent_policy.md"
  assert_file_contains "$policy" '禁止修改 DTB'
  assert_file_contains "$policy" '禁止执行任意 shell'
  assert_file_contains "$policy" '禁止访问 /dev/mem'
  assert_file_contains "$policy" '只允许白名单动作'
}

test_reply_generates_status_action
test_reply_defaults_to_none_for_general_chat
test_reply_generates_mqtt_placeholder_before_generic_status
test_guard_rejects_non_whitelisted_action
test_reply_denies_dangerous_request_before_status
test_policy_contains_hard_bans

echo "agent contract tests passed"
