#ifdef WIN32
#include <windows.h>

int main(int args,char ** argv)
{
	MessageBox(NULL,argv[1],"Network UPS Tools",MB_OK|MB_ICONEXCLAMATION|MB_SERVICE_NOTIFICATION);

	return 0;
}
#endif
