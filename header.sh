# **************************************************************************** #
#  ftorture header, sourced by torture.sh.                                     #
#  Pixel "FTORTURE": 3x5 font, 2 chars per pixel, 1-pixel drop shadow,         #
#  phosphor green with scanlines. Same art as banner() in src/engine.c.        #
#  Uses: COLOR (1 or empty), PH PHM PHD X, SUBTITLE, TAG,                      #
#        PROJ_LABEL, DIR, SPEC, SEED                                           #
# **************************************************************************** #

declare -A FONT=(
	[F]="###:#..:##.:#..:#.."
	[T]="###:.#.:.#.:.#.:.#."
	[O]="###:#.#:#.#:#.#:###"
	[R]="##.:#.#:##.:#.#:#.#"
	[U]="#.#:#.#:#.#:#.#:###"
	[E]="###:#..:##.:#..:###"
)

# logo + subtitle + tagline (drawn at the top of every screen)
header_art() {
	local word="FTORTURE" W=32 r c i line on sh rows
	local grid=()
	local scan=(46 40 46 40 46 22)
	local esc=$'\033'

	for ((i = 0; i < ${#word}; i++)); do
		IFS=: read -ra rows <<< "${FONT[${word:i:1}]}"
		for ((r = 0; r < 5; r++)); do
			for ((c = 0; c < 3; c++)); do
				[ "${rows[r]:c:1}" = "#" ] && grid[r * W + i * 4 + c]=1
			done
		done
	done

	echo
	for ((r = 0; r < 6; r++)); do
		line="   "
		for ((c = 0; c < W; c++)); do
			on="${grid[r * W + c]}"
			sh=""
			if [ -z "$on" ] && [ "$r" -gt 0 ] && [ "$c" -gt 0 ]; then
				sh="${grid[(r - 1) * W + c - 1]}"
			fi
			if [ -n "$on" ]; then
				[ -n "$COLOR" ] && line+="${esc}[38;5;${scan[r]}m"
				line+="██"
			elif [ -n "$sh" ]; then
				[ -n "$COLOR" ] && line+="${esc}[38;5;22m"
				line+="▒▒"
			else
				line+="  "
			fi
		done
		printf '%s%s\n' "$line" "$X"
	done

	local blink=""
	[ -n "$COLOR" ] && blink="${esc}[5m"
	printf '   %s▚▚ %s ▞▞%s\n' "$PHM" "${SUBTITLE:-42 TESTER · LIBFT · FT_PRINTF · GET_NEXT_LINE}" "$X"
	printf '   %s%s%s%s%s_%s\n\n' "$PH" "${TAG:-> ton code va parler}" "$X" "$blink" "$PH" "$X"
}

# project / path / mode / seed, under the logo once a project is chosen
header_info() {
	printf '   %sprojet%s %s\n' "$PHD" "$X" "$PROJ_LABEL"
	printf '   %schemin%s %s\n' "$PHD" "$X" "$DIR"
	if [ "$SPEC" = 1 ]; then
		printf '   %smode  %s strict (-s) : les ▲ comptent comme erreurs\n' "$PHD" "$X"
	else
		printf '   %smode  %s normal : ▲ = avertissements (-s pour les compter)\n' "$PHD" "$X"
	fi
	printf '   %sseed  %s %s\n\n' "$PHD" "$X" "$SEED"
}
