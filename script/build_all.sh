#!/bin/bash
# script/build_all.sh (Fixed to Qt 6.10.2, g++, No LTO, No O)
set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

LOG_FILE="build_debug.log"
echo "--- STRICT Build x86_64 (Qt 6.10.2, g++, No LTO, No O) started at $(date) ---" > "$LOG_FILE"

# Функция для логирования ошибок
error_handler() {
    echo -e "${RED}✘ Error occurred at line $1. Check $LOG_FILE for details.${NC}"
    tail -n 40 "$LOG_FILE"
    exit 1
}
trap 'error_handler $LINENO' ERR

echo -e "${YELLOW}[0/5] Pre-build Environment Check...${NC}"
command -v g++ >/dev/null 2>&1 || { echo -e "${RED}g++ not found!${NC}"; exit 1; }
command -v go >/dev/null 2>&1 || { echo -e "${RED}go not found!${NC}"; exit 1; }
command -v ccache >/dev/null 2>&1 || { echo -e "${YELLOW}ccache not found, proceeding without it...${NC}"; }

echo -e "${YELLOW}[1/5] Setup Environment (Qt 6.10.2)...${NC}"
# Жесткая привязка к версии 6.10.2
export QT_ROOT="/var/home/sanya/Qt6.10/6.10.2/gcc_64"
export PATH="$QT_ROOT/bin:$PATH:$(go env GOPATH)/bin"
export CMAKE_PREFIX_PATH="$QT_ROOT"
export GOOS=linux
export GOARCH=amd64
export GOMODCACHE=$HOME/.cache/go-mod
export GOCACHE=$HOME/.cache/go-build
mkdir -p "$GOMODCACHE" "$GOCACHE"

# 1. Сборка переводов
mkdir -p build/lang
echo "Compiling translations with lrelease (from Qt 6.10.2)..."
find res/translations -name "*.ts" -exec $QT_ROOT/bin/lrelease {} \; >> "$LOG_FILE" 2>&1
cp res/translations/*.qm build/lang/ || true

echo -e "${YELLOW}[2/5] Building Go Backend Core...${NC}"
{
    cd core/server
    go mod tidy
    VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
    go build -v -trimpath -ldflags="-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -checklinkname=0" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0" -o ../../build/NekoCore .
    cd ../..
} >> "$LOG_FILE" 2>&1

echo -e "${YELLOW}[3/5] Compiling C++ Frontend (GUI) with g++ (NO LTO, NO OPT, CCACHE)...${NC}"
mkdir -p build && cd build
# Принудительно g++, без LTO, без оптимизаций (-O0), использование ccache
cmake -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
  -DCMAKE_CXX_FLAGS="-O0 -g" \
  -DCMAKE_C_FLAGS="-O0 -g" \
  -DQt6_DIR="$QT_ROOT/lib/cmake/Qt6" \
  .. >> "../$LOG_FILE" 2>&1
ninja >> "../$LOG_FILE" 2>&1
cp ../res/translations/*.qm lang/ || true
cd ..

echo -e "${YELLOW}[4/5] Running C++ QTest...${NC}"
cd build && ctest --output-on-failure >> "../$LOG_FILE" 2>&1
cd ..

echo -e "${YELLOW}[5/5] Packaging...${NC}"
export NO_STRIP=1
export QT_ROOT="/var/home/sanya/Qt6.10/6.10.2/gcc_64"
./script/deploy_linux64.sh >> "$LOG_FILE" 2>&1
./script/pack_debian.sh "0.5.1-qt6102-gxx-no-opt-$(date +%Y%m%d)" >> "$LOG_FILE" 2>&1

echo -e "${GREEN}✔ STRICT BUILD SUCCESSFUL!${NC}"
echo "Qt Version: 6.10.2"
echo "Compiler:   g++"
echo "Optimized:  No (-O0)"
echo "LTO:        No"
