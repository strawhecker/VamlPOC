// S289 (dep:S288) — Pack/identity implementation. See Pack.h for the contract.
// Delivers the WPF-pack-style identity parser + the embedded-resource lookup +
// the pack-URI→bytes seam the release app-shell (S444) consumes.
//
// Single-threaded + pure (ADR-018: ZERO new locks); ADR-021 headless.

#include "vaml/tools/Pack.h"

#include <algorithm>
#include <stdexcept>

namespace vaml::tools::pack {

namespace {

// True when `c` is a blank (space/tab/CR/LF) character.
inline bool is_blank(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Trims leading/trailing blank characters from `s`.
inline std::string_view trim(std::string_view s) noexcept
{
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_blank(s[b]))
    {
        ++b;
    }
    while (e > b && is_blank(s[e - 1]))
    {
        --e;
    }
    return s.substr(b, e - b);
}

constexpr std::string_view kPackScheme = "pack://";

} // namespace

PackUri parse_pack_uri(std::string_view src) noexcept
{
    PackUri result;
    result.valid = false;
    result.error = PackUriError::InvalidPath;

    std::string_view s = trim(src);
    if (s.empty())
    {
        return result;   // nothing to parse → InvalidPath
    }

    // Bare-path identity: no "pack://" scheme.
    if (s.rfind(kPackScheme, 0) != 0)
    {
        result.path = std::string(trim(s));
        if (result.path.empty() || result.path.front() == '/' || result.path.back() == '/')
        {
            result.error = PackUriError::InvalidPath;
            return result;
        }
        result.valid = true;
        result.error = PackUriError::None;
        return result;
    }

    // Full pack URI: "pack://" then <authority> ",,," <path>.
    // WPF form: pack://application:,,,/Sub/Foo.xaml
    // The authority is the segment between "pack://" and the first ',' in the
    // ",,," separator. We accept 'application' / 'application,' / a bare
    // authority terminated by ',,' — anything else is UnsupportedAuthority.
    std::string_view body = s.substr(kPackScheme.size());

    // Find the ",,," separator (the path begins after it). The authority is
    // everything before the FIRST ',' (so "application" out of
    // "application:,,,").
    const std::size_t firstComma = body.find(',');
    const std::string_view authority =
        firstComma == std::string_view::npos ? body : body.substr(0, firstComma);

    std::string auth(trim(authority));
    // Normalize: strip a trailing ':' (WPF uses "application:" sometimes).
    while (!auth.empty() && auth.back() == ':')
    {
        auth.pop_back();
    }
    // Normalize: "application" is the ONLY supported authority (plus empty).
    if (!auth.empty() && auth != "application")
    {
        result.error = PackUriError::UnsupportedAuthority;
        result.authority = std::move(auth);
        return result;
    }
    result.authority = std::move(auth);

    // The path: everything after the ",,," separator.
    std::size_t pathStart = body.find(",,");
    if (pathStart != std::string_view::npos)
    {
        pathStart += 2;   // skip the ",,"
        // skip any ',' immediately after (the third comma of ",,,")
        while (pathStart < body.size() && body[pathStart] == ',')
        {
            ++pathStart;
        }
        std::string_view rawPath = body.substr(pathStart);
        // normalize: strip any leading '/' (WPF path segment separator)
        while (!rawPath.empty() && rawPath.front() == '/')
        {
            rawPath.remove_prefix(1);
        }
        result.path = std::string(trim(rawPath));
    }
    else
    {
        // No separator: a bare "pack://application" with no path → InvalidPath.
        result.path.clear();
    }

    if (result.path.empty())
    {
        result.error = PackUriError::InvalidPath;
        return result;
    }
    result.valid = true;
    result.error = PackUriError::None;
    return result;
}

ResourcePack::ResourcePack() = default;
ResourcePack::~ResourcePack() = default;
ResourcePack::ResourcePack(ResourcePack&&) noexcept = default;
ResourcePack& ResourcePack::operator=(ResourcePack&&) noexcept = default;

void ResourcePack::add_resource(std::string_view key, std::span<const std::uint8_t> bytes)
{
    if (has(key))
    {
        throw std::runtime_error("ResourceAlreadyRegistered: '" + std::string(key) + "'");
    }
    Entry e;
    e.key = std::string(key);
    e.bytes.assign(bytes.begin(), bytes.end());
    entries_.push_back(std::move(e));
}

void ResourcePack::add_embedded(std::string_view key, std::span<const std::uint8_t> bytes)
{
    add_resource(key, bytes);
}

std::span<const std::uint8_t> ResourcePack::resolve(std::string_view key) const noexcept
{
    for (const Entry& e : entries_)
    {
        if (e.key == key)
        {
            return std::span<const std::uint8_t>(e.bytes.data(), e.bytes.size());
        }
    }
    return {};
}

bool ResourcePack::has(std::string_view key) const noexcept
{
    for (const Entry& e : entries_)
    {
        if (e.key == key)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ResourcePack::registered_keys() const noexcept
{
    std::vector<std::string> keys;
    keys.reserve(entries_.size());
    for (const Entry& e : entries_)
    {
        keys.push_back(e.key);
    }
    return keys;
}

std::size_t ResourcePack::size() const noexcept
{
    return entries_.size();
}

std::span<const std::uint8_t> resolve_pack_uri(const ResourcePack& pack,
                                               std::string_view packUri) noexcept
{
    const PackUri parsed = parse_pack_uri(packUri);
    if (!parsed.valid)
    {
        return {};
    }
    return pack.resolve(parsed.path);
}

} // namespace vaml::tools::pack
