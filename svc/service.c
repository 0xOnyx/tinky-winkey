#include "common.h"

static SERVICE_STATUS         g_status;
static SERVICE_STATUS_HANDLE  g_statusHandle;
static HANDLE                 g_stopEvent;
static PROCESS_INFORMATION    g_winkey;

int launch_winkey_as_system(PROCESS_INFORMATION* out);

static void set_state(DWORD state, DWORD exitCode) {
	g_status.dwCurrentState = state;
	g_status.dwWin32ExitCode = exitCode;
	g_status.dwControlsAccepted = (state == SERVICE_RUNNING) ? SERVICE_ACCEPT_STOP : 0;
	SetServiceStatus(g_statusHandle, &g_status);
}

static DWORD WINAPI ControlHandler(DWORD ctrl, DWORD evt, LPVOID data, LPVOID ctx) {
	
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
	else if (ctrl == SERVICE_CONTROL_INTERROGATE) return NO_ERROR;
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}

static void redirect_stderr_to_file(void)
{
	FILE* f = _wfreopen(L"C:\\tinky_debug.log", L"a", stderr);
	(void)f;
}

VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {

	(void)argc; (void)argv;

	redirect_stderr_to_file();

	ZeroMemory(&g_status, sizeof(g_status));
	g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	g_statusHandle = RegisterServiceCtrlHandlerExW(SVC_NAME, ControlHandler, NULL);
	if (!g_statusHandle) {
		fwprintf(stderr, L"RegisterServiceCtrlHandlerExW failed: %lu\n", GetLastError());
		return;
	}

	set_state(SERVICE_START_PENDING, 0);
	g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	

	if (launch_winkey_as_system(&g_winkey)) {
		fwprintf(stderr, L"Failed to launch winkey as system\n");
		set_state(SERVICE_STOPPED, 1);
		return;
	}


	set_state(SERVICE_RUNNING, 0);
	WaitForSingleObject(g_stopEvent, INFINITE);

	CloseHandle(g_stopEvent);
	set_state(SERVICE_STOPPED, 0);
}