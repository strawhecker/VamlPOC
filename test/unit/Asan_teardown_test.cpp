// S146 — ASan-gate teardown hook.
//
// The S141 InternStore and the S145 ObjectPool are RETAIN-FOREVER thread-local
// pools (they deliberately never free during the thread's life — that is what
// makes steady-state layout/dedup churn-free, ADR-018). Under ASan, retaining
// them to process exit would surface as "leaked" bytes. To make the ASan lane
// deterministic (no-leak = green), the retain-forever arenas must be freed
// AFTER the last test but BEFORE process teardown, while ASan's leak checker
// still runs.
//
// This file installs a file-scope object whose DESTRUCTOR runs at program exit
// (after doctest::Context::run() returns, i.e. after EVERY test has run) and
// calls the opt-in master teardown hooks. That ordering matters and is why this
// is a destructor, NOT a TEST_CASE: a test-case that called master_destroy_all
// early would invalidate retained views/storage used by the other tests in the
// same process, AND freeing-then-reusing would leave the pools repopulated at
// process exit (defeating the leak check). Running at exit frees exactly what
// the whole suite retained, once, with nothing to reuse afterward.
//
// These hooks invalidate all previously-retained string_views / pooled object
// pointers (see Intern.h) — correct here because the process is ending.

#include "vaml/core/Intern.h"   // vaml::intern_master_destroy_all
#include "ObjectPool.h"         // vaml::detail::object_pool_master_destroy_all (test seam)

namespace {

// A process-exit destructor that frees the retain-forever pools so ASan's leak
// checker sees a clean heap. Constructed to force static-init (no observers),
// destroyed at program exit after doctest has finished.
struct VamlAsanTeardown
{
    ~VamlAsanTeardown()
    {
        vaml::intern_master_destroy_all();
        vaml::detail::object_pool_master_destroy_all();
    }
};

// Installed as an exit-time hook via a single file-scope object (its destructor
// runs during static destruction, AFTER main()/doctest::run()).
VamlAsanTeardown g_vamlAsanTeardown;

} // namespace
