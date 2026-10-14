// S086 (dep:S085) — Parser round-trips a realistic VAML document into the tree.
//
// The FORMAL round-trip gate over S085's tree shape: prove parse_vaml turns a
// realistic, multi-feature VAML document into an accurate ParsedElement tree,
// and that the tree reproduces the authored source faithfully.
//
// TEST-ONLY step: zero new product symbols. The parser pipe already exists via
// S080-S085 and is unit-tested; S086 adds the full-document golden gate.
//
// Five groups, all against ONE realistic document (plus focused variants) that
// exercises every S080-S085 feature at once — namespaces, x: directives,
// attached-style dotted names, markup extensions {Binding}, numeric attrs,
// self-closing, text-content leaves, comments:
//
//   * Group 1 — GOLDEN STRUCTURAL ROUND-TRIP: a canonicalize() re-serializer
//     (attributes in order, self-closing preserved, children then content,
//     xml namespaces correctly absent since they are declarations) proves
//     canonical(parse(source)) == the hand-written expected string.
//   * Group 2 — FULL TREE-SHAPE assertions: per-node tagName/ns/children/
//     content/self_closing; text leaves carry `content`, containers carry none.
//   * Group 3 — FEATURE-CLASSIFICATION round-trip: MarkupExtension tagging,
//     numeric detection, x: directive + namespace metadata, quoted non-numeric
//     comma-list.
//   * Group 4 — Comments dropped + as-authored content preservation.
//   * Group 5 — Error path: the realistic doc truncated (missing close) throws
//     XmlParseException::UnterminatedElement; the full doc parses without throw.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "doctest/doctest.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ParsedElement.h"
#include "vaml/core/XmlScanner.h"

#include <string>
#include <vector>

using vaml::AttributeValueKind;
using vaml::ParsedAttribute;
using vaml::ParsedElement;
using vaml::parse_vaml;
using vaml::XmlDirectiveType;
using vaml::XmlParseError;
using vaml::XmlParseException;
using vaml::kXamlLanguageNamespace;

namespace {

// The ONE realistic, multi-feature document S086 round-trips. Spans namespaces
// (xmlns:x), x: directives (x:Class/x:Name), attached-style dotted names
// (DockPanel.Dock / Grid.Row / Grid.Column), markup extensions ({Binding ...}),
// numeric attrs (Width/Height/Grid.Row/Grid.Column/FontSize), quoted literals
// (Title/Content), self-closing tags (TextBox/Button/Texts), and a text-content
// leaf (<Text x:Name="SearchLabel">Search:</Text>). Elements are flush (no
// inter-element whitespace) so the golden string is exact and non-ambiguous.
const char* kRealisticDocument = R"(<Window xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Class="App.MainView" Width="1024" Height="768" Title="Contact Manager"><DockPanel Margin="10,10,10,10"><StackPanel DockPanel.Dock="Top" Orientation="Horizontal" Height="36"><Text x:Name="SearchLabel">Search:</Text><TextBox Width="200" Text="{Binding SearchText}"/><Button Content="Search" Command="{Binding SearchCommand}" Margin="4,0,0,0"/></StackPanel><StackPanel DockPanel.Dock="Left" Width="220"><Text FontSize="18" FontWeight="Bold" Text="{Binding SelectedContact.Name}"/><Text Text="{Binding SelectedContact.Email}"/></StackPanel><Grid Grid.Row="1" Grid.Column="0"><Text Visibility="Collapsed"/></Grid></DockPanel></Window>)";

// The expected output of canonicalize() on parse(kRealisticDocument).
// Reproduces the authored source EXACTLY, modulo the xmlns:x declaration
// (xmlns/xmlns:p are namespace DECLARATIONS, never ordinary attributes — S082 —
// so a faithful element re-serializer omits them).
const char* kExpectedCanonical = R"(<Window x:Class="App.MainView" Width="1024" Height="768" Title="Contact Manager"><DockPanel Margin="10,10,10,10"><StackPanel DockPanel.Dock="Top" Orientation="Horizontal" Height="36"><Text x:Name="SearchLabel">Search:</Text><TextBox Width="200" Text="{Binding SearchText}"/><Button Content="Search" Command="{Binding SearchCommand}" Margin="4,0,0,0"/></StackPanel><StackPanel DockPanel.Dock="Left" Width="220"><Text FontSize="18" FontWeight="Bold" Text="{Binding SelectedContact.Name}"/><Text Text="{Binding SelectedContact.Email}"/></StackPanel><Grid Grid.Row="1" Grid.Column="0"><Text Visibility="Collapsed"/></Grid></DockPanel></Window>)";

// Test-local re-serializer: emits structure faithfully, per the S086 round-trip
// contract — attributes in document order, self-closing preserved, children
// then content, and NO xml namespaces (they are declarations). NOT product
// code; the real tree re-serializer/serializer is S282/S283's step.
std::string canonicalize(const ParsedElement& el)
{
    std::string out = "<" + el.tagName;
    for (const ParsedAttribute& a : el.attributes)
    {
        out += " " + a.key + "=\"" + a.value + "\"";
    }
    if (el.self_closing)
    {
        out += "/>";
        return out;
    }
    out += ">";
    for (const ParsedElement& c : el.children)
    {
        out += canonicalize(c);
    }
    out += el.content;
    out += "</" + el.tagName + ">";
    return out;
}

// Convenience: find the attribute with `key` inside `el`, or null when absent.
const ParsedAttribute* findAttr(const ParsedElement& el, const std::string& key)
{
    for (const ParsedAttribute& a : el.attributes)
    {
        if (a.key == key)
        {
            return &a;
        }
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Group 1 — Golden structural round-trip
// ---------------------------------------------------------------------------

TEST_CASE("S086: realistic document round-trips to the authored structure (golden)")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    // text -> tree -> structure reproduces the authored document exactly
    // (modulo the xmlns declaration, which is not an element attribute).
    SHOULD(ShouldlyString(canonicalize(root)).ShouldBe(std::string(kExpectedCanonical)));
}

// ---------------------------------------------------------------------------
// Group 2 — Full tree-shape assertions
// ---------------------------------------------------------------------------

TEST_CASE("S086: root + DockPanel carry the full shape (tagName/ns/children/attrs)")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    // Root: unprefixed Window, empty ns.
    SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("Window")));
    SHOULD(ShouldlyString(root.ns).ShouldBe(std::string("")));
    // xmlns:x is a declaration, NOT an attribute (S082), so 4 real attrs.
    SHOULD(ShouldlyIntegral(root.attributes.size()).ShouldBe(static_cast<std::size_t>(4)));
    SHOULD(ShouldlyIntegral(findAttr(root, "xmlns:x") != nullptr).ShouldBe(false));

    // Container root: no own content, one child (DockPanel).
    SHOULD(ShouldlyIntegral(root.content.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    SHOULD(ShouldlyIntegral(static_cast<int>(root.children[0].self_closing)).ShouldBe(0));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("DockPanel")));

    const ParsedElement& dock = root.children[0];
    SHOULD(ShouldlyIntegral(dock.content.empty()).ShouldBe(true));
    // DockPanel children: SP1, SP2, Grid — document order.
    SHOULD(ShouldlyIntegral(dock.children.size()).ShouldBe(static_cast<std::size_t>(3)));
    SHOULD(ShouldlyString(dock.children[0].tagName).ShouldBe(std::string("StackPanel")));
    SHOULD(ShouldlyString(dock.children[1].tagName).ShouldBe(std::string("StackPanel")));
    SHOULD(ShouldlyString(dock.children[2].tagName).ShouldBe(std::string("Grid")));
}

TEST_CASE("S086: StackPanel#1 — leaf content + self-closing siblings")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);
    const ParsedElement& sp1 = root.children[0].children[0];

    // SP1 is a container: no own content, 3 children.
    SHOULD(ShouldlyIntegral(sp1.content.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sp1.children.size()).ShouldBe(static_cast<std::size_t>(3)));

    // <Text x:Name="SearchLabel">Search:</Text> — NON-self-closing text leaf.
    const ParsedElement& text = sp1.children[0];
    SHOULD(ShouldlyString(text.tagName).ShouldBe(std::string("Text")));
    SHOULD(ShouldlyIntegral(static_cast<int>(text.self_closing)).ShouldBe(0));
    SHOULD(ShouldlyString(text.content).ShouldBe(std::string("Search:")));
    SHOULD(ShouldlyIntegral(text.children.size()).ShouldBe(static_cast<std::size_t>(0)));

    // TextBox is self-closing: no content, no children.
    const ParsedElement& textBox = sp1.children[1];
    SHOULD(ShouldlyString(textBox.tagName).ShouldBe(std::string("TextBox")));
    SHOULD(ShouldlyIntegral(static_cast<int>(textBox.self_closing)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(textBox.content.empty()).ShouldBe(true));

    // Button is self-closing.
    const ParsedElement& button = sp1.children[2];
    SHOULD(ShouldlyString(button.tagName).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyIntegral(static_cast<int>(button.self_closing)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(button.content.empty()).ShouldBe(true));
}

TEST_CASE("S086: StackPanel#2 + Grid — attached-style dotted names kept raw")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);
    const ParsedElement& sp2 = root.children[0].children[1];
    const ParsedElement& grid = root.children[0].children[2];

    // SP2: two self-closing Text children.
    SHOULD(ShouldlyIntegral(sp2.children.size()).ShouldBe(static_cast<std::size_t>(2)));
    SHOULD(ShouldlyIntegral(static_cast<int>(sp2.children[0].self_closing)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(sp2.children[1].self_closing)).ShouldBe(1));

    // Grid: one child; Grid.Row/Grid.Column kept as FULL raw keys ('.' is NOT a
    // namespace separator — S082), so no prefix/local split on the dot.
    SHOULD(ShouldlyIntegral(grid.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    const ParsedAttribute* row = findAttr(grid, "Grid.Row");
    SHOULD(ShouldlyIntegral(row != nullptr).ShouldBe(true));
    if (row)
    {
        SHOULD(ShouldlyString(row->key).ShouldBe(std::string("Grid.Row")));
        SHOULD(ShouldlyString(row->local_name).ShouldBe(std::string("Grid.Row")));
        SHOULD(ShouldlyString(row->prefix).ShouldBe(std::string("")));
    }
    SHOULD(ShouldlyIntegral(findAttr(grid, "Grid.Column") != nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// Group 3 — Feature-classification round-trip
// ---------------------------------------------------------------------------

TEST_CASE("S086: markup-extension value is tagged MarkupExtension + not numeric")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    const ParsedAttribute* bind = findAttr(root.children[0].children[0].children[1], "Text");
    SHOULD(ShouldlyIntegral(bind != nullptr).ShouldBe(true));
    if (bind)
    {
        SHOULD(ShouldlyString(bind->value).ShouldBe(std::string("{Binding SearchText}")));
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->isExtension)).ShouldBe(1));
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::MarkupExtension)));
        // An extension is never numeric at S084.
        SHOULD(ShouldlyIntegral(static_cast<int>(bind->numeric)).ShouldBe(0));
    }
}

TEST_CASE("S086: numeric attribute values are detected")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    // Window Width=1024 / Height=768; SP1 Height=36 — all scalar integers.
    const ParsedAttribute* width = findAttr(root, "Width");
    SHOULD(ShouldlyIntegral(width != nullptr).ShouldBe(true));
    if (width)
    {
        SHOULD(ShouldlyString(width->value).ShouldBe(std::string("1024")));
        SHOULD(ShouldlyIntegral(static_cast<int>(width->numeric)).ShouldBe(1));
        SHOULD(ShouldlyIntegral(static_cast<int>(width->isExtension)).ShouldBe(0));
    }
    const ParsedAttribute* spHeight = findAttr(root.children[0].children[0], "Height");
    SHOULD(ShouldlyIntegral(spHeight != nullptr).ShouldBe(true));
    if (spHeight)
    {
        SHOULD(ShouldlyString(spHeight->value).ShouldBe(std::string("36")));
        SHOULD(ShouldlyIntegral(static_cast<int>(spHeight->numeric)).ShouldBe(1));
    }
}

TEST_CASE("S086: x: directive + namespace metadata carried (Class + Name)")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    // x:Class on the root -> directive Class, resolved to the xaml URI.
    const ParsedAttribute* cls = findAttr(root, "x:Class");
    SHOULD(ShouldlyIntegral(cls != nullptr).ShouldBe(true));
    if (cls)
    {
        SHOULD(ShouldlyString(cls->key).ShouldBe(std::string("x:Class")));
        SHOULD(ShouldlyString(cls->local_name).ShouldBe(std::string("Class")));
        SHOULD(ShouldlyString(cls->prefix).ShouldBe(std::string("x")));
        SHOULD(ShouldlyString(cls->namespace_uri).ShouldBe(std::string(kXamlLanguageNamespace)));
        SHOULD(ShouldlyIntegral(static_cast<int>(cls->directive))
                   .ShouldBe(static_cast<int>(XmlDirectiveType::Class)));
        SHOULD(ShouldlyIntegral(static_cast<int>(cls->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    }

    // x:Name on the leaf Text -> directive Name.
    const ParsedAttribute* name = findAttr(root.children[0].children[0].children[0], "x:Name");
    SHOULD(ShouldlyIntegral(name != nullptr).ShouldBe(true));
    if (name)
    {
        SHOULD(ShouldlyString(name->local_name).ShouldBe(std::string("Name")));
        SHOULD(ShouldlyString(name->namespace_uri).ShouldBe(std::string(kXamlLanguageNamespace)));
        SHOULD(ShouldlyIntegral(static_cast<int>(name->directive))
                   .ShouldBe(static_cast<int>(XmlDirectiveType::Name)));
    }
}

TEST_CASE("S086: comma-list thickness form is quoted, NOT numeric")
{
    const ParsedElement root = parse_vaml(kRealisticDocument);

    // Margin="10,10,10,10" is a comma-list (thickness -> S089 converter domain);
    // S084 detects only scalar numerics, so it stays quoted + non-numeric.
    const ParsedAttribute* margin = findAttr(root.children[0], "Margin");
    SHOULD(ShouldlyIntegral(margin != nullptr).ShouldBe(true));
    if (margin)
    {
        SHOULD(ShouldlyString(margin->value).ShouldBe(std::string("10,10,10,10")));
        SHOULD(ShouldlyIntegral(static_cast<int>(margin->numeric)).ShouldBe(0));
        SHOULD(ShouldlyIntegral(static_cast<int>(margin->isExtension)).ShouldBe(0));
        SHOULD(ShouldlyIntegral(static_cast<int>(margin->value_kind))
                   .ShouldBe(static_cast<int>(AttributeValueKind::QuotedLiteral)));
    }
}

// ---------------------------------------------------------------------------
// Group 4 — Comments dropped + as-authored content
// ---------------------------------------------------------------------------

TEST_CASE("S086: a comment in a realistic doc adds nothing to the tree")
{
    // Same structure as the realistic doc but with comments inserted; comments
    // are DROPPED (not tree nodes) and contribute no content.
    const ParsedElement root = parse_vaml(
        "<Window xmlns:x=\"" + std::string(kXamlLanguageNamespace) + "\" x:Class=\"App.MainView\">"
        "<!-- root comment --><DockPanel><!-- inside --><Text x:Name=\"A\">Hi</Text></DockPanel>"
        "</Window>");

    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    const ParsedElement& dock = root.children[0];
    SHOULD(ShouldlyString(dock.tagName).ShouldBe(std::string("DockPanel")));
    SHOULD(ShouldlyIntegral(dock.children.size()).ShouldBe(static_cast<std::size_t>(1)));
    // The leaf text 'Hi' survives; the comments add no content anywhere.
    SHOULD(ShouldlyString(dock.children[0].content).ShouldBe(std::string("Hi")));
    SHOULD(ShouldlyIntegral(dock.content.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(root.content.empty()).ShouldBe(true));
}

TEST_CASE("S086: pretty-printed inter-element whitespace is kept as-authored in content")
{
    // A pretty-printed snippet: the inter-element newlines/indent are element
    // TEXT (kept as authored by S085), not dropped.
    const ParsedElement root = parse_vaml(
        "<StackPanel>\n"
        "  <Button/>\n"
        "  <Text>A</Text>\n"
        "</StackPanel>");

    // Two child elements.
    SHOULD(ShouldlyIntegral(root.children.size()).ShouldBe(static_cast<std::size_t>(2)));
    SHOULD(ShouldlyString(root.children[0].tagName).ShouldBe(std::string("Button")));
    SHOULD(ShouldlyString(root.children[1].tagName).ShouldBe(std::string("Text")));
    SHOULD(ShouldlyString(root.children[1].content).ShouldBe(std::string("A")));

    // Inter-element whitespace accumulates into the PARENT's content verbatim:
    // "\n  " (after open) + "\n  " (between children) + "\n" (before close).
    SHOULD(ShouldlyString(root.content).ShouldBe(std::string("\n  \n  \n")));
}

// ---------------------------------------------------------------------------
// Group 5 — Error path
// ---------------------------------------------------------------------------

TEST_CASE("S086: the full realistic document parses with no throw")
{
    bool threw = false;
    try
    {
        const ParsedElement root = parse_vaml(kRealisticDocument);
        // Sanity: the root survived the parse (and the canonical golden still
        // matches — the whole tree was built).
        SHOULD(ShouldlyString(root.tagName).ShouldBe(std::string("Window")));
        SHOULD(ShouldlyString(canonicalize(root)).ShouldBe(std::string(kExpectedCanonical)));
    }
    catch (const std::exception&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(0));
}

TEST_CASE("S086: truncated realistic document -> UnterminatedElement")
{
    // Take the realistic doc and cut before the final </Window>: the root
    // Window element is left open at EOF -> the parser's OWN structural path.
    const std::string truncated = std::string(kRealisticDocument).substr(
        0, std::string(kRealisticDocument).size() - std::string("</Window>").size());

    bool threw = false;
    try
    {
        (void)parse_vaml(truncated);
    }
    catch (const XmlParseException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(XmlParseError::UnterminatedElement)));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBeGreaterThan(0));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBeGreaterThan(0));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
