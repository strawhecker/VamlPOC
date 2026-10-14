# VAML ROADMAP — S000..S999 (authoritative tokenized step list)

> This is THE step enumeration for VAML. It was originally framed as a "POC," but it stopped being one
> before S001 — per the standing decision (ADR-017), this is **v1 built at v2 quality** ("version 2 as our
> first version"): a real production first version, not a throwaway to be ported. `progress.md` (repo root)
> is the continuation file that tracks completion. This file is the source of WHAT to do; each step has a token
> `S###`, a dependency (`dep:`), and a one-line description. The "no-stub" rule applies: a step
> is only ✅ when its behavior is real and verified by build + tests.
>
> Sprint headers list the sprint's goal. Token ranges reserve room; gaps mean reserved headroom
> for steps discovered mid-execution (append new steps into the nearest open token, then renumber
> the tail in one pass — do NOT edit the checkpoint by hand).

## GUARD RAILS (operative project contracts — codified from S003 forward)
### MINIMAL REAL (official term)
- A small, **permanent**, linkable public symbol that seeds a component's true interface (e.g. a
  version marker). Distinct from a **STUB**: a STUB is throwaway scaffold deleted later; MINIMAL
  REAL is **kept and extended** into the real implementation. It is NOT lazy — it is a deliberate,
  declared, forward-resolvable seed.
- Every MINIMAL REAL symbol carries a **searchable sentinel comment**:
  `// [MINIMAL REAL] <component> — RESOLVED BY <sprint-token>. Do NOT extend silently; do NOT backfill.`
- A MINIMAL REAL symbol may ONLY be removed or replaced by the step that **explicitly declares**
  its resolution.
- Active MINIMAL REAL marks (searchable): `vaml-vulkan` version marker → **RESOLVED BY S161**;
  `vaml-tools` version marker → **RESOLVED BY S287**; `Color` (S030) → **RESOLVED BY S089**;
  `Thickness` (S030) → **RESOLVED BY S089**; built-in element seeds (S087, in `vaml/core/Element.h`)
  → each resolved by its OWN control/layout sprint: `Button` → **S401**, `StackPanel` → **S129**,
  `Grid` → **S131**, `Text` → **S400**, `TextBox` → **S402**, `CheckBox` → **S404**,
  `Slider` → **S403**, `ScrollViewer` → **S405**, `Window` → **S440**.

### NO-GAP GUARANTEE (official term)
- Binding project contract: **no feature or symbol may be left behind for a silent backfill step.**
  Anything introduced as MINIMAL REAL must have an explicit, scheduled resolution point, recorded
  in the roadmap AND progress tracking, authored at the SAME step that delivers the real content.
- Any future step that uses MINIMAL REAL carries an obligation: it must be **searchable and
  resolvable** by its named sprint. Resolution is part of the real feature's own step, never a
  separate cleanup/backfill step.
- Forward-declared resolutions (contracts): **S161 must resolve the vulkan MINIMAL REAL marker;
  S287 must resolve the tools MINIMAL REAL marker. S089 must resolve the Color + Thickness
  MINIMAL REAL markers (introduced by S030).**

### NO LAZY LLM SYNDROME (official term)
- The project's default reasoning posture. When in doubt, **reason the opposite of lazy LLM**:
  prefer the explicit, the searchable, the contractually-bound, and the non-silent option.
- **MINIMAL REAL is explicitly NOT lazy-LLM** — it is a strategic choice made to keep resolution
  steps coherent **by design**, justified and recorded in this documentation, never a shortcut
  taken for convenience.

## Conventions
- `dep:S00X` = this step requires S00X complete first. Multiple deps separated by `+`.
- `dep:-` = no dependency (root step of its sprint).
- Gates: each sprint ends with a `REFACTOR GATE` step (force `-G` in the token's line) that removes
  dead code and re-verifies full build (MSVC + Clang) + ctest.
- Layer rule: `src/core` NEVER depends on Vulkan/GLFW; only `src/vulkan` may. Enforced by CMake.
- Distribution: consumer chooses shared/static; default static. [ADR-003]

---
## Sprint 0 — Repo, toolchain, CI, test harness   (goal: green skeleton, no feature code)
- S000 dep:-  Repo skeleton + tracking files (progress.md, ROADMAP.md, ADR seeds, architecture.md). [DONE]
- S001 dep:S000  Top-level CMake (project(VAML), C++ latest, /EHsc /permissive- MSVC, -Wall -Wextra -Wpedantic Clang, Werror on). [DONE — verified MSVC 19.51 via preset-driven verify scripts (S008 removed the superseded build/verify_msvc.cmd)]
- S002 dep:S001  cmake/ compiler presets + toolchain files (MSVC VS 2026, Linux Clang).
- S003 dep:S002  Static-library artifact model: VAML_BUILD_SHARED (default OFF); vaml-core, vaml-vulkan, vaml-tools targets. [MINIMAL REAL: vulkan→S161, tools→S287]
- S004 dep:S003  CI matrices (.github/workflows): windows-msvc + ubuntu-clang, configure+build+ctest green. [DONE — .github/workflows/ci.yml: rolling windows-latest (VS 2026)/ubuntu-latest, reuses msvc-debug + clang-debug presets]
- S005 dep:S003  Vendor doctest single-header into third_party; test target + smoke test (0-feature) passes. [DONE — third_party/doctest v2.4.11 single header; vaml-core-tests exec links vaml-core; smoke_test.cpp; CTest PASS on MSVC (VS 2026) + Clang (Ubuntu VM)]
- S006 dep:S005  Port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h (namespace vaml::test): strip MSTest refs, __forceinline, Windows bits; pluggable failure sink (throw TestFailureException). All predicate names preserved. [DONE — S006 is a no-build-by-contract step; compile gate belongs to S007]
- S007 dep:S006  Shouldly.h compiles under MSVC + Clang; assertion round-trip tests. [DONE — test/unit/shouldly_test.cpp exercises every predicate pass+fail; fixed ShouldBeNullOrEmpty to "null OR empty" bug; portability fixes in header (exception ctor, ignored-qualifiers, std::fpclassify/FP_ZERO/isnormal); verified MSVC + real Linux Clang, ctest green]
- S008 dep:S004  REFACTOR GATE(S0): no dead code, headers internal-only, static-link default verified, clean PR. [DONE — removed superseded build/verify_msvc.cmd; added include-surface convention + static-model STATUS log to cmake/artifact-model.cmake; MSVC + Linux Clang build + ctest green]
- S009 dep:S008  SURVIVOR block written to progress.md for Sprint 0. [DONE — consolidated SPRINT 0 SURVIVOR block + regenerated checkpoint (next = S030); docs-only, no build]

---
## Sprint 1 — Property system core  (goal: full DP semantics; the heart)  [token S030+]
- S030 dep:S009  Typeless Value: bool/int32/int64/float/double/string/Color/Thickness/Brush-id/untyped-ptr/Null + type tag + type-safe get(). [DONE — src/core/include/vaml/core/{Color,Thickness,Value}.h + src/core/src/Value.cpp; factories map to 11 tags; get<T> type-safe; equality + copy/move; msvc-debug build + ctest green]
- S031 dep:S030  Value round-trip + type-mismatch-throws tests. [DONE — test/unit/Value_roundtrip_test.cpp: every non-Null payload round-trips through get<T> exactly + re-factory==original; Null + every wrong-type get throws ValueTypeMismatch; BrushId reads as int32_t only; mismatch never corrupts state (valid re-read still succeeds); msvc-debug + clang-debug ctest green]
- S032 dep:S031  PropertyKey (name, type, owner-type id, flags) + PropertyRegistry (name→key, dedupe detection). [DONE — src/core/include/vaml/core/{PropertyFlags,PropertyKey,PropertyRegistry}.h + src/core/src/{PropertyFlags,PropertyKey,PropertyRegistry}.cpp + test/unit/PropertyKeyRegistry_test.cpp; real public symbols (no MINIMAL REAL); flags = inherits|readonly|bindsTwoWayByDefault; owner-id = string type-name; registry instance-based pimpl, name-unique + dedupe-throws; msvc-debug + clang-debug ctest green]
- S033 dep:S032  Registry: register/lookup/duplicate-throws tests; constexpr-safe static id. [DONE — src/core/include/vaml/core/PropertyId.h (header-only constexpr-safe name-token id: of(), name(), valid(), ==/!=, std::hash) + PropertyRegistry id_of/try_id bridge + test/unit/{PropertyId,PropertyRegistryExhaustive}_test.cpp (static_assert compile-time proof + exhaustive bulk register/lookup tr/dedupe-throws-unchanged + missing-lookup + move semantics); global static registry instance deferred to S034 (no consumer yet — recorded, not silent); msvc-debug + clang-debug ctest green]
- S034 dep:S033  PropertyMetadata { defaultValue; flags(inherits|readonly|bindsTwoWayByDefault); changed cb; coerce; validate }. [DONE — src/core/include/vaml/core/PropertyMetadata.h + src/core/src/PropertyMetadata.cpp + test/unit/PropertyMetadata_test.cpp; move-only metadata bundle under pimpl: WithDefault factory, && builder hooks (with_coerce/with_validate/with_changed) return-by-value, passthrough when absent, type-bound callbacks throw ValueTypeMismatch on wrong tag; global static registry + Register landing site deferred to S036 (recorded, not silent); msvc-debug + clang-debug ctest green]
- S035 dep:S034  Metadata tests: default applied, cb fires only on real change, coerce applied, validate rejects. [DONE — test/unit/PropertyMetadata_semantics_test.cpp: doctest-scope MiniStore harness models the Set-time apply pipeline (default applied at construction; coerce→validate(coerced)→fire-on-real-change→store); verifies all four title behaviors + coerce-before-validate ordering + passthrough + type-bound throw. Harness is test infra (no product symbol, no MINIMAL REAL sentinel) and is the reference SPEC for S036's DependencyObject::Set — S036 must reproduce these exact outcomes. msvc-debug build + ctest green]
- S036 dep:S035  DependencyObject base: local value store, Get/Set, per-prop mastery, thread-affinity marker. [DONE — src/core/include/vaml/core/DependencyObject.h + src/core/src/DependencyObject.cpp + test/unit/DependencyObject_test.cpp + build/verify_s036.cmd; object-scoped registration: register_property(PropertyKey, PropertyMetadata) installs (metadata, defaultValue) keyed by owned NAME, dedupe-on-name throws PropertyAlreadyRegistered-unchanged; local-only store (absent entry = "default applied", Get by value falls back to defaultValue()); Set reproduces the S035 pipeline verbatim — coerce → validate(on coerced) → fire changed cb ONLY on a real change → store — returning APPLIED/REJECTED/NO_CHANGE, unregistered id throws PropertyNotFound, type-bound violated tag throws ValueTypeMismatch; per-prop mastery + dedupe + Get-by-value + thread-affinity marker (thread_affinity_id/is_affine_to_current_thread, created-thread recorded; wrong-thread QUEUING deferred to S037) all tested. GOTCHA: internal store keys on owned std::string, NOT PropertyId — a PropertyId is a non-owning string_view that dangles once the registering key dies. msvc-debug configure+build+ctest green (100% tests passed).]
- S036_1 dep:S036  Shouldly routing reform: Shouldly.h predicates terminate through doctest's reactor (doctest-aware macro seam `SHOULD`/`SHOULD_REQUIRE`; severity explicit); shouldly_test.cpp rewritten to assert the new routing contract in the SAME step (old throw-contract is invalidated — deferring leaves an unbuildable window). Gate: full suite green, feature files still `CHECK` (untouched). [adopts re-formulated Shouldly as the assertion VOCABULARY; doctest stays the RUNNER] [DONE — SHOULD/SHOULD_REQUIRE macros in Shouldly.h (gated #ifdef DOCTEST_LIBRARY_INCLUDED so core stays doctest-free); shouldly_test.cpp rewritten to the routing contract in the same step; msvc-debug green]
- S036_2 dep:S036_1  StringMaker domain exposure: framework-native `StringMaker<vaml::Value>`, `<SetResult>`, `<PropertyId>` (and core types surfaced in asserts) so raw stringified fallback is human-readable. Pure additions, no macro surgery. Gate: build+ctest green. [DONE — test/unit/vaml_test_stringmakers.h + StringMaker_test.cpp; doctest::StringMaker for Value/SetResult/PropertyId; msvc-debug green]
- S036_3 dep:S036_2  `ShouldThrow<Ex>` typed-exception assertion category: reports actual thrown type + message on wrong-type failure (nothing-thrown / wrong-type / right-type + optional message matcher). EXACTLY replaces `CHECK_THROWS_AS`/`_WITH`/`_WITH_AS` (~40 sites) — a first-class category, NOT lazy-avoidance (NO-LAZY-LLM/NO-GAP). Resolves the typed-assertion category at its own step. [DONE — ShouldThrow<Ex> 3 overloads in Shouldly.h (if constexpr handles Ex==std::exception to avoid duplicate-handler C2312); 6 ShouldThrow test cases; msvc-debug green]
- S036_4 dep:S036_3  Migrate Value / roundtrip / PropertyId / PropertyKeyRegistry / PropertyRegistryExhaustive / PropertyMetadata feature tests: equality `CHECK`s → `SHOULD( Shouldly…ShouldBe/ShouldBeGreaterThan/ShouldBeNull/ShouldBeInRangeTo )`; typed-exception → `ShouldThrow<Ex>`. `smoke_test.cpp` (doctest runner/main) NOT migrated. Verify green per file. [DONE — 6 feature files rewritten full-file to the Shouldly vocabulary; test-side operator<< added to vaml_test_stringmakers.h (ACCEPTED blocker resolution: Shouldly's stream formatter needs operator<< which the domain types lacked); 3 sanctioned REQUIRE precondition guards kept; msvc-debug green per file]
- S036_5 dep:S036_4  Migrate `DependencyObject_test.cpp` (S036 proof) as the capstone template + final gate: Value/SetResult equality → Shouldly, typed-exception → `ShouldThrow`; thread-affinity + apply-pipeline assertions read as the S035 spec. Final: full suite green (both compilers where feasible), no `CHECK_THROWS_AS` remains, doctest is runner/mechanism only. Annotates S047 ("every numbered predicate via adapted Shouldly") as realized early by these steps. [DONE — test/unit/DependencyObject_test.cpp rewritten to SHOULD(ShouldlyString(...))/.ShouldThrow<Ex>; SetResult/Value/tag/thread-affinity asserts all in the Shouldly vocabulary; msvc-debug configure+build+ctest green (100% tests passed). GATE met: zero CHECK*/CHECK_THROWS_AS across all 7 migrated feature files (Value_test, Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test, PropertyMetadata_test, DependencyObject_test). Sanctioned exception, Recorded: `PropertyMetadata_semantics_test.cpp` (S035 reference SPEC harness) is intentionally NOT migrated — it IS the spec S036 must satisfy, and rewriting it would alter the very contract under test; its single CHECK_THROWS_AS stays. Other sanctioned non-migrated asserts: smoke_test.cpp (harness), StringMaker_test.cpp (asserts the raw doctest::String output of the S036_2 StringMaker itself — not a vaml domain assertion), and 3 REQUIRE precondition guards (null-deref gates). S047's "every numbered predicate via adapted Shouldly" is now the standing vocabulary from S036_5 onward.]
- S037 dep:S036_5  Set/Get local values; wrong-thread write queued-not-applied tests (marshalling foundation). [DONE — src/core/include/vaml/core/DependencyObject.h + src/core/src/DependencyObject.cpp + test/unit/DependencyObject_marshalling_test.cpp + test/unit/vaml_test_stringmakers.h + test/unit/CMakeLists.txt + build/verify_s037.cmd; CONSUMES S036's thread-affinity marker (thread_affinity_id / is_affine_to_current_thread) as the basis for wrong-thread detection; S036 records affinity only, S037 adds actual wrong-thread write QUEUING (queued-not-applied). Set() is now affinity-aware: on the AFFINE thread it is exactly the S035 pipeline (APPLIED/REJECTED/NO_CHANGE, unchanged); on a NON-affine thread, coerce+validate run AT QUEUE TIME (instant writer feedback: REJECTED on refusal, ValueTypeMismatch on wrong tag) but a passing value is POSTED to a mutex-protected per-object pending queue and Set() returns new SetResult::QUEUED — store NOT written, changed cb NOT fired. ProcessPendingChanges() (affinity-thread-only; throws new WrongThreadException from a non-affine thread, NO-LAZY) drains the queue FIFO through the S036 store half, so queued [5,7] on a default-0 prop fires cb(0->5) then cb(5->7); it swaps the queue out under the lock so writers during a drain enqueue into a fresh queue. pending_change_count() exposes queued-not-applied depth. Cross-thread contract (recorded): the only truly cross-thread datum is the mutex-protected queue; the value store stays affinity-thread-owned by construction (only the affinity thread stores, via on-affine Set or drain); Get() is safe on the affinity thread or with no concurrent affinity-thread mutation in flight; full store-race freedom is TSan/S620/S723 territory (recorded, not silent). S041 generalizes the per-object queue into the per-thread ChangeQueue + notification processor (S037's OWN resolution point — NO backfill). SetResult enum is backward-compatible (QUEUED appended, three originals unshifted). msvc-debug + ubuntu-clang BOTH green (100% tests passed).]
- S038 dep:S037  ChangeRecorder: pending per-property changes for deferred notification. [DONE — src/core/include/vaml/core/ChangeRecorder.h + src/core/src/ChangeRecorder.cpp + test/unit/ChangeRecorder_test.cpp + build/verify_s038.cmd; standalone vaml::ChangeRecorder (pimpl'd, move-only, NOT MINIMAL REAL / NOT a stub) holds pending per-property notifications for deferred flush. record(id,old,new) captures (old,new) per DISTINCT property; a repeat for the same property COALESCES — FIRST old kept, LATEST new adopted (one notification per property) — realizing S042's outcome EARLY (annotated on S042). flush(NotifyFn) emits one notify(id,old,new) per pending property in FIRST-RECORD order (insertion-ordered vector+name-map, never unordered_map iteration), then clears, and returns the emit count; it is snapshot-then-emit (a record during flush lands in fresh state, mirroring S037's swap-out). clear() drops all pending. Deliberately SINGLE-THREADED (a pipeline-internal deferral device) — the cross-thread ChangeQueue/mutex is S041's job (recorded, not silent), so S038 stays out of S041's scope and does NOT rewire DependencyObject's synchronous callbacks (would break the S037 marshalling tests). Owned-name gotcha honored (S036/S037): records key on an owned std::string copy of PropertyId::name(), never the non-owning string_view. NOTE: this type is S041's deferred-notification building block (recorded NO-GAP — S041 consumes it; no silent backfill); S042's coalescing outcome is REALIZED-EARLY here per the S036_1..S036_5 → S047 precedent. msvc-debug + ubuntu-clang BOTH green (100% tests passed).]
- S039 dep:S038  Effective-value resolution: local ?? inherited-from-parent ?? default; inherit-only flags. [DONE — src/core/include/vaml/core/DependencyProperty.h + src/core/src/DependencyProperty.cpp + test/unit/{DependencyProperty,EffectiveValue}_test.cpp + build/verify_s039.cmd; AUTHORS the S036 re-scope global static registry + DependencyProperty::Register landing site (the S034-recorded, previously "S036's concern") — the first real consumer of a global property table. vaml::DependencyProperty (pimpl'd, NOT MINIMAL REAL / NOT a stub): `Register(name,type,owner,metadata)` inserts a PropertyKey into the new `PropertyRegistry::global()` (Meyers static, name-unique -> PropertyAlreadyRegistered unchanged) + a companion static table holding the full (key,metadata) incl. flags+default; `TryGet`/`Get` (null / PropertyNotFound); `property_id()/metadata()/flags()`. `DependencyObject::GetEffectiveValue(PropertyId)` — read-side pull resolution of `local ?? inherited-from-parent ?? default`, with new `PropertyFlags::InheritOnly` (own local override does NOT contribute; value only from the ancestor chain). `Get` delegates to GetEffectiveValue (backward compatible — no existing test uses Inherits/InheritOnly). Inheritance descriptor sourced from the GLOBAL DependencyProperty; local value from the object's per-object store. Added single non-owning `set_parent()/parent()` link (read-side seed, NO-GAP: S040 cascade + S120 visual tree consume it). Set/ProcessPendingChanges UNTOUCHED (read-side only; write/cascade = S040, recorded NOT silent). msvc-debug + ubuntu-clang BOTH green (100% tests passed).]
- S040 dep:S039  Inheritance cascade: parent change recomputes child effective value without child write (test). [DONE — test/unit/InheritanceCascade_test.cpp + build/verify_s040.cmd + test/unit/CMakeLists.txt; TEST-ONLY, zero new product symbols. Consumes the S039 parent-link seed + global DependencyProperty table and PROVES the S039-deferred write/cascade side is real via the read-side pull model (GetEffectiveValue walks the ancestor chain fresh on each read). New dimensions vs S039: dynamic ordering (parent change AFTER attach) + local-vs-inherited interaction (child Set vs inherited old). 6 TEST_CASEs: cascade-after-attach; multi-level (great-grandparent change recomputes great-grandchild); local-wins (child local override not swept by parent change); InheritOnly recompute (own local ignored, parent change recomputes); child-Set-vs-inherited-old (later parent change never clobbers a child write); parent-reverts (child follows both moves). NO-GAP: push notification of descendants is explicitly S041's scope (recorded, NOT silent) — S040 does NOT build descendant tracking. No MINIMAL REAL introduced/resolved. msvc-debug + ubuntu-clang BOTH green (100% tests passed).]
- S041 dep:S040  Change notification pipeline: ChangeQueue (per-thread) + processor drains in registration order. [DONE — src/core/include/vaml/core/{ChangeQueue,ChangeProcessor}.h + src/core/src/{ChangeQueue,ChangeProcessor}.cpp + test/unit/{ChangeQueue,ChangeNotificationPipeline}_test.cpp + build/verify_s041.cmd + MOD src/core/{DependencyObject.h,cpp,CMakeLists.txt} + test/unit/CMakeLists.txt. Delivers the S041 headline as three REAL public symbols (none MINIMAL REAL / STUB): (1) vaml::ChangeQueue — per-thread pending NOTIFICATION queue (S037's per-object value-queue generalized onto the notification axis — S037's OWN resolution point, NO backfill): cross-thread enqueue/pending_count (mutex), affinity-thread-only process/clear (WrongThreadException from a non-affine thread, mirroring S037 drain), FIFO swap-out (record-during-process lands in a fresh queue), owned-name gotcha honored. (2) vaml::ChangeProcessor — "drains in registration order": owns a ChangeQueue + consumes S038's ChangeRecorder INSIDE the drain so two enqueues of the same property before a drain collapse to ONE notification (first old kept, latest new adopted — S042 realized end-to-end). (3) DependencyObject gains a registration-ordered subscriber registry (add_property_changed(PropertyId,handler)->token / remove_property_changed(token)) fired AT APPLY TIME only (additive dispatch; S037 SetResult + the existing metadata with_changed cb + marshalling tests stay green) + per-object notification queue (ProcessNotificationQueue, affinity-thread-only, coalescing at drain) + the S040-recorded DESCENDANT PUSH delivered here NOT silent. DESCENDANT PUSH: set_parent() now ALSO registers the child into the parent's non-owning inheritance-scope (the INVERSE edge of the S039 parent link); on an applied Inherits/InheritOnly change (on-affine Set or drain-applied queued write) the two-phase pull model captures each registering descendant's effective value BEFORE the store and notifies AFTER, so each descendant receives its OWN effective-old->effective-new delta and is notified ONLY when it actually changes (S040 local-wins respected THROUGH an intermediate — a local-wins node shields its whole subtree from the parent). No visual tree: S121 later adds the visual children list (create/attach/detach + walk), a DISTINCT structure that composes with set_parent — recorded, no overlap, no gap. Non-breaking guarantees verified by the existing S036/S037/S038/S039/S040 tests staying green. msvc-debug + ubuntu-clang BOTH green (146 test cases pass).]
- S042 dep:S041  Coalescing: two sets of same prop before drain == one notification (test). [DONE — test/unit/Coalescing_test.cpp + build/verify_s042.cmd. TEST-ONLY, zero new product symbols. The coalescing PRIMITIVE was REALIZED-EARLY by S038 (ChangeRecorder coalesces per-property by design — first old kept, latest new adopted — tested in ChangeRecorder_test.cpp); S041 wired it into ChangeProcessor + the live-object per-object notification queue + ProcessNotificationQueue. S042 is the FORMAL LIVE-OBJECT integration gate it names: two Set() calls on an Inherits property against live DependencyObjects, drained once, collapse to EXACTLY ONE subscriber notification (first old kept, latest new adopted). This was the one remaining gap — S041 proved coalescing only on the standalone ChangeProcessor and proved live descendant-push with a DRAIN BETWEEN Sets (two sequential notifications), never two live Sets before ONE drain. 6 TEST_CASEs: headline two-Sets-one-drain==1 notif (raw queue depth 2 proves coalescing at drain, then 1 notif old=0 new=8); three Sets→one notif (earliest old+newest new); drain-is-the-boundary (set→drain→set→drain = two sequential, no cross-drain collapse); distinct props do NOT coalesce with each other (two notifs in first-record order); NO_CHANGE Set does not inflate the queue; aggregate exactly-one-notification-per-property. msvc-debug + ubuntu-clang BOTH green (152 cases pass). No MINIMAL REAL introduced/resolved.]
- S043 dep:S042  Attached properties: key owned by A settable on B; registry supports owner≠class. [DONE — DependencyObject made ATTACHED-AWARE by GLOBAL RESOLUTION: Set()/Get()/GetEffectiveValue() resolve a globally-registered DependencyProperty even when the object has not locally registered it (S035/S037 pipeline + cross-thread contract preserved — a non-affine Set NEVER mutates the store; attached entry materialized only on the affinity thread / at drain); move-only PropertyMetadata NEVER copied into an object's entry (attached/borrowed PropEntry holds a NON-OWNING pointer to the process-lifetime global descriptor — metadata-provider design decision); lazy attached materialization on affine Set + at ProcessPendingChanges drain; DependencyProperty::owner_type_id() exposes the declaring owner (attached owner A vs storage class B); test/unit/AttachedProperty_test.cpp + build/verify_s043.cmd; msvc-debug + ubuntu-clang BOTH green. NO-GAP: S044 = the dedicated attached inheritance + notification test gate (recorded, NOT silent).]
- S044 dep:S043  Attached set/get + inheritance + notification tests. [DONE — test/unit/AttachedInheritanceNotification_test.cpp + MOD test/unit/CMakeLists.txt + build/verify_s044.cmd + MOD src/core/{DependencyObject.h,cpp}: 6 TEST_CASEs prove the attached property story end-to-end against live DependencyObjects — attached-inherits cascade (child effective follows each parent move on an owner-A attached Inherits prop never locally registered on parent/child); attached inherited + local-wins (a child override is not clobbered by a later parent change); PASSIVE attached-Inherits descendant IS notified on parent change (the S044 push-walk broadening: a purely-passive descendant reads via S043's global-resolved GetEffectiveValue but was previously never captured → never notified — S041 captured descendants only when they had a local store entry; S044 broadens collect_descendant_effectives/visit_scope to capture a descendant when the ORIGIN's own entry is BORROWED (owned==nullptr, the precise attached signal) AND the property carries the global Inherits flag, so passive attached descendants are notified / local-wins+shielded descendants still filter (old!=new) to zero); attached coalescing (two Sets before one drain == one notification, first-old-kept/latest-new-adopted on the attached axis); wrong-thread attached Inherits -> QUEUED -> affinity drain applies -> descendant ProcessNotificationQueue fires (old,new); owner != class preserved via owner_type_id() while plain objects store, local registration shadows the global path, unregistered names throw PropertyNotFound. PRODUCT change (NO-GAP — authored at the same step as the tests, not a silent backfill): DependencyObject.{h,cpp} thread an `captureAttachedInherits` flag into the S041 push walk (Set + ProcessPendingChanges both), computed from the origin's BORROWED entry + global Inherits flag; no new public symbol; existing S040/S041/S042/S043 tests stay green (object-scoped Inherits keeps S041 behavior because the origin OWNS its entry). msvc-debug + ubuntu-clang BOTH green (163 cases pass).]
- S045 dep:S044  ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored. [DONE — src/core/include/vaml/core/DependencyObject.h + src/core/src/DependencyObject.cpp (MOD: new `PropertyReadOnlyException : std::logic_error` + S045 Set() read-only guard + ProcessPendingChanges defensive guard) + test/unit/ReadOnlyAndValidation_test.cpp (NEW) + test/unit/CMakeLists.txt + build/verify_s045.cmd. Set() resolves the metadata provider, THEN throws PropertyReadOnlyException if the resolved property carries PropertyFlags::ReadOnly — the guard sits immediately after provider resolution and BEFORE coerce/validate/affinity, so a read-only refusal is STRUCTURAL + VALUE-INDEPENDENT (throws even when input == current/default, never REJECTED/NO_CHANGE) and fires on ANY thread (a wrong-thread read-only Set THROWS, it is never QUEUED to a drain; a refused read-only write never materializes a borrowed store entry, so Get still reads the default). ProcessPendingChanges() gains a DEFENSIVE apply-time guard: it resolves the provider before materializing and skips a queued write whose property is now read-only (covers the edge where a property became read-only between queue-time and drain). Read-only is EXTERNALLY immutable: any Set throws — an owner-internal write path is deliberately deferred + recorded (S120+ controls), not silently omitted. "ALL FLAGS HONORED" scoped honestly: Inherits (effective resolution) + InheritOnly + ReadOnly (throws) behave now; BindsTwoWayByDefault is carried through key + metadata flags with no runtime behavior — genuine two-way binding is S242's scope, recorded NO-GAP, not faked. Tests (Shouldly vocabulary, S045-unique global names): read-only Set throws + Get default + store unchanged; read-only throws even for value == current; read-only wrong-thread throws (never QUEUED, pending 0); globally-registered read-only attached property Set throws + Get works + no materialization; read-only throw precedes coerce/validate (neither runs); validate out-of-range REJECTS (store unchanged, no cb) + in-range APPLIED (cb fires); consolidated all-flags (ReadOnly|BindsTwoWayByDefault combined throws; Inherits|InheritOnly effective resolution). msvc-debug + ubuntu-clang BOTH green.]
- S046 dep:S045  DependencyProperty registration-time dependency validation (ValidateDependencies). [DONE — REDIRECTED (user-accepted, ADR-style decision recorded): the registry-level "ValidateDependencies" dependency-graph gate was reviewed against real commercial WPF MVVM and judged inert — the "children VM depending on a hasa DP" relationship is instance composition, not static registry linkage (WPF itself declares property coupling inline via coercion and never validates a dependency graph at registration). S046 instead delivers the WPF-faithful value hooks — coercion + per-set validation — end-to-end. The coerce→validate(coerced)→changed→store pipeline + PropertyMetadata CoerceFunc/ValidateFunc already exist (S034/S036/S037/S045); S046 adds the UNCOVERED proofs via the Shouldly vocabulary (test/unit/DependencyCallbacks_test.cpp, verify_s046.cmd): (1) attached/global axis — a globally-registered DependencyProperty with with_coerce+with_validate, set on a non-registering object: coerced value applied, validate-after-coerce rejects with store unchanged, wrong-tag throws ValueTypeMismatch; (2) backward compat — global property with no hooks stores as-given; (3) wrong-thread queued coercion — coerce/validate run at queue time (S037), so the DRAIN stores the COERCED value, not the raw input. No new product symbol (a standalone CoerceValue seam / duplicated CoerceValueCallback alias would be dead surface — S048-removable; NO-LAZY-LLM). msvc-debug green.]
- S047 dep:S046  Property-system unit-test suite complete; every numbered predicate via adapted Shouldly. [DONE — REALIZED EARLY by S036_1..S036_5; S047 is the FORMAL suite-complete gate now established + certified. The Shouldly-for-doctest vocabulary (SHOULD/SHOULD_REQUIRE predicate seam + StringMaker + ShouldThrow) is the standing assertion convention from S036_5 onward, and S046 added its proof TU (DependencyCallbacks_test.cpp) fully in that vocabulary. GATE MET: zero unadapted feature `CHECK`/`CHECK_THROWS_AS` across every migrated property feature file (Value, roundtrip, PropertyId, PropertyKeyRegistry, PropertyRegistryExhaustive, PropertyMetadata, DependencyObject, plus the new DependencyCallbacks). Sanctioned non-feature exceptions (deliberate, documented): PropertyMetadata_semantics_test.cpp (the S035 reference SPEC harness — rewriting it would alter the exact contract under test), smoke_test.cpp (doctest runner/main), StringMaker_test.cpp (asserts the raw doctest::String output of the S036_2 stringifier itself), shouldly_test.cpp (tests the predicate/routing library + the S036_1 routing seam + the intentional should_fail demonstration), and the REQUIRE precondition guards (null-deref gates in PropertyKeyRegistry/PropertyRegistryExhaustive). The FULL Sprint-1 property suite — now including DependencyCallbacks_test.cpp — is green on both ci lanes (msvc-debug + ubuntu-clang). No new product symbols. Both compilers green.]
- S048 dep:S047  REFACTOR GATE(S1): unused removed, build+ctest green both compilers. [DONE — full build + ctest green on BOTH ci lanes (windows-msvc via verify_s046.cmd's msvc-debug lane + ubuntu-clang via /mnt/hgfs/VamlPOC clang-debug). No dead/unused code introduced across Sprint-1 closing: S046 deliberately added NO product symbol (a standalone CoerceValue seam / duplicated CoerceValueCallback alias would have been dead surface — removed by design at S046, NO-LAZY-LLM), and no documented public surface or MINIMAL REAL marker was touched. Suite: vaml-core builds warning-as-error clean on both compilers; ctest 1/1 PASS both lanes. Sprint-1 property system core is clean + green.]
- S049 dep:S048  SURVIVOR + checkpoint advance to S080. [DONE — docs-only (no build): consolidated SPRINT 1 SURVIVOR block written to progress.md (S030..S048: property system complete + suite-vocabulary convention + refactor gate), regenerated checkpoint to next = S080 (Sprint 2 — VAML markup parser), roadmap S049 [DONE]. No MINIMAL REAL resolved in Sprint 1; all four active seeds (vulkan→S161, tools→S287, Color→S089, Thickness→S089) carry forward unresolved, NO-GAP intact.]

---
## Sprint 2 — VAML markup parser (runtime path first)  (goal: text → ParsedElement tree)  [token S080+]
- S080 dep:S049  XML scanner (hand-rolled): elements, attributes, self-closing, comments, CDATA, entities, nested tags.
- S081 dep:S080  Scanner line:col error reporting; malformed inputs → structured errors.
- S082 dep:S081  Namespaces + local/prefixed names + default ns; clr-namespace-style mapping tokenized. [DONE — XML scanner namespace-aware: names split on first ':' into (prefix, local_name) with `name` kept FULL raw text (S080 contract); per-element xmlns scope (default + prefixed) pushed/popped on open/close, inherited by descendants + self-applying (element sees its own xmlns); unprefixed ELEMENTS resolve to default ns, default does NOT apply to ATTRIBUTES (XML rule); xmlns/xmlns:p are declarations (XmlToken::xmlns), never ordinary attributes; unmapped prefix -> "" (NOT a scan error — unknown-ns authority is S085/S087, NO-GAP); WPF clr-namespace mapping TOKENIZED via parse_clr_namespace -> ClrNamespaceReference{clrNamespace,assembly} (parts only; type resolution is S087) + new XmlScanError::MalformedClrNamespace (keeps S081 1:1 taxonomy). Dedicated TU test/unit/XmlNamespace_test.cpp (12 cases, S082-unique urn:vm:ns:* URIs); existing S080/S081 scanner tests unchanged & green. Both CI lanes verified (msvc-debug via build/verify_s082.cmd + ubuntu-clang).]
- S083 dep:S082  x: directives (Name, Class, Key, Null) tokenized. [DONE — XML scanner recognizes the four XAML x-language directives as a distinct, structured token category, layering on S082's namespace model. NEW public surface: inline constexpr std::string_view kXamlLanguageNamespace ("http://schemas.microsoft.com/winfx/2006/xaml") + enum class XmlDirectiveType { None, Name, Class, Key, Null } + XmlAttribute::directive (default None). Classification is keyed on the RESOLVED namespace URI, NOT the literal "x" prefix: any prefix mapped to the canonical xaml URI is the x-language (foo:Name under the xaml URI IS a Name directive), while S082's fake xmlns:x="urn:vm:ns:x" attrs stay PLAIN attributes (backward compat, all S082 tests unchanged+green); matching is case-sensitive; an unknown local in the xaml ns -> None. Tagged ON the attribute (value stays in `value`), keeping the single ordered attributes list intact (no re-merge churn — NO-LAZY-LLM classify-in-place). Static/file-local classifyXDirective helper + a per-token directive clear. TOKENIZATION ONLY — application recorded NO-GAP (not silent): x:Name name-registry -> S092; x:Class root-owner binding -> S092/S089; x:Key resource-key -> S361; x:Null null-set (markup-extension {x:Null} -> S084) and element-in-language-ns -> parsed by S085. Dedicated TU test/unit/XmlDirective_test.cpp (12 cases, 45 assertions; S083-unique via the real kXamlLanguageNamespace). XmlScanError taxonomy unchanged (19 categories — no new malformed path). Both CI lanes verified (msvc-debug via build/verify_s083.cmd + ubuntu-clang). XmlScanner stays a full feature extended by normal progression; no MINIMAL REAL introduced/resolved.]
- S084 dep:S083  Attribute value grammar: quoted literal, markup-extension {Binding...} tagged, numeric detection. [DONE — src/core/include/vaml/core/XmlScanner.h + src/core/src/XmlScanner.cpp (MOD) + test/unit/AttributeValueGrammar_test.cpp (NEW) + test/unit/CMakeLists.txt + build/verify_s084.cmd. The scanner classifies EVERY attribute value (ordinary props AND S083 x: directive values) so the S085 ParsedAttribute {key,value,isExtension} consumes isExtension/numeric directly. NEW public surface: enum class AttributeValueKind { QuotedLiteral, MarkupExtension }; XmlAttribute gains value_kind (default QuotedLiteral) + numeric (default false); free VAML_CORE_EXPORT AttributeValueKind classify_attribute_value(std::string_view value, bool* isNumeric = nullptr). Classifier wired into the S082/S083 attribute-resolution loop (after directive) so every scanned attribute carries value_kind + numeric, per-token (no leakage). Three recognitions: (1) QuotedLiteral — the default, a plain decoded quoted string (Width="100", Text="Hello"); (2) MarkupExtension — value begins with '{' AND ends with a BALANCED closing '}' with nesting honored ({Binding X}, {x:Null}, {StaticResource {x:Type Button}} -> MarkupExtension; "{abc" unbalanced -> QuotedLiteral); (3) numeric — a QuotedLiteral whose scalar grammar parses as [-+]?(digits|digits.digits|.digits)[eE][-+]?digits ("100","-45","3.14","+2.5E-3",".5","1e6" -> numeric; "Hello","auto","1.","10,20","-" -> not). Boundary recorded NO-GAP (not silent): scalar numeric DETECTION only — the comma-list thickness form ("10,20"->Thickness) and full type conversion are S089's converter-domain step (S084 detects, S089 converts); the WPF {}-escape-for-a-literal-{ is a parser-level (S085) concern, not scanner scope. 14 NEW TEST_CASEs in AttributeValueGrammar_test.cpp (Shouldly vocabulary): classifier quoted-vs-extension; leading-{ without balanced } -> not extension; nested braces honored; numeric int/sign/frac/exponent; non-numeric + edge cases (trailing dot, bare sign, empty); extension never numeric at the classifier; scanner integration (quoted+flag, {Binding Title} tagged, nested extension, unbalanced-{ stays literal, sign/exponent numeric, x: directive value {x:Null}->MarkupExtension while x:Name="foo"->QuotedLiteral, per-token no leakage). xml-language {x:Null} path resolves S083's recorded NO-GAP hand-off for x:Null null-set. XmlScanError taxonomy UNCHANGED (still 19 categories); the classifier is pure classification, no new malformed path. Both CI lanes verified (windows-msvc via build/verify_s084.cmd msvc-debug + ubuntu-clang via /mnt/hgfs/VamlPOC clang-debug).]
- S085 dep:S084  ParsedElement {tagName, ns, attributes, children} + ParsedAttribute {key, value, isExtension}. [DONE — src/core/include/vaml/core/ParsedElement.h + src/core/src/ParsedElement.cpp (NEW) + MOD src/core/CMakeLists.txt + test/unit/ParsedElement_test.cpp (NEW) + MOD test/unit/CMakeLists.txt + build/verify_s085.cmd (NEW). The tree that consumes the S080-S084 token stream into a lossless, readable hierarchy — first Sprint-2 step beyond a linear token list. ParsedAttribute {key (full raw name), value, isExtension} + extended scanner metadata carried (prefix/local_name/namespace_uri/directive/value_kind/numeric) so consumers never re-scan (NO-LAZY-LLM). ParsedElement {tagName, ns, attributes, children, content} + extended (prefix/local_name/self_closing); `content` holds an element's text between bounding tokens — the finalized decision is tags+children+content (NOT a pseudo-element sentinel) so readers distinguish elements from text at the type level; CONSCIENCE NOTE recorded in the header: one content string + ordered children does not capture interleaved text/child ordering ("<p>Hello <b>world</b> again</p>") — full mixed-content ordering is a later explicit decision, not this step. Assembly: ElementOpen->frame pushed (self-closing closes immediately), Text accumulates into open frame's content (as authored), ElementClose finalizes/moves the frame to its parent or the single root, Comment/CDATA DROPPED (S080 hand-off resolves: not tree nodes); end-tag matching remains the SCANNER's job (XmlScanException::MismatchedEndTag); the parser's OWN structural path = EOF with an open element / missing root -> NEW XmlParseException + XmlParseError::UnterminatedElement (taxonomy has only what the parser can actually produce — no dead MismatchedEndTag at parse level, S048 discipline). Unknown-ns authority boundary kept (empty ns NOT a parse error; resolution is S087). Both CI lanes green: windows-msvc via build/verify_s085.cmd (msvc-debug warning-clean + ctest 1/1 PASS, 279 cases) + ubuntu-clang via /mnt/hgfs/VamlPOC (configure+build+ctest 1/1 PASS). No MINIMAL REAL introduced/resolved.]
- S086 dep:S085  Parser round-trips a realistic VAML document into the tree (tests). [DONE — test/unit/ParsedRoundtrip_test.cpp (NEW) + MOD test/unit/CMakeLists.txt + build/verify_s086.cmd (NEW) + MOD docs/roadmap.md + session-context.md. TEST-ONLY — zero new product symbols (S080-S085 pipe consumed as-is); the formal round-trip gate over S085's tree shape. One realistic multi-feature doc (namespaces xmlns:x, x:Class/x:Name directives, attached-style dotted names DockPanel.Dock/Grid.Row/Grid.Column, markup extensions {Binding ...}, numeric Width/Height/Grid.Row/Grid.Column/FontSize, quoted Title/Content, self-closing TextBox/Button/Texts, and a text-content leaf <Text x:Name="SearchLabel">Search:</Text>) is asserted five ways: (1) GOLDEN STRUCTURAL round-trip — a test-local canonicalize() re-serializer (attrs in document order, self-closing preserved, children-then-content, xmlns correctly omitted as declarations) proves canonical(parse(source)) == the hand-written expected string, so text→tree→structure reproduces the authored doc exactly modulo non-semantic whitespace; (2) full tree-shape — root Window tagName/ns/empty-content, containers carry no content, the leaf carries "Search:", self_closing true on TextBox/Button/self-closing Texts and false on containers, DockPanel child ordering SP1/SP2/Grid, Grid.Row/Grid.Column kept as FULL raw keys ('.' is NOT a namespace separator — S082); (3) feature-classification — {Binding SearchText} → isExtension + MarkupExtension + not-numeric, Width/Height → numeric, x:Class/x:Name → Class/Name directive with kXamlLanguageNamespace uri, Margin="10,10,10,10" → quoted literal non-numeric (comma-list is S089 converter domain, S084 detects only scalars); (4) comments dropped (add nothing) + pretty-printed inter-element whitespace kept as-authored in parent content ("\n  \n  \n"); (5) error path — truncated doc (missing </Window>) throws XmlParseException::UnterminatedElement, full doc parses with no throw. Round-trip is text→tree→readable structure (no serializer yet — VBAML serialization is S282/S283); unknown-ns resolution stays S087 (recorded, not silent); mixed-content ordering stays the S085 conscience note. Both CI lanes green: windows-msvc via build/verify_s086.cmd (msvc-debug warning-clean + ctest 1/1 PASS, 291 cases, 12 new S086 cases all green) + ubuntu-clang via /mnt/hgfs/VamlPOC (clang-debug configure+build+ctest 1/1 PASS). No MINIMAL REAL introduced/resolved.]
- S087 dep:S086  ElementFactoryRegistry: typeName→factory<unique_ptr<DependencyObject>>; startup registration. [DONE — src/core/include/vaml/core/{ElementFactoryRegistry,Element}.h + src/core/src/{ElementFactoryRegistry,Element}.cpp + test/unit/ElementFactoryRegistry_test.cpp + MOD src/core/CMakeLists.txt + test/unit/CMakeLists.txt + build/verify_s087.cmd. vaml::ElementFactoryRegistry (pimpl'd, real, not MINIMAL REAL): maps type NAME -> ElementFactory (std::function<unique_ptr<DependencyObject>()>), name-unique dedupe (duplicate -> ElementAlreadyRegistered, registry UNCHANGED, factory never swapped), try_create (noexcept, nullptr if absent) / create (throws ElementNotFound), is_registered, size/empty, registered_names in INSERTION ORDER (deterministic, never unordered_map iteration). Exceptions mirror the S032 property taxonomy. Global Meyers singleton matches PropertyRegistry/DependencyProperty::global; stores an OWNED name copy (same SSO/dangle discipline as S036/S039). MINIMAL REAL element seeds (vaml::Button/StackPanel/Grid/Text/TextBox/CheckBox/Slider/ScrollViewer/Window — thin concrete DependencyObject subclasses, identity/type only, tagged with searchable `// [MINIMAL REAL] Element <X> — RESOLVED BY S###` sentinels) + register_builtin_elements() (idempotent startup registration of all 9 names -> make_unique<Concrete>()). Tests (Shouldly vocabulary): register+create -> live concrete type (dynamic_cast non-null); fresh instance per call; dedupe throws + unchanged + original factory preserved; create-unknown throws ElementNotFound; try_create-unknown -> nullptr; is_registered/size/empty; insertion-order deterministic; STARTUP GATE — register_builtin_elements() then create("Button"/"StackPanel"/...) yields each concrete seed + global size==9; idempotent second call no-throw. NO-GAP: unknown-type structured error naming type+line is S088's step (registry only throws ElementNotFound here; S088 maps it, recorded not silent). Element classes are seeds only (no logic) — real content at each control's own sprint. Both CI lanes green (msvc-debug via verify_s087.cmd + ubuntu-clang).]
- S088 dep:S087  Resolve "Button"→instance; unknown type → structured error naming type+line. [DONE — src/core/include/vaml/core/ElementResolver.h + src/core/src/ElementResolver.cpp + test/unit/ElementResolver_test.cpp + MOD src/core/{ParsedElement.h,cpp,CMakeLists.txt} + MOD test/unit/CMakeLists.txt + build/verify_s088.cmd. Reader-boundary resolution primitive: `resolve_element(ParsedElement, ElementFactoryRegistry)` (plus a global() overload) resolves one element into a live DependencyObject instance via the S087 registry, keyed on the element's local_name (fallback tagName) so `<local:Button>` resolves "Button". An unknown/absent/empty type -> NEW structured `ElementResolutionException` with `ElementResolutionError::{None,UnknownType}`, exposing `kind()/type_name()/line()/col()` and a what() rendering "unknown element type '<name>' (line N, column M)". Mapping the S087-registry ElementNotFound into the line-carrying reader error is exactly the S087-recorded NO-GAP hand-off: ElementFactoryRegistry itself still carries NO source-location concept. S088 also extends ParsedElement with line/col carried from the scanner's ElementOpen token (S085's "carried, not re-scanned" discipline — no line-map re-derivation) so errors name the real authored position. Scope boundary (NO-GAP, not silent): S088 delivers the SINGLE-ELEMENT primitive + structured error only; child-hierarchy building / property setting is S091's explicit step, which will consume resolve_element. Error split honored: only ElementNotFound -> UnknownType; a throwing factory's construction exception PROPAGATES (never swallowed). Tests (Shouldly): resolve Button -> live Button (custom + global); custom type resolves; fresh instance per resolve; prefixed resolves via local_name; unknown -> exception naming type + real parsed line/col; ShouldThrow form; what() names type + line; deep-doc line accuracy (real multi-line markup, offending element's line reported); parsed line/col carried onto tree; empty name -> UnknownType; throwing factory propagates (registry still reports registered). Both CI lanes green: windows-msvc via build/verify_s088.cmd (msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang via /mnt/hgfs/VamlPOC clang-debug.]
- S089 dep:S088  Value converter registry: "42"→int, "10,20"→Thickness, "#FF0000"→Color, "true"→bool. [RESOLVES S030 Color + Thickness MINIMAL REAL markers → give them full string-parsing semantics + remove their sentinel tags]
- S090 dep:S089  Unknown literal → error; converter registry tests.
- S091 dep:S090  Runtime VamlReader::Load(text/ParsedElement→live tree): build children, set props.
- S092 dep:S091  VamlReader defers markup-extension values (S2 tag) + event hooks (S8); records x:Name registry.
- S093 dep:S092  Load a StackPanel+Button with a property set from pure text — tree correct, no hooks.
- S094 dep:S093  VamlReader error cases: unknown attr, unknown element, bad type name, malformed value.
- S095 dep:S094  Parser + reader full test suite (golden VAML docs).
- S096 dep:S095  REFACTOR GATE(S2): full parser green, no manual XML dep, no stubs.
- S097 dep:S096  SURVIVOR + checkpoint advance to S120.

---
## Sprint 3 — Element / scene graph + measure/arrange layout  (goal: full layout contract)  [token S120+]
- S120 dep:S097  UIElement base (derives DependencyObject): visibility, opacity, transform owner slot, layout slot.
- S121 dep:S120  Create/attach/detach children; parent traversal; visual-tree-walk tests.
- S122 dep:S121  LayoutSlot + final rect + DesiredSize + Margin slot + alignment.
- S123 dep:S122  Measure contract (infinity-aware): Measure(available)->DesiredSize; stretch vs wrap.
- S124 dep:S123  Arrange contract + clip: Arrange(finalRect); clip bounds honored by render slot.
- S125 dep:S124  Leaf elements (Text) measure to content; panels measure children.
- S126 dep:S125  InvalidateMeasure/InvalidateArrange; LayoutManager drives measure+arrange over dirty subgraph.
- S127 dep:S126  MeasureOverride/ArrangeOverride virtuals for panels.
- S128 dep:S127  Dirty-only subpass: changing a child re-measures only subtree (test).
- S129 dep:S128  StackPanel (vertical+horizontal; orientation prop; last-child-stretch).
- S130 dep:S129  StackPanel DesiredSize + stretch correctness tests.
- S131 dep:S130  Grid: row/col definitions; Absolute/Auto/Star resolution.
- S132 dep:S131  Grid ColumnSpan/RowSpan + measure+arrange math.
- S133 dep:S132  Grid star-sizing + span tests.
- S134 dep:S133  WrapPanel: flow, wraps at boundary, aligned rows.
- S135 dep:S134  WrapPanel tests.
- S136 dep:S135  Canvas: explicit position via attached Left/Top (attached props via S1).
- S137 dep:S136  Canvas tests.
- S138 dep:S137  Layout suite green both compilers; REFACTOR GATE(S3).
- S139 dep:S138  SURVIVOR + checkpoint advance to S160.

---
## Sprint 4 — Vulkan window + swapchain + first pixels (single-thread proof)  [token S160+]
- S160 dep:S139  Platform window via GLFW (Win32 + X11); resize + input callback stub surface.
- S161 dep:S160  CMake finds glfw3 + vulkan + VMA; link vaml-vulkan. [RESOLVES S003 vulkan MINIMAL REAL marker → replace version marker with real surface + remove its sentinel tag]
- S162 dep:S161  Vulkan instance + surface + physical device + queue family selection; validation layers debug-only.
- S163 dep:S162  Best-GPU auto-selection: score ALL physical devices (DISCRETE>INTEGRATED>OTHER; device-local memory; apiVersion tiebreak); filter to present-capable families; VAML_PREFERRED_GPU name override; log full ranked list at init. Identical code in VamlPOC.cpp + VamlPOC-linux/main.cpp; two-GPU Windows proves multi-adapter selection; single-GPU Linux proves ranking+override path. [both apps, not CI]
- S164 dep:S163  Swapchain (VK_KHR_swapchain) + image views + depth; format/BPP negotiation.
- S165 dep:S164  Render pass + single-triangle graphics pipeline + framebuffers (hardcoded MVP).
- S166 dep:S165  Command buffers + sync (semaphores/fences) + main-thread draw loop; clear + triangle visible.
- S167 dep:S166  Resize-safe swapchain recreation; no validation errors in debug.
- S168 dep:S167  MSAA sample-count negotiation + multisampled triangle.
- S169 dep:S168  Vulkan RAII wrappers (device/queue/swapchain/command-pool/framebuffer) — no raw vk* owned outside device.cpp.
- S170 dep:S169  RAII destruction-order clean at close; no leaks, no staging.
- S171 dep:S170  REFACTOR GATE(S4): triangle renders, no validation errors, resize-safe, RAII clean.
- S172 dep:S171  SURVIVOR + checkpoint advance to S200.

---
## Sprint 5 — Vulkan UI renderer (retained scene → GPU)  (goal: VAML-declared UI on screen)  [token S200+]
- S200 dep:S172  Quad renderer: vertex/index buffers, textured quads, uniform MVP/UI scale, alpha blend, SRGB.
- S201 dep:S200  Draw N quads from CPU list each frame; blend + SRGB tests.
- S202 dep:S201  Texture atlas + image upload (staging → device-local); sub-region invalidation.
- S202_1 dep:S202  Icon asset pipeline (single quad engine; per ADR-015): PNG and SVG are BOTH source asset
      formats, each rasterized ONCE into the atlas exactly as fonts are (S203/S204) — no vector-on-screen,
      no second renderer. Output tagged with its texel count for best-fit selection (S213_1).
- S203 dep:S202  Embedded bitmap-font loader + ASCII 32–126 glyph atlas build.
- S204 dep:S203  SDF rasterizer: glyphs → SDF into atlas; soft-edge sampling.
- S205 dep:S204  UI shaders (SDF text + solid color + border) + descriptor sets; shared layout.
- S205_1 dep:S205  Monochrome icon tint at sample: single-channel alpha-mask sampled by the quad shader,
      Foreground color applied in-shader (no bitmap color-effect recompositing; preserves partial-alpha).
      TintMode=Monochrome uses this; TintMode=Preserve samples the multicolor texture.
- S206 dep:S205  Draw text + rect from shared descriptor layout; batch same-shader draws.
- S207 dep:S206  Scene→command-buffer encoder: walk visual tree → draw list by material; z-order via tree order.
- S208 dep:S207  Encoder honors Opacity + Clip (draw grouping).
- S209 dep:S208  GPU batching by texture+material; minimal pipeline switches; retired-buffer lifetime via fence.
- S210 dep:S209  Render-thread loop end-to-end (single thread first): retained scene drawn each frame.
- S211 dep:S210  Marshalled-change consumption per frame (UI thread mutates, render thread draws) — first wire.
- S212 dep:S211  MERGE POINT: UI declared in VAML renders to window via full S5 path (first real vertical slice).
- S213 dep:S212  DPI + framebuffer scale: logical px layout, render scales by framebufferScale; high-DPI crisp.
- S213_1 dep:S213  DPI-aware icon best-fit selection (per ADR-015): scaling policy { none | down-only | both },
      default none. "none" = never resample, pick largest authored texel set ≤ physical N (N_physical =
      logical_size × dpiScale), sprite may be smaller than its slot; "down-only" may sample a larger set
      down to exactly N_physical, never up; "both" permits either way (blur possible). Per (IconKey,
      logical size) computed once at runtime against framebufferScale, then cached.
- S214 dep:S213  REFACTOR GATE(S5): retained UI renders via Vulkan; no stubs; no validation errors.
- S215 dep:S214  SURVIVOR + checkpoint advance to S240.

---
## Sprint 6 — Binding engine + MVVM core  (goal: v1 is an MVVM app)  [token S240+]
- S240 dep:S215  PropertyPath parser + evaluator (dot-path; indexers; null-safe; errors reported).
- S240_1 dep:S240  BindingDiagnostic record {bindingId, sourcePath, targetProp, direction, reasonCode,
      sourceType, targetType, converter, detail, timestamp, sourceFile, sourceLine} + IBindingDiagnostics
      sink emitted at every failure point (S240 resolve, S242 converter/fallback, S243 one-way, S244
      two-way); Class A (static/authoring error) vs Class B (defined runtime data outcome) tagging at emit;
      NO-OP default sink (app opt-in, zero release cost unless subscribed).
- S241 dep:S240  ObservableObject: Set() + typed PropertyChanged signal (INotifyPropertyChanged-analogue).
- S242 dep:S241  BindingExpression: source/target, mode one/one-time/two-way, converter, fallback value.
- S242_1 dep:S242  BindingPolicy { Strict | Fallback } on BindingExpression: Strict promotes Class A to a
      bind-time throw; Class B NEVER throws (fallback + diagnostic only, per ADR-016).
- S243 dep:S242  Binding engine one-way: source→target on change; DP invalidation coalescing.
- S244 dep:S243  Two-way: target→source; TextBox edit → VM updates; converter both directions.
- S245 dep:S244  Height/glide target (fallback) + expression lifecycle (detach on element destroy).
- S245_1 dep:S245  Binding-failure test suite: harness subscribes to the sink and asserts exact reasonCode
      per Class (not just fallback values); detach/lifecycle (S245) is a Class-B-tagged, silenced
      non-error. Diagnostics ≠ control-flow (subscriber gets info, never control).
- S246 dep:S245  RelayCommand (ICommand-analogue): invoke + CanExecute + CanExecuteChanged (full, no stub).
- S247 dep:S246  RelayCommand tests.
- S248 dep:S247  Messenger (typed loosely-coupled bus): publish/subscribe + token unsubscribe, no leaks.
- S249 dep:S248  Messenger tests.
- S250 dep:S249  Static CompositionRoot (user's DI style): resolves VM + service; no runtime container.
- S250_1 dep:S250  IBindingDiagnostics sink + BindingPolicy injectable via CompositionRoot, reachable from
      BOTH VM and view/control layers (V vs VM = one channel, two consumers; per ADR-016).
- S251 dep:S250  CompositionRoot + VM-with-injected-service tests.
- S252 dep:S251  Markup {Binding} resolved in VamlReader (deferred extension now bound): <Text Text="{Binding Title}"/>.
- S253 dep:S252  Command via {Binding DoIt}; one/two-way markup binding end-to-end.
- S254 dep:S253  REFACTOR GATE(S6): full MVVM works in v1, all modes + converters, diagnostics wired, no stubs.
- S255 dep:S254  SURVIVOR + checkpoint advance to S280.

---
## Sprint 7 — VBAML compiler (BAML analogue; FIRST full pipeline)  (goal: text→vbaml→tree)  [token S280+]
- S280 dep:S255  vamlcc CLI skeleton: read .vaml → write compiled form; parses via S2 scanner + tables.
- S280_1 dep:S280  Static binding validation in vamlcc (per ADR-016): each binding path is validated against
      its statically-declared root type via the property registry (S032–S039). Typo / wrong hop type →
      compile-time error with source line/col. Roots that are truly runtime-typed (object/dynamic DataContext,
      untyped template item) are NOT validated here — they defer to bind-time Class A (S242_1) and count
      toward the coverage ratio (S826_1). [compile-time is the rule; dynamic root is the documented,
      Strict-excludable escape hatch]
- S281 dep:S280  Binary record set: versioned schema (element-id/type-token/prop-token/event-token/resource-token/string-table).
- S282 dep:S281  Serializer round-trips a sample tree to bytes and decode-back equal (byte-identical).
- S283 dep:S282  Parser → record-emit (reuse scanner/factory/value tables; no stubs).
- S284 dep:S283  Goldens: a full VAML file compiles to a .vbaml with correct token stream.
- S285 dep:S284  Runtime VBamlReader: bytes → DependencyObject tree (InitializeComponent-style).
- S286 dep:S285  Reader instantiates via factory registry, sets props, attaches deferred events (S8).
- S287 dep:S286  vamlcc integrated in CMake: custom command per .vaml → .vbaml embedded or sidecar. [RESOLVES S003 tools MINIMAL REAL marker → extend version marker into real tooling surface + remove its sentinel tag]
- S288 dep:S287  Dual-path parity harness: runtime-parsed vs compiled trees behaviorally identical (asserted).
- S289 dep:S288  Pack/identity: x:Class name mapping; embedded resource lookup; release uses compiled only.
- S290 dep:S289  Refactor-gated dual path: S2 reader = dev tool, S7 reader = release.
- S291 dep:S290  REFACTOR GATE(S7): FULL text→vbaml→tree pipeline, parity proven, no stubs.
- S292 dep:S291  SURVIVOR + checkpoint advance to S320.

---
## Sprint 8 — Routed events + input + async  (goal: real events + input + dispatcher)  [token S320+]
- S320 dep:S292  RoutedEvent primitive: Signal<T> + token + route-policy hook; reentrancy-safe add/remove.
- S321 dep:S320  Bubbling + tunneling + handled: child click bubbles to root unless handled; preview tunnel.
- S322 dep:S321  Markup event wiring: Button Click="OnClick" → handler on x:Class owner via registry.
- S323 dep:S322  Mouse/keyboard input → routed events via GLFW callbacks + hit-testing.
- S324 dep:S323  Click hit-tests element, raises bubbling routed event with correct source.
- S325 dep:S324  Keyboard focus + routed key events (bubble).
- S326 dep:S325  Dispatcher: input→UI-thread marshalling; continuations scheduled (first Task usage) [ADR-010].
- S327 dep:S326  Async continuation updates VM prop → binding updates UI; no races.
- S328 dep:S327  REFACTOR GATE(S8): events, routing, markup wiring, input, async dispatcher all real.
- S329 dep:S328  SURVIVOR + checkpoint advance to S360.

---
## Sprint 9 — Styles / resources / templates  (goal: full styling)  [token S360+]
- S360 dep:S329  ResourceDictionary + merged dictionaries + lookup (tree + app + system).
- S360_1 dep:S360  IconKey resource entry: key → icon asset reference, resolved through the ResourceDictionary
      (per ADR-015) so ViewModels expose only an IconKey (never a path/SVG/render object); reuses S361
      missing-key error for resolution failure.
- S361 dep:S360  StaticResource/DynamicResource resolution; merge precedence; missing-key errors.
- S362 dep:S361  Style + Setter + BasedOn + PropertyTrigger + DataTrigger.
- S363 dep:S362  Style applied to type; property setters; BasedOn chain; triggers fire.
- S364 dep:S363  ControlTemplate + visual states (normal/hover/pressed swap visuals via brushes).
- S365 dep:S364  TemplateBinding to control props works; templatable Button restyle.
- S366 dep:S365  Template lookup from markup + compiled (S2 and S7 readers agree).
- S367 dep:S366  REFACTOR GATE(S9): resources/styles/templates fully functional; no stubs.
- S368 dep:S367  SURVIVOR + checkpoint advance to S400.

---
## Sprint 10 — Core controls  (goal: usable control set)  [token S400+]
- S400 dep:S368  Text control: content, font size/weight/color, wrapping via measure, SDF render.
- S400_1 dep:S400  Icon control (per ADR-015): Dependency-Property surface Source/IconKey, TintMode
      {Preserve | Monochrome}, Foreground, Width/Height/Stretch; resolves IconKey through the S9 Resource
      Dictionary + a cached icon provider; missing/unsupported asset → deliberate fallback glyph +
      structured diagnostic (mirrors binding diagnostics; NO-GAP).
- S401 dep:S400  Button: template, hover/pressed visual, Click routed event, IsEnabled.
- S402 dep:S401  TextBox: single+multi line, caret, editing, two-way binding, selection, focus.
- S403 dep:S402  Slider: value binding, thumb drag, routed change event.
- S404 dep:S403  CheckBox: toggle + IsChecked two-way binding.
- S405 dep:S404  ScrollViewer: scroll + clipping + wheel/scrollbar.
- S406 dep:S405  Focus + tab order + Enter-activates-default + routed key events.
- S407 dep:S406  Milestone: all controls fully implemented; each has test; no stubs.
- S408 dep:S407  REFACTOR GATE(S10) + SURVIVOR.

---
## Sprint 11 — v1 app vertical slice  (goal: complete MVVM app on VAML+Vulkan)  [token S440+]
- S440 dep:S408  app/hmi MainView.vaml: Grid+StackPanel+Button+Text+TextBox+Slider+CheckBox layout.
- S441 dep:S440  MainViewModel: observable props + RelayCommands wiring to a domain service.
- S442 dep:S441  app/infra service (filesystem/data demo service) + CompositionRoot wiring.
- S443 dep:S442  Wire MainView to MainViewModel via markup {Binding} + {Command}.
- S444 dep:S443  Compile MainView.vaml→.vbaml via vamlcc in the CMake custom command (release path).
- S445 dep:S444  Launch exe → full declared UI renders; click/type/slide updates VM-driven UI end to end.
- S446 dep:S445  Zero validation-layer errors; no leaks over 10k frames; no layout thrash.
- S447 dep:S446  v1 E2E integration test (headless/offscreen where feasible).
- S448 dep:S447  v1 acceptance doc written from real behavior (not spec).
- S449 dep:S448  REFACTOR GATE(S11) — v1 VERTICAL SLICE COMPLETE. SURVIVOR. (This is NOT a throwaway
      "POC complete" — it is the working first-version app that v2-as-v1 builds on; see ADR-017.)

---
## Sprint 12 — Docs from evidence + production packaging  (goal: production-ready repo + packaging)  [token S480+]
- S480 dep:S449  Write architecture.md FINAL from real v1 code (not the seed).
- S481 dep:S480  Write design docs (measure/arrange, renderer, binding, VBAML schema) from evidence.
- S482 dep:S481  Complete ADR log (all standing decisions + lessons learned with rationale).
- S483 dep:S482  v1 lessons document: what to keep/change.
- S484 dep:S483  Harden repo for production consumers (packaging, exported facade, single-exe demo).
- S485 dep:S484  Production structure finalization (clean organization of core→vulkan→tools; no throwaway port).
- S486 dep:S485  REFACTOR GATE(S12) + SURVIVOR. Production repo posture finalized.
      (ADR-017 RE-SCOPE: this sprint NO LONGER "opens a separate production repo and ports POC." v2 is v1 —
      the code you are building IS the product; S12 hardens and packages it in place, it does not restart.)

---
## Sprint 13 — Test coverage expansion  (goal: CI-grade coverage, property-based, fuzz)  [token S520+]
- S520 dep:S486  Coverage gate (gcov/lcov + llvm-cov) wired into CI on core.
- S521 dep:S520  Property-system fuzz harness (random value sets → invariants).
- S522 dep:S521  Parser fuzz harness (malformed VAML → no crash, structure errors only).
- S523 dep:S522  VBamlReader fuzz harness (random byte stream → no crash).
- S524 dep:S523  Layout property-based tests (random trees → no negative size, terminate).
- S525 dep:S524  Binding stress tests (rapid invalidation → coalesced, correct final).
- S526 dep:S525  Event routing random-tree tests (hit path + handled correctness).
- S527 dep:S526  Style/resource lookup precedence fuzz.
- S528 dep:S527  Renderer offscreen correctness tests (pixel-snapshot on fixtures).
- S528_1 dep:S528  Icon test gallery + rendered-result tests (per ADR-015, GPT point 7): pixel-snapshot the
      atlas/framebuffer for transparency + partial-alpha, monochrome tint vs multicolor preserve, strokes/
      clip/viewBox/blank margins, size-family + DPI, missing/malformed/multicolor fallback, cache behavior.
- S529 dep:S528  REFACTOR GATE(S13) + SURVIVOR. Coverage + fuzz green CI.

---
## Sprint 14 — Performance & GPU-driven rendering  (goal: production perf)  [token S620+]
- S620 dep:S529  Async layout: measure/arrange on worker threads, single commit to render; TSan-clean.
- S621 dep:S620  Async layout correctness == single-threaded (diff test).
- S622 dep:S621  GPU-driven batching: indirect/draw-list, instance buffer, minimal pipeline switches.
- S623 dep:S622  Draw-call count near-constant under 10k elements; no correctness drift.
- S624 dep:S623  Per-frame arena allocator; element pool; no heap churn in steady state.
- S625 dep:S624  Zero-copy texture update + cache-resident font/brush cache.
- S626 dep:S625  Thread vs render ring scale-out: multiple UI threads; copy-on-write hot nodes.
- S627 dep:S626  Perf smoke harness: 60fps @ 10k elements target; profiling budget in CI (event).
- S628 dep:S627  MEMORY profile gate: bounded memory, no leaks (ASan).
- S629 dep:S628  REFACTOR GATE(S14) + SURVIVOR. Perf gates met.

---
## Sprint 15 — Production hardening  (goal: release-quality robustness)  [token S720+]
- S720 dep:S629  Cancellation tokens through Task/Executor; no deadlocks; deterministic ordering.
- S721 dep:S720  Reentrancy audit across property/binding/event paths.
- S722 dep:S721  Allocator policy finalization (arena/pool rules in ADR).
- S723 dep:S722  TSan + ASan clean across suite; Valgrind (Linux) spot-check.
- S724 dep:S723  Thread-affinity enforcement; wrong-thread writes blocked with clear error.
- S725 dep:S724  Graceful shutdown: device/swapchain teardown ordering, zero validation errors on exit.
- S726 dep:S725  Error taxonomy: structured codes + diagnostics channel (no exceptions in release hot paths).
- S727 dep:S726  REFACTOR GATE(S15) + SURVIVOR.

---
## Sprint 16 — DPI, i18n, accessibility, tooling  (goal: broad applicability)  [token S820+]
- S820 dep:S727  Per-monitor DPI correctness across multi-monitor.
- S821 dep:S820  i18n: resource fallback (culture→neutral), string tables from VAML resources.
- S822 dep:S821  RTL mirroring + glyph shaping for localized text.
- S823 dep:S822  Culture-aware number/date formatting in bindings (converters).
- S824 dep:S823  Accessibility: text alternatives + keyboard navigation completeness.
- S825 dep:S824  Hot-reload dev loop for .vaml (S2 reader) in tooling.
- S826 dep:S825  vamlcc diagnostics (nice errors: source line/col, suggested fix).
- S826_1 dep:S826  Compile-time binding coverage ratio (per ADR-016): vamlcc reports "this build validated
      N% of bindings at compile time; M% fell back to bind-time" so a large codebase (e.g. 500k lines) can
      PROVE its bindings are statically checked rather than assume it — surfaces convention drift where a
      screen's root silently becomes runtime-typed.
- S827 dep:S826  Packaging: single-exe consumer link demo + install rules.
- S828 dep:S827  REFACTOR GATE(S16) + SURVIVOR.

---
## Sprint 17 — Release readiness + production roadmap  (goal: v1 control set + release)  [token S920+]
- S920 dep:S828  Documented v1 control set + API stabilization pass.
- S921 dep:S920  Public headers/export facade audit (IP behind libs) [ADR-003].
- S922 dep:S921  Licensing + dual-license docs (OSS / prod options).
- S923 dep:S922  Sample gallery + tutorial VAML docs.
- S924 dep:S923  Full end-to-end release pipeline (tag → build both CI → package → notes).
- S925 dep:S924  v1 release candidate; regression suite green.
- S926 dep:S925  REFACTOR GATE(S17) — FINAL. SURVIVOR + close-out.
- S927 dep:S926  Post-1.0 roadmap drafted (advanced controls, GPU compute, etc.).
- S928 dep:S927  Documentation site skeleton (from docs/).
- S929 dep:S928  Community/contribution guide.
- S930 dep:S929  Roadmap v1.1 planning; guide sprint is Sprint 18 (S940+); reserve S990–S999 for post-release work.

---
## Sprint 18 — Commercial user guide: authoring a VAML app in C++ (goal: v1.0 commercial guide covering ONLY the C#→C++ application gap)  [token S940+]
- S940 dep:S930  Guide charter + outline + audience + style guide; docs/guide/README.md.
      Rules: every chapter declares its evidence source = an exact ROADMAP token; no chapter cites an
      unimplemented API (NO-LAZY-LLM gate).
- S941 dep:S940  Ch.1 Hello VAML (01-hello-vaml.md): bootstrap a consumer app from scratch (CMake +
      preset, link vaml-core/vulkan, static vs shared). C# analog: "New WPF project". Full compilable TU.
- S942 dep:S941  Ch.2 Markup (02-markup.md): the exact VAML dialect an author writes — elements, properties,
      attached props, resources, styles, templates, x:Name/x:Class, {Binding}/{StaticResource}. Snippets must
      pass vamlcc.
- S943 dep:S942  Ch.3 MVVM core in C++ — THE HEART OF THE GAP (03-mvvm-core.md): ObservableObject::Set +
      PropertyChanged vs INotifyPropertyChanged; RelayCommand CanExecute/Changed vs ICommand; header/cpp
      split; value vs reference semantics; ownership (unique_ptr/shared_ptr/raw) vs GC at DI seams; move-only
      discipline. Gate: every class snippet is a full TU compiled in CI.
- S944 dep:S943  Ch.4 Bindings & commands (04-bindings-commands.md): PropertyPath syntax, one/one-time/two-way,
      C++ converter vs IValueConverter, fallback, two-way TextBox→VM, detach/lifetime.
- S945 dep:S944  Ch.5 Events & input (05-events-input.md): Click="OnClick" wiring + handler declaration on the
      x:Class owner, routed/bubbling-tunnel/handled, focus/keyboard, async continuation vs async/await (Task/Executor).
- S946 dep:S945  Ch.6 Layout, styles, templates, controls (06-layout-styles-controls.md): shipped control set,
      styling, ControlTemplate authored in VAML.
- S947 dep:S946  Ch.7 Compiled path & release (07-compiled-vbaml-release.md): vamlcc in CMake, InitializeComponent
      analog, dual path, static vs shared, single-exe shipping.
- S948 dep:S947  Ch.8 Threading model (08-threading.md): render thread vs UI thread, marshalling, thread-affinity,
      and why lifetime/ownership differs from C#.
- S949 dep:S948  Ch.9 Full worked example (09-worked-example.md): the sample app (mirrors S440–445) as a
      walkthrough, C# sketch vs VAML C++ side-by-side per section.
- S950 dep:S949  Appendices (appendix-a-glossary.md, appendix-b-cheatsheet.md): C#/XAML term → VAML C++ term
      mapping table + one-page quick reference.
- S951 dep:S950  GUIDE GATE: doc→CI job extracts every code block, compiles+runs it, resolves every API reference
      against a real public header symbol; zero snippets using unimplemented features.
- S952 dep:S951  Ship: mount guide into S928 docs-site skeleton, version-tag with v1.0, index from S923 tutorial docs.
- S953 dep:S952  REFACTOR GATE(S18): docs-only sprint — no dead code; guide build green both platforms; link-check
      green. SURVIVOR + close-out.
- S954–S989  Reserved for guide v2 / extra appendix material.

---
## Reserved / tail (S990–S999)
- Reserved tokens for post-1.0 discoveries and next-plan increments. Append new steps here in
  dependency order as work is scoped; never hand-edit the checkpoint in progress.md.

## End of ROADMAP
