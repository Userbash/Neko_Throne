# Neko Throne Development Guide

This guide describes the process of building, testing, and contributing to the **Neko Throne** project on Linux and Windows.

## 🛠 Prerequisites

### Linux (Arch/Fedora/Ubuntu)
*   **Compiler:** GCC 11+ (g++)
*   **Build System:** CMake 3.20+ and Ninja
*   **Framework:** Qt 6.10.2
*   **Go:** 1.22+ (for the backend core)
*   **Tools:** `ccache`, `linuxdeploy`, `dpkg-dev` (for .deb packages)

### Containerized Environment (Recommended)
We use **Distrobox** with an Arch Linux image (`dev-qt`) to ensure a consistent build environment with strict Qt 6.10.2 dependencies.

## 🏗 Building the Project

### The "One-Click" Strict Build
To perform a clean build with all optimizations disabled (useful for debugging) and ccache enabled:
```bash
./script/build_all.sh
```
This script handles:
1.  Environment verification.
2.  Qt translation compilation.
3.  **NekoCore** (Go backend) compilation with appropriate build tags.
4.  **Neko_Throne** (C++ GUI) compilation using `g++` and `ccache`.
5.  Packaging into portable and `.deb` formats.

### Manual C++ Build
```bash
mkdir build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DQt6_DIR=/path/to/qt6/lib/cmake/Qt6 ..
ninja
```

## 🧪 Testing

### Automated Tests
Run internal Qt-based tests:
```bash
cd build && ctest
```

### End-to-End (E2E) Verification
Run the functional verification script to check core execution and dynamic library integrity:
```bash
./script/e2e_test.sh
```

## 🐞 Troubleshooting

### Memory Errors (Segfaults)
If the application crashes, a detailed backtrace is written to:
1.  `crash.log` (Current directory)
2.  `/tmp/throne_crash.log`

Use the log collector for deep analysis:
```bash
./script/collect_logs.sh
```

### Headless/Container Issues
The application automatically detects if it's running in a restricted environment (like a container without a proper X11/Wayland session) and disables unstable System Tray features to prevent crashes.

## 📂 Project Structure
*   `src/`: C++ Implementation files.
*   `include/`: C++ Header files.
*   `core/server/`: Go implementation of the proxy backend.
*   `script/`: Automation and deployment scripts.
*   `3rdparty/`: Bundled external dependencies.
