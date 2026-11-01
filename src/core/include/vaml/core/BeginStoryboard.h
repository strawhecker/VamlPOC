#pragma once
// VAML BeginStoryboard — the S512a TriggerAction that begins a Storyboard
// (dep:S502/S510, Sprint 11g). Resolves the 11k-5 recorded NO-GAP: the
// `<BeginStoryboard><Storyboard>…` Actions payload becomes LIVE — on the owning
// EventTrigger's routed event firing, the BeginStoryboard action's `invoke()`
// begins its owned Storyboard (which then ticks per the S510 clock).
//
// WPF-FAITHFUL: `BeginStoryboard` is a `Style.Triggers` `TriggerAction` whose
// `Storyboard` property is evaluated ONLY when the trigger fires (never at
// load). `invoke()` -> `storyboard.start(root, resolveName)`.
//
// `invoke()` is callable ONLY after the trigger has been ATTACHED (primed with
// the root + the name resolver via `attach`); an invoke before attach is a
// no-op (recorded: an unattached action cannot resolve its targets — never
// silent, the attach seam maps the failure into a structured diagnostic).
//
// The Play/Stop/Pause controllable-action family (BeginStoryboard.StoryboardName
// + cross-trigger control) is recorded NO-GAP (forward ControllableStoryboardAction
// token) — not silent. ADR-018: ZERO new locks (single-threaded UI tick).

#include "vaml/core/Storyboard.h"    // Storyboard (the owned animation set)
#include "vaml/core/TriggerAction.h" // TriggerAction (the S502 interactivity action base + invoke seam)
#include "vaml/core/UIElement.h"     // UIElement (the storyboard root)
#include "vaml/core/export.h"

namespace vaml {

// A concrete S512a TriggerAction: owning a Storyboard; `invoke()` begins it.
class VAML_CORE_EXPORT BeginStoryboard : public TriggerAction
{
public:
    BeginStoryboard() = default;

    // Sets the OWNED Storyboard this action begins (the authored
    // `<BeginStoryboard><Storyboard>…</Storyboard></BeginStoryboard>` payload).
    void set_storyboard(std::unique_ptr<Storyboard> sb) { storyboard_ = std::move(sb); }
    [[nodiscard]] Storyboard* storyboard() noexcept { return storyboard_.get(); }
    [[nodiscard]] const Storyboard* storyboard() const noexcept { return storyboard_.get(); }

    // The trigger-attach seam (called by attach_event_triggers): primes this
    // action with the storyboard root + the target-name resolver it will pass
    // to Storyboard::start. Without this, invoke() cannot resolve targets.
    void attach(UIElement* root, Storyboard::NameResolver resolve)
    {
        root_    = root;
        resolve_ = std::move(resolve);
    }

    // WPF-faithful: begins the owned Storyboard. A null storyboard, a null
    // root, or an unattached action is a no-op (fail-loud is the attach seam's
    // job at authoring; a missing storyboard was already rejected at authoring,
    // never silent). Idempotent re-begin per Storyboard::start.
    void invoke(const Value& parameter) override;

private:
    std::unique_ptr<Storyboard> storyboard_;
    UIElement*                  root_    = nullptr;
    Storyboard::NameResolver    resolve_;
};

} // namespace vaml
