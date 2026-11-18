/* ft_printf project descriptor (subject v12.1) */
#include "zft.h"
#include "project.h"
#include "pf.h"

#define S(key, label, part) { #key, label, part, (void *)ft_printf }

static t_func	g_pf_suites[] = {
	S(pf_c, "%c", 1), S(pf_s, "%s", 1), S(pf_p, "%p", 1), S(pf_d, "%d", 1),
	S(pf_i, "%i", 1), S(pf_u, "%u", 1), S(pf_x, "%x", 1), S(pf_X, "%X", 1),
	S(pf_pct, "%%", 1), S(pf_mix, "mélanges", 1), S(pf_edge, "cas limites", 1),
	S(pf_minus, "flag -", 2), S(pf_zero, "flag 0", 2),
	S(pf_prec, "précision .", 2), S(pf_width, "largeur", 2),
	S(pf_hash, "flag #", 2), S(pf_space, "flag ' '", 2),
	S(pf_plus, "flag +", 2), S(pf_combo, "combinaisons", 2),
	S(pf_undef, "cas indéfinis", 2),
};

t_project	g_proj = {
	"ft_printf", "libftprintf.a", "ft_printf.h",
	"FT_PRINTF · SUJET v12.1 · LINUX",
	"> ton printf va parler",
	g_pf_suites,
	(int)(sizeof(g_pf_suites) / sizeof(g_pf_suites[0])),
	2,
	{"", "Partie obligatoire · cspdiuxX%", "Bonus · -0. largeur # +", ""},
	{"", "obligatoire", "bonus", ""},
};
