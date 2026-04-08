#!/bin/bash
# collect_logs.sh - Улучшенный сборщик логов

REPORT="crash_report.log"
APP="./Neko_Throne"

echo "=== Log Collection Started: $(date) ===" > "$REPORT"
echo "Environment: $(uname -a)" >> "$REPORT"
echo "Distrobox: ${DISTROBOX_ENTER_PATH:-Not in Distrobox}" >> "$REPORT"

# Запуск
echo "--- Terminal Output ---" >> "$REPORT"
$APP "$@" 2>&1 | tee -a "$REPORT"

RET_CODE=${PIPESTATUS[0]}

if [ $RET_CODE -ne 0 ]; then
    echo -e "\n--- CRASH DETECTED (Exit Code: $RET_CODE) ---" >> "$REPORT"
    
    # Пытаемся получить dmesg
    if command -v dmesg >/dev/null && dmesg >/dev/null 2>&1; then
        echo "--- Relevant dmesg lines ---" >> "$REPORT"
        dmesg | grep -iE "Neko_Throne|segfault" | tail -n 15 >> "$REPORT"
    else
        echo "--- dmesg access denied or unavailable ---" >> "$REPORT"
    fi
    
    # Добавляем внутренние логи, если есть
    for log in "crash.log" "/tmp/throne_crash.log"; do
        if [ -f "$log" ]; then
            echo "--- Content of $log ---" >> "$REPORT"
            cat "$log" >> "$REPORT"
        fi
    done
    
    echo "Diagnostic complete. Report: $REPORT"
fi
