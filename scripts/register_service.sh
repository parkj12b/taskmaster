#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)"
   exit 1
fi

BINARY_PATH="$(pwd)/taskmasterd"
CTL_PATH="$(pwd)/taskmasterctl"
CONFIG_PATH="/etc/taskmaster"

if [ ! -f "$BINARY_PATH" ]; then
    echo "Error: taskmasterd not found. Please run 'make' first."
    exit 1
fi

# Setup directories
mkdir -p "$CONFIG_PATH"
cp -n test.yaml "$CONFIG_PATH/default.yaml" 2>/dev/null

# Install binaries to /usr/local/bin for global access
cp "$BINARY_PATH" /usr/local/bin/taskmasterd
cp "$CTL_PATH" /usr/local/bin/taskmasterctl

echo "--- Registering Systemd Service ---"
cat <<EOF > /etc/systemd/system/taskmaster.service
[Unit]
Description=Taskmaster Job Control Daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/taskmasterd $CONFIG_PATH
ExecReload=/bin/kill -HUP \$MAINPID
Restart=always
User=root

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
echo "Systemd service created at /etc/systemd/system/taskmaster.service"
echo "To start: systemctl start taskmaster"

echo -e "\n--- Registering SysVinit Script ---"
cat <<EOF > /etc/init.d/taskmaster
#!/bin/sh
### BEGIN INIT INFO
# Provides:          taskmaster
# Required-Start:    \$remote_fs \$syslog
# Required-Stop:     \$remote_fs \$syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Taskmaster job control daemon
### END INIT INFO

DAEMON=/usr/local/bin/taskmasterd
NAME=taskmaster
DESC="Taskmaster Daemon"
CONFIG=$CONFIG_PATH

case "\$1" in
  start)
    echo -n "Starting \$DESC: "
    start-stop-daemon --start --background --exec \$DAEMON -- \$CONFIG
    echo "\$NAME."
    ;;
  stop)
    echo -n "Stopping \$DESC: "
    start-stop-daemon --stop --name \$NAME
    echo "\$NAME."
    ;;
  restart|force-reload)
    echo -n "Restarting \$DESC: "
    start-stop-daemon --stop --name \$NAME
    sleep 1
    start-stop-daemon --start --background --exec \$DAEMON -- \$CONFIG
    echo "\$NAME."
    ;;
  reload)
    echo -n "Reloading \$DESC configuration: "
    killall -HUP taskmasterd
    echo "\$NAME."
    ;;
  status)
    /usr/local/bin/taskmasterctl status
    ;;
  *)
    echo "Usage: \$0 {start|stop|restart|reload|status}" >&2
    exit 1
    ;;
esac

exit 0
EOF

chmod +x /etc/init.d/taskmaster
echo "SysVinit script created at /etc/init.d/taskmaster"
echo "To start: /etc/init.d/taskmaster start"
