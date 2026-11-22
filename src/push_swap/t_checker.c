/* ************************************************************************** */
/*  push_swap bonus: the checker program (subject v1.2, chapter VIII).      */
/*  Its verdict is compared with the built-in simulator.                    */
/* ************************************************************************** */
#include "ps.h"

#define A(...) ((const char *const []){__VA_ARGS__, NULL})

static int	g_v[PS_MAX];

static void	verdict(const char *const *args, const char *ops, const char *exp)
{
	t_proc	p;

	ps_checker(args, ops, &p);
	if (strcmp(p.out, exp))
		zft_fail(__FILE__, __LINE__, "stdout", zft_str(exp), zft_str(p.out));
	ps_free(&p);
}

static void	ck_error(const char *const *args, const char *ops)
{
	t_proc	p;

	ps_checker(args, ops, &p);
	if (strcmp(p.err, "Error\n"))
		zft_fail(__FILE__, __LINE__, "stderr", "\"Error\\n\"", zft_str(p.err));
	CHECK(!strstr(p.out, "OK") && !strstr(p.out, "KO"), T("après une erreur, "
			"ni OK ni KO sur stdout (reçu %s)", "after an error, neither OK nor "
			"KO on stdout (got %s)"), zft_str(p.out));
	ps_free(&p);
}

/* -------------------------------------------------------------- verdict -- */
TEST(checker, "3 2 1 0 → OK")
{
	verdict(A("3", "2", "1", "0"), "rra\npb\nsa\nrra\npa\n", "OK\n");
}
TEST(checker, "3 2 1 0 → KO")
{
	verdict(A("3", "2", "1", "0"), "sa\nrra\npb\n", "KO\n");
}
TEST(checker, "1 2 3, rien → OK")	{ verdict(A("1", "2", "3"), "", "OK\n"); }
TEST(checker, "2 1, rien → KO")		{ verdict(A("2", "1"), "", "KO\n"); }
TEST(checker, "2 1, sa → OK")		{ verdict(A("2", "1"), "sa\n", "OK\n"); }
TEST(checker, "b non vide → KO")
{
	verdict(A("1", "2", "3"), "pb\n", "KO\n");
}
TEST(checker, "pb pa → OK")			{ verdict(A("1", "2", "3"), "pb\npa\n", "OK\n"); }
TEST(checker, "ops sur piles vides")
{
	verdict(A("1"), "sa\nsb\nss\npa\nra\nrb\nrr\nrra\nrrb\nrrr\n", "OK\n");
}
TEST(checker, "-1 -2 INT_MIN, sa rra")
{
	verdict(A("-1", "-2", "-2147483648"), "rra\nra\nsa\nrra\n", "OK\n");
}

/* random operation lists: the verdict must match the simulator        */
static void	random_ops(int n, int len, int set)
{
	static const char	*ops[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb",
		"rr", "rra", "rrb", "rrr"};
	static char			buf[PS_MAX * 64];
	size_t				l = 0;
	t_replay			r;
	int					k;

	while (set-- >= 0)
		ps_random(g_v, n);
	for (k = 0; k < len; k++)
		l += (size_t)snprintf(buf + l, sizeof(buf) - l, "%s\n",
			ops[zrand() % 11]);
	ps_replay(g_v, n, buf, l, &r);
	verdict(ps_args(NULL, NULL, g_v, n), buf, r.sorted ? "OK\n" : "KO\n");
}

TEST(checker, "11 opérations aléatoires #1")	{ random_ops(6, 30, 0); }
TEST(checker, "11 opérations aléatoires #2")	{ random_ops(8, 60, 1); }
TEST(checker, "11 opérations aléatoires #3")	{ random_ops(20, 200, 2); }

/* a valid sort written by the tester: selection sort with ra / pb / pa  */
static void	own_sort(int n, int drop_last)
{
	static char	buf[PS_MAX * PS_MAX * 4];
	static int	a[PS_MAX];
	size_t		l = 0;
	int			size = n;
	int			k;
	int			pushed = 0;

	ps_random(g_v, n);
	memcpy(a, g_v, sizeof(int) * (size_t)n);
	while (size > 0)
	{
		int	m = 0;

		for (k = 1; k < size; k++)
			if (a[k] < a[m])
				m = k;
		for (k = 0; k < m; k++)
			l += (size_t)snprintf(buf + l, sizeof(buf) - l, "ra\n");
		l += (size_t)snprintf(buf + l, sizeof(buf) - l, "pb\n");
		/* a rotated m times, then its top removed */
		{
			int	tmp[PS_MAX];
			int	j;

			for (j = 0; j < size; j++)
				tmp[j] = a[(m + j) % size];
			for (j = 1; j < size; j++)
				a[j - 1] = tmp[j];
		}
		size--;
		pushed++;
	}
	while (pushed-- > 0)
		l += (size_t)snprintf(buf + l, sizeof(buf) - l, "pa\n");
	if (drop_last)
		l -= 3;
	buf[l] = 0;
	verdict(ps_args(NULL, NULL, g_v, n), buf, drop_last ? "KO\n" : "OK\n");
}

TEST(checker, "100 nombres trié → OK")		{ own_sort(100, 0); }
TEST(checker, "100 nombres, pa en moins → KO") { own_sort(100, 1); }
TEST(checker, "500 nombres trié → OK")		{ own_sort(500, 0); }
TEST(checker, "avec ./push_swap 100")
{
	t_proc	p;

	ps_random(g_v, 100);
	ps_sorts_keep(NULL, NULL, g_v, 100, &p);
	verdict(ps_args(NULL, NULL, g_v, 100), p.out, "OK\n");
	ps_free(&p);
}
TEST(checker, "./checker")
{
	t_proc	p;

	INPUT("./checker");
	ps_exec("checker", NULL, NULL, 0, &p);
	CHECK(!p.sig, "crash (signal %d)", p.sig);
	CHECK(!p.outlen && !p.errlen, T("sans argument, rien ne doit s'afficher "
			"(stdout %s, stderr %s)", "without arguments nothing must be "
			"displayed (stdout %s, stderr %s)"), zft_str(p.out),
		zft_str(p.err));
	ps_free(&p);
}

/* --------------------------------------------------------------- errors -- */
TEST(ck_errors, "3 2 one 0")			{ ck_error(A("3", "2", "one", "0"), ""); }
TEST(ck_errors, "\"\" 1")				{ ck_error(A("", "1"), ""); }
TEST(ck_errors, "1 2 1")				{ ck_error(A("1", "2", "1"), ""); }
TEST(ck_errors, "2147483648")			{ ck_error(A("2147483648"), ""); }
TEST(ck_errors, "-2147483649")			{ ck_error(A("-2147483649"), ""); }
TEST(ck_errors, "1a 2")					{ ck_error(A("1a", "2"), ""); }
TEST(ck_errors, "instruction foo")		{ ck_error(A("2", "1"), "foo\n"); }
TEST(ck_errors, "instruction SA")		{ ck_error(A("2", "1"), "SA\n"); }
TEST(ck_errors, "instruction \" sa\"")	{ ck_error(A("2", "1"), " sa\n"); }
TEST(ck_errors, "instruction \"sa \"")	{ ck_error(A("2", "1"), "sa \n"); }
TEST(ck_errors, "instruction rrrr")		{ ck_error(A("2", "1"), "rrrr\n"); }
TEST(ck_errors, "sa pb xx pa")			{ ck_error(A("2", "1"), "sa\npb\nxx\npa\n"); }
SPEC_TEST(ck_errors, "ligne vide")		{ ck_error(A("2", "1"), "sa\n\n"); }
SPEC_TEST(ck_errors, "\"sa\" sans \\n")
{
	t_proc	p;

	ps_checker(A("2", "1"), "sa", &p);
	CHECK(!strcmp(p.out, "OK\n") || !strcmp(p.err, "Error\n"), T("« sa » sans "
			"\\n final : OK ou Error attendu (stdout %s, stderr %s)", "\"sa\" "
			"without a final \\n: OK or Error expected (stdout %s, stderr %s)"),
		zft_str(p.out), zft_str(p.err));
	ps_free(&p);
}
