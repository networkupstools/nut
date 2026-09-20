dnl Check for semaphore headers, libraries, and named or unnamed semaphores.
dnl Sets SEMLIBS, nut_have_semaphore_* and HAVE_SEMAPHORE_* definitions.
dnl Call after the fcntl.h and sys/stat.h checks, before threading policy.

AC_DEFUN([NUT_CHECK_SEMAPHORES],
[
SEMLIBS=""
nut_have_semaphore_h=no
nut_have_semaphore_unnamed=no
nut_have_semaphore_named=no
AC_CHECK_HEADER([semaphore.h],
    [AC_DEFINE([HAVE_SEMAPHORE_H], [1],
        [Define to 1 if you have <sys/semaphore.h>.])
     nut_have_semaphore_h=yes

     AC_LANG_PUSH([C])
     myLIBS="${LIBS}"
     LIBS=""
     SEMLIBS_LRT=""

     dnl Solaris 8 builds complain about indirect dependency involved:
     dnl   sem_init   nut_scanner-nut-scanner.o
     dnl   (symbol belongs to implicit dependency /usr/lib/librt.so.1)
     AC_SEARCH_LIBS([sem_init], [pthread], [], [
        unset ac_cv_search_sem_init
        AC_SEARCH_LIBS([sem_init], [pthread], [SEMLIBS_LRT=" -lrt"], [], [-lrt])])
     AC_SEARCH_LIBS([sem_open], [pthread], [], [
        unset ac_cv_search_sem_open
        AC_SEARCH_LIBS([sem_open], [pthread], [SEMLIBS_LRT=" -lrt"], [], [-lrt])])

     AS_CASE([${ac_cv_search_sem_init}], [no*], [], [SEMLIBS="${ac_cv_search_sem_init}"])
     AS_CASE([${ac_cv_search_sem_open}], [no*], [], ["${SEMLIBS}"], [], [SEMLIBS="${ac_cv_search_sem_open}"])
     SEMLIBS="${SEMLIBS}${SEMLIBS_LRT}"

     LIBS="${SEMLIBS}"

     AC_MSG_CHECKING([for sem_t, sem_init() and sem_destroy()])
     AX_RUN_OR_LINK_IFELSE([AC_LANG_PROGRAM([
#include <semaphore.h>
],
[sem_t semaphore;
sem_init(&semaphore, 0, 4);
sem_destroy(&semaphore);
/* Do not care about actual return value in this test,
 * normally check for non-zero meaning to look in errno */
]
        )],
        [AC_DEFINE([HAVE_SEMAPHORE_UNNAMED], [1],
            [Define to 1 if you have <sys/semaphore.h> with usable sem_t, sem_init() and sem_destroy() for unnamed semaphores.])
         AC_MSG_RESULT([ok])
         nut_have_semaphore_unnamed=yes
        ],
        [AC_MSG_RESULT([no])]
     )

     AC_MSG_CHECKING([for sem_t, sem_open() and sem_close()])
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <semaphore.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>           /* For O_* constants */
#endif
#ifdef SYS_STAT_H
# include <sys/stat.h>        /* For mode constants */
#endif
],
[sem_t *semaphore = sem_open("/s", O_CREAT, 0644, 4);
if (semaphore != SEM_FAILED)
    sem_close(semaphore);
/* Do not care about actual return value in this test,
 * normally check for non-zero meaning to look in errno */
]
        )],
        [AC_DEFINE([HAVE_SEMAPHORE_NAMED], [1],
            [Define to 1 if you have <sys/semaphore.h> with usable sem_t, sem_open() and sem_close() for named semaphores.])
         AC_MSG_RESULT([ok])
         nut_have_semaphore_named=yes
        ],
        [AC_MSG_RESULT([no])]
     )

     LIBS="${myLIBS}"
     AC_LANG_POP([C])
    ]
)
AM_CONDITIONAL(HAVE_SEMAPHORE_LIBS, [test -n "${SEMLIBS}"])
AS_IF([test -n "${SEMLIBS}" -o x"${nut_have_semaphore_h}" = xyes -o x"${nut_have_semaphore_unnamed}" = xyes -o x"${nut_have_semaphore_named}" = xyes],
    [nut_have_semaphore_ability=yes],
    [nut_have_semaphore_ability=no])
])
