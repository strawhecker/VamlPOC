#pragma once
// VAML PropertyRegistry — maps property NAME -> PropertyKey and enforces
// name uniqueness (dedupe detection). S032 (dep:S031). Foundation for the DP
// property system.
//
// The registry is *instance-based*: construct one per owner-setup scope. A
// global static registry instance and constexpr-safe static id/lookup are an
// S033 concern (the exhaustive register/lookup/dedupe suite) and are NOT baked
// in here — the shape is instance-based so S033 can add the static plumbing
// without disturbing this layer.
//
// Uniqueness scope (recorded design decision): keys are globally unique by
// NAME (WPF-style). register_key() throws PropertyAlreadyRegistered on a name
// collision; the map is unchanged by a rejected insert. ownerTypeId lives
// inside the key as identity metadata, not as a second registry dimension.
//
// Implementation is pimpl'd (class Impl + unique_ptr) to keep the ABI surface
// stable and std::unordered_map out of this public header.

#include "vaml/core/PropertyKey.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

namespace vaml {

// register_key() saw a name that is already registered (dedupe detection).
class PropertyAlreadyRegistered : public std::logic_error
{
public:
    explicit PropertyAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// find() did not locate a key for the requested name.
class PropertyNotFound : public std::out_of_range
{
public:
    explicit PropertyNotFound(const char* what) : std::out_of_range(what) {}
};

class VAML_CORE_EXPORT PropertyRegistry
{
public:
    PropertyRegistry();
    ~PropertyRegistry();

    PropertyRegistry(const PropertyRegistry&) = delete;
    PropertyRegistry& operator=(const PropertyRegistry&) = delete;
    PropertyRegistry(PropertyRegistry&&) noexcept;
    PropertyRegistry& operator=(PropertyRegistry&&) noexcept;

    // Registers `key`, uniques by key.name(). Throws PropertyAlreadyRegistered
    // if a key with the same name is already registered (the map is left
    // unchanged). Returns a const reference to the stored key (valid for the
    // registry's lifetime).
    [[nodiscard]] const PropertyKey& register_key(PropertyKey key);

    // Lookup: returns a pointer to the stored key, or nullptr if none. Never
    // throws.
    [[nodiscard]] const PropertyKey* try_find(const std::string& name) const noexcept;

    // Lookup: returns the stored key, or throws PropertyNotFound.
    [[nodiscard]] const PropertyKey& find(const std::string& name) const;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

private:
    class Impl;                 // pimpl: hides std::unordered_map
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
