# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S087 (no commits; no NEW md files)
- plan_repo_state : S087 complete — **BOTH CI LANES GREEN** (verified from first principles):
                    msvc-debug (VS 2026) via build/verify_s087.cmd configure+build warning-clean +
                    ctest 1/1 PASS; ubuntu-clang (Ubuntu VM, clang 21, shared folder
                    /mnt/hgfs/VamlPOC) configure+build + ctest 1/1 PASS. No commit+push (per command).
                    S087 delivers the ElementFactoryRegistry: typeName→factory<unique_ptr<
                    DependencyObject>> + startup registration, the machinery S088 (Resolve
                    "Button"→instance) and S091 (Runtime VamlReader builds children) consume.
                    Registry is real and tested: name-unique dedupe (duplicate → ElementAlreadyRegistered,
                    registry UNCHANGED, factory never swapped), try_create (noexcept → nullptr) /
                    create (throws ElementNotFound), is_registered, size/empty, registered_names in
                    INSERTION ORDER (deterministic). Global Meyers singleton matches
                    PropertyRegistry/DependencyProperty::global; stores an OWNED name copy.
                    Nine built-in element seeds are introduced as MINIMAL REAL concrete
                    DependencyObject subclasses (Button/StackPanel/Grid/Text/TextBox/CheckBox/Slider/
                    ScrollViewer/Window — identity/type only, tagged with searchable
                    `// [MINIMAL REAL] Element <X> — RESOLVED BY S###` sentinels) and registered at
                    startup by register_builtin_elements() (idempotent). STARTUP GATE tested:
                    create("Button") etc. yields the concrete type (dynamic_cast non-null), global
                    size == 9. All resolution sprints already declared in roadmap.md guard rails.
- plan_digest     : S087 (dep:S086). ADDED src/core/include/vaml/core/ElementFactoryRegistry.h +
                    src/core/src/ElementFactoryRegistry.cpp (pimpl'd registry; ElementFactory =
                    std::function<unique_ptr<DependencyObject>()>; exceptions ElementAlreadyRegistered /
                    ElementNotFound mirror the S032 taxonomy; global() Meyers singleton). ADDED
                    src/core/include/vaml/core/Element.h + src/core/src/Element.cpp (MINIMAL REAL
                    element seeds + register_builtin_elements()). ADDED test/unit/
                    ElementFactoryRegistry_test.cpp (10 Shouldly TEST_CASEs incl. STARTUP GATE +
                    idempotency, no IMPLEMENT_WITH_MAIN). ADDED build/verify_s087.cmd. MOD
                    src/core/CMakeLists.txt (+2 .cpp), test/unit/CMakeLists.txt (+1 test TU),
                    docs/roadmap.md (S087 [DONE] + active MINIMAL REAL element-seed marks),
                    session-context.md (this).
- files_touched   :
    ADDED    src/core/include/vaml/core/ElementFactoryRegistry.h
    ADDED    src/core/src/ElementFactoryRegistry.cpp
    ADDED    src/core/include/vaml/core/Element.h
    ADDED    src/core/src/Element.cpp
    ADDED    test/unit/ElementFactoryRegistry_test.cpp     (10 S087 cases)
    ADDED    build/verify_s087.cmd
    MODIFIED src/core/CMakeLists.txt                       (+ ElementFactoryRegistry.cpp, Element.cpp)
    MODIFIED test/unit/CMakeLists.txt                      (+ ElementFactoryRegistry_test.cpp)
    MODIFIED docs/roadmap.md                               (S087 [DONE] + MINIMAL REAL seed marks)
    MODIFIED session-context.md (this — S087 completion)
- build_status    : GREEN — BOTH CI LANES. windows-msvc: msvc-debug (VS 2026) via build/verify_s087.cmd —
                    configure+build warning-clean + ctest 1/1 PASS (301 cases green, 10 new S087).
                    ubuntu-clang: clang-debug (Ubuntu VM 192.168.198.129, clang 21, /mnt/hgfs/VamlPOC)
                    configure+build + ctest 1/1 PASS. Only Clang warnings = pre-existing upstream
                    <ciso646> doctest deprecation from third_party (not this TU; non-error).
                    (windows-clang-debug is NOT a lane — not built/tested/flagged per standing rule.)
- next_step       : S088. S088 = Resolve "Button"→instance; unknown type → structured error naming
                    type+line. S087 proved typeName→creation; S088 wires that to the parsed tree —
                    consuming S085's ns/prefix metadata + the S087 registry (create("Button")), and
                    mapping the registry's ElementNotFound into a line-carrying reader error type.
- notes           :
    - S087 introduces REAL product surface (registry + element seeds); the seeds are MINIMAL REAL
      per the Guard Rails — resolution sprints already declared in roadmap.md, no silent backfill,
      no silent extension (real control logic is each element's OWN sprint: Button→S401, StackPanel
      →S129, Grid→S131, Text→S400, TextBox→S402, CheckBox→S404, Slider→S403, ScrollViewer→S405,
      Window→S440).
    - NO-GAP: the unknown-type structured error (naming type+line) is S088's step — the S087 registry
      only throws ElementNotFound; S088 maps it to the line-carrying reader error (recorded, not silent).
    - try_create's noexcept swallows a throwing factory into a nullptr result — recorded in the header;
      a throwing element-construction failure is a bug surfaced at the element's own resolution sprint,
      never silently masked past the S087 probe contract.
    - registry stores OWNED name copies (same SSO/dangle discipline as S036/S039 — callers may pass a
      transient std::string_view).
    - NO NEW md files (directive honored). No commits made (per command).
    - COMMITIT STATUS: commit+push of S032..S044 (incl. S036_1..S036_5), S045, S046..S049, and S080,
      S081, S082, S083, S084, S085, S086, and now S087 is outstanding, awaiting your authorization.
      After commit+push: repo is the committed good state; next step S088 (Sprint 2). Laptop-added
      files uncommitted as well — one commit covers all.

