# **************************************************************************** #
#  ftorture — get_next_line (subject v14.3), sourced by torture.sh.            #
#  No Makefile in this project: the sources are compiled by ftorture itself,  #
#  in its own build folder (nothing is written in your repository), once per  #
#  BUFFER_SIZE. Each size is one line of the report.                           #
# **************************************************************************** #

GNL_M=(get_next_line.c get_next_line_utils.c)
GNL_B=(get_next_line_bonus.c get_next_line_utils_bonus.c)
GNL_FLAGS=(-Wall -Wextra -Werror)
GNL_M_SIZES=(1 2 3 5 7 10 42 100 1024 9999 100000 10000000 def 0)
GNL_B_SIZES=(1 42 9999 10000000)
GNL_FAILED=0

# gnl_compile OUTDIR [defines...]   compiles GNL_SRC into OUTDIR/*.o
gnl_compile() {
	local out="$1" f
	shift
	mkdir -p "$out"
	: > "$out/cc.log"
	for f in "${GNL_SRC[@]}"; do
		"$CC_BIN" "${GNL_FLAGS[@]}" "$@" -I"$DIR" -c "$DIR/$f" -o "$out/${f%.c}.o" \
			2>> "$out/cc.log" || return 1
	done
}

gnl_readme() {
	local rd="$DIR/README.md" first_line miss=()
	if [ ! -f "$rd" ]; then
		ck KO "README.md" "absent (obligatoire, chapitre Readme du sujet)"
		return
	fi
	first_line="$(head -n 1 "$rd" | tr -d '\r')"
	grep -qiE '^#+[[:space:]]*description' "$rd" || miss+=("Description")
	grep -qiE '^#+[[:space:]]*instructions?' "$rd" || miss+=("Instructions")
	grep -qiE '^#+[[:space:]]*resources?' "$rd" || miss+=("Resources")
	grep -qiE 'algorithm|algorithme' "$rd" || miss+=("explication de l'algorithme")
	if ! echo "$first_line" | grep -qE '^[*_]This project has been created as part of the 42 curriculum by [^*_]+[*_][[:space:]]*$'; then
		ck KO "README.md" "1re ligne attendue en italique : *This project has been created as part of the 42 curriculum by <login>*"
	elif [ ${#miss[@]} -gt 0 ]; then
		ck WARN "README.md" "manquant : ${miss[*]}"
	else
		ck OK "README.md" "1re ligne + sections + algorithme"
	fi
}

run_checks_gnl() {
	local f missing=() nb=0

	emit S "FICHIERS"
	for f in get_next_line.c get_next_line_utils.c get_next_line.h; do
		[ -f "$DIR/$f" ] || missing+=("$f")
	done
	[ ${#missing[@]} -eq 0 ] && ck OK "Fichiers à rendre" "get_next_line.c, get_next_line_utils.c, get_next_line.h" \
		|| ck KO "Fichiers à rendre" "absent(s) : ${missing[*]}"
	for f in get_next_line_bonus.c get_next_line_bonus.h get_next_line_utils_bonus.c; do
		[ -f "$DIR/$f" ] && nb=$((nb + 1))
	done
	case $nb in
		3) ck OK "Fichiers bonus" "les 3 fichiers _bonus sont présents" ;;
		0) ck SKIP "Fichiers bonus" "absents : bonus non testé" ;;
		*) ck WARN "Fichiers bonus" "$nb/3 fichiers _bonus (il faut les 3)" ;;
	esac
	check_files
	gnl_readme

	emit S "COMPILATION"
	step "compilation de contrôle"
	GNL_SRC=("${GNL_M[@]}")
	if gnl_compile "$BUILD/chk42" -D BUFFER_SIZE=42; then
		ck OK "-D BUFFER_SIZE=42" "cc -Wall -Wextra -Werror -D BUFFER_SIZE=42 *.c"
	else
		ck KO "-D BUFFER_SIZE=42" "ne compile pas avec les flags du sujet"
		while IFS= read -r l; do detail "$l"; done < <(head -n 4 "$BUILD/chk42/cc.log" | sed 's/|/ /g')
		GNL_FAILED=1
	fi
	if gnl_compile "$BUILD/chkdef"; then
		ck OK "Sans -D BUFFER_SIZE" "une valeur par défaut est définie"
	else
		ck KO "Sans -D BUFFER_SIZE" "doit aussi compiler sans -D (valeur par défaut, sujet)"
		GNL_FAILED=1
	fi
	if [ $HAS_BONUS = 1 ]; then
		GNL_SRC=("${GNL_B[@]}")
		if gnl_compile "$BUILD/chkb" -D BUFFER_SIZE=42; then
			ck OK "Bonus" "les fichiers _bonus compilent"
		else
			ck KO "Bonus" "les fichiers _bonus ne compilent pas"
			while IFS= read -r l; do detail "$l"; done < <(head -n 4 "$BUILD/chkb/cc.log" | sed 's/|/ /g')
			HAS_BONUS=0
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

	local objs=("$BUILD"/chk42/*.o)
	[ $HAS_BONUS = 1 ] && objs+=("$BUILD"/chkb/*.o)
	if [ -f "${objs[0]}" ]; then
		local def und ext s forb=() allowed=" read malloc free " tolerated=" __stack_chk_fail _GLOBAL_OFFSET_TABLE_ "
		def="$(nm --defined-only "${objs[@]}" 2>/dev/null | awk 'NF>=3{print $NF}' | LC_ALL=C sort -u)"
		und="$(nm -u "${objs[@]}" 2>/dev/null | awk '$1=="U"{print $2}' | LC_ALL=C sort -u)"
		ext="$(LC_ALL=C comm -23 <(echo "$und") <(echo "$def"))"
		for s in $ext; do
			case "$allowed$tolerated" in *" $s "*) continue ;; esac
			[ "$s" = lseek ] && forb+=("lseek (interdit explicitement par le sujet)") && continue
			case "$s" in ft_*) forb+=("$s (la libft est interdite dans ce projet)"); continue ;; esac
			forb+=("$s")
		done
		if [ ${#forb[@]} -eq 0 ]; then
			ck OK "Fonctions externes" "uniquement read, malloc, free"
		else
			ck KO "Fonctions externes" "interdite(s) :"
			for f in "${forb[@]}"; do detail "$f"; done
		fi

		local glob
		glob="$(nm --defined-only -A "${objs[@]}" 2>/dev/null | awk '$(NF-1) ~ /^[DBCRGSV]$/ {n=split($1,a,"/"); print $NF " (" a[n] ")"}' | tr '\n' ' ')"
		[ -z "$glob" ] && ck OK "Variables globales" "aucune" \
			|| ck KO "Variables globales" "interdites par le sujet : $glob"

		nm --defined-only "$BUILD"/chk42/*.o 2>/dev/null | awk '$2=="T"{print $3}' | grep -qx get_next_line \
			&& ck OK "get_next_line" "définie dans get_next_line.c" \
			|| ck KO "get_next_line" "fonction introuvable dans les objets compilés"

		if [ $HAS_BONUS = 1 ]; then
			# local static variables show up as lowercase b / d symbols
			local statics n
			statics="$(nm --defined-only "$BUILD"/chkb/*.o 2>/dev/null | awk '$2 ~ /^[bd]$/ {print $3}' | sort -u)"
			n="$(echo "$statics" | grep -c .)"
			if [ "$n" -le 1 ]; then
				ck OK "Bonus : variable static" "une seule ($(echo "$statics" | tr '\n' ' '))"
			else
				ck KO "Bonus : variable static" "$n trouvées, le sujet en autorise une seule : $(echo "$statics" | tr '\n' ' ')"
			fi
		fi
	fi

	check_protos
}

# size_selected SIZE : honours -f 1,42,def
size_selected() {
	[ -z "$FUNC_FILTER" ] && return 0
	case ",$FUNC_FILTER," in *",$1,"*) return 0 ;; esac
	return 1
}

# gnl_pass KEY SIZE BONUS SPEC PART
# Every pass adds its counters to $STATS; the summary is printed at the end
# by gnl_summary (so it is shown even if the last size does not compile).
gnl_pass() {
	local key="$1" bs="$2" bonus="$3" spec="$4" part="$5"
	local out="$BUILD/$key" zbs=-1 defs=() tests=() args=()

	if [ "$bs" != def ]; then
		defs=(-D "BUFFER_SIZE=$bs")
		zbs="$bs"
	fi
	if [ "$bonus" = 1 ]; then GNL_SRC=("${GNL_B[@]}"); else GNL_SRC=("${GNL_M[@]}"); fi
	step "BUFFER_SIZE=$bs : compilation"
	if ! gnl_compile "$out" "${defs[@]}"; then
		if [ "$bs" = 0 ]; then
			printf '%s   %-16s %s▲ ne compile pas avec BUFFER_SIZE=0 (accepté)%s\n' "$CL" "BUFFER_SIZE=0" "$Y" "$X"
		else
			printf '%s   %-16s %s✘ ne compile pas avec ce BUFFER_SIZE%s\n' "$CL" "BUFFER_SIZE=$bs" "$R" "$X"
			head -n 4 "$out/cc.log" | sed 's/^/        /'
			GNL_FAILED=1
		fi
		return 0
	fi
	tests=("$FT_DIR/src/gnl/gnl.c" "$FT_DIR/src/gnl/proj.c" "$FT_DIR/src/gnl/t_gnl.c")
	[ "$bonus" = 1 ] && tests+=("$FT_DIR/src/gnl/t_gnl_bonus.c")
	if ! "$CC_BIN" -std=gnu11 -O0 -g -w -fno-builtin -I"$FT_DIR/src" -I"$FT_DIR/src/gnl" \
		-D Z_BS="$zbs" -D GNL_SUITE="$key" "$BUILD/engine.o" "$BUILD/memtrack.o" \
		"${tests[@]}" "$out"/*.o -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=read \
		-o "$out/tester" 2> "$out/link.log"; then
		printf '%s   %-16s %s✘ impossible de lier le tester%s\n' "$CL" "BUFFER_SIZE=$bs" "$R" "$X"
		head -n 6 "$out/link.log" | sed 's/^/        /'
		GNL_FAILED=1
		return 0
	fi
	[ -t 1 ] && printf '%s' "$CL"
	args=(--no-banner --path "$DIR" --log "$FT_DIR/ftorture.log" "${PASS[@]}"
		-f "$key" --no-summary --stats-out "$STATS")
	[ -z "$TIMEOUT" ] && args+=(--timeout 10)
	if [ "$GNL_SHOWN" = 0 ]; then
		# first pass that runs: checks + legend, fresh counters
		[ $DO_CHECKS -eq 1 ] && args+=(--checks "$CHECKS")
		GNL_SHOWN=1
	else
		args+=(--no-legend --stats-in "$STATS")
	fi
	if [ "$part" = "$GNL_PART_SHOWN" ]; then
		args+=(--no-section)
	fi
	GNL_PART_SHOWN="$part"
	[ "$spec" = 1 ] && args+=(--spec-pass)
	(cd "$BUILD" && "$out/tester" "${args[@]}")
	GNL_LAST="$out/tester"
	return 0
}

# summary of every pass, printed by the last tester that was built
gnl_summary() {
	[ -n "$GNL_LAST" ] || return 1
	(cd "$BUILD" && "$GNL_LAST" --no-banner --no-legend --no-section \
		--path "$DIR" --log "$FT_DIR/ftorture.log" "${PASS[@]}" \
		--stats-in "$STATS" -f "__resume__" "${GNL_NOTE[@]}")
}

run_gnl() {
	local passes=() p key bs bonus spec i n status

	if [ $DO_CHECKS -eq 1 ]; then
		run_checks_gnl
	fi
	STATS="$BUILD/stats.bin"
	GNL_NOTE=()
	GNL_SHOWN=0
	GNL_PART_SHOWN=""
	GNL_LAST=""

	# shared engine objects, compiled once
	step "compilation du moteur"
	if ! { "$CC_BIN" -std=gnu11 -O0 -g -w -fno-builtin -I"$FT_DIR/src" \
		-c "$FT_DIR/src/engine.c" -o "$BUILD/engine.o" \
		&& "$CC_BIN" -std=gnu11 -O0 -g -w -fno-builtin -I"$FT_DIR/src" \
		-c "$FT_DIR/src/memtrack.c" -o "$BUILD/memtrack.o"; } 2> "$BUILD/engine.log"; then
		cat "$BUILD/engine.log"
		die "le moteur de ftorture ne compile pas"
	fi

	# pass list: "part key size bonus spec"
	if [ "$PART" != 2 ]; then
		for bs in "${GNL_M_SIZES[@]}"; do
			size_selected "$bs" || continue
			spec=0
			{ [ "$bs" = 0 ] || [ "$bs" = 10000000 ]; } && spec=1
			passes+=("1 bs$bs $bs 0 $spec")
		done
	fi
	if [ "$PART" != 1 ]; then
		if [ $HAS_BONUS = 1 ]; then
			for bs in "${GNL_B_SIZES[@]}"; do
				size_selected "$bs" || continue
				spec=0
				[ "$bs" = 10000000 ] && spec=1
				passes+=("2 bbs$bs $bs 1 $spec")
			done
		else
			tr_text 'Bonus non testé : fichiers _bonus absents ou qui ne compilent pas'
			GNL_NOTE=(--note "$REPLY")
		fi
	fi
	n=${#passes[@]}
	[ "$n" -eq 0 ] && die "aucun BUFFER_SIZE sélectionné (-f 1,42,def…)"
	for ((i = 0; i < n; i++)); do
		read -r p key bs bonus spec <<< "${passes[$i]}"
		gnl_pass "$key" "$bs" "$bonus" "$spec" "$p"
	done
	gnl_summary
	status=$?
	[ $GNL_FAILED = 1 ] && return 1
	return $status
}
