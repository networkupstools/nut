#ifndef GENERIC_GPIO_COMMON_H
#define GENERIC_GPIO_COMMON_H

#include <stdlib.h>
#include <gpiod.h>
#include <regex.h>
#include <ctype.h>

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
    void    *lib_data;  /* pointer to driver's gpio support library data structure */
    const char    *chipName;  /* port or file name to reference GPIO chip */
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

void gpio_open(struct gpioups_t *gpioupsfd);
void gpio_get_lines_states(struct gpioups_t *gpioupsfd);
void gpio_close(struct gpioups_t *gpioupsfd);

#endif	/* GENERIC_GPIO_COMMON_H */
