/* ************************************************************************** */
/*  Efficiency workload of get_next_line (--efficiency), one per BUFFER_SIZE. */
/*  The EXPECTED get_next_line below is the classic one (stash, the rest     */
/*  moved to the front after each line, BUFFER_SIZE bytes per read()), with  */
/*  two sensible habits: the stash grows by doubling instead of a strjoin    */
/*  after every read(), and '\n' is only searched in the new bytes. So it    */
/*  stays linear in the length of a line, even with BUFFER_SIZE=1.           */
/*  Not measured without -D BUFFER_SIZE (your default value is unknown).     */
/* ************************************************************************** */
#include <fcntl.h>
#include "gnl.h"

#if Z_BS > 0

typedef struct s_stash
{
	char	*buf;
	size_t	off;	/* first byte not returned yet */
	size_t	len;	/* bytes in buf                */
	size_t	cap;
}	t_stash;

static int	grow(t_stash *s, size_t need)
{
	char	*nb;
	size_t	cap = s->cap ? s->cap : 64;

	if (s->off && s->off * 2 >= s->len)
	{
		memmove(s->buf, s->buf + s->off, s->len - s->off);
		s->len -= s->off;
		s->off = 0;
	}
	if (s->len + need <= s->cap)
		return (1);
	while (cap < s->len + need)
		cap *= 2;
	nb = malloc(cap);
	if (!nb)
		return (0);
	if (s->buf)
		memcpy(nb, s->buf, s->len);
	free(s->buf);
	s->buf = nb;
	s->cap = cap;
	return (1);
}

static char	*cut(t_stash *s, size_t end)
{
	char	*line = malloc(end - s->off + 1);

	if (!line)
		return (NULL);
	memcpy(line, s->buf + s->off, end - s->off);
	line[end - s->off] = 0;
	/* like most gnl: what is left goes back to the front of the stash */
	memmove(s->buf, s->buf + end, s->len - end);
	s->len -= end;
	s->off = 0;
	return (line);
}

static char	*e_gnl(int fd)
{
	static t_stash	s;
	char			*buf;
	size_t			seen = 0;	/* bytes after s.off already searched */
	ssize_t			n = 1;
	char			*nl = NULL;

	while (1)
	{
		if (s.len - s.off > seen)
			nl = memchr(s.buf + s.off + seen, '\n', s.len - s.off - seen);
		if (nl || n <= 0)
			break ;
		seen = s.len - s.off;
		buf = malloc((size_t)Z_BS + 1);
		if (!buf)
			return (NULL);
		n = read(fd, buf, (size_t)Z_BS);
		if (n > 0 && grow(&s, (size_t)n))
		{
			memcpy(s.buf + s.len, buf, (size_t)n);
			s.len += (size_t)n;
		}
		free(buf);
	}
	if (nl)
		return (cut(&s, (size_t)(nl - s.buf) + 1));
	if (s.len > s.off && n == 0)
		return (cut(&s, s.len));
	free(s.buf);
	s = (t_stash){0};
	return (NULL);
}

/* 400 lines of 0..120 chars, then one line of 20 000 chars */
static int	bench_fd(void)
{
	static int	fd = -1;
	static char	data[400 * 122 + 20002];
	size_t		len = 0;
	int			i;
	int			k;

	if (fd >= 0)
		return (fd);
	for (i = 0; i < 400; i++)
	{
		int	l = (int)(zrand() % 121);

		for (k = 0; k < l; k++)
			data[len++] = (char)('a' + (i + k) % 26);
		data[len++] = '\n';
	}
	for (k = 0; k < 20000; k++)
		data[len++] = (char)('A' + k % 26);
	data[len++] = '\n';
	fd = gnl_file(data, len);
	return (fd);
}

BENCH(GNL_SUITE)
{
	int		fd = bench_fd();
	char	*l;

	BENCH_LOOP
	{
		lseek(fd, 0, SEEK_SET);
		while ((l = PICK(get_next_line, e_gnl)(fd)))
		{
			SINK(l[0]);
			free(l);
		}
	}
}

#endif
