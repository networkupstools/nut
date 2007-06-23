/*
 * powercom.c - model specific routines for following units:
 *  -Trust 425/625
 *  -Powercom
 *  -Advice Partner/King PR750
 *    See http://www.advice.co.il/product/inter/ups.html for its specifications.
 *    This model is based on PowerCom (www.powercom.com) models.
 *  -Socomec Sicon Egys 420
 *
 * $Id$
 *
 * Copyrights:
 * (C) 2002 Simon Rozman <simon@rozman.net>
 * (C) 1999  Peter Bieringer <pb@bieringer.de>
 *                              
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *                            
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *    
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * 
 */ 

#include "main.h"
#include "serial.h"
#include "powercom.h"

#define POWERCOM_DRIVER_VERSION      "$ Revision: 0.5 $"
#define NUM_OF_SUBTYPES              (sizeof (types) / sizeof (*types))

/* variables used by module */
static unsigned char raw_data[MAX_NUM_OF_BYTES_FROM_UPS]; /* raw data reveived from UPS */
static unsigned int linevoltage = 230U; /* line voltage, can be defined via command line option */
static const char *manufacturer = "PowerCom";
static const char *modelname    = "Unknown";
static const char *serialnumber = "Unknown";
static unsigned int type = 0;

/* forward declaration of functions used to setup flow control */
static void dtr0rts1 (void);
static void dtr1 (void);
static void no_flow_control (void);

/* struct defining types */
static struct type types[] = {
        {
                "Trust",
                11,
				{  "dtr0rts1", dtr0rts1 },
                { { 5U, 0U }, { 7U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'n' },
                {  0.00020997, 0.00020928 },
                {  6.1343, -0.3808,  4.3110,  0.1811 },
                {  5.0000,  0.3268,  -825.00,  4.5639, -835.82 },
                {  1.9216, -0.0977,  0.9545,  0.0000 },
        },
        {
                "KP625AP",
                16,
				{  "dtr0rts1", dtr0rts1 },
                { { 5U, 0x80U }, { 7U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'n' },
                {  0.00020997, 0.00020928 },
                {  6.1343, -0.3808,  4.3110,  0.1811 },
                {  5.0000,  0.3268,  -825.00,  4.5639, -835.82 },
                {  1.9216, -0.0977,  0.9545,  0.0000 },
        },
        {
                "KIN2200AP",
                16,
				{  "dtr0rts1", dtr0rts1 },
                { { 7U, 0U }, { 8U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'n' },
                {  0.00020997, 0.0 },
                {  6.1343, -0.3808,  1.075,  0.1811 },
                {  5.0000,  0.3268,  -825.00,  0.46511, 0 },
                {  1.9216, -0.0977,  0.82857,  0.0000 },
        },
        {
                "Egys",
                16,
				{  "no_flow_control", no_flow_control },
                { { 5U, 0x80U }, { 7U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'n' },
                {  0.00020997, 0.00020928 },
                {  6.1343, -0.3808,  1.3333,  0.6667 },
                {  5.0000,  0.3268,  -825.00,  2.2105, -355.37 },
                {  1.9216, -0.0977,  0.9545,  0.0000 },
        },
        {
                "KIN525AP",
                16,
				{  "dtr1", dtr1 },
                { { 5U, 0x80U }, { 7U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'y' },
                {  0.00020997, 0.00020928 },
                {  6.1343, -0.3808,  4.3110,  0.1811 },
                {  5.0000,  0.3268,  -825.00,  4.5639, -835.82 },
                {  1.9216, -0.0977,  0.9545,  0.0000 },
        },
        {
                "KIN1500AP",
                16,
				{  "no_flow_control", no_flow_control },
                { { 7U, 0U }, { 8U, 0U }, { 8U, 0U } },
                { { 0U, 10U }, 'n' },
                {  0.00020997, 0.0 },
                {  6.1343, -0.3808,  1.075,  0.1811 },
                {  5.0000,  0.3268,  -825.00,  0.46511, 0 },
                {  1.9216, -0.0977,  0.82857,  0.0000 },
        },
};


/*
 * local used functions
 */

static void nut_shutdown(void)
{
	printf ("Initiating UPS shutdown!\n");
        
	ser_send_char (upsfd, SHUTDOWN);
	if (types[type].shutdown_arguments.minutesShouldBeUsed != 'n') 
	    ser_send_char (upsfd, types[type].shutdown_arguments.delay[0]);
	ser_send_char (upsfd, types[type].shutdown_arguments.delay[1]);
			
	exit (0);
}

/* registered instant commands */
static int instcmd (const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.start")) { 
	    ser_send_char (upsfd, BATTERY_TEST);
	    return STAT_INSTCMD_HANDLED;
    }

    upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
    return STAT_INSTCMD_UNKNOWN;
}

/* set DTR and RTS lines on a serial port to supply a passive
 * serial interface: DTR to 0 (-V), RTS to 1 (+V)
 */
static void dtr0rts1 (void)
{
	upsdebugx(2, "DTR => 0, RTS => 1");

	/* set DTR to low and RTS to high */
	ser_set_dtr(upsfd, 0);
	ser_set_rts(upsfd, 1);
}

/* set DTR line on a serial port for KIN525AP
 * serial interface: DTR to 1 (+V)
 */
static void dtr1 (void)
{
	upsdebugx(2, "DTR => 1");

	/* set DTR to high */
	ser_set_dtr(upsfd, 1);
}

/* clear any flow control */
static void no_flow_control (void)
{
	struct termios tio;
	
	tcgetattr (upsfd, &tio);
	
	tio.c_iflag &= ~ (IXON | IXOFF);
	tio.c_cc[VSTART] = _POSIX_VDISABLE;
	tio.c_cc[VSTOP] = _POSIX_VDISABLE;
				
	upsdebugx(2, "Flow control disable");

	/* disable any flow control */
	tcsetattr(upsfd, TCSANOW, &tio);
}

/* sane check for returned buffer */
static int validate_raw_data (void)
{
	int i = 0, 
    num_of_tests = 
		sizeof types[0].validation / sizeof types[0].validation[0];
	
	for (i = 0;
		 i < num_of_tests  && 
		   raw_data[
		        types[type].validation[i].index_of_byte] ==
          		       types[type].validation[i].required_value;
		 i++)  ;
	return (i < num_of_tests) ? 1 : 0;
}


/*
 * global used functions
 */

/* update information */
void upsdrv_updateinfo(void)
{
	char	val[32];
	float	tmp;
	int	i, c;
	
	/* send trigger char to UPS */
	if (ser_send_char (upsfd, SEND_DATA) != 1) {
		upslogx(LOG_NOTICE, "writing error");
		dstate_datastale();
		return;
	} else {
		upsdebugx(5, "Num of bytes requested for reading from UPS: %d", types[type].num_of_bytes_from_ups);

		c = ser_get_buf_len(upsfd, raw_data,
			types[type].num_of_bytes_from_ups, 3, 0);
	
		if (c != types[type].num_of_bytes_from_ups) {
			upslogx(LOG_NOTICE, "data receiving error (%d instead of %d bytes)", c, types[type].num_of_bytes_from_ups);
			dstate_datastale();
			return;
		} else
			upsdebugx(5, "Num of bytes received from UPS: %d", c);

	};

	/* optional dump of raw data */
	if (nut_debug_level > 4) {
		printf("Raw data from UPS:\n");
		for (i = 0; i < types[type].num_of_bytes_from_ups; i++) {
	 		printf("%2d 0x%02x (%c)\n", i, raw_data[i], raw_data[i]>=0x20 ? raw_data[i] : ' ');
		};
	};

	/* validate raw data for correctness */
	if (validate_raw_data() != 0) {
		upslogx(LOG_NOTICE, "data receiving error (validation check)");
		dstate_datastale();
		return;
	};
	
	/* input.frequency */
	upsdebugx(3, "input.frequency   (raw data): [raw: %u]",
	                            raw_data[INPUT_FREQUENCY]);
	dstate_setinfo("input.frequency", "%02.2f",
	    raw_data[INPUT_FREQUENCY] ? 
			1.0 / (types[type].freq[0] *
	                raw_data[INPUT_FREQUENCY] +
	                        types[type].freq[1]) : 0);
	upsdebugx(2, "input.frequency: %s", dstate_getinfo("input.frequency"));

	/* output.frequency */
	upsdebugx(3, "output.frequency   (raw data): [raw: %u]",
	                            raw_data[OUTPUT_FREQUENCY]);
	dstate_setinfo("output.frequency", "%02.2f",
	    raw_data[OUTPUT_FREQUENCY] ? 
			1.0 / (types[type].freq[0] *
	                raw_data[OUTPUT_FREQUENCY] +
	                        types[type].freq[1]) : 0);
	upsdebugx(2, "output.frequency: %s", dstate_getinfo("output.frequency"));

	/* ups.load */	
	upsdebugx(3, "ups.load  (raw data): [raw: %u]",
	                            raw_data[UPS_LOAD]);
	dstate_setinfo("ups.load", "%03.1f", 
	    tmp = raw_data[STATUS_A] & MAINS_FAILURE ?
	 		types[type].loadpct[0] * raw_data[UPS_LOAD] +
	                                    types[type].loadpct[1] :
			types[type].loadpct[2] * raw_data[UPS_LOAD] +
	                                    types[type].loadpct[3]);
	upsdebugx(2, "ups.load: %s", dstate_getinfo("ups.load"));

	/* battery.charge */
	upsdebugx(3, "battery.charge (raw data): [raw: %u]",
	                            raw_data[BATTERY_CHARGE]);
	dstate_setinfo("battery.charge", "%03.1f",
	    raw_data[STATUS_A] & MAINS_FAILURE ?
			types[type].battpct[0] * raw_data[BATTERY_CHARGE] +
		        types[type].battpct[1] * tmp + types[type].battpct[2] :
			types[type].battpct[3] * raw_data[BATTERY_CHARGE] +
			                                   types[type].battpct[4]);
	upsdebugx(2, "battery.charge: %s", dstate_getinfo("battery.charge"));

	/* input.voltage */	
	upsdebugx(3, "input.voltage (raw data): [raw: %u]",
	                            raw_data[INPUT_VOLTAGE]);
	dstate_setinfo("input.voltage", "%03.1f",
	    tmp = linevoltage >= 220 ?
			types[type].voltage[0] * raw_data[INPUT_VOLTAGE] +
			                                types[type].voltage[1] :
			types[type].voltage[2] * raw_data[INPUT_VOLTAGE] +
			                                types[type].voltage[3]);
	upsdebugx(2, "input.voltage: %s", dstate_getinfo("input.voltage"));
	
	/* output.voltage */	
	upsdebugx(3, "output.voltage (raw data): [raw: %u]",
	                            raw_data[OUTPUT_VOLTAGE]);
	dstate_setinfo("output.voltage", "%03.1f",
	    linevoltage >= 220 ?
			types[type].voltage[0] * raw_data[OUTPUT_VOLTAGE] +
			                                types[type].voltage[1] :
			types[type].voltage[2] * raw_data[OUTPUT_VOLTAGE] +
			                                types[type].voltage[3]);
	upsdebugx(2, "output.voltage: %s", dstate_getinfo("output.voltage"));

	status_init();
	
	*val = 0;
	if (!(raw_data[STATUS_A] & MAINS_FAILURE)) {
		!(raw_data[STATUS_A] & OFF) ? 
			status_set("OL") : status_set("OFF");
	} else {
		status_set("OB");
	}

	if (raw_data[STATUS_A] & LOW_BAT)  status_set("LB");

	if (raw_data[STATUS_A] & AVR_ON) {
		tmp < linevoltage ? 
			status_set("BOOST") : status_set("TRIM");
	}

	if (raw_data[STATUS_A] & OVERLOAD)  status_set("OVER");

	if (raw_data[STATUS_B] & BAD_BAT)  status_set("RB");

	if (raw_data[STATUS_B] & TEST)  status_set("TEST");

	status_commit();

	upsdebugx(2, "STATUS: %s", dstate_getinfo("ups.status"));
	dstate_dataok();
}

/* shutdown UPS */
void upsdrv_shutdown(void)
{
	/* power down the attached load immediately */
	printf("Forced UPS shutdown triggered, do it...\n");
	nut_shutdown();
}

/* initialize UPS */
void upsdrv_initups(void)
{
	int tmp;
	unsigned int i;

	/* check manufacturer name from arguments */
	if (getval("manufacturer") != NULL) 
		manufacturer = getval("manufacturer");
	
	/* check model name from arguments */
	if (getval("modelname") != NULL) 
		modelname = getval("modelname");
	
	/* check serial number from arguments */
	if (getval("serialnumber") != NULL) 
		serialnumber = getval("serialnumber");
	
	/* get and check type */
	if (getval("type") != NULL) {
		for (i = 0; 
			 i < NUM_OF_SUBTYPES  &&  strcmp(types[i].name, getval("type"));
			 i++) ;
		if (i >= NUM_OF_SUBTYPES) {
			printf("Given UPS type '%s' isn't valid!\n", getval("type"));
			exit (1);
		}
		type = i;	
	};
	
	/* check line voltage from arguments */
	if (getval("linevoltage") != NULL) {
		tmp = atoi(getval("linevoltage"));
		if (! ( (tmp >= 220 && tmp <= 240) || (tmp >= 110 && tmp <= 120) ) ) {
			printf("Given line voltage '%d' is out of range (110-120 or 220-240 V)\n", tmp);
			exit (1);
		};
		linevoltage = (unsigned int) tmp;
	};

	if (getval("numOfBytesFromUPS") != NULL) {
		tmp = atoi(getval("numOfBytesFromUPS"));
		if (! (tmp > 0 && tmp <= MAX_NUM_OF_BYTES_FROM_UPS) ) {
			printf("Given numOfBytesFromUPS '%d' is out of range (1 to %d)\n",
	               tmp, MAX_NUM_OF_BYTES_FROM_UPS);
			exit (1);
		};
		types[type].num_of_bytes_from_ups = (unsigned char) tmp;
	}

	if (getval("methodOfFlowControl") != NULL) {
		for (i = 0; 
			 i < NUM_OF_SUBTYPES  &&  
					strcmp(types[i].flowControl.name,
							getval("methodOfFlowControl"));
			 i++) ;
		if (i >= NUM_OF_SUBTYPES) {
			printf("Given methodOfFlowControl '%s' isn't valid!\n", 
					getval("methodOfFlowControl"));
			exit (1);
		};
		types[type].flowControl = types[i].flowControl;	
	}

	if (getval("validationSequence")  &&
            sscanf(getval("validationSequence"),
					"{{%u,%x},{%u,%x},{%u,%x}}",
			                &types[type].validation[0].index_of_byte,
			                &types[type].validation[0].required_value,
			                &types[type].validation[1].index_of_byte,
			                &types[type].validation[1].required_value,
			                &types[type].validation[2].index_of_byte,
			                &types[type].validation[2].required_value
			      ) < 6
	   ) {
		printf("Given validationSequence '%s' isn't valid!\n", 
								         getval("validationSequence"));
		exit (1);
	}

	if (getval("shutdownArguments")  &&
	    sscanf(getval("shutdownArguments"), "{{%u,%u},%c}",
	                &types[type].shutdown_arguments.delay[0],
	                &types[type].shutdown_arguments.delay[1],
	                &types[type].shutdown_arguments.minutesShouldBeUsed 
	          ) < 3
	   ) {
	    printf("Given shutdownArguments '%s' isn't valid!\n", 
								         getval("shutdownArguments"));
		exit (1);
	} 

	if (getval("frequency")  &&
            sscanf(getval("frequency"), "{%f,%f}",
			                &types[type].freq[0], &types[type].freq[1]
	              ) < 2
	   ) {
		printf("Given frequency '%s' isn't valid!\n", 
										getval("frequency"));
		exit (1);
	}

	if (getval("loadPercentage")  && 
            sscanf(getval("loadPercentage"), "{%f,%f,%f,%f}",
	            &types[type].loadpct[0], &types[type].loadpct[1],
	            &types[type].loadpct[2], &types[type].loadpct[3]
	              ) < 4
	   ) {
		printf("Given loadPercentage '%s' isn't valid!\n", 
								         getval("loadPercentage"));
		exit (1);
	}

	if (getval("batteryPercentage")  && 
            sscanf(getval("batteryPercentage"), "{%f,%f,%f,%f,%f}",
	                &types[type].battpct[0], &types[type].battpct[1],
	                &types[type].battpct[2], &types[type].battpct[3],
	                &types[type].battpct[4]
	              ) < 5
	   ) {
		printf("Given batteryPercentage '%s' isn't valid!\n", 
								         getval("batteryPercentage"));
		exit (1);
	}

	if (getval("voltage")  &&
            sscanf(getval("voltage"), "{%f,%f,%f,%f}",
	            &types[type].voltage[0], &types[type].voltage[1],
	            &types[type].voltage[2], &types[type].voltage[3]
				  ) < 4
	   ) {
		printf("Given voltage '%s' isn't valid!\n", getval("voltage"));
		exit (1);
	}

	upsdebugx(1, "Values of arguments:");
	upsdebugx(1, " manufacturer            : '%s'", manufacturer);
	upsdebugx(1, " model name              : '%s'", modelname);
	upsdebugx(1, " serial number           : '%s'", serialnumber);
	upsdebugx(1, " line voltage            : '%u'", linevoltage);
	upsdebugx(1, " type                    : '%s'", types[type].name);
	upsdebugx(1, " number of bytes from UPS: '%u'", 
	                        types[type].num_of_bytes_from_ups);
	upsdebugx(1, " method of flow control  : '%s'", 
    	                    types[type].flowControl.name);
	upsdebugx(1, " validation sequence: '{{%u,%#x},{%u,%#x},{%u,%#x}}'",
			            types[type].validation[0].index_of_byte,
			            types[type].validation[0].required_value,
			            types[type].validation[1].index_of_byte,
			            types[type].validation[1].required_value,
			            types[type].validation[2].index_of_byte,
			            types[type].validation[2].required_value);
	upsdebugx(1, " shutdown arguments: '{{%u,%u},%c}'",
	                types[type].shutdown_arguments.delay[0],
	                types[type].shutdown_arguments.delay[1],
	                types[type].shutdown_arguments.minutesShouldBeUsed); 
	upsdebugx(1, " frequency calculation coefficients: '{%f,%f}'",
			                types[type].freq[0], types[type].freq[1]);
	upsdebugx(1, " load percentage calculation coefficients: "
                 "'{%f,%f,%f,%f}'",
	                types[type].loadpct[0], types[type].loadpct[1],
	                types[type].loadpct[2], types[type].loadpct[3]);
	upsdebugx(1, " battery percentage calculation coefficients: " 
                 "'{%f,%f,%f,%f,%f}'",
	                types[type].battpct[0], types[type].battpct[1],
	                types[type].battpct[2], types[type].battpct[3],
	                types[type].battpct[4]);
    upsdebugx(1, " voltage calculation coefficients: '{%f,%f}'",
	                    types[type].voltage[2], types[type].voltage[3]);

	/* open serial port */
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);
	
	/* setup flow control */
	types[type].flowControl.setup_flow_control();
}

/* display help */
void upsdrv_help(void)
{
	printf("\n");
	return;
}

/* display banner */
void upsdrv_banner(void)
{
	printf("Network UPS Tools - PowerCom and similars protocol UPS driver %s (%s)\n\n", 
		POWERCOM_DRIVER_VERSION, UPS_VERSION);
}

/* initialize information */
void upsdrv_initinfo(void)
{
	/* write constant data for this model */
	dstate_setinfo("driver.version.internal", "%s", POWERCOM_DRIVER_VERSION);
	dstate_setinfo ("ups.mfr", "%s", manufacturer);
	dstate_setinfo ("ups.model", "%s", modelname);
	dstate_setinfo ("ups.serial", "%s", serialnumber);
	dstate_setinfo ("ups.model.type", "%s", types[type].name);
	dstate_setinfo ("input.voltage.nominal", "%u", linevoltage);

	/* now add the instant commands */
    dstate_addcmd ("test.battery.start");
	upsh.instcmd = instcmd;
}

/* define possible arguments */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "manufacturer",  "Specify manufacturer name (default: 'PowerCom')");
	addvar(VAR_VALUE, "linevoltage",   "Specify line voltage (110-120 or 220-240 V), because it cannot detect automagically (default: 230 V)");
	addvar(VAR_VALUE, "modelname",     "Specify model name, because it cannot detect automagically (default: Unknown)");
	addvar(VAR_VALUE, "serialnumber",  "Specify serial number, because it cannot detect automagically (default: Unknown)");
	addvar(VAR_VALUE, "type",       "Type of UPS like 'Trust', 'KP625AP', 'KIN2200AP', 'Egys', 'KIN525AP' or 'KIN1500AP' (default: 'Trust')");
	addvar(VAR_VALUE, "numOfBytesFromUPS",  "The number of bytes in a UPS frame");
	addvar(VAR_VALUE, "voltage",  "A quad to convert the raw data to human readable voltage");
	addvar(VAR_VALUE, "methodOfFlowControl",  "The flow control method engaged by the UPS");
	addvar(VAR_VALUE, "frequency",  "A pair to convert the raw data to human readable freqency");
	addvar(VAR_VALUE, "batteryPercentage",  "A 5 tuple to convert the raw data to human readable battery percentage");
	addvar(VAR_VALUE, "loadPercentage",  "A quad to convert the raw data to human readable load percentage");
	addvar(VAR_VALUE, "validationSequence",  "3 pairs to be used for validating the UPS");  
	addvar(VAR_VALUE, "shutdownArguments",  "The number of delay arguments and their values for the shutdown operation");
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
