/*
 * powerp-txt.c - Model specific routines for CyberPower text
 *                protocol UPSes
 *
 * Copyright (C)
 *	2007        Doug Reynolds <mav@wastegate.net>
 *	2007-2008   Arjen de Korte <adkorte-guest@alioth.debian.org>
 *	2012-2016   Timothy Pearson <kb9vqf@pearsoncomputing.net>
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

#ifndef POWERP_TXT_PARSER_H
#define POWERP_TXT_PARSER_H
#include <stddef.h>

typedef struct {
	float           i_volt;
	float           o_volt;
	int             o_load;
	int             b_chrg;
	unsigned char   has_u_temp;
	int             u_temp;
	float           i_freq;
	unsigned char flags[2];
	unsigned char   has_b_volt;
	float           b_volt;
	unsigned char   has_o_freq;
	float           o_freq;
	unsigned char   has_runtime;
	int             runtime;
	unsigned char   has_o_current;
	float           o_current;
	unsigned char   has_realpower;
	int             realpower;
	unsigned char   has_power;
	int             power;
} powpan_status_t;

int powpan_parse_status(const char *answer, size_t length, powpan_status_t *status);
#endif
