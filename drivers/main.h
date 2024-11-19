#ifndef NUT_MAIN_H_SEEN
#define NUT_MAIN_H_SEEN 1

#include "common.h"
#include "upsconf.h"
#include "upshandler.h"
#include "dstate.h"
#include "extstate.h"
#ifdef WIN32
#include "wincompat.h"
#endif

/* public functions & variables from main.c, documented in detail there */
extern const char	*progname, *upsname, *device_name;
extern char		*device_path, *device_sdcommands;
extern int		broken_driver, experimental_driver,
			do_lock_port, exit_flag, handling_upsdrv_shutdown;
extern TYPE_FD		upsfd, extrafd;
extern time_t		poll_interval;

/* functions & variables required in each driver */
void upsdrv_initups(void);	/* open connection to UPS, fail if not found */
void upsdrv_initinfo(void);	/* prep data, settings for UPS monitoring */
void upsdrv_updateinfo(void);	/* update state data if possible */
void upsdrv_shutdown(void);	/* make the UPS power off the load */
void upsdrv_help(void);		/* tack on anything useful for the -h text */
void upsdrv_banner(void);	/* print your version information */
void upsdrv_cleanup(void);	/* free any resources before shutdown */

void set_exit_flag(int sig);

/* --- details for the variable/value sharing --- */

/* Try each instant command in the comma-separated list of
 * sdcmds, until the first one that reports it was handled.
 * Returns STAT_INSTCMD_HANDLED if one of those was accepted
 * by the device, or STAT_INSTCMD_INVALID if none succeeded.
 * If cmdused is not NULL, it is populated by the command
 * string which succeeded (or NULL if none), and the caller
 * should free() it eventually.
 */
int do_loop_shutdown_commands(const char *sdcmds, char **cmdused);

/* Use driver-provided sdcmds_default, unless a custom driver parameter value
 * "sdcommands" is set - then use it instead. Call do_loop_shutdown_commands()
 * for actual work; return STAT_INSTCMD_HANDLED or STAT_INSTCMD_HANDLED as
 * applicable; if caller-provided cmdused is not NULL, populate it with the
 * command that was used successfully (if any).
 */
int loop_shutdown_commands(const char *sdcmds_default, char **cmdused);

/*
 * Effectively call loop_shutdown_commands("shutdown.default") (which in turn
 * probably calls some other INSTCMD, but may be using a more custom logic),
 * and report how that went.
 * Depending on run-time circumstances, probably set_exit_flag() too.
 */
int upsdrv_shutdown_sdcommands_or_default(const char *sdcmds_default, char **cmdused);

/* handle instant commands common for all drivers
 * (returns STAT_INSTCMD_* state values per enum in upshandler.h)
 */
int main_instcmd(const char *cmdname, const char *extra, conn_t *conn);

/* handle instant commands common for all drivers - fallback for common
 * command names that could be implemented in a driver but were not */
int main_instcmd_fallback(const char *cmdname, const char *extra, conn_t *conn);

/* handle setting variables common for all drivers
 * (returns STAT_SET_* state values per enum in upshandler.h)
 */
int main_setvar(const char *varname, const char *val, conn_t *conn);

/* main calls this driver function - it needs to call addvar */
void upsdrv_makevartable(void);

/* retrieve the value of variable <var> if possible */
char *getval(const char *var);

/* see if <var> has been defined, even if no value has been given to it */
int testvar(const char *var);

/* extended variable table - used for -x defines/flags */
typedef struct vartab_s {
	int	vartype;	/* VAR_* value, below			 */
	char	*var;		/* left side of =, or whole word if none */
	char	*val;		/* right side of = 			 */
	char	*desc;		/* 40 character description for -h text	 */
	int	found;		/* set once encountered, for testvar()	 */
	int	reloadable;	/* driver reload may redefine this value */
	struct vartab_s	*next;
} vartab_t;

/* flags to define types in the vartab */

#define VAR_FLAG	0x0001	/* argument is a flag (no value needed) */
#define VAR_VALUE	0x0002	/* argument requires a value setting	*/
#define VAR_SENSITIVE	0x0004	/* do not publish in driver.parameter	*/

/* callback from driver - create the table for future -x entries */
void addvar(int vartype, const char *name, const char *desc);
void addvar_reloadable(int vartype, const char *name, const char *desc);

/* Several helpers for driver configuration reloading follow:
 * * testval_reloadable() checks if we are currently reloading (or initially
 *   loading) the configuration, and if strings oldval==newval or not,
 *   e.g. for values saved straight into driver source code variables;
 * * testinfo_reloadable() checks this for a name saved as dstate_setinfo();
 * * testvar_reloadable() checks in vartab_t list as maintained by addvar().
 *
 * All these methods check if value can be (re-)loaded now:
 * * either it is reloadable by argument or vartab_t definition,
 * * or no value has been saved into it yet (e.g. <oldval> is NULL),
 * * or we are handling initial loading and keep legacy behavior of trusting
 *   the inputs (e.g. some values may be defined as defaults in global section
 *   and tuned in a driver section).
 *
 * Return values:
 * * -1 -- if nothing needs to be done and that is not a failure
 *   (e.g. value not modified so we do not care if we may change it or not);
 * * 0 -- if can not modify this value (but it did change in config);
 * * 1 -- if we can and should apply a new (maybe initial) value.
 */
int testvar_reloadable(const char *var, const char *val, int vartype);
int testval_reloadable(const char *var, const char *oldval, const char *newval, int reloadable);
int testinfo_reloadable(const char *var, const char *infoname, const char *newval, int reloadable);

/* subdriver description structure */
typedef struct upsdrv_info_s {
	const char	*name;		/* driver full name, for banner printing, ... */
	const char	*version;	/* driver version */
	const char	*authors;	/* authors name */
	const int	status;		/* driver development status */
	struct upsdrv_info_s	*subdrv_info[2];	/* sub driver information */
} upsdrv_info_t;

/* flags to define the driver development status */
#define DRV_BROKEN		0x0001	/* dito... */
#define DRV_EXPERIMENTAL	0x0002	/* dito... */
#define DRV_BETA		0x0004	/* more stable and complete, but still
					 * not suitable for production systems
					 */
#define DRV_STABLE		0x0008	/* suitable for production systems, but
					 * not 100 % feature complete */
#define DRV_COMPLETE		0x0010	/* gold level: implies 100 % of the
					 * protocol implemented and the full QA
					 * pass */
/* FIXME: complete with mfr support, and other interesting info */

/* public driver information from the driver file */
extern upsdrv_info_t	upsdrv_info;

/* functions and data possibly used via libdummy_mockdrv.la for unit-tests */
#ifdef DRIVERS_MAIN_WITHOUT_MAIN
extern vartab_t *vartab_h;
void dparam_setinfo(const char *var, const char *val);
void storeval(const char *var, char *val);
void vartab_free(void);
void setup_signals(void);
#endif /* DRIVERS_MAIN_WITHOUT_MAIN */

#ifndef WIN32
# define SIGCMD_RELOAD                  SIGHUP
/* not a signal, so negative; relies on socket protocol */
# define SIGCMD_EXIT                    -SIGTERM
# define SIGCMD_RELOAD_OR_ERROR         -SIGCMD_RELOAD
# define SIGCMD_RELOAD_OR_EXIT          SIGUSR1
/* // FIXME: Implement this self-recycling in drivers (keeping the PID):
# define SIGCMD_RELOAD_OR_RESTART       SIGUSR2
*/

/* This is commonly defined on systems we know; file bugs/PRs for
 * relevant systems where it is not present (SIGWINCH might be an
 * option there, though terminal resizes might cause braindumps).
 * Their packaging may want to add a patch for this bit (and docs).
 */
# if (defined SIGURG)
#  define SIGCMD_DATA_DUMP              SIGURG
# else
#  if (defined SIGWINCH)
#   define SIGCMD_DATA_DUMP             SIGWINCH
#  else
#   pragma warn "This OS lacks SIGURG and SIGWINCH, will not handle SIGCMD_DATA_DUMP"
#  endif
# endif
#else
/* FIXME: handle WIN32 builds for other signals too */
# define SIGCMD_EXIT                    "driver.exit"
# define SIGCMD_RELOAD_OR_ERROR         "driver.reload-or-error"
#endif	/* WIN32 */

#endif /* NUT_MAIN_H_SEEN */
