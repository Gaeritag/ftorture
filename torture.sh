#!/usr/bin/env bash
# **************************************************************************** #
#  ftorture — 42 tester: libft v19.3, ft_printf v12.1, get_next_line v14.3   #
#                                                                              #
#  torture                       interactive menu (TUI)                        #
#  torture libft  [dir] [opts]   test a libft                                  #
#  torture printf [dir] [opts]   test a ft_printf (mandatory + bonus)          #
#  torture gnl    [dir] [opts]   test a get_next_line (every BUFFER_SIZE)      #
#  torture [dir] [opts]          project detected from the files in dir       #
# **************************************************************************** #

FT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC_BIN="${CC:-cc}"
PROJ=""
DIR=""
PASS=()
DO_CHECKS=1
DO_NORM=1
IGNORE_HIDDEN=0
EFF=0
EFF_TARGET=80
SPEC=0
VERBOSE=0
SEED=42
PART=""
FUNC_FILTER=""
TIMEOUT=""
NARGS=$#
[ "$#" -eq 2 ] && [ "$1" = --lang ] && NARGS=0

# Explicit language overrides locale detection; English is the fallback.
LANG_CODE="${LC_ALL:-${LC_MESSAGES:-${LANG:-en}}}"
case "$LANG_CODE" in fr*) LANG_CODE=fr ;; *) LANG_CODE=en ;; esac
for ((li=1; li<=$#; li++)); do
	if [ "${!li}" = --lang ]; then
		lj=$((li + 1))
		[ "$lj" -le "$#" ] || { echo 'Missing value for --lang' >&2; exit 2; }
		LANG_CODE="${!lj}"
	fi
done
case "$LANG_CODE" in en|fr) ;; *) echo 'Use --lang en or --lang fr' >&2; exit 2 ;; esac
export FTORTURE_LANG="$LANG_CODE"
# shellcheck source=locale.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/locale.sh"

usage() {
	local text
	if [ "$LANG_CODE" = en ]; then
		cat <<'EN_HELP'
usage: torture                         interactive menu
       torture libft  [directory] [options]
       torture printf [directory] [options]
       torture gnl    [directory] [options]
       torture [directory] [options]    (project detected)

  -s, --speculative   count speculative ▲ failures
  -v, --verbose       show details of every failure
  -e, --efficiency [N] compare each function with a "perfect" time computed
                      for this machine, gauge at the bottom, target N% (default 80)
  -f, --func LIST     filter functions or BUFFER_SIZE values
  -p, --part N        test only part N (1, 2 or 3)
      --no-checks     skip Makefile, norm, prototype and symbol checks
      --no-norm       skip norminette
      --no-hidden     ignore hidden files and folders (.vscode, .DS_Store…)
      --seed N        random seed (default: 42)
      --timeout S     per-test timeout in seconds (default: 3)
      --no-color      disable colors
      --lang en|fr    output language (detected from locale by default)
  -h, --help

Your function and the reference run in separate processes. Matching crashes
are accepted; a crash in your function alone is an error. A crash in the
reference alone is a speculative ▲ warning (counted with -s).
EN_HELP
		return
	fi
	text="$(cat <<EOF
usage: torture                         menu interactif
       torture libft  [dossier] [options]
       torture printf [dossier] [options]
       torture gnl    [dossier] [options]
       torture [dossier] [options]     (projet détecté)

  -s, --speculative   les erreurs spéculatives (▲) comptent comme des erreurs
  -v, --verbose       affiche le détail de chaque échec
  -e, --efficiency [N] compare chaque fonction à un temps « parfait » calculé
                      pour cette machine, jauge en bas, objectif N% (défaut 80)
  -f, --func LISTE    libft : split,itoa,ft_strlcat · printf : %d,%s,flag -
                      gnl : liste de BUFFER_SIZE, ex. 1,42,9999
  -p, --part N        libft : 1, 2 ou 3 · printf / gnl : 1 (obligatoire) ou 2 (bonus)
      --no-checks     saute les vérifications (Makefile, norme, protos, nm…)
      --no-norm       saute norminette
      --no-hidden     ignore les fichiers et dossiers cachés (.vscode, .DS_Store…)
      --seed N        graine des tests aléatoires (défaut 42)
      --timeout S     timeout par test en secondes (défaut 3)
      --no-color      sans couleurs
      --lang en|fr  output language (detected from locale by default)
  -h, --help

Tests comparés à la vraie fonction :
  ta fonction crash  + la vraie crash   -> OK (même comportement)
  ta fonction crash  + la vraie non     -> ERREUR (même sans -s)
  ta fonction ok     + la vraie crash   -> ▲ spéculatif (tu protèges, la libc non)
  les deux ok                           -> les résultats doivent être identiques
EOF
)"
	tr_text "$text"
	builtin printf '%s\n' "$REPLY"
}

while [ $# -gt 0 ]; do
	case "$1" in
		libft) PROJ=libft ;;
		printf|ft_printf) PROJ=printf ;;
		gnl|get_next_line) PROJ=gnl ;;
		-s|--speculative) SPEC=1 ;;
		-v|--verbose) VERBOSE=1 ;;
		-e|--efficiency)
			EFF=1
			if [[ "${2:-}" =~ ^[0-9]+$ ]]; then EFF_TARGET="$2"; shift; fi ;;
		-f|--func) FUNC_FILTER="$2"; shift ;;
		-p|--part) PART="$2"; shift ;;
		--seed) SEED="$2"; shift ;;
		--timeout) TIMEOUT="$2"; shift ;;
		--no-color) export NO_COLOR=1 ;;
		--no-checks) DO_CHECKS=0 ;;
		--no-norm) DO_NORM=0 ;;
		--no-hidden) IGNORE_HIDDEN=1 ;;
		--lang) shift ;;
		-h|--help) usage; exit 0 ;;
		-*) echo "option inconnue: $1"; usage; exit 2 ;;
		*) DIR="$1" ;;
	esac
	shift
done

if [ -t 1 ] && [ -z "$NO_COLOR" ]; then
	R=$'\033[38;5;203m'; G=$'\033[38;5;82m'; Y=$'\033[38;5;221m'
	D=$'\033[38;5;244m'; B=$'\033[1m'; X=$'\033[0m'; CL=$'\r\033[K'
	PH=$'\033[38;5;46m'; PHM=$'\033[38;5;34m'; PHD=$'\033[38;5;22m'
	INV=$'\033[48;5;46m\033[38;5;16m\033[1m'; COLOR=1
else
	R=""; G=""; Y=""; D=""; B=""; X=""; CL=""; PH=""; PHM=""; PHD=""
	INV=""; COLOR=""
fi

die() { printf '%s\n %s✘ %s%s\n\n' "$CL" "$R$B" "$*" "$X" >&2; exit 2; }
# retro progress line, rewritten in place:   ▶ norminette…
step() { [ -t 1 ] && printf '%s   %s▶%s %s%s…%s' "$CL" "$PH" "$X" "$PHM" "$*" "$X"; }
cls() { [ -t 1 ] && printf '\033[H\033[2J\033[3J'; }
# shellcheck source=header.sh
. "$FT_DIR/header.sh"
# shellcheck source=checks.sh
. "$FT_DIR/checks.sh"

# ------------------------------------------------------------- projects --
detect_project() {
	if [ -f "$1/get_next_line.h" ]; then echo gnl
	elif [ -f "$1/ft_printf.h" ]; then echo printf
	elif [ -f "$1/libft.h" ]; then echo libft
	fi
}

set_project() {
	PROJ="$1"
	if [ "$PROJ" = gnl ]; then
		PROJ_LABEL="get_next_line"
		ARCHIVE="get_next_line.c"; HDR="get_next_line.h"; SRC_SUB="gnl"
		PROTOS="$FT_DIR/protos/gnl.txt"; FUNCS=(get_next_line)
		SUBTITLE="GET_NEXT_LINE · SUJET v14.3 · LINUX"; TAG="> ta gnl va parler"
	elif [ "$PROJ" = printf ]; then
		PROJ_LABEL="ft_printf"
		ARCHIVE="libftprintf.a"; HDR="ft_printf.h"; SRC_SUB="printf"
		PROTOS="$FT_DIR/protos/printf.txt"; FUNCS=(ft_printf)
		SUBTITLE="FT_PRINTF · SUJET v12.1 · LINUX"; TAG="> ton printf va parler"
	else
		PROJ_LABEL="libft"
		ARCHIVE="libft.a"; HDR="libft.h"; SRC_SUB="libft"
		PROTOS="$FT_DIR/protos/libft.txt"
		FUNCS=(ft_isalpha ft_isdigit ft_isalnum ft_isascii ft_isprint ft_strlen
			ft_memset ft_bzero ft_memcpy ft_memmove ft_strlcpy ft_strlcat ft_toupper
			ft_tolower ft_strchr ft_strrchr ft_strncmp ft_memchr ft_memcmp ft_strnstr
			ft_atoi ft_calloc ft_strdup ft_substr ft_strjoin ft_strtrim ft_split
			ft_itoa ft_strmapi ft_striteri ft_putchar_fd ft_putstr_fd ft_putendl_fd
			ft_putnbr_fd ft_lstnew ft_lstadd_front ft_lstsize ft_lstlast
			ft_lstadd_back ft_lstdelone ft_lstclear ft_lstiter ft_lstmap)
		SUBTITLE="LIBFT · SUJET v19.3 · LINUX"; TAG="> ta libft va parler"
	fi
}

# ------------------------------------------------------------------ TUI --
read_key() {
	local k k2=""
	IFS= read -rsn1 k
	if [ "$k" = $'\033' ]; then
		IFS= read -rsn2 -t 0.05 k2
		k+="$k2"
	fi
	case "$k" in
		$'\033[A'|k|K) echo up ;;
		$'\033[B'|j|J) echo down ;;
		'') echo enter ;;
		' ') echo space ;;
		q|Q|$'\033') echo quit ;;
		[1-9]) echo "n$k" ;;
		*) echo other ;;
	esac
}

tui_restore() { printf '\033[?25h'; stty echo 2>/dev/null; }

menu_line() {   # $1 selected? $2 label $3 description
	if [ "$1" = 1 ]; then
		printf '   %s▶ %-12s%s %s%s%s\n' "$INV" "$2" "$X" "$PH" "$3" "$X"
	else
		printf '     %s%-12s%s %s%s%s\n' "$PHM" "$2" "$X" "$PHD" "$3" "$X"
	fi
}

tui_project() {
	local sel=0 k
	local labels=("LIBFT" "FT_PRINTF" "GNL" "QUITTER")
	local descs=("libft.a · 43 fonctions · parties 1, 2, 3"
		"libftprintf.a · cspdiuxX% · bonus -0. largeur # +"
		"get_next_line · 14 BUFFER_SIZE · bonus multi-fd" "")
	local guess
	guess="$(detect_project "$PWD")"
	[ "$guess" = printf ] && sel=1
	[ "$guess" = gnl ] && sel=2
	while true; do
		cls
		SUBTITLE="" TAG="" header_art
		printf '   %sQUEL PROJET ON TORTURE ?%s\n\n' "$B$PH" "$X"
		for i in 0 1 2 3; do
			menu_line "$([ $i = $sel ] && echo 1)" "${labels[$i]}" "${descs[$i]}"
		done
		printf '\n   %s↑ ↓ choisir · Entrée valider · 1 2 3 raccourcis · q quitter%s\n' "$PHD" "$X"
		[ -n "$guess" ] && printf '   %s(dossier courant : %s détecté)%s\n' "$PHD" "$guess" "$X"
		k="$(read_key)"
		case "$k" in
			up) sel=$(((sel + 3) % 4)) ;;
			down) sel=$(((sel + 1) % 4)) ;;
			n1) sel=0; k=enter ;;
			n2) sel=1; k=enter ;;
			n3) sel=2; k=enter ;;
			quit) cls; exit 0 ;;
		esac
		if [ "$k" = enter ]; then
			[ $sel = 3 ] && { cls; exit 0; }
			case $sel in
				0) set_project libft ;;
				1) set_project printf ;;
				2) set_project gnl ;;
			esac
			return
		fi
	done
}

tui_path() {
	local def="" err=""
	if [ -f "$PWD/$HDR" ]; then def="$PWD"
	elif [ "$PROJ" = libft ] && [ -f "$HOME/libft/$HDR" ]; then def="$HOME/libft"
	elif [ "$PROJ" = printf ] && [ -f "$HOME/ft_printf/$HDR" ]; then def="$HOME/ft_printf"
	elif [ "$PROJ" = gnl ] && [ -f "$HOME/get_next_line/$HDR" ]; then def="$HOME/get_next_line"
	else def="$PWD"
	fi
	while true; do
		cls
		header_art
		printf '   %sOÙ EST TON %s ?%s\n\n' "$B$PH" "$PROJ_LABEL" "$X"
		[ -n "$err" ] && printf '   %s✘ %s%s\n\n' "$R" "$err" "$X"
		printf '   %sTab pour compléter · Entrée pour valider%s\n\n' "$PHD" "$X"
		printf '\033[?25h'
		tr_text 'path'; [ "$LANG_CODE" = fr ] && REPLY='chemin'
		IFS= read -r -e -p "   ${PH}▶${X} $REPLY : " -i "$def" DIR || exit 0
		printf '\033[?25l'
		DIR="${DIR/#\~/$HOME}"
		if [ ! -d "$DIR" ]; then err="dossier introuvable : $DIR"
		elif [ "$PROJ" != gnl ] && [ ! -f "$DIR/Makefile" ]; then err="pas de Makefile dans $DIR"
		elif [ ! -f "$DIR/$HDR" ]; then err="pas de $HDR dans $DIR"
		else return
		fi
		def="$DIR"
	done
}

tui_options() {
	local sel=6 k
	local labels=("mode strict (-s) : les ▲ comptent comme erreurs"
		"verbose (-v) : tous les détails des échecs"
		"efficacité (-e) : ton temps face au temps parfait de la machine, objectif $EFF_TARGET %"
		"vérifications (Makefile, nm, prototypes, README…)"
		"norminette"
		"ignorer les fichiers cachés (--no-hidden)")
	local vals=("$SPEC" "$VERBOSE" "$EFF" "$DO_CHECKS" "$DO_NORM" "$IGNORE_HIDDEN")
	while true; do
		cls
		header_art
		printf '   %sOPTIONS%s  %s%s · %s%s\n\n' "$B$PH" "$X" "$PHD" "$PROJ_LABEL" "$DIR" "$X"
		for i in 0 1 2 3 4 5; do
			local box="[ ]"
			[ "${vals[$i]}" = 1 ] && box="[■]"
			if [ $i = $sel ]; then
				printf '   %s▶ %s %s%s\n' "$PH$B" "$box" "${labels[$i]}" "$X"
			else
				printf '     %s%s%s %s%s%s\n' "$PHM" "$box" "$X" "$PHD" "${labels[$i]}" "$X"
			fi
		done
		echo
		if [ $sel = 6 ]; then
			printf '   %s ▶ LANCER LA TORTURE %s\n' "$INV" "$X"
		else
			printf '     %s▶ LANCER LA TORTURE%s\n' "$PHM" "$X"
		fi
		printf '\n   %s↑ ↓ naviguer · Espace cocher · Entrée lancer · q quitter%s\n' "$PHD" "$X"
		k="$(read_key)"
		case "$k" in
			up) sel=$(((sel + 6) % 7)) ;;
			down) sel=$(((sel + 1) % 7)) ;;
			space) [ $sel -lt 6 ] && vals[$sel]=$((1 - vals[$sel])) ;;
			enter)
				if [ $sel -lt 6 ]; then vals[$sel]=$((1 - vals[$sel]))
				else break; fi ;;
			quit) cls; exit 0 ;;
		esac
	done
	SPEC="${vals[0]}"; VERBOSE="${vals[1]}"; EFF="${vals[2]}"
	DO_CHECKS="${vals[3]}"; DO_NORM="${vals[4]}"; IGNORE_HIDDEN="${vals[5]}"
}

if [ "$NARGS" -eq 0 ] && [ -t 0 ] && [ -t 1 ]; then
	trap 'tui_restore' EXIT
	printf '\033[?25l'
	tui_project
	tui_path
	tui_options
	tui_restore
	trap - EXIT
	cls
fi

# ----------------------------------------------------------- resolve dir --
[ -z "$DIR" ] && DIR="."
DIR="$(cd "$DIR" 2>/dev/null && pwd)" || die "dossier introuvable"
if [ -z "$PROJ" ]; then
	PROJ="$(detect_project "$DIR")"
	[ -z "$PROJ" ] && die "ni libft.h, ni ft_printf.h, ni get_next_line.h dans $DIR (précise : torture libft | printf | gnl)"
fi
set_project "$PROJ"
[ "$SPEC" = 1 ] && PASS+=("-s")
[ "$VERBOSE" = 1 ] && PASS+=("-v")
[ "$EFF" = 1 ] && PASS+=("-e" "$EFF_TARGET")
PASS+=("--seed" "$SEED")
PASS+=("--lang" "$LANG_CODE")
[ -n "$NO_COLOR" ] && PASS+=("--no-color")
[ -n "$FUNC_FILTER" ] && [ "$PROJ" != gnl ] && PASS+=("-f" "$FUNC_FILTER")
[ -n "$TIMEOUT" ] && PASS+=("--timeout" "$TIMEOUT")

header_art
header_info

[ "$(uname -s)" = "Linux" ] || printf '   %s▲ ftorture vise Linux (prototypes, --wrap, glibc). Résultats non garantis sur %s.%s\n' "$Y" "$(uname -s)" "$X"
command -v "$CC_BIN" >/dev/null || die "compilateur '$CC_BIN' introuvable"
command -v make >/dev/null || die "make introuvable"
command -v nm >/dev/null || die "nm introuvable (binutils)"
[ "$PROJ" = gnl ] || [ -f "$DIR/Makefile" ] || die "pas de Makefile dans $DIR"

BUILD="$FT_DIR/build/$PROJ"
mkdir -p "$BUILD" || die "impossible de créer $BUILD"
CHECKS="$BUILD/checks.txt"
: > "$CHECKS"
A="$DIR/$ARCHIVE"
MK=(make --no-print-directory -C "$DIR")
HAS_BONUS=0
if [ "$PROJ" = gnl ]; then
	[ -f "$DIR/get_next_line_bonus.c" ] && [ -f "$DIR/get_next_line_utils_bonus.c" ] && HAS_BONUS=1
else
	"${MK[@]}" -n bonus >/dev/null 2>&1 && HAS_BONUS=1
fi

# Always leave the project without .o files, whatever happens (end of tests,
# build/link error, Ctrl-C). Uses the project's own `make clean`.
cleanup() {
	[ "$PROJ" = gnl ] && return	# gnl is compiled in ftorture's build dir only
	[ -f "$DIR/Makefile" ] || return
	"${MK[@]}" clean >/dev/null 2>&1
	printf '%s   %s▶ make clean : .o supprimés%s\n\n' "$CL" "$PHD" "$X"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

emit()   { local IFS='|'; printf '%s\n' "$*" >> "$CHECKS"; }
ck()     { local a; local translated=(); for a in "$@"; do tr_text "$a"; translated+=("$REPLY"); done; emit C "${translated[@]}"; }
detail() { tr_text "$*"; emit D "$REPLY"; }

# ------------------------------------------------------------------ build --
# build_proj [rule]   (default: all)
build_proj() {
	local rule="${1:-all}"
	"${MK[@]}" fclean >/dev/null 2>&1
	step "make $rule"
	if ! "${MK[@]}" "$rule" > "$BUILD/make.log" 2>&1; then
		printf '%s\n   %s✘ make %s échoue :%s\n\n' "$CL" "$R$B" "$rule" "$X"
		tail -n 25 "$BUILD/make.log" | sed 's/^/     /'
		echo
		return 1
	fi
	[ -f "$A" ] || { printf '%s\n   %s✘ make %s n'"'"'a pas créé %s à la racine%s\n' "$CL" "$R$B" "$rule" "$ARCHIVE" "$X"; return 1; }
	return 0
}

is_relink() { echo "$1" | grep -qE '(^|[[:space:]/])(cc|gcc|clang|ar)[[:space:]]'; }

# ----------------------------------------------------------------- checks --
run_checks() {
	emit S "STRUCTURE & MAKEFILE"

	# files
	"${MK[@]}" fclean >/dev/null 2>&1
	if [ -f "$DIR/$HDR" ]; then
		[ "$PROJ" = libft ] && ck OK "Fichiers à rendre" "Makefile, libft.h, ft_*.c à la racine" \
			|| ck OK "Fichiers à rendre" "Makefile, ft_printf.h, *.c, *.h"
	else
		ck KO "Fichiers à rendre" "$HDR absent de la racine"
	fi
	check_files

	# rules
	local missing=()
	for r in "$ARCHIVE" all clean fclean re; do
		"${MK[@]}" -n "$r" >/dev/null 2>&1 || missing+=("$r")
	done
	grep -qE '^[[:space:]]*NAME[[:space:]]*:?=' "$DIR/Makefile" || missing+=('$(NAME)')
	if [ ${#missing[@]} -eq 0 ]; then
		ck OK "Règles Makefile" "\$(NAME) all clean fclean re"
	else
		ck KO "Règles Makefile" "manquante(s) : ${missing[*]}"
	fi
	if [ "$PROJ" = printf ]; then
		[ $HAS_BONUS = 1 ] && ck OK "Règle bonus" "présente : le bonus sera testé" \
			|| ck SKIP "Règle bonus" "absente : bonus non testé"
	fi

	# compile commands (dry-run, even with @ silenced lines)
	local dry cl bad_flags=0 bad_cc=0 c99=0 n=0 first=""
	dry="$("${MK[@]}" -nB all 2>/dev/null)"
	while IFS= read -r cl; do
		[ -z "$cl" ] && continue
		n=$((n + 1))
		for fl in -Wall -Wextra -Werror; do
			echo " $cl " | grep -q -- " $fl " || bad_flags=1
		done
		first="$(echo "$cl" | awk '{print $1}')"
		[ "$(basename "$first")" = "cc" ] || bad_cc=1
		echo "$cl" | grep -q -- '-std=c99' && c99=1
	done < <(echo "$dry" | grep -E '(^|[[:space:]])-c([[:space:]]|$)' | grep -E '\.c([[:space:]]|$)')
	if [ "$n" -eq 0 ]; then
		ck WARN "Flags de compilation" "aucune commande de compilation détectée (make -nB all)"
	else
		[ $bad_flags -eq 0 ] && ck OK "Flags de compilation" "-Wall -Wextra -Werror sur les $n fichiers" \
			|| ck KO "Flags de compilation" "il manque -Wall, -Wextra ou -Werror sur au moins un fichier"
		[ $bad_cc -eq 0 ] && ck OK "Compilateur" "cc" \
			|| ck KO "Compilateur" "le sujet impose cc (trouvé : $first)"
		[ $c99 -eq 1 ] && ck KO "Flag -std=c99" "interdit par le sujet"
	fi
	if echo "$dry" | grep -q libtool; then
		ck KO "Archive" "libtool est interdit, utilise ar"
	elif echo "$dry" | grep -qE '(^|[[:space:]/])ar[[:space:]]'; then
		ck OK "Archive" "créée avec ar"
	else
		ck WARN "Archive" "commande ar non détectée"
	fi

	# relink (mandatory, then bonus)
	build_proj all || exit 1
	step "vérification du relink"
	local relink
	relink="$("${MK[@]}" -n all 2>/dev/null; "${MK[@]}" -n "$ARCHIVE" 2>/dev/null)"
	if is_relink "$relink"; then
		ck KO "Relink" "make all relance une compilation alors que rien n'a changé"
		detail "$(echo "$relink" | grep -E '(cc|ar) ' | head -n 2 | tr '\n' ' ')"
	else
		ck OK "Relink" "aucun relink"
	fi
	if [ "$PROJ" = printf ] && [ $HAS_BONUS = 1 ]; then
		if build_proj bonus; then
			relink="$("${MK[@]}" -n bonus 2>/dev/null)"
			is_relink "$relink" && ck KO "Relink bonus" "make bonus relance une compilation alors que rien n'a changé" \
				|| ck OK "Relink bonus" "aucun relink"
		else
			ck KO "make bonus" "échoue (voir $BUILD/make.log)"
		fi
	fi

	# clean / fclean
	step "make clean / fclean"
	build_proj all >/dev/null || exit 1
	"${MK[@]}" clean >/dev/null 2>&1
	local objs
	objs="$(find "$DIR" -name '*.o' 2>/dev/null | wc -l | tr -d ' ')"
	if [ "$objs" != "0" ]; then
		ck KO "make clean" "$objs fichier(s) .o restant(s)"
	elif [ ! -f "$A" ]; then
		ck WARN "make clean" "clean supprime aussi $ARCHIVE (c'est le rôle de fclean)"
	else
		ck OK "make clean" "supprime les .o, garde $ARCHIVE"
	fi
	"${MK[@]}" fclean >/dev/null 2>&1
	[ -f "$A" ] && ck KO "make fclean" "$ARCHIVE n'est pas supprimé" \
		|| ck OK "make fclean" "supprime $ARCHIVE"
	build_proj all || exit 1

	# README
	local rd="$DIR/README.md"
	if [ ! -f "$rd" ]; then
		ck KO "README.md" "absent (obligatoire, chapitre Readme du sujet)"
	else
		local first_line miss=()
		first_line="$(head -n 1 "$rd" | tr -d '\r')"
		grep -qiE '^#+[[:space:]]*description' "$rd" || miss+=("Description")
		grep -qiE '^#+[[:space:]]*instructions?' "$rd" || miss+=("Instructions")
		grep -qiE '^#+[[:space:]]*resources?' "$rd" || miss+=("Resources")
		[ "$PROJ" = printf ] && ! grep -qiE 'algorithm|algorithme' "$rd" && miss+=("explication de l'algorithme")
		if ! echo "$first_line" | grep -qE '^[*_]This project has been created as part of the 42 curriculum by [^*_]+[*_][[:space:]]*$'; then
			ck KO "README.md" "1re ligne attendue en italique : *This project has been created as part of the 42 curriculum by <login>*"
		elif [ ${#miss[@]} -gt 0 ]; then
			ck WARN "README.md" "manquant : ${miss[*]}"
		else
			ck OK "README.md" "1re ligne + sections demandées"
		fi
	fi

	emit S "CODE"
	if [ $DO_NORM -eq 0 ]; then
		ck SKIP "Norminette" "désactivée"
	elif command -v norminette >/dev/null 2>&1; then
		step "norminette"
		local nout nerr nfiles
		nout="$(cd "$DIR" && norminette 2>&1)"
		nerr="$(echo "$nout" | grep -cE '^[[:space:]]*Error:')"
		nfiles="$(echo "$nout" | grep -E ': Error!' | sed 's/: Error!.*//' | xargs -n1 basename 2>/dev/null | tr '\n' ' ')"
		if [ "$nerr" = "0" ] && ! echo "$nout" | grep -q 'Error!'; then
			ck OK "Norminette" "aucune erreur"
		else
			ck KO "Norminette" "$nerr erreur(s) dans : $nfiles"
			while IFS= read -r l; do detail "$l"; done < <(echo "$nout" | grep -E 'Error:' | head -n 6 | sed 's/|/ /g')
		fi
	else
		ck SKIP "Norminette" "non installée (pip install norminette)"
	fi

	check_includes

	# symbols (va_* are compiler builtins: they never appear in nm)
	local def und ext forb=() allowed=" malloc free write " tolerated=" __stack_chk_fail _GLOBAL_OFFSET_TABLE_ "
	def="$(nm --defined-only "$A" 2>/dev/null | awk 'NF>=3{print $NF}' | LC_ALL=C sort -u)"
	und="$(nm -u "$A" 2>/dev/null | awk '$1=="U"{print $2}' | LC_ALL=C sort -u)"
	ext="$(LC_ALL=C comm -23 <(echo "$und") <(echo "$def"))"
	for s in $ext; do
		case "$allowed$tolerated" in *" $s "*) continue ;; esac
		local where
		where="$(nm -A -u "$A" 2>/dev/null | awk -v s="$s" '$NF==s{n=split($1,a,":"); print a[n-1]}' | sort -u | tr '\n' ' ')"
		forb+=("$s ($where)")
	done
	if [ ${#forb[@]} -eq 0 ]; then
		[ "$PROJ" = printf ] && ck OK "Fonctions externes" "uniquement malloc, free, write, va_*" \
			|| ck OK "Fonctions externes" "uniquement malloc, free, write"
	else
		ck KO "Fonctions externes" "interdite(s) :"
		for f in "${forb[@]}"; do detail "$f"; done
	fi

	local glob
	glob="$(nm --defined-only -A "$A" 2>/dev/null | awk '$(NF-1) ~ /^[DBCRGSV]$/ {n=split($1,a,":"); print $NF " (" a[n-1] ")"}' | tr '\n' ' ')"
	[ -z "$glob" ] && ck OK "Variables globales" "aucune" \
		|| ck KO "Variables globales" "interdites par la norme : $glob"

	if [ "$PROJ" = libft ]; then
		local helpers
		helpers="$(nm --defined-only "$A" 2>/dev/null | awk '$2=="T" && $3 !~ /^ft_/ {print $3}' | sort -u | tr '\n' ' ')"
		[ -z "$helpers" ] && ck OK "Fonctions auxiliaires" "toutes static" \
			|| ck WARN "Fonctions auxiliaires" "non static : $helpers(le sujet demande static)"
	fi

	local absent=() tot=${#FUNCS[@]}
	for f in "${FUNCS[@]}"; do
		nm --defined-only "$A" 2>/dev/null | awk '$2=="T"{print $3}' | grep -qx "$f" || absent+=("$f")
	done
	[ ${#absent[@]} -eq 0 ] && ck OK "Fonctions obligatoires" "$tot/$tot présente(s) dans $ARCHIVE" \
		|| ck KO "Fonctions obligatoires" "$((tot - ${#absent[@]}))/$tot — absente(s) : ${absent[*]}"

	check_protos
}

# ------------------------------------------------------------- prototypes --
check_protos() {
	case "$PROJ" in
		printf) emit S "PROTOTYPE · sujet ft_printf v12.1" ;;
		gnl) emit S "PROTOTYPE · sujet get_next_line v14.3" ;;
		*) emit S "PROTOTYPES · man Linux + sujet v19.3" ;;
	esac
	local H="$DIR/$HDR" pre="$BUILD/header_pre.h" prefix=""
	if [ ! -f "$H" ]; then
		ck KO "$HDR" "absent"
		return
	fi
	if ! printf '#include "%s"\nint main(void){return 0;}\n' "$HDR" \
		| "$CC_BIN" -fsyntax-only -I"$DIR" -x c - 2>"$BUILD/hdr.err"; then
		prefix=$'#include <stddef.h>\n#include <stdlib.h>\n#include <unistd.h>\n#include <stdarg.h>\n'
		if printf '%s#include "%s"\nint main(void){return 0;}\n' "$prefix" "$HDR" \
			| "$CC_BIN" -fsyntax-only -I"$DIR" -x c - 2>/dev/null; then
			ck WARN "$HDR autonome" "ne compile pas seul : il manque un #include"
		else
			ck KO "$HDR" "ne compile pas : $(head -n 1 "$BUILD/hdr.err" | sed 's/|/ /g')"
			return
		fi
	else
		ck OK "$HDR autonome" "compile seul"
	fi

	if [ "$PROJ" = libft ]; then
		if printf '%s#include "libft.h"\n#include <stddef.h>\n'\
'_Static_assert(__builtin_types_compatible_p(__typeof__(((t_list *)0)->content), void *), "content");\n'\
'_Static_assert(__builtin_types_compatible_p(__typeof__(((t_list *)0)->next), struct s_list *), "next");\n'\
'_Static_assert(offsetof(t_list, content) == 0, "order");\n'\
'_Static_assert(sizeof(t_list) == 2 * sizeof(void *), "size");\n' "$prefix" \
			| "$CC_BIN" -fsyntax-only -I"$DIR" -x c - 2>/dev/null; then
			ck OK "Structure t_list" "struct s_list { void *content; struct s_list *next; }"
		else
			ck KO "Structure t_list" "doit être exactement : typedef struct s_list { void *content; struct s_list *next; } t_list;"
		fi
	fi

	printf '%s#include "%s"\n' "$prefix" "$HDR" | "$CC_BIN" -E -P -I"$DIR" -x c - > "$pre" 2>/dev/null
	tr '\n\t' '  ' < "$pre" | sed 's/;/;\n/g' > "$BUILD/decls.txt"

	local src="$BUILD/proto_check.c" names=() exps=() founds=()
	{
		printf '%s#include "%s"\n#include <stdio.h>\n' "$prefix" "$HDR"
		printf '#define CHK(n, ...) printf("%%s|%%d\\n", #n, __builtin_types_compatible_p(__typeof__(&n), __VA_ARGS__));\n'
		printf 'int main(void)\n{\n'
	} > "$src"
	while IFS='|' read -r name type shown; do
		case "$name" in ''|\#*) continue ;; esac
		local found
		found="$(grep -E "(^|[^A-Za-z0-9_])$name[[:space:]]*\(" "$BUILD/decls.txt" | head -n 1 \
			| tr -s ' ' | sed -E 's/^ +//; s/ +;/;/; s/\( /(/g; s/ \)/)/g; s/\|/ /g')"
		if [ -z "$found" ]; then
			emit P "$name" KO "$shown" ""
		else
			names+=("$name"); exps+=("$shown"); founds+=("$found")
			printf '\tCHK(%s, %s)\n' "$name" "$type" >> "$src"
		fi
	done < "$PROTOS"
	printf '\treturn (0);\n}\n' >> "$src"

	if "$CC_BIN" -w -I"$DIR" "$src" -o "$BUILD/proto_check" 2>"$BUILD/proto.err"; then
		local i=0 res
		while IFS='|' read -r name res; do
			for i in "${!names[@]}"; do
				[ "${names[$i]}" = "$name" ] || continue
				[ "$res" = "1" ] && emit P "$name" OK "${exps[$i]}" "${founds[$i]}" \
					|| emit P "$name" KO "${exps[$i]}" "${founds[$i]}"
			done
		done < <("$BUILD/proto_check")
	else
		for i in "${!names[@]}"; do
			emit P "${names[$i]}" WARN "${exps[$i]}" "${founds[$i]} (vérification impossible)"
		done
	fi
	emit PS
}

# ---------------------------------------------------------------- tester --
# link_tester OUTPUT : engine + this project's tests + the current archive
link_tester() {
	local out="$1" usyms=() extra=()
	step "compilation du tester"
	for s in $(nm --defined-only "$A" 2>/dev/null | awk '$2=="T" && $3 ~ /^ft_/ {print $3}' | sort -u); do
		usyms+=("-Wl,-u,$s")
	done
	# a ft_printf that keeps its libft in a separate archive
	[ "$PROJ" = printf ] && [ -f "$DIR/libft/libft.a" ] && extra+=("$DIR/libft/libft.a")
	if ! "$CC_BIN" -std=gnu11 -O0 -g -w -fno-builtin -I"$FT_DIR/src" \
		-I"$FT_DIR/src/$SRC_SUB" "$FT_DIR/src/engine.c" "$FT_DIR/src/memtrack.c" \
		"$FT_DIR/src/$SRC_SUB"/*.c -Wl,--wrap=malloc -Wl,--wrap=free \
		"${usyms[@]}" "$A" "${extra[@]}" -lm -o "$out" 2> "$BUILD/link.log"; then
		printf '%s\n   %s✘ Impossible de lier le tester avec ton %s :%s\n\n' "$CL" "$R$B" "$ARCHIVE" "$X"
		head -n 20 "$BUILD/link.log" | sed 's/^/     /'
		echo
		echo "   ${D}(un symbole entre en conflit, ou l'archive contient un main ?)${X}"
		exit 1
	fi
	[ -t 1 ] && printf '%s' "$CL"
}

# -------------------------------------------------------- get_next_line --
. "$FT_DIR/gnl.sh"
if [ "$PROJ" = gnl ]; then
	run_gnl
	exit $?
fi

# ------------------------------------------------------------------- main --
if [ $DO_CHECKS -eq 1 ]; then
	run_checks
else
	build_proj all || exit 1
fi

LOG="$FT_DIR/ftorture.log"
T="$BUILD/ftorture_tester"
CHK_ARGS=()
[ $DO_CHECKS -eq 1 ] && CHK_ARGS=(--checks "$CHECKS")
COMMON=(--no-banner --path "$DIR" --log "$LOG")

if [ "$PROJ" = libft ]; then
	link_tester "$T"
	[ -n "$PART" ] && PASS+=("-p" "$PART")
	(cd "$BUILD" && "$T" "${COMMON[@]}" "${CHK_ARGS[@]}" "${PASS[@]}")
	exit $?
fi

# ft_printf: pass 1 = mandatory archive (make), pass 2 = bonus archive
# (make bonus), one combined report.
STATS="$BUILD/stats.bin"
rm -f -- "$STATS"
if [ "$PART" = 2 ]; then
	DO_MANDATORY=0
else
	DO_MANDATORY=1
fi
DO_BONUS=0
[ "$PART" != 1 ] && [ $HAS_BONUS = 1 ] && DO_BONUS=1

NOTE=()
if [ $HAS_BONUS = 0 ] && [ "$PART" != 1 ]; then
	tr_text 'Bonus non testé : pas de règle bonus dans le Makefile'
	NOTE=(--note "$REPLY")
fi

status=0
if [ $DO_MANDATORY = 1 ]; then
	link_tester "$T"
	if [ $DO_BONUS = 1 ]; then
		(cd "$BUILD" && "$T" "${COMMON[@]}" "${CHK_ARGS[@]}" "${PASS[@]}" -p 1 \
			--no-summary --stats-out "$STATS")
	else
		(cd "$BUILD" && "$T" "${COMMON[@]}" "${CHK_ARGS[@]}" "${PASS[@]}" -p 1 "${NOTE[@]}")
		exit $?
	fi
	CHK_ARGS=()
fi

if [ $DO_BONUS = 1 ]; then
	if ! build_proj bonus; then
		echo "   ${R}✘ make bonus échoue : bonus non testé${X}"
		exit 1
	fi
	link_tester "$T"_bonus
	IN=()
	[ -f "$STATS" ] && IN=(--stats-in "$STATS" --no-legend)
	(cd "$BUILD" && "$T"_bonus "${COMMON[@]}" "${CHK_ARGS[@]}" "${PASS[@]}" -p 2 "${IN[@]}")
	status=$?
fi
exit $status
