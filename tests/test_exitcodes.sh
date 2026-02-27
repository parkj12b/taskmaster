#!/bin/bash
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "Testing exitcodes parsing and behavior..."
EXIT42_PATH="$(pwd)/tests/exit42"

# Ensure the helper exists
if [ ! -f "$EXIT42_PATH" ]; then
    echo "int main() { return 42; }" > tests/exit42.c
    gcc tests/exit42.c -o tests/exit42
fi

echo "--- Test 1: Expected Exit (No Restart) ---"
cat <<EOF > test_exit.yaml
programs:
  exit_test:
    cmd: "$EXIT42_PATH"
    autostart: true
    autorestart: unexpected
    exitcodes:
      - 42
    startretries: 1
EOF

rm -f error_output.txt
./taskmasterd test_exit.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 2
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null

if grep -q "exited with code 42 (expected)" error_output.txt; then
    echo -e "${GREEN}[PASS]${NC} Exit code 42 correctly identified as expected"
    if grep -q "Restarting process exit_test" error_output.txt; then
        echo -e "${RED}[FAIL]${NC} Process restarted despite being an expected exit"
        exit 1
    else
        echo -e "${GREEN}[PASS]${NC} Process did not restart (correct)"
    fi
else
    echo -e "${RED}[FAIL]${NC} Exit code 42 not identified as expected"
    cat error_output.txt
    exit 1
fi

echo "--- Test 2: Unexpected Exit (With Restart) ---"
cat <<EOF > test_exit_unexp.yaml
programs:
  exit_test_unexp:
    cmd: "$EXIT42_PATH"
    autostart: true
    autorestart: unexpected
    exitcodes:
      - 0
    startretries: 1
EOF

rm -f error_output.txt
./taskmasterd test_exit_unexp.yaml 2> error_output.txt &
DAEMON_PID=$!
sleep 2
./taskmasterctl shutdown > /dev/null
wait $DAEMON_PID 2>/dev/null

if grep -q "exited with code 42 (unexpected)" error_output.txt; then
    echo -e "${GREEN}[PASS]${NC} Exit code 42 correctly identified as unexpected"
    if grep -q "Restarting process exit_test_unexp" error_output.txt; then
        echo -e "${GREEN}[PASS]${NC} Process correctly restarted after unexpected exit"
    else
        echo -e "${RED}[FAIL]${NC} Process failed to restart after unexpected exit"
        cat error_output.txt
        exit 1
    fi
else
    echo -e "${RED}[FAIL]${NC} Exit code 42 not identified as unexpected"
    cat error_output.txt
    exit 1
fi

rm -f test_exit.yaml test_exit_unexp.yaml error_output.txt
echo -e "${GREEN}All exitcode and restart tests passed!${NC}"
