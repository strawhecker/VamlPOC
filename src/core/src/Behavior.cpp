// S502 — Behavior: the base behavior lifecycle (attach/detach + associated object).
// See Behavior.h for the design.

#include "vaml/core/Behavior.h"

#include <stdexcept>

namespace vaml {

void Behavior::attach_to(UIElement* element)
{
    if (element == nullptr)
    {
        return; // nothing to attach to — no-op (never throw for a null element)
    }
    // A behavior has exactly ONE associated object: a behavior attached to
    // another element must be detached first (WPF-faithful — the S170
    // single-owner discipline). Fail loud, never silently steal.
    if (attached_ && associated_ != element)
    {
        throw std::logic_error("Behavior::attach_to: behavior already attached to another element");
    }
    associated_ = element;
    attached_   = true;
    on_attach(); // concrete hook (may subscribe the element's routed events)
}

void Behavior::detach() noexcept
{
    if (!attached_)
    {
        return; // idempotent + no-throw on a not-attached behavior
    }
    on_detach(); // concrete hook BEFORE clearing (on_detach may read associated_object)
    attached_   = false;
    associated_ = nullptr;
}

} // namespace vaml
