#!/bin/bash
set -e

source script/env_deploy.sh
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/windows64 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "386" ] && DEST=$DEPLOYMENT/windows32 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/windows-arm64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/linux-amd64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/linux-arm64 || true

if [[ "$GOOS" =~ legacy$ ]]; then
  GOCMD="$PWD/go/bin/go"
  if [[ "$GOOS" == "windowslegacy" ]]; then
    GOOS="windows"
    if [[ $GOARCH == 'amd64' ]]; then
      DEST=$DEPLOYMENT/windowslegacy64
    else
      DEST=$DEPLOYMENT/windows32
    fi
  else
    echo "Unsupported legacy OS: $GOOS"
    exit 1
  fi
else
  GOCMD="go"
fi

if [ -z $DEST ]; then
  echo "Please set GOOS GOARCH"
  exit 1
fi
rm -rf $DEST
mkdir -p $DEST

if [[ "$GOOS" == "windows" ]]; then
  UPDATER_FOUND=false
  if [[ "$GOARCH" == "386" ]]; then
    assets=("updater-windows32.exe" "updater-win32.exe")
  else
    assets=("updater-windows64.exe" "updater-windows-amd64.exe" "updater-win64.exe")
  fi
  for asset in "${assets[@]}"; do
    echo ">> Trying to download ${asset}..."
    if curl -fLso $DEST/updater.exe "https://github.com/throneproj/updater/releases/latest/download/${asset}"; then
      echo ">> Successfully downloaded ${asset}"
      UPDATER_FOUND=true
      break
    fi
  done
  if [ "$UPDATER_FOUND" = false ]; then
    echo "WARNING: Could not download Windows updater. Build will continue without it."
  fi
fi

if [[ "$GOOS" == "linux" ]]; then
  UPDATER_FOUND=false
  if [[ "$GOARCH" == "arm64" ]]; then
    assets=("updater-linux-arm64")
  else
    assets=("updater-linux-amd64" "updater-linux-x86_64")
  fi
  for asset in "${assets[@]}"; do
    echo ">> Trying to download ${asset}..."
    if curl -fLso $DEST/updater "https://github.com/throneproj/updater/releases/latest/download/${asset}"; then
      echo ">> Successfully downloaded ${asset}"
      chmod +x $DEST/updater
      UPDATER_FOUND=true
      break
    fi
  done
  if [ "$UPDATER_FOUND" = false ]; then
    echo "WARNING: Could not download Linux updater. Build will continue without it."
  fi
fi

export CGO_ENABLED=0

#### Go: core ####
pushd core/server
mkdir -p gen
pushd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
CORE_BIN="NekoCore"
if [[ "$GOOS" == "windows" ]]; then CORE_BIN="NekoCore.exe"; fi
$GOCMD build -v -o $DEST/$CORE_BIN -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -checklinkname=0" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0"
popd
