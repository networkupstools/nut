dnl # For the sake of portability, check if the system offers a "bool" or a
dnl # "bool_t" type, and "true"/"false" values for it, and try to determine
dnl # how exactly these are spelled and implemented. For consumers in the
dnl # codebase, this plays along with "includes/nut_bool.h" header.
dnl # Ideally the <stdbool.h> just exists and provides reasonable types and
dnl # values (all lower-case), per the standard, which we would just alias
dnl # as "nut_bool_t" and use.
dnl # See also https://en.cppreference.com/w/cpp/header/cstdbool for more
dnl # info about what should be available where per standard approach.

dnl # Copyright (C) 2024 by Jim Klimov <jimklimov+nut@gmail.com>

AC_DEFUN([NUT_CHECK_BOOL],
[
if test -z "${nut_check_bool_seen}"; then
    nut_check_bool_seen="yes"

    AC_MSG_NOTICE([Checking below whether bool types are provided by system headers and how])

    AC_LANG_PUSH([C++])
    AC_CHECK_HEADERS_ONCE([cstdbool])
    AC_LANG_POP([C++])

    AC_LANG_PUSH([C])

    dnl # Check for existing definition of boolean type (should be stdbool.h, but...)
    AC_CHECK_HEADERS_ONCE([stdbool.h])

    myINCLUDE_STDBOOL="
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#endif
"

    dnl # Below we would check for C99 built-in "_Bool", and for "bool",
    dnl # "boolean" and/or "bool_t" type names with various spellings
    dnl # that may come from headers like <stdbool.h>

    FOUND__BOOL_TYPE=""
    FOUND__BOOL_IMPLEM_STR=""
    dnl # _bool
    HAVE__BOOL_TYPE_LOWERCASE=false
    dnl # _BOOL
    HAVE__BOOL_TYPE_UPPERCASE=false
    dnl # _Bool
    HAVE__BOOL_TYPE_CAMELCASE=false

    FOUND_BOOL_TYPE=""
    FOUND_BOOL_IMPLEM_STR=""
    dnl # bool
    HAVE_BOOL_TYPE_LOWERCASE=false
    dnl # BOOL
    HAVE_BOOL_TYPE_UPPERCASE=false
    dnl # Bool
    HAVE_BOOL_TYPE_CAMELCASE=false

    FOUND_BOOLEAN_TYPE=""
    FOUND_BOOLEAN_IMPLEM_STR=""
    dnl # boolean
    HAVE_BOOLEAN_TYPE_LOWERCASE=false
    dnl # BOOLEAN
    HAVE_BOOLEAN_TYPE_UPPERCASE=false
    dnl # Boolean
    HAVE_BOOLEAN_TYPE_CAMELCASE=false

    FOUND_BOOL_T_TYPE=""
    FOUND_BOOL_T_IMPLEM_STR=""
    dnl # bool_t
    HAVE_BOOL_T_TYPE_LOWERCASE=false
    dnl # BOOL_T
    HAVE_BOOL_T_TYPE_UPPERCASE=false
    dnl # Bool_t?
    HAVE_BOOL_T_TYPE_CAMELCASE=false

    FOUND__BOOL_VALUE_TRUE=""
    FOUND__BOOL_VALUE_FALSE=""
    dnl # true/false
    HAVE__BOOL_VALUE_LOWERCASE=false
    dnl # TRUE/FALSE
    HAVE__BOOL_VALUE_UPPERCASE=false
    dnl # True/False
    HAVE__BOOL_VALUE_CAMELCASE=false

    FOUND_BOOL_VALUE_TRUE=""
    FOUND_BOOL_VALUE_FALSE=""
    HAVE_BOOL_VALUE_LOWERCASE=false
    HAVE_BOOL_VALUE_UPPERCASE=false
    HAVE_BOOL_VALUE_CAMELCASE=false

    FOUND_BOOLEAN_VALUE_TRUE=""
    FOUND_BOOLEAN_VALUE_FALSE=""
    HAVE_BOOLEAN_VALUE_LOWERCASE=false
    HAVE_BOOLEAN_VALUE_UPPERCASE=false
    HAVE_BOOLEAN_VALUE_CAMELCASE=false

    FOUND_BOOL_T_VALUE_TRUE=""
    FOUND_BOOL_T_VALUE_FALSE=""
    HAVE_BOOL_T_VALUE_LOWERCASE=false
    HAVE_BOOL_T_VALUE_UPPERCASE=false
    HAVE_BOOL_T_VALUE_CAMELCASE=false

    HAVE__BOOL_IMPLEM_MACRO=false
    HAVE__BOOL_IMPLEM_INT=false
    HAVE__BOOL_IMPLEM_ENUM=false

    HAVE_BOOL_IMPLEM_MACRO=false
    HAVE_BOOL_IMPLEM_INT=false
    HAVE_BOOL_IMPLEM_ENUM=false

    HAVE_BOOLEAN_IMPLEM_MACRO=false
    HAVE_BOOLEAN_IMPLEM_INT=false
    HAVE_BOOLEAN_IMPLEM_ENUM=false

    HAVE_BOOL_T_IMPLEM_MACRO=false
    HAVE_BOOL_T_IMPLEM_INT=false
    HAVE_BOOL_T_IMPLEM_ENUM=false

    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [_bool b])], [HAVE__BOOL_TYPE_LOWERCASE=true; FOUND__BOOL_TYPE="_bool"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [_BOOL b])], [HAVE__BOOL_TYPE_UPPERCASE=true; FOUND__BOOL_TYPE="_BOOL"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [_Bool b])], [HAVE__BOOL_TYPE_CAMELCASE=true; FOUND__BOOL_TYPE="_Bool"])

    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [bool b])], [HAVE_BOOL_TYPE_LOWERCASE=true; FOUND_BOOL_TYPE="bool"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [BOOL b])], [HAVE_BOOL_TYPE_UPPERCASE=true; FOUND_BOOL_TYPE="BOOL"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [Bool b])], [HAVE_BOOL_TYPE_CAMELCASE=true; FOUND_BOOL_TYPE="Bool"])

    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [boolean b])], [HAVE_BOOLEAN_TYPE_LOWERCASE=true; FOUND_BOOLEAN_TYPE="boolean"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [BOOLEAN b])], [HAVE_BOOLEAN_TYPE_UPPERCASE=true; FOUND_BOOLEAN_TYPE="BOOLEAN"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [Boolean b])], [HAVE_BOOLEAN_TYPE_CAMELCASE=true; FOUND_BOOLEAN_TYPE="Boolean"])

    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [bool_t b])], [HAVE_BOOL_T_TYPE_LOWERCASE=true; FOUND_BOOL_T_TYPE="bool_t"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [BOOL_T b])], [HAVE_BOOL_T_TYPE_UPPERCASE=true; FOUND_BOOL_T_TYPE="BOOL_T"])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [Bool_t b])], [HAVE_BOOL_T_TYPE_CAMELCASE=true; FOUND_BOOL_T_TYPE="Bool_t"])

    AS_IF([test x"${FOUND__BOOL_TYPE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND__BOOL_TYPE} bT = true, bF = false])], [HAVE__BOOL_VALUE_LOWERCASE=true; FOUND__BOOL_VALUE_TRUE="true"; FOUND__BOOL_VALUE_FALSE="false"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND__BOOL_TYPE} bT = TRUE, bF = FALSE])], [HAVE__BOOL_VALUE_UPPERCASE=true; FOUND__BOOL_VALUE_TRUE="TRUE"; FOUND__BOOL_VALUE_FALSE="FALSE"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND__BOOL_TYPE} bT = True, bF = False])], [HAVE__BOOL_VALUE_CAMELCASE=true; FOUND__BOOL_VALUE_TRUE="True"; FOUND__BOOL_VALUE_FALSE="False"])
    ])

    AS_IF([test x"${FOUND_BOOL_TYPE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_TYPE} bT = true, bF = false])], [HAVE_BOOL_VALUE_LOWERCASE=true; FOUND_BOOL_VALUE_TRUE="true"; FOUND_BOOL_VALUE_FALSE="false"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_TYPE} bT = TRUE, bF = FALSE])], [HAVE_BOOL_VALUE_UPPERCASE=true; FOUND_BOOL_VALUE_TRUE="TRUE"; FOUND_BOOL_VALUE_FALSE="FALSE"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_TYPE} bT = True, bF = False])], [HAVE_BOOL_VALUE_CAMELCASE=true; FOUND_BOOL_VALUE_TRUE="True"; FOUND_BOOL_VALUE_FALSE="False"])
    ])

    AS_IF([test x"${FOUND_BOOLEAN_TYPE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOLEAN_TYPE} bT = true, bF = false])], [HAVE_BOOLEAN_VALUE_LOWERCASE=true; FOUND_BOOLEAN_VALUE_TRUE="true"; FOUND_BOOL_VALUE_FALSE="false"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOLEAN_TYPE} bT = TRUE, bF = FALSE])], [HAVE_BOOLEAN_VALUE_UPPERCASE=true; FOUND_BOOLEAN_VALUE_TRUE="TRUE"; FOUND_BOOL_VALUE_FALSE="FALSE"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOLEAN_TYPE} bT = True, bF = False])], [HAVE_BOOLEAN_VALUE_CAMELCASE=true; FOUND_BOOLEAN_VALUE_TRUE="True"; FOUND_BOOL_VALUE_FALSE="False"])
    ])

    AS_IF([test x"${FOUND_BOOL_T_TYPE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_T_TYPE} b = true, bF = false])], [HAVE_BOOL_T_VALUE_LOWERCASE=true; FOUND_BOOL_T_VALUE_TRUE="true"; FOUND_BOOL_T_VALUE_FALSE="false"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_T_TYPE} b = TRUE, bF = FALSE])], [HAVE_BOOL_T_VALUE_UPPERCASE=true; FOUND_BOOL_T_VALUE_TRUE="TRUE"; FOUND_BOOL_T_VALUE_FALSE="FALSE"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [${FOUND_BOOL_T_TYPE} b = True, bF = False])], [HAVE_BOOL_T_VALUE_CAMELCASE=true; FOUND_BOOL_T_VALUE_TRUE="True"; FOUND_BOOL_T_VALUE_FALSE="False"])
    ])

    dnl # FIXME: Some more diligent checks for signed/unsigned int/char/...
    dnl #  type details? e.g. via sizeof, assignment to negatives, etc.
    AS_IF([test x"${FOUND__BOOL_TYPE}" = x && test x"${FOUND_BOOL_TYPE}" = x && test x"${FOUND_BOOLEAN_TYPE}" = x && test x"${FOUND_BOOL_T_TYPE}" = x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [int bT = true, bF = false])], [FOUND_BOOL_TYPE="int"; HAVE_BOOL_VALUE_LOWERCASE=true; FOUND_BOOL_VALUE_TRUE="true"; FOUND_BOOL_VALUE_FALSE="false"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [int bT = TRUE, bF = FALSE])], [FOUND_BOOL_TYPE="int"; HAVE_BOOL_VALUE_UPPERCASE=true; FOUND_BOOL_VALUE_TRUE="TRUE"; FOUND_BOOL_VALUE_FALSE="FALSE"])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [int bT = True, bF = False])], [FOUND_BOOL_TYPE="int"; HAVE_BOOL_VALUE_CAMELCASE=true; FOUND_BOOL_VALUE_TRUE="True"; FOUND_BOOL_VALUE_FALSE="False"])
    ])

    dnl # Assume there are only 3 implementation options we can discern here

    dnl ####################################################################

    AS_IF([test x"${FOUND__BOOL_TYPE}" != x && test x"${FOUND__BOOL_VALUE_TRUE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
#ifndef ${FOUND__BOOL_VALUE_TRUE}
#error "${FOUND__BOOL_VALUE_TRUE} is not a macro
#endif
#ifndef ${FOUND__BOOL_VALUE_FALSE}
#error "${FOUND__BOOL_VALUE_FALSE} is not a macro
#endif
    ])], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND__BOOL_TYPE} bT = ${FOUND__BOOL_VALUE_TRUE};
bT = 42
    ])], [HAVE__BOOL_IMPLEM_INT=true; FOUND__BOOL_IMPLEM="number"], [HAVE__BOOL_IMPLEM_ENUM=true; FOUND__BOOL_IMPLEM="enum"])
    ], [HAVE__BOOL_IMPLEM_MACRO=true; FOUND__BOOL_IMPLEM="macro"])

            dnl # Final check
            AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND__BOOL_TYPE} b = ${FOUND__BOOL_VALUE_TRUE};
int ret = 0;

if (!(!b == ${FOUND__BOOL_VALUE_FALSE})) ret = 1;
if (!(b  != ${FOUND__BOOL_VALUE_FALSE})) ret = 2;
if (!(!b != ${FOUND__BOOL_VALUE_TRUE}))  ret = 3;
if (!b) ret = 4;
if (ret)
    return ret
/* no ";return 0;" here - autoconf adds one */
            ])],
            [ dnl # All tests passed, remember this
                    AC_MSG_NOTICE([Detected a semantically usable "_Bool"-like type name ${FOUND__BOOL_TYPE} implemented as ${FOUND__BOOL_IMPLEM} with boolean values '${FOUND__BOOL_VALUE_TRUE}' and '${FOUND__BOOL_VALUE_FALSE}'])
                    AC_DEFINE_UNQUOTED([FOUND__BOOL_TYPE],        [${FOUND__BOOL_TYPE}], [C spelling of _Bool type])
                    AC_DEFINE_UNQUOTED([FOUND__BOOL_IMPLEM_STR],  ["${FOUND__BOOL_IMPLEM}"], [String spelling of _Bool type implementation])
                    AC_DEFINE_UNQUOTED([FOUND__BOOL_VALUE_TRUE],  [${FOUND__BOOL_VALUE_TRUE}], [C spelling of _Bool type true value])
                    AC_DEFINE_UNQUOTED([FOUND__BOOL_VALUE_FALSE], [${FOUND__BOOL_VALUE_FALSE}], [C spelling of _Bool type false value])

                    AS_IF([${HAVE__BOOL_TYPE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_TYPE_LOWERCASE], [1], [Name of ${FOUND__BOOL_TYPE} is defined as lower-case token])])
                    AS_IF([${HAVE__BOOL_TYPE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_TYPE_UPPERCASE], [1], [Name of ${FOUND__BOOL_TYPE} is defined as upper-case token])])
                    AS_IF([${HAVE__BOOL_TYPE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_TYPE_CAMELCASE], [1], [Name of ${FOUND__BOOL_TYPE} is defined as camel-case token])])

                    AS_IF([${HAVE__BOOL_VALUE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_VALUE_LOWERCASE], [1], [Boolean values of ${FOUND__BOOL_TYPE} are defined as lower-case tokens])])
                    AS_IF([${HAVE__BOOL_VALUE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_VALUE_UPPERCASE], [1], [Boolean values of ${FOUND__BOOL_TYPE} are defined as upper-case tokens])])
                    AS_IF([${HAVE__BOOL_VALUE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_VALUE_CAMELCASE], [1], [Boolean values of ${FOUND__BOOL_TYPE} are defined as camel-case tokens])])

                    AS_IF([${HAVE__BOOL_IMPLEM_INT}],   [AC_DEFINE_UNQUOTED([HAVE__BOOL_IMPLEM_INT],   [1], [Boolean type ${FOUND__BOOL_TYPE} is defined as a general-purpose number type (int)])])
                    AS_IF([${HAVE__BOOL_IMPLEM_ENUM}],  [AC_DEFINE_UNQUOTED([HAVE__BOOL_IMPLEM_ENUM],  [1], [Boolean type ${FOUND__BOOL_TYPE} is defined as an enum with specific values allowed])])
                    AS_IF([${HAVE__BOOL_IMPLEM_MACRO}], [AC_DEFINE_UNQUOTED([HAVE__BOOL_IMPLEM_MACRO], [1], [Boolean values of ${FOUND__BOOL_TYPE} are defined as a preprocessor macro (type/implem is questionable)])])
            ],
            [AC_MSG_NOTICE([Detected a "_Bool"-like type name ${FOUND__BOOL_TYPE}, but it was not semantically usable])])
    ], [AC_MSG_NOTICE([A "_Bool"-like type name or its useful values were not detected])])

    dnl ####################################################################

    AS_IF([test x"${FOUND_BOOL_TYPE}" != x && test x"${FOUND_BOOL_VALUE_TRUE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
#ifndef ${FOUND_BOOL_VALUE_TRUE}
#error "${FOUND_BOOL_VALUE_TRUE} is not a macro
#endif
#ifndef ${FOUND_BOOL_VALUE_FALSE}
#error "${FOUND_BOOL_VALUE_FALSE} is not a macro
#endif
    ])], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND_BOOL_TYPE} bT = ${FOUND_BOOL_VALUE_TRUE};
bT = 42
    ])], [HAVE_BOOL_IMPLEM_INT=true; FOUND_BOOL_IMPLEM="number"], [HAVE_BOOL_IMPLEM_ENUM=true; FOUND_BOOL_IMPLEM="enum"])
    ], [HAVE_BOOL_IMPLEM_MACRO=true; FOUND_BOOL_IMPLEM="macro"])

            dnl # Final check
            AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND_BOOL_TYPE} b = ${FOUND_BOOL_VALUE_TRUE};
int ret = 0;

if (!(!b == ${FOUND_BOOL_VALUE_FALSE})) ret = 1;
if (!(b  != ${FOUND_BOOL_VALUE_FALSE})) ret = 2;
if (!(!b != ${FOUND_BOOL_VALUE_TRUE}))  ret = 3;
if (!b) ret = 4;
if (ret)
    return ret
/* no ";return 0;" here - autoconf adds one */
            ])],
            [ dnl # All tests passed, remember this
                    AC_MSG_NOTICE([Detected a semantically usable "bool"-like type name ${FOUND_BOOL_TYPE} implemented as ${FOUND_BOOL_IMPLEM} with boolean values '${FOUND_BOOL_VALUE_TRUE}' and '${FOUND_BOOL_VALUE_FALSE}'])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_TYPE],        [${FOUND_BOOL_TYPE}], [C spelling of bool type])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_IMPLEM_STR],  ["${FOUND_BOOL_IMPLEM}"], [String spelling of bool type implementation])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_VALUE_TRUE],  [${FOUND_BOOL_VALUE_TRUE}], [C spelling of bool type true value])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_VALUE_FALSE], [${FOUND_BOOL_VALUE_FALSE}], [C spelling of bool type false value])

                    AS_IF([${HAVE_BOOL_TYPE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_TYPE_LOWERCASE], [1], [Name of ${FOUND_BOOL_TYPE} is defined as lower-case token])])
                    AS_IF([${HAVE_BOOL_TYPE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_TYPE_UPPERCASE], [1], [Name of ${FOUND_BOOL_TYPE} is defined as upper-case token])])
                    AS_IF([${HAVE_BOOL_TYPE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_TYPE_CAMELCASE], [1], [Name of ${FOUND_BOOL_TYPE} is defined as camel-case token])])

                    AS_IF([${HAVE_BOOL_VALUE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_VALUE_LOWERCASE], [1], [Boolean values of ${FOUND_BOOL_TYPE} are defined as lower-case tokens])])
                    AS_IF([${HAVE_BOOL_VALUE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_VALUE_UPPERCASE], [1], [Boolean values of ${FOUND_BOOL_TYPE} are defined as upper-case tokens])])
                    AS_IF([${HAVE_BOOL_VALUE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_VALUE_CAMELCASE], [1], [Boolean values of ${FOUND_BOOL_TYPE} are defined as camel-case tokens])])

                    AS_IF([${HAVE_BOOL_IMPLEM_INT}],   [AC_DEFINE_UNQUOTED([HAVE_BOOL_IMPLEM_INT],   [1], [Boolean type ${FOUND_BOOL_TYPE} is defined as a general-purpose number type (int)])])
                    AS_IF([${HAVE_BOOL_IMPLEM_ENUM}],  [AC_DEFINE_UNQUOTED([HAVE_BOOL_IMPLEM_ENUM],  [1], [Boolean type ${FOUND_BOOL_TYPE} is defined as an enum with specific values allowed])])
                    AS_IF([${HAVE_BOOL_IMPLEM_MACRO}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_IMPLEM_MACRO], [1], [Boolean values of ${FOUND_BOOL_TYPE} are defined as a preprocessor macro (type/implem is questionable)])])
            ],
            [AC_MSG_NOTICE([Detected a "bool"-like type name ${FOUND_BOOL_TYPE}, but it was not semantically usable])])
    ], [AC_MSG_NOTICE([A "bool"-like type name or its useful values were not detected])])

    dnl ####################################################################

    AS_IF([test x"${FOUND_BOOLEAN_TYPE}" != x && test x"${FOUND_BOOLEAN_VALUE_TRUE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
#ifndef ${FOUND_BOOLEAN_VALUE_TRUE}
#error "${FOUND_BOOLEAN_VALUE_TRUE} is not a macro
#endif
#ifndef ${FOUND_BOOLEAN_VALUE_FALSE}
#error "${FOUND_BOOLEAN_VALUE_FALSE} is not a macro
#endif
    ])], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND_BOOLEAN_TYPE} bT = ${FOUND_BOOLEAN_VALUE_TRUE};
bT = 42
    ])], [HAVE_BOOLEAN_IMPLEM_INT=true; FOUND_BOOLEAN_IMPLEM="number"], [HAVE_BOOLEAN_IMPLEM_ENUM=true; FOUND_BOOLEAN_IMPLEM="enum"])
    ], [HAVE_BOOLEAN_IMPLEM_MACRO=true; FOUND_BOOLEAN_IMPLEM="macro"])

            dnl # Final check
            AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND_BOOLEAN_TYPE} b = ${FOUND_BOOLEAN_VALUE_TRUE};
int ret = 0;

if (!(!b == ${FOUND_BOOLEAN_VALUE_FALSE})) ret = 1;
if (!(b  != ${FOUND_BOOLEAN_VALUE_FALSE})) ret = 2;
if (!(!b != ${FOUND_BOOLEAN_VALUE_TRUE}))  ret = 3;
if (!b) ret = 4;
if (ret)
    return ret
/* no ";return 0;" here - autoconf adds one */
            ])],
            [ dnl # All tests passed, remember this
                    AC_MSG_NOTICE([Detected a semantically usable "boolean"-like type name ${FOUND_BOOLEAN_TYPE} implemented as ${FOUND_BOOLEAN_IMPLEM} with boolean values '${FOUND_BOOLEAN_VALUE_TRUE}' and '${FOUND_BOOL_VALUE_FALSE}'])
                    AC_DEFINE_UNQUOTED([FOUND_BOOLEAN_TYPE],        [${FOUND_BOOLEAN_TYPE}], [C spelling of boolean type])
                    AC_DEFINE_UNQUOTED([FOUND_BOOLEAN_IMPLEM_STR],  ["${FOUND_BOOLEAN_IMPLEM}"], [String spelling of boolean type implementation])
                    AC_DEFINE_UNQUOTED([FOUND_BOOLEAN_VALUE_TRUE],  [${FOUND_BOOLEAN_VALUE_TRUE}], [C spelling of boolean type true value])
                    AC_DEFINE_UNQUOTED([FOUND_BOOLEAN_VALUE_FALSE], [${FOUND_BOOLEAN_VALUE_FALSE}], [C spelling of boolean type false value])

                    AS_IF([${HAVE_BOOLEAN_TYPE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_TYPE_LOWERCASE], [1], [Name of ${FOUND_BOOLEAN_TYPE} is defined as lower-case token])])
                    AS_IF([${HAVE_BOOLEAN_TYPE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_TYPE_UPPERCASE], [1], [Name of ${FOUND_BOOLEAN_TYPE} is defined as upper-case token])])
                    AS_IF([${HAVE_BOOLEAN_TYPE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_TYPE_CAMELCASE], [1], [Name of ${FOUND_BOOLEAN_TYPE} is defined as camel-case token])])

                    AS_IF([${HAVE_BOOLEAN_VALUE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_VALUE_LOWERCASE], [1], [Boolean values of ${FOUND_BOOLEAN_TYPE} are defined as lower-case tokens])])
                    AS_IF([${HAVE_BOOLEAN_VALUE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_VALUE_UPPERCASE], [1], [Boolean values of ${FOUND_BOOLEAN_TYPE} are defined as upper-case tokens])])
                    AS_IF([${HAVE_BOOLEAN_VALUE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_VALUE_CAMELCASE], [1], [Boolean values of ${FOUND_BOOLEAN_TYPE} are defined as camel-case tokens])])

                    AS_IF([${HAVE_BOOLEAN_IMPLEM_INT}],   [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_IMPLEM_INT],   [1], [Boolean type ${FOUND_BOOLEAN_TYPE} is defined as a general-purpose number type (int)])])
                    AS_IF([${HAVE_BOOLEAN_IMPLEM_ENUM}],  [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_IMPLEM_ENUM],  [1], [Boolean type ${FOUND_BOOLEAN_TYPE} is defined as an enum with specific values allowed])])
                    AS_IF([${HAVE_BOOLEAN_IMPLEM_MACRO}], [AC_DEFINE_UNQUOTED([HAVE_BOOLEAN_IMPLEM_MACRO], [1], [Boolean values of ${FOUND_BOOLEAN_TYPE} are defined as a preprocessor macro (type/implem is questionable)])])
            ],
            [AC_MSG_NOTICE([Detected a "boolean"-like type name ${FOUND_BOOLEAN_TYPE}, but it was not semantically usable])])
    ], [AC_MSG_NOTICE([A "boolean"-like type name or its useful values were not detected])])

    dnl ####################################################################

    AS_IF([test x"${FOUND_BOOL_T_TYPE}" != x && test x"${FOUND_BOOL_T_VALUE_TRUE}" != x], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
#ifndef ${FOUND_BOOL_T_VALUE_TRUE}
#error "${FOUND_BOOL_T_VALUE_TRUE} is not a macro
#endif
    ])], [
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_T_STDBOOL}], [
${FOUND_BOOL_T_TYPE} bT = ${FOUND_BOOL_T_VALUE_TRUE};
bT = 42
    ])], [HAVE_BOOL_T_IMPLEM_INT=true; FOUND_BOOL_T_IMPLEM="number"], [HAVE_BOOL_T_IMPLEM_ENUM=true; FOUND_BOOL_T_IMPLEM="enum"])
    ], [HAVE_BOOL_T_IMPLEM_MACRO=true; FOUND_BOOL_T_IMPLEM="macro"])

            dnl # Final check
            AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([${myINCLUDE_STDBOOL}], [
${FOUND_BOOL_T_TYPE} b = ${FOUND_BOOL_T_VALUE_TRUE};
int ret = 0;

if (!(!b == ${FOUND_BOOL_T_VALUE_FALSE})) ret = 1;
if (!(b  != ${FOUND_BOOL_T_VALUE_FALSE})) ret = 2;
if (!(!b != ${FOUND_BOOL_T_VALUE_TRUE}))  ret = 3;
if (!b) ret = 4;
if (ret)
    return ret
/* no ";return 0;" here - autoconf adds one */
            ])],
            [ dnl # All tests passed, remember this
                    AC_MSG_NOTICE([Detected a semantically usable "bool_t"-like type name ${FOUND_BOOL_T_TYPE} implemented as ${FOUND_BOOL_T_IMPLEM} with boolean values '${FOUND_BOOL_T_VALUE_TRUE}' and '${FOUND_BOOL_T_VALUE_FALSE}'])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_T_TYPE],        [${FOUND_BOOL_T_TYPE}], [C spelling of bool_t type])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_T_IMPLEM_STR],  ["${FOUND_BOOL_T_IMPLEM}"], [String spelling of bool_t type implementation])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_T_VALUE_TRUE],  [${FOUND_BOOL_T_VALUE_TRUE}], [C spelling of bool_t type true value])
                    AC_DEFINE_UNQUOTED([FOUND_BOOL_T_VALUE_FALSE], [${FOUND_BOOL_T_VALUE_FALSE}], [C spelling of bool_t type false value])

                    AS_IF([${HAVE_BOOL_T_TYPE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_TYPE_LOWERCASE], [1], [Name of ${FOUND_BOOL_T_TYPE} is defined as lower-case token])])
                    AS_IF([${HAVE_BOOL_T_TYPE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_TYPE_UPPERCASE], [1], [Name of ${FOUND_BOOL_T_TYPE} is defined as upper-case token])])
                    AS_IF([${HAVE_BOOL_T_TYPE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_TYPE_CAMELCASE], [1], [Name of ${FOUND_BOOL_T_TYPE} is defined as camel-case token])])

                    AS_IF([${HAVE_BOOL_T_VALUE_LOWERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_VALUE_LOWERCASE], [1], [Boolean values of ${FOUND_BOOL_T_TYPE} are defined as lower-case tokens])])
                    AS_IF([${HAVE_BOOL_T_VALUE_UPPERCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_VALUE_UPPERCASE], [1], [Boolean values of ${FOUND_BOOL_T_TYPE} are defined as upper-case tokens])])
                    AS_IF([${HAVE_BOOL_T_VALUE_CAMELCASE}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_VALUE_CAMELCASE], [1], [Boolean values of ${FOUND_BOOL_T_TYPE} are defined as camel-case tokens])])

                    AS_IF([${HAVE_BOOL_T_IMPLEM_INT}],   [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_IMPLEM_INT],   [1], [Boolean type ${FOUND_BOOL_T_TYPE} is defined as a general-purpose number type (int)])])
                    AS_IF([${HAVE_BOOL_T_IMPLEM_ENUM}],  [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_IMPLEM_ENUM],  [1], [Boolean type ${FOUND_BOOL_T_TYPE} is defined as an enum with specific values allowed])])
                    AS_IF([${HAVE_BOOL_T_IMPLEM_MACRO}], [AC_DEFINE_UNQUOTED([HAVE_BOOL_T_IMPLEM_MACRO], [1], [Boolean values of ${FOUND_BOOL_T_TYPE} are defined as a preprocessor macro (type/implem is questionable)])])
            ],
            [AC_MSG_NOTICE([Detected a "bool_t"-like type name ${FOUND_BOOL_T_TYPE}, but it was not semantically usable])])
    ], [AC_MSG_NOTICE([A "bool_t"-like type name or its useful values were not detected])])

    dnl ####################################################################

    AC_LANG_POP([C])

fi
])
