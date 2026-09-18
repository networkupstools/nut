/* nut-pragmas-format-nonliteral.h - quiesce nonliteral format warnings

   Copyright (C) 2020-2026 Jim Klimov <jimklimov+nut@gmail.com>

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

/* This file is intentionally repeat-includable so one translation unit can
 * bracket several uses of nonliteral format strings. */

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
# pragma GCC diagnostic ignored "-Wformat-security"
#endif
