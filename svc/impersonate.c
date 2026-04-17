#include "common.h"
#include <TlHelp32.h>

static int enable_debug_privilege() {
	HANDLE token;

	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return 0;

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
	int result = GetLastError() == ERROR_SUCCESS;
	CloseHandle(token);
	return !result;
}

DWORD find_winlogon_pid(void) {
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (snap == INVALID_HANDLE_VALUE) return 0;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32FirstW(snap, &pe)) return 0;

	DWORD pid = 0;
	do {
		if (!_wcsicmp(pe.szExeFile, L"winlogon.exe")) {
			pid = pe.th32ProcessID;
			break;
		}
	} while (Process32NextW(snap, &pe));

	CloseHandle(snap);
	return pid;
}

int launch_winkey_as_system(PROCESS_INFORMATION* out) {
	if (enable_debug_privilege()) {
		fwprintf(stderr, L"Failed to enable debug privilege\n");
		return 1;
	}

	DWORD pid = find_winlogon_pid();
	if (!pid) {
		fwprintf(stderr, L"Failed to find winlogon pid\n");
		return 1;
	}

	HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (!hproc) {
		fwprintf(stderr, L"Failed to open winlogon process\n");
		return 1;
	}

	HANDLE htoken = NULL;
	if (!OpenProcessToken(hproc,
		TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY |
		TOKEN_IMPERSONATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
		&htoken)) {
		fwprintf(stderr, L"Failed to open winlogon token\n");
		CloseHandle(hproc);
		return 1;
	}

	HANDLE hDup = NULL;
	if (!DuplicateTokenEx(htoken, MAXIMUM_ALLOWED, NULL,
		SecurityImpersonation, TokenPrimary, &hDup)) {
		CloseHandle(htoken);
		CloseHandle(hproc);
		return 1;
	}

	wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
	wchar_t* slash = wcsrchr(path, L'\\');

	if (slash) *(slash + 1) = 0;
	wcscat_s(path, MAX_PATH, WINKEY_EXE);

	STARTUPINFOW si;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	si.lpDesktop = L"winsta0\\default";

	BOOL ok = CreateProcessAsUserW(hDup, path, NULL, NULL, NULL, FALSE,
		CREATE_NO_WINDOW, NULL, NULL, &si, out);

	CloseHandle(hDup);
	CloseHandle(htoken);
	CloseHandle(hproc);
	return !ok;
}