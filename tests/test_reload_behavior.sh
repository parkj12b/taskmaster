#!/bin/bash

set -u

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CFG="$ROOT_DIR/tests/tmp_reload.yaml"
LOG="$ROOT_DIR/error_output.txt"
DAEMON_PID=""

pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    [ -f "$LOG" ] && { echo "--- daemon log ---"; cat "$LOG"; }
    cleanup
    exit 1
}

cleanup() {
    "$ROOT_DIR/taskmasterctl" shutdown >/dev/null 2>&1 || true
    if [ -n "$DAEMON_PID" ]; then
        wait "$DAEMON_PID" 2>/dev/null || true
    fi
}

wait_for_daemon() {
    local i
    for i in $(seq 1 100); do
        if "$ROOT_DIR/taskmasterctl" status >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

status_pid() {
    local name="$1"
    "$ROOT_DIR/taskmasterctl" status | awk -v n="$name" '$1 == n && $2 == 0 { print $5 }'
}

is_live_pid() {
    local pid="$1"
    [ -n "$pid" ] && [ "$pid" -gt 0 ]
}

write_cfg_v1() {
cat > "$CFG" <<EOF_CFG
programs:
  stable:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_stable.out"
    stderr: "/tmp/reload_stable.err"

  volatile:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_volatile.out"
    stderr: "/tmp/reload_volatile.err"
EOF_CFG
}

write_cfg_v2() {
cat > "$CFG" <<EOF_CFG
programs:
  stable:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_stable.out"
    stderr: "/tmp/reload_stable.err"

  volatile:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_volatile_changed.out"
    stderr: "/tmp/reload_volatile.err"

  added:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_added.out"
    stderr: "/tmp/reload_added.err"
EOF_CFG
}

write_cfg_v3() {
cat > "$CFG" <<EOF_CFG
programs:
  stable:
    cmd: "$ROOT_DIR/tests/dummy_program.sh"
    autostart: true
    autorestart: never
    starttime: 5
    stdout: "/tmp/reload_stable.out"
    stderr: "/tmp/reload_stable.err"
EOF_CFG
}

echo "Testing reload behavior..."

"$ROOT_DIR/taskmasterctl" shutdown >/dev/null 2>&1 || true

write_cfg_v1
"$ROOT_DIR/taskmasterd" "$CFG" 2> "$LOG" &
DAEMON_PID=$!
wait_for_daemon || fail "daemon not ready"
sleep 1

stable_pid_v1="$(status_pid stable)"
volatile_pid_v1="$(status_pid volatile)"
is_live_pid "$stable_pid_v1" || fail "stable missing before reload"
is_live_pid "$volatile_pid_v1" || fail "volatile missing before reload"
pass "initial processes started"

write_cfg_v2
"$ROOT_DIR/taskmasterctl" reload >/dev/null
sleep 2

stable_pid_v2="$(status_pid stable)"
volatile_pid_v2="$(status_pid volatile)"
added_pid_v2="$(status_pid added)"

[ "$stable_pid_v2" = "$stable_pid_v1" ] || fail "unchanged stable process PID changed across reload ($stable_pid_v1 -> $stable_pid_v2)"
pass "unchanged process preserved across reload"

is_live_pid "$volatile_pid_v2" || fail "changed process missing after reload"
[ "$volatile_pid_v2" != "$volatile_pid_v1" ] || fail "changed process PID did not change across reload"
pass "changed process restarted"

is_live_pid "$added_pid_v2" || fail "added process missing after reload"
pass "added process started"

write_cfg_v3
"$ROOT_DIR/taskmasterctl" reload >/dev/null
sleep 2

stable_pid_v3="$(status_pid stable)"
status_v3="$("$ROOT_DIR/taskmasterctl" status)"

[ "$stable_pid_v3" = "$stable_pid_v2" ] || fail "stable PID changed when only other programs were removed ($stable_pid_v2 -> $stable_pid_v3)"
pass "stable process still preserved when removing others"

echo "$status_v3" | grep -q '^volatile' && fail "volatile still present after removal"
echo "$status_v3" | grep -q '^added' && fail "added still present after removal"
pass "removed programs no longer managed"

cleanup

echo -e "${GREEN}Reload behavior test passed!${NC}"
