#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)"
   exit 1
fi

echo "--- Unregistering Taskmaster ---"

# 1. Stop services
if command -v systemctl >/dev/null 2>&1; then
    echo "Stopping systemd service..."
    systemctl stop taskmaster 2>/dev/null
    systemctl disable taskmaster 2>/dev/null
    rm -f /etc/systemd/system/taskmaster.service
    systemctl daemon-reload
fi

if [ -f /etc/init.d/taskmaster ]; then
    echo "Stopping SysVinit service..."
    /etc/init.d/taskmaster stop 2>/dev/null
    rm -f /etc/init.d/taskmaster
fi

# 2. Remove binaries
echo "Removing binaries from /usr/local/bin..."
rm -f /usr/local/bin/taskmasterd
rm -f /usr/local/bin/taskmasterctl

# 3. Final cleanup
echo "Note: /etc/taskmaster and logs were not removed to preserve configuration."
echo "Unregistration complete."
