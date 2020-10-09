/* apcupsd-ups.c - client for apcupsd

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>

#include "main.h"
#include "apcupsd-ups.h"

#define DRIVER_NAME	"apcupsd network client UPS driver"
#define DRIVER_VERSION	"0.5"

#define POLL_INTERVAL_MIN 10

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Andreas Steinmetz <ast@domdv.de>",
	DRV_STABLE,
	{ NULL }
};

static int port=3551;
static struct sockaddr_in host;

static void process(char *item,char *data)
{
	int i;
	char *p1;
	char *p2;

	for(i=0;nut_data[i].info_type;i++)if(!(nut_data[i].apcupsd_item))
		dstate_setinfo(nut_data[i].info_type,"%s",
			nut_data[i].default_value);
	else if(!strcmp(nut_data[i].apcupsd_item,item))
			switch(nut_data[i].drv_flags&~DU_FLAG_INIT)
	{
	case DU_FLAG_STATUS:
		status_init();
		if(!strcmp(data,"COMMLOST")||!strcmp(data,"NETWORK ERROR")||
		   !strcmp(data,"ERROR"))status_set("OFF");
		else if(!strcmp(data,"SELFTEST"))status_set("OB");
		else for(;(data=strtok(data," "));data=NULL)
		{
			if(!strcmp(data,"CAL"))status_set("CAL");
			else if(!strcmp(data,"TRIM"))status_set("TRIM");
			else if(!strcmp(data,"BOOST"))status_set("BOOST");
			else if(!strcmp(data,"ONLINE"))status_set("OL");
			else if(!strcmp(data,"ONBATT"))status_set("OB");
			else if(!strcmp(data,"OVERLOAD"))status_set("OVER");
			else if(!strcmp(data,"SHUTTING DOWN")||
				!strcmp(data,"LOWBATT"))status_set("LB");
			else if(!strcmp(data,"REPLACEBATT"))status_set("RB");
			else if(!strcmp(data,"NOBATT"))status_set("BYPASS");
		}
		status_commit();
		break;

	case DU_FLAG_DATE:
		if((p1=strchr(data,' ')))
		{
			*p1=0;
			dstate_setinfo(nut_data[i].info_type,"%s",data);
			*p1=' ';
		}
		else dstate_setinfo(nut_data[i].info_type,"%s",data);
		break;

	case DU_FLAG_TIME:
		if((p1=strchr(data,' ')))
		{
			*p1=0;
			if((p2=strchr(p1+1,' ')))
			{
				*p2=0;
				dstate_setinfo(nut_data[i].info_type,"%s",p1+1);
				*p2=' ';
			}
			else dstate_setinfo(nut_data[i].info_type,"%s",p1+1);
			*p1=' ';
		}
		break;

	case DU_FLAG_FW1:
		if((p1=strchr(data,'/')))
		{
			for(;p1!=data;p1--)if(p1[-1]!=' ')break;
			if(*p1==' ')
			{
				*p1=0;
				dstate_setinfo(nut_data[i].info_type,"%s",data);
				*p1=' ';
			}
			else dstate_setinfo(nut_data[i].info_type,"%s",data);
		}
		else dstate_setinfo(nut_data[i].info_type,"%s",data);
		break;

	case DU_FLAG_FW2:
		if((p1=strchr(data,'/')))
		{
			for(;*p1;p1++)if(p1[1]!=' ')break;
			if(*p1&&p1[1])dstate_setinfo(nut_data[i].info_type,"%s",
				p1+1);
		}
		break;

	default:if(nut_data[i].info_flags&ST_FLAG_STRING)
		{
			if((int)strlen(data)>(int)nut_data[i].info_len)
				data[(int)nut_data[i].info_len]=0;
			dstate_setinfo(nut_data[i].info_type,"%s",data);
		}
		else dstate_setinfo(nut_data[i].info_type,
			nut_data[i].default_value,
			atof(data)*nut_data[i].info_len);
		break;
	}
}

static int getdata(void)
{
	int x, fd_flags;
	short n;
	char *item;
	char *data;
	struct pollfd p;
	char bfr[1024];

	for(x=0;nut_data[x].info_type;x++)
		if(!(nut_data[x].drv_flags & DU_FLAG_INIT) && !(nut_data[x].drv_flags & DU_FLAG_PRESERVE))
			dstate_delinfo(nut_data[x].info_type);

	if((p.fd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		upsdebugx(1,"socket error");
		return -1;
	}

	if(connect(p.fd,(struct sockaddr *)&host,sizeof(host)))
	{
		upsdebugx(1,"can't connect to apcupsd");
		close(p.fd);
		return -1;
	}

	fd_flags = fcntl(p.fd, F_GETFL);
	fd_flags |= O_NONBLOCK;
	if(fcntl(p.fd, F_SETFL, fd_flags))
	{
		upsdebugx(1,"unexpected fcntl(fd, F_SETFL, fd_flags|O_NONBLOCK) failure");
		close(p.fd);
		return -1;
	}

	p.events=POLLIN;

	n=htons(6);
	x=write(p.fd,&n,2);
	x=write(p.fd,"status",6);

	/* TODO: double-check for poll() in configure script */
	while(poll(&p,1,15000)==1)
	{
		if(read(p.fd,&n,2)!=2)
		{
			upsdebugx(1,"apcupsd communication error");
			close(p.fd);
			return -1;
		}

		if(!(x=ntohs(n)))
		{
			close(p.fd);
			return 0;
		}
		else if(x<0||x>=(int)sizeof(bfr))
		{
			upsdebugx(1,"apcupsd communication error");
			close(p.fd);
			return -1;
		}

		if(poll(&p,1,15000)!=1)break;

		if(read(p.fd,bfr,x)!=x)
		{
			upsdebugx(1,"apcupsd communication error");
			close(p.fd);
			return -1;
		}

		bfr[x]=0;

		if(!(item=strtok(bfr," \t:\r\n")))
		{
			upsdebugx(1,"apcupsd communication error");
			close(p.fd);
			return -1;
		}

		if(!(data=strtok(NULL,"\r\n")))
		{
			upsdebugx(1,"apcupsd communication error");
			close(p.fd);
			return -1;
		}
		while(*data==' '||*data=='\t'||*data==':')data++;

		process(item,data);
	}

	upsdebugx(1,"unexpected connection close by apcupsd");
	close(p.fd);
	return -1;
}

void upsdrv_initinfo(void)
{
	if(!port)fatalx(EXIT_FAILURE,"invalid host or port specified!");
	if(getdata())fatalx(EXIT_FAILURE,"can't communicate with apcupsd!");
	else dstate_dataok();

	poll_interval = (poll_interval > POLL_INTERVAL_MIN) ? POLL_INTERVAL_MIN : poll_interval;
}

void upsdrv_updateinfo(void)
{
	if(getdata())upslogx(LOG_ERR,"can't communicate with apcupsd!");
	else dstate_dataok();

	poll_interval = (poll_interval > POLL_INTERVAL_MIN) ? POLL_INTERVAL_MIN : poll_interval;
}

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	char *p;
	struct hostent *h;

	if(device_path&&*device_path)
	{
		/* TODO: fix parsing since bare IPv6 addresses contain colons */
		if((p=strchr(device_path,':')))
		{
			*p++=0;
			port=atoi(p);
			if(port<1||port>65535)port=0;
		}
	}
	else device_path="localhost";

	if(!(h=gethostbyname(device_path)))port=0;
	else memcpy(&host.sin_addr,h->h_addr,4);

	/* TODO: add IPv6 support */
	host.sin_family=AF_INET;
	host.sin_port=htons(port);
}

void upsdrv_cleanup(void)
{
}
