#include "config.h"
#include "common.h"

const char *upsversion(void)
{
    if (SVNREV[0])
	return UPS_VERSION " (" SVNREV ")";
    else
	return UPS_VERSION;
}
