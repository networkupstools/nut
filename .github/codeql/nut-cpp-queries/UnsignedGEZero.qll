/**
 * Copy of `cpp/ql/src/Likely Bugs/Arithmetic/UnsignedGEZero.qll` from
 * github/codeql (MIT licensed), as of codeql-cli/v2.27.5. It lives in the
 * codeql/cpp-queries pack, not the library, so it is copied here for
 * ConstantComparison.ql, which uses it to avoid reporting a comparison that
 * cpp/unsigned-comparison-zero already reports.
 */

import cpp

class ConstantZero extends Expr {
  ConstantZero() {
    this.isConstant() and
    this.getValue() = "0"
  }
}

/**
 * Holds if `candidate` is an expression such that if it's unsigned then we
 * want an alert at `ge`.
 */
private predicate lookForUnsignedAt(RelationalOperation ge, Expr candidate) {
  // Base case: `candidate >= 0` (or `0 <= candidate`)
  (
    ge instanceof GEExpr or
    ge instanceof LEExpr
  ) and
  ge.getLesserOperand() instanceof ConstantZero and
  candidate = ge.getGreaterOperand().getFullyConverted() and
  // left/greater operand was a signed or unsigned IntegralType before conversions
  // (not a pointer, checking a pointer >= 0 is an entirely different mistake)
  // (not an enum, as the fully converted type of an enum is compiler dependent
  //  so checking an enum >= 0 is always reasonable)
  ge.getGreaterOperand().getUnderlyingType() instanceof IntegralType
  or
  // Recursive case: `...(largerType)candidate >= 0`
  exists(Conversion conversion |
    lookForUnsignedAt(ge, conversion) and
    candidate = conversion.getExpr() and
    conversion.getType().getSize() > candidate.getType().getSize()
  )
}

class UnsignedGEZero extends ComparisonOperation {
  UnsignedGEZero() {
    exists(Expr ue |
      lookForUnsignedAt(this, ue) and
      ue.getUnderlyingType().(IntegralType).isUnsigned()
    )
  }
}

predicate unsignedGEZero(UnsignedGEZero ugez, string msg) {
  not exists(MacroInvocation mi |
    // ugez is in mi
    mi.getAnExpandedElement() = ugez and
    // and ugez was apparently not passed in as a macro parameter
    ugez.getLocation().getStartLine() = mi.getLocation().getStartLine() and
    ugez.getLocation().getStartColumn() = mi.getLocation().getStartColumn()
  ) and
  not ugez.isFromTemplateInstantiation(_) and
  msg = "Pointless comparison of unsigned value to zero."
}
