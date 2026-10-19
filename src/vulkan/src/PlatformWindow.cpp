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

#include <utility>

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

} // namespace

// ----------------------------------------------------------------------------

struct PlatformWindow::Impl
{
    GLFWwindow* window = nullptr;
    WindowCallbacks callbacks;
};

PlatformWindow::PlatformWindow() = default;

PlatformWindow::PlatformWindow(PlatformWindow&&) noexcept = default;
PlatformWindow& PlatformWindow::operator=(PlatformWindow&&) noexcept = default;
PlatformWindow::~PlatformWindow() = default;

std::unique_ptr<PlatformWindow> PlatformWindow::create(Config config, WindowCallbacks callbacks)
{
    if (!glfwInit())
        return nullptr;

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
        glfwTerminate();
        return nullptr;
    }

    // Own the callback surface through GLFW's user pointer (non-owning; the
    // Impl lives as long as the PlatformWindow).
    glfwSetWindowUserPointer(self->impl_->window, &self->impl_->callbacks);
    glfwSetFramebufferSizeCallback(self->impl_->window, &glfwResize);
    glfwSetKeyCallback(self->impl_->window, &glfwKey);
    glfwSetCursorPosCallback(self->impl_->window, &glfwCursor);
    glfwSetMouseButtonCallback(self->impl_->window, &glfwMouseButton);
    glfwSetScrollCallback(self->impl_->window, &glfwScroll);

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
