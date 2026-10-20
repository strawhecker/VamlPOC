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

# NOT registered with CTest — the local gate (verify_s177_wsiresize.cmd) runs it on
# a display-backed host and treats SKIP(2)/PASS(0)/FAIL(1) explicitly. Registering it
# with CTest would make CI attempt it headless (where it SKIPs) — a noise source;
# the target builds everywhere, the RUN is the local gate's job.
