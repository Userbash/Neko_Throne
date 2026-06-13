#!/bin/bash
set -e
cd /var/home/sanya/wisper/core/.mounted/Neko_Throne

echo "=== [1/4] Cleaning old artifacts and cache ==="
rm -rf build/
rm -rf build_tidy/
rm -f failed_log.txt repo_contents.txt job_log.txt

# Ensure srslist.h is present (critical dependency)
if [ ! -f "srslist.h" ]; then
    echo "Downloading srslist.h..."
    curl -fLso srslist.h "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h"
fi

echo "=== [2/4] Initializing clean build (Release) ==="
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja

echo "=== [3/4] Running Unit Tests (ServerListModelTest) ==="
./ServerListModelTest

echo "=== [4/4] Running E2E & Smoke Tests (ArtifactTest) ==="
export QT_QPA_PLATFORM=offscreen
./ArtifactTest

echo "=== QA CYCLE COMPLETE ==="
