/* victronups.h - Model specific routines for GE/IMV/Victron units
 * Match, Match Lite, NetUps
 *
 * Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
 * Copyright (C) 2000  Radek Benedikt <benedikt@lphard.cz>
 * old style "victronups"
 * Copyright (C) 2001  Daniel.Prynych <Daniel.Prynych@hornet.cz>
 * porting to now style "newvictron"
 * Copyright (C) 2003  Gert Lynge <gert@lynge.org>
 * Porting to new serial functions. Now removes \n from data (was causing
 * periodic misreadings of temperature and voltage levels)
 * Copyright (C) 2004  Gert Lynge <gert@lynge.org>
 * Implemented some Instant Commands.
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

/* FIXME (AQU): DRV_VERSION should be "X.YZ", ie "0.19" */
#define DRV_VERSION "0.1.9"
