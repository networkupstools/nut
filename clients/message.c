#ifdef WIN32
#include <windows.h>

int main(int argc, char ** argv)
{
	if (argc < 2)
		return 1;

	MessageBox(NULL, argv[1], "Network UPS Tools",
		MB_OK|MB_ICONEXCLAMATION|MB_SERVICE_NOTIFICATION);

	return 0;
}
#endif
