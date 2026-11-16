// S811 — DependencyPropertyExpression out-of-line definitions (dep:S810).
// The expression-as-value interface is defined entirely in its header
// (virtual, no out-of-line definitions required beyond the virtual destructor,
// which is implicitly defaulted). This TU exists so the VAML_CORE_EXPORT symbol
// has a home in the library and to keep the header free of a .cpp include.
//
// WPF ground truth (audited): `Expression` (Expression.cs:140-155) is the
// abstract base of a live DP value; a bound `BindingExpression` implements
// `SetValue` (BindingExpressionBase.cs:397-414) which "handles" a non-
// expression target Set on the affinity thread (DO.cs:705-708) — the WPF
// target→source write-back primitive. See DependencyPropertyExpression.h.

#include "vaml/core/DependencyPropertyExpression.h"

namespace vaml {

// Nothing to define — the interface is header-declared (virtual). This TU is
// kept as the symbol home / future extension seam (S812: the forward
// source→target conversation).

} // namespace vaml
