# Changelog

## [Unreleased]

### Infrastructure & CI/CD
- **Restored gRPC definitions:** Reinstated the `core/server/gen/libcore.proto` file, restoring the service contract between the frontend and backend.
- **CI/CD Hardening:** 
  - Fixed "No such file or directory" errors in GitHub Actions by adding automatic directory synchronization (`mkdir -p`) for generated assets.
  - Added defensive environment checks for `ccache` to prevent noise in build logs during execution failures.
- **Repository Hygiene:** 
  - Standardized `.gitignore` rules to strictly separate source code from binaries and localized IDE configurations.
  - Performed a deep cleanup of the workspace, removing legacy deployment tools and temporary analysis reports.

### Go Backend (ThroneCore)
- **Breaking API Compatibility:** Migrated routing rule logic to support `sing-box v1.11+`. Updated outbound tag resolution to use the new `DefaultOptions.ActionOptions.Outbound` hierarchy.
- **Code Quality:** Resolved variable shadowing and redeclaration issues in the core service initialization logic.
- **Build Enforcement:** Standardized the Go build pipeline to strictly enforce `amd64` architecture for both Linux and Windows targets.

## [0.0.0.1] - 2026-04-16
### Initial Architecture Overhaul
- Migrated to `QSaveFile` for atomic configuration persistence.
- Implemented EAFP pattern in Go backend for more resilient privilege management.
- Introduced `PrivilegeValidator` for dynamic routing of core binaries on restricted filesystems.
- Standardized C++ linting and static analysis (clang-tidy, ASAN).
