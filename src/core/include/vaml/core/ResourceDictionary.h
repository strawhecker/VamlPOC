#pragma once
// VAML ResourceDictionary + merged dictionaries + three-scope lookup.
// S360 (dep:S329) — Sprint 9, the styling/resource foundation.
//
// A ResourceDictionary maps a resource KEY (interned string_view, S140/S144
// discipline) to an owned `ResourceValue`. It is the store WPF's
// `FrameworkElement.Resources` / `ResourceDictionary` models: resources are
// authored under an x:Key, merged via MergedDictionaries semantics, and looked
// up across element -> parent chain -> app -> system scopes.
//
// RESOURCE VALUE MODEL (accepted Sprint-9 decision 2):
//   - Object   — an owned `DependencyObject` (a built Brush/Style/Template/
//                IconAsset, produced via the S087 ElementFactoryRegistry at
//                authoring time). Owned => the dictionary is move-only; the
//                resource's lifetime is the dictionary's.
//   - Plain    — a plain `Value` (a Color, a Thickness, a scalar...).
//   - Deferred — a not-yet-resolved REFERENCE ({StaticResource key} /
//                {DynamicResource key} / a TemplateBinding marker). S361/S365
//                resolve deferred rows at apply time. S360 carries the kind so
//                the store is forward-stable; it does NOT resolve them.
//
// MERGE PRECEDENCE (the S361 precedence contract, authored + proven HERE):
//   A dictionary's OWN entries always beat a merged child for the same key
//   (self-wins — WPF-faithful); among merged children the LAST merged wins
//   (reverse iteration). This is the exact order `find_resource` resolves.
//   [Reconciles the plan's "child-first-then-self" phrasing with its own
//   explicit "self always beats a merged child": self-wins + sibling-last-wins
//   is the WPF-correct, internally-consistent contract, and is what is tested.]
//
// INTERNED-KEY STORE: keys are interned string_views (S140 — pool-backed,
// content-equal, stable for the interning thread; NO owned std::string per
// entry). Insertion order is preserved (keys() returns registration order);
// dedupe-on-key throws `ResourceAlreadyRegistered` and leaves the store
// unchanged (the S032/S039 registry discipline).
//
// MISSING-KEY ERROR (shared by S360_1 icon resolution + S361 StaticResource):
//   `ResourceLookupError::{None,MissingKey}` + `ResourceLookupException`
//   carrying `key()`. get() throws it on a missing key; S360_1 and S361 both
//   REUSE it (decision 3 — one error type, two consumers, no duplication).
//
// THREE-SCOPE LOOKUP (roadmap "tree + app + system"):
//   - `find_resource(dict, key)`  — self + merged (the precedence above).
//   - `find_for_element(element, key)` — the element's OWN Resources -> parent
//     chain (each UIElement ancestor's Resources) -> app -> system.
//   - `ReferenceDictionary::app_resources()`  — the process-wide app scope
//     (a Meyer's singleton; the app installs its app dictionary here).
//   - `ReferenceDictionary::system_resources()` — a static immutable default
//     set (a small documented system-key default set TODAY; the full
//     SystemColors/Brushes feed is a later renderer/culture step — recorded
//     NO-GAP, not silent).
//
// ADR-018: ZERO new locks (single-threaded startup + per-thread use).

#include "vaml/core/DependencyObject.h" // owned DependencyObject (Object resources)
#include "vaml/core/UIElement.h"        // UIElement (find_for_element; resources() slot)
#include "vaml/core/Value.h"            // Value (Plain resources)
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

// The missing-key error category shared by S360.get, S360_1.resolve_icon and
// S361 StaticResource. One error type, two consumers (decision 3).
enum class ResourceLookupError
{
    None,       // no lookup error
    MissingKey, // the requested resource key is not present in scope
};

// Structured missing-key error. `key()` names the absent key; what() renders
// "resource '<key>' not found". Catched distinctively (like PropertyNotFound /
// ValueConversionException) so a caller can tell a missing resource from a
// value/type error.
class VAML_CORE_EXPORT ResourceLookupException : public std::runtime_error
{
public:
    ResourceLookupException(ResourceLookupError kind, std::string key, std::string what);

    [[nodiscard]] ResourceLookupError kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& key() const noexcept { return key_; }

private:
    ResourceLookupError kind_;
    std::string         key_;
};

// set() saw a key already present (dedupe — the S032/S039 registry discipline).
class VAML_CORE_EXPORT ResourceAlreadyRegistered : public std::logic_error
{
public:
    explicit ResourceAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// ONE dictionary entry: an owned built resource, a plain Value, or a deferred
// reference (resolved by S361/S365). Move-only (holds an owned unique_ptr).
class VAML_CORE_EXPORT ResourceValue
{
public:
    // The kind of value a dictionary entry holds.
    enum class Kind : std::uint8_t
    {
        Object   = 0, // an owned DependencyObject (built Brush/Style/Template/asset)
        Plain    = 1, // a plain Value (Color/Thickness/scalar)
        Deferred = 2, // an UNRESOLVED reference ({StaticResource}/{DynamicResource}/TemplateBinding)
    };

    // Non-movable deferred-reference payload (S361/S365 resolve these).
    enum class RefKind : std::uint8_t
    {
        StaticResource   = 0, // {StaticResource key} — resolve once at apply time
        DynamicResource  = 1, // {DynamicResource key} — a live link (S361)
        TemplateBinding  = 2, // {TemplateBinding prop} — read from the control DP (S365)
    };

    ResourceValue() = default;

    // Wrap an owned built resource (Object kind).
    explicit ResourceValue(std::unique_ptr<DependencyObject> obj);
    // Wrap a plain Value (Plain kind).
    explicit ResourceValue(Value value);
    // Wrap a deferred reference (Deferred kind).
    static ResourceValue DeferredRef(RefKind refKind, std::string_view key);

    // Move-only (owns a unique_ptr; a Value may own a std::string).
    ResourceValue(const ResourceValue&) = delete;
    ResourceValue& operator=(const ResourceValue&) = delete;
    ResourceValue(ResourceValue&&) noexcept;
    ResourceValue& operator=(ResourceValue&&) noexcept;
    ~ResourceValue();

    [[nodiscard]] Kind kind() const noexcept { return kind_; }

    // --- Object accessors (valid only when kind()==Object) -------------
    [[nodiscard]] DependencyObject* object() noexcept;        // non-owning mutable
    [[nodiscard]] const DependencyObject* object() const noexcept; // non-owning const

    // --- Plain accessors (valid only when kind()==Plain) ---------------
    [[nodiscard]] const Value& value() const noexcept;

    // --- Deferred accessors (valid only when kind()==Deferred) ---------
    [[nodiscard]] RefKind ref_kind() const noexcept;
    [[nodiscard]] std::string_view ref_key() const noexcept;

private:
    Kind kind_ = Kind::Plain;
    std::unique_ptr<DependencyObject> object_;
    Value        value_ = Value::Null();
    RefKind      refKind_ = RefKind::StaticResource;
    std::string  refKey_;
};

// The ResourceDictionary — move-only, pimpl-free (S142 collapse discipline),
// keyed on interned string_views, insertion-ordered, with WPF merge semantics.
class VAML_CORE_EXPORT ResourceDictionary
{
public:
    ResourceDictionary();
    ~ResourceDictionary();

    ResourceDictionary(const ResourceDictionary&) = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;
    ResourceDictionary(ResourceDictionary&&) noexcept;
    ResourceDictionary& operator=(ResourceDictionary&&) noexcept;

    // Registers `key` -> `value`. Keys are unique: an already-present key
    // throws ResourceAlreadyRegistered and leaves the store unchanged. The key
    // is interned (S140 pool-backed; stable for this thread; content-equal).
    void set(std::string_view key, ResourceValue value);
    // Same, over a built resource (Object kind) — the authoring convenience a
    // Brush/Style/Template resource uses.
    void set(std::string_view key, std::unique_ptr<DependencyObject> obj);

    // Removes the SELF entry for `key` (not merged children). Returns true when
    // a self entry was present and removed. Used by tests/teardown to release a
    // resource without outliving a process-lifetime scope.
    bool remove(std::string_view key) noexcept;

    // --- Lookup ----------------------------------------------------------
    [[nodiscard]] bool has(std::string_view key) const noexcept;
    // Returns the entry for `key` (self + merged, self-wins / sibling-last-wins),
    // or nullptr when absent. Never throws.
    [[nodiscard]] const ResourceValue* try_get(std::string_view key) const noexcept;
    // Same, but throws ResourceLookupException(MissingKey) on absence.
    [[nodiscard]] const ResourceValue& get(std::string_view key) const;

    // --- Merged dictionaries (S360 merge semantics) ----------------------
    // Adds `merged` as a BORROWED merged child. A child's OWN entries are
    // searched after this dictionary's own (self-wins); among merged children
    // the LAST merged wins (reverse iteration). `merged` must outlive this
    // dictionary (the app owns both). Idempotent: adding the same dictionary
    // twice is a no-op.
    void merge_with(ResourceDictionary& merged);
    // The merged children, in merge order.
    [[nodiscard]] const std::vector<ResourceDictionary*>& merged() const noexcept;

    // --- Introspection ---------------------------------------------------
    // Number of SELF entries (excluding merged children).
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }
    // Self keys in INSERTION ORDER (interned views, valid for this dict's life).
    [[nodiscard]] std::vector<std::string_view> keys() const;

private:
    // One interned-keyed entry. ResourceValue is move-only, so the vector is a
    // stable ordering of owned entries.
    struct Entry
    {
        std::string_view key; // interned
        ResourceValue    value;
    };

    [[nodiscard]] const Entry* find_self(std::string_view key) const noexcept;
    [[nodiscard]] Entry* find_self(std::string_view key) noexcept;

    std::vector<Entry>             entries_; // self, insertion order
    std::vector<ResourceDictionary*> merged_; // borrowed merged children, merge order
};

// --- Three-scope lookup -----------------------------------------------------

// S360 element-resource side-table (out-of-line; see ResourceDictionary.cpp for
// why — the S145 object-pool budget). `set_element_resources` takes ownership
// of the element's ResourceDictionary; `get_element_resources` returns the
// non-owning pointer (nullptr when the element has none); `remove_element_`
// `resources` releases + erases an element's entry (e.g. on element destroy);
// `element_resources_master_reset` clears the whole table (test/teardown).
// Single-threaded UI use (ADR-018: no lock).
namespace detail {

VAML_CORE_EXPORT void set_element_resources(UIElement* element,
                                            std::unique_ptr<ResourceDictionary> resources);
VAML_CORE_EXPORT const ResourceDictionary* get_element_resources(const UIElement* element) noexcept;
VAML_CORE_EXPORT void remove_element_resources(UIElement* element) noexcept;
VAML_CORE_EXPORT void element_resources_master_reset() noexcept;

} // namespace detail

// The app-scope + system-scope singletons (WPF ReferenceDictionary analogue).
class VAML_CORE_EXPORT ReferenceDictionary
{public:
    // The process-wide APP resource scope. A Meyer's function-local static
    // (mirrors PropertyRegistry/ElementFactoryRegistry::global). The app (the
    // S250 app-owned composition pattern) installs its app dictionary HERE by
    // calling set_app_resources(...). Lookup walks app-scope AFTER the element
    // tree and BEFORE the system scope.
    [[nodiscard]] static ResourceDictionary& app_resources() noexcept;

    // The static IMMUTABLE SYSTEM default set. A small documented system-key
    // default set is placed HERE at S360 (a handful of standard resource keys);
    // the full SystemColors/SystemBrushes feed (S12 renderer / culture) is a
    // recorded later step, NOT silent. Lookup reaches system scope LAST.
    [[nodiscard]] static const ResourceDictionary& system_resources() noexcept;
};

// Self + merged lookup (the S360 precedence: self wins, sibling-last-wins).
// Returns nullptr when absent. Never throws.
[[nodiscard]] VAML_CORE_EXPORT const ResourceValue*
find_resource(const ResourceDictionary& dict, std::string_view key) noexcept;

// Three-scope lookup (roadmap "tree + app + system"): the element's own
// Resources -> each UIElement ancestor's Resources (via parent()) -> app ->
// system. Returns nullptr when absent. Never throws.
[[nodiscard]] VAML_CORE_EXPORT const ResourceValue*
find_for_element(const UIElement& element, std::string_view key) noexcept;

} // namespace vaml
