// S600 (dep:S596) — EnumDomainRegistry out-of-line definitions + the WPF-true
// enum-domain registration table (root A1).
//
// This is the REAL permanent surface that replaces the S440 cross-cut
// `enum_name_to_int32` behavior-guess map. Each registered domain binds an
// Int32-typed DP to its DECLARED enum domain, so the literal converter resolves
// a name against the DP's OWN members (WPF Enum.Parse semantics) instead of a
// shared first-name-wins table. The ordinals are the VERIFIED WPF-true values
// audited against the checked-out WPF source this sprint (file:line cited in
// EnumDomainRegistry.h). NO-GAP: every domain rides the DP's own bound type.

#include "vaml/core/EnumDomainRegistry.h"

#include <charconv>
#include <string>
#include <string_view>

namespace vaml {

namespace {

// Case-insensitive ASCII fold (WPF/XAML is case-insensitive for enum names:
// "Visible"/"visible"/"VISIBLE" all valid — mirrors ValueConverter.cpp's own
// iequals; kept local here for enumeration-name resolution).
bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char y = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y)
        {
            return false;
        }
    }
    return true;
}

} // namespace

VAML_CORE_EXPORT
Value convert_int32_for_dp(const DependencyProperty& dp, std::string_view literal)
{
    // Numeric literals are unambiguous and always valid on any Int32 DP.
    if (!literal.empty())
    {
        int value = 0;
        auto [ptr, ec] = std::from_chars(literal.data(), literal.data() + literal.size(), value);
        if (ec == std::errc() && ptr == (literal.data() + literal.size()))
        {
            return Value::Int32(static_cast<std::int32_t>(value));
        }
    }

    // Resolve the name against the DP's DECLARED enum domain (if any).
    const EnumDomain* domain = EnumDomainRegistry::global().try_find(dp.property_id().name());
    if (domain != nullptr)
    {
        for (std::size_t i = 0; i < domain->count; ++i)
        {
            if (iequals(literal, domain->memberNames[i]))
            {
                return Value::Int32(domain->memberValues[i]);
            }
        }
        // Name not in THIS domain -> MalformedValue (WPF Enum.Parse fails).
        throw ValueConversionException(
            ValueConversionError::MalformedValue, std::string(domain->name),
            std::string(literal),
            "cannot convert '" + std::string(literal) + "' to " +
                std::string(domain->name) + " (MalformedValue)");
    }

    // No registered domain -> the DP takes only numeric literals; a name is
    // MalformedValue (never 0, never a cross-cut guess).
    throw ValueConversionException(
        ValueConversionError::MalformedValue, "Int32", std::string(literal),
        "cannot convert '" + std::string(literal) + "' to Int32 (MalformedValue)");
}

VAML_CORE_EXPORT
EnumDomainRegistry::EnumDomainRegistry() = default;

VAML_CORE_EXPORT
EnumDomainRegistry::~EnumDomainRegistry() = default;

VAML_CORE_EXPORT
const EnumDomain* EnumDomainRegistry::register_domain(std::string dpName, EnumDomain domain)
{
    // NameTable content dedupe: a duplicate DP name returns nullptr (existing
    // entry unchanged) — mirrors DependencyProperty::Register / the converter
    // registry (S032 discipline).
    return table_.insert(std::move(dpName), std::move(domain));
}

VAML_CORE_EXPORT
const EnumDomain* EnumDomainRegistry::try_find(std::string_view dpName) const noexcept
{
    return table_.find(dpName);
}

VAML_CORE_EXPORT
std::size_t EnumDomainRegistry::size() const noexcept
{
    return table_.size();
}

VAML_CORE_EXPORT
EnumDomainRegistry& EnumDomainRegistry::global() noexcept
{
    // Meyers function-local static (thread-safe init, no static-init-order
    // fiasco) — mirrors PropertyRegistry::global / DependencyProperty::global /
    // ValueConverterRegistry::global / ElementFactoryRegistry::global.
    static EnumDomainRegistry instance;
    return instance;
}

VAML_CORE_EXPORT
void register_builtin_enum_domains()
{
    EnumDomainRegistry& registry = EnumDomainRegistry::global();

    // Idempotency guard: a stray second call is a no-op (mirrors
    // register_builtin_converters / register_builtin_elements). NO-LAZY: this
    // is a startup-ordering guard, not dead code.
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;

    // Helper: build an EnumDomain from a name + parallel member arrays.
    // memberNames/memberValues must be the same length.
    auto make_domain = [](std::string_view dpName,
                          std::initializer_list<std::pair<std::string_view, std::int32_t>> members)
    {
        EnumDomain d;
        d.name = dpName;
        d.count = members.size();
        std::size_t i = 0;
        for (const auto& [name, value] : members)
        {
            d.memberNames[i]  = name;
            d.memberValues[i] = value;
            ++i;
        }
        return d;
    };

    // ---- Enum domains for the Int32-typed DPs (WPF-true ordinals) ----
    //
    // TextAlignment { Left=0, Right=1, Center=2, Justify=3 } (TextAndFontProperties.cs:49)
    registry.register_domain("TextAlignment",
        make_domain("TextAlignment", {
            {"Left", 0}, {"Right", 1}, {"Center", 2}, {"Justify", 3},
        }));

    // TextWrapping { WrapWithOverflow=0, NoWrap=1, Wrap=2 } (:200)
    registry.register_domain("TextWrapping",
        make_domain("TextWrapping", {
            {"WrapWithOverflow", 0}, {"NoWrap", 1}, {"Wrap", 2},
        }));

    // TextTrimming { None=0, CharacterEllipsis=1, WordEllipsis=2 } (:231)
    registry.register_domain("TextTrimming",
        make_domain("TextTrimming", {
            {"None", 0}, {"CharacterEllipsis", 1}, {"WordEllipsis", 2},
        }));

    // StretchDirection { UpOnly=0, DownOnly=1, Both=2 } (ViewBox.cs:30)
    registry.register_domain("StretchDirection",
        make_domain("StretchDirection", {
            {"UpOnly", 0}, {"DownOnly", 1}, {"Both", 2},
        }));

    // ResizeMode { NoResize=0, CanMinimize=1, CanResize=2, CanResizeWithGrip=3 } (Window.cs:8018)
    registry.register_domain("ResizeMode",
        make_domain("ResizeMode", {
            {"NoResize", 0}, {"CanMinimize", 1}, {"CanResize", 2}, {"CanResizeWithGrip", 3},
        }));

    // WindowStartupLocation { Manual=0, CenterScreen=1, CenterOwner=2 } (Window.cs:7992)
    registry.register_domain("WindowStartupLocation",
        make_domain("WindowStartupLocation", {
            {"Manual", 0}, {"CenterScreen", 1}, {"CenterOwner", 2},
        }));

    // WindowStyle { None=0, SingleBorderWindow=1, ThreeDBorderWindow=2, ToolWindow=3 }
    // (Window.cs — the WindowStyle enum; cross-checked with vaml's own
    // registration comment at Element.cpp:2127).
    registry.register_domain("WindowStyle",
        make_domain("WindowStyle", {
            {"None", 0}, {"SingleBorderWindow", 1}, {"ThreeDBorderWindow", 2}, {"ToolWindow", 3},
        }));

    // WindowState { Normal=0, Minimized=1, Maximized=2 } (Window.cs:7955)
    registry.register_domain("WindowState",
        make_domain("WindowState", {
            {"Normal", 0}, {"Minimized", 1}, {"Maximized", 2},
        }));

    // SizeToContent { Manual=0, Width=1, Height=2, WidthAndHeight=3 } (SizeToContent.cs)
    registry.register_domain("SizeToContent",
        make_domain("SizeToContent", {
            {"Manual", 0}, {"Width", 1}, {"Height", 2}, {"WidthAndHeight", 3},
        }));

    // Orientation { Horizontal=0, Vertical=1 } (Orientation.cs) — RE-POINTED S11p.
    registry.register_domain("Orientation",
        make_domain("Orientation", {
            {"Horizontal", 0}, {"Vertical", 1},
        }));

    // HorizontalAlignment { Left=0, Center=1, Right=2, Stretch=3 } (FrameworkElement.cs:28)
    registry.register_domain("HorizontalAlignment",
        make_domain("HorizontalAlignment", {
            {"Left", 0}, {"Center", 1}, {"Right", 2}, {"Stretch", 3},
        }));

    // VerticalAlignment { Top=0, Center=1, Bottom=2, Stretch=3 } (FrameworkElement.cs:57)
    registry.register_domain("VerticalAlignment",
        make_domain("VerticalAlignment", {
            {"Top", 0}, {"Center", 1}, {"Bottom", 2}, {"Stretch", 3},
        }));

    // HorizontalContentAlignment { Left=0, Center=1, Right=2, Stretch=3 } — the
    // Control-base content-alignment DP. WPF Control.cs declares
    // HorizontalContentAlignment as HorizontalAlignment
    // (System.Windows.Controls.Control), so its members are the SAME set+ordinals
    // as HorizontalAlignment (FrameworkElement.cs:28). The Control DP is
    // registered by its BARE name (Element.cpp:1159, owner "Control"), which is
    // exactly what convert_int32_for_dp sees via dp.property_id().name() — so the
    // domain is keyed by that bare name. A1 A1: SplashWindow's real
    // `HorizontalContentAlignment="Center"` previously threw MalformedValue.
    registry.register_domain("HorizontalContentAlignment",
        make_domain("HorizontalContentAlignment", {
            {"Left", 0}, {"Center", 1}, {"Right", 2}, {"Stretch", 3},
        }));

    // VerticalContentAlignment { Top=0, Center=1, Bottom=2, Stretch=3 } — the
    // Control-base content-alignment DP. WPF Control.cs declares
    // VerticalContentAlignment as VerticalAlignment, so its members are the SAME
    // set+ordinals as VerticalAlignment (FrameworkElement.cs:57). Keyed by the
    // bare DP name (Element.cpp:1163, owner "Control"). A1 A1: SplashWindow's
    // real `VerticalContentAlignment="Center"` previously threw MalformedValue.
    registry.register_domain("VerticalContentAlignment",
        make_domain("VerticalContentAlignment", {
            {"Top", 0}, {"Center", 1}, {"Bottom", 2}, {"Stretch", 3},
        }));

    // Visibility { Visible=0, Hidden=1, Collapsed=2 } (UIElement.cs)
    registry.register_domain("Visibility",
        make_domain("Visibility", {
            {"Visible", 0}, {"Hidden", 1}, {"Collapsed", 2},
        }));

    // Dock { Left=0, Top=1, Right=2, Bottom=3 } (WPF Dock.cs) — REAL A1 fix:
    // `DockPanel.Dock="Left"` previously resolved to 1 (=Top) via the shared
    // Left=1 map. The DP's canonical registry name is the DOTTED "DockPanel.Dock"
    // (Element.cpp:379), which is exactly what convert_int32_for_dp sees via
    // dp.property_id().name() — so the domain is keyed by that dotted name.
    // NO `Fill` member: WPF's Dock enum has exactly Left/Top/Right/Bottom; the
    // LAST-child fill is the positional rule + LastChildFill bool, never a value.
    registry.register_domain("DockPanel.Dock",
        make_domain("Dock", {
            {"Left", 0}, {"Top", 1}, {"Right", 2}, {"Bottom", 3},
        }));

    // IconStretch { None=0, Fill=1, Uniform=2, UniformToFill=3 } (matches WPF Stretch)
    registry.register_domain("IconStretch",
        make_domain("IconStretch", {
            {"None", 0}, {"Fill", 1}, {"Uniform", 2}, {"UniformToFill", 3},
        }));
}

} // namespace vaml
