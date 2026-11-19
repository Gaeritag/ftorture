#!/usr/bin/env bash
# Compatibility entry point for existing installation instructions.
exec python3 "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/install.py" "$@"
