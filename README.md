# VamlPOC

**VAML** — a markup language and rendering engine for declarative user interfaces, built on C++ + Vulkan.
"VAML, not WPF in C++." VAML is its own thing: a fast, primitive, convention-following markup-to-renderer
library, not a re-implementation of any one framework's runtime.

> Working title: this repository was historically the "POC." Per ADR-017 it is **v1 built at v2 quality**
> — a real production first version, not a throwaway to be ported. The plan of record is `docs/roadmap.md`
> (authoritative token list); completion is tracked in `progress.md`; per-run state in `session-context.md`.

## The identity: what VAML is, and is not

VAML is a **declarative UI language + a renderer + a binding/event primitive layer**. It is NOT a framework
that imposes an application architecture on you.

- **VAML does not impose DI or pub/sub.** No matter how invaluable dependency injection or message
  buses (event aggregators, messengers) are to commercial software, they are *application-structure*
  concerns — the architect's job, not a markup library's imposition. VAML does **not** use them internally
  (so offering them would only add speculative coupling and indirection that costs performance), and VAML
  does **not** offer or own one.
- **We do not offer a ViewModelLocator, a DI container, a CompositionRoot type, or an EventAggregator.**
  Resolution-by-name and container-driven wiring are outside our lane. The engineer wires their app; we
  stay in ours. If you want these, use whatever fits — VAML supports *consumer-side interop* with them,
  but we do not build them for you.
- **Please note:** the WPF framework contains DI + pub/sub tooling, and the popular WPF/MVVM packages
  (CommunityToolkit, Prism) bundle them. We are not those frameworks; we do not point you at "our selected
  outcome." You decide what is best for your project.

## The layering (onboarding bridges, not abstraction tiers)

VAML's include layers are **onboarding ramps for existing projects that eat our dog food** — matched to the
C#/WPF/MVVM packages those projects already know (Toolkit, Prism) — so their existing ViewModel patterns keep
working. They are convenience *over* VAML's one binding spine; none of them swaps the wiring model out.

- **`vaml/core`** — the base: the VAML parser, the renderer, and the core `Binding` + `Event` primitives.
  Data-flow only; no structure imposed. Programmer effort: high (you wire the backend yourself and link it
  to the UI).
- **`vaml/mvvm`** — the Toolkit-analog convenience layer (ships when the S6 binding engine lands): a
  `ViewModel`/`ObservableObject`-style base, a `Command`/`RelayCommand`, and bindable-property generation
  (`PROPERTY_BINDABLE(type, Name)`-style) that automates the observable boilerplate. No DI, no reflection —
  source-generation spirit, plain fast C++. Programmer effort: medium.
- **A thin user-glue slot (deferred / maybe-never)** — a service-registration seam the *engineer* plugs
  their own container into, if they want one. We provide the slot at most; we never own or resolve.

**The load-bearing line: VAML's binding is the spine.** The engineer does the wiring and follows VAML's way
of doing it — it is the only way we offer. Any optional layer is a convenience *over* that binding; anything
that would replace the wiring model (a ViewModelLocator's name-resolution, a DI container) is explicitly
outside our lane. Industrious individuals may build either on top — we support them, we don't build them.

## The MAUI story (same consideration, not Prism's MAUI)

Existing MAUI projects should get the same onboarding consideration as WPF ones. We **mirror the
source-generated-observable spirit of CommunityToolkit.Mvvm** (fast, honest, no indirection — aligned with
VAML's goal) and we **reject MAUIProgram's built-in service container / Prism-for-MAUI's container
abstraction + ViewModelLocator** (the anti-VAML direction). Concretely: the same binding-convention bridge
over the VAML C API + a documented C#→C++/CLI-or-FFI path so a MAUI ViewModel can ride VAML — **no
container we own, no locator.**

## Status

- Rendered UI is produced by `vaml_viewer` (S220+), the single official consumer app that replaces the
  throwaway POCs. It is preview-only (pan/zoom on the rendered view); there is no embedded source editor —
  edit in any external editor and the viewer live-reloads (S222_1). It shows the declared UI live as each
  sprint ships. vaml_viewer is also the **in-repo v1 vertical-slice deliverable** (S11): its own canonical
  app-shell containing all it can, so the library is pre-validated by a full consumer before any external use.
- **V1 APP (external); ACT; then RESUME here on acceptance.**
- NO-GAP / NO-LAZY-LLM / MINIMAL REAL governance and the full step list: `docs/roadmap.md`.
