// vaml_viewer — DRAG-AND-DROP → reopen seams (S223, dep:S222_1).
//
// S223's contract: `PlatformWindow` (S160) gains a FILE-DROP callback (GLFW drop
// path — `glfwSetDropCallback` covers BOTH Win32 + X11); the viewer REOPENS the
// dropped `.vaml`; and the drop-dispatch → reopen-handler seam is
// HEADLESS-TESTABLE. This header is the viewer half of that seam — a PURE
// DECISION layer, mirroring the S222_1 `live_reload.h` split (pure inline
// decision table + the REAL coordinator in the app), so it is proven headless
// (ctest without a live window) exactly like `reload_action`.
//
// The split:
//   * PURE seams here (inline, header-only — the S222_1 `reload_action`
//     precedent): `is_vaml_drop` (the extension guard) + `drop_choice` (the
//     dispatch decision: which dropped path reopens).
//   * The REAL reopen HANDLER lives in the app: `ViewerApp::reopen(...)` — a
//     thin public wrapper over the S222_1 `apply_reload` (the whole-S5-path
//     re-adoption machinery). Drop-reopen and file-watch reload share ONE
//     reopen path (no drift). The app wires the GLFW on_drop into a pending
//     slot it drains in run_loop (ADR-018: single-threaded, UI thread, no locks).
//
// Reopen = load_file(chosen) -> reopen(root): the load half is the S221
// `load_file` seam (already golden-proven → live tree); the apply half is
// `apply_reload` (golden-proven by the S222_1 LiveReload "edit → reload → same
// tree"). Nothing new needs a window — headless-testable.
//
// NO MINIMAL REAL introduced; drop is independent of S323/S324 (routed events +
// hit-testing), which later layers onto the input hooks, not the drop path.

#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace vaml::viewer {

// The EXTENSION GUARD (pure): is `path` a `.vaml` file (case-INSENSITIVE —
// Windows Explorer / the X11 file manager may hand us ".VAML")? A dropped
// non-`.vaml` must NEVER be reopened (stray files dragged over the window are
// ignored — we never render garbage). Pure + deterministic.
inline bool is_vaml_drop(const std::string& path) noexcept
{
    const std::size_t n = path.size();
    if (n < 5)
        return false;                       // too short to end in ".vaml"
    const char c0 = path[n - 5];            // '.'
    const char c1 = path[n - 4];            // v
    const char c2 = path[n - 3];            // a
    const char c3 = path[n - 2];            // m
    const char c4 = path[n - 1];            // l
    const bool dot   = (c0 == '.');
    const bool v     = (c1 == 'v' || c1 == 'V');
    const bool a     = (c2 == 'a' || c2 == 'A');
    const bool m     = (c3 == 'm' || c3 == 'M');
    const bool l     = (c4 == 'l' || c4 == 'L');
    return dot && v && a && m && l;
}

// The DISPATCH decision (pure): given a GLFW drop payload (`count` absolute
// UTF-8 paths), pick the FIRST `.vaml` path to reopen, or nullopt when the drop
// contains NO `.vaml` (the drop is ignored — nothing reopens). This is the
// headless-testable "drop → which file reopens" selection. Pure + deterministic.
inline std::optional<std::string> drop_choice(int count, const char* const* paths) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        if (paths && paths[i] && is_vaml_drop(paths[i]))
            return std::string(paths[i]);
    }
    return std::nullopt;
}

} // namespace vaml::viewer
