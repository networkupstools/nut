#ifndef APC_IEM_MIB_H
#define APC_IEM_MIB_H

/*
 * FIXME: The below is needed because the main driver body uses this to determine
 * whether a conversion from Fahrenheit to Celsius is needed (which really should
 * be solved in subdriver specific formatting functions, like we do in usbhid-ups
 * This is used in both snmp-ups.c and apc.c logics.
 */

/* IEM ambient variables */
/* IEM: integrated environment monitor probe */

#define APCC_OID_IEM_TEMP    	".1.3.6.1.4.1.318.1.1.10.2.3.2.1.4.1"
#define APCC_OID_IEM_TEMP_UNIT	".1.3.6.1.4.1.318.1.1.10.2.3.2.1.5.1"
#define APCC_IEM_FAHRENHEIT  	2
#define APCC_OID_IEM_HUMID   	".1.3.6.1.4.1.318.1.1.10.2.3.2.1.6.1"

#endif /* APC_IEM_MIB_H */
