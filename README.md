# Neko Throne (Development Branch)

**Neko Throne** is a modern, cross-platform GUI proxy client built with Qt 6. This is the **development branch**, featuring the latest experimental optimizations and debugging tools.

[![Build Status](https://github.com/DpaKc404/Neko_Throne/actions/workflows/main_ci.yml/badge.svg)](https://github.com/DpaKc404/Neko_Throne/actions)
[![License](https://img.shields.io/github/license/DpaKc404/Neko_Throne)](LICENSE)

## ✨ Recent Dev Highlights
*   **Optimized Performance:** Build system tuned with `-O3` and `x86-64-v3` instruction sets for better execution speed on modern CPUs.
*   **Enhanced Debugging:** Integrated UI event filtering to track user interactions in real-time, making it easier to diagnose interface issues.
*   **Linux Hardening:** Improved path resolution for privileged operations (sudo/pkexec) and better support for Flatpak environments.
*   **Refined Core Integration:** Better handling of core life-cycle states and improved logging for backend-frontend communication.

## 🚀 Getting Started

### Build from Source
To take advantage of the latest performance optimizations:
1.  Ensure you have **Qt 6.10+** and a modern **GCC** compiler.
2.  Clone this branch: `git checkout dev`
3.  Run the build script: `./script/build_all.sh`

### UI Interaction Logs
Developers can now monitor button clicks and UI events directly in the debug console. This helps in mapping out user flows and verifying signal-slot connections.

## 🛠 For Developers
Detailed technical documentation can be found in the [Development Guide](docs/DEVELOPMENT.md).

## 🤝 Contributing
We appreciate all contributions! Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting changes to the `dev` branch.

## 📜 License
This project is licensed under the **GPL-2.0-or-later** license. See the [LICENSE](LICENSE) file for details.
