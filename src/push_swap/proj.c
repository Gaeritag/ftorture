/* push_swap project descriptor (subject v1.2).
   The suites run ./push_swap and ./checker from the project directory. */
#include "zft.h"
#include "project.h"

static int	g_present = 1;

#define S(key, label, part) { #key, label, part, &g_present }

static t_func	g_ps_suites[] = {
	S(args, "sans argument", 1), S(errors, "erreurs", 1),
	S(sorted, "déjà trié", 1), S(small, "2 à 5 nombres", 1),
	S(simple, "--simple", 1), S(medium, "--medium", 1),
	S(complex, "--complex", 1), S(adaptive, "--adaptive", 1),
	S(bench, "--bench", 1), S(perf100, "100 aléatoires", 1),
	S(perf500, "500 aléatoires", 1),
	S(checker, "checker", 2), S(ck_errors, "erreurs checker", 2),
};

t_project	g_proj = {
	"push_swap", "push_swap", "push_swap.h",
	"PUSH_SWAP · SUJET v1.2 · LINUX",
	"> ton push_swap va parler",
	g_ps_suites,
	(int)(sizeof(g_ps_suites) / sizeof(g_ps_suites[0])),
	2,
	{"", "Obligatoire · push_swap", "Bonus · checker", ""},
	{"", "obligatoire", "bonus", ""},
};
