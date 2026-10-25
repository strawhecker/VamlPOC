// vaml_viewer — the S329_1 VIEWER-GROWTH seam (dep:S323; Sprint 8).
//
// THE "INPUT WORKS IN THE LIVE VIEWER" SEAM — the anti-grave-whistle proof for
// Sprint 8. The viewer's reload path now ties the S8 INPUT+RENDER chain together:
//   a rendered element's `Click="OnClick"` markup (S322 wires it) -> a routed
//   ClickEvent on the REAL hit-tested tree (S323/S324 hit-test + the S321
//   bubble) -> the app-owned handler runs (S322) -> it updates the VM (S250) ->
//   the live one-way binding (S243/S252) re-pushes -> the S208 encoder repaints
//   -> a VISIBLE state change in the live window.
//
// SPLIT (the standing S221/S222_1/S256 precedent):
//   * PURE DECISION SEAM — `viewer_interactive_drive(...)` (HEADLESS): given a
//     resolved click hit + handler outcome, WHAT does the interactive plan do?
//   * The REAL plan runner — `run_viewer_interactive_plan(text, dim)` (HEADLESS
//     — the exact code the live reload uses): constructs the app-owned VM +
//     handler, Registers the Click event on a RETAINED VamlReader, BINDS the
//     OnClick handler via the S322 class-owner registry, Loads the source text,
//     BindExtensions, BindEvents (wires the markup Click to a live routed
//     handler), and returns { ok, root, vm, reader, clicked, detail }. The
//     caller (the live reload OR a test) can THEN hit-test + raise a Click on a
//     rendered element and observe the handler → VM → binding → target repaint.
//
// RENDERABLE-PROOF NOTE (recorded, NOT silent — the Button constraint): real
// control materials (Text/Button) do not render until S400/S401 (their MINIMAL
// REAL DependencyObject seeds are unresolved). So the interactive proof wires
// `Click="OnClick"` + a `{Binding ...}` on a RENDERED real panel (StackPanel — a
// real UIElement that renders + hit-tests). This satisfies "input works in the
// live viewer" honestly with today's renderable surface; the SAME mechanism
// becomes Button's at S401. ADR-018: ZERO new locks (single-threaded on the
// viewer's UI thread; the deferred Dispatcher/async is Sprint 11a, NOT here).

#pragma once

#include "vaml/core/DependencyObject.h"   // the plan result owns a unique_ptr<DependencyObject>
#include "vaml/core/Element.h"            // UIElement (hit-test + the routed handler surface)
#include "vaml/core/ObservableObject.h"   // ObservableObject (the VM base)
#include "vaml/core/RoutedEvent.h"        // ClickEvent (the S320 routed event)
#include "vaml/core/UIElement.h"          // UIElement (add_routed_handler / RaiseEvent / HitTestAt)
#include "vaml/core/VamlReader.h"         // VamlReader (Load + BindExtensions + BindEvents)
#include "vaml/mvvm/property_bindable.h"  // PROPERTY_BINDABLE (the S247_1 convenience layer)
#include "viewer_binding.h"   // ViewerDemoViewModel (S256 — the app-owned VM the interactive proof reuses)

#include <memory>
#include <string>

namespace vaml::viewer {

// The S329_1 interactive plan outcome, HEADLESS.
//
// The reader/vm (lost_list objects) are owned on the heap and RETAINED so the
// S322 wired Click handler + the S243 LIVE BINDING stay armed after the plan
// returns: a later `HitTestAt`+`RaiseEvent(ClickEvent)` on `root` fires the
// handler → the handler's VM set → the bound target repaints. The reader + VM
// are move-only; holding them by pointer keeps the raw S2-tag targets (into
// `root`) valid.
struct ViewerInteractivePlanResult
{
    bool ok = false;                              // text loaded + bound + wired cleanly
    std::unique_ptr<vaml::DependencyObject> root; // the wired live tree (UIElement root)
    std::shared_ptr<vaml::ObservableObject> vm;   // the app-owned VM (handler's Set target)
    std::unique_ptr<vaml::VamlReader> reader;     // RETAINED — keeps the live bindings + wired Click armed
    std::size_t bound = 0;                        // the S252 BindExtensions count
    std::size_t wired = 0;                        // the S322 BindEvents count (Click handlers wired)
    std::string detail;                           // structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live reload uses): registers
// the Click event + the app-owned OnClick handler on a RETAINED VamlReader, Loads
// the source text, BindExtensions (the S252 {Binding} resolution), BindEvents (the
// S322 markup `Click="OnClick"` -> live routed handler), and reports. `dim` is the
// VM's initial bound Dim (visible). The handler `onClick` is the app-owned
// RoutedEventHandler (S322) that updates the VM (e.g. set_Dim / set_Highlighted).
// ok=false on ANY reader/parse/event error with the structured detail (never silent).
[[nodiscard]] inline ViewerInteractivePlanResult
run_viewer_interactive_plan(const std::string& text, double dim,
                            vaml::RoutedEventHandler onClick)
{
    ViewerInteractivePlanResult r;
    try
    {
        auto vm = std::make_shared<vaml::viewer::ViewerDemoViewModel>(dim);
        r.reader = std::make_unique<vaml::VamlReader>();
        r.reader->register_event("Click");                    // S092/S322: mark Click as an event
        r.reader->set_data_context(vm.get());                 // the app owns the VM; the reader borrows it
        r.reader->bind_class_handler("OnClick", std::move(onClick)); // S322: the class-owner handler
        r.root = r.reader->Load(text);
        r.bound = r.reader->BindExtensions();                 // S252: {Binding ...} -> live bindings
        r.wired = r.reader->BindEvents();                     // S322: Click="OnClick" -> live routed handler
        r.vm = vm;
        r.ok = (r.root != nullptr);
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                                  // never silent
    }
    return r;
}

// The PURE interactive drive decision (HEADLESS): given a Click ROUTED on a hit
// element, what does the live viewer observe? The handler's VM set re-pushes the
// binding -> the target DP changes (a visible repaint). A click that hit NO
// element (off-tree) raises nothing. Returns true iff the plan SHOULD repaint
// (a handler ran and changed a bound target).
inline bool viewer_interactive_repaints(bool clickedHit, bool handlerRan) noexcept
{
    // A repaint happens when a real click hit a wired element AND the handler ran
    // (a change worth folding into the next frame). A miss or a no-op handler is
    // not a repaint.
    return clickedHit && handlerRan;
}

} // namespace vaml::viewer
