// VAML AdornerLayer — the WPF-faithful adorner layer (11n-6).
//
// Registers BORROWED (non-owning) adorner references under their AdornedElement
// (the WPF ElementMap + ZOrderMap unified) with WPF's z-order model
// (DefaultZOrder = attach order). Add registers, Remove never throws on an
// absent adorner, Update re-syncs the layer over its adorned elements. The
// layer NEVER frees a caller-owned adorner (the caller owns lifetime); Remove
// before a caller destroys an adorned element whose adorner is registered.
// ADR-018: ZERO new locks (single affinity thread).

#include "vaml/core/AdornerLayer.h"

#include <algorithm> // std::sort, std::stable_sort
#include <vector>

namespace vaml {

AdornerLayer::AdornerLayer() = default;

VAML_CORE_EXPORT
bool AdornerLayer::Add(Adorner* adorner)
{
    return Add(adorner, DefaultZOrder);
}

VAML_CORE_EXPORT
bool AdornerLayer::Add(Adorner* adorner, std::int32_t zOrder)
{
    if (!adorner)
        return false;
    UIElement* adorned = adorner->AdornedElement();
    if (!adorned)
        return false; // an un-bound adorner cannot be registered (no adorned key)

    auto& list = hosted_[adorned];

    // Single-registration discipline: adding the same adorner twice is a no-op.
    for (const Hosted& h : list)
    {
        if (h.adorner == adorner)
            return false;
    }

    list.push_back(Hosted{ adorner, zOrder });

    // WPF z-order: the adorner renders TOP when its z is the MAX among its
    // adorned element's adorners. Keep the list z-ascending (a later topmost
    // adorner comes last). Within an equal z, attach order is preserved
    // (stable sort — the WPF attach-order fallback).
    std::stable_sort(list.begin(), list.end(),
                     [](const Hosted& a, const Hosted& b) { return a.zOrder < b.zOrder; });
    return true;
}

VAML_CORE_EXPORT
bool AdornerLayer::Remove(Adorner* adorner)
{
    if (!adorner)
        return false;
    UIElement* adorned = adorner->AdornedElement();
    if (!adorned)
        return false;

    auto it = hosted_.find(adorned);
    if (it == hosted_.end())
        return false; // absent adorner -> silent no-op (WPF "never throws")

    auto& list = it->second;
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        if (list[i].adorner == adorner)
        {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(i)); // drop the reference
            if (list.empty())
                hosted_.erase(it); // no adorners left -> drop the key
            return true;
        }
    }
    return false;
}

VAML_CORE_EXPORT
const std::vector<Adorner*>* AdornerLayer::GetAdorners(UIElement* adorned) const noexcept
{
    if (!adorned)
        return nullptr;
    auto it = hosted_.find(adorned);
    if (it == hosted_.end() || it->second.empty())
        return nullptr;
    // The WPF GetAdorners returns the LIVE list; VAML exposes a borrowed
    // snapshot so a caller can iterate without ownership ambiguity.
    static thread_local std::vector<Adorner*> snapshot;
    snapshot.clear();
    snapshot.reserve(it->second.size());
    for (const Hosted& h : it->second)
        snapshot.push_back(h.adorner);
    return &snapshot;
}

VAML_CORE_EXPORT
bool AdornerLayer::HasAdorners(UIElement* adorned) const noexcept
{
    if (!adorned)
        return false;
    auto it = hosted_.find(adorned);
    return it != hosted_.end() && !it->second.empty();
}

VAML_CORE_EXPORT
std::size_t AdornerLayer::AdornerCount() const noexcept
{
    std::size_t total = 0;
    for (const auto& kv : hosted_)
        total += kv.second.size();
    return total;
}

VAML_CORE_EXPORT
std::size_t AdornerLayer::Update()
{
    // Re-sync EVERY adorned element's adorners (the WPF "Update the whole
    // layer" contract). Iterate a stable KEY copy (not the live map) — the
    // no-lazy safe choice even though Update(adorned) does not erase keys.
    std::vector<UIElement*> adornedKeys;
    adornedKeys.reserve(hosted_.size());
    for (const auto& kv : hosted_)
        adornedKeys.push_back(kv.first);

    std::size_t synced = 0;
    for (UIElement* adorned : adornedKeys)
        synced += Update(adorned);
    return synced;
}

VAML_CORE_EXPORT
std::size_t AdornerLayer::Update(UIElement* adorned)
{
    if (!adorned)
        return 0;
    auto it = hosted_.find(adorned);
    if (it == hosted_.end())
        return 0;

    const vaml::Rect slot = adorned->final_rect();
    std::size_t synced = 0;
    for (Hosted& h : it->second)
    {
        // Re-measure the adorner at the adorned element's box (the WPF
        // measure-to-adorned-size contract) + arrange it into that box, so the
        // overlay re-syncs over its decorated element after a move/resize.
        h.adorner->Measure(vaml::Size{ slot.width, slot.height });
        h.adorner->Arrange(slot);
        ++synced;
    }
    return synced;
}

} // namespace vaml
