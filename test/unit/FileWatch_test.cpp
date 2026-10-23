// S222_1 (dep:S222) — the FILE-WATCH mtime-poll + NOTIFY seam, headless suite.
//
// Proves the file-watch seam (vaml_viewer/file_watch.{h,cpp}) WITHOUT a live
// window (the standing host-seam precedent). Two halves:
//   (1) The PURE decision table — file_view_state_transition + stamp_to_millis —
//       proven with NO filesystem I/O (the whole Unchanged/Changed/Appeared/
//       Disappeared surface is exhausted deterministically).
//   (2) The REAL lifecycle — FileWatcher::begin/poll against actual temp files
//       (write → baseline → poll Unchanged; rewrite → Changed + re-arm; delete →
//       Disappeared + re-arm; recreate → Appeared) + the notify seam (the app-side
//       hook fires + can swallow; default no-op).
//
// The seam is compiled in (vaml_viewer/file_watch.cpp inside vaml-core-tests)
// so the headless suite exercises the exact code both apps run (ADR-021).

#include "file_watch.h"   // the S222_1 seam (vaml_viewer)

#include "doctest/doctest.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace vaml::viewer;

namespace {

// A coarse filesystem-granularity-tolerant delay, so two sequential writes are
// guaranteed to produce a distinct mtime. The VM CI lane runs on the VMware HGFS
// shared folder (/mnt/hgfs/VamlPOC), whose mtime granularity is ~2s (vs ~100ns on
// native NTFS/ext4) — 2.2s covers BOTH so the real-file watcher tests are
// deterministic on every lane.
void wait_for_stamp_granularity()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));
}

void write_file(const std::string& path, const std::string& text)
{
    std::ofstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.close();
}

} // namespace

// ---------------------------------------------------------------------------
// Part (1) — the PURE decision table (no filesystem involved)
// ---------------------------------------------------------------------------

TEST_CASE("S222_1: file_view_state_transition — the full decision surface (headless, no I/O)")
{
    // absent + absent -> Unchanged
    CHECK(file_view_state_transition({kNoStamp, false}, {kNoStamp, false}) == FileEvent::Unchanged);
    // present + present, equal stamp -> Unchanged
    CHECK(file_view_state_transition({1000, true}, {1000, true}) == FileEvent::Unchanged);
    // present + present, differing stamp -> Changed
    CHECK(file_view_state_transition({1000, true}, {1001, true}) == FileEvent::Changed);
    // absent + present -> Appeared
    CHECK(file_view_state_transition({kNoStamp, false}, {1000, true}) == FileEvent::Appeared);
    // present + absent -> Disappeared
    CHECK(file_view_state_transition({1000, true}, {kNoStamp, false}) == FileEvent::Disappeared);
}

TEST_CASE("S222_1: stamp_to_millis is a portable monotone-ish conversion (no I/O)")
{
    // A later last-write-time converts to a larger-or-equal millis stamp. Two
    // file_time_types built from seq clocks (no filesystem) — pure conversion.
    const auto a = std::filesystem::file_time_type::min();
    const auto b = std::filesystem::file_time_type::max();
    CHECK(stamp_to_millis(a) <= stamp_to_millis(b));
    CHECK(stamp_to_millis(b) >= stamp_to_millis(a));
    // The result is an integer (never fractional / never NaN).
    const auto t = std::filesystem::file_time_type::clock::now();
    CHECK(stamp_to_millis(t) != kNoStamp);
}

// ---------------------------------------------------------------------------
// Part (2) — the REAL file lifecycle (FileWatcher against actual temp files)
// ---------------------------------------------------------------------------

TEST_CASE("S222_1: FileWatcher baseline poll is Unchanged; a rewrite is Changed + re-arms")
{
    const std::string path = "s222_1_watch_a.vaml";
    write_file(path, "first");
    FileWatcher w;
    w.begin(path);
    CHECK(w.watching() == true);
    CHECK(w.path() == path);
    CHECK(w.poll() == FileEvent::Unchanged);   // baseline

    wait_for_stamp_granularity();
    write_file(path, "second");                // mtime bump, different bytes
    CHECK(w.poll() == FileEvent::Changed);     // the edit -> reload signal

    // Re-arm: the observed state is now the baseline, so a repeat poll is Unchanged.
    CHECK(w.poll() == FileEvent::Unchanged);

    std::remove(path.c_str());
}

TEST_CASE("S222_1: FileWatcher delete is Disappeared + re-arms; recreate is Appeared")
{
    const std::string path = "s222_1_watch_b.vaml";
    write_file(path, "content");
    FileWatcher w;
    w.begin(path);
    CHECK(w.poll() == FileEvent::Unchanged);   // baseline

    std::remove(path.c_str());
    CHECK(w.poll() == FileEvent::Disappeared); // delete -> disappeared
    CHECK(w.poll() == FileEvent::Unchanged);   // re-armed (still absent)

    wait_for_stamp_granularity();
    write_file(path, "back");                  // recreate
    CHECK(w.poll() == FileEvent::Appeared);    // came back -> appeared

    std::remove(path.c_str());
}

TEST_CASE("S222_1: the notify seam fires on a non-Unchanged event + can swallow")
{
    const std::string path = "s222_1_watch_c.vaml";
    write_file(path, "one");
    FileWatcher w;
    w.begin(path);

    int fired = 0;
    FileEvent seen = FileEvent::Unchanged;
    w.on_event([&](FileEvent e) -> bool {
        ++fired;
        seen = e;
        return true;   // swallow + continue
    });

    // baseline: no notify
    (void)w.poll();
    CHECK(fired == 0);

    // a real change (mtime bump) fires the notch
    wait_for_stamp_granularity();
    write_file(path, "two");
    CHECK(w.poll() == FileEvent::Changed);
    CHECK(fired == 1);
    CHECK(seen == FileEvent::Changed);

    std::remove(path.c_str());
}

TEST_CASE("S222_1: the notify default is a no-op (no hook) and poll is safe pre-begin")
{
    FileWatcher w;
    CHECK(w.poll() == FileEvent::Unchanged);   // not watching -> Unchanged, no crash
    w.on_event(nullptr);                        // a null hook is harmless
    CHECK(w.watching() == false);
}
