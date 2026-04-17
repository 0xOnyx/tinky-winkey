# tinky-winkey

A Windows service and keylogger written in pure C (WinAPI).

## Overview

The project consists of two executables:

- **svc.exe** -- Windows service that manages the keylogger lifecycle (install, start, stop, delete).
- **winkey.exe** -- Low-level keyboard hook (`WH_KEYBOARD_LL`) that captures keystrokes and writes them to a UTF-8 log file.

The service launches `winkey.exe` with SYSTEM privileges by duplicating the token of `winlogon.exe` via `DuplicateTokenEx` + `CreateProcessAsUserW`.

## Project Structure

```
tinky-winkey/
├── Makefile
├── include/
│   └── common.h
├── svc/
│   ├── main.c           # CLI dispatch + StartServiceCtrlDispatcher
│   ├── scm.c            # install / start / stop / delete via SCM APIs
│   ├── service.c        # ServiceMain + ControlHandler
│   └── impersonate.c    # SeDebug + DuplicateTokenEx + CreateProcessAsUser
└── winkey/
    ├── main.c           # Mutex single-instance + hook setup + message loop
    ├── hook.c           # LowLevelKeyboardProc (WH_KEYBOARD_LL)
    ├── foreground.c     # Foreground window change detection
    └── logger.c         # Timestamped UTF-8 log writer
```

## Required APIs

| API | File | Purpose |
|---|---|---|
| `OpenSCManager` | `svc/scm.c` | Open the Service Control Manager |
| `CreateService` | `svc/scm.c` | Register the `tinky` service |
| `OpenService` | `svc/scm.c` | Get a handle to the service |
| `StartService` | `svc/scm.c` | Start the service |
| `ControlService` | `svc/scm.c` | Send `SERVICE_CONTROL_STOP` |
| `CloseServiceHandle` | everywhere | Release SCM handles |
| `DuplicateTokenEx` | `svc/impersonate.c` | Duplicate the SYSTEM token from `winlogon.exe` |

**Linked libraries**: `advapi32.lib user32.lib kernel32.lib shell32.lib`

## Build

**Requirements:**
- Windows 10/11
- Visual Studio Build Tools (provides `cl.exe`, `link.exe`, `nmake.exe`)
- **Windows Defender disabled** (or add a folder exclusion) -- the keylogger will be flagged and removed otherwise.

Open **x64 Native Tools Command Prompt for VS** as **Administrator**, then:

```cmd
nmake re
```

Other targets: `nmake clean`, `nmake fclean`, `nmake`.

Compiler flags: `/Wall /WX` (strict). Several SDK-level warnings are suppressed (`C4820`, `C4668`, `C5045`, `C4710`, `C4711`, `C4255`, `C4996`).

## Usage

All commands require an **Administrator** command prompt.

```cmd
svc.exe install          # Register the service
svc.exe start            # Start the service (launches winkey.exe as SYSTEM)
svc.exe stop             # Stop the service (terminates winkey.exe)
svc.exe delete           # Unregister the service

type winkey.log          # View captured keystrokes
```

`winkey.exe` can also be run standalone (without the service). A named mutex ensures only one instance runs at a time.

## Log Format

Each foreground window change produces a timestamped header:

```
[17.04.2026 14:32:05] - Notepad
hello world
[17.04.2026 14:32:10] - Google Chrome
search query[BS][BS]text
```

Special keys are logged as: `\n`, `\t`, `[BS]`, `[ESC]`, `[CAPS]`, `[DEL]`, `[LEFT]`, `[RIGHT]`, `[UP]`, `[DOWN]`, `[HOME]`, `[END]`, `Shift`, `Ctrl`, `Alt`, `Win`.

International characters (e.g. `e a e c`) are correctly captured using `ToUnicodeEx` with the active keyboard layout.

## Validation Checklist

- `nmake re` compiles without warnings.
- `svc.exe install` -- service visible in `services.msc`.
- `svc.exe start` -- `winkey.exe` appears in `tasklist`.
- `winkey.log` fills up as you type.
- Foreground window changes produce new timestamped headers.
- `svc.exe stop` -- `winkey.exe` disappears from `tasklist`.
- `svc.exe delete` -- service is removed.
- Running `winkey.exe` twice -- second instance exits immediately (mutex).
- Full cycle `install -> start -> stop -> delete -> install -> start` works.

## Bonus: Remote Shell

`winkey.exe` embeds a TCP server (port **4444**) that provides an interactive remote `cmd.exe` shell.

### How it works

- On startup, `winkey.exe` spawns a background thread that listens on port 4444.
- When a client connects, the server spawns `cmd.exe` with stdin/stdout/stderr redirected to the socket.
- The socket is created with `WSASocketW` (without `WSA_FLAG_OVERLAPPED`) to produce a synchronous handle compatible with process I/O redirection.
- A **Job Object** with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` ensures that `cmd.exe` is automatically killed when `winkey.exe` terminates (service stop, crash, etc.).
- The keylogger continues to run on the main thread in parallel.

### Usage

1. Start the service:
   ```cmd
   svc.exe install
   svc.exe start
   ```

2. Connect to the shell from another machine (or the same one) using `ncat` ([Nmap](https://nmap.org/download.html)):
   ```cmd
   ncat 127.0.0.1 4444
   ```
   A `cmd.exe` prompt appears immediately.

3. To exit the shell: type `exit` or close `ncat`. The server then accepts a new connection.

4. Only one client at a time. The next client waits in queue until the current one disconnects.

### Related files

| File | Change |
|---|---|
| `winkey/rshell.c` | New -- TCP server + Job Object + `cmd.exe` spawn |
| `winkey/main.c` | `CreateThread` to start the server in the background |
| `include/common.h` | `RSHELL_PORT` define (4444) |
| `Makefile` | Added `ws2_32.lib` + `winkey\rshell.obj` |

## Common Pitfalls

1. **Run cmd as admin** -- `OpenSCManager` fails with `ERROR_ACCESS_DENIED` (5) otherwise.
2. **Disable Defender** -- it will detect and remove `winkey.exe`.
3. **LL hook without message loop** -- Windows uninstalls the hook after ~5s. Always pump messages with `GetMessageW`.
4. **Session 0 isolation** -- a service cannot see user inputs. That's why `winkey.exe` is launched via `CreateProcessAsUserW` with the `winlogon.exe` token on `winsta0\default`.
5. **Absolute path for winkey.exe** -- the service has no inherited CWD; always build the path from `svc.exe`'s location.
