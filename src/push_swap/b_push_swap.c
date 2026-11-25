/* ************************************************************************** */
/*  push_swap efficiency (--efficiency): not a time, a number of operations. */
/*  100 % = the "excellent" threshold of the subject (700 operations for 100 */
/*  numbers, 5500 for 500), compared with your average on random lists.     */
/* ************************************************************************** */
#include "ps.h"

static int	g_v[PS_MAX];

static void	score(int n, int sets, double excellent)
{
	double	sum = 0;
	int		k;

	for (k = 0; k < sets; k++)
	{
		t_proc		p;
		t_replay	r;

		ps_random(g_v, n);
		ps_exec("push_swap", ps_args(NULL, NULL, g_v, n), NULL, 0, &p);
		ps_replay(g_v, n, p.out, p.outlen, &r);
		if (ps_official(g_v, n, p.out, p.outlen) == 0)
			r.sorted = 0;
		ps_free(&p);
		if (p.sig || r.bad || !r.sorted)
			return ;		/* not measured: the tests report why */
		sum += r.count;
	}
	SCORE_SET(excellent, sum / sets > 1 ? sum / sets : 1);
}

SCORE(perf100)	{ score(100, 20, 700); }
SCORE(perf500)	{ score(500, 8, 5500); }
