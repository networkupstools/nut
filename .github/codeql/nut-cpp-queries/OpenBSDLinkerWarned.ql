/**
 * @name Call to a function OpenBSD's linker warns about
 * @description OpenBSD's libc marks these functions with __warn_references(),
 *              so every link against them prints a warning (e.g. "sprintf()
 *              is often misused, please use snprintf()"). A call in a shared
 *              helper such as libcommon warns for every program linked with
 *              it. NUT has portable replacements for all of them.
 * @kind problem
 * @problem.severity recommendation
 * @precision high
 * @id nut/cpp/openbsd-linker-warned-call
 * @tags portability
 *       maintainability
 */

import cpp

/** The replacement to suggest for `name`. */
string advice(string name) {
  name = ["strcpy", "stpcpy"] and result = "snprintf() or xstrdup()"
  or
  name = "strcat" and result = "snprintfcat() or str_concat()"
  or
  name = "sprintf" and result = "snprintf() or snprintfcat()"
  or
  name = "vsprintf" and result = "vsnprintf() or vsnprintfcat()"
  or
  name = ["wcscpy", "wcscat"] and result = "a bounded call such as swprintf()"
  or
  name = ["rand", "rand_r", "random"] and result = "nut_random_bytes()"
  or
  name = ["tmpnam", "tempnam", "mktemp"] and result = "mkstemp()"
  or
  name = "getwd" and result = "getcwd()"
}

from FunctionCall c, string name
where
  name = c.getTarget().getName() and
  c.getTarget().hasGlobalOrStdName(name) and
  exists(advice(name)) and
  // only code built from the NUT tree
  exists(c.getFile().getRelativePath()) and
  // nut_random_bytes() falls back to rand() on purpose when no system RNG
  // answers; that is the one place it belongs
  not c.getEnclosingFunction().hasGlobalName("nut_random_bytes")
select c,
  "Call to " + name + "(), which OpenBSD's linker warns about for every program linked with it. Use "
    + advice(name) + " instead (see include/common.h and include/str.h)."
