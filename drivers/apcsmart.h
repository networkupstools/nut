/*
 * apcsmart.h - common defines for apcsmart driver
 *
 * Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
 *           (C) 2000  Nigel Metheringham <Nigel.Metheringham@Intechnology.co.uk>
 *           (C) 2011+ Michal Soltys <soltys@ziu.info>
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
 */

#ifndef NUT_APCSMART_H_SEEN
#define NUT_APCSMART_H_SEEN 1

#define DRIVER_NAME	"APC Smart protocol driver"
#define DRIVER_VERSION	"3.1"

#define ALT_CABLE_1 "940-0095B"

/*
 * alerts and other stuff for quick reference:
 *
 * $ OL
 * ! OB
 * % LB
 * + not LB anymore
 * # RB
 * ? OVER
 * = not OVER anymore
 * * powering down now (only older models ?), handled by upsread()
 * 	otherwise ignored (it doesn't have to be in ignore sets)
 *
 * | eeprom change
 * & check alarm register for fail
 * ~ ???
 */

/*
 * old ones for reference:
 * 	#define IGNCHARS "\015+$|!~%?=#&"
 * 	#define POLL_IGNORE "\015&|"
 *	#define POLL_ALERT "$!%+#?="
 *	#define MINIGNCHARS "\015+$|!"
 * notice ~ that was present in IGNCHARS, but not in POLL_IGNORE - this kinda
 * didn't make sense (?); new versions doesn't filter ~, but keep that in mind
 * in case something obscure surfaces
 * due to switch to ICANON tty mode, we removed \015 from ignored characters,
 * as it's handled by IGNCR at read() level
 */

/*
 * about CR:
 * apparently windows is unable to ignore CRs by means of IGNCR flag (or
 * perhaps there is something else involved); so despite IGNCR we re-aad \015
 * to ignore sets for now; see:
 * http://article.gmane.org/gmane.comp.monitoring.nut.user/7762
 *
 * furthermore, since the canonical/non-canonical mode is user selectable now,
 * we have to ignore this character explicitly
 */

/* Basic UPS reply line structure */
#define ENDCHAR 10		/* APC ends responses with LF (and CR, but it's IGNCRed) */

/* what to ignore during alert aware serial reads */
#define IGN_AACHARS "\015|&"

/* what alert_handler() should care about */
#define ALERT_CHARS "$!%+#?="

/* characters ignored by alertless reads */
#define IGN_CHARS IGN_AACHARS ALERT_CHARS

/*
 * these ones are used only during Capability Check read, due to ^Z sending
 * certain characters such as #; it seems it could be equal to just IGN_CHARS
 * w/o # old: #define IGN_CCCHARS "|$!+"
 */
#define IGN_CCCHARS "\015|&$!%+?="	/* capability check ignore set */

/*
 * Command Set 'a' reports everything - protocol number, alerts and supported
 * commands
 */
#define IGN_CSCHARS ""	/* command set ignore set */

/* dangerous instant commands must be reconfirmed within a 12 second window */
#define MINCMDTIME	3
#define MAXCMDTIME	15

/* it only does two strings, and they're both the same length */
#define APC_STRLEN	8

#define SER_D0	0x001	/* 0 sec., for flushes */
#define SER_DX	0x002	/* 200 ms for long/repeated cmds, in case of unexpected NAs */
#define SER_D1	0x004	/* 1.5 sec. */
#define SER_D3	0x008	/* 3 sec. (default) */
#define SER_AA	0x010	/* Alert Aware set */
#define SER_CC	0x020	/* Capability Check ign set */
#define SER_CS	0x040	/* Command Set ign set */
#define SER_TO	0x080	/* timeout allowed */
#define SER_HA	0x100	/* handle asterisk */


/* sets of the above (don't test against them, obviously */

/*
 * Some cmd codes to ignore (nut doesn't expose those, though the driver might
 * use them internally (e.g. [a]). If you decide to support them at some
 * point, remember about removing them from here !
 */
#define APC_UNR_CMDS "\032\177~')-+8QRYayz"

/* --------------- */

/* status bits */

#define APC_STAT_CAL   (1L << 0)        /* calibration */
#define APC_STAT_TRIM  (1L << 1)        /* SmartTrim */
#define APC_STAT_BOOST (1L << 2)        /* SmartBoost */
#define APC_STAT_OL    (1L << 3)        /* on line */
#define APC_STAT_OB    (1L << 4)        /* on battery */
#define APC_STAT_OVER  (1L << 5)        /* overload */
#define APC_STAT_LB    (1L << 6)        /* low battery */
#define APC_STAT_RB    (1L << 7)        /* replace battery */

/*
 * serial protocol: special commands - initialization and such
 * these are not exposed as instant commands
 */
#define APC_STATUS	'Q'
#define APC_GOSMART	'Y'
#define APC_GODUMB	'R'
#define APC_CMDSET	'a'
#define APC_CMDSET_FMT	"^[0-9]\\.[^.]*\\.[^.]+(\\.[^.]+)?$"
#define APC_CAPS	'\032'	/* ^Z */
#define APC_NEXTVAL	'-'
#define APC_FW_OLD	'V'
#define APC_FW_NEW	'b'

#define APC_LBUF	512
#define APC_SBUF	32

/* default a.w.d. value / regex format for command '@' */
#define APC_AWDFMT	"^[0-9]{1,3}$"

/* sdtype method regex format*/
#define APC_SDFMT	"^[0-5]$"

/* advorder method regex format*/
#define APC_ADVFMT	"^([0-4]{1,5}|[Nn][Oo])$"

/* cshdelay format */
#define APC_CSHDFMT	"^([0-9]\\.?|[0-9]?\\.[0-9])$"

#endif  /* NUT_APCSMART_H_SEEN */
