# Changelog

## [Unreleased]

### Infrastructure & CI/CD
- **Fixed:** Restored `core/server/gen/libcore.proto` definition which was missing in the development branch, causing gRPC code generation failures.
- **Improved CI Robustness:** Updated GitHub Actions workflows and Go build scripts to automatically ensure the existence of the `gen` directory before code generation.
- **Workspace Optimization:** Performed a deep cleanup of the repository, removing legacy deployment tools, temporary build artifacts, and local log files.
- **Refined Version Control:** Completely overhauled `.gitignore` rules at both root and core levels to prevent accidental commits of binaries, IDE configs, and generated Go code while protecting critical `.proto` definitions.

### Build System
- Standardized the Go backend build pipeline to strictly enforce `amd64` architecture.
- Added explicit directory guards in `scripts/ci/build_go.sh` to support clean-slate builds in containerized environments.
- Updated documentation to reflect the latest stability improvements in the build process.

## [0.0.0.1] - 2026-04-16
### Initial Architecture Overhaul
- Migrated to `QSaveFile` for atomic configuration persistence.
- Implemented EAFP pattern in Go backend for more resilient privilege management.
- Introduced `PrivilegeValidator` for dynamic routing of core binaries on restricted filesystems.
- Standardized C++ linting and static analysis (clang-tidy, ASAN).
