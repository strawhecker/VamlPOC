# VAML session-context (per-run state)

## last_command
implementit — S222 (dep:S221) MVP "soup-to-nuts" render host, completed + verified + LIVE-proven.

## plan_digest
S222 complete + verify + docs: (A) build/verify_s222.cmd lane (msvc-debug + ctest 3/3 + vaml_viewer build + CLI proofs 0/2/1/3 never-silent + LIVE soup-to-nuts window close=0), (B) GREEN BOTH CI LANES from first principles (windows-msvc + ubuntu-clang incl. vaml_viewer-linux), (C) roadmap S222 [DONE] + progress checkpoint -> S222_1 + session-context. Scope: reload=S222_1, pan/zoom/--help=S224, drag-drop=S223, real control materials=S400+/S401.

## files_touched
- ADDED build/verify_s222.cmd
- MOD src/vulkan/include/vaml/vulkan/Swapchain.h (+create/rebuild oldSwapchain param)
- MOD src/vulkan/src/Swapchain.cpp (create + rebuild thread oldSwapchain into vkCreateSwapchainKHR; pOffsets not applicable here)
- MOD src/vulkan/include/vaml/vulkan/SwapchainState.h (build seam gained VkSwapchainKHR param)
- MOD src/vulkan/src/SwapchainState.cpp (real builder passes active handle as oldSwapchain; create/prepare_recreate thread it)
- MOD src/vulkan/src/ui/UiScene.cpp (vkCmdBindVertexBuffers passes a real pOffsets — null was UB/NVIDIA AV)
- MOD src/presentation/src/Presenter.cpp (restored clean; no functional change — resize block + no debug traces)
- MOD src/presentation/src/VamlRenderer.cpp (cleaned debug traces; no net change)
- MOD vaml_viewer/app.{h,cpp} (owned loader_ FIRST/LAST member; invalidate() publish after set_root; run_loop request_resize only on change; portable VAML_VIEWER_AUTO_CLOSE_MS getenv; NO debug traces)
- MOD vaml_viewer/main.cpp (NON-const LoadResult moved into create)
- MOD vaml_viewer-linux/src/main.cpp (NON-const LoadResult)
- MOD test/unit/MockIcd_test.cpp, Msaa_test.cpp, SwapchainGeneration_test.cpp (build mock lambda 3-arg signature)
- MOD docs/roadmap.md (S222 [DONE] write-up)
- MOD progress.md (checkpoint -> S222_1; Sprint 5a S222 SURVIVOR)
- MOD session-context.md (this file)

## build_status
GREEN both CI lanes (from first principles):
- windows-msvc: build/verify_s222.cmd — msvc-debug warning-as-error CLEAN + ctest 3/3 PASS (incl. ViewerRenderPlan 6 headless cases) + vaml_viewer Debug/x64 clean + CLI proofs 0/2/1/3 never-silent + LIVE soup-to-nuts window close=0 on NVIDIA RTX A5000.
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — configure/build warning-clean + ctest 3/3 PASS + vaml_viewer-linux builds clean under clang 21.1.8.
windows-clang-debug is NOT a lane (pre-existing /Zc:preprocessor failure; not built/tested).

## next_step
S222_1 (dep:S222) — file-watch live reload (the viewer's permanent visual-feedback vehicle): on file mtime change → VamlReader::Load again → re-render. mtime-poll + notify seam both load_file-headless-ctestable. [VIEWER GROWTH: reload]

## notes
- Real LIVE-proven fixes authored at S222 (a real window + NVIDIA driver surfaced them): (1) owned loader_ lifetime; (2) S167_1 two-generation oldSwapchain threading; (3) vkCmdBindVertexBuffers real pOffsets; (4) request_resize only on change; (5) NON-const LoadResult in both main.cpp. Every fix is in the committed scope of S222 (the viewer cannot go green live without them).
- No commits made (per standing directive unless 'commitit' is explicitly commanded).
- No NEW md files created (only the existing operational files roadmap/progress/session-context were updated).
