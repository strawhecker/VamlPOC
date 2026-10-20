// S173/S174 — doctest implementation + main for the vaml-presentation-tests exec.
//
// Defines DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN exactly once in this target (mirrors
// smoke_test.cpp for vaml-core-tests and PlatformWindow_test.cpp for
// vaml-vulkan-tests) so the doctest runtime symbols resolve. The S173/S174 test
// TUs (Presenter_test.cpp / Composition_test.cpp) only declare tests against this
// implementation.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
