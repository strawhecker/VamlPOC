#pragma once
// VAML EnumDomainRegistry — the S11p (S600) TYPE-AWARE enum-authoring authority.
//
// S600 (dep:S596) root A1: the literal converter must bind to the DP's DECLARED
// bound type — an Int32-typed DP that declares an enum domain converts its
// literal through THAT domain's name->ordinal table, never through a shared
// cross-cut behavior name-map. This registry is that per-DP enum domain table.
//
// WPF-fidelity model (grounded in the checked-out WPF source): WPF selects a
// `TypeConverter` per DECLARED property type — an enum-typed DP uses
// `Enum.Parse(enumType, name)` (System.ComponentModel.EnumConverter), which
// resolves the name ONLY against that enum type's members and throws if the
// name is not a member. A VAML Int32-typed DP with a registered enum domain
// mirrors exactly that: the name is resolved against the DP's OWN members, and
// a name NOT in the domain fails with MalformedValue (never a silent cross-cut
// guess — e.g. `TextWrapping="Center"` (a TextAlignment member) MUST fail).
//
// The enum ordinals in each domain are the VERIFIED WPF-true values (this
// sprint's audit, file:line cited in the registration comments):
//   StretchDirection { UpOnly=0, DownOnly=1, Both=2 }        (ViewBox.cs:30)
//   ResizeMode       { NoResize=0, CanMinimize=1, CanResize=2, CanResizeWithGrip=3 } (Window.cs:8018)
//   WindowStartupLocation { Manual=0, CenterScreen=1, CenterOwner=2 } (Window.cs:7992)
//   TextAlignment     { Left=0, Right=1, Center=2, Justify=3 } (TextAndFontProperties.cs:49)
//   TextWrapping      { WrapWithOverflow=0, NoWrap=1, Wrap=2 } (:200)
//   TextTrimming      { None=0, CharacterEllipsis=1, WordEllipsis=2 } (:231)
//   Orientation       { Horizontal=0, Vertical=1 }           (Orientation.cs)
//   HorizontalAlignment { Left=0, Center=1, Right=2, Stretch=3 } (FrameworkElement.cs:28)
//   VerticalAlignment   { Top=0, Center=1, Bottom=2, Stretch=3 } (FrameworkElement.cs:57)
//   Visibility        { Visible=0, Hidden=1, Collapsed=2 }   (UIElement.cs)
//   Dock              { Left=0, Top=1, Right=2, Bottom=3, Fill=4 } (WPF Dock.cs)
//   IconStretch       { None=0, Fill=1, Uniform=2, UniformToFill=3 } (matches WPF Stretch)
//
// Every domain is keyed by the DP's CANONICAL name (S143/S144 NameTable
// discipline), so BOTH readers (S2 text VamlReader::convertLiteral + S7 compiled
// VBamlReader::convertLiterals) consult the SAME table — S288 parity preserved.
// A DP with NO registered domain keeps the numeric-only Int32 path (a numeric
// literal is always valid); it does NOT silently enter any cross-cut enum map.
//
// NO-GAP: every enum domain rides the DP's OWN declared bound type. This is
// REAL permanent surface (not MINIMAL REAL, not a STUB) — it replaces the old
// S440 cross-cut `enum_name_to_int32` behavior-guess map, which could not
// distinguish `Dock` (Left=0) from `HorizontalAlignment` (Left=1) and
// mis-resolved `DockPanel.Dock="Left"` to 1 (=Top) — the exact wrong-typed
// class A1 exists to kill.

#include "vaml/core/DependencyProperty.h"   // DependencyProperty (dp.name via property_id)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"               // Value::Int32
#include "vaml/core/ValueConversionError.h" // ValueConversionException / ValueConversionError
#include "vaml/core/detail/NameTable.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// An enum domain: the set of (case-insensitive name -> WPF-true ordinal) members
// a specific Int32-typed DP declares. Mirrors the members Enum.Parse would accept
// for the equivalent WPF enum TYPE (the DP's declared bound type authority).
struct EnumDomain
{
    std::string_view name;                    // DP canonical name this domain binds to
    std::string_view memberNames[16];         // member names, WPF-true order
    std::int32_t     memberValues[16];        // WPF-true ordinals, 1:1 with memberNames
    std::size_t      count = 0;               // number of members
};

// The per-DP enum-domain registry. Singleton (Meyers static) shared by both
// readers + populated at built-in startup (register_builtin_enum_domains()).
class VAML_CORE_EXPORT EnumDomainRegistry
{
public:
    EnumDomainRegistry();
    ~EnumDomainRegistry();

    EnumDomainRegistry(const EnumDomainRegistry&) = delete;
    EnumDomainRegistry& operator=(const EnumDomainRegistry&) = delete;

    // Registers `domain` keyed by `dpName`. Content-unique (NameTable dedupe):
    // a duplicate DP name returns nullptr and leaves the existing entry.
    // Returns a pointer to the STABLE stored domain (or nullptr on dup).
    const EnumDomain* register_domain(std::string dpName, EnumDomain domain);

    // Looks up the domain for `dpName`, or nullptr if none. Never throws.
    const EnumDomain* try_find(std::string_view dpName) const noexcept;

    // Number of registered domains.
    std::size_t size() const noexcept;

    // The process-wide singleton.
    static EnumDomainRegistry& global() noexcept;

private:
    detail::NameTable<EnumDomain> table_;
};

// Populates EnumDomainRegistry::global() with the WPF-true enum domains for the
// built-in Int32-typed DPs. Idempotent (guarded). Called from the readers'
// ensure_startup_registrations path.
VAML_CORE_EXPORT void register_builtin_enum_domains();

// S600 (dep:S596) root A1 — TYPE-AWARE Int32 literal conversion, SHARED by both
// readers (S2 VamlReader::convertLiteral + S7 VBamlReader::convertLiterals) so
// the S288 parity contract holds. A numeric literal is always valid on any Int32
// DP (strict int, tried first). Otherwise the DP's DECLARED enum domain
// (EnumDomainRegistry, keyed by DP canonical name) resolves the name against the
// DP's OWN members — case-insensitive, WPF Enum.Parse semantics. A name NOT in
// the DP's domain fails with MalformedValue naming type + literal (never a
// silent cross-cut guess: `TextWrapping="Center"` — a TextAlignment member —
// MUST fail, exactly as Enum.Parse(typeof(TextWrapping), "Center") fails in
// WPF). A DP with NO registered domain keeps the numeric-only path (a name on a
// domain-less Int32 DP is MalformedValue, never 0).
VAML_CORE_EXPORT Value convert_int32_for_dp(const DependencyProperty& dp,
                                            std::string_view literal);

} // namespace vaml
