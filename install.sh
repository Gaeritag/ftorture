#!/usr/bin/env bash
# Install a shell-independent command; optionally fast-forward from upstream.
set -euo pipefail

UPSTREAM=https://github.com/HorusExE69/ftorture.git
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$HOME/.local/applications/torture.sh"
BIN="$HOME/.local/bin/torture"

if [ "${1:-}" = --update ] && [ "$#" -eq 1 ]; then
	if [ ! -d "$DIR/.git" ] || [ "$(git -C "$DIR" branch --show-current)" != main ]; then
		echo 'Update requires a Git checkout on the main branch.' >&2
		exit 1
	fi
	if [ -n "$(git -C "$DIR" status --porcelain --untracked-files=normal)" ]; then
		echo 'Update refused: the checkout has local changes.' >&2
		exit 1
	fi
	git -C "$DIR" fetch "$UPSTREAM" main
	git -C "$DIR" merge --ff-only FETCH_HEAD
elif [ "$#" -ne 0 ]; then
	echo 'Usage: bash install.sh [--update]' >&2
	exit 2
fi

mkdir -p "${APP%/*}" "${BIN%/*}"
if [ -e "$BIN" ] || [ -L "$BIN" ]; then
	if [ ! -L "$BIN" ] || [ "$(readlink "$BIN")" != "$APP" ]; then
		echo "Refusing to replace an existing command: $BIN" >&2
		exit 1
	fi
fi
if [ -e "$APP" ] && ! grep -qx '# ftorture managed launcher' "$APP"; then
	echo "Refusing to replace an existing file: $APP" >&2
	exit 1
fi
printf '#!/usr/bin/env bash\n# ftorture managed launcher\nexec bash %q "$@"\n' "$DIR/torture.sh" > "$APP"
chmod +x "$APP"
ln -sfn "$APP" "$BIN"
echo "Installed $BIN -> $APP"
if ! command -v torture >/dev/null 2>&1; then
	echo 'If torture is not found, add ~/.local/bin to your PATH (then restart your shell).'
fi
echo 'To update from the original repository, run: bash install.sh --update'
