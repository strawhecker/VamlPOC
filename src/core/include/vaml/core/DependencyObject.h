#pragma once
// VAML DependencyObject — the base value-holder for the dependency-property
// system. S036 (dep:S035) + S037 (dep:S036_5, marshalling foundation) +
// S039 (dep:S038, effective-value resolution).
//
// S039 title contract (read-side, pull model):
//   8. GetEffectiveValue() — locals ?? inherited-from-parent ?? default, and
//      the InheritOnly flag (own local override does not contribute). `Get`
//      delegates to it (backward compatible: no existing test uses Inherits/
//      InheritOnly, so local??default semantics are unchanged for them).
//   9. Parent link — single non-owning parent; seed consumed by S040 (cascade)
//      and S120 (visual tree). NO-GAP, NOT lazy.
//   Set/ProcessPendingChanges are UNTOUCHED here (read-side only); the
//   write/cascade side is S040 (recorded, NOT silent).
//
// S036 title contract:
//   1. local value store        — a per-object store keyed by PropertyId;
//   2. Get/Set                  — property read/write through the store;
//   3. per-prop mastery         — register_property() installs (metadata,
//                                defaultValue) per property on the object;
//   4. thread-affinity marker   — the object records its creating thread.
//
// S037 (this step) title contract — wrong-thread write QUEUING:
//   5. Set() is thread-affinity aware: a write from any thread other than the
//      object's creating (affinity) thread is QUEUED-not-applied — it runs
//      coerce + validate immediately (instant writer feedback) but does NOT
//      store and does NOT fire the changed cb; on accept it is posted to a
//      mutex-protected per-object pending queue and Set() returns
//      SetResult::QUEUED.
//   6. ProcessPendingChanges() drains that queue (FIFO) on the affinity
//      thread, replaying each queued write through the S036 store half — so a
//      real change fires the same cb(old,new) with the correct effective
//      value chain (queued [5,7] on a default-0 prop fires cb(0->5) then
//      cb(5->7)). Drain is affinity-thread-only; calling it from a non-affine
//      thread throws WrongThreadException (NO-LAZY — a silent wrong-thread
//      no-op would hide bugs).
//   7. pending_change_count() exposes the queue depth (mutex-protected) so a
//      test can observe "queued-not-applied" before a drain runs.
//
// Set() (on the AFFINE thread) MUST reproduce exactly the S035 apply pipeline
// (the MiniStore harness in test/unit/PropertyMetadata_semantics_test.cpp is
// the reference spec):
//      coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//      real change -> store.
// See roadmap/progress for this recorded contract to S036/S037.
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
//     S036 records + exposes it (thread_affinity_id / is_affine_to_current_thread);
//     S037 (this step) consumes it as the basis for wrong-thread detection.
//   - WRONG-THREAD WRITE QUEUING (S037). A Set() from a non-affine thread
//     returns SetResult::QUEUED on an accepted (coerced+validated) value — the
//     write is posted, not applied. The write is NOT stored and the changed cb
//     is NOT fired until ProcessPendingChanges() (affinity thread only) drains
//     it. Coerce + validate run AT QUEUE TIME so the wrong-thread writer gets
//     immediate feedback (REJECTED on refusal, ValueTypeMismatch on wrong tag);
//     the drain only ever sees accepted, coerced values. The pending queue is
//     the ONE genuinely cross-thread datum and is mutex-protected.
//   - CROSS-THREAD CONTRACT (S037). The value store stays affinity-thread-owned
//     by construction: only the affinity thread stores (via on-affine Set or
//     via ProcessPendingChanges). Get() reads the store and is declared safe on
//     the affinity thread or when no concurrent affinity-thread mutation is in
//     flight (WPF-faithful; full store-race freedom is TSan/S620/S723 territory,
//     recorded not silent). The S037 tests use a join barrier so no concurrent
//     drain/read occurs in the demonstration. S041 generalizes the per-object
//     queue into the per-thread ChangeQueue + notification processor (its own
//     resolution step — NO backfill).
//
// Storage is pimpl'd (class Impl + unique_ptr) so the public header exposes
// no std::unordered_map / no PropertyMetadata value — keeping the ABI surface
// stable and move-only members out of the public declaration.
//
// NOTE — S142 (supersedes): this pimpl rationale is RETRACTED. This MIT-public
// lib is statically linked (ADR-003 revised): there is NO runtime ABI boundary
// to defend (the consumer *is* the exe). The pimpl now only costs a
// ~100%-of-access `impl_->` trampoline + a per-object heap hop on the CPU
// Get/Set/cascade path — the exact cost the CPU/GPU line (ADR-019) says to
// remove. S142 collapses this pimpl + store into one block-resident,
// inline-started unit (pimpl removed, not preserved). This comment is a RECORD
// of the superseded rationale.

#include "vaml/core/ChangeQueue.h"      // S041/S143 per-object postbox (inline member)
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h" // PropertyMetadata, ValueTypeMismatch
#include "vaml/core/PropertyRegistry.h" // PropertyAlreadyRegistered, PropertyNotFound
#include "vaml/core/Value.h"            // Value (subscriber signature + pushed values)

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vaml {

// Set() outcome, mirroring the S035 MiniStore pipeline + the S037 marshalling
// extension. The first three mirror S035 exactly; QUEUED is S037's addition
// (backward-compatible — the three original values are unshifted).
enum class SetResult
{
    APPLIED,   // accepted, changed cb fired, stored
    REJECTED,  // validate refused the coerced value; store unchanged, no cb
    NO_CHANGE, // coerced == current effective; no cb (real change only)
    QUEUED,    // S037: write from a NON-affine thread was accepted (coerced+
               // validated) and POSTED to the pending queue — not yet applied.
               // cb NOT fired, store NOT written until ProcessPendingChanges().
};

// S037: ProcessPendingChanges() (and a future affinity-required API) was
// invoked from a thread other than the object's affinity thread. Drain is
// affinity-thread-only — a silent wrong-thread no-op would be lazy (NO-LAZY).
class VAML_CORE_EXPORT WrongThreadException : public std::logic_error
{
public:
    explicit WrongThreadException(const char* what) : std::logic_error(what) {}
};

// S045: a Set() targeting a read-only dependency property. Structural + value-
// independent: it throws even when the input equals the current/default value,
// and it throws on ANY thread (never QUEUED — the guard precedes the S037
// affinity branch, so a read-only refusal is never deferred to a drain).
// Catchable specifically so a caller can distinguish a read-only rejection
// from a wrong-thread QUEUED result.
class VAML_CORE_EXPORT PropertyReadOnlyException : public std::logic_error
{
public:
    explicit PropertyReadOnlyException(const char* what) : std::logic_error(what) {}
};

class VAML_CORE_EXPORT DependencyObject
{
public:
    DependencyObject();
    // S087: virtual — the ElementFactoryRegistry produces derived element
    // classes (Button, StackPanel, ...) owned through this base as
    // unique_ptr<DependencyObject>; correct polymorphic deletion requires a
    // virtual destructor. This also makes the type polymorphic (enabling
    // dynamic_cast concrete-type checks in tests).
    virtual ~DependencyObject();

    // --- S145 class-scoped allocation (the object-pool funnel) -------------
    // Every element (DependencyObject + all derived types) allocates/deallocates
    // through the thread-local ObjectPool: dole from blocks, recycle to the
    // free-list, NEVER delete during the thread's life (opt-in
    // object_pool_master_destroy_all). This is the honest rewrite of the
    // S120/S121 element/layout ownership: the unique_ptr owner/lifetime
    // semantics are UNCHANGED, only the allocation/deallocation path beneath
    // them routes through the pool (ADR-018: thread-local + LOCKLESS).
    //
    // The object's internal dynamic state (Store overflow, children_/subs_/
    // inheritance_scope_ vectors, owned PropertyMetadata/PropertyKey nodes,
    // Grid::Impl) is torn down NORMALLY by the destructor; only the object's
    // slab bytes are retained on the free-list and reused — recycle == slab
    // reuse, not state carryover.
    static void* operator new(std::size_t size);
    static void  operator delete(void* p) noexcept;
    static void  operator delete(void* p, std::size_t) noexcept;

    DependencyObject(const DependencyObject&) = delete;
    DependencyObject& operator=(const DependencyObject&) = delete;
    DependencyObject(DependencyObject&&) noexcept;
    DependencyObject& operator=(DependencyObject&&) noexcept;

    // Installs `metadata` (and its defaultValue) for the property named by
    // `key`, keyed by PropertyId(key.name()). Returns a const reference to the
    // STORED key (valid for this object's lifetime; the owned stable key node
    // is a separate heap allocation that S142 preserves deliberately so store
    // growth/relocation never invalidates the returned reference).
    //
    //   - name dedupe: registering the same NAME twice throws
    //     PropertyAlreadyRegistered and leaves the store unchanged;
    //   - type coherence: key.type() must equal metadata.boundType(), else
    //     ValueTypeMismatch (NO-LAZY guard — never register a key/metadata pair
    //     that disagree on the value type).
    const PropertyKey& register_property(PropertyKey key, PropertyMetadata metadata);

    // Reads the property's current EFFECTIVE value. S039 is the read-side
    // resolution primitive; `Get` delegates to it. Resolution order:
    //   - NOT Inherits:          local ?? default
    //   - Inherits (no InheritOnly): local ?? inherited-from-parent ?? default
    //   - InheritOnly:           inherited-from-parent ?? default
    //                            (the object's OWN local override does NOT
    //                             contribute)
    // The inheritance descriptor (flags + default) is sourced from the global
    // DependencyProperty table when the property is globally registered,
    // else from this object's own store metadata. The local value always
    // comes from this object's per-object store. Returns BY VALUE. Throws
    // PropertyNotFound for an unregistered PropertyId (the internal parent
    // chain walk is tolerant — an ancestor lacking the property contributes
    // nothing, it never throws).
    [[nodiscard]] Value GetEffectiveValue(PropertyId id) const;

    // Reads the property's effective value (S039). Equivalent to
    // GetEffectiveValue(id) — the two are synonyms; Get is retained for the
    // S036/S037 call sites and reads the same resolved value. Returns BY
    // VALUE. Throws PropertyNotFound for an unregistered PropertyId.
    [[nodiscard]] Value Get(PropertyId id) const;

    // Writes `input` through the S035 pipeline (coerce -> validate(coerced) ->
    // fire-on-real-change -> store). Throws PropertyNotFound for an
    // unregistered PropertyId, ValueTypeMismatch for a wrong-tag input.
    //
    // S037 affinity-aware behavior: on the AFFINE thread this is exactly the
    // S035 pipeline (APPLIED/REJECTED/NO_CHANGE). On a NON-affine thread,
    // coerce + validate still run immediately (REJECTED / ValueTypeMismatch
    // give the writer instant feedback) but a passing value is POSTED to the
    // pending queue and Set() returns SetResult::QUEUED — the store is NOT
    // written and the changed cb is NOT fired until ProcessPendingChanges()
    // drains on the affinity thread.
    //
    // S045 read-only contract: if the resolved property carries
    // PropertyFlags::ReadOnly, Set() throws PropertyReadOnlyException. The
    // check runs IMMEDIATELY after resolving the metadata provider and BEFORE
    // coerce / validate / the S037 affinity branch — so a read-only refusal is
    // structural and value-independent (throws even for value == current
    // effective, never REJECTED/NO_CHANGE/QUEUED) and fires on ANY thread
    // (a wrong-thread read-only Set throws, it is never deferred to a drain).
    // No local override is materialized for a refused read-only write.
    [[nodiscard]] SetResult Set(PropertyId id, const Value& input);

    // S037 (S143 relocation): drains the pending VALUE queue (FIFO) on the
    // affinity thread via the object's single ChangeQueue (queue_.drain_values),
    // replaying each queued write through the S036 store half (fire
    // cb(old=new-effective?) -> store). Must be called from the object's affinity
    // thread — otherwise throws WrongThreadException (NO-LAZY guard).
    void ProcessPendingChanges();

    // S037 (S143 relocation): number of writes currently sitting in the pending
    // VALUE queue (i.e. queued-not-applied), via queue_.value_pending_count().
    // Mutex-protected (the queue's single mutex); observable from any thread. A
    // test uses this to confirm a wrong-thread Set() did NOT apply (depth 1) and
    // that a drain returns it to 0.
    [[nodiscard]] std::size_t pending_change_count() const noexcept;

    // --- Thread-affinity marker (S036 records/exposes; S037 consumes) ----
    [[nodiscard]] std::thread::id thread_affinity_id() const noexcept;
    [[nodiscard]] bool is_affine_to_current_thread() const noexcept
    {
        return thread_affinity_id() == std::this_thread::get_id();
    }

    // --- Parent link (S039 read-side seed; NOT lazy, NO-GAP) -------------
    // The single non-owning parent used by S039's effective-value resolution
    // to walk the ancestor chain ("inherited-from-parent"). This is a REAL,
    // deliberate seed: S040 (inheritance cascade / write recompute) and S120
    // (UIElement -> visual tree, create/attach/detach children) both consume
    // it. It is deliberately a LOOSE (non-owning) pointer: a DependencyObject
    // never owns its parent. Setting it does NOT trigger a cascade in S039 —
    // S039 is read-side only; S040 owns the write/cascade side (recorded,
    // NOT silent).
    //
    // S041 (this step) adds the INVERSE EDGE: set_parent() ALSO registers this
    // object into the parent's non-owning inheritance-scope (the reverse edge
    // of this parent link). S041's descendant push consumes it: when an
    // Inherits/InheritOnly property changes on a parent, the parent walks its
    // inheritance-scope and enqueues a pushed notification for each inheriting
    // descendant (S040's recorded obligation — delivered here, NOT silent).
    // Reparent/detach updates the scope. The inheritance-scope is DISTINCT from
    // the visual children list (S121's create/attach/detach + visual walk),
    // which composes with set_parent — recorded, no overlap, no gap.
    void set_parent(DependencyObject* parent) noexcept;
    [[nodiscard]] DependencyObject* parent() const noexcept;

    // --- Registration-ordered property-changed subscribers (S041 §2.3) ----
    // The processor's dispatch TARGET. `add_property_changed` appends a handler
    // for `id` in REGISTRATION ORDER (never hash order, never reverse) and
    // returns a removal token; `remove_property_changed(token)` removes it
    // (remaining handlers keep their registration order; a re-add appends at
    // the end). Handlers fire ONLY on an ACTUALLY APPLIED change to `id` —
    // either an on-affine Set that produced a real change, or a drain-applied
    // queued write (ProcessPendingChanges), OR a pushed descendant notification
    // flushed via ProcessNotificationQueue() (below). The existing metadata
    // `with_changed` cb is UNTOUCHED — this subscriber dispatch is additive
    // (S037 SetResult + marshalling tests stay green).
    using PropertyChangedHandler = std::function<void(PropertyId, const Value&, const Value&)>;
    using PropertyChangedToken   = std::size_t;

    // Adds a handler for `id`, firing in registration order. Returns a token
    // usable with remove_property_changed. Recovery from an exception during a
    // fire is the caller's concern (matches the metadata changed-cb contract).
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyId id, PropertyChangedHandler handler);

    // --- S245 destruction lifecycle (the "element destroy" notification) ----
    // A real destruction-lifecycle notification. `add_destroyed` registers a
    // handler that fires ONCE in the destructor (copy-before-invoke, BEFORE any
    // member teardown), in registration order; `remove_destroyed(token)` removes
    // one (no-op for an unknown/expired token). This is the "element destroyed"
    // signal a long-lived observer (e.g. a binding expression) listens to so it
    // can detach cleanly before the object's memory is reclaimed — the S245
    // "detach on element destroy" mechanism. Empty by default → zero cost unless
    // subscribed. ADR-018: single-threaded affinity, no new locks (mirrors subs_).
    using DestroyedHandler = std::function<void()>;
    using DestroyedToken   = std::size_t;

    [[nodiscard]] DestroyedToken add_destroyed(DestroyedHandler handler);
    void remove_destroyed(DestroyedToken token);

    // Removes the handler identified by `token`. No-op for an unknown/expired
    // token. Removal is O(n) but preserves the registration order of what
    // remains.
    void remove_property_changed(PropertyChangedToken token);

    // --- S041 descendant-push drain (the object's own notification queue) --
    // Each DependencyObject owns ONE per-thread ChangeQueue `queue_` (affinity
    // == this object's affinity) whose S041 notification family receives
    // descendant-push notifications (oldEffective -> newEffective) for inheriting
    // descendants, and whose S037 value-write family receives wrong-thread
    // writes (S143 unified postbox). ProcessNotificationQueue() drains the NOTIFICATION
    // half on the object's affinity thread (throws WrongThreadException from a
    // non-affine thread, mirroring S037's ProcessPendingChanges contract),
    // firing this object's registration-ordered subscribers for each pushed
    // property.
    void ProcessNotificationQueue();

    // Number of pushed notifications currently sitting in this object's
    // notification queue (queued-not-yet-fired). Cross-thread safe.
    [[nodiscard]] std::size_t pending_notification_count() const noexcept;

private:
    // -- ONE block-resident property-record type (no common/root) -------
    // A single flat PD holds BOTH metadata modes (S043):
    //   - OWNED   (`owned` non-null): object-scoped register_property() — the
    //     move-only PropertyMetadata is heap-owned (never copied) and addressed
    //     by pointer, so PD relocation never invalidates it.
    //   - BORROWED (`borrowed` non-null): an attached/global descriptor —
    //     NON-OWNING pointer to the process-lifetime global DependencyProperty's
    //     metadata. Safe (never dangles).
    // md() returns whichever is active (exactly one set for a live entry).
    struct PD
    {
        std::string_view         name;       // interned canonical key (S140, content-equal)
        Value                    local = Value::Null(); // Value::Null == no local override
        PropertyMetadata*        owned  = nullptr; // object-scoped owned metadata (heap-stable)
        const PropertyMetadata*  borrowed = nullptr; // attached/global borrowed metadata
        PropertyKey*             stableKey = nullptr; // stable heap key copy (register_property returned ref)

        // Returns whichever provider is active (exactly one for a live entry).
        const PropertyMetadata& md() const noexcept { return owned != nullptr ? *owned : *borrowed; }
    };

    // -- S142 block-resident store: inline-started, heap-grown on demand. -----
    // Lives at a fixed offset in the same allocation as the object (NO per-object
    // heap hop). Typical objects (0..kInline-1 stored props) touch zero heap; only
    // when count_ exceeds kInline is an append-only overflow array allocated (and
    // never shrunk). Lookup is a small linear scan (per-object props are few) by
    // the interned NAME — cache-friendly and insertion-ordered. See S142.
    class Store
    {
    public:
        static constexpr std::size_t kInline = 8;

        Store() = default;
        ~Store();
        Store(const Store&) = delete;
        Store& operator=(const Store&) = delete;
        Store(Store&& other) noexcept;
        Store& operator=(Store&& other) noexcept;

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

        // Linear scan over inline-then-overflow by interned name (content-equal).
        [[nodiscard]] PD* find(std::string_view name) noexcept
        {
            const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
            for (std::size_t i = 0; i < inlineN; ++i)
            {
                if (slots_[i].name == name)
                {
                    return &slots_[i];
                }
            }
            if (count_ > kInline)
            {
                for (std::size_t i = 0; i < count_ - kInline; ++i)
                {
                    if (overflow_[i].name == name)
                    {
                        return &overflow_[i];
                    }
                }
            }
            return nullptr;
        }
        [[nodiscard]] const PD* find(std::string_view name) const noexcept
        {
            const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
            for (std::size_t i = 0; i < inlineN; ++i)
            {
                if (slots_[i].name == name)
                {
                    return &slots_[i];
                }
            }
            if (count_ > kInline)
            {
                for (std::size_t i = 0; i < count_ - kInline; ++i)
                {
                    if (overflow_[i].name == name)
                    {
                        return &overflow_[i];
                    }
                }
            }
            return nullptr;
        }

        // Appends `pd` (which must not already be present) and returns the stored
        // PD. Grows onto the heap overflow array only when count_ >= kInline.
        PD* emplace(PD pd);

    private:
        // Frees the two OWNED heap nodes of a live PD slot (owned + stableKey).
        // Never touches `borrowed` (non-owning). Safe on a default (all-null) PD.
        static void release_slot(PD& pd) noexcept;

        PD          slots_[kInline];
        std::size_t count_    = 0;
        PD*         overflow_ = nullptr;
        std::size_t overflowCap_ = 0;
    };

    // S039 tolerant ancestor walk: returns the NEAREST ancestor of `self` with a
    // non-null LOCAL override for `name`, else Value::Null. Never throws (an
    // ancestor that does not register the property, or has no local, contributes
    // nothing). A same-class member so it may read the private store of any
    // instance on the parent chain.
    static Value nearest_ancestor_local(const DependencyObject& self, std::string_view name);

    // S041 §2.4 descendant push, driven by S039's pull model so each descendant
    // receives its OWN effective-old -> effective-new delta (never the parent's
    // coarse delta) and is notified ONLY when its effective value actually
    // changes (S040 local-wins respected through an intermediate, not just at the
    // direct child).
    //
    // collect_descendant_effectives() walks this object's inheritance-scope (and
    // recursively each child's scope) and captures each descendant with its
    // CURRENT effective value (computed pre-change by the caller, BEFORE the
    // origin stores its new value) into `out` (S144 — out-param, no return-by-
    // value). A descendant is captured when it either REGISTERS `name` (has a
    // local store entry) OR — S044 broadening — the property is a GLOBALLY
    // registered Inherits/InheritOnly attached property
    // (`captureAttachedInherits`): a purely-passive attached-Inherits descendant
    // never materializes a store entry, yet it reads (and must be notified of)
    // the inherited value. notify_descendants still filters to real change, so
    // over-capturing a passive descendant that ends up unchanged is harmless.
    //
    // notify_descendants() is called AFTER the origin has stored its new value:
    // for each captured descendant it recomputes the NEW effective value and
    // enqueues a pushed notification {name, oldEffective, newEffective} into that
    // descendant's OWN notification queue ONLY when old != new (local-wins /
    // inherited-unchanged descendants produce no notification).
    static void collect_descendant_effectives(
        const DependencyObject& root, std::string_view name, bool captureAttachedInherits,
        std::vector<std::pair<DependencyObject*, Value>>& out);
    void notify_descendants(
        std::string_view name,
        const std::vector<std::pair<DependencyObject*, Value>>& before);

    // S245: fires THIS object's registered destruction handlers (copy-before-
    // invoke, in registration order). Called at the START of the destructor,
    // BEFORE any member teardown, so an observer can still safely consult the
    // object's own value store / subscription state during the notification.
    void fire_destroyed();

    // S041 §2.5 (S244 completion): fires THIS object's own registration-ordered
    // add_property_changed subscribers for an actually-applied real change to
    // `name` (oldEffective -> newEffective). Called from BOTH the on-affine
    // Set() apply and the pending-drain apply (ProcessPendingChanges). This is
    // the object's SELF-change observation seam — the S041 header has always
    // documented it ("handlers fire on an on-affine Set that produced a real
    // change") but the implementation only ever fired the descendant-push
    // path; S244's target->source write-back needs the self-change seam (a
    // TextBox edit landing on the target DP fires the binding's write-back).
    // Copy-before-invoke (a handler that (un)subscribes mid-fire is safe) —
    // same discipline as ProcessNotificationQueue.
    void fire_self_subscribers(
        std::string_view name, const Value& oldEffective, const Value& newEffective);

    // Recursive scope walker used by collect_descendant_effectives(): walks
    // `this`'s inheritance-scope, capturing each descendant with its current
    // effective value (recursing into each child's scope). A descendant is
    // captured when it REGISTERS `name` (local store entry) or, when
    // `captureAttachedInherits` is set, whenever the property is a globally
    // registered attached Inherits/InheritOnly property (S044: passive attached
    // descendants must be notified even without a local store entry).
    void visit_scope(
        std::string_view name,
        std::vector<std::pair<DependencyObject*, Value>>& out,
        bool captureAttachedInherits) const;

    // ------------------------------------------------------------------
    // ONE block-resident unit: the per-object store + all inline state live
    // directly in the object's allocation (no per-object heap hop). S142
    // collapsed the pimpl here (pimpl removed, not preserved; ADR-003/ADR-019).
    // ------------------------------------------------------------------
    Store                 store_;            // block-resident property store (inline + overflow)
    std::thread::id       affinity_ = std::this_thread::get_id();
    DependencyObject*     parent_ = nullptr; // S039 loose non-owning parent link

    // S041 §2.3 registration-ordered subscribers (keyed by interned name).
    std::unordered_map<std::string_view,
        std::vector<std::pair<PropertyChangedToken, PropertyChangedHandler>>> subs_;
    PropertyChangedToken  next_sub_token_ = 1;

    // S245 destruction-lifecycle subscribers (registration order). A monotonic
    // token that is never reused; empty by default (zero cost unless subscribed).
    std::vector<std::pair<DestroyedToken, DestroyedHandler>> destroyed_;
    DestroyedToken next_destroyed_token_ = 1;

    // S041 §2.4 non-owning inheritance-scope (the inverse edge of parent_).
    std::vector<DependencyObject*> inheritance_scope_;

    // S143 (rewrites S037/S041): the SINGLE per-thread cross-thread postbox —
    // one ChangeQueue owning BOTH the S041 notification family and the S037
    // value-write family under ONE mutex + ONE affinity marker. The per-object
    // pending mutex physically relocated OUT of DependencyObject INTO the queue
    // (pending_/queue_mutex_ removed). Executed under ADR-018 — the kept
    // S037/S041 lock, moved not created; ZERO new locks.
    ChangeQueue           queue_;
};

} // namespace vaml
