/*
	anything commented is optional
	anything else is mandatory

	for more information, refer to:
	* docs/developers.txt
	* docs/new-drivers.txt
	* docs/new-names.txt

	and possibly also to:
	* docs/hid-subdrivers.txt for USB/HID devices
	* or docs/snmp-subdrivers.txt for SNMP devices
*/
/* ./configure --with-pidpath=/run/nut --with-altpidpath=/run/nut --with-statepath=/run/nut --sysconfdir=/etc/nut --with-gpio --with-user=nut --with-group=nut	*/
#pragma GCC optimize("O0")

#include "config.h"
#include "main.h"
#include "attribute.h"
#include "gpio.h"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"ModrisB <modrisb@apollo.lv>",
	DRV_STABLE,
	{ NULL }
};

/*	CyberPower 12V open collector state definitions
	0 ON BATTERY			Low when operating from utility line
							Open when operating from battery
	1 REPLACE BATTERY		Low when battery is charged
							Open when battery fails the Self Test
	6 BATTERY MISSING		Low when battery is present
							Open when battery is missing
	3 LOW BATTERY			Low when battery is near full charge capacity
							Open when operating from a battery with < 20% capacity

	NUT supported states
	OL      On line (mains is present)
	OB      On battery (mains is not present)
	LB      Low battery
	HB      High battery
	RB      The battery needs to be replaced
	CHRG    The battery is charging
	DISCHRG The battery is discharging (inverter is providing load power)
	BYPASS  UPS bypass circuit is active -- no battery protection is available
	CAL     UPS is currently performing runtime calibration (on battery)
	OFF     UPS is offline and is not supplying power to the load
	OVER    UPS is overloaded
	TRIM    UPS is trimming incoming voltage (called "buck" in some hardware)
	BOOST   UPS is boosting incoming voltage
	FSD     Forced Shutdown (restricted use, see the note below)

	CyberPower rules setting
	OL=^0;OB=0;LB=3;HB=^3;RB=1;DISCHRG=0&^3;BYPASS=6;
*/

struct gpioups_t *gpioupsfd=(struct gpioups_t *)NULL;

static struct gpioups_t *gpio_open(const char *chipName);
static void reserve_lines_libgpiod(struct gpioups_t *gpioupsfd, int inner);
static void gpio_close(struct gpioups_t *upsfd);
static void gpio_get_ups_rules(struct gpioups_t *upsfd);
static void gpio_get_lines_states_libgpiod(struct gpioups_t *gpioupsfd);
static void gpio_set_states(struct gpioups_t *gpioupsfd);

#define LOCALTEST
#ifdef LOCALTEST
struct gpiod_chip *gpiod_chip_open_by_name(const char *name) {
	NUT_UNUSED_VARIABLE(name);
	return (struct gpiod_chip *)1;
}

unsigned int gpiod_chip_num_lines(struct gpiod_chip *chip) {
	NUT_UNUSED_VARIABLE(chip);
	return 32;
}
int gpiod_chip_get_lines(struct gpiod_chip *chip,
			 unsigned int *offsets, unsigned int num_offsets,
			 struct gpiod_line_bulk *bulk) {
	return 0;
}

int gpiod_line_request_bulk(struct gpiod_line_bulk *bulk,
			    const struct gpiod_line_request_config *config,
			    const int *default_vals)
{
	return 0;
}

int gStatus = 0;

int gpiod_line_get_value_bulk(struct gpiod_line_bulk *bulk,
			      int *values)
{
	int pinPos=1;
	if(gpioupsfd)
	for(int i=0; i<gpioupsfd->upsLinesCount; i++) {
		values[i]=(gStatus&pinPos)!=0;
		pinPos=pinPos<<1;
	}

	gStatus++;
	return 0;
}

int gpiod_line_event_wait_bulk(struct gpiod_line_bulk *bulk,
			       const struct timespec *timeout,
			       struct gpiod_line_bulk *event_bulk)
{
	switch(gStatus%3) {
		case 0:
			sleep(2);
		break;
		case 1:
			sleep(4);
		break;
		case 2:
			sleep(6);
		break;
	}
	return 0;
}
#endif

/* reserve GPIO lines as per run options and inner parameter: do reservation once
   or per each status read                                                       */
static void reserve_lines_libgpiod(struct gpioups_t *gpioupsfd, int inner) {
	struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfd->lib_data);
	upsdebugx(LOG_DEBUG, "reserve_lines_libgpiod runOptions %d, inner %d", gpioupsfd->runOptions, inner);
	if(((gpioupsfd->runOptions&ROPT_REQRES)!=0)==inner) {
		struct gpiod_line_request_config config;
		int gpioRc;
		config.consumer=upsdrv_info.name;
		if(gpioupsfd->runOptions&ROPT_EVMODE) {
			config.request_type=GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
			upsdebugx(LOG_DEBUG, "reserve_lines_libgpiod GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES");
		} else {
			config.request_type=GPIOD_LINE_REQUEST_DIRECTION_INPUT;
			upsdebugx(LOG_DEBUG, "reserve_lines_libgpiod GPIOD_LINE_REQUEST_DIRECTION_INPUT");
		}
		config.flags=0;	/*	GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN;	*/
		gpioRc=gpiod_line_request_bulk(&libgpiod_data->gpioLines, &config, NULL);
		if(gpioRc)
			fatal_with_errno(
				LOG_ERR,
				"GPIO gpiod_line_request_bulk call failed, check for other applications that may have reserved GPIO lines"
			);
		upsdebugx(
			LOG_DEBUG,
			"GPIO gpiod_line_request_bulk with type %d return code %d",
			config.request_type,
			gpioRc
		);
	}
}

/* open gpiochip, process rules and check lines numbers validity */
static void gpio_open_libgpiod(struct gpioups_t *gpioupsfd) {
	struct libgpiod_data_t *libgpiod_data = xcalloc(sizeof(struct libgpiod_data_t),1);
	gpioupsfd->lib_data=libgpiod_data;
	libgpiod_data->gpioChipHandle=gpiod_chip_open_by_name(gpioupsfd->chipName);
	if(!libgpiod_data->gpioChipHandle) {
		fatal_with_errno(
			LOG_ERR,
			"Could not open GPIO chip [%s], check chips presence and/or access rights",
			gpioupsfd->chipName
		);
	} else {
		int gpioRc;
		upslogx(LOG_NOTICE, "GPIO chip [%s] opened", gpioupsfd->chipName);
		gpioupsfd->chipLinesCount=gpiod_chip_num_lines(libgpiod_data->gpioChipHandle);
		upslogx(LOG_NOTICE, "Find %d lines on GPIO chip [%s]", gpioupsfd->chipLinesCount, gpioupsfd->chipName);
		if(gpioupsfd->chipLinesCount<gpioupsfd->upsMaxLine)
			fatalx(
				LOG_ERR,
				"GPIO chip lines count %d smaller than UPS line number used (%d)",
				gpioupsfd->chipLinesCount,
				gpioupsfd->upsMaxLine
			);
		gpioupsfd->upsLinesStates=xcalloc(sizeof(int), gpioupsfd->upsLinesCount);
		gpiod_line_bulk_init(&libgpiod_data->gpioLines);
		gpiod_line_bulk_init(&libgpiod_data->gpioEventLines);
		gpioRc=gpiod_chip_get_lines(
			libgpiod_data->gpioChipHandle,
			(unsigned int *)gpioupsfd->upsLines,
			gpioupsfd->upsLinesCount,
			&libgpiod_data->gpioLines
		);
		if(gpioRc)
			fatal_with_errno(
				LOG_ERR,
				"GPIO line reservation (gpiod_chip_get_lines call) failed with code %d, check for possible issue in rules parameter",
				gpioRc
			);
		upsdebugx(LOG_DEBUG, "GPIO gpiod_chip_get_lines return code %d", gpioRc);
		reserve_lines_libgpiod(gpioupsfd, 0);
	}
}

/* open gpiochip, process rules and check lines numbers validity */
static struct gpioups_t *gpio_open(const char *chipName) {
	struct gpioups_t *upsfdlocal=xcalloc(sizeof(*upsfdlocal),1);
	upsfdlocal->runOptions=0; /*	don't use ROPT_REQRES and ROPT_EVMODE yet	*/
	upsfdlocal->chipName=chipName;
	gpio_get_ups_rules(upsfdlocal);
	return upsfdlocal;
}

static void gpio_close_libgpiod(struct gpioups_t *gpioupsfd) {
	if(gpioupsfd) {
		struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfd->lib_data);
		if(libgpiod_data) {
			if(libgpiod_data->gpioChipHandle) {
				gpiod_chip_close(libgpiod_data->gpioChipHandle);
			}
			free(gpioupsfd->lib_data);
			gpioupsfd->lib_data = NULL;
		}
	}
}

/* close gpiochip and release any allocated resources */
static void gpio_close(struct gpioups_t *gpioupsfd) {
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

/* add compiled subrules item to the array */
static void add_rule_item(struct gpioups_t *upsfd, int newValue) {
	int     subCount=(upsfd->rules[upsfd->rulesCount-1]) ? upsfd->rules[upsfd->rulesCount-1]->subCount+1 : 1;
	int     itemSize=subCount*sizeof(upsfd->rules[0]->cRules[0])+sizeof(rulesint);
	upsfd->rules[upsfd->rulesCount-1]=xrealloc(upsfd->rules[upsfd->rulesCount-1], itemSize);
	upsfd->rules[upsfd->rulesCount-1]->subCount=subCount;
	upsfd->rules[upsfd->rulesCount-1]->cRules[subCount-1]=newValue;
}

/* get next lexem out of rules configuration string recognizing separators = and ; ,
logical commands ^ , & , | , state names - several ascii characters matching NUT states,
and several numbers to denote GPIO chip lines to read statuses   */
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
	unsigned char lexTypeChr=lexType[rulesBuff[*startPos]];
	*endPos=(*startPos)+1;
	if(lexTypeChr=='a' || lexTypeChr=='0') {
		for(; lexType[rulesBuff[*endPos]]==lexTypeChr; (*endPos)++);
	}
	return (int)lexTypeChr;
}

/* split subrules and translate them to array of commands/line numbers */
static void gpio_get_ups_rules(struct gpioups_t *upsfd) {
	unsigned char   *rulesString=(unsigned char *)getval("rules");
	/*	statename=[^]line[&||[line]]	*/
	char    lexBuff[33];
	int     startPos=0, endPos;
	int     lexType;
	int lexStatus=0;
	int	i, j, k;
	int	tranformationDelta;
	upsdebugx(LOG_DEBUG, "rules =[%s]", rulesString);
	while((lexType=get_rule_lex(rulesString, &startPos, &endPos))>0 && lexStatus>=0) {
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
				if(lexType!='a') {
					lexStatus=-1;
				} else {
					lexStatus=1;
					upsfd->rulesCount++;
					upsfd->rules=xrealloc(upsfd->rules, (size_t)(sizeof(upsfd->rules[0])*upsfd->rulesCount));
					upsfd->rules[upsfd->rulesCount-1]=xcalloc(sizeof(rulesint), 1);
					strncpy(upsfd->rules[upsfd->rulesCount-1]->stateName, (char *)(rulesString+startPos), endPos-startPos);
					upsfd->rules[upsfd->rulesCount-1]->stateName[endPos-startPos]=0;
				}
			break;
			case 1:
				if(lexType!='=') {
					lexStatus=-1;
				} else {
					lexStatus=2;
				}
			break;
			case 2:
				if(lexType=='^') {
					lexStatus=3;
					add_rule_item(upsfd, RULES_CMD_NOT);
				} else if(lexType=='0') {
					lexStatus=4;
					add_rule_item(upsfd, atoi((char *)(rulesString+startPos)));
				} else {
					lexStatus=-1;
				}
			break;
			case 3:
				if(lexType!='0') {
					lexStatus=-1;
				} else {
					lexStatus=4;
					add_rule_item(upsfd, atoi((char *)(rulesString+startPos)));
				}
			break;
			case 4:
				if(lexType=='&') {
					lexStatus=2;
					add_rule_item(upsfd, RULES_CMD_AND);
				} else if(lexType=='|') {
					lexStatus=2;
					add_rule_item(upsfd, RULES_CMD_OR);
				}
				else if(lexType==';') {
					lexStatus=0;
				} else {
					lexStatus=-1;
				}
			break;
			default:
				lexStatus=-1;
			break;
		}
		if(lexStatus==-1)
			fatalx(LOG_ERR, "Line processing rule error at position %d", startPos);
		startPos=endPos;
	}

	upsdebugx(LOG_DEBUG, "rules count [%d]", upsfd->rulesCount);
	for(i=0; i<upsfd->rulesCount; i++) {
		upsdebugx(
			LOG_DEBUG,
			"rule state name [%s], subcount %d",
			upsfd->rules[i]->stateName,
			upsfd->rules[i]->subCount
		);
		for(j=0; j<upsfd->rules[i]->subCount; j++) {
			upsdebugx(
				LOG_DEBUG,
				"[%s] substate %d [%d]",
				upsfd->rules[i]->stateName,
				j,
				upsfd->rules[i]->cRules[j]
			);
		}
	}

	upsfd->upsLinesCount=0;
	upsfd->upsMaxLine=0;
	for(i=0; i<upsfd->rulesCount; i++) {
		for(j=0; j<upsfd->rules[i]->subCount; j++) {
			int pinOnList=0;
			for(k=0; k<upsfd->upsLinesCount && !pinOnList; k++) {
				if(upsfd->upsLines[k]==upsfd->rules[i]->cRules[j]) {
					pinOnList=1;
				}
			}
			if(!pinOnList) {
				if(upsfd->rules[i]->cRules[j]>=0) {
					upsfd->upsLinesCount++;
					upsfd->upsLines=xrealloc(upsfd->upsLines, sizeof(upsfd->upsLines[0])*upsfd->upsLinesCount);
					upsfd->upsLines[upsfd->upsLinesCount-1]=upsfd->rules[i]->cRules[j];
					if(upsfd->upsLines[upsfd->upsLinesCount-1]>upsfd->upsMaxLine) {
						upsfd->upsMaxLine=upsfd->upsLines[upsfd->upsLinesCount-1];
					}
				}
			}
		}
	}

	upsdebugx(LOG_DEBUG, "UPS line count = %d", upsfd->upsLinesCount);
	for(i=0; i<upsfd->upsLinesCount; i++) {
		upsdebugx(LOG_DEBUG, "UPS line%d number %d", i, upsfd->upsLines[i]);
	}

	tranformationDelta=upsfd->upsMaxLine-RULES_CMD_LAST+1;
	for(i=0; i<upsfd->rulesCount; i++) {
		for(j=0; j<upsfd->rules[i]->subCount; j++) {
			if(upsfd->rules[i]->cRules[j]>=0) {
				upsfd->rules[i]->cRules[j]-=tranformationDelta;
			}
		}
	}
	for(k=0; k<upsfd->upsLinesCount; k++) {
		for(i=0; i<upsfd->rulesCount; i++) {
			for(j=0; j<upsfd->rules[i]->subCount; j++) {
				if((upsfd->rules[i]->cRules[j]+tranformationDelta)==upsfd->upsLines[k]) {
					upsfd->rules[i]->cRules[j]=k;
				}
			}
		}
	}
	upsdebugx(LOG_DEBUG, "rules count [%d] translated", upsfd->rulesCount);
	for(i=0; i<upsfd->rulesCount; i++) {
		upsdebugx(
			LOG_DEBUG,
			"rule state name [%s], subcount %d translated",
			upsfd->rules[i]->stateName,
			upsfd->rules[i]->subCount
		);
		for(j=0; j<upsfd->rules[i]->subCount; j++) {
			upsdebugx(
				LOG_DEBUG,
				"[%s] substate %d [%d]",
				upsfd->rules[i]->stateName, j,
				upsfd->rules[i]->cRules[j]
			);
		}
	}
}

/* get GPIO line states for all needed lines */
static void gpio_get_lines_states_libgpiod(struct gpioups_t *gpioupsfd) {
	int i;
	int gpioRc;
	struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfd->lib_data);
	reserve_lines_libgpiod(gpioupsfd, 1);
	if(gpioupsfd->runOptions&ROPT_EVMODE) {
		struct timespec timeoutLong={1,0};
		struct gpiod_line_event event;
		int monRes;
		upsdebugx(
			LOG_DEBUG,
			"gpio_get_lines_states_libgpiod initial %d, timeout %ld",
			gpioupsfd->initial,
			timeoutLong.tv_sec
		);
		if(gpioupsfd->initial) {
			timeoutLong.tv_sec=35;
		} else {
			gpioupsfd->initial=1;
		}
		upsdebugx(
			LOG_DEBUG,
			"gpio_get_lines_states_libgpiod initial %d, timeout %ld",
			gpioupsfd->initial,
			timeoutLong.tv_sec
		);
		gpiod_line_bulk_init(&libgpiod_data->gpioEventLines);
		monRes=gpiod_line_event_wait_bulk(
			&libgpiod_data->gpioLines,
			&timeoutLong,
			&libgpiod_data->gpioEventLines
		);
		upsdebugx(
			LOG_DEBUG,
			"gpiod_line_event_wait_bulk completed with %d return code and timeout %ld s",
			monRes,
			timeoutLong.tv_sec
		);
		if(monRes==1) {
			int num_lines=(int)gpiod_line_bulk_num_lines(&libgpiod_data->gpioEventLines);
			int i;
			for(i=0; i<num_lines; i++) {
				struct gpiod_line *eLine=gpiod_line_bulk_get_line(
					&libgpiod_data->gpioEventLines,
					i
				);
				int eventRc=gpiod_line_event_read(eLine, &event);
				unsigned int lineOffset=gpiod_line_offset(eLine);
				event.event_type=0;
				upsdebugx(
					LOG_DEBUG,
					"Event read return code %d and event type %d for line %d",
					eventRc,
					event.event_type,
					lineOffset
				);
			}
		}
	}
	for(i=0; i<gpioupsfd->upsLinesCount; i++) {
		gpioupsfd->upsLinesStates[i]=-1;
	}
	gpioRc=gpiod_line_get_value_bulk(
		&libgpiod_data->gpioLines,
		gpioupsfd->upsLinesStates
	);
	if (gpioRc<0)
		fatal_with_errno(LOG_ERR, "GPIO line status read call failed");
	upsdebugx(
		LOG_DEBUG,
		"GPIO gpiod_line_get_value_bulk completed with %d return code, status values:",
		gpioRc
	);
	for(i=0; i<gpioupsfd->upsLinesCount; i++) {
		upsdebugx(
			LOG_DEBUG,
			"Line%d state = %d",
			i,
			gpioupsfd->upsLinesStates[i]
		);
	}
	if(gpioupsfd->runOptions&ROPT_REQRES) {
		gpiod_line_release_bulk(&libgpiod_data->gpioLines);
	}
}

/* calculate state rule value based on GPIO line values */
static int gpio_calc_rule_states(int cRules[], int subCount, int sIndex) {
	int ruleVal=0;
	int iopStart=sIndex;
	int rs;
	if(iopStart<subCount) {
		if(cRules[iopStart]>=0) {
			ruleVal=gpioupsfd->upsLinesStates[cRules[iopStart]];
		} else {
			iopStart++;
			ruleVal=!gpioupsfd->upsLinesStates[cRules[iopStart]];
		}
		iopStart++;
	}
	if(iopStart<subCount && cRules[iopStart]==RULES_CMD_OR) {
		ruleVal=ruleVal || gpio_calc_rule_states(cRules, subCount, iopStart+1);
	} else {
		for(; iopStart<subCount; iopStart++) {
			if(cRules[iopStart]==RULES_CMD_NOT) {
				iopStart++;
				rs=!gpioupsfd->upsLinesStates[cRules[iopStart]];
			} else {
				rs=gpioupsfd->upsLinesStates[cRules[iopStart]];
			}
			ruleVal= ruleVal && rs;
		}
	}

	return ruleVal;
}

/* 	set ups state according to rules, do adjustments for CHRG/DISCHRG
	and battery charge states										*/
static void gpio_set_states(struct gpioups_t *gpioupsfd) {
	int batLow=0;
	int chargerStatusSet=0;
	int ruleNo;

	status_init();

	for(ruleNo=0; ruleNo<gpioupsfd->rulesCount; ruleNo++) {
		gpioupsfd->rules[ruleNo]->currVal=
			gpio_calc_rule_states(
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
				batLow=1;
			}
		}
		if(gpioupsfd->aInfoAvailable &&
			gpioupsfd->rules[ruleNo]->archVal!=gpioupsfd->rules[ruleNo]->currVal) {
			upslogx(LOG_WARNING, "UPS state [%s] changed to %d",
				gpioupsfd->rules[ruleNo]->stateName,
				gpioupsfd->rules[ruleNo]->currVal
			);
		}
		gpioupsfd->rules[ruleNo]->archVal=gpioupsfd->rules[ruleNo]->currVal;
	}

	if(chargerStatusSet<=0) {
		dstate_delinfo("battery.charger.status");
	}

	if(dstate_getinfo("battery.charge.low")!=NULL) {
		if(batLow) {
			dstate_setinfo("battery.charge", "%s", dstate_getinfo("battery.charge.low"));
		} else {
			dstate_setinfo("battery.charge", "%s", "100");
		}
	}

	gpioupsfd->aInfoAvailable=1;

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
	gpio_get_lines_states_libgpiod(gpioupsfd);

	/*  calculate/set UPS states based on line values	*/
	gpio_set_states(gpioupsfd);

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
}

void upsdrv_initups(void)
{
	/* prepare rules and allocate related structures */
	gpioupsfd=gpio_open(device_path);
	/* open GPIO chip and check pin consistence */
	if(gpioupsfd) {
		gpio_open_libgpiod(gpioupsfd);
	}
}

void upsdrv_cleanup(void)
{
	/* release gpio library resources	*/
	gpio_close_libgpiod(gpioupsfd);
	/* release related generic resources	*/
	gpio_close(gpioupsfd);
}