// S839 (dep:S836) - LayoutContext definition. Faithful ContextLayoutManager
// analogue: per-thread measure/arrange queues drained topmost-first. See header.
#include <vaml/core/LayoutContext.h>
#include <vaml/core/UIElement.h>
#include <algorithm>
#include <cmath>
namespace vaml {
namespace detail {
namespace {
bool axis_close(float a, float b) noexcept
{
    if (std::isnan(a) || std::isnan(b)) { return false; }
    if (std::isinf(a) || std::isinf(b)) { return a == b; }
    return std::fabs(a - b) <= 1e-4f;
}
bool constraint_close(const Size& a, const Size& b) noexcept
{
    return axis_close(a.width, b.width) && axis_close(a.height, b.height);
}
UIElement* ui_parent(UIElement* e) noexcept
{
    if (e == nullptr) { return nullptr; }
    for (DependencyObject* p = e->parent(); p != nullptr; p = p->parent())
    {
        if (auto* ue = dynamic_cast<UIElement*>(p)) { return ue; }
    }
    return nullptr;
}
} // namespace
LayoutContext& LayoutContext::current() noexcept
{
    static thread_local LayoutContext ctx;
    return ctx;
}
bool LayoutContext::in_subtree(UIElement* e, UIElement* root) const noexcept
{
    if (e == nullptr) { return false; }
    if (root == nullptr) { return true; }
    for (UIElement* cur = e; cur != nullptr;)
    {
        if (cur == root) { return true; }
        DependencyObject* p = cur->parent();
        cur = (p != nullptr) ? dynamic_cast<UIElement*>(p) : nullptr;
    }
    return false;
}
void LayoutContext::add_measure(UIElement* e)
{
    if (e == nullptr) { return; }
    if (std::find(measure_queue_.begin(), measure_queue_.end(), e) != measure_queue_.end()) { return; }
    const int childLevel = e->tree_level() + 1;
    measure_queue_.erase(std::remove_if(measure_queue_.begin(), measure_queue_.end(), [&](UIElement* q) { return q != e && q->tree_level() == childLevel && ui_parent(q) == e; }), measure_queue_.end());
    UIElement* parent = ui_parent(e);
    if (parent != nullptr && parent->is_measure_dirty() && !parent->is_measure_in_progress()) { return; }
    measure_queue_.push_back(e);
}
void LayoutContext::add_arrange(UIElement* e)
{
    if (e == nullptr) { return; }
    if (std::find(arrange_queue_.begin(), arrange_queue_.end(), e) != arrange_queue_.end()) { return; }
    const int childLevel = e->tree_level() + 1;
    arrange_queue_.erase(std::remove_if(arrange_queue_.begin(), arrange_queue_.end(), [&](UIElement* q) { return q != e && q->tree_level() == childLevel && ui_parent(q) == e; }), arrange_queue_.end());
    UIElement* parent = ui_parent(e);
    if (parent != nullptr && parent->is_arrange_dirty() && !parent->is_arrange_in_progress()) { return; }
    arrange_queue_.push_back(e);
}
void LayoutContext::remove_measure(UIElement* e) noexcept
{
    measure_queue_.erase(std::remove(measure_queue_.begin(), measure_queue_.end(), e), measure_queue_.end());
}
void LayoutContext::remove_arrange(UIElement* e) noexcept
{
    arrange_queue_.erase(std::remove(arrange_queue_.begin(), arrange_queue_.end(), e), arrange_queue_.end());
}
void LayoutContext::detach(UIElement* e) noexcept
{
    remove_measure(e);
    remove_arrange(e);
    // S844 (G3): a destroyed element must not linger in the size-changed chain
    // (WPF unlinks on element teardown) - prevents a stale-pointer fire later.
    size_changed_queue_.erase(std::remove_if(size_changed_queue_.begin(), size_changed_queue_.end(),
        [e](const SizeChangedInfo& i) { return i.element == e; }), size_changed_queue_.end());
}
bool LayoutContext::has_measure_work(UIElement* root) const noexcept
{
    for (UIElement* e : measure_queue_) { if (e->is_measure_dirty() && in_subtree(e, root)) { return true; } }
    return false;
}
bool LayoutContext::has_arrange_work(UIElement* root) const noexcept
{
    for (UIElement* e : arrange_queue_) { if (e->is_arrange_dirty() && in_subtree(e, root)) { return true; } }
    return false;
}
UIElement* LayoutContext::pop_top_measure(UIElement* root)
{
    UIElement* best = nullptr; std::size_t bestIdx = 0;
    for (std::size_t i = 0; i < measure_queue_.size(); ++i) {
        UIElement* e = measure_queue_[i];
        if (!e->is_measure_dirty() || !in_subtree(e, root)) { continue; }
        if (best == nullptr || e->tree_level() < best->tree_level()) { best = e; bestIdx = i; }
    }
    if (best == nullptr) { return nullptr; }
    measure_queue_.erase(measure_queue_.begin() + static_cast<std::ptrdiff_t>(bestIdx));
    return best;
}
UIElement* LayoutContext::pop_top_arrange(UIElement* root)
{
    UIElement* best = nullptr; std::size_t bestIdx = 0;
    for (std::size_t i = 0; i < arrange_queue_.size(); ++i) {
        UIElement* e = arrange_queue_[i];
        if (!e->is_arrange_dirty() || !in_subtree(e, root)) { continue; }
        if (best == nullptr || e->tree_level() < best->tree_level()) { best = e; bestIdx = i; }
    }
    if (best == nullptr) { return nullptr; }
    arrange_queue_.erase(arrange_queue_.begin() + static_cast<std::ptrdiff_t>(bestIdx));
    return best;
}
void LayoutContext::update_layout(UIElement* root, const Size& rootConstraint, const Rect& rootSlot, bool root_slot_from_desired)
{
    if (root == nullptr) { return; }
    int guard = 0;
    for (;;)
    {
        if (++guard > 512) { break; }
        if (root->is_measure_dirty() || !constraint_close(root->previous_available(), rootConstraint)) { root->Measure(rootConstraint); }
        UIElement* e = nullptr;
        while ((e = pop_top_measure(root)) != nullptr) {
            if (e == root) { e->Measure(rootConstraint); } else { e->Measure(e->previous_available()); }
        }
        if (root->is_arrange_dirty()) {
            const Rect slot = root_slot_from_desired ? Rect{ 0.0f, 0.0f, root->desired_size().width, root->desired_size().height } : rootSlot;
            root->Arrange(slot);
        }
        UIElement* a = nullptr;
        while (has_arrange_work(root)) {
            a = pop_top_arrange(root);
            if (a == nullptr) { break; }
            if (a == root) {
                const Rect slot = root_slot_from_desired ? Rect{ 0.0f, 0.0f, root->desired_size().width, root->desired_size().height } : rootSlot;
                a->Arrange(slot);
            } else { a->Arrange(a->arrange_slot()); }
            if (has_measure_work(root)) { break; }
        }
        if (has_size_changed()) { fire_size_changed(); }
        const bool more = root->is_measure_dirty() || root->is_arrange_dirty() || has_measure_work(root) || has_arrange_work(root) || has_size_changed();
        if (!more) { break; }
    }
}
void LayoutContext::add_size_changed(UIElement* e, const Size& previous, const Size& next)
{
    if (e == nullptr) { return; }
    size_changed_queue_.push_back(SizeChangedInfo{ e, previous, next });
}
bool LayoutContext::has_size_changed() const noexcept
{
    return !size_changed_queue_.empty();
}
void LayoutContext::fire_size_changed()
{
    std::vector<SizeChangedInfo> chain;
    chain.swap(size_changed_queue_);
    for (const SizeChangedInfo& info : chain)
    {
        if (info.element != nullptr) { info.element->on_render_size_changed(info.previous, info.next); }
    }
}

} // namespace detail
} // namespace vaml
