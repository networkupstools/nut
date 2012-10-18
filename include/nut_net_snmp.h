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
 *  Note that this header should therefore be always included BEFORE
 *  NUT's config.h.
 *
 *  \author  Vaclav Krpec  <VaclavKrpec@eaton.com>
 *  \date    2012/10/18
 */

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

#endif  /* end of #ifndef nut_net_snmp_h */
