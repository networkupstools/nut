dnl Check for LIBSSL compiler flags. On success, set nut_have_libssl="yes"
dnl and set LIBSSL_CFLAGS and LIBSSL_LDFLAGS. On failure, set
dnl nut_have_libssl="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBSSL], 
[
if test -z "${nut_have_libssl_seen}"; then
   nut_have_libssl_seen=yes

   AC_MSG_CHECKING(for SSL library availability)

   dnl save CFLAGS and LDFLAGS
   CFLAGS_ORIG="${CFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   CFLAGS=""
   LDFLAGS="-lssl -lcrypto"

   AC_TRY_LINK([#include <openssl/ssl.h>], [SSL_library_init()], 
	       nut_have_libssl=yes, 
	       nut_have_libssl=no)

   if test "${nut_have_libssl}" != "yes"; then
      CFLAGS="-I/usr/kerberos/include"
      LDFLAGS="-lssl -lcrypto"

      AC_TRY_LINK([#include <openssl/ssl.h>], [SSL_library_init], 
                   nut_have_libssl=yes, 
		   nut_have_libssl=no)
   fi

   if test "${nut_have_libssl}" != "yes"; then
      CFLAGS="-I/usr/local/ssl/include"
      LDFLAGS="-L/usr/local/ssl/lib -lssl -lcrypto"

      AC_TRY_LINK([#include <openssl/ssl.h>], [SSL_library_init], 
                  nut_have_libssl=yes, 
		  nut_have_libssl=no)
   fi

   if test "${nut_have_libssl}" != "yes"; then
      CFLAGS="-I/usr/local/ssl/include -I/usr/kerberos/include"
      LDFLAGS="-L/usr/local/ssl/lib -lssl -lcrypto"

      AC_TRY_LINK([#include <openssl/ssl.h>], [SSL_library_init], 
                   nut_have_libssl=yes, 
		   nut_have_libssl=no)
   fi

   if test "${nut_have_libssl}" = "yes"; then
	LIBSSL_CFLAGS="${CFLAGS}"
	LIBSSL_LDFLAGS="${LDFLAGS}"
   fi

   dnl restore original CFLAGS and LDFLAGS
   CFLAGS="${CFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"

   AC_MSG_RESULT([${nut_have_libssl}])

fi
])

