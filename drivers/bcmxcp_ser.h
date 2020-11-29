/*
 * bcmxcp_ser.h -- header for BCM/XCP RS-232 module
 */

#ifndef BCMXCP_SER__
#define BCMXCP_SER__

/* This header is needed for this line, to avoid warnings about it not
 * being static in C file (can't hide, is also needed by nut-scanner)
 */
extern unsigned char AUT[4];

typedef struct {
	int rate;
	int name;
} pw_baud_rate_t;

extern pw_baud_rate_t pw_baud_rates[];

#endif  /* BCMXCP_SER__ */
