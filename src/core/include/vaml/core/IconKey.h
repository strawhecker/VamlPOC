#pragma once
// VAML IconKey — the strong resource-key wrapper for icon assets.
// S360_1 (dep:S360) — Sprint 9, per ADR-015.
//
// A ViewModel exposes an `IconKey` ONLY — never a path, SVG string, or render
// object. The ResourceDictionary maps an IconKey to an icon-asset resource
// entry (an Object resource holding the icon asset reference); S400_1 (the
// Icon control) resolves that asset through the dictionary.
//
// IconKey is a strong string-wrapped key: it wraps a `std::string_view` value
// and compares BY CONTENT (operator==), so `IconKey("save") == IconKey("save")`
// regardless of which buffer each view points at. It is meant as a lightweight
// key handle (like PropertyId) — the owner passes a string that outlives it
// (the resource-key strings are interned, S140/S144).
//
// RESOLUTION (S360_1): `resolve_icon` looks a key up through the S360 lookup
// path and returns a typed `IconAssetRef` (the non-owning asset object) or the
// shared `ResourceLookupError::MissingKey` (decision 3 — the S360 missing-key
// error type is reused here AND by S361, NOT duplicated). `resolve_icon` does
// NOT touch render state — the asset reference is returned so S400_1 (and the
// S213_1 best-fit selection) consumes it. NO-GAP recorded (not silent):
// S400_1 consumes resolve_icon; DPI best-fit = S213_1; raster ownership = S202_1.
//
// ADR-018: ZERO new locks (pure key + dictionary lookup).

#include "vaml/core/ResourceDictionary.h" // ResourceValue, ResourceLookupError, find_for_element
#include "vaml/core/UIElement.h"          // UIElement (the resolution anchor)
#include "vaml/core/export.h"

#include <string_view>

namespace vaml {

// A strong string-wrapped icon-asset key. Content-equal; a ViewModel stores
// this (ADR-015) — never a path/SVG/render object.
struct VAML_CORE_EXPORT IconKey
{
    std::string_view value;

    IconKey() = default;
    explicit constexpr IconKey(std::string_view v) noexcept : value(v) {}

    [[nodiscard]] constexpr operator std::string_view() const noexcept { return value; }
    [[nodiscard]] bool operator==(const IconKey& other) const noexcept { return value == other.value; }
    [[nodiscard]] bool operator!=(const IconKey& other) const noexcept { return !(*this == other); }
};

// One RESOLVED icon-asset reference returned by resolve_icon: the non-owning
// Object resource the key maps to. `ok=false` => the key was missing (the
// error is delivered via `error`, the shared S360 missing-key category).
struct VAML_CORE_EXPORT IconAssetRef
{
    bool ok = false;
    const std::string_view key;       // the requested key
    const DependencyObject* asset = nullptr; // non-owning asset (Object kind), or nullptr
    ResourceLookupError error = ResourceLookupError::None; // MissingKey when !ok
};

// Resolves `key` through the S360 three-scope lookup anchored at `element`
// (element -> parent chain -> app -> system), returning the icon-asset entry
// as an IconAssetRef. Reuses the S360 ResourceLookupException::MissingKey
// category (decision 3 — shared with S361). Never throws. NO-GAP: S400_1
// (the Icon control) consumes resolve_icon.
[[nodiscard]] VAML_CORE_EXPORT IconAssetRef
resolve_icon(const UIElement& element, IconKey key) noexcept;

} // namespace vaml

// std::hash for IconKey (usable as an unordered container key, by content).
namespace std {
template <>
struct hash<vaml::IconKey>
{
    std::size_t operator()(const vaml::IconKey& k) const noexcept
    {
        return std::hash<std::string_view>{}(k.value);
    }
};
} // namespace std
