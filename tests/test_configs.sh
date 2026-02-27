#!/bin/bash

set -u

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOCKET_PATH="/tmp/taskmaster.sock"
DAEMON_PID=""

pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    if [ -f "$ROOT_DIR/error_output.txt" ]; then
        echo "--- daemon log (stderr) ---"
        cat "$ROOT_DIR/error_output.txt"
    fi
    cleanup
    exit 1
}

cleanup() {
    if [ -n "${DAEMON_PID}" ] && kill -0 "${DAEMON_PID}" 2>/dev/null; then
        "$ROOT_DIR/taskmasterctl" shutdown >/dev/null 2>&1 || true
        wait "${DAEMON_PID}" 2>/dev/null || true
    fi
    rm -f "$SOCKET_PATH"
}

wait_for_daemon() {
    local i
    for i in $(seq 1 100); do
        if [ -S "$SOCKET_PATH" ] && "$ROOT_DIR/taskmasterctl" status >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

start_daemon() {
    local cfg="$1"
    rm -f "$ROOT_DIR/error_output.txt"
    "$ROOT_DIR/taskmasterd" "$cfg" 2> "$ROOT_DIR/error_output.txt" &
    DAEMON_PID=$!
    if ! wait_for_daemon; then
        fail "Daemon did not become ready for config: $cfg"
    fi
}

stop_daemon() {
    "$ROOT_DIR/taskmasterctl" shutdown >/dev/null 2>&1 || true
    wait "${DAEMON_PID}" 2>/dev/null || true
    DAEMON_PID=""
    rm -f "$SOCKET_PATH"
}

assert_grep() {
    local pattern="$1"
    local file="$2"
    local msg="$3"
    if grep -Eq "$pattern" "$file"; then
        pass "$msg"
    else
        fail "$msg (pattern '$pattern' missing in $file)"
    fi
}

assert_not_grep() {
    local pattern="$1"
    local file="$2"
    local msg="$3"
    if grep -Eq "$pattern" "$file"; then
        fail "$msg (unexpected pattern '$pattern' in $file)"
    else
        pass "$msg"
    fi
}

build_helpers() {
    cat > "$ROOT_DIR/tests/helper_emit.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

int main(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("cwd=%s\n", cwd);
    }

    const char *v1 = getenv("TEST_ENV_ONE");
    const char *v2 = getenv("TEST_ENV_TWO");
    printf("TEST_ENV_ONE=%s\n", v1 ? v1 : "");
    printf("TEST_ENV_TWO=%s\n", v2 ? v2 : "");
    printf("uid=%d\n", getuid());
    printf("stdout-line\n");
    fprintf(stderr, "stderr-line\n");

    int fd = open("umask_probe_file", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, "ok\n", 3);
        close(fd);
    }

    return 0;
}
EOF

    gcc "$ROOT_DIR/tests/helper_emit.c" -o "$ROOT_DIR/tests/helper_emit"
    gcc "$ROOT_DIR/tests/exit42.c" -o "$ROOT_DIR/tests/exit42"
}

test_parser_and_application() {
    local test_dir="$ROOT_DIR/tests/tmp_cfg_probe"
    rm -rf "$test_dir"
    mkdir -p "$test_dir"

    cat > "$ROOT_DIR/tests/tmp_cfg_probe.yaml" <<EOF
programs:
  cfg_probe:
    cmd: "$ROOT_DIR/tests/helper_emit"
    autostart: true
    autorestart: never
    workingdir: "$test_dir"
    umask: 027
    stdout: "$test_dir/probe.stdout"
    stderr: "$test_dir/probe.stderr"
    user: "$(id -un)"
    env:
      TEST_ENV_ONE: "alpha"
      TEST_ENV_TWO: 42
EOF

    start_daemon "$ROOT_DIR/tests/tmp_cfg_probe.yaml"
    sleep 1
    stop_daemon

    assert_grep "Started process cfg_probe\\[0\\]" "$ROOT_DIR/error_output.txt" "cmd + autostart applied"
    assert_grep "cwd=$test_dir" "$test_dir/probe.stdout" "workingdir applied"
    assert_grep "TEST_ENV_ONE=alpha" "$test_dir/probe.stdout" "env TEST_ENV_ONE applied"
    assert_grep "TEST_ENV_TWO=42" "$test_dir/probe.stdout" "env TEST_ENV_TWO applied"
    assert_grep "stdout-line" "$test_dir/probe.stdout" "stdout redirection applied"
    assert_grep "stderr-line" "$test_dir/probe.stderr" "stderr redirection applied"

    local perms
    perms="$(stat -c "%a" "$test_dir/umask_probe_file" 2>/dev/null || true)"
    if [ "$perms" = "640" ]; then
        pass "umask applied (expected 640, got $perms)"
    else
        fail "umask applied (expected 640, got ${perms:-missing})"
    fi

    assert_not_grep "unknown property 'stoptime'" "$ROOT_DIR/error_output.txt" "known keys are recognized by parser"
}

test_numprocs_autostart_and_stopsignal() {
    cat > "$ROOT_DIR/tests/tmp_multi.yaml" <<EOF
programs:
  multi:
    cmd: "/bin/sleep 30"
    numprocs: 2
    autostart: false
    stopsignal: INT
    stoptime: 5
EOF

    start_daemon "$ROOT_DIR/tests/tmp_multi.yaml"

    local status_out
    status_out="$("$ROOT_DIR/taskmasterctl" status)"
    if [ "$(echo "$status_out" | grep -c '^multi')" -eq 2 ]; then
        pass "numprocs parsed (2 entries in status)"
    else
        fail "numprocs parsed (expected 2 entries in status)"
    fi
    assert_grep "multi[[:space:]]+0[[:space:]]+STOPPED" <(echo "$status_out") "autostart=false leaves process stopped"

    "$ROOT_DIR/taskmasterctl" start multi >/dev/null
    sleep 1
    assert_grep "Started process multi\\[0\\]" "$ROOT_DIR/error_output.txt" "start command starts instance 0"
    assert_grep "Started process multi\\[1\\]" "$ROOT_DIR/error_output.txt" "start command starts instance 1"

    "$ROOT_DIR/taskmasterctl" stop multi >/dev/null
    sleep 1
    assert_grep "with signal 2" "$ROOT_DIR/error_output.txt" "stopsignal INT applied"
    stop_daemon
}

test_starttime_transition() {
    cat > "$ROOT_DIR/tests/tmp_starttime.yaml" <<EOF
programs:
  slow_start:
    cmd: "/bin/sleep 30"
    autostart: true
    starttime: 5
EOF

    start_daemon "$ROOT_DIR/tests/tmp_starttime.yaml"

    local status_early status_late
    status_early="$("$ROOT_DIR/taskmasterctl" status)"
    assert_grep "slow_start[[:space:]]+0[[:space:]]+STARTING" <(echo "$status_early") "starttime keeps process in STARTING initially"

    sleep 6
    status_late="$("$ROOT_DIR/taskmasterctl" status)"
    assert_grep "slow_start[[:space:]]+0[[:space:]]+RUNNING" <(echo "$status_late") "starttime transition to RUNNING occurs"
    stop_daemon
}

test_restart_policy_always_and_retries() {
    cat > "$ROOT_DIR/tests/tmp_restart.yaml" <<EOF
programs:
  restart_always:
    cmd: "$ROOT_DIR/tests/exit42"
    autostart: true
    autorestart: always
    starttime: 5
    startretries: 2
EOF

    start_daemon "$ROOT_DIR/tests/tmp_restart.yaml"
    sleep 2
    stop_daemon

    local restarts
    restarts="$(grep -c "Restarting process restart_always" "$ROOT_DIR/error_output.txt" || true)"
    if [ "$restarts" -eq 2 ]; then
        pass "autorestart=always + startretries=2 applied (2 restart attempts)"
    else
        fail "autorestart=always + startretries=2 applied (expected 2 restarts, got $restarts)"
    fi
    assert_grep "failed to start after 2 retries" "$ROOT_DIR/error_output.txt" "startretries exhaustion handled"
}

test_exitcodes_unexpected_policy() {
    if ! timeout 25s bash "$ROOT_DIR/tests/test_exitcodes.sh" >/tmp/test_exitcodes.out 2>&1; then
        cat /tmp/test_exitcodes.out
        fail "exitcodes + autorestart=unexpected regression test"
    fi
    pass "exitcodes + autorestart=unexpected regression test"
}

final_cleanup() {
    rm -f "$ROOT_DIR/tests/helper_emit.c" \
          "$ROOT_DIR/tests/helper_emit" \
          "$ROOT_DIR/tests/tmp_cfg_probe.yaml" \
          "$ROOT_DIR/tests/tmp_multi.yaml" \
          "$ROOT_DIR/tests/tmp_starttime.yaml" \
          "$ROOT_DIR/tests/tmp_restart.yaml" \
          /tmp/test_exitcodes.out
    rm -rf "$ROOT_DIR/tests/tmp_cfg_probe"
}

echo "Running comprehensive config parsing/application tests..."
build_helpers

test_parser_and_application
test_numprocs_autostart_and_stopsignal
test_starttime_transition
test_restart_policy_always_and_retries
test_exitcodes_unexpected_policy

final_cleanup
echo -e "${GREEN}All config parsing/application tests passed!${NC}"
