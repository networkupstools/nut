/*
 *  Copyright (C) 2024 Arnaud Quette
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

/*! \file scan_nut_simulation.c
    \brief detect local NUT simulation devices (.dev & .seq)
    \author Arnaud Quette <arnaud.quette@free.fr>
*/

#include "common.h"
#include "nut-scan.h"
#include "nut_stdint.h"
#include <dirent.h>
#if !HAVE_DECL_REALPATH
# include <sys/stat.h>
#endif

#define SCAN_NUT_SIMULATION_DRIVERNAME "dummy-ups"

/* dynamic link library stuff */
static nutscan_device_t * dev_ret = NULL;

#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

/* return 1 when filter is ok (.dev or .seq) */
static int filter_ext(const struct dirent *dir)
{
	if(!dir)
		return 0;

	if(dir->d_type == DT_REG) { /* only deal with regular file */
		const char *ext = strrchr(dir->d_name,'.');
		if((!ext) || (ext == dir->d_name))
			return 0;
		else {
			if ((strcmp(ext, ".dev") == 0) || (strcmp(ext, ".seq") == 0)) {
				return 1;
			}
		}
	}
	return 0;
}

nutscan_device_t * nutscan_scan_nut_simulation(void)
{
	nutscan_device_t * dev = NULL;
	struct dirent **namelist;
	int n;

#if HAVE_PTHREAD
	pthread_mutex_init(&dev_mutex, NULL);
#endif /* HAVE_PTHREAD */

	upsdebugx(1,"Scanning: %s", CONFPATH);

	n = scandir(CONFPATH, &namelist, filter_ext, NULL);
	if (n < 0) {
		fatal_with_errno(EXIT_FAILURE, "Failed to scandir");
		return NULL;
	}
	else {
		while (n--) {
			upsdebugx(1,"Found simulation file: %s", namelist[n]->d_name);

			dev = nutscan_new_device();
			dev->type = TYPE_NUT_SIMULATION;
			dev->driver = strdup(SCAN_NUT_SIMULATION_DRIVERNAME);
			dev->port = strdup(namelist[n]->d_name);

#ifdef HAVE_PTHREAD
			pthread_mutex_lock(&dev_mutex);
#endif
			dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
			pthread_mutex_unlock(&dev_mutex);
#endif
			free(namelist[n]);
		}
		free(namelist);
	}

#if HAVE_PTHREAD
	pthread_mutex_destroy(&dev_mutex);
#endif /* HAVE_PTHREAD */

	return nutscan_rewind_device(dev_ret);
}
