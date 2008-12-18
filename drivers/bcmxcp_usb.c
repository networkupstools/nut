#include "main.h"
#include "bcmxcp.h"
#include "bcmxcp_io.h"
#include "nut_usb.h"

usb_dev_handle *upsdev = NULL;

#define SUBDRIVER_NAME	"USB communication subdriver"
#define SUBDRIVER_VERSION	"0.17"

/* communication driver description structure */
upsdrv_info_t comm_upsdrv_info = {
	SUBDRIVER_NAME,
	SUBDRIVER_VERSION,
	NULL,
	0,
	{ NULL }
};

void send_read_command(unsigned char command)
{
        unsigned char buf[4];

	buf[0]=PW_COMMAND_START_BYTE;
	buf[1]=0x01;                    /* data length */
	buf[2]=command;                 /* command to send */
	buf[3]=calc_checksum(buf);      /* checksum */
	usb_set_descriptor(upsdev, USB_DT_STRING, 4, buf, 4); /* Ignore error */
}

void send_write_command(unsigned char *command, int command_length)
{
	unsigned char sbuf[128];

	/* Prepare the send buffer */
	sbuf[0] = PW_COMMAND_START_BYTE;
	sbuf[1] = (unsigned char)(command_length);
	memcpy(sbuf+2, command, command_length);
	command_length += 2;

	/* Add checksum */
	sbuf[command_length] = calc_checksum(sbuf);
	command_length += 1;
	usb_set_descriptor(upsdev, USB_DT_STRING, 4, sbuf, command_length);  /* Ignore error */
}

/* get the answer of a command from the ups. And check that the answer is for this command */
int get_answer(unsigned char *data, unsigned char command)
{
	unsigned char buf[1024], *my_buf = buf;
	int length, end_length, res, endblock, start;
	unsigned char block_number, sequence, pre_sequence;

	end_length = 0;
	endblock = 0;
	pre_sequence = 0;
	start = 0;

	res = usb_interrupt_read(upsdev, 1, (char *)buf, sizeof(buf), 1000);
	if (res < 0) {
		nutusb_comm_fail("Receive error (Request command): COMMAND: %x\n", command);
		upsdrv_reconnect();
		return -1;
	}

	
	while (endblock != 1 && my_buf < buf+sizeof(buf) && res > 0){
		/* Read block number byte */
		block_number = (unsigned char)my_buf[1];

		if (command <= 0x43) {
			if ((command - 0x30) != block_number){
				nutusb_comm_fail("Receive error (Request command): BLOCK: %x, COMMAND: %x!\n", block_number, command);
				return -1;
			}
		}

		if (command >= 0x89) {
			if ((command == 0xA0) && (block_number != 0x01)){
				nutusb_comm_fail("Receive error (Request command): BLOCK: %x, COMMAND: %x!\n", block_number, command);
				return -1;
			}
			else if ((command != 0xA0) && (block_number != 0x09)){
				nutusb_comm_fail("Receive error (Request command): BLOCK: %x, COMMAND: %x!\n", block_number, command);
				return -1;
			}
		}

		/* Read data length byte */
		length = (unsigned char)my_buf[2];

		if (length < 1) {
			nutusb_comm_fail("Receive error (length): packet length %x!!!\n", length);
			return -1;
		}

		/* Read sequence byte */
		sequence = (unsigned char)my_buf[3];
		if ((sequence & 0x80) == 0x80) {
			endblock = 1;
		}

		if ((sequence & 0x07) != (pre_sequence + 1)) {
			nutusb_comm_fail("Not the right sequence received %x!!!\n", sequence);
			return -1;
		}

		pre_sequence = sequence;

		/* Try to read all the remainig bytes */
		if (res-5 < length) {
			nutusb_comm_fail("Receive error (data): got %d bytes instead of %d!!!\n", res-5, length);
			return -1;
		}

		/* now we have the whole answer from the ups, we can checksum it */
		if (!checksum_test(my_buf)) {
			nutusb_comm_fail("checksum error! ");
			return -1;
		}

		memcpy(data+end_length, my_buf+4, length);
		end_length += length;

		my_buf += length + 5;
		res -= length - 5;
	}
	return end_length;
}

/* Sends a single command (length=1). and get the answer */
int command_read_sequence(unsigned char command, unsigned char *data)
{
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_read_command(command);
		bytes_read = get_answer(data, command);
		retry++;
	}

	if (bytes_read < 1) {
		nutusb_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	return bytes_read;
}

/* Sends a setup command (length > 1) */
int command_write_sequence(unsigned char *command, int command_length, unsigned	char *answer)
{
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_write_command(command, command_length);
		bytes_read = get_answer(answer, command[0]);		
		retry ++;
	}

	if (bytes_read < 1) {
		nutusb_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	return bytes_read;
}


void upsdrv_comm_good()
{
	nutusb_comm_good();
}

void upsdrv_initups(void)
{
	upsdev = nutusb_open("USB");
}

void upsdrv_cleanup(void)
{
	upslogx(LOG_ERR, "CLOSING\n");
	nutusb_close(upsdev, "USB");
}

void upsdrv_reconnect(void)
{
	
	upslogx(LOG_WARNING, "RECONNECT USB DEVICE\n");
	nutusb_close(upsdev, "USB");
	upsdev = NULL;
	sleep(3);
	upsdrv_initups();	
}
