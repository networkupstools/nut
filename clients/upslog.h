/* upslog.h - table of functions for handling various logging functions */

/* function list */
struct flist_t {
	void	(*fptr)(const char *arg);
	const	char	*arg;
	struct	flist_t	*next;
};

static void do_host(const char *arg);
static void do_upshost(const char *arg);
static void do_pid(const char *arg);
static void do_time(const char *arg);
static void do_var(const char *arg);
static void do_etime(const char *arg);

struct {
	const	char	*name;
	void	(*func)(const char *arg);
}	logcmds[] =
{
	{ "HOST",	do_host			},
	{ "UPSHOST",	do_upshost		},
	{ "PID",	do_pid			},
	{ "TIME",	do_time			},
	{ "VAR",	do_var			},
	{ "ETIME",	do_etime		},
	{ NULL,		(void(*)())(NULL)	}
};
