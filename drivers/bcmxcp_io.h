/*
 * bcmxcp_io.h -- header for BCM/XCP IO module
 */

#ifndef BCMXCP_IO__
#define BCMXCP_IO__

#include "main.h" /* for usbdrv_info_t */

void send_read_command(unsigned char command);
void send_write_command(unsigned char *command, int command_length);
int get_answer(unsigned char *data, unsigned char command);
int command_read_sequence(unsigned char command, unsigned char *data);
int command_write_sequence(unsigned char *command, int command_length, unsigned	char *answer);
void upsdrv_initups(void);
void upsdrv_cleanup(void);
void upsdrv_reconnect(void);
void upsdrv_comm_good(void);

extern upsdrv_info_t comm_upsdrv_info;

#endif  /* BCMXCP_IO__ */
