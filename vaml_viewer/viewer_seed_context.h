// vaml_viewer — Sprint 11q SEED-CONTEXT seam (S620, dep:S605). VIEWER-ONLY,
// ZERO lib change.
//
// WHY (the failure mode this sprint fixes): `VamlReader::BindExtensions()` builds
// a `BindingExpression` for every deferred `{Binding ...}` against `dataContext_`.
// When the app installs NO data context (the viewer's generic reload path only
// binds ONE hardwired property — S256's `ViewerDemoViewModel.Dim -> Opacity` via
// viewer_binding.h), `dataContext_` is null and every transfer is a Class B
// `NotReadable` — the bound element is left at its DP default, which renders
// black/transparent. The "clean" external corpus files (Splash/OverlayWindow/…)
// that author `{Binding ...}`-backed text/brushes resolve to nothing and paint
// nothing.
//
// THIS SEAM (S620 root B1) injects a SEED/placeholder data-context BEFORE
// BindExtensions(): a plain `ObservableObject` whose properties are derived
// DETERMINISTICALLY from the authored binding paths, so a bound-but-data-empty
// document renders VISIBLE placeholder content instead of black/transparent.
// The S252 first-transfer lands the seed on the target DP; the retained render
// path (S212 encoder) shows it.
//
// VIEWER-ONLY: the lib's binding engine (BindExtensions / BindingExpression /
// ObservableObject) is UNTOUCHED. The viewer merely populates the reader's data
// context before binding — a consumer of existing public ADR-021 surface
// (`set_data_context`, `deferred_extensions()`, `parse_markup_binding`,
// `ObservableObject::Set`). NO MINIMAL REAL — these are permanent, kept-and-
// extended ADR-021 consumer seams; the S440–S449 app-shell runway is the named
// forward consumer.
//
// SPLIT (the standing S256/S221 precedent):
//   * PURE DECISION SEAM — `viewer_seed_render_action(ok)` — the deterministic
//     "from a (load, seed) outcome, what should the reload do?" table
//     (Render / Report-keep-last). HEADLESS.
//   * The REAL plan runner — `run_viewer_seed_plan(text, seedEnabled)` — Loads
//     the text through a RETAINED VamlReader, SEEDS the app-owned
//     ObservableObject from the deferred single-segment {Binding} paths (the
//     property name AS TEXT), sets it as the data context, BindExtensions(), and
//     returns { ok, root, seedVm, reader, bound, seeded, detail }. HEADLESS.
//
// S620 SCOPE (recorded, not silent): the seed resolves SINGLE-SEGMENT binding
// paths to their leaf property-name-as-text. Multi-hop (`{Binding Employee.Name}`)
// is a documented seed limitation for this step (the corpus seed files bind
// single-segment names) — recorded forward, never a silent backfill. Type-aware
// brush/opacity/color seeding is S621 (root B2), not here.
//
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/BindingMarkup.h"      // parse_markup_binding (the {Binding} source grammar)
#include "vaml/core/Brush.h"              // S643b: SolidColorBrush (the Brush-object seed)
#include "vaml/core/Color.h"              // S621: Color (the deterministic opaque placeholder fill)
#include "vaml/core/DependencyObject.h"   // the load root (unique_ptr<DependencyObject>)
#include "vaml/core/DependencyProperty.h" // TryGet (the type-aware target read)
#include "vaml/core/GridLength.h"         // S621: GridLength::Pixel (a non-zero track default)
#include "vaml/core/ObservableObject.h"   // ObservableObject (the SEED data-context VM)
#include "vaml/core/PropertyId.h"         // PropertyId::of
#include "vaml/core/ResourceDictionary.h" // S643a: register_element_brush (seed brush registration)
#include "vaml/core/Thickness.h"          // S621: Thickness (a 1px inset default)
#include "vaml/core/UIElement.h"          // S643a: the seed-brush registration target (ext.target)
#include "vaml/core/Value.h"              // Value::String / Value::Color / Value::Double / Value::BrushId
#include "vaml/core/VamlReader.h"         // VamlReader (Load + set_data_context + BindExtensions)

#include <cmath>     // S621: std::fabs / std::fmod (the deterministic hue->RGB)
#include <cstdint>   // S621: std::uint32_t (the FNV hash)
#include <memory>
#include <string>
#include <string_view>

namespace vaml::viewer {

// What the reload should do from a (load, seed) outcome (headless decision):
//   Render — the text loaded AND (when seeding) seeded-bindable -> the app
//            re-renders the seeded bound tree (visible placeholder layout).
//   Report — the text failed to load/seed -> report + keep the last-good tree
//            (never tear down a working window on a transient failure).
enum class SeedRenderAction
{
    Render = 0,
    Report,
};

// The PURE decision: given whether the source text loaded+bound cleanly, what
// should the live reload do? A clean load -> Render; a failed load -> Report.
// HEADLESS + deterministic (mirrors S256's viewer_bind_render_action).
inline SeedRenderAction viewer_seed_render_action(bool ok) noexcept
{
    return ok ? SeedRenderAction::Render : SeedRenderAction::Report;
}

// The outcome of a viewer seed plan run.
//
// The reader + seed VM are retained so the S243 LIVE WIRE STAYS ALIVE after the
// plan returns (the S256 retention precedent): the S252 bindings are reader-
// owned, and only a live reader keeps the source→target subscription armed. The
// seed VM is a shared_ptr the app owns (the S250 app-owned pattern; the reader
// borrows it). `seeded` is the number of single-segment {Binding} paths seeded;
// `bound` is the BindExtensions bound-row count (the S252 S2-tag resolution).
struct ViewerSeedPlanResult
{
    bool ok = false;                                // text loaded (+ seeded/bound when enabled) cleanly
    std::unique_ptr<vaml::DependencyObject> root;   // the bound live tree (UIElement root)
    std::shared_ptr<vaml::ObservableObject> seedVm; // the SEED data-context VM (owned)
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED reader — keeps the live bindings armed
    std::size_t seeded = 0;                         // single-segment {Binding} paths seeded (S620)
    std::size_t bound  = 0;                         // the S252 BindExtensions bound-row count
    std::string detail;                             // the structured failure reason (never silent)
};

// S620 — seeds a plain ObservableObject from the reader's deferred SINGLE-SEGMENT
// {Binding} extensions: for each well-formed `{Binding <path>}` whose path has NO
// dot (a leaf property name), `vm.Set(PropertyId::of(path), Value::String(path))`
// — the property name AS TEXT. (A multi-hop path is NOT seeded here — recorded
// S620 scope.) Returns the number of paths seeded. Deterministic: same doc ->
// same seed. Pure over the reader's deferred registry.
[[nodiscard]] inline std::size_t
seed_string_bindings(vaml::VamlReader& reader, vaml::ObservableObject& vm)
{
    std::size_t n = 0;
    for (const vaml::DeferredExtension& ext : reader.deferred_extensions())
    {
        const vaml::BindingMarkup m = vaml::parse_markup_binding(ext.source);
        if (m.kind != vaml::MarkupExtensionKind::Binding)
        {
            continue; // not this layer's row (Command/Static/StaticResource...)
        }
        if (m.malformed || m.path.empty())
        {
            continue; // a malformed {Binding} is BindExtensions' Class A error, not ours
        }
        // S620 scope: SINGLE-SEGMENT path only — the leaf property name IS the
        // seed key (a multi-hop path would need a nested object, out of scope).
        if (m.path.find('.') != std::string_view::npos)
        {
            continue;
        }
        // A leaf path -> seed the property name AS TEXT (S620 root B1). If the
        // same name is authored on multiple targets, Set is idempotent on the
        // VM (real-change discipline) — the single seed value serves them all.
        const vaml::PropertyId id = vaml::PropertyId::of(m.path);
        (void)vm.Set(id, vaml::Value::String(std::string(m.path)));
        ++n;
    }
    return n;
}

// S621 (dep:S620) — root B2 extension of the SAME seam: TYPE-AWARE seed values.
// The S620 seed is TEXT-ONLY; a bound-but-data-empty doc whose {Binding} targets
// a BRUSH/OPACITY/COLOR DP (Background/Foreground/Fill/Opacity …) still resolves
// to nothing with the text seed (the S252 transfer can't push a String onto a
// BrushId/Color/Double DP — wrong-tag -> SetTargetFailed -> the target is left at
// its transparent/black default). S621 makes the seed TYPE-AWARE on the TARGET
// DP's DECLARED bound type so brush/opacity/color-bound values render VISIBLE
// (never transparent/black). VIEWER-ONLY — the lib binding engine is untouched.
//
// DETERMINISM (the same input -> the same seed): the non-text seeds derive from
// a stable FNV-1a hash of the property NAME (not a random value), mapped to a
// SATURATED HUE at FULL opacity (Color) or a NON-ZERO id (BrushId/Int32/Int64).
// So the same {Binding Name} always produces the same visible value on every
// run — reproducible + inspectable.
//
// WHY A SOLID COLOR (NOT a BrushId) for the background/brush case:
//   * `resolve_foreground_color` resolves a NON-ZERO BrushId through the S360
//     three-scope walk to a registered SolidColorBrush — but the SEED VM cannot
//     register a tree-scoped brush (that is S361/S453 authored-resource
//     territory). A non-ZERO BrushId with NO registered brush falls back to
//     opaque BLACK + a structured diagnostic (never silent — but still black).
//   * The base classifier reads `Background` as a **Color**-typed DP (the S364
//     visual-state shared register); seeding a VISIBLE `Value::Color` (full
//     opacity, deterministic hue) on that target makes the element render a
//     visible opaque fill through the retained render path — ZERO lib change.
//   * A `Foreground`/`Fill`/`BorderBrush` (BrushId-typed) target receives a
//     deterministic NON-ZERO BrushId (never the default transparent/black 0) so
//     it has a stable, inspectable placeholder identity; full registered-brush
//     rendering is S361/S453-authored-resource territory (recorded NO-GAP).
//
// The seed VALUES delivered at S621 (per the target DP's declared bound type):
//   * String     -> the property name as text (S620, unchanged);
//   * Color      -> a deterministic OPAQUE visible color (hue from the name hash);
//   * BrushId    -> a deterministic NON-ZERO id (never the transparent/black 0);
//   * Double     -> Opacity -> 1.0 (fully visible); other lengths -> a
//                   deterministic non-zero value;
//   * Int32/Int64-> a deterministic non-zero id;
//   * Thickness  -> a 1px inset;  GridLength -> a 16px pixel track.
// Bool / UntypedPtr / Matrix2D / Null -> NO seed (a bool at its default is
// already visible; an object pointer needs a real object, out of the seed scope).

// S621 — a stable FNV-1a 32-bit hash of a property NAME. Deterministic + cheap;
// the SAME name always produces the SAME seed value. `kindSalt` folds in a
// per-kind salt so a color-derived value stays distinct from a brush-id-derived
// value even for the same name (they are different Value tags).
[[nodiscard]] inline std::uint32_t seed_name_hash(std::string_view name,
                                                  std::uint32_t kindSalt) noexcept
{
    std::uint32_t h = 2166136261u;
    for (const char c : name)
    {
        h ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
        h *= 16777619u;
    }
    h ^= kindSalt;
    h *= 16777619u;
    return h ? h : 1u;   // never a degenerate zero seed
}

// S621 — maps a stable hash to a SATURATED HUE at FULL opacity. The hue is a
// deterministic function of `h`; the returned Color is fully OPAQUE (a=255) so a
// brush/background-bound element renders visibly. Pure + deterministic (same h ->
// same color). HSV->RGB on a hue in [0,1).
[[nodiscard]] inline vaml::Color seed_visible_color(std::uint32_t h) noexcept
{
    // A deterministic saturated hue in [0, 1): fold the low 24 bits through the
    // golden-ratio (0.6180339887) wrap — a well-spread, reproducible chroma.
    const double t = std::fmod(static_cast<double>(h & 0x00FFFFFFu) * 0.6180339887498949, 1.0);
    const double s = 0.75;
    const double v = 0.85;
    const double i = std::floor(t * 6.0);
    const double f = t * 6.0 - i;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - f * s);
    const double t2 = v * (1.0 - (1.0 - f) * s);
    double rr = v, gg = v, bb = v;
    switch (static_cast<int>(i) % 6)
    {
    case 0: rr = v; gg = t2; bb = p; break;
    case 1: rr = q; gg = v; bb = p; break;
    case 2: rr = p; gg = v; bb = t2; break;
    case 3: rr = p; gg = q; bb = v; break;
    case 4: rr = t2; gg = p; bb = v; break;
    default: rr = v; gg = p; bb = q; break;
    }
    vaml::Color c;
    c.r = static_cast<std::uint8_t>((rr * 255.0) + 0.5);
    c.g = static_cast<std::uint8_t>((gg * 255.0) + 0.5);
    c.b = static_cast<std::uint8_t>((bb * 255.0) + 0.5);
    c.a = 255;   // FULL opacity — the S621 "never transparent/black" contract
    return c;
}

// S621 — computes the TYPE-AWARE seed Value for a {Binding <leaf>} whose target
// DP (the FIRST target that binds the leaf) declares `boundType`. This is what
// B2 adds over B1's text-only seed. Pure + deterministic.
[[nodiscard]] inline vaml::Value
seed_value_for_type(vaml::ValueType boundType, std::string_view leaf) noexcept
{
    switch (boundType)
    {
    case vaml::ValueType::String:
        // B1 (S620) unchanged: the property name as text.
        return vaml::Value::String(std::string(leaf));

    case vaml::ValueType::Color:
        // B2: a deterministic OPAQUE visible color (never transparent/black).
        return vaml::Value::Color(seed_visible_color(seed_name_hash(leaf, 0x01u)));

    case vaml::ValueType::BrushId:
        {
            // B2: a deterministic NON-ZERO brush id (never the transparent/black 0).
            const std::uint32_t h = seed_name_hash(leaf, 0x02u);
            return vaml::Value::BrushId(static_cast<std::int32_t>(h & 0x7FFFFFFFu));
        }

    case vaml::ValueType::Double:
        {
            // B2: Opacity -> 1.0 (fully visible); other Double targets -> a
            // deterministic non-zero positive value (a visible extent).
            const std::uint32_t h = seed_name_hash(leaf, 0x03u);
            if (leaf == "Opacity")
                return vaml::Value::Double(1.0);   // fully visible
            return vaml::Value::Double(8.0 + static_cast<double>(h % 24u));
        }

    case vaml::ValueType::Int32:
        return vaml::Value::Int32(
            static_cast<std::int32_t>(seed_name_hash(leaf, 0x04u) & 0x7FFFFFFFu));

    case vaml::ValueType::Int64:
        return vaml::Value::Int64(
            static_cast<std::int64_t>(seed_name_hash(leaf, 0x05u) & 0x7FFFFFFFu));

    case vaml::ValueType::Thickness:
        return vaml::Value::Thickness(vaml::Thickness{ 1.0f, 1.0f, 1.0f, 1.0f });

    case vaml::ValueType::GridLength:
        return vaml::Value::GridLength(vaml::GridLength::Pixel(16.0f));

    default:
        // Bool / UntypedPtr / Matrix2D / Null -> no seed (already visible at a
        // default, or an object slot out of the seed context's scope).
        return vaml::Value::Null();
    }
}

// S621 — the TYPE-AWARE SEED: like seed_string_bindings (B1) but for EVERY
// single-segment {Binding <leaf>} it ALSO consumes the FIRST target's DECLARED
// DP bound type (DependencyProperty::TryGet(ext.key)) and seeds the leaf with the
// type-appropriate visible value (seed_value_for_type). Returns the number of
// leaf paths seeded. Deterministic: same doc -> same seed.
[[nodiscard]] inline std::size_t
seed_typed_bindings(vaml::VamlReader& reader, vaml::ObservableObject& vm)
{
    std::size_t n = 0;
    for (const vaml::DeferredExtension& ext : reader.deferred_extensions())
    {
        const vaml::BindingMarkup m = vaml::parse_markup_binding(ext.source);
        if (m.kind != vaml::MarkupExtensionKind::Binding)
        {
            continue;
        }
        if (m.malformed || m.path.empty())
        {
            continue;
        }
        if (m.path.find('.') != std::string_view::npos)
        {
            continue;   // multi-hop: recorded S620/S621 scope, NOT seeded
        }
        // The FIRST target that binds this leaf determines the seed's bound type.
        vaml::ValueType boundType = vaml::ValueType::String;   // B1 default
        if (const vaml::DependencyProperty* dp = vaml::DependencyProperty::TryGetByName(ext.key))
        {
            boundType = dp->metadata().boundType();
        }
        // S643a/S643b (WPF paint-DP compliance): a paint-bound target must seed
        // a REAL brush so the render-time read resolves the seeded fill.
        //   * ValueType::Brush  (S643b-1 D1 — the enumerated paint DPs): seed a
        //     fresh SolidColorBrush OBJECT (Value::Brush) with the seed color.
        //   * ValueType::BrushId (the legacy internal render seam — shared
        //     Control Background/BorderBrush): register a BrushId via
        //     register_element_brush (fresh-object-per-conversion).
        // The seed color is deterministic + opaque + non-black (seed_visible_color).
        vaml::Value seed = seed_value_for_type(boundType, m.path);
        if (boundType == vaml::ValueType::Brush)
        {
            const vaml::Color seedColor =
                seed_visible_color(seed_name_hash(m.path, 0x02u));
            auto brush = std::make_shared<vaml::SolidColorBrush>();
            (void)brush->set_color(seedColor);
            seed = vaml::Value::Brush(std::move(brush));
        }
        else if (boundType == vaml::ValueType::BrushId)
        {
            if (const auto* ui = dynamic_cast<const vaml::UIElement*>(ext.target))
            {
                const vaml::Color seedColor =
                    seed_visible_color(seed_name_hash(m.path, 0x02u));
                // S643a: register_element_brush mutates the element's resources
                // dict (writes a "Brush:<id>" Object entry), so it needs a
                // non-const UIElement*. The reader owns the live tree element;
                // the const-ness here is only the DeferredExtension view's, so a
                // documented const_cast is the correct seam (not UB — the object
                // truly is mutable).
                auto* mut = const_cast<vaml::UIElement*>(ui);
                const std::int32_t registeredId =
                    vaml::register_element_brush(*mut, seedColor);
                seed = vaml::Value::BrushId(registeredId);
            }
        }
        if (seed.isNull())
        {
            continue;   // no seed for this bound type (Bool/pointer/etc.)
        }
        const vaml::PropertyId id = vaml::PropertyId::of(m.path);
        (void)vm.Set(id, seed);
        ++n;
    }
    return n;
}

// THE REAL PLAN RUNNER (headless — the exact code the live reload uses to render
// a bound-but-data-empty document): Loads the source text through a RETAINED
// VamlReader, SEEDS the app-owned ObservableObject data-context from the deferred
// single-segment {Binding} paths (typed — S620 text + S621 type-aware values,
// when seedEnabled), set_data_context + BindExtensions() (the S252 markup
// resolution), and reports. Returns ok=false on ANY reader/parse error with the
// structured detail (never silent). The reader + seed VM are returned owned so
// the S243 live wire can be re-driven.
[[nodiscard]] inline ViewerSeedPlanResult
run_viewer_seed_plan(const std::string& text, bool seedEnabled)
{
    ViewerSeedPlanResult r;
    try
    {
        // The reader is created once and RETAINED (heap) so BindExtensions' live
        // bindings (reader-owned, per-Load) survive the plan's return — a live
        // S243 subscription needs a live reader to re-push a later VM change.
        r.reader = std::make_unique<vaml::VamlReader>();
        r.seedVm = std::make_shared<vaml::ObservableObject>();
        r.root   = r.reader->Load(text);               // records the deferred S2 tags
        if (r.root == nullptr)
        {
            throw std::runtime_error("load produced no root");
        }
        // SEED the data-context from the deferred single-segment {Binding} paths
        // BEFORE BindExtensions — this is the S620+S621 injection (viewer-only).
        if (seedEnabled)
        {
            r.seeded = seed_typed_bindings(*r.reader, *r.seedVm);
        }
        r.reader->set_data_context(r.seedVm.get());    // the app owns the VM; the reader borrows it
        r.bound = r.reader->BindExtensions();          // S252: resolve {Binding} -> live bindings
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                           // never silent
    }
    return r;
}

} // namespace vaml::viewer
