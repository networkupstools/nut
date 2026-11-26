/**
 * @name Comparison result is always the same (NUT)
 * @description When a comparison operation, such as x < y, always
 *              returns the same result, it means that the comparison
 *              is redundant and may mask a bug because a different
 *              check was intended. Unlike cpp/constant-comparison, this
 *              ignores portable range checks that are only constant on
 *              some platforms.
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id nut/cpp/constant-comparison
 * @tags maintainability
 *       readability
 */

/*
 * Copy of `cpp/ql/src/Likely Bugs/Arithmetic/PointlessComparison.ql`
 * (cpp/constant-comparison) from github/codeql (MIT licensed), as of
 * codeql-cli/v2.27.5, with two NUT exclusions marked below. NUT checks
 * values against type limits that differ between platforms and library
 * versions, e.g.
 *
 *   if (ret < 0 || (uintmax_t)ret > SIZE_MAX)
 *   || ReportId < 0 || (uintmax_t)ReportId > UINT16_MAX
 *
 * Such a check is constant on the build CodeQL analyzes but not on others,
 * so the stock query flags it although it is deliberate.
 */

import cpp
private import semmle.code.cpp.commons.Exclusions
private import semmle.code.cpp.rangeanalysis.PointlessComparison
private import semmle.code.cpp.rangeanalysis.RangeAnalysisUtils
import UnsignedGEZero

/**
 * NUT exclusion 1: `e` (ignoring casts and parentheses) is a type-limit
 * macro such as `SIZE_MAX`, `INT_MAX`, `UINT16_MAX` or a project-defined
 * `..._MIN`/`..._MAX`.
 */
predicate isLimitMacro(Expr e) {
  exists(MacroInvocation mi |
    mi.getMacroName().regexpMatch(".*_(MAX|MIN)") and
    mi.getAnExpandedElement() = e.getUnconverted()
  )
}

/**
 * NUT exclusion 2: `e` (ignoring casts) has a typedef'd type whose
 * signedness depends on the platform or library version, such as NUT's
 * `usb_ctrl_*` types or `socklen_t`. Fixed-width and `size_t` types are
 * the same everywhere, so comparisons on them are still reported.
 */
predicate hasPortableTypedefType(Expr e) {
  exists(TypedefType t |
    (
      t = e.getUnconverted().getType()
      or
      t = e.getUnconverted().getType().(SpecifiedType).getBaseType()
    ) and
    not t.getName().regexpMatch("u?int(8|16|32|64|max|ptr)_t|size_t|ssize_t")
  )
}

/** Holds if `cmp` is one of the portable range checks described above. */
predicate isPortableRangeCheck(ComparisonOperation cmp) {
  isLimitMacro(cmp.getAnOperand())
  or
  exists(Expr e, Expr z |
    e = cmp.getAnOperand() and
    z = cmp.getAnOperand() and
    e != z and
    z.getUnconverted() instanceof ConstantZero and
    hasPortableTypedefType(e)
  )
}

// Trivial comparisons of the form 1 > 0 are usually due to macro expansion.
// For example:
//
// #define PRINTMSG(val,msg) { if (val >= PRINTLEVEL) printf(msg); }
//
// So to reduce the number of false positives, we do not report a result if
// the comparison is in a macro expansion. Similarly for template
// instantiations, static asserts, non-type template arguments, enum constants,
// and constexprs.
from ComparisonOperation cmp, SmallSide ss, float left, float right, boolean value, string reason
where
  not cmp.isInMacroExpansion() and
  not cmp.isFromTemplateInstantiation(_) and
  not exists(StaticAssert s | s.getCondition() = cmp.getParent*()) and
  not exists(Declaration d | d.getATemplateArgument() = cmp.getParent*()) and
  not exists(Variable v | v.isConstexpr() | v.getInitializer().getExpr() = cmp.getParent*()) and
  not exists(EnumConstant e | e.getInitializer().getExpr() = cmp.getParent*()) and
  not functionContainsDisabledCode(cmp.getEnclosingFunction()) and
  // NUT: not a portable range check
  not isPortableRangeCheck(cmp) and
  reachablePointlessComparison(cmp, left, right, value, ss) and
  // a comparison between an enum and zero is always valid because whether
  // the underlying type of an enum is signed is compiler-dependent
  not exists(Expr e, ConstantZero z |
    relOpWithSwap(cmp, e.getFullyConverted(), z, _, _) and
    e.getUnderlyingType() instanceof Enum
  ) and
  // Construct a reason for the message. Something like: x >= 5 and 3 >= y.
  exists(string cmpOp, string leftReason, string rightReason |
    (
      ss = LeftIsSmaller() and cmpOp = " <= "
      or
      ss = RightIsSmaller() and cmpOp = " >= "
    ) and
    leftReason = cmp.getLeftOperand().toString() + cmpOp + left.toString() and
    rightReason = right.toString() + cmpOp + cmp.getRightOperand().toString() and
    // If either of the operands is constant, then don't include it.
    (
      if cmp.getLeftOperand().isConstant()
      then not cmp.getRightOperand().isConstant() and reason = rightReason
      else
        if cmp.getRightOperand().isConstant()
        then reason = leftReason
        else reason = leftReason + " and " + rightReason
    )
  ) and
  // Don't report results which have already been reported by UnsignedGEZero.
  not unsignedGEZero(cmp, _)
select cmp, "Comparison is always " + value.toString() + " because " + reason + "."
