/*
 * apcsmart_tabs.h - tables for apcsmart driver
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

#ifndef NUT_APCSMART_TABS_H_SEEN
#define NUT_APCSMART_TABS_H_SEEN 1

#include "main.h"

#define APC_TABLE_VERSION	"version 3.1"

/* common flags */

#define APC_PRESENT	0x00000001	/* capability seen on this UPS		*/

/* instant commands' flags */

#define APC_NASTY	0x00000002	/* Nasty command - must be reconfirmed	*/
#define APC_REPEAT	0x00000004	/* Command needs sending twice		*/

/* variables' flags */

#define APC_POLL	0x00000100	/* poll this variable regularly		*/
#define APC_RW		0x00000200	/* read-write variable			*/
#define APC_ENUM	0x00000400	/* enumerated type variable		*/
#define APC_STRING	0x00000800	/* string variable			*/
#define APC_MULTI	0x00001000	/* there're other vars like that	*/
#define APC_PACK	0x00002000	/* packed variable			*/

#define APC_PACK_MAX	4		/* max count of subfields in packed var	*/

/* variables' format */

#define APC_F_MASK	0xFF000000	/* Mask for apc data formats		*/
#define APC_F_LEAVE	0x00000000	/* Just pass this through		*/
#define APC_F_PERCENT	0x01000000	/* Data in a percent format		*/
#define APC_F_VOLT	0x02000000	/* Data in a voltage format		*/
#define APC_F_AMP	0x03000000	/* Data in a current/amp format		*/
#define APC_F_CELSIUS	0x04000000	/* Data in a temp/C format		*/
#define APC_F_HEX	0x05000000	/* Data in a hex number format		*/
#define APC_F_DEC	0x06000000	/* Data in a decimal format		*/
#define APC_F_SECONDS	0x07000000	/* Time in seconds			*/
#define APC_F_MINUTES	0x08000000	/* Time in minutes			*/
#define APC_F_HOURS	0x09000000	/* Time in hours			*/
#define APC_F_REASON	0x0A000000	/* Reason of transfer			*/

/* instant commands */

#define APC_CMD_CUSTOM		0	/* command uses separate function */
#define APC_CMD_OFF		'Z'
#define APC_CMD_ON		'\016'	/* ^N */
#define APC_CMD_FPTEST		'A'
#define APC_CMD_SIMPWF		'U'
#define APC_CMD_BTESTTOGGLE	'W'
#define APC_CMD_GRACEDOWN	'@'
#define APC_CMD_SOFTDOWN	'S'
#define APC_CMD_SHUTDOWN	'K'
#define APC_CMD_CALTOGGLE	'D'
#define APC_CMD_BYPTOGGLE	'^'


typedef struct {
	const char	*name;		/* the variable name	*/
	char		cmd;		/* variable character	*/
	unsigned int	flags;	 	/* various flags	*/
	const char	*regex;		/* variable must match this regex */
	size_t		nlen0;		/* var name + null len	*/
	int		cnt;		/* curr. count of subs	*/
} apc_vartab_t;

typedef struct {
	const char	*name, *ext;
	char		cmd;
	int		flags;
} apc_cmdtab_t;

typedef struct {
	const char	*firmware;
	const char	*cmdchars;
	int		flags;
} apc_compattab_t;

extern apc_vartab_t apc_vartab[];
extern apc_cmdtab_t apc_cmdtab[];
extern apc_compattab_t apc_compattab[];
extern upsdrv_info_t apc_tab_info;

#endif  /* NUT_APCSMART_TABS_H_SEEN */
