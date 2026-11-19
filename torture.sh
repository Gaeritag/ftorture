#!/usr/bin/env bash
# Compatibility entry point; the runner and TUI are entirely Python-native.
exec python3 "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/torture.py" "$@"
