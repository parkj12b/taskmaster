#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "Starting Taskmaster Modular Tests..."

check_log() {
    if grep -E -q "$1" error_output.txt 2>/dev/null; then
        echo -e "${GREEN}[PASS]${NC} Found in log: $1"
    else
        echo -e "${RED}[FAIL]${NC} Not found in log: $1"
        # cat error_output.txt
        exit 1
    fi
}

rm -f taskmaster.log error_output.txt
killall taskmasterd 2>/dev/null || true

# 1. Basic Startup and Connect
echo "Testing basic startup..."
cat <<EOF > test_basic.yaml
programs:
  ls_test:
    cmd: "ls"
    autostart: true
EOF

./taskmasterd test_basic.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 1
./taskmasterctl status > /dev/null
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null
check_log "Started process ls_test"
check_log "Daemon started"

# 2. Start/Stop commands
echo "Testing start/stop commands..."
cat <<EOF > test_cmd.yaml
programs:
  sleeper:
    cmd: "sleep 100"
    autostart: false
EOF

./taskmasterd test_cmd.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 1
./taskmasterctl start sleeper > /dev/null
sleep 1
./taskmasterctl stop sleeper > /dev/null
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null
check_log "Client requested start: sleeper"
check_log "Client requested stop: sleeper"

# 3. Privilege De-escalation (Self-test)
echo "Testing privilege de-escalation (self-switch)..."
CURRENT_USER=$(id -un)
cat <<EOF > test_priv.yaml
programs:
  priv_test:
    cmd: "whoami"
    user: "$CURRENT_USER"
    autostart: true
    stdout: "whoami_out.txt"
EOF

rm -f whoami_out.txt
./taskmasterd test_priv.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 2
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null

if [ -f whoami_out.txt ] && grep -q "$CURRENT_USER" whoami_out.txt; then
    echo -e "${GREEN}[PASS]${NC} Privilege de-escalation logic worked (ran as $CURRENT_USER)"
else
    echo -e "${RED}[FAIL]${NC} Privilege de-escalation failed"
    exit 1
fi

# 4. Real Privilege De-escalation (Requires sudo)
echo "Testing real privilege de-escalation (root -> nobody)..."
if sudo -n true 2>/dev/null; then
    cat <<EOF > test_root.yaml
programs:
  nobody_test:
    cmd: "whoami"
    user: "nobody"
    autostart: true
    stdout: "nobody_out.txt"
EOF
    rm -f nobody_out.txt
    sudo ./taskmasterd test_root.yaml 2> error_output.txt &
    DAEMON_PID=$!
    sleep 2
    sudo ./taskmasterctl shutdown > /dev/null
    wait $DAEMON_PID 2>/dev/null
    
    if [ -f nobody_out.txt ] && grep -q "nobody" nobody_out.txt; then
        echo -e "${GREEN}[PASS]${NC} Root to nobody de-escalation worked"
    else
        echo -e "${RED}[FAIL]${NC} Root to nobody de-escalation failed"
        # cat error_output.txt
        exit 1
    fi
    rm -f test_root.yaml nobody_out.txt
else
    echo -e "${NC}[SKIP]${NC} sudo required for real privilege de-escalation test"
fi

# 5. Environment Variables
echo "Testing environment variables..."
cat <<EOF > test_env.yaml
programs:
  env_test:
    cmd: "env"
    env:
      MY_VAR: "hello_world"
      ANSWER: 42
    autostart: true
    stdout: "env_out.txt"
EOF

rm -f env_out.txt
./taskmasterd test_env.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 2
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null

if [ -f env_out.txt ] && grep -q "MY_VAR=hello_world" env_out.txt && grep -q "ANSWER=42" env_out.txt; then
    echo -e "${GREEN}[PASS]${NC} Environment variables correctly set"
else
    echo -e "${RED}[FAIL]${NC} Environment variables missing or incorrect"
    # cat env_out.txt
    exit 1
fi

echo -e "\n${GREEN}All modular tests passed!${NC}"
rm -f test_basic.yaml test_cmd.yaml test_priv.yaml test_env.yaml error_output.txt whoami_out.txt env_out.txt
