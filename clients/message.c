#ifdef WIN32
#include <windows.h>

void main(int args,char ** argv)
{
	MessageBox(NULL,argv[1],"Network UPS Tools",MB_OK|MB_ICONEXCLAMATION|MB_SERVICE_NOTIFICATION);
}
#endif
