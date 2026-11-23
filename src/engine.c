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
#include <math.h>
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
	int			no_summary;		/* first of two chained runs            */
	int			no_legend;
	const char	*stats_out;		/* save counters for a later run        */
	const char	*stats_in;		/* load counters from a previous run    */
	const char	*note;			/* extra line in the summary box        */
	int			spec_pass;		/* every failure of this run is a ▲     */
	int			no_section;		/* chained run: part header already out */
	int			english;
	int			eff;			/* --efficiency: time against the expected */
	int			eff_target;		/* expected efficiency, in %              */
	const char	*names;			/* --name: only tests whose name has one  */
	const char	*repro_cmd;		/* "torture libft /path": -v replay hint  */
}	o = {0, 0, 1, 0, NULL, NULL, NULL, ".", 42, 3, 3, 0, 0, 0, NULL, NULL,
	NULL, 0, 0, 0, 0, 50, NULL, NULL};

#define L(fr, en) (o.english ? (en) : (fr))

static FILE	*g_log;

/* Translate labels and assertion descriptions only; never rewrite test data. */
static const char	*localize(const char *src)
{
	static char buf[8][2048];
	static unsigned idx;
	static const struct { const char *fr, *en; } words[] = {
		{"une ligne avec", "one line with"},
		{"une ligne sans", "one line without"},
		{"un seul", "a single"},
		{"caractère", "character"},
		{"lignes vides", "empty lines"},
		{"lignes variées", "varied lines"},
		{"ligne longue", "long line"},
		{"ligne très longue", "very long line"},
		{"ligne différente", "line differs"},
		{"dernière ligne", "last line"},
		{"au milieu", "in the middle"},
		{"à la fin", "at the end"},
		{"entrée standard", "standard input"},
		{"pipe alimenté par petits morceaux", "pipe fed in small pieces"},
		{"fd invalides", "invalid fds"},
		{"fd fermé", "closed fd"},
		{"fd élevé", "high fd"},
		{"un dossier", "a directory"},
		{"renvoyé alors qu'il reste des lignes", "returned while lines remain"},
		{"rappel après la fin du fichier", "call after end of file"},
		{"reçu", "received"},
		{"octets", "bytes"}, {"octet", "byte"},
		{"échoue", "fails"}, {"fuite", "leak"},
		{"fermés", "closed"}, {"fermé", "closed"},
		{"fichier", "file"}, {"ligne", "line"},
		{"après", "after"}, {"sans", "without"},
		{"avec", "with"}, {"alors que", "while"},
		{"NULL renvoyé", "NULL returned"},
		{"alors qu'il reste des lignes", "while lines remain"},
		{"pas de", "no"}, {"ni de", "or"},
		{"jusqu'à", "up to"}, {"en moins de", "in less than"},
		{"fichier généré :", "generated file:"}, {"ton fichier :", "your file:"},
		{"fichier généré", "generated file"}, {"ton fichier", "your file"},
		{" Ko", " KB"}, {" Mo", " MB"},
		/* push_swap */
		{"nombres", "numbers"}, {"coups", "moves"}, {"aléatoires", "random"},
		{"inversé", "reversed"}, {"trié", "sorted"},
		{"désordre fort", "high disorder"}, {"désordre en", "disorder in"},
		{"désordre", "disorder"}, {"croissance", "growth"},
		{"différent de", "differs from"}, {"chaque opération", "each operation"},
		{"total des opérations", "operation total"}, {"opérations", "operations"},
		{"nom ", "name "}, {"(bien)", "(good)"}, {"rien", "nothing"},
		{"non vide", "not empty"}, {"en moins", "missing"},
		{"ligne vide", "empty line"}, {"flag", "flag"},
		{NULL, NULL}
	};
	char *out;
	size_t i, n = 0;

	if (!o.english || !src)
		return (src);
	out = buf[(idx++) % 8];
	while (*src && n + 1 < sizeof(buf[0]))
	{
		for (i = 0; words[i].fr; i++)
			if (!strncmp(src, words[i].fr, strlen(words[i].fr)))
				break ;
		if (words[i].fr)
		{
			size_t len = strlen(words[i].en);
			if (n + len >= sizeof(buf[0])) break ;
			memcpy(out + n, words[i].en, len);
			n += len;
			src += strlen(words[i].fr);
		}
		else out[n++] = *src++;
	}
	out[n] = 0;
	return (out);
}

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
/* the project (libft / ft_printf) is described in src/<project>/proj.c */
#include "project.h"
#define g_funcs (g_proj.funcs)
#define NFUNCS (g_proj.nfuncs)
#define g_part_names (g_proj.part_names)

static void	localize_project(void)
{
	int i;

	if (!o.english)
		return ;
	if (!strcmp(g_proj.name, "libft"))
	{
		g_part_names[1] = "libc functions";
		g_part_names[2] = "Additional functions";
		g_part_names[3] = "Linked lists";
		g_proj.short_names[2] = "additional";
		g_proj.short_names[3] = "linked lists";
	}
	else if (!strcmp(g_proj.name, "ft_printf"))
	{
		g_part_names[1] = "Mandatory · cspdiuxX%";
		g_part_names[2] = "Bonus · -0. width # +";
		g_proj.short_names[1] = "mandatory";
		for (i = 0; i < g_proj.nfuncs; i++)
		{
			if (!strcmp(g_funcs[i].name, "pf_mix")) g_funcs[i].label = "mixed";
			if (!strcmp(g_funcs[i].name, "pf_edge")) g_funcs[i].label = "edge cases";
			if (!strcmp(g_funcs[i].name, "pf_prec")) g_funcs[i].label = "precision .";
			if (!strcmp(g_funcs[i].name, "pf_width")) g_funcs[i].label = "width";
			if (!strcmp(g_funcs[i].name, "pf_combo")) g_funcs[i].label = "combinations";
			if (!strcmp(g_funcs[i].name, "pf_undef")) g_funcs[i].label = "undefined cases";
		}
	}
	else if (!strcmp(g_proj.name, "push_swap"))
	{
		static const char	*en[][2] = {
			{"args", "no arguments"}, {"errors", "errors"},
			{"sorted", "already sorted"}, {"small", "2 to 5 numbers"},
			{"simple", "--simple"}, {"medium", "--medium"},
			{"complex", "--complex"}, {"adaptive", "--adaptive"},
			{"bench", "--bench"}, {"perf100", "100 random"},
			{"perf500", "500 random"}, {"checker", "checker"},
			{"ck_errors", "checker errors"}, {NULL, NULL}};
		int					k;

		g_part_names[1] = "Mandatory · push_swap";
		g_part_names[2] = "Bonus · checker";
		g_proj.short_names[1] = "mandatory";
		for (i = 0; i < g_proj.nfuncs; i++)
			for (k = 0; en[k][0]; k++)
				if (!strcmp(g_funcs[i].name, en[k][0]))
					g_funcs[i].label = en[k][1];
	}
	else
	{
		g_part_names[1] = "Mandatory · one BUFFER_SIZE per line";
		g_part_names[2] = "Bonus · multiple fds at once";
		g_proj.short_names[1] = "mandatory";
		for (i = 0; i < g_proj.nfuncs; i++)
			if (!strcmp(g_funcs[i].name, "bsdef"))
				g_funcs[i].label = "without -D (default)";
	}
}

static void	padv(char *out, size_t osz, const char *s, int w);

static t_test	*g_tests;
static int		g_ntests;
static int		g_cap;

void	zft_register(const char *func, const char *name, t_kind kind, int spec,
			void (*fn)(void), const char *file, int line, int idx)
{
	if (g_ntests == g_cap)
	{
		g_cap = g_cap ? g_cap * 2 : 256;
		g_tests = realloc(g_tests, sizeof(t_test) * (size_t)g_cap);
	}
	g_tests[g_ntests] = (t_test){func, name, kind, spec, fn, file, line,
		g_ntests, idx};
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
				L("fuite mémoire : %d bloc(s), %zu octet(s) jamais libéré(s)", "memory leak: %d block(s), %zu byte(s) not freed"),
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
		case SIGFPE: return (L("SIGFPE · erreur arithmétique", "SIGFPE · arithmetic error"));
		case SIGALRM: return (L("TIMEOUT · boucle infinie ?", "TIMEOUT · infinite loop?"));
		case SIGILL: return (L("SIGILL · instruction illégale", "SIGILL · illegal instruction"));
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
	detail_kv(b, L("entrée", "input"), localize(s->input), "");
	if (s->failed)
	{
		if (s->exp[0] || s->got[0])
		{
			detail_kv(b, "expr", localize(s->msg), C_DIM);
			detail_kv(b, L("attendu", "expected"), s->exp, C_GRN);
			detail_kv(b, L("obtenu", "actual"), s->got, C_RED);
		}
		else
			detail_kv(b, L("erreur", "error"), localize(s->msg), "");
	}
	(void)with_values;
}

/* ===================================================== verbose details === */
/* With -v every failure also gets: why it fails and what to look at, the
   source of the test, the call that triggers it, a main.c to paste, and the
   command that rebuilds this single test with your code (--repro). The
   extra part starts with DETAIL_SEP: without -v only the part before it is
   printed, the log always gets everything.                                */
#define DETAIL_SEP '\x1e'

enum e_why
{
	W_NONE,
	W_SIG,			/* normal test killed by a signal                         */
	W_ASSERT,		/* an assertion failed                                    */
	W_LEAK,
	W_EXIT,
	W_VS_CRASH,		/* yours crashes, the reference does not                  */
	W_VS_SPEC,		/* the reference crashes, yours does not                  */
	W_VS_DIFF,
	W_VS_DIFF_SPEC,
	W_VS_TIMEOUT,
	W_MALLOC		/* failure while malloc n°k returns NULL                  */
};

static int	g_why;
static int	g_why_sig;
static int	g_fail_k;

static void	why(int w, int sig, int k)
{
	g_why = w;
	g_why_sig = sig;
	g_fail_k = k;
}

/* "        label    text" wrapped at ~76 columns */
static void	sb_wrap(t_sb *b, const char *label, const char *text,
				const char *col)
{
	const int	width = 64;
	const char	*p = text;
	int			first = 1;

	while (*p)
	{
		const char	*end = p;
		const char	*cut = NULL;
		int			n = 0;

		while (*end && *end != '\n' && n < width)
		{
			if (*end == ' ')
				cut = end;
			if (((unsigned char)*end & 0xC0) != 0x80)
				n++;
			end++;
		}
		if (*end && *end != '\n' && cut)
			end = cut;
		sb_add(b, "        %s%-9s%s %s%.*s%s\n", C_GRY, first ? label : "",
			C_RST, col, (int)(end - p), p, C_RST);
		first = 0;
		p = end;
		while (*p == ' ' || *p == '\n')
			p++;
	}
}

static int	has(const char *s, const char *w)
{
	return (s && strstr(s, w) != NULL);
}

static const char	*sig_hint(int sig, const t_test *t)
{
	int	guarded = has(t->name, "gardé") || has(t->name, "bornée")
		|| has(t->name, "ne lit pas") || has(t->name, "sans lire");

	if (sig == SIGSEGV || sig == SIGBUS)
		return (guarded
			? L("Ce test colle les données juste avant une page mémoire "
				"interdite : ta fonction lit (ou écrit) au-delà de la fin. "
				"Vérifie la condition d'arrêt de ta boucle : teste la taille "
				"(n, size, len) AVANT de lire le caractère suivant.",
				"This test puts the data right before a forbidden memory "
				"page: your function reads (or writes) past the end. Check "
				"your loop condition: test the size (n, size, len) BEFORE "
				"reading the next character.")
			: L("Accès mémoire interdit : pointeur NULL déréférencé, lecture "
				"ou écriture après la fin d'un buffer, ou utilisation d'un "
				"bloc déjà libéré. Les données du test sont collées à une "
				"page interdite et chaque malloc aussi : le moindre octet de "
				"trop crashe.",
				"Forbidden memory access: NULL pointer dereferenced, read or "
				"write past the end of a buffer, or use of a freed block. "
				"The test data and every malloc block sit right before a "
				"forbidden page: one byte too far crashes."));
	if (sig == SIGABRT)
		return (L("La libc a détecté une corruption et a tout arrêté : free() "
			"d'un pointeur qui ne vient pas de malloc (chaîne littérale, "
			"tableau sur la pile, ptr + i), double free, ou écriture hors "
			"d'un bloc malloc.",
			"The libc detected a corruption and aborted: free() of a "
			"pointer that does not come from malloc (string literal, stack "
			"array, ptr + i), double free, or a write outside a malloc "
			"block."));
	if (sig == SIGALRM)
		return (L("Ta fonction ne rend jamais la main : boucle infinie "
			"(indice qui n'avance pas, condition toujours vraie, size_t "
			"comparé à < 0, i-- sur un unsigned) ou algorithme beaucoup "
			"trop lent (strlen recalculé à chaque tour, recopie complète à "
			"chaque ajout).",
			"Your function never returns: infinite loop (index that does "
			"not move, condition always true, size_t compared with < 0, "
			"i-- on an unsigned) or a far too slow algorithm (strlen "
			"recomputed every turn, full copy on every append)."));
	if (sig == SIGFPE)
		return (L("Division ou modulo par zéro (ou INT_MIN / -1).",
			"Division or modulo by zero (or INT_MIN / -1)."));
	return (L("Ton code a été tué par un signal.",
		"Your code was killed by a signal."));
}

static const char	*explain(const t_test *t, const t_shm *s)
{
	switch (g_why)
	{
		case W_SIG:
			return (sig_hint(g_why_sig, t));
		case W_LEAK:
			return (L("Des blocs alloués avec malloc ne sont jamais libérés à "
				"la fin du test (le test libère tout ce que ta fonction "
				"renvoie). Chaque malloc doit avoir son free, y compris "
				"dans les chemins d'erreur, et une variable static doit "
				"être libérée quand on a fini (fin de fichier pour gnl).",
				"Blocks allocated with malloc are never freed at the end of "
				"the test (the test frees everything your function returns). "
				"Every malloc needs its free, error paths included, and a "
				"static variable must be freed once done (end of file for "
				"gnl)."));
		case W_EXIT:
			return (L("Le processus s'est terminé pendant le test : une "
				"fonction de bibliothèque ne doit jamais appeler exit().",
				"The process ended during the test: a library function must "
				"never call exit()."));
		case W_VS_CRASH:
			return (L("La vraie fonction (glibc, BSD, ou la référence pour les "
				"fonctions sans original) traite cette entrée sans crasher : "
				"la tienne ne doit pas crasher non plus. « attendu » montre "
				"ce que renvoie l'originale. Souvent : une taille à 0 doit "
				"empêcher toute lecture, même avec un pointeur NULL.",
				"The real function (glibc, BSD, or the reference for "
				"functions without an original) handles this input without "
				"crashing: yours must not crash either. \"expected\" shows "
				"what the original returns. Often: a size of 0 must prevent "
				"any read, even with a NULL pointer."));
		case W_VS_SPEC:
			return (L("La vraie fonction crash sur cette entrée, pas la tienne : "
				"c'est une protection que le sujet ne demande pas. Ce n'est "
				"pas une erreur, mais en correction on peut te demander "
				"pourquoi ton comportement diffère de l'original.",
				"The real function crashes on this input, yours does not: "
				"a protection the subject does not ask for. Not an error, "
				"but in evaluation you may be asked why your behavior "
				"differs from the original."));
		case W_VS_DIFF:
			return (L("Même entrée, résultats différents : « attendu » est ce "
				"que renvoie la vraie fonction, « obtenu » ce que renvoie la "
				"tienne (les valeurs sont séparées par |, une par appel du "
				"test).",
				"Same input, different results: \"expected\" is what the real "
				"function returns, \"actual\" what yours returns (values are "
				"separated by |, one per call of the test)."));
		case W_VS_DIFF_SPEC:
			return (L("Résultat différent de l'original sur un cas que le "
				"standard ne définit pas (comportement indéfini) : ▲ "
				"seulement, à savoir expliquer.",
				"Result differs from the original on a case the standard "
				"does not define (undefined behavior): ▲ only, be ready to "
				"explain it."));
		case W_VS_TIMEOUT:
			return (sig_hint(SIGALRM, t));
		case W_MALLOC:
			return (g_why_sig
				? L("Le test est rejoué en faisant échouer un malloc précis "
					"(il renvoie NULL) et ta fonction crash : un retour de "
					"malloc n'est pas vérifié avant d'être utilisé.",
					"The test is replayed with one malloc failing (it returns "
					"NULL) and your function crashes: a malloc return value "
					"is used without being checked.")
				: has(s->msg, "fuite") || has(s->msg, "leak")
				? L("Le test est rejoué en faisant échouer un malloc précis : "
					"ta fonction renvoie bien une erreur mais ne libère pas ce "
					"qu'elle avait déjà alloué avant l'échec.",
					"The test is replayed with one malloc failing: your "
					"function reports the error but does not free what it had "
					"already allocated before the failure.")
				: L("Le test est rejoué en faisant échouer un malloc précis : "
					"ta fonction doit alors libérer ce qu'elle a déjà alloué "
					"et renvoyer NULL (ou l'erreur prévue par le sujet).",
					"The test is replayed with one malloc failing: your "
					"function must then free what it already allocated and "
					"return NULL (or the error the subject asks for)."));
		case W_ASSERT:
			if (has(s->msg, "double free") || has(s->msg, "free()"))
				return (L("Tu libères deux fois le même bloc, ou un pointeur qui "
					"n'est pas le début d'un bloc malloc (free(ptr + i), free "
					"d'une chaîne littérale ou d'un tableau).",
					"You free the same block twice, or a pointer that is not "
					"the start of a malloc block (free(ptr + i), free of a "
					"string literal or an array)."));
			if (has(s->msg, "bloc") || has(s->msg, "block")
				|| has(s->msg, "malloc"))
				return (L("Problème d'allocation : le bloc renvoyé doit venir de "
					"malloc et contenir au moins la chaîne + 1 octet pour le "
					"'\\0'. Compte bien la longueur avant le malloc.",
					"Allocation problem: the returned block must come from "
					"malloc and hold at least the string + 1 byte for the "
					"'\\0'. Count the length carefully before malloc."));
			return (L("Une vérification du test a échoué : compare « attendu » "
				"et « obtenu » pour l'entrée indiquée. La ligne du fichier de "
				"test donne la vérification exacte.",
				"A check of the test failed: compare \"expected\" and "
				"\"actual\" for the given input. The test file line gives "
				"the exact check."));
	}
	return (NULL);
}

/* ------------------------------------------------- source of the test -- */
static const char	*k_test_macros[] = {"TEST(", "SPEC_TEST(", "VS(",
	"VS_SPEC(", "MTEST(", NULL};

static int	is_test_line(const char *l)
{
	while (*l == ' ' || *l == '\t')
		l++;
	for (int i = 0; k_test_macros[i]; i++)
		if (!strncmp(l, k_test_macros[i], strlen(k_test_macros[i])))
			return (1);
	return (0);
}

/* braces of one line, outside string and char literals */
static int	brace_delta(const char *l, int *seen)
{
	int		d = 0;
	char	q = 0;

	for (; *l; l++)
	{
		if (q)
		{
			if (*l == '\\' && l[1])
				l++;
			else if (*l == q)
				q = 0;
		}
		else if (*l == '"' || *l == '\'')
			q = *l;
		else if (*l == '/' && l[1] == '/')
			break ;
		else if (*l == '{')
		{
			d++;
			*seen = 1;
		}
		else if (*l == '}')
			d--;
	}
	return (d);
}

/* the test's lines (at most 18), in *out; returns 0 if unreadable */
static int	test_source(const t_test *t, t_sb *out, int *macro)
{
	FILE	*f = fopen(t->file, "r");
	char	line[512];
	int		n = 0;
	int		depth = 0;
	int		seen = 0;
	int		kept = 0;

	*macro = 0;
	if (!f)
		return (0);
	while (fgets(line, sizeof(line), f) && ++n < t->line)
		;
	if (n != t->line)
	{
		fclose(f);
		return (0);
	}
	if (!is_test_line(line))
	{
		/* generated by a macro (IS_TESTS, FILE_TEST, INT_SUITE...) */
		*macro = 1;
		line[strcspn(line, "\n")] = 0;
		sb_add(out, "%s\n", line);
		fclose(f);
		return (1);
	}
	do
	{
		line[strcspn(line, "\n")] = 0;
		for (char *p = line; *p; p++)
			if (*p == '\t')
				*p = ' ';
		if (kept < 18)
			sb_add(out, "%s\n", line);
		else if (kept == 18)
			sb_add(out, "…\n");
		kept++;
		depth += brace_delta(line, &seen);
		if (seen && depth <= 0)
			break ;
	} while (fgets(line, sizeof(line), f) && kept < 60);
	fclose(f);
	return (1);
}

/* PICK(ft_x, r_x)(args) in the source -> "ft_x(args)" */
static int	call_from_pick(const char *src, char *out, size_t osz)
{
	const char	*p = strstr(src, "PICK(");
	const char	*name;
	size_t		nl;
	int			depth;
	const char	*a;

	if (!p)
		return (0);
	name = p + 5;
	while (*name == ' ')
		name++;
	nl = strcspn(name, ", )");
	p = name + nl;
	depth = 1;
	while (*p && depth)
	{
		if (*p == '(')
			depth++;
		else if (*p == ')')
			depth--;
		p++;
	}
	if (*p != '(')
		return (0);
	a = p;
	depth = 0;
	do
	{
		if (*p == '(')
			depth++;
		else if (*p == ')')
			depth--;
		p++;
	} while (*p && depth);
	if (depth)
		return (0);
	snprintf(out, osz, "%.*s%.*s", (int)nl, name, (int)(p - a), a);
	return (1);
}

/* first call of the student's function in the test source:
   "p = ft_calloc(4, 8);" -> "ft_calloc(4, 8)"                             */
static int	call_from_source(const char *src, char *out, size_t osz)
{
	static const char	*prefixes[] = {"ft_", "get_next_line(", NULL};

	for (int i = 0; prefixes[i]; i++)
	{
		const char	*p = src;

		while ((p = strstr(p, prefixes[i])))
		{
			const char	*q = p;
			int			depth = 0;

			if (p > src && (isalnum((unsigned char)p[-1]) || p[-1] == '_'))
			{
				p++;
				continue ;
			}
			while (isalnum((unsigned char)*q) || *q == '_')
				q++;
			if (*q != '(')
			{
				p = q;
				continue ;
			}
			do
			{
				if (*q == '(')
					depth++;
				else if (*q == ')')
					depth--;
				else if (*q == '\n')
					break ;
				q++;
			} while (*q && depth);
			if (!depth)
			{
				snprintf(out, osz, "%.*s", (int)(q - p), p);
				return (1);
			}
			p = q;
		}
	}
	return (0);
}

/* INPUT that already is a C call: "ft_printf(\"%d\", 42)" */
static int	call_from_input(const char *in, char *out, size_t osz)
{
	const char	*p = in;
	int			depth = 0;

	if (!in || !*in || has(in, "·") || has(in, "(sans") || has(in, "gardé")
		|| has(in, "guarded") || has(in, "[") || has(in, ", fd)"))
		return (0);
	if (!(isalpha((unsigned char)*p) || *p == '_'))
		return (0);
	while (isalnum((unsigned char)*p) || *p == '_')
		p++;
	if (*p != '(')
		return (0);
	for (const char *q = p; *q; q++)
	{
		if (*q == '(')
			depth++;
		else if (*q == ')' && --depth == 0 && q[1])
			return (0);
	}
	if (depth)
		return (0);
	snprintf(out, osz, "%s", in);
	return (1);
}

static const char	*short_val(const char *v, char *buf, size_t sz)
{
	if (!v || !*v)
		return ("?");
	snprintf(buf, sz, "%.*s%s", 40, v, strlen(v) > 40 ? "…" : "");
	return (buf);
}

static void	code_block(t_sb *b, const char *label, const char *text)
{
	const char	*p = text;
	int			first = 1;

	while (*p)
	{
		size_t	l = strcspn(p, "\n");

		sb_add(b, "        %s%-9s%s %s│%s %.*s\n", C_GRY, first ? label : "",
			C_RST, C_PHD, C_RST, (int)l, p);
		first = 0;
		p += l + (p[l] == '\n');
	}
}

/* main.c to paste in your own project */
static void	main_block(t_sb *b, const char *call, const t_shm *s,
				const char *exp, const char *got)
{
	t_sb	m = {0};
	char	e[64];
	char	g[64];
	int		gnl = !strcmp(g_proj.header, "get_next_line.h");

	if (!exp && s && s->failed)
		exp = s->exp;
	if (!got && s && s->failed)
		got = s->got;
	sb_add(&m, "#include \"%s\"\n#include <stdio.h>\n", g_proj.header);
	if (gnl)
		sb_add(&m, "#include <fcntl.h>\n");
	sb_add(&m, "\nint\tmain(void)\n{\n");
	if (gnl)
	{
		sb_add(&m, "\tint\t\tfd = open(\"test.txt\", O_RDONLY);  /* %s */\n"
			"\tchar\t*line;\n\n"
			"\twhile ((line = get_next_line(fd)))\n\t{\n"
			"\t\tprintf(\"[%%s]\", line);\n\t\tfree(line);\n\t}\n"
			"\treturn (0);\n}\n",
			L("mets-y le contenu du test", "put the test content in it"));
	}
	else
	{
		sb_add(&m, "\t%s;", call);
		if (exp && *exp)
			sb_add(&m, L("  /* attendu : %s · obtenu : %s */",
				"  /* expected: %s · actual: %s */"),
				short_val(exp, e, sizeof(e)), short_val(got, g, sizeof(g)));
		sb_add(&m, "\n");
		if (!strncmp(call, "ft_printf(", 10))
			sb_add(&m, "\t%s;  /* %s */\n", call + 3,
				L("la vraie, pour comparer", "the real one, to compare"));
		sb_add(&m, "\treturn (0);\n}\n");
	}
	for (char *p = m.s; p && *p; p++)
		if (*p == '\t')
			*p = ' ';
	code_block(b, "main.c", m.s);
	if (gnl)
		sb_add(b, "        %-9s %scc -g -D BUFFER_SIZE=42 main.c "
			"get_next_line.c get_next_line_utils.c && valgrind ./a.out%s\n",
			"", C_GRY, C_RST);
	else
		sb_add(b, "        %-9s %scc -g main.c %s && valgrind ./a.out%s\n", "",
			C_GRY, g_proj.archive, C_RST);
	free(m.s);
}

/* token understood by `torture <project> <dir> --repro TOKEN` */
static void	repro_block(t_sb *b, const t_test *t)
{
	const char	*label = NULL;
	char		tok[256];
	size_t		n;

	if (!o.repro_cmd || t->idx < 0)
		return ;
	for (int i = 0; i < NFUNCS; i++)
		if (!strcmp(g_funcs[i].name, t->func))
			label = g_funcs[i].label;
	n = (size_t)snprintf(tok, sizeof(tok), "%s#%d", base_name(t->file), t->idx);
	if (g_fail_k)
		n += (size_t)snprintf(tok + n, sizeof(tok) - n, "m%d", g_fail_k);
	n += (size_t)snprintf(tok + n, sizeof(tok) - n, "@%s", t->func);
	if (label && !strncmp(label, "BUFFER_SIZE=", 12))
		snprintf(tok + n, sizeof(tok) - n, "=%s", label + 12);
	else if (label && (has(label, "défaut") || has(label, "default")))
		snprintf(tok + n, sizeof(tok) - n, "=def");
	sb_add(b, "        %s%-9s%s %s%s --repro '%s'%s\n", C_GRY,
		L("rejouer", "replay"), C_RST, C_B, o.repro_cmd, tok, C_RST);
	sb_wrap(b, "", L("recompile ce test seul avec ton code (sans fork, malloc "
		"normal) et le lance sous valgrind si installé : la ligne fautive de "
		"ton code s'affiche. Ajoute --gdb pour le déboguer.",
		"rebuilds this single test with your code (no fork, plain malloc) "
		"and runs it under valgrind when installed: the faulty line of your "
		"code is shown. Add --gdb to debug it."), C_GRY);
}

static void	verbose_extra(t_sb *b, const t_test *t, const t_shm *s,
				const char *exp, const char *got)
{
	const char	*w = explain(t, s);
	t_sb		src = {0};
	int			macro = 0;
	char		call[600];
	int			has_call = 0;

	sb_add(b, "%c", DETAIL_SEP);
	if (w)
		sb_wrap(b, L("pourquoi", "why"), w, "");
	if (test_source(t, &src, &macro) && src.s)
	{
		code_block(b, L("test", "test"), src.s);
		if (macro)
			sb_add(b, "        %-9s %s%s%s\n", "", C_GRY, L("(test généré par "
				"une macro : sa définition est plus haut dans ce fichier)",
				"(test generated by a macro: its definition is higher in "
				"this file)"), C_RST);
	}
	if (s && call_from_input(s->input, call, sizeof(call)))
		has_call = 1;
	else if (src.s && call_from_pick(src.s, call, sizeof(call)))
		has_call = 1;
	else if (src.s && !macro && call_from_source(src.s, call, sizeof(call)))
		has_call = 1;
	if (has_call)
		sb_add(b, "        %s%-9s%s %s%s;%s\n", C_GRY, L("appel", "call"),
			C_RST, C_B, call, C_RST);
	if (has_call || !strcmp(g_proj.header, "get_next_line.h"))
		main_block(b, has_call ? call : "", s, exp, got);
	repro_block(b, t);
	free(src.s);
}

static char	*make_detail(const t_test *t, int idx, t_res res, const char *head,
			const t_shm *s, const char *exp, const char *got)
{
	t_sb		b = {0};
	const char	*col = res == R_SPEC ? C_YEL : C_RED;
	const char	*mark = res == R_SPEC ? "▲" : "✘";

	sb_add(&b, "      %s%s #%d%s %s« %s »%s  %s%s:%d%s\n", col, mark, idx, C_RST,
		C_B, localize(t->name), C_RST, C_GRY, base_name(t->file), t->line, C_RST);
	if (head && *head)
		sb_add(&b, "        %s%s%s\n", col, head, C_RST);
	if (s)
		detail_shm(&b, s, 1);
	detail_kv(&b, L("attendu", "expected"), exp, C_GRN);
	detail_kv(&b, L("obtenu", "actual"), got, C_RED);
	verbose_extra(&b, t, s, exp, got);
	why(W_NONE, 0, 0);
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
		why(W_SIG, a.sig, 0);
		r->detail = make_detail(t, idx, bad, sig_name(a.sig), &a.s, NULL, NULL);
	}
	else if (a.s.failed)
	{
		r->res = bad;
		set_glyph(r, a.s.leaks ? "L" : "✘");
		why(a.s.leaks ? W_LEAK : W_ASSERT, 0, 0);
		r->detail = make_detail(t, idx, bad, NULL, &a.s, NULL, NULL);
	}
	else if (!a.s.done)
	{
		r->res = bad;
		set_glyph(r, "E");
		why(W_EXIT, 0, 0);
		r->detail = make_detail(t, idx, bad,
				L("le processus s'est arrêté (exit() appelé ?)", "process stopped (exit() called?)"), &a.s, NULL, NULL);
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
		why(a.s.leaks ? W_LEAK : W_ASSERT, 0, 0);
		r->detail = make_detail(t, idx, R_FAIL, NULL, &a.s, NULL, NULL);
	}
	else if (fc && a.sig == SIGALRM)
	{
		r->res = R_FAIL;
		set_glyph(r, "T");
		why(W_VS_TIMEOUT, SIGALRM, 0);
		r->detail = make_detail(t, idx, R_FAIL, sig_name(a.sig), &a.s,
				rc ? L("(l'originale crash)", "(reference crashes)") : b.s.out, NULL);
	}
	else if (fc && rc)
	{
		r->res = R_PASS_CRASH;
		set_glyph(r, "✔");
	}
	else if (fc)
	{
		snprintf(head, sizeof(head),
			L("ta fonction crash (%s) alors que l'originale ne crash pas", "your function crashes (%s), but the reference does not"),
			sig_name(a.sig));
		r->res = R_FAIL;
		set_glyph(r, "S");
		why(W_VS_CRASH, a.sig, 0);
		r->detail = make_detail(t, idx, R_FAIL, head, &a.s,
				b.s.out[0] ? b.s.out : L("(pas de crash)", "(no crash)"), "CRASH");
	}
	else if (rc)
	{
		snprintf(head, sizeof(head),
			L("l'originale crash (%s) mais pas la tienne : protection non "
			"demandée par le sujet",
			"reference crashes (%s), but yours does not: protection "
			"not required by the subject"), sig_name(b.sig));
		r->res = R_SPEC;
		set_glyph(r, "▲");
		why(W_VS_SPEC, b.sig, 0);
		r->detail = make_detail(t, idx, R_SPEC, head, &a.s, "CRASH",
				a.s.out[0] ? a.s.out : L("(pas de crash)", "(no crash)"));
	}
	else if (strcmp(a.s.out, b.s.out) != 0)
	{
		r->res = t->spec ? R_SPEC : R_FAIL;
		set_glyph(r, t->spec ? "▲" : "✘");
		why(t->spec ? W_VS_DIFF_SPEC : W_VS_DIFF, 0, 0);
		r->detail = make_detail(t, idx, r->res,
				t->spec ? L("résultat différent de l'originale (comportement "
				"indéfini, spéculatif)",
				"result differs from reference (undefined behavior, speculative)")
				: L("résultat différent de l'originale",
				"result differs from the reference"),
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
					L("crash (%s) quand le malloc n°%d/%d renvoie NULL", "crash (%s) when malloc #%d/%d returns NULL"),
					sig_name(a.sig), k, n);
			else
				snprintf(head, sizeof(head),
					L("quand le malloc n°%d/%d renvoie NULL :", "when malloc #%d/%d returns NULL:"), k, n);
			r->res = R_FAIL;
			set_glyph(r, a.s.leaks ? "L" : "M");
			why(W_MALLOC, a.sig, k);
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
	/* Pixel "FTORTURE", phosphor green with scanlines. */
	static const int	scan[] = {46, 40, 46, 40, 46, 22};
	char				row[1024];
	int					i;

	printf("\n");
	for (i = 0; i < 6; i++)
	{
		pixel_row(row, sizeof(row), "FTORTURE", i, scan[i], 22);
		printf("   %s\n", row);
	}
	printf("   %s▚▚ %s ▞▞%s\n", C_PHM, L(g_proj.subtitle, "42 TESTER · LIBFT · FT_PRINTF · GET_NEXT_LINE · PUSH_SWAP"), C_RST);
	printf("   %s%s%s%s_%s\n\n", C_PH, L(g_proj.tagline, "> your code will talk"), C_RST,
		o.color ? "\033[5m\033[38;5;46m" : "", C_RST);
	printf("   %s%s%s %s · %s\n", C_PHD, L("projet", "project"), C_RST, g_proj.name, o.path);
	printf("   %smode  %s %s\n", C_PHD, C_RST, o.spec
		? L("strict (-s) : les ▲ comptent comme erreurs", "strict (-s): ▲ count as errors")
		: L("normal : ▲ = avertissements (-s pour les compter)", "normal: ▲ are warnings (-s to count them)"));
	printf("   %sseed  %s %u\n", C_PHD, C_RST, o.seed);
}

static void	legend(void)
{
	printf(L("\n   %sLégende%s  %s✔%s ok   %s✔%s crash identique à la libc   "
		"%s✘%s erreur   %sS%s crash\n            %sT%s timeout   %sL%s fuite   "
		"%sM%s malloc protégé   %s▲%s spéculatif\n",
		"\n   %sLegend%s  %s✔%s ok   %s✔%s same crash as libc   "
		"%s✘%s error   %sS%s crash\n            %sT%s timeout   %sL%s leak   "
		"%sM%s malloc protection   %s▲%s speculative\n"),
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
			sb_add(&protos, L("       %sattendu%s %s%s%s\n", "       %sexpected%s %s%s%s\n"), C_GRY, C_RST,
				C_GRN, exp, C_RST);
			sb_add(&protos, L("       %strouvé %s %s%s%s\n",
				"       %sfound %s %s%s%s\n"), C_GRY, C_RST,
				col, *found ? found : L("(non déclaré dans le header)", "(not declared in header)"), C_RST);
			if (g_log)
				fprintf(g_log, L("[PROTO %s] %s\n   attendu: %s\n   trouvé : %s\n",
					"[PROTO %s] %s\n   expected: %s\n   found: %s\n"),
					st, name, exp, found);
		}
		else if (!strcmp(kind, "PS"))
		{
			int	bad = g_ck.proto_tot - g_ck.proto_ok;

			printf(L("   %s%s%s %-24s %s%d/%d conformes%s\n", "   %s%s%s %-24s %s%d/%d matching%s\n"),
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

static int	in_list(const char *list, const char *w)
{
	const char	*p = list;
	size_t		l = strlen(w);

	if (!l)
		return (0);
	while ((p = strstr(p, w)))
	{
		if ((p == list || p[-1] == ',') && (p[l] == ',' || p[l] == 0))
			return (1);
		p += l;
	}
	return (0);
}

/* -f accepts "ft_split", "split", "pf_d" or the label ("%d", "flag -") */
static int	selected(const t_func *f)
{
	if (o.part && f->part != o.part)
		return (0);
	if (!o.only)
		return (1);
	return (in_list(o.only, f->name)
		|| (strlen(f->name) > 3 && in_list(o.only, f->name + 3))
		|| (f->label && in_list(o.only, f->label)));
}

/* ========================================================= efficiency === */
/* "Perfect" time: each workload declares the minimal work one repetition
   needs (COST: bytes scanned, copied, calls, mallocs, system calls…). The
   cost of each unit is measured once on THIS machine with the tightest
   loop that does it (compiled like the tester, -O0), best of 5. The
   perfect time is the sum, and
       efficiency = perfect time / your real time
   (100% = no wasted work; more is possible with -O2 or word-sized tricks).
   Your side runs in its own process, malloc not instrumented, calibrated
   to ~15 ms of repetitions, best time per repetition of 3 runs.            */
typedef struct s_bench
{
	const char	*func;
	void		(*fn)(long);
	void		(*score)(void);	/* SCORE(): not a time                   */
}	t_bench;

typedef struct s_bres
{
	char	label[40];
	int		pct;		/* -1: crashed / could not be measured            */
	int		bound;		/* 1: stopped by the time limit, real value lower  */
	double	ideal;		/* perfect seconds per repetition                  */
	double	real;		/* your seconds per repetition                     */
	int		score;		/* SCORE(): ideal and real are not seconds         */
}	t_bres;

typedef struct s_bshm
{
	double	tft;		/* seconds per repetition, yours                   */
	double	ft_start;	/* when your current run started (0: not running) */
	long	n;			/* workload repetitions per run                   */
	t_cost	cost;		/* declared by the workload                        */
	double	ideal;		/* SCORE(): ideal value                            */
}	t_bshm;

#define BENCH_MAX 64
#define BENCH_CALIB 0.015	/* seconds for one run of yours                  */
#define BENCH_LIMIT 2.0		/* seconds one run of yours may take            */
/* (a run of yours is at most ~15 ms, or ONE repetition when that is longer) */

static t_bench	g_benches[BENCH_MAX];
static int		g_nbench;
static t_bres	g_br[BENCH_MAX];
static int		g_nbr;
static t_bshm	*g_bshm;
volatile long	g_bench_sink;
t_cost			g_bench_cost;
static t_cost	g_hw;		/* seconds per unit on this machine            */
static int		g_hw_ok;

void	zft_bench_register(const char *func, void (*fn)(long),
			const char *file, int line)
{
	(void)file;
	(void)line;
	if (g_nbench < BENCH_MAX)
		g_benches[g_nbench++] = (t_bench){func, fn, NULL};
}

double	g_score_ideal;
double	g_score_real;

void	zft_score_register(const char *func, void (*fn)(void),
			const char *file, int line)
{
	(void)file;
	(void)line;
	if (g_nbench < BENCH_MAX)
		g_benches[g_nbench++] = (t_bench){func, NULL, fn};
}

const char	*zft_path(void)
{
	return (o.path);
}

int	zft_en(void)
{
	return (o.english);
}

static double	bnow(void)
{
	struct timespec	t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return ((double)t.tv_sec + (double)t.tv_nsec * 1e-9);
}

/* ------------------------------------------------- machine calibration -- */
typedef struct s_hnode
{
	struct s_hnode	*next;
	void			*content;
}	t_hnode;

static int	hw_id(int c)
{
	return (c);
}

static int	hw_range(int c)
{
	return (c >= 'a' && c <= 'z');
}

static int	(*volatile g_hw_fid)(int) = hw_id;
static int	(*volatile g_hw_frange)(int) = hw_range;

#define HW_N 4096		/* bytes: fits the L1 cache, like the workloads   */
#define HW_RN 65536		/* bytes of one read() to price a read byte       */
#define HW_NODES 1024

/* best of 20 of one primitive, in seconds per unit */
#define HW_BEST(dst, units, body) do { double zq_b = 1e9; int zq_k; \
	for (zq_k = 0; zq_k < 20; zq_k++) { double zq_t = bnow(); body; \
	zq_t = bnow() - zq_t; if (zq_t < zq_b) zq_b = zq_t; } \
	(dst) = zq_b / (double)(units); } while (0)

static void	hw_calibrate(void)
{
	static char		a[HW_RN + 1];
	static char		b[HW_N + 1];
	static char		d[HW_RN + 1];
	static t_hnode	nodes[HW_NODES];
	char			path[] = "/tmp/ftorture_hwXXXXXX";
	int				dn = open("/dev/null", O_WRONLY);
	int				fd;
	volatile long	k = 0;
	long			i;
	double			range;

	if (g_hw_ok)
		return ;
	memset(a, 'a', HW_RN);
	a[HW_N] = 0;
	memset(b, 'a', HW_N);
	for (i = 0; i < HW_NODES; i++)
		nodes[i].next = i + 1 < HW_NODES ? &nodes[i + 1] : NULL;
	/* each unit: the fastest of an index loop and a pointer loop, locals
	   only (the "perfect" student loop at -O0)                             */
	{
		const char	*pa = a;
		const char	*pb = b;
		char		*pd = d;
		int			(*fid)(int) = g_hw_fid;
		int			(*frange)(int) = g_hw_frange;
		double		v1;
		double		v2;

		HW_BEST(v1, HW_N, { size_t j = 0; while (pa[j]) j++; k = (long)j; });
		HW_BEST(v2, HW_N, { const char *q = pa; while (*q) q++; k = q - pa; });
		g_hw.scan = v1 < v2 ? v1 : v2;
		HW_BEST(v1, HW_N, { int j = 0; while (pa[j] != '#' && pa[j]) j++;
			k = j; });
		if (v1 < g_hw.scan)
			g_hw.scan = v1;
		HW_BEST(v1, HW_N, { size_t j = 0; while (pa[j]) { pd[j] = pa[j];
			j++; } });
		HW_BEST(v2, HW_N, { const char *q = pa; char *w = pd;
			while (*q) *w++ = *q++; });
		g_hw.copy = v1 < v2 ? v1 : v2;
		HW_BEST(v1, HW_N, { int j = 0; while (pa[j]) { pd[j] = pa[j]; j++; } });
		if (v1 < g_hw.copy)
			g_hw.copy = v1;
		HW_BEST(v1, HW_N, { size_t j = 0; while (j < HW_N) pd[j++] = 'x'; });
		HW_BEST(v2, HW_N, { char *w = pd; size_t m = HW_N;
			while (m--) *w++ = 'x'; });
		g_hw.store = v1 < v2 ? v1 : v2;
		HW_BEST(v1, HW_N, { int j = 0; while (j < HW_N) { pd[j] = 'x'; j++; } });
		if (v1 < g_hw.store)
			g_hw.store = v1;
		HW_BEST(v1, HW_N, { size_t j = 0; while (pa[j] && pa[j] == pb[j])
			j++; k = (long)j; });
		HW_BEST(v2, HW_N, { const char *x = pa; const char *y = pb;
			while (*x && *x == *y) { x++; y++; } k = x - pa; });
		g_hw.cmp = v1 < v2 ? v1 : v2;
		HW_BEST(v1, HW_N, { int j = 0; while (pa[j] && pa[j] == pb[j]) j++;
			k = j; });
		if (v1 < g_hw.cmp)
			g_hw.cmp = v1;
		/* a call or a comparison counts on top of the loop turn it sits in
		   (a node walk, a scan…): measured against an empty loop           */
		HW_BEST(g_hw.iters, HW_N, { long sum = 0; int j;
			for (j = 0; j < HW_N; j++) sum += j; k = sum; });
		HW_BEST(v1, HW_N, { long sum = 0; int j;
			for (j = 0; j < HW_N; j++) sum += fid(j); k = sum; });
		HW_BEST(range, HW_N, { long sum = 0; int j;
			for (j = 0; j < HW_N; j++) sum += frange(j); k = sum; });
		g_hw.ops = (range - v1) / 2;
		g_hw.calls = v1 - g_hw.iters;
	}
	if (g_hw.calls < g_hw.iters / 4)
		g_hw.calls = g_hw.iters / 4;
	if (g_hw.ops < g_hw.iters / 20)
		g_hw.ops = g_hw.iters / 20;
	HW_BEST(g_hw.digits, 8192 * 10, for (i = 0; i < 8192; i++) {
		long v = 1000000000L + i; int j = 0;
		while (v) { d[j++] = (char)('0' + v % 10); v /= 10; } });
	HW_BEST(g_hw.nodes, HW_NODES * 16, for (i = 0; i < 16; i++) {
		t_hnode *l = nodes; long c = 0; while (l) { c++; l = l->next; }
		k = c; });
	HW_BEST(g_hw.allocs, 4096, for (i = 0; i < 4096; i++) {
		void *p = malloc(64); k += (long)(p != NULL); free(p); });
	HW_BEST(g_hw.big_allocs, 64, for (i = 0; i < 64; i++) {
		void *p = malloc(10 << 20); k += (long)(p != NULL); free(p); });
	HW_BEST(g_hw.sys, 2048, for (i = 0; i < 2048; i++) k += write(dn, a, 1));
	fd = mkstemp(path);
	if (fd >= 0)
	{
		double	big;

		unlink(path);
		memset(a, 'a', HW_RN);
		k += write(fd, a, HW_RN);
		HW_BEST(big, 64, for (i = 0; i < 64; i++) k += pread(fd, d, HW_RN, 0));
		g_hw.rbytes = (big - g_hw.sys) / HW_RN;
		if (g_hw.rbytes < 0)
			g_hw.rbytes = 0;
		close(fd);
	}
	close(dn);
	g_hw_ok = 1;
	if (getenv("FTORTURE_BENCH_DEBUG"))
		fprintf(stderr, "[hw] iter %.3fns scan %.3fns copy %.3fns store %.3fns cmp %.3fns"
			" call %.3fns op %.3fns digit %.3fns node %.3fns malloc %.1fns"
			" bigmalloc %.0fns syscall %.0fns readbyte %.4fns\n",
			g_hw.iters * 1e9, g_hw.scan * 1e9, g_hw.copy * 1e9, g_hw.store * 1e9, g_hw.cmp * 1e9,
			g_hw.calls * 1e9, g_hw.ops * 1e9, g_hw.digits * 1e9,
			g_hw.nodes * 1e9, g_hw.allocs * 1e9, g_hw.big_allocs * 1e9,
			g_hw.sys * 1e9, g_hw.rbytes * 1e9);
}

static double	ideal_time(const t_cost *c)
{
	return (c->iters * g_hw.iters + c->calls * g_hw.calls + c->ops * g_hw.ops + c->scan * g_hw.scan
		+ c->copy * g_hw.copy + c->store * g_hw.store + c->cmp * g_hw.cmp
		+ c->digits * g_hw.digits + c->nodes * g_hw.nodes
		+ c->allocs * g_hw.allocs + c->big_allocs * g_hw.big_allocs
		+ c->sys * g_hw.sys + c->rbytes * g_hw.rbytes);
}

static double	bench_once(void (*fn)(long), long n)
{
	double	t;

	g_ref = 0;
	g_bshm->ft_start = bnow();
	t = bnow();
	fn(n);
	t = bnow() - t;
	g_bshm->ft_start = 0;
	return (t);
}

static const t_bench	*bench_for(const char *func)
{
	int	i;

	for (i = 0; i < g_nbench; i++)
		if (!strcmp(g_benches[i].func, func))
			return (&g_benches[i]);
	return (NULL);
}

/* measures one benchmark, stores and returns its result */
static t_bres	*run_bench(const t_bench *b, const char *label)
{
	t_bres	*r;
	pid_t	pid;
	int		st;
	double	start;
	double	ideal;

	if (g_nbr >= BENCH_MAX)
		return (NULL);
	r = &g_br[g_nbr++];
	memset(r, 0, sizeof(*r));
	copy_trunc(r->label, sizeof(r->label), label);
	r->pct = -1;
	r->score = b->score != NULL;
	if (!b->score)
		hw_calibrate();
	if (!g_bshm)
		g_bshm = mmap(NULL, sizeof(t_bshm), PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (g_bshm == MAP_FAILED)
		return (g_bshm = NULL, r);
	memset(g_bshm, 0, sizeof(*g_bshm));
	fflush(stdout);
	if (g_log)
		fflush(g_log);
	pid = fork();
	if (pid < 0)
		return (r);
	if (pid == 0)
	{
		int		dn = open("/dev/null", O_RDWR);
		long	n = 1;
		double	t;
		int		k;

		dup2(dn, 0);
		dup2(dn, 1);
		dup2(dn, 2);
		signal(SIGPIPE, SIG_IGN);
		g_zm_active = 0;
		g_fail_at = 0;
		g_armed = 0;
		zsrand(o.seed);
		if (b->score)
		{
			g_score_ideal = 0;
			g_score_real = 0;
			b->score();
			g_bshm->ideal = g_score_ideal;
			g_bshm->tft = g_score_real;
			g_bshm->n = 1;
			_exit(0);
		}
		/* warm-up: builds the fixtures and declares the cost */
		memset(&g_bench_cost, 0, sizeof(g_bench_cost));
		t = bench_once(b->fn, 1);
		g_bshm->cost = g_bench_cost;
		g_bshm->tft = t;
		while ((t = bench_once(b->fn, n)) < BENCH_CALIB && n < (1L << 30))
		{
			g_bshm->tft = t / (double)n;
			n = t > BENCH_CALIB / 64 ? (long)((double)n * BENCH_CALIB / t) + 1
				: n * 4;
		}
		g_bshm->tft = t / (double)n;
		g_bshm->n = n;
		for (k = 1; k < 3; k++)
			if ((t = bench_once(b->fn, n) / (double)n) < g_bshm->tft)
				g_bshm->tft = t;
		_exit(0);
	}
	start = bnow();
	while (1)
	{
		pid_t	w = waitpid(pid, &st, WNOHANG);
		double	fs = g_bshm->ft_start;

		if (w == pid || (w < 0 && errno != EINTR))
			break ;
		if ((!b->score && fs > 0 && bnow() - fs > BENCH_LIMIT)
			|| bnow() - start > (b->score ? 120.0 : 30.0))
		{
			kill(pid, SIGKILL);
			while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
				;
			if (!g_bshm->n && fs > 0)
			{
				g_bshm->tft = bnow() - fs;	/* one repetition takes more */
				r->bound = 1;
			}
			st = 0;
			break ;
		}
		usleep(2000);
	}
	ideal = b->score ? g_bshm->ideal : ideal_time(&g_bshm->cost);
	r->ideal = ideal;
	r->real = g_bshm->tft;
	if (getenv("FTORTURE_BENCH_DEBUG"))
		fprintf(stderr, "[bench] %s: n=%ld perfect=%.3fus yours=%.3fus%s\n",
			label, g_bshm->n, ideal * 1e6, g_bshm->tft * 1e6,
			r->bound ? " (limit)" : "");
	if ((r->bound || (WIFEXITED(st) && !WEXITSTATUS(st)))
		&& ideal > 0 && g_bshm->tft > 0)
	{
		double	p = ideal / g_bshm->tft * 100.0;

		r->pct = p > 9999 ? 9999 : p < 1 ? 1 : (int)(p + 0.5);
	}
	return (r);
}

static const char	*eff_col(int pct)
{
	if (pct < 0)
		return (C_GRY);
	if (pct >= o.eff_target)
		return (C_GRN);
	return (pct * 2 >= o.eff_target ? C_YEL : C_RED);
}

static void	print_eff(const t_bres *r)
{
	if (!r)
		return ;
	if (r->pct < 0)
		printf(L("  %s⚡ non mesurée (crash)%s", "  %s⚡ not measured (crash)%s"),
			C_GRY, C_RST);
	else
		printf("  %s⚡ %s%d%%%s", eff_col(r->pct), r->bound ? "<" : "",
			r->pct, C_RST);
	if (g_log)
	{
		if (r->pct < 0)
			fprintf(g_log, L("%s : efficacité non mesurée (crash)\n",
				"%s: efficiency not measured (crash)\n"), r->label);
		else if (r->score)
			fprintf(g_log, L("%s : efficacité %d%% (objectif %d%%, référence "
				"%.0f, le tien %.0f)\n", "%s: efficiency %d%% (target %d%%, "
				"reference %.0f, yours %.0f)\n"), r->label, r->pct,
				o.eff_target, r->ideal, r->real);
		else
			fprintf(g_log, L("%s : efficacité %s%d%% (objectif %d%%, temps "
				"parfait %.2f µs, le tien %s%.2f µs)\n", "%s: efficiency %s%d%% "
				"(target %d%%, perfect time %.2f µs, yours %s%.2f µs)\n"),
				r->label, r->bound ? "<" : "", r->pct, o.eff_target,
				r->ideal * 1e6, r->bound ? ">" : "", r->real * 1e6);
	}
}

/* benchmarks attached to no function of the table (ft_printf): one line */
static void	run_project_benches(void)
{
	int		i;
	int		j;
	char	lbl[64];

	if (!o.eff)
		return ;
	for (i = 0; i < g_nbench; i++)
	{
		for (j = 0; j < NFUNCS; j++)
			if (!strcmp(g_funcs[j].name, g_benches[i].func))
				break ;
		if (j < NFUNCS)
			continue ;
		char	pad[96];

		snprintf(lbl, sizeof(lbl), "%s%s", g_benches[i].func,
			o.part == 2 ? " (bonus)" : "");
		padv(pad, sizeof(pad), lbl, 16);
		printf("\n   %s%s%s", C_B, pad, C_RST);
		print_eff(run_bench(&g_benches[i], lbl));
		printf("\n");
	}
}

/* gauge ▕████░░┃░░▏ with the target mark */
static void	eff_gauge(char *out, size_t osz, int pct, int width)
{
	int		f = (pct > 100 ? 100 : pct) * width / 100;
	int		m = o.eff_target * width / 100;
	size_t	j = 0;
	int		i;

	if (m >= width)
		m = width - 1;
	out[0] = 0;
	for (i = 0; i < width && j + 32 < osz; i++)
	{
		if (i == m)
			j += (size_t)snprintf(out + j, osz - j, "%s%s%s", C_YEL,
				i < f ? "▓" : "┃", C_RST);
		else
			j += (size_t)snprintf(out + j, osz - j, "%s%s%s",
				i < f ? eff_col(pct) : C_PHD, i < f ? "█" : "░", C_RST);
	}
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
	{
		char	lbl[64];

		padv(lbl, sizeof(lbl), f->label ? f->label : f->name, 16);
		printf("   %s%s%s ", C_B, lbl, C_RST);
	}
	if (!f->addr)
	{
		fs->missing = 1;
		printf(L("%s✘ ABSENTE de %s%s %s(%d tests ignorés)%s\n", "%s✘ MISSING from %s%s %s(%d tests skipped)%s\n"), C_BGR,
			g_proj.archive, C_RST, C_GRY, fs->total, C_RST);
		if (g_log)
			fprintf(g_log, L("\n%s : ABSENTE\n", "\n%s : MISSING\n"),
				f->label ? f->label : f->name);
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
		if (o.spec_pass && r->res == R_FAIL)
			r->res = R_SPEC;
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
		if (o.eff && bench_for(f->name))
		{
			char	bl[64];
			int		k;
			int		dup = 0;

			/* gnl: the bonus suites have the same labels as the mandatory */
			for (k = 0; k < NFUNCS; k++)
				if (k != fi && g_funcs[k].part != f->part && f->label
					&& g_funcs[k].label && !strcmp(g_funcs[k].label, f->label))
					dup = 1;
			snprintf(bl, sizeof(bl), "%s%s", f->label ? f->label : f->name,
				dup && f->part == 2 ? " (bonus)" : "");
			print_eff(run_bench(bench_for(f->name), bl));
		}
		printf("\n");
	}
	if (g_log)
		fprintf(g_log, L("\n%s : %d/%d (%d spéculatif)\n",
			"\n%s : %d/%d (%d speculative)\n"),
			f->label ? f->label : f->name, fs->pass,
			fs->total, fs->spec);
	for (i = 0; i < end - start; i++)
	{
		if (!res[i].detail)
			continue ;
		{
			char	*sep = strchr(res[i].detail, DETAIL_SEP);

			if (sep)
				*sep = 0;
			log_plain(res[i].detail);
			if (sep)
				log_plain(sep + 1);
			if (o.verbose || (shown < o.max_details && (res[i].res == R_FAIL
						|| o.spec)))
			{
				fputs(res[i].detail, stdout);
				if (sep && o.verbose)
					fputs(sep + 1, stdout);
				shown++;
				if (sep)
					*sep = DETAIL_SEP;
				free(res[i].detail);
				continue ;
			}
			if (sep)
				*sep = DETAIL_SEP;
		}
		if (res[i].res == R_SPEC && shown < o.max_details + 2)
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
			printf(L("      %s… %d autre(s) détail(s) : relance avec -v ou "
				"regarde %s%s\n", "      %s… %d more details: rerun with -v or "
				"see %s%s\n"), C_GRY, hidden - shown,
				o.log ? o.log : L("le log", "the log"), C_RST);
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

/* summary lines: overall gauge + the slowest functions */
static void	eff_summary(void)
{
	double	sum = 0;
	int		n = 0;
	int		i;
	int		bw;
	int		pct;
	char	g[2048];
	char	slow[512];
	int		idx[BENCH_MAX];
	int		ns = 0;

	if (!o.eff && !g_nbr)
		return ;
	for (i = 0; i < g_nbr; i++)
	{
		if (g_br[i].pct < 0)
			continue ;
		/* geometric mean, each value kept in [1%, 200%]: one very fast
		   function cannot hide a slow one                                  */
		sum += log((double)(g_br[i].pct > 200 ? 200 : g_br[i].pct));
		n++;
		if (g_br[i].pct < o.eff_target)
			idx[ns++] = i;
	}
	box_line("%s", "");
	if (!n)
	{
		box_line(L("%sEFFICACITÉ%s %saucune mesure%s", "%sEFFICIENCY%s %sno measurement%s"),
			C_PHD, C_RST, C_GRY, C_RST);
		return ;
	}
	pct = (int)(exp(sum / n) + 0.5);
	{
		char	tl[64];

		/* "EFFICACITÉ" + 9 spaces + ▕▏ + " 100%  " + the target label */
		snprintf(tl, sizeof(tl), L("objectif %d%%", "target %d%%"), o.eff_target);
		bw = g_inner - 2 - 28 - vis_len(tl);
	}
	if (bw < 6)
		bw = 6;
	eff_gauge(g, sizeof(g), pct, bw);
	box_line(L("%sEFFICACITÉ%s         %s▕%s%s▏%s %s%3d%%%s  %sobjectif %d%%%s", "%sEFFICIENCY%s         %s▕%s%s▏%s %s%3d%%%s  %starget %d%%%s"),
		C_PH, C_RST, C_PHD, g, C_PHD, C_RST, eff_col(pct), pct, C_RST,
		C_YEL, o.eff_target, C_RST);
	/* the slowest first */
	for (i = 1; i < ns; i++)
	{
		int	k = idx[i];
		int	j = i - 1;

		while (j >= 0 && g_br[idx[j]].pct > g_br[k].pct)
		{
			idx[j + 1] = idx[j];
			j--;
		}
		idx[j + 1] = k;
	}
	if (ns)
	{
		size_t	l = 0;

		int		room = g_inner - 2 - 22;	/* after "  sous l'objectif : " */
		char	item[160];

		slow[0] = 0;
		for (i = 0; i < ns; i++)
		{
			snprintf(item, sizeof(item), "%s%s%s %s%s%d%%%s", i ? " · " : "",
				C_B, g_br[idx[i]].label, eff_col(g_br[idx[i]].pct),
				g_br[idx[i]].bound ? "<" : "", g_br[idx[i]].pct, C_RST);
			if (vis_len(slow) + vis_len(item) + (i + 1 < ns ? 2 : 0) > room)
			{
				if (i)
					l += (size_t)snprintf(slow + l, sizeof(slow) - l, " …");
				break ;
			}
			l += (size_t)snprintf(slow + l, sizeof(slow) - l, "%s", item);
		}
		box_line(L("%s  sous l'objectif :%s %s", "%s  below target:%s %s"), C_PHD, C_RST, slow);
	}
	else
		box_line(L("%s  %d/%d mesure(s) au-dessus de l'objectif%s", "%s  %d/%d measurement(s) above target%s"),
			C_GRN, n, n, C_RST);
	for (i = 0; i < g_nbr && g_br[i].score; i++)
		;
	if (g_hw_ok && i < g_nbr)
		box_line(L("%s  coûts mesurés ici : octet %.1f ns · malloc %.0f ns · "
			"syscall %.0f ns%s", "%s  costs measured here: byte %.1f ns · "
			"malloc %.0f ns · syscall %.0f ns%s"),
			C_PHD, g_hw.scan * 1e9, g_hw.allocs * 1e9, g_hw.sys * 1e9, C_RST);
	if (g_log && g_hw_ok)
		fprintf(g_log, L("machine : tour de boucle %.2f ns, octet lu %.2f ns, "
			"copié %.2f ns, écrit %.2f ns, comparé %.2f ns, appel +%.2f ns, "
			"comparaison +%.2f ns, chiffre %.1f ns, maillon %.2f ns, malloc "
			"%.1f ns, gros malloc %.0f ns, syscall %.0f ns, octet lu par read "
			"%.3f ns\n", "machine: loop turn %.2f ns, byte scanned %.2f ns, "
			"copied %.2f ns, stored %.2f ns, compared %.2f ns, call +%.2f ns, "
			"comparison +%.2f ns, digit %.1f ns, node %.2f ns, malloc %.1f ns, "
			"big malloc %.0f ns, syscall %.0f ns, byte read() %.3f ns\n"),
			g_hw.iters * 1e9, g_hw.scan * 1e9, g_hw.copy * 1e9, g_hw.store * 1e9,
			g_hw.cmp * 1e9, g_hw.calls * 1e9, g_hw.ops * 1e9, g_hw.digits * 1e9,
			g_hw.nodes * 1e9, g_hw.allocs * 1e9, g_hw.big_allocs * 1e9,
			g_hw.sys * 1e9, g_hw.rbytes * 1e9);
	if (g_log)
		fprintf(g_log, L("EFFICACITÉ: %d%% (objectif %d%%, %d mesure(s), "
			"%d sous l'objectif)\n", "EFFICIENCY: %d%% (target %d%%, "
			"%d measurement(s), %d below target)\n"), pct, o.eff_target, n,
			ns);
}

static int	summary(void)
{
	int					p;
	int					i;
	int					tp = 0, tf = 0, ts = 0, tce = 0, tt = 0;
	int					fok = 0, fall = 0, miss = 0;
	int					bad;
	char				row[1024];

	g_inner = term_width() - 6;
	if (g_inner > 76)
		g_inner = 76;
	printf("\n   %s█▀▀▀%s %s %s", C_PHM, C_INV, L("RÉSULTAT", "RESULT"), C_RST);
	printf("%s", C_PHM);
	rep("▀", g_inner - 5 - vis_len(L("RÉSULTAT", "RESULT")));
	printf("█%s\n", C_RST);
	box_line("%s", "");
	for (p = 1; p <= g_proj.nparts; p++)
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
			padv(lbl, sizeof(lbl), g_proj.short_names[p], 16);
			box_line("%sP%d%s %s %s▕%s%s▏%s %s%2d/%-2d%s  %3d%%", C_PH, p,
				C_RST, lbl, C_PHD, mb, C_PHD, C_RST, ok == all ? C_PH : C_RED,
				ok, all, C_RST, ok * 100 / all);
		}
	}
	if (o.note)
		box_line("%s%s%s", C_YEL, o.note, C_RST);
	box_line("%s", "");
	box_line("%sTESTS %s %s%d ✔%s  %s%d ✘%s  %s%d ▲%s  %s/ %d%s", C_PHD, C_RST,
		C_GRN, tp, C_RST, tf ? C_RED : C_PHD, tf, C_RST, ts ? C_YEL : C_PHD, ts,
		C_RST, C_PHD, tt, C_RST);
	if (tce)
		box_line(L("%s       dont %d crash identique(s) à la libc (acceptés)%s", "%s       including %d libc-matching crashes (accepted)%s"),
			C_CYN, tce, C_RST);
	if (miss)
		box_line("%sABSENT%s %s%d fonction(s)%s", C_PHD, C_RST, C_RED, miss,
			C_RST);
	if (o.checks || g_ck.ok || g_ck.ko || g_ck.proto_tot)
		box_line("%sCHECKS%s %s%d ✔%s  %s%d ✘%s  %s%d ▲%s  %sprotos %d/%d%s",
			C_PHD, C_RST, C_GRN, g_ck.ok, C_RST, g_ck.ko ? C_RED : C_PHD,
			g_ck.ko, C_RST, g_ck.warn ? C_YEL : C_PHD, g_ck.warn, C_RST,
			g_ck.proto_ok == g_ck.proto_tot ? C_GRN : C_RED, g_ck.proto_ok,
			g_ck.proto_tot, C_RST);
	eff_summary();
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
		box_center(L("%s%d/%d fonctions parfaites · aucune erreur%s", "%s%d/%d perfect functions · no errors%s"), C_PH, fok,
			fall, C_RST);
	else
		box_center(L("%s%d problème(s) · %d/%d fonctions OK%s", "%s%d problems · %d/%d functions OK%s"), C_RED, bad, fok,
			fall, C_RST);
	if (!o.spec && ts)
		box_center(L("%s%d avertissement(s) ▲ à savoir défendre%s", "%s%d speculative ▲ warnings to review%s"), C_YEL, ts,
			C_RST);
	box_line("%s", "");
	printf("   %s█", C_PHM);
	rep("▄", g_inner);
	printf("█%s\n", C_RST);
	if (o.log)
		printf(L("   %sDétails complets : %s%s\n", "   %sFull details: %s%s\n"), C_GRY, o.log, C_RST);
	printf("\n");
	if (g_log)
		fprintf(g_log, L("\nTOTAL: %d ok, %d ko, %d spéculatifs, %d absentes, "
			"%d vérifs KO -> %s\n",
			"\nTOTAL: %d ok, %d failed, %d speculative, %d missing, "
			"%d failed checks -> %s\n"), tp, tf, ts, miss, g_ck.ko,
			bad ? "KO" : "OK");
	return (bad ? 1 : 0);
}

/* ------------------------------------------ chained runs (mandatory, bonus) */
static void	stats_save(const char *path)
{
	FILE	*f = fopen(path, "wb");

	if (!f)
		return ;
	fwrite(g_fs, sizeof(g_fs), 1, f);
	fwrite(&g_ck, sizeof(g_ck), 1, f);
	fwrite(&g_nbr, sizeof(g_nbr), 1, f);
	fwrite(g_br, sizeof(t_bres), (size_t)g_nbr, f);
	fwrite(&g_hw_ok, sizeof(g_hw_ok), 1, f);
	fwrite(&g_hw, sizeof(g_hw), 1, f);
	fclose(f);
}

static void	stats_load(const char *path)
{
	FILE	*f = fopen(path, "rb");

	if (!f)
		return ;
	if (fread(g_fs, sizeof(g_fs), 1, f) != 1
		|| fread(&g_ck, sizeof(g_ck), 1, f) != 1)
	{
		memset(g_fs, 0, sizeof(g_fs));
		memset(&g_ck, 0, sizeof(g_ck));
	}
	else if (fread(&g_nbr, sizeof(g_nbr), 1, f) != 1 || g_nbr < 0
		|| g_nbr > BENCH_MAX
		|| fread(g_br, sizeof(t_bres), (size_t)g_nbr, f) != (size_t)g_nbr)
		g_nbr = 0;
	else if (fread(&g_hw_ok, sizeof(g_hw_ok), 1, f) != 1
		|| fread(&g_hw, sizeof(g_hw), 1, f) != 1)
		g_hw_ok = 0;
	fclose(f);
}

static int	run_status(void)
{
	int	i;

	for (i = 0; i < NFUNCS; i++)
		if (g_fs[i].ran && (g_fs[i].missing || g_fs[i].fail
				|| (o.spec && g_fs[i].spec)))
			return (1);
	return (g_ck.ko ? 1 : 0);
}

static void	usage(void)
{
	if (o.english)
	{
		printf("usage: ftorture_tester [options]\n"
			"  -s, --speculative   count speculative ▲ failures\n"
			"  -v, --verbose       show all failure details\n"
			"  -f, --func LIST     filter functions\n"
			"  -p, --part N        test part N\n"
			"  -e, --efficiency [N] compare each function with a perfect time\n"
			"                      computed for this machine, target N%% (default 50)\n"
			"  -t, --target N      efficiency target in %%, turns -e on\n"
			"      --lang en|fr    output language\n"
			"      --no-color     disable colors\n");
		return ;
	}
	printf("usage: ftorture_tester [options]\n"
		"  -s, --speculative  les erreurs spéculatives (▲) comptent\n"
		"  -v, --verbose      affiche tous les détails\n"
		"  -f, --func LIST    ne teste que ces fonctions (ft_split,itoa,...)\n"
		"  -p, --part N       ne teste que la partie N (1, 2 ou 3)\n"
		"      --checks FILE  résultats des vérifications (généré par torture)\n"
		"      --log FILE     écrit un rapport sans couleurs\n"
		"      --path PATH    chemin affiché du projet\n"
		"      --no-summary, --no-legend, --note TEXT\n"
		"      --stats-out FILE, --stats-in FILE (passes chaînées)\n"
		"      --seed N       graine des tests aléatoires\n"
		"      --timeout S    timeout par test (défaut 3s)\n"
		"  -e, --efficiency [N] compare chaque fonction à un temps parfait\n"
		"                     calculé pour cette machine, objectif N%% (défaut 50)\n"
		"  -t, --target N     objectif d'efficacité en %%, active -e\n"
		"      --lang en|fr   langue de sortie\n"
		"      --no-color\n");
}

int	main(int ac, char **av)
{
	int	i;
	int	ti;
	int	part;
	const char *locale;

	locale = getenv("LC_ALL");
	if (!locale || !*locale) locale = getenv("LC_MESSAGES");
	if (!locale || !*locale) locale = getenv("LANG");
	o.english = (!locale || strncmp(locale, "fr", 2));
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
		else if (!strcmp(a, "-e") || !strcmp(a, "--efficiency"))
		{
			o.eff = 1;
			if (i + 1 < ac && av[i + 1][0] >= '0' && av[i + 1][0] <= '9')
				o.eff_target = atoi(av[++i]);
			if (o.eff_target < 1 || o.eff_target > 100)
				o.eff_target = 50;
		}
		else if ((!strcmp(a, "-t") || !strcmp(a, "--target")) && i + 1 < ac)
		{
			o.eff = 1;
			o.eff_target = atoi(av[++i]);
			if (o.eff_target < 1 || o.eff_target > 100)
				o.eff_target = 50;
		}
		else if (!strcmp(a, "--lang") && i + 1 < ac)
		{
			const char *lang = av[++i];
			if (strcmp(lang, "en") && strcmp(lang, "fr"))
				return (fprintf(stderr, "Use --lang en or --lang fr\n"), 2);
			o.english = !strcmp(lang, "en");
		}
		else if (!strcmp(a, "--no-banner"))
			o.no_banner = 1;
		else if (!strcmp(a, "--no-summary"))
			o.no_summary = 1;
		else if (!strcmp(a, "--no-legend"))
			o.no_legend = 1;
		else if (!strcmp(a, "--stats-out") && i + 1 < ac)
			o.stats_out = av[++i];
		else if (!strcmp(a, "--stats-in") && i + 1 < ac)
			o.stats_in = av[++i];
		else if (!strcmp(a, "--note") && i + 1 < ac)
			o.note = av[++i];
		else if (!strcmp(a, "--spec-pass"))
			o.spec_pass = 1;
		else if (!strcmp(a, "--no-section"))
			o.no_section = 1;
		else if (!strcmp(a, "--name") && i + 1 < ac)
			o.names = av[++i];
		else if (!strcmp(a, "--repro-cmd") && i + 1 < ac)
			o.repro_cmd = av[++i];
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
	localize_project();
	g_shm = mmap(NULL, sizeof(t_shm), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (g_shm == MAP_FAILED)
		return (perror("mmap"), 2);
	if (o.log)
		g_log = fopen(o.log, o.stats_in ? "a" : "w");
	if (o.stats_in)
		stats_load(o.stats_in);
	if (o.names)
	{
		int	k = 0;

		/* raw (untranslated) test names containing one of the comma items */
		for (i = 0; i < g_ntests; i++)
		{
			const char	*p = o.names;
			int			keep = 0;

			while (*p && !keep)
			{
				size_t	l = strcspn(p, ",");
				char	w[256];

				snprintf(w, sizeof(w), "%.*s", (int)(l < 255 ? l : 255), p);
				keep = l && strstr(g_tests[i].name, w) != NULL;
				p += l + (p[l] == ',');
			}
			if (keep)
				g_tests[k++] = g_tests[i];
		}
		g_ntests = k;
	}
	qsort(g_tests, (size_t)g_ntests, sizeof(t_test), cmp_tests);
	if (!o.no_banner)
		banner();
	print_checks();
	if (!o.no_legend)
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
			if (any && !o.no_section)
			{
				snprintf(title, sizeof(title), "%s %d · %s", L("PARTIE", "PART"), part,
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
	run_project_benches();
	if (o.stats_out)
		stats_save(o.stats_out);
	i = o.no_summary ? run_status() : summary();
	if (g_log)
		fclose(g_log);
	return (i);
}
