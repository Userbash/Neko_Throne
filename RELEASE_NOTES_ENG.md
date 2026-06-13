# Technical Update: Neko Throne v0.0.0.4-alpha

## 💎 Critical Fixes & Architecture

### 1. Windows Stability (C++)
*   **Linker Fix:** Resolved the `undeclared identifier` error in `src/configs/generate.cpp`. High-level systemd integration is now correctly isolated behind `#ifdef Q_OS_LINUX` guards, preventing Windows compiler failures.

### 2. Modernized Container CI/CD (Linux)
*   **Docker-in-Runner Architecture:** Migrated Linux builds to a dedicated **Docker container (`ubuntu:20.04`)**.
*   **Portability Guarantee:** Compiling within a controlled Ubuntu 20.04 environment ensures the binary is linked against **glibc 2.31**, allowing the application to run on almost any Linux distribution released in the last 6 years.
*   **Bootstrap Optimization:** Implemented a pre-build dependency injection step that installs all necessary development headers (`libx11-dev`, `libgl1-mesa-dev`, etc.) and tools (`ccache`, `ninja`) directly into the ephemeral container.

### 3. Build System & Performance
*   **ThroneLib Modularization:** Transitioned to a library-centric build model. This allows for 100% test coverage linking without "undefined reference" errors.
*   **Network Optimization:** Injected `GOPROXY` support into the Go build pipeline, dramatically reducing dependency resolution time and preventing CI timeouts.

---
**Build Health:** 🟢 All systems verified (Windows & Linux).
**E2E Validation:** Successfully completed via `offscreen` Qt verification.
**Tools Used:** SourceCraft, ghtools, and Multi-Agent Orchestrator.
