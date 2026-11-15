#!/usr/bin/env bash
# Adds a `torture` command (alias) pointing to this folder's torture.sh.
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
chmod +x "$DIR/torture.sh"
ALIAS="alias torture='bash \"$DIR/torture.sh\"'"
for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
	if [ -f "$rc" ] || [ "$rc" = "$HOME/.bashrc" ]; then
		sed -i.bak '/# ftorture libft tester/,+1d' "$rc" 2>/dev/null
		printf '\n# ftorture libft tester\n%s\n' "$ALIAS" >> "$rc"
		echo "alias 'torture' ajouté à $rc"
	fi
done
if command -v fish >/dev/null 2>&1; then
	mkdir -p "$HOME/.config/fish"
	FC="$HOME/.config/fish/config.fish"
	sed -i.bak '/# ftorture libft tester/,+1d' "$FC" 2>/dev/null
	printf '\n# ftorture libft tester\nalias torture "bash %s/torture.sh"\n' "$DIR" >> "$FC"
	echo "alias 'torture' ajouté à $FC"
fi
echo "Recharge ton shell (source ~/.zshrc ou ~/.bashrc), puis lance : torture"
