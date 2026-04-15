# Neko_Throne Changelog

## [Version 1.0.0-stable] - 2026-04-15

### Security & Hardening
- **Privilege Elevation (Async):** Replaced synchronous `pkexec` calls and `sudo` fallbacks with a robust, asynchronous `QProcess` implementation. This eliminates GUI freezing during authorization.
- **Nosuid Protection:** Implemented `PrivilegeValidator` to dynamically detect file system security flags (`nosuid`/`noexec`). The system now safely re-routes privileged core binaries to the user's `~\/.cache` directory, ensuring `setcap` operations succeed.
- **Immutable OS Compatibility:** Added automatic detection for Immutable OS (e.g., Fedora Silverblue). On such systems, the application skips `setcap` and uses direct `pkexec` elevation to maintain compliance with system security policies.
- **EAFP Implementation:** Refactored backend network management (IPv6 Leak Guard) to adopt the EAFP (Easier to Ask for Forgiveness than Permission) pattern, removing rigid UID/root checks that caused instability in containerized environments.

### System Stability
- **Atomic Persistence:** Migrated configuration storage to `QSaveFile`, guaranteeing data integrity even if the process crashes during a write operation.
- **IPC Watchdog:** Integrated `fuser`-based port monitoring to kill stale gRPC server processes before launching a new instance, preventing "Process is already running" errors.
- **UI Responsiveness:** Implemented a non-blocking UI state machine. The application now disables only the "Start" button during initialization, keeping the interface interactive and responsive.
- **Thread Safety:** Enforced safe UI updates using `QMetaObject::invokeMethod` to prevent inter-thread access violations in the GUI.

### Configuration & Routing
- **Last Mile Filtering:** Added an automated outbound tag validator in the Go generator. If a routing rule references an invalid outbound tag, it automatically falls back to `direct`, preventing the core from crashing on startup.
- **Auto-Migration:** Integrated automatic configuration migration. Upon loading, legacy VLESS profiles are updated to support `xtls-rprx-vision`, and outdated transport settings are mapped to modern HTTP/2 standards.
