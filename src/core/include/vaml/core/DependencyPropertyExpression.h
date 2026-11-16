#pragma once
// VAML DependencyPropertyExpression — the expression-as-value interface.
// S811 (corrective, dep:S810). Headline: "D2/D3 FIX — remove the implicit
// cross-thread value queue AND the invented S041 change-notification pipeline;
// wrong-thread Set/Get THROWS."
//
// WPF ground truth (audited — this SEAM is the refactor target):
//   - `Expression` (Expression.cs:140-155) is the abstract base of a live DP
//     value. A `BindingExpression : BindingExpressionBase : Expression` holds
//     the target DP's LOCAL VALUE when bound.
//   - A target-side Set (a TextBox edit) flows through `SetValueCommon` which,
//     when the current effective value IS an Expression, routes the incoming
//     value through `currentExpr.SetValue(this, dp, value)` (DO.cs:705-708).
//     `BindingExpressionBase.SetValue` (BindingExpressionBase.cs:397-414)
//     "handles" the set — for a reflective (TwoWay) binding it stores the value
//     and writes it back to the source (UpdateValue → source). The expression
//     IS the value; there is NO per-object subscriber list (DO.cs:1704/1732/2039).
//   - The target→source write-back is therefore EXPRESSION.SetValue interception,
//     NOT a `PropertyChangedCallback` on the target metadata (DO.cs:2037 fires
//     the metadata callback, but that is a single coerced/notify callback, not
//     a write-back carrier) and NOT a subscriber registry.
//
// S811 therefore introduces this minimal seam — the WPF-faithful primitive the
// fabricated `DependencyObject::add_property_changed` registry usurped. A bound
// target DP stores ONE live expression in `DependencyObject::expressions_`
// (per-DP single slot, keyed by the composite PropertyId) and routes a
// subsequent non-expression Set() through it when present.
//
// This is a REAL, permanent core surface (nothing to backfill later). It is a
// deliberate seed: the TODO comment marks the ONE piece extended by S812 (the
// forward source→target conversation remains on the caller/ObservableObject
// subscription).

#include "vaml/core/Value.h"

namespace vaml {

class DependencyObject;
class PropertyId;

// The expression-as-value interface (WPF `Expression`, Expression.cs:140-155).
// A live expression holds a target DP's value and is given the FIRST say over a
// subsequent Set() on the affinity thread — mirroring WPF SetValueCommon
// routing a non-expression value through `currentExpr.SetValue`.
class VAML_CORE_EXPORT DependencyPropertyExpression
{
public:
    virtual ~DependencyPropertyExpression() = default;

    // WPF Expression.SetValue / BindingExpressionBase.SetValue
    // (BindingExpressionBase.cs:397-414). Called by DependencyObject::Set on
    // the AFFINE thread when this expression currently holds the target DP's
    // value and a non-expression value arrives.
    //
    //   - returns TRUE  when the expression "handles" the value (it stored
    //     it / wrote it back — for a reflective TwoWay binding);
    //   - returns FALSE when the expression declines (the caller then falls
    //     through to the plain local store, detaching this expression — WPF's
    //     `return false` in the non-reflective branch, BindingExpressionBase.cs:412).
    [[nodiscard]] virtual bool SetValue(const Value& value) = 0;

protected:
    DependencyPropertyExpression() = default;
    DependencyPropertyExpression(const DependencyPropertyExpression&) = delete;
    DependencyPropertyExpression& operator=(const DependencyPropertyExpression&) = delete;
};

} // namespace vaml
