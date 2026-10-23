// vaml_viewer — the FILE-WATCH mtime-POLL + NOTIFY seam implementation (S222_1).
//
// See file_watch.h for the contract. The REAL I/O half (std::filesystem) lives
// here; the pure decision table + stamp conversion stay inline in the header (so
// the headless tests exercise them without ever touching the filesystem).
#include "file_watch.h"

#include <system_error>

namespace vaml::viewer {

std::optional<FileStamp> file_mtime(const std::string& path) noexcept
{
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(path, ec);
    if (ec)
        return std::nullopt;
    return stamp_to_millis(t);
}

FileViewEntry view_file(const std::string& path) noexcept
{
    auto m = file_mtime(path);
    if (!m)
        return {kNoStamp, false};
    return {*m, true};
}

void FileWatcher::begin(const std::string& path)
{
    path_ = path;
    prev_ = view_file(path);   // the baseline (or the missing state)
    watching_ = true;
}

FileEvent FileWatcher::poll()
{
    if (!watching_)
        return FileEvent::Unchanged;
    const FileViewEntry cur = view_file(path_);
    const FileEvent ev = file_view_state_transition(prev_, cur);
    // RE-ARM: the observed state becomes the new baseline (the pure table derives
    // from prev_, so a repeat of the same state yields Unchanged on the next poll).
    prev_ = cur;
    if (ev != FileEvent::Unchanged && onEvent_)
        (void)onEvent_(ev);   // the notify seam; the return is "swallow", not watch-stop
    return ev;
}

} // namespace vaml::viewer
