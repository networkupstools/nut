#include "main.h"
#include "bcmxcp.h"
#include "bcmxcp_io.h"
#include "bcmxcp_ser.h"
#include "serial.h"


#define SUBDRIVER_NAME    "RS-232 communication subdriver"
#define SUBDRIVER_VERSION "0.20"

/* communication driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	SUBDRIVER_NAME,
	SUBDRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

#define PW_MAX_BAUD 5

/* NOT static: also used from nut-scanner, so extern'ed via bcmxcp_ser.h */
pw_baud_rate_t pw_baud_rates[] = {
	{ B19200, 19200 },
	{ B9600,  9600 },
	{ B4800,  4800 },
	{ B2400,  2400 },
	{ B1200,  1200 },
	/* end of structure. */
	{ 0,  0 }
};

/* NOT static: also used from nut-scanner, so extern'ed via bcmxcp_ser.h */
unsigned char AUT[4] = {0xCF, 0x69, 0xE8, 0xD5}; /* Authorisation command */

static void send_command(unsigned char *command, int command_length)
{
	int retry = 0, sent;
	unsigned char sbuf[128];

	/* Prepare the send buffer */
	sbuf[0] = PW_COMMAND_START_BYTE;
	sbuf[1] = (unsigned char)(command_length);
	memcpy(sbuf+2, command, command_length);
	command_length += 2;

	/* Add checksum */
	sbuf[command_length] = calc_checksum(sbuf);
	command_length += 1;

	upsdebug_hex (3, "send_command", sbuf, command_length);

	while (retry++ < PW_MAX_TRY) {

		if (retry == PW_MAX_TRY) {
			ser_send_char(upsfd, 0x1d); /* last retry is preceded by a ESC.*/
			usleep(250000);
		}

		sent = ser_send_buf(upsfd, sbuf, command_length);

		if (sent == command_length) {
			return;
		}
	}
}

void send_read_command(unsigned char command)
{
	send_command(&command, 1);
}

void send_write_command(unsigned char *command, int command_length)
{
	send_command(command, command_length);
}

/* get the answer of a command from the ups. And check that the answer is for this command */
int get_answer(unsigned char *data, unsigned char command)
{
	unsigned char	my_buf[128]; /* packet has a maximum length of 121+5 bytes */
	int		length, end_length = 0, res, endblock = 0, start = 0;
	unsigned char	block_number, sequence, pre_sequence = 0;

	while (endblock != 1){

		do {
			/* Read PW_COMMAND_START_BYTE byte */
			res = ser_get_char(upsfd, my_buf, 1, 0);

			if (res != 1) {
				upsdebugx(1,"Receive error (PW_COMMAND_START_BYTE): %d, cmd=%x!!!\n", res, command);
				return -1;
			}

			start++;

		} while ((my_buf[0] != PW_COMMAND_START_BYTE) && (start < 128));

		if (start == 128) {
			ser_comm_fail("Receive error (PW_COMMAND_START_BYTE): packet not on start!!%x\n", my_buf[0]);
			return -1;
		}

		/* Read block number byte */
		res = ser_get_char(upsfd, my_buf+1, 1, 0);

		if (res != 1) {
			ser_comm_fail("Receive error (Block number): %d!!!\n", res);
			return -1;
		}

		block_number = (unsigned char)my_buf[1];

		if (command <= 0x43) {
			if ((command - 0x30) != block_number){
				ser_comm_fail("Receive error (Request command): %x!!!\n", block_number);
				return -1;
			}
		}

		if (command >= 0x89) {
			if ((command == 0xA0) && (block_number != 0x01)){
				ser_comm_fail("Receive error (Requested only mode command): %x!!!\n", block_number);
				return -1;
			}

			if ((command != 0xA0) && (block_number != 0x09)){
				ser_comm_fail("Receive error (Control command): %x!!!\n", block_number);
				return -1;
			}
		}

		/* Read data length byte */
		res = ser_get_char(upsfd, my_buf+2, 1, 0);

		if (res != 1) {
			ser_comm_fail("Receive error (length): %d!!!\n", res);
			return -1;
		}

		length = (unsigned char)my_buf[2];

		if (length < 1) {
			ser_comm_fail("Receive error (length): packet length %x!!!\n", length);
			return -1;
		}

		/* Read sequence byte */
		res = ser_get_char(upsfd, my_buf+3, 1, 0);

		if (res != 1) {
			ser_comm_fail("Receive error (sequence): %d!!!\n", res);
			return -1;
		}

		sequence = (unsigned char)my_buf[3];

		if ((sequence & 0x80) == 0x80) {
			endblock = 1;
		}

		if ((sequence & 0x07) != (pre_sequence + 1)) {
			ser_comm_fail("Not the right sequence received %x!!!\n", sequence);
			return -1;
		}

		pre_sequence = sequence;

		/* Try to read all the remainig bytes */
		res = ser_get_buf_len(upsfd, my_buf+4, length, 1, 0);

		if (res != length) {
			ser_comm_fail("Receive error (data): got %d bytes instead of %d!!!\n", res, length);
			return -1;
		}

		/* Get the checksum byte */
		res = ser_get_char(upsfd, my_buf+(4+length), 1, 0);

		if (res != 1) {
			ser_comm_fail("Receive error (checksum): %x!!!\n", res);
			return -1;
		}

		/* now we have the whole answer from the ups, we can checksum it */
		if (!checksum_test(my_buf)) {
			ser_comm_fail("checksum error! ");
			return -1;
		}

		memcpy(data+end_length, my_buf+4, length);
		end_length += length;

	}

	upsdebug_hex (5, "get_answer", data, end_length);
	ser_comm_good();

	return end_length;
}

static int command_sequence(unsigned char *command, int command_length, unsigned char *answer)
{
	int bytes_read, retry = 0;

	while (retry++ < PW_MAX_TRY) {

		if (retry == PW_MAX_TRY) {
			ser_flush_in(upsfd, "", 0);
		}

		send_write_command(command, command_length);

		bytes_read = get_answer(answer, *command);

		if (bytes_read > 0) {
			return bytes_read;
		}
	}

	return -1;
}

/* Sends a single command (length=1). and get the answer */
int command_read_sequence(unsigned char command, unsigned char *answer)
{
	int bytes_read;

	bytes_read = command_sequence(&command, 1, answer);

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
	}

	return bytes_read;
}

/* Sends a setup command (length > 1) */
int command_write_sequence(unsigned char *command, int command_length, unsigned	char *answer)
{
	int bytes_read;

	bytes_read = command_sequence(command, command_length, answer);

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
	}

	return bytes_read;
}

void upsdrv_comm_good()
{
	ser_comm_good();
}

void pw_comm_setup(const char *port)
{
	unsigned char command = PW_SET_REQ_ONLY_MODE;
	unsigned char id_command = PW_ID_BLOCK_REQ;
	unsigned char answer[256];
	int i = 0, baud, mybaud = 0, ret = -1;

	if (getval("baud_rate") != NULL)
	{
		baud = atoi(getval("baud_rate"));

		for(i = 0; i < PW_MAX_BAUD; i++) {
			if (baud == pw_baud_rates[i].name) {
				mybaud = pw_baud_rates[i].rate;
				break;
			}
		}

		if (mybaud == 0) {
			fatalx(EXIT_FAILURE, "Specified baudrate \"%s\" is invalid!", getval("baud_rate"));
		}

		ser_set_speed(upsfd, device_path, mybaud);
		ser_send_char(upsfd, 0x1d); /* send ESC to take it out of menu */
		usleep(90000);
		send_write_command(AUT, 4);
		usleep(500000);
		ret = command_sequence(&command, 1, answer);
		if (ret <= 0) {
			usleep(500000);
			ret = command_sequence(&id_command, 1, answer);
		}

		if (ret > 0) {
			upslogx(LOG_INFO, "Connected to UPS on %s with baudrate %d", port, baud);
			return;
		}

		upslogx(LOG_ERR, "No response from UPS on %s with baudrate %d", port, baud);
	}

	upslogx(LOG_INFO, "Attempting to autodect baudrate");

	for (i=0; i<PW_MAX_BAUD; i++) {

		ser_set_speed(upsfd, device_path, pw_baud_rates[i].rate);
		ser_send_char(upsfd, 0x1d); /* send ESC to take it out of menu */
		usleep(90000);
		send_write_command(AUT, 4);
		usleep(500000);
		ret = command_sequence(&command, 1, answer);
		if (ret <= 0) {
			usleep(500000);
			ret = command_sequence(&id_command, 1, answer);
		}

		if (ret > 0) {
			upslogx(LOG_INFO, "Connected to UPS on %s with baudrate %d", port, pw_baud_rates[i].name);
			return;
		}

		upsdebugx(2, "No response from UPS on %s with baudrate %d", port, pw_baud_rates[i].name);
	}

	fatalx(EXIT_FAILURE, "Can't connect to the UPS on port %s!\n", port);
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	pw_comm_setup(device_path);
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path);
}

void upsdrv_reconnect(void)
{
}

