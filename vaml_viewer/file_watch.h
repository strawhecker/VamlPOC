// vaml_viewer — the FILE-WATCH mtime-POLL + NOTIFY seam (S222_1, dep:S222).
//
// The viewer's PERMANENT visual-feedback vehicle: on a `.vaml` file's mtime
// change the app re-Loads the file (vaml::load_file, S221) and re-runs the whole
// S5 render path with ZERO imperative tree-building ("render live VAML like an
// app would, without a code-behind"). This early hot-reload loop is mtime-POLL
// (a simple, dependency-free, cross-platform watch) + a NOTIFY SEAM (the app-side
// Hook). BOTH are `load_file`-headless-ctestable — the golden fixture proof
// (edit → reload → same tree) lives in LiveReload_test.cpp. S825 MATURES this
// loop (async reload + tooling integration), it does NOT re-do it.
//
// SPLIT (the S162/S200/S206/S222 standing precedent):
//   * PURE DECISION SEAMS (headless-testable; NO std::filesystem dependency) —
//       FileEvent / FileViewEntry + file_view_state_transition(prev, cur): the
//         deterministic mtime-poll DECISION TABLE (Unchanged/Changed/Appeared/
//         Disappeared), proven entirely without touching the filesystem.
//       stamp_to_millis(file_time_type) — the portable last_write_time →
//         epoch-millis conversion (MSVC + clang/libstdc++).
//   * `FileWatcher` — the REAL live watch object (RAII, move-only): polls a
//     file's mtime via the I/O seams, applies the pure transition, RE-ARMS, and
//     fires the notify seam. This is the mtime-poll half.
//
// NO MINIMAL REAL introduced/resolved: every symbol here is REAL permanent code
// (the early hot-reload loop, kept-and-extended). ADR-018: ZERO new locks — the
// watcher is single-threaded, driven off the viewer's UI thread in run_loop().

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace vaml::viewer {

// A file's last-write "stamp" is an epoch-millis integer (see stamp_to_millis).
// kNoStamp is the sentinel for a missing/unreadable file ("has no stamp").
using FileStamp = std::int64_t;
inline constexpr std::int64_t kNoStamp = -1;

// The deterministic mtime-poll EVENT — the decision surface a watcher emits.
//   * Unchanged    — same state (both absent, or both present with an equal stamp).
//   * Changed      — the file exists and its mtime changed (the edit → reload case).
//   * Appeared     — was absent, now exists (created / restored).
//   * Disappeared  — was present, now absent (deleted / a transient editor save).
enum class FileEvent
{
    Unchanged = 0,
    Changed,
    Appeared,
    Disappeared,
};

// The observed state of a watched file at one instant. `exists` + `stamp` are
// kept independent so the transition table can distinguish an absent file from
// one whose stamp we could not read (both land as "absent" today — recorded at
// file_view_state_transition — but the shape keeps the decision explicit).
struct FileViewEntry
{
    FileStamp stamp = kNoStamp;  // kNoStamp when the file is absent/unreadable
    bool      exists = false;
};

// The PURE mtime-poll DECISION TABLE: maps (prev, cur) views -> the FileEvent.
// Deterministic + headless — the ctest suite proves the whole surface with NO
// std::filesystem dependency:
//   absent + absent            -> Unchanged
//   present + present, equal   -> Unchanged
//   present + present, differs -> Changed
//   absent  + present          -> Appeared
//   present + absent           -> Disappeared
inline FileEvent file_view_state_transition(const FileViewEntry& prev,
                                            const FileViewEntry& cur) noexcept
{
    if (!prev.exists && !cur.exists)
        return FileEvent::Unchanged;
    if (prev.exists && cur.exists)
        return (prev.stamp == cur.stamp) ? FileEvent::Unchanged : FileEvent::Changed;
    return cur.exists ? FileEvent::Appeared : FileEvent::Disappeared;
}

// Converts a std::filesystem last_write_time to a coarse millis integer
// (portable across MSVC + clang/libstdc++). The value is a stable, comparable
// stamp (no epoch anchor is needed — the watcher compares equality: a content
// change that bumps the mtime always flips the stamp). PURE (no filesystem I/O).
inline std::int64_t stamp_to_millis(const std::filesystem::file_time_type& t) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               t.time_since_epoch())
        .count();
}

// Returns the file's mtime as a FileStamp (kNoStamp on failure) or std::nullopt
// when the file does not exist / cannot be stat'd. NO throw (the I/O seam).
[[nodiscard]] std::optional<FileStamp> file_mtime(const std::string& path) noexcept;

// The single I/O observation the watcher uses: the file's {stamp, exists} view.
// Missing/unreadable -> {kNoStamp, exists=false}. NO throw.
[[nodiscard]] FileViewEntry view_file(const std::string& path) noexcept;

// The REAL mtime-poll watch object (RAII; move-only — the S170 discipline).
//   * begin(path) — records the current view as the baseline (or the missing
//     state) and arms the watch.
//   * poll()      — re-observes the file, applies the pure transition to the
//     stored previous view, RE-ARMS (the observed state becomes the new baseline,
//     so a repeat poll of the same state is Unchanged), fires the notify seam for
//     any non-Unchanged event, and returns the event.
//   * THE NOTIFY SEAM: on_event(std::function<bool(FileEvent)>) — the app-side
//     hook invoked by poll() for any non-Unchanged event. The return value is
//     "swallow+continue" (true) vs "stop further diffing this frame" (false);
//     the watcher stays armed for the next poll regardless. Default no-op.
class FileWatcher
{
public:
    FileWatcher() = default;
    ~FileWatcher() = default;
    FileWatcher(FileWatcher&&) noexcept = default;
    FileWatcher& operator=(FileWatcher&&) noexcept = default;
    FileWatcher(const FileWatcher&)            = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // Arms the watch on `path`, recording its current view (or the missing
    // state) as the baseline.
    void begin(const std::string& path);

    // One poll: returns the transition event (Unchanged when nothing changed).
    // Re-arms + notifies on a non-Unchanged event. Safe to call before begin()
    // (returns Unchanged without informing the seam).
    [[nodiscard]] FileEvent poll();

    [[nodiscard]] bool watching() const noexcept { return watching_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

    // The NOTIFY SEAM (see the class comment).
    void on_event(std::function<bool(FileEvent)> hook) { onEvent_ = std::move(hook); }

private:
    std::string path_;
    bool watching_ = false;
    FileViewEntry prev_;
    std::function<bool(FileEvent)> onEvent_;
};

} // namespace vaml::viewer
