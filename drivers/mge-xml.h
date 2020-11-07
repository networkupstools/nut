/* mge-xml.h		Model specific data for MGE XML protocol UPSes

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#ifndef MGE_XML_H
#define MGE_XML_H

#include "netxml-ups.h"

extern subdriver_t	mge_xml_subdriver;

/**
 *  \brief  Convert NUT variable name to MGE XML
 *
 *  \param  name  NUT variable name
 *
 *  \return MGE XML variable name
 */
const char *vname_nut2mge_xml(const char *name);

/**
 *  \brief  Convert MGE XML variable name to NUT
 *
 *  \param  name  MGE XML variable name
 *
 *  \return NUT variable name
 */
const char * vname_mge_xml2nut(const char *name);

/**
 *  \brief  Convert MGE XML variable value to NUT value
 *
 *  The function produces a newly created C-string that should
 *  be destroyed using \c free.
 *
 *  \param  name   NUT variable name
 *  \param  value  MGE XML variable value
 *  \param  len    MGE XML variable value length (in characters)
 *
 *  \return NUT variable value
 */
char *vvalue_mge_xml2nut(const char *name, const char *value, size_t len);

/**
 *  \brief  Register set of R/W variables
 */
void vname_register_rw(void);

#endif /* MGE_XML_H */
