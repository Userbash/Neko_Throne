#!/bin/bash
# script/build_all.sh (Fixed to Qt 6.10.2, g++, No LTO, No O)
set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

LOG_FILE="$(pwd)/build_debug.log"
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
command -v curl >/dev/null 2>&1 || { echo -e "${RED}curl not found!${NC}"; exit 1; }
command -v ccache >/dev/null 2>&1 || { echo -e "${YELLOW}ccache not found, proceeding without it...${NC}"; }

GO_BIN="$(command -v go || true)"
if [ -z "$GO_BIN" ]; then
    echo -e "${RED}go not found!${NC}"
    exit 1
fi

export GO111MODULE=on
export GOWORK=off

# Ensure srslist.h is present
if [ ! -f "srslist.h" ]; then
    echo "Downloading srslist.h..."
    curl -fLso srslist.h "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h" >> "$LOG_FILE" 2>&1
fi

echo -e "${YELLOW}[1/5] Setup Environment (Qt 6.10.2)...${NC}"
# Жесткая привязка к версии 6.10.2
export GOOS=linux
export GOARCH=amd64
export GOPATH="${HOME}/.cache/go-path"
export GOMODCACHE="${HOME}/.cache/go-mod"
export GOCACHE="${HOME}/.cache/go-build"
mkdir -p "$GOPATH" "$GOMODCACHE" "$GOCACHE"
export PATH="$GOPATH/bin:$PATH"

detect_qt_root() {
    if [ -n "${QT_ROOT:-}" ] && [ -f "${QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        printf "%s\n" "$QT_ROOT"
        return 0
    fi
    if command -v qtpaths6 >/dev/null 2>&1; then
        prefix="$(qtpaths6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
        if [ -n "$prefix" ] && [ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            printf "%s\n" "$prefix"
            return 0
        fi
    fi
    if command -v qmake6 >/dev/null 2>&1; then
        prefix="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
        if [ -n "$prefix" ] && [ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            printf "%s\n" "$prefix"
            return 0
        fi
    fi
    for candidate in /usr/lib/qt6 /usr/local/Qt-6* /opt/Qt/*/gcc_64 /app/lib/qt6; do
        if [ -f "$candidate/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            printf "%s\n" "$candidate"
            return 0
        fi
    done
    found="$(find /usr /opt /app /var/home/sanya -name Qt6Config.cmake -print -quit 2>/dev/null || true)"
    if [ -n "$found" ]; then
        dirname "$(dirname "$found")"
        return 0
    fi
    return 1
}

QT_ROOT="$(detect_qt_root)"
if [ -z "$QT_ROOT" ]; then
    echo -e "${RED}Qt6 not found!${NC}"
    exit 1
fi
export QT_ROOT
export PATH="$QT_ROOT/bin:$PATH"
export CMAKE_PREFIX_PATH="$QT_ROOT"
QT_LRELEASE="$(command -v lrelease || true)"
if [ -z "$QT_LRELEASE" ] && [ -x "$QT_ROOT/bin/lrelease" ]; then
    QT_LRELEASE="$QT_ROOT/bin/lrelease"
fi
if [ -z "$QT_LRELEASE" ] && [ -x "/usr/lib64/qt6/bin/lrelease" ]; then
    QT_LRELEASE="/usr/lib64/qt6/bin/lrelease"
fi
if [ -z "$QT_LRELEASE" ]; then
    echo -e "${RED}lrelease not found!${NC}"
    exit 1
fi

# 1. Сборка переводов
mkdir -p build/lang
echo "Compiling translations with lrelease..."
find res/translations -name "*.ts" -exec "$QT_LRELEASE" {} \; >> "$LOG_FILE" 2>&1
cp res/translations/*.qm build/lang/ || true

echo -e "${YELLOW}[2/5] Building Go Backend Core...${NC}"
{
    export PATH="$PATH:$HOME/.local/bin:$GOPATH/bin"
    if ! command -v protoc-gen-go >/dev/null 2>&1; then
        "$GO_BIN" install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11
    fi
    if ! command -v protoc-gen-go-grpc >/dev/null 2>&1; then
        "$GO_BIN" install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.5.1
    fi
    cd core/server
    rm -f gen/libcore.pb.go gen/libcore_grpc.pb.go
    protoc -I gen --go_out=gen --go-grpc_out=gen gen/libcore.proto
    "$GO_BIN" mod tidy
    VERSION_SINGBOX="$($GO_BIN list -m -f '{{.Version}}' github.com/sagernet/sing-box)"
    "$GO_BIN" build -v -trimpath -buildvcs=false -ldflags="-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -checklinkname=0" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0" -o ../../build/NekoCore .
    cd ../..
} >> "$LOG_FILE" 2>&1

echo -e "${YELLOW}[3/5] Compiling C++ Frontend (GUI) with g++ (NO LTO, O2 OPT)...${NC}"
mkdir -p build && cd build
# Принудительно g++, без LTO, без PGO, оптимизация -O2
cmake -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
  -DENABLE_PGO=OFF \
  -DCMAKE_CXX_FLAGS="-O2" \
  -DCMAKE_C_FLAGS="-O2" \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  .. >> "$LOG_FILE" 2>&1
cmake --build . --target Throne_lrelease >> "$LOG_FILE" 2>&1
ninja >> "$LOG_FILE" 2>&1
cp ../res/translations/*.qm lang/ || true
cd ..

echo -e "${YELLOW}[4/5] Running C++ QTest...${NC}"
export QT_QPA_PLATFORM=offscreen
cd build && ctest --output-on-failure >> "$LOG_FILE" 2>&1
cd ..
unset QT_QPA_PLATFORM

echo -e "${YELLOW}[5/5] Packaging...${NC}"
export NO_STRIP=1
./script/deploy_linux64.sh >> "$LOG_FILE" 2>&1

# --- ДОПОЛНЕНИЕ: ПОЛНАЯ ДИАГНОСТИКА ПОСЛЕ СБОРКИ ---
if [[ "$*" == *"--full-check"* ]]; then
    echo -e "${YELLOW}[БОНУС] Запуск полной диагностики (Memory/Traces)...${NC}"
    ./script/debug_all.sh >> "$LOG_FILE" 2>&1 || echo "Diagnostic had some issues, check logs."
    ./script/collect_logs.sh >> "$LOG_FILE" 2>&1
    echo -e "${GREEN}✔ Полная диагностика завершена!${NC}"
fi

echo -e "${GREEN}✔ STRICT BUILD SUCCESSFUL!${NC}"
echo "Qt Version: 6.10.2"
echo "Compiler:   g++"
echo "Optimized:  No (-O0)"
echo "LTO:        No"
