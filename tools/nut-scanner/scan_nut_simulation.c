/*
 *  Copyright (C) 2023-2024 Arnaud Quette
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

#define SCAN_NUT_SIMULATION_DRIVERNAME "dummy-ups"

/* dynamic link library stuff */
static nutscan_device_t * dev_ret = NULL;

#ifdef HAVE_PTHREAD
static pthread_mutex_t dev_mutex;
#endif

nutscan_device_t * nutscan_scan_nut_simulation(void)
{
	DIR *dp;
	struct dirent *dirp;
	nutscan_device_t * dev = NULL;

#if HAVE_PTHREAD
	pthread_mutex_init(&dev_mutex, NULL);
#endif /* HAVE_PTHREAD */

	upsdebugx(1, "Scanning: %s", CONFPATH);

	if ((dp = opendir(CONFPATH)) == NULL) {
		upsdebugx(1, "%s: Failed to open %s: %s (%d)",
			__func__, CONFPATH, strerror(errno), errno);
		upsdebugx(0, "Failed to open %s, skip NUT simulation scan",
			CONFPATH);
		return NULL;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		const char *ext;

		upsdebugx(5, "Comparing file %s with simulation file extensions", dirp->d_name);
		ext = strrchr(dirp->d_name, '.');
		if((!ext) || (ext == dirp->d_name))
			continue;

		/* Filter on '.dev' and '.seq' extensions' */
		if ((strcmp(ext, ".dev") == 0) || (strcmp(ext, ".seq") == 0)) {
			upsdebugx(1, "Found simulation file: %s", dirp->d_name);

			dev = nutscan_new_device();
			dev->type = TYPE_NUT_SIMULATION;
			dev->driver = strdup(SCAN_NUT_SIMULATION_DRIVERNAME);
			dev->port = strdup(dirp->d_name);

#ifdef HAVE_PTHREAD
			pthread_mutex_lock(&dev_mutex);
#endif
			dev_ret = nutscan_add_device_to_device(dev_ret, dev);
#ifdef HAVE_PTHREAD
			pthread_mutex_unlock(&dev_mutex);
#endif
		}
	}
	closedir(dp);

#if HAVE_PTHREAD
	pthread_mutex_destroy(&dev_mutex);
#endif /* HAVE_PTHREAD */

	return nutscan_rewind_device(dev_ret);
}
