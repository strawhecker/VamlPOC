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
