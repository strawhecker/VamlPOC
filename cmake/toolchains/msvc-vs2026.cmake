# ============================================================================
# VAML — MSVC toolchain file  (S002)
# Target: Windows host, native MSVC (cl.exe) from Visual Studio 2026 (VS 18).
# Native build: do NOT set CMAKE_SYSTEM_NAME — CMake must keep native Windows
# detection so the VS environment (vcvars) and MSVC runtime resolve correctly.
#
# Used by presets: msvc-debug, msvc-release.
# Governing docs: docs/adr/ADR-LOG.md (ADR-007 CMake canonical).
# ============================================================================

# --- C++ compiler: prefer cl.exe (already selected by vcvars environment) ---
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "cl" CACHE FILEPATH "MSVC C++ compiler (cl.exe)" FORCE)
endif()

# --- Static CRT, debug-aware (avoids the runtime-DLL deployment problem) ---
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
    "MSVC runtime library selection (static CRT, debug-aware)" FORCE)

# --- Toolset ----------------------------------------------------------------
# No CMAKE_GENERATOR_TOOLSET is pinned here: S001/S002 use the Ninja generator,
# which does not support toolset specification. The active MSVC toolset is set
# by vcvars64.bat (the VS 2026 / v144-series environment), so pinning is both
# unsupported under Ninja and unnecessary. A VS-generator preset may add its own.
