# Neko_Throne Architecture Updates: Stability & Security (V2)

## 1. Data Persistence & Schema Versioning
- **Atomic Saving:** Migrated from QFile to `QSaveFile` to ensure atomic writes, preventing configuration file corruption during power loss or app crashes.
- **Schema Versioning:** Added `schema_version` field to `JsonStore` and implemented `RunMigrations()` logic. 
- **Auto-Migration:** Profiles are automatically upgraded upon import/load (e.g., VLESS flow automation to `xtls-rprx-vision`, mapping legacy XHTTP to HTTP v2).

## 2. Security & Privilege Elevation (EAFP Pattern)
- **Privilege Separation:** GUI no longer requires root privileges. It utilizes `setcap` capabilities and `pkexec` (PolicyKit) for elevated operations.
- **EAFP Implementation:** The Go backend now uses the "Easier to Ask for Forgiveness than Permission" pattern. Rigid `os.Geteuid() != 0` checks were removed, allowing the core to attempt operations and gracefully log failures if privileges are insufficient, preventing unnecessary application crashes.
- **Nosuid Protection:** The C++ GUI now detects if `/dev/shm` is mounted with `nosuid` or `noexec` and automatically re-routes the core binary to a safe user cache location (`~\/.cache\/Neko_Throne\/privileged`).

## 3. UI Stability & UX
- **Thread Safety:** Added `QMetaObject::invokeMethod` and `QPointer` guards for all gRPC callbacks, eliminating Use-after-free and Invalid read errors when the GUI is closed or the core exits unexpectedly.
- **UX Improvements:** 
    - The main UI is no longer fully disabled during core startup; only the "Start" button is toggled.
    - Added an "Emergency Network Reset" and "Reset Privilege Cache" option in the VPN Troubleshooting dialog for user-driven recovery.
- **Graceful Failure:** Added explicit check for `pkexec` existence and handled exit code 126/127 (Polkit cancellation) as a graceful user cancellation rather than a fatal gRPC error.

## 4. Build System
- **Deterministic Builds:** Post-build commands in CMake ensure `NekoCore` binary is always executable (`chmod +x`).
- **Dependency Management:** Fixed Protobuf generation (`myproto`) to guarantee consistent header availability (`libcore.pb.h`).
