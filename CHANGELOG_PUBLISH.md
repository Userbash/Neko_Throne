# Technical Update Summary: Neko Throne CI/CD & Core Stability

## 🚀 Key Improvements

### 1. Windows Build Fix (Compilation Error)
*   **Issue:** Resolved a fatal compilation error in `src/configs/generate.cpp` where the function `isSystemdResolvedDefaultResolver()` was called without proper platform guards, causing an `undeclared identifier` error on Windows (MSVC/Clang-cl).
*   **Fix:** Implemented strict preprocessor directives (`#ifdef Q_OS_LINUX`) to isolate Linux-specific systemd logic from the cross-platform codebase.

### 2. Linux Portability & glibc Compatibility
*   **Improvement:** Migrated the Linux build environment from `ubuntu-22.04` to **`ubuntu-20.04`** in GitHub Actions.
*   **Result:** Reduced the minimum required `glibc` version to **2.31**, ensuring the binary runs out-of-the-box on older and stable distributions like Debian 11, Ubuntu 20.04 LTS, and Enterprise Linux derivatives.
*   **Linking:** Enforced `-static-libgcc` and `-static-libstdc++` to eliminate runtime dependencies on host compiler libraries.

### 3. Testing Infrastructure Refactoring
*   **Architecture:** Refactored `CMakeLists.txt` to introduce **`ThroneLib`** (Static Library).
*   **Benefits:** This architectural change allows for reliable linking of Unit and E2E tests with all core dependencies (Qt Network, DBus, etc.), resolving previous "undefined reference" linker errors.
*   **New Tests:** Introduced `ServerListModelTest` for high-performance data model verification (validated with 10,000+ profiles).

### 4. Code Quality & CI/CD
*   **Static Analysis:** Cleaned up `.clang-tidy` configurations, removing deprecated keys to support modern LLVM 18+ toolchains.
*   **Deployment:** Enhanced `deploy_linux64.sh` with automated library bundling via `linuxdeploy` and integrated RPATH fixes.

---
**Status:** 🟢 All systems operational. 
**Platform Support:** Windows (amd64), Linux (glibc >= 2.31).
**Build System:** CMake + Ninja + Qt 6.10+.
