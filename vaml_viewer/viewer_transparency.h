// vaml_viewer — Sprint 11o S595 VIEWER-GROWTH + LIVE AllowsTransparency seam (dep: S594).
//
// The S595 consumer seam: `Window.AllowsTransparency` BECOMES a LIVE CONSUMED
// surface — it flows into S592's per-window clear decision (via the pure
// `clear::decide_clear` seam) + S594's OS-window transparency hint (via the pure
// `window_transparency_hints(featureOn)` seam). ADR-021 consumer path (same as
// S503/S512/11k-6) — compiled by both apps + the presentation test target.
// NO MINIMAL REAL introduced — every seam is real permanent code.
//
// SPLIT (the standing S221/S256/S503 precedent):
//   * The REAL plan runner — `run_viewer_transparency_plan(text)`: loads the
//     SplashWindow-shaped transparent sample through a RETAINED VamlReader,
//     finalizes it, and returns { ok, root, reader, allowsTransparency, detail }.
//   * The RENDER/OS-hint decisions — PURE seams that thread the loaded
//     AllowsTransparency into the library surface:
//       `viewer_transparency_clear(allows, authoredBg :: optional Color)` -> the
//       `clear::ClearValue` (delegates to `clear::decide_clear`), and
//       `viewer_transparency_os_flag(allows)` -> the S594 featureOn flag for the
//       OS-window hint (an opaque/template would keep it false).
//
// ADR-018: zero new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VamlReader.h"
#include "vaml/core/Color.h"                       // vaml::Color
#include <vaml/presentation/VamlRenderer.h>        // clear::decide_clear / clear::ClearValue
#include <vaml/vulkan/PlatformWindow.h>            // window_transparency_hints (S594)

#include <memory>
#include <optional>
#include <string>

namespace vaml::viewer {

// The outcome of the S595 viewer-transparency plan run.
struct ViewerTransparencyPlanResult
{
    bool ok = false;                                 // the transparent sample loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;    // the loaded live transparent tree (a Window root)
    std::unique_ptr<vaml::VamlReader> reader;        // RETAINED — keeps named() + authored props armed
    bool allowsTransparency = false;                 // the consumed `AllowsTransparency` (S595 LIVE surface)
    std::string detail;                              // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs): loads
// the SplashWindow-shaped transparent sample through a RETAINED VamlReader +
// finalize_visual_tree (the S120/S121 hand-off that RENDERS the declared
// children), then READS the Window root's `AllowsTransparency` (Bool, owner
// "Window") — the S595 LIVE-consumed surface. Returns ok=false on ANY reader/
// parse error with the structured detail (never silent).
[[nodiscard]] inline ViewerTransparencyPlanResult
run_viewer_transparency_plan(const std::string& text)
{
    ViewerTransparencyPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.transparent.vaml did not produce a root");
        (void)r.reader->finalize_visual_tree();
        // The Window root is the LIVE transparency consumer surface. A Window
        // registered the Bool AllowsTransparency (owner "Window") at its own
        // construction; read it verbatim (default false).
        if (const auto* w = dynamic_cast<const vaml::Window*>(r.root.get()))
        {
            r.allowsTransparency = w->Get(vaml::PropertyId::of("AllowsTransparency")).get<bool>();
        }
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what(); // never silent
    }
    return r;
}

// The S595 RENDER decision (PURE): threads the loaded AllowsTransparency into
// S592's per-window clear via `clear::decide_clear`. `authoredBg` is the resolved
// (authored-local) Window.Background Color, or std::nullopt when none is authored.
[[nodiscard]] inline vaml::presentation::clear::ClearValue
viewer_transparency_clear(bool allowsTransparency,
                          const std::optional<vaml::Color>& authoredBg) noexcept
{
    const vaml::Color* resolved = authoredBg.has_value() ? &*authoredBg : nullptr;
    return vaml::presentation::clear::decide_clear(allowsTransparency, resolved);
}

// The S595 OS-WINDOW hint decision (PURE): an AllowsTransparency window requests
// the S594 transparency feature (featureOn=true); an opaque window stays false.
// `window_transparency_hints` consumes this: featureOn -> the per-pixel DWM hints.
[[nodiscard]] inline bool viewer_transparency_os_flag(bool allowsTransparency) noexcept
{
    return allowsTransparency;
}

} // namespace vaml::viewer
