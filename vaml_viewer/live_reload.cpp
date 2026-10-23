// vaml_viewer — the LIVE-RELOAD coordinator implementation (S222_1).
//
// See live_reload.h for the contract. check() polls the FileWatcher, applies the
// pure reload_action table, and (on Reload) re-Loads the file via the S221
// vaml::load_file seam — the exact file→tree path the CLI runs (never a duplicate).
// On success the freshly loaded tree is handed to the app through on_reloaded; a
// load failure surfaces on_reload_error with the structured detail (last-good kept).
#include "live_reload.h"

#include "load_file.h"   // vaml::load_file / LoadResult (S221 — the real seam)

namespace vaml::viewer {

LiveReload::LiveReload(std::string path)
    : path_(std::move(path))
{
}

void LiveReload::begin()
{
    watcher_.begin(path_);
    // The reload notify wiring: any non-Unchanged event drives the pure action.
    watcher_.on_event([this](FileEvent e) -> bool {
        switch (reload_action(e))
        {
        case ReloadAction::Reload:
            // Appeared = the file came back (restored): notify that it returned.
            if (e == FileEvent::Appeared && onRestored_)
                onRestored_();
            // Re-LOAD the file via the S221 seam (the exact CLI path).
            {
                vaml::LoadResult r = vaml::load_file(path_);
                if (r.ok && r.root)
                {
                    if (onReloaded_)
                        onReloaded_(std::move(r.root));
                }
                else
                {
                    if (onReloadError_)
                        onReloadError_(r.detail.empty() ? "reload: load failed (no detail)"
                                                        : r.detail);
                }
            }
            break;
        case ReloadAction::Report:
            // Disappeared: notify the operator, KEEP the last-good tree rendering.
            if (onDisappeared_)
                onDisappeared_();
            break;
        case ReloadAction::KeepLast:
        default:
            break;   // Unchanged — no action
        }
        return true;   // swallow + continue watching
    });
}

bool LiveReload::check()
{
    const FileEvent ev = watcher_.poll();
    // Reload happened iff the event was a reload event — but rather than track a
    // mutable flag, re-derive: we re-loaded when the event drove Reload. The
    // on_reloaded hook runs inside poll()'s notify seam, so the return here is
    // informational (true = the file was re-loaded).
    return ev == FileEvent::Changed || ev == FileEvent::Appeared;
}

} // namespace vaml::viewer
