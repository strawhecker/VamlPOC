# VAML session-context (per-run state)

## last_command
implementit — SPRINT 7 (S280..S288) COMPLETE. The VBAML pipeline (text -> vamlcc -> .vbaml -> VBamlReader -> tree) is end-to-end GREEN on BOTH CI lanes. S284 goldens + S285 VBamlReader runtime + S286 deferred surface + S287 vamlcc CMake integration/embed (S003 tools-marker RESOLVED) + S288 dual-path parity harness all DONE from first principles. The S284..S288 continuation delivered: the compiled-path runtime reader, its deferred surface, the CMake build integration (sidecar + embedded twin), the tools-marker resolution, and the dual-path parity proof.

## plan_digest
Sprint 7 S284..S288 (VBAML goldens -> runtime reader -> CMake integration -> parity).
- S284 (goldens): NEW test/unit/Vamlc_s284_test.cpp (7 TEST_CASEs) — the formal golden gate over compile_to_bytes(): the produced TOKEN STREAM asserted DECODED FROM REAL BYTES (magic 0x76424D4C + version 1; decode_schema type/prop/event/resource tables + string table; element-id records in document order + child edges; attribute flag byte isExtension/isBinding + binding path + target-prop token; string-table content-dedup (once, VarU32-indexed only); event table Click/Changed + resource table Brush/Color; the S282 re-proof (decode structural-equal + re-encode BYTE-IDENTICAL); positive/negative controls). TEST-ONLY, zero product symbols.
- S285 (VBamlReader runtime): NEW src/tools/include/vaml/tools/VBamlReader.h + src/tools/src/VBamlReader.cpp + test/unit/VBamlReader_s285_test.cpp. The compiled-path "S7 reader" (vaml-tools): Load(bytes) decodes + builds a live DependencyObject tree InitializeComponent-style (S087 factory, S089 converter, S032-039 property table; literal props set; post-order retained_; structured VBamlError/VBamlException taxonomy).
- S286 (deferred surface): extended VBamlReader + test/unit/VBamlReader_s286_test.cpp. x:Name/x:Class/x:Key registries (DuplicateName/ClassOnNonRoot/root_class), {Binding} DeferredExtensions recorded verbatim (NOT resolved), register_event -> VBamlDeferredEventHandler, per-Load clearing, error-taxonomy distinctness.
- S287 (vamlcc in CMake + embed + marker RESOLVED): vaml_compile_vaml() CMake function (sidecar .vbaml + optional embedded C++ pair via $<TARGET_FILE:vamlcc>), vamlcc --embed mode + write_embed()/sanitize_embed_symbol(), demo_main.vaml via the custom command, test/unit/Vamlc_s287_test.cpp (5 cases; compiles the ACTUAL generated embed .cpp). RESOLVES S003 tools MINIMAL REAL marker: version.h sentinel REMOVED, real version()/tools_name() KEPT + extended. Both lanes verified the custom command produced demo_main.{vbaml,h,cpp}.
- S288 (dual-path parity): NEW src/tools/include/vaml/tools/parity/DualPathParity.h + src/tools/src/parity/DualPathParity.cpp + test/unit/VBamlReader_s288_parity_test.cpp. compare_dual_path/compare_dual_path_props drive the SAME source through the S2 text reader AND the S7 compiled reader and assert behavioral identity per axis (root type, retained order/types, parent chain, x:Name registry, deferred bindings, deferred events, literal-prop converted values incl. attached). Release-consumer contract: S7 = release path, S2 = dev tool; parity, not duplication.

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

## build_status
SPRINT 7 (S280..S288) GREEN BOTH CI LANES from first principles:
- windows-msvc: build/verify_s288.cmd (msvc-debug) — configure clean + build warning-as-error (/WX) CLEAN + FULL ctest 4/4 100% PASS (vaml-tools-tests incl. all S284..S288 suites: S284 7 cases, S285 4 cases, S286 7 cases, S287 5 cases, S288 4 cases; + vaml-core-tests + vaml-vulkan-tests + vaml-presentation-tests). The CMake vaml_compile_vaml() custom command + vamlcc --embed produced build/msvc-debug/src/tools/demo_main.{vbaml,h,cpp} and the S287 test compiled the ACTUAL generated embed .cpp.
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — configure clean + build warnings==0 + ctest 4/4 100% PASS (same S284..S288 suites + core/vulkan/presentation). Same pipeline produced demo_main.{vbaml,h,cpp} on linux.
- Note: an ENVIRONMENT C1060 ("out of heap space") struck the windows lane midway through S287 — root cause was NO-PAGEFILE + exhausted commit (FreeVirtualMemory ~20GB/127GB, 25GB memory-compression + many devenv), NOT a code defect; the toolchain is 64-bit (Hostx64\x64 cl, x64 OS). Resolved by freeing memory + a full rebuild; both lanes re-verified green.
windows-clang-debug is NOT a lane (pre-existing failure; not built/tested).

## next_step
S289 (dep:S288) — Sprint 8: the first consumer of the compiled .vbaml (the embedded span) at the TRACE/app layer, feeding the single-exe path (S484). Sprint 7 (S284..S288) is fully delivered + verified; the checkpoint in progress.md + roadmap.md is regenerated to S289.

## notes
- The compiled-path reader (VBamlReader) is the "S7 reader" and lives in vaml-tools (core cannot depend on tools); it is PERMANENT (NOT MINIMAL REAL, NOT a stub) — the release reader. Confirmed at S288 that S7 == S2 behaviorally (parity, not duplication).
- The S003 tools MINIMAL REAL marker is RESOLVED at S287 (sentinel removed; version symbol kept + extended; grep-audited) — recorded NO-GAP at its own step.
- {Binding} resolution on the compiled path stays deferred (release consumer / S252 analogue) — recorded NO-GAP at S286/S288.
- ADR-018 zero locks (both readers single-threaded/pure); ADR-021 headless (vaml-tools-tests drives both real readers, never spawns an exe); ADR-011 tools stays CORE (no Vulkan).
- No commits made (per standing directive unless 'commitit' is commanded).
- No NEW md files created.
