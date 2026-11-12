/* driver_shutdown_utest.c - replay shutdown through the real driver framework.
 * Checks command results, nested legacy callbacks, process termination and
 * successful legacy callbacks without contacting any UPS. GPLv2+.
 * Copyright (C) 2026 NUT contributors.
 */
#include "config.h"
#include "main.h"

upsdrv_info_t upsdrv_info = { "Shutdown test", "0.01", "NUT contributors", DRV_EXPERIMENTAL, { NULL } };
static int behavior, failures;

void upsdrv_cleanup(void) {}
void upsdrv_initups(void) {}
void upsdrv_initinfo(void) {}
void upsdrv_makevartable(void) {}
void upsdrv_tweak_prognames(void) {}
void upsdrv_updateinfo(void) {}
void upsdrv_help(void) {}

void upsdrv_shutdown(void)
{
	if (behavior == 3) {
		behavior = 1;
		upsdrv_shutdown_sdcommands_or_default(NULL, NULL);
	} else if (behavior == 4) {
		set_exit_flag(SIGTERM);
	} else if (handling_upsdrv_shutdown > 0 && behavior) {
		set_exit_flag(behavior == 1 ? EF_EXIT_FAILURE : EF_EXIT_SUCCESS);
	}
}

static int reject_command(const char *name, const char *extra)
{
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(extra);
	return STAT_INSTCMD_UNKNOWN;
}

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "line %d: %s\n", __LINE__, #test); failures++; } } while (0)

int main(void)
{
	int i, has_handler;
	char *used = NULL;
	/* The callback registry is normally populated by the executable entry. */
	default_register_upsdrv_callbacks();
	upsname = "shutdown-test";
	for (has_handler = 0; has_handler < 2; has_handler++) {
		upsh.instcmd = has_handler ? reject_command : NULL;
		for (i = 0; i < 20; i++) {
			behavior = 1;
			exit_flag = 0;
			handling_upsdrv_shutdown = 0;
			CHECK(main_instcmd("shutdown.default", NULL, NULL) == STAT_INSTCMD_INVALID);
			CHECK(exit_flag == 0 && handling_upsdrv_shutdown == 0);
			CHECK(do_loop_shutdown_commands("shutdown.default", &used) == STAT_INSTCMD_INVALID);
			CHECK(used == NULL && exit_flag == 0);
			handling_upsdrv_shutdown = 1;
			CHECK(upsdrv_shutdown_sdcommands_or_default(NULL, NULL) == STAT_INSTCMD_INVALID);
			CHECK(exit_flag == EF_EXIT_FAILURE);
			dstate_setinfo("driver.flag.allow_killpower", "1");
			CHECK(main_instcmd("driver.killpower", NULL, NULL) == STAT_INSTCMD_INVALID);
			CHECK(exit_flag == EF_EXIT_FAILURE);
			/* Both legacy no-result and explicit success remain successful. */
			for (behavior = 0; behavior <= 2; behavior += 2) {
				exit_flag = 0;
				CHECK(upsdrv_shutdown_sdcommands_or_default(NULL, &used) == STAT_INSTCMD_HANDLED);
				CHECK(exit_flag == EF_EXIT_SUCCESS && used && !strcmp(used, "shutdown.default"));
				free(used);
				used = NULL;
			}
			behavior = 3;
			exit_flag = 0;
			handling_upsdrv_shutdown = 0;
			CHECK(main_instcmd("shutdown.default", NULL, NULL) == STAT_INSTCMD_INVALID);
			CHECK(exit_flag == 0 && handling_upsdrv_shutdown == 0);
			behavior = 4;
			CHECK(main_instcmd("shutdown.default", NULL, NULL) == STAT_INSTCMD_HANDLED);
			CHECK(exit_flag == SIGTERM);
		}
	}
	dstate_free();
	printf("Shutdown framework replay: %d failures\n", failures);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
