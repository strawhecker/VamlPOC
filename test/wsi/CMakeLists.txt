# test/wsi — S177 RAIL 2: Hidden-window real-WSI resize gate (LOCAL, NOT GH).
#
# `vaml-wsi-resize` is a REAL, non-doctest EXECUTABLE (not a CTest-registered test)
# that drives a real hidden window + real Vulkan surface + a programmatic resize
# storm with validation layers ENABLED — the "any validation error = test failure"
# local pre-✅ gate (build/verify_s177_wsiresize.cmd on THIS box). It is NOT a GH
# CI lane (runners have no WSI surface/display); on a headless host it exits SKIP
# (exit 2). The target still BUILDS green on both CI lanes (compile-only), and the
# local gate forces the RUN.
#
# It links the OPTIONAL framework (vaml-presentation PUBLIC-links vaml-vulkan) so
# it can build the presenter machine + real builders the S167_1 swapchain path uses.
add_executable(vaml-wsi-resize
    WSIResize_main.cpp
)

# Not a library, so the artifact model does not apply; hold it to the same strict
# compile bar as the unit tests.
vaml_apply_compiler_policy(vaml-wsi-resize)

target_include_directories(vaml-wsi-resize PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/vulkan/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/presentation/include
)
target_include_directories(vaml-wsi-resize SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

if(MSVC)
    target_compile_options(vaml-wsi-resize PRIVATE /external:W0)
else()
    target_compile_options(vaml-wsi-resize PRIVATE "-Wno-error=#warnings")
endif()

target_link_libraries(vaml-wsi-resize PRIVATE vaml-presentation)

# ============================================================================
# vaml-wsi-appshell — S446 RAIL 2 LOCAL pre-✅ gate (this box, NOT GH): the
# "zero validation-layer errors over 10k PRESENTED frames" proof against the
# REAL in-repo app-shell on the real driver (Deliverable C of the S446 plan).
# Mirrors vaml-wsi-resize (S177): a REAL, non-doctest EXECUTABLE that drives
# a hidden window + the full real stack (loader->instance->surface->device->
# render pass->the S222 3-pipeline UI scene spec + SDF atlas->the S210 retained
# SceneRenderer->the S173 Presenter) with validation layers ENABLED + a
# VK_EXT_debug_utils messenger that fails on ANY validation error, then runs a
# change-driven loop of 10,000 REAL PRESENTED frames over the RETAINED app-shell
# scene (no per-frame re-encode) + a clean vkDeviceWaitIdle + reverse teardown.
# Exit 0=PASS / 1=FAIL / 2=SKIP (headless). NOT a GH lane (CI has no WSI surface);
# the local gate (build/verify_s446.cmd) forces the RUN.
# ============================================================================
add_executable(vaml-wsi-appshell
    WSIAppShell_main.cpp
    ../../vaml_viewer/app_shell_vm.cpp   # the app-shell VM + S442 composition (run_app_shell_binding_plan)
)

vaml_apply_compiler_policy(vaml-wsi-appshell)

target_include_directories(vaml-wsi-appshell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/vulkan/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/presentation/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer   # app_shell_binding.h seam (the --app launch)
)
target_include_directories(vaml-wsi-appshell SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

if(MSVC)
    target_compile_options(vaml-wsi-appshell PRIVATE /external:W0)
else()
    target_compile_options(vaml-wsi-appshell PRIVATE "-Wno-error=#warnings")
endif()

target_link_libraries(vaml-wsi-appshell PRIVATE vaml-presentation)

# NOT registered with CTest — the local gate (verify_s446.cmd) runs it on a
# display-backed host and treats SKIP(2)/PASS(0)/FAIL(1) explicitly (the S177
# contract). The target builds green on both CI lanes (compile-only); the RUN is
# the local gate's job.

# NOT registered with CTest — the local gate (verify_s177_wsiresize.cmd) runs it on
# a display-backed host and treats SKIP(2)/PASS(0)/FAIL(1) explicitly. Registering it
# with CTest would make CI attempt it headless (where it SKIPs) — a noise source;
# the target builds everywhere, the RUN is the local gate's job.
