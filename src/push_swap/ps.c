/* ************************************************************************** */
/*  push_swap helpers: process runner, operation replay, input generators.   */
/* ************************************************************************** */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include "ps.h"

/* ------------------------------------------------------------ process -- */
static void	grow(char **buf, size_t *len, size_t *cap, const char *src,
				size_t n)
{
	if (*len + n + 1 > *cap)
	{
		char	*nb;

		/* malloc + free only: the tester wraps them, not realloc */
		while (*len + n + 1 > *cap)
			*cap = *cap ? *cap * 2 : 4096;
		nb = malloc(*cap);
		if (!nb)
			abort();
		if (*buf)
			memcpy(nb, *buf, *len + 1);
		free(*buf);
		*buf = nb;
	}
	memcpy(*buf + *len, src, n);
	*len += n;
	(*buf)[*len] = 0;
}

void	ps_exec(const char *prog, const char *const *args, const char *in,
			size_t inlen, t_proc *p)
{
	char			path[4096];
	const char		*argv[PS_MAX + 8];
	int				pin[2], pout[2], perr[2];
	int				n = 0;
	pid_t			pid;
	size_t			cap_o = 0, cap_e = 0, sent = 0;
	struct pollfd	fds[3];
	char			buf[65536];
	int				st;

	memset(p, 0, sizeof(*p));
	grow(&p->out, &p->outlen, &cap_o, "", 0);
	grow(&p->err, &p->errlen, &cap_e, "", 0);
	snprintf(path, sizeof(path), "%s/%s", zft_path(), prog);
	argv[n++] = path;
	while (args && args[n - 1] && n < PS_MAX + 6)
	{
		argv[n] = args[n - 1];
		n++;
	}
	argv[n] = NULL;
	if (pipe2(pin, O_CLOEXEC) || pipe2(pout, O_CLOEXEC)
		|| pipe2(perr, O_CLOEXEC))
		zft_failf(__FILE__, __LINE__, "pipe: %s", strerror(errno));
	pid = fork();
	if (pid < 0)
		zft_failf(__FILE__, __LINE__, "fork: %s", strerror(errno));
	if (pid == 0)
	{
		/* killed with the test when it times out */
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		signal(SIGPIPE, SIG_DFL);
		signal(SIGALRM, SIG_DFL);
		alarm(0);
		dup2(pin[0], 0);
		dup2(pout[1], 1);
		dup2(perr[1], 2);
		execv(path, (char *const *)argv);
		_exit(127);
	}
	close(pin[0]);
	close(pout[1]);
	close(perr[1]);
	fcntl(pin[1], F_SETFL, O_NONBLOCK);
	if (!in || !inlen)
	{
		close(pin[1]);
		pin[1] = -1;
	}
	fds[0] = (struct pollfd){pout[0], POLLIN, 0};
	fds[1] = (struct pollfd){perr[0], POLLIN, 0};
	fds[2] = (struct pollfd){pin[1], POLLOUT, 0};
	while (fds[0].fd >= 0 || fds[1].fd >= 0)
	{
		int	k;

		if (poll(fds, 3, -1) < 0)
		{
			if (errno == EINTR)
				continue ;
			break ;
		}
		for (k = 0; k < 2; k++)
		{
			ssize_t	r;

			if (fds[k].fd < 0 || !fds[k].revents)
				continue ;
			r = read(fds[k].fd, buf, sizeof(buf));
			if (r > 0 && k == 0)
				grow(&p->out, &p->outlen, &cap_o, buf, (size_t)r);
			else if (r > 0)
				grow(&p->err, &p->errlen, &cap_e, buf, (size_t)r);
			else if (r == 0 || errno != EINTR)
			{
				close(fds[k].fd);
				fds[k].fd = -1;
			}
		}
		if (fds[2].fd >= 0 && fds[2].revents)
		{
			ssize_t	w = write(fds[2].fd, in + sent, inlen - sent);

			if (w > 0)
				sent += (size_t)w;
			if ((w < 0 && errno != EAGAIN && errno != EINTR) || sent >= inlen)
			{
				close(fds[2].fd);
				fds[2].fd = -1;
			}
		}
	}
	if (fds[2].fd >= 0)
		close(fds[2].fd);
	while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
		;
	p->status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
	p->sig = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
	if (p->status == 127 && !p->outlen && !p->errlen)
		zft_failf(__FILE__, __LINE__, T("impossible d'exécuter %s",
				"cannot execute %s"), path);
}

void	ps_free(t_proc *p)
{
	free(p->out);
	free(p->err);
	p->out = NULL;
	p->err = NULL;
}

/* ------------------------------------------------------------- replay -- */
typedef struct s_dq
{
	int	*v;
	int	cap;
	int	head;
	int	size;
}	t_dq;

static int	at(const t_dq *d, int i)
{
	return (d->v[(d->head + i) % d->cap]);
}

static void	push_top(t_dq *d, int x)
{
	d->head = (d->head - 1 + d->cap) % d->cap;
	d->v[d->head] = x;
	d->size++;
}

static int	pop_top(t_dq *d)
{
	int	x = d->v[d->head];

	d->head = (d->head + 1) % d->cap;
	d->size--;
	return (x);
}

static void	swap(t_dq *d)
{
	int	i = d->head;
	int	j = (d->head + 1) % d->cap;
	int	t;

	if (d->size < 2)
		return ;
	t = d->v[i];
	d->v[i] = d->v[j];
	d->v[j] = t;
}

static void	rot(t_dq *d)
{
	int	x;

	if (d->size < 2)
		return ;
	x = pop_top(d);
	d->v[(d->head + d->size) % d->cap] = x;
	d->size++;
}

static void	rrot(t_dq *d)
{
	int	x;

	if (d->size < 2)
		return ;
	x = d->v[(d->head + d->size - 1) % d->cap];
	d->size--;
	push_top(d, x);
}

static int	apply(t_dq *a, t_dq *b, const char *op, size_t len)
{
	static const char	*names[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb",
		"rr", "rra", "rrb", "rrr"};
	int					k;

	for (k = 0; k < 11; k++)
		if (strlen(names[k]) == len && !memcmp(names[k], op, len))
			break ;
	if (k == 11)
		return (0);
	if (k == 0 || k == 2)
		swap(a);
	if (k == 1 || k == 2)
		swap(b);
	if (k == 3 && b->size)
		push_top(a, pop_top(b));
	if (k == 4 && a->size)
		push_top(b, pop_top(a));
	if (k == 5 || k == 7)
		rot(a);
	if (k == 6 || k == 7)
		rot(b);
	if (k == 8 || k == 10)
		rrot(a);
	if (k == 9 || k == 10)
		rrot(b);
	return (1);
}

void	ps_replay(const int *v, int n, const char *ops, size_t len,
			t_replay *r)
{
	static int	va[PS_MAX + 1];
	static int	vb[PS_MAX + 1];
	t_dq		a = {va, n > 0 ? n : 1, 0, 0};
	t_dq		b = {vb, n > 0 ? n : 1, 0, 0};
	size_t		i = 0;
	int			k;

	memset(r, 0, sizeof(*r));
	for (k = 0; k < n; k++)
		va[k] = v[k];
	a.size = n;
	r->no_final_nl = len > 0 && ops[len - 1] != '\n';
	while (i < len)
	{
		size_t	e = i;

		while (e < len && ops[e] != '\n')
			e++;
		r->count++;
		if (!apply(&a, &b, ops + i, e - i))
		{
			size_t	l = e - i < sizeof(r->bad_text) - 1 ? e - i
				: sizeof(r->bad_text) - 1;

			r->bad = 1;
			r->bad_line = r->count;
			zft_esc(r->bad_text, sizeof(r->bad_text), ops + i, l);
			return ;
		}
		i = e + 1;
	}
	r->b_left = b.size;
	r->sorted = !b.size;
	for (k = 1; k < a.size && r->sorted; k++)
		if (at(&a, k - 1) > at(&a, k))
			r->sorted = 0;
}

/* ------------------------------------------------------------- inputs -- */
void	ps_sorted(int *v, int n)
{
	int	k;
	int	x = -(int)(zrand() % 50000);

	for (k = 0; k < n; k++)
	{
		x += 1 + (int)(zrand() % 997);
		v[k] = x;
	}
}

void	ps_random(int *v, int n)
{
	int	k;

	ps_sorted(v, n);
	for (k = n - 1; k > 0; k--)
	{
		int	j = (int)(zrand() % (uint32_t)(k + 1));
		int	t = v[k];

		v[k] = v[j];
		v[j] = t;
	}
}

double	ps_disorder_of(const int *v, int n)
{
	long	bad = 0;
	long	pairs = (long)n * (n - 1) / 2;
	int		i;
	int		j;

	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++)
			bad += v[i] > v[j];
	return (pairs ? (double)bad / (double)pairs : 0);
}

/* sorted, then random swaps until the disorder lands in [lo, hi) */
void	ps_disorder(int *v, int n, double lo, double hi)
{
	int		tries;
	double	d;

	for (tries = 0; tries < 200; tries++)
	{
		int	swaps = 0;

		ps_sorted(v, n);
		while ((d = ps_disorder_of(v, n)) < lo && swaps < n * 4)
		{
			int	i = (int)(zrand() % (uint32_t)n);
			int	j = (int)(zrand() % (uint32_t)n);
			int	t;

			if (i > j)
			{
				t = i;
				i = j;
				j = t;
			}
			/* swapping i < j only adds disorder when v[i] < v[j] */
			if (v[i] < v[j])
			{
				t = v[i];
				v[i] = v[j];
				v[j] = t;
			}
			swaps++;
		}
		if (d >= lo && d < hi)
			return ;
	}
}

const char	**ps_args(const char *flag1, const char *flag2, const int *v,
				int n)
{
	static char			arena[PS_MAX * 13];
	static const char	*argv[PS_MAX + 4];
	size_t				off = 0;
	int					c = 0;
	int					k;

	if (flag1)
		argv[c++] = flag1;
	if (flag2)
		argv[c++] = flag2;
	for (k = 0; k < n && k < PS_MAX; k++)
	{
		argv[c++] = arena + off;
		off += (size_t)snprintf(arena + off, sizeof(arena) - off, "%d", v[k])
			+ 1;
	}
	argv[c] = NULL;
	return (argv);
}

void	ps_input(const char *flag, const int *v, int n)
{
	char	b[900];
	size_t	l = 0;
	int		k;

	b[0] = 0;
	if (flag)
		l += (size_t)snprintf(b, sizeof(b), "%s ", flag);
	for (k = 0; k < n && l + 16 < sizeof(b) - 40; k++)
		l += (size_t)snprintf(b + l, sizeof(b) - l, "%d ", v[k]);
	if (k < n)
		snprintf(b + l, sizeof(b) - l, T("… (%d nombres)", "… (%d numbers)"),
			n);
	INPUT("./push_swap %s", b);
}

/* ------------------------------------------------------------ checks --- */
static void	expect_alive(const t_proc *p, const char *what)
{
	if (p->sig)
		zft_failf(__FILE__, __LINE__, T("%s a planté (signal %d : %s)",
				"%s crashed (signal %d: %s)"), what, p->sig,
			strsignal(p->sig));
}

int	ps_sorts_keep(const char *flag, const char *flag2, const int *v, int n,
		t_proc *p)
{
	t_replay	r;
	char		show[64];
	int			bench = (flag && !strcmp(flag, "--bench"))
		|| (flag2 && !strcmp(flag2, "--bench"));

	snprintf(show, sizeof(show), "%s%s%s", flag ? flag : "",
		flag && flag2 ? " " : "", flag2 ? flag2 : "");
	ps_input(flag || flag2 ? show : NULL, v, n);
	ps_exec("push_swap", ps_args(flag, flag2, v, n), NULL, 0, p);
	expect_alive(p, "push_swap");
	if (!bench && p->errlen)
		zft_failf(__FILE__, __LINE__, T("rien ne doit sortir sur stderr, reçu %s",
				"nothing expected on stderr, got %s"), zft_str(p->err));
	ps_replay(v, n, p->out, p->outlen, &r);
	if (r.bad)
		zft_failf(__FILE__, __LINE__, T("ligne %d : « %s » n'est pas une "
				"opération (sa sb ss pa pb ra rb rr rra rrb rrr, une par ligne)",
				"line %d: \"%s\" is not an operation (sa sb ss pa pb ra rb rr "
				"rra rrb rrr, one per line)"), r.bad_line, r.bad_text);
	if (r.no_final_nl)
		zft_failf(__FILE__, __LINE__, "%s", T("la dernière opération doit "
				"finir par \\n", "the last operation must end with \\n"));
	if (!r.sorted)
		zft_failf(__FILE__, __LINE__, T("pas trié après %d opérations%s",
				"not sorted after %d operations%s"), r.count, r.b_left
			? T(" (des nombres restent dans b)", " (numbers left in b)") : "");
	return (r.count);
}

int	ps_sorts(const char *flag, const char *flag2, const int *v, int n)
{
	t_proc	p;
	int		c = ps_sorts_keep(flag, flag2, v, n, &p);

	ps_free(&p);
	return (c);
}

void	ps_error(const char *const *args)
{
	t_proc	p;
	char	b[600];
	size_t	l = 0;
	int		k;

	for (k = 0; args[k] && l + 8 < sizeof(b); k++)
	{
		char	e[200];

		zft_esc(e, sizeof(e), args[k], strlen(args[k]));
		l += (size_t)snprintf(b + l, sizeof(b) - l, "\"%s\" ", e);
	}
	b[l] = 0;
	INPUT("./push_swap %s", b);
	ps_exec("push_swap", args, NULL, 0, &p);
	expect_alive(&p, "push_swap");
	if (p.outlen)
		zft_failf(__FILE__, __LINE__, T("rien ne doit sortir sur stdout, reçu %s",
				"nothing expected on stdout, got %s"), zft_str(p.out));
	if (strcmp(p.err, "Error\n"))
		zft_fail(__FILE__, __LINE__, "stderr", "\"Error\\n\"", zft_str(p.err));
	ps_free(&p);
}

void	ps_checker(const char *const *args, const char *ops, t_proc *p)
{
	char	b[700];
	char	e[300];
	size_t	l = 0;
	int		k;

	for (k = 0; args[k] && l + 16 < sizeof(b) - 320; k++)
		l += (size_t)snprintf(b + l, sizeof(b) - l, "%s ", args[k]);
	if (args[k])
		l += (size_t)snprintf(b + l, sizeof(b) - l, "… ");
	b[l] = 0;
	zft_esc(e, sizeof(e), ops ? ops : "", ops ? (strlen(ops) > 120 ? 120
			: strlen(ops)) : 0);
	INPUT("printf \"%s%s\" | ./checker %s", e, ops && strlen(ops) > 120
		? "…" : "", b);
	ps_exec("checker", args, ops, ops ? strlen(ops) : 0, p);
	expect_alive(p, "checker");
}
