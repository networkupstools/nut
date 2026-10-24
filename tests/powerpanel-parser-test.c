/* Regression cases from OLS2000ERT2UA USB text captures and documented
 * legacy powerpanel replies. No device access or control commands.
 * This program is distributed under the GNU GPL version 2 or later.
 */
#include "common.h"
#include "powerp-txt-parser.h"
#include <math.h>

static int failed;

static void check(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failed++;
	}
}

int main(void)
{
	powpan_status_t status;
	const char *frames[] = {
		"#I232.5O230.0L013B100V54.7F50.0H50.0R060Q01.1J190E270S\x80\x80\xd0\x88\x80" "W\x81\x81",
		"#I233.0O230.0L009B100V54.7F50.0H50.0R069Q00.7J149E191S\x80\x80\xd0\x88\x80" "W\x81\x81P3",
		"#I233.0O230.0L012B100V54.7F50.0H50.0R063Q01.0J150E254S\x80\x80\xd0\x88\x80" "W\x81\x81P9",
		"#I233.0O230.0L011B100V54.7F50.0H50.0R063Q00.9J200E253S\x80\x80\xd0\x88\x80" "W\x81\x81"
	};
	const int watts[] = {190, 149, 150, 200};
	const int va[] = {270, 191, 254, 253};
	const float amps[] = {1.1f, 0.7f, 1.0f, 0.9f};
	const int minutes[] = {60, 69, 63, 63};
	const char *bad[] = {
		"#I233O230J149", "#I233O230J149S\x80",
		"#I233O230Q-1S\x80\x80", "#I233O230QnanS\x80\x80",
		"#I233O230Q1e999S\x80\x80", "#I233O230J999999999999999999999S\x80\x80",
		"#I233O230J1.5S\x80\x80", "#I233O230J149J150S\x80\x80",
		"#I233O230Q111111111111111111111111111111111111111111111111S\x80\x80"
	};
	size_t i;
	for (i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
		check(powpan_parse_status(frames[i], strlen(frames[i]), &status) == 0, "captured status accepted");
		check(status.has_realpower && status.realpower == watts[i], "reported watts decoded");
		check(status.has_power && status.power == va[i], "reported VA decoded");
		check(status.has_o_current && fabs(status.o_current - amps[i]) < 0.001, "reported amperes decoded");
		check(status.has_runtime && status.runtime == minutes[i], "runtime remains in wire minutes");
	}
	{
		const char legacy[] = "#I119.0O119.0L000B100T027F060.0S\x80\x80";
		check(powpan_parse_status(legacy, sizeof(legacy)-1, &status) == 0, "legacy status accepted");
		check(status.has_u_temp && status.u_temp == 27, "legacy temperature retained");
		check(!status.has_realpower && !status.has_power && !status.has_o_current, "absent fields remain absent");
	}
	{
		const char legacy[] = "#I118.1O118.1L13B100V27.5F60.0HF60.0R65Q1.4S\x80\x84\xc0\x88\x80" "W";
		check(powpan_parse_status(legacy, sizeof(legacy)-1, &status) == 0, "legacy HF frequency accepted");
		check(status.has_o_freq && fabs(status.o_freq - 60.0f) < 0.001, "legacy output frequency retained");
	}
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		check(powpan_parse_status(bad[i], strlen(bad[i]), &status) < 0, "malformed status rejected");
	}
	check(powpan_parse_status(NULL, 0, &status) < 0, "null input rejected");
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
