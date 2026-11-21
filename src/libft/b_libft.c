/* ************************************************************************** */
/*  Efficiency workloads (--efficiency).                                      */
/*  Each BENCH runs one workload zb_n times with YOUR function, and COST()   */
/*  declares the minimal work one repetition needs: what a perfect           */
/*  implementation cannot avoid (bytes it must read, copy or write, calls,   */
/*  mallocs, system calls). The engine prices each unit on this machine and  */
/*  compares that "perfect" time with your real time.                        */
/*  The harness work inside the loop (callbacks, frees) is counted too.      */
/* ************************************************************************** */
#include <fcntl.h>
#include "zft.h"
#include "libft_decl.h"

#define LEN 1000

static char		g_s[LEN + 1];		/* "abcd…" (LEN chars)                  */
static char		g_t[LEN + 1];		/* same content, other buffer           */
static char		g_d[2 * LEN + 2];	/* destination                          */
static char		g_words[LEN + 1];	/* "  word word  word…"                 */
static char		g_trim[LEN + 1];	/* 100 × "xy…", 800 letters, 100 × "xy" */
static int		g_nwords;
static int		g_nletters;
static int		g_null = -1;		/* /dev/null                            */
static t_list	g_nodes[LEN];
static t_list	*g_list;			/* LEN nodes, built once                */

static void	init(void)
{
	static int	done;
	int			i;

	if (done)
		return ;
	done = 1;
	for (i = 0; i < LEN; i++)
	{
		g_s[i] = (char)('a' + i % 26);
		g_words[i] = (i % 7 < 2) ? ' ' : (char)('a' + i % 26);
		if (g_words[i] != ' ')
		{
			g_nletters++;
			if (i == 0 || g_words[i - 1] == ' ')
				g_nwords++;
		}
		g_trim[i] = (i < 100 || i >= LEN - 100) ? "xy"[i % 2]
			: (char)('A' + i % 26);
		g_nodes[i].content = &g_nodes[i];
		g_nodes[i].next = i + 1 < LEN ? &g_nodes[i + 1] : NULL;
	}
	memcpy(g_t, g_s, sizeof(g_s));
	g_list = &g_nodes[0];
	g_null = open("/dev/null", O_WRONLY);
}

/* ------------------------------------------------------------- helpers -- */
static char	upper_i(unsigned int i, char c)
{
	(void)i;
	return (c >= 'a' && c <= 'z' ? (char)(c - 32) : c);
}

static void	upper_p(unsigned int i, char *c)
{
	(void)i;
	if (*c >= 'a' && *c <= 'z')
		*c = (char)(*c - 32);
}

static void	nodel(void *p)
{
	(void)p;
}

static void	touch(void *p)
{
	SINK(p != NULL);
}

static void	*same(void *p)
{
	return (p);
}

/* 257 calls (c = -1 .. 255), OPS comparisons each */
#define CHARS(F, OPS) BENCH(F) { init(); \
	COST(.iters = 257, .calls = 257, .ops = 257 * (OPS)); BENCH_LOOP { int c; \
	for (c = -1; c < 256; c++) SINK(F(c)); } }

/* --------------------------------------------------------------- part 1 -- */
CHARS(ft_isalpha, 2)
CHARS(ft_isdigit, 2)
CHARS(ft_isalnum, 4)
CHARS(ft_isascii, 2)
CHARS(ft_isprint, 2)
CHARS(ft_toupper, 2)
CHARS(ft_tolower, 2)

BENCH(ft_strlen)
{
	init();
	COST(.calls = 1, .scan = LEN);
	BENCH_LOOP SINK(ft_strlen(g_s));
}

BENCH(ft_memset)
{
	init();
	COST(.calls = 1, .store = LEN);
	BENCH_LOOP SINK(ft_memset(g_d, 'x', LEN));
}

BENCH(ft_bzero)
{
	init();
	COST(.calls = 1, .store = LEN);
	BENCH_LOOP ft_bzero(g_d, LEN);
}

BENCH(ft_memcpy)
{
	init();
	COST(.calls = 1, .copy = LEN);
	BENCH_LOOP SINK(ft_memcpy(g_d, g_s, LEN));
}

BENCH(ft_memmove)
{
	init();
	COST(.calls = 2, .copy = 2 * (LEN - 10));
	BENCH_LOOP
	{
		memcpy(g_d, g_s, LEN);
		SINK(ft_memmove(g_d + 10, g_d, LEN - 10));
		SINK(ft_memmove(g_d, g_d + 10, LEN - 10));
	}
}

BENCH(ft_strlcpy)
{
	init();
	COST(.calls = 1, .copy = LEN);
	BENCH_LOOP SINK(ft_strlcpy(g_d, g_s, sizeof(g_d)));
}

/* empty dst: copy LEN; then find the end (LEN) and copy LEN again */
BENCH(ft_strlcat)
{
	init();
	COST(.calls = 2, .scan = LEN, .copy = 2 * LEN);
	BENCH_LOOP
	{
		g_d[0] = 0;
		SINK(ft_strlcat(g_d, g_s, sizeof(g_d)));
		SINK(ft_strlcat(g_d, g_s, sizeof(g_d)));
	}
}

BENCH(ft_strchr)
{
	init();
	COST(.calls = 2, .scan = 2 * LEN);
	BENCH_LOOP
	{
		SINK(ft_strchr(g_s, '\0'));
		SINK(ft_strchr(g_s, '#'));
	}
}

/* the last 'a' is only known at the end: the whole string, once */
BENCH(ft_strrchr)
{
	init();
	COST(.calls = 2, .scan = 2 * LEN);
	BENCH_LOOP
	{
		SINK(ft_strrchr(g_s, 'a'));
		SINK(ft_strrchr(g_s, '#'));
	}
}

BENCH(ft_strncmp)
{
	init();
	COST(.calls = 1, .cmp = LEN);
	BENCH_LOOP SINK(ft_strncmp(g_s, g_t, LEN + 1));
}

BENCH(ft_memchr)
{
	init();
	COST(.calls = 1, .scan = LEN);
	BENCH_LOOP SINK(ft_memchr(g_s, '#', LEN));
}

BENCH(ft_memcmp)
{
	init();
	COST(.calls = 1, .cmp = LEN);
	BENCH_LOOP SINK(ft_memcmp(g_s, g_t, LEN));
}

/* one look per position, plus the short partial matches ("xyz", "uvwxyz") */
BENCH(ft_strnstr)
{
	init();
	COST(.calls = 2, .scan = 2 * LEN, .ops = 350);
	BENCH_LOOP
	{
		SINK(ft_strnstr(g_s, "xyz#", LEN));
		SINK(ft_strnstr(g_s, "uvwxyz", LEN));
	}
}

/* 49 characters read, 37 digits (one multiply-add each) */
BENCH(ft_atoi)
{
	static const char	*in[] = {"42", "   -2147483648", "+2147483647",
		"\t\n 123456abc", "0", "-17", "  999999"};

	init();
	COST(.iters = 7, .calls = 7, .scan = 49, .ops = 74);
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
			SINK(ft_atoi(in[k]));
	}
}

BENCH(ft_calloc)
{
	init();
	COST(.calls = 1, .allocs = 1, .store = LEN);
	BENCH_LOOP
	{
		void	*p = ft_calloc(LEN, 1);

		SINK(p);
		free(p);
	}
}

BENCH(ft_strdup)
{
	init();
	COST(.calls = 1, .scan = LEN, .allocs = 1, .copy = LEN);
	BENCH_LOOP
	{
		char	*p = ft_strdup(g_s);

		SINK(p);
		free(p);
	}
}

/* --------------------------------------------------------------- part 2 -- */
/* the length is only needed up to start + len (700) */
BENCH(ft_substr)
{
	init();
	COST(.calls = 1, .scan = 700, .allocs = 1, .copy = 600);
	BENCH_LOOP
	{
		char	*p = ft_substr(g_s, 100, 600);

		SINK(p);
		free(p);
	}
}

BENCH(ft_strjoin)
{
	init();
	COST(.calls = 1, .scan = 2 * LEN, .allocs = 1, .copy = 2 * LEN);
	BENCH_LOOP
	{
		char	*p = ft_strjoin(g_s, g_t);

		SINK(p);
		free(p);
	}
}

/* length + ~1.5 set comparisons per trimmed char, then copy the middle */
BENCH(ft_strtrim)
{
	init();
	COST(.calls = 1, .scan = LEN, .ops = 300, .allocs = 1, .copy = LEN - 200);
	BENCH_LOOP
	{
		char	*p = ft_strtrim(g_trim, "xy");

		SINK(p);
		free(p);
	}
}

/* one pass to count the words, one to cut them */
BENCH(ft_split)
{
	init();
	COST(.calls = 1, .scan = 2 * LEN, .copy = g_nletters,
		.allocs = g_nwords + 1);
	BENCH_LOOP
	{
		char	**p = ft_split(g_words, ' ');
		size_t	k;

		SINK(p);
		for (k = 0; p && p[k]; k++)
			free(p[k]);
		free(p);
	}
}

/* 32 digits for these 7 numbers */
BENCH(ft_itoa)
{
	static const int	in[] = {0, 42, -42, 2147483647, -2147483647 - 1,
		123456, -7};

	init();
	COST(.iters = 7, .calls = 7, .digits = 32, .store = 41, .allocs = 7);
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
		{
			char	*p = ft_itoa(in[k]);

			SINK(p);
			free(p);
		}
	}
}

BENCH(ft_strmapi)
{
	init();
	COST(.calls = 1 + LEN, .ops = 2 * LEN, .scan = LEN, .store = LEN,
		.allocs = 1);
	BENCH_LOOP
	{
		char	*p = ft_strmapi(g_s, upper_i);

		SINK(p);
		free(p);
	}
}

BENCH(ft_striteri)
{
	init();
	COST(.calls = 1 + LEN, .ops = 2 * LEN, .scan = LEN);
	BENCH_LOOP
	{
		memcpy(g_d, g_s, LEN + 1);
		ft_striteri(g_d, upper_p);
	}
}

BENCH(ft_putchar_fd)
{
	init();
	COST(.iters = 20, .calls = 20, .sys = 20);
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 20; k++)
			ft_putchar_fd('a', g_null);
	}
}

/* the length, then ONE write() */
BENCH(ft_putstr_fd)
{
	init();
	COST(.calls = 1, .scan = LEN, .sys = 1);
	BENCH_LOOP ft_putstr_fd(g_s, g_null);
}

BENCH(ft_putendl_fd)
{
	init();
	COST(.calls = 1, .scan = LEN, .sys = 2);
	BENCH_LOOP ft_putendl_fd(g_s, g_null);
}

/* digits in a small buffer, then ONE write() per number */
BENCH(ft_putnbr_fd)
{
	static const int	in[] = {0, 42, -42, 2147483647, -2147483647 - 1,
		123456, -7};

	init();
	COST(.iters = 7, .calls = 7, .digits = 32, .sys = 7);
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
			ft_putnbr_fd(in[k], g_null);
	}
}

/* --------------------------------------------------------------- part 3 -- */
BENCH(ft_lstnew)
{
	init();
	COST(.iters = 100, .calls = 100, .allocs = 100);
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = ft_lstnew(g_s);

			SINK(n);
			free(n);
		}
	}
}

BENCH(ft_lstadd_front)
{
	static t_list	nodes[LEN];

	init();
	COST(.iters = LEN, .calls = LEN);
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < LEN; k++)
			ft_lstadd_front(&head, &nodes[k]);
		SINK(head);
	}
}

BENCH(ft_lstsize)
{
	init();
	COST(.calls = 1, .nodes = LEN);
	BENCH_LOOP SINK(ft_lstsize(g_list));
}

BENCH(ft_lstlast)
{
	init();
	COST(.calls = 1, .nodes = LEN);
	BENCH_LOOP SINK(ft_lstlast(g_list));
}

/* the k-th add_back has to walk k nodes: 0 + 1 + … + 199 */
BENCH(ft_lstadd_back)
{
	static t_list	nodes[200];

	init();
	COST(.iters = 200, .calls = 200, .nodes = 199 * 200 / 2);
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < 200; k++)
		{
			nodes[k].next = NULL;
			ft_lstadd_back(&head, &nodes[k]);
		}
		SINK(head);
	}
}

/* the harness mallocs each node, your lstdelone frees it (+ del) */
BENCH(ft_lstdelone)
{
	init();
	COST(.iters = 100, .calls = 200, .allocs = 100);
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = malloc(sizeof(t_list));

			if (!n)
				continue ;
			n->content = NULL;
			n->next = NULL;
			ft_lstdelone(n, nodel);
		}
	}
}

BENCH(ft_lstclear)
{
	init();
	COST(.iters = 100, .calls = 101, .allocs = 100, .nodes = 100);
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = malloc(sizeof(t_list));

			if (!n)
				continue ;
			n->content = NULL;
			n->next = head;
			head = n;
		}
		ft_lstclear(&head, nodel);
		SINK(head);
	}
}

BENCH(ft_lstiter)
{
	init();
	COST(.calls = 1 + LEN, .nodes = LEN);
	BENCH_LOOP ft_lstiter(g_list, touch);
}

/* one walk of the list (+ the harness walk that frees the copy) */
BENCH(ft_lstmap)
{
	init();
	COST(.calls = 1 + LEN, .nodes = 2 * LEN, .allocs = LEN);
	BENCH_LOOP
	{
		t_list	*m = ft_lstmap(g_list, same, nodel);
		t_list	*nx;

		SINK(m);
		while (m)
		{
			nx = m->next;
			free(m);
			m = nx;
		}
	}
}
