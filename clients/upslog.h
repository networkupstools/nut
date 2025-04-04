/* upslog.h - table of functions for handling various logging functions */

#ifndef NUT_UPSLOG_H_SEEN
#define NUT_UPSLOG_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* log targets, defined independently of monitored "systems" below, because
 * several devices can get logged into the same target (stdout or filename) */
struct 	logtarget_t {
	char	*logfn;
	FILE	*logfile;
	struct 	logtarget_t	*next;
};

/* monitored "systems" */
struct 	monhost_ups_t {
	char	*monhost;
	char	*upsname;
	char	*hostname;
	uint16_t	port;
	UPSCONN_t	*ups;
	struct 	logtarget_t	*logtarget;
	struct	monhost_ups_t	*next;
};

/* function list */
typedef struct flist_s {
	void	(*fptr)(const char *arg, const struct monhost_ups_t *monhost_ups_print);
	const	char	*arg;
	struct flist_s	*next;
} flist_t;

static void do_host(const char *arg, const struct monhost_ups_t *monhost_ups_print);
static void do_upshost(const char *arg, const struct monhost_ups_t *monhost_ups_print);
static void do_pid(const char *arg, const struct monhost_ups_t *monhost_ups_print);
static void do_time(const char *arg, const struct monhost_ups_t *monhost_ups_print);
static void do_var(const char *arg, const struct monhost_ups_t *monhost_ups_print);
static void do_etime(const char *arg, const struct monhost_ups_t *monhost_ups_print);

/* This is only used in upslog.c, but refers to routines declared here...
 * To move or not to move?..
 */
static struct {
	const	char	*name;
	void	(*func)(const char *arg, const struct monhost_ups_t *monhost_ups_print);
}	logcmds[] =
{
	{ "HOST",	do_host			},
	{ "UPSHOST",	do_upshost		},
	{ "PID",	do_pid			},
	{ "TIME",	do_time			},
	{ "VAR",	do_var			},
	{ "ETIME",	do_etime		},
	{ NULL,		(void(*)(const char*, const struct monhost_ups_t *))(NULL)	}
};

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSLOG_H_SEEN */
