/* dmfcore.c - The core of generic DMF support, allowing to parse data
 * from DMF documents with LibNEON (possibly LTDLed just for this purpose).
 *
 * This file implements procedures to load the LibNEON library if needed
 * and to request a parse of an in-memory string, a single file, or of
 * all '*.dmf' files in a named directory.
 *
 * For general developer reference on LTDL see
 * https://www.gnu.org/software/libtool/manual/html_node/Libltdl-interface.html
 *
 * Copyright (C) 2016 Carlos Dominguez <CarlosDominguez@eaton.com>
 * Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
 * Copyright (C) 2016 - 2017 Jim Klimov <EvgenyKlimov@eaton.com>
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

#include <errno.h>
#include <dirent.h>
#include <assert.h>

#ifdef HAVE_LIBXML_PARSER_FUNC
#undef HAVE_LIBXML_PARSER_FUNC
#endif
#ifdef HAVE_LIBXML_ENCODING_FUNC
#undef HAVE_LIBXML_ENCODING_FUNC
#endif

#if WITH_LIBLTDL
# include <ltdl.h>
#else
/* else: We are linked to LibNEON at compile-time */
# if HAVE_LIBXML_PARSER_H
#    include <libxml/parser.h>
#    define HAVE_LIBXML_PARSER_FUNC 1
# endif
# if HAVE_LIBXML_ENCODING_H
#    include <libxml/encoding.h>
#    define HAVE_LIBXML_ENCODING_FUNC 1
# endif
#endif

#ifndef HAVE_LIBXML_PARSER_FUNC
#define HAVE_LIBXML_PARSER_FUNC 0
#endif
#ifndef HAVE_LIBXML_ENCODING_FUNC
#define HAVE_LIBXML_ENCODING_FUNC 0
#endif

#include "common.h"
#include "dmfcore.h"

/*
 *
 *  C FILE
 *
 */

#if WITH_LIBLTDL
/* LTDL variables needed to load LibNEON */
static lt_dlhandle dl_handle_libneon = NULL;
static const char *dl_error = NULL;

/* Pointers to dynamically-loaded LibNEON functions; do not mistake these
 * with xml_*_cb callbacks that we implement in dmfcore_parser_t for actual
 * parsing. If not loaded dynamically by LTDL, these should be available
 * via classic LDD dynamic linking at compile-time.
 */
static ne_xml_parser *(*xml_create)(void);
static void (*xml_push_handler)(ne_xml_parser*,
			ne_xml_startelm_cb*,
			ne_xml_cdata_cb*,
			ne_xml_endelm_cb*,
			void*);
static int (*xml_parse)(ne_xml_parser*, const char*, size_t);
static void (*xml_destroy)(ne_xml_parser*);
static void (*xml_init)(void);
static void (*xml_uninit)(void);
static void (*xml_uninitenc)(void);
#else
# define	xml_create		ne_xml_create
# define	xml_push_handler	ne_xml_push_handler
# define	xml_parse		ne_xml_parse
# define	xml_destroy		ne_xml_destroy
# if HAVE_LIBXML_PARSER_FUNC
#  define	xml_init		xmlInitParser
#  define	xml_uninit		xmlCleanupParser
# else
#  define	xml_init		((void (*)(void))NULL)
#  define	xml_uninit		((void (*)(void))NULL)
# endif
# if HAVE_LIBXML_ENCODING_FUNC
#  define	xml_uninitenc	xmlCleanupCharEncodingHandlers
# else
#  define	xml_uninitenc	((void (*)(void))NULL)
# endif
#endif

/* Library-loader returns OK if all went well, or ERR on error (including
 * a build without NEON support enabled */
/* Based on nut-scanner/scan_xml_http.c code */
/* Note: names for popular libneon implementations are hard-coded at this
 * time - and just one, so extra DLLs (if an OS requires several to load)
 * are not supported. (TODO: link this to LIBNEON_LIBS from configure) */
int load_neon_lib(void){
#ifdef WITH_NEON
# if WITH_LIBLTDL
	char *neon_libname_path = get_libname("libneon.so");
	int lt_dlinit_succeeded = 0;

	upsdebugx(1, "load_neon_lib(): neon_libname_path = %s", neon_libname_path);
	if(!neon_libname_path) {
		upslogx(LOG_NOTICE, "Error loading Neon library required for DMF: %s not found by dynamic loader; please verify it is in your /usr/lib or some otherwise searched dynamic-library path", "libneon.so");

		neon_libname_path = get_libname("libneon-gnutls.so");
		upsdebugx(1, "load_neon_lib(): neon_libname_path = %s", neon_libname_path);
		if(!neon_libname_path) {
			upslogx(LOG_ERR, "Error loading Neon library required for DMF: %s not found by dynamic loader; please verify it is in your /usr/lib or some otherwise searched dynamic-library path", "libneon-gnutls.so");
			return ERR;
		}
	}

	if( lt_dlinit() != 0 ) {
		/* FIXME : Make the search for candidate library names restartable,
		 * so if we hit a bad filename, it is not instantly the end of road.
		 * Applies here and below, where we check for symbols in the lib.
		 */
		upsdebugx(1, "load_neon_lib(): lt_dlinit() action failed");
		goto err;
	}
	lt_dlinit_succeeded = 1;

	if( dl_handle_libneon != NULL ) {
		/* if previous init failed */
		if( dl_handle_libneon == (void *)1 ) {
			upsdebugx(1, "load_neon_lib(): previous ltdl engine init had failed");
			goto err;
		}
		/* init has already been done and not unloaded yet */
		free(neon_libname_path);
		return OK;
	}

	dl_handle_libneon = lt_dlopen(neon_libname_path);

	if(!dl_handle_libneon) {
		dl_error = lt_dlerror();
		upsdebugx(1, "load_neon_lib(): lt_dlopen() action failed");
		goto err;
	}

	lt_dlerror();      /* Clear any existing error */

	*(void**) (&xml_create) = lt_dlsym(dl_handle_libneon, "ne_xml_create");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_create) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s()", "xml_create");
		goto err;
	}

	*(void**) (&xml_push_handler) = lt_dlsym(dl_handle_libneon, "ne_xml_push_handler");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_push_handler) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s()", "xml_push_handler");
		goto err;
	}

	*(void**) (&xml_parse) = lt_dlsym(dl_handle_libneon, "ne_xml_parse");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_parse) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s()", "xml_parse");
		goto err;
	}

	*(void**) (&xml_destroy) = lt_dlsym(dl_handle_libneon, "ne_xml_destroy");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_destroy) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s()", "xml_destroy");
		goto err;
	}

	/* These three are not in libneon, but in libxml2 through it. We only need
	 * them for graceful cleanup, so no big deal if missing (minor leak in case
	 * of libxml, unknown effect if neon is backed by something else). These
	 * are also usually not needed explicitly when linked persistently, since
	 * the shared library unload should free these resources; for the headers
	 * and func pointers to do work, consumers may have to link with "-lxml2".
	 */
	*(void**) (&xml_init) = lt_dlsym(dl_handle_libneon, "xmlInitParser");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_init) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s() - SKIPPED", "xml_init");
		xml_init = NULL;
	}

	*(void**) (&xml_uninit) = lt_dlsym(dl_handle_libneon, "xmlCleanupParser");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_uninit) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s() - SKIPPED", "xml_uninit");
		xml_uninit = NULL;
	}

	*(void**) (&xml_uninitenc) = lt_dlsym(dl_handle_libneon, "xmlCleanupCharEncodingHandlers");
	if ( ((dl_error = lt_dlerror()) != NULL) || (!xml_uninitenc) ) {
		upsdebugx(1, "load_neon_lib(): lt_dlsym() action failed to find %s() - SKIPPED", "xml_uninitenc");
		xml_uninitenc = NULL;
	}

	dl_error = lt_dlerror();
	if (dl_error) {
		upsdebugx(1, "load_neon_lib(): lt_dlerror() final check failed");
		goto err;
	}
	else {
		upsdebugx(1, "load_neon_lib(): lt_dlerror() final succeeded, library loaded");
		free(neon_libname_path);
		if (xml_init != NULL) {
			upsdebugx(1, "load_neon_lib(): calling xmlInitParser()");
			xml_init();
		}
		return OK;
	}

err:
	upslogx(LOG_ERR, "Error loading Neon library %s required for DMF: %s",
		neon_libname_path,
		dl_error ? dl_error : "No details passed");
	free(neon_libname_path);
	if (lt_dlinit_succeeded)
		lt_dlexit();
	return ERR;
# else /* not WITH_LIBLTDL */
	upsdebugx(1, "load_neon_lib(): no-op because ltdl was not enabled during compilation,\nusual dynamic linking should be in place instead");
	if (xml_init != NULL) {
		upsdebugx(1, "load_neon_lib(): calling xmlInitParser()");
		xml_init();
	}
	return OK;
# endif /* WITH_LIBLTDL */

#else /* not WITH_NEON */
	upslogx(LOG_ERR, "Error loading Neon library required for DMF: not enabled during compilation");
	upsdebugx(1, "load_neon_lib(): not enabled during compilation");
	return ERR;
#endif /* WITH_NEON */
}

void unload_neon_lib(){
#ifdef WITH_NEON
	if (xml_uninitenc != NULL) {
		upsdebugx(1, "unload_neon_lib(): calling xmlCleanupCharEncodingHandlers()");
		xml_uninitenc();
	}
	if (xml_uninit != NULL) {
		upsdebugx(1, "unload_neon_lib(): calling xmlCleanupParser()");
		xml_uninit();
	}
#if WITH_LIBLTDL
	upsdebugx(1, "unload_neon_lib(): unloading the library");
	lt_dlclose(dl_handle_libneon);
	dl_handle_libneon = NULL;
	lt_dlexit();
#endif /* WITH_LIBLTDL */
#endif /* WITH_NEON */
}


/* calloc() a new DCP object and set its fields to NULLs.
 * There is no destroyer because this object only holds references to
 * other objects or functions whole lifecycle is managed by caller.
 * However, this DCP object should also be free()d by the caller now.
 * The caller should populate DCP with callback functions and pointer to
 * the format-specific parsed_data structure, and pass to dmfcore_parse*().
 */
dmfcore_parser_t*
dmfcore_parser_new()
{
	dmfcore_parser_t *self = (dmfcore_parser_t *) calloc (1, sizeof (dmfcore_parser_t));
	assert (self);
	return self;
}

dmfcore_parser_t*
dmfcore_parser_new_init(
	void *parsed_data,
	int (*dmf_parse_begin_cb) (void*),
	int (*dmf_parse_finish_cb) (void*, int),
	ne_xml_startelm_cb* xml_dict_start_cb,
	ne_xml_cdata_cb* xml_cdata_cb,
	ne_xml_endelm_cb* xml_end_cb)
{
	dmfcore_parser_t *self = dmfcore_parser_new();
	self->parsed_data = parsed_data;
	self->dmf_parse_begin_cb = dmf_parse_begin_cb;
	self->dmf_parse_finish_cb = dmf_parse_finish_cb;
	self->xml_dict_start_cb = xml_dict_start_cb;
	self->xml_cdata_cb = xml_cdata_cb;
	self->xml_end_cb = xml_end_cb;
	return self;
}

/* Load DMF XML file into structure tree at *list (precreate with alist_new)
 Returns 0 on success, or an <errno> code on system or parsing errors*/
int
dmfcore_parse_file(char *file_name, dmfcore_parser_t *dcp)
{
	char buffer[4096]; /* Align with common cluster/FSblock size nowadays */
	FILE *f;
	int result = 0;
#if WITH_LIBLTDL
	int flag_libneon = 0;
#endif /* WITH_LIBLTDL */

	upsdebugx(1, "%s(%s)", __func__, file_name);

	assert (file_name);
	assert (dcp);
	assert (dcp->parsed_data);
	if (dcp->dmf_parse_begin_cb != NULL)
		if ( dcp->dmf_parse_begin_cb(dcp->parsed_data) == ERR ) {
			upslogx(LOG_ERR, "ERROR parsing DMF from '%s' "
				"(can not initialize)", file_name);
			return ECANCELED;
		}
/* The caller is expected to do something like this :
	dmp = mydcp->parsed_data;
	mibdmf_parser_new_list(dmp);
	assert (mibdmf_get_aux_list(dmp)!=NULL);
*/
	assert (dcp->xml_dict_start_cb);
	assert (dcp->xml_cdata_cb);
	assert (dcp->xml_end_cb);

	if ( (file_name == NULL ) || \
	     ( (f = fopen(file_name, "r")) == NULL ) )
	{
		upsdebugx(1, "ERROR: DMF file '%s' not found or not readable",
			file_name ? file_name : "<NULL>");
		return ENOENT;
	}

#if WITH_LIBLTDL
	/* Library could be loaded by the caller like the directory
	 * parser - do not unload it then in the end of single-file work */
	if(!dl_handle_libneon){
#endif /* WITH_LIBLTDL */
		/* Note: do not "die" from the library context; that's up to the caller */
		if(load_neon_lib() == ERR) return ERR; /* Errors printed by that loader */
#if WITH_LIBLTDL
		flag_libneon = 1;
	}
#endif /* WITH_LIBLTDL */

	ne_xml_parser *parser = xml_create ();
	xml_push_handler (parser, dcp->xml_dict_start_cb,
		dcp->xml_cdata_cb
		, dcp->xml_end_cb, dcp->parsed_data);

	/* The neon XML parser would get blocks from the DMF file and build
	   the in-memory representation with our xml_dict_start_cb() callback.
	   Any hiccup (FS, neon, callback) is failure. */
	while (!feof (f))
	{
		size_t len = fread(buffer, sizeof(char), sizeof(buffer), f);
		if (len == 0) /* Should not zero-read from a non-EOF file */
		{
			upslogx(LOG_ERR, "ERROR parsing DMF from '%s' "
				"(unexpected short read)", file_name);
			result = EIO;
			break;
		} else {
			if ((result = xml_parse (parser, buffer, len)))
			{
				upslogx(LOG_ERR, "ERROR parsing DMF from '%s' "
					"(unexpected markup?)\n", file_name);
				result = ENOMSG;
				break;
			}
		}
	}
	fclose (f);
	if (!result) /* no errors, complete the parse with len==0 call */
		xml_parse (parser, buffer, 0);
	xml_destroy (parser);

#if WITH_LIBLTDL
	if(flag_libneon == 1)
#endif /* WITH_LIBLTDL */
		unload_neon_lib();

	upsdebugx(1, "%s DMF acquired from '%s' (result = %d) %s",
		( result == 0 ) ? "[--OK--]" : "[-FAIL-]", file_name, result,
		( result == 0 ) ? "" : strerror(result)
	);

/* The format-specific caller may want to ensure the last entry in the loaded
 * table is the zeroed-out sentinel after getting a non-zero return value */
	if (dcp->dmf_parse_finish_cb != NULL)
		result = dcp->dmf_parse_finish_cb(dcp->parsed_data, result);

	return result;
}

/* Parse a buffer with complete DMF XML (from <nut> to </nut> for DMF MIB) */
int
dmfcore_parse_str (const char *dmf_string, dmfcore_parser_t *dcp)
{
	int result = 0;
	size_t len;
#if WITH_LIBLTDL
	int flag_libneon = 0;
#endif /* WITH_LIBLTDL */

	upsdebugx(1, "%s(string)", __func__);

	assert (dmf_string);
	assert (dcp);
	assert (dcp->parsed_data);
	if (dcp->dmf_parse_begin_cb != NULL)
		if ( dcp->dmf_parse_begin_cb(dcp->parsed_data) == ERR ) {
			upslogx(LOG_ERR, "ERROR parsing DMF from string "
				"(can not initialize)");
			return ECANCELED;
		}
/* The caller-defined callback is expected to do something like this :
	dmp = mydcp->parsed_data;
	mibdmf_parser_new_list(dmp);
	assert (mibdmf_get_aux_list(dmp)!=NULL);
*/
	assert (dcp->xml_dict_start_cb);
	assert (dcp->xml_cdata_cb);
	assert (dcp->xml_end_cb);

	if ( (dmf_string == NULL ) || \
	     ( (len = strlen(dmf_string)) == 0 ) )
	{
		upslogx(LOG_ERR, "ERROR: DMF passed in a string is empty or NULL");
		return ENOENT;
	}

#if WITH_LIBLTDL
	/* Library could be loaded by the caller - so do not unload it then in the
	 * end of single-string work */
	if(!dl_handle_libneon){
#endif /* WITH_LIBLTDL */
		/* Note: do not "die" from the library context; that's up to the caller */
		if(load_neon_lib() == ERR) return ERR; /* Errors printed by that loader */
#if WITH_LIBLTDL
		flag_libneon = 1;
	}
#endif /* WITH_LIBLTDL */

	ne_xml_parser *parser = xml_create ();
	xml_push_handler (parser, dcp->xml_dict_start_cb,
		dcp->xml_cdata_cb
		, dcp->xml_end_cb, dcp->parsed_data);

	if ((result = xml_parse (parser, dmf_string, len)))
	{
		upslogx(LOG_ERR, "ERROR parsing DMF from string "
			"(unexpected markup?)");
		result = ENOMSG;
	}

	if (!result) /* no errors, complete the parse with len==0 call */
		xml_parse (parser, dmf_string, 0);
	xml_destroy (parser);

#if WITH_LIBLTDL
	if(flag_libneon == 1)
#endif /* WITH_LIBLTDL */
		unload_neon_lib();

	upsdebugx(1, "%s DMF acquired from string (result = %d) %s",
		( result == 0 ) ? "[--OK--]" : "[-FAIL-]", result,
		( result == 0 ) ? "" : strerror(result)
	);

/* The format-specific caller may want to ensure the last entry in the loaded
 * table is the zeroed-out sentinel after getting a non-zero return value */
	if (dcp->dmf_parse_finish_cb != NULL)
		result = dcp->dmf_parse_finish_cb(dcp->parsed_data, result);

	return result;
}

/* Load all `*.dmf` DMF XML files from specified directory into aux list tree
 NOTE: Technically by current implementation, this is `*.dmf*`*/
int
dmfcore_parse_dir (char *dir_name, dmfcore_parser_t *dcp)
{
	struct dirent **dir_ent;
	int i = 0, x = 0, result = 0, n = 0;
#if WITH_LIBLTDL
	int flag_libneon = 0;
#endif /* WITH_LIBLTDL */

	upsdebugx(1, "%s(%s)", __func__, dir_name);

	assert (dir_name);
	assert (dcp);

	if ( (dir_name == NULL ) || \
	     ( (n = scandir(dir_name, &dir_ent, NULL, alphasort)) == 0 ) )
	{
		upslogx(LOG_ERR, "ERROR: DMF directory '%s' not found or not readable",
			dir_name ? dir_name : "<NULL>");
		return ENOENT;
	}

	if (n < 0) {
		result = errno;
		upslog_with_errno(LOG_ERR, "ERROR: DMF directory '%s' not found or not readable",
			dir_name ? dir_name : "<NULL>");
		return result;
	}

#if WITH_LIBLTDL
	/* Library could be loaded by the caller - so do not unload it then in the
	 * end of single-dir work */
	if(!dl_handle_libneon){
#endif /* WITH_LIBLTDL */
		/* Note: do not "die" from the library context; that's up to the caller */
		if(load_neon_lib() == ERR) return ERR; /* Errors printed by that loader */
#if WITH_LIBLTDL
		flag_libneon = 1;
	}
#endif /* WITH_LIBLTDL */

	upsdebugx(2, "Got %d entries to parse in directory %s", n, dir_name);

	int c;
	for (c = 0; c < n; c++)
	{
		upsdebugx (5, "dmfcore_parse_dir(): dir_ent[%d]->d_name=%s", c, dir_ent[c]->d_name);
		size_t fname_len = strlen(dir_ent[c]->d_name);
		if ( (fname_len > 4) &&
		     (strstr(dir_ent[c]->d_name + fname_len - 4, ".dmf") ||
		      strstr(dir_ent[c]->d_name + fname_len - 4, ".DMF") ) )
		{
			i++;
			if(strlen(dir_name) + strlen(dir_ent[c]->d_name) < PATH_MAX_SIZE){
				char *file_path = (char *) calloc(PATH_MAX_SIZE, sizeof(char));
				if (!file_path)
				{
					upslogx(LOG_ERR, "dmfcore_parse_dir(): calloc() failed");
				} else {
					sprintf(file_path, "%s/%s", dir_name, dir_ent[c]->d_name);
					assert(file_path);
					int res = dmfcore_parse_file(file_path, dcp);
					upsdebugx (5, "dmfcore_parse_file (\"%s\", <%p>)=%d", file_path, (void*)dcp, res);
					if ( res != 0 )
					{
						x++;
						result = res;
						/* No debug: parse_file() did it if enabled*/
					}
					free(file_path);
				}
			}else{
				upslogx(LOG_ERR, "dmfcore_parse_dir(): File path too long");
			}
		}
		free(dir_ent[c]);
	}
	free(dir_ent);

#if WITH_LIBLTDL
	if(flag_libneon == 1)
#endif /* WITH_LIBLTDL */
		unload_neon_lib();

	if (i==0) {
		upsdebugx(1, "WARN: No '*.dmf' DMF files were found or readable in directory '%s'",
			dir_name ? dir_name : "<NULL>");
	} else {
		upsdebugx(1, "INFO: %d '*.dmf' DMF files were inspected in directory '%s'",
			i, dir_name ? dir_name : "<NULL>");
	}
	if (result!=0 || x>0) {
		upsdebugx(1, "WARN: Some %d DMF files were not readable in directory '%s' (last bad result %d)",
			x, dir_name ? dir_name : "<NULL>", result);
	}

	return result;
}
