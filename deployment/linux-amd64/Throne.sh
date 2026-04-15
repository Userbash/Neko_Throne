#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QML2_IMPORT_PATH="$HERE/usr/qml"

# Enable Core Dumps for debugging
ulimit -c unlimited
# Set core dump pattern to the current directory if possible
# (Requires /proc/sys/kernel/core_pattern to be just "core", but we try our best)

echo "--- Launching Throne (Segfault Debug Mode) ---"
exec "$HERE/Neko_Throne" "$@"
