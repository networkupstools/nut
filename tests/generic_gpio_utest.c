/*  tests/generic_gpio_utest.c - gpio NUT driver code test tool
 *
 *  Copyright (C)
 *	2023 - 2025		Modris Berzonis <modrisb@apollo.lv>
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

#include "config.h"
#include "main.h"
#include "dstate.h"
#include "attribute.h"
#include "nut_stdint.h"
#include "generic_gpio_utest.h"

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

extern struct gpioups_t *gpioupsfd;

static int done=0;
static int test_with_exit;
static jmp_buf env_buffer;
static FILE * testData;
static int fEof;
static int cases_passed;
static int cases_failed;

static char * pass_fail[2] = {"pass", "fail"};

void getWithoutUnderscores(char *var) {
	int i;

	fEof=fscanf(testData, "%s", var);
	for (i=0; var[i]; i++) {
		if (var[i]=='_') var[i]=' ';
	}
}

#define MFR "CyberPower"
#define MODEL "CyberShield CSN27U12V"
#define DESCRIPTION "modem and DNS server UPS"

int get_test_status(struct gpioups_t *result, int on_fail_path) {
	int	expecting_failure;
	int	upsLinesCount;  /* no of lines used in rules */
	int	upsMaxLine;     /* maximum line number referenced in rules */
	int	rulesCount;     /* rules subitem count: no of NUT states defined in rules*/
	char	stateName[12];  /* NUT state name for rules in cRules */
	int	subCount;       /* element count in translated rules subitem */
	int	ruleInt;
	int	i, j;

	fEof = fscanf(testData, "%d", &expecting_failure);
	if (on_fail_path) {
		if(expecting_failure) cases_failed++; else cases_passed++;
		return expecting_failure;
	}

	if (!expecting_failure) {
		cases_failed++;
		printf("expecting case to fail\n");
		return 1;
	}

	fEof = fscanf(testData, "%d", &upsLinesCount);
	if (result->upsLinesCount!=upsLinesCount) {
		cases_failed++;
		printf("expecting upsLinesCount %d, got %d\n", upsLinesCount, result->upsLinesCount);
		return 1;
	}

	fEof = fscanf(testData, "%d", &upsMaxLine);
	if (result->upsMaxLine!=upsMaxLine) {
		cases_failed++;
		printf("expecting rulesCount %d, got %d\n", upsMaxLine, result->upsMaxLine);
		return 1;
	}

	fEof = fscanf(testData, "%d", &rulesCount);
	if (result->rulesCount!=rulesCount) {
		cases_failed++;
		printf("expecting rulesCount %d, got %d\n", rulesCount, result->rulesCount);
		return 1;
	}

	for (i=0; i<result->rulesCount; i++) {
		fEof=fscanf(testData, "%s", stateName);
		if(strcmp(result->rules[i]->stateName,stateName)) {
			cases_failed++;
			printf("expecting stateName %s, got %s for rule %d\n", stateName, result->rules[i]->stateName, i);
			return 1;
		}
		fEof=fscanf(testData, "%d", &subCount);
		if(result->rules[i]->subCount!=subCount) {
			cases_failed++;
			printf("expecting subCount %d, got %d for rule %d\n", result->rules[i]->subCount, subCount, i);
			return 1;
		}
		for (j=0; j<subCount; j++) {
			fEof=fscanf(testData, "%d", &ruleInt);
			if(result->rules[i]->cRules[j]!=ruleInt) {
				cases_failed++;
				printf("expecting cRule %d, got %d for rule %d subRule %d\n", ruleInt, result->rules[i]->cRules[j], i, j);
				return 1;
			}
		}
	}

	cases_passed++;
	return 0;
}

void exit(int code)
{
	if (!done) {
		longjmp(env_buffer, 1);
	}
	else {
		_exit(code);
	}
}

int main(int argc, char **argv) {
	int jmp_result;
	char rules[128];
	char testType[128];
	char testDescFileNameBuf[LARGEBUF];
	char *testDescFileName = "generic_gpio_test.txt";
	unsigned int i;
	unsigned long version = WITH_LIBGPIO_VERSION;
	printf("Tests running for libgpiod library version %lu\n", version);

	test_with_exit=0;

	if(argc==2) {
		testDescFileName=argv[1];
	}

	testData = fopen (testDescFileName, "r");
	if(!testData) {
		if (!strchr(testDescFileName, '/')) {
			/* "srcdir" may be set by automake test harness, see
			 * https://www.gnu.org/software/automake/manual/1.12.2/html_node/Scripts_002dbased-Testsuites.html
			 */
			char *testDescFileDir = getenv("srcdir");
			if (testDescFileDir) {
				printf("failed to open test description file %s "
					"in current working directory, "
					"retrying with srcdir %s\n",
					testDescFileName, testDescFileDir);
				if (snprintf(testDescFileNameBuf, sizeof(testDescFileNameBuf), "%s/%s",
					testDescFileDir, testDescFileName) > 0
				) {
					testDescFileName = testDescFileNameBuf;
					testData = fopen (testDescFileName, "r");
				}
			}
		}

		if(!testData) {
			done = 1;
			printf("failed to open test description file %s\n", testDescFileName);
			exit(EXIT_FAILURE);
		}
	}

	cases_passed=0;
	cases_failed=0;
	fEof = 1;
	for (i=0; fEof!=EOF; i++) {
		char fmt[16];
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		/* To avoid safety warnings, must provide a limit
		 * here (bufsize - 1), and use fixed format strings
		 * because scanf() does not support asterisk for
		 * width specifier; have to create it on the fly.
		 */
		snprintf(fmt, sizeof(fmt), "%%%" PRIuSIZE "s", sizeof(rules)-1);
		do {
			fEof=fscanf(testData, fmt, rules);
		} while(strcmp("*", rules));
		snprintf(fmt, sizeof(fmt), "%%%" PRIuSIZE "s", sizeof(testType)-1);
		fEof=fscanf(testData, fmt, testType);
		snprintf(fmt, sizeof(fmt), "%%%" PRIuSIZE "s", sizeof(rules)-1);
		fEof=fscanf(testData, fmt, rules);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		if(fEof!=EOF) {
			if(!strcmp(testType, "rules")) {
				struct gpioups_t *upsfdtest = xcalloc(1, sizeof(*upsfdtest));
				/* NOTE: here and below, freed by generic_gpio_close(&upsfdtest) */
				jmp_result = setjmp(env_buffer);
				if(jmp_result) {	/* test case  exiting */
					printf("%s %s test rule %u [%s]\n", pass_fail[get_test_status(upsfdtest, 1)], testType, i, rules);
				} else { /* run test case */
					get_ups_rules(upsfdtest, (unsigned char *)rules);
					printf("%s %s test rule %u [%s]\n", pass_fail[get_test_status(upsfdtest, 0)], testType, i, rules);
				}
				generic_gpio_close(&upsfdtest);
			}
			if(!strcmp(testType, "states")) {
				int expectedStateValue;
				int calculatedStateValue;
				struct gpioups_t *upsfdtest = xcalloc(1, sizeof(*upsfdtest));
				int j;

				get_ups_rules(upsfdtest, (unsigned char *)rules);
				upsfdtest->upsLinesStates = xcalloc(upsfdtest->upsLinesCount, sizeof(int));
				for (j=0; j < upsfdtest->upsLinesCount; j++) {
					fEof=fscanf(testData, "%d", &upsfdtest->upsLinesStates[j]);
				}
				for (j=0; j < upsfdtest->rulesCount; j++) {
					fEof=fscanf(testData, "%d", &expectedStateValue);
					calculatedStateValue=calc_rule_states(upsfdtest->upsLinesStates, upsfdtest->rules[j]->cRules, upsfdtest->rules[j]->subCount, 0);
					if (expectedStateValue==calculatedStateValue) {
						printf("%s %s test rule %u [%s]\n", pass_fail[0], testType, i, rules);
						cases_passed++;
					} else {
						int k;
						printf("%s %s test rule %u [%s] %s", pass_fail[1], testType, i, rules, upsfdtest->rules[j]->stateName);
						for(k=0; k<upsfdtest->upsLinesCount; k++) {
							printf(" %d", upsfdtest->upsLinesStates[k]);
						}
						printf("\n");
						cases_failed++;
					}
				}
				generic_gpio_close(&upsfdtest);
			}
			if(!strcmp(testType, "update")) {
				char upsStatus[256];
				char chargeStatus[256];
				char chargeLow[256];
				char charge[256];
				struct gpioups_t *upsfdtest = xcalloc(1, sizeof(*upsfdtest));
				int j;

				/* "volatile" trickery to avoid the likes of:
				 *    error: variable 'failed' might be clobbered by 'longjmp' or 'vfork' [-Werror=clobbered]
				 * due to presence of setjmp().
				 */
				int volatile failed = 0;
				const char * volatile currUpsStatus = NULL;
				const char * volatile currChargerStatus = NULL;
				const char * volatile currCharge = NULL;

				get_ups_rules(upsfdtest, (unsigned char *)rules);
				upsfdtest->upsLinesStates = xcalloc(upsfdtest->upsLinesCount, sizeof(int));
				for (j = 0; j < upsfdtest->upsLinesCount; j++) {
					fEof=fscanf(testData, "%d", &upsfdtest->upsLinesStates[j]);
				}
				getWithoutUnderscores(upsStatus);
				getWithoutUnderscores(chargeStatus);
				getWithoutUnderscores(chargeLow);
				getWithoutUnderscores(charge);
				if (strcmp(chargeLow, "."))
					dstate_setinfo("battery.charge.low", "%s", chargeLow);
				jmp_result = setjmp(env_buffer);
				if (jmp_result) {
					failed=1;
				} else {
					update_ups_states(upsfdtest);
					currUpsStatus=dstate_getinfo("ups.status");
					currChargerStatus=dstate_getinfo("battery.charger.status");
					currCharge=dstate_getinfo("battery.charge");
					if(strcmp(currUpsStatus, upsStatus)) failed=1;
					if( strcmp(chargeStatus,".") && (!currChargerStatus || strcmp(currChargerStatus, chargeStatus))) failed=1;
					if(!strcmp(chargeStatus,".") && currChargerStatus!=NULL) failed=1;
					if( strcmp(chargeLow,".") && strcmp(charge,".") && (!currCharge || strcmp(currCharge, charge))) failed=1;
					if(!strcmp(chargeLow,".") && !strcmp(charge,".") && currCharge!=NULL) failed=1;
				}
				generic_gpio_close(&upsfdtest);
				printf("%s %s test rule %u [%s] ([%s] %s %s (%s)) ([%s] %s %s)\n",
					pass_fail[failed], testType, i, rules,
					upsStatus, chargeStatus, charge, chargeLow,
					NUT_STRARG(currUpsStatus),
					NUT_STRARG(currChargerStatus),
					NUT_STRARG(currCharge));
				if (!failed) {
					cases_passed++;
				} else {
					cases_failed++;
				}
				vartab_free(); vartab_h = NULL;
			}
			if(!strcmp(testType, "library")) {
				char chipNameLocal[NUT_GPIO_CHIPNAMEBUF];
				int expecting_failure, failed;
				char subType[NUT_GPIO_SUBTYPEBUF];
				fEof=fscanf(testData, "%d", &expecting_failure);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
				/* To avoid safety warnings, must provide a limit
				 * here (bufsize - 1), and use fixed format strings
				 * because scanf() does not support asterisk for
				 * width specifier; have to create it on the fly.
				 */
				snprintf(fmt, sizeof(fmt), "%%%us", NUT_GPIO_CHIPNAMEBUF-1);
				fEof=fscanf(testData, fmt, chipNameLocal);
				snprintf(fmt, sizeof(fmt), "%%%us", NUT_GPIO_SUBTYPEBUF-1);
				fEof=fscanf(testData, fmt, subType);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
				jmp_result = setjmp(env_buffer);
				failed = expecting_failure;
				if(jmp_result) {	/* test case  exiting */
					if(expecting_failure) failed=0;
					upsdrv_cleanup();
				} else {
					if(expecting_failure) failed=1;
					device_path = chipNameLocal;
					if(strcmp(subType, "initinfo") || !expecting_failure) {
						addvar(VAR_VALUE, "rules", "");
						storeval("rules", rules);
					}
					addvar(VAR_VALUE, "mfr", MFR);
					storeval("mfr", MFR);
					addvar(VAR_VALUE, "model", MODEL);
					storeval("model", MODEL);
					dstate_setinfo("device.description", DESCRIPTION);
					upsdrv_initups();
					if(!strcmp(subType, "initinfo")) {
						upsdrv_makevartable();
						if(expecting_failure) setNextLinesReadToFail();
						upsdrv_initinfo();
						if(!dstate_getinfo("device.mfr") || strcmp(dstate_getinfo("device.mfr"), MFR) ||
							!dstate_getinfo("device.model") || strcmp(dstate_getinfo("device.model"), MODEL) ||
							!dstate_getinfo("device.description") || strcmp(dstate_getinfo("device.description"), DESCRIPTION)) failed=1;
					}
					if(!strcmp(subType, "updateinfo")) {
						int k;
						for(k=0; k<gpioupsfd->upsLinesCount; k++) {
							gpioupsfd->upsLinesStates[k]=-1;
						}
						if(expecting_failure) setNextLinesReadToFail();
						upsdrv_updateinfo();
						for(k=0; k<gpioupsfd->upsLinesCount && failed!=1; k++) {
							if(gpioupsfd->upsLinesStates[k]<0) failed=1;
						}
					}
					upsdrv_cleanup();
				}
				printf("%s %s %s test rule %u [%s] %s %d\n",
					pass_fail[failed], testType, subType, i, rules, chipNameLocal, expecting_failure);
				if (!failed) {
					cases_passed++;
				} else {
					cases_failed++;
				}
				vartab_free();
				vartab_h = NULL;
			}
		}
	}

	printf("test_rules completed. Total cases %d, passed %d, failed %d\n",
		cases_passed+cases_failed, cases_passed, cases_failed);
	fclose(testData);
	done = 1;

	dstate_free();
	/* Should be safe if we happen to run this twice for
	 * generic_gpio_common.c, it only frees driver variables once */
	upsdrv_cleanup();

	/* Return 0 (exit-code OK, boolean false) if no tests failed and some ran */
	if ( (cases_failed == 0) && (cases_passed > 0) )
		return 0;

	return 1;
}
