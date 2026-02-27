# Taskmaster Wiki

Taskmaster is split into:
- `taskmasterd`: daemon that supervises processes.
- `taskmasterctl`: client that sends commands over Unix socket (`/tmp/taskmaster.sock`).

## Pages
- [Configuration Attributes](Config-Reference.md)
- [taskmasterctl Commands](Taskmasterctl-Commands.md)

## Quick Start
Build:
```bash
make
```

Start daemon with a config file:
```bash
./taskmasterd ./test.yaml
```

Open interactive control shell:
```bash
./taskmasterctl
```

Run a direct command:
```bash
./taskmasterctl status
```

## Config Loading
- If `taskmasterd` is started with an argument, it treats it as either:
1. a config file path, or
2. a directory path (loads all `.yaml` and `.conf` files in that directory).
- Without an argument, it uses the built-in default path: `/etc/taskmaster`.
