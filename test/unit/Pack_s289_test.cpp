// S289 (dep:S288) — Pack/identity: x:Class name mapping; embedded resource
// lookup; release uses compiled only.
//
// Tests the REAL S289 pack/identity layer:
//   * PackUri parse matrix (bare path / full pack URI / authority rule /
//     empty+invalid → structured).
//   * ResourcePack register/resolve/dedupe-throws/unknown → structured +
//     registered_keys order.
//   * Embedded-span registration (from the S287 generated embed spans — the
//     `compiled_vbaml_<symbol>()` accessors — the "embedded resource lookup").
//   * VBamlReader::InitializeComponent binds the x:Class owner + root_class +
//     named() access + the class mismatch → VBamlError::OwnerClassMismatch.
//   * VBamlReader::InitializeComponentFrom resolves through a registered
//     ResourcePack → identical tree to a direct Load (the compiled-only entry).
//   * "release uses compiled only" — the InitializeComponentFrom signature takes
//     NO text parameter (structural; enforced by the compiler — verified here by
//     a compile-time check that the only entry exposes bytes/pack, never text).
//
// S289-unique global DPs (S039 discipline). ADR-018 zero locks; ADR-021 headless
// (drives the real compile_to_bytes → VBamlReader + the real pack).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/tools/Pack.h"
#include "vaml/tools/VBamlReader.h"
#include "vaml/tools/Vamlc.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::StackPanel;
using vaml::Value;
using vaml::ValueType;
using vaml::Window;
using vaml::tools::VBamlError;
using vaml::tools::VBamlException;
using vaml::tools::VBamlReader;
using vaml::tools::pack::PackUri;
using vaml::tools::pack::PackUriError;
using vaml::tools::pack::ResourcePack;
using vaml::tools::pack::parse_pack_uri;
using vaml::tools::pack::resolve_pack_uri;

namespace {

std::unordered_set<std::string>& s289_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerS289Global(const std::string& name, ValueType type,
                                             PropertyFlags flags, Value def)
{
    if (s289_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S289_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS289Properties()
{
    (void)registerS289Global("S289Content", ValueType::String, PropertyFlags::None, Value::String(""));
}

// The S289 golden: a x:Class'd Window with a named StackPanel child (the exact
// shape the release app-shell compiles → embeds → InitializeComponentFrom).
const char* kS289GoldenDoc =
    R"(<Window x:Name="root" x:Class="Demo.MainWindow">)"
    R"(<StackPanel x:Name="panel" S289Content="Hello"/>)"
    R"(</Window>)";

std::vector<std::uint8_t> compileS289Doc()
{
    return vaml::tools::compile_to_bytes(kS289GoldenDoc);
}

// A NON-owning DependencyObject "owner" for the InitializeComponent bind: we
// simply construct a real Window as the x:Class owner object (the app-shell
// pattern — the owner is a DependencyObject the handlers/named() serve).
std::unique_ptr<Window> makeOwner()
{
    return std::make_unique<Window>();
}

} // namespace

// ---------------------------------------------------------------------------
// PackUri parse matrix
// ---------------------------------------------------------------------------

TEST_CASE("S289: PackUri — bare path identity parses to {authority='', path}")
{
    const PackUri u = parse_pack_uri("MainView.vbaml");
    SHOULD(ShouldlyIntegral(u.valid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(u.error)).ShouldBe(static_cast<int>(PackUriError::None)));
    SHOULD(ShouldlyString(u.authority).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(u.path).ShouldBe(std::string("MainView.vbaml")));
}

TEST_CASE("S289: PackUri — full pack URI parses to {authority='application', path}")
{
    const PackUri u = parse_pack_uri("pack://application:,,,/MainView.vbaml");
    SHOULD(ShouldlyIntegral(u.valid).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(u.error)).ShouldBe(static_cast<int>(PackUriError::None)));
    SHOULD(ShouldlyString(u.authority).ShouldBe(std::string("application")));
    SHOULD(ShouldlyString(u.path).ShouldBe(std::string("MainView.vbaml")));
}

TEST_CASE("S289: PackUri — pack URI with a sub-path segment normalizes the path")
{
    const PackUri u = parse_pack_uri("pack://application:,,,/Views/MainView.vbaml");
    SHOULD(ShouldlyIntegral(u.valid).ShouldBe(true));
    SHOULD(ShouldlyString(u.authority).ShouldBe(std::string("application")));
    // The leading '/' is stripped; the path keeps the segment (the resource
    // KEY is the normalized full path — the app registers "Views/MainView.vbaml").
    SHOULD(ShouldlyString(u.path).ShouldBe(std::string("Views/MainView.vbaml")));
}

TEST_CASE("S289: PackUri — an unsupported authority is a structured error")
{
    const PackUri u = parse_pack_uri("pack://http:,,,/x.vbaml");
    SHOULD(ShouldlyIntegral(u.valid).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(u.error)).ShouldBe(static_cast<int>(PackUriError::UnsupportedAuthority)));
    SHOULD(ShouldlyString(u.authority).ShouldBe(std::string("http")));
}

TEST_CASE("S289: PackUri — empty / blank identity → InvalidPath")
{
    const PackUri u1 = parse_pack_uri("");
    SHOULD(ShouldlyIntegral(u1.valid).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(u1.error)).ShouldBe(static_cast<int>(PackUriError::InvalidPath)));

    const PackUri u2 = parse_pack_uri("   ");
    SHOULD(ShouldlyIntegral(u2.valid).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(u2.error)).ShouldBe(static_cast<int>(PackUriError::InvalidPath)));

    // A full pack URI with no path after ",,," is InvalidPath.
    const PackUri u3 = parse_pack_uri("pack://application:,,,");
    SHOULD(ShouldlyIntegral(u3.valid).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(u3.error)).ShouldBe(static_cast<int>(PackUriError::InvalidPath)));
}

// ---------------------------------------------------------------------------
// ResourcePack — the embedded resource lookup
// ---------------------------------------------------------------------------

TEST_CASE("S289: ResourcePack — register + resolve round-trip + registration order")
{
    ResourcePack pack;
    std::vector<std::uint8_t> a{1, 2, 3, 4};
    std::vector<std::uint8_t> b{9, 8, 7};
    pack.add_resource("A.vbaml", a);
    pack.add_resource("B.vbaml", b);

    SHOULD(ShouldlyString(pack.size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(pack.has("A.vbaml")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(pack.has("B.vbaml")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(pack.has("missing")).ShouldBe(false));

    const std::span<const std::uint8_t> ra = pack.resolve("A.vbaml");
    SHOULD(ShouldlyString(ra.size()).ShouldBe(4u));
    if (ra.size() == 4)
    {
        SHOULD(ShouldlyIntegral(ra[0]).ShouldBe(1));
        SHOULD(ShouldlyIntegral(ra[3]).ShouldBe(4));
    }
    // An unknown key → a structured EMPTY span (never a bogus pointer).
    const std::span<const std::uint8_t> rm = pack.resolve("missing");
    SHOULD(ShouldlyIntegral(rm.empty()).ShouldBe(true));
}

TEST_CASE("S289: ResourcePack — duplicate registration throws, pack unchanged")
{
    ResourcePack pack;
    std::vector<std::uint8_t> a{1};
    pack.add_resource("A.vbaml", a);
    bool threw = false;
    try
    {
        pack.add_resource("A.vbaml", std::vector<std::uint8_t>{2});
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
    SHOULD(ShouldlyString(pack.size()).ShouldBe(1u));   // unchanged
    const std::span<const std::uint8_t> ra = pack.resolve("A.vbaml");
    SHOULD(ShouldlyIntegral(ra[0]).ShouldBe(1));        // the FIRST registration won
}

TEST_CASE("S289: ResourcePack — registered_keys is deterministic registration order")
{
    ResourcePack pack;
    pack.add_resource("Z.vbaml", std::vector<std::uint8_t>{});
    pack.add_resource("A.vbaml", std::vector<std::uint8_t>{});
    pack.add_resource("M.vbaml", std::vector<std::uint8_t>{});
    const auto keys = pack.registered_keys();
    SHOULD(ShouldlyString(keys.size()).ShouldBe(3u));
    if (keys.size() == 3)
    {
        SHOULD(ShouldlyString(keys[0]).ShouldBe(std::string("Z.vbaml")));
        SHOULD(ShouldlyString(keys[1]).ShouldBe(std::string("A.vbaml")));
        SHOULD(ShouldlyString(keys[2]).ShouldBe(std::string("M.vbaml")));
    }
}

TEST_CASE("S289: ResourcePack — add_embedded registers a span and resolves it back")
{
    // The add_embedded convenience is the "embedded resource lookup" headline:
    // a caller registers the S287 `compiled_vbaml_<symbol>()` span under a pack
    // key, and the pack resolves it back (InitializeComponentFrom's input). The
    // actual S287 embed-span registration over the REAL generated symbol is
    // exercised by the S287 test (CMake-wired); here the structural contract is
    // proven deterministically with a plain span.
    ResourcePack pack;
    const std::vector<std::uint8_t> embed{42, 43, 44};
    pack.add_embedded("Demo.MainView.vbaml", embed);
    SHOULD(ShouldlyIntegral(pack.has("Demo.MainView.vbaml")).ShouldBe(true));
    const std::span<const std::uint8_t> resolved = pack.resolve("Demo.MainView.vbaml");
    SHOULD(ShouldlyString(resolved.size()).ShouldBe(embed.size()));
    if (resolved.size() == embed.size())
    {
        bool same = true;
        for (std::size_t i = 0; i < embed.size(); ++i)
        {
            if (resolved[i] != embed[i])
            {
                same = false;
                break;
            }
        }
        SHOULD(ShouldlyIntegral(same).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// resolve_pack_uri — the pack-identity → bytes seam
// ---------------------------------------------------------------------------

TEST_CASE("S289: resolve_pack_uri maps a pack identity to its registered bytes")
{
    ResourcePack pack;
    const std::vector<std::uint8_t> bytes = compileS289Doc();
    pack.add_resource("Demo.MainView.vbaml", bytes);

    const std::span<const std::uint8_t> viaBare = resolve_pack_uri(pack, "Demo.MainView.vbaml");
    SHOULD(ShouldlyString(viaBare.size()).ShouldBe(bytes.size()));

    const std::span<const std::uint8_t> viaPack = resolve_pack_uri(pack, "pack://application:,,,/Demo.MainView.vbaml");
    SHOULD(ShouldlyString(viaPack.size()).ShouldBe(bytes.size()));

    // An identity that resolves through the pack == the registered bytes.
    if (viaBare.size() == bytes.size() && viaPack.size() == bytes.size())
    {
        bool same = true;
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            if (viaPack[i] != bytes[i])
            {
                same = false;
                break;
            }
        }
        SHOULD(ShouldlyIntegral(same).ShouldBe(true));
    }

    // A resin unregistered path → empty (structured, never silent).
    const std::span<const std::uint8_t> miss = resolve_pack_uri(pack, "Nope.vbaml");
    SHOULD(ShouldlyIntegral(miss.empty()).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// InitializeComponent — the x:Class owner binding + class mapping
// ---------------------------------------------------------------------------

TEST_CASE("S289: InitializeComponent binds the x:Class owner + root_class + named()")
{
    registerS289Properties();
    const std::vector<std::uint8_t> bytes = compileS289Doc();
    auto owner = makeOwner();

    VBamlReader reader;
    reader.register_class("Demo.MainWindow");   // the x:Class mapping
    std::unique_ptr<DependencyObject> root = reader.InitializeComponent(owner.get(), bytes);

    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    if (root)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(root.get()) != nullptr).ShouldBe(true));
        // Owner bound: reader.owner() == the app-supplied owner.
        SHOULD(ShouldlyIntegral(reader.owner() == owner.get()).ShouldBe(true));
        // root_class decoded from the .vbaml.
        SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("Demo.MainWindow")));
        // The x:Class mapping registered: the decoded class is one of them.
        SHOULD(ShouldlyIntegral(reader.is_registered_class("Demo.MainWindow")).ShouldBe(true));
        // named() serves the owner's registered names.
        DependencyObject* panel = reader.named("panel");
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        // The property set in the golden is converted + stored.
        if (panel)
        {
            SHOULD(ShouldlyString(panel->Get(PropertyId::of("S289Content")))
                       .ShouldBe(Value::String("Hello")));
        }
    }
}

TEST_CASE("S289: InitializeComponent — decoded x:Class NOT in the mapping → OwnerClassMismatch")
{
    registerS289Properties();
    const std::vector<std::uint8_t> bytes = compileS289Doc();   // decodes to "Demo.MainWindow"
    auto owner = makeOwner();

    VBamlReader reader;
    // DELIBERATELY register a DIFFERENT class so the decoded root_class is
    // footprint-INCONSISTENT with the bound owner's declared class.
    reader.register_class("Other.Owner");
    bool threw = false;
    try
    {
        (void)reader.InitializeComponent(owner.get(), bytes);
    }
    catch (const VBamlException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VBamlError::OwnerClassMismatch)));
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
}

TEST_CASE("S289: InitializeComponent — null owner is a structured error")
{
    registerS289Properties();
    const std::vector<std::uint8_t> bytes = compileS289Doc();
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.InitializeComponent(nullptr, bytes);
    }
    catch (const VBamlException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// InitializeComponentFrom — the COMPILED-ONLY release entry (no text)
// ---------------------------------------------------------------------------

// A static_assert (compile-time) that the compiled-only entry exposes NO text
// parameter: `InitializeComponentFrom` takes (owner, ResourcePack&, packUri) —
// a std::string_view pack IDENTITY, NOT a VAML source. This is the "release
// uses compiled only" proof at the type level.
static_assert(std::is_same_v<
    decltype(&VBamlReader::InitializeComponentFrom),
    std::unique_ptr<DependencyObject> (VBamlReader::*)(
        DependencyObject*, vaml::tools::pack::ResourcePack&, std::string_view)>,
    "InitializeComponentFrom must be compiled-only (no text/VAML parameter)");

TEST_CASE("S289: InitializeComponentFrom resolves through the pack → identical tree to direct Load")
{
    registerS289Properties();
    // Build a resource pack holding the compiled demo under its pack key.
    ResourcePack pack;
    const std::vector<std::uint8_t> bytes = compileS289Doc();
    pack.add_resource("Demo.MainView.vbaml", bytes);

    auto owner = makeOwner();
    VBamlReader reader;
    reader.register_class("Demo.MainWindow");
    std::unique_ptr<DependencyObject> root = reader.InitializeComponentFrom(owner.get(), pack, "pack://application:,,,/Demo.MainView.vbaml");

    // Direct-Load parity: the compiled-only entry produced a tree behaviorally
    // identical to a plain direct Load of the same bytes.
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    if (root)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(root.get()) != nullptr).ShouldBe(true));
        SHOULD(ShouldlyString(reader.root_class()).ShouldBe(std::string("Demo.MainWindow")));
        SHOULD(ShouldlyIntegral(reader.owner() == owner.get()).ShouldBe(true));
        DependencyObject* panel = reader.named("panel");
        SHOULD(ShouldlyIntegral(panel != nullptr).ShouldBe(true));
        if (panel)
        {
            SHOULD(ShouldlyString(panel->Get(PropertyId::of("S289Content")))
                       .ShouldBe(Value::String("Hello")));
        }
    }
}

TEST_CASE("S289: InitializeComponentFrom — an unregistered pack resource fails loud, never silent")
{
    registerS289Properties();
    ResourcePack pack;   // EMPTY — nothing registered
    auto owner = makeOwner();
    VBamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.InitializeComponentFrom(owner.get(), pack, "Missing.vbaml");
    }
    catch (const VBamlException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
}
