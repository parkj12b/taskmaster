# Taskmaster

Taskmaster is a job control daemon similar to `supervisor`, designed to supervise and manage child processes on Unix-like systems. It is split into a background daemon (`taskmasterd`) and a command-line controller (`taskmasterctl`).

## Features

### Process Supervision
- **Lifecycle Management**: Automatically starts, monitors, and restarts processes.
- **Configurable Restart Policies**: `always`, `never`, or `unexpected`.
- **Startup Verification**: Verified after staying alive for `starttime`.
- **Graceful Termination**: Sends configurable `stopsignal` and manages process cleanup.
- **Privilege De-escalation**: Optionally run processes as a specific `user`.

### Client-Server Architecture
- **Daemon (`taskmasterd`)**: Handles the heavy lifting of process management, logging, and state tracking.
- **Controller (`taskmasterctl`)**: A lightweight client that communicates with the daemon via Unix domain sockets.
- **Service Integration**: Includes scripts to register Taskmaster as a `systemd` or `SysVinit` service.

### Flexible Configuration Loading
The daemon scans for configurations in the following order:
1. **Command Line**: A path provided as the first argument to `taskmasterd` (file or directory).
2. **Default**: `./configs/` directory (fallback).

## Building and Installation

### Build from Source
```bash
make
```
This produces two binaries: `taskmasterd` and `taskmasterctl`.

### Register as a System Service
To install the binaries to `/usr/local/bin` and register Taskmaster as a system service (supports systemd and SysVinit):
```bash
sudo ./scripts/register_service.sh
```

## Usage

### Starting the Daemon
```bash
# Start with default config directory (./configs/)
./taskmasterd

# Start with a specific config file
./taskmasterd my_config.yaml
```

### Using the Controller
`taskmasterctl` can be used in interactive mode or direct command mode.

#### Interactive Mode
```bash
./taskmasterctl
taskmasterctl> status
```

#### Direct Mode
```bash
./taskmasterctl status
./taskmasterctl stop my_app
```

### Commands
- `status`: Show state and PID of all managed processes.
- `start <name>`: Start all instances of a program.
- `stop <name>`: Stop instances gracefully.
- `restart <name>`: Restart instances.
- `reload`: Re-scan config files and apply changes to the daemon.
- `shutdown`: Stop all processes and shut down the daemon.
- `exit` / `quit`: Exit the controller shell (does not stop the daemon).

## Logging
- **Syslog**: The daemon logs events to the system logger (`taskmasterd`).
- **Process Logs**: Individual program `stdout` and `stderr` can be redirected to files as specified in the configuration.
