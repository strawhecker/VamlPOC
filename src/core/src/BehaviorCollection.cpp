// S502 — BehaviorCollection: the ordered owned behavior list + the
// `i:Interaction.Behaviors` attached surface.

#include "vaml/core/BehaviorCollection.h"

#include <unordered_map>

namespace vaml {

BehaviorCollection::~BehaviorCollection()
{
    detach_all();
}

BehaviorCollection::BehaviorCollection(BehaviorCollection&& other) noexcept
    : owner_(other.owner_)
    , behaviors_(std::move(other.behaviors_))
{
    other.owner_ = nullptr;
}

BehaviorCollection& BehaviorCollection::operator=(BehaviorCollection&& other) noexcept
{
    if (this != &other)
    {
        detach_all();
        owner_     = other.owner_;
        behaviors_ = std::move(other.behaviors_);
        other.owner_ = nullptr;
    }
    return *this;
}

void BehaviorCollection::set_owner(UIElement* owner) noexcept
{
    if (owner_ == owner)
    {
        return;
    }
    // A new owner (or a clear): detach every behavior from the prior owner,
    // then re-attach to the new one (each behavior attaches to its owner).
    detach_all();
    owner_ = owner;
    for (auto& b : behaviors_)
    {
        if (owner_ != nullptr)
        {
            b->attach_to(owner_);
        }
    }
}

Behavior* BehaviorCollection::add(std::unique_ptr<Behavior> behavior)
{
    Behavior* raw = behavior.get();
    if (owner_ != nullptr)
    {
        behavior->attach_to(owner_);
    }
    behaviors_.push_back(std::move(behavior));
    return raw;
}

std::vector<Behavior*> BehaviorCollection::as_pointers() const noexcept
{
    std::vector<Behavior*> out;
    out.reserve(behaviors_.size());
    for (const auto& b : behaviors_)
    {
        out.push_back(b.get());
    }
    return out;
}

void BehaviorCollection::detach_all() noexcept
{
    for (auto& b : behaviors_)
    {
        b->detach();
    }
}

namespace {

// The `i:Interaction.Behaviors` side-table (out-of-line, keyed by element
// address). Mirrors the S360 ResourceDictionary / S500 DataContext side-table
// pattern: a UIElement with no behaviors does not grow (the S145 object-pool
// budget). The element OWNS the entry; torn down on element destruction via
// the element's destroyed-notification (see _register_owner_cleanup).
struct BehaviorsSideTable
{
    std::unordered_map<const void*, std::unique_ptr<BehaviorCollection>> map;
};

BehaviorsSideTable& behaved_table()
{
    static BehaviorsSideTable table;
    return table;
}

} // namespace

VAML_CORE_EXPORT
BehaviorCollection* get_element_behaviors(UIElement& element)
{
    auto& table = behaved_table().map;
    auto it = table.find(&element);
    if (it != table.end())
    {
        return it->second.get();
    }
    // Lazily create the owner collection + attach it to the element. The
    // behavior collection is owned by the side-table. The entry is released by
    // `remove_element_behaviors` (called from ~UIElement, mirrored the S360
    // resource + S500 DataContext side-table teardown) so the collection's
    // detach_all (which calls the trigger's remove_routed_handler on the
    // element) runs while the element's routed handler store is still alive.
    auto collection = std::make_unique<BehaviorCollection>();
    collection->set_owner(&element);
    BehaviorCollection* raw = collection.get();
    table.emplace(&element, std::move(collection));
    return raw;
}

VAML_CORE_EXPORT
void remove_element_behaviors(UIElement* element) noexcept
{
    if (element != nullptr)
    {
        behaved_table().map.erase(element);
    }
}

VAML_CORE_EXPORT
bool element_has_behaviors(const UIElement& element) noexcept
{
    auto& table = behaved_table().map;
    return table.find(&element) != table.end();
}

} // namespace vaml
