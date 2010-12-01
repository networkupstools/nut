/* external state structures used by things like upsd */

#ifndef EXTSTATE_H_SEEN
#define EXTSTATE_H_SEEN 1

/* this could be made dynamic if someone really needs more than this... */
#define ST_MAX_VALUE_LEN 256

/* state tree flags */
#define ST_FLAG_RW      0x0001
#define ST_FLAG_STRING  0x0002
#define ST_FLAG_IMMUTABLE	0x0004

/* list of possible ENUM values */
typedef struct enum_s {
	char    *val;
	struct enum_s	*next;
} enum_t;

/* list of instant commands */
typedef struct cmdlist_s {
        char    *name;
        struct cmdlist_s	*next;
} cmdlist_t;

#endif	/* EXTSTATE_H_SEEN */
