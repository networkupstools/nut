/*  generic_gpio_utest.c - gpio NUT driver code test tool
 *
 *  Copyright (C)
 *	2023       	Modris Berzonis <modrisb@apollo.lv>
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
	fEof=fscanf(testData, "%s", var);
	for(int i=0; var[i]; i++) {
		if(var[i]=='_') var[i]=' ';
	}
}

#define MFR "CyberPower"
#define MODEL "CyberShield CSN27U12V"
#define DESCRIPTION "modem and DNS server UPS"

int get_test_status(struct gpioups_t *result, int on_fail_path) {
	int		expecting_failure;
    int     upsLinesCount;  /* no of lines used in rules */
    int     upsMaxLine;     /* maximum line number referenced in rules */
    int     rulesCount;     /* rules subitem count: no of NUT states defined in rules*/
    char    stateName[12];  /* NUT state name for rules in cRules */
    int     subCount;       /* element count in translated rules subitem */
	int		ruleInt;

	fEof=fscanf(testData, "%d", &expecting_failure);
	if(on_fail_path) {
		if(expecting_failure) cases_failed++; else cases_passed++;
		return expecting_failure;
	}

	if(!expecting_failure) {
		cases_failed++;
		printf("expecting case to fail\n");
		return 1;
	}

	fEof=fscanf(testData, "%d", &upsLinesCount);
	if(result->upsLinesCount!=upsLinesCount) {
		cases_failed++;
		printf("expecting upsLinesCount %d, got %d\n", upsLinesCount, result->upsLinesCount);
		return 1;
	}

	fEof=fscanf(testData, "%d", &upsMaxLine);
	if(result->upsMaxLine!=upsMaxLine) {
		cases_failed++;
		printf("expecting rulesCount %d, got %d\n", upsMaxLine, result->upsMaxLine);
		return 1;
	}

	fEof=fscanf(testData, "%d", &rulesCount);
	if(result->rulesCount!=rulesCount) {
		cases_failed++;
		printf("expecting rulesCount %d, got %d\n", rulesCount, result->rulesCount);
		return 1;
	}

	for(int i=0; i<result->rulesCount; i++) {
		fEof=fscanf(testData, "%s", stateName);
		if(!strcmp(result->rules[i]->stateName,stateName)) {
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
		for(int j=0; j<subCount; j++) {
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
	test_with_exit=0;
	int jmp_result;
	char rules[128];
	char testType[128];
	char testDescFileNameBuf[LARGEBUF];
	char *testDescFileName="generic_gpio_test.txt";

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
	for(unsigned int i=0; fEof!=EOF; i++) {
		do {
			fEof=fscanf(testData, "%s", rules);
		} while(strcmp("*", rules));
		fEof=fscanf(testData, "%s", testType);
		fEof=fscanf(testData, "%s", rules);
		if(fEof!=EOF) {
			if(!strcmp(testType, "rules")) {
				jmp_result = setjmp(env_buffer);
				struct gpioups_t *upsfdtest=xcalloc(sizeof(*upsfdtest),1);
				if(jmp_result) {	/* test case  exiting */
					generic_gpio_close(upsfdtest);
					printf("%s %s test rule %d [%s]\n", pass_fail[get_test_status(upsfdtest, 1)], testType, i, rules);
				} else { /* run test case */
					get_ups_rules(upsfdtest, (unsigned char *)rules);
					generic_gpio_close(upsfdtest);
					printf("%s %s test rule %d [%s]\n", pass_fail[get_test_status(upsfdtest, 0)], testType, i, rules);
				}
			}
			if(!strcmp(testType, "states")) {
				int expectedStateValue;
				int calculatedStateValue;
				struct gpioups_t *upsfdtest=xcalloc(sizeof(*upsfdtest),1);
				get_ups_rules(upsfdtest, (unsigned char *)rules);
				upsfdtest->upsLinesStates=xcalloc(sizeof(int),upsfdtest->upsLinesCount);
				for(int j=0; j<upsfdtest->upsLinesCount; j++) {
					fEof=fscanf(testData, "%d", &upsfdtest->upsLinesStates[j]);
				}
				for(int j=0; j<upsfdtest->rulesCount; j++) {
					fEof=fscanf(testData, "%d", &expectedStateValue);
					calculatedStateValue=calc_rule_states(upsfdtest->upsLinesStates, upsfdtest->rules[j]->cRules, upsfdtest->rules[j]->subCount, 0);
					if(expectedStateValue==calculatedStateValue) {
						printf("%s %s test rule %d [%s]\n", pass_fail[0], testType, i, rules);
						cases_passed++;
					} else {
						printf("%s %s test rule %d [%s] %s", pass_fail[1], testType, i, rules, upsfdtest->rules[j]->stateName);
						for(int k=0; k<upsfdtest->upsLinesCount; k++) {
							printf(" %d", upsfdtest->upsLinesStates[k]);
						}
						printf("\n");
						cases_failed++;
					}
				}
				generic_gpio_close(upsfdtest);
			}
			if(!strcmp(testType, "update")) {
				char upsStatus[256];
				char chargeStatus[256];
				char chargeLow[256];
				char charge[256];
				struct gpioups_t *upsfdtest=xcalloc(sizeof(*upsfdtest),1);
				get_ups_rules(upsfdtest, (unsigned char *)rules);
				upsfdtest->upsLinesStates=xcalloc(sizeof(int),upsfdtest->upsLinesCount);
				for(int j=0; j<upsfdtest->upsLinesCount; j++) {
					fEof=fscanf(testData, "%d", &upsfdtest->upsLinesStates[j]);
				}
				getWithoutUnderscores(upsStatus);
				getWithoutUnderscores(chargeStatus);
				getWithoutUnderscores(chargeLow);
				getWithoutUnderscores(charge);
				if(strcmp(chargeLow, "."))
					dstate_setinfo("battery.charge.low", "%s", chargeLow);
				jmp_result = setjmp(env_buffer);
				int failed=0;
				const char *currUpsStatus=NULL;
				const char *currChargerStatus=NULL;
				const char *currCharge=NULL;
				if(jmp_result) {
					failed=1;
					generic_gpio_close(upsfdtest);
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
					generic_gpio_close(upsfdtest);
				}
				printf("%s %s test rule %d [%s] ([%s] %s %s (%s)) ([%s] %s %s)\n",
					pass_fail[failed], testType, i, rules,
					upsStatus, chargeStatus, charge, chargeLow,
					currUpsStatus, currChargerStatus, currCharge);
				if(!failed) {
					cases_passed++;
				} else {
					cases_failed++;
				}
				vartab_free(); vartab_h = NULL;
			}
			if(!strcmp(testType, "library")) {
				char chipNameLocal[NUT_GPIO_CHIPNAMEBUF];
				int expecting_failure;
				char subType[NUT_GPIO_SUBTYPEBUF];
				fEof=fscanf(testData, "%d", &expecting_failure);
				fEof=fscanf(testData, "%s", chipNameLocal);
				fEof=fscanf(testData, "%s", subType);
				jmp_result = setjmp(env_buffer);
				int failed=expecting_failure;
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
						for(int k=0; k<gpioupsfd->upsLinesCount; k++) {
							gpioupsfd->upsLinesStates[k]=-1;
						}
						if(expecting_failure) setNextLinesReadToFail();
						upsdrv_updateinfo();
						for(int k=0; k<gpioupsfd->upsLinesCount && failed!=1; k++) {
							if(gpioupsfd->upsLinesStates[k]<0) failed=1;
						}
					}
					upsdrv_cleanup();
				}
				printf("%s %s %s test rule %d [%s] %s %d\n",
					pass_fail[failed], testType, subType, i, rules, chipNameLocal, expecting_failure);
				if(!failed) {
					cases_passed++;
				} else {
					cases_failed++;
				}
				vartab_free(); vartab_h = NULL;
			}
		}
	}

	printf("test_rules completed. Total cases %d, passed %d, failed %d\n", cases_passed+cases_failed, cases_passed, cases_failed);
	fclose(testData);
	done = 1;
}
