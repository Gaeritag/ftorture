/* ************************************************************************** */
/*  push_swap, mandatory part (subject v1.2).                               */
/*  Every sort is replayed on a built-in simulator: the output must be only */
/*  operations, one per line, that leave a sorted and b empty.              */
/*  ▲ (speculative) = expected by the evaluation, not written in the subject */
/* ************************************************************************** */
#define _GNU_SOURCE
#include <math.h>
#include "ps.h"

#define A(...) ((const char *const []){__VA_ARGS__, NULL})

static int	g_v[PS_MAX];

/* ------------------------------------------------------------ no args -- */
TEST(args, "./push_swap")
{
	t_proc	p;

	INPUT("./push_swap");
	ps_exec("push_swap", NULL, NULL, 0, &p);
	CHECK(!p.sig, T("crash sans argument (signal %d)", "crash without "
			"arguments (signal %d)"), p.sig);
	CHECK(!p.outlen && !p.errlen, T("sans argument, rien ne doit s'afficher "
			"(stdout %s, stderr %s)", "without arguments nothing must be "
			"displayed (stdout %s, stderr %s)"), zft_str(p.out),
		zft_str(p.err));
	ps_free(&p);
}

TEST(args, "1 2147483647 -2147483648 0")
{
	static const int	v[] = {1, 2147483647, -2147483648, 0};

	ps_sorts(NULL, NULL, v, 4);
}

TEST(args, "-5 -1 -3 -2 -4")
{
	static const int	v[] = {-5, -1, -3, -2, -4};

	ps_sorts(NULL, NULL, v, 5);
}

SPEC_TEST(args, "\"3 2 1\"")
{
	t_proc		p;
	t_replay	r;
	static const int	v[] = {3, 2, 1};

	INPUT("./push_swap \"3 2 1\"");
	ps_exec("push_swap", A("3 2 1"), NULL, 0, &p);
	CHECK(!p.sig, "crash (signal %d)", p.sig);
	ps_replay(v, 3, p.out, p.outlen, &r);
	CHECK(r.sorted && !r.bad, T("une seule chaîne \"3 2 1\" n'est pas triée "
			"(stdout %s, stderr %s)", "a single string \"3 2 1\" is not sorted "
			"(stdout %s, stderr %s)"), zft_str(p.out), zft_str(p.err));
	ps_free(&p);
}

SPEC_TEST(args, "+3 2 007")
{
	t_proc		p;
	t_replay	r;
	static const int	v[] = {3, 2, 7};

	INPUT("./push_swap +3 2 007");
	ps_exec("push_swap", A("+3", "2", "007"), NULL, 0, &p);
	CHECK(!p.sig, "crash (signal %d)", p.sig);
	ps_replay(v, 3, p.out, p.outlen, &r);
	CHECK(r.sorted && !r.bad, T("+3 et 007 sont des entiers valides "
			"(stdout %s, stderr %s)", "+3 and 007 are valid integers "
			"(stdout %s, stderr %s)"), zft_str(p.out), zft_str(p.err));
	ps_free(&p);
}

/* ------------------------------------------------------------- errors -- */
TEST(errors, "1 a 3")				{ ps_error(A("1", "a", "3")); }
TEST(errors, "one")					{ ps_error(A("one")); }
TEST(errors, "1.5 2")				{ ps_error(A("1.5", "2")); }
TEST(errors, "12a 3")				{ ps_error(A("12a", "3")); }
TEST(errors, "+ 1")					{ ps_error(A("+", "1")); }
TEST(errors, "- 1")					{ ps_error(A("-", "1")); }
TEST(errors, "1- 2")				{ ps_error(A("1-", "2")); }
TEST(errors, "--5 1")				{ ps_error(A("--5", "1")); }
TEST(errors, "2147483648")			{ ps_error(A("2147483648")); }
TEST(errors, "-2147483649 0")		{ ps_error(A("-2147483649", "0")); }
TEST(errors, "4294967296 1")		{ ps_error(A("4294967296", "1")); }
TEST(errors, "99999999999999999999")
{
	ps_error(A("99999999999999999999"));
}
TEST(errors, "1 2 1")				{ ps_error(A("1", "2", "1")); }
TEST(errors, "3 2 3 (--simple)")	{ ps_error(A("--simple", "3", "2", "3")); }
TEST(errors, "0 one 2 (--adaptive)")
{
	ps_error(A("--adaptive", "0", "one", "2", "3"));
}
TEST(errors, "5 4 3 2 1 5")			{ ps_error(A("5", "4", "3", "2", "1", "5")); }
TEST(errors, "-2147483648 -2147483648")
{
	ps_error(A("-2147483648", "-2147483648"));
}
SPEC_TEST(errors, "\"\" 1")			{ ps_error(A("", "1")); }
SPEC_TEST(errors, "\" \"")			{ ps_error(A(" ")); }
SPEC_TEST(errors, "+1 1")			{ ps_error(A("+1", "1")); }
SPEC_TEST(errors, "0 -0")			{ ps_error(A("0", "-0")); }
SPEC_TEST(errors, "--foo 1 2")		{ ps_error(A("--foo", "1", "2")); }
SPEC_TEST(errors, "--simple --complex 2 1")
{
	ps_error(A("--simple", "--complex", "2", "1"));
}

/* ------------------------------------------------------- already sorted -- */
static void	sorted_nothing(const char *flag, int n)
{
	int	c;

	ps_sorted(g_v, n);
	c = ps_sorts(flag, NULL, g_v, n);
	CHECK(c == 0, T("déjà trié : aucune opération attendue, %d affichée(s)",
			"already sorted: no operation expected, %d printed"), c);
}

TEST(sorted, "42")					{ sorted_nothing(NULL, 1); }
TEST(sorted, "2 nombres")			{ sorted_nothing(NULL, 2); }
TEST(sorted, "3 nombres")			{ sorted_nothing(NULL, 3); }
TEST(sorted, "10 nombres")			{ sorted_nothing(NULL, 10); }
TEST(sorted, "500 nombres")			{ sorted_nothing(NULL, 500); }
TEST(sorted, "--simple 100")		{ sorted_nothing("--simple", 100); }
TEST(sorted, "--medium 100")		{ sorted_nothing("--medium", 100); }
TEST(sorted, "--complex 100")		{ sorted_nothing("--complex", 100); }
TEST(sorted, "--adaptive 100")		{ sorted_nothing("--adaptive", 100); }

/* ---------------------------------------------------------- 2 to 5 ----- */
/* every permutation of n numbers; returns the largest operation count   */
static int	all_perms(int n)
{
	int	idx[8];
	int	c[8] = {0};
	int	best = 0;
	int	i = 0;
	int	k;

	int	worst[8];

	for (k = 0; k < n; k++)
		idx[k] = k * 7 - 3;
	best = ps_sorts(NULL, NULL, idx, n);
	memcpy(worst, idx, sizeof(worst));
	while (i < n)
	{
		if (c[i] < i)
		{
			int	j = i % 2 ? c[i] : 0;
			int	t = idx[j];
			int	ops;

			idx[j] = idx[i];
			idx[i] = t;
			ops = ps_sorts(NULL, NULL, idx, n);
			if (ops > best)
			{
				best = ops;
				memcpy(worst, idx, sizeof(worst));
			}
			c[i]++;
			i = 0;
		}
		else
			c[i++] = 0;
	}
	ps_input(NULL, worst, n);
	return (best);
}

SPEC_TEST(small, "2 1")
{
	static const int	v[] = {2, 1};
	int					c = ps_sorts(NULL, NULL, v, 2);

	CHECK(c == 1, T("2 1 se trie en 1 opération (sa), %d affichée(s)",
			"2 1 is sorted in 1 operation (sa), %d printed"), c);
}

TEST(small, "3 nombres × 6")		{ all_perms(3); }
SPEC_TEST(small, "3 nombres ≤ 3 coups")
{
	int	c = all_perms(3);

	CHECK(c <= 3, T("au pire %d coups pour 3 nombres (la correction attend "
			"3 au maximum)", "worst case %d moves for 3 numbers (the evaluation "
			"expects 3 at most)"), c);
}
TEST(small, "4 nombres × 24")		{ all_perms(4); }
TEST(small, "5 nombres × 120")		{ all_perms(5); }
SPEC_TEST(small, "5 nombres ≤ 12 coups")
{
	int	c = all_perms(5);

	CHECK(c <= 12, T("au pire %d coups pour 5 nombres (la correction attend "
			"12 au maximum)", "worst case %d moves for 5 numbers (the evaluation "
			"expects 12 at most)"), c);
}

/* --------------------------------------------------------- strategies -- */
static void	random_sort(const char *flag, int n)
{
	ps_random(g_v, n);
	ps_sorts(flag, NULL, g_v, n);
}

static void	reverse_sort(const char *flag, int n)
{
	int	k;

	for (k = 0; k < n; k++)
		g_v[k] = n - k;
	ps_sorts(flag, NULL, g_v, n);
}

static void	extremes_sort(const char *flag)
{
	static const int	v[] = {0, 2147483647, -2147483648, -1, 1,
		2147483646, -2147483647};

	ps_sorts(flag, NULL, v, 7);
}

/* average operation count of `sets` random inputs of n numbers          */
static double	avg_ops(const char *flag, int n, int sets)
{
	double	sum = 0;
	int		k;

	for (k = 0; k < sets; k++)
	{
		ps_random(g_v, n);
		sum += ps_sorts(flag, NULL, g_v, n);
	}
	return (sum / sets);
}

/* growth exponent between n = 100 and n = 400: ops ~ n^e. n² gives about 2,
   n√n 1.5, n log n about 1.2                                              */
static void	growth(const char *flag, double max, const char *cls)
{
	double	a = avg_ops(flag, 100, 2);
	double	b = avg_ops(flag, 400, 2);
	double	e = a > 0 && b > 0 ? log(b / a) / log(4.0) : 0;

	INPUT("%s : %s %.0f, %s %.0f", flag,
		T("100 nombres", "100 numbers"), a, T("400 nombres", "400 numbers"), b);
	CHECK(e <= max, T("les opérations croissent comme n^%.2f, trop pour %s "
			"(attendu ≤ n^%.2f)", "operations grow like n^%.2f, too fast for %s "
			"(expected ≤ n^%.2f)"), e, cls, max);
}

/* the four strategies must be four different algorithms                */
static void	distinct(const char *flag)
{
	static const char	*all[] = {"--simple", "--medium", "--complex",
		"--adaptive"};
	t_proc				mine;
	int					k;

	ps_random(g_v, 100);
	ps_sorts_keep(flag, NULL, g_v, 100, &mine);
	for (k = 0; k < 4; k++)
	{
		t_proc	other;

		if (!strcmp(all[k], flag) || !strcmp(all[k], "--adaptive")
			|| !strcmp(flag, "--adaptive"))
			continue ;
		ps_sorts_keep(all[k], NULL, g_v, 100, &other);
		CHECK(other.outlen != mine.outlen || memcmp(other.out, mine.out,
				mine.outlen), T("%s et %s donnent exactement les mêmes "
				"opérations : il faut 4 stratégies distinctes", "%s and %s give "
				"exactly the same operations: 4 distinct strategies are "
				"required"), flag, all[k]);
		ps_free(&other);
	}
	ps_free(&mine);
}

#define STRATEGY(F, FLAG, MAXE, CLS) \
	TEST(F, "2 1")					{ static const int v[] = {2, 1}; \
										ps_sorts(FLAG, NULL, v, 2); } \
	TEST(F, "3 nombres")			{ random_sort(FLAG, 3); } \
	TEST(F, "5 nombres")			{ random_sort(FLAG, 5); } \
	TEST(F, "10 nombres")			{ random_sort(FLAG, 10); } \
	TEST(F, "INT_MIN INT_MAX")		{ extremes_sort(FLAG); } \
	TEST(F, "50 aléatoires")		{ random_sort(FLAG, 50); } \
	TEST(F, "100 aléatoires")		{ random_sort(FLAG, 100); } \
	TEST(F, "100 inversé")			{ reverse_sort(FLAG, 100); } \
	TEST(F, "500 aléatoires")		{ random_sort(FLAG, 500); } \
	TEST(F, "500 inversé")			{ reverse_sort(FLAG, 500); }

STRATEGY(simple, "--simple", 2.3, "O(n²)")
SPEC_TEST(simple, "différent de --medium/--complex") { distinct("--simple"); }

STRATEGY(medium, "--medium", 1.75, "O(n√n)")
SPEC_TEST(medium, "croissance O(n√n)")	{ growth("--medium", 1.75, "O(n√n)"); }
SPEC_TEST(medium, "différent de --simple/--complex") { distinct("--medium"); }

STRATEGY(complex, "--complex", 1.4, "O(n log n)")
SPEC_TEST(complex, "croissance O(n log n)")
{
	growth("--complex", 1.4, "O(n log n)");
}
SPEC_TEST(complex, "différent de --simple/--medium") { distinct("--complex"); }

STRATEGY(adaptive, "--adaptive", 1.4, "O(n log n)")

/* adaptive: one input per disorder regime of the subject                */
static void	regime(int n, double lo, double hi)
{
	double	d;

	ps_disorder(g_v, n, lo, hi);
	d = ps_disorder_of(g_v, n);
	ps_sorts("--adaptive", NULL, g_v, n);
	CHECK(d >= lo && d < hi, "%s", T("entrée non générée", "input not "
			"generated"));
}

TEST(adaptive, "désordre < 0.2")		{ regime(500, 0.02, 0.2); }
TEST(adaptive, "désordre 0.2–0.5")	{ regime(500, 0.25, 0.45); }
TEST(adaptive, "désordre ≥ 0.5")		{ regime(500, 0.5, 1.01); }
TEST(adaptive, "sans flag = --adaptive")
{
	t_proc	a;
	t_proc	b;

	ps_random(g_v, 100);
	ps_sorts_keep(NULL, NULL, g_v, 100, &a);
	ps_sorts_keep("--adaptive", NULL, g_v, 100, &b);
	CHECK(a.outlen == b.outlen && !memcmp(a.out, b.out, a.outlen), "%s",
		T("sans sélecteur, la sortie doit être celle de --adaptive (comportement "
			"par défaut)", "without a selector the output must be the one of "
			"--adaptive (default behaviour)"));
	ps_free(&a);
	ps_free(&b);
}
SPEC_TEST(adaptive, "croissance désordre fort")
{
	growth("--adaptive", 1.4, "O(n log n)");
}

/* -------------------------------------------------------------- bench -- */
static int	has_word(const char *s, const char *w)
{
	size_t	l = strlen(w);
	const char *p = s;

	while ((p = strcasestr(p, w)))
	{
		int	before = p == s || !(isalnum((unsigned char)p[-1]));
		int	after = !isalnum((unsigned char)p[l]);

		if (before && after)
			return (1);
		p += l;
	}
	return (0);
}

static void	bench_run(const char *flag, t_proc *p, int *count)
{
	ps_random(g_v, 100);
	*count = ps_sorts_keep("--bench", flag, g_v, 100, p);
	CHECK(p->errlen, "%s", T("--bench n'affiche rien sur stderr",
			"--bench prints nothing on stderr"));
}

TEST(bench, "stdout = opérations")
{
	t_proc	p;
	t_proc	q;
	int		c;

	bench_run("--complex", &p, &c);
	ps_sorts_keep("--complex", NULL, g_v, 100, &q);
	CHECK(p.outlen == q.outlen && !memcmp(p.out, q.out, p.outlen), "%s",
		T("avec --bench, stdout doit rester la même liste d'opérations (les "
			"métriques vont sur stderr)", "with --bench, stdout must stay the "
			"same operation list (metrics go to stderr)"));
	ps_free(&p);
	ps_free(&q);
}

TEST(bench, "désordre en %")
{
	t_proc	p;
	int		c;
	char	r1[32];
	char	r2[32];
	double	d;

	bench_run("--adaptive", &p, &c);
	d = ps_disorder_of(g_v, 100) * 100.0;
	snprintf(r1, sizeof(r1), "%.2f", d);
	snprintf(r2, sizeof(r2), "%.2f", floor(d * 100.0) / 100.0);
	CHECK(strstr(p.err, r1) || strstr(p.err, r2), T("le désordre %s%% "
			"(2 décimales) n'apparaît pas dans %s", "the disorder %s%% (2 "
			"decimals) does not appear in %s"), r1, zft_str(p.err));
	ps_free(&p);
}

TEST(bench, "total des opérations")
{
	t_proc	p;
	int		c;
	char	n[16];

	bench_run("--medium", &p, &c);
	snprintf(n, sizeof(n), "%d", c);
	CHECK(has_word(p.err, n), T("le total (%d opérations) n'apparaît pas dans "
			"%s", "the total (%d operations) does not appear in %s"), c,
		zft_str(p.err));
	ps_free(&p);
}

TEST(bench, "compte de chaque opération")
{
	static const char	*ops[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb",
		"rr", "rra", "rrb", "rrr"};
	t_proc				p;
	int					c;
	int					k;

	bench_run("--complex", &p, &c);
	for (k = 0; k < 11; k++)
		CHECK(has_word(p.err, ops[k]), T("le compte de %s manque dans %s",
				"the count of %s is missing from %s"), ops[k], zft_str(p.err));
	ps_free(&p);
}

static void	bench_names(const char *flag, const char *name)
{
	t_proc	p;
	int		c;

	bench_run(flag, &p, &c);
	CHECK(strcasestr(p.err, name), T("le nom de la stratégie (%s) manque dans "
			"%s", "the strategy name (%s) is missing from %s"), name,
		zft_str(p.err));
	CHECK(strstr(p.err, "O("), T("la classe de complexité O(...) manque dans "
			"%s", "the complexity class O(...) is missing from %s"),
		zft_str(p.err));
	ps_free(&p);
}

TEST(bench, "nom --simple")			{ bench_names("--simple", "simple"); }
TEST(bench, "nom --medium")			{ bench_names("--medium", "medium"); }
TEST(bench, "nom --complex")		{ bench_names("--complex", "complex"); }
TEST(bench, "nom --adaptive")		{ bench_names("--adaptive", "adaptive"); }
SPEC_TEST(bench, "--simple --bench")
{
	t_proc	p;

	ps_random(g_v, 50);
	ps_sorts_keep("--simple", "--bench", g_v, 50, &p);
	CHECK(p.errlen, "%s", T("--bench après le sélecteur n'affiche rien",
			"--bench after the selector prints nothing"));
	ps_free(&p);
}
SPEC_TEST(bench, "trié + --bench")
{
	t_proc	p;

	ps_sorted(g_v, 20);
	CHECK(ps_sorts_keep("--bench", NULL, g_v, 20, &p) == 0, "%s",
		T("déjà trié : aucune opération attendue", "already sorted: no "
			"operation expected"));
	CHECK(p.errlen, "%s", T("--bench doit afficher ses métriques même si "
			"l'entrée est déjà triée", "--bench must print its metrics even when "
			"the input is already sorted"));
	ps_free(&p);
}

/* -------------------------------------------------------- performance -- */
/* thresholds of the subject: pass, good, excellent                      */
static void	perf_one(int n, int limit, int set)
{
	int	c;

	/* every test starts from the same seed: skip to its own list */
	while (set-- >= 0)
		ps_random(g_v, n);
	c = ps_sorts(NULL, NULL, g_v, n);
	CHECK(c < limit, T("%d opérations pour %d nombres (minimum pour valider : "
			"moins de %d)", "%d operations for %d numbers (minimum to pass: "
			"fewer than %d)"), c, n, limit);
}

static void	perf_tier(int n, int sets, int limit, const char *tier)
{
	int	worst = 0;
	int	k;

	for (k = 0; k < sets; k++)
	{
		int	c;

		ps_random(g_v, n);
		c = ps_sorts(NULL, NULL, g_v, n);
		if (c > worst)
			worst = c;
	}
	INPUT(T("%d listes de %d nombres aléatoires", "%d lists of %d random "
			"numbers"), sets, n);
	CHECK(worst < limit, T("au pire %d opérations : pas encore « %s » (moins "
			"de %d)", "worst case %d operations: not \"%s\" yet (fewer than %d)"),
		worst, tier, limit);
}

TEST(perf100, "100 aléatoires #1")	{ perf_one(100, 2000, 0); }
TEST(perf100, "100 aléatoires #2")	{ perf_one(100, 2000, 1); }
TEST(perf100, "100 aléatoires #3")	{ perf_one(100, 2000, 2); }
TEST(perf100, "100 aléatoires #4")	{ perf_one(100, 2000, 3); }
TEST(perf100, "100 aléatoires #5")	{ perf_one(100, 2000, 4); }
SPEC_TEST(perf100, "< 1500 (bien)")
{
	perf_tier(100, 10, 1500, T("bien", "good"));
}
SPEC_TEST(perf100, "< 700 (excellent)")
{
	perf_tier(100, 10, 700, "excellent");
}

TEST(perf500, "500 aléatoires #1")	{ perf_one(500, 12000, 0); }
TEST(perf500, "500 aléatoires #2")	{ perf_one(500, 12000, 1); }
TEST(perf500, "500 aléatoires #3")	{ perf_one(500, 12000, 2); }
TEST(perf500, "500 aléatoires #4")	{ perf_one(500, 12000, 3); }
TEST(perf500, "500 aléatoires #5")	{ perf_one(500, 12000, 4); }
SPEC_TEST(perf500, "< 8000 (bien)")
{
	perf_tier(500, 5, 8000, T("bien", "good"));
}
SPEC_TEST(perf500, "< 5500 (excellent)")
{
	perf_tier(500, 5, 5500, "excellent");
}
