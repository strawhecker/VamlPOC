#pragma once
// S839 (dep:S836) - LayoutContext: WPF-faithful per-Dispatcher layout driver
// (ContextLayoutManager analogue). Resolves the S126 ancestor-walk drift (D1)
// + the S126 independent-axes property (D2). Invalidate sets ONLY the local bit
// + enqueues (UIElement.cs:249-306); no parent walk. Dirtiness climbs lazily
// via the measure waterfall. Measure re-arms arrange unconditionally
// (UIElement.cs:629). Driver drains topmost-first at PreviousConstraint then
// arranges. Single-threaded (ADR-018) thread_local context; ZERO new locks.
#include <vaml/core/export.h>
#include <vaml/core/Geometry.h>
#include <vector>
namespace vaml {
class UIElement;
namespace detail {
class VAML_CORE_EXPORT LayoutContext
{
public:
    static LayoutContext& current() noexcept;
    void add_measure(UIElement* e);
    void add_arrange(UIElement* e);
    void remove_measure(UIElement* e) noexcept;
    void remove_arrange(UIElement* e) noexcept;
    void detach(UIElement* e) noexcept;
    [[nodiscard]] bool has_measure_work(UIElement* root) const noexcept;
    [[nodiscard]] bool has_arrange_work(UIElement* root) const noexcept;
    void update_layout(UIElement* root, const Size& rootConstraint,
                       const Rect& rootSlot, bool root_slot_from_desired);
    // S844 (G3): WPF fireSizeChangedEvents - size-changed chain drained by
    // update_layout each pass; each entry raises SizeChanged + updates
    // ActualWidth/ActualHeight (UIElement::on_render_size_changed).
    void add_size_changed(UIElement* e, const Size& previous, const Size& next);
    void fire_size_changed();
    [[nodiscard]] bool has_size_changed() const noexcept;
private:
    LayoutContext() = default;
    LayoutContext(const LayoutContext&) = delete;
    LayoutContext& operator=(const LayoutContext&) = delete;
    [[nodiscard]] bool in_subtree(UIElement* e, UIElement* root) const noexcept;
    [[nodiscard]] UIElement* pop_top_measure(UIElement* root);
    [[nodiscard]] UIElement* pop_top_arrange(UIElement* root);
    std::vector<UIElement*> measure_queue_;
    std::vector<UIElement*> arrange_queue_;
    struct SizeChangedInfo { UIElement* element; Size previous; Size next; };
    std::vector<SizeChangedInfo> size_changed_queue_;
};
} // namespace detail
} // namespace vaml
