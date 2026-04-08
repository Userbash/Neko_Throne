#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════════
# scripts/ci/build_go.sh — Build Throne Go backend (sing-box + Xray-core)
# ═══════════════════════════════════════════════════════════════════════════════
# Builds the Go RPC server core for a SINGLE target OS.
# Architecture is ALWAYS amd64 — no ARM, no 32-bit, no legacy.
#
# Usage:
#   GOOS=linux  ./scripts/ci/build_go.sh
#   GOOS=windows ./scripts/ci/build_go.sh
#
# Environment variables (input):
#   GOOS          — Target operating system (required: linux | windows)
#   INPUT_VERSION — Release tag / version string (optional)
#
# Output:
#   deployment/<os>-amd64/  — compiled binaries + updater
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

echo "═══════════════════════════════════════════════════"
echo " Throne Go Build — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "═══════════════════════════════════════════════════"

# ─── Enforce amd64 ────────────────────────────────────────────────────────────
export GOARCH="amd64"

if [[ -z "${GOOS:-}" ]]; then
    echo "ERROR: GOOS is not set. Must be one of: linux, windows"
    exit 1
fi

case "${GOOS}" in
    linux|windows) ;;
    *)
        echo "ERROR: Unsupported GOOS='${GOOS}'. Must be one of: linux, windows"
        exit 1
        ;;
esac

echo ">> Target: GOOS=${GOOS} GOARCH=${GOARCH}"

# ─── Setup paths ─────────────────────────────────────────────────────────────
source script/env_deploy.sh

DEST="${DEPLOYMENT}/${GOOS}-amd64"
if [[ "${GOOS}" == "windows" ]]; then
    DEST="${DEPLOYMENT}/windows64"
fi

rm -rf "${DEST}"
mkdir -p "${DEST}"

echo ">> Output directory: ${DEST}"

# ─── Download updater binary ─────────────────────────────────────────────────
echo ""
echo ">> Downloading updater..."

case "${GOOS}" in
    windows)
        curl -fLso "${DEST}/updater.exe" \
            "https://github.com/throneproj/updater/releases/latest/download/updater-windows64.exe"
        ;;
    linux)
        curl -fLso "${DEST}/updater" \
            "https://github.com/throneproj/updater/releases/latest/download/updater-linux-amd64"
        chmod +x "${DEST}/updater"
        ;;
esac

# ─── Build core ──────────────────────────────────────────────────────────────
export CGO_ENABLED=0

echo ""
echo ">> Building Go core (sing-box + Xray)..."

pushd core/server > /dev/null

# Generate protobuf code
pushd gen > /dev/null
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd > /dev/null

# Extract sing-box version for ldflags
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
echo ">> sing-box version: ${VERSION_SINGBOX}"

# Build with all required tags
CORE_BIN="NekoCore"
if [[ "${GOOS}" == "windows" ]]; then CORE_BIN="NekoCore.exe"; fi
go build -v -o "${DEST}/${CORE_BIN}" -trimpath \
    -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -checklinkname=0" \
    -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0"

popd > /dev/null

echo ""
echo ">> Build artifacts in ${DEST}:"
ls -la "${DEST}/"

echo ""
echo "═══════════════════════════════════════════════════"
echo " Go build complete: ${GOOS}/amd64"
echo "═══════════════════════════════════════════════════"
