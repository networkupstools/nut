dnl Check for an unsigned 16-bit integer type

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
