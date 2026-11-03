#!/bin/bash
# 11n-4 foundation — full ubuntu-clang suite (no-regression check).
set -e
cd /mnt/hgfs/VamlPOC 2>/dev/null || cd "$HOME/VamlPOC"
./build/clang-debug/test/unit/vaml-core-tests > /tmp/_11n4_full.log 2>&1 || true
echo "EXIT=$?"
tail -5 /tmp/_11n4_full.log
