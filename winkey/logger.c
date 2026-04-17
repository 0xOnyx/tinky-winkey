#include "common.h"

static CRITICAL_SECTION g_cs;
static BOOL				g_init = FALSE;


static void log_path(wchar_t* out, size_t cap) {
	GetModuleFileName(NULL, out, (DWORD)cap);
	wchar_t* lastSlash = wcsrchr(out, L'\\');
	if (lastSlash) {
		*(lastSlash + 1) = 0;
	}
	wcscat_s(out, cap, LOG_NAME);
}

static void write_utf8(const wchar_t* s)
{
	if (!g_init) {
		InitializeCriticalSection(&g_cs);
		g_init = TRUE;
	}

	EnterCriticalSection(&g_cs);

	wchar_t path[MAX_PATH];
	log_path(path, MAX_PATH);

	HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		LeaveCriticalSection(&g_cs);
		return;
	}

	int need = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
	if (need > 1) {
		char* utf8 = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)need);
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

void logger_write_raw(const wchar_t *s) {
	write_utf8(s);
}

void logger_write_header(const wchar_t* title) {
	SYSTEMTIME t; GetLocalTime(&t);
	wchar_t buf[768];
	_snwprintf_s(buf, 768, _TRUNCATE,
		L"\n[%02u.%02u.%04u %02u:%02u:%02u] - %ls\n",
		t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute, t.wSecond, title);
	write_utf8(buf);
}