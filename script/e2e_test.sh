#!/bin/bash
# script/e2e_test.sh
set -e

CORE="./deployment/linux-amd64/NekoCore"
GUI="./build/Neko_Throne"

echo "--- E2E: Functional Verification ---"

# 1. Проверка бинарных файлов
[ -f "$CORE" ] && echo "PASS: Core Binary exists" || { echo "FAIL: Core Binary missing"; exit 1; }
[ -f "$GUI" ] && echo "PASS: GUI Binary exists" || { echo "FAIL: GUI Binary missing"; exit 1; }

# 2. Проверка версии ядра (тест на работоспособность Go кода)
# Пытаемся получить версию, игнорируя ошибки флагов (так как ядро может требовать подкоманды)
CORE_VER=$($CORE --version 2>&1 || true)
echo "Core Version Output: $CORE_VER"
if [[ ${CORE_VER,,} == *"sing-box"* ]] || [[ ${CORE_VER,,} == *"xray"* ]]; then
    echo "PASS: Core execution test"
else
    echo "FAIL: Core execution returned unexpected output"
    exit 1
fi

# 3. Проверка динамических библиотек GUI (тест на Qt зависимости)
MISSING_LIBS=$(ldd "$GUI" | grep "not found" || true)
if [ -z "$MISSING_LIBS" ]; then
    echo "PASS: GUI dynamic libraries check"
else
    echo "FAIL: Missing libraries for GUI:"
    echo "$MISSING_LIBS"
    exit 1
fi

# 4. Проверка прав доступа (SUID - опционально в контейнере, но важно для деплоя)
if [[ $(stat -c "%a" "$CORE") =~ ^4 ]]; then
    echo "INFO: SUID bit is set (OK for TUN)"
else
    echo "INFO: SUID bit not set (Standard for local builds)"
fi

echo "--- E2E: ALL TESTS PASSED ---"
