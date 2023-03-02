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

struct gpioups_t *gpioupsfd=(struct gpioups_t *)NULL;

static struct gpioups_t *generic_gpio_open(const char *chipName);
static void generic_gpio_close(struct gpioups_t *gpioupsfd);
static void get_ups_rules(struct gpioups_t *upsfd, unsigned char *rulesString);
static void add_rule_item(struct gpioups_t *upsfd, int newValue);
static int get_rule_lex(unsigned char *rulesBuff, int *startPos, int *endPos);
static int calc_rule_states(int cRules[], int subCount, int sIndex);
static void update_ups_states(struct gpioups_t *gpioupsfd);

/*
 * allocate common data structures and process/check rules
 */
static struct gpioups_t *generic_gpio_open(const char *chipName) {
	struct gpioups_t *upsfdlocal=xcalloc(sizeof(*upsfdlocal),1);
	upsfdlocal->runOptions=0; /*	don't use ROPT_REQRES and ROPT_EVMODE yet	*/
	upsfdlocal->chipName=chipName;
	get_ups_rules(upsfdlocal, (unsigned char *)getval("rules"));
	return upsfdlocal;
}

/*
 * release common data structures
 */
static void generic_gpio_close(struct gpioups_t *gpioupsfd) {
	if(gpioupsfd) {
		if(gpioupsfd->upsLines) {
			free(gpioupsfd->upsLines);
		}
		if(gpioupsfd->upsLinesStates) {
			free(gpioupsfd->upsLinesStates);
		}
		if(gpioupsfd->rules) {
			int i;
			for(i=0; i<gpioupsfd->rulesCount; i++) {
				free(gpioupsfd->rules[i]);
			}
		}
		free(gpioupsfd);
	}
}

/*
 * add compiled subrules item to the array
 */
static void add_rule_item(struct gpioups_t *upsfd, int newValue) {
	int     subCount = (upsfd->rules[upsfd->rulesCount-1]) ? upsfd->rules[upsfd->rulesCount-1]->subCount+1 : 1;
	int     itemSize = subCount*sizeof(upsfd->rules[0]->cRules[0])+sizeof(rulesint);

	upsfd->rules[upsfd->rulesCount-1] = xrealloc(upsfd->rules[upsfd->rulesCount-1], itemSize);
	upsfd->rules[upsfd->rulesCount-1]->subCount = subCount;
	upsfd->rules[upsfd->rulesCount-1]->cRules[subCount-1] = newValue;
}

/*
 * get next lexem out of rules configuration string recognizing separators = and ; ,
 * logical commands ^ , & , | , state names - several ascii characters matching NUT states,
 * and several numbers to denote GPIO chip lines to read statuses
 */
static int get_rule_lex(unsigned char *rulesBuff, int *startPos, int *endPos) {
	static unsigned char lexType[256]={
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
	unsigned char lexTypeChr = lexType[rulesBuff[*startPos]];

	*endPos = (*startPos)+1;
	if(lexTypeChr == 'a' || lexTypeChr == '0') {
		for(; lexType[rulesBuff[*endPos]] == lexTypeChr; (*endPos)++);
	}
	return (int)lexTypeChr;
}

/*
 * split subrules and translate them to array of commands/line numbers
 */
static void get_ups_rules(struct gpioups_t *upsfd, unsigned char *rulesString) {
	/*	statename=[^]line[&||[line]]	*/
	char    lexBuff[33];
	int     startPos = 0, endPos;
	int     lexType;
	int lexStatus = 0;
	int	i, j, k;
	int	tranformationDelta;

	upsdebugx(LOG_DEBUG, "rules = [%s]", rulesString);
	/* state machine to process rules definition */
	while((lexType=get_rule_lex(rulesString, &startPos, &endPos)) > 0 && lexStatus >= 0) {
		memset(lexBuff, 0, sizeof(lexBuff));
		strncpy(lexBuff, (char *)(rulesString+startPos), endPos-startPos);
		upsdebugx(
			LOG_DEBUG,
			"rules start %d, end %d, lexType %d, lex [%s]",
			startPos,
			endPos,
			lexType,
			lexBuff
		);
		switch(lexStatus) {
			case 0:
				if(lexType != 'a') {
					lexStatus=-1;
				} else {
					lexStatus = 1;
					upsfd->rulesCount++;
					upsfd->rules = xrealloc(upsfd->rules, (size_t)(sizeof(upsfd->rules[0])*upsfd->rulesCount));
					upsfd->rules[upsfd->rulesCount-1] = xcalloc(sizeof(rulesint), 1);
					strncpy(upsfd->rules[upsfd->rulesCount-1]->stateName, (char *)(rulesString+startPos), endPos-startPos);
					upsfd->rules[upsfd->rulesCount-1]->stateName[endPos-startPos] = 0;
				}
			break;

			case 1:
				if(lexType != '=') {
					lexStatus = -1;
				} else {
					lexStatus = 2;
				}
			break;

			case 2:
				if(lexType == '^') {
					lexStatus = 3;
					add_rule_item(upsfd, RULES_CMD_NOT);
				} else if(lexType == '0') {
					lexStatus = 4;
					add_rule_item(upsfd, atoi((char *)(rulesString+startPos)));
				} else {
					lexStatus = -1;
				}
			break;

			case 3:
				if(lexType != '0') {
					lexStatus = -1;
				} else {
					lexStatus = 4;
					add_rule_item(upsfd, atoi((char *)(rulesString+startPos)));
				}
			break;

			case 4:
				if(lexType == '&') {
					lexStatus = 2;
					add_rule_item(upsfd, RULES_CMD_AND);
				} else if(lexType == '|') {
					lexStatus = 2;
					add_rule_item(upsfd, RULES_CMD_OR);
				}
				else if(lexType == ';') {
					lexStatus = 0;
				} else {
					lexStatus = -1;
				}
			break;

			default:
				lexStatus = -1;
			break;
		}
		if(lexStatus == -1)
			fatalx(LOG_ERR, "Line processing rule error at position %d", startPos);
		startPos = endPos;
	}

	/* debug printout for extracted rules */
	upsdebugx(LOG_DEBUG, "rules count [%d]", upsfd->rulesCount);
	for(i = 0; i < upsfd->rulesCount; i++) {
		upsdebugx(
			LOG_DEBUG,
			"rule state name [%s], subcount %d",
			upsfd->rules[i]->stateName,
			upsfd->rules[i]->subCount
		);
		for(j = 0; j<upsfd->rules[i]->subCount; j++) {
			upsdebugx(
				LOG_DEBUG,
				"[%s] substate %d [%d]",
				upsfd->rules[i]->stateName,
				j,
				upsfd->rules[i]->cRules[j]
			);
		}
	}

	/* get gpio lines used in rules, find max line number used to check with chip lines count*/
	upsfd->upsLinesCount = 0;
	upsfd->upsMaxLine = 0;
	for(i = 0; i < upsfd->rulesCount; i++) {
		for(j = 0; j < upsfd->rules[i]->subCount; j++) {
			int pinOnList = 0;
			for(k = 0; k < upsfd->upsLinesCount && !pinOnList; k++) {
				if(upsfd->upsLines[k] == upsfd->rules[i]->cRules[j]) {
					pinOnList = 1;
				}
			}
			if(!pinOnList) {
				if(upsfd->rules[i]->cRules[j] >= 0) {
					upsfd->upsLinesCount++;
					upsfd->upsLines = xrealloc(upsfd->upsLines, sizeof(upsfd->upsLines[0])*upsfd->upsLinesCount);
					upsfd->upsLines[upsfd->upsLinesCount-1] = upsfd->rules[i]->cRules[j];
					if(upsfd->upsLines[upsfd->upsLinesCount-1] > upsfd->upsMaxLine) {
						upsfd->upsMaxLine = upsfd->upsLines[upsfd->upsLinesCount-1];
					}
				}
			}
		}
	}

	upsdebugx(LOG_DEBUG, "UPS line count = %d", upsfd->upsLinesCount);
	for(i = 0; i < upsfd->upsLinesCount; i++) {
		upsdebugx(LOG_DEBUG, "UPS line%d number %d", i, upsfd->upsLines[i]);
	}

	/* transform lines to indexes for easier state calculation */
	tranformationDelta = upsfd->upsMaxLine - RULES_CMD_LAST + 1;
	for(i = 0; i < upsfd->rulesCount; i++) {
		for(j = 0; j < upsfd->rules[i]->subCount; j++) {
			if(upsfd->rules[i]->cRules[j] >= 0) {
				upsfd->rules[i]->cRules[j] -= tranformationDelta;
			}
		}
	}
	for(k = 0; k < upsfd->upsLinesCount; k++) {
		for(i = 0; i < upsfd->rulesCount; i++) {
			for(j = 0; j < upsfd->rules[i]->subCount; j++) {
				if((upsfd->rules[i]->cRules[j] + tranformationDelta) == upsfd->upsLines[k]) {
					upsfd->rules[i]->cRules[j] = k;
				}
			}
		}
	}

	/* debug printout of transformed lines numbers */
	upsdebugx(LOG_DEBUG, "rules count [%d] translated", upsfd->rulesCount);
	for(i = 0; i < upsfd->rulesCount; i++) {
		upsdebugx(
			LOG_DEBUG,
			"rule state name [%s], subcount %d translated",
			upsfd->rules[i]->stateName,
			upsfd->rules[i]->subCount
		);
		for(j = 0; j < upsfd->rules[i]->subCount; j++) {
			upsdebugx(
				LOG_DEBUG,
				"[%s] substate %d [%d]",
				upsfd->rules[i]->stateName, j,
				upsfd->rules[i]->cRules[j]
			);
		}
	}
}

/*
 * calculate state rule value based on GPIO line values
 */
static int calc_rule_states(int cRules[], int subCount, int sIndex) {
	int ruleVal = 0;
	int iopStart = sIndex;
	int rs;
	if(iopStart < subCount) {
		if(cRules[iopStart] >= 0) {
			ruleVal = gpioupsfd->upsLinesStates[cRules[iopStart]];
		} else {
			iopStart++;
			ruleVal = !gpioupsfd->upsLinesStates[cRules[iopStart]];
		}
		iopStart++;
	}
	if(iopStart < subCount && cRules[iopStart] == RULES_CMD_OR) {
		ruleVal = ruleVal || calc_rule_states(cRules, subCount, iopStart+1);
	} else {
		for(; iopStart < subCount; iopStart++) {
			if(cRules[iopStart] == RULES_CMD_NOT) {
				iopStart++;
				rs = !gpioupsfd->upsLinesStates[cRules[iopStart]];
			} else {
				rs = gpioupsfd->upsLinesStates[cRules[iopStart]];
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
static void update_ups_states(struct gpioups_t *gpioupsfd) {
	int batLow = 0;
	int chargerStatusSet = 0;
	int ruleNo;

	status_init();

	for(ruleNo = 0; ruleNo < gpioupsfd->rulesCount; ruleNo++) {
		gpioupsfd->rules[ruleNo]->currVal =
			calc_rule_states(
				gpioupsfd->rules[ruleNo]->cRules,
				gpioupsfd->rules[ruleNo]->subCount, 0
			);
		if(gpioupsfd->rules[ruleNo]->currVal) {
			status_set(gpioupsfd->rules[ruleNo]->stateName);

			if(!strcmp(gpioupsfd->rules[ruleNo]->stateName, "CHRG")) {
				dstate_setinfo("battery.charger.status", "%s", "charging");
				chargerStatusSet++;
			}
			if(!strcmp(gpioupsfd->rules[ruleNo]->stateName, "DISCHRG")) {
				dstate_setinfo("battery.charger.status", "%s", "discharging");
				chargerStatusSet++;
			}
			if(!strcmp(gpioupsfd->rules[ruleNo]->stateName, "LB")) {
				batLow = 1;
			}
		}
		if(gpioupsfd->aInfoAvailable &&
			gpioupsfd->rules[ruleNo]->archVal != gpioupsfd->rules[ruleNo]->currVal) {
			upslogx(LOG_WARNING, "UPS state [%s] changed to %d",
				gpioupsfd->rules[ruleNo]->stateName,
				gpioupsfd->rules[ruleNo]->currVal
			);
		}
		gpioupsfd->rules[ruleNo]->archVal = gpioupsfd->rules[ruleNo]->currVal;
	}

	if(chargerStatusSet <= 0) {
		dstate_delinfo("battery.charger.status");
	}

	if(dstate_getinfo("battery.charge.low")!=NULL) {
		if(batLow) {
			dstate_setinfo("battery.charge", "%s", dstate_getinfo("battery.charge.low"));
		} else {
			dstate_setinfo("battery.charge", "%s", "100");
		}
	}

	gpioupsfd->aInfoAvailable = 1;

	status_commit();
}

void upsdrv_initinfo(void)
{
	if(testvar("mfr")) {
		dstate_setinfo("device.mfr", "%s", getval("mfr"));
	}
	if(testvar("model")) {
		dstate_setinfo("device.model", "%s", getval("model"));
	}
	if(testvar("description")) {
		dstate_setinfo("device.description", "%s", getval("description"));
	}

	if(!testvar("rules"))	/* rules is required configuration parameter */
		fatalx(EXIT_FAILURE, "UPS status calculation rules not specified");
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
	__attribute__((noreturn));

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_SENSITIVE, "mfr", "UPS manufacturer");
	addvar(VAR_SENSITIVE, "model", "UPS model");
	addvar(VAR_VALUE, "rules", "Line rules to produce status strings");
	addvar(VAR_SENSITIVE, "description", "Device description");
	addvar(VAR_SENSITIVE, "desc", "Device description");
#ifdef LOCALTEST
	test_rules();
#endif
}

void upsdrv_initups(void)
{
	/* prepare rules and allocate related structures */
	gpioupsfd=generic_gpio_open(device_path);
	/* open GPIO chip and check pin consistence */
	if(gpioupsfd) {
		gpio_open(gpioupsfd);
	}
}

void upsdrv_cleanup(void)
{
	/* release gpio library resources	*/
	gpio_close(gpioupsfd);
	/* release related generic resources	*/
	generic_gpio_close(gpioupsfd);
}
