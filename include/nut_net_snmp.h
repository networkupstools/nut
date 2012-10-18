#ifndef nut_net_snmp_h
#define nut_net_snmp_h

/**
 *  \brief  net-snmp-*.h headers wrapper
 *
 *  net-snmp package net-snmp-config.h (at least on certain platforms)
 *  contains project-specific definitions produced by configure
 *  which therefore collide with NUT's own config.h definitions
 *  (note that config.h should always be considered internal).
 *  Because of that, warnings about the symbols re-definitions
 *  pop out during NUT compilation.
 *
 *  This is not only ugly, but may also cause real problems
 *  if the net-snmp-config.h header is included after NUT's config.h,
 *  thus wrong project-specific definitions may be actually used.
 *  Therefore, this wrapper header was created, to #undef the net-SNMP
 *  symbols in question (only those that are project-specific, of course).
 *
 *  Note that config.h may also be included.
 *  That's because if it must've been included before this one
 *  (which might be necessary to actually decide whether net-SNMP
 *  is used or not), the project-specific symbols must be re-defined.
 *  Unfortunately, they can't be backed up, since macra expand to depth
 *  at point of usage, not at definition (by ANSI requirement).
 *  However, there's no warning if macro is redefined to the same value
 *  as previously, and since config.h doesn't contain the once-only guard,
 *  the (possible) secondary inclusion is both warnings-free and effective
 *  in re-defining the #undefined project-specific symbols.
 *
 *  \author  Vaclav Krpec  <VaclavKrpec@eaton.com>
 *  \date    2012/10/18
 */

#ifdef PACKAGE_BUGREPORT
#define INCLUDE_CONFIG_H
#undef PACKAGE_BUGREPORT
#endif

#ifdef PACKAGE_NAME
#define INCLUDE_CONFIG_H
#undef PACKAGE_NAME
#endif

#ifdef PACKAGE_VERSION
#define INCLUDE_CONFIG_H
#undef PACKAGE_VERSION
#endif

#ifdef PACKAGE_STRING
#define INCLUDE_CONFIG_H
#undef PACKAGE_STRING
#endif

#ifdef PACKAGE_TARNAME
#define INCLUDE_CONFIG_H
#undef PACKAGE_TARNAME
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif

#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif

#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif

/* config.h was included before this file, re-define undefined symbols */
#ifdef INCLUDE_CONFIG_H
#undef INCLUDE_CONFIG_H
#include "config.h"
#endif

#endif  /* end of #ifndef nut_net_snmp_h */
