# Chapter 7 — Compiled Path & Release: From `.vaml` to a Single Binary

> ### In This Chapter
> - What `vamlcc` actually does to your `.vaml` at build time
> - `InitializeComponent`'s new address: `vaml::markup::load`
> - The two paths — compiled vs interpreted — and when each earns its keep
> - Static vs shared, revisited, and shipping one file instead of a haunted house
> - The release checklist that keeps a Friday from becoming a Monday

**Evidence:** S2 (vamlcc), S826 (vamlcc diagnostics), S827 (single-exe packaging + install rules),
S920/S921 (public facade), S947. Snippets compiled in CI (S951).

---

You've written markup, wired bindings, made it respond. Now for the grown-up question every C# dev secretly
wants answered: **"what happens to my XAML at build time, and what do I ship?"** VAML answers both without
leaving you to assemble a haunted house of dependencies.

## 7.1 The C# You Knew

XAML compiled to BAML and got embedded. `InitializeComponent()` deserialized it. You shipped with the .NET
runtime, and packaging was mostly the framework's business. Convenient — and also a bag you never looked into.

## 7.2 `vamlcc`: Markup → C++ (Meet Your Compile-Time Friend)

`vamlcc` compiles a `.vaml` file into C++ at build time. Your CMake invokes it via `vamlcc_target(myapp)`
from Chapter 1.

- **The generated C++** is a per-markup loader: it constructs the element tree, applies styles, resolves
  resources, wires handlers (`S322`) and bindings, and runs layout — *the* `InitializeComponent` analog.
  You never read it. You call it from your `x:Class` constructor:
  ```cpp
  vaml::markup::load(this, "MainWindow.vaml");   // Ch.3.5 — InitializeComponent's new address
  ```
- **Diagnostics are written for you, the author:** types, properties, handlers, and bindings that don't
  resolve are **compile-time errors** with source line/column + a suggested fix (`S826`). Earlier and
  clearer than WPF's runtime `XamlParseException`. (We've said this twice; it bears a third.)

> ### Remember
> **WPF:** your typo blew up at runtime on a customer's machine.
> **VAML:** your typo is a compile-time error with line, column, and a suggested fix.
> This is not a small quality-of-life win. It's the whole difference between "shipped a landmine" and
> "caught it at the door."

## 7.3 The Two Paths: Compiled and Interpreted

VAML supports two ways to run markup — and they're kept behaviorally identical as an invariant:

- **Compiled (default, release):** `vamlcc` → C++ → baked into your executable. Fastest; no parser at
  runtime; ships *inside* the binary.
- **Interpreted (dev/hot-reload):** a `vaml` reader (`S2`) loads `.vaml` at runtime. This powers the
  hot-reload dev loop (`S825`) — edit the markup, see it live — and is the fallback when you want to poke a
  single file without a rebuild.

The two paths are kept identical by invariant tests (`S122/S123` parser, `S523` reader).

> 💡 **Tip — how to choose:** Release? Compiled. Debugging a layout and want live edits? Interpreted
> (hot-reload). The default is compiled for release; keep the interpreted reader only where the live-edit
> loop is worth carrying a parser in the process. You don't have to pick a side forever — you pick per
> build.

## 7.4 The `InitializeComponent` Mental Map

| WPF / C# | VAML C++ |
|---|---|
| `MainWindow.xaml` | `MainWindow.vaml` |
| `InitializeComponent()` | `vaml::markup::load(this, "MainWindow.vaml")` |
| XAML → BAML at build | `.vaml` → C++ at build |
| BAML deserialized at runtime | compiled C++ executed (or interpreted, dev-only) |
| runtime `XamlParseException` | **compile-time** `vamlcc` diagnostic |

> ◇ **Remember:** one swap — `InitializeComponent()` → `vaml::markup::load(this, "X.vaml")` — and the whole
> "how does my markup get into my code" story resolves into a single, readable line.

## 7.5 Static vs Shared, Revisited (The Packaging Sequel)

From Chapter 1 you chose static by default. Packaging (`S827`) is where that choice gets its reward:

- **Static:** one executable — VAML + backend + your app, all inside. Ship exactly **one file**; no runtime
  search paths, no version-skew between "the VAML you built with" and "the VAML on the machine." This is the
  v1 release posture, and it's the IP-hiding posture too.
- **Shared:** separate `vaml` runtime libraries + your exe. Install rules (`S827`) place them correctly.
  Worth it only for plugin/hot-reload architectures.

> ⚠ **Warning:** shared + "I forgot to ship the runtime" is how desktop apps get their "works on my machine"
> reputation. If you didn't *choose* shared for a reason, you chose static — and static has nothing to
> forget.

## 7.6 The Release Checklist (S824/S847 Flow)

1. **Build both CI platforms green** (`S924`) — not "mine." Both.
2. **`vamlcc` clean** on the markup — no diagnostics.
3. **Single-exe packaging confirmed** (one artifact) — or the shared libs installed per `S827` rules.
4. **Regression suite green** against the release candidate (`S925`).
5. **Version-tag with v1.0** and document the v1 control set (`S920`).

> 💡 **Tip:** the checklist is deliberately short and in dependency order. If you only do three, do 1, 2, and 3
> — the "does it build on both, is the markup clean, and does it ship as one file" trio catches the top of
> the mountain of launch-week crimes.

## 7.7 Troubleshooting, the "Why Won't My Build / Binary Behave" Table

| You see | Cause | Fix |
|---|---|---|
| "Unknown property" compile error | typo vs the public facade | check `S921` facade names |
| Interpreted ≠ compiled behavior | a path-only quirk | file as `S122/S123` invariant gap; use compiled for release |
| Giant binary | compiled markup inflates? | measure; static single-exe is expected; strip if needed |
| Handler not wired | signature mismatch | match `vaml::routed_event_args const&` (Ch.5) |

## 7.8 In This Chapter, You Learned…

- `vamlcc` turns `.vaml` into C++ at build; **errors are compile-time and author-friendly** (`S826`).
- Two paths — compiled (release) and interpreted (dev/hot-reload) — **validated to agree**.
- The `InitializeComponent` analog is `vaml::markup::load(...)`; one swap, whole story resolved.
- **Static single-exe** is the v1 release posture; shared is for plugins/hot-reload only.
