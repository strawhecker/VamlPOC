#pragma once
// VAML vulkan layer — PlatformWindow (S160).
//
// A real, permanent, linkable platform window backed by GLFW, covering the two
// backends S160 names: Win32 (Windows) and X11 (Linux). GLFW's window is created
// with GLFW_NO_API (no GL context) because this is a Vulkan presentation
// surface host — the Vulkan surface/surface-khr wiring lands at S162, and this
// type deliberately exposes native_handle()/framebuffer access for it.
//
// This is the "resize + input callback stub surface" of S160 — NOT a STUB and
// NOT MINIMAL REAL: the resize + input callback surface IS the real, kept
// contract, extended later (recorded NO-GAP):
//   * routed-event translation (WindowCallbacks -> routed events + hit-testing)
//     is resolved/extended at S323/S324 (Sprint 8).
//   * resize-safe swapchain recreation (consumes on_resize's framebuffer px)
//     is S167's scope.
// Do NOT backfill either here.
//
// Design: PlatformWindow is implemented in terms of GLFW (a concrete private
// member; no vk types here — this file must stay Vulkan-surface-agnostic so
// S162's surface code composes it cleanly). Non-copyable, movable; create() is
// the sole construction path and returns nullptr when window creation fails, so
// callers never juggle a half-constructed window.
//
// ADR-011: this header is in src/vulkan — only vaml-vulkan may touch GLFW.

#include <functional>
#include <memory>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// The S160 "resize + input callback stub surface": the concrete, kept callback
// contract the GLFW window forwards into. Each hook is invoked on
// PlatformWindow::poll_events() from the GLFW callback GLFW fires for the
// corresponding event; an EMPTY std::function is a silent no-op. Extend (add
// hooks) rather than reshape — existing consumers keep compiling (NO-GAP).
struct WindowCallbacks
{
    // Framebuffer resize, in framebuffer (physical) pixels — the value a
    // swapchain's extent must match (see S167).
    std::function<void(int width, int height)> on_resize;

    // Keyboard: GLFW key codes + action (GLFW_PRESS/RELEASE/REPEAT) + mods.
    std::function<void(int key, int scancode, int action, int mods)> on_key;

    // Mouse move, in window (logical) coordinates. Deliberately GLFW doubles so
    // no precision is lost; logical->physical scale is a later step (S213).
    std::function<void(double x, double y)> on_mouse_move;

    // Mouse button: GLFW button + action + mods; cursor position is passed via
    // on_mouse_move (GLFW has no per-button position), so S324's hit-testing
    // reads the last-reported cursor.
    std::function<void(int button, int action, int mods)> on_mouse_button;

    // Scroll wheel offset (horizontal/vertical).
    std::function<void(double xoffset, double yoffset)> on_scroll;

    // FILE-DROP (S223): GLFW's drag-and-drop event — Windows Explorer / X11 file
    // manager dropping one or more files onto the window. `count` absolute UTF-8
    // paths follow (paths[0]..paths[count-1]); the viewer reopens the dropped
    // `.vaml`. GLFW's single drop callback transparently covers BOTH backends
    // (Win32 + X11), exactly like the resize/key/mouse/scroll hooks above.
    std::function<void(int count, const char* const* paths)> on_drop;
};

// The S160 dispatch seams: pure function-style forwarders that invoke the
// matching (possibly-empty) WindowCallbacks hook. Kept PUBLIC + FREE so the
// callback surface is unit-testable without a live GLFW window, and so
// S323/S324 (routed events + hit-testing) can consume and extend them. Each is
// a safe no-op when its hook is empty.
void dispatch_resize(WindowCallbacks& cb, int width, int height);
void dispatch_key(WindowCallbacks& cb, int key, int scancode, int action, int mods);
void dispatch_mouse_move(WindowCallbacks& cb, double x, double y);
void dispatch_mouse_button(WindowCallbacks& cb, int button, int action, int mods);
void dispatch_scroll(WindowCallbacks& cb, double xoffset, double yoffset);
void dispatch_drop(WindowCallbacks& cb, int count, const char* const* paths);

// S594 (dep:S593) — OS-WINDOW TRANSPARENCY (per-platform, WPF-SOURCE-VALIDATED).
// The PURE decision seam: maps the feature-on flag to the GLFW window-hint set
// that makes the OS window per-pixel-transparent. Windows = DWM per-pixel
// (strictly the GLFW `GLFW_TRANSPARENT_FRAMEBUFFER` + `GLFW_ALPHA_BITS 8`
// hints — GLFW's Win32 implementation realizes per-pixel transparency via
// DwmEnableBlurBehindWindow, the WPF `ApplicationManagedLayer` per-pixel model
// mirrored here — NEVER the `WS_EX_LAYERED` constant-alpha color-key path that
// is WPF's forbidden `SystemManagedLayer`).
//
// This header stays GLFW-include-free (the S160 Vulkan-surface-agnostic intent);
// the Windows-side POC/test pins the hint tokens to the real `<GLFW/glfw3.h>`
// macros. NO-GAP: the `AllowsTransparency` -> `Config.transparentFramebuffer`
// wiring (the LIVE consumed surface) lands at S595.
constexpr int kMaxTransparencyHints = 2;

struct WindowTransparencyHint
{
    int hint  = 0;
    int value = 0;
};

struct WindowTransparencyHints
{
    int count = 0;
    WindowTransparencyHint entries[kMaxTransparencyHints]{};

    [[nodiscard]] bool empty() const noexcept { return count == 0; }
};

// Returns the GLFW window-hint set for the OS-window transparency feature (the
// S594 decision surface, headless-testable):
//   * featureOn == true  -> count 2: {GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE} +
//                          {GLFW_ALPHA_BITS, 8} (per-pixel DWM transparency).
//   * featureOn == false -> count 0 (empty; an opaque window — the ROADMAP
//                          NON-conditional rule: non-splash windows stay opaque).
// The header is GLFW-include-free; a consumer/Windows test pins the tokens to
// the real GLFW macros (S594's PlatformWindow_test.cpp asserts against them).
VAML_VULKAN_EXPORT WindowTransparencyHints window_transparency_hints(bool featureOn) noexcept;

// A real GLFW-backed platform window (S160). Win32 on Windows, X11 on Linux.
class PlatformWindow
{
public:
    struct Config
    {
        const char* title  = "VAML";
        int         width  = 800;
        int         height = 600;
        // S594 — OS-window transparency gate. Default FALSE (non-splash windows
        // stay OPAQUE — the ROADMAP NON-conditional rule). When TRUE, create()
        // applies `window_transparency_hints(true)` (GLFW_TRANSPARENT_FRAMEBUFFER
        // + GLFW_ALPHA_BITS 8) so the OS window is per-pixel-transparent.
        // The `AllowsTransparency` -> this flag wiring is S595 (NO-GAP).
        bool transparentFramebuffer = false;
    };

    // Creates the window + registers GLFW callbacks wired into `callbacks`.
    // Returns nullptr (no throw) if GLFW init or window creation fails.
    static std::unique_ptr<PlatformWindow> create(Config config, WindowCallbacks callbacks);

    // Pumps the GLFW event loop; dispatches any pending resize/input events
    // into the stored WindowCallbacks. Call from the window's message loop.
    void poll_events();

    // True once the user requests close (window close button / request_close).
    [[nodiscard]] bool should_close() const;

    // Sets the close flag (GLFW drive: ESC handler, programmatic close).
    void request_close();

    // The underlying GLFWwindow* (opaque; owned). null before create().
    [[nodiscard]] void* native_handle() const;

    // Framebuffer (physical pixel) size — the swapchain-extent source (S167).
    void get_framebuffer_size(int* width, int* height) const;

    // rule-of-five: movable, NOT copyable (a GLFWwindow is a singe-owned handle).
    PlatformWindow(PlatformWindow&&) noexcept;
    PlatformWindow& operator=(PlatformWindow&&) noexcept;
    ~PlatformWindow();
    PlatformWindow(const PlatformWindow&)            = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

private:
    PlatformWindow();  // constructed via create()
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml::vulkan
