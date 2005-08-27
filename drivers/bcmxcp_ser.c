#include "main.h"
#include "bcmxcp.h"
#include "bcmxcp_io.h"
#include "serial.h"

void send_read_command(unsigned char command)
{
	int retry, sent;
	unsigned char buf[4];
	retry = 0;
	sent = 0;
	
	while ((sent != 4) && (retry < PW_MAX_TRY)) {
		buf[0]=PW_COMMAND_START_BYTE;
		buf[1]=0x01;			/* data length */
		buf[2]=command;			/* command to send */
		buf[3]=calc_checksum(buf);	/* checksum */
		
		if (retry == 4) ser_send_char(upsfd, 0x1d);	/* last retry is preceded by a ESC.*/
			sent = ser_send_buf(upsfd, (char*)buf, 4);
		retry += 1;
	}
}


void send_write_command(unsigned char *command, int command_length)
{
	int  retry, sent;
	unsigned char sbuf[128];

	/* Prepare the send buffer */
	sbuf[0] = PW_COMMAND_START_BYTE;
	sbuf[1] = (unsigned char)(command_length);
	memcpy(sbuf+2, command, command_length);
	command_length += 2;

	/* Add checksum */
	sbuf[command_length] = calc_checksum(sbuf);
	command_length += 1;

	/* Try to send the command */
	retry = 0;
	sent = 0;

	while ((sent != (command_length)) && (retry < PW_MAX_TRY)) {
		sent = ser_send_buf(upsfd, (char*)sbuf, (command_length));
		if (sent != (command_length)) printf("Error sending command %x\n", (unsigned char)sbuf[2]);
			retry += 1;
	}
}

/* get the answer of a command from the ups. And check that the answer is for this command */
int get_answer(unsigned char *data, unsigned char command)
{
	unsigned char my_buf[128];	/* packet has a maximum length of 121+5 bytes */
	int length, end_length, res, endblock, start;
	unsigned char block_number, sequence, pre_sequence;

	end_length = 0;
	endblock = 0;
	pre_sequence = 0;
	start = 0;

	while (endblock != 1){

		do {
			/* Read PW_COMMAND_START_BYTE byte */
			res = ser_get_char(upsfd, (char*)my_buf, 1, 0);
			if (res != 1) {
				ser_comm_fail("Receive error (PW_COMMAND_START_BYTE): %d!!!\n", res);
				return -1;
			}
			start++;

		} while ((my_buf[0] != PW_COMMAND_START_BYTE) || start == 128);
		
		if (start == 128) {
			ser_comm_fail("Receive error (PW_COMMAND_START_BYTE): packet not on start!!%x\n", my_buf[0]);
			return -1;
		}

		/* Read block number byte */
		res = ser_get_char(upsfd, (char*)(my_buf+1), 1, 0);
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
			else if ((command != 0xA0) && (block_number != 0x09)){
				ser_comm_fail("Receive error (Control command): %x!!!\n", block_number);
				return -1;
			}
		}

		/* Read data length byte */
		res = ser_get_char(upsfd, (char*)(my_buf+2), 1, 0);
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
		res = ser_get_char(upsfd, (char*)(my_buf+3), 1, 0);
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
		res = ser_get_buf_len(upsfd, (char*)(my_buf+4), length, 1, 0);
		if (res != length) {
			ser_comm_fail("Receive error (data): got %d bytes instead of %d!!!\n", res, length);
			return -1;
		}

		/* Get the checksum byte */
		res = ser_get_char(upsfd, (char*)(my_buf+(4+length)), 1, 0);

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
		
		if (retry > 2) 
			ser_flush_in(upsfd, "", 0);
		retry++;
	}

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	ser_comm_good();

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
		
		if (retry > 2) 
			ser_flush_in(upsfd, "", 0);
		retry ++;
	}

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	ser_comm_good();

	return bytes_read;
}

void upsdrv_comm_good()
{
	ser_comm_good();
}

void upsdrv_initups(void)
{
	int i;
	experimental_driver=1;	

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	for(i = 0;i < 2; i++)	/* send ESC two times to take it out of menu */
	ser_send_char(upsfd, 0x1d);
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path);
}

void upsdrv_reconnect(void)
{
}

