#pragma once
// S501 (dep:S500) — BooleanToVisibilityConverter: the SOLE framework-owned
// concrete binding value-converter. bool -> Visibility Int32 DP.
//
// WPF GROUND TRUTH (verified against E:\GitRepos\wpf):
//   src\Microsoft.DotNet.Wpf\src\PresentationFramework\System\Windows\Controls\
//   BooleanToVisibilityConverter.cs — public sealed converter that implements
//   IValueConverter:
//     Convert:    bool/Nullable<bool> true -> Visible, false -> Collapsed.
//     ConvertBack: Visibility Visible -> true, Collapsed/Hidden -> false.
// The VAML Visibility DP records its states as an Int32 (the S089 enum-name
// map: Visible=0, Hidden=1, Collapsed=2 — see ValueConverter.cpp
// enum_name_to_int32); this converter therefore reads/writes Int32 payloads:
//   Convert:      Bool(true)                        -> Int32(0)   (Visible)
//                 Bool(false)                       -> Int32(2)   (Collapsed)
//   ConvertBack:  Int32 == 0 (Visible)              -> Bool(true)
//                 anything else (Hidden/Collapsed)  -> Bool(false)
//
// Non-bool Convert input: WPF coerces a non-bool to `false` (-> Collapsed).
// The VAML converter instead FAILS (ok=false) so the S242 ConverterFailed +
// fallback path is reached and the author is told loudly — never a silent
// collapse. This is the one deliberate, documented deviation from WPF silence
// (ADR-016: fail-loud beats silent-coerce on the diagnostic channel).
//
// This is the ONLY framework-owned concrete converter. The app authors any
// further `*Converter` (incl. `NullTo*`) by implementing IValueConverter (see
// IValueConverter.h). The remaining WPF framework converters
// (AlternationConverter, BooleanToSelectiveScrollingOrientationConverter,
// BorderGapMaskConverter, MenuScrollingVisibilityConverter,
// DataGridHeadersVisibilityToVisibilityConverter) are S556-consumed (recorded
// NO-GAP, not built here).
//
// ADR-018: ZERO new locks (single-threaded binding use).

#include "vaml/core/IValueConverter.h" // IValueConverter + BindingConversion
#include "vaml/core/export.h"

namespace vaml {

// The framework-owned bool -> Visibility converter (see the header comment).
// WPF-faithful; the sole framework concrete converter.
class VAML_CORE_EXPORT BooleanToVisibilityConverter : public IValueConverter
{
public:
    // Bool(true) -> Int32(0) Visible; Bool(false) -> Int32(2) Collapsed.
    // A non-Bool input FAILS (ok=false, "BooleanToVisibilityConverter requires
    // a Bool") — never a silent collapse (see the header comment).
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;

    // Int32(0) (Visible) -> Bool(true); any other Int32 (Hidden/Collapsed) ->
    // Bool(false). A non-Int32 input FAILS (ok=false) — never a silent guess.
    [[nodiscard]] BindingConversion ConvertBack(const Value& value) const override;
};

} // namespace vaml
