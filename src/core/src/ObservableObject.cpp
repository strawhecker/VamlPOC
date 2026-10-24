// S241 — ObservableObject: Set() + typed PropertyChanged signal
// (INotifyPropertyChanged-analogue). See ObservableObject.h.

#include "vaml/core/ObservableObject.h"

#include <algorithm>

namespace vaml {

ObservableObject::PropertyChangedToken ObservableObject::add_property_changed(PropertyChangedHandler handler)
{
    return add_property_changed(PropertyId(), std::move(handler));
}

ObservableObject::PropertyChangedToken ObservableObject::add_property_changed(
    PropertyId id, PropertyChangedHandler handler)
{
    const PropertyChangedToken token = next_token_++;
    subs_.push_back(Sub{ std::move(id), token, std::move(handler) });
    return token;
}

void ObservableObject::remove_property_changed(PropertyChangedToken token)
{
    const auto it = std::find_if(subs_.begin(), subs_.end(),
                                 [token](const Sub& s) { return s.token == token; });
    if (it != subs_.end())
    {
        subs_.erase(it);
    }
}

bool ObservableObject::Set(PropertyId id, const Value& value)
{
    const auto key   = std::string(id.name());
    const auto found = store_.find(key);

    if (found != store_.end() && found->second == value)
    {
        // Real-change-only discipline (S035/S036/S037): an equal value does
        // NOT store, does NOT fire.
        return false;
    }

    const Value oldValue = (found != store_.end()) ? found->second : Value::Null();
    store_.insert_or_assign(key, value);
    notify(id, oldValue, value);
    return true;
}

Value ObservableObject::Get(PropertyId id) const
{
    const auto it = store_.find(std::string(id.name()));
    return (it != store_.end()) ? it->second : Value::Null();
}

bool ObservableObject::has(PropertyId id) const
{
    return store_.find(std::string(id.name())) != store_.end();
}

void ObservableObject::notify(PropertyId id, const Value& oldValue, const Value& newValue) const
{
    // Fires in REGISTRATION order (the vector is insertion-ordered). A
    // broadcast subscriber fires for every stored id; a per-property
    // subscriber fires only when its owned id matches (by content).
    for (const Sub& s : subs_)
    {
        if (!s.id.valid() || s.id == id)
        {
            s.handler(id, oldValue, newValue);
        }
    }
}

// --- S240 PropertyReader over an ObservableObject --------------------------
PropertyReader observable_object_reader()
{
    return [](const Value& node, const PropertySegment& segment) -> PropertyRead {
        // The ObservableObject is transported as Value::UntypedPtr; any other
        // node type (a scalar, an unrelated object) is NotReadable.
        if (node.type() != ValueType::UntypedPtr)
        {
            return PropertyRead{ false, PropertyPathError::NotReadable, Value::Null(), 0 };
        }
        void* raw = node.get<void*>();
        if (raw == nullptr)
        {
            return PropertyRead{ false, PropertyPathError::NotReadable, Value::Null(), 0 };
        }
        // Only a Property segment is readable on an ObservableObject — it is a
        // named-value holder, not an indexable collection (a collection reader
        // lands with collections, recorded NO-GAP at S240).
        if (segment.kind != PropertySegmentKind::Property)
        {
            return PropertyRead{ false, PropertyPathError::NotReadable, Value::Null(), 0 };
        }

        auto* obj = static_cast<ObservableObject*>(raw);
        const PropertyId id = PropertyId::of(segment.name);
        if (!obj->has(id))
        {
            // Absent name -> Class B MissingProperty (NEVER a raw throw, per
            // the S240 dependency-object reader contract).
            return PropertyRead{ false, PropertyPathError::MissingProperty, Value::Null(), 0 };
        }
        // Read the current value; a stored null is a REAL resolved value (the
        // evaluator interprets non-final null as NullIntermediate).
        return PropertyRead{ true, PropertyPathError::Empty, obj->Get(id), 0 };
    };
}

} // namespace vaml
