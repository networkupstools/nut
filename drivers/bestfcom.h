/* 
   bestfcom.h - model specific routines for Best Power F-Command ups models

   This module is yet another rewritten mangle of the bestuferrups
   driver.  This driver was written in an attempt to consolidate
   the various Best Fortress/FERRUPS modules that support the
   'f'-command set and provide support for more of these models.

   Models tested with this new version:
   FortressII	LI720
   FERRUPS	FE2.1K
   FERRUPS	FE4.3K
   FERRUPS	FE18K
   FERRUPS	FD4.3K

   From bestuferrups.c :

   This module is a 40% rewritten mangle of the bestfort module by
   Grant, which is a 75% rewritten mangle of the bestups module by
   Russell.   It has no test battery command since my ME3100 does this
   by itself. (same as Grant's driver in this respect)

   Copyright (C) 2002  Andreas Wrede  <andreas@planix.com>
   Copyright (C) 2000  John Stone  <johns@megapixel.com>
   Copyright (C) 2000  Grant Taylor <gtaylor@picante.com>
   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*/

#define DRV_VERSION "0.11"

