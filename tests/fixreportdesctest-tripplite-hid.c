/* fixreportdesctest-tripplite-hid - check the Report Descriptor repair which
 * compensates for Tripp Lite HID firmware that declares the HID PDC power
 * unit while leaving the Unit Exponent at 0 (as used in the
 * drivers/tripplite-hid.c subdriver of usbhid-ups).
 *
 * Modelled on tests/getexponenttest-belkin-hid.c.
 *
 * See also:
 *  https://github.com/networkupstools/nut/issues/3580
 *
 * Copyright (C)
 *      2026        Kyle Mason <masonkr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include "nut_stdint.h"
#include "nut_float.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

/* Dummy here */
#include "nut_libusb.h"
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_FIELD_INITIALIZERS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_BRACES_BESIDEFUNC) )
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_FIELD_INITIALIZERS_BESIDEFUNC
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_BRACES_BESIDEFUNC
#pragma GCC diagnostic ignored "-Wmissing-braces"
#endif
usb_communication_subdriver_t   usb_subdriver = {0};
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP_BESIDEFUNC) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_FIELD_INITIALIZERS_BESIDEFUNC) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_MISSING_BRACES_BESIDEFUNC) )
#pragma GCC diagnostic pop
#endif

#include "tripplite-hid.c"
/* from drivers/tripplite-hid.c we test:
static int tripplite_fix_report_desc(HIDDevice_t *pDev, HIDDesc_t *pDesc_arg);
 */

/* Owned by usbhid-ups.c in a real build; the subdriver honors it.
 * Marked extern so the compiler does not bother if it is static or shared by object files.
 */
extern int disable_fix_report_desc;
int disable_fix_report_desc = 0;

/* Lookup tables and helpers owned by usbhid-ups.c / libhid.c in a real
 * build. The repair under test does not consult them; they are referenced
 * only by the subdriver's mapping table, which must still link. */
info_lkp_t awaitingpower_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t boost_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t charging_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t commfault_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t depleted_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t discharging_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t fullycharged_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t kelvin_celsius_conversion[] = { { 0, NULL, NULL, NULL } };
info_lkp_t lowbatt_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t online_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t overheat_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t overload_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t replacebatt_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t shutdownimm_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t test_read_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t trim_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t vrange_info[] = { { 0, NULL, NULL, NULL } };

char *HIDGetIndexString(hid_dev_handle_t arg_udev, const int Index, char *buf, size_t buflen) {
	NUT_UNUSED_VARIABLE(arg_udev);
	NUT_UNUSED_VARIABLE(Index);
	if (buf && buflen) buf[0] = '\0';
	return buf;
}

const char *dstate_getinfo(const char *var) {
	NUT_UNUSED_VARIABLE(var);
	return NULL;
}

/* 10^expo without pulling in libm for one call */
static double scale_by_expo(double v, int expo) {
	while (expo > 0) { v *= 10.0; expo--; }
	while (expo < 0) { v /= 10.0; expo++; }
	return v;
}

/* NUT's HIDUnits table (drivers/libhid.c) records the exponent that NUT
 * expects the PDC power unit 0x0000D121 to carry. get_unit_expo() subtracts
 * it from the device-declared UnitExp. Duplicated here because that table and
 * get_unit_expo() are both static to libhid.c and cannot be linked from a
 * unit test without a live USB handle. */
#define NUT_EXPECTED_POWER_EXPO	7

static int failures = 0;

static void mkitem(HIDData_t *d, HIDNode_t usage, long unit, int8_t unitexp, uint8_t rid)
{
	memset((void *)d, 0, sizeof(*d));
	d->Path.Size = 4;
	d->Path.Node[0] = 0x00840004;	/* UPS */
	d->Path.Node[1] = 0x00840016;	/* PowerConverter */
	d->Path.Node[2] = 0x0084001c;	/* Output */
	d->Path.Node[3] = usage;
	d->Unit = unit;
	d->UnitExp = unitexp;
	d->ReportID = rid;
	d->LogMin = 0;
	d->LogMax = 65535;
}

static void check(const char *name, int cond)
{
	printf("  %-62s %s\n", name, cond ? "PASS" : "FAIL");
	if (!cond) failures++;
}

/* Run tripplite_fix_report_desc() over a one-item descriptor */
static int run_one(HIDData_t *item, int8_t *resulting_exp)
{
	HIDDesc_t	desc;
	HIDDevice_t	dev;
	int		ret;

	memset((void *)&desc, 0, sizeof(desc));
	memset((void *)&dev, 0, sizeof(dev));
	dev.VendorID = 0x09ae;
	dev.ProductID = 0x2012;
	desc.nitems = 1;
	desc.item = item;

	ret = tripplite_fix_report_desc(&dev, &desc);
	if (resulting_exp) *resulting_exp = item->UnitExp;
	return ret;
}

int main(void)
{
	HIDData_t	item;
	int8_t		exp_after;
	int		fired;

	printf("tripplite-hid: Report Descriptor repair tests\n");

	/* 1. The proven SMART1500LCD (09ae:2012) defect: ActivePower declares
	 *    the PDC power unit with UnitExp 0, so NUT would apply 10^-7. */
	printf("\n[1] malformed ActivePower (Unit=0x0000D121, UnitExp=0)\n");
	mkitem(&item, USAGE_POW_ACTIVE_POWER, 0x0000D121L, 0, 0x47);
	fired = run_one(&item, &exp_after);
	check("repair fires", fired == 1);
	check("UnitExp raised to 7", exp_after == NUT_EXPECTED_POWER_EXPO);
	check("effective exponent becomes 0",
		(exp_after - NUT_EXPECTED_POWER_EXPO) == 0);
	check("raw 520 now interprets as 520 W (was 5.2e-05)",
		f_equal(scale_by_expo(520.0, exp_after - NUT_EXPECTED_POWER_EXPO), 520.0));

	/* 2. A conformant descriptor must be left alone. */
	printf("\n[2] correct ActivePower (Unit=0x0000D121, UnitExp=7)\n");
	mkitem(&item, USAGE_POW_ACTIVE_POWER, 0x0000D121L, NUT_EXPECTED_POWER_EXPO, 0x47);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire", fired == 0);
	check("UnitExp untouched at 7", exp_after == NUT_EXPECTED_POWER_EXPO);

	/* 3. Right usage, but no PDC power unit declared: our own unit's
	 *    voltage items look like this and read correctly today. */
	printf("\n[3] ActivePower with Unit=0x00000000 (no PDC unit)\n");
	mkitem(&item, USAGE_POW_ACTIVE_POWER, 0x00000000L, 0, 0x47);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire", fired == 0);
	check("UnitExp untouched at 0", exp_after == 0);

	/* 4. Right unit and exponent, but an unrelated usage. */
	printf("\n[4] unrelated usage (PercentLoad) with Unit=0x0000D121, UnitExp=0\n");
	mkitem(&item, 0x00840035L /* PercentLoad */, 0x0000D121L, 0, 0x1e);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire", fired == 0);
	check("UnitExp untouched at 0", exp_after == 0);

	/* 5. The user's escape hatch must be honored. */
	printf("\n[5] disable_fix_report_desc=1 on the malformed item\n");
	disable_fix_report_desc = 1;
	mkitem(&item, USAGE_POW_ACTIVE_POWER, 0x0000D121L, 0, 0x47);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire", fired == 0);
	check("UnitExp untouched at 0", exp_after == 0);
	disable_fix_report_desc = 0;

	/* 6. Scope boundary. ApparentPower shares the same PDC unit, and
	 *    ApparentPower remains a valid semantic mapping target for
	 *    ups.power. But no Tripp Lite descriptor is currently known to
	 *    expose an ApparentPower item at all, so nothing proves the same
	 *    malformed exponent for that usage. The descriptor repair is
	 *    deliberately limited to the hardware-proven ActivePower item. */
	printf("\n[6] scope boundary: ApparentPower (Unit=0x0000D121, UnitExp=0)\n");
	mkitem(&item, USAGE_POW_APPARENT_POWER, 0x0000D121L, 0, 0x40);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire (ApparentPower is out of scope)", fired == 0);
	check("UnitExp untouched at 0", exp_after == 0);

	/* 7. Scope boundary. Voltage uses a different PDC unit (0x00F0D121)
	 *    and issue #1018 implies a different declared exponent (-1) there.
	 *    We have no device that both exhibits it and can be tested, and
	 *    output.voltage already passes through the per-product
	 *    io_voltage_scale conversion function, so a voltage repair could
	 *    compose with it. This repair must not touch voltage. */
	printf("\n[7] scope boundary: Output.Voltage (Unit=0x00F0D121, UnitExp=-1)\n");
	mkitem(&item, 0x00840030L /* Voltage */, 0x00F0D121L, -1, 0x1b);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire (voltage is out of scope)", fired == 0);
	check("UnitExp untouched at -1", exp_after == -1);

	/* 8. A naive "value looks too small, so rescale it" heuristic would
	 *    corrupt this: a legitimately tiny reading on a conformant item. */
	printf("\n[8] anti-heuristic: conformant item carrying a genuinely small value\n");
	mkitem(&item, USAGE_POW_ACTIVE_POWER, 0x0000D121L, NUT_EXPECTED_POWER_EXPO, 0x47);
	fired = run_one(&item, &exp_after);
	check("repair does NOT fire on magnitude alone", fired == 0);

	printf("\n%s: %d check(s) failed\n", failures ? "FAILED" : "OK", failures);
	return failures ? 1 : 0;
}
