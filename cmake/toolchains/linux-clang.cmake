# ============================================================================
# VAML — Linux Clang toolchain file  (S002)
# Target: Linux host, native Clang (clang++). Since the host IS Linux this is a
# native build; CMAKE_SYSTEM_NAME is set explicitly so cross/host intent is
# unambiguous for CI, and CMAKE_SYSTEM_VERSION defaults to the running kernel.
#
# Used by preset: clang-debug (Linux).
# Governing docs: docs/adr/ADR-LOG.md (ADR-007 CMake canonical).
# ============================================================================

# --- Explicit native Linux target --------------------------------------------
set(CMAKE_SYSTEM_NAME Linux)
if(NOT CMAKE_SYSTEM_VERSION)
    set(CMAKE_SYSTEM_VERSION "" CACHE STRING "Linux kernel version" FORCE)
endif()

# --- C++ compiler: clang++ (must exist on PATH or be supplied via preset) ----
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "clang++" CACHE FILEPATH "Clang C++ compiler (clang++)" FORCE)
endif()

# --- Clang version probe ------------------------------------------------------
# If a triple is not requested, resolve it from the compiler so that -stdlib
# selection and the implicit system include/lib paths are consistent.
if(NOT CMAKE_CXX_COMPILER_TARGET)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --version
        RESULT_VARIABLE clang_version_result
        OUTPUT_VARIABLE clang_version_out
        ERROR_QUIET
    )
endif()

# Keep the default C++ standard library (libstdc++ on GNU/Linux). No override
# here — libc++ is opt-in per target via a later step, not forced by this chain.
