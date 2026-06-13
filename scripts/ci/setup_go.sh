#!/usr/bin/env bash
set -euo pipefail

# ═══════════════════════════════════════════════════════════════════════════════
# scripts/ci/setup_go.sh — Optimized Go toolchain installation for CI
# ═══════════════════════════════════════════════════════════════════════════════

export GOPROXY=https://proxy.golang.org,direct
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo "═══════════════════════════════════════════════════"
echo " Throne Go Setup (Container Optimized) "
echo "═══════════════════════════════════════════════════"

# ─── Determine Go version ─────────────────────────────────────────────────────
if [[ -z "${GO_VERSION:-}" ]]; then
    GO_VERSION=$(grep '^go ' "${REPO_ROOT}/core/server/go.mod" | awk '{print $2}')
fi
echo ">> Target Go version: ${GO_VERSION}"

# ─── Install Protoc ───────────────────────────────────────────────────────────
PROTOC_VER="${PROTOC_VER:-31.1}"
if ! command -v protoc &>/dev/null; then
    ARCH="x86_64"
    PROTOC_ZIP="protoc-${PROTOC_VER}-linux-${ARCH}.zip"
    PROTOC_URL="https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_VER}/${PROTOC_ZIP}"

    echo ">> Downloading protoc with retry logic..."
    curl -fLO --retry 5 --retry-delay 3  "${PROTOC_URL}"
    unzip -qo "${PROTOC_ZIP}" -d /tmp/protoc_install
    sudo cp /tmp/protoc_install/bin/protoc /usr/local/bin/
    rm -rf "${PROTOC_ZIP}" /tmp/protoc_install
fi

# ─── Install Go protoc plugins ───────────────────────────────────────────────
if command -v go &>/dev/null; then
    echo ">> Installing Go protoc plugins..."
    go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
    go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
fi

echo ">> Go setup complete."
