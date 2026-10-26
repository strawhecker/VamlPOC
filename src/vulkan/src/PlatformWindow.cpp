// S160: PlatformWindow implementation — GLFW-backed, Win32 + X11.
//
// The window is a thin real shell over GLFW: create() runs glfwInit +
// glfwCreateWindow(GLFW_NO_API), registers the GLFW resize/key/cursor/button/
// scroll callbacks, and forwards each into the stored WindowCallbacks via the
// free dispatch seams below. The dispatch seams are deliberately FREE functions
// (not member/private) so the S160 unit tests can feed synthetic resize/key/
// mouse payloads WITHOUT a real display/GLFW window — the callback surface is
// proven headless; only the live window (manual POCs) exercises GLFW itself.
//
// S167 (resize-safe swapchain) consumes on_resize's framebuffer px; S323/S324
// (routed events + hit-testing) consume the input hooks. Recorded NO-GAP; not
// backfilled here.

#include <vaml/vulkan/PlatformWindow.h>

// S160's window is Vulkan-AGNOSTIC: it does NOT include the Vulkan headers
// (GLFW_INCLUDE_VULKAN is deliberately NOT defined here). GLFW only needs its
// own glfw3.h to create a GLFW_NO_API window; the Vulkan surface wiring arrives
// at S162 as a separate TU that includes vulkan.h (with the video-std
// pre-includes that this vendored header version requires — see VmaImpl.cpp).
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <atomic>
#include <utility>

namespace {

// S446 hardening — GLFW process-lifetime singleton ownership.
//
// GLFW is a PROCESS-LIFETIME library: `glfwInit()` allocates an internal
// `_glfw` context, and `glfwCreateWindow()` allocates the `_glfw.window` heap
// block. Nothing is reclaimed until `glfwTerminate()` runs, and GLFW has
// explicit lifetime rules:
//   * every window must be `glfwDestroyWindow`'d;
//   * `glfwTerminate()` must only run AFTER the last window is destroyed.
//
// Before S446, PlatformWindow created windows but NEVER destroyed them nor
// terminated GLFW, so the library context + every window silently outlived the
// whole process — a genuine leak the S446 LSan lane (verify_s446_asan.cmd,
// clang-asan + detect_leaks=1 on the Ubuntu VM) caught:
//
//   allocated by vaml::vulkan::PlatformWindow::create
//     -> ViewerApp::create_compiled (the pre-existing S293 test builds a REAL
//        GLFW window)
//   ERROR: LeakSanitizer: detected memory leaks
//
// This was a REAL defect (not a false positive), and the S446 "no leaks over
// 10k frames / no leaks" contract is exactly the warrant to close it — the
// retained app-shell / viewer windows must fully release at teardown.
//
// Fix: PlatformWindow now REFERENCE-COUNTS its GLFW windows. The first window
// runs `glfwInit()` (returns false and drops the ref if init is unavailable);
// each window is `glfwDestroyWindow`'d in its destructor (the TRUE RAII close);
// and the LAST window to be destroyed runs `glfwTerminate()` — the only legal
// caller, after all windows are gone (GLFW's own lifetime rule). This makes the
// window+library lifetime exactly match PlatformWindow's own lifetime on BOTH
// backends (Win32 + X11), so a fully-torn-down window leaves a clean LSan heap.
std::atomic<int> g_glfwWindowCount{0};

} // namespace

namespace vaml::vulkan {

// ---- S160 testable dispatch seams (free functions) -------------------------

void dispatch_resize(WindowCallbacks& cb, int width, int height)
{
    if (cb.on_resize)
        cb.on_resize(width, height);
}

void dispatch_key(WindowCallbacks& cb, int key, int scancode, int action, int mods)
{
    if (cb.on_key)
        cb.on_key(key, scancode, action, mods);
}

void dispatch_mouse_move(WindowCallbacks& cb, double x, double y)
{
    if (cb.on_mouse_move)
        cb.on_mouse_move(x, y);
}

void dispatch_mouse_button(WindowCallbacks& cb, int button, int action, int mods)
{
    if (cb.on_mouse_button)
        cb.on_mouse_button(button, action, mods);
}

void dispatch_scroll(WindowCallbacks& cb, double xoffset, double yoffset)
{
    if (cb.on_scroll)
        cb.on_scroll(xoffset, yoffset);
}

void dispatch_drop(WindowCallbacks& cb, int count, const char* const* paths)
{
    if (cb.on_drop && count > 0 && paths)
        cb.on_drop(count, paths);
}

// ---- GLFW callback trampolines ---------------------------------------------

namespace {

// GLFW's user-pointer is the WindowCallbacks (non-owning). One helper casts it.
WindowCallbacks* callbacksOf(GLFWwindow* window)
{
    return static_cast<WindowCallbacks*>(glfwGetWindowUserPointer(window));
}

void glfwResize(GLFWwindow* window, int w, int h)
{
    if (auto* cb = callbacksOf(window))
        dispatch_resize(*cb, w, h);
}

void glfwKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (auto* cb = callbacksOf(window))
        dispatch_key(*cb, key, scancode, action, mods);
}

void glfwCursor(GLFWwindow* window, double x, double y)
{
    if (auto* cb = callbacksOf(window))
        dispatch_mouse_move(*cb, x, y);
}

void glfwMouseButton(GLFWwindow* window, int button, int action, int mods)
{
    if (auto* cb = callbacksOf(window))
        dispatch_mouse_button(*cb, button, action, mods);
}

void glfwScroll(GLFWwindow* window, double xo, double yo)
{
    if (auto* cb = callbacksOf(window))
        dispatch_scroll(*cb, xo, yo);
}

void glfwDrop(GLFWwindow* window, int count, const char** paths)
{
    if (auto* cb = callbacksOf(window))
        dispatch_drop(*cb, count, paths);
}

} // namespace

// ----------------------------------------------------------------------------

struct PlatformWindow::Impl
{
    GLFWwindow* window = nullptr;
    WindowCallbacks callbacks;

    // S446 RAII close: destroy THIS window, and when it is the last one, legally
    // terminate GLFW (GLFW requires every window be glfwDestroyWindow'd before
    // glfwTerminate). This is what releases the library context + window that
    // previously leaked for the whole process lifetime.
    ~Impl()
    {
        if (window)
            glfwDestroyWindow(window);
        if (--g_glfwWindowCount == 0)
            glfwTerminate();
    }
};

PlatformWindow::PlatformWindow() = default;

PlatformWindow::PlatformWindow(PlatformWindow&& other) noexcept
    : impl_(std::move(other.impl_))
{
}

PlatformWindow& PlatformWindow::operator=(PlatformWindow&& other) noexcept
{
    if (this != &other)
    {
        // Release any window THIS object currently owns before adopting other's.
        impl_.reset();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

PlatformWindow::~PlatformWindow()
{
    impl_.reset();  // destroys the Impl (and its GLFW window when owned)
}

std::unique_ptr<PlatformWindow> PlatformWindow::create(Config config, WindowCallbacks callbacks)
{
    if (g_glfwWindowCount == 0 && !glfwInit())
        return nullptr;   // first window: init GLFW; bail out clean if unavailable

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Vulkan host, no GL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    auto self = std::unique_ptr<PlatformWindow>(new PlatformWindow());
    self->impl_              = std::make_unique<Impl>();
    self->impl_->callbacks   = std::move(callbacks);

    self->impl_->window = glfwCreateWindow(config.width, config.height,
                                           config.title ? config.title : "VAML",
                                           nullptr, nullptr);
    if (!self->impl_->window)
    {
        // No live windows remain from this failed attempt => legally terminate.
        if (g_glfwWindowCount == 0)
            glfwTerminate();
        return nullptr;
    }
    ++g_glfwWindowCount;  // OWN one GLFW window; the Impl's destructor will release it

    // Own the callback surface through GLFW's user pointer (non-owning; the
    // Impl lives as long as the PlatformWindow).
    glfwSetWindowUserPointer(self->impl_->window, &self->impl_->callbacks);
    glfwSetFramebufferSizeCallback(self->impl_->window, &glfwResize);
    glfwSetKeyCallback(self->impl_->window, &glfwKey);
    glfwSetCursorPosCallback(self->impl_->window, &glfwCursor);
    glfwSetMouseButtonCallback(self->impl_->window, &glfwMouseButton);
    glfwSetScrollCallback(self->impl_->window, &glfwScroll);
    glfwSetDropCallback(self->impl_->window, &glfwDrop);

    return self;
}

void PlatformWindow::poll_events()
{
    if (impl_ && impl_->window)
        glfwPollEvents();
}

bool PlatformWindow::should_close() const
{
    return impl_ && impl_->window && glfwWindowShouldClose(impl_->window);
}

void PlatformWindow::request_close()
{
    if (impl_ && impl_->window)
        glfwSetWindowShouldClose(impl_->window, GLFW_TRUE);
}

void* PlatformWindow::native_handle() const
{
    return impl_ ? impl_->window : nullptr;
}

void PlatformWindow::get_framebuffer_size(int* width, int* height) const
{
    if (impl_ && impl_->window)
        glfwGetFramebufferSize(impl_->window, width, height);
}

} // namespace vaml::vulkan
