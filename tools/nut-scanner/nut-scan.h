/*
 *  Copyright (C)
 *    2011 - EATON
 *    2012 - 2024 Arnaud Quette <arnaud.quette@free.fr>
 *    2016 - EATON - IP addressed XML scan
 *    2016 - 2021 - EATON - Various threads-related improvements
 *    2023 - 2024 - Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file nut-scan.h
    \brief general header for nut-scanner
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Arnaud Quette <arnaud.quette@free.fr>
    \author Michal Vyskocil <MichalVyskocil@eaton.com>
    \author Jim Klimov <EvgenyKlimov@eaton.com>
*/

#ifndef NUT_SCAN_H
#define NUT_SCAN_H

#include <sys/types.h>

/* Ensure uint16_t et al: */
#if defined HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#if defined HAVE_STDINT_H
#  include <stdint.h>
#endif

#if defined HAVE_LIMITS_H
#  include <limits.h>
#endif

/* Ensure useconds_t et al: */
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "nutscan-init.h"
#include "nutscan-device.h"
#include "nutscan-ip.h"

#ifdef WITH_IPMI
#include <freeipmi/freeipmi.h>
#endif

#ifdef HAVE_PTHREAD
# include <pthread.h>

# ifdef HAVE_SEMAPHORE
#  include <semaphore.h>
# endif

# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE)
extern size_t max_threads, curr_threads, max_threads_netxml, max_threads_oldnut, max_threads_netsnmp;
# endif

# ifdef HAVE_PTHREAD_TRYJOIN
extern pthread_mutex_t threadcount_mutex;
# endif

typedef struct nutscan_thread {
	pthread_t	thread;
	int		active;	/* true if the thread was created, false if joined (to not join twice) */
} nutscan_thread_t;
#endif /* HAVE_PTHREAD */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* SNMP structure */
typedef struct nutscan_snmp {
	char * community;
	char * secLevel;
	char * secName;
	char * authPassword;
	char * privPassword;
	char * authProtocol;
	char * privProtocol;
	char * peername;
	void * handle;
} nutscan_snmp_t;

/* IPMI structure */
/* Settings for OutofBand (remote) connection */
typedef struct nutscan_ipmi {
	char*			username;            /* IPMI 1.5 and 2.0 */
	char*			password;            /* IPMI 1.5 and 2.0 */
	int				authentication_type; /* IPMI 1.5 */
	int				cipher_suite_id;     /* IPMI 2.0 */
	char*			K_g_BMC_key;         /* IPMI 2.0, optional key for 2 key auth. */
	int				privilege_level;     /* for both */
	unsigned int	workaround_flags;    /* for both */
	int				ipmi_version;        /* IPMI 1.5 or 2.0? */
} nutscan_ipmi_t;

/* IPMI auth defines, simply using FreeIPMI defines */
#ifndef IPMI_AUTHENTICATION_TYPE_NONE
  #define IPMI_AUTHENTICATION_TYPE_NONE                  0x00
  #define IPMI_AUTHENTICATION_TYPE_MD2                   0x01
  #define IPMI_AUTHENTICATION_TYPE_MD5                   0x02
  #define IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY 0x04
  #define IPMI_AUTHENTICATION_TYPE_OEM_PROP              0x05
  #define IPMI_AUTHENTICATION_TYPE_RMCPPLUS              0x06
#endif /* IPMI_AUTHENTICATION_TYPE_NONE */
#ifndef IPMI_PRIVILEGE_LEVEL_ADMIN
  #define IPMI_PRIVILEGE_LEVEL_ADMIN                     0x04
#endif /* IPMI_PRIVILEGE_LEVEL_ADMIN */

#define IPMI_1_5		1
#define IPMI_2_0		0

/* XML HTTP structure */
typedef struct nutscan_xml {
	uint16_t port_http;		/* Port for xml http (tcp) */
	uint16_t port_udp;		/* Port for xml udp */
	useconds_t usec_timeout;	/* Wait this long for a response */
	char *peername;		/* Hostname or NULL for broadcast mode */
} nutscan_xml_t;

/* USB scan options structure */
typedef struct nutscan_usb {
	/* Hardware link related values below are not reliable for run-time
	 * matching (they can change over time) but can be useful if e.g.
	 * "serial" is not available or unique */
	int report_bus;
	int report_busport;
	int report_device;

	/* The value is not currently used for device matching, but might be
	 * used later, and it is available from discovery */
	int report_bcdDevice;
} nutscan_usb_t;

/* Scanning */
nutscan_device_t * nutscan_scan_snmp(const char * start_ip, const char * stop_ip, useconds_t usec_timeout, nutscan_snmp_t * sec);

nutscan_device_t * nutscan_scan_usb(nutscan_usb_t * scanopts);

/* If "ip" == NULL, do a broadcast scan */
/* If sec->usec_timeout <= 0 then the common usec_timeout arg overrides it */
nutscan_device_t * nutscan_scan_xml_http_range(const char *start_ip, const char *end_ip, useconds_t usec_timeout, nutscan_xml_t * sec);

nutscan_device_t * nutscan_scan_nut(const char * startIP, const char * stopIP, const char * port, useconds_t usec_timeout);

nutscan_device_t * nutscan_scan_nut_simulation(void);

nutscan_device_t * nutscan_scan_avahi(useconds_t usec_timeout);

nutscan_device_t * nutscan_scan_ipmi(const char * startIP, const char * stopIP, nutscan_ipmi_t * sec);

nutscan_device_t * nutscan_scan_eaton_serial(const char* ports_list);

#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
/* Expose shared libnutscanner semaphore for overall thread count
 * limited across different scanning methods (protocols/media): */
sem_t * nutscan_semaphore(void);
# endif
#endif

/* Display functions */
void nutscan_display_ups_conf(nutscan_device_t * device);
void nutscan_display_parsable(nutscan_device_t * device);

/* Display sanity-check concerns for various fields etc. (if any) */
void nutscan_display_ups_conf_with_sanity_check(nutscan_device_t * device);
void nutscan_display_sanity_check(nutscan_device_t * device);
void nutscan_display_sanity_check_serial(nutscan_device_t * device);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif
