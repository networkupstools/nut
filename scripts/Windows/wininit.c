/* wininit.c - MS Windows service which replace the init script

   Copyright (C)
	2010	Frederic Bohe <fredericbohe@eaton.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifdef WIN32

#include "config.h"	/* should be first */
#include "common.h"
#include "winevent.h"
#include "wincompat.h"

#define NUT_START	TRUE
#define NUT_STOP	FALSE

typedef struct conn_s {
	HANDLE		handle;
	OVERLAPPED	overlapped;
	char		buf[LARGEBUF];
	struct conn_s	*prev;
	struct conn_s	*next;
} conn_t;

static DWORD			upsd_pid = 0;
static DWORD			upsmon_pid = 0;
static BOOL			service_flag = TRUE;
HANDLE				svc_stop = NULL;
static SERVICE_STATUS		SvcStatus;
static SERVICE_STATUS_HANDLE	SvcStatusHandle;

static void print_event(DWORD priority, const char * fmt, ...)
{
	HANDLE EventSource;
	va_list ap;
	CHAR * buf;
	int ret;

	buf = xmalloc(LARGEBUF);

	va_start(ap, fmt);
	ret = vsnprintf(buf, LARGEBUF, fmt, ap);
	va_end(ap);

	if(ret<0) {
		return;
	}

	if( !service_flag ) {
		upslogx(LOG_ERR, "EventLog : %s\n",buf);
	}

	EventSource = RegisterEventSource(NULL, SVCNAME);

	if( NULL != EventSource ) {
		ReportEvent( EventSource,	/* event log handle */
				priority,	/* event type */
				0,		/* event category */
				SVC_EVENT,	/* event identifier */
				NULL,		/* no security identifier*/
				1,		/* size of string array */
				0,		/* no binary data */
				(const char **)&buf,	/* array of string */
				NULL);		/* no binary data */

		DeregisterEventSource(EventSource);

	}

	if( buf )
		free(buf);
}

/* returns PID of the newly created process or 0 on failure */
static DWORD create_process(char * command)
{
	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	BOOL res;
	DWORD LastError;

	memset(&StartupInfo,0,sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(StartupInfo);
	memset(&ProcessInformation,0,sizeof(ProcessInformation));

	res = CreateProcess(
			NULL,
			command,
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_PROCESS_GROUP,
			NULL,
			NULL,
			&StartupInfo,
			&ProcessInformation
			);
	LastError = GetLastError();

	if( res == 0 ) {
		print_event(LOG_ERR, "Can't create process %s : %d", command, LastError);
		return 0;
	}

	return  ProcessInformation.dwProcessId;
}

/* return PID of created process or 0 on failure */
static DWORD run_drivers()
{
	char command[MAX_PATH];
	char *path;

	path = getfullpath(PATH_BIN);
	snprintf(command,sizeof(command),"%s\\upsdrvctl.exe start",path);
	free(path);
	return create_process(command);
}

/* return PID of created process or 0 on failure */
static DWORD stop_drivers()
{
	char command[MAX_PATH];
	char *path;

	path = getfullpath(PATH_BIN);
	snprintf(command,sizeof(command),"%s\\upsdrvctl.exe stop",path);
	free(path);
	return create_process(command);
}

/* return PID of created process or 0 on failure */
static void run_upsd()
{
	char command[MAX_PATH];
	char *path;

	path = getfullpath(PATH_SBIN);
	snprintf(command,sizeof(command),"%s\\upsd.exe",path);
	free(path);
	upsd_pid = create_process(command);
}

static void stop_upsd()
{
	if ( sendsignal( UPSD_PIPE_NAME, COMMAND_STOP ) ) {
		print_event(LOG_ERR, "Error stopping upsd (%d)",GetLastError());
	}
}

/* return PID of created process or 0 on failure */
static void run_upsmon()
{
	char command[MAX_PATH];
	char *path;

	path = getfullpath(PATH_SBIN);
	snprintf(command,sizeof(command),"%s\\upsmon.exe",path);
	free(path);
	upsmon_pid = create_process(command);
}

static void stop_upsmon()
{
	if ( sendsignal( UPSMON_PIPE_NAME, COMMAND_STOP ) ) {
		print_event(LOG_ERR, "Error stopping upsmon (%d)",GetLastError());
	}
}

/* Return 0 if powerdown flag is set */
static DWORD test_powerdownflag()
{
	char command[MAX_PATH];
	char *path;
	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	BOOL res;
	DWORD LastError;
	DWORD status;
	int i = 10;
	int timeout = 500;

	path = getfullpath(PATH_SBIN);
	snprintf(command,sizeof(command),"%s\\upsmon.exe -K",path);
	free(path);

	memset(&StartupInfo,0,sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(StartupInfo);
	memset(&ProcessInformation,0,sizeof(ProcessInformation));

	res = CreateProcess(
			NULL,
			command,
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_PROCESS_GROUP,
			NULL,
			NULL,
			&StartupInfo,
			&ProcessInformation
			);
	LastError = GetLastError();

	if( res == 0 ) {
		print_event(LOG_ERR, "Can't create process %s : %d", command, LastError);
		return 1;
	}

	while( i > 0) {
		res = GetExitCodeProcess(ProcessInformation.hProcess, &status);
		if( res != 0) {
			if( status != STILL_ACTIVE) {
				return status;
			}
		}
		Sleep(timeout);
		i--;
	}

	return 1;
}

static DWORD shutdown_ups()
{
	char command[MAX_PATH];
	char *path;

	path = getfullpath(PATH_BIN);
	snprintf(command,sizeof(command),"%s\\upsdrvctl.exe shutdown",path);
	free(path);
	return create_process(command);
}

/* return 0 on failure */
static int parse_nutconf(BOOL start_flag)
{
	char	fn[SMALLBUF];
	FILE	*nutf;
	char	buf[SMALLBUF];
	char	fullname[SMALLBUF];

	snprintf(fn,sizeof(fn),"%s/nut.conf",confpath());

	nutf = fopen(fn, "r");
	if(nutf == NULL) {
		snprintf(buf,sizeof(buf),"Error opening %s",fn);
		print_event(LOG_ERR,buf);
		return 0;
	}

	while( fgets(buf,sizeof(buf),nutf) != NULL ) {
		if(buf[0] != '#') {
			if( strstr(buf,"standalone") != NULL ||
					strstr(buf,"netserver") != NULL ) {
				if( start_flag == NUT_START ) {
					print_event(LOG_INFO,"Starting drivers");
					run_drivers();
					print_event(LOG_INFO,"Starting upsd");
					run_upsd();
					/* Wait a moment for the drivers to start */
					Sleep(5000);
					print_event(LOG_INFO,"Starting upsmon");
					run_upsmon();
					return 1;
				}
				else {
					print_event(LOG_INFO,"stop upsd");
					stop_upsd();
					print_event(LOG_INFO,"stop drivers");
					stop_drivers();
					print_event(LOG_INFO,"stop upsmon");
					stop_upsmon();
					/* Give a chance to upsmon to write the POWERDOWNFLAG  file */
					Sleep(1000);
					if( test_powerdownflag() == 0 ) {
						print_event(LOG_INFO,"shutdown ups");
						shutdown_ups();
					}
					print_event(LOG_INFO,"End of NUT stop");
					return 1;
				}
			}
			if( strstr(buf,"netclient") != NULL ) {
				if( start_flag == NUT_START ) {
					run_upsmon();
					return 1;
				}
				else {
					stop_upsmon();
					return 1;
				}
			}
		}
	}

	GetFullPathName(fn,sizeof(fullname),fullname,NULL);
	snprintf(buf,sizeof(buf),"nut disabled, please adjust the configuration to your needs. Then set MODE to a suitable value in %s to enable it.",fullname);
	print_event(LOG_ERR,buf);
	return 0;
}

static int SvcInstall(const char * SvcName, const char * args)
{
	SC_HANDLE SCManager;
	SC_HANDLE Service;
	TCHAR Path[MAX_PATH];

	if( !GetModuleFileName( NULL, Path, MAX_PATH ) ) {
		printf("Cannot install service (%d)\n", (int)GetLastError());
		return EXIT_FAILURE;
	}

	if( args != NULL ) {
		snprintfcat(Path, sizeof(Path), " %s", args);
	}

	SCManager = OpenSCManager(
			NULL,			/* local computer */
			NULL,			/* ServiceActive database */
			SC_MANAGER_ALL_ACCESS);	/* full access rights */

	if (NULL == SCManager) {
		upslogx(LOG_ERR, "OpenSCManager failed (%d)\n", (int)GetLastError());
		return EXIT_FAILURE;
	}

	Service = CreateService(
			SCManager,			/* SCM database */
			SvcName,			/* name of service */
			SvcName,			/* service name to display */
			SERVICE_ALL_ACCESS,		/* desired access */
			SERVICE_WIN32_OWN_PROCESS,	/* service type */
			SERVICE_AUTO_START,		/* start type */
			SERVICE_ERROR_NORMAL,		/* error control type */
			Path,				/* path to service binary */
			NULL,				/* no load ordering group */
			NULL,				/* no tag identifier */
			NULL,				/* no dependencies */
			NULL,				/* LocalSystem account */
			NULL);				/* no password */

	if (Service == NULL) {
		upslogx(LOG_ERR, "CreateService failed (%d)\n", (int)GetLastError());
		CloseServiceHandle(SCManager);
		return EXIT_FAILURE;
	}
	else {
		upslogx(LOG_INFO, "Service installed successfully\n");
	}

	CloseServiceHandle(Service);
	CloseServiceHandle(SCManager);

	return EXIT_SUCCESS;
}

static int SvcUninstall(const char * SvcName)
{
	SC_HANDLE SCManager;
	SC_HANDLE Service;

	SCManager = OpenSCManager(
			NULL,			/* local computer */
			NULL,			/* ServicesActive database */
			SC_MANAGER_ALL_ACCESS);	/* full access rights */

	if (NULL == SCManager) {
		upslogx(LOG_ERR, "OpenSCManager failed (%d)\n", (int)GetLastError());
		return EXIT_FAILURE;
	}

	Service = OpenService(
			SCManager,	/* SCM database */
			SvcName,	/* name of service */
			DELETE);	/* need delete access */

	if (Service == NULL) {
		upslogx(LOG_ERR, "OpenService failed (%d)\n", (int)GetLastError());
		CloseServiceHandle(SCManager);
		return EXIT_FAILURE;
	}

	if (! DeleteService(Service) )  {
		upslogx(LOG_ERR,"DeleteService failed (%d)\n", (int)GetLastError());
	}
	else {
		upslogx(LOG_ERR,"Service deleted successfully\n");
	}

	CloseServiceHandle(Service);
	CloseServiceHandle(SCManager);

	return EXIT_SUCCESS;
}

static void ReportSvcStatus(   DWORD CurrentState,
		DWORD Win32ExitCode,
		DWORD WaitHint)
{
	static DWORD CheckPoint = 1;

	SvcStatus.dwCurrentState = CurrentState;
	SvcStatus.dwWin32ExitCode = Win32ExitCode;
	SvcStatus.dwWaitHint = WaitHint;

	if (CurrentState == SERVICE_START_PENDING)
		SvcStatus.dwControlsAccepted = 0;
	else SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	if ( (CurrentState == SERVICE_RUNNING) ||
			(CurrentState == SERVICE_STOPPED) ) {
		SvcStatus.dwCheckPoint = 0;
	}
	else {
		SvcStatus.dwCheckPoint = CheckPoint++;
	}

	/* report the status of the service to the SCM */
	SetServiceStatus( SvcStatusHandle, &SvcStatus );
}

static void WINAPI SvcCtrlHandler( DWORD Ctrl )
{
	switch(Ctrl)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

			/* Signal the service to stop */
			SetEvent(svc_stop);
			ReportSvcStatus(SvcStatus.dwCurrentState, NO_ERROR, 0);

			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
}

static void SvcStart(char * SvcName)
{
	/* Register the handler function for the service */
	SvcStatusHandle = RegisterServiceCtrlHandler(
			SvcName,
			SvcCtrlHandler);

	if( !SvcStatusHandle ) {
		upslogx(LOG_ERR, "RegisterServiceCtrlHandler\n");
		return;
	}

	SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	SvcStatus.dwServiceSpecificExitCode = 0;

	/* Report initial status to the SCM */
	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );
}

static void SvcReady(void)
{
	svc_stop = CreateEvent(
			NULL,	/* default security attributes */
			TRUE,	/* manual reset event */
			FALSE,	/* not signaled */
			NULL);	/* no name */

	if( svc_stop == NULL ) {
		ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}
	ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0);
}

static void close_all(void)
{
	pipe_conn_t	*conn;

	for (conn = pipe_connhead; conn; conn = conn->next) {
		pipe_disconnect(conn);
	}

}

static void WINAPI SvcMain( DWORD argc, LPTSTR *argv )
{
	DWORD	ret;
	HANDLE	handles[MAXIMUM_WAIT_OBJECTS];
	int	maxhandle = 0;
	pipe_conn_t	*conn;
	DWORD priority;
	char * buf;

	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);

	if(service_flag) {
		SvcStart(SVCNAME);
	}

	/* A service has no console, so do has its children. */
	/* So if we want to be able to send CTRL+BREAK signal we must */
	/* create a console which will be inherited by children */
	AllocConsole();

	print_event(LOG_INFO,"Starting");

	/* pipe for event log proxy */
	pipe_create(EVENTLOG_PIPE_NAME);

	/* parse nut.conf and start relevant processes */
	if ( parse_nutconf(NUT_START) == 0 ) {
		print_event(LOG_INFO, "exiting");
		if( service_flag ) {
			ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0);
		}
		return;
	}

	if(service_flag) {
		SvcReady();
	}

	while (1) {
		maxhandle = 0;
		memset(&handles,0,sizeof(handles));

		/* Wait on the read IO of each connections */
		for (conn = pipe_connhead; conn; conn = conn->next) {
			handles[maxhandle] = conn->overlapped.hEvent;
			maxhandle++;
		}
		/* Add the new pipe connected event */
		handles[maxhandle] = pipe_connection_overlapped.hEvent;
		maxhandle++;

		/* Add SCM event handler in service mode*/
		if(service_flag) {
			handles[maxhandle] = svc_stop;
			maxhandle++;
		}

		ret = WaitForMultipleObjects(maxhandle,handles,FALSE,INFINITE);

		if (ret == WAIT_FAILED) {
			print_event(LOG_ERR, "Wait failed");
			return;
		}

		if( handles[ret] == svc_stop && service_flag ) {
			parse_nutconf(NUT_STOP);
			if(service_flag) {
				print_event(LOG_INFO, "Exiting");
				close_all();
				ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0);
			}
			return;
		}

		/* Retrieve the signaled connection */
		for(conn = pipe_connhead; conn != NULL; conn = conn->next) {
			if( conn->overlapped.hEvent == handles[ret-WAIT_OBJECT_0]) {
				break;
			}
		}
		/* a new pipe connection has been signaled */
		if (handles[ret] == pipe_connection_overlapped.hEvent) {
			pipe_connect();
		}
		/* one of the read event handle has been signaled */
		else {
			if( conn != NULL) {
				if( pipe_ready(conn) ) {
					buf = conn->buf;
					/* a frame is a DWORD indicating priority followed by an array of char (not necessarily followed by a terminal 0 */
					priority =*((DWORD *)buf);
					buf = buf + sizeof(DWORD);
					print_event(priority,buf);

					pipe_disconnect(conn);
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	while ((i = getopt(argc, argv, "+IUN")) != -1) {
		switch (i) {
			case 'I':
				return SvcInstall(SVCNAME,NULL);
			case 'U':
				return SvcUninstall(SVCNAME);
			case 'N':
				service_flag = FALSE;
				upslogx(LOG_ERR, "Running in non-service mode\n");
				break;
			default:
				break;
		}
	}

	optind = 0;

	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain },
		{ NULL, NULL }
	};
	/* This call returns when the service has stopped */
	if(service_flag ) {
		if (!StartServiceCtrlDispatcher( DispatchTable ))
		{
			print_event(LOG_ERR, "StartServiceCtrlDispatcher failed : exiting, this is a Windows service which can't be run as a regular application by default. Try -N to start it as a regular application");
		}
	}
	else {
		SvcMain(argc,argv);
	}

	return EXIT_SUCCESS;
}

#else

/* Just avoid: ISO C forbids an empty translation unit [-Werror=pedantic] */
int main (int argc, char ** argv);

#endif  /* WIN32 */
