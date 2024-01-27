/* nutdrv_qx.h - Driver for USB and serial UPS units with Q* protocols
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 * Based on usbhid-ups.h - Copyright (C)
 *   2003-2009 Arnaud Quette <http://arnaud.quette.free.fr/contact.html>
 *   2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *   2007-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#ifndef NUTDRV_QX_H
#define NUTDRV_QX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

/* For testing purposes */
/*#define TESTING*/

/* Driver's parameters */
#define QX_VAR_ONDELAY	"ondelay"
#define QX_VAR_OFFDELAY	"offdelay"
#define QX_VAR_POLLFREQ	"pollfreq"

/* Parameters default values */
#define DEFAULT_ONDELAY		"180"	/* Delay between return of utility power and powering up of load, in seconds */
#define DEFAULT_OFFDELAY	"30"	/* Delay before power off, in seconds */
#define DEFAULT_POLLFREQ	30	/* Polling interval between full updates, in seconds; the driver will do quick polls in the meantime */

#ifndef TRUE
typedef enum { FALSE, TRUE } bool_t;
#else
typedef int bool_t;
#endif

/* Structure for rw vars */
typedef struct {
	char	value[SMALLBUF];				/* Value for enum/range, or length for ST_FLAG_STRING */
	int	(*preprocess)(char *value, const size_t len);	/* Optional function to preprocess range/enum value.
								 * This function will be given value and its size_t and must return either 0 if value is supported or -1 if not supported. */
} info_rw_t;

/* Structure containing information about how to get/set data from/to the UPS and convert these to/from NUT standard */
typedef struct item_t {
	const char	*info_type;		/* NUT variable name
						 * If QX_FLAG_NONUT is set, name to print to the logs
						 * If both QX_FLAG_NONUT and QX_FLAG_SETVAR are set, name of the var to retrieve from ups.conf */
	const int	info_flags;		/* NUT flags (ST_FLAG_* values to set in dstate_addinfo) */
	info_rw_t	*info_rw;		/* An array of info_rw_t to handle r/w variables:
						 * If ST_FLAG_STRING is set: length of the string (dstate_setaux)
						 * If QX_FLAG_ENUM is set: enumerated values (dstate_addenum)
						 * If QX_FLAG_RANGE is set: range boundaries (dstate_addrange)
						 * If QX_FLAG_SETVAR is set the value given by the user will be checked against these infos. */
	const char	*command;		/* Command sent to the UPS to get answer/to execute an instant command/to set a variable */

	char		answer[SMALLBUF];	/* Answer from the UPS, filled at runtime.
						 * If you expect a nonvalid C string (e.g.: inner '\0's) or need to perform actions before the answer is used (and treated as a null-terminated string), you should set a preprocess_answer() function */
	const size_t	answer_len;		/* Expected min length of the answer. Set it to 0 if there's no minimum length to look after. */
	const char	leading;		/* Expected leading character of the answer (optional) */

	char		value[SMALLBUF];	/* Value from the answer, filled at runtime (i.e. answer between from and to) */
	const int	from;			/* Position of the starting character of the info (i.e. 'value') we're after in the answer */
	const int	to;			/* Position of the ending character of the info (i.e. 'value') we're after in the answer: use 0 if all the remaining of the line is needed */

	const char	*dfl;			/* Format to store value from the UPS in NUT variables. Not used by the driver for QX_FLAG_{CMD,SETVAR} items.
						 * If there's no preprocess function, set it either to %s for strings or to a floating point specifier (e.g. %.1f) for numbers.
						 * Otherwise:
						 * If QX_FLAG_ABSENT: default value
						 * If QX_FLAG_CMD: default command value */

	unsigned long	qxflags;		/* Driver's own flags */

	int		(*preprocess_command)(struct item_t *item, char *command, const size_t commandlen);
						/* Last chance to preprocess the command to be sent to the UPS (e.g. to add CRC, ...).
						 * This function is given the currently processed item (item), the command to be sent to the UPS (command) and its size_t (commandlen).
						 * Return -1 in case of errors, else 0.
						 * command must be filled with the actual command to be sent to the UPS. */

	int		(*preprocess_answer)(struct item_t *item, const int len);
						/* Function to preprocess the answer we got from the UPS before we do anything else (e.g. for CRC, decoding, ...).
						 * This function is given the currently processed item (item) with the answer we got from the UPS unmolested and already stored in item->answer and the length of that answer (len).
						 * Return -1 in case of errors, else the length of the newly allocated item->answer (from now on, treated as a null-terminated string). */

	int		(*preprocess)(struct item_t *item, char *value, const size_t valuelen);
						/* Function to preprocess the data from/to the UPS
						 * This function is given the currently processed item (item), a char array (value) and its size_t (valuelen).
						 * Return -1 in case of errors, else 0.
						 * If QX_FLAG_SETVAR/QX_FLAG_CMD -> process command before it is sent: value must be filled with the command to be sent to the UPS.
						 * Otherwise -> process value we got from answer before it gets stored in a NUT variable: value must be filled with the processed value already compliant to NUT standards. */
} item_t;

/* Driver's own flags */
#define QX_FLAG_STATIC		2UL	/* Retrieve info only once. */
#define QX_FLAG_SEMI_STATIC	4UL	/* Retrieve info smartly, i.e. only when a command/setvar is executed and we expect that data could have been changed. */
#define QX_FLAG_ABSENT		8UL	/* Data is absent in the device, use default value. */
#define QX_FLAG_QUICK_POLL	16UL	/* Mandatory vars, polled also in QX_WALKMODE_QUICK_UPDATE.
					 * If there's a problem with a var not flagged as QX_FLAG_QUICK_POLL in QX_WALKMODE_INIT, the driver will automagically set QX_FLAG_SKIP on it and then it'll skip that item in QX_WALKMODE_{QUICK,FULL}_UPDATE.
					 * Otherwise, if the item has the flag QX_FLAG_QUICK_POLL set, in case of errors in QX_WALKMODE_INIT the driver will set datastale. */
#define QX_FLAG_CMD		32UL	/* Instant command. */
#define QX_FLAG_SETVAR		64UL	/* The var is settable and the actual item stores info on how to set it. */
#define QX_FLAG_TRIM		128UL	/* This var's value need to be trimmed of leading/trailing spaces/hashes. */
#define QX_FLAG_ENUM		256UL	/* Enum values exist and are stored in info_rw. */
#define QX_FLAG_RANGE		512UL	/* Ranges for this var available and are stored in info_rw. */
#define QX_FLAG_NONUT		1024UL	/* This var doesn't have a corresponding var in NUT. */
#define QX_FLAG_SKIP		2048UL	/* Skip this var: this item won't be processed. */

#define MAXTRIES		3	/* Max number of retries */

#ifdef TESTING
/* Testing struct */
typedef struct {
	const char	*cmd;			/* Command to match */
	const char	answer[SMALLBUF];	/* Answer for that command.
						 * Note: if 'answer' contains inner '\0's, in order to preserve them, 'answer_len' as well as an item_t->preprocess_answer() function must be set */
	const int	answer_len;		/* Answer length:
						 * - if set to -1 -> auto calculate answer length (treat 'answer' as a null-terminated string)
						 * - otherwise -> use the provided length (if reasonable) and preserve inner '\0's (treat 'answer' as a sequence of bytes till the item_t->preprocess_answer() function gets called) */
} testing_t;
#endif	/* TESTING */

/* Subdriver interface */
typedef struct {
	const char	*name;			/* Name of this subdriver, i.e. name (must be equal to the protocol name) + space + version */
	int		(*claim)(void);		/* Function that allows the subdriver to "claim" a device: return 1 if device is covered by this subdriver, else 0 */
	item_t		*qx2nut;		/* Main table of vars and instcmds */
	void		(*initups)(void);	/* Subdriver specific upsdrv_initups. Called at the end of nutdrv_qx's own upsdrv_initups */
	void		(*initinfo)(void);	/* Subdriver specific upsdrv_initinfo. Called at the end of nutdrv_qx's own upsdrv_initinfo */
	void		(*makevartable)(void);	/* Subdriver specific ups.conf flags/vars */
	const char	*accepted;		/* String to match if the driver is expecting a reply from the UPS on instcmd/setvar.
						 * This comparison is done after the answer we got back from the UPS has been processed to get the value we are searching:
						 *  - you don't have to include the trailing carriage return (\r)
						 *  - you can decide at which index of the answer the value should start or end setting the appropriate from and to in the item_t */
	const char	*rejected;		/* String to match if the driver is expecting a reply from the UPS in case of error.
						 * This comparison is done on the answer we got back from the UPS before it has been processed:
						 *  - include also the trailing carriage return (\r) and whatever character is expected */
#ifdef TESTING
	testing_t	*testing;		/* Testing table: commands and the replies used for testing the subdriver */
#endif	/* TESTING */
} subdriver_t;

/* The following functions are exported for the benefit of subdrivers */
	/* Execute an instant command. In detail:
	 * - look up the given 'cmdname' in the qx2nut data structure (if not found, try to fallback to commonly known commands);
	 * - if 'cmdname' is found, call its preprocess function, passing to it 'extradata', if any, otherwise its dfl value, if any;
	 * - send the command to the device and check the reply.
	 * Return STAT_INSTCMD_INVALID if the command is invalid, STAT_INSTCMD_FAILED if it failed, STAT_INSTCMD_HANDLED on success. */
int	instcmd(const char *cmdname, const char *extradata);
	/* Set r/w variable to a value after it has been checked against its info_rw structure. Return STAT_SET_HANDLED on success, otherwise STAT_SET_UNKNOWN. */
int	setvar(const char *varname, const char *val);
	/* Find an item of item_t type in qx2nut data structure by its info_type, optionally filtered by its qxflags, and return it if found, otherwise return NULL.
	 *  - 'flag': flags that have to be set in the item, i.e. if one of the flags is absent in the item it won't be returned
	 *  - 'noflag': flags that have to be absent in the item, i.e. if at least one of the flags is set in the item it won't be returned */
item_t	*find_nut_info(const char *varname, const unsigned long flag, const unsigned long noflag);
	/* Send 'command' (a null-terminated byte string) or, if it is NULL, send the command stored in the item to the UPS and process the reply, saving it in item->answer. Return -1 on errors, 0 on success. */
int	qx_process(item_t *item, const char *command);
	/* Process the value we got back from the UPS (set status bits and set the value of other parameters), calling the item-specific preprocess function, if any, otherwise executing the standard preprocessing (including trimming if QX_FLAG_TRIM is set).
	 * Return -1 on failure, 0 for a status update and 1 in all other cases. */
int	ups_infoval_set(item_t *item);
	/* Return the currently processed status so that it can be checked with one of the status_bit_t passed to the STATUS() macro. */
unsigned int	qx_status(void);
	/* Edit the current status: it takes one of the NUT status (all but OB are supported, simply set it as not OL), eventually preceded with an exclamation mark to clear it from the status (e.g. !OL). */
void	update_status(const char *nutvalue);

/* Data for processing status values */
#define	STATUS(x)	((unsigned int)1U<<x)

typedef enum {
	OL = 0,		/* On line */
	LB,		/* Low battery */
	RB,		/* Replace battery */
	CHRG,		/* Charging */
	DISCHRG,	/* Discharging */
	BYPASS,		/* On bypass */
	CAL,		/* Calibration */
	OFF,		/* UPS is off */
	OVER,		/* Overload */
	TRIM,		/* SmartTrim */
	BOOST,		/* SmartBoost */
	FSD 		/* Shutdown imminent */
} status_bit_t;

#endif	/* NUTDRV_QX_H */
