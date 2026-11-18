/* ************************************************************************** */
/*  Test engine: one fork per run, crash comparison with the real function,  */
/*  malloc-failure injection, terminal UI and report.                         */
/* ************************************************************************** */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include "zft.h"

/* memtrack.c */
extern int		g_zm_active;
extern int		g_armed_count;
extern int		g_zm_bad_free;
extern char		g_zm_bad_msg[160];
void			zm_reset(void);
size_t			zm_live_bytes(void);

int	g_ref;

/* ============================================================ options === */
static struct s_opt
{
	int			spec;		/* speculative failures count as failures */
	int			verbose;
	int			color;
	int			part;
	const char	*only;
	const char	*checks;
	const char	*log;
	const char	*path;
	unsigned	seed;
	int			timeout;
	int			max_details;
	int			no_banner;
}	o = {0, 0, 1, 0, NULL, NULL, NULL, ".", 42, 3, 3, 0};

static FILE	*g_log;

/* ============================================================== colors === */
#define C_RST  (o.color ? "\033[0m" : "")
#define C_B    (o.color ? "\033[1m" : "")
#define C_DIM  (o.color ? "\033[2m" : "")
#define C_IT   (o.color ? "\033[3m" : "")
#define C_RED  (o.color ? "\033[38;5;203m" : "")
#define C_GRN  (o.color ? "\033[38;5;82m" : "")
/* retro phosphor palette */
#define C_PH   (o.color ? "\033[38;5;46m" : "")
#define C_PHM  (o.color ? "\033[38;5;34m" : "")
#define C_PHD  (o.color ? "\033[38;5;22m" : "")
#define C_INV  (o.color ? "\033[48;5;46m\033[38;5;16m\033[1m" : "")
#define C_INVR (o.color ? "\033[48;5;203m\033[38;5;16m\033[1m" : "")
#define C_YEL  (o.color ? "\033[38;5;221m" : "")
#define C_BLU  (o.color ? "\033[38;5;75m" : "")
#define C_CYN  (o.color ? "\033[38;5;80m" : "")
#define C_MAG  (o.color ? "\033[38;5;177m" : "")
#define C_GRY  (o.color ? "\033[38;5;244m" : "")
#define C_BGR  (o.color ? "\033[48;5;203m\033[38;5;16m\033[1m" : "")
#define C_BGG  (o.color ? "\033[48;5;114m\033[38;5;16m\033[1m" : "")
#define C_BGY  (o.color ? "\033[48;5;221m\033[38;5;16m\033[1m" : "")

/* ========================================================== registry ==== */
typedef struct s_func
{
	const char	*name;
	int			part;
	void		*addr;
}	t_func;

#define F(n, p) { #n, p, (void *)(n) }
static t_func	g_funcs[] = {
	F(ft_isalpha, 1), F(ft_isdigit, 1), F(ft_isalnum, 1), F(ft_isascii, 1),
	F(ft_isprint, 1), F(ft_strlen, 1), F(ft_memset, 1), F(ft_bzero, 1),
	F(ft_memcpy, 1), F(ft_memmove, 1), F(ft_strlcpy, 1), F(ft_strlcat, 1),
	F(ft_toupper, 1), F(ft_tolower, 1), F(ft_strchr, 1), F(ft_strrchr, 1),
	F(ft_strncmp, 1), F(ft_memchr, 1), F(ft_memcmp, 1), F(ft_strnstr, 1),
	F(ft_atoi, 1), F(ft_calloc, 1), F(ft_strdup, 1),
	F(ft_substr, 2), F(ft_strjoin, 2), F(ft_strtrim, 2), F(ft_split, 2),
	F(ft_itoa, 2), F(ft_strmapi, 2), F(ft_striteri, 2), F(ft_putchar_fd, 2),
	F(ft_putstr_fd, 2), F(ft_putendl_fd, 2), F(ft_putnbr_fd, 2),
	F(ft_lstnew, 3), F(ft_lstadd_front, 3), F(ft_lstsize, 3),
	F(ft_lstlast, 3), F(ft_lstadd_back, 3), F(ft_lstdelone, 3),
	F(ft_lstclear, 3), F(ft_lstiter, 3), F(ft_lstmap, 3),
};
#define NFUNCS ((int)(sizeof(g_funcs) / sizeof(g_funcs[0])))
static const char	*g_part_names[] = {"", "Fonctions de la libc",
	"Fonctions supplémentaires", "Listes chaînées"};

static t_test	*g_tests;
static int		g_ntests;
static int		g_cap;

void	zft_register(const char *func, const char *name, t_kind kind, int spec,
			void (*fn)(void), const char *file, int line)
{
	if (g_ntests == g_cap)
	{
		g_cap = g_cap ? g_cap * 2 : 256;
		g_tests = realloc(g_tests, sizeof(t_test) * (size_t)g_cap);
	}
	g_tests[g_ntests] = (t_test){func, name, kind, spec, fn, file, line,
		g_ntests};
	g_ntests++;
}

static int	func_index(const char *name)
{
	int	i;

	for (i = 0; i < NFUNCS; i++)
		if (!strcmp(g_funcs[i].name, name))
			return (i);
	return (NFUNCS);
}

static int	cmp_tests(const void *a, const void *b)
{
	const t_test	*x = a;
	const t_test	*y = b;
	int				fx = func_index(x->func);
	int				fy = func_index(y->func);
	int				c;

	if (fx != fy)
		return (fx - fy);
	c = strcmp(x->file, y->file);
	if (c)
		return (c);
	if (x->line != y->line)
		return (x->line - y->line);
	return (x->seq - y->seq);
}

/* =================================================== shared child state = */
typedef struct s_shm
{
	int			done;
	int			failed;
	int			fline;
	char		ffile[64];
	char		msg[600];
	char		exp[2048];
	char		got[2048];
	char		input[1024];
	char		out[4096];
	size_t		outlen;
	int			leaks;
	size_t		leak_bytes;
	int			armed_count;
}	t_shm;

static t_shm	*g_shm;

static void	copy_trunc(char *dst, size_t dsz, const char *src)
{
	size_t	n;

	if (!src)
	{
		dst[0] = 0;
		return ;
	}
	n = strlen(src);
	if (n >= dsz)
		n = dsz - 1;
	memcpy(dst, src, n);
	dst[n] = 0;
}

static const char	*base_name(const char *p)
{
	const char	*s = strrchr(p, '/');

	return (s ? s + 1 : p);
}

void	zft_fail(const char *file, int line, const char *expr,
			const char *expected, const char *got)
{
	g_armed = 0;
	g_shm->failed = 1;
	g_shm->fline = line;
	copy_trunc(g_shm->ffile, sizeof(g_shm->ffile), base_name(file));
	copy_trunc(g_shm->msg, sizeof(g_shm->msg), expr);
	copy_trunc(g_shm->exp, sizeof(g_shm->exp), expected);
	copy_trunc(g_shm->got, sizeof(g_shm->got), got);
	g_shm->armed_count = g_armed_count;
	g_shm->done = 1;
	_exit(1);
}

void	zft_failf(const char *file, int line, const char *fmt, ...)
{
	char	buf[600];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	zft_fail(file, line, buf, NULL, NULL);
}

void	zft_out(const char *fmt, ...)
{
	va_list	ap;
	size_t	room;
	int		n;

	if (g_shm->outlen >= sizeof(g_shm->out) - 4)
		return ;
	if (g_shm->outlen)
	{
		memcpy(g_shm->out + g_shm->outlen, " | ", 3);
		g_shm->outlen += 3;
	}
	room = sizeof(g_shm->out) - g_shm->outlen;
	va_start(ap, fmt);
	n = vsnprintf(g_shm->out + g_shm->outlen, room, fmt, ap);
	va_end(ap);
	if (n < 0)
		return ;
	g_shm->outlen += (size_t)n < room ? (size_t)n : room - 1;
}

void	zft_input(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(g_shm->input, sizeof(g_shm->input), fmt, ap);
	va_end(ap);
}

void	zft_esc(char *d, size_t dsz, const char *s, size_t n)
{
	size_t	j = 0;
	size_t	i;

	for (i = 0; i < n && j + 8 < dsz; i++)
	{
		unsigned char	c = (unsigned char)s[i];

		if (c == '\n')
			j += (size_t)snprintf(d + j, dsz - j, "\\n");
		else if (c == '\t')
			j += (size_t)snprintf(d + j, dsz - j, "\\t");
		else if (c == '"' || c == '\\')
			j += (size_t)snprintf(d + j, dsz - j, "\\%c", c);
		else if (c < 32 || c >= 127)
			j += (size_t)snprintf(d + j, dsz - j, "\\x%02x", c);
		else
			d[j++] = (char)c;
	}
	if (i < n)
		j += (size_t)snprintf(d + j, dsz - j, "…");
	d[j] = 0;
}

static char	*ring(void)
{
	static char	r[8][1100];
	static int	i;

	return (r[i++ & 7]);
}

const char	*zft_str(const char *s)
{
	char	*b;
	size_t	n;

	if (!s)
		return ("NULL");
	b = ring();
	n = strlen(s);
	b[0] = '"';
	zft_esc(b + 1, 1090, s, n > 300 ? 300 : n);
	if (n > 300)
		strcat(b, "…");
	strcat(b, "\"");
	return (b);
}

const char	*zft_mem(const void *p, size_t n)
{
	char		*b = ring();
	size_t		i;
	size_t		j = 0;
	const unsigned char	*c = p;

	for (i = 0; i < n && i < 64; i++)
		j += (size_t)snprintf(b + j, 1100 - j, "%02x ", c[i]);
	if (i < n)
		snprintf(b + j, 1100 - j, "… (%zu octets)", n);
	return (b);
}

/* ================================================================ runs === */
typedef struct s_run
{
	int		sig;
	int		exited;
	t_shm	s;
}	t_run;

static void	run_child(const t_test *t, int ref, int fail_at, t_run *r)
{
	pid_t			pid;
	int				st;
	struct rlimit	rl = {0, 0};

	memset(g_shm, 0, sizeof(*g_shm));
	fflush(stdout);
	if (g_log)
		fflush(g_log);
	pid = fork();
	if (pid < 0)
	{
		perror("fork");
		exit(2);
	}
	if (pid == 0)
	{
		int	dn = open("/dev/null", O_RDWR);

		setrlimit(RLIMIT_CORE, &rl);
		dup2(dn, 0);
		dup2(dn, 1);
		dup2(dn, 2);
		signal(SIGPIPE, SIG_IGN);
		g_ref = ref;
		g_fail_at = fail_at;
		g_armed = 0;
		zm_reset();
		zsrand(o.seed);
		g_zm_active = 1;
		alarm((unsigned)o.timeout);
		t->fn();
		g_armed = 0;
		if (!ref && g_zm_bad_free)
			zft_failf(t->file, t->line, "%s", g_zm_bad_msg);
		if (!ref && zm_live_blocks())
		{
			g_shm->leaks = zm_live_blocks();
			g_shm->leak_bytes = zm_live_bytes();
			zft_failf(t->file, t->line,
				"fuite mémoire : %d bloc(s), %zu octet(s) jamais libéré(s)",
				g_shm->leaks, g_shm->leak_bytes);
		}
		g_shm->armed_count = g_armed_count;
		g_shm->done = 1;
		_exit(0);
	}
	while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
		;
	r->sig = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
	r->exited = WIFEXITED(st);
	memcpy(&r->s, g_shm, sizeof(t_shm));
}

static const char	*sig_name(int sig)
{
	static char	b[64];

	switch (sig)
	{
		case SIGSEGV: return ("SIGSEGV · segmentation fault");
		case SIGBUS: return ("SIGBUS · bus error");
		case SIGABRT: return ("SIGABRT · abort (double free / corruption ?)");
		case SIGFPE: return ("SIGFPE · erreur arithmétique");
		case SIGALRM: return ("TIMEOUT · boucle infinie ?");
		case SIGILL: return ("SIGILL · instruction illégale");
	}
	snprintf(b, sizeof(b), "signal %d", sig);
	return (b);
}

/* ============================================================= results === */
typedef enum e_res
{
	R_PASS,
	R_PASS_CRASH,	/* crashed exactly like the real function: fine */
	R_FAIL,
	R_SPEC,
	R_SKIP
}	t_res;

typedef struct s_result
{
	t_res	res;
	char	glyph[8];
	char	*detail;	/* malloc'ed text block, may be NULL */
}	t_result;

/* small growable string */
typedef struct s_sb
{
	char	*s;
	size_t	n;
	size_t	cap;
}	t_sb;

__attribute__((format(printf, 2, 3)))
static void	sb_add(t_sb *b, const char *fmt, ...)
{
	va_list	ap;
	int		n;

	va_start(ap, fmt);
	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (b->n + (size_t)n + 1 > b->cap)
	{
		b->cap = (b->n + (size_t)n + 1) * 2;
		b->s = realloc(b->s, b->cap);
	}
	va_start(ap, fmt);
	vsnprintf(b->s + b->n, (size_t)n + 1, fmt, ap);
	va_end(ap);
	b->n += (size_t)n;
}

static void	detail_kv(t_sb *b, const char *k, const char *v, const char *col)
{
	if (v && *v)
		sb_add(b, "        %s%-9s%s %s%s%s\n", C_GRY, k, C_RST, col, v, C_RST);
}

static void	detail_shm(t_sb *b, const t_shm *s, int with_values)
{
	detail_kv(b, "entrée", s->input, "");
	if (s->failed)
	{
		if (s->exp[0] || s->got[0])
		{
			detail_kv(b, "expr", s->msg, C_DIM);
			detail_kv(b, "attendu", s->exp, C_GRN);
			detail_kv(b, "obtenu", s->got, C_RED);
		}
		else
			detail_kv(b, "erreur", s->msg, "");
	}
	(void)with_values;
}

static char	*make_detail(const t_test *t, int idx, t_res res, const char *head,
			const t_shm *s, const char *exp, const char *got)
{
	t_sb		b = {0};
	const char	*col = res == R_SPEC ? C_YEL : C_RED;
	const char	*mark = res == R_SPEC ? "▲" : "✘";

	sb_add(&b, "      %s%s #%d%s %s« %s »%s  %s%s:%d%s\n", col, mark, idx, C_RST,
		C_B, t->name, C_RST, C_GRY, base_name(t->file), t->line, C_RST);
	if (head && *head)
		sb_add(&b, "        %s%s%s\n", col, head, C_RST);
	if (s)
		detail_shm(&b, s, 1);
	detail_kv(&b, "attendu", exp, C_GRN);
	detail_kv(&b, "obtenu", got, C_RED);
	return (b.s);
}

static void	set_glyph(t_result *r, const char *g)
{
	snprintf(r->glyph, sizeof(r->glyph), "%s", g);
}

static void	eval_normal(const t_test *t, int idx, t_result *r)
{
	t_run	a;
	t_res	bad = t->spec ? R_SPEC : R_FAIL;

	run_child(t, 0, 0, &a);
	if (a.sig)
	{
		r->res = bad;
		set_glyph(r, a.sig == SIGALRM ? "T" : "S");
		r->detail = make_detail(t, idx, bad, sig_name(a.sig), &a.s, NULL, NULL);
	}
	else if (a.s.failed)
	{
		r->res = bad;
		set_glyph(r, a.s.leaks ? "L" : "✘");
		r->detail = make_detail(t, idx, bad, NULL, &a.s, NULL, NULL);
	}
	else if (!a.s.done)
	{
		r->res = bad;
		set_glyph(r, "E");
		r->detail = make_detail(t, idx, bad,
				"le processus s'est arrêté (exit() appelé ?)", &a.s, NULL, NULL);
	}
	else
	{
		r->res = R_PASS;
		set_glyph(r, "✔");
	}
}

static void	eval_vs(const t_test *t, int idx, t_result *r)
{
	t_run	a;
	t_run	b;
	char	head[256];
	int		fc;
	int		rc;

	run_child(t, 0, 0, &a);
	run_child(t, 1, 0, &b);
	fc = a.sig != 0;
	rc = b.sig != 0 && b.sig != SIGALRM;
	if (!fc && a.s.failed)
	{
		r->res = R_FAIL;
		set_glyph(r, a.s.leaks ? "L" : "✘");
		r->detail = make_detail(t, idx, R_FAIL, NULL, &a.s, NULL, NULL);
	}
	else if (fc && a.sig == SIGALRM)
	{
		r->res = R_FAIL;
		set_glyph(r, "T");
		r->detail = make_detail(t, idx, R_FAIL, sig_name(a.sig), &a.s,
				rc ? "(l'originale crash)" : b.s.out, NULL);
	}
	else if (fc && rc)
	{
		r->res = R_PASS_CRASH;
		set_glyph(r, "✔");
	}
	else if (fc)
	{
		snprintf(head, sizeof(head),
			"ta fonction crash (%s) alors que l'originale ne crash pas",
			sig_name(a.sig));
		r->res = R_FAIL;
		set_glyph(r, "S");
		r->detail = make_detail(t, idx, R_FAIL, head, &a.s,
				b.s.out[0] ? b.s.out : "(pas de crash)", "CRASH");
	}
	else if (rc)
	{
		snprintf(head, sizeof(head),
			"l'originale crash (%s) mais pas la tienne : protection non "
			"demandée par le sujet", sig_name(b.sig));
		r->res = R_SPEC;
		set_glyph(r, "▲");
		r->detail = make_detail(t, idx, R_SPEC, head, &a.s, "CRASH",
				a.s.out[0] ? a.s.out : "(pas de crash)");
	}
	else if (strcmp(a.s.out, b.s.out) != 0)
	{
		r->res = t->spec ? R_SPEC : R_FAIL;
		set_glyph(r, t->spec ? "▲" : "✘");
		r->detail = make_detail(t, idx, r->res,
				t->spec ? "résultat différent de l'originale (comportement "
				"indéfini, spéculatif)" : "résultat différent de l'originale",
				&a.s, b.s.out, a.s.out);
	}
	else
	{
		r->res = R_PASS;
		set_glyph(r, "✔");
	}
}

static void	eval_malloc(const t_test *t, int idx, t_result *r)
{
	t_run	a;
	char	head[300];
	int		n;
	int		k;

	eval_normal(t, idx, r);
	if (r->res != R_PASS)
		return ;
	run_child(t, 0, 0, &a);
	n = a.s.armed_count;
	for (k = 1; k <= n && k <= 400; k++)
	{
		run_child(t, 0, k, &a);
		if (a.sig || a.s.failed || !a.s.done)
		{
			if (a.sig)
				snprintf(head, sizeof(head),
					"crash (%s) quand le malloc n°%d/%d renvoie NULL",
					sig_name(a.sig), k, n);
			else
				snprintf(head, sizeof(head),
					"quand le malloc n°%d/%d renvoie NULL :", k, n);
			r->res = R_FAIL;
			set_glyph(r, a.s.leaks ? "L" : "M");
			r->detail = make_detail(t, idx, R_FAIL, head, &a.s, NULL, NULL);
			return ;
		}
	}
	set_glyph(r, "M");
}

/* ============================================================ terminal === */
static int	term_width(void)
{
	struct winsize	w;

	if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_col > 20)
		return (w.ws_col > 110 ? 110 : w.ws_col);
	return (80);
}

/* visible width: skips ANSI escapes, counts UTF-8 code points */
static int	vis_len(const char *s)
{
	int	n = 0;

	while (*s)
	{
		if (*s == '\033')
		{
			while (*s && *s != 'm')
				s++;
			if (*s)
				s++;
			continue ;
		}
		if (((unsigned char)*s & 0xC0) != 0x80)
			n++;
		s++;
	}
	return (n);
}

static void	rep(const char *s, int n)
{
	while (n-- > 0)
		fputs(s, stdout);
}

/* retro section header:  ▌ TITLE ▐ ▀▀▀▀▀▀▀▀▀▀▀▀▀▀ */
static void	section(const char *title, const char *col)
{
	int	w = term_width();
	int	l = vis_len(title);

	(void)col;
	printf("\n   %s ▶ %s %s ", C_INV, title, C_RST);
	printf("%s", C_PHD);
	rep("▀", w - l - 10);
	printf("%s\n\n", C_RST);
}

/* ------------------------------------------------ 3x5 pixel font ------- */
static const char	*font(char c)
{
	switch (c)
	{
		case 'F': return ("###" "#.." "##." "#.." "#..");
		case 'T': return ("###" ".#." ".#." ".#." ".#.");
		case 'O': return ("###" "#.#" "#.#" "#.#" "###");
		case 'R': return ("##." "#.#" "##." "#.#" "#.#");
		case 'U': return ("#.#" "#.#" "#.#" "#.#" "###");
		case 'E': return ("###" "#.." "##." "#.." "###");
		case 'K': return ("#.#" "#.#" "##." "#.#" "#.#");
	}
	return ("...............");
}

static int	px(const char *w, int r, int c)
{
	int	n = (int)strlen(w);
	int	li = c / 4;
	int	lc = c % 4;

	if (r < 0 || r > 4 || c < 0 || li >= n || lc == 3)
		return (0);
	return (font(w[li])[r * 3 + lc] == '#');
}

/* one row (0..5, row 5 = shadow) of a word in pixel font, 2 chars/pixel,
   with a 1-pixel drop shadow down-right. Visible width = strlen(w) * 8.  */
static void	pixel_row(char *out, size_t osz, const char *w, int r, int col,
				int shadow)
{
	int		W = (int)strlen(w) * 4;
	int		c;
	size_t	j = 0;
	int		cur = -1;

	out[0] = 0;
	for (c = 0; c < W && j + 32 < osz; c++)
	{
		int	on = px(w, r, c);
		int	sh = !on && px(w, r - 1, c - 1);
		int	want = on ? col : sh ? shadow : -1;

		if (o.color && want != cur && want >= 0)
			j += (size_t)snprintf(out + j, osz - j, "\033[38;5;%dm", want);
		cur = want >= 0 ? want : cur;
		j += (size_t)snprintf(out + j, osz - j, "%s",
				on ? "██" : sh ? "▒▒" : "  ");
	}
	snprintf(out + j, osz - j, "%s", C_RST);
}

/* strip ANSI for the log file */
static void	log_plain(const char *s)
{
	if (!g_log || !s)
		return ;
	while (*s)
	{
		if (*s == '\033')
		{
			while (*s && *s != 'm')
				s++;
			if (*s)
				s++;
			continue ;
		}
		fputc(*s++, g_log);
	}
}

static void	banner(void)
{
	/* pixel "FTORTURE", phosphor green with scanlines. Same art as in
	   torture.sh (which prints it before the checks).                      */
	static const int	scan[] = {46, 40, 46, 40, 46, 22};
	char				row[1024];
	int					i;

	printf("\n");
	for (i = 0; i < 6; i++)
	{
		pixel_row(row, sizeof(row), "FTORTURE", i, scan[i], 22);
		printf("   %s\n", row);
	}
	printf("   %s▚▚ LIBFT TESTER · SUJET v19.3 · LINUX ▞▞%s\n", C_PHM, C_RST);
	printf("   %s> ta libft va parler%s%s_%s\n\n", C_PH, C_RST,
		o.color ? "\033[5m\033[38;5;46m" : "", C_RST);
	printf("   %slibft %s %s\n", C_PHD, C_RST, o.path);
	printf("   %smode  %s %s\n", C_PHD, C_RST, o.spec
		? "strict (-s) : les ▲ comptent comme erreurs"
		: "normal : ▲ = avertissements (-s pour les compter)");
	printf("   %sseed  %s %u\n", C_PHD, C_RST, o.seed);
}

static void	legend(void)
{
	printf("\n   %sLégende%s  %s✔%s ok   %s✔%s crash identique à la libc   "
		"%s✘%s erreur   %sS%s crash\n            %sT%s timeout   %sL%s fuite   "
		"%sM%s malloc protégé   %s▲%s spéculatif\n",
		C_GRY, C_RST, C_GRN, C_RST, C_CYN, C_RST, C_RED, C_RST, C_RED, C_RST,
		C_RED, C_RST, C_RED, C_RST, C_GRN, C_RST, C_YEL, C_RST);
}

/* ============================================================ prechecks == */
typedef struct s_ck
{
	int	ok;
	int	ko;
	int	warn;
	int	proto_ok;
	int	proto_tot;
	int	present;
}	t_ck;

static t_ck	g_ck;

static char	*field(char **p)
{
	char	*s = *p;
	char	*e;

	if (!s)
		return ("");
	e = strchr(s, '|');
	if (e)
	{
		*e = 0;
		*p = e + 1;
	}
	else
		*p = NULL;
	return (s);
}

static void	status_mark(const char *st, const char **mk, const char **col)
{
	*mk = "•";
	*col = C_GRY;
	if (!strcmp(st, "OK"))
	{
		*mk = "✔";
		*col = C_GRN;
	}
	else if (!strcmp(st, "KO"))
	{
		*mk = "✘";
		*col = C_RED;
	}
	else if (!strcmp(st, "WARN"))
	{
		*mk = "▲";
		*col = C_YEL;
	}
	else if (!strcmp(st, "SKIP"))
		*mk = "–";
}

static void	print_checks(void)
{
	FILE		*f;
	char		line[4096];
	int			proto_header = 0;
	t_sb		protos = {0};

	if (!o.checks || !(f = fopen(o.checks, "r")))
		return ;
	while (fgets(line, sizeof(line), f))
	{
		char		*p = line;
		char		*kind;
		const char	*mk;
		const char	*col;

		line[strcspn(line, "\n")] = 0;
		kind = field(&p);
		if (!strcmp(kind, "S"))
		{
			if (protos.n)
			{
				fputs(protos.s, stdout);
				protos.n = 0;
			}
			{
				char	*title = field(&p);

				section(title, C_MAG);
				if (g_log)
					fprintf(g_log, "\n== %s ==\n", title);
			}
		}
		else if (!strcmp(kind, "C"))
		{
			char	*st = field(&p);
			char	*label = field(&p);
			char	*detail = p ? p : "";

			status_mark(st, &mk, &col);
			if (!strcmp(st, "OK"))
				g_ck.ok++;
			else if (!strcmp(st, "KO"))
				g_ck.ko++;
			else if (!strcmp(st, "WARN"))
				g_ck.warn++;
			printf("   %s%s%s %-24s %s%s%s\n", col, mk, C_RST, label,
				!strcmp(st, "OK") ? C_GRY : "", detail, C_RST);
			if (g_log)
				fprintf(g_log, "[%s] %s : %s\n", st, label, detail);
		}
		else if (!strcmp(kind, "D"))
		{
			printf("       %s%s%s\n", C_GRY, p ? p : "", C_RST);
			if (g_log)
				fprintf(g_log, "       %s\n", p ? p : "");
		}
		else if (!strcmp(kind, "P"))
		{
			char	*name = field(&p);
			char	*st = field(&p);
			char	*exp = field(&p);
			char	*found = p ? p : "";

			(void)proto_header;
			g_ck.proto_tot++;
			if (!strcmp(st, "OK"))
			{
				g_ck.proto_ok++;
				continue ;
			}
			status_mark(st, &mk, &col);
			sb_add(&protos, "   %s%s%s %s%s%s\n", col, mk, C_RST, C_B, name,
				C_RST);
			sb_add(&protos, "       %sattendu%s %s%s%s\n", C_GRY, C_RST,
				C_GRN, exp, C_RST);
			sb_add(&protos, "       %strouvé %s %s%s%s\n", C_GRY, C_RST,
				col, *found ? found : "(non déclaré dans libft.h)", C_RST);
			if (g_log)
				fprintf(g_log, "[PROTO %s] %s\n   attendu: %s\n   trouvé : %s\n",
					st, name, exp, found);
		}
		else if (!strcmp(kind, "PS"))
		{
			int	bad = g_ck.proto_tot - g_ck.proto_ok;

			printf("   %s%s%s %-24s %s%d/%d conformes%s\n",
				bad ? C_RED : C_GRN, bad ? "✘" : "✔", C_RST, "Prototypes",
				bad ? C_RED : C_GRY, g_ck.proto_ok, g_ck.proto_tot, C_RST);
			if (bad)
				g_ck.ko++;
			else
				g_ck.ok++;
			if (protos.n)
			{
				fputs(protos.s, stdout);
				protos.n = 0;
			}
		}
	}
	if (protos.n)
		fputs(protos.s, stdout);
	free(protos.s);
	fclose(f);
}

/* ============================================================ main loop == */
typedef struct s_fstat
{
	int	pass;
	int	fail;
	int	spec;
	int	crash_eq;
	int	total;
	int	missing;
	int	ran;
}	t_fstat;

static t_fstat	g_fs[64];

static int	selected(const t_func *f)
{
	const char	*p;
	size_t		l;

	if (o.part && f->part != o.part)
		return (0);
	if (!o.only)
		return (1);
	l = strlen(f->name);
	p = o.only;
	while ((p = strstr(p, f->name)))
	{
		if ((p == o.only || p[-1] == ',') && (p[l] == ',' || p[l] == 0))
			return (1);
		p += l;
	}
	/* accept names without the ft_ prefix */
	p = o.only;
	l -= 3;
	while ((p = strstr(p, f->name + 3)))
	{
		if ((p == o.only || p[-1] == ',') && (p[l] == ',' || p[l] == 0))
			return (1);
		p += l;
	}
	return (0);
}

static void	run_function(int fi, int *ti)
{
	t_func		*f = &g_funcs[fi];
	t_fstat		*fs = &g_fs[fi];
	int			start = *ti;
	int			end = start;
	int			i;
	int			col;
	int			w = term_width();
	t_result	*res;
	int			shown = 0;
	const char	*cnt_col;

	while (end < g_ntests && !strcmp(g_tests[end].func, f->name))
		end++;
	*ti = end;
	if (!selected(f))
		return ;
	fs->ran = 1;
	fs->total = end - start;
	printf("   %s%-16s%s ", C_B, f->name, C_RST);
	if (!f->addr)
	{
		fs->missing = 1;
		printf("%s✘ ABSENTE de libft.a%s %s(%d tests ignorés)%s\n", C_BGR,
			C_RST, C_GRY, fs->total, C_RST);
		if (g_log)
			fprintf(g_log, "\n%s : ABSENTE\n", f->name);
		return ;
	}
	fflush(stdout);
	res = calloc((size_t)(end - start) + 1, sizeof(t_result));
	col = 21;
	for (i = start; i < end; i++)
	{
		t_result	*r = &res[i - start];
		const char	*gc;

		if (g_tests[i].kind == K_VSREF)
			eval_vs(&g_tests[i], i - start + 1, r);
		else if (g_tests[i].kind == K_MALLOC)
			eval_malloc(&g_tests[i], i - start + 1, r);
		else
			eval_normal(&g_tests[i], i - start + 1, r);
		if (r->res == R_PASS)
			fs->pass++;
		else if (r->res == R_PASS_CRASH)
		{
			fs->pass++;
			fs->crash_eq++;
		}
		else if (r->res == R_SPEC)
			fs->spec++;
		else if (r->res == R_FAIL)
			fs->fail++;
		gc = r->res == R_PASS ? C_GRN : r->res == R_PASS_CRASH ? C_CYN
			: r->res == R_SPEC ? C_YEL : C_RED;
		if (col + 2 > w - 10)
		{
			printf("\n%*s", 21, "");
			col = 21;
		}
		printf("%s%s%s ", gc, r->glyph, C_RST);
		col += 2;
		fflush(stdout);
	}
	{
		int	bad = fs->fail + (o.spec ? fs->spec : 0);

		cnt_col = bad ? C_RED : fs->spec ? C_YEL : C_GRN;
		printf(" %s%s%d/%d%s", C_B, cnt_col, fs->pass, fs->total, C_RST);
		if (fs->spec)
			printf(" %s(%d ▲)%s", C_YEL, fs->spec, C_RST);
		printf("\n");
	}
	if (g_log)
		fprintf(g_log, "\n%s : %d/%d (%d spéculatif)\n", f->name, fs->pass,
			fs->total, fs->spec);
	for (i = 0; i < end - start; i++)
	{
		if (!res[i].detail)
			continue ;
		log_plain(res[i].detail);
		if (o.verbose || (shown < o.max_details && (res[i].res == R_FAIL
					|| o.spec)))
		{
			fputs(res[i].detail, stdout);
			shown++;
		}
		else if (res[i].res == R_SPEC && shown < o.max_details + 2)
		{
			/* compact one-liner for speculative warnings */
			char	*nl = strchr(res[i].detail, '\n');

			if (nl)
				fwrite(res[i].detail, 1, (size_t)(nl - res[i].detail) + 1,
					stdout);
			shown++;
		}
		free(res[i].detail);
	}
	{
		int	hidden = 0;

		for (i = 0; i < end - start; i++)
			if (res[i].res == R_FAIL || res[i].res == R_SPEC)
				hidden++;
		if (hidden > shown)
			printf("      %s… %d autre(s) détail(s) : relance avec -v ou "
				"regarde %s%s\n", C_GRY, hidden - shown,
				o.log ? o.log : "le log", C_RST);
	}
	free(res);
}

static void	bar(char *out, size_t osz, int done, int tot, int width,
				const char *col)
{
	int		f = tot ? done * width / tot : 0;
	size_t	j;
	int		i;

	/* pixel segments: ▰ filled, ▱ empty */
	j = (size_t)snprintf(out, osz, "%s", col);
	for (i = 0; i < width && j + 16 < osz; i++)
	{
		if (i == f)
			j += (size_t)snprintf(out + j, osz - j, "%s%s", C_RST, C_PHD);
		j += (size_t)snprintf(out + j, osz - j, "%s", i < f ? "█" : "░");
	}
	snprintf(out + j, osz - j, "%s", C_RST);
}

/* copy s into out, padded with spaces to a VISIBLE width of w (UTF-8 safe) */
static void	padv(char *out, size_t osz, const char *s, int w)
{
	int	l = vis_len(s);

	snprintf(out, osz, "%s%*s", s, w > l ? w - l : 0, "");
}

/* pixel dialog box:  █▀▀ TITLE ▀▀▀█ / █ content █ / █▄▄▄▄▄▄▄▄▄▄█
   `inner` = visible width between the two side borders                   */
static int	g_inner;

__attribute__((format(printf, 1, 2)))
static void	box_line(const char *fmt, ...)
{
	char	buf[2048];
	va_list	ap;
	int		l;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	l = vis_len(buf);
	printf("   %s█%s %s", C_PHM, C_RST, buf);
	rep(" ", g_inner - 2 - l);
	printf(" %s█%s\n", C_PHM, C_RST);
}

__attribute__((format(printf, 1, 2)))
static void	box_center(const char *fmt, ...)
{
	char	buf[2048];
	va_list	ap;
	int		pad;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	pad = (g_inner - 2 - vis_len(buf)) / 2;
	box_line("%*s%s", pad > 0 ? pad : 0, "", buf);
}

static int	summary(void)
{
	static const char	*short_names[] = {"", "libc", "supplémentaires",
		"listes chaînées"};
	int					p;
	int					i;
	int					tp = 0, tf = 0, ts = 0, tce = 0, tt = 0;
	int					fok = 0, fall = 0, miss = 0;
	int					bad;
	char				row[1024];

	g_inner = term_width() - 6;
	if (g_inner > 76)
		g_inner = 76;
	printf("\n   %s█▀▀▀%s RÉSULTAT %s", C_PHM, C_INV, C_RST);
	printf("%s", C_PHM);
	rep("▀", g_inner - 13);
	printf("█%s\n", C_RST);
	box_line("%s", "");
	for (p = 1; p <= 3; p++)
	{
		int		ok = 0, all = 0;

		for (i = 0; i < NFUNCS; i++)
		{
			t_fstat	*fs = &g_fs[i];
			int		bad_f;

			if (g_funcs[i].part != p || !fs->ran)
				continue ;
			all++;
			bad_f = fs->missing || fs->fail || (o.spec && fs->spec);
			if (!bad_f)
				ok++;
			tp += fs->pass;
			tf += fs->fail;
			ts += fs->spec;
			tce += fs->crash_eq;
			tt += fs->total;
			miss += fs->missing;
		}
		if (!all)
			continue ;
		fok += ok;
		fall += all;
		{
			/* fixed columns, measured in visible characters:
			   "P1 " + label(16) + " ▕" + bar + "▏ " + "21/23" + "  " + "100%" */
			int		bw = g_inner - 2 - 34;
			char	mb[1200];
			char	lbl[64];

			if (bw < 6)
				bw = 6;
			bar(mb, sizeof(mb), ok, all, bw, ok == all ? C_PH
				: ok * 2 >= all ? C_YEL : C_RED);
			padv(lbl, sizeof(lbl), short_names[p], 16);
			box_line("%sP%d%s %s %s▕%s%s▏%s %s%2d/%-2d%s  %3d%%", C_PH, p,
				C_RST, lbl, C_PHD, mb, C_PHD, C_RST, ok == all ? C_PH : C_RED,
				ok, all, C_RST, ok * 100 / all);
		}
	}
	box_line("%s", "");
	box_line("%sTESTS %s %s%d ✔%s  %s%d ✘%s  %s%d ▲%s  %s/ %d%s", C_PHD, C_RST,
		C_GRN, tp, C_RST, tf ? C_RED : C_PHD, tf, C_RST, ts ? C_YEL : C_PHD, ts,
		C_RST, C_PHD, tt, C_RST);
	if (tce)
		box_line("%s       dont %d crash identique(s) à la libc (acceptés)%s",
			C_CYN, tce, C_RST);
	if (miss)
		box_line("%sABSENT%s %s%d fonction(s)%s", C_PHD, C_RST, C_RED, miss,
			C_RST);
	if (o.checks)
		box_line("%sCHECKS%s %s%d ✔%s  %s%d ✘%s  %s%d ▲%s  %sprotos %d/%d%s",
			C_PHD, C_RST, C_GRN, g_ck.ok, C_RST, g_ck.ko ? C_RED : C_PHD,
			g_ck.ko, C_RST, g_ck.warn ? C_YEL : C_PHD, g_ck.warn, C_RST,
			g_ck.proto_ok == g_ck.proto_tot ? C_GRN : C_RED, g_ck.proto_ok,
			g_ck.proto_tot, C_RST);
	box_line("%s", "");
	bad = tf + miss + g_ck.ko + (o.spec ? ts : 0);
	/* big pixel verdict */
	for (i = 0; i < 6; i++)
	{
		pixel_row(row, sizeof(row), bad ? "KO" : "OK", i,
			bad ? (i % 2 ? 160 : 196) : (i % 2 ? 40 : 46), bad ? 52 : 22);
		box_center("%s", row);
	}
	if (!bad)
		box_center("%s%d/%d fonctions parfaites · aucune erreur%s", C_PH, fok,
			fall, C_RST);
	else
		box_center("%s%d problème(s) · %d/%d fonctions OK%s", C_RED, bad, fok,
			fall, C_RST);
	if (!o.spec && ts)
		box_center("%s%d avertissement(s) ▲ à savoir défendre%s", C_YEL, ts,
			C_RST);
	box_line("%s", "");
	printf("   %s█", C_PHM);
	rep("▄", g_inner);
	printf("█%s\n", C_RST);
	if (o.log)
		printf("   %sDétails complets : %s%s\n", C_GRY, o.log, C_RST);
	printf("\n");
	if (g_log)
		fprintf(g_log, "\nTOTAL: %d ok, %d ko, %d spéculatifs, %d absentes, "
			"%d vérifs KO -> %s\n", tp, tf, ts, miss, g_ck.ko,
			bad ? "KO" : "OK");
	return (bad ? 1 : 0);
}

static void	usage(void)
{
	printf("usage: ftorture_tester [options]\n"
		"  -s, --speculative  les erreurs spéculatives (▲) comptent\n"
		"  -v, --verbose      affiche tous les détails\n"
		"  -f, --func LIST    ne teste que ces fonctions (ft_split,itoa,...)\n"
		"  -p, --part N       ne teste que la partie N (1, 2 ou 3)\n"
		"      --checks FILE  résultats des vérifications (généré par torture)\n"
		"      --log FILE     écrit un rapport sans couleurs\n"
		"      --path PATH    chemin affiché de la libft\n"
		"      --seed N       graine des tests aléatoires\n"
		"      --timeout S    timeout par test (défaut 3s)\n"
		"      --no-color\n");
}

int	main(int ac, char **av)
{
	int	i;
	int	ti;
	int	part;

	for (i = 1; i < ac; i++)
	{
		const char	*a = av[i];

		if (!strcmp(a, "-s") || !strcmp(a, "--speculative"))
			o.spec = 1;
		else if (!strcmp(a, "-v") || !strcmp(a, "--verbose"))
			o.verbose = 1;
		else if ((!strcmp(a, "-f") || !strcmp(a, "--func")) && i + 1 < ac)
			o.only = av[++i];
		else if ((!strcmp(a, "-p") || !strcmp(a, "--part")) && i + 1 < ac)
			o.part = atoi(av[++i]);
		else if (!strcmp(a, "--checks") && i + 1 < ac)
			o.checks = av[++i];
		else if (!strcmp(a, "--log") && i + 1 < ac)
			o.log = av[++i];
		else if (!strcmp(a, "--path") && i + 1 < ac)
			o.path = av[++i];
		else if (!strcmp(a, "--seed") && i + 1 < ac)
			o.seed = (unsigned)strtoul(av[++i], NULL, 10);
		else if (!strcmp(a, "--timeout") && i + 1 < ac)
			o.timeout = atoi(av[++i]);
		else if (!strcmp(a, "--no-color"))
			o.color = 0;
		else if (!strcmp(a, "--no-banner"))
			o.no_banner = 1;
		else
		{
			usage();
			return (a[1] == 'h' || !strcmp(a, "--help") ? 0 : 2);
		}
	}
	if (!isatty(1) || getenv("NO_COLOR"))
		o.color = 0;
	if (o.timeout < 1)
		o.timeout = 3;
	g_shm = mmap(NULL, sizeof(t_shm), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (g_shm == MAP_FAILED)
		return (perror("mmap"), 2);
	if (o.log)
		g_log = fopen(o.log, "w");
	qsort(g_tests, (size_t)g_ntests, sizeof(t_test), cmp_tests);
	if (!o.no_banner)
		banner();
	print_checks();
	legend();
	ti = 0;
	part = 0;
	for (i = 0; i < NFUNCS; i++)
	{
		if (g_funcs[i].part != part && (!o.part || o.part == g_funcs[i].part))
		{
			int		j;
			int		any = 0;
			char	title[128];

			for (j = i; j < NFUNCS && g_funcs[j].part == g_funcs[i].part; j++)
				any |= selected(&g_funcs[j]);
			part = g_funcs[i].part;
			if (any)
			{
				snprintf(title, sizeof(title), "PARTIE %d · %s", part,
					g_part_names[part]);
				section(title, C_BLU);
				if (g_log)
					fprintf(g_log, "\n===== %s =====\n", title);
			}
		}
		while (ti < g_ntests && func_index(g_tests[ti].func) < i)
			ti++;
		run_function(i, &ti);
	}
	i = summary();
	if (g_log)
		fclose(g_log);
	return (i);
}
