/*  nutdrv_mqtt.c - Driver for MQTT
 *
 *  Copyright (C)
 *    2015  Arnaud Quette <ArnaudQuette@Eaton.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * TODO list:
 * - everything
 */

#include "main.h"
#include <mosquitto.h>

#define DRIVER_NAME	"MQTT driver"
#define DRIVER_VERSION	"0.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <ArnaudQuette@Eaton.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* Variables */
struct mosquitto *mosq = NULL;
char *topic = NULL;
char *client_id = NULL;
bool clean_session = true;

/* Callbacks */
void mqtt_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void mqtt_connect_callback(struct mosquitto *mosq, void *obj, int result);
void mqtt_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
int mqtt_reconnect();

/* NUT routines */

void upsdrv_initinfo(void)
{
	upsdebugx(1, "%s...", __func__);

	/* try to detect the UPS here - call fatal_with_errno(EXIT_FAILURE, ...)
	 * or fatalx(EXIT_FAILURE, ...) if it fails */

	/* dstate_setinfo("ups.mfr", "skel manufacturer"); */
	/* dstate_setinfo("ups.model", "longrun 15000"); */
	/* note: for a transition period, these data are redundant! */
	/* dstate_setinfo("device.mfr", "skel manufacturer"); */
	/* dstate_setinfo("device.model", "longrun 15000"); */


	/* upsh.instcmd = instcmd; */
}

void upsdrv_updateinfo(void)
{
	int rc;

	upsdebugx(1, "%s...", __func__);

	rc = mosquitto_loop(mosq, 2 /* timeout */, 1 /* max_packets */);
	if(rc) {
		upsdebugx(1, "Error: %s", mosquitto_strerror(rc));
		if (rc == MOSQ_ERR_CONN_LOST)
			mqtt_reconnect();
	}
	/* int flags; */
	/* char temp[256]; */

	/* ser_sendchar(upsfd, 'A'); */
	/* ser_send(upsfd, "foo%d", 1234); */
	/* ser_send_buf(upsfd, bincmd, 12); */

	/* 
	 * ret = ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS);
	 *
	 * if (ret < STATUS_LEN) {
	 * 	upslogx(LOG_ERR, "Short read from UPS");
	 *	dstate_datastale();
	 *	return;
	 * }
	 */

	/* dstate_setinfo("var.name", ""); */

	/* if (ioctl(upsfd, TIOCMGET, &flags)) {
	 *	upslog_with_errno(LOG_ERR, "TIOCMGET");
	 *	dstate_datastale();
	 *	return;
	 * }
	 */

	/* status_init();
	 *
	 * if (ol)
	 * 	status_set("OL");
	 * else
	 * 	status_set("OB");
	 * ...
	 *
	 * status_commit();
	 *
	 * dstate_dataok();
	 */

	/*
	 * poll_interval = 2;
	 */
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x topic=<some value>' */
	addvar(VAR_VALUE, "topic", "Specify the MQTT topic to subscribe to");
}

void upsdrv_initups(void)
{
	char hostname[256];
	int len;
	char *bind_address = NULL;
	int keepalive = 60;
	int rc;
	int port = 1883;
	char err[1024];

	upsdebugx(1, "%s...", __func__);

	topic = getval("topic");
	if (topic == NULL)
		fatalx(EXIT_FAILURE, "No topic specified, aborting");

	mosquitto_lib_init();

	/* Build client_id for subscription */
	hostname[0] = '\0';
	gethostname(hostname, 256);
	hostname[255] = '\0';
	len = strlen("mosqsub/-") + 6 + strlen(hostname);
	client_id = malloc(len);
	if(!client_id){
		mosquitto_lib_cleanup();
		fatalx(EXIT_FAILURE, "Error: Out of memory");
	}
	snprintf(client_id, len, "mosqsub/%d-%s", getpid(), hostname);

	upsdebugx(2, "subscribing using id = %s", client_id);

	mosq = mosquitto_new(client_id, clean_session, NULL);
	if(!mosq){
		mosquitto_lib_cleanup();
		switch(errno){
			case ENOMEM:
				fatalx(EXIT_FAILURE, "Error: Out of memory");
				break;
			case EINVAL:
				fatalx(EXIT_FAILURE, "Error: Invalid id and/or clean_session");
				break;
		}
	}

	mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
	mosquitto_message_callback_set(mosq, mqtt_message_callback);
	mosquitto_subscribe_callback_set(mosq, mqtt_subscribe_callback);
	
	//rc = mosquitto_connect_srv(mosq, device_path, keepalive, bind_address);
	rc = mosquitto_connect_bind(mosq, device_path, port, keepalive, bind_address);
	if(rc){

		if(rc == MOSQ_ERR_ERRNO){
#ifndef WIN32
			strerror_r(errno, err, 1024);
#else
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errno, 0, (LPTSTR)&err, 1024, NULL);
#endif
			upsdebugx(1, "Error: %s", err);
		}else{
			upsdebugx(1, "Unable to connect (%d)", rc);
		}

		mosquitto_lib_cleanup();
		return rc;
	}

/*	rc = mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();

	if(rc){
		fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
	}
*/

	/* upsfd = ser_open(device_path); */
	/* ser_set_speed(upsfd, device_path, B1200); */

	/* probe ups type */

	/* to get variables and flags from the command line, use this:
	 *
	 * first populate with upsdrv_makevartable() above, then...
	 *
	 *                   set flag foo : /bin/driver -x foo
	 * set variable 'cable' to '1234' : /bin/driver -x cable=1234
	 *
	 * to test flag foo in your code:
	 *
	 * 	if (testvar("foo"))
	 * 		do_something();
	 *
	 * to show the value of cable:
	 *
	 *      if ((cable = getval("cable")))
	 *		printf("cable is set to %s\n", cable);
	 *	else
	 *		printf("cable is not set!\n");
	 *
	 * don't use NULL pointers - test the return result first!
	 */

	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */

	/* don't try to detect the UPS here */
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "%s...", __func__);

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
}

/*
 * Mosquitto specific routines
 ******************************/

void mqtt_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	if(!result) {
		upsdebugx(1, "Connected to host %s", device_path);
		upsdebugx(1, "Subscribing to topic %s", topic);
		mosquitto_subscribe(mosq, NULL, topic, 0 /* topic_qos */);
	}
	else
		upsdebugx(1, "%s", mosquitto_connack_string(result));
}

void mqtt_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;

	/* TODO: build a string with granted_qos */
	upsdebugx(1, "Subscribed to topic %s (msg id: %d) with QOS: %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++) {
		upsdebugx(1, ", %d", granted_qos[i]);
	}
}


void mqtt_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	int i;
	bool res;
	char *mqtt_message = NULL;

//	if(message->retain && ud->no_retain) return;
/*	if(ud->filter_outs){
		for(i=0; i<ud->filter_out_count; i++){
			mosquitto_topic_matches_sub(ud->filter_outs[i], message->topic, &res);
			if(res) return;
		}
	}
*/
	if(message->payloadlen) {
		upsdebugx(1, "Message received on topic[%s]", message->topic);
		mqtt_message = malloc(message->payloadlen +1);
		if(mqtt_message){
			snprintf(mqtt_message, message->payloadlen +1, "%s", (char*)message->payload);
			upsdebugx(1, "Message: %s", mqtt_message);
			//fwrite(message->payload, 1, message->payloadlen, stdout);
		}
		else
			upsdebugx(1, "Error while retrieving message");
	}
}

int mqtt_reconnect()
{
	return mosquitto_reconnect(mosq);
}
