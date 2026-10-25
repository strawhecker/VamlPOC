# VAML session-context (per-run state)

## last_command
implementit — SPRINT 7 CLOSED (S289 -> S290 -> S291 -> S293 -> S292) COMPLETE. The VBAML pipeline (text -> vamlcc -> .vbaml -> VBamlReader -> tree) is end-to-end GREEN on BOTH CI lanes AND demonstrated LIVE in the viewer's compiled path. S289 (pack/identity: PackUri + ResourcePack + the compiled-only InitializeComponentFrom + x:Class owner mapping, "release uses compiled only") + S290 (dual-path contract: S2=dev / S7=release, documented + grep-gated) + S291 (REFACTOR GATE(S7): full pipeline parity, no stubs) + S293 (VIEWER-GROWTH compiled path: viewer opens a compiled .vbaml via the runtime VBamlReader; reload recompiles .vaml->.vbaml then reopens) all DONE from first principles, closed by the S292 SURVIVOR (checkpoint advanced to S320, Sprint 8).

## plan_digest
Sprint 7 CLOSE-OUT (S289 -> S290 -> S291 -> S293 -> S292, execution order; the S293 growth token before the S292 survivor).
- S289 (pack/identity): NEW src/tools/include/vaml/tools/Pack.h + src/tools/src/Pack.cpp (vaml::tools::pack: PackUri + parse_pack_uri + the AUTHORITY RULE + ResourcePack the EMBEDDED-RESOURCE LOOKUP + resolve_pack_uri) + NEW VBamlReader surface (SetOwner/owner() + InitializeComponent + InitializeComponentFrom THE COMPILED-ONLY RELEASE ENTRY (owner, ResourcePack&, packUri) — never text + register_class/is_registered_class the x:Class owner mapping + new VBamlError::OwnerClassMismatch). Resolves the S092/S286 "x:Class -> owner binding deferred" NO-GAP. "release uses compiled only" enforced by signature; S444 app-shell = the named release consumer (recorded not silent). NEW test/unit/Pack_s289_test.cpp (15 cases).
- S290 (dual-path): DUAL-PATH CONTRACT blocks in vaml/core/VamlReader.h + vaml/tools/VBamlReader.h: S2 = DEV TOOL, S7 = RELEASE; the compiled path is text-free by construction (grep audit: VBamlReader.cpp + Pack.cpp have ZERO text-reader/vamlcc calls). Split enforced at LINKAGE (no ReaderRole enum = dead surface). Docs in roadmap + progress only.
- S291 (REFACTOR GATE(S7)): both lanes rebuilt + ctest 4/4 from first principles; S288 parity re-run; grep-audit no stubs; S003 tools sentinel confirmed resolved (removed at S287); dead-surface audit (every S7 symbol consumed); ASan no-leak; no MINIMAL REAL introduced/resolved at the gate.
- S293 (VIEWER-GROWTH compiled path): NEW vaml_viewer/compiled_open.h (CompiledOpenResult + load_compiled_bytes + compile_to_vbaml + load_compiled_file — in-process vamlcc, ADR-021) + MOD app.{h,cpp} (create_impl shared stack-builder + create_compiled + the compiled S222_1 reload recompiles-then-reopens keeping last-good) + MOD cli.h (--compiled + cli_wants_compiled) + both main.cpp (compiled dispatch) + the viewer (Windows vcxproj + Linux CMakeLists) gains the vaml-tools link. Compile capture via vaml_viewer-linux on the VM. NEW tests ViewerCompiledOpen_s293_test.cpp (7) + ViewerCompiledPlan_s293_test.cpp.
- S292 (SURVIVOR + checkpoint to S320): docs-only wrap; Sprint 7 SURVIVOR + VIEWER-GROWTH table S7->S293 DONE + checkpoint regenerated (last=S292, next=S320) + guard rails (tools RESOLVED at S287) + ADR-018 reaffirmed (zero new locks) + roadmap S289/S290/S291/S293/S292 [DONE].

## files_touched
- ADDED src/tools/include/vaml/tools/VBamlReader.h + src/tools/src/VBamlReader.cpp (S285)
- ADDED src/tools/include/vaml/tools/parity/DualPathParity.h + src/tools/src/parity/DualPathParity.cpp (S288)
- MOD src/tools/include/vaml/tools/Vamlc.h + src/tools/src/Vamlc.cpp (S287 embed: write_embed + sanitize_embed_symbol)
- MOD src/tools/vamlcc_main.cpp (S287 --embed mode)
- MOD src/tools/include/vaml/tools/version.h + src/tools/src/version.cpp (S287 tools-marker RESOLVED: sentinel removed, symbol extended)
- MOD src/tools/CMakeLists.txt (S287 vaml_compile_vaml() + demo wiring + S285/S288 sources; S288 export vars)
- ADDED src/tools/samples/demo_main.vaml (S287 demo)
- MOD test/unit/CMakeLists.txt (+Vamlc_s284 +VBamlReader_s285 +VBamlReader_s286 +Vamlc_s287 +VBamlReader_s288_parity; links the generated embed .cpp)
- ADDED test/unit/Vamlc_s284_test.cpp + VBamlReader_s285_test.cpp + VBamlReader_s286_test.cpp + Vamlc_s287_test.cpp + VBamlReader_s288_parity_test.cpp
- ADDED build/verify_s284.cmd (S284..S288 gate) — verify_s288.cmd consolidated gate
- MOD docs/roadmap.md (S284..S288 [DONE] + Sprint 7 complete marker)
- MOD progress.md (checkpoint -> S289/Sprint 8)
- MOD session-context.md (this file)
- No NEW md files (operational files only, per protocol scope).
- ADDED src/tools/include/vaml/tools/Pack.h + src/tools/src/Pack.cpp (S289) + MOD src/tools/CMakeLists.txt (+Pack.cpp)
- MOD src/tools/include/vaml/tools/VBamlReader.h + src/tools/src/VBamlReader.cpp (S289: SetOwner + InitializeComponent + InitializeComponentFrom + register_class/is_registered_class + OwnerClassMismatch; S290 dual-path contract doc)
- MOD src/core/include/vaml/core/VamlReader.h (S290 dual-path contract doc)
- ADDED vaml_viewer/compiled_open.h (S293) + MOD vaml_viewer/app.{h,cpp} (create_impl/create_compiled + compiled reload) + MOD vaml_viewer/cli.h (--compiled/cli_wants_compiled) + MOD vaml_viewer/main.cpp + vaml_viewer-linux/src/main.cpp (compiled dispatch) + MOD vaml_viewer/vaml_viewer.vcxproj + vaml_viewer-linux/CMakeLists.txt (vaml-tools link)
- ADDED test/unit/Pack_s289_test.cpp + ViewerCompiledOpen_s293_test.cpp + ViewerCompiledPlan_s293_test.cpp; MOD test/unit/CMakeLists.txt (all three + the presentation-tests tools link/include)
- ADDED build/verify_s289.cmd (the S289..S291 consolidated windows-msvc gate incl. the viewer build)
- MOD docs/roadmap.md (S289/S290/S291/S293/S292 [DONE] + Sprint 7 (S280..S293) complete marker + tools marker RESOLVED at S287)
- MOD progress.md (Sprint 7 SURVIVOR + checkpoint -> S320/Sprint 8)
- MOD session-context.md (this file)

## build_status
SPRINT 7 CLOSE-OUT (S289..S293) GREEN BOTH CI LANES from first principles:
- windows-msvc: build/verify_s289.cmd (msvc-debug) — configure clean + build warning-as-error (/WX) CLEAN + FULL ctest 4/4 100% PASS incl. the S289 15-case Pack suite (vaml-tools-tests) + the S293 7-case ViewerCompiledOpen suite + the S293 ViewerCompiledPlan suite (vaml-presentation-tests) + all prior S280..S288 suites; the vaml_viewer.vcxproj builds green (Debug|x64, vaml-tools.lib linked). 15/15 + 7/7 + plan cases green on the direct exe run.
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — configure clean + build warnings==0 + ctest 4/4 100% PASS (same suites). The vaml_viewer-linux target builds green with the vaml-tools link on the VM.
- The vml viewer LIVE compiled-path proof (vaml_viewer --compiled sample.vaml -> compile -> render -> mid-run rewrite recompiles + reopens) is [both apps, not CI] on the NVIDIA box (standing S222 display-proof precedent).
windows-clang-debug is NOT a lane (pre-existing failure; not built/tested).

## next_step
S320 (dep:S292) — Sprint 8: RoutedEvent primitive (Signal<T> + token + route-policy hook; reentrancy-safe add/remove). Sprint 7 (S280..S293) is fully delivered + verified + wrapped by the S292 SURVIVOR; the checkpoint in progress.md + roadmap.md is regenerated to S320 (Sprint 8).

## notes
- The compiled-path reader (VBamlReader) + Pack form the S7 RELEASE path (vaml-tools; core cannot depend on tools); the S2 text VamlReader is the DEV TOOL — the dual-path split (S290) is enforced at LINKAGE, documented in both reader headers + roadmap/progress. Confirmed at S288+S291 that S7 == S2 behaviorally (parity, not duplication).
- "Release uses compiled only" (S289) is enforced by SIGNATURE (InitializeComponentFrom takes a ResourcePack + pack URI, never text) + at linkage (release links vaml-tools for VBamlReader, never the text reader); S444 app-shell = the named release consumer (recorded NO-GAP not silent).
- The S003 tools MINIMAL REAL marker is RESOLVED at S287 (sentinel removed; version symbol kept + extended; grep-audited) — recorded NO-GAP at its own step.
- {Binding} resolution on the compiled path stays deferred (release consumer, S444/S322) — recorded NO-GAP at S286/S288/S293. The viewer compiled mode renders the real panels via the S212 classifier.
- ADR-018 ZERO locks across Sprint 7 (both readers + Pack single-threaded/pure; the viewer compiled seam is UI-thread); ADR-021 headless (tools tests + viewer tests drive the REAL seam, never spawn an exe); ADR-011 tools stays CORE (no Vulkan).
- The viewer compiled-open seam (compiled_open.h) is header-only REAL permanent code (kept-and-extended by S444), NOT MINIMAL REAL.
- No commits made (per standing directive unless 'commitit' is commanded).
- No NEW md files created (operational files only).
