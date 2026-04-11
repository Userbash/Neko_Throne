#!/bin/bash
# collect_logs.sh - Улучшенный сборщик логов
# Собирает все отчеты из debug_reports, системные логи и внутренние логи приложения.

REPORT="full_diagnostic_report_$(date +%Y%m%d_%H%M%S).log"
APP="./build/Neko_Throne"

echo "=== Log Collection Started: $(date) ===" > "$REPORT"
echo "Environment: $(uname -a)" >> "$REPORT"
echo "Distrobox: ${DISTROBOX_ENTER_PATH:-Not in Distrobox}" >> "$REPORT"

# 1. Сбор последних отчетов из debug_reports
if [ -d "debug_reports" ]; then
    echo "--- Found debug_reports directory ---" >> "$REPORT"
    LATEST_DEBUG=$(ls -td debug_reports/*/ | head -1)
    if [ -n "$LATEST_DEBUG" ]; then
        echo "Including latest debug session from: $LATEST_DEBUG" >> "$REPORT"
        for f in "$LATEST_DEBUG"*; do
            if [ -f "$f" ] && [[ "$f" == *.log || "$f" == *.txt ]]; then
                echo -e "\n--- Content of $(basename "$f") ---" >> "$REPORT"
                cat "$f" >> "$REPORT"
            fi
        done
    fi
fi

# 2. Запуск приложения для быстрого получения текущего вывода терминала
echo -e "\n--- Live Terminal Output (10s) ---" >> "$REPORT"
timeout 10s $APP "$@" 2>&1 | tee -a "$REPORT" || true

# 3. Сбор системных ошибок
if command -v dmesg >/dev/null && dmesg >/dev/null 2>&1; then
    echo -e "\n--- Relevant dmesg lines ---" >> "$REPORT"
    dmesg | grep -iE "Neko_Throne|segfault|oom-killer" | tail -n 20 >> "$REPORT"
fi

# 4. Сбор внутренних логов
for log in "build/application.log" "crash.log" "/tmp/throne_crash.log"; do
    if [ -f "$log" ]; then
        echo -e "\n--- Content of $log ---" >> "$REPORT"
        tail -n 100 "$log" >> "$REPORT"
    fi
done

echo "Диагностика завершена. Отчет сохранен в: $REPORT"
