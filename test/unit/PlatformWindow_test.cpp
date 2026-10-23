// S160 — PlatformWindow callback surface (headless-safe).
//
// Tests the S160 "resize + input callback stub surface" WITHOUT a live GLFW
// window or display: the dispatch seams (dispatch_resize/dispatch_key/...) are
// free public functions that forward into the (possibly empty) WindowCallbacks
// hooks. This proves the S160 callback contract on CI (headless); the LIVE
// window (glfwCreateWindow actually firing GLFW callbacks) is proven manually
// via the VamlPOC / VamlPOC-linux apps, NOT in ctest.
//
// Cases:
//   1. WindowCallbacks default-constructs with all-empty hooks (no-op safety).
//   2. Each of the five dispatch seams is a silent no-op when its hook is empty.
//   3. Each dispatch invokes exactly its matching hook, with exact arguments.
//   4. Independent hooks don't cross-fire.
//
// Deliberately has NO GTEST-style display dependency. Kept + extended by
// S323/S324 (routed events + hit-testing) and S167 (resize + swapchain).

#include <vaml/vulkan/PlatformWindow.h>

#include <GLFW/glfw3.h>  // GLFW_* constants referenced by the callback surface

#include <string>
#include <vector>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

using namespace vaml::vulkan;

TEST_CASE("S160 WindowCallbacks default-constructs empty (no-op safe)")
{
    WindowCallbacks cb;  // all std::function are empty

    CHECK(!cb.on_resize);
    CHECK(!cb.on_key);
    CHECK(!cb.on_mouse_move);
    CHECK(!cb.on_mouse_button);
    CHECK(!cb.on_scroll);
    CHECK(!cb.on_drop);
}

TEST_CASE("S160 dispatch seams are silent no-ops on empty hooks")
{
    WindowCallbacks cb;  // empty

    // Must not throw / do anything.
    dispatch_resize(cb, 640, 480);
    dispatch_key(cb, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    dispatch_mouse_move(cb, 10.0, 20.0);
    dispatch_mouse_button(cb, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    dispatch_scroll(cb, 1.0, 2.0);
    // S223: an empty on_drop is a silent no-op — drops ignored until a hook is set.
    dispatch_drop(cb, 1, nullptr);

    CHECK(true);  // reached without incident
}

TEST_CASE("S160 dispatch_resize forwards exact framebuffer size")
{
    WindowCallbacks cb;
    int gotW = -1, gotH = -1;
    cb.on_resize = [&](int w, int h) { gotW = w; gotH = h; };

    dispatch_resize(cb, 1024, 768);

    CHECK(gotW == 1024);
    CHECK(gotH == 768);
}

TEST_CASE("S160 dispatch_key forwards key/scancode/action/mods")
{
    WindowCallbacks cb;
    int k = -1, s = -1, a = -1, m = -1;
    cb.on_key = [&](int key, int scancode, int action, int mods) {
        k = key; s = scancode; a = action; m = mods;
    };

    dispatch_key(cb, GLFW_KEY_A, 30, GLFW_REPEAT, GLFW_MOD_SHIFT);

    CHECK(k == GLFW_KEY_A);
    CHECK(s == 30);
    CHECK(a == GLFW_REPEAT);
    CHECK(m == GLFW_MOD_SHIFT);
}

TEST_CASE("S160 dispatch_mouse_move forwards cursor doubles")
{
    WindowCallbacks cb;
    double x = 0.0, y = 0.0;
    cb.on_mouse_move = [&](double mx, double my) { x = mx; y = my; };

    dispatch_mouse_move(cb, 12.5, 99.25);

    CHECK(x == 12.5);
    CHECK(y == 99.25);
}

TEST_CASE("S160 dispatch_mouse_button forwards button/action/mods")
{
    WindowCallbacks cb;
    int b = -1, a = -1, m = -1;
    cb.on_mouse_button = [&](int button, int action, int mods) {
        b = button; a = action; m = mods;
    };

    dispatch_mouse_button(cb, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, GLFW_MOD_CONTROL);

    CHECK(b == GLFW_MOUSE_BUTTON_RIGHT);
    CHECK(a == GLFW_RELEASE);
    CHECK(m == GLFW_MOD_CONTROL);
}

TEST_CASE("S160 dispatch_scroll forwards offsets")
{
    WindowCallbacks cb;
    double xo = 0.0, yo = 0.0;
    cb.on_scroll = [&](double ox, double oy) { xo = ox; yo = oy; };

    dispatch_scroll(cb, 3.0, -1.0);

    CHECK(xo == 3.0);
    CHECK(yo == -1.0);
}

TEST_CASE("S160 dispatch seams fire only their own hook")
{
    WindowCallbacks cb;
    int resizeFires = 0, keyFires = 0;
    cb.on_resize = [&](int, int) { ++resizeFires; };
    cb.on_key    = [&](int, int, int, int) { ++keyFires; };

    dispatch_resize(cb, 100, 100);
    dispatch_key(cb, GLFW_KEY_B, 0, GLFW_PRESS, 0);

    CHECK(resizeFires == 1);
    CHECK(keyFires == 1);
}

// ===========================================================================
// S223 (dep:S222_1) — drag-and-drop FILE-DROP callback surface.
// The GLFW drop path (glfwSetDropCallback -> glfwDrop trampoline -> dispatch_drop)
// covers BOTH Win32 + X11. Headless here: dispatch_drop is a pure free seam like
// the S160 siblings — fed synthetic count/path payloads, never a live window.
// ===========================================================================

TEST_CASE("S223 dispatch_drop is a silent no-op on an empty hook / null payload")
{
    WindowCallbacks cb;  // empty on_drop
    const char* p0 = "C:/a.vaml";
    const char* paths[] = { p0 };

    dispatch_drop(cb, 1, paths);   // empty hook -> silent no-op
    dispatch_drop(cb, 0, paths);   // count 0 -> silent no-op
    dispatch_drop(cb, 1, nullptr); // null path array -> silent no-op

    CHECK(true);  // reached without incident
}

TEST_CASE("S223 dispatch_drop forwards exact count + paths (single-file drop)")
{
    WindowCallbacks cb;
    int gotCount = -1;
    std::vector<std::string> got;
    cb.on_drop = [&](int count, const char* const* paths) {
        gotCount = count;
        got.clear();
        for (int i = 0; i < count; ++i)
            got.emplace_back(paths[i]);
    };

    const char* p0 = "C:/scenes/scene_one.vaml";
    const char* paths[] = { p0 };
    dispatch_drop(cb, 1, paths);

    CHECK(gotCount == 1);
    REQUIRE(got.size() == 1u);
    CHECK(got[0] == std::string("C:/scenes/scene_one.vaml"));
}

TEST_CASE("S223 dispatch_drop forwards a MULTI-file drop intact")
{
    WindowCallbacks cb;
    std::vector<std::string> got;
    cb.on_drop = [&](int count, const char* const* paths) {
        got.clear();
        for (int i = 0; i < count; ++i)
            got.emplace_back(paths[i]);
    };

    const char* p0 = "/home/robert/a.vaml";
    const char* p1 = "/home/robert/b.vaml";
    const char* p2 = "/mnt/hgfs/notes.txt";
    const char* paths[] = { p0, p1, p2 };
    dispatch_drop(cb, 3, paths);

    REQUIRE(got.size() == 3u);
    CHECK(got[0] == std::string("/home/robert/a.vaml"));
    CHECK(got[1] == std::string("/home/robert/b.vaml"));
    CHECK(got[2] == std::string("/mnt/hgfs/notes.txt"));
}

TEST_CASE("S223 dispatch_drop does not cross-fire other hooks")
{
    WindowCallbacks cb;
    int resizeFires = 0, dropFires = 0;
    cb.on_resize = [&](int, int) { ++resizeFires; };
    cb.on_drop   = [&](int, const char* const*) { ++dropFires; };

    const char* p0 = "drop.vaml";
    const char* paths[] = { p0 };
    dispatch_resize(cb, 10, 10);
    dispatch_drop(cb, 1, paths);

    CHECK(resizeFires == 1);
    CHECK(dropFires == 1);
}
