# Chapter 1 — Hello, VAML!

> ### In This Chapter
> - Building a VAML app the way you used to click "New WPF Project"
> - Meeting the four parts of every VAML consumer
> - Choosing **static** vs **shared** (and why one is your friend at bedtime)
> - Running your first window and fighting your first empty `TextBlock`

**Evidence:** S440–S445 (POC app), S920/S921 (public facade), S942 (markup), S947 (compiled path).

---

So you said yes to VAML. Good for you—adventurous, slightly reckless, exactly our type. Congratulations,
you get to build software the way a C# dev builds it, in a language where the garbage collector has quit
and left no forwarding address.

Let's get a window on screen. Fast.

## 1.1 What the Wizard Used to Hide

> ### Remember
> The IDE *used to* hand you `App.xaml`, `MainWindow.xaml`, a generated `MainWindow.xaml.cs` with
> `InitializeComponent()`, and an `App.xaml.cs` with `OnStartup`. The shell, the markup compile, and the
> event loop were all free, instant, and invisible.

VAML has no wizard, and thank goodness, because wizards give you three secret files per project and call it
a feature. Instead you write the **four pieces of every VAML app** out in the open. This is not *harder* —
it just turns invisible magic into visible, debuggable code. You'll thank us the first time you need to
know *why* something is wired the way it is.

The four pieces:

1. **A CMake `add_executable`** that links the VAML core and a window/render backend.
2. **A `main.cpp`** that boots the runtime, builds your root view, and runs the events loop.
3. **A `.vaml` markup file** for your window (Chapter 2 does the dialect justice).
4. **The code-behind** — the C++ class named by `x:Class` (Chapters 3 and 5).

## 1.2 Static or Shared? Choose Once, Choose Static

Two ways to link, two philosophies, and — let's save you the suspense — **pick static unless you have a
reason not to.**

| | Static | Shared |
|---|---|---|
| What you get | One executable with VAML inside | A separate runtime library + your exe |
| Shipping | One file, done | Runtime ships alongside (version-skew watches you sleep) |
| Good for | **Release. IP-hiding. Your sanity.** | Hot-reload dev loops, plugin-style architectures |
| VAML's suggestion | ✅ (default) | Only if you need it |

> 💡 **Tip:** For a desktop app that just has to *work*, static is the answer. Shared earns its keep only
> when you genuinely want a plugin host or a live-edit loop (that's `S825`, hot-reload territory).

## 1.3 The Minimal CMake Consumer

Here's the whole CMake, no salad dressing:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyVamlApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Your checkout usually defines VAML_ROOT via a preset.
find_package(vaml CONFIG REQUIRED)

add_executable(myapp
  main.cpp
  MainWindow.vaml   # compiled to C++ by vamlcc (Chapter 7)
  MainWindow.cpp    # the x:Class code-behind owner
)

target_link_libraries(myapp PRIVATE
  vaml::core        # property/binding/command/events/runtime (S920 facade)
  vaml::vulkan      # window + render backend
)

# Compile-time markup → C++ (S947 / vamlcc). Omit it and you get the interpreted
# dev path; static vs shared is a separate knob (S827 packaging).
vamlcc_target(myapp)
```

> ⚙ **Technical Stuff (the skippable bit):** `vamlcc` is the build-time compiler that turns your `.vaml`
> into C++. It's the analog of XAML→BAML. Whether you *link* that statically or as a shared lib is
> orthogonal (Chapter 7). Don't let the two decisions blur — they're separate knobs.

## 1.4 The Minimal `main.cpp` — and Your First Mental-Model Change

```cpp
#include <vaml/app.h>
#include <vaml/run.h>
#include "MainWindow.h"

int main(int argc, char** argv)
{
    vaml::App app(argc, argv);                        // runtime: property system, dispatcher, render threads

    auto window = std::make_unique<MyMainWindow>();   // build your root x:Class owner

    int rc = vaml::run(app, std::move(window));       // runtime OWNS the window, runs the events loop
    return rc;                                        // graceful shutdown (S725 teardown order)
}
```

> ⚠ **Warning — the first forehead-slap: ownership.**
>
> In C#, the garbage collector kept your `window` alive after `Show()`. You could forget it existed and the
> runtime would hold it up for you forever.
>
> In VAML, **the runtime owns the window** — you hand it over with `std::move(window)`, and from then on
> *it* manages it. Do **not** keep a raw pointer copy around and use it after exit. Do **not** try to
> `delete` it yourself. This one habit — *decide who owns, and let them own* — is the root of more porting
> bugs than every syntax difference combined. We'll say it again in Chapters 3 and 8. Consider yourself
> warned three times in one sentence.

## 1.5 Your First `.vaml`

```vaml
<UserControl x:Class="MyMainWindow" Width="480" Height="320">
    <StackPanel Margin="24">
        <TextBlock Text="Hello from VAML" FontSize="28"/>
        <TextBlock Text="{Binding Greeting}" Margin="0,12,0,0"/>
    </StackPanel>
</UserControl>
```

That `x:Class="MyMainWindow"` is the handshake: it says "there is a C++ class named `MyMainWindow`, and this
markup is its body." You write that class in `MainWindow.h` (a taste in Chapter 2; the full affair in
Chapters 3 and 5).

## 1.6 Build and Run

```bash
cmake -S . -B build --preset vaml-debug   # presets live in your checkout
cmake --build build
./build/myapp                              # one static binary
```

You get a 480×320 window: "Hello from VAML" big and bold, and a second line that is either your bound
`Greeting` or — if you're reading this in a moment of quiet crisis — *empty*.

## 1.7 The Empty TextBlock: Your First Diagnosis

An empty binding is the VAML rite of passage. You'll see it, and you'll think "VAML is broken." It isn't.
The rule, which you'll meet properly in Chapter 3, is:

> ### Remember
> **Set to notify.** A bound property only refreshes when it fires `PropertyChanged` — and the only way to
> do that in VAML is through `property<T>::set(...)` (or replacing the whole value). Assignment to a raw
> member is silent and does nothing.

Your `Greeting` property probably doesn't call `set`. Chapter 3 fixes you up.

## 1.8 Troubleshooting, the "Day One" Table

| Symptom | Cause | Fix |
|---|---|---|
| Empty bound text | Property changed but never notified | Use `property<T>::set` (Ch.3) |
| Crash on exit | Raw pointer to window outlived it | Runtime owns it; don't keep a raw copy |
| "No Vulkan device" | Backend not linked / no driver | `target_link_libraries` the backend; check driver (S622) |
| Window opens then vanishes | Runtime doesn't own the root | `std::move(window)` into `vaml::run` |

## 1.9 In This Chapter, You Learned…

- A consumer is **four explicit parts**: CMake target, `main.cpp`, a `.vaml`, and its `x:Class` C++ owner.
- **Static** linking by default; shared only for hot-reload/plugins.
- The runtime **owns** the root view — the first and most important mental-model shift.
- An empty binding means you didn't **set to notify** (a very Chapter 3 problem).

**Next up:** in Chapter 2 we grab the markup dialect by the lapels. But if you're here for the life-changing
stuff, Chapter 3 is where the garbage collector leaves and you become the lifeguard.
