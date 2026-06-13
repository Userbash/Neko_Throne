# Technical Update Summary: Neko Throne v0.0.0.3-alpha

## 🚀 Infrastructure & Compatibility Overhaul

### 1. Containerized CI/CD Pipeline (Linux)
*   **Architecture Migration:** Transitioned the Linux build process from host-based environments to a dedicated **Docker container (`ubuntu:20.04`)** running on `ubuntu-latest` runners.
*   **Benefit:** This hybrid approach combines the high availability and performance of modern GitHub runners with a controlled compilation environment.
*   **Result:** Guaranteed binary compatibility across a wide range of Linux distributions by strictly linking against **glibc 2.31**.

### 2. Enhanced Portability (glibc Fix)
*   **Issue:** Previous builds required `glibc 2.35+`, preventing execution on stable/LTS systems (e.g., Ubuntu 20.04, Debian 11).
*   **Fix:** Enforced a legacy build environment and verified dependency isolation through optimized RPATH and static runtime linking (`-static-libgcc`, `-static-libstdc++`).

### 3. Build System Optimization
*   **Go Toolchain:** Optimized `setup_go.sh` by implementing `GOPROXY=https://proxy.golang.org,direct` to eliminate build hangs during dependency resolution.
*   **Resilience:** Added aggressive retry logic to core tool downloads (protoc) to ensure CI stability in varying network conditions.

### 4. Code Quality & Core Stability
*   **Static Analysis:** Refined `.clang-tidy` rules to support modern LLVM 18+ toolchains without configuration errors.
*   **Linker Fixes:** Finalized the `ThroneLib` architectural split, ensuring 100% successful linking for both the main application and diagnostic test suites.

---
**Build Status:** 🟢 Stable / Production-Ready
**Compatibility:** Windows (amd64), Linux (Generic x86_64, glibc 2.31+)
**Release Version:** v0.0.0.3-alpha
