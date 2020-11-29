#ifndef NUT_MAIN_H_SEEN
#define NUT_MAIN_H_SEEN

#include "common.h"
#include "upsconf.h"
#include "dstate.h"
#include "extstate.h"

/* public functions & variables from main.c */
extern const char	*progname, *upsname, *device_name;
extern char		*device_path;
extern int		upsfd, extrafd, broken_driver, experimental_driver, do_lock_port, exit_flag;
extern unsigned int	poll_interval;

/* functions & variables required in each driver */
void upsdrv_initups(void);	/* open connection to UPS, fail if not found */
void upsdrv_initinfo(void);	/* prep data, settings for UPS monitoring */
void upsdrv_updateinfo(void);	/* update state data if possible */
void upsdrv_shutdown(void);	/* make the UPS power off the load */
void upsdrv_help(void);		/* tack on anything useful for the -h text */
void upsdrv_banner(void);	/* print your version information */
void upsdrv_cleanup(void);	/* free any resources before shutdown */

/* --- details for the variable/value sharing --- */

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
	struct vartab_s	*next;
} vartab_t;

/* flags to define types in the vartab */

#define VAR_FLAG	0x0001	/* argument is a flag (no value needed) */
#define VAR_VALUE	0x0002	/* argument requires a value setting	*/
#define VAR_SENSITIVE	0x0004	/* do not publish in driver.parameter	*/

/* callback from driver - create the table for future -x entries */
void addvar(int vartype, const char *name, const char *desc);

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

#endif /* NUT_MAIN_H_SEEN */
