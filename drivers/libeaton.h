/* libeaton.h - Header for Eaton SDK
 * Copyright (C) 2011 Eaton
 *  Author: Frederic BOHE <fredericbohe@eaton.com>
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

void libeaton_init(const char *);
void libeaton_free();
const char *libeaton_read(const char *varname);
void libeaton_update(void);
int libeaton_write(const char *varname, const char *val);
int libeaton_command(const char *cmdname, const char *extradata);
const char * libeaton_dump_all(void);
