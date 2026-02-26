#ifndef NUT_MAIN_H_SEEN
#define NUT_MAIN_H_SEEN 1

#include "common.h"
#include "upsconf.h"
#include "upshandler.h"
#include "dstate.h"
#include "extstate.h"
#ifdef WIN32
#include "wincompat.h"
#endif	/* WIN32 */

/* public functions & variables from main.c, documented in detail there */
extern const char	*upsname, *device_name;
extern char		*device_path, *device_sdcommands;
extern int		broken_driver, experimental_driver,
			do_lock_port, exit_flag, handling_upsdrv_shutdown;
extern TYPE_FD		upsfd, extrafd;
extern time_t		poll_interval;

/* We allow for aliases to certain program names (e.g. when renaming a driver
 * between "old" and "new" and default implementations, it should accept both
 * or more names it can be called by).
 * The [0] entry is used to set up stuff like pipe names, man page links, etc.
 */
#define	MAX_PROGNAMES	4
extern const char	*prognames[MAX_PROGNAMES];
extern char	prognames_should_free[MAX_PROGNAMES];
#define	progname	(prognames[0])

int drv_main(int argc, char **argv);

/* functions & variables required in each driver
 * See also: register_upsdrv_callbacks()
 */
void upsdrv_tweak_prognames(void);	/* optionally add aliases and/or set preferred name into [0] (for pipe name etc.); called just after populating prognames[0] and prognames_should_free[] entries */
void upsdrv_initups(void);	/* open connection to UPS, fail if not found */
void upsdrv_initinfo(void);	/* prep data, settings for UPS monitoring */
void upsdrv_updateinfo(void);	/* update state data if possible */
void upsdrv_shutdown(void);	/* make the UPS power off the load */
void upsdrv_help(void);		/* tack on anything useful for the -h text */
void upsdrv_cleanup(void);	/* free any resources before shutdown */
void upsdrv_makevartable(void);	/* main calls this driver function - it needs to call addvar */

void set_exit_flag(int sig);

void upsdrv_banner(void);	/* print your version information - shared in main.c */

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
#define	MAX_SDCOMMANDS_DEPTH	15

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

/* handle setting variables common for all drivers
 * (returns STAT_SET_* state values per enum in upshandler.h)
 */
int main_setvar(const char *varname, const char *val, conn_t *conn);

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

/* public driver information from the driver file
 * See also: register_upsdrv_callbacks()
 */
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
#else	/* WIN32 */
/* FIXME NUT_WIN32_INCOMPLETE : handle WIN32 builds for other signals too */
# define SIGCMD_EXIT                    "driver.exit"
# define SIGCMD_RELOAD_OR_ERROR         "driver.reload-or-error"	/* NUT_WIN32_INCOMPLETE */
#endif	/* WIN32 */

/* allow main.c code in both static and shared driver builds
 * to see implementations defined by specific driver sources,
 * called by main-stub.c where used:
 */
#define UPSDRV_CALLBACK_MAGIC "NUT_UPSDrv_CB"

/* An array in the end pads this structure to allow for some more entries
 * (if we ever think of any) to be communicated later without some easily
 * triggered overflow during binary-to-library calls, years down the road.
 */
#define UPSDRV_CALLBACK_PADDING 14
typedef struct upsdrv_callback_s {
	/* A few entries for sanity check, in case different
	 * generations of NUT drivers try to link with the
	 * main() logic shipped as a shared library aimed
	 * at its particular release */
	uint64_t	struct_version;	/* how to interpret what we see */
	uint64_t	ptr_size;	/* be sure about bitness */
	uint64_t	ptr_count;	/* amount of non-null pointers passed here */
	char		struct_magic[16];
	/* char		NUT_VERSION[32];	/ * Just have it built into each binary */

	/* Do not change the order of these entries,
	 * only add at the end of list (if needed).
	 * All except sentinel must be not-NULL. */
	/* 01 */	upsdrv_info_t*	upsdrv_info;	/* public driver information from the driver file */

	/* 02 */	void (*upsdrv_tweak_prognames)(void);	/* optionally add aliases and/or set preferred name into [0] (for pipe name etc.); called just after populating prognames[0] and prognames_should_free[] entries */
	/* 03 */	void (*upsdrv_initups)(void);	/* open connection to UPS, fail if not found */
	/* 04 */	void (*upsdrv_initinfo)(void);	/* prep data, settings for UPS monitoring */
	/* 05 */	void (*upsdrv_updateinfo)(void);	/* update state data if possible */
	/* 06 */	void (*upsdrv_shutdown)(void);	/* make the UPS power off the load */
	/* 07 */	void (*upsdrv_help)(void);		/* tack on anything useful for the -h text */
	/* 08 */	void (*upsdrv_cleanup)(void);	/* free any resources before shutdown */
	/* 09 */	void (*upsdrv_makevartable)(void);	/* main calls this driver function - it needs to call addvar */

	/* A few values from common_nut-version.c which we link into
	 * the binaries to know *their* relevant NUT build, not the
	 * library's, so the libnutprivate-*drivers* shared object
	 * and its use of libdummy_main.la gets confused with direct
	 * links to those symbols on some platforms. Not truly upsdrv
	 * stuff, but similar in problem and solution, so here goes:
	 */
	/* 10 */	const char*	UPS_VERSION;	/* usually equals NUT_VERSION_MACRO */
	/* 11 */	int		(*banner_is_disabled)(void);
	/* 12 */	const char*	(*describe_NUT_VERSION_once)(void);
	/* 13 */	int		(*print_banner_once)(const char *arg_prog, int arg_even_if_disabled);	/* not CURRENTLY referenced from main.c */
	/* 14 */	void		(*nut_report_config_flags)(void);
	/* 15 */	const char*	(*suggest_doc_links)(const char *arg_progname, const char *arg_progconf);
	/* 16 */	void		(*suggest_NDE_conflict)(void);	/* not CURRENTLY referenced from main.c */

	/* 17 */	void	*sentinel;	/* must be initialized to NULL (whichever way the platform defines one)	*/
	/* 18..31 */	void	*padding[UPSDRV_CALLBACK_PADDING];	/* commented near the macro above */
} upsdrv_callback_t;
void register_upsdrv_callbacks(upsdrv_callback_t *runtime_callbacks, size_t cb_struct_sz);

/* simple call to register implementations named as dictated
 * by this header, which (being a macro) can be called easily
 * from both static and shared builds; keep in mind that builds
 * using these macros for binaries that try to fit together may
 * be years apart eventually. Note that consumers may have to
 * use HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_ADDRESS and/or
 * some of HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE*
 * pragmas around these macros, for builds to succeed under
 * stricter compiler settings (see examples in existing code).
 */
#define init_upsdrv_callbacks(cbptr, cbsz) do {		\
	size_t	cbptr_counter = 0;					\
	if ((cbptr) == NULL) fatalx(EXIT_FAILURE, "Could not init callbacks for shared driver code: null structure pointer");	\
	if ((cbsz) != sizeof(upsdrv_callback_t)) fatalx(EXIT_FAILURE, "Could not init callbacks for shared driver code: unexpected structure size");	\
	memset((cbptr), 0, sizeof(upsdrv_callback_t));			\
	(cbptr)->struct_version = 1;					\
	(cbptr)->ptr_size = sizeof(void*);				\
	(cbptr)->ptr_count = 16;						\
	(cbptr)->sentinel = NULL;					\
	for (cbptr_counter = 0; cbptr_counter < UPSDRV_CALLBACK_PADDING; cbptr_counter++)	\
		(cbptr)->padding[cbptr_counter] = NULL;			\
	snprintf((cbptr)->struct_magic, sizeof((cbptr)->struct_magic), "%s", UPSDRV_CALLBACK_MAGIC);	\
	} while (0)

#define validate_upsdrv_callbacks(cbptr, cbsz, isnew) do {		\
	upsdebugx(5, "validate_upsdrv_callbacks: cbsz=%" PRIuMAX, (uintmax_t)cbsz);	\
	if ((cbptr) == NULL) fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: null structure pointer");	\
	if ((cbsz) != sizeof(upsdrv_callback_t)) fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: unexpected structure size");	\
	upsdebugx(5, "validate_upsdrv_callbacks: ver=%" PRIu64 " ptr_count=%" PRIu64, (cbptr)->struct_version, (cbptr)->ptr_count);	\
	if ((cbptr)->struct_version != 1				\
	 || (cbptr)->ptr_count != 16					\
	) fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: unexpected structure contents");	\
	upsdebugx(5, "validate_upsdrv_callbacks: ptr_size: passed=%" PRIu64 " expected=%" PRIuSIZE, (cbptr)->ptr_size, sizeof(void*));	\
	if ((cbptr)->ptr_size != sizeof(void*))				\
		fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: wrong structure bitness");	\
	if (strcmp((cbptr)->struct_magic, UPSDRV_CALLBACK_MAGIC))	\
		fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: wrong magic");	\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: sentinel: %s", (cbptr)->sentinel == NULL ? "Y" : "N");	\
	if ((cbptr)->sentinel != NULL)					\
		fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: wrong sentinels");	\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_info: %s", (cbptr)->upsdrv_info == NULL ? "Y" : "N");				\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_tweak_prognames: %s", (cbptr)->upsdrv_tweak_prognames == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_initups: %s", (cbptr)->upsdrv_initups == NULL ? "Y" : "N");				\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_initinfo: %s", (cbptr)->upsdrv_initinfo == NULL ? "Y" : "N");			\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_updateinfo: %s", (cbptr)->upsdrv_updateinfo == NULL ? "Y" : "N");			\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_shutdown: %s", (cbptr)->upsdrv_shutdown == NULL ? "Y" : "N");			\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_help: %s", (cbptr)->upsdrv_help == NULL ? "Y" : "N");				\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_cleanup: %s", (cbptr)->upsdrv_cleanup == NULL ? "Y" : "N");				\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: upsdrv_makevartable: %s", (cbptr)->upsdrv_makevartable == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: UPS_VERSION: %s", (cbptr)->UPS_VERSION == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: banner_is_disabled: %s", (cbptr)->banner_is_disabled == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: describe_NUT_VERSION_once: %s", (cbptr)->describe_NUT_VERSION_once == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: print_banner_once: %s", (cbptr)->print_banner_once == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: nut_report_config_flags: %s", (cbptr)->nut_report_config_flags == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: suggest_doc_links: %s", (cbptr)->suggest_doc_links == NULL ? "Y" : "N");		\
	upsdebugx(5, "validate_upsdrv_callbacks: NULL-check: suggest_NDE_conflict: %s", (cbptr)->suggest_NDE_conflict == NULL ? "Y" : "N");		\
	if ((cbptr)->upsdrv_info == NULL				\
	 || (cbptr)->upsdrv_tweak_prognames == NULL			\
	 || (cbptr)->upsdrv_initups == NULL				\
	 || (cbptr)->upsdrv_initinfo == NULL				\
	 || (cbptr)->upsdrv_updateinfo == NULL				\
	 || (cbptr)->upsdrv_shutdown == NULL				\
	 || (cbptr)->upsdrv_help == NULL				\
	 || (cbptr)->upsdrv_cleanup == NULL				\
	 || (cbptr)->upsdrv_makevartable == NULL			\
	 || (cbptr)->UPS_VERSION == NULL				\
	 || (cbptr)->banner_is_disabled == NULL				\
	 || (cbptr)->describe_NUT_VERSION_once == NULL			\
	 || (cbptr)->print_banner_once == NULL				\
	 || (cbptr)->nut_report_config_flags == NULL			\
	 || (cbptr)->suggest_doc_links == NULL				\
	 || (cbptr)->suggest_NDE_conflict == NULL			\
	) if (!isnew) fatalx(EXIT_FAILURE, "Could not register callbacks for shared driver code: some pointers are not initialized");	\
	if (isnew) upsdebugx(5, "validate_upsdrv_callbacks: this is a newly created structure, so some/all NULL references are okay");	\
	} while (0)

#define safe_copy_upsdrv_callbacks(cbptrDrv, cbptrLib, cbszDrv) do {			\
	validate_upsdrv_callbacks(cbptrDrv, cbszDrv, 0);				\
	init_upsdrv_callbacks(cbptrLib, sizeof(upsdrv_callback_t));			\
	validate_upsdrv_callbacks(cbptrLib, sizeof(upsdrv_callback_t), 1);		\
	(cbptrLib)->upsdrv_info			= (cbptrDrv)->upsdrv_info;		\
	(cbptrLib)->upsdrv_tweak_prognames	= (cbptrDrv)->upsdrv_tweak_prognames;	\
	(cbptrLib)->upsdrv_initups		= (cbptrDrv)->upsdrv_initups;		\
	(cbptrLib)->upsdrv_initinfo		= (cbptrDrv)->upsdrv_initinfo;		\
	(cbptrLib)->upsdrv_updateinfo		= (cbptrDrv)->upsdrv_updateinfo;	\
	(cbptrLib)->upsdrv_shutdown		= (cbptrDrv)->upsdrv_shutdown;		\
	(cbptrLib)->upsdrv_help			= (cbptrDrv)->upsdrv_help;		\
	(cbptrLib)->upsdrv_cleanup		= (cbptrDrv)->upsdrv_cleanup;		\
	(cbptrLib)->upsdrv_makevartable		= (cbptrDrv)->upsdrv_makevartable;	\
	(cbptrLib)->UPS_VERSION			= (cbptrDrv)->UPS_VERSION;		\
	(cbptrLib)->banner_is_disabled		= (cbptrDrv)->banner_is_disabled;	\
	(cbptrLib)->describe_NUT_VERSION_once	=(cbptrDrv)->describe_NUT_VERSION_once;	\
	(cbptrLib)->print_banner_once		= (cbptrDrv)->print_banner_once;	\
	(cbptrLib)->nut_report_config_flags	= (cbptrDrv)->nut_report_config_flags;	\
	(cbptrLib)->suggest_doc_links		= (cbptrDrv)->suggest_doc_links;	\
	(cbptrLib)->suggest_NDE_conflict	= (cbptrDrv)->suggest_NDE_conflict;	\
	} while (0)

#define default_register_upsdrv_callbacks() do {				\
	upsdrv_callback_t	callbacksTmp;					\
	init_upsdrv_callbacks(&callbacksTmp, sizeof(callbacksTmp));		\
	callbacksTmp.upsdrv_info		= &upsdrv_info;			\
	callbacksTmp.upsdrv_tweak_prognames	= upsdrv_tweak_prognames;	\
	callbacksTmp.upsdrv_initups		= upsdrv_initups;		\
	callbacksTmp.upsdrv_initinfo		= upsdrv_initinfo;		\
	callbacksTmp.upsdrv_updateinfo		= upsdrv_updateinfo;		\
	callbacksTmp.upsdrv_shutdown		= upsdrv_shutdown;		\
	callbacksTmp.upsdrv_help		= upsdrv_help;			\
	callbacksTmp.upsdrv_cleanup		= upsdrv_cleanup;		\
	callbacksTmp.upsdrv_makevartable	= upsdrv_makevartable;		\
	callbacksTmp.UPS_VERSION		= UPS_VERSION;			\
	callbacksTmp.banner_is_disabled		= banner_is_disabled;		\
	callbacksTmp.describe_NUT_VERSION_once	= describe_NUT_VERSION_once;	\
	callbacksTmp.print_banner_once		= print_banner_once;		\
	callbacksTmp.nut_report_config_flags	= nut_report_config_flags;	\
	callbacksTmp.suggest_doc_links		= suggest_doc_links;		\
	callbacksTmp.suggest_NDE_conflict	= suggest_NDE_conflict;		\
	register_upsdrv_callbacks(&callbacksTmp, sizeof(upsdrv_callback_t));	\
	} while (0)

#endif /* NUT_MAIN_H_SEEN */
