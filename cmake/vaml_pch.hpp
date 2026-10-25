// vaml_pch.hpp — shared precompiled-header root for MSVC (msvc-debug lane).
//
// S440 (Sprint 11): precompiled headers for MSVC. Without a PCH, every TU
// re-parses the standard-library headers (the STL is the dominant repeated
// parse across the tree — <vector>/<string>/<memory>/<optional>/<functional>
// are included by every VAML TU, and each STL header is large), so a full
// msvc-debug build spends most of its time re-lexing STL. A shared PCH (the
// frequency-ranked stable standard includes actually used across the tree)
// cuts that cost dramatically.
//
// IMPORTANT (S440 rationale):
//   - PCH is a BUILD-SPEED optimization ONLY. It never changes observable
//     semantics: every TU is still compiled from its own .cpp under the same
//     strict /W4 /WX policy; the PCH only amortizes the repeated include parse.
//   - The shared PCH is compiled ONCE per target but reused across many TUs;
//     therefore it may contain ONLY headers resolvable in EVERY target's
//     context. This is why it is STANDARD-LIBRARY-ONLY: vaml/core and other
//     vaml headers are NOT safe here, because a core-layer target (e.g. vamlcc,
//     the tool) compiles with only its own include path and cannot see
//     <vaml/core/...>. A shared PCH that required them would fail to build
//     those targets. STL is universally available, so this PCH is safe for all.
//   - clang-cl preset is NOT a CI lane; PCH is applied to MSVC targets only via
//     cmake/pch.cmake (MSVC toolset), so no other lane's behavior changes.
#ifndef VAML_PCH_HPP
#define VAML_PCH_HPP

// --- standard library (stable, header-only, universally resolvable) ---
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#endif // VAML_PCH_HPP
