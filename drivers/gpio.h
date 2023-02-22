#ifndef GPIO_H
#define GPIO_H

#include <stdlib.h>
#include <gpiod.h>
#include <regex.h>
#include <ctype.h>

#define DRIVER_NAME	"GPIO UPS driver"
#define DRIVER_VERSION	"1.00"

/*  rules commands definition    */
#define RULES_CMD_NOT   -2
#define RULES_CMD_AND   -3
#define RULES_CMD_OR    -4
#define RULES_CMD_OBR   -5
#define RULES_CMD_CBR   -6
#define RULES_CMD_LAST  RULES_CMD_CBR

/*  run option definitions      */
#define ROPT_REQRES 0x00000001  /* reserve GPIO lines only during request processing    */
#define ROPT_EVMODE 0x00000002  /* event driven run                                     */

typedef struct rulesint_t { /* structure to store processed rules configuration per each state */
    char    stateName[12];  /* NUT state name for rules in cRules */
    int     archVal;        /* previous state value */
    int     currVal;        /* current state value  */
    int     subCount;       /* element count in translated rules subitem */
    int     cRules[];       /* translated rules subitem - rules commands followed by line number(s) */
}   rulesint;

typedef struct gpioups_t {
    struct gpiod_chip *gpioChipHandle;      /* libgpiod chip handle when opened */
    struct gpiod_line_bulk gpioLines;       /* libgpiod lines to monitor */
    struct gpiod_line_bulk gpioEventLines;  /* libgpiod lines for event monitoring */
    int     initial;    /* initialization flag - 0 on 1st entry */
    int     runOptions; /* run options, not yet used */
    int     aInfoAvailable; /* non-zero if previous state information is available */
    int     chipLinesCount; /* gpio chip lines count, set after sucessful open */
    int     upsLinesCount;  /* no of lines used in rules */
    int     *upsLines;      /* lines numbers */
    int     *upsLinesStates;    /* lines states */
    int     upsMaxLine;     /* maximum line number referenced in rules */
    int     rulesCount;     /* rules subitem count: no of NUT states defined in rules*/
    struct rulesint_t **rules;
} gpioups;

#endif	/* GPIO_H */
