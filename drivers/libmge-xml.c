/*!
 * @file libmge-xml.c
 * @brief HID Library - Network MGE XML communication sub driver and HID stubs
 *
 * @author Copyright (C)
 *  2007 Arnaud Quette <arnaudquette@eaton.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

/* TODO list
 * - cleanup, cleanup, cleanup
 * - manage notifications (broadcasted alarms)
 * - fix the outlet handling
 * - manage the settings (authenticated mode)
 * - final XML parser selection (must be tiny and possibly GPL):
 *   * ezXML (current): MIT license, small (tiny)
 *   * Pico XML: tiny, pull-parser, LGPL, http://kd7yhr.org/bushbo/pico_xml.md
 * - if ezXML is kept, clean it up (mostly, the C++ style comment have to be converted)
 */

/* Protocol overview
 *   * MGE XML/HTTP exposes the same "HID" data than the ones used in USB and SHUT.
 *   * The main difference is that these are in textual form only (string paths)
 *   in an XML file. So there is no need for a HID Parser, but only for the
 *   hid2nut lookup table.
 *   * Moreover, the "bus" is the HTTP protocol (port 80 or 4679/4680)
 *   * There are 2 main files:
 *     - product.xml: a kind of descriptor for interacting with the card
 *     - upsprop.xml: list the supported data and values, in the form
 *     <OBJECT name="UPS.PowerSummary.iProduct">Evolution</OBJECT>
 */
 
#include "main.h"
#include "config.h"
#include "libhid.h"			/* For interface declaration */
#include "libmge-xml.h"
#include "common.h"			/* for xmalloc, upsdebugx prototypes */

#include "mge-hid.h"		/* MGE Data (models, paths, ...) */

/* For Network - HTTP access */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* EzXML - XML parser */
#include "ezxml.h"

#define BUFF_SIZE 10240

#define MGE_XML_DRIVER_NAME	"MGE XML/HTTP communication driver"
#define MGE_XML_DRIVER_VERSION	"0.10"

#define MGE_VENDORID 0x0463

/* UPS properties buffer structure: holds upsprop data */
typedef struct upsprop_s {
       time_t	ts;		/* timestamp when report was retrieved */
       ezxml_t xmlUpsProp;	/* XML data */
} upsprop_t;

upsprop_t *ups_properties = NULL;

#define MAX_TIME	5	/* validity period of upsprop (5 sec) */


/* TCP Network defines, variables and functions */ 
static int comport = -1;
#define DEFAULT_PORT 80 	/* works on the standard HTTP port */
#define ALT_PORT 4679		/* atlernate MGE dedicated HTTP port */

struct sockaddr_in *agent_sock = NULL;

static int tcp_connect();
static void tcp_close();

/* To obtain the <PRODUCT_INFO><SUMMARY><XML_SUMMARY_PAGE> tag */
#define PRODUCT_FILE	"mgeups/product.xml"
char *upsprop_url = NULL;	/* obtained using the above path */


char *getHTTP(const char* url, const char* login, const char* password);


/* ---------------------------------------------
 * XML layer functions implementations
 * --------------------------------------------- */

/*! get an object 
 * @param xml_data XML data
 * @param path the XML path of the object in fully qualified form (including the root)
 *  (ex: <PRODUCT_INFO><SUMMARY><XML_SUMMARY_PAGE>)
 * @param attr_name the name of an attribute to match (can be NULL)
 * @param attr_value the value of an attribute to match (can be NULL)
 */
ezxml_t get_object(ezxml_t xml_data, const char *path, const char *attr_name, const char *attr_value)
{
	ezxml_t object = xml_data;
	const char *cur_attr_value;
	char *cur_obj_name, *tmp_path;

	upsdebugx(5, "get_object: path = %s", path);
		
	/* Get the requested object node names */
	tmp_path = xstrdup(path);
	if (strstr(path, "><") != NULL) {
		cur_obj_name = strtok (tmp_path,"><");
		cur_obj_name = strtok (NULL, "><"); /* skip the root */
	}
	else
		cur_obj_name = strtok (tmp_path,"><");

	/* Reach the required depth */
	while ( cur_obj_name != NULL ) {
		object = ezxml_child(object, cur_obj_name);
		cur_obj_name = strtok (NULL, "><");
	}

	/* Now check if the object matches attr_name / value */
	if (attr_name != NULL) {
		while (1) {
			if (object != NULL) {
				cur_attr_value = ezxml_attr(object, attr_name);
				if (cur_attr_value != NULL) {
					upsdebugx (3, "%s->%s = %s", path, attr_name, cur_attr_value);
					if (!strcmp(attr_value, cur_attr_value))
						break;
				}
				object = object->next;
			}
			else
				break;
		}
	}
	if (object == NULL)
		upsdebugx(3, "get_object: no object found!");

	free (tmp_path);
	return object;
}

/*! get an attribute value 
 * @param xml_data XML data
 * @param path the XML path of the attribute in fully qualified form (including the root)
 *  (ex: <PRODUCT_INFO><SUMMARY><XML_SUMMARY_PAGE>)
 * @param attr_name the name of the attribute
 */

char *get_attribute_value(ezxml_t xml_data, const char *path, const char *attr_name)
{
	const char *attr_value = NULL;
	ezxml_t object;

	upsdebugx (3, "get_attribute_value: path = %s", path);

	/* Now get the requested object */
	object = get_object(xml_data, path, NULL, NULL);

	if (object != NULL) {
		attr_value = ezxml_attr(object, attr_name);
		upsdebugx (3, "%s->%s = %s", path, attr_name, attr_value);
	}
	
	return xstrdup(attr_value);
}

/*! get an object value 
 * @param xml_data XML data
 * @param path the XML path of the object in fully qualified (including the root) doted form
 *  (ex: PRODUCT_INFO.SUMMARY.XML_SUMMARY_PAGE)
 * @param attr_name the name of an attribute to match (can be NULL)
 * @param attr_value the value of an attribute to match (can be NULL)
 */
char *get_object_value(ezxml_t xml_data, const char *path, const char *attr_name, const char *attr_value)
{
	ezxml_t object = get_object(xml_data, path, attr_name, attr_value);
	if (object != NULL)
		return object->txt;
	else
		return NULL;
}


#if 0


/*!
 * MGE XML functions for HID marshalling
 */

/* FIXME */
char * mge_xml_strerror() { return ""; }

#endif /* if 0 */

/************************************************************************/
/************************************************************************/
/********************** COMM DRIVER LAYER *******************************/
/************************************************************************/
/************************************************************************/


/*************************************************************************
 * xml_synchronise ()
 * 
 * initiate communication with the UPS
 *
 * return TRUE on success, FALSE on failure
 *
 ************************************************************************/
int xml_synchronise()
{
	/* Try to get the default root document */
	if (getHTTP("", "", "") != NULL)
		return TRUE;
	else
		return FALSE;
}

/* Wrapper to retrieve upsprop.xml, and set timestamp */
int get_ups_properties()
{
	char *XmlStatus;
	
	/* first time init */
	if (ups_properties == NULL) {
		ups_properties = xmalloc (sizeof (upsprop_t));
		ups_properties->ts = -1;
		ups_properties->xmlUpsProp = NULL;
	}

	/* check if we need to refresh upsprop */
	if (ups_properties->ts + MAX_TIME > time(NULL)) {
		/* buffered upsprop is still good; nothing to do */
		return 0;
	}

	/* else, refresh upsprop */
	upsdebugx(4, "refreshing upsprop");
	if (ups_properties->xmlUpsProp != NULL) {
		ezxml_free(ups_properties->xmlUpsProp);
		ups_properties->xmlUpsProp = NULL;
	}

	if ( (XmlStatus = getHTTP(upsprop_url, "", "")) != NULL) {
		ups_properties->xmlUpsProp = ezxml_parse_str(XmlStatus, strlen(XmlStatus));
		free (XmlStatus);

		/* valid report */
		time(&ups_properties->ts);
	}
	else
		return -1;

	return 0;
}
/* On success, fill in the curDevice structure and return the report
 * descriptor length. On failure, return -1.
 * Note: When callback is not NULL, the report descriptor will be
 * passed to this function together with the udev and MGEXMLDevice_t
 * information. This callback should return a value > 0 if the device
 * is accepted, or < 1 if not.
 */

int libmge_xml_open(hid_dev_handle_t *udev, MGEXMLDevice_t *curDevice, char *matcher,
	int (*callback)(hid_dev_handle_t udev, MGEXMLDevice_t *hd, unsigned char *rdbuf, int rdlen))
{
	int res;
	char *string;
	char *XmlProperty;
	ezxml_t xmlContent;

	upsdebugx(2, "libmge_xml_open: using host %s", device_path);

	/* Initial connection */
	tcp_connect();

	if (!xml_synchronise())
	{
		upsdebugx(2, "No communication with UPS");
		return -1;
	}

	upsdebugx(2, "Communication with UPS established");

	/* we can skip the rest due to network bus specifics! */
	if (!callback) {
		return 1;
	}

 /*
	* Next steps:
	* ===========
	* 1) Get and parse product.xml for <PRODUCT_INFO><SUMMARY><XML_SUMMARY_PAGE> tag (default value = upsprop.xml)
	*
	* <PRODUCT_INFO name="Network Management Card" type="Mosaic M" version="V4_00_14">
	*		=>  ups.firmware[.aux]
	*/
	if ( (XmlProperty = getHTTP(PRODUCT_FILE, "", "")) != NULL) {
		upsdebugx(3, "%s:\n%s", PRODUCT_FILE, XmlProperty);
		xmlContent = ezxml_parse_str(XmlProperty, strlen(XmlProperty));
		
		free (XmlProperty);
		upsprop_url = get_attribute_value(xmlContent,
			"<PRODUCT_INFO><SUMMARY><XML_SUMMARY_PAGE>", "url");
	}
	else {
		upsdebugx(2, "Unable to get /%s", PRODUCT_FILE);
		return -1;
	}

	/* No existing DEVICE descriptor */

	/* collect the identifying information of this
		device. Note that this is safe, because
		there's no need to claim an interface for
		this (and therefore we do not yet need to
		detach any kernel drivers). */

	free(curDevice->Vendor);
	free(curDevice->Product);
	free(curDevice->Serial);
	free(curDevice->Bus);
	memset(curDevice, '\0', sizeof(*curDevice));

	curDevice->VendorID = MGE_VENDORID;
	curDevice->ProductID = 0x001;	/* Fake */
	curDevice->Vendor = strdup("MGE Office Protection Systems");
	curDevice->Bus = strdup("xml/http");


	/*	2) Get and parse upsprop.xml */
	if (upsprop_url	!= NULL) {
		if (get_ups_properties() != -1) {
		
			/* Get iProduct */
			string = get_object_value(ups_properties->xmlUpsProp, "<OBJECT>", "name", "UPS.PowerSummary.iProduct");
			if (string != NULL) {
				upsdebugx(3, "iProduct = %s", string);
				curDevice->Product = strdup(string);
			} else {
				curDevice->Product = strdup("unknown");
			}

			/* Get iSerialNumber */
			string = get_object_value(ups_properties->xmlUpsProp, "<OBJECT>", "name", "UPS.PowerSummary.iSerialNumber");
			if (string != NULL) {
				upsdebugx(3, "iSerialNumber = %s", string);
				curDevice->Serial = strdup(string);
			} else {
				curDevice->Serial = strdup("unknown");
			}
		}
		else {
			upsdebugx(2, "error retrieving %s", upsprop_url);
			return -1;
		}
	}
	else {
		upslogx(LOG_ERR, "No upsprop.xml URL found");
		return -1;
	}

	upsdebugx(2, "- VendorID: %04x", curDevice->VendorID);
	upsdebugx(2, "- ProductID: %04x", curDevice->ProductID);
	upsdebugx(2, "- Manufacturer: %s", curDevice->Vendor);
	upsdebugx(2, "- Product: %s", curDevice->Product);
	upsdebugx(2, "- Serial Number: %s", curDevice->Serial);
	upsdebugx(2, "- Bus: %s", curDevice->Bus);
	upsdebugx(2, "Device matches");

	/* No existing HID descriptor! */

	/* No existing REPORT descriptor! */

	res = callback(*udev, curDevice, NULL, 0);
	
	upsdebugx(2, "Found HID device");
	fflush(stdout);

	return 1;
}

void libmge_xml_close(hid_dev_handle_t udev)
{
	free (ups_properties);
	ups_properties = NULL;

	free (upsprop_url);
	upsprop_url = NULL;

	tcp_close();
}

/* Fake report retrieval stub
 * XML/HTTP doesn't have the report notion
 */
int libmge_xml_get_report(hid_dev_handle_t udev, int ReportId,
		       unsigned char *raw_buf, int ReportSize )
{
	return 0; /* sufficient? */
}

/* Fake report setting stub
 * XML/HTTP doesn't have the report notion
 */
int libmge_xml_set_report(hid_dev_handle_t udev, int ReportId,
		       unsigned char *raw_buf, int ReportSize )
{
	return 0; /* sufficient? */
}

/* Fake string retrieval stub
 * XML/HTTP doesn't have the indexed string notion
 */
int libmge_xml_get_string(hid_dev_handle_t udev, int StringIdx, char *buf, size_t buflen)
{
	return 0; /* sufficient? */
}

/* FIXME: this is a stub for now, but 
 * XML/HTTP support notifications through alarms (tcp/connected mode)
 * or through broadcasted alarms (udp mode)
 */
int libmge_xml_get_interrupt(hid_dev_handle_t udev, unsigned char *buf,
			   int bufsize, int timeout)
{
	return 0;
}

mgexml_communication_subdriver_t mgexml_subdriver = {
	MGE_XML_DRIVER_NAME,
	MGE_XML_DRIVER_VERSION,
	libmge_xml_open,
	libmge_xml_close,
	libmge_xml_get_report,
	libmge_xml_set_report,
	libmge_xml_get_string,
	libmge_xml_get_interrupt
};

/**********************************************************************
 * HID stub functions implementations
 * This replaces libhid.ch, hidparser.ch and hidtype.h
 **********************************************************************/

/*** from libhid.ch ***/

/* Communication layers and drivers (MGE XML/HTTP) */
communication_subdriver_t *comm_driver = &mgexml_subdriver;

/* Global usage table (from USB HID class definition)
 * XML/HTTP process only string path
 * Since we don't use path lookup, we expose an empty table
 */
usage_lkp_t hid_usage_lkp[] = {
	/* end of structure. */
	{ NULL, 0 }
};

/* rework path to fix naming problems */
void FixPathName(char *hidpath)
{
	char *ptr;
	int counter = 0, path_len;

	/* indexed collection */
	/* MGE expose these as "x[index]", while nut separate the path
	 * components with dots like "x.[index]"
	 */
	if ( (ptr = strchr(hidpath, '[')) != NULL)
	{
		for (path_len = strlen (hidpath) ; counter < path_len ; counter++) {
			if (hidpath[counter] == '[') {
				hidpath[counter - 1] = '\0'; /* clear the previous dot */
			}
		}
		strcat(hidpath, ptr);
	}

	/* Handle SwitchOn/Off => SwitchOnOff misspelling in XML */
	if ( (ptr = strstr(hidpath, "SwitchOn/Off")) != NULL)
	{
		*ptr = '\0';
		strcat(hidpath, "SwitchOnOff");
	}
}

/*
 * HIDGetItemValue
 * -------------------------------------------------------------------------- */
int HIDGetItemValue(hid_dev_handle_t udev, const char *hidpath, double *Value, usage_tables_t *utab)
{
	return 0;
}

/*
 * HIDGetItemString
 * -------------------------------------------------------------------------- */
char *HIDGetItemString(hid_dev_handle_t udev, const char *hidpath, char *buf, size_t buflen, usage_tables_t *utab)
{
	char *string;

	upsdebugx(2, "HIDGetItemString(%s)", hidpath);

	/* Refresh data */
	get_ups_properties();

	if ((string = get_object_value(ups_properties->xmlUpsProp, "<OBJECT>", "name", hidpath)) != NULL) {
		strcpy(buf, string);
		return buf;
	}
	else
		return NULL;
}

/*
 * HIDSetItemValue
 * -------------------------------------------------------------------------- */
bool_t HIDSetItemValue(hid_dev_handle_t udev, const char *hidpath, double value, usage_tables_t *utab)
{
	/* FIXME: this has to be implemented using product.xml-> <UPS_DATA><SET_OBJECT> */
	return FALSE;
}

/*
 * GetItemData
 * -------------------------------------------------------------------------- */
HIDData_t *HIDGetItemData(const char *hidpath, usage_tables_t *utab)
{
  HIDData_t *pData = NULL;
	char *search_path;

	search_path = xstrdup(hidpath);
	FixPathName(search_path);

	upsdebugx(2, "HIDGetItemData(%s)", search_path);

	/* Refresh data */
	if (get_ups_properties() != -1) {

		/* we only need to return a non null object to tell that it exists */
		if (get_object_value(ups_properties->xmlUpsProp, "<OBJECT>", "name", search_path) != NULL) {
			pData = (HIDData_t *) xmalloc (sizeof(HIDData_t));
			pData->Type = ITEM_FEATURE;
			pData->ReportID = 0;
			pData->Offset = 0;
			pData->Size = 0;
		}
	}
	free (search_path);
	return pData;
}

/*
 * GetDataItem
 * -------------------------------------------------------------------------- */
char *HIDGetDataItem(const HIDData_t *hiddata, usage_tables_t *utab)
{
	return NULL;
}

/*
 * HIDGetDataValue
 * -------------------------------------------------------------------------- */
int HIDGetDataValue(hid_dev_handle_t udev, const char *hidpath, double *Value, int age)
{
	char *string;
	int ret = 0;
	char *search_path;

	/* Refresh data */
	if (get_ups_properties() == -1)
		return -1;

	search_path = xstrdup(hidpath);
	FixPathName(search_path);

	upsdebugx(2, "HIDGetDataValue(%s)", search_path);

	/* Get the object */
	if ( (string = get_object_value(ups_properties->xmlUpsProp, "<OBJECT>", "name", search_path)) != NULL) {

		*Value = strtod(string, NULL);
		ret = 1; /* object found */
	}
	
	free (search_path);
	return ret;
}

/*
 * HIDSetDataValue
 * -------------------------------------------------------------------------- */
int HIDSetDataValue(hid_dev_handle_t udev, HIDData_t *hiddata, double Value)
{
	return 0;
}

/*
 * HIDGetIndexString
 * -------------------------------------------------------------------------- */
char *HIDGetIndexString(hid_dev_handle_t udev, int Index, char *buf, size_t buflen)
{
	return NULL;
}

/*
 * HIDGetEvents
 * -------------------------------------------------------------------------- */
int HIDGetEvents(hid_dev_handle_t udev, HIDData_t **event, int eventlen)
{
	/* FIXME: implement using <ALARMS><SUBSCRIPTION url=""/> or <BROADCAST> */
	return 0;
}

/*
 * Support functions
 * -------------------------------------------------------------------------- */
void HIDDumpTree(hid_dev_handle_t udev, usage_tables_t *utab)
{
	/* FIXME: implement an iterator on upsprop.xml */
}

const char *HIDDataType(const HIDData_t *hiddata)
{
	switch (hiddata->Type)
	{
	case ITEM_FEATURE:
		return "Feature";
	case ITEM_INPUT:
		return "Input";
	case ITEM_OUTPUT:
		return "Output";
	default:
		return "Unknown";
	}
}

void free_report_buffer(reportbuf_t *rbuf){
}

reportbuf_t *new_report_buffer(HIDDesc_t *pDesc)
{
	return NULL;
}

void Free_ReportDesc(HIDDesc_t *pDesc) {
}

HIDData_t *FindObject_with_Path(HIDDesc_t *pDesc, HIDPath_t *Path, u_char Type)
{
	return NULL;
}

/**********************************************************************
 * HTTP Network handling code 
 **********************************************************************/

static void tcp_close(void)
{
	if (upsfd < 1) {
		return;
	}

	close(upsfd);
}

static int tcp_connect(void)
{
	int ret = -1;
	struct hostent *serv;
	
	/* If device is still open, close it */
	tcp_close();

	/* Resolve the agent address if needed */
	if (agent_sock == NULL) {
		serv = gethostbyname(device_path);
		if (serv == NULL) {
			struct  in_addr listenaddr;

			if (!inet_aton(device_path, &listenaddr))
				return -1;

		  serv = gethostbyaddr(&listenaddr, sizeof(listenaddr), AF_INET);

		  if (serv == NULL)
		    return -1;
		}

		/* initialize network port */
		if ((upsfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			upsdebug_with_errno(3, "libmge_xml_open: socket");
			return -1;
		}

		/* initialise communication */
		/* FIXME: use fcntl (!blocking sock) + select */

		agent_sock = xmalloc(sizeof(struct sockaddr_in));
		memset(agent_sock, '\0', sizeof(struct sockaddr_in));
		agent_sock->sin_family = AF_INET;
		agent_sock->sin_port = htons(DEFAULT_PORT);
		memcpy(&agent_sock->sin_addr, serv->h_addr, serv->h_length);
		if (connect(upsfd, (struct sockaddr *) agent_sock, sizeof(struct sockaddr_in)) == -1) {
			upsdebug_with_errno(3, "libmge_xml_open: failed to connect on port %i", DEFAULT_PORT);
			close(upsfd);
			upsfd = -1;
			/* Try the alternate port */
			agent_sock->sin_port = htons(ALT_PORT);
			if (connect(upsfd, (struct sockaddr *) agent_sock, sizeof(struct sockaddr_in)) == -1) {
			upsdebug_with_errno(3, "libmge_xml_open: failed to connect on port %i", ALT_PORT);
				close(upsfd);
				upsfd = -1;
				return -1;
			}
			else {
				comport = ALT_PORT;
				ret = 1;
			}
		}
		else {
			comport = DEFAULT_PORT;
			ret = 1;
		}
	}
	else {
		/* initialize network port */
		if ((upsfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			upsdebug_with_errno(3, "libmge_xml_open: socket");
			return -1;
		}

		if (connect(upsfd, (struct sockaddr *) agent_sock, sizeof(struct sockaddr_in)) == -1) {
			close(upsfd);
			upsfd = -1;
			ret = -1;
		}
		else
			ret = 1;
	}
	
	if (ret != -1)	
		upsdebugx(2, "Connected on host %s, port %i (fd %i)", device_path, ntohs(agent_sock->sin_port), upsfd);

	return ret;
}

/* return the number of bytes received, or -1 */
static int tcp_receive(char *buf)
{
  int ret = 1;					/* non-null value */
  unsigned char c = 1;	/* non-null value */
	int bufPos = 0;

	if (buf == NULL)
		return -1;

	/* FIXME: Check data availability using select */

	upsdebugx(2, "tcp_receive");
	
  while(ret > 0 && c > 0)
  {
    bufPos = 0;
    while(ret > 0 && c > 0 && bufPos < (BUFF_SIZE - 1))
    {
      ret = recv(upsfd, (char*)&c, 1, 0);
      if(ret > 0)
      {
        buf[bufPos] = c;
        bufPos ++;
      }
    }
    /* Make sure the string is null terminated before concatenation */
    buf[bufPos] = '\0';
  }
	return bufPos;
}

char *getHTTP(const char* url, const char* login, const char* password)
{
	int rqlen, err;
	int totalLength, xmllen = -1;
	char *auth_string = "";
	char request[BUFF_SIZE];
	char *answer = NULL, tmpbuf[BUFF_SIZE];

	/* Handle authentication information */
/*	if(login != NULL && password != NULL)
	{
		auth_string = encode64(String::Format("%s:%s", login, password));
	}
*/
		
	tcp_connect ();
	
	if (upsfd < 1) {
		upsdebugx(2, "getHTTP: socket (%i) is NULL", upsfd);
		return NULL;
	}

	upsdebugx(2, "getHTTP: socket is %i", upsfd);
		
	sprintf(request, "GET /%s HTTP/1.0\r\n"
					"Content-type: text/plain\r\n"
					"User-Agent: XMLClient\r\n"
					"Authorization: Basic %s\r\n"
					"\r\n",
					url, auth_string);

	rqlen = strlen (request);

	upsdebugx(4, "request %s", request);
	
	if( (err = send(upsfd, request, rqlen, 0)) > 0)
	{
		int count = tcp_receive(tmpbuf);

		upsdebugx(3, "=> request sent:\n%s", request);
		
		if (count > 0) {
			int http_code = atoi(strchr(tmpbuf, ' '));

			upsdebugx(4, "==> answer received (size: %i, code: %i):\n%s ", count, http_code, tmpbuf);
			
			/* Check for server answer */
			if(http_code < 400) {

				/* extract the XML content... */
				totalLength = count;
				/* ... using method 1: Content-Length field of the HTTP header */
				if (strstr(tmpbuf, "Content-Length: ") != NULL)
					xmllen = atoi(strstr(tmpbuf, "Content-Length: ")+16);
				else {
					/* or method 2: search for the first HTML/XML starting tag */
					while (xmllen++ < totalLength) {
						if ( tmpbuf[xmllen] == '<') {
							xmllen = totalLength - xmllen;
							break;
						}
					}

					/* if nothing was found, reset xmllen! */
					if ( xmllen >= totalLength )
						xmllen = -1;
				}

				if (xmllen != -1) {
					upsdebugx(4, "totalLength: %i, data len: %i", totalLength, xmllen);

					answer = xmalloc(xmllen + 1);
					memcpy (answer, tmpbuf + (totalLength - xmllen), xmllen);
				}
				else
					upsdebugx(3, "can't find XML or HTTP content in answer (%s)!", tmpbuf);
			}
			else
				upsdebugx(3, "HTTP answer with error (err: %i)!", http_code);
		}
		else
			upsdebugx(3, "==> no answer received (err: %i, errno: %s)!", count, strerror(errno));
	}
	else
		upsdebugx(2, "error while sending request (err: %i, errno: %s)!", err, strerror(errno));

	/*	tcp_close(); => Don't close connection since it results in select() issues */ 

	return answer;
}
