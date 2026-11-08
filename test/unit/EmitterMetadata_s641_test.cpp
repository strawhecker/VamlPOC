// S641 — EmitterMetadata_s641_test.cpp (forward-compatible sidecar JSON reader).
//
// Sprint 11r (S641): the reader that consumes the S640b emitter's `out.json`
// sidecar (dims + per-quad kinds) as the SECONDARY oracle for the compare. This
// suite proves the exact schema the emitter writes + the FORWARD-COMPAT contract
// (unknown/extra fields are IGNORED — a later S642 shape never strands the
// reader). Pure + headless (both CI lanes).

#include <vaml/presentation/scan/EmitterMetadata.h>

#include "doctest/doctest.h"

#include <string>
#include <vector>

using namespace vaml::presentation::scan;

TEST_CASE("S641 parse_emitter_metadata reads the emitter sidecar schema")
{
    // The EXACT shape S640b's System.Text.Json writes (see ImageBuffer.cs /
    // Program.cs): indented, enum as string, Quads[] with Name/X/Y/Width/Height/Kind.
    const std::string json = R"({
  "Width": 440,
  "Height": 240,
  "Source": "sample.image.xaml",
  "Quads": [
    {
      "Name": "SampleText",
      "X": 16,
      "Y": 96,
      "Width": 120,
      "Height": 40,
      "Kind": "Text"
    },
    {
      "Name": "SampleIcon",
      "X": 16,
      "Y": 16,
      "Width": 64,
      "Height": 64,
      "Kind": "Icon"
    }
  ]
})";
    const auto r = parse_emitter_metadata(json);
    REQUIRE(r.ok);
    CHECK(r.metadata.width == 440u);
    CHECK(r.metadata.height == 240u);
    CHECK(r.metadata.source == "sample.image.xaml");
    REQUIRE(r.metadata.quads.size() == 2u);
    CHECK(r.metadata.quads[0].name == "SampleText");
    CHECK(r.metadata.quads[0].x == 16);
    CHECK(r.metadata.quads[0].y == 96);
    CHECK(r.metadata.quads[0].w == 120);
    CHECK(r.metadata.quads[0].h == 40);
    CHECK(r.metadata.quads[0].kind == QuadKind::Text);
    CHECK(r.metadata.quads[0].kind_known == true);
    CHECK(r.metadata.quads[1].kind == QuadKind::Icon);
}

TEST_CASE("S641 parse_emitter_metadata is forward-compatible (unknown fields ignored)")
{
    // A FUTURE metadata shape with extra top-level + per-quad fields must PARSE
    // (never strand the reader) — the forward-compat contract.
    const std::string json = R"({
  "Width": 10,
  "Height": 20,
  "Source": "x.xaml",
  "Version": 2,
  "Extra": { "a": 1 },
  "Quads": [
    { "Name": "q1", "X": 0, "Y": 0, "Width": 4, "Height": 4, "Kind": "Text",
      "NewField": 7, "IconType": "fancy" }
  ]
})";
    const auto r = parse_emitter_metadata(json);
    REQUIRE(r.ok);
    REQUIRE(r.metadata.quads.size() == 1u);
    CHECK(r.metadata.quads[0].name == "q1");
    CHECK(r.metadata.quads[0].kind == QuadKind::Text);
}

TEST_CASE("S641 parse_emitter_metadata treats an unknown kind as Solid (not fatal)")
{
    const std::string json = R"({ "Width": 5, "Height": 5, "Quads": [
      { "Name": "a", "X": 0, "Y": 0, "Width": 2, "Height": 2, "Kind": "Mystery" }
    ]})";
    const auto r = parse_emitter_metadata(json);
    REQUIRE(r.ok);
    REQUIRE(r.metadata.quads.size() == 1u);
    CHECK(r.metadata.quads[0].kind == QuadKind::Solid);  // graceful default
    CHECK(r.metadata.quads[0].kind_known == true);       // a "Mystery" string was read
}

TEST_CASE("S641 parse_emitter_metadata rejects a non-object root (never silent)")
{
    const auto r = parse_emitter_metadata("[1,2,3]");
    CHECK_FALSE(r.ok);
    CHECK(r.error == MetadataError::NotObject);
}

TEST_CASE("S641 parse_emitter_metadata rejects malformed JSON (typed, never silent)")
{
    const auto r = parse_emitter_metadata("{ \"Width\": ");
    CHECK_FALSE(r.ok);
    const bool isTruncated = (r.error == MetadataError::Truncated);
    // Binary comparison (doctest cannot decompose a compound ||).
    if (isTruncated)
    {
        CHECK(r.error == MetadataError::Truncated);
    }
    else
    {
        CHECK(r.error == MetadataError::BadNumber);
    }
    CHECK_FALSE(r.detail.empty());
}

TEST_CASE("S641 parse_emitter_metadata with no quads is valid (empty quads)")
{
    const auto r = parse_emitter_metadata("{ \"Width\": 4, \"Height\": 4 }");
    REQUIRE(r.ok);
    CHECK(r.metadata.width == 4u);
    CHECK(r.metadata.quads.empty());
}
