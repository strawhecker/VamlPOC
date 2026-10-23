// vaml_viewer — the LIVE-RELOAD coordinator (S222_1, dep:S222).
//
// Composes the FileWatcher (file_watch.h) + the S221 `load_file` seam into the
// "edit → reload → same tree" coordinator. When the watched `.vaml` file changes,
// LiveReload RE-LOADS it (vaml::load_file) and hands the app a FRESHLY LOADED tree
// so the whole S5 path re-runs with ZERO imperative tree-building — the app
// re-adopts the tree + set_root + set_frame_size + invalidate() (the exact S222
// producer path) and the retained renderer records + presents it. This is the
// viewer's PERMANENT visual-feedback vehicle (S825 matures the loop, not re-do it).
//
// SPLIT (the standing precedent):
//   * PURE DECISION SEAM — reload_action(FileEvent, prevOk, err): the deterministic
//     "what do we do when a file event fires" table. Headless-testable.
//   * The REAL coordinator LiveReload (RAII, move-only): owns a FileWatcher + calls
//     vaml::load_file on Reload; `check()` polls + applies reload_action + dispatches
//     to the notify hooks. The golden fixture (edit → reload → same tree) drives it
//     against REAL temp files in LiveReload_test.cpp.
//
// NO MINIMAL REAL introduced/resolved. ADR-018: ZERO new locks (single-threaded,
// driven off the viewer's UI thread).

#pragma once

#include "file_watch.h"   // FileWatcher, FileEvent

#include "vaml/core/DependencyObject.h"   // the LoadResult owns a unique_ptr<DependencyObject>

#include <functional>
#include <memory>
#include <string>

namespace vaml::viewer {

// What a file event should DRIVE (the pure decision table, headless-testable):
//   Reload    — load the file again + re-run the pipeline (Changed / Appeared).
//   KeepLast  — do nothing (Unchanged).
//   Report    — notify the operator, KEEP the last-good tree (Disappeared — never
//               tear down a working view on a transient editor save/delete).
enum class ReloadAction
{
    Reload = 0,
    KeepLast,
    Report,
};

// The PURE decision: a file event → the action, given the previous load's ok-ness
// and (for genuine failures) the structured error. HEADLESS + deterministic:
//   Changed / Appeared -> Reload
//   Disappeared        -> Report (keep last-good)
//   Unchanged          -> KeepLast (never an action on an idle poll)
inline ReloadAction reload_action(FileEvent e) noexcept
{
    switch (e)
    {
    case FileEvent::Changed:
    case FileEvent::Appeared:
        return ReloadAction::Reload;
    case FileEvent::Disappeared:
        return ReloadAction::Report;
    case FileEvent::Unchanged:
    default:
        return ReloadAction::KeepLast;
    }
}

// The REAL "edit → reload → same tree" coordinator (RAII; move-only — S170).
//   * LiveReload(path) — records the path; begin() arms the watch.
//   * check() — polls the watcher; on Reload it RE-LOADS the file via vaml::load_file
//     and, on success, hands the app a freshly loaded tree through on_reloaded; on a
//     parse/read failure it invokes on_reload_error (NEVER silent) and the app KEEPS
//     the last-good tree. On Report it invokes on_file_disappeared. Returns true iff
//     the file was re-Loaded this check.
class LiveReload
{
public:
    explicit LiveReload(std::string path);
    ~LiveReload() = default;
    LiveReload(LiveReload&&) noexcept = default;
    LiveReload& operator=(LiveReload&&) noexcept = default;
    LiveReload(const LiveReload&)            = delete;
    LiveReload& operator=(const LiveReload&) = delete;

    // Arms the watch (records the baseline view).
    void begin();

    // One poll + handle. Returns true iff a RELOAD was driven this check (a
    // Changed/Appeared event fired the reload path — on_reloaded may have fired,
    // or on_reload_error if the edited file failed to Load). False on an idle
    // poll (KeepLast) or when the file disappeared (Report).
    [[nodiscard]] bool check();

    // ---- NOTIFY SEAMS ----
    // Invoked with a freshly Loaded root tree when a reload produced an OK new
    // tree. THE WHOLE S5 PATH RE-RUNS FROM HERE WITH ZERO IMPERATIVE TREE-BUILDING:
    // the callback re-adopts the tree and drives the retained renderer (set_root +
    // set_frame_size + invalidate -> Measure/Arrange -> encode -> publish -> render).
    void on_reloaded(std::function<void(std::unique_ptr<vaml::DependencyObject>)> hook)
    {
        onReloaded_ = std::move(hook);
    }
    // Invoked with the structured ParseError detail when the edited file failed to
    // Load. The last-good tree STAYS rendered. Never silent.
    void on_reload_error(std::function<void(const std::string&)> hook)
    {
        onReloadError_ = std::move(hook);
    }
    // Invoked when the file vanished (Disappeared — last-good kept) / came back.
    void on_file_disappeared(std::function<void()> hook) { onDisappeared_ = std::move(hook); }
    void on_file_restored(std::function<void()> hook)    { onRestored_ = std::move(hook); }

    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    std::string path_;
    FileWatcher watcher_;
    std::function<void(std::unique_ptr<vaml::DependencyObject>)> onReloaded_;
    std::function<void(const std::string&)> onReloadError_;
    std::function<void()> onDisappeared_;
    std::function<void()> onRestored_;
};

} // namespace vaml::viewer
