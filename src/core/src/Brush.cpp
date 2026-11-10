// S364 — SolidColorBrush definitions. See Brush.h.
// S460 — LinearGradientBrush + GradientStop definitions (BRUSH AUTHORING).

#include "vaml/core/Brush.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"

#include <algorithm>
#include <atomic>
#include <vector>

namespace vaml {

namespace {

// Registers the SolidColorBrush "Color" DP once per process (Meyers-static
// guard). Color type, not inherited, default opaque black.
void register_brush_properties()
{
    (void)DependencyProperty::Register(
        "Color", ValueType::Color, "SolidColorBrush",
        PropertyMetadata::WithDefault(Value::Color(vaml::Color{ 0, 0, 0, 255 }),
                                      PropertyFlags::None));
}

const bool kBrushRegistered = [] {
    register_brush_properties();
    return true;
}();

} // namespace

VAML_CORE_EXPORT
SolidColorBrush::SolidColorBrush()
{
    (void)kBrushRegistered; // ensure the global Color DP is registered
}

VAML_CORE_EXPORT
SolidColorBrush::~SolidColorBrush() = default;

VAML_CORE_EXPORT
Brush::~Brush() = default;

VAML_CORE_EXPORT
const std::shared_ptr<SolidColorBrush>& default_black_brush() noexcept
{
    // S643b-1 — the shared default paint brush (opaque black, the WPF default
    // foreground/background/... brush). A Meyer's static so every Brush-typed
    // paint DP's DEFAULT shares ONE immutable object (WPF DefaultValue is a
    // single shared instance). The SolidColorBrush ctor self-registers the Color
    // DP (idempotent); the first call here guarantees the global Color DP exists.
    // S643b-4 (D3): the shared default is the SHARED FROZEN opaque-black brush.
    static const std::shared_ptr<SolidColorBrush> brush =
        shared_frozen_solid_brush(vaml::Color{ 0, 0, 0, 255 });
    return brush;
}

VAML_CORE_EXPORT
std::int32_t next_auto_brush_id() noexcept
{
    // S643b-2 — the single process-lifetime monotonic auto-id allocator.
    // A single std::atomic fetch_add (ADR-018 zero new locks). Seeded at
    // kAutoBrushIdBase so it can never collide with app-registered small ids.
    static std::atomic<std::int32_t> next{kAutoBrushIdBase};
    return next.fetch_add(1, std::memory_order_relaxed);
}

VAML_CORE_EXPORT
SetResult SolidColorBrush::set_color(const vaml::Color& value)
{
    // S643b-4 (D3): a FROZEN brush is immutable — reject the write exactly as
    // WPF's "cannot change a frozen Freezable" (Freezable.cpp throw_if_frozen).
    throw_if_frozen("SolidColorBrush.Color");
    return Set(PropertyId::of("Color"), Value::Color(value));
}

VAML_CORE_EXPORT
vaml::Color SolidColorBrush::color() const
{
    const Value v = Get(PropertyId::of("Color"));
    return v.type() == ValueType::Color ? v.get<vaml::Color>() : vaml::Color{ 0, 0, 0, 255 };
}

VAML_CORE_EXPORT
void freeze_solid_brush(SolidColorBrush& brush)
{
    // WPF Freeze — irreversible; the shared frozen brush is then immutable +
    // safe to share (D3). Must run on the brush's affinity thread (ADR-018).
    brush.Freeze();
}

VAML_CORE_EXPORT
std::shared_ptr<SolidColorBrush> shared_frozen_solid_brush(const vaml::Color& color)
{
    // S643b-4 (D3) — the WPF `IFreezeFreezables.TryGetFreezable` analogue: the
    // SAME color resolves to a SINGLE SHARED, FROZEN SolidColorBrush (Brush.cs:
    // 55-77). A process-lifetime pool (Meyer's static, ADR-018 zero new locks)
    // caches one frozen brush per distinct color; a color seen before returns
    // the SAME instance (identity-shared). The brush is FROZEN on creation so
    // it is immutable + safe for every consumer (renderer reads color() only).
    static std::vector<std::pair<vaml::Color, std::shared_ptr<SolidColorBrush>>> pool;
    for (auto& entry : pool)
    {
        if (entry.first == color)
        {
            return entry.second;
        }
    }
    auto brush = std::make_shared<SolidColorBrush>();
    (void)brush->set_color(color);
    freeze_solid_brush(*brush);
    pool.emplace_back(color, brush);
    return brush;
}

// ---------------------------------------------------------------------------
// S460 — GradientStopHost (the authorable <GradientStop> element). See Brush.h.
// ---------------------------------------------------------------------------

namespace {

// Registers the GradientStopHost "Offset" DP once per process (the "Color" DP is
// already globals, reused by index 0/1 of the S039 shared-name registration; a
// fresh "Color" re-registration would collide — so the stop rides the existing
// global "Color" by name, exactly like SolidColorBrush). The authored
// `<GradientStop Color="..." Offset="...">` attribute keys are PLAIN names that
// resolve via DependencyProperty::TryGet: "Color" (the shared global, Color
// type) + "Offset" (this Double DP).
void register_gradientstop_properties()
{
    (void)DependencyProperty::Register(
        "Offset", ValueType::Double, "GradientStop",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
}

const bool kGradientStopRegistered = [] {
    register_gradientstop_properties();
    return true;
}();

} // namespace

VAML_CORE_EXPORT
GradientStopHost::GradientStopHost()
{
    (void)kGradientStopRegistered;
}

VAML_CORE_EXPORT
GradientStopHost::~GradientStopHost() = default;

VAML_CORE_EXPORT
SetResult GradientStopHost::set_color(const vaml::Color& value)
{
    return Set(PropertyId::of("Color"), Value::Color(value));
}

VAML_CORE_EXPORT
vaml::Color GradientStopHost::color() const
{
    const Value v = Get(PropertyId::of("Color"));
    return v.type() == ValueType::Color ? v.get<vaml::Color>() : vaml::Color{ 0, 0, 0, 255 };
}

VAML_CORE_EXPORT
SetResult GradientStopHost::set_offset(double value)
{
    return Set(PropertyId::of("Offset"), Value::Double(value));
}

VAML_CORE_EXPORT
double GradientStopHost::offset() const
{
    return Get(PropertyId::of("Offset")).get<double>();
}

// ---------------------------------------------------------------------------
// S460 — LinearGradientBrush (BRUSH AUTHORING). See Brush.h.
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
LinearGradientBrush::LinearGradientBrush() = default;

VAML_CORE_EXPORT
LinearGradientBrush::~LinearGradientBrush() = default;

VAML_CORE_EXPORT
void LinearGradientBrush::add_stop(GradientStop stop)
{
    stops_.push_back(stop);
}

VAML_CORE_EXPORT
void LinearGradientBrush::set_stops(std::vector<GradientStop> stops)
{
    stops_ = std::move(stops);
    // A degenerate (empty) stop list is an authoring error the consumer names —
    // never silently filled. The representative_color() seam handles it.
}

VAML_CORE_EXPORT
const std::vector<GradientStop>& LinearGradientBrush::stops() const noexcept
{
    return stops_;
}

VAML_CORE_EXPORT
void LinearGradientBrush::set_start_point(double x, double y)
{
    startX_ = x;
    startY_ = y;
}

VAML_CORE_EXPORT
double LinearGradientBrush::start_x() const noexcept { return startX_; }

VAML_CORE_EXPORT
double LinearGradientBrush::start_y() const noexcept { return startY_; }

VAML_CORE_EXPORT
void LinearGradientBrush::set_end_point(double x, double y)
{
    endX_ = x;
    endY_ = y;
}

VAML_CORE_EXPORT
double LinearGradientBrush::end_x() const noexcept { return endX_; }

VAML_CORE_EXPORT
double LinearGradientBrush::end_y() const noexcept { return endY_; }

VAML_CORE_EXPORT
Color LinearGradientBrush::representative_color() const noexcept
{
    if (stops_.empty())
    {
        return Color{ 0, 0, 0, 255 };
    }
    if (stops_.size() == 1)
    {
        return stops_.front().color;
    }
    // Two endpoints: average the first + last stop colors (byte-wise). S465
    // replaces this with the full vertex-sampled gradient along Start/EndPoint.
    const Color a = stops_.front().color;
    const Color b = stops_.back().color;
    return Color{
        static_cast<std::uint8_t>((static_cast<unsigned>(a.r) + b.r) / 2u),
        static_cast<std::uint8_t>((static_cast<unsigned>(a.g) + b.g) / 2u),
        static_cast<std::uint8_t>((static_cast<unsigned>(a.b) + b.b) / 2u),
        255u,
    };
}

} // namespace vaml
