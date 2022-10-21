/*
 * blazer.h: defines/macros for Megatec/Q1 protocol based UPSes
 *
 * OBSOLETION WARNING: Please to not base new development on this
 * codebase, instead create a new subdriver for nutdrv_qx which
 * generally covers all Megatec/Qx protocol family and aggregates
 * device support from such legacy drivers over time.
 *
 * A document describing the protocol implemented by this driver can be
 * found online at "http://www.networkupstools.org/protocols/megatec.html".
 *
 * Copyright (C) 2008 - Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#ifndef BLAZER_H
#define BLAZER_H

#define MAXTRIES	3

/*
 * The driver core will do all the protocol handling and provides
 * the following (interface independent) parts already
 *
 *	upsdrv_updateinfo()
 *	upsdrv_shutdown()
 *
 * Communication with the UPS is done through blazer_command() of which
 * the prototype is declared below. It shall send a command and reads
 * a reply if buf is not a NULL pointer and buflen > 0.
 *
 * Returns < 0 on error, 0 on timeout and the number of bytes send/read on
 * success.
 */
ssize_t blazer_command(const char *cmd, char *buf, size_t buflen);

void blazer_makevartable(void);
void blazer_initups(void);
void blazer_initinfo(void);

#endif /* BLAZER_H */
