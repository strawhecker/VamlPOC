# VAML For Dummies® — Authoring a VAML App in C++

> **The C# → VAML C++ application-code gap: the part of VAML you can't Google.**
>
> You already know C++. You already know C# and WPF. This book is for the one thing nobody writes down:
> **exactly what to type when the thing you used to type in C# now has to be C++ in VAML.**

---

## About This Book

Welcome, friend. You've built WPF apps. You know your way around `INotifyPropertyChanged`, `ICommand`,
and a `DataContext` like the back of your hand. Then someone said "we're writing it in VAML now," and you
discovered that — surprise — all the **mental models** you relied on have quietly changed.

- The garbage collector is gone. **You** own objects now.
- Properties are **values**, not references.
- `async`/`await` doesn't exist in the UI model. You get **continuations** instead.
- There's more than one thread, and the render thread is *not* the UI thread.

> ⚙ **Technical Stuff — how "rendering" differs from what you're used to.** VAML's renderer is a **Vulkan
> quad/texture renderer**, not a DirectX-style retained geometry + immediate-mode shape engine. Everything
> on screen is a textured quad sampled from a texture atlas — even text is rasterized into the atlas once,
> then drawn as quads. The practical upshot for you (the author):
>
> - **Icons are simple now.** An icon is just a raster you slot into the atlas (`PNG`, or `SVG` pre-rasterized
>   once) and draw as a quad — nothing about it needs a "vector" path. Monochrome icons are tinted at sample
>   in the shader; the `DrawIcon`/`Icon` control handles it. See `Icon` in Chapter 6.
> - **...but Vulkan is *not*.** "Vulkan" does not mean "your DirectX habits carry over." You never write
>   shaders, buffers, or pipelines in a VAML app — but when you leave the quad world (raw GPU work,
>   post-1.0), the mental model is explicitly *not* DirectX. For 99% of app authoring you stay inside
>   textured quads + SDF text and never touch any of it.

This book does **not** teach you C++ (plenty of excellent books do that). It does **not** teach you VAML's
markup from zero (the markup is a loyal clone of XAML, so it'd be insulting to treat you as a beginner).
It teaches the one open gap: **the C++ you type instead of the C# you used to type**, and the handful of
places where the difference isn't syntax — it's a whole new way of thinking.

Think of it as the ultimate "quick on-ramp": a C#/WPF dev's cheat code for VAML in C++. We keep the jokes.
You keep your sanity.

## Foolish Assumptions

We're going to do you the courtesy of assuming you're *not* a fool — you just look like one the first time
(and only the first time) you hit a dangle pointer in a `DataContext`. Specifically, we assume:

- You're comfortable **writing and reading C++** — you know headers vs sources, `unique_ptr` vs `shared_ptr`
  vs raw, and you've never once on purpose returned a reference to a local.
- You've **shipped a WPF or otherwise XAML-based app** — you know MVVM, bindings, commands, styles, and a
  `ControlTemplate` from `TemplateBinding`.
- You know CMake well enough to add a target and a dependency, or you're ready to copy-paste ours.
- You have **never written a VAML app** — that's the whole point of this book, and we promise you're not
  late to the party, you're early.

If you tick all four, put your feet up. Everything else — no, really, *everything else* — we cover.

## How This Book Is Organized

This book is arranged the way the For Dummies series likes it: **little by little, part by part, each piece
building on the last**, with icons to tell you when we're nudging your memory or flashing a warning light.

### Part I: Getting Started with VAML — *the "Hello, world" with extra steps*

- **Chapter 1 — Hello, VAML!** Bootstrap a consumer app from scratch. Static vs shared, the four parts of
  a VAML app, and your first window. (This is our free sample — try before you buy.)

### Part II: The Model-View-Whatever — *the heart of the book*

- **Chapter 3 — MVVM Core in C++: The Part That Bites** — `ObservableObject`, `RelayCommand`, and above
  all **ownership**: the garbage collector is on vacation and you're the new lifeguard.
- **Chapter 4 — Bindings & Commands: Making the UI and the Model Date** — PropertyPaths, modes, converters
  (which are *callables*, not classes), and the two-way tango.
- **Chapter 2 — Markup: The Dialect You Actually Speak** (yes, we deliberately slipped it here, in its
  natural home — markup lives with what it drives).

### Part III: Making It Respond

- **Chapter 5 — Events & Input: When the User Pokes the Screen** — wiring `Click="on_click"`, routed
  events, and the big one: **async without `await`**.
- **Chapter 6 — Layout, Styles, and Templates: Dressing Up the Same Old Controls** — the shipped control
  set, making it pretty, and re-shaping a control from the inside.

### Part IV: The Grown-Up Stuff

- **Chapter 7 — Compiled Path & Release: From `.vaml` to a Single Binary** — `vamlcc`, the
  `InitializeComponent` analog, and shipping.
- **Chapter 8 — Threading: There's More Than One Thread, and They Don't Talk Much** — render vs UI vs
  layout threads, marshalling, and why lifetime is *your* problem now.

### Part V: Putting It All Together

- **Chapter 9 — The Order Tracker: A Whole App, Start to Finish** — one worked example, C# on the left,
  VAML C++ on the right, side by side. The payoff.

### The Part of Tens — and the Cheat Sheet

- **Appendix A — Glossary: C#/XAML → VAML C++** (the ten things that changed, and the ten that didn't).
- **Appendix B — The Cheat Sheet & Ten Common Mistakes** — one screen to pin next to your monitor, and a
  tense list of the bugs every tourist hits on day one (so you don't have to).

## Icons Used in This Book

| Icon | Means |
|---|---|
| 💡 **Tip** | A faster, cuter, or safer way to do something. We're doing you a solid. |
| ◇ **Remember** | A point worth re-stating because it's the one you'll forget at 2 a.m. |
| ⚠ **Warning** | A genuine "don't do this" — usually a dangle pointer or a wrong-thread write. Heed it. |
| ⚙ **Technical Stuff** | The "under the hood" detail that makes you sound smart at lunch, skippable if you're in a hurry. |

## Where to Go from Here

The book is written to be read in order, but we know how these things go. If you've already got a VAML app
on screen from Chapter 1, skip ahead and come back; each chapter names the roadmap step that proves its
claims (so a curious reader — that's you — can check the receipts).

New to it all? Start with Chapter 1. Have a "why does my binding show nothing" emergency? That's a
Chapter 3 / Chapter 4 problem. Seeing thread errors? Chapter 8. In every case: read the ⚠ warnings first —
they're the ones that cost you a Tuesday.

And remember the single rule that unlocks everything else:

> **Set to notify. Own what you create. Continuations, not `await`. Errors at compile time.**
> More on all four in Chapter 3, 5, 7, and 8 — and one more time, terser, in Appendix B.

# About the Author

A long-suffering C#/WPF developer who one day agreed to port their hard-won MVVM instincts to a C++ GUI
stack and decided the world deserved the notes. When not writing `vaml::property<T>` declarations, they can
be found explaining to people that "the render thread and the UI thread are different threads," usually
while their colleagues nod politely and write a test for it anyway.

---

**A note on this repo:** this guide is authored under **Sprint 18** (`docs/roadmap.md`, tokens `S940`–`S953`).
It's written from real production evidence, never from spec; no chapter cites a feature that isn't
implemented. The guide CI gate (**S951**) extracts and compiles every code block in this book. Shipping and
indexing are **S952**; the docs-only refactor gate and close-out are **S953**.

> "VAML For Dummies" reflects the series' tone and structure as a homage. This is original authoring for
> this project's own user guide; it is not affiliated with or endorsed by the For Dummies series or its
> publisher.
