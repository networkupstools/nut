/* external state structures used by things like upsd */

#ifndef NUT_EXTSTATE_H_SEEN
#define NUT_EXTSTATE_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* this could be made dynamic if someone really needs more than this... */
#define ST_MAX_VALUE_LEN 256

/* state tree flags */
#define ST_FLAG_NONE      0x0000
#define ST_FLAG_RW        0x0001
#define ST_FLAG_STRING    0x0002 /* not STRING implies NUMBER */
#define ST_FLAG_NUMBER    0x0004
#define ST_FLAG_IMMUTABLE 0x0008

/* list of possible ENUM values */
typedef struct enum_s {
	char	*val;
	struct enum_s	*next;
} enum_t;

/* RANGE boundaries */
typedef struct range_s {
	int min;
	int max;
	struct range_s	*next;
} range_t;

/* list of instant commands */
typedef struct cmdlist_s {
	char	*name;
	struct cmdlist_s	*next;
} cmdlist_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_EXTSTATE_H_SEEN */
