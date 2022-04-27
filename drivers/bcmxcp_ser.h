/*
 * bcmxcp_ser.h -- header for BCM/XCP RS-232 module
 */

#ifndef BCMXCP_SER__
#define BCMXCP_SER__

#include "serial.h"	/* pulls termios.h to define speed_t */

/* This header is needed for this line, to avoid warnings about it not
 * being static in C file (can't hide, is also needed by nut-scanner)
 */
extern unsigned char BCMXCP_AUTHCMD[4];

typedef struct {
	speed_t rate;	/* Value like B19200 defined in termios.h; note: NOT the bitrate numerically */
	size_t name;	/* Actual rate... WHY is this "name" - number to print interactively? */
} pw_baud_rate_t;

extern pw_baud_rate_t pw_baud_rates[];

#endif  /* BCMXCP_SER__ */
