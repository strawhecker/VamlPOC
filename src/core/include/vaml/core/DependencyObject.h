#pragma once
// VAML DependencyObject — the base value-holder for the dependency-property
// system. S036 (dep:S035) + S037 (dep:S036_5, marshalling foundation) +
// S039 (dep:S038, effective-value resolution).
//
// S039 title contract (read-side, pull model):
//   8. GetEffectiveValue() — locals ?? inherited-from-parent ?? default. The
//      InheritOnly arm is RETIRED by S814: every inheriting property resolves
//      local ?? inherited ?? default (a local override on the object WINS,
//      mirroring EvaluateEffectiveValue / GetValueEntry in WPF). `Get`
//      delegates to it (non-inheriting properties keep local??default).
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
// S037 (this step) title contract — wrong-thread access THROWS:
//   5. Set()/Get() are thread-affinity aware: any access from a thread other
//      than the object's creating (affinity) thread THROWS WrongThreadException
//      (mirror WPF DependencyObject.VerifyAccess, DO.cs:161/412/...). There is
//      NO cross-thread value queue — a non-affine write is refused with a clear
//      error, never silently deferred. S811 retracts S037's original write-
//      QUEUING (queued-not-applied + ProcessPendingChanges drain).
//   6. (removed by S811) — no pending queue, no ProcessPendingChanges, no
//      pending_change_count.
//   7. (removed by S811) — no queue depth to observe.
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
//   - PIPELINE IN Set(). validate runs on the RAW input FIRST (WPF
//     DO.cs:653 — a raw value invalid at the boundary is rejected/thrown
//     before coercion), then coerce, then validate ON THE COERCED value (WPF
//     DO.cs:1692) — the corrected D1 (S810) ordering. The changed cb fires with
//     the OLD effective (local override if present else default) and the
//     coerced NEW value, and ONLY when they differ (real change). A rejected
//     value leaves the store unchanged and fires nothing.
//   - THREAD-AFFINITY MARKER. The ctor captures std::this_thread::get_id().
//     S036 records + exposes it (thread_affinity_id / is_affine_to_current_thread);
//     S037 (this step) consumes it as the basis for wrong-thread detection.
//   - WRONG-THREAD ACCESS THROWS (S037 corrected by S811). Set()/Get() from a
//     non-affine thread THROW WrongThreadException (WPF VerifyAccess,
//     DO.cs:161/412/...). S037's original cross-thread value QUEUE (queued-not-
//     applied + ProcessPendingChanges drain + SetResult::QUEUED) and S041's
//     fabricated change-notification pipeline (per-object subscriber registry,
//     ProcessNotificationQueue, descendant-push Inherits postbox,
//     ChangeQueue/ChangeProcessor/ChangeRecorder) are RETRACTED by S811 — there
//     is NO per-object multi-subscriber list in WPF (DO.cs:1704/1732/2039).
//   - CROSS-THREAD CONTRACT (S037 corrected). The value store stays affinity-
//     thread-owned by construction: only the affinity thread stores (any Set
//     from a non-affine thread throws before touching the store). Get() reads
//     the store and is declared safe on the affinity thread or when no
//     concurrent affinity-thread mutation is in flight (WPF-faithful; full
//     store-race freedom is TSan/S620/S723 territory, recorded not silent).
//   - SYNCHRONOUS NOTIFICATION (S811). The changed notification is the single
//     PropertyMetadata::with_changed/notifyChanged callback fired inside Set()
//     on a real change (== WPF PropertyChangedCallback, DO.cs:2039). There is
//     NO deferred/queued notification, NO subscriber list, NO coalescing —
//     WPF has none of those.
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

// Forward declaration of the global dependency-property descriptor (S039/S043).
// DependencyObject resolves an attached/global property's canonical flags +
// default from this type; the full definition lives in DependencyProperty.h
// (included only by DependencyObject.cpp to avoid a heavier public dependency).
class DependencyProperty;

// Forward declaration of the expression-as-value interface (WPF `Expression`,
// DO.cs:705-708 / Expression.cs:140-155). A bound target DP holds ONE live
// expression (e.g. a BindingExpression); a Set() on the affinity thread is
// routed through its `SetValue` for the two-way write-back. The full definition
// lives in DependencyPropertyExpression.h (included only where needed).
class DependencyPropertyExpression;

// Set() outcome, mirroring the S035 MiniStore pipeline + the S037 marshalling
// extension. The first three mirror S035 exactly; QUEUED is S037's addition
// (backward-compatible — the three original values are unshifted).
enum class SetResult
{
    APPLIED,   // accepted, changed cb fired, stored
    REJECTED,  // S810_1: RETIRED as the out-of-range disposition. A Set() whose
               // ValidateFunc refuses the value now THROWS InvalidPropertyValueException
               // (WPF ArgumentException(InvalidPropertyValue), DO.cs:653/1692) instead
               // of returning this. This enum member REMAINS DEFINED (for binary/
               // source backward compatibility with any surviving consumer) but the
               // Set pipeline never returns it. Removal of the appended S037 tail
               // (QUEUED) is S811's scope; REJECTED is a pre-S811 member and stays.
    NO_CHANGE, // coerced == current effective; no cb (real change only)
    // S811: SetResult::QUEUED (the S037 wrong-thread write-queueing appended
    // tail) is REMOVED. A non-affine Set/Get now THROWS WrongThreadException
    // (mirror WPF DependencyObject.VerifyAccess, DO.cs:161/412/...). This is a
    // BACKWARD-COMPATIBLE enum change: only the appended tail is dropped, so
    // the pre-S037 members (APPLIED/REJECTED/NO_CHANGE) keep their values.
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

// S810_1: a Set() whose ValidateFunc refuses the (raw or coerced) value throws
// this — the WPF ArgumentException(InvalidPropertyValue) disposition at
// DO.cs:653 (raw validate) and DO.cs:1692 (post-coerce re-validate). It names
// the offending property and the value that was refused so a binding can map
// it to TransferResult::TargetRejected at the binding boundary (ADR-016 —
// observation, never control-flow in the core). `std::invalid_argument` base
// matches WPF's ArgumentException lineage.
class VAML_CORE_EXPORT InvalidPropertyValueException : public std::invalid_argument
{
public:
    InvalidPropertyValueException(std::string what, std::string propertyName, std::string rejectedValue)
        : std::invalid_argument(std::move(what))
        , propertyName_(std::move(propertyName))
        , rejectedValue_(std::move(rejectedValue)) {}

    // The offending property in canonical dotted `owner.name` form.
    [[nodiscard]] const std::string& property_name() const noexcept { return propertyName_; }
    // The value that failed validation, rendered as text.
    [[nodiscard]] const std::string& value() const noexcept { return rejectedValue_; }

private:
    std::string propertyName_;
    std::string rejectedValue_;
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
    // The object's internal dynamic state (Store overflow, children_/
    // expressions_ map, destroyed_ list, owned PropertyMetadata/PropertyKey
    // nodes, Grid::Impl) is torn down NORMALLY by the destructor; only the
    // object's slab bytes are retained on the free-list and reused — recycle ==
    // slab reuse, not state carryover.
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

    // True when this object holds a LOCAL override for the property named by
    // `id` (i.e. a value was Set on THIS object, distinct from an inherited or
    // default value). S363 (the style applier) uses this for the WPF-faithful
    // LOCAL-WINS contract: a style setter must NOT clobber an authored local
    // value. A Null-valued local IS a local (a store entry with local != Null).
    // Returns false for an unregistered/absent property or an inherited/default
    // value. Never throws.
    [[nodiscard]] bool has_local_value(PropertyId id) const noexcept;

    // Reads the property's current EFFECTIVE value. S039 is the read-side
    // resolution primitive; `Get` delegates to it. Resolution order:
    //   - NOT Inherits:          local ?? default
    //   - Inherits:              local ?? inherited-from-parent ?? default
    //                            (a local override on this object WINS, exactly
    //                             as EvaluateEffectiveValue / GetValueEntry do
    //                             in WPF — the InheritOnly arm is retired by
    //                             S814)
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

    // Writes `input` through the D1 (S810) pipeline (validate(raw) -> coerce ->
    // validate(coerced) -> fire-on-real-change -> store) — the WPF order
    // (DO.cs:653 raw-validate, DO.cs:1692 coerce + re-validate). Throws
    // PropertyNotFound for an unregistered PropertyId, ValueTypeMismatch for a
    // wrong-tag input.
    //
    // S810_1 validation disposition: a ValidateFunc refusal — at the RAW gate
    // (DO.cs:653) OR on the POST-COERCE re-check (DO.cs:1692) — THROWS
    // InvalidPropertyValueException (WPF ArgumentException(InvalidPropertyValue)),
    // naming the property + the offending value. SetResult::REJECTED is retired
    // as the out-of-range disposition on this pipeline (the enum member remains
    // defined; removal is S811). A refused value leaves the store unchanged and
    // fires nothing.
    //
    // S037 (corrected by S811) affinity-aware behavior: this is exactly the
    // S035 pipeline (APPLIED/NO_CHANGE, or InvalidPropertyValueException on a
    // validate refusal) ON THE AFFINE THREAD. On a NON-affine thread, Set()
    // THROWS WrongThreadException (mirror WPF DependencyObject.VerifyAccess,
    // DO.cs:412) BEFORE coerce/validate — there is no cross-thread value queue,
    // no deferral. The store is NEVER written from a non-affine thread.
    //
    // S045 read-only contract: if the resolved property carries
    // PropertyFlags::ReadOnly, Set() throws PropertyReadOnlyException. The
    // check runs IMMEDIATELY after resolving the metadata provider and BEFORE
    // coerce / validate / the affinity branch — so a read-only refusal is
    // structural and value-independent (throws even for value == current
    // effective, never NO_CHANGE, never a validate refusal) and fires on ANY
    // thread (a wrong-thread read-only Set throws WrongThreadException or
    // PropertyReadOnlyException, whichever its guard reaches first). No local
    // override is materialized for a refused read-only write.
    [[nodiscard]] SetResult Set(PropertyId id, const Value& input);

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
    // S039 is read-side only.
    //
    // S811: the S041 inverse-edge (set_parent registering this object into the
    // parent's inheritance-scope for a descendant-push postbox) is RETRACTED.
    // The INHERITANCE VALUE still propagates through the S039 ancestor walk in
    // Get()/GetEffectiveValue() (pull model) — only the fabricated "push a
    // notification into a descendant's queue" surface is gone. There is no
    // inheritance-scope member, no descendant push, no per-object notification
    // queue in WPF (DO.cs:1704/1732/2039).
    void set_parent(DependencyObject* parent) noexcept;
    [[nodiscard]] DependencyObject* parent() const noexcept;

    // --- S818 OnPropertyChanged virtual (the WPF DO.cs:2039 change call-out) --
    // A protected virtual, the WPF `DependencyObject.OnPropertyChanged(
    // DependencyPropertyChangedEventArgs)` analogue (DO.cs:2039 — invoked ON a
    // real effective-value change, after the change is detected, synchronous,
    // affinity-thread). The default is a no-op; `UIElement` overrides it to run
    // the `FrameworkElement.OnPropertyChanged` (FE.cs:2198-2251) Affects*
    // invalidation (InvalideMeasure/Arrange/Visual + the nearest layout-parent
    // walk). Called from Set() ONLY when a real change occurred (NO_CHANGE does
    // not fire it). `flags` is the resolved property's flag set (the carrier of
    // the Affects* arms); `oldValue`/`newValue` are the effective-value old/new
    // (mirroring the changed-cb contract).
protected:
    virtual void OnPropertyChanged(
        PropertyId id,
        const Value& oldValue,
        const Value& newValue,
        PropertyFlags flags);

public:
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

    // --- S811 expression-as-value accessors (public: the binding attachment ---
    // seam). A bound target DP holds ONE live expression (e.g. a
    // BindingExpression); a Set() on the affinity thread is routed through
    // `expr->SetValue(...)` for the two-way write-back BEFORE the plain store
    // (WPF SetValueCommon, DO.cs:705-708). `set_current_expression(id, null)`
    // detaches. There is NO per-object subscriber list (DO.cs:1704/1732/2039).
    [[nodiscard]] DependencyPropertyExpression* current_expression(PropertyId id) const noexcept;
    void set_current_expression(PropertyId id, DependencyPropertyExpression* expr) noexcept;

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
    //
    // S802 (dep:S801) — the per-object store identity is the COMPOSITE
    // (name, owner) pair (WPF ground truth: DependencyProperty.cs
    // `FromNameKey(name, ownerType)`). `name` AND `owner` are both INTERNED
    // canonical views (S140/ADR-018, content-equal, stable for the affinity
    // thread's life). Two same-name/different-owner DPs are DISTINCT store
    // entries that resolve independently.
    struct PD
    {
        std::string_view         name;       // interned canonical key (S140, content-equal)
        std::string_view         owner;      // S802: interned canonical DECLARING-TYPE identity (empty ONLY on a legacy bridge entry)
        Value                    local = Value::Null(); // Value::Null == no local override
        PropertyMetadata*        owned  = nullptr; // object-scoped owned metadata (heap-stable)
        const PropertyMetadata*  borrowed = nullptr; // attached/global borrowed metadata
        PropertyKey*             stableKey = nullptr; // stable heap key copy (register_property returned ref)

        // Composite identity helper — equal IFF name AND owner are equal.
        [[nodiscard]] bool matches(std::string_view n, std::string_view o) const noexcept
        {
            return owner == o && name == n;
        }

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

        // S802 composite lookup: linear scan over inline-then-overflow by the
        // interned (name, owner) PAIR (content-equal). Two same-name/
        // different-owner entries are distinct.
        [[nodiscard]] PD* find(std::string_view name, std::string_view owner) noexcept
        {
            const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
            for (std::size_t i = 0; i < inlineN; ++i)
            {
                if (slots_[i].matches(name, owner))
                {
                    return &slots_[i];
                }
            }
            if (count_ > kInline)
            {
                for (std::size_t i = 0; i < count_ - kInline; ++i)
                {
                    if (overflow_[i].matches(name, owner))
                    {
                        return &overflow_[i];
                    }
                }
            }
            return nullptr;
        }
        [[nodiscard]] const PD* find(std::string_view name, std::string_view owner) const noexcept
        {
            const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
            for (std::size_t i = 0; i < inlineN; ++i)
            {
                if (slots_[i].matches(name, owner))
                {
                    return &slots_[i];
                }
            }
            if (count_ > kInline)
            {
                for (std::size_t i = 0; i < count_ - kInline; ++i)
                {
                    if (overflow_[i].matches(name, owner))
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
    // non-null LOCAL override for the (name, owner) PAIR, else Value::Null. Never
    // throws (an ancestor that does not register the property, or has no local,
    // contributes nothing). A same-class member so it may read the private store
    // of any instance on the parent chain.
    static Value nearest_ancestor_local(
        const DependencyObject& self, std::string_view name, std::string_view owner);

    // S802 resolution helpers. Chosen so a legacy owner-less id (PropertyId::of
    // (name)) still resolves through the GUARDED unique-name store bridge while
    // a composite id resolves on its (name, owner) pair.
    //   - resolve_entry(id): the per-object store entry for `id`, or nullptr —
    //     composite pair when id.owner() != "", else find_unique_name (guarded:
    //     nullptr on an ambiguous same-name/different-owner coexist pair).
    //   - resolve_global(id): the global DependencyProperty for `id`, or
    //     nullptr — DependencyProperty::TryGet(name, owner) when the id carries
    //     an owner, else the GUARDED TryGetByName (unique-name bridge,
    //     MINIMAL REAL, S803-resolved).
    [[nodiscard]] const PD* resolve_entry(PropertyId id) const noexcept;
    [[nodiscard]] PD* resolve_entry(PropertyId id) noexcept;
    [[nodiscard]] const DependencyProperty* resolve_global(PropertyId id) const noexcept;

    // S245: fires THIS object's registered destruction handlers (copy-before-
    // invoke, in registration order). Called at the START of the destructor,
    // BEFORE any member teardown, so an observer can still safely consult the
    // object's own value store / subscription state during the notification.
    void fire_destroyed();

    // ------------------------------------------------------------------
    // ONE block-resident unit: the per-object store + all inline state live
    // directly in the object's allocation (no per-object heap hop). S142
    // collapsed the pimpl here (pimpl removed, not preserved; ADR-003/ADR-019).
    // ------------------------------------------------------------------
    Store                 store_;            // block-resident property store (inline + overflow)
    std::thread::id       affinity_ = std::this_thread::get_id();
    DependencyObject*     parent_ = nullptr; // S039 loose non-owning parent link

    // S245 destruction-lifecycle subscribers (registration order). A monotonic
    // token that is never reused; empty by default (zero cost unless subscribed).
    std::vector<std::pair<DestroyedToken, DestroyedHandler>> destroyed_;
    DestroyedToken next_destroyed_token_ = 1;

    // S811 expression store: the DP->expression map for expression-as-value
    // (WPF's effective-value expression marker). A bound target DP holds one
    // live expression (e.g. a BindingExpression) that intercepts subsequent
    // Set() calls on the affinity thread (see current_expression/set_ and Set).
    // There is NO per-object subscriber list — only this per-DP single
    // expression slot.
    std::unordered_map<PropertyId, DependencyPropertyExpression*> expressions_;
};

} // namespace vaml
