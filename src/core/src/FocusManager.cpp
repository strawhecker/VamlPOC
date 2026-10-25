// S325 — FocusManager out-of-line definitions. See FocusManager.h for the
// contract: per-tree keyboard focus + deterministic pre-order tab-order +
// routed KeyDown/KeyUp on the focused element.

#include "vaml/core/FocusManager.h"

#include "vaml/core/Element.h"   // Button (S406 default-button activation)

#include <algorithm>

namespace vaml {

FocusManager::FocusManager(UIElement* root)
    : root_(root)
{
}

void FocusManager::KeyboardFocus(UIElement* element)
{
    focused_ = element;
}

void FocusManager::set_default_button(Button* b)
{
    defaultButton_ = b;
}

namespace {

// The key routed event (KeyDown vs KeyUp) to raise.
const RoutedEvent& key_event(bool isDown)
{
    return isDown ? KeyDownEvent() : KeyUpEvent();
}

} // namespace

void FocusManager::collect_focusable(UIElement& node, std::vector<UIElement*>& out) const
{
    // Depth-first PRE-ORDER: this element first (if focusable), then each child
    // in index order. This is the deterministic tab order.
    if (node.focusable())
    {
        out.push_back(&node);
    }
    for (std::size_t i = 0; i < node.child_count(); ++i)
    {
        UIElement* child = node.child_at(i);
        if (child != nullptr)
        {
            collect_focusable(*child, out);
        }
    }
}

UIElement* FocusManager::MoveFocusNext()
{
    if (root_ == nullptr)
    {
        return nullptr;
    }
    std::vector<UIElement*> order;
    collect_focusable(*root_, order);
    if (order.empty())
    {
        return nullptr;
    }
    // Find the current focused element in the order (if none, start at the first).
    auto it = std::find(order.begin(), order.end(), focused_);
    UIElement* next = nullptr;
    if (it == order.end())
    {
        next = order.front();
    }
    else
    {
        // Advance, wrapping to the front.
        const std::size_t idx = static_cast<std::size_t>(it - order.begin());
        next = order[(idx + 1) % order.size()];
    }
    KeyboardFocus(next);
    return next;
}

UIElement* FocusManager::MoveFocusPrevious()
{
    if (root_ == nullptr)
    {
        return nullptr;
    }
    std::vector<UIElement*> order;
    collect_focusable(*root_, order);
    if (order.empty())
    {
        return nullptr;
    }
    auto it = std::find(order.begin(), order.end(), focused_);
    UIElement* prev = nullptr;
    if (it == order.end())
    {
        prev = order.back();
    }
    else
    {
        const std::size_t idx = static_cast<std::size_t>(it - order.begin());
        prev = order[(idx == 0) ? order.size() - 1 : idx - 1];
    }
    KeyboardFocus(prev);
    return prev;
}

bool FocusManager::RaiseKeyRouted(KeyCode key, bool isDown)
{
    const bool focusedPresent = (focused_ != nullptr);
    bool handled = false;
    if (focusedPresent)
    {
        KeyRoutedEventArgs args;
        args.source = focused_;
        args.key    = key;
        args.isDown = isDown;
        focused_->RaiseEvent(key_event(isDown), args);
        handled = args.handled;
    }
    // S406 ENTER-ACTIVATES-DEFAULT: on an Enter KEY-DOWN that NO focused
    // element/handler consumed, raise the Click on the default button (the
    // WPF-faithful primitive). The default button need not be focused (WPF).
    // A handled Enter (e.g. a TextBox single-line accept) does NOT re-activate.
    if (!handled && isDown && key == KeyCode::Enter && defaultButton_ != nullptr)
    {
        (void)defaultButton_->raise_click();
    }
    return focusedPresent;
}

} // namespace vaml
