# ============================================================================
# VAML — Windows Clang toolchain file  (S002, Alternative B)
# Target: Windows host, Clang with the MSVC ABI (clang-cl). This produces
# MSVC-compatible binaries while using the Clang front end — the ADR-007
# "MSVC + Linux Clang" matrix, plus a Windows-Clang lane for extra coverage.
#
# Native build: do NOT set CMAKE_SYSTEM_NAME (host is Windows).
# Used by preset: windows-clang-debug.
# ============================================================================

# --- C++ compiler: clang-cl (must be on PATH, e.g. via LLVM install) ---------
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "clang-cl" CACHE FILEPATH "Clang C++ compiler (clang-cl, MSVC ABI)" FORCE)
endif()

# --- Static CRT, debug-aware (consistent with the MSVC toolchain) -------------
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
    "MSVC runtime library selection (static CRT, debug-aware)" FORCE)

# clang-cl accepts the same conformance flags the policy applies to MSVC, and
# uses MSVC-compatible defines/triples. No further configuration required.
