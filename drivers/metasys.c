/* metasys.c - driver for Meta System UPS

   Copyright (C) 2004  Fabio Di Niro <fabio.diniro@email.it>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* Uncomment if you want to read additional Meta System UPS data */
/* 
#define EXTRADATA 
*/

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"Metasystem UPS driver"
#define DRIVER_VERSION	"0.07"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Fabio Di Niro <fabio.diniro@email.it>",
	DRV_STABLE,
	{ NULL }
};

/* Autorestart flag */
int autorestart = 0;
int nominal_power = 0;

/* ups commands */
#define UPS_INFO 			0x00
#define UPS_OUTPUT_DATA 		0x01
#define UPS_INPUT_DATA 			0x02
#define UPS_STATUS 			0x03
#define UPS_BATTERY_DATA 		0x04
#define UPS_HISTORY_DATA		0x05
#define UPS_GET_SCHEDULING		0x06
#define UPS_EVENT_LIST			0x07
#define UPS_GET_TIMES_ON_BATTERY	0x08
#define UPS_GET_NEUTRAL_SENSE		0x09
#define UPS_SET_SCHEDULING		0x0a
#define UPS_SET_NEUTRAL_SENSE		0x0b
#define UPS_SET_TIMES_ON_BATTERY	0x0c
#define UPS_SET_BUZZER_MUTE		0x0d
#define UPS_SET_BATTERY_TEST		0x0e

static int instcmd(const char *cmdname, const char *extra);

/*
	Metasystem UPS data transfer are made with packet of the format:
	STX		DATA_LENGTH		DATA		CHECKSUM
	where:
	STX is 0x02 and is the start of transmission byte
	DATA_LENGTH is number of data bytes + the checksum byte
	DATA ......
	CHECKSUM is the sum modulus 256 of all DATA bytes + DATA_LENGTH
	
	The answer from the UPS have the same packet format and the first
	data byte is equal to the command that the ups is answering to
*/
int get_word(unsigned char *buffer) {		/* return an integer reading a word in the supplied buffer */
	unsigned char a, b;
	int result;
	
	a = buffer[0];
	b = buffer[1];
	result = b*256 + a;
	return result;
}


long int get_long(unsigned char *buffer) {	/* return a long integer reading 4 bytes in the supplied buffer */
	unsigned char a, b, c, d;
	long int result;
	a=buffer[0];
	b=buffer[1];
	c=buffer[2];
	d=buffer[3];
	result = (256*256*256*d) + (256*256*c) + (256*b) + a;
	return result;
}

void send_zeros(void) {				/* send 100 times the value 0x00.....it seems to be used for resetting */
	unsigned char buf[100];				/* the ups serial port */

	memset(buf, '\0', sizeof(buf));
	ser_send_buf(upsfd, buf, sizeof(buf));
	return;
}


/* was used just for the debug process */
void dump_buffer(unsigned char *buffer, int buf_len) {
	int i;
	for (i = 0; i < buf_len; i++) { 
		printf("byte %d: %x\n", i, buffer[i]);	
	}
	return;
}

/* send a read command to the UPS, it retries 5 times before give up
   it's a 4 byte request (STX, LENGTH, COMMAND and CHECKSUM) */
void send_read_command(char command) {
	int retry, sent;
	unsigned char buf[4];
	retry = 0;
	sent = 0;
	while ((sent != 4) && (retry < 5)) {
		buf[0]=0x02; 			/* STX Start of Transmission */
		buf[1]=0x02;			/* data length(data + checksum byte) */
		buf[2]=command;			/* command to send */
		buf[3]=buf[1] + buf[2];	/* checksum (sum modulus 256 of data bytes + length) */
		if (retry == 4) send_zeros();	/* last retry is preceded by a serial reset...*/
		sent = ser_send_buf(upsfd, buf, 4);
		retry += 1;
	}
}

/* send a write command to the UPS, the write command and the value to be written are passed 
   with a char* buffer 
   it retries 5 times before give up */
void send_write_command(unsigned char *command, int command_length) {
	int i, retry, sent, checksum;
	unsigned char raw_buf[255];

	/* prepares the raw data */
	raw_buf[0] = 0x02;		/* STX byte */
	raw_buf[1] = (unsigned char)(command_length + 1);		/* data length + checksum */
	memcpy(raw_buf+2, command, command_length);
	command_length += 2;

	/* calculate checksum */
	checksum = 0;
	for (i = 1; i < command_length; i++) checksum += raw_buf[i];
	checksum = checksum % 256;
	raw_buf[command_length] = (unsigned char)checksum;
	command_length +=1;

	retry = 0;
	sent = 0;
	while ((sent != (command_length)) && (retry < 5)) {
		if (retry == 4) send_zeros();	/* last retry is preceded by a serial reset... */
		sent = ser_send_buf(upsfd, raw_buf, (command_length));
		if (sent != (command_length)) printf("Error sending command %d\n", raw_buf[2]);
		retry += 1;
	}
}


/* get the answer of a command from the ups */
int get_answer(unsigned char *data) {
	unsigned char my_buf[255];	/* packet has a maximum length of 256 bytes */
	int packet_length, checksum, i, res;
	/* Read STX byte */
	res = ser_get_char(upsfd, my_buf, 1, 0);
	if (res < 1) {
		ser_comm_fail("Receive error (STX): %d!!!\n", res);
		return -1;	
	}
	if (my_buf[0] != 0x02) {
		ser_comm_fail("Receive error (STX): packet not on start!!\n");
		return -1;	
	}
	/* Read data length byte */
	res = ser_get_char(upsfd, my_buf, 1, 0);
	if (res < 1) {
		ser_comm_fail("Receive error (length): %d!!!\n", res);
		return -1;	
	}
	packet_length = my_buf[0];
	if (packet_length < 2) {
		ser_comm_fail("Receive error (length): packet length %d!!!\n", packet_length);
		return -1;	
	}
	/* Try to read all the remainig bytes (packet_length) */
	res = ser_get_buf_len(upsfd, my_buf, packet_length, 1, 0);
	if (res != packet_length) {
		ser_comm_fail("Receive error (data): got %d bytes instead of %d!!!\n", res, packet_length);
		return -1;	
	}

	/* now we have the whole answer from the ups, we can checksum it 
	   checksum byte is equal to the sum modulus 256 of all the data bytes + packet_length
	   (no STX no checksum byte itself) */
	checksum = packet_length;
	for (i = 0; i < (packet_length - 1); i++) checksum += my_buf[i];  
	checksum = checksum % 256;
	if (my_buf[packet_length-1] != checksum) {
		ser_comm_fail("checksum error! got %x instead of %x, received %d bytes \n", my_buf[packet_length - 1], checksum, packet_length);
		dump_buffer(my_buf, packet_length);
		return -1;
	}
	packet_length-=1;		/* get rid of the checksum byte */
	memcpy(data, my_buf, packet_length);
	return packet_length;
}

/* send a read command and try get the answer, if something fails, it retries (5 times max)
   if it is on the 4th or 5th retry, it will flush the serial before sending commands
   it returns the length of the received answer or -1 in case of failure */
int command_read_sequence(unsigned char command, unsigned char *data) {
	int bytes_read = 0;
	int retry = 0;

	while ((bytes_read < 1) && (retry < 5)) {
		send_read_command(command);
		bytes_read = get_answer(data);
		if (retry > 2) ser_flush_in(upsfd, "", 0);
		retry += 1;
	}
	if ((data[0] != command) || (retry == 5)) {
		ser_comm_fail("Error executing command %d\n", command);
		dstate_datastale();
		return -1;
	}
	ser_comm_good();
	return bytes_read;
}

/* send a write command and try get the answer, if something fails, it retries (5 times max)
   if it is on the 4th or 5th retry, it will flush the serial before sending commands
   it returns the length of the received answer or -1 in case of failure */
int command_write_sequence(unsigned char *command, int command_length, unsigned char *answer) {
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_write_command(command, command_length);
		bytes_read = get_answer(answer);
		if (retry > 2) ser_flush_in(upsfd, "", 0);
		retry += 1;
	}
	if ((answer[0] != command[0]) || (retry == 5)) {
		ser_comm_fail("Error executing command N.%d\n", command[0]);
		dstate_datastale();
		return -1;
	}
	ser_comm_good();
	return bytes_read;
}

void upsdrv_initinfo(void)
{
	unsigned char my_answer[255];
	char serial[13];
	int res, i;

	/* Initial setup of variables */
#ifdef EXTRADATA
	 dstate_setinfo("output.power", "%d", -1);
	dstate_setflags("output.power", ST_FLAG_RW);
#endif
	 dstate_setinfo("output.voltage", "%d", -1);
	dstate_setflags("output.voltage", ST_FLAG_RW);
	 dstate_setinfo("output.current", "%d", -1);
	dstate_setflags("output.current", ST_FLAG_RW);
#ifdef EXTRADATA
	 dstate_setinfo("output.current.peak", "%2.2f", -1);	
	dstate_setflags("output.current.peak", ST_FLAG_RW);
	 dstate_setinfo("input.power", "%d", -1);
	dstate_setflags("input.power", ST_FLAG_RW);
#endif
	 dstate_setinfo("input.voltage", "%d", -1);
	dstate_setflags("input.voltage", ST_FLAG_RW);
#ifdef EXTRADATA
	 dstate_setinfo("input.current", "%2.2f", -1);
	dstate_setflags("input.current", ST_FLAG_RW);
	 dstate_setinfo("input.current.peak", "%2.2f", -1);	
	dstate_setflags("input.current.peak", ST_FLAG_RW);
#endif
	 dstate_setinfo("battery.voltage", "%d", -1);
	dstate_setflags("battery.voltage", ST_FLAG_RW);
#ifdef EXTRADATA
	 dstate_setinfo("battery.voltage.low", "%2.2f", -1);
	dstate_setflags("battery.voltage.low", ST_FLAG_RW);
	 dstate_setinfo("battery.voltage.exhaust", "%2.2f", -1);
	dstate_setflags("battery.voltage.exhaust", ST_FLAG_RW);
	 dstate_setinfo("ups.total.runtime", "retrieving...");
	dstate_setflags("ups.total.runtime", ST_FLAG_STRING | ST_FLAG_RW);
	  dstate_setaux("ups.total.runtime", 20);
	 dstate_setinfo("ups.inverter.runtime", "retrieving...");
	dstate_setflags("ups.inverter.runtime", ST_FLAG_STRING | ST_FLAG_RW);
	  dstate_setaux("ups.inverter.runtime", 20);
	 dstate_setinfo("ups.inverter.interventions", "%d", -1);
	dstate_setflags("ups.inverter.interventions", ST_FLAG_RW);
	 dstate_setinfo("battery.full.discharges", "%d", -1);
	dstate_setflags("battery.full.discharges", ST_FLAG_RW);
	 dstate_setinfo("ups.bypass.interventions", "%d", -1);
	dstate_setflags("ups.bypass.interventions", ST_FLAG_RW);
	 dstate_setinfo("ups.overheatings", "%d", -1);
	dstate_setflags("ups.overheatings", ST_FLAG_RW);
#endif
	 dstate_setinfo("ups.load", "%d", -1);
	dstate_setflags("ups.load", ST_FLAG_RW);
	 dstate_setinfo("ups.delay.shutdown", "%d", -1);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW);
	 dstate_setinfo("ups.delay.start", "%d", -1);
	dstate_setflags("ups.delay.start", ST_FLAG_RW);
	 dstate_setinfo("ups.temperature", "%d", -1);
	dstate_setflags("ups.temperature", ST_FLAG_RW);
	 dstate_setinfo("ups.test.result", "not yet done...");
	dstate_setflags("ups.test.result", ST_FLAG_STRING | ST_FLAG_RW);
	  dstate_setaux("ups.test.result", 20);
	
	/* UPS INFO READ */
	res = command_read_sequence(UPS_INFO, my_answer);
	if (res < 0) fatal_with_errno(EXIT_FAILURE, "Could not communicate with the ups");
	/* the manufacturer is hard coded into the driver, the model type is in the second 
		byte of the answer, the third byte identifies the model version */
	dstate_setinfo("ups.mfr", "Meta System");
	i = my_answer[1] * 10 + my_answer[2];
	switch (i) {	
		case 11:
			dstate_setinfo("ups.model", "%s", "HF Line (1 board)");
			nominal_power = 630;
			break;
		case 12:
			dstate_setinfo("ups.model", "%s", "HF Line (2 boards)");
			nominal_power = 1260;
			break;
		case 13:
			dstate_setinfo("ups.model", "%s", "HF Line (3 boards)");
			nominal_power = 1890;
			break;
		case 14:
			dstate_setinfo("ups.model", "%s", "HF Line (4 boards)");
			nominal_power = 2520;
			break;
		case 21:
			dstate_setinfo("ups.model", "%s", "ECO Network 750/1000");
			nominal_power = 500;
			break;	
		case 22:
			dstate_setinfo("ups.model", "%s", "ECO Network 1050/1500");
			nominal_power = 700;
			break;	
		case 23:
			dstate_setinfo("ups.model", "%s", "ECO Network 1500/2000");
			nominal_power = 1000;
			break;	
		case 24:
			dstate_setinfo("ups.model", "%s", "ECO Network 1800/2500");
			nominal_power = 1200;
			break;	
		case 25:
			dstate_setinfo("ups.model", "%s", "ECO Network 2100/3000");
			nominal_power = 1400;
			break;	
		case 31:
			dstate_setinfo("ups.model", "%s", "ECO 308");
			nominal_power = 500;
			break;	
		case 32:
			dstate_setinfo("ups.model", "%s", "ECO 311");
			nominal_power = 700;
			break;	
		case 44:
			dstate_setinfo("ups.model", "%s", "HF Line (4 boards)/2");
			nominal_power = 2520;
			break;
		case 45:
			dstate_setinfo("ups.model", "%s", "HF Line (5 boards)/2");
			nominal_power = 3150;
			break;
		case 46:
			dstate_setinfo("ups.model", "%s", "HF Line (6 boards)/2");
			nominal_power = 3780;
			break;
		case 47:
			dstate_setinfo("ups.model", "%s", "HF Line (7 boards)/2");
			nominal_power = 4410;
			break;
		case 48:
			dstate_setinfo("ups.model", "%s", "HF Line (8 boards)/2");
			nominal_power = 5040;
			break;
		case 51:
			dstate_setinfo("ups.model", "%s", "HF Millennium 810");
			nominal_power = 700;
			break;
		case 52:
			dstate_setinfo("ups.model", "%s", "HF Millennium 820");
			nominal_power = 1400;
			break;
		case 61:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 910");
			nominal_power = 700;
			break;
		case 62:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 920");
			nominal_power = 1400;
			break;
		case 63:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 930");
			nominal_power = 2100;
			break;
		case 64:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 940");
			nominal_power = 2800;
			break;
		case 74:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 940/2");
			nominal_power = 2800;
			break;
		case 75:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 950/2");
			nominal_power = 3500;
			break;
		case 76:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 960/2");
			nominal_power = 4200;
			break;
		case 77:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 970/2");
			nominal_power = 4900;
			break;
		case 78:
			dstate_setinfo("ups.model", "%s", "HF TOP Line 980/2");
			nominal_power = 5600;
			break;
		case 81:
			dstate_setinfo("ups.model", "%s", "ECO 508");
			nominal_power = 500;
			break;
		case 82:
			dstate_setinfo("ups.model", "%s", "ECO 511");
			nominal_power = 700;
			break;
		case 83:
			dstate_setinfo("ups.model", "%s", "ECO 516");
			nominal_power = 1000;
			break;
		case 84:
			dstate_setinfo("ups.model", "%s", "ECO 519");
			nominal_power = 1200;
			break;
		case 85:
			dstate_setinfo("ups.model", "%s", "ECO 522");
			nominal_power = 1400;
			break;
		case 91:
			dstate_setinfo("ups.model", "%s", "ECO 305 / Harviot 530 SX");
			nominal_power = 330;
			break;
		case 92:
			dstate_setinfo("ups.model", "%s", "ORDINATORE 2");
			nominal_power = 330;
			break;
		case 93:
			dstate_setinfo("ups.model", "%s", "Harviot 730 SX");
			nominal_power = 430;
			break;
		case 101:
			dstate_setinfo("ups.model", "%s", "ECO 308 SX / SX Interactive / Ordinatore");
			nominal_power = 500;
			break;
		case 102:
			dstate_setinfo("ups.model", "%s", "ECO 311 SX / SX Interactive");
			nominal_power = 700;
			break;
		case 111:
			dstate_setinfo("ups.model", "%s", "ally HF 800 / BI-TWICE 800");
			nominal_power = 560;
			break;
		case 112:
			dstate_setinfo("ups.model", "%s", "ally HF 1600");
			nominal_power = 1120;
			break;
		case 121:
			dstate_setinfo("ups.model", "%s", "ally HF 1000 / BI-TWICE 1000");
			nominal_power = 700;
			break;
		case 122:
			dstate_setinfo("ups.model", "%s", "ally HF 2000");
			nominal_power = 1400;
			break;
		case 131:
			dstate_setinfo("ups.model", "%s", "ally HF 1250 / BI-TWICE 1250");
			nominal_power = 875;
			break;
		case 132:
			dstate_setinfo("ups.model", "%s", "ally HF 2500");
			nominal_power = 1750;
			break;
		case 141:
			dstate_setinfo("ups.model", "%s", "Megaline 1250");
			nominal_power = 875;
			break;
		case 142:
			dstate_setinfo("ups.model", "%s", "Megaline 2500");
			nominal_power = 1750;
			break;
		case 143:
			dstate_setinfo("ups.model", "%s", "Megaline 3750");
			nominal_power = 2625;
			break;
		case 144:
			dstate_setinfo("ups.model", "%s", "Megaline 5000");
			nominal_power = 3500;
			break;
		case 154:
			dstate_setinfo("ups.model", "%s", "Megaline 5000 / 2");
			nominal_power = 3500;
			break;
		case 155:
			dstate_setinfo("ups.model", "%s", "Megaline 6250 / 2");
			nominal_power = 4375;
			break;
		case 156:
			dstate_setinfo("ups.model", "%s", "Megaline 7500 / 2");
			nominal_power = 5250;
			break;
		case 157:
			dstate_setinfo("ups.model", "%s", "Megaline 8750 / 2");
			nominal_power = 6125;
			break;
		case 158:
			dstate_setinfo("ups.model", "%s", "Megaline 10000 / 2");
			nominal_power = 7000;
			break;

		default:
			fatal_with_errno(EXIT_FAILURE, "Unknown UPS");
			break;
	} 
		
	/* Get the serial number */
	memcpy(serial, my_answer + 7, res - 7);
	/* serial number start from the 8th byte */
	serial[12]=0;		/* terminate string */
	dstate_setinfo("ups.serial", "%s", serial);
	
	/* get the ups firmware. The major number is in the 5th byte, the minor is in the 6th */
	dstate_setinfo("ups.firmware", "%u.%u", my_answer[5], my_answer[6]);

	printf("Detected %s [%s] v.%s on %s\n", dstate_getinfo("ups.model"), dstate_getinfo("ups.serial"), dstate_getinfo("ups.firmware"), device_path);
	
	/* Add instant commands */
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("test.failure.start");
	dstate_addcmd("test.failure.stop");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.mute");
	dstate_addcmd("beeper.on");
	dstate_addcmd("beeper.off");
	upsh.instcmd = instcmd;
	return;
}

void upsdrv_updateinfo(void)
{
	int res, int_num;
#ifdef EXTRADATA
	int day, hour, minute;
#endif
	float float_num;
	long int long_num;
	unsigned char my_answer[255];
	
	/* GET Output data */
	res = command_read_sequence(UPS_OUTPUT_DATA, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		/* Active power */
		int_num = get_word(&my_answer[1]);
		if (nominal_power != 0) {
			float_num = (float)((int_num * 100)/nominal_power);
			dstate_setinfo("ups.load", "%2.1f", float_num);
		} else {
			dstate_setinfo("ups.load", "%s", "not available");
		}	
#ifdef EXTRADATA
		dstate_setinfo("output.power", "%d", int_num);
#endif
		/* voltage */
		int_num = get_word(&my_answer[3]);
		if (int_num > 0) dstate_setinfo("output.voltage", "%d", int_num);
		if (int_num == -1) dstate_setinfo("output.voltage", "%s", "overrange");
		if (int_num == -2) dstate_setinfo("output.voltage", "%s", "not available");
		/* current */
		float_num = get_word(&my_answer[5]);
		if (float_num == -1) dstate_setinfo("output.current", "%s", "overrange");
		if (float_num == -2) dstate_setinfo("output.current", "%s", "not available");
		if (float_num > 0) {
			float_num = (float)(float_num/10);
			dstate_setinfo("output.current", "%2.2f", float_num);
		}
#ifdef EXTRADATA
		/* peak current */
		float_num = get_word(&my_answer[7]);
		if (float_num == -1) dstate_setinfo("output.current.peak", "%s", "overrange");
		if (float_num == -2) dstate_setinfo("output.current.peak", "%s", "not available");
		if (float_num > 0) {
			float_num = (float)(float_num/10);
			dstate_setinfo("output.current.peak", "%2.2f", float_num);
		}
		
#endif
	}
		
	/* GET Input data */
	res = command_read_sequence(UPS_INPUT_DATA, my_answer);
	if (res < 0){
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
#ifdef EXTRADATA
		/* Active power */
		int_num = get_word(&my_answer[1]);
		if (int_num > 0) dstate_setinfo("input.power", "%d", int_num);
		if (int_num == -1) dstate_setinfo("input.power", "%s", "overrange");
		if (int_num == -2) dstate_setinfo("input.power", "%s", "not available");
#endif
		/* voltage */
		int_num = get_word(&my_answer[3]);
		if (int_num > 0) dstate_setinfo("input.voltage", "%d", int_num);
		if (int_num == -1) dstate_setinfo("input.voltage", "%s", "overrange");
		if (int_num == -2) dstate_setinfo("input.voltage", "%s", "not available");
#ifdef EXTRADATA
		/* current */
		float_num = get_word(&my_answer[5]);
		if (float_num == -1) dstate_setinfo("input.current", "%s", "overrange");
		if (float_num == -2) dstate_setinfo("input.current", "%s", "not available");
		if (float_num > 0) {
			float_num = (float)(float_num/10);
			dstate_setinfo("input.current", "%2.2f", float_num);
		}
		/* peak current */
		float_num = get_word(&my_answer[7]);
		if (float_num == -1) dstate_setinfo("input.current.peak", "%s", "overrange");
		if (float_num == -2) dstate_setinfo("input.current.peak", "%s", "not available");
		if (float_num > 0) {
			float_num = (float)(float_num/10);
			dstate_setinfo("input.current.peak", "%2.2f", float_num);
		}
#endif
	}
	
	
	/* GET Battery data */
	res = command_read_sequence(UPS_BATTERY_DATA, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		/* Actual value */
		float_num = get_word(&my_answer[1]);
		float_num = (float)(float_num/10);
		dstate_setinfo("battery.voltage", "%2.2f", float_num);
#ifdef EXTRADATA
		/* reserve threshold */
		float_num = get_word(&my_answer[3]);
		float_num = (float)(float_num/10);
		dstate_setinfo("battery.voltage.low", "%2.2f", float_num);
		/* exhaust threshold */
		float_num = get_word(&my_answer[5]);
		float_num = (float)(float_num/10);
		dstate_setinfo("battery.voltage.exhaust", "%2.2f", float_num);
#endif
	}
	
#ifdef EXTRADATA
	/* GET history data */
	res = command_read_sequence(UPS_HISTORY_DATA, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		/* ups total runtime */
		long_num = get_long(&my_answer[1]);
		day = (int)(long_num / 86400);
		long_num -= (long)(day*86400);
		hour = (int)(long_num / 3600);
		long_num -= (long)(hour*3600);
		minute = (int)(long_num / 60);
		long_num -= (minute*60);
		dstate_setinfo("ups.total.runtime", "%d days %dh %dm %lds", day, hour, minute, long_num);
		
		/* ups inverter runtime */
		long_num = get_long(&my_answer[5]);
		day = (int)(long_num / 86400);
		long_num -= (long)(day*86400);
		hour = (int)(long_num / 3600);
		long_num -= (long)(hour*3600);
		minute = (int)(long_num / 60);
		long_num -= (minute*60);
		dstate_setinfo("ups.inverter.runtime", "%d days %dh %dm %lds", day, hour, minute, long_num);
		/* ups inverter interventions */
		dstate_setinfo("ups.inverter.interventions", "%d", get_word(&my_answer[9]));
		/* battery full discharges */
		dstate_setinfo("battery.full.discharges", "%d", get_word(&my_answer[11]));
		/* ups bypass / stabilizer interventions */
		int_num = get_word(&my_answer[13]);
		if (int_num == -2) dstate_setinfo("ups.bypass.interventions", "%s", "not avaliable");
		if (int_num >= 0) dstate_setinfo("ups.bypass.interventions", "%d", int_num);
		/* ups overheatings */
		int_num = get_word(&my_answer[15]);
		if (int_num == -2) dstate_setinfo("ups.overheatings", "%s", "not avalilable");
		if (int_num >= 0) dstate_setinfo("ups.overheatings", "%d", int_num);
	}
#endif
	
	/* GET times on battery */
	res = command_read_sequence(UPS_GET_TIMES_ON_BATTERY, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		autorestart = my_answer[5];
	}
	
	
	/* GET schedule */
	res = command_read_sequence(UPS_GET_SCHEDULING, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		/* time remaining to shutdown */
		long_num = get_long(&my_answer[1]);
		if (long_num == -1) {
			dstate_setinfo("ups.delay.shutdown", "%d", 120);	
		} else {
			dstate_setinfo("ups.delay.shutdown", "%ld", long_num);
		}
		/* time remaining to restart  */
		long_num = get_long(&my_answer[5]);
		if (long_num == -1) {
			dstate_setinfo("ups.delay.start", "%d", 0);	
		} else {
			dstate_setinfo("ups.delay.start", "%ld", long_num);
		}	
	}
	
	
	/* GET ups status */
	res = command_read_sequence(UPS_STATUS, my_answer);
	if (res < 0) {
		printf("Could not communicate with the ups");
		dstate_datastale();
	} else {
		/* ups temperature */
		my_answer[3] -=128;
		if (my_answer[3] > 0) {
			dstate_setinfo("ups.temperature", "%d", my_answer[3]);
		} else {
			dstate_setinfo("ups.temperature", "%s", "not available");
		}	
		/* Status */
		status_init();
		switch (my_answer[1]) {	/* byte 1 = STATUS */
			case 0x00:
				status_set("OL"); /* running on mains power */
				break;
			case 0x01:
				status_set("OB"); /* running on battery power */
				break;
			case 0x02:
				status_set("LB"); /* battery reserve */
				break;
			case 0x03:			/* bypass engaged */
			case 0x04:			/* manual bypass engaged */
				status_set("BY");
				break;
			default:
				printf("status unknown \n");
				break;
		} 
		switch (my_answer[2]) {		/* byte 2 = FAULTS */
			case 0x00:				/* all right */
				break;
			case 0x01:				/* overload */
				status_set("OVER");
				break;
			case 0x02:				/* overheat */
				break;
			case 0x03:				/* hardware fault */
				break;
			case 0x04:				/* battery charger failure (overcharging) */
				break;
			case 0x05:				/* replace batteries */
				status_set("RB");
				break;
			default:
				printf("status unknown \n");
				break;
		}
		status_commit();
		dstate_dataok();
	}
	return;
}

void upsdrv_shutdown(void)
{
	unsigned char command[10], answer[10];

	/* Ensure that the ups is configured for automatically
	   restart after a complete battery discharge 
	   and when the power comes back after a shutdown */
	if (! autorestart) {
		command[0]=UPS_SET_TIMES_ON_BATTERY;
		command[1]=0x00;					/* max time on  */ 
		command[2]=0x00;					/* battery */
		
		command[3]=0x00;					/* max time after */
		command[4]=0x00;					/* battery reserve */
		
		command[5]=0x01;					/* autorestart after battery depleted enabled */
		command_write_sequence(command, 6, answer);
	}

	/* shedule a shutdown in 120 seconds */
	command[0]=UPS_SET_SCHEDULING;		
	command[1]=0x96;					/* remaining  */
	command[2]=0x00;					/* time		 */
	command[3]=0x00;					/* to */
	command[4]=0x00;					/* shutdown 150 secs */

	/* restart time has been set to 1 instead of 0 for avoiding
		a bug in some ups firmware */
	command[5]=0x01;					/* programmed */
	command[6]=0x00;					/* time		 */
	command[7]=0x00;					/* to */
	command[8]=0x00;					/* restart 1 sec */
	command_write_sequence(command, 9, answer);

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}


static int instcmd(const char *cmdname, const char *extra)
{
	unsigned char command[10], answer[10];
	int res;

	if (!strcasecmp(cmdname, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.mute' for this driver");
		return instcmd("beeper.mute", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	if (!strcasecmp(cmdname, "shutdown.return")) {
		/* Same stuff as upsdrv_shutdown() */
		if (! autorestart) {
			command[0]=UPS_SET_TIMES_ON_BATTERY;
			command[1]=0x00;					/* max time on  */ 
			command[2]=0x00;					/* battery */
			command[3]=0x00;					/* max time after */
			command[4]=0x00;					/* battery reserve */
			command[5]=0x01;					/* autorestart after battery depleted enabled */
			command_write_sequence(command, 6, answer);
		}
		/* shedule a shutdown in 30 seconds */
		command[0]=UPS_SET_SCHEDULING;		
		command[1]=0x1e;					/* remaining  */
		command[2]=0x00;					/* time		 */
		command[3]=0x00;					/* to */
		command[4]=0x00;					/* shutdown 30 secs */

		command[5]=0x01;					/* programmed */
		command[6]=0x00;					/* time		 */
		command[7]=0x00;					/* to */
		command[8]=0x00;					/* restart 1 sec */
		command_write_sequence(command, 9, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		/* shedule a shutdown in 30 seconds with no restart (-1) */
		command[0]=UPS_SET_SCHEDULING;		
		command[1]=0x1e;					/* remaining  */
		command[2]=0x00;					/* time		 */
		command[3]=0x00;					/* to */
		command[4]=0x00;					/* shutdown 150 secs */
				
		command[5]=0xff;					/* programmed */
		command[6]=0xff;					/* time		 */
		command[7]=0xff;					/* to */
		command[8]=0xff;					/* restart -1 no restart*/
		command_write_sequence(command, 9, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "shutdown.stop")) {
		/* set shutdown and restart time to -1 (no shutdown, no restart) */
		command[0]=UPS_SET_SCHEDULING;		
		command[1]=0xff;					/* remaining  */
		command[2]=0xff;					/* time		 */
		command[3]=0xff;					/* to */
		command[4]=0xff;					/* shutdown -1 (no shutdown) */
				
		command[5]=0xff;					/* programmed */
		command[6]=0xff;					/* time		 */
		command[7]=0xff;					/* to */
		command[8]=0xff;					/* restart -1 no restart */
		command_write_sequence(command, 9, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.failure.start")) {
		/* force ups on battery power */
		command[0]=UPS_SET_BATTERY_TEST;	
		command[1]=0x01;					
		/* 0 = perform battery test
		   1 = force UPS on battery power
		   2 = restore standard mode (mains power) */
		command_write_sequence(command, 2, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.failure.stop")) {
		/* restore standard mode (mains power) */
		command[0]=UPS_SET_BATTERY_TEST;
		command[1]=0x02;					
		/* 0 = perform battery test
		   1 = force UPS on battery power
		   2 = restore standard mode (mains power) */
		command_write_sequence(command, 2, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		/* launch battery test */
		command[0]=UPS_SET_BATTERY_TEST;		
		command[1]=0x00;					
		/* 0 = perform battery test
		   1 = force UPS on battery power
		   2 = restore standard mode (mains power) */
		send_write_command(command, 2);
		sleep(15);
		res = get_answer(answer);
		switch (answer[1]) {		/* byte 1 = Test result */
			case 0x00:				/* all right */
				dstate_setinfo("ups.test.result", "OK");
				break;
			case 0x01:				
				dstate_setinfo("ups.test.result", "Battery charge: 20%%");
				break;
			case 0x02:				
				dstate_setinfo("ups.test.result", "Battery charge: 40%%");
				break;
			case 0x03:				
				dstate_setinfo("ups.test.result", "Battery charge: 60%%");
				break;
			case 0x04:				
				dstate_setinfo("ups.test.result", "Battery charge: 80%%");
				break;
			case 0x05:				
				dstate_setinfo("ups.test.result", "Battery charge: 100%%");
				break;
			case 0xfe:				
				dstate_setinfo("ups.test.result", "Bad battery pack: replace");
				break;
			default:
				dstate_setinfo("ups.test.result", "Impossible to test");
				break;
		}
		dstate_dataok();
		upslogx(LOG_NOTICE, "instcmd: test battery returned with %d bytes", res);
		upslogx(LOG_NOTICE, "test battery byte 1 = %x", answer[1]);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.enable")) {
		/* set buzzer to not muted */
		command[0]=UPS_SET_BUZZER_MUTE;		
		command[1]=0x00;					
		/* 0 = not muted
		   1 = muted
		   2 = read current status */
		command_write_sequence(command, 2, answer);
		return STAT_INSTCMD_HANDLED;
	}

	if (!strcasecmp(cmdname, "beeper.mute")) {
		/* set buzzer to muted */
		command[0]=UPS_SET_BUZZER_MUTE;		
		command[1]=0x01;					
		/* 0 = not muted
		   1 = muted
		   2 = read current status */
		command_write_sequence(command, 2, answer);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */

}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path); 
	ser_set_speed(upsfd, device_path, B2400); 
	send_zeros();
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path); 
}
