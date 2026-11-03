#!/bin/bash
# 11n-4 — ubuntu-clang lane (run from /mnt/hgfs/VamlPOC).
set -e
cd /mnt/hgfs/VamlPOC 2>/dev/null || cd "$HOME/VamlPOC"
cmake --preset clang-debug > /tmp/_11n4_cfg.log 2>&1
cmake --build build/clang-debug --target vaml-core-tests -j2 > /tmp/_11n4_bld.log 2>&1
./build/clang-debug/test/unit/vaml-core-tests --test-case='11n-4*' > /tmp/_11n4_ctest.log 2>&1
echo "== 11n-4 lane result =="
tail -6 /tmp/_11n4_ctest.log
