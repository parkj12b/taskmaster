# Configuration Attributes

Taskmaster expects a top-level `programs:` map.
Each child key under `programs:` is a program name.

Example:
```yaml
programs:
  my_app:
    cmd: "/usr/bin/sleep 30"
    autostart: true
    autorestart: unexpected
    exitcodes:
      - 0
    startretries: 3
    starttime: 2
    stopsignal: TERM
    stoptime: 5
    stdout: "/tmp/my_app.out.log"
    stderr: "/tmp/my_app.err.log"
    workingdir: "/tmp"
    umask: "022"
    user: "nobody"
    env:
      PATH: "/usr/bin:/bin"
      MODE: "prod"
```

## Program-Level Attributes

| Key | Type | Allowed values / format | Default | Notes |
|---|---|---|---|---|
| `cmd` | string | shell-like command string | none | Required for usable program launch. Executed via `execvp` after splitting on spaces. |
| `numprocs` | int | integer >= 1 | `1` | Number of instances started and tracked. |
| `umask` | octal string | e.g. `022` | `0` | Parsed in base-8 (`strtol(..., 8)`). |
| `workingdir` | string | valid directory path | empty | If set, daemon `chdir`s before `execvp`. |
| `autostart` | bool | `true` / anything else treated as false | `true` | Only literal `true` enables it in parser. |
| `autorestart` | enum | `always`, `unexpected`, `never` | `never` | Unknown value logs warning and leaves default. |
| `exitcodes` | list[int] | YAML-style list under key | empty | Used to decide expected vs unexpected exit status. |
| `startretries` | int | integer >= 0 | `0` | Max restart attempts used by restart logic. |
| `starttime` | int | seconds | `0` | Time process must survive before considered `RUNNING`. |
| `stopsignal` | string | `TERM`, `KILL`, `HUP`, `INT`, `USR1`, `USR2` | `TERM` | Unknown signal name falls back to `TERM`. |
| `stoptime` | int | seconds | `0` | Parsed and stored; currently not used in shutdown wait logic. |
| `stdout` | string | file path | empty | If set, stdout is appended to this file. |
| `stderr` | string | file path | empty | If set, stderr is appended to this file. |
| `env` | map[string]string | indented key/value block | empty | Converted to `KEY=VALUE` and exported before `execvp`. |
| `user` | string | system username | empty | If set, daemon tries `setgid` + `setuid` before exec. |

## Exit Code + Restart Behavior

For a normal exit:
- Exit is `expected` if code is present in `exitcodes`.
- If `exitcodes` is empty, only exit code `0` is expected.

Restart decision:
- `autorestart: never`: never restart after exit.
- `autorestart: always`: always restart after exit (until `startretries` limit).
- `autorestart: unexpected`: restart only when exit is unexpected.

Retry limit:
- Restart occurs when `restart_count < startretries`.
- If restart is needed but retries are exhausted, process state becomes `FATAL`.

## Directory Config Mode

If daemon is started with a directory path, it loads all files ending in:
- `.yaml`
- `.conf`

## Parser Notes

- Unknown attributes log a warning and are ignored.
- Indentation/syntax issues log config errors.
- Quoted string values have surrounding `"` stripped when present.
