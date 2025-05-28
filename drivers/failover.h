/* failover.h - UPS Failover Driver (Header)

   Copyright (C)
       2025 - Sebastian Kuttnig <sebastian.kuttnig@gmail.com>

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

#ifndef FAILOVER_H_SEEN
#define FAILOVER_H_SEEN 1

#include "config.h"
#include "main.h"
#include "parseconf.h"
#include "timehead.h"
#include "upsdrvquery.h"

#define VAR_ALLOC_BATCH      50
#define SUBVAR_ALLOC_BATCH   10
#define CMD_ALLOC_BATCH      20
#define CONN_READ_TIMEOUT     3
#define CONN_CMD_TIMEOUT      3
#define ALARM_PROPAG_TIME    15

#define DEFAULT_INIT_TIMEOUT         30
#define DEFAULT_DEAD_TIMEOUT         30
#define DEFAULT_CONNECTION_COOLOFF   15
#define DEFAULT_NO_PRIMARY_TIMEOUT   15
#define DEFAULT_MAX_CONNECT_FAILS     5
#define DEFAULT_RELOG_TIMEOUT         5
#define DEFAULT_CHECK_RUNTIME         1
#define DEFAULT_FSD_MODE              0
#define DEFAULT_STRICT_FILTERING      0

typedef enum {
    PRIORITY_SKIPPED = -1,
    PRIORITY_FORCED = 0,
    PRIORITY_USERFILTERS = 1,
    PRIORITY_GOOD = 2,
    PRIORITY_WEAK = 3,
    PRIORITY_LASTRESORT = 4
} ups_priority_t;

typedef enum {
	UPS_FLAG_NONE      = 0,
	UPS_FLAG_ALIVE     = 1 << 0,
	UPS_FLAG_DUMPED    = 1 << 1,
	UPS_FLAG_DATA_OK   = 1 << 2,
	UPS_FLAG_ONLINE    = 1 << 3,
	UPS_FLAG_PRIMARY   = 1 << 4
} ups_flags_t;

typedef struct {
	int min;
	int max;
} var_range_t;

typedef struct {
	char *key;
	char *value;

	char **enum_list;
	var_range_t **range_list;

	size_t enum_count;
	size_t enum_allocs;
	size_t range_count;
	size_t range_allocs;

	long aux;

	int flags;
	int needs_export;
} ups_var_t;

typedef struct {
	char *value;
	int needs_export;
} ups_cmd_t;

typedef struct {
	char **have_any;
	size_t have_any_count;

	char **have_all;
	size_t have_all_count;

	char **nothave_any;
	size_t nothave_any_count;

	char **nothave_all;
	size_t nothave_all_count;
} status_filters_t;

typedef struct {
	char *socketname;

	udq_pipe_conn_t *conn;
	PCONF_CTX_t parse_ctx;

	ups_var_t **var_list;
	ups_cmd_t **cmd_list;

	size_t var_count;
	size_t var_allocs;
	size_t cmd_count;
	size_t cmd_allocs;

	char *status;

	time_t last_heard_time;
	time_t last_pinged_time;
	time_t last_failure_time;
	time_t force_ignore_time;
	time_t force_primary_time;

	ups_flags_t flags;
	ups_priority_t priority;
	int failure_count;

	int force_ignore;
	int force_primary;
	int force_dstate_export;
} ups_device_t;

#endif	/* FAILOVER_H_SEEN */
