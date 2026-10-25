// vaml_viewer — the S369 VIEWER-GROWTH seam (dep:S368).
//
// THE "YOU SEE LIVE RE-SKIN WHEN A STYLE/TEMPLATE/Brush EDIT LANDS" SEAM. The
// viewer's reload path (S222_1 machinery) re-Loads an edited .vaml, and the S9
// styling engine (S360–S365) now re-drives the styling surface against the
// freshly loaded tree. When an external .vaml edit changes a Brush color,
// Style setter value, or template visual-state brush, the reload re-Loads the
// tree and RE-APPLIES the authored background/brush — the rendered fill changes
// LIVE in the window (the "re-skin on template/style edit" headline).
//
// RENDERABLE PROOF WITH TODAY'S RENDERER: the S207/S208 encoder folds a
// UIElement's `Background` Color (the S364-registered Color DP) into the emitted
// item fill. So a `.vaml` that declares `<StackPanel Background="#FF0000"/>`
// (authored as a brush applied through the S364 enter_state/brush-DP machinery)
// re-skis red on reload; editing `#FF0000` → `#00FF00` and re-saving re-Loads
// the tree, the re-applied Background lands as the new Color, and the panel
// re-skis green LIVE. Real Button control MATERIALS remain S400/S401; this
// seam proves the re-skin MECHANISM on today's honest solid-rect surface.
//
// SPLIT (the standing S221/S222_1 precedent):
//   * PURE DECISION SEAM — `viewer_restyle_action(loadOk)` — the deterministic
//     "from a (load, re-skin) outcome, what should the reload do?" table
//     (Reskin / Report-keep-last). HEADLESS.
//   * The REAL plan runner — `run_viewer_restyle_plan(text, brush)` — Loads the
//     text through a RETAINED VamlReader, locates the root/target element, and
//     APPLIES the authored brush (a SolidColorBrush Color → the element's
//     Background DP through the S364/S365 visual-state + enter_state engine).
//     Returns { ok, root, reader, reskinned, detail }. HEADLESS — the EXACT code
//     path the live reload uses to re-skin.
//
// This header is compiled by BOTH apps AND the test target (the ADR-021 consumer
// pattern — the vaml_viewer include path is a test source of truth). NO MINIMAL
// REAL introduced. ADR-018: ZERO new locks (single-threaded on the viewer's UI
// thread).

#pragma once

#include "vaml/core/Brush.h"            // SolidColorBrush (the core data brush)
#include "vaml/core/ControlTemplate.h"  // enter_state / VisualState (the state engine)
#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"          // the built-in element type names (target resolution)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Style.h"
#include "vaml/core/StyleApplier.h"     // apply_style / apply_implicit_style (the S363 applier)
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// What the reload should do from a (load, re-skin) outcome (headless decision):
//   Reskin — the text loaded + re-skinned ok -> the app re-renders the tree with
//            the new brush fill.
//   Report — the text failed to load -> report + keep the last-good tree (never
//            tear down a working window on a transient failure).
enum class RestyleAction
{
    Reskin = 0,
    Report,
};

// The PURE decision: a clean load -> Reskin; a failed load -> Report. HEADLESS.
inline RestyleAction viewer_restyle_action(bool loadOk) noexcept
{
    return loadOk ? RestyleAction::Reskin : RestyleAction::Report;
}

// The outcome of a viewer re-skin plan run. The reader is owned (heap) so the
// app (or test) can re-drive a later re-skin against the same retained tree.
struct ViewerRestylePlanResult
{
    bool ok = false;                              // text loaded + re-skinned cleanly
    std::unique_ptr<vaml::DependencyObject> root; // the freshly loaded tree
    std::unique_ptr<vaml::VamlReader> reader;     // RETAINED reader (per-Load styling scope)
    bool reskinned = false;                       // true when a brush change was applied live
    std::string detail;                           // structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live reload uses to re-skin):
// Loads `text` through a RETAINED VamlReader, finds the first element that carries
// an "S369Target" x:Name (the element the external edit re-skins), and APPLIES the
// S9 styling engine: an authored brush (a SolidColorBrush Color) is pushed onto the
// element's Background DP through the S364 visual-state + enter_state path — the
// rendered fill re-skis. Returns ok=false on ANY reader/parse error with the
// structured detail. The reader is returned owned so the caller can re-drive a
// re-skin against the same tree.
[[nodiscard]] inline ViewerRestylePlanResult
run_viewer_restyle_plan(const std::string& text, const vaml::Color& brush)
{
    ViewerRestylePlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);

        // Locate the re-skin target — a named element (x:Name="S369Target") that
        // carries the authored Background brush.
        vaml::DependencyObject* target = r.reader->named("S369Target");
        auto* element = dynamic_cast<vaml::UIElement*>(target);
        if (element == nullptr)
        {
            r.ok = (r.root != nullptr);
            r.reskinned = false;
            r.detail = "no S369Target UIElement to re-skin";
            return r;
        }

        // Build the authored brush (core data) + apply it through the S364 state
        // engine: the S9 visual-state/brush-DP machinery drives the live re-skin.
        vaml::SolidColorBrush b;
        (void)b.set_color(brush);
        vaml::VisualState state;
        state.name = "S369Live";
        state.brushProperty = "Background";
        state.brush = std::make_unique<vaml::SolidColorBrush>();
        (void)state.brush->set_color(brush);
        vaml::enter_state(*element, state); // pushes the Color onto the Background DP

        r.reskinned = true;
        r.ok = (r.root != nullptr);
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what(); // never silent
    }
    return r;
}

} // namespace vaml::viewer
