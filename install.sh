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

# ~/.local/bin is only on PATH where something else put it (Ubuntu's
# ~/.profile when the folder existed at login, another tool...). Add it to
# the shell startup files, once, so `torture` is found everywhere.
MARK='# ftorture: ~/.local/bin on PATH'
LINE='export PATH="$HOME/.local/bin:$PATH"'
user_shell="${SHELL##*/}"
has_path_line() {
	[ -f "$1" ] && grep -Eq '^[^#]*(\$HOME|\$\{HOME\}|~)/\.local/bin' "$1"
}
added=0
for rc in "$HOME/.zshrc" "$HOME/.bashrc"; do
	name="${rc##*/.}"
	name="${name%rc}"
	# create the file only for the user's own shell
	[ -f "$rc" ] || [ "$name" = "$user_shell" ] || continue
	if has_path_line "$rc"; then
		echo "PATH: already set in $rc"
		continue
	fi
	printf '\n%s\n%s\n' "$MARK" "$LINE" >> "$rc"
	echo "PATH: ~/.local/bin added to $rc"
	added=1
done
FC="$HOME/.config/fish/config.fish"
if command -v fish >/dev/null 2>&1 || [ -f "$FC" ]; then
	if [ -f "$FC" ] && grep -Eq '^[^#]*(\$HOME|~)/\.local/bin' "$FC"; then
		echo "PATH: already set in $FC"
	else
		mkdir -p "${FC%/*}"
		printf '\n%s\nfish_add_path -g $HOME/.local/bin\n' "$MARK" >> "$FC"
		echo "PATH: ~/.local/bin added to $FC"
		added=1
	fi
fi
case ":$PATH:" in
	*":$HOME/.local/bin:"*) ;;
	*) added=1 ;;
esac
if [ "$added" = 1 ]; then
	echo 'Open a new terminal (or run: source ~/.zshrc / source ~/.bashrc), then run: torture'
fi
echo 'To update from the original repository, run: bash install.sh --update'
