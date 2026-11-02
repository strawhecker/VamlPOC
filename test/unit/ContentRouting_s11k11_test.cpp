// 11k-11 (dep:S085/11k-1/S440) — GENERIC CONTENT-OWNER ROUTING + the S649
// control-material RE-BIND (the content-routing issue settled IN-11K).
//
// (A) THE GENERAL RULE — 11k-1's TextBlock-scoped proof
//     (`<TextBlock>label</TextBlock>` → Text DP) is GENERALIZED at 11k-11: an
//     element registers a SINGLE content-owning DP; the reader routes its
//     INLINE AUTHORED TEXT through it IN DOCUMENT ORDER. TextBlock→Text DP
//     (11k-1 regression, pinned unchanged); Label→Content DP (the
//     ContentControl-family String content). Any OTHER content-owning container
//     (Window/UserControl/Border/TabItem/…) routes its single OBJECT child via
//     the S440 finalize set_content slot — NOT a text DP (the S649 control-
//     material re-bind). Value/resource elements consume inline content as
//     their parsed value (11k-3) — not here.
//
// (B) MIXED-CONTENT ORDERING (the S085 conscience note: "a single content
//     string plus an ordered children vector does not capture INTERLEAVED
//     text/child ORDERING" — resolved IN-11K, an explicit decision): parse_vaml
//     now captures `contentRuns` — the FULL document-order Text|Child stream.
//     The reader routes each TEXT run through the content-owning DP in that
//     order; child runs become the visual children (document order preserved).
//     This test asserts the ORDER is lossless + that the Text DP received the
//     ordered concatenation.
//
// (C) S649 RE-BIND — the phantom orphaned `S649` "control-material
//     continuation" forward-carry (recorded at 11j-10 + 11k-10 NO-GAP) is
//     RESOLVED at 11k-11: the ContentControl-base/control-material surface is
//     re-bound HERE. Records: roadmap + progress.md mark S649 fully resolved by
//     11k-11 (history preserved un-edited, never a silent deferral).
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/VamlReader.h>
#include <vaml/core/Element.h>
#include <vaml/core/ParsedElement.h>
#include <vaml/core/UIElement.h>

using vaml::Label;
using vaml::TextBlock;
using vaml::VamlReader;
using vaml::parse_vaml;
using vaml::register_builtin_elements;

// ---------------------------------------------------------------------------
// (A) THE GENERAL CONTENT-OWNER RULE
// ---------------------------------------------------------------------------
TEST_CASE("11k-11: <TextBlock>label</TextBlock> still routes to the Text DP (11k-1 regression)")
{
    register_builtin_elements();
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<TextBlock FontSize=\"24\">label</TextBlock>");
    REQUIRE(root != nullptr);
    vaml::TextBlock* tb = dynamic_cast<TextBlock*>(root.get());
    REQUIRE(tb != nullptr);
    SHOULD(ShouldlyString(tb->text()).ShouldBe(std::string("label")));
}

TEST_CASE("11k-11: <Label>text</Label> routes to the Content DP (the ContentControl-family String content)")
{
    register_builtin_elements();
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<Label>hello</Label>");
    REQUIRE(root != nullptr);
    vaml::Label* lb = dynamic_cast<Label*>(root.get());
    REQUIRE(lb != nullptr);
    SHOULD(ShouldlyString(lb->content()).ShouldBe(std::string("hello")));
}

TEST_CASE("11k-11: a content container's single OBJECT child routes via the S440 set_content slot, not a text DP")
{
    register_builtin_elements();
    VamlReader reader;
    // A Window hosts ONE content child; the object child (a TextBlock) becomes
    // its content via the S440 finalize set_content re-bind — NOT a "Content"
    // text DP (a UIElement object cannot be a String Content DP).
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<Window Width=\"300\" Height=\"200\"><TextBlock Text=\"hi\"/></Window>");
    REQUIRE(root != nullptr);
    vaml::Window* w = dynamic_cast<vaml::Window*>(root.get());
    REQUIRE(w != nullptr);
    // The TextBlock child is routed to the Window's single-content slot.
    (void)reader.finalize_visual_tree();
    SHOULD(ShouldlyIntegral(w->content() != nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (B) MIXED-CONTENT DOCUMENT ORDER (the S085 note, resolved IN-11K)
// ---------------------------------------------------------------------------
TEST_CASE("11k-11: parse_vaml captures the mixed text+child INTERLEAVED ORDER (S085 note resolved)")
{
    // `<p>Hello <b>world</b> again</p>` — the exact shape the S085 conscience
    // note flagged. contentRuns must capture: Text("Hello ") → Child(0) → Text(" again").
    const std::string src = "<p>Hello <b>world</b> again</p>";
    vaml::ParsedElement root = parse_vaml(src);
    REQUIRE(root.contentRuns.size() == 3);
    // Run 1: a text chunk "Hello ".
    SHOULD(ShouldlyIntegral(root.contentRuns[0].is_text).ShouldBe(true));
    SHOULD(ShouldlyString(root.contentRuns[0].text).ShouldBe(std::string("Hello ")));
    // Run 2: a child run -> children[0] (the <b>).
    SHOULD(ShouldlyIntegral(root.contentRuns[1].is_text).ShouldBe(false));
    SHOULD(ShouldlyIntegral(root.contentRuns[1].child_index == 0).ShouldBe(true));
    // Run 3: a text chunk " again".
    SHOULD(ShouldlyIntegral(root.contentRuns[2].is_text).ShouldBe(true));
    SHOULD(ShouldlyString(root.contentRuns[2].text).ShouldBe(std::string(" again")));
    // The children vector holds exactly the one <b>.
    SHOULD(ShouldlyIntegral(root.children.size() == 1u).ShouldBe(true));
    // `content` still carries the FULL TEXT the PARENT owns — the <b> element's
    // "world" is a CHILD, not the parent's own text, so the parent content is
    // "Hello " + " again" = "Hello  again" (kept as authored; interior double
    // space preserved). The "world" is nested in the child's own content.
    SHOULD(ShouldlyString(root.content).ShouldBe(std::string("Hello  again")));
    // Sanity: the child (parsed from "<b>world</b>") carries its own "world" content.
    SHOULD(ShouldlyIntegral(root.children[0].content.find("world") != std::string::npos).ShouldBe(true));
}

TEST_CASE("11k-11: the reader routes TEXT runs through the content DP in document order")
{
    register_builtin_elements();
    VamlReader reader;
    // A TextBlock with interleaved text + a child Run: the reader concatenates
    // the TEXT runs IN ORDER ("Hello " + " again") onto the Text DP, while the
    // child run enters the visual tree.
    // (No <Run> built-in exists; use a nested <TextBlock> as the interleaved child.)
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<TextBlock>Hello <TextBlock Text=\"world\"/> again</TextBlock>");
    REQUIRE(root != nullptr);
    vaml::TextBlock* tb = dynamic_cast<TextBlock*>(root.get());
    REQUIRE(tb != nullptr);
    // Document-order text concatenation, trimmed leading/trailing only:
    // "Hello " + " again" -> "Hello  again" (the interior double-space is
    // preserved — concatenation does NOT collapse interior whitespace).
    SHOULD(ShouldlyString(tb->text()).ShouldBe(std::string("Hello  again")));
    // The interleaved child was parsed (the ORDER captured by contentRuns;
    // the child itself enters the visual tree, not the Text DP).
    SHOULD(ShouldlyIntegral(tb->text().find("Hello") == 0).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// (C) S649 RE-BIND is a RECORD (roadmap/progress) — asserted here as the surface
//     contract: a ContentControl-family container routes its object child via
//     the set_content slot (S440) — the S649 re-bind resolved in-11k.
// ---------------------------------------------------------------------------
TEST_CASE("11k-11: UserControl content routes via the S440 set_content slot (S649 re-bind)")
{
    register_builtin_elements();
    VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root =
        reader.Load("<UserControl Width=\"200\" Height=\"100\"><Grid/></UserControl>");
    REQUIRE(root != nullptr);
    vaml::UserControl* uc = dynamic_cast<vaml::UserControl*>(root.get());
    REQUIRE(uc != nullptr);
    (void)reader.finalize_visual_tree();
    SHOULD(ShouldlyIntegral(uc->content() != nullptr).ShouldBe(true));
}
