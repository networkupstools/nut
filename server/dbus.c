/* dbus.c - dbus communication functions

   Copyright (C) 2018 Emilien Kia <emilien.kia+dev@gmail.com>

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

#include "dbus.h"

#include "common.h"
#include "timehead.h"
#include "upstype.h"
#include "extstate.h"
#include "state.h"
#include "upsd.h"
#include "netset.h"

#include <dbus/dbus.h>
#include <string.h>
#include <stdlib.h>


static const char* empty_string = "";

/* Optimized building of string by multiple appending.
 */
typedef struct {
	char* str; /* Pointer to allocated buffer. */
	size_t size; /* Size of allocated buffer. */
	size_t len; /* Length of string. */
} str_builder;

static void str_builder_init(str_builder* strbld, unsigned char size)
{
	strbld->str  = (char*)xmalloc(size);
	strbld->size = size;
	strbld->len  = 0;
}

static void str_builder_init_str(str_builder* strbld, const char* str)
{
	strbld->len  = strbld->size = strlen(str);
	strbld->str  = xstrdup(str);
}

static void str_builder_free(str_builder* strbld)
{
	free(strbld->str);
	strbld->str = NULL;
	strbld->size = strbld->len = 0;
}

static void str_builder_append(str_builder* strbld, const char* str)
{
	size_t len = strlen(str);
	if (strbld->size < strbld->len+len+1) {
		strbld->size = (strbld->len+len)*2 + 1;
		strbld->str = xrealloc(strbld->str, strbld->size);
	}
	memcpy(strbld->str+strbld->len, str, len +1);
	strbld->len += len;
}

static void str_builder_append_args(str_builder* strbld, const char* fmt, ...)
{
	va_list ap;
	char buffer[LARGEBUF];
	
	va_start(ap, fmt);
	vsnprintf(buffer, LARGEBUF-1, fmt, ap);
	va_end(ap);
	
	str_builder_append(strbld, buffer);
}

/** DBus error management.*/
DBusError upsd_dbus_err;

/** DBus connection. */
DBusConnection* upsd_dbus_conn;



static void dbus_send_error_reply(DBusConnection *connection, DBusMessage *request, const char* errname, const char* errdesc) {
    DBusMessage *reply;
    reply = dbus_message_new_error(request, errname, errdesc);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
}

static void dbus_read_arg_basic (DBusMessageIter* iter, void* value) {
    DBusMessageIter variant;
    if(dbus_message_iter_get_arg_type(iter)==DBUS_TYPE_VARIANT) {
	dbus_message_iter_recurse (iter, &variant);
	dbus_message_iter_get_basic (&variant, value);
    } else {
	dbus_message_iter_get_basic (iter, value);
    }
}

static void dbus_add_variant_string(DBusMessageIter* iter, const char* value) {
    DBusMessageIter variant;
    
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant)) {
	printf("ERROR: dbus_add_variant_string dbus_message_iter_open_container (variant(s))");
	return;
    }
    if(!dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value)) {
	printf("ERROR: dbus_add_variant_string dbus_message_iter_append_basic(string)");
	return;
    }
    dbus_message_iter_close_container(iter, &variant);
}

static void dbus_add_dict_entry_string_variant_string(DBusMessageIter* iter, const char* name, const char* value) {
    DBusMessageIter entry, variant;
    
    /* Dict entry */
    if(!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL, &entry)) {
	printf("ERROR: dbus_add_dict_entry_string_variant_string dbus_message_iter_open_container entry");
	return;
    }

    /* Entry name */
    if(!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name)) {
	printf("ERROR: dbus_add_dict_entry_string_variant_string dbus_message_iter_append_basic name");
	// TODO close container
	return;
    }

    /* Entry value variant. */
    if(!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant)) {
	printf("ERROR: dbus_add_dict_entry_string_variant_string dbus_message_iter_open_container variant");
	// TODO close container
	return;
    }
    
    /* Entry value variant value. */
    if(!dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &value)) {
	printf("ERROR: dbus_add_dict_entry_string_variant_string dbus_message_iter_append_basic value");
	// TODO close containers
	return;
    }
    
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(iter, &entry);
}

static const char *server_introspection_data_begin =
" <!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \n"
"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
" <node>\n"
"   <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"     <method name=\"Introspect\"><arg name=\"data\" direction=\"out\" type=\"s\" /></method>\n"
"   </interface>\n";
static const char *server_introspection_node_data = "   <node name=\"%s\" />\n";
static const char *server_introspection_data_end = " </node>\n";


static void dbus_respond_to_server_introspect(DBusConnection *connection, DBusMessage *request) {
	DBusMessage *reply;
	str_builder buffer;
	upstype_t *ups;

	str_builder_init_str(&buffer, server_introspection_data_begin);
	ups = firstups;
	while (ups) {
		str_builder_append_args(&buffer, server_introspection_node_data, ups->name);
		ups = ups->next;
	}
	str_builder_append(&buffer, server_introspection_data_end);

	reply = dbus_message_new_method_return(request);
	dbus_message_append_args(reply, DBUS_TYPE_STRING, &buffer.str, DBUS_TYPE_INVALID);
	dbus_connection_send(connection, reply, NULL);
	dbus_message_unref(reply);
	str_builder_free(&buffer);
}

static const char *device_introspection_data_begin =
	" <!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \n"
	"\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
	" <node>\n"
	"   <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
	"     <method name=\"Introspect\"><arg name=\"data\" direction=\"out\" type=\"s\" /></method>\n"
	"   </interface>\n"
	"   <interface name=\"org.freedesktop.DBus.Properties\">\n"
	"     <method name=\"Get\">\n"
	"       <arg name=\"interface_name\" direction=\"in\" type=\"s\" />\n"
	"       <arg name=\"property_name\" direction=\"in\" type=\"s\" />\n"
	"       <arg name=\"value\" direction=\"out\" type=\"v\" />\n"
	"     </method>\n"
	"     <method name=\"GetAll\">\n"
	"       <arg name=\"interface_name\" direction=\"in\" type=\"s\" />\n"
	"       <arg name=\"properties\" direction=\"out\" type=\"a{sv}\" />\n"
	"     </method>\n"
	"     <method name=\"Set\">\n"
	"       <arg name=\"interface_name\" direction=\"in\" type=\"s\" />\n"
	"       <arg name=\"property_name\" direction=\"in\" type=\"s\" />\n"
	"       <arg name=\"value\" direction=\"in\" type=\"v\" />\n"
	"     </method>\n"
	"   </interface>\n"
	"   <interface name=\"org.networkupstools.Device\">\n"
	"      <property name=\"name\" type=\"s\" access=\"read\" />\n"
	"      <property name=\"fn\" type=\"s\" access=\"read\" />\n"
	"      <property name=\"desc\" type=\"s\" access=\"read\" />\n";

static const char *device_introspection_prop_read = "      <property name=\"%s\" type=\"s\" access=\"read\" />\n";
static const char *device_introspection_prop_rw = "      <property name=\"%s\" type=\"s\" access=\"readwrite\" />\n";

static const char *device_introspection_data_end =
	"   </interface>\n"
	" </node>\n";



static void dbus_respond_to_device_introspect(DBusConnection *connection, DBusMessage *request, const char* device) {
    DBusMessage *reply;
    str_builder buffer;
    st_tree_t* info;
    upstype_t* ups = get_ups_ptr(device);
    if (ups==NULL) {
	dbus_send_error_reply(connection, request, "unknwon_device", "Device name is not known");
	return;
    }
    
    str_builder_init_str(&buffer, device_introspection_data_begin);
    
    if(ups->inforoot!=NULL)
    {
	for (info = ups->inforoot; info!=NULL; info = info->left) {
	    if (info->flags & ST_FLAG_RW) {
		str_builder_append_args(&buffer, device_introspection_prop_rw, info->var);
	    } else {
		str_builder_append_args(&buffer, device_introspection_prop_read, info->var);
	    }
	}
	for (info = ups->inforoot->right; info!=NULL; info = info->right) {
	    if (info->flags & ST_FLAG_RW) {
		str_builder_append_args(&buffer, device_introspection_prop_rw, info->var);
	    } else {
		str_builder_append_args(&buffer, device_introspection_prop_read, info->var);
	    }
	}
    }
    str_builder_append(&buffer, device_introspection_data_end);
    
    reply = dbus_message_new_method_return(request);
    dbus_message_append_args(reply,
                 DBUS_TYPE_STRING, &buffer.str,
                 DBUS_TYPE_INVALID);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    str_builder_free(&buffer);
}

static void dbus_respond_to_device_get(DBusConnection *connection, DBusMessage *request, const char* device) {
    DBusMessage *reply;
    DBusMessageIter value;

    upstype_t* ups;
    st_tree_t *var;
    
    char *interface, *name;
    const char* val;
    
    ups = get_ups_ptr(device);
    if (ups==NULL)
    {
	dbus_send_error_reply(connection, request, DBUS_ERROR_UNKNOWN_OBJECT, "Device name is not known");
	return;
    }

    /* Read parameters. */
    dbus_message_get_args(request, &upsd_dbus_err, DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
    if (dbus_error_is_set(&upsd_dbus_err)) {
	dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Illegal arguments to " DBUS_INTERFACE_PROPERTIES "::Get(s,s)->v");
	return;
    }

    /* Create reply message. */
    reply = dbus_message_new_method_return(request);
    
    dbus_message_iter_init_append(reply, &value);
    
    if (0==strcmp(name, "name")) {
	dbus_add_variant_string(&value, ups->name);
    } else if (0==strcmp(name, "fn")) {
	dbus_add_variant_string(&value, ups->fn);
    } else if (0==strcmp(name, "desc")) {
	dbus_add_variant_string(&value, ups->desc);
    } else {
	/* Verify var presence. */
	var = state_tree_find(ups->inforoot, name);
	if (var==NULL) {
	    dbus_message_unref(reply);
	    dbus_send_error_reply(connection, request, DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown property name for " DBUS_INTERFACE_PROPERTIES "::Get(s,s)->v");
	    return;
	}
	dbus_add_variant_string(&value, var->val ? var->val : empty_string);
    }

    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
}

static void dbus_respond_to_device_set(DBusConnection *connection, DBusMessage *request, const char* device) {
    DBusMessage *reply;
    DBusMessageIter args;
    
    char *interface, *name, *val;
    
    upstype_t* ups;

    ups = get_ups_ptr(device);
    if (ups==NULL)
    {
	dbus_send_error_reply(connection, request, DBUS_ERROR_UNKNOWN_OBJECT, "Device name is not known");
	return;
    }

    /* Read parameters. */
    if (!dbus_message_iter_init (request, &args)) {
	dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Illegal arguments to " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
	return;
    }
    
    dbus_message_iter_get_basic (&args, &interface);
    
    if (!dbus_message_iter_next (&args)) {
	dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Illegal arguments count to " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
	return;
    }

    dbus_message_iter_get_basic (&args, &name);

    if (!dbus_message_iter_next (&args)) {
	dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Illegal arguments count to " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
	return;
    }

    dbus_read_arg_basic (&args, &val);

	/* Check variable new value. */
	switch(set_var_check_val(ups, name, val))
	{
		case SET_VAR_CHECK_VAL_VAR_NOT_SUPPORTED:
			dbus_send_error_reply(connection, request, DBUS_ERROR_UNKNOWN_PROPERTY, "Property not supported for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
			return;
		case SET_VAR_CHECK_VAL_READONLY:
			dbus_send_error_reply(connection, request, DBUS_ERROR_PROPERTY_READ_ONLY, "Property is read-only for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
			return;
		case SET_VAR_CHECK_VAL_SET_FAILED:
			dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Property value is malformed for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
			return;
		case SET_VAR_CHECK_VAL_TOO_LONG:
			dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Property value is too long for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
			return;
		case SET_VAR_CHECK_VAL_INVALID_VALUE:
			dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Property value is not valid enum value for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
			return;
		default:
			/* Do nothing, continue. */
			break;
	}

	/* Really do the set operation. */

	if (!do_set_var(ups, name, val)) {
		dbus_send_error_reply(connection, request, DBUS_ERROR_FAILED , "Failed to set new value for " DBUS_INTERFACE_PROPERTIES "::Set(s,s,v)");
		return;
	}

    reply = dbus_message_new_method_return(request);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
}

static void dbus_respond_to_device_getall(DBusConnection *connection, DBusMessage *request, const char* device) {
    DBusMessage *reply;
    DBusMessageIter args, array;
    
    char *interface;
    
    st_tree_t* info;
    upstype_t* ups = get_ups_ptr(device);
    if (ups==NULL)
    {
	dbus_send_error_reply(connection, request, DBUS_ERROR_UNKNOWN_OBJECT, "Device name is not known");
	return;
    }

    /* Read parameters. */
    dbus_message_get_args(request, &upsd_dbus_err, DBUS_TYPE_STRING, &interface, DBUS_TYPE_INVALID);
    if (dbus_error_is_set(&upsd_dbus_err)) {
	dbus_send_error_reply(connection, request, DBUS_ERROR_INVALID_ARGS, "Illegal arguments to " DBUS_INTERFACE_PROPERTIES "::GetAll(s)->a{sv}");
	return;
    }

    /* Create reply message. */
    reply = dbus_message_new_method_return(request);
    dbus_message_iter_init_append(reply, &args);
    if(!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &array)) {
	printf("ERROR: dbus_respond_to_device_getall dbus_message_iter_open_container arr");
	return;
    }
    
    if(ups->inforoot!=NULL)
    {
	for (info = ups->inforoot; info!=NULL; info = info->left) {
	    dbus_add_dict_entry_string_variant_string(&array, info->var, info->val);
	    // TODO handle errors
	}
	for (info = ups->inforoot->right; info!=NULL; info = info->right) {
	    dbus_add_dict_entry_string_variant_string(&array, info->var, info->val);
	    // TODO handle errors
	}
    }
    
    dbus_message_iter_close_container(&args, &array);
    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);

}

static void dbus_dump_message(DBusMessage* msg) {
	switch(dbus_message_get_type(msg))
	{
		case DBUS_MESSAGE_TYPE_METHOD_CALL:
			printf("method call : ");
			break;
		case  DBUS_MESSAGE_TYPE_METHOD_RETURN:
			printf("method return : ");
			break;
		case  DBUS_MESSAGE_TYPE_ERROR:
			printf("error : ");
			break;
		case  DBUS_MESSAGE_TYPE_SIGNAL:
			printf("signal : ");
			break;
		default:
			printf("other : ");
			break;
	}
	char ** path;
	if(dbus_message_get_path_decomposed(msg, &path)) {
		printf(" path=");
		for(char** p = path; *p != NULL; ++p) {
			printf("%s/", *p);
		}
		dbus_free_string_array(path);
	}
	printf(" interface=%s", dbus_message_get_interface(msg));
	printf(" member=%s", dbus_message_get_member(msg));
	printf(" destination=%s", dbus_message_get_destination(msg));
	printf(" sender=%s", dbus_message_get_sender(msg));
	printf(" signature=%s", dbus_message_get_signature(msg));
	printf("\n");
}


static DBusHandlerResult dbus_messages(DBusConnection *connection, DBusMessage *message, void *user_data) {
	// dbus_dump_message(message);
	
	const char *path = dbus_message_get_path(message);
	const char *interface_name = dbus_message_get_interface(message);
	const char *member_name = dbus_message_get_member(message);

	if (0==strcmp("/org/networkupstools/Upsd", path)) {
		/* Request on Upsd object itself. */
		if (0==strcmp(DBUS_INTERFACE_INTROSPECTABLE, interface_name) &&
			0==strcmp("Introspect", member_name)) {
			dbus_respond_to_server_introspect(connection, message);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	} else {
		/* Request on Upsd sub object (device). */
		char ** splitpath;
		if(dbus_message_get_path_decomposed(message, &splitpath)) {
			/* TODO Test splitpath */
			if (0==strcmp(DBUS_INTERFACE_INTROSPECTABLE, interface_name) &&
				0==strcmp("Introspect", member_name)) {
				dbus_respond_to_device_introspect(connection, message, splitpath[3]);
			} else if (0==strcmp(DBUS_INTERFACE_PROPERTIES, interface_name)) {
				if (0==strcmp("Get", member_name)) {
				    dbus_respond_to_device_get(connection, message, splitpath[3]);
				} else if (0==strcmp("GetAll", member_name)) {
				    dbus_respond_to_device_getall(connection, message, splitpath[3]);
				} else if (0==strcmp("Set", member_name)) {
				    dbus_respond_to_device_set(connection, message, splitpath[3]);
				}
			}
			dbus_free_string_array(splitpath);
			return DBUS_HANDLER_RESULT_HANDLED;
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void dbus_init()
{
	int ret;
	DBusObjectPathVTable vtable;
	
	/* initialise the errors */
	dbus_error_init(&upsd_dbus_err);
	
	/* connect to the bus */
	upsd_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &upsd_dbus_err);
	if (dbus_error_is_set(&upsd_dbus_err)) {
		fprintf(stderr, "Connection Error (%s)\n", upsd_dbus_err.message); 
		dbus_error_free(&upsd_dbus_err); 
	}
	if (NULL == upsd_dbus_conn) {
		exit(1); 
	}
	
	ret = dbus_bus_request_name(upsd_dbus_conn, "org.networkupstools.Upsd",
		DBUS_NAME_FLAG_REPLACE_EXISTING , &upsd_dbus_err);
	if (dbus_error_is_set(&upsd_dbus_err)) {
		fprintf(stderr, "Name Error (%s)\n", upsd_dbus_err.message);
		dbus_error_free(&upsd_dbus_err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
		exit(1);
	}
	
	vtable.message_function = dbus_messages;
	vtable.unregister_function = NULL;
	dbus_connection_try_register_fallback(upsd_dbus_conn,
		"/org/networkupstools/Upsd", &vtable, NULL, &upsd_dbus_err);
	if (dbus_error_is_set(&upsd_dbus_err)) {
		fprintf(stderr, "Object Error (%s)\n", upsd_dbus_err.message);
		dbus_error_free(&upsd_dbus_err);
		exit(1);
	}

}

void dbus_cleanup()
{
	/*dbus_connection_close(upsd_dbus_conn);*/
}

void dbus_loop()
{
	dbus_connection_read_write_dispatch(upsd_dbus_conn, 100);
}
