/* gcc -mwindows -mno-cygwin -o halt.exe halt.c
NAME
	halt - stopping the system

SYNOPSIS
	halt [-pq]

DESCRIPTION
	The halt utility logs off the current user, flushes the file system
	buffers to disk, stops all processes (non-responsive processes are
	only forced to stop in Windows 2000), and shuts the system down.

	The options are as follows

	-p	Attempt to powerdown the system.  If the powerdown fails, or
		the system does not support software powerdown, the system
		will halt.

	-q	Do not give processes a chance to shut down before halting or
		restarting.  This option should not normally be used.

AUTHOR
	Ben Collver <collver@softhome.net>
	Jim Klimov <jimklimov+nut@gmail.com> - slight adjustments for NUT builds
 */

#include "config.h"	/* should be first */
#include "common.h"

#include <windows.h>

#ifndef EWX_FORCEIFHUNG
#define EWX_FORCEIFHUNG 0x00000010
#endif

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	TOKEN_PRIVILEGES privileges = {1, {{{0, 0}, SE_PRIVILEGE_ENABLED}}};
	HANDLE my_token;
	UINT my_flags;

	NUT_UNUSED_VARIABLE(hInstance);
	NUT_UNUSED_VARIABLE(hPrevInstance);
	NUT_UNUSED_VARIABLE(nCmdShow);

	my_flags = EWX_SHUTDOWN | EWX_FORCEIFHUNG;

	if (strstr(lpCmdLine, "q") != NULL) {
		my_flags |= EWX_FORCE;
	}

	if (strstr(lpCmdLine, "p") != NULL) {
		my_flags |= EWX_POWEROFF;
	}

	if (!LookupPrivilegeValue(
		NULL,
		SE_SHUTDOWN_NAME,
		&privileges.Privileges[0].Luid))
	{
		exit(1);
	}

	if (!OpenProcessToken(
		GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES,
		&my_token))
	{
		exit(2);
	}

	if (!AdjustTokenPrivileges(
		my_token,
		FALSE,
		&privileges,
		sizeof(TOKEN_PRIVILEGES),
		NULL,
		NULL))
	{
		exit(3);
	}

	CloseHandle(my_token);
	if (!ExitWindowsEx(my_flags, 0)) {
		exit(4);
	}
	exit(0);
}
