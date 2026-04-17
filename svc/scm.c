#include "common.h"


int svc_install(void) {
	wchar_t path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);

	SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (!scm) { fwprintf(stderr, L"OpenSCManager failed: %lu\n", GetLastError()); return 1; }


    SC_HANDLE svc = CreateServiceW(
        scm, SVC_NAME, SVC_DISPLAY,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL, NULL, NULL, NULL, NULL);


    if (!svc){
		fwprintf(stderr, L"Create service failed: %lu\n", GetLastError());
		CloseServiceHandle(scm);
        return 1;
    }

	wprintf(L"Service {%ls} installed successfully.\n", SVC_NAME);
	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
    return 0;
}

int svc_start(void) {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { fwprintf(stderr, L"OpenSCManagerW failed %lu\n", GetLastError()); return 1; }

    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, SERVICE_START);
    if (!svc) {
		fwprintf(stderr, L"OpenServiceW failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }

    if (!StartServiceW(svc, 0, NULL)) {
		fwprintf(stderr, L"StartserviceW failed %lu\n", GetLastError());
        CloseServiceHandle(svc);
		CloseServiceHandle(scm);
        return 1;
    }

	wprintf(L"Service {%ls} started successfully.\n", SVC_NAME);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

int svc_stop(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { fwprintf(stderr, L"OpenSCManagerW failed %lu\n", GetLastError()); return 1; }


    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        fwprintf(stderr, L"OpenServiceW failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }

    SERVICE_STATUS st;
    if (!ControlService(svc, SERVICE_CONTROL_STOP, &st)) {
        wprintf(L"ControlService failed: %lu\n", GetLastError());
        CloseServiceHandle(svc);
        CloseServiceHandle(scm); 
        return 1;
    }

    wprintf(L"Service {%ls} stopped successfully.\n", SVC_NAME);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

int svc_delete(void)
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { fwprintf(stderr, L"OpenSCManagerW failed %lu\n", GetLastError()); return 1; }


    SC_HANDLE svc = OpenServiceW(scm, SVC_NAME, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        fwprintf(stderr, L"OpenServiceW failed: %lu\n", GetLastError());
        CloseServiceHandle(scm);
        return 1;
    }    SERVICE_STATUS st;

	ControlService(svc, SERVICE_CONTROL_STOP, &st); // best effort to stop the service before deleting it

    if (!DeleteService(svc)) {
        wprintf(L"DeleteService failed: %lu\n", GetLastError());
        CloseServiceHandle(svc); CloseServiceHandle(scm); return 1;
    }
    
    wprintf(L"Service {%ls} deleted successfully.\n", SVC_NAME);
    CloseServiceHandle(svc); 
    CloseServiceHandle(scm);
    return 0;
}