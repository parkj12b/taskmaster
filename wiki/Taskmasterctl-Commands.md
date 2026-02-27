# taskmasterctl Commands

`taskmasterctl` supports:
- Interactive mode: `./taskmasterctl`
- Direct mode: `./taskmasterctl <command> [args]`

It connects to daemon socket: `/tmp/taskmaster.sock`.

## Command Reference

| Command | Args | Client accepts | Daemon handles | Effect |
|---|---|---|---|---|
| `status` | none | yes | yes | Prints process table (`NAME`, `INDEX`, `STATE`, `pid`). |
| `start` | `<name>` | yes | yes | Starts all instances of program `<name>`. |
| `stop` | `<name>` | yes | yes | Sends configured `stopsignal` to all instances of `<name>`. |
| `restart` | `<name>` | yes | no (current code) | Client sends `CMD_RESTART`, but daemon currently responds as unknown command. |
| `reload` | none | yes | yes | Requests config reload in daemon loop. |
| `shutdown` | none | yes | yes | Stops daemon main loop and exits daemon process. |
| `exit` | none | yes | n/a | Exits client interactive shell only. |
| `quit` | none | yes | n/a | Same as `exit`. |

## Examples

Direct mode:
```bash
./taskmasterctl status
./taskmasterctl start my_app
./taskmasterctl stop my_app
./taskmasterctl reload
./taskmasterctl shutdown
```

Interactive mode:
```text
taskmasterctl> status
taskmasterctl> start my_app
taskmasterctl> stop my_app
taskmasterctl> reload
taskmasterctl> quit
```

## Error Cases

- If daemon is not running or socket is missing:
```text
Error: Could not connect to daemon at /tmp/taskmaster.sock
```
- Unknown client command prints:
```text
Unknown command: <cmd>
```
