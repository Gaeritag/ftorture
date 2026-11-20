/* ************************************************************************** */
/*  Efficiency workloads (--efficiency).                                      */
/*  Each BENCH runs one workload zb_n times with your function or with the   */
/*  EXPECTED one (PICK). The expected versions below are plain, sensible     */
/*  loops compiled like the tester (-O0), NOT the optimised glibc:           */
/*  100% = as fast as a simple correct implementation.                        */
/*  "Sensible" means: one pass when one pass is enough, one write() per      */
/*  string or number (not one per character), no useless allocation.         */
/* ************************************************************************** */
#include <fcntl.h>
#include "zft.h"
#include "libft_decl.h"

#define LEN 1000

static char		g_s[LEN + 1];		/* "abcd…" (LEN chars)                  */
static char		g_t[LEN + 1];		/* same content, other buffer           */
static char		g_d[2 * LEN + 2];	/* destination                          */
static char		g_words[LEN + 1];	/* "  word word  word…"                 */
static char		g_trim[LEN + 1];	/* "xyxy…text…yxyx"                     */
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
		g_trim[i] = (i < 100 || i >= LEN - 100) ? "xy"[i % 2]
			: (char)('A' + i % 26);
		g_nodes[i].content = &g_nodes[i];
		g_nodes[i].next = i + 1 < LEN ? &g_nodes[i + 1] : NULL;
	}
	memcpy(g_t, g_s, sizeof(g_s));
	g_list = &g_nodes[0];
	g_null = open("/dev/null", O_WRONLY);
}

/* ---------------------------------------------------- expected: part 1 -- */
static int	e_isalpha(int c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static int	e_isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

static int	e_isalnum(int c)
{
	return (e_isalpha(c) || e_isdigit(c));
}

static int	e_isascii(int c)
{
	return (c >= 0 && c <= 127);
}

static int	e_isprint(int c)
{
	return (c >= 32 && c <= 126);
}

static int	e_toupper(int c)
{
	return (c >= 'a' && c <= 'z' ? c - 32 : c);
}

static int	e_tolower(int c)
{
	return (c >= 'A' && c <= 'Z' ? c + 32 : c);
}

static size_t	e_strlen(const char *s)
{
	size_t	i = 0;

	while (s[i])
		i++;
	return (i);
}

static void	*e_memset(void *s, int c, size_t n)
{
	unsigned char	*p = s;

	while (n--)
		*p++ = (unsigned char)c;
	return (s);
}

static void	e_bzero(void *s, size_t n)
{
	e_memset(s, 0, n);
}

static void	*e_memcpy(void *d, const void *s, size_t n)
{
	unsigned char		*a = d;
	const unsigned char	*b = s;

	while (n--)
		*a++ = *b++;
	return (d);
}

static void	*e_memmove(void *d, const void *s, size_t n)
{
	unsigned char		*a = d;
	const unsigned char	*b = s;

	if (a < b)
		while (n--)
			*a++ = *b++;
	else
		while (n--)
			a[n] = b[n];
	return (d);
}

static size_t	e_strlcpy(char *d, const char *s, size_t size)
{
	size_t	l = e_strlen(s);
	size_t	i;

	if (size)
	{
		for (i = 0; i < l && i + 1 < size; i++)
			d[i] = s[i];
		d[i] = 0;
	}
	return (l);
}

static size_t	e_strlcat(char *d, const char *s, size_t size)
{
	size_t	dl = 0;
	size_t	sl = e_strlen(s);
	size_t	i;

	while (dl < size && d[dl])
		dl++;
	if (dl == size)
		return (size + sl);
	for (i = 0; i < sl && dl + i + 1 < size; i++)
		d[dl + i] = s[i];
	d[dl + i] = 0;
	return (dl + sl);
}

static char	*e_strchr(const char *s, int c)
{
	while (*s && *s != (char)c)
		s++;
	return (*s == (char)c ? (char *)s : NULL);
}

static char	*e_strrchr(const char *s, int c)
{
	const char	*last = NULL;

	while (1)
	{
		if (*s == (char)c)
			last = s;
		if (!*s++)
			break ;
	}
	return ((char *)last);
}

static int	e_strncmp(const char *a, const char *b, size_t n)
{
	size_t	i = 0;

	while (i < n && a[i] && a[i] == b[i])
		i++;
	return (i == n ? 0 : (unsigned char)a[i] - (unsigned char)b[i]);
}

static void	*e_memchr(const void *s, int c, size_t n)
{
	const unsigned char	*p = s;

	while (n--)
	{
		if (*p == (unsigned char)c)
			return ((void *)p);
		p++;
	}
	return (NULL);
}

static int	e_memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char	*x = a;
	const unsigned char	*y = b;

	while (n--)
	{
		if (*x != *y)
			return (*x - *y);
		x++;
		y++;
	}
	return (0);
}

static char	*e_strnstr(const char *big, const char *lit, size_t len)
{
	size_t	i;
	size_t	j;

	if (!*lit)
		return ((char *)big);
	for (i = 0; big[i] && i < len; i++)
	{
		j = 0;
		while (lit[j] && i + j < len && big[i + j] == lit[j])
			j++;
		if (!lit[j])
			return ((char *)big + i);
	}
	return (NULL);
}

static int	e_atoi(const char *s)
{
	int	sign = 1;
	int	r = 0;

	while (*s == ' ' || (*s >= '\t' && *s <= '\r'))
		s++;
	if (*s == '-' || *s == '+')
		sign = *s++ == '-' ? -1 : 1;
	while (*s >= '0' && *s <= '9')
		r = r * 10 + (*s++ - '0');
	return (r * sign);
}

static void	*e_calloc(size_t n, size_t sz)
{
	void	*p;

	if (sz && n > (size_t)-1 / sz)
		return (NULL);
	p = malloc(n * sz);
	if (p)
		e_bzero(p, n * sz);
	return (p);
}

static char	*e_strdup(const char *s)
{
	size_t	l = e_strlen(s);
	char	*d = malloc(l + 1);

	if (d)
		e_memcpy(d, s, l + 1);
	return (d);
}

/* ---------------------------------------------------- expected: part 2 -- */
static char	*e_substr(const char *s, unsigned int st, size_t len)
{
	size_t	l = e_strlen(s);
	char	*r;

	if (st >= l)
		return (e_strdup(""));
	if (len > l - st)
		len = l - st;
	r = malloc(len + 1);
	if (!r)
		return (NULL);
	e_memcpy(r, s + st, len);
	r[len] = 0;
	return (r);
}

static char	*e_strjoin(const char *a, const char *b)
{
	size_t	la = e_strlen(a);
	size_t	lb = e_strlen(b);
	char	*r = malloc(la + lb + 1);

	if (!r)
		return (NULL);
	e_memcpy(r, a, la);
	e_memcpy(r + la, b, lb + 1);
	return (r);
}

static char	*e_strtrim(const char *s, const char *set)
{
	size_t	st = 0;
	size_t	end = e_strlen(s);

	while (s[st] && e_strchr(set, s[st]))
		st++;
	while (end > st && e_strchr(set, s[end - 1]))
		end--;
	return (e_substr(s, (unsigned int)st, end - st));
}

static char	**e_split(const char *s, char c)
{
	size_t	n = 0;
	size_t	i = 0;
	size_t	k = 0;
	char	**r;

	while (s[i])
	{
		if (s[i] != c && (i == 0 || s[i - 1] == c))
			n++;
		i++;
	}
	r = malloc((n + 1) * sizeof(char *));
	if (!r)
		return (NULL);
	i = 0;
	while (k < n)
	{
		size_t	st;

		while (s[i] == c)
			i++;
		st = i;
		while (s[i] && s[i] != c)
			i++;
		r[k] = malloc(i - st + 1);
		if (r[k])
		{
			e_memcpy(r[k], s + st, i - st);
			r[k][i - st] = 0;
		}
		if (!r[k])
		{
			while (k)
				free(r[--k]);
			return (free(r), NULL);
		}
		k++;
	}
	r[n] = NULL;
	return (r);
}

static char	*e_itoa(int v)
{
	char	b[12];
	int		i = 11;
	long	n = v;

	b[i] = 0;
	if (n == 0)
		b[--i] = '0';
	if (n < 0)
		n = -n;
	while (n)
	{
		b[--i] = (char)('0' + n % 10);
		n /= 10;
	}
	if (v < 0)
		b[--i] = '-';
	return (e_strdup(b + i));
}

static char	*e_strmapi(const char *s, char (*f)(unsigned int, char))
{
	size_t	l = e_strlen(s);
	char	*r = malloc(l + 1);
	size_t	i;

	if (!r)
		return (NULL);
	for (i = 0; i < l; i++)
		r[i] = f((unsigned int)i, s[i]);
	r[l] = 0;
	return (r);
}

static void	e_striteri(char *s, void (*f)(unsigned int, char *))
{
	unsigned int	i;

	for (i = 0; s[i]; i++)
		f(i, s + i);
}

static void	e_putchar_fd(char c, int fd)
{
	write(fd, &c, 1);
}

static void	e_putstr_fd(char *s, int fd)
{
	write(fd, s, e_strlen(s));
}

static void	e_putendl_fd(char *s, int fd)
{
	write(fd, s, e_strlen(s));
	write(fd, "\n", 1);
}

static void	e_putnbr_fd(int v, int fd)
{
	char	b[12];
	int		i = 12;
	long	n = v;

	if (n == 0)
		b[--i] = '0';
	if (n < 0)
		n = -n;
	while (n)
	{
		b[--i] = (char)('0' + n % 10);
		n /= 10;
	}
	if (v < 0)
		b[--i] = '-';
	write(fd, b + i, (size_t)(12 - i));
}

/* ---------------------------------------------------- expected: part 3 -- */
static t_list	*e_lstnew(void *content)
{
	t_list	*n = malloc(sizeof(t_list));

	if (!n)
		return (NULL);
	n->content = content;
	n->next = NULL;
	return (n);
}

static void	e_lstadd_front(t_list **l, t_list *n)
{
	n->next = *l;
	*l = n;
}

static unsigned int	e_lstsize(t_list *l)
{
	unsigned int	n = 0;

	while (l)
	{
		n++;
		l = l->next;
	}
	return (n);
}

static t_list	*e_lstlast(t_list *l)
{
	while (l && l->next)
		l = l->next;
	return (l);
}

static void	e_lstadd_back(t_list **l, t_list *n)
{
	if (!*l)
		*l = n;
	else
		e_lstlast(*l)->next = n;
}

static void	e_lstdelone(t_list *l, void (*del)(void *))
{
	del(l->content);
	free(l);
}

static void	e_lstclear(t_list **l, void (*del)(void *))
{
	t_list	*nx;

	while (*l)
	{
		nx = (*l)->next;
		e_lstdelone(*l, del);
		*l = nx;
	}
}

static void	e_lstiter(t_list *l, void (*f)(void *))
{
	while (l)
	{
		f(l->content);
		l = l->next;
	}
}

static t_list	*e_lstmap(t_list *l, void *(*f)(void *), void (*del)(void *))
{
	t_list	*head = NULL;
	t_list	**tail = &head;

	while (l)
	{
		*tail = e_lstnew(f(l->content));
		if (!*tail)
			return (e_lstclear(&head, del), NULL);
		tail = &(*tail)->next;
		l = l->next;
	}
	return (head);
}

/* ------------------------------------------------------------- helpers -- */
static char	upper_i(unsigned int i, char c)
{
	(void)i;
	return (e_toupper(c));
}

static void	upper_p(unsigned int i, char *c)
{
	(void)i;
	*c = (char)e_toupper(*c);
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

static void	free_list(t_list *l)
{
	t_list	*nx;

	while (l)
	{
		nx = l->next;
		free(l);
		l = nx;
	}
}

#define CHARS(F, ft, e) BENCH(F) { init(); BENCH_LOOP { int c; \
	for (c = -1; c < 256; c++) SINK(PICK(ft, e)(c)); } }

/* --------------------------------------------------------------- part 1 -- */
CHARS(ft_isalpha, ft_isalpha, e_isalpha)
CHARS(ft_isdigit, ft_isdigit, e_isdigit)
CHARS(ft_isalnum, ft_isalnum, e_isalnum)
CHARS(ft_isascii, ft_isascii, e_isascii)
CHARS(ft_isprint, ft_isprint, e_isprint)
CHARS(ft_toupper, ft_toupper, e_toupper)
CHARS(ft_tolower, ft_tolower, e_tolower)

BENCH(ft_strlen) { init(); BENCH_LOOP SINK(PICK(ft_strlen, e_strlen)(g_s)); }

BENCH(ft_memset)
{
	init();
	BENCH_LOOP SINK(PICK(ft_memset, e_memset)(g_d, 'x', LEN));
}

BENCH(ft_bzero) { init(); BENCH_LOOP PICK(ft_bzero, e_bzero)(g_d, LEN); }

BENCH(ft_memcpy)
{
	init();
	BENCH_LOOP SINK(PICK(ft_memcpy, e_memcpy)(g_d, g_s, LEN));
}

BENCH(ft_memmove)
{
	init();
	BENCH_LOOP
	{
		e_memcpy(g_d, g_s, LEN);
		SINK(PICK(ft_memmove, e_memmove)(g_d + 10, g_d, LEN - 10));
		SINK(PICK(ft_memmove, e_memmove)(g_d, g_d + 10, LEN - 10));
	}
}

BENCH(ft_strlcpy)
{
	init();
	BENCH_LOOP SINK(PICK(ft_strlcpy, e_strlcpy)(g_d, g_s, sizeof(g_d)));
}

BENCH(ft_strlcat)
{
	init();
	BENCH_LOOP
	{
		g_d[0] = 0;
		SINK(PICK(ft_strlcat, e_strlcat)(g_d, g_s, sizeof(g_d)));
		SINK(PICK(ft_strlcat, e_strlcat)(g_d, g_s, sizeof(g_d)));
	}
}

BENCH(ft_strchr)
{
	init();
	BENCH_LOOP
	{
		SINK(PICK(ft_strchr, e_strchr)(g_s, '\0'));
		SINK(PICK(ft_strchr, e_strchr)(g_s, '#'));
	}
}

BENCH(ft_strrchr)
{
	init();
	BENCH_LOOP
	{
		SINK(PICK(ft_strrchr, e_strrchr)(g_s, 'a'));
		SINK(PICK(ft_strrchr, e_strrchr)(g_s, '#'));
	}
}

BENCH(ft_strncmp)
{
	init();
	BENCH_LOOP SINK(PICK(ft_strncmp, e_strncmp)(g_s, g_t, LEN + 1));
}

BENCH(ft_memchr)
{
	init();
	BENCH_LOOP SINK(PICK(ft_memchr, e_memchr)(g_s, '#', LEN));
}

BENCH(ft_memcmp)
{
	init();
	BENCH_LOOP SINK(PICK(ft_memcmp, e_memcmp)(g_s, g_t, LEN));
}

BENCH(ft_strnstr)
{
	init();
	BENCH_LOOP
	{
		SINK(PICK(ft_strnstr, e_strnstr)(g_s, "xyz#", LEN));
		SINK(PICK(ft_strnstr, e_strnstr)(g_s, "uvwxyz", LEN));
	}
}

BENCH(ft_atoi)
{
	static const char	*in[] = {"42", "   -2147483648", "+2147483647",
		"\t\n 123456abc", "0", "-17", "  999999"};

	init();
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
			SINK(PICK(ft_atoi, e_atoi)(in[k]));
	}
}

BENCH(ft_calloc)
{
	init();
	BENCH_LOOP
	{
		void	*p = PICK(ft_calloc, e_calloc)(LEN, 1);

		SINK(p);
		free(p);
	}
}

BENCH(ft_strdup)
{
	init();
	BENCH_LOOP
	{
		char	*p = PICK(ft_strdup, e_strdup)(g_s);

		SINK(p);
		free(p);
	}
}

/* --------------------------------------------------------------- part 2 -- */
BENCH(ft_substr)
{
	init();
	BENCH_LOOP
	{
		char	*p = PICK(ft_substr, e_substr)(g_s, 100, 600);

		SINK(p);
		free(p);
	}
}

BENCH(ft_strjoin)
{
	init();
	BENCH_LOOP
	{
		char	*p = PICK(ft_strjoin, e_strjoin)(g_s, g_t);

		SINK(p);
		free(p);
	}
}

BENCH(ft_strtrim)
{
	init();
	BENCH_LOOP
	{
		char	*p = PICK(ft_strtrim, e_strtrim)(g_trim, "xy");

		SINK(p);
		free(p);
	}
}

BENCH(ft_split)
{
	init();
	BENCH_LOOP
	{
		char	**p = PICK(ft_split, e_split)(g_words, ' ');
		size_t	k;

		SINK(p);
		for (k = 0; p && p[k]; k++)
			free(p[k]);
		free(p);
	}
}

BENCH(ft_itoa)
{
	static const int	in[] = {0, 42, -42, 2147483647, -2147483647 - 1,
		123456, -7};

	init();
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
		{
			char	*p = PICK(ft_itoa, e_itoa)(in[k]);

			SINK(p);
			free(p);
		}
	}
}

BENCH(ft_strmapi)
{
	init();
	BENCH_LOOP
	{
		char	*p = PICK(ft_strmapi, e_strmapi)(g_s, upper_i);

		SINK(p);
		free(p);
	}
}

BENCH(ft_striteri)
{
	init();
	BENCH_LOOP
	{
		e_memcpy(g_d, g_s, LEN + 1);
		PICK(ft_striteri, e_striteri)(g_d, upper_p);
	}
}

BENCH(ft_putchar_fd)
{
	init();
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 20; k++)
			PICK(ft_putchar_fd, e_putchar_fd)('a', g_null);
	}
}

BENCH(ft_putstr_fd)
{
	init();
	BENCH_LOOP PICK(ft_putstr_fd, e_putstr_fd)(g_s, g_null);
}

BENCH(ft_putendl_fd)
{
	init();
	BENCH_LOOP PICK(ft_putendl_fd, e_putendl_fd)(g_s, g_null);
}

BENCH(ft_putnbr_fd)
{
	static const int	in[] = {0, 42, -42, 2147483647, -2147483647 - 1,
		123456, -7};

	init();
	BENCH_LOOP
	{
		size_t	k;

		for (k = 0; k < sizeof(in) / sizeof(*in); k++)
			PICK(ft_putnbr_fd, e_putnbr_fd)(in[k], g_null);
	}
}

/* --------------------------------------------------------------- part 3 -- */
BENCH(ft_lstnew)
{
	init();
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = PICK(ft_lstnew, e_lstnew)(g_s);

			SINK(n);
			free(n);
		}
	}
}

BENCH(ft_lstadd_front)
{
	static t_list	nodes[LEN];

	init();
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < LEN; k++)
			PICK(ft_lstadd_front, e_lstadd_front)(&head, &nodes[k]);
		SINK(head);
	}
}

BENCH(ft_lstsize)
{
	init();
	BENCH_LOOP SINK(PICK(ft_lstsize, e_lstsize)(g_list));
}

BENCH(ft_lstlast)
{
	init();
	BENCH_LOOP SINK(PICK(ft_lstlast, e_lstlast)(g_list));
}

BENCH(ft_lstadd_back)
{
	static t_list	nodes[200];

	init();
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < 200; k++)
		{
			nodes[k].next = NULL;
			PICK(ft_lstadd_back, e_lstadd_back)(&head, &nodes[k]);
		}
		SINK(head);
	}
}

BENCH(ft_lstdelone)
{
	init();
	BENCH_LOOP
	{
		int	k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = e_lstnew(NULL);

			if (n)
				PICK(ft_lstdelone, e_lstdelone)(n, nodel);
		}
	}
}

BENCH(ft_lstclear)
{
	init();
	BENCH_LOOP
	{
		t_list	*head = NULL;
		int		k;

		for (k = 0; k < 100; k++)
		{
			t_list	*n = e_lstnew(NULL);

			if (n)
				e_lstadd_front(&head, n);
		}
		PICK(ft_lstclear, e_lstclear)(&head, nodel);
		SINK(head);
	}
}

BENCH(ft_lstiter)
{
	init();
	BENCH_LOOP PICK(ft_lstiter, e_lstiter)(g_list, touch);
}

BENCH(ft_lstmap)
{
	init();
	BENCH_LOOP
	{
		t_list	*m = PICK(ft_lstmap, e_lstmap)(g_list, same, nodel);

		SINK(m);
		free_list(m);
	}
}
