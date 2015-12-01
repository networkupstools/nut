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
#define DRIVER_VERSION	"0.02"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <ArnaudQuette@Eaton.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};


typedef struct {
	char  *topic_name;
	int   topic_qos; /* Default to QOS 0 */
} topic_info_t;

/* Variables */
struct mosquitto *mosq = NULL;
topic_info_t *topics[10] = { NULL }; /* Max of 10 topics! */
char *client_id = NULL;
bool clean_session = false; //true;

/* Callbacks */
void mqtt_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void mqtt_connect_callback(struct mosquitto *mosq, void *obj, int result);
void mqtt_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
int mqtt_reconnect();

/* NUT routines */

void upsdrv_initinfo(void)
{
	int rc;

	upsdebugx(1, "%s...", __func__);

	rc = mosquitto_loop(mosq, 2 /* timeout */, 10 /* max_packets */);
	if(rc) {
		upsdebugx(1, "Error: %s", mosquitto_strerror(rc));
		if (rc == MOSQ_ERR_CONN_LOST)
			mqtt_reconnect();
	}

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

	rc = mosquitto_loop(mosq, 2 /* timeout */, 10 /* max_packets */);
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
	addvar(VAR_VALUE, "topics", "Specify the MQTT topic(s) to subscribe to, optionally with QOS");
	addvar(VAR_VALUE, "client_id", "Specify the MQTT client ID");
	addvar(VAR_FLAG, "clean_session", "When set, client session won't persist on the broker");
}

void upsdrv_initups(void)
{
	char hostname[256];
	int len;
	char *bind_address = NULL;
	int keepalive = 60;
	int rc, i;
	int port = 1883;
	char err[1024];
	char *arg_topics = NULL;
	char *topic_ptr, *qos_ptr;
	char *cur_topic_str;

	upsdebugx(1, "%s... with broker %s", __func__, device_path);

	/* Get configuration points */
	/* Topic(s) */
	arg_topics = getval("topics");
	if (arg_topics == NULL) {
		fatalx(EXIT_FAILURE, "No topic specified, aborting");
	}
	else {
		/* Split multiple topics
		 * Format: topic[:qos][,topic[:qos]]... */
	 	topic_ptr = strtok(arg_topics, ",");
		for (i = 0; topic_ptr; i++) {
			
			cur_topic_str = xstrdup(topic_ptr);
	
			qos_ptr = strchr(cur_topic_str, ':');
			topics[i] = (topic_info_t *)malloc(sizeof(topic_info_t));
			if (topics[i]) {
				/* Process QOS */
				if (qos_ptr) {
					*qos_ptr = '\0';
					topics[i]->topic_qos = atoi(qos_ptr+1);
				}
				else {
					upsdebugx(3, "No QOS specified, defaulting to 0");
					topics[i]->topic_qos = 0;
				}
				/* Process topic */
				topics[i]->topic_name = xstrdup(cur_topic_str);

				upsdebugx(1, "Adding topic '%s' with QOS %i",
							topics[i]->topic_name,
							topics[i]->topic_qos);
				/* Cleanup */
				free(cur_topic_str);
			}
			else {
				free(cur_topic_str);
				fatalx(EXIT_FAILURE, "Can't allocate memory for topics");
			}
			/* Get next topic */
			topic_ptr = strtok(NULL, ",");
		}

	}

	/* Should we set clean_session */
	if (testvar ("clean_session")) {
		upsdebugx(2, "clean_session set to true, as per request");
		clean_session = true;
	}

	/* Initialize Mosquitto */
	mosquitto_lib_init();

	/* Client id */
	client_id = getval("client_id");
	if (client_id == NULL) {
		/* Build client_id for subscription */
		hostname[0] = '\0';
		gethostname(hostname, 256);
		hostname[255] = '\0';
		len = strlen("nutdrv_mqtt/-") + 6 + strlen(hostname);
		client_id = malloc(len);
		if(!client_id){
			mosquitto_lib_cleanup();
			fatalx(EXIT_FAILURE, "Error: Out of memory");
		}
		snprintf(client_id, len, "nutdrv_mqtt/%d-%s", getpid(), hostname);
	}
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

	/* Setup callbacks for connection, subscription and messages */
	mosquitto_connect_callback_set(mosq, mqtt_connect_callback);
	mosquitto_subscribe_callback_set(mosq, mqtt_subscribe_callback);
	mosquitto_message_callback_set(mosq, mqtt_message_callback);
	
	//rc = mosquitto_connect_srv(mosq, device_path, keepalive, bind_address);
	/* Connect to the broker */
	rc = mosquitto_connect_bind(mosq, device_path, port, keepalive, bind_address);
	if(rc) {

		if(rc == MOSQ_ERR_ERRNO) {
#ifndef WIN32
			if (strerror_r(errno, err, 1024) == 0)
#else
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errno, 0, (LPTSTR)&err, 1024, NULL);
#endif
				upsdebugx(1, "Error: %s", err);
		}else{
			upsdebugx(1, "Unable to connect (%d)", rc);
		}

		mosquitto_lib_cleanup();
		fatalx(EXIT_FAILURE, "Error at init time");
	}

	/* First loop to allow connection / subscription */
	rc = mosquitto_loop(mosq, 1 /* timeout */, 10 /* max_packets */);
	if(rc) {
		upsdebugx(1, "Error: %s", mosquitto_strerror(rc));
		if (rc == MOSQ_ERR_CONN_LOST)
			mqtt_reconnect();
	}
}

void upsdrv_cleanup(void)
{
	int i;

	upsdebugx(1, "%s...", __func__);

	/* TODO: unsubscribe... */

	for (i = 0; topics[i]; i++) {
		
		if (topics[i]->topic_name)
			free(topics[i]->topic_name);
		
		free(topics[i]);
	}

	/* Cleanup Mosquitto */
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
}

/*
 * Mosquitto specific routines
 ******************************/

void mqtt_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	int i;

	if(!result) {
		upsdebugx(1, "Connected to host %s", device_path);
		/* Subscribe to all provided topics */
		for (i = 0; topics[i]; i++) {
			if (topics[i]->topic_name)
				upsdebugx(1, "Subscribing to topic %s", topics[i]->topic_name);
				mosquitto_subscribe(mosq, NULL,
									topics[i]->topic_name,
									topics[i]->topic_qos);
		}
		
	}
	else
		upsdebugx(1, "%s", mosquitto_connack_string(result));
}

void mqtt_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;

	/* TODO: build a string with granted_qos */
	upsdebugx(1, "Subscribed to topic %s (msg id: %d) with QOS: %d",
				topics[mid-1]->topic_name, /* Not sure it's actually stable!! */
				mid, granted_qos[0]);
	for(i = 1; i < qos_count; i++) {
		upsdebugx(1, ", %d", granted_qos[i]);
	}
}


void mqtt_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
/*	int i;
	bool res; */
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
