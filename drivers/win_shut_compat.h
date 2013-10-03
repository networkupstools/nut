/*!
 * @file win_shut_comapt.h
 * @brief Windows specific macros for shut protocol
 *
 * @author Copyright (C) 2013
 *      Arnaud Quette <fredericbohe@eaton.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#ifndef WIN_SHUT_COMPAT
#define WIN_SHUT_COMPAT

#ifndef WIN32
#define ERROR_FD (-1)
#define VALID_FD(a) (a>0)
#define TYPE_FD int
#else /* WIN32 */
#define ERROR_FD (NULL)
#define VALID_FD(a) (a!=NULL)
#ifdef SHUT_MODE
#define TYPE_FD serial_handler_t *
#else /* SHUT_MODE */
#define TYPE_FD hid_dev_handle_t
#endif /* SHUT_MODE */
#endif /* WIN32 */

#endif
