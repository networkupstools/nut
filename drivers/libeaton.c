/* libeaton.c - Stub functions for Eaton SDK libraries
 * Copyright (C) 2011 Eaton
 *  Author: Frederic BOHE <fredericbohe@eaton.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include "common.h"
#include "config.h"
#include "dstate.h"

#define LIBEATON_REVISION "3"
#define LIBEATON_VERSION PACKAGE_VERSION "-" LIBEATON_REVISION
char libeaton_version[SMALLBUF] = LIBEATON_VERSION;

/* public functions & variables from main.c */
char *device_path;
int upsfd;
int  exit_flag = 0;
int do_lock_port;
const char *progname = "NUT SDK";
const char *upsname = NULL;
const char *device_name = NULL;
int extrafd;

/* subdriver description structure */
typedef struct upsdrv_info_s {
        const char      *name;          /* driver full name, for banner printing, ... */
        const char      *version;       /* driver version */
        const char      *authors;       /* authors name */
        const int       status;         /* driver development status */
        struct upsdrv_info_s    *subdrv_info[2];        /* sub driver information */
} upsdrv_info_t;

extern upsdrv_info_t    upsdrv_info;

/* fake dstate (driver interface with upsd) */
int dstate_addenum(const char *var, const char *fmt, ...) { return 0; }
int dstate_delinfo(const char *var) { return 0; }
/* not needed
 * void dstate_init(const char *prog, const char *devname):
 * int dstate_poll_fds(struct timeval timeout, int extrafd);
 * int dstate_delenum(const char *var, const char *val);
 * int dstate_delcmd(const char *cmd);
 * void dstate_free(void);
 * const st_tree_t *dstate_getroot(void);
 * const cmdlist_t *dstate_getcmdlist(void); */

void dstate_dataok(void) { ; }
void dstate_datastale(void) { ; }

/* fake main (driver core) */
void do_upsconf_args(char *confupsname, char *var, char *val) { ; }
void addvar(int vartype, const char *name, const char *desc) { ; }
char *getval(const char *var) { return NULL; }
int testvar(const char *var) { return 0; }

unsigned int    poll_interval = 2;
struct ups_handler      upsh;

void upsdrv_initinfo(void);
void upsdrv_initups(void);
void upsdrv_updateinfo(void);

/* ups.status management functions */
static int alarm_active = 0, ignorelb = 0;
static char     status_buf[ST_MAX_VALUE_LEN], alarm_buf[ST_MAX_VALUE_LEN];
/* clean out the temp space for a new pass */
/* copied from dstate.c */
void status_init(void)
{
        if (dstate_getinfo("driver.flag.ignorelb")) {
                ignorelb = 1;
        }

        memset(status_buf, 0, sizeof(status_buf));
}

/* add a status element */
void status_set(const char *buf)
{
        if (ignorelb && !strcasecmp(buf, "LB")) {
                upsdebugx(2, "%s: ignoring LB flag from device", __func__);
                return;
        }

        /* separate with a space if multiple elements are present */
        if (strlen(status_buf) > 0) {
                snprintfcat(status_buf, sizeof(status_buf), " %s", buf);
        } else {
                snprintfcat(status_buf, sizeof(status_buf), "%s", buf);
        }
}

/* write the status_buf into the externally visible dstate storage */
void status_commit(void)
{
        while (ignorelb) {
                const char      *val, *low;

                val = dstate_getinfo("battery.charge");
                low = dstate_getinfo("battery.charge.low");

                if (val && low && (strtol(val, NULL, 10) < strtol(low, NULL, 10))) {
                        snprintfcat(status_buf, sizeof(status_buf), " LB");
                        upsdebugx(2, "%s: appending LB flag [charge '%s' below '%s']", __func__, val, low);
                        break;
                }

                val = dstate_getinfo("battery.runtime");
                low = dstate_getinfo("battery.runtime.low");

                if (val && low && (strtol(val, NULL, 10) < strtol(low, NULL, 10))) {
                        snprintfcat(status_buf, sizeof(status_buf), " LB");
                        upsdebugx(2, "%s: appending LB flag [runtime '%s' below '%s']", __func__, val, low);
                        break;
                }

                /* LB condition not detected */
                break;
        }

        if (alarm_active) {
                dstate_setinfo("ups.status", "ALARM %s", status_buf);
        } else {
                dstate_setinfo("ups.status", "%s", status_buf);
        }
}
/* similar handlers for ups.alarm */

void alarm_init(void)
{
        memset(alarm_buf, 0, sizeof(alarm_buf));
}

void alarm_set(const char *buf)
{
        if (strlen(alarm_buf) > 0) {
                snprintfcat(alarm_buf, sizeof(alarm_buf), " %s", buf);
        } else {
                snprintfcat(alarm_buf, sizeof(alarm_buf), "%s", buf);
        }
}

/* write the status_buf into the info array */
void alarm_commit(void)
{
        if (strlen(alarm_buf) > 0) {
                dstate_setinfo("ups.alarm", "%s", alarm_buf);
                alarm_active = 1;
        } else {
                dstate_delinfo("ups.alarm");
                alarm_active = 0;
        }
}

/* Variable cache managment */
static int num_info = 0;
static char **info_name = NULL;
static char **info_data = NULL;
static int *info_aux = NULL;
static int *info_flags = NULL;
static int num_cmd = 0;
static char **info_cmd = NULL;
static char * dump_buffer=NULL;

int dstate_setinfo(const char *var, const char *fmt, ...)
{

        char    value[ST_MAX_VALUE_LEN];
        va_list ap;
        int i;

        va_start(ap, fmt);
        vsnprintf(value, sizeof(value), fmt, ap);
        va_end(ap);

        for( i=0; i<num_info; i++) {
                if(strcmp(info_name[i],var) == 0) {
                        free(info_data[i]);
                        info_data[i] = strdup(value);
                        if( info_data[num_info-1] == NULL ) {
                                return 0;
                        }
                        return 1;
                }
        }

        num_info++;

        info_name = realloc(info_name,sizeof(char *) * num_info);
        if(info_name == NULL) {
                num_info--;
                return 0;
        }
        info_data = realloc(info_data,sizeof(char *) * num_info);
        if(info_data == NULL) {
                num_info--;
                return 0;
        }

        info_flags = realloc(info_flags,sizeof(int) * num_info);
        if(info_flags == NULL) {
                num_info--;
                return 0;
        }

        info_aux = realloc(info_aux,sizeof(int) * num_info);
        if(info_aux == NULL) {
                num_info--;
                return 0;
        }

        info_name[num_info-1] = strdup(var);
        if( info_name[num_info-1] == NULL ) {
                num_info--;
                return 0;
        }

        info_data[num_info-1] = strdup(value);
        if( info_data[num_info-1] == NULL ) {
                num_info--;
                return 0;
        }

	info_flags[num_info-1] = 0;
	info_aux[num_info-1] = 0;

        return 1;
}

const char *dstate_getinfo(const char *var)
{
        int i;

        for( i=0; i<num_info; i++) {
                if(strcmp(info_name[i],var) == 0) {
                        return info_data[i];
                }
        }
        return NULL;
}

void dstate_setflags(const char *var, int flags)
{
	int i;

        for( i=0; i<num_info; i++) {
                if(strcmp(info_name[i],var) == 0) {
                        info_flags[i] |= flags;
			return;
                }
        }
}

void dstate_setaux(const char *var, int aux)
{
	int i;

        for( i=0; i<num_info; i++) {
                if(strcmp(info_name[i],var) == 0) {
                        info_aux[i] = aux;
			return;
                }
        }
}

void dstate_addcmd(const char *cmdname)
{
        num_cmd++;

        info_cmd = realloc(info_cmd,sizeof(char *) * num_cmd);
        if(info_cmd == NULL) {
                num_cmd--;
                return;
        }

        info_cmd[num_cmd-1] = strdup(cmdname);
        if( info_cmd[num_cmd-1] == NULL ) {
                num_cmd--;
                return;
        }
}

/* libeaton API */
void libeaton_init(char * device)
{
	if( nut_debug_level > 0 ) {
		printf("libeaton version : %s\n",libeaton_version);
	}
	device_path = device;

        upsdrv_initups();
        upsdrv_initinfo();

}

void libeaton_free()
{
        int i;

        for(i=0; i<num_info ; i++){
                free(info_name[i]);
                free(info_data[i]);
        }

        if(num_info) {
                free(info_name);
                free(info_data);
                info_name = NULL;
                info_data = NULL;
        }
}

const char *libeaton_read(const char *varname)
{
	return dstate_getinfo(varname);
}

int libeaton_write(const char *varname, const char *val)
{
	return upsh.setvar(varname,val);
}

int libeaton_command(const char *cmdname, const char *extradata)
{
	return upsh.instcmd(cmdname,extradata);
}

void libeaton_update(void)
{
	upsdrv_updateinfo();
}

const char * libeaton_dump_all(void)
{
	int i;
	int size = 0;
	char buf[SMALLBUF];

	if(dump_buffer!=NULL) {
		free(dump_buffer);
	}

	for(i = 0 ; i < num_info ; i++) {
		if( info_flags[i] & ST_FLAG_RW ) {
			sprintf(buf,
				"VAR_RW\t%s\t%s\n",info_name[i],
				info_data[i]);
			dump_buffer = realloc(dump_buffer,size+strlen(buf)+1);
			memcpy(dump_buffer+size,buf,strlen(buf));
			size = size + strlen(buf);
			dump_buffer[size] = 0;
		}
		else {
			sprintf(buf,
				"VAR_RO\t%s\t%s\n",info_name[i],
				info_data[i]);
			dump_buffer = realloc(dump_buffer,size+strlen(buf)+1);
			memcpy(dump_buffer+size,buf,strlen(buf));
			size = size + strlen(buf);
			dump_buffer[size] = 0;
		}
	}

	for(i = 0 ; i < num_cmd ; i++) {
		sprintf(buf,
			"CMD\t%s\n",info_cmd[i]);
		dump_buffer = realloc(dump_buffer,size+strlen(buf)+1);
		memcpy(dump_buffer+size,buf,strlen(buf));
		size = size + strlen(buf);
		dump_buffer[size] = 0;
	}

	return dump_buffer;
}

