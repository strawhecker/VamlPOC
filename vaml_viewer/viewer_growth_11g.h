// vaml_viewer — the S512 VIEWER-GROWTH(11g) seam (dep:S510, S511).
//
// THE 11g SEAM — the sample authors a Storyboard (a transform animation + an
// opacity fade) + a DropShadowEffect, and the LIVE viewer animates visibly.
//
// The loaded tree is the REAL visual tree (sample.11g.vaml); the ANIMATION is
// authored by the seam against the S510 API (TransformAnimation/DoubleAnimation
// into a Storyboard, wired to named targets in the loaded tree) — the same
// authoring surface the S512 reader-markup production produces (the reader
// builds the S510 model from `<Storyboard>`/`<DoubleAnimation>`/...; the seam
// constructs it with the same objects). The seam owns the Storyboard + a
// per-frame clock; the viewer's run_loop ticks it (`storyboard.tick(elapsed)`),
// and each animation frame marks the renderer invalidated so the S208 encoder
// re-emits (retained re-encode). The effect (DropShadow) is attached to a named
// element via the S511 set_effect surface, rendered by the S511 encoder arm.
//
// SPLIT (the S221/S256/S329_1/S503 precedent):
//   * The REAL plan runner — `run_viewer_growth_11g_plan(text)`: loads the
//     sample through a RETAINED VamlReader (finalize_visual_tree), wires the
//     authored Storyboard (named transforms/fades) + the DropShadowEffect, and
//     returns { ok, root, reader, storyboard, detail }. The reader + storyboard
//     are RETAINED so the live bindings + the per-frame tick stay armed after
//     the plan returns.
//   * The RENDER-PLAN seam — `viewer_growth_11g_render_plan(root, storyboard,
//     elapsed, out)`: arranges the root, advances the storyboard tick to
//     `elapsed`, and encodes into `out` (the RENDER-SURFACE proof: the animated
//     RenderTransform/Opacity + the DropShadow shadow quad land in the draw
//     list).
//
// This header is compiled by BOTH apps AND the presentation test target (the
// ADR-021 consumer pattern). NO MINIMAL REAL. ADR-018: ZERO new locks
// (single-threaded per-frame tick on the viewer's UI thread).

#pragma once

#include "vaml/core/Animation.h"       // TransformAnimation / DoubleAnimation (S510)
#include "vaml/core/DependencyObject.h"
#include "vaml/core/Effect.h"          // DropShadowEffect (S511)
#include "vaml/core/Storyboard.h"      // Storyboard (S510)
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"      // VamlReader (Load + named() + finalize_visual_tree)
#include <vaml/presentation/VamlRenderer.h> // S212 — encode_visual_slice + default_visual_of

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11g viewer growth plan run.
struct ViewerGrowth11gPlanResult
{
    bool ok = false;                                // text loaded + storyboard wired
    std::unique_ptr<vaml::DependencyObject> root;   // the loaded live tree
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED — keeps named() + bindings alive
    std::unique_ptr<vaml::Storyboard> storyboard;   // the authored S510 animation set
    std::string detail;                             // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): loads
// the sample through a RETAINED VamlReader (finalize_visual_tree so the visual
// tree + named() registry are live), then AUTHORS the 11g animation surface
// against the loaded tree:
//   * a TransformAnimation (translate the named "Ball" +50 X over 2s),
//   * a DoubleAnimation (fade the named "Ball" Opacity 1 -> 0.3 over 2s),
//   * a DropShadowEffect attached to the named "Ball" element (S511).
// The storyboard's name resolver maps onto reader->named() (the S092 registry).
// Returns ok=false on ANY reader/parse error with the structured detail.
[[nodiscard]] inline ViewerGrowth11gPlanResult
run_viewer_growth_11g_plan(const std::string& text)
{
    ViewerGrowth11gPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        (void)r.reader->finalize_visual_tree();
        if (r.root == nullptr)
            throw std::runtime_error("sample.11g.vaml did not produce a root");

        vaml::UIElement* rootUi = dynamic_cast<vaml::UIElement*>(r.root.get());
        if (rootUi == nullptr)
            throw std::runtime_error("sample.11g.vaml root is not a UIElement");

        // Author the S510 Storyboard (the exact surface the reader-markup
        // production constructs from <Storyboard>/<DoubleAnimation>/...).
        r.storyboard = std::make_unique<vaml::Storyboard>();
        auto move = std::make_unique<vaml::TransformAnimation>();
        move->set_target_name("Ball");
        move->set_channel(vaml::TransformChannel::TranslateX);
        move->set_from(0.0);
        move->set_to(50.0);
        move->set_duration(2.0);
        auto fade = std::make_unique<vaml::DoubleAnimation>();
        fade->set_target_name("Ball");
        fade->set_target_property(vaml::PropertyId::of("Opacity"));
        fade->set_from(1.0);
        fade->set_to(0.3);
        fade->set_duration(2.0);
        r.storyboard->add_child(std::move(move));
        r.storyboard->add_child(std::move(fade));

        // BEGIN the storyboard: prime it with the root + a name resolver that
        // maps each animation's target_name onto the S092 x:Name registry. The
        // viewer's per-frame render-plan then ticks it (storyboard.tick).
        r.storyboard->start(rootUi, [reader = r.reader.get()](const std::string& name)
                                    -> vaml::UIElement* {
            vaml::DependencyObject* n = reader->named(name);
            return n ? dynamic_cast<vaml::UIElement*>(n) : nullptr;
        });

        // Attach the DropShadowEffect to the named "Ball" (S511).
        vaml::UIElement* ball = r.reader->named("Ball")
            ? dynamic_cast<vaml::UIElement*>(r.reader->named("Ball")) : nullptr;
        if (ball != nullptr)
        {
            auto ds = std::make_unique<vaml::DropShadowEffect>();
            (void)ds->set_blur_radius(5.0);
            (void)ds->set_shadow_depth(5.0);
            (void)ds->set_direction(315.0);
            ball->set_effect(std::move(ds));
        }

        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                           // never silent
    }
    return r;
}

// The RENDER-PLAN seam: advances the authored Storyboard to `elapsed` seconds
// (the per-frame tick the viewer's run_loop drives) and encodes the arranged
// tree into `out` — the S511 encoder emits the DropShadow shadow quad + the
// animated RenderTransform/Opacity. Returns the number of DrawItems emitted.
[[nodiscard]] inline std::size_t
viewer_growth_11g_render_plan(vaml::DependencyObject* root, vaml::Storyboard* storyboard,
                              double elapsed,
                              std::vector<vaml::vulkan::ui::DrawItem>& out,
                              float width = 280.0f, float height = 240.0f)
{
    if (root == nullptr)
        return 0;
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
        return 0;
    // The per-frame tick (the S510 change-driven evaluation).
    if (storyboard != nullptr)
        storyboard->tick(elapsed);
    // Arrange the root so final_rect()/DesiredSize are real (S123/S124).
    ui->Measure(vaml::Size{width, height});
    ui->Arrange(vaml::Rect{0, 0, width, height});
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
