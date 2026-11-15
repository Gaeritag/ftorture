#!/usr/bin/env bash
# **************************************************************************** #
#  ftorture — libft tester (subject v19.3)                                     #
#  usage: torture [chemin_libft] [-s] [-v] [-f ft_split,itoa] [-p 2] [--no-checks] #
# **************************************************************************** #

FT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$FT_DIR/build"
LIBFT="."
PASS=()
DO_CHECKS=1
DO_NORM=1
CC_BIN="${CC:-cc}"

usage() {
	cat <<EOF
usage: torture [chemin_libft] [options]

  -s, --speculative   les erreurs spéculatives (▲) comptent comme des erreurs
  -v, --verbose       affiche le détail de chaque échec
  -f, --func LISTE    ne teste que ces fonctions   ex: -f split,itoa,ft_strlcat
  -p, --part N        ne teste que la partie N (1, 2 ou 3)
      --no-checks     saute les vérifications (Makefile, norme, protos, nm…)
      --no-norm       saute norminette
      --seed N        graine des tests aléatoires (défaut 42)
      --timeout S     timeout par test en secondes (défaut 3)
      --no-color      sans couleurs
  -h, --help

Règles des tests "comparés à la vraie fonction" :
  ta fonction crash  + la vraie crash   -> OK (même comportement)
  ta fonction crash  + la vraie non     -> ERREUR (même sans -s)
  ta fonction ok     + la vraie crash   -> ▲ spéculatif (tu protèges, la libc non)
  les deux ok                           -> les résultats doivent être identiques
EOF
}

while [ $# -gt 0 ]; do
	case "$1" in
		-s|--speculative) PASS+=("-s") ;;
		-v|--verbose) PASS+=("-v") ;;
		-f|--func) PASS+=("-f" "$2"); shift ;;
		-p|--part) PASS+=("-p" "$2"); shift ;;
		--seed) PASS+=("--seed" "$2"); shift ;;
		--timeout) PASS+=("--timeout" "$2"); shift ;;
		--no-color) PASS+=("--no-color"); export NO_COLOR=1 ;;
		--no-checks) DO_CHECKS=0 ;;
		--no-norm) DO_NORM=0 ;;
		-h|--help) usage; exit 0 ;;
		-*) echo "option inconnue: $1"; usage; exit 2 ;;
		*) LIBFT="$1" ;;
	esac
	shift
done

if [ -t 1 ] && [ -z "$NO_COLOR" ]; then
	R=$'\033[38;5;203m'; G=$'\033[38;5;114m'; Y=$'\033[38;5;221m'
	D=$'\033[38;5;244m'; B=$'\033[1m'; X=$'\033[0m'; CL=$'\r\033[K'
else
	R=""; G=""; Y=""; D=""; B=""; X=""; CL=""
fi

die() { printf '%s\n %s✘ %s%s\n\n' "$CL" "$R$B" "$*" "$X" >&2; exit 2; }
step() { [ -t 1 ] && printf '%s %s⧗ %s…%s' "$CL" "$D" "$*" "$X"; }

[ "$(uname -s)" = "Linux" ] || printf ' %s▲ ftorture vise Linux (prototypes, --wrap, glibc). Résultats non garantis sur %s.%s\n' "$Y" "$(uname -s)" "$X"
command -v "$CC_BIN" >/dev/null || die "compilateur '$CC_BIN' introuvable"
command -v make >/dev/null || die "make introuvable"
command -v nm >/dev/null || die "nm introuvable (binutils)"

LIBFT="$(cd "$LIBFT" 2>/dev/null && pwd)" || die "dossier libft introuvable"
mkdir -p "$BUILD" || die "impossible de créer $BUILD"
CHECKS="$BUILD/checks.txt"
: > "$CHECKS"
A="$LIBFT/libft.a"
MK=(make --no-print-directory -C "$LIBFT")

emit()   { local IFS='|'; printf '%s\n' "$*" >> "$CHECKS"; }
ck()     { emit C "$@"; }
detail() { emit D "$*"; }

FUNCS=(ft_isalpha ft_isdigit ft_isalnum ft_isascii ft_isprint ft_strlen ft_memset
	ft_bzero ft_memcpy ft_memmove ft_strlcpy ft_strlcat ft_toupper ft_tolower
	ft_strchr ft_strrchr ft_strncmp ft_memchr ft_memcmp ft_strnstr ft_atoi
	ft_calloc ft_strdup ft_substr ft_strjoin ft_strtrim ft_split ft_itoa
	ft_strmapi ft_striteri ft_putchar_fd ft_putstr_fd ft_putendl_fd ft_putnbr_fd
	ft_lstnew ft_lstadd_front ft_lstsize ft_lstlast ft_lstadd_back ft_lstdelone
	ft_lstclear ft_lstiter ft_lstmap)

[ -f "$LIBFT/Makefile" ] || die "pas de Makefile dans $LIBFT"

# ------------------------------------------------------------------ build --
build_libft() {
	"${MK[@]}" fclean >/dev/null 2>&1
	step "compilation de la libft"
	if ! "${MK[@]}" all > "$BUILD/make.log" 2>&1; then
		printf '%s\n %s✘ La libft ne compile pas :%s\n\n' "$CL" "$R$B" "$X"
		tail -n 25 "$BUILD/make.log" | sed 's/^/     /'
		echo
		exit 1
	fi
	[ -f "$A" ] || die "make all n'a pas créé libft.a à la racine"
}

# ----------------------------------------------------------------- checks --
run_checks() {
	emit S "STRUCTURE & MAKEFILE"

	# files at the root
	local extra=() sub=()
	"${MK[@]}" fclean >/dev/null 2>&1
	while IFS= read -r f; do
		f="${f#./}"
		case "$f" in
			Makefile|libft.h|ft_*.c|README.md|.git|.gitignore) ;;
			*) if [ -d "$LIBFT/$f" ]; then sub+=("$f/"); else extra+=("$f"); fi ;;
		esac
	done < <(cd "$LIBFT" && ls -A)
	[ -f "$LIBFT/libft.h" ] && ck OK "Fichiers à rendre" "Makefile, libft.h, ft_*.c à la racine" \
		|| ck KO "Fichiers à rendre" "libft.h absent de la racine"
	if [ ${#extra[@]} -gt 0 ] || [ ${#sub[@]} -gt 0 ]; then
		ck WARN "Fichiers en trop" "${extra[*]} ${sub[*]} (fichiers inutiles interdits, sujet IV.1)"
	fi

	# rules
	local missing=()
	for r in libft.a all clean fclean re; do
		"${MK[@]}" -n "$r" >/dev/null 2>&1 || missing+=("$r")
	done
	grep -qE '^[[:space:]]*NAME[[:space:]]*:?=' "$LIBFT/Makefile" || missing+=('$(NAME)')
	if [ ${#missing[@]} -eq 0 ]; then
		ck OK "Règles Makefile" "\$(NAME) all clean fclean re"
	else
		ck KO "Règles Makefile" "manquante(s) : ${missing[*]}"
	fi

	# compile commands (dry-run, even with @ silenced lines)
	local dry cl bad_flags=0 bad_cc=0 c99=0 n=0
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

	# build + relink
	build_libft
	step "vérification du relink"
	local relink
	relink="$("${MK[@]}" -n all 2>/dev/null; "${MK[@]}" -n libft.a 2>/dev/null)"
	if echo "$relink" | grep -qE '(^|[[:space:]/])(cc|gcc|clang|ar)[[:space:]]'; then
		ck KO "Relink" "make all relance une compilation alors que rien n'a changé"
		detail "$(echo "$relink" | grep -E '(cc|ar) ' | head -n 2 | tr '\n' ' ')"
	else
		ck OK "Relink" "aucun relink"
	fi

	# clean / fclean
	step "make clean / fclean"
	"${MK[@]}" clean >/dev/null 2>&1
	local objs
	objs="$(find "$LIBFT" -name '*.o' 2>/dev/null | wc -l | tr -d ' ')"
	if [ "$objs" != "0" ]; then
		ck KO "make clean" "$objs fichier(s) .o restant(s)"
	elif [ ! -f "$A" ]; then
		ck WARN "make clean" "clean supprime aussi libft.a (c'est le rôle de fclean)"
	else
		ck OK "make clean" "supprime les .o, garde libft.a"
	fi
	"${MK[@]}" fclean >/dev/null 2>&1
	[ -f "$A" ] && ck KO "make fclean" "libft.a n'est pas supprimé" \
		|| ck OK "make fclean" "supprime libft.a"
	build_libft

	# README
	local rd="$LIBFT/README.md"
	if [ ! -f "$rd" ]; then
		ck KO "README.md" "absent (obligatoire, sujet chap. V)"
	else
		local first_line miss=()
		first_line="$(head -n 1 "$rd" | tr -d '\r')"
		grep -qiE '^#+[[:space:]]*description' "$rd" || miss+=("Description")
		grep -qiE '^#+[[:space:]]*instructions?' "$rd" || miss+=("Instructions")
		grep -qiE '^#+[[:space:]]*resources?' "$rd" || miss+=("Resources")
		if ! echo "$first_line" | grep -qE '^[*_]This project has been created as part of the 42 curriculum by [^*_]+[*_][[:space:]]*$'; then
			ck KO "README.md" "1re ligne attendue en italique : *This project has been created as part of the 42 curriculum by <login>*"
		elif [ ${#miss[@]} -gt 0 ]; then
			ck WARN "README.md" "section(s) manquante(s) : ${miss[*]}"
		else
			ck OK "README.md" "1re ligne + Description / Instructions / Resources"
		fi
	fi

	emit S "CODE"
	# norminette
	if [ $DO_NORM -eq 0 ]; then
		ck SKIP "Norminette" "désactivée (--no-norm)"
	elif command -v norminette >/dev/null 2>&1; then
		step "norminette"
		local nout nerr nfiles
		nout="$(cd "$LIBFT" && norminette 2>&1)"
		nerr="$(echo "$nout" | grep -cE '^[[:space:]]*Error:')"
		nfiles="$(echo "$nout" | grep -E ': Error!' | sed 's/: Error!.*//' | xargs -n1 basename 2>/dev/null | tr '\n' ' ')"
		if [ "$nerr" = "0" ] && ! echo "$nout" | grep -q 'Error!'; then
			ck OK "Norminette" "aucune erreur"
		else
			ck KO "Norminette" "$nerr erreur(s) dans : $nfiles"
			while IFS= read -r l; do detail "$l"; done < <(echo "$nout" | grep -B0 -E 'Error:' | head -n 6 | sed 's/|/ /g')
		fi
	else
		ck SKIP "Norminette" "non installée (pip install norminette)"
	fi

	# symbols
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
		ck OK "Fonctions externes" "uniquement malloc, free, write"
	else
		ck KO "Fonctions externes" "interdite(s) :"
		for f in "${forb[@]}"; do detail "$f"; done
	fi

	local glob
	glob="$(nm --defined-only -A "$A" 2>/dev/null | awk '$(NF-1) ~ /^[DBCRGSV]$/ {n=split($1,a,":"); print $NF " (" a[n-1] ")"}' | tr '\n' ' ')"
	[ -z "$glob" ] && ck OK "Variables globales" "aucune" \
		|| ck KO "Variables globales" "interdites : $glob"

	local helpers
	helpers="$(nm --defined-only "$A" 2>/dev/null | awk '$2=="T" && $3 !~ /^ft_/ {print $3}' | sort -u | tr '\n' ' ')"
	[ -z "$helpers" ] && ck OK "Fonctions auxiliaires" "toutes static" \
		|| ck WARN "Fonctions auxiliaires" "non static : $helpers(le sujet demande static)"

	local absent=()
	for f in "${FUNCS[@]}"; do
		nm --defined-only "$A" 2>/dev/null | awk '$2=="T"{print $3}' | grep -qx "$f" || absent+=("$f")
	done
	[ ${#absent[@]} -eq 0 ] && ck OK "Fonctions obligatoires" "43/43 présentes dans libft.a" \
		|| ck KO "Fonctions obligatoires" "$((43 - ${#absent[@]}))/43 — absentes : ${absent[*]}"

	check_protos
}

# ------------------------------------------------------------- prototypes --
check_protos() {
	emit S "PROTOTYPES · man Linux + sujet v19.3"
	local H="$LIBFT/libft.h" pre="$BUILD/libft_pre.h" prefix=""
	if [ ! -f "$H" ]; then
		ck KO "libft.h" "absent"
		return
	fi
	if ! printf '#include "libft.h"\nint main(void){return 0;}\n' \
		| "$CC_BIN" -fsyntax-only -I"$LIBFT" -x c - 2>"$BUILD/hdr.err"; then
		prefix=$'#include <stddef.h>\n#include <stdlib.h>\n#include <unistd.h>\n'
		if printf '%s#include "libft.h"\nint main(void){return 0;}\n' "$prefix" \
			| "$CC_BIN" -fsyntax-only -I"$LIBFT" -x c - 2>/dev/null; then
			ck WARN "libft.h autonome" "ne compile pas seul : il manque un #include (stddef.h ?)"
		else
			ck KO "libft.h" "ne compile pas : $(head -n 1 "$BUILD/hdr.err" | sed 's/|/ /g')"
			return
		fi
	else
		ck OK "libft.h autonome" "compile seul"
	fi

	# t_list
	if printf '%s#include "libft.h"\n#include <stddef.h>\n'\
'_Static_assert(__builtin_types_compatible_p(__typeof__(((t_list *)0)->content), void *), "content");\n'\
'_Static_assert(__builtin_types_compatible_p(__typeof__(((t_list *)0)->next), struct s_list *), "next");\n'\
'_Static_assert(offsetof(t_list, content) == 0, "order");\n'\
'_Static_assert(sizeof(t_list) == 2 * sizeof(void *), "size");\n' "$prefix" \
		| "$CC_BIN" -fsyntax-only -I"$LIBFT" -x c - 2>/dev/null; then
		ck OK "Structure t_list" "struct s_list { void *content; struct s_list *next; }"
	else
		ck KO "Structure t_list" "doit être exactement : typedef struct s_list { void *content; struct s_list *next; } t_list;"
	fi

	printf '%s#include "libft.h"\n' "$prefix" | "$CC_BIN" -E -P -I"$LIBFT" -x c - > "$pre" 2>/dev/null
	tr '\n\t' '  ' < "$pre" | sed 's/;/;\n/g' > "$BUILD/decls.txt"

	local src="$BUILD/proto_check.c" names=() exps=() founds=()
	{
		printf '%s#include "libft.h"\n#include <stdio.h>\n' "$prefix"
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
	done < "$FT_DIR/protos.txt"
	printf '\treturn (0);\n}\n' >> "$src"

	if "$CC_BIN" -w -I"$LIBFT" "$src" -o "$BUILD/proto_check" 2>"$BUILD/proto.err"; then
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

# ------------------------------------------------------------------- main --
if [ $DO_CHECKS -eq 1 ]; then
	run_checks
else
	build_libft
	: > "$CHECKS"
fi

step "compilation du tester"
USYMS=()
for s in $(nm --defined-only "$A" 2>/dev/null | awk '$2=="T" && $3 ~ /^ft_/ {print $3}' | sort -u); do
	USYMS+=("-Wl,-u,$s")
done
if ! "$CC_BIN" -std=gnu11 -O0 -g -w -fno-builtin -I"$FT_DIR/src" "$FT_DIR"/src/*.c \
	-Wl,--wrap=malloc -Wl,--wrap=free "${USYMS[@]}" "$A" \
	-o "$BUILD/ftorture_tester" 2> "$BUILD/link.log"; then
	printf '%s\n %s✘ Impossible de lier le tester avec ta libft.a :%s\n\n' "$CL" "$R$B" "$X"
	head -n 20 "$BUILD/link.log" | sed 's/^/     /'
	echo
	echo "   ${D}(un symbole de ta libft entre en conflit, ou libft.a contient un main ?)${X}"
	exit 1
fi
[ -t 1 ] && printf '%s' "$CL"

CHK_ARGS=()
[ $DO_CHECKS -eq 1 ] && CHK_ARGS=(--checks "$CHECKS")
cd "$BUILD" || exit 2
exec "$BUILD/ftorture_tester" "${CHK_ARGS[@]}" --path "$LIBFT" --log "$FT_DIR/ftorture.log" "${PASS[@]}"
