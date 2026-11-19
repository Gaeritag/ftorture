# **************************************************************************** #
#  ftorture — checks shared by every project, sourced by torture.sh.          #
#    check_files     KO on files that have nothing to do in the repository   #
#    check_includes  KO on #include lines whose header is never used         #
# **************************************************************************** #

# ----------------------------------------------------------- extra files --
# allowed_path PROJ REL   is this file expected in a PROJ repository?
allowed_path() {
	local base="${2##*/}"
	case "$base" in .gitignore) return 0 ;; esac
	case "$1:$2" in
		*:Makefile|*:README.md) return 0 ;;
		libft:libft.h|libft:ft_*.c) return 0 ;;
		printf:*/*/*) return 1 ;;
		printf:*.c|printf:*.h) return 0 ;;	# root and one sub-folder (*/*.c)
		printf:libft/Makefile|printf:libft/README.md) return 0 ;;
		gnl:get_next_line.c|gnl:get_next_line_utils.c|gnl:get_next_line.h) return 0 ;;
		gnl:get_next_line_bonus.c|gnl:get_next_line_utils_bonus.c|gnl:get_next_line_bonus.h) return 0 ;;
	esac
	return 1
}

# why_extra REL   short reason shown next to a rejected file
why_extra() {
	local base="${1##*/}" ext
	case "/$1" in */.*) echo "fichier caché"; return ;; esac
	case "$base" in
		*.o|*.a|*.so|*.out|a.out) echo "binaire / objet compilé" ;;
		*.swp|*.swo|*~|\#*\#) echo "fichier temporaire d'éditeur" ;;
		*.*) ext="${base##*.}"; echo "extension .$ext inattendue" ;;
		*) echo "fichier inattendu" ;;
	esac
}

# check_files   needs DIR, PROJ; run it before any build (after fclean)
check_files() {
	local f bad=() n=0
	while IFS= read -r -d '' f; do
		f="${f#./}"
		if [ -d "$DIR/$f" ]; then
			# an empty folder is rejected too; a full one through its files
			[ -z "$(ls -A "$DIR/$f")" ] || continue
			case "/$f" in */.*) ;; *) [ "$PROJ" = printf ] && [ "$f" = libft ] && continue ;; esac
			bad+=("$f/ (dossier vide)")
			continue
		fi
		allowed_path "$PROJ" "$f" || bad+=("$f ($(why_extra "$f"))")
	done < <(cd "$DIR" && find . -mindepth 1 -name .git -prune -o -print0)
	n=${#bad[@]}
	if [ "$n" -eq 0 ]; then
		ck OK "Fichiers en trop" "aucun"
	else
		ck KO "Fichiers en trop" "$n fichier(s) à ne pas rendre :"
		for f in "${bad[@]:0:12}"; do detail "$f"; done
		[ "$n" -gt 12 ] && detail "… et $((n - 12)) autre(s)"
	fi
	return 0
}

# -------------------------------------------------------------- includes --
# inc_strip FILE [keep]   C source without comments; strings / chars
#                          become "" '' unless keep is given
inc_strip() {
	awk -v keep="${2:+1}" '
	BEGIN { st = 0 }	# 0 code, 1 /* */, 2 "..", 3 '"'"'..'"'"'
	{
		out = ""; n = length($0)
		for (i = 1; i <= n; i++) {
			c = substr($0, i, 1); d = substr($0, i + 1, 1)
			if (st == 1) { if (c == "*" && d == "/") { st = 0; i++ } continue }
			if (st == 2 || st == 3) {
				if (c == "\\") { if (keep) out = out c d; i++; continue }
				if ((st == 2 && c == "\"") || (st == 3 && c == "'"'"'")) st = 0
				if (keep || st == 0) out = out c
				continue
			}
			if (c == "/" && d == "*") { st = 1; out = out " "; i++; continue }
			if (c == "/" && d == "/") break
			if (c == "\"") { st = 2; out = out c; continue }
			if (c == "'"'"'") { st = 3; out = out c; continue }
			out = out c
		}
		print out
	}' "$1"
}

# inc_decls   reads stripped C on stdin, prints every name it declares:
#             functions, variables, typedefs, struct/union/enum tags, enum
#             constants and #define names (reserved _names are skipped)
inc_decls() {
	awk '
	function emit(s) { if (s !~ /^_/ && !(s in KW)) print s }
	function flush(   k, fn, last, pd, t, ty) {
		fn = ""; last = ""; pd = 0; ty = 0
		for (k = 1; k <= nt; k++) {
			t = tok[k]
			if (t == "typedef") ty = 1
			if (t == "(") { pd++; if (pd == 1 && k > 1 && ID[k - 1] && fn == "" && !(tok[k - 1] in KW) && tok[k - 1] !~ /^__/) fn = tok[k - 1]; continue }
			if (t == ")") { pd--; continue }
			if (pd == 1 && tok[k - 1] == "(" && t == "*" && ID[k + 1]) { last = tok[k + 1]; fnp = 1 }
			if (pd == 0 && ID[k] && !(t in KW) && t !~ /^__/) last = t
		}
		if (ty || fn == "") { if (last != "") emit(last) }
		else emit(fn)
		nt = 0
	}
	BEGIN {
		split("auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while asm", a, " ")
		for (i in a) KW[a[i]] = 1
		depth = 0; nt = 0; inenum = 0
	}
	/^[ \t]*#/ {
		if (match($0, /^[ \t]*#[ \t]*define[ \t]+[A-Za-z_][A-Za-z0-9_]*/)) {
			s = substr($0, RSTART, RLENGTH); sub(/.*define[ \t]+/, "", s); emit(s)
		}
		next
	}
	{
		line = $0
		while (match(line, /[A-Za-z_][A-Za-z0-9_]*|[(){};,=*\[\]]/)) {
			t = substr(line, RSTART, RLENGTH); line = substr(line, RSTART + RLENGTH)
			isid = (t ~ /^[A-Za-z_]/)
			if (isid && (prev == "struct" || prev == "union" || prev == "enum")) emit(t)
			if (isid && inenum && depth == enumd && (prev == "{" || prev == ",")) emit(t)
			if (t == "{") {
				if (prev == "enum" || prev2 == "enum") { inenum = 1; enumd = depth + 1 }
				if (depth == 0 && prev == ")") body = 1	# function definition
				depth++
			} else if (t == "}") {
				depth--; if (inenum && depth < enumd) inenum = 0
				if (depth == 0 && body) { body = 0; flush() }
			} else if (depth == 0) {
				if (t == ";") flush()
				else { nt++; tok[nt] = t; ID[nt] = isid }
			}
			prev2 = prev; prev = t
		}
	}'
}

# inc_tokens FILE   identifiers used by FILE, #include lines left out
inc_tokens() {
	inc_strip "$1" | grep -vE '^[[:space:]]*#[[:space:]]*include' \
		| grep -oE '[A-Za-z_][A-Za-z0-9_]*' | LC_ALL=C sort -u
}

# inc_sys NAME   names a system header provides (cached); fails if unknown
inc_sys() {
	local key="${1//\//_}" out
	out="$INC_TMP/sys_$key"
	[ -f "$out" ] && { [ -s "$out" ] && cat "$out"; return 0; }
	if ! printf '#include <%s>\n' "$1" | "$CC_BIN" -E -P -x c - > "$INC_TMP/pp" 2>/dev/null; then
		: > "$out"
		return 1
	fi
	{
		inc_decls < "$INC_TMP/pp"
		LC_ALL=C comm -23 \
			<(printf '#include <%s>\n' "$1" | "$CC_BIN" -E -dM -x c - 2>/dev/null | awk '{print $2}' | sed 's/(.*//' | LC_ALL=C sort -u) \
			<("$CC_BIN" -E -dM -x c /dev/null 2>/dev/null | awk '{print $2}' | sed 's/(.*//' | LC_ALL=C sort -u) \
			| grep -v '^_'
	} | LC_ALL=C sort -u > "$out"
	cat "$out"
}

# inc_list FILE   "kind<TAB>name" for each #include of FILE (kind: sys|usr)
inc_list() {
	inc_strip "$1" keep | sed -nE \
		-e 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*<([^>]+)>.*/sys\t\1/p' \
		-e 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*"([^"]+)".*/usr\t\1/p'
}

# inc_resolve FROM NAME   project path of a "header", empty if outside it
inc_resolve() {
	local d c
	d="$(dirname "$1")"
	for c in "$d/$2" "$2"; do
		c="$(cd "$DIR" && realpath -m --relative-to=. "$c" 2>/dev/null)"
		[ -n "$c" ] && [ "${c#../}" = "$c" ] && [ -f "$DIR/$c" ] && { echo "$c"; return; }
	done
}

# inc_provides HDR   names a project header gives to whoever includes it:
#                    its own declarations + all it includes (transitively)
inc_provides() {
	local key="${1//\//_}" out kind name r
	out="$INC_TMP/usr_$key"
	[ -f "$out" ] && { cat "$out"; return; }
	: > "$out"	# recursion guard (headers that include each other)
	{
		inc_strip "$DIR/$1" | inc_decls
		while IFS=$'\t' read -r kind name; do
			if [ "$kind" = sys ]; then
				inc_sys "$name"
			else
				r="$(inc_resolve "$1" "$name")"
				[ -n "$r" ] && inc_provides "$r"
			fi
		done < <(inc_list "$DIR/$1")
	} | LC_ALL=C sort -u > "$out.tmp"
	mv "$out.tmp" "$out"
	cat "$out"
}

# inc_users HDR   tokens of HDR and of every project file that includes it
#                 (directly or through another header)
inc_users() {
	local seen=" $1 " todo=("$1") cur f kind name r
	while [ ${#todo[@]} -gt 0 ]; do
		cur="${todo[0]}"; todo=("${todo[@]:1}")
		cat "$INC_TMP/tok_${cur//\//_}"
		for f in "${INC_FILES[@]}"; do
			case "$seen" in *" $f "*) continue ;; esac
			while IFS=$'\t' read -r kind name; do
				[ "$kind" = usr ] || continue
				r="$(inc_resolve "$f" "$name")"
				if [ "$r" = "$cur" ]; then seen+="$f "; todo+=("$f"); break; fi
			done < <(inc_list "$DIR/$f")
		done
	done
}

# check_includes   needs DIR, CC_BIN, BUILD
check_includes() {
	local f kind name r label why n=0 tot=0 bad=() seen tok
	INC_TMP="$BUILD/includes"
	rm -rf -- "$INC_TMP"
	mkdir -p "$INC_TMP"
	INC_FILES=()
	while IFS= read -r -d '' f; do
		INC_FILES+=("${f#./}")
	done < <(cd "$DIR" && find . -name .git -prune -o -type f \( -name '*.c' -o -name '*.h' \) -print0 | sort -z)
	for f in "${INC_FILES[@]}"; do
		inc_tokens "$DIR/$f" > "$INC_TMP/tok_${f//\//_}"
	done
	# names that almost every header defines: they justify one include of a
	# file (the first one giving them), not a <stdio.h> kept just for NULL
	printf '%s\n' NULL size_t ssize_t wchar_t ptrdiff_t off_t pid_t \
		va_list | LC_ALL=C sort -u > "$INC_TMP/common"
	for f in "${INC_FILES[@]}"; do
		seen=" "
		tok="$INC_TMP/tok_${f//\//_}"
		# a header's includes serve the files including it too
		[ "${f%.h}" != "$f" ] && { inc_users "$f" | LC_ALL=C sort -u > "$INC_TMP/use"; tok="$INC_TMP/use"; }
		: > "$INC_TMP/covered"
		while IFS=$'\t' read -r kind name; do
			tot=$((tot + 1))
			[ "$kind" = sys ] && label="<$name>" || label="\"$name\""
			case "$seen" in *" $kind:$name "*) bad+=("$f : $label inclus deux fois"); continue ;; esac
			seen+="$kind:$name "
			if [ "$kind" = sys ]; then
				inc_sys "$name" > "$INC_TMP/prov" || continue	# unknown here: no verdict
				why="rien de cette lib n'est utilisé"
			else
				r="$(inc_resolve "$f" "$name")"
				[ -z "$r" ] && continue	# outside the repository: not ours to judge
				inc_provides "$r" > "$INC_TMP/prov"
				why="aucune fonction, struct, macro ou lib de ce header n'est utilisée"
			fi
			LC_ALL=C comm -12 "$tok" "$INC_TMP/prov" > "$INC_TMP/hit"
			LC_ALL=C comm -12 "$INC_TMP/hit" "$INC_TMP/common" > "$INC_TMP/hitc"
			if [ -n "$(LC_ALL=C comm -23 "$INC_TMP/hit" "$INC_TMP/common" | head -n 1)" ] \
				|| [ -n "$(LC_ALL=C comm -23 "$INC_TMP/hitc" "$INC_TMP/covered" | head -n 1)" ]; then
				LC_ALL=C sort -u "$INC_TMP/covered" "$INC_TMP/hitc" -o "$INC_TMP/covered"
			elif [ -s "$INC_TMP/hitc" ]; then
				bad+=("$f : $label (seulement $(tr '\n' ' ' < "$INC_TMP/hitc")déjà fourni par un autre include)")
			else
				bad+=("$f : $label ($why)")
			fi
		done < <(inc_list "$DIR/$f")
	done
	n=${#bad[@]}
	if [ "$tot" -eq 0 ]; then
		ck SKIP "Includes" "aucun #include"
	elif [ "$n" -eq 0 ]; then
		ck OK "Includes" "les $tot #include servent tous"
	else
		ck KO "Includes" "$n #include inutile(s) :"
		for f in "${bad[@]:0:12}"; do detail "$f"; done
		[ "$n" -gt 12 ] && detail "… et $((n - 12)) autre(s)"
	fi
	return 0
}
