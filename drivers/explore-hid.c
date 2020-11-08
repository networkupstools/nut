/*  explore-hid.c - this is a "stub" subdriver used to collect data
 *  about HID UPS systems that are not yet supported.
 *
 *  This subdriver will match any UPS, but only if the "-x explore" option
 *  is given.
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "main.h"
#include "usbhid-ups.h"
#include "explore-hid.h"

#define EXPLORE_HID_VERSION	"EXPLORE HID 0.1"

static usage_tables_t explore_utab[] = {
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/*                 Data lookup table (HID <-> NUT)                 */
/* --------------------------------------------------------------- */

static hid_info_t explore_hid2nut[] =
{
  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *explore_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *explore_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor;
}

static const char *explore_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int explore_claim(HIDDevice_t *hd) {
	if (testvar("explore")) {
		return 1;
	} else {
		return 0;
	}
}

subdriver_t explore_subdriver = {
	EXPLORE_HID_VERSION,
	explore_claim,
	explore_utab,
	explore_hid2nut,
	explore_format_model,
	explore_format_mfr,
	explore_format_serial,
};
