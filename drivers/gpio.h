#ifndef GPIO_H
#define GPIO_H

#include <stdlib.h>
#include <gpiod.h>
#include <regex.h>
#include <ctype.h>

#define DRIVER_NAME	"GPIO UPS driver"
#define DRIVER_VERSION	"0.09"

/*  rules command definition    */
#define RULES_CMD_NOT   -2
#define RULES_CMD_AND   -3
#define RULES_CMD_OR    -4
#define RULES_CMD_OBR   -5
#define RULES_CMD_CBR   -6
#define RULES_CMD_LAST  RULES_CMD_CBR

/*  run option definitions      */
#define ROPT_REQRES 0x00000001  /* reserve GPIO lines only during request processing    */
#define ROPT_EVMODE 0x00000002  /* event driven run                                     */

typedef struct rulesint_t {
    char    stateName[12];
    int     archVal;
    int     currVal;
    int     subCount;
    int     cRules[];
}   rulesint;

typedef struct gpioups_t {
    struct gpiod_chip *gpioChipHandle;
    struct gpiod_line_bulk gpioLines;
    struct gpiod_line_bulk gpioEventLines;
    int     initial;
    int     runOptions;
    int     aInfoAvailable;
    int     chipLinesCount;
    int     upsLinesCount;
    int     *upsLines;
    int     *upsLinesStates;
    int     upsMaxLine;
    int     rulesCount;
    struct rulesint_t **rules;
} gpioups;

#endif	/* GPIO_H */