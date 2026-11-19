#!/usr/bin/env bash
# Translate the shell frontend's user-facing output without changing test logic.
# The original French strings remain the source for both languages.
tr_text() {
	REPLY="$1"
	[ "$LANG_CODE" = en ] || return 0
	local from to
	while IFS='|' read -r from to; do
		[ -n "$from" ] && REPLY="${REPLY//"$from"/"$to"}"
	done <<'WORDS'
menu interactif|interactive menu
liste de BUFFER_SIZE|BUFFER_SIZE list
ex.|e.g.
LISTE|LIST
obligatoire|mandatory
chapitre Readme du sujet|subject's README chapter
uniquement|only
autonome|self-contained
sujet|subject
la norme|the standard
inutiles|unneeded
à rendre|to submit
bonus non testé|bonus not tested
présents|present
dans la|in the
au moins un|at least one
les flags du sujet|the required flags
une valeur|a value
norme|norm
protos|prototypes
les deux ok|both succeed
même sans -s|even without -s
non     |does not
ta fonction ok|your function succeeds
graine|seed
fichiers inutiles à ne pas rendre|unneeded files, do not submit
fichiers _bonus absents ou qui ne compilent pas|bonus files missing or failing to compile
il faut les 3|all three required
les 3 fichiers _bonus sont présents|all 3 bonus files are present
une valeur par défaut est définie|a default value is defined
doit aussi compiler sans -D|must also compile without -D
une seule|only one
trouvées, le sujet en autorise|found, the subject allows
fonction introuvable dans les objets compilés|function not found in compiled objects
définie dans|defined in
les fichiers _bonus compilent|bonus files compile
les fichiers _bonus ne compilent pas|bonus files do not compile
ne compile pas avec ce BUFFER_SIZE|does not compile with this BUFFER_SIZE
accepté|accepted
impossible de lier le tester|cannot link tester
réservé|reserved
FICHIERS|FILES
COMPILATION|COMPILATION
STRUCTURE & MAKEFILE|STRUCTURE & MAKEFILE
CODE|CODE
PROTOTYPE|PROTOTYPE
vérification|check
vérifs|checks
sans -D|without -D
Sans -D|Without -D
Fichiers bonus|Bonus files
Fichiers à rendre|Files to submit
absents|missing
fichier(s)|file(s)
restant(s)|remaining
garde|keeps
supprime aussi|also removes
supprime|removes
non détectée|not detected
aucune commande de compilation détectée|no compile command detected
commande ar non détectée|ar command not detected
créée avec ar|created with ar
le sujet impose cc|the subject requires cc
trouvé|found
interdit par le sujet|forbidden by subject
le bonus sera testé|bonus will be tested
bonus non testé|bonus not tested
relance une compilation alors que rien n'a changé|rebuilds despite no changes
non static|not static
le sujet demande static|the subject requires static
pas de|no
dans :|in:
manquant :|missing:
à savoir défendre|to review
jamais libéré(s)|never freed
à ne pas rendre|do not submit
1re ligne + sections + algorithme|first line + sections + algorithm
1re ligne + sections demandées|first line + required sections
explication de l'algorithme|algorithm explanation
obligatoire, chapitre Readme du sujet|required by the subject's README chapter
le sujet|the subject
dossier|directory
projet détecté|project detected
les erreurs spéculatives|speculative errors
comptent comme des erreurs|count as errors
affiche le détail de chaque échec|show details of every failure
saute les vérifications|skip checks
saute norminette|skip norminette
graine des tests aléatoires|random test seed
défaut|default
timeout par test en secondes|timeout per test in seconds
sans couleurs|no colors
Tests comparés à la vraie fonction|Tests compared with the real function
ta fonction|your function
la vraie|the reference
même comportement|same behavior
ERREUR|ERROR
spéculatif|speculative
tu protèges, la libc non|your code protects, libc does not
les résultats doivent être identiques|results must match
option inconnue|unknown option
QUEL PROJET ON TORTURE ?|WHICH PROJECT TO TEST?
QUITTER|QUIT
fonctions|functions
parties|parts
largeur|width
choisir|select
Entrée valider|Enter confirm
raccourcis|shortcuts
quitter|quit
dossier courant|current directory
détecté|detected
OÙ EST TON|WHERE IS YOUR
Tab pour compléter|Tab to complete
Entrée pour valider|Enter to confirm
chemin|path
introuvable|not found
pas de Makefile dans|no Makefile in
vérifications|checks
tous les détails des échecs|all failure details
les ▲ comptent comme erreurs|▲ count as errors
LANCER LA TORTURE|START TESTING
naviguer|navigate
Espace cocher|Space toggle
Entrée lancer|Enter start
ni libft.h, ni ft_printf.h, ni get_next_line.h dans|no libft.h, ft_printf.h or get_next_line.h in
précise|specify
vise Linux|targets Linux
Résultats non garantis sur|Results not guaranteed on
compilateur|compiler
impossible de créer|cannot create
échoue|failed
n'a pas créé|did not create
à la racine|at the root
Impossible de lier le tester avec ton|Cannot link the tester with your
un symbole entre en conflit, ou l'archive contient un main ?|symbol conflict, or the archive contains main?
non testé|not tested
pas de règle bonus dans le Makefile|no bonus rule in the Makefile
ta libft va parler|your libft will talk
ton printf va parler|your printf will talk
ta gnl va parler|your gnl will talk
ton code va parler|your code will talk
SUJET|SUBJECT
projet|project
strict (-s) : les ▲ comptent comme erreurs|strict (-s): ▲ count as errors
normal : ▲ = avertissements (-s pour les compter)|normal: ▲ are warnings (-s to count them)
Fichiers à rendre|Files to submit
Fichiers en trop|Extra files
Règles Makefile|Makefile rules
Règle bonus|Bonus rule
Flags de compilation|Compiler flags
Compilateur|Compiler
Fonctions externes|External functions
Variables globales|Global variables
Fonctions auxiliaires|Helper functions
Fonctions obligatoires|Mandatory functions
Structure t_list|t_list structure
manquante(s)|missing
absente(s)|missing
absent|missing
présente(s)|present
présente|present
aucune erreur|no errors
aucun relink|no relink
aucune|none
interdites|forbidden
interdite(s)|forbidden
non installée|not installed
désactivée|disabled
ne compile pas|does not compile
compile seul|compiles on its own
vérification impossible|unable to check
1re ligne attendue en italique|first line must be italic
manquant|missing
vérification du relink|checking relink
compilation du tester|compiling tester
compilation du moteur|compiling engine
compilation de contrôle|checking compilation
Bonus non testé|Bonus not tested
aucun BUFFER_SIZE sélectionné|no BUFFER_SIZE selected
WORDS
}

# printf is also used for compiler-generated source: preserve its format and
# arguments unless they contain a known human-readable phrase.
printf() {
	local format="$1" arg
	shift
	tr_text "$format"; format="$REPLY"
	local args=()
	for arg in "$@"; do tr_text "$arg"; args+=("$REPLY"); done
	builtin printf -- "$format" "${args[@]}"
}
