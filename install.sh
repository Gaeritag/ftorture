#!/usr/bin/env bash
# Compatibility entry point. Installation logic lives in install.py.
exec python3 "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/install.py" "$@"
