# Neko Throne Development Guide

This guide describes the process of building, testing, and contributing to the **Neko Throne** project on Linux and Windows.

## 🛠 Prerequisites

### Linux (Arch/Fedora/Ubuntu)
*   **Compiler:** GCC 11+ (g++)
*   **Build System:** CMake 3.20+ and Ninja
*   **Framework:** Qt 6.10.2
*   **Go:** 1.22+ (for the backend core)
*   **Tools:** `ccache`, `linuxdeploy`, `dpkg-dev`, `polkit` (pkexec), `fuser` (psmisc)

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
3.  **NekoCore** (Go backend) compilation.
4.  **Neko_Throne** (C++ GUI) compilation.
5.  Packaging.

## 🧪 Testing

### Automated Tests
Run internal Qt-based tests:
```bash
cd build && ctest
```

## ⚠️ Critical Maintenance & Stability Patches (April 2026)

Following major architectural stability improvements, the following mechanisms are now mandatory:

### 1. Transport & Configuration Hardening
*   **XHTTP Mapping:** Any `xhttp` transport type is automatically mapped to `http` (version: 2) for `sing-box` compatibility.
*   **Strict Host Format:** Host configurations are strictly enforced as JSON arrays of strings (`["host.com"]`) to comply with core requirements.
*   **Outbound Integrity:** Added a "Last Mile" filter in `ConfigGenerator` that automatically aliases missing proxy tags to `direct` to prevent core crashes.

### 2. Privilege Elevation & TUN Mode
*   **Immutable OS Support:** On systems like Fedora Silverblue/Kinoite (detected via `/run/ostree-booted`), the app uses direct `pkexec` launch instead of filesystem manipulations to bypass `noexec`/`nosuid` restrictions.
*   **Shadow Copy:** On other Linux systems, core binaries are shadowed to `/dev/shm/NekoCore_privileged` (RAM-based storage) to reliably apply `setcap` capabilities.
*   **Capabilities Audit:** Added strict verification using `getcap` post-elevation.

### 3. IPC & Lifecycle Stability
*   **IPC Watchdog:** Before launching, `CoreProcess` kills any zombie processes holding the RPC port using `fuser`.
*   **Atomic Launch:** System proxy settings and TUN leak guards are only enabled **after** a successful gRPC start confirmed by the core.
*   **Memory Hygiene:** Added a thread-safe log queue (`g_logQueue` + `QMutex`) to completely eliminate Qt memory corruption (`Invalid read`).

## 🐞 Troubleshooting
If you encounter unexpected issues, the system performs a pre-flight check for necessary tools (`pkexec`, `fuser`, `setcap`). Ensure these are installed on your distribution.

## 📂 Project Structure
*   `src/`: C++ Implementation files.
*   `include/`: C++ Header files.
*   `core/server/`: Go implementation of the proxy backend.
*   `script/`: Automation and deployment scripts.
*   `3rdparty/`: Bundled external dependencies.
