#!/usr/bin/env bash

set -u

NAME="${DUMMY_NAME:-dummy_program}"
INTERVAL="${DUMMY_INTERVAL:-2}"
EXIT_AFTER="${DUMMY_EXIT_AFTER:-0}"
EXIT_CODE="${DUMMY_EXIT_CODE:-0}"

count=0

on_term() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] received SIGTERM, exiting"
    exit 0
}

on_int() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] received SIGINT, exiting"
    exit 0
}

on_hup() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] received SIGHUP, exiting"
    exit 0
}

trap on_term TERM
trap on_int INT
trap on_hup HUP

echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] started (pid=$$ interval=${INTERVAL}s)"

while true; do
    count=$((count + 1))
    echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] tick=$count"

    if [ "$EXIT_AFTER" -gt 0 ] && [ "$count" -ge "$EXIT_AFTER" ]; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') [$NAME] exiting intentionally with code $EXIT_CODE"
        exit "$EXIT_CODE"
    fi

    sleep "$INTERVAL"
done
