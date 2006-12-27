dnl Check for an unsigned 8-bit integer type

AC_DEFUN([NUT_TYPE_UINT8_T],
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
