// S202 — texture-atlas decision seams (headless).
//
// Tests S202's PURE decision seams with fabricated values — NO live Vulkan
// runtime, loader, device, or atlas (the S160..S200 precedent: CI proves the
// decision layer headless; the LIVE TextureAtlas create/upload/flush path is
// manual via the POCs, [both apps, not CI]).
//
// The HEADLINE proofs: the EXACT-GRID contract (a valid layout requires
// atlasW%cols==0 && atlasH%rows==0 — no tile straddles a partial edge), the
// tile placement math (region + byte offset for first/mid/last tiles), the byte
// extent seams (tile/image bytes), and the SUB-REGION INVALIDATION derivation
// (atlas_dirty_regions — empty/ascending/deduped/multi).

#include <vaml/vulkan/quad/TextureAtlas.h>

#include <cstdint>
#include <vector>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for the
// TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan::quad;

TEST_CASE("S202 atlas_layout exact-grid contract")
{
    // An exact 8x8 image as a 4x4 grid: 2x2 tiles. Valid.
    const AtlasLayout l = atlas_layout(8, 8, 4, 4);
    CHECK(atlas_layout_valid(l));
    CHECK(l.cols == 4u);
    CHECK(l.rows == 4u);
    CHECK(l.tileW == 2u);
    CHECK(l.tileH == 2u);

    // Misaligned (atlas 7x7, 4x4 grid) -> INVALID (7 % 4 != 0 both axes).
    CHECK_FALSE(atlas_layout_valid(atlas_layout(7, 7, 4, 4)));

    // One misaligned axis is enough to invalidate.
    CHECK_FALSE(atlas_layout_valid(atlas_layout(8, 7, 4, 4)));
    CHECK_FALSE(atlas_layout_valid(atlas_layout(7, 8, 4, 4)));

    // Zero-dimension inputs -> invalid (not a crash, all-zero layout).
    CHECK_FALSE(atlas_layout_valid(atlas_layout(0, 8, 4, 4)));
    CHECK_FALSE(atlas_layout_valid(atlas_layout(8, 0, 4, 4)));
    CHECK_FALSE(atlas_layout_valid(atlas_layout(8, 8, 0, 4)));
    CHECK_FALSE(atlas_layout_valid(atlas_layout(8, 8, 4, 0)));

    // A single tile (1x1 grid) over any size is always exact.
    CHECK(atlas_layout_valid(atlas_layout(64, 32, 1, 1)));

    // 3x2 grid over 12x8: 4x4 tiles, exact.
    const AtlasLayout l32 = atlas_layout(12, 8, 3, 2);
    CHECK(atlas_layout_valid(l32));
    CHECK(l32.tileW == 4u);
    CHECK(l32.tileH == 4u);
}

TEST_CASE("S202 atlas_tile_count")
{
    CHECK(atlas_tile_count(atlas_layout(8, 8, 4, 4)) == 16u);
    CHECK(atlas_tile_count(atlas_layout(12, 8, 3, 2)) == 6u);
    CHECK(atlas_tile_count(atlas_layout(64, 32, 1, 1)) == 1u);
    CHECK(atlas_tile_count(atlas_layout(8, 7, 4, 4)) == 0u);  // invalid
    CHECK(atlas_tile_count(AtlasLayout{}) == 0u);             // default/invalid
}

TEST_CASE("S202 atlas_tile_region placement (first/mid/last)")
{
    const AtlasLayout l = atlas_layout(8, 8, 4, 4);  // 4x4 grid, 2x2 tiles
    CHECK(atlas_tile_count(l) == 16u);

    const AtlasRegion r0 = atlas_tile_region(l, 0);
    CHECK(r0.x == 0u);
    CHECK(r0.y == 0u);
    CHECK(r0.w == 2u);
    CHECK(r0.h == 2u);

    // Row 1, col 2: x = 2*2 = 4, y = 1*2 = 2.
    const AtlasRegion r6 = atlas_tile_region(l, 6);
    CHECK(r6.x == 4u);
    CHECK(r6.y == 2u);

    // Last tile (index 15): x = 3*2 = 6, y = 3*2 = 6.
    const AtlasRegion r15 = atlas_tile_region(l, 15);
    CHECK(r15.x == 6u);
    CHECK(r15.y == 6u);

    // Out-of-range -> all-zero.
    const AtlasRegion rr = atlas_tile_region(l, 16);
    CHECK(rr.x == 0u);
    CHECK(rr.y == 0u);
    CHECK(rr.w == 0u);
    CHECK(rr.h == 0u);
}

TEST_CASE("S202 atlas_tile_offset byte math")
{
    const AtlasLayout l = atlas_layout(8, 8, 4, 4);  // 2x2 tiles, RGBA8
    // Tile 0 offset = 0.
    CHECK(atlas_tile_offset(l, 0) == 0u);
    // Tile 6 = row 1, col 2: (2*8 + 4)*4 = (16+4)*4 = 80.
    CHECK(atlas_tile_offset(l, 6) == 80u);
    // Tile 15 = row 3, col 3: x = (15%4)*2 = 6, y = (15/4)*2 = 6;
    // offset = (y*atlasW + x)*4 = (6*8 + 6)*4 = 54*4 = 216.
    CHECK(atlas_tile_offset(l, 15) == 216u);
    // Out-of-range -> 0.
    CHECK(atlas_tile_offset(l, 16) == 0u);
    // Invalid layout -> 0.
    CHECK(atlas_tile_offset(AtlasLayout{}, 0) == 0u);
}

TEST_CASE("S202 atlas_tile_bytes + atlas_image_bytes")
{
    const AtlasLayout l = atlas_layout(8, 8, 4, 4);
    CHECK(atlas_tile_bytes(l) == 2u * 2u * 4u);   // 16 bytes
    CHECK(atlas_image_bytes(l) == 8u * 8u * 4u);  // 256 bytes

    const AtlasLayout big = atlas_layout(12, 8, 3, 2);  // 4x4 tiles
    CHECK(atlas_tile_bytes(big) == 4u * 4u * 4u);       // 64 bytes
    CHECK(atlas_image_bytes(big) == 12u * 8u * 4u);     // 384 bytes

    CHECK(atlas_tile_bytes(AtlasLayout{}) == 0u);
    CHECK(atlas_image_bytes(AtlasLayout{}) == 0u);
    CHECK(atlas_tile_bytes(atlas_layout(8, 7, 4, 4)) == 0u);  // invalid
}

TEST_CASE("S202 atlas_dirty_regions — empty, ascending, deduped, multi")
{
    const AtlasLayout l = atlas_layout(8, 8, 4, 4);  // 2x2 tiles

    // Empty -> empty.
    CHECK(atlas_dirty_regions(l, {}).empty());

    // Out-of-range indices are ignored.
    CHECK(atlas_dirty_regions(l, {99u, 1000u}).empty());

    // Ascending order is preserved (tile 1 then tile 6).
    const auto r = atlas_dirty_regions(l, {1u, 6u});
    REQUIRE(r.size() == 2u);
    CHECK(r[0].x == 2u);  // tile 1 = row 0, col 1
    CHECK(r[0].y == 0u);
    CHECK(r[1].x == 4u);  // tile 6 = row 1, col 2
    CHECK(r[1].y == 2u);

    // Input order does NOT matter — result is always ascending by tile index.
    const auto rRev = atlas_dirty_regions(l, {6u, 1u});
    REQUIRE(rRev.size() == 2u);
    CHECK(rRev[0].x == 2u);
    CHECK(rRev[1].x == 4u);

    // Dedupe: an index appearing twice yields ONE region.
    const auto rDup = atlas_dirty_regions(l, {3u, 3u});
    REQUIRE(rDup.size() == 1u);
    CHECK(rDup[0].x == 6u);
    CHECK(rDup[0].y == 0u);

    // Multi with a duplicate interleaved + an out-of-range: {0,2,2,99,7}
    const auto rMulti = atlas_dirty_regions(l, {0u, 2u, 2u, 99u, 7u});
    REQUIRE(rMulti.size() == 3u);  // 0, 2, 7 (99 ignored, dup dropped)
    CHECK(rMulti[0].x == 0u);
    CHECK(rMulti[0].y == 0u);
    CHECK(rMulti[1].x == 4u);
    CHECK(rMulti[1].y == 0u);      // tile 2 = row 0, col 2
    CHECK(rMulti[2].x == 6u);
    CHECK(rMulti[2].y == 2u);      // tile 7 = row 1, col 3

    // Invalid layout -> empty.
    CHECK(atlas_dirty_regions(AtlasLayout{}, {0u}).empty());
}
