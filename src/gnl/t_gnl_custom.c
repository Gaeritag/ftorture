/* ************************************************************************** */
/*  get_next_line on your own inputs (--file): a file of yours, read line by */
/*  line and compared with its content, or a generated file of a given size  */
/*  (random lines of 0 to 200 characters, one of up to 10 000).             */
/*  The list comes from FTORTURE_GNL_FILES, one "path:/x" or "size:N" per   */
/*  line; the tests are registered at start-up, in the suite GNL_SUITE.     */
/* ************************************************************************** */
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "gnl.h"

#define CUSTOM_MAX 16
#define STR2(x) #x
#define STR(x) STR2(x)

typedef struct s_custom
{
	char	name[300];
	char	what[300];
	char	path[4096];
	size_t	size;		/* 0: path                                          */
}	t_custom;

static t_custom	g_custom[CUSTOM_MAX];
static int		g_ncustom;

static void	run_path(const t_custom *c)
{
	struct stat	st;
	char		*d;
	size_t		n = 0;
	int			fd = open(c->path, O_RDONLY);

	if (fd < 0 || fstat(fd, &st) < 0)
		zft_failf(__FILE__, __LINE__, "%s : %s", c->path, strerror(errno));
	d = malloc((size_t)st.st_size + 1);
	if (!d)
		zft_failf(__FILE__, __LINE__, "malloc (tester)");
	while (n < (size_t)st.st_size)
	{
		ssize_t	r = read(fd, d + n, (size_t)st.st_size - n);

		if (r <= 0)
			break ;
		n += (size_t)r;
	}
	close(fd);
	CHECK(!memchr(d, '\0', n), "%s", zft_en()
		? "your file contains a '\\0' byte: get_next_line returns C strings, "
		"use a text file" : "ton fichier contient un octet '\\0' : "
		"get_next_line renvoie des chaînes C, prends un fichier texte");
	g_rd_bytes = 0;
	g_rd_calls = 0;
	fd = open(c->path, O_RDONLY);
	EXPECT_FD(fd, d, n, c->what);
	close(fd);
	free(d);
}

static void	run_size(const t_custom *c)
{
	char	*d = malloc(c->size + 1);
	size_t	k = 0;
	/* one long line, but the volume is what this test is about: the quadratic
	   cost of very long lines has its own tests                            */
	size_t	longline = c->size / 4 < 10000 ? c->size / 4 : 10000;
	int		fd;

	if (!d)
		zft_failf(__FILE__, __LINE__, "malloc (tester)");
	while (k < c->size)
	{
		size_t	l = zrand() % 201;

		if (longline && k >= c->size / 2)
		{
			l = longline;
			longline = 0;
		}
		for (size_t i = 0; i < l && k < c->size; i++)
			d[k++] = (char)(' ' + zrand() % 95);
		/* the last line has no '\n' */
		if (k < c->size - 1)
			d[k++] = '\n';
		else if (k < c->size)
			d[k++] = 'z';
	}
	fd = gnl_file(d, k);
	EXPECT_FD(fd, d, k, c->what);
	close(fd);
	free(d);
}

static void	run(int i)
{
	if (g_custom[i].size)
		run_size(&g_custom[i]);
	else
		run_path(&g_custom[i]);
}

#define T(k) static void t##k(void) { run(k); }
T(0) T(1) T(2) T(3) T(4) T(5) T(6) T(7)
T(8) T(9) T(10) T(11) T(12) T(13) T(14) T(15)
static void	(*const g_fn[CUSTOM_MAX])(void) = {t0, t1, t2, t3, t4, t5, t6,
	t7, t8, t9, t10, t11, t12, t13, t14, t15};

static const char	*human(size_t n, char *b, size_t bsz)
{
	if (n >= 1024 * 1024 && n % (1024 * 1024) == 0)
		snprintf(b, bsz, "%zu Mo", n / (1024 * 1024));
	else if (n >= 1024 && n % 1024 == 0)
		snprintf(b, bsz, "%zu Ko", n / 1024);
	else
		snprintf(b, bsz, "%zu octets", n);
	return (b);
}

__attribute__((constructor)) static void	register_custom(void)
{
	const char	*list = getenv("FTORTURE_GNL_FILES");

	while (list && *list && g_ncustom < CUSTOM_MAX)
	{
		size_t		l = strcspn(list, "\n");
		t_custom	*c = &g_custom[g_ncustom];
		char		b[64];

		if (l > 5 && !strncmp(list, "size:", 5))
		{
			c->size = (size_t)strtoull(list + 5, NULL, 10);
			snprintf(c->name, sizeof(c->name), "fichier généré : %s",
				human(c->size, b, sizeof(b)));
			snprintf(c->what, sizeof(c->what), "fichier généré de %s",
				human(c->size, b, sizeof(b)));
		}
		else if (l > 5 && !strncmp(list, "path:", 5))
		{
			const char	*base;

			snprintf(c->path, sizeof(c->path), "%.*s", (int)(l - 5), list + 5);
			base = strrchr(c->path, '/');
			snprintf(c->name, sizeof(c->name), "ton fichier : %s",
				base ? base + 1 : c->path);
			snprintf(c->what, sizeof(c->what), "%s", c->path);
		}
		if (c->size || c->path[0])
		{
			zft_register(STR(GNL_SUITE), c->name, K_NORMAL, 0,
				g_fn[g_ncustom], __FILE__, __LINE__, -1);
			g_ncustom++;
		}
		list += l + (list[l] == '\n');
	}
}
