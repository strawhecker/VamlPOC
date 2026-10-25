#pragma once
// S289 (dep:S288) — Pack/identity: x:Class name mapping; embedded resource
// lookup; release uses compiled only.
//
// The identity + pack layer the RELEASE app-shell (S444) builds on, and the
// resolution of the S092/S286-recorded "x:Class → owner binding deferred"
// NO-GAP. Delivers two REAL permanent Category-1 facilities (none MINIMAL REAL):
//
//   1. `PackUri` + `parse_pack_uri` — the WPF-pack-style identity, honestly C++:
//      accepts `pack://application:,,,/Path.vbaml` or a bare `Path.vbaml`,
//      parses into {authority, path} with an AUTHORITY RULE (only `application`
//      or empty; any other authority → PackUriError::UnsupportedAuthority),
//      and normalizes to a resource KEY (the path). Empty/invalid path →
//      PackUriError::InvalidPath. Pure + headless-testable (ADR-021).
//
//   2. `ResourcePack` (RAII) — the EMBEDDED-RESOURCE LOOKUP: `add_resource(key,
//      span)` (dedupe-throws `ResourceAlreadyRegistered`), `add_embedded(key,
//      span)` (convenience over the S287 `compiled_vbaml_<symbol>()` spans),
//      `resolve(key) → span` (nullopt / structured ResourceNotFound naming the
//      key), `has`/`registered_keys`. CONTENT-keyed storage mirroring the
//      S143/S144 NameTable discipline (no transient std::string on lookup; the
//      key is interned/copied once and owned).
//
//   3. `resolve_pack_uri(pack, packUri)` — the seam that maps a pack identity
//      to its compiled bytes (parses the pack URI → resolve the parsed path
//      through the pack).
//
// "RELEASE USES COMPILED ONLY" (recorded, NOT silent): the release consumer
// (S444 app-shell / S484 single-exe) faces ONLY `InitializeComponentFrom` /
// `VBamlReader` + `ResourcePack` (compiled bytes). The text `VamlReader` +
// vamlcc are DEV tools. The link-level enforcement is the release app-shell —
// linkage is the C++ boundary where release LINKS vaml-tools for VBamlReader
// and NEVER the text reader. Recorded not silent.
//
// Single-threaded + pure (ADR-018: ZERO new locks); ADR-021 headless-testable.

#include "vaml/tools/export.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::tools::pack {

// S289: the pack-URI error taxonomy — ONLY what a pack identity parse can
// produce (S048 discipline — no dead arms).
enum class PackUriError
{
    None,                  // no error (the parse succeeded)
    InvalidPath,           // the pack identity has an empty/invalid resource path
    UnsupportedAuthority,  // the authority is neither empty nor `application`
};

// S289: ONE parsed pack identity. `authority` is `application`, `application,`
// normalization, or EMPTY (a bare path identity has no authority);
// `path` is the normalized resource key. `valid` is true only after a clean
// parse (`error == PackUriError::None`).
struct PackUri
{
    std::string authority;   // "application" or "" 
    std::string path;        // the normalized resource key (e.g. "MainView.vbaml")
    bool        valid = false;
    PackUriError error = PackUriError::None;
};

// S289: parses a pack identity into a PackUri. Accepts:
//   * `pack://application:,,,/Path.vbaml`   → { authority="application", path="Path.vbaml" }
//   * a bare `Path.vbaml`                    → { authority="", path="Path.vbaml" }
// An explicit `pack://<other-authority>` → UnsupportedAuthority. An empty /
// blank path → InvalidPath. Pure + headless-testable (ADR-021); noexcept.
[[nodiscard]] VAML_TOOLS_EXPORT PackUri parse_pack_uri(std::string_view src) noexcept;

// S289: the EMBEDDED-RESOURCE lookup (RAII). Maps a resource KEY (the
// normalized path from a PackUri) to a std::span over the compiled .vbaml
// bytes (the S287 embedded spans). Content-keyed; the key is owned/copied once
// at registration, never re-allocated on lookup (the S143/S144 NameTable
// discipline — no transient std::string on resolve). Single-threaded + pure
// (ADR-018: ZERO new locks).
class VAML_TOOLS_EXPORT ResourcePack
{
public:
    ResourcePack();
    ~ResourcePack();

    ResourcePack(const ResourcePack&) = delete;
    ResourcePack& operator=(const ResourcePack&) = delete;
    ResourcePack(ResourcePack&&) noexcept;
    ResourcePack& operator=(ResourcePack&&) noexcept;

    // Registers `bytes` under `key`. A duplicate key THROWS
    // `ResourceAlreadyRegistered` and leaves the pack unchanged (never
    // last-wins silently — a duplicate embed is an authoring/link error).
    // The span is copied into pack-owned storage (the S287 register span is
    // usually static-lifetime, but we own it so the pack is self-contained).
    void add_resource(std::string_view key, std::span<const std::uint8_t> bytes);

    // Convenience over the S287 embedded span accessors
    // (`vaml::tools::compiled_vbaml_<symbol>()`). Equivalent to add_resource.
    void add_embedded(std::string_view key, std::span<const std::uint8_t> bytes);

    // Resolves `key` → a span over the registered bytes. Returns std::nullopt
    // when the key is not registered (structured ResourceNotFound — the caller
    // learns the missing key, never a silent zero-length span). The returned
    // span is valid for the pack's lifetime (and outlives the caller).
    [[nodiscard]] std::span<const std::uint8_t> resolve(std::string_view key) const noexcept;

    // True when `key` is registered.
    [[nodiscard]] bool has(std::string_view key) const noexcept;

    // The registered keys, in REGISTRATION order (deterministic; returned
    // BY VALUE so the caller gets a stable, copyable snapshot).
    [[nodiscard]] std::vector<std::string> registered_keys() const noexcept;

    // Number of registered resources.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    struct Entry
    {
        std::string key;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Entry> entries_;
};

// S289: maps a pack identity to its compiled bytes through a ResourcePack.
// Parses `packUri` (structured error on a bad identity) and resolves the
// normalized path through `pack`. Returns the span, or std::nullopt when the
// path is not a registered resource. This is the seam S444's
// `InitializeComponentFrom` consumes. noexcept.
[[nodiscard]] VAML_TOOLS_EXPORT std::span<const std::uint8_t>
resolve_pack_uri(const ResourcePack& pack, std::string_view packUri) noexcept;

} // namespace vaml::tools::pack
