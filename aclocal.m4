dnl Check for socklen_t: historically on BSD it is an int, and in
dnl POSIX 1g it is a type of its own, but some platforms use different
dnl types for the argument to getsockopt, getpeername, etc.  So we
dnl have to test to find something that will work.

dnl This code gets around.  This instance came from rsync 2.5.6.

AC_DEFUN([TYPE_SOCKLEN_T],
[
   AC_CHECK_TYPE([socklen_t], ,[
      AC_MSG_CHECKING([for socklen_t equivalent])
      AC_CACHE_VAL([nut_cv_socklen_t_equiv],
      [
         # Systems have either "struct sockaddr *" or
         # "void *" as the second argument to getpeername
         nut_cv_socklen_t_equiv=
         for arg2 in "struct sockaddr" void; do
            for t in int size_t unsigned long "unsigned long"; do
               AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>

                  int getpeername (int, $arg2 *, $t *);
               ],[
                  $t len;
                  getpeername(0,0,&len);
               ],[
                  nut_cv_socklen_t_equiv="$t"
                  break
               ])
            done
         done

         if test "x$nut_cv_socklen_t_equiv" = x; then
            AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
         fi
      ])
      AC_MSG_RESULT($nut_cv_socklen_t_equiv)
      AC_DEFINE_UNQUOTED(socklen_t, $nut_cv_socklen_t_equiv,
			[type to use in place of socklen_t if not defined])],
      [#include <sys/types.h>
#include <sys/socket.h>])
])

AC_DEFUN([TYPE_UINT16_T],
[
   nut_type_includes="";
   AC_CHECK_HEADERS(stdint.h, [nut_type_includes="#include <stdint.h>"])
   AC_CHECK_TYPE(uint16_t, ,[
      AC_CHECK_TYPE(u_int16_t, [
         nut_cv_uint16_t_equiv="u_int16_t"
      ], [
         AC_CHECK_SIZEOF(unsigned short)
         if test "$ac_cv_sizeof_unsigned_short" -eq 2; then
            nut_cv_uint16_t_equiv="unsigned short"
         fi
      ], [$nut_type_includes])
      dnl Delay this message until after all the other tests:
      AC_MSG_CHECKING([for uint16_t replacement])
      AC_MSG_RESULT($nut_cv_uint16_t_equiv)
      if test -z "$nut_cv_uint16_t_equiv"; then
         AC_MSG_ERROR(cannot find a 16 bit integer type to replace uint16_t)
      fi
      AC_DEFINE_UNQUOTED(uint16_t, $nut_cv_uint16_t_equiv,
                           [type to use in place of uint16_t if not defined])
   ], [$nut_type_includes])
])

AC_DEFUN([TYPE_UINT8_T],
[
   nut_type_includes="";
   AC_CHECK_HEADERS(stdint.h, [nut_type_includes="#include <stdint.h>"])
   AC_CHECK_TYPE(uint8_t, ,[
      AC_CHECK_TYPE(u_int8_t, [
         nut_cv_uint8_t_equiv="u_int8_t"
      ], [
         AC_CHECK_SIZEOF(unsigned char)
         if test "$ac_cv_sizeof_unsigned_char" -eq 1; then
            nut_cv_uint8_t_equiv="unsigned char"
         fi
      ], [$nut_type_includes])
      dnl Delay this message until after all the other tests:
      AC_MSG_CHECKING([for uint8_t replacement])
      AC_MSG_RESULT($nut_cv_uint8_t_equiv)
      if test -z "$nut_cv_uint8_t_equiv"; then
         AC_MSG_ERROR(cannot find a 8 bit integer type to replace uint8_t)
      fi
      AC_DEFINE_UNQUOTED(uint8_t, $nut_cv_uint8_t_equiv,
                           [type to use in place of uint8_t if not defined])
   ], [$nut_type_includes])
])
