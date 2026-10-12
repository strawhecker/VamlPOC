#pragma once
// VAML DependencyObject — the base value-holder for the dependency-property
// system. S036 (dep:S035). Delivers the S036 title contract:
//   1. local value store        — a per-object store keyed by PropertyId;
//   2. Get/Set                  — property read/write through the store;
//   3. per-prop mastery         — register_property() installs (metadata,
//                                defaultValue) per property on the object;
//   4. thread-affinity marker   — the object records its creating thread.
//
// Set() MUST reproduce exactly the S035 apply pipeline (the MiniStore harness
// in test/unit/PropertyMetadata_semantics_test.cpp is the reference spec):
//      coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//      real change -> store.
// See roadmap/progress for this recorded contract to S036.
//
// Design choices (recorded for forward steps):
//   - OBJECT-SCOPED REGISTRATION. register_property(PropertyKey, PropertyMetadata)
//     installs the metadata + its defaultValue into THIS object's store, keyed
//     by the PropertyId derived from the key's NAME. There is deliberately NO
//     global static registry / DependencyProperty::Register in S036 — that
//     surface is re-scoped (recorded, NOT silent) to S039, where effective-value
//     resolution (local ?? inherited ?? default) becomes the first real
//     consumer of a global property table. Object-scoped registration is the
//     minimal honest surface that makes the S035 pipeline real and testable.
//   - LOCAL-ONLY STORE. The store holds ONLY local overrides. An absent entry
//     for a registered property means "default applied" — Get() falls back to
//     metadata.defaultValue(). Every registered property is present as a
//     (metadata+key) entry, but its `local` is empty until first Set().
//   - Get() RETURNS BY VALUE. No reference into the store is exposed, so
//     callers are immune to future store mutation/coalescing (S037 marshalling,
//     S039 effective-value), and there is no ref-invalidation hazard.
//   - PIPELINE IN Set(). coerce runs BEFORE validate (validate sees the
//     coerced value — S035 ordering contract). The changed cb fires with the
//     OLD effective (local override if present else default) and the coerced
//     NEW value, and ONLY when they differ (real change). A rejected value
//     leaves the store unchanged and fires nothing.
//   - THREAD-AFFINITY MARKER. The ctor captures std::this_thread::get_id().
//     S036 only records + exposes it (thread_affinity_id / is_affine_to_current_thread).
//     Wrong-thread write QUEUING is S037's concern (recorded contract in
//     roadmap/progress), NOT S036.
//
// Storage is pimpl'd (class Impl + unique_ptr) so the public header exposes
// no std::unordered_map / no PropertyMetadata value — keeping the ABI surface
// stable and move-only members out of the public declaration.

#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h" // PropertyMetadata, ValueTypeMismatch
#include "vaml/core/PropertyRegistry.h" // PropertyAlreadyRegistered, PropertyNotFound

#include <memory>
#include <thread>

namespace vaml {

// Set() outcome, mirroring the S035 MiniStore pipeline.
enum class SetResult
{
    APPLIED,   // accepted, changed cb fired, stored
    REJECTED,  // validate refused the coerced value; store unchanged, no cb
    NO_CHANGE, // coerced == current effective; no cb (real change only)
};

class VAML_CORE_EXPORT DependencyObject
{
public:
    DependencyObject();
    ~DependencyObject();

    DependencyObject(const DependencyObject&) = delete;
    DependencyObject& operator=(const DependencyObject&) = delete;
    DependencyObject(DependencyObject&&) noexcept;
    DependencyObject& operator=(DependencyObject&&) noexcept;

    // Installs `metadata` (and its defaultValue) for the property named by
    // `key`, keyed by PropertyId(key.name()). Returns a const reference to the
    // STORED key (valid for this object's lifetime; unordered_map preserves
    // element references across later insertions/rehashes).
    //
    //   - name dedupe: registering the same NAME twice throws
    //     PropertyAlreadyRegistered and leaves the store unchanged;
    //   - type coherence: key.type() must equal metadata.boundType(), else
    //     ValueTypeMismatch (NO-LAZY guard — never register a key/metadata pair
    //     that disagree on the value type).
    const PropertyKey& register_property(PropertyKey key, PropertyMetadata metadata);

    // Reads the property's current effective value: the local override if one
    // has been stored, else metadata.defaultValue() ("default applied").
    // Returns BY VALUE. Throws PropertyNotFound for an unregistered PropertyId.
    [[nodiscard]] Value Get(PropertyId id) const;

    // Writes `input` through the S035 pipeline (coerce -> validate(coerced) ->
    // fire-on-real-change -> store). Throws PropertyNotFound for an
    // unregistered PropertyId, ValueTypeMismatch for a wrong-tag input.
    [[nodiscard]] SetResult Set(PropertyId id, const Value& input);

    // --- Thread-affinity marker (S036 records/exposes; S037 consumes) ----
    [[nodiscard]] std::thread::id thread_affinity_id() const noexcept;
    [[nodiscard]] bool is_affine_to_current_thread() const noexcept
    {
        return thread_affinity_id() == std::this_thread::get_id();
    }

private:
    struct Impl; // pimpl: hides std::unordered_map + PropertyMetadata
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
