# test/ — test suite for VAML. S005: introduces the first real test harness.
# add_subdirectory(test) is guarded so it contributes NO symbols to any VAML
# library (test-only subtree).
add_subdirectory(unit)
# S177 (RAIL 2): the hidden-window real-WSI resize gate — a REAL executable built
# green on both CI lanes, RUN only by the local build/verify_s177_wsiresize.cmd
# (NOT a CTest/GH lane; headless hosts SKIP).
add_subdirectory(wsi)
