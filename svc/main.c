#include "common.h"


int svc_install(void);
int svc_start(void);
int svc_stop(void);
int svc_delete(void);

VOID WINAPI ServiceMain(DWORD argc, LPWSTR* argv);

int wmain(int argc, wchar_t** argv)
{
	if (argc == 2){
		if (!lstrcmpW(argv[1], L"install"))	return svc_install();
		if (!lstrcmpW(argv[1], L"start"))	return svc_start();
		if (!lstrcmpW(argv[1], L"stop"))	return svc_stop();
		if (!lstrcmpW(argv[1], L"delete"))	return svc_delete();
	}

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
