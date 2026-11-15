/* Reference implementations. Compiled with -O0 -fno-builtin.                */
#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "refs.h"

/* ------------------------------------------------------- BSD functions -- */
static size_t	bsd_strlen(const char *s)
{
	const char	*p = s;

	while (*p)
		p++;
	return ((size_t)(p - s));
}

/* OpenBSD strlcpy.c */
static size_t	bsd_strlcpy(char *dst, const char *src, size_t dsize)
{
	const char	*osrc = src;
	size_t		nleft = dsize;

	if (nleft != 0)
	{
		while (--nleft != 0)
		{
			if ((*dst++ = *src++) == '\0')
				break ;
		}
	}
	if (nleft == 0)
	{
		if (dsize != 0)
			*dst = '\0';
		while (*src++)
			;
	}
	return ((size_t)(src - osrc - 1));
}

/* OpenBSD strlcat.c */
static size_t	bsd_strlcat(char *dst, const char *src, size_t dsize)
{
	const char	*odst = dst;
	const char	*osrc = src;
	size_t		n = dsize;
	size_t		dlen;

	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = (size_t)(dst - odst);
	n = dsize - dlen;
	if (n-- == 0)
		return (dlen + bsd_strlen(src));
	while (*src != '\0')
	{
		if (n != 0)
		{
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';
	return (dlen + (size_t)(src - osrc));
}

/* FreeBSD strnstr.c */
static char	*bsd_strnstr(const char *s, const char *find, size_t slen)
{
	char	c;
	char	sc;
	size_t	len;

	if ((c = *find++) != '\0')
	{
		len = bsd_strlen(find);
		do
		{
			do
			{
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

/* glibc's isascii may be a macro; wrap it */
static int	libc_isascii(int c) { return (isascii(c)); }

int		(*volatile r_isalpha)(int) = isalpha;
int		(*volatile r_isdigit)(int) = isdigit;
int		(*volatile r_isalnum)(int) = isalnum;
int		(*volatile r_isascii)(int) = libc_isascii;
int		(*volatile r_isprint)(int) = isprint;
size_t	(*volatile r_strlen)(const char *) = strlen;
void	*(*volatile r_memset)(void *, int, size_t) = memset;
void	(*volatile r_bzero)(void *, size_t) = bzero;
void	*(*volatile r_memcpy)(void *, const void *, size_t) = memcpy;
void	*(*volatile r_memmove)(void *, const void *, size_t) = memmove;
size_t	(*volatile r_strlcpy)(char *, const char *, size_t) = bsd_strlcpy;
size_t	(*volatile r_strlcat)(char *, const char *, size_t) = bsd_strlcat;
int		(*volatile r_toupper)(int) = toupper;
int		(*volatile r_tolower)(int) = tolower;
char	*(*volatile r_strchr)(const char *, int) = strchr;
char	*(*volatile r_strrchr)(const char *, int) = strrchr;
int		(*volatile r_strncmp)(const char *, const char *, size_t) = strncmp;
void	*(*volatile r_memchr)(const void *, int, size_t) = memchr;
int		(*volatile r_memcmp)(const void *, const void *, size_t) = memcmp;
char	*(*volatile r_strnstr)(const char *, const char *, size_t) = bsd_strnstr;
int		(*volatile r_atoi)(const char *) = atoi;
void	*(*volatile r_calloc)(size_t, size_t) = calloc;
char	*(*volatile r_strdup)(const char *) = strdup;

/* ------------------------------------------------- part 2 (unprotected) -- */
/* These use malloc/free through the tester's wrapper (tracked memory).     */
static char	*ref_substr(char const *s, unsigned int start, size_t len)
{
	size_t	sl = bsd_strlen(s);
	char	*r;
	size_t	i;

	if (start >= sl)
		len = 0;
	else if (len > sl - start)
		len = sl - start;
	r = malloc(len + 1);
	if (!r)
		return (NULL);
	i = 0;
	while (i < len)
	{
		r[i] = s[start + i];
		i++;
	}
	r[i] = '\0';
	return (r);
}

static char	*ref_strjoin(char const *s1, char const *s2)
{
	size_t	a = bsd_strlen(s1);
	size_t	b = bsd_strlen(s2);
	char	*r = malloc(a + b + 1);

	if (!r)
		return (NULL);
	memcpy(r, s1, a);
	memcpy(r + a, s2, b);
	r[a + b] = '\0';
	return (r);
}

static int	in_set(char c, const char *set)
{
	while (*set)
		if (*set++ == c)
			return (1);
	return (0);
}

static char	*ref_strtrim(char const *s1, char const *set)
{
	size_t	b = 0;
	size_t	e = bsd_strlen(s1);

	while (s1[b] && in_set(s1[b], set))
		b++;
	while (e > b && in_set(s1[e - 1], set))
		e--;
	return (ref_substr(s1, (unsigned int)b, e - b));
}

static char	**ref_split(char const *s, char c)
{
	size_t	n = 0;
	size_t	i = 0;
	size_t	w = 0;
	char	**r;

	while (s[i])
	{
		if (s[i] != c && (i == 0 || s[i - 1] == c))
			n++;
		i++;
	}
	r = malloc(sizeof(char *) * (n + 1));
	if (!r)
		return (NULL);
	i = 0;
	while (w < n)
	{
		size_t	st;

		while (s[i] == c)
			i++;
		st = i;
		while (s[i] && s[i] != c)
			i++;
		r[w] = ref_substr(s, (unsigned int)st, i - st);
		if (!r[w])
		{
			while (w > 0)
				free(r[--w]);
			free(r);
			return (NULL);
		}
		w++;
	}
	r[n] = NULL;
	return (r);
}

static char	*ref_itoa(int n)
{
	char	buf[16];
	int		i = 15;
	long	v = n;
	int		neg = v < 0;
	char	*r;

	buf[i] = '\0';
	if (neg)
		v = -v;
	do
	{
		buf[--i] = (char)('0' + v % 10);
		v /= 10;
	} while (v);
	if (neg)
		buf[--i] = '-';
	r = malloc((size_t)(16 - i));
	if (!r)
		return (NULL);
	memcpy(r, buf + i, (size_t)(16 - i));
	return (r);
}

static char	*ref_strmapi(char const *s, char (*f)(unsigned int, char))
{
	size_t	l = bsd_strlen(s);
	char	*r = malloc(l + 1);
	size_t	i;

	if (!r)
		return (NULL);
	i = 0;
	while (i < l)
	{
		r[i] = f((unsigned int)i, s[i]);
		i++;
	}
	r[l] = '\0';
	return (r);
}

static void	ref_striteri(char *s, void (*f)(unsigned int, char *))
{
	unsigned int	i = 0;

	while (s[i])
	{
		f(i, &s[i]);
		i++;
	}
}

static void	ref_putchar_fd(char c, int fd) { (void)!write(fd, &c, 1); }
static void	ref_putstr_fd(char *s, int fd) { (void)!write(fd, s, bsd_strlen(s)); }
static void	ref_putendl_fd(char *s, int fd)
{
	ref_putstr_fd(s, fd);
	ref_putchar_fd('\n', fd);
}
static void	ref_putnbr_fd(int n, int fd)
{
	long	v = n;

	if (v < 0)
	{
		ref_putchar_fd('-', fd);
		v = -v;
	}
	if (v >= 10)
		ref_putnbr_fd((int)(v / 10), fd);
	ref_putchar_fd((char)('0' + v % 10), fd);
}

/* ------------------------------------------------- part 3 (unprotected) -- */
static t_list	*ref_lstnew(void *content)
{
	t_list	*n = malloc(sizeof(t_list));

	if (!n)
		return (NULL);
	n->content = content;
	n->next = NULL;
	return (n);
}

static void	ref_lstadd_front(t_list **lst, t_list *new)
{
	new->next = *lst;
	*lst = new;
}

static unsigned int	ref_lstsize(t_list *lst)
{
	unsigned int	n = 0;

	while (lst)
	{
		n++;
		lst = lst->next;
	}
	return (n);
}

static t_list	*ref_lstlast(t_list *lst)
{
	if (!lst)
		return (NULL);
	while (lst->next)
		lst = lst->next;
	return (lst);
}

static void	ref_lstadd_back(t_list **lst, t_list *new)
{
	if (!*lst)
		*lst = new;
	else
		ref_lstlast(*lst)->next = new;
}

static void	ref_lstdelone(t_list *lst, void (*del)(void *))
{
	del(lst->content);
	free(lst);
}

static void	ref_lstclear(t_list **lst, void (*del)(void *))
{
	t_list	*cur = *lst;
	t_list	*nx;

	while (cur)
	{
		nx = cur->next;
		ref_lstdelone(cur, del);
		cur = nx;
	}
	*lst = NULL;
}

static void	ref_lstiter(t_list *lst, void (*f)(void *))
{
	while (lst)
	{
		f(lst->content);
		lst = lst->next;
	}
}

static t_list	*ref_lstmap(t_list *lst, void *(*f)(void *), void (*del)(void *))
{
	t_list	*head = NULL;
	t_list	**tail = &head;
	t_list	*n;
	void	*c;

	while (lst)
	{
		c = f(lst->content);
		n = malloc(sizeof(t_list));
		if (!n)
		{
			del(c);
			ref_lstclear(&head, del);
			return (NULL);
		}
		n->content = c;
		n->next = NULL;
		*tail = n;
		tail = &n->next;
		lst = lst->next;
	}
	return (head);
}

char	*(*volatile r_substr)(char const *, unsigned int, size_t) = ref_substr;
char	*(*volatile r_strjoin)(char const *, char const *) = ref_strjoin;
char	*(*volatile r_strtrim)(char const *, char const *) = ref_strtrim;
char	**(*volatile r_split)(char const *, char) = ref_split;
char	*(*volatile r_itoa)(int) = ref_itoa;
char	*(*volatile r_strmapi)(char const *, char (*)(unsigned int, char))
	= ref_strmapi;
void	(*volatile r_striteri)(char *, void (*)(unsigned int, char *))
	= ref_striteri;
void	(*volatile r_putchar_fd)(char, int) = ref_putchar_fd;
void	(*volatile r_putstr_fd)(char *, int) = ref_putstr_fd;
void	(*volatile r_putendl_fd)(char *, int) = ref_putendl_fd;
void	(*volatile r_putnbr_fd)(int, int) = ref_putnbr_fd;

t_list			*(*volatile r_lstnew)(void *) = ref_lstnew;
void			(*volatile r_lstadd_front)(t_list **, t_list *) = ref_lstadd_front;
unsigned int	(*volatile r_lstsize)(t_list *) = ref_lstsize;
t_list			*(*volatile r_lstlast)(t_list *) = ref_lstlast;
void			(*volatile r_lstadd_back)(t_list **, t_list *) = ref_lstadd_back;
void			(*volatile r_lstdelone)(t_list *, void (*)(void *)) = ref_lstdelone;
void			(*volatile r_lstclear)(t_list **, void (*)(void *)) = ref_lstclear;
void			(*volatile r_lstiter)(t_list *, void (*)(void *)) = ref_lstiter;
t_list			*(*volatile r_lstmap)(t_list *, void *(*)(void *),
					void (*)(void *)) = ref_lstmap;
