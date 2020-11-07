/*
	anything commented is optional
	anything else is mandatory

	for more information, refer to:
	* docs/developers.txt
	* docs/new-drivers.txt
	* docs/new-names.txt

	and possibly also to:
	* docs/hid-subdrivers.txt for USB/HID devices
	* or docs/snmp-subdrivers.txt for SNMP devices
*/

#include "main.h"

/* #include "serial.h" */

/* #define ENDCHAR	'\r'	*/
/* #define IGNCHARS	""	*/

#define DRIVER_NAME	"Skeleton UPS driver"
#define DRIVER_VERSION	"0.02"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"John Doe <john@doe.org>",
	DRV_STABLE,
	{ NULL }
};

void upsdrv_initinfo(void)
{
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
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */
}

void upsdrv_initups(void)
{
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
	/* free(dynamic_mem); */
	/* ser_close(upsfd, device_path); */
}
