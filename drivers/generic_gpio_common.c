/*  generic_gpio_common.c - common NUT driver code for GPIO attached UPS devices
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
#include "attribute.h"
#include "generic_gpio_common.h"

struct gpioups_t *gpioupsfd = (struct gpioups_t *)NULL;

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
struct gpioups_t *generic_gpio_open(const char *chipName);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void generic_gpio_close(struct gpioups_t **gpioupsfdptr);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void get_ups_rules(struct gpioups_t *upsfd, unsigned char *rulesString);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void add_rule_item(struct gpioups_t *upsfd, int newValue);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
int get_rule_lex(unsigned char *rulesBuff, int *startPos, int *endPos);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
int calc_rule_states(int upsLinesStates[], int cRules[], int subCount, int sIndex);

#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void update_ups_states(struct gpioups_t *gpioupsfdlocal);

/*
 * allocate common data structures and process/check rules
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
struct gpioups_t *generic_gpio_open(const char *chipName) {
	char	*rules = getval("rules");
	struct gpioups_t	*upsfdlocal = NULL;

	if (!rules)	/* rules is required configuration parameter */
		fatalx(EXIT_FAILURE, "UPS status calculation rules not specified");

	upsfdlocal = xcalloc(1, sizeof(*upsfdlocal));

	upsfdlocal->runOptions = 0; /*	don't use ROPT_REQRES and ROPT_EVMODE yet	*/
	upsfdlocal->chipName = chipName;

	get_ups_rules(upsfdlocal, (unsigned char *)rules);
	upsfdlocal->upsLinesStates = xcalloc(upsfdlocal->upsLinesCount, sizeof(int));

	return upsfdlocal;
}

/*
 * release common data structures
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void generic_gpio_close(struct gpioups_t **gpioupsfdptr) {
	if (gpioupsfdptr && *gpioupsfdptr) {
		if ((*gpioupsfdptr)->upsLines) {
			free((*gpioupsfdptr)->upsLines);
		}
		if ((*gpioupsfdptr)->upsLinesStates) {
			free((*gpioupsfdptr)->upsLinesStates);
		}
		if ((*gpioupsfdptr)->rules) {
			int	i;
			for (i = 0; i < (*gpioupsfdptr)->rulesCount; i++) {
				free((*gpioupsfdptr)->rules[i]);
			}
			free((*gpioupsfdptr)->rules);
		}
		free(*gpioupsfdptr);
		*gpioupsfdptr = NULL;
	}
}

/*
 * add compiled subrules item to the array
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void add_rule_item(struct gpioups_t *upsfdlocal, int newValue) {
	int	subCount = (upsfdlocal->rules[upsfdlocal->rulesCount - 1]) ? upsfdlocal->rules[upsfdlocal->rulesCount - 1]->subCount + 1 : 1;
	int	itemSize = subCount * sizeof(upsfdlocal->rules[0]->cRules[0]) + sizeof(rulesint);

	upsfdlocal->rules[upsfdlocal->rulesCount - 1] = xrealloc(upsfdlocal->rules[upsfdlocal->rulesCount - 1], itemSize);
	upsfdlocal->rules[upsfdlocal->rulesCount - 1]->subCount = subCount;
	upsfdlocal->rules[upsfdlocal->rulesCount - 1]->cRules[subCount - 1] = newValue;
}

/*
 * get next lexem out of rules configuration string recognizing separators = and ; ,
 * logical commands ^ , & , | , state names - several ascii characters matching NUT states,
 * and several numbers to denote GPIO chip lines to read statuses
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
int get_rule_lex(unsigned char *rulesBuff, int *startPos, int *endPos) {
	static unsigned char lexType[256] = {
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,    /*   00 0x00	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	 16	0x10	*/
		  0,  0,  0,  0,  0,  0,'&',  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	 32	0x20	*/
		'0','0','0','0','0','0','0','0','0','0',  0,';',  0,'=',  0,  0,	/*	 48	0x30	*/
		  0,'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a',	/*	 64	0x40	*/
		'a','a','a','a','a','a','a','a','a','a','a',  0,  0,  0,'^',  0,	/*	 80	0x50	*/
		  0,'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a',	/*	 96	0x60	*/
		'a','a','a','a','a','a','a','a','a','a','a',  0,'|',  0,  0,  0,	/*	112	0x70	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	128	0x80	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	144	0x90	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	160	0xa0	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	176	0xb0	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	192	0xc0	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	208	0xd0	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,	/*	224	0xe0	*/
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0		/*	240	0xf0	*/
	};
	unsigned char	lexTypeChr = lexType[rulesBuff[*startPos]];

	*endPos = (*startPos) + 1;
	if (lexTypeChr == 'a' || lexTypeChr == '0') {
		for (; lexType[rulesBuff[*endPos]] == lexTypeChr; (*endPos)++);
	}
	return (int)lexTypeChr;
}

/*
 * split subrules and translate them to array of commands/line numbers
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void get_ups_rules(struct gpioups_t *upsfdlocal, unsigned char *rulesString) {
	/*	statename = [^]line[&||[line]]	*/
	char	lexBuff[33];
	int	startPos = 0, endPos;
	int	lexType;
	int	lexStatus = 0;
	int	i, j, k;
	int	tranformationDelta;

	upsdebugx(4, "rules = [%s]", rulesString);
	/* state machine to process rules definition */
	while ((lexType = get_rule_lex(rulesString, &startPos, &endPos)) > 0 && lexStatus >= 0) {
		memset(lexBuff, 0, sizeof(lexBuff));
		strncpy(lexBuff, (char *)(rulesString + startPos), endPos - startPos);
		upsdebugx(4,
			"rules start %d, end %d, lexType %d, lex [%s]",
			startPos,
			endPos,
			lexType,
			lexBuff
		);
		switch(lexStatus) {
			case 0:
				if (lexType != 'a') {
					lexStatus = -1;
				} else {
					lexStatus = 1;
					upsfdlocal->rulesCount++;
					upsfdlocal->rules = xrealloc(upsfdlocal->rules, (size_t)(sizeof(upsfdlocal->rules[0])*upsfdlocal->rulesCount));
					upsfdlocal->rules[upsfdlocal->rulesCount -1 ] = xcalloc(1, sizeof(rulesint));
					strncpy(upsfdlocal->rules[upsfdlocal->rulesCount - 1]->stateName, (char *)(rulesString + startPos), endPos - startPos);
					upsfdlocal->rules[upsfdlocal->rulesCount - 1]->stateName[endPos - startPos] = 0;
				}
			break;

			case 1:
				if (lexType != '=') {
					lexStatus = -1;
				} else {
					lexStatus = 2;
				}
			break;

			case 2:
				if (lexType == '^') {
					lexStatus = 3;
					add_rule_item(upsfdlocal, RULES_CMD_NOT);
				} else if (lexType == '0') {
					lexStatus = 4;
					add_rule_item(upsfdlocal, atoi((char *)(rulesString + startPos)));
				} else {
					lexStatus = -1;
				}
			break;

			case 3:
				if (lexType != '0') {
					lexStatus = -1;
				} else {
					lexStatus = 4;
					add_rule_item(upsfdlocal, atoi((char *)(rulesString + startPos)));
				}
			break;

			case 4:
				if (lexType == '&') {
					lexStatus = 2;
					add_rule_item(upsfdlocal, RULES_CMD_AND);
				} else if (lexType == '|') {
					lexStatus = 2;
					add_rule_item(upsfdlocal, RULES_CMD_OR);
				}
				else if (lexType == ';') {
					lexStatus = 0;
				} else {
					lexStatus = -1;
				}
			break;

			default:
				lexStatus = -1;
			break;
		}
		if (lexStatus == -1)
			fatalx(LOG_ERR, "Line processing rule error at position %d", startPos);
		startPos = endPos;
	}
	if (lexType == 0 && lexStatus != 0)
		fatalx(LOG_ERR, "Line processing rule error at position %d", startPos);

	/* debug printout for extracted rules */
	upsdebugx(4, "rules count [%d]", upsfdlocal->rulesCount);
	for (i = 0; i < upsfdlocal->rulesCount; i++) {
		upsdebugx(4,
			"rule state name [%s], subcount %d",
			upsfdlocal->rules[i]->stateName,
			upsfdlocal->rules[i]->subCount
		);
		for (j = 0; j<upsfdlocal->rules[i]->subCount; j++) {
			upsdebugx(4,
				"[%s] substate %d [%d]",
				upsfdlocal->rules[i]->stateName,
				j,
				upsfdlocal->rules[i]->cRules[j]
			);
		}
	}

	/* get gpio lines used in rules, find max line number used to check with chip lines count*/
	upsfdlocal->upsLinesCount = 0;
	upsfdlocal->upsMaxLine = 0;
	for (i = 0; i < upsfdlocal->rulesCount; i++) {
		for (j = 0; j < upsfdlocal->rules[i]->subCount; j++) {
			int	pinOnList = 0;
			for (k = 0; k < upsfdlocal->upsLinesCount && !pinOnList; k++) {
				if (upsfdlocal->upsLines[k] == upsfdlocal->rules[i]->cRules[j]) {
					pinOnList = 1;
				}
			}
			if (!pinOnList) {
				if (upsfdlocal->rules[i]->cRules[j] >= 0) {
					upsfdlocal->upsLinesCount++;
					upsfdlocal->upsLines = xrealloc(upsfdlocal->upsLines, sizeof(upsfdlocal->upsLines[0])*upsfdlocal->upsLinesCount);
					upsfdlocal->upsLines[upsfdlocal->upsLinesCount - 1] = upsfdlocal->rules[i]->cRules[j];
					if (upsfdlocal->upsLines[upsfdlocal->upsLinesCount - 1] > upsfdlocal->upsMaxLine) {
						upsfdlocal->upsMaxLine = upsfdlocal->upsLines[upsfdlocal->upsLinesCount - 1];
					}
				}
			}
		}
	}

	upsdebugx(4, "UPS line count = %d", upsfdlocal->upsLinesCount);
	for (i = 0; i < upsfdlocal->upsLinesCount; i++) {
		upsdebugx(4, "UPS line%d number %d", i, upsfdlocal->upsLines[i]);
	}

	/* transform lines to indexes for easier state calculation */
	tranformationDelta = upsfdlocal->upsMaxLine - RULES_CMD_LAST + 1;
	for (i = 0; i < upsfdlocal->rulesCount; i++) {
		for (j = 0; j < upsfdlocal->rules[i]->subCount; j++) {
			if (upsfdlocal->rules[i]->cRules[j] >= 0) {
				upsfdlocal->rules[i]->cRules[j] -= tranformationDelta;
			}
		}
	}
	for (k = 0; k < upsfdlocal->upsLinesCount; k++) {
		for (i = 0; i < upsfdlocal->rulesCount; i++) {
			for (j = 0; j < upsfdlocal->rules[i]->subCount; j++) {
				if ((upsfdlocal->rules[i]->cRules[j] + tranformationDelta) == upsfdlocal->upsLines[k]) {
					upsfdlocal->rules[i]->cRules[j] = k;
				}
			}
		}
	}

	/* debug printout of transformed lines numbers */
	upsdebugx(4, "rules count [%d] translated", upsfdlocal->rulesCount);
	for (i = 0; i < upsfdlocal->rulesCount; i++) {
		upsdebugx(4,
			"rule state name [%s], subcount %d translated",
			upsfdlocal->rules[i]->stateName,
			upsfdlocal->rules[i]->subCount
		);
		for (j = 0; j < upsfdlocal->rules[i]->subCount; j++) {
			upsdebugx(4,
				"[%s] substate %d [%d]",
				upsfdlocal->rules[i]->stateName, j,
				upsfdlocal->rules[i]->cRules[j]
			);
		}
	}
}

/*
 * calculate state rule value based on GPIO line values
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
int calc_rule_states(int upsLinesStates[], int cRules[], int subCount, int sIndex) {
	int	ruleVal = 0;
	int	iopStart = sIndex;
	int	rs;

	if (iopStart < subCount) { /* calculate left side */
		if (cRules[iopStart] >= 0) {
			ruleVal = upsLinesStates[cRules[iopStart]];
		} else {
			iopStart++;
			ruleVal = !upsLinesStates[cRules[iopStart]];
		}
		iopStart++;
	}

	for (; iopStart < subCount; iopStart++) { /* right side calculation */
		if (cRules[iopStart] == RULES_CMD_OR) {
			ruleVal = ruleVal || calc_rule_states(upsLinesStates, cRules, subCount, iopStart + 1);
			break;
		} else {
			iopStart++;
			if (cRules[iopStart] == RULES_CMD_NOT) {
				iopStart++;
				rs = !upsLinesStates[cRules[iopStart]];
			} else {
				rs = upsLinesStates[cRules[iopStart]];
			}
			ruleVal = ruleVal && rs;
		}
	}

	return ruleVal;
}

/*
 *	set ups state according to rules, do adjustments for CHRG/DISCHRG
 *  and battery charge states
 */
#ifndef DRIVERS_MAIN_WITHOUT_MAIN
static
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */
void update_ups_states(struct gpioups_t *gpioupsfdlocal) {
	int	batLow = 0;
	int	bypass = 0;
	int	chargerStatusSet = 0;
	int	ruleNo;

	status_init();

	for (ruleNo = 0; ruleNo < gpioupsfdlocal->rulesCount; ruleNo++) {
		gpioupsfdlocal->rules[ruleNo]->currVal =
			calc_rule_states(
				gpioupsfdlocal->upsLinesStates,
				gpioupsfdlocal->rules[ruleNo]->cRules,
				gpioupsfdlocal->rules[ruleNo]->subCount, 0
			);
		if (gpioupsfdlocal->rules[ruleNo]->currVal) {
			status_set(gpioupsfdlocal->rules[ruleNo]->stateName);

			if (!strcmp(gpioupsfdlocal->rules[ruleNo]->stateName, "CHRG")) {
				dstate_setinfo("battery.charger.status", "%s", "charging");
				chargerStatusSet++;
			}
			if (!strcmp(gpioupsfdlocal->rules[ruleNo]->stateName, "DISCHRG")) {
				dstate_setinfo("battery.charger.status", "%s", "discharging");
				chargerStatusSet++;
			}
			if (!strcmp(gpioupsfdlocal->rules[ruleNo]->stateName, "LB")) {
				batLow = 1;
			}
			if (!strcmp(gpioupsfdlocal->rules[ruleNo]->stateName, "BYPASS")) {
				bypass = 1;
			}
		}
		if (gpioupsfdlocal->aInfoAvailable &&
			gpioupsfdlocal->rules[ruleNo]->archVal != gpioupsfdlocal->rules[ruleNo]->currVal) {
			upslogx(LOG_WARNING, "UPS state [%s] changed to %d",
				gpioupsfdlocal->rules[ruleNo]->stateName,
				gpioupsfdlocal->rules[ruleNo]->currVal
			);
		}
		gpioupsfdlocal->rules[ruleNo]->archVal = gpioupsfdlocal->rules[ruleNo]->currVal;
	}

	if (chargerStatusSet <= 0) {
		dstate_delinfo("battery.charger.status");
	}

	if (dstate_getinfo("battery.charge.low") != NULL) {
		if (batLow) {
			dstate_setinfo("battery.charge", "%s", dstate_getinfo("battery.charge.low"));
		} else {
			dstate_setinfo("battery.charge", "%s", "100");
		}
	}

	if (bypass) {
		dstate_delinfo("battery.charge");
	}

	gpioupsfdlocal->aInfoAvailable = 1;

	status_commit();
}

void upsdrv_initinfo(void)
{
	if (testvar("mfr")) {
		dstate_setinfo("device.mfr", "%s", getval("mfr"));
	}
	if (testvar("model")) {
		dstate_setinfo("device.model", "%s", getval("model"));
	}
}

void upsdrv_updateinfo(void)
{
	/*  read GPIO lines states	*/
	gpio_get_lines_states(gpioupsfd);

	/*  calculate/set UPS states based on line values	*/
	update_ups_states(gpioupsfd);

	/*  no protocol failures possible - mark data as OK	*/
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "mfr", "Override UPS manufacturer name");
	addvar(VAR_VALUE, "model", "Override UPS model name");
	addvar(VAR_VALUE, "rules", "Line rules to produce status strings");
}

void upsdrv_initups(void)
{
	/* prepare rules and allocate related structures */
	gpioupsfd = generic_gpio_open(device_path);
	/* open GPIO chip and check pin consistence */
	if (gpioupsfd) {
		gpio_open(gpioupsfd);
	}
}

void upsdrv_cleanup(void)
{
	if (gpioupsfd) {
		/* release gpio library resources	*/
		gpio_close(gpioupsfd);
		/* release related generic resources	*/
		generic_gpio_close(&gpioupsfd);
	}
}
