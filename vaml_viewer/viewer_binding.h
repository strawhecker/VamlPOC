// vaml_viewer — the S256 VIEWER-GROWTH seam (dep:S253).
//
// THE "YOU SEE DATA BINDING APPEAR IN THE LIVE WINDOW" SEAM. The viewer's
// reload path (S222_1 machinery) now constructs a ViewModel (a real
// vaml::ObservableObject + PROPERTY_BINDABLE members, plain ctor per S250/S251
// — NO DI, NO container: the app owns the VM) and binds it as the VamlReader
// data context, so markup `{Binding <path>}` (S252) resolves onto the LIVE tree
// the window renders.
//
// RENDERABLE PROOF WITH TODAY'S RENDERER: the S207/S208 encoder folds a
// UIElement's `effective_opacity()` (the S120 Opacity DP, inherited) into the
// emitted item alpha. So a `.vaml` that declares `<StackPanel Opacity="{Binding
// Dim}"/>` binds the VM's `Dim` (double, [0,1]) and — when the reload binds +
// renders — the panel visibly dims in the live window (BindExtensions applies
// the FIRST transfer, the S242 first-push landing the bound Dim onto the target
// Opacity DP, which the encoder honors). Real control MATERIALS (Text/Button ->
// S400/S401) do not exist yet; Opacity dimming is the honest visible data-
// binding demonstration available on TODAY's surface (recorded NO-GAP, not
// silent).
//
// SPLIT (the standing S221/S222_1 precedent):
//   * PURE DECISION SEAM — `viewer_bind_render_action(loadOk)` — the
//     deterministic "from a (load, bind) outcome, what should the reload do?"
//     table (Render / Report-keep-last). HEADLESS.
//   * The REAL plan runner — `run_viewer_binding_plan(text, dim)` — constructs
//     the app-owned VM + injected service (plain ctor), Loads the text through a
//     RETAINED VamlReader, set_data_context + BindExtensions (the S252 S2-tag
//     resolution), and returns { ok, root, vm, bound, detail }. This is the
//     EXACT code path the live reload uses to bind; HEADLESS (no window/runtime).
//
// This header is compiled by BOTH apps AND the test target (the ADR-021
// consumer pattern — the vaml_viewer include path is a test source of truth;
// see the S223 drop_reopen.h precedent). NO MINIMAL REAL introduced — the VM +
// the plan runner are REAL permanent code, kept-and-extended by the S400/S443
// control-material sprints. ADR-018: ZERO new locks (single-threaded on the
// viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"   // the LoadResult owns a unique_ptr<DependencyObject>
#include "vaml/core/ObservableObject.h"   // ObservableObject (the VM base)
#include "vaml/core/VamlReader.h"         // VamlReader (Load + set_data_context + BindExtensions)
#include "vaml/mvvm/property_bindable.h"  // PROPERTY_BINDABLE (the S247_1 convenience layer)

#include <memory>
#include <string>

namespace vaml::viewer {

// ---------------------------------------------------------------------------
// The S250/S251 app-constructed domain service (plain ctor wiring, NO DI).
// This is the "app's own service injected via plain constructor" half of the
// S250 "engineer wires, we bind" headline — the viewer's demo VM takes it in by
// value (a plain constructor arg), exactly the pattern S251 proves at the
// binding level. The VM drives a property the S252 markup binds.
// ---------------------------------------------------------------------------
class ViewerDemoService
{
public:
    // The service's one piece of state — the "room dim" the demo VM surfaces as
    // its (double, [0,1]) Dim bindable. Plain constructor; no container.
    explicit ViewerDemoService(double dim) noexcept : dim_(dim) {}
    [[nodiscard]] double dim() const noexcept { return dim_; }

private:
    double dim_;
};

// ---------------------------------------------------------------------------
// The S256 app-constructed ViewModel (a REAL ObservableObject + PROPERTY_BINDABLE
// over the S240/S241 core). The markup `{Binding Dim}` (S252) resolves its `Dim`
// property against the reader's data context; the bound value lands on the target
// UIElement's Opacity DP (the visible dim, folded by the S208 encoder).
// ---------------------------------------------------------------------------
class ViewerDemoViewModel final : public ObservableObject
{
public:
    // Plain ctor wiring: constructs the injected service by VALUE from the same
    // argument the caller passes (the S250 pattern — no library container). The
    // VM's bindable Dim is initialized from the service's dim.
    explicit ViewerDemoViewModel(double dim) : service_(dim)
    {
        (void)set_Dim(service_.dim());
    }

    // The bound (double, [0,1]) dim property — `{Binding Dim}` resolves this.
    PROPERTY_BINDABLE(double, Dim);

    // The injected service (S250: the engineer wired it; the VM owns it).
    [[nodiscard]] const ViewerDemoService& service() const noexcept { return service_; }

private:
    ViewerDemoService service_;
};

// What the reload should do from a (load, bind) outcome (headless decision):
//   Render  — the text loaded AND bound ok -> the app re-renders the bound tree.
//   Report  — the text failed to load -> report + keep the last-good tree (never
//             tear down a working window on a transient failure).
// (There is no "KeepLast on a clean bind" arm — a clean bind always renders.)
enum class BindRenderAction
{
    Render = 0,
    Report,
};

// The PURE decision: given whether the source text loaded + bound cleanly, what
// should the live reload do? A clean load -> Render; a failed load -> Report.
// HEADLESS + deterministic.
inline BindRenderAction viewer_bind_render_action(bool loadOk) noexcept
{
    return loadOk ? BindRenderAction::Render : BindRenderAction::Report;
}

// The outcome of a viewer binding plan run.
//
// The reader is owned on the HEAP and returned so the S243 LIVE WIRE STAYS ALIVE
// after the plan returns: the S252 bindings are reader-owned, and only a live
// reader keeps the source→target subscription armed — so the app (or the test)
// can re-drive `vm->set_Dim(...)` and observe the bound Opacity DP re-push
// (the "data binding appears + updates in the live window" headline). The reader
// is move-only; holding it via unique_ptr keeps the S2-tag targets (raw pointers
// into `root`, which the plan also owns) valid.
struct ViewerBindingPlanResult
{
    bool ok = false;                                // text loaded + bound cleanly
    std::unique_ptr<vaml::DependencyObject> root;   // the bound live tree (UIElement root)
    std::shared_ptr<ViewerDemoViewModel> vm;        // the app-owned VM (kept alive)
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED reader — keeps the live bindings armed
    std::size_t bound = 0;                          // the S252 BindExtensions bound-row count
    std::string detail;                             // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live reload uses to bind):
// constructs the app-owned VM + injected service, Loads the source text through a
// RETAINED VamlReader, set_data_context(vm) + BindExtensions() (the S252 markup
// resolution — the bound Dim lands on the target Opacity DP), and reports.
// Returns ok=false on ANY reader/parse error with the structured detail (never
// silent). The reader + VM are returned owned so the S243 live wire can be
// re-driven by the caller (a later `set_Dim` re-pushes the visible dim).
[[nodiscard]] inline ViewerBindingPlanResult
run_viewer_binding_plan(const std::string& text, double dim)
{
    ViewerBindingPlanResult r;
    try
    {
        // The reader is created once and RETAINED (heap) so BindExtensions' live
        // bindings (reader-owned, per-Load) survive the plan's return — a live
        // S243 subscription needs a live reader to re-push a later VM change.
        r.reader = std::make_unique<vaml::VamlReader>();
        r.vm = std::make_shared<ViewerDemoViewModel>(dim);
        r.reader->set_data_context(r.vm.get());       // the app owns the VM; the reader borrows it
        r.root = r.reader->Load(text);                // Load records the deferred S2 tags
        r.bound = r.reader->BindExtensions();         // S252: resolve {Binding Dim} -> live bindings
        r.ok = (r.root != nullptr);
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                          // never silent
    }
    return r;
}

} // namespace vaml::viewer
