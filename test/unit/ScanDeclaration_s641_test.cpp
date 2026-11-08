// S641 — ScanDeclaration_s641_test.cpp (in-XAML SCAN DECLARATION parser).
//
// Sprint 11r (S641): the in-XAML `SCAN DECLARATION` comment-block parser — the
// PRIMARY oracle the compliance scanner reads. This suite proves the grammar
// (frame/quad solid|text|icon + tolerance/band/expect-literal/centroid/presence)
// + the typed never-silent error taxonomy, on BOTH CI lanes (headless, no Vulkan).
//
// ZERO MINIMAL REAL — permanent ADR-021 surface.

#include <vaml/presentation/scan/ScanDeclaration.h>

#include "doctest/doctest.h"

#include <string>

using namespace vaml::presentation::scan;

TEST_CASE("S641 parse_scan_declaration parses a full per-quad declaration")
{
    const std::string xaml = R"xaml(<!-- OTHER -->
<StackPanel Width="440" Height="360">
  <!-- SCAN DECLARATION
    frame 440 360
    quad  img   solid  rect 32,32,128,128   tolerance 0.10
    quad  title text   auto                band 16  expect-literal "Hello"
    quad  ok    icon   auto                centroid tolerance 0.25
  END SCAN DECLARATION -->
</StackPanel>)xaml";

    const auto r = parse_scan_declaration(xaml);
    REQUIRE(r.ok);
    CHECK(r.declaration.width == 440u);
    CHECK(r.declaration.height == 360u);
    CHECK(r.declaration.quads.size() == 3u);

    const ScanQuad& img = r.declaration.quads[0];
    CHECK(img.name == "img");
    CHECK(img.kind == QuadKind::Solid);
    CHECK(img.located == true);
    CHECK(img.x == 32.0f);
    CHECK(img.y == 32.0f);
    CHECK(img.w == 128.0f);
    CHECK(img.h == 128.0f);
    CHECK(img.has_tolerance == true);
    CHECK(img.tolerance == doctest::Approx(0.10));

    const ScanQuad& title = r.declaration.quads[1];
    CHECK(title.name == "title");
    CHECK(title.kind == QuadKind::Text);
    CHECK(title.located == false);       // auto-locate
    CHECK(title.band == 16);
    CHECK(title.expect_literal == "Hello");

    const ScanQuad& ok = r.declaration.quads[2];
    CHECK(ok.name == "ok");
    CHECK(ok.kind == QuadKind::Icon);
    CHECK(ok.located == false);
    CHECK(ok.require_centroid == true);
    CHECK(ok.require_presence == false);
    CHECK(ok.has_tolerance == true);
    CHECK(ok.tolerance == doctest::Approx(0.25));
}

TEST_CASE("S641 text/icon default handling + presence")
{
    const std::string xaml = R"xaml(<!-- SCAN DECLARATION
  frame 100 80
  quad a text auto
  quad b icon auto presence
END SCAN DECLARATION -->)xaml";
    const auto r = parse_scan_declaration(xaml);
    REQUIRE(r.ok);
    REQUIRE(r.declaration.quads.size() == 2u);
    CHECK(r.declaration.quads[0].kind == QuadKind::Text);
    CHECK(r.declaration.quads[1].kind == QuadKind::Icon);
    CHECK(r.declaration.quads[1].require_presence == true);
}

TEST_CASE("S641 default_tolerance_for keys tight solid, intermediate text, loose icon")
{
    CHECK(default_tolerance_for(QuadKind::Solid) == doctest::Approx(0.02));
    CHECK(default_tolerance_for(QuadKind::Text)  == doctest::Approx(0.08));
    CHECK(default_tolerance_for(QuadKind::Icon)  == doctest::Approx(0.25));
}

TEST_CASE("S641 no declaration block is a typed NoBlock error (never silent)")
{
    const auto r = parse_scan_declaration("<StackPanel Width=\"10\" Height=\"10\"/>");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::NoBlock);
    CHECK_FALSE(r.detail.empty());
}

TEST_CASE("S641 missing END marker is a typed MissingEnd error")
{
    const std::string xaml = "<!-- SCAN DECLARATION\nframe 10 10\n";
    const auto r = parse_scan_declaration(xaml);
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::MissingEnd);
}

TEST_CASE("S641 a malformed frame is a typed BadNumber/BadRect error")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nframe abc def\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::BadNumber);
    CHECK_FALSE(r.detail.empty());
}

TEST_CASE("S641 an unknown directive is a typed UnknownKeyword error")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nframe 10 10\nbogus 1\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::UnknownKeyword);
}

TEST_CASE("S641 a bad quad kind is a typed BadKind error")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nframe 10 10\nquad q weird auto\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::BadKind);
}

TEST_CASE("S641 a declaration without a frame is malformed")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nquad q solid rect 0,0,4,4\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::MalformedLine);
}

TEST_CASE("S641 comment lines and blank lines are skipped")
{
    const std::string xaml = R"xaml(<!-- SCAN DECLARATION
  ; a comment line
  frame 40 30

  quad a solid rect 1,1,2,2
END SCAN DECLARATION -->)xaml";
    const auto r = parse_scan_declaration(xaml);
    REQUIRE(r.ok);
    REQUIRE(r.declaration.quads.size() == 1u);
    CHECK(r.declaration.quads[0].name == "a");
}

TEST_CASE("S641 tolerance outside 0..1 is rejected")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nframe 10 10\nquad a solid rect 0,0,2,2 tolerance 1.5\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::BadNumber);
}

TEST_CASE("S642 the SCAN DECLARATION quad carries an optional capability id (the SYNC TOKEN)")
{
    // S642 extends the in-XAML declaration grammar with `capability <id>` so the
    // author's intent keys the SAME catalogue row as the emitter sidecar + the matrix
    // (the "keep both in sync" mechanism). The id is an optional bare token.
    const std::string xaml = R"xaml(<!-- SCAN DECLARATION
  frame 100 80
  quad title text auto band 8 capability text_01
  quad ok    icon auto centroid presence capability icon_02
  quad solid solid rect 0,0,10,10 tolerance 0.10 capability slot_int_01
END SCAN DECLARATION -->)xaml";
    const auto r = parse_scan_declaration(xaml);
    REQUIRE(r.ok);
    REQUIRE(r.declaration.quads.size() == 3u);
    CHECK(r.declaration.quads[0].capability == "text_01");
    CHECK(r.declaration.quads[0].kind == QuadKind::Text);
    CHECK(r.declaration.quads[1].capability == "icon_02");
    CHECK(r.declaration.quads[1].kind == QuadKind::Icon);
    CHECK(r.declaration.quads[2].capability == "slot_int_01");
    CHECK(r.declaration.quads[2].kind == QuadKind::Solid);
    CHECK(r.declaration.quads[2].has_tolerance);
    CHECK(r.declaration.quads[2].tolerance == doctest::Approx(0.10));
}

TEST_CASE("S642 a quad without a capability id leaves it empty (forward-compatible)")
{
    const std::string xaml = R"xaml(<!-- SCAN DECLARATION
  frame 20 20
  quad a solid rect 0,0,4,4
END SCAN DECLARATION -->)xaml";
    const auto r = parse_scan_declaration(xaml);
    REQUIRE(r.ok);
    REQUIRE(r.declaration.quads.size() == 1u);
    CHECK(r.declaration.quads[0].capability.empty());
}

TEST_CASE("S642 a bare capability token with no id is a typed MalformedLine error")
{
    const auto r = parse_scan_declaration(
        "<!-- SCAN DECLARATION\nframe 10 10\nquad a text auto capability\nEND SCAN DECLARATION -->");
    CHECK_FALSE(r.ok);
    CHECK(r.error == DeclarationError::MalformedLine);
}
