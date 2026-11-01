// S512a — BeginStoryboard: invoke() begins the owned Storyboard (resolves the
// 11k-5 recorded NO-GAP action payload — the Actions become LIVE). See
// BeginStoryboard.h for the WPF-faithful semantics.

#include "vaml/core/BeginStoryboard.h"

namespace vaml {

VAML_CORE_EXPORT
void BeginStoryboard::invoke(const Value& parameter)
{
    (void)parameter; // WPF BeginStoryboard ignores the parameter
    if (storyboard_ == nullptr || root_ == nullptr)
    {
        // An unattached/empty action has nothing to begin. The authoring path
        // rejected a missing/empty Storyboard loudly; an action that was never
        // attach()'d (no root resolved) is a no-op — never a silent surprise,
        // because attach_event_triggers drives this seam after priming.
        return;
    }
    storyboard_->start(root_, resolve_);
}

} // namespace vaml
