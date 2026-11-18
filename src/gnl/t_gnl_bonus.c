/* ************************************************************************** */
/*  get_next_line — BONUS PART: several file descriptors at the same time    */
/*  Built with the _bonus files; the mandatory tests (t_gnl.c) also run on   */
/*  the bonus version, in the same suite.                                     */
/* ************************************************************************** */
#include <fcntl.h>
#include <sys/resource.h>
#include "gnl.h"

#if Z_BS != 0

/* round-robin over n fds until all of them are finished */
static void	round_robin(const char *file, int line, t_gnl *g, int n)
{
	int	active = n;

	while (active > 0)
	{
		active = 0;
		for (int i = 0; i < n; i++)
			active += gnl_step(file, line, &g[i]);
	}
}
#define ROUND_ROBIN(g, n) round_robin(__FILE__, __LINE__, g, n)

TEST(GNL_SUITE, "3 fichiers lus en alternance")
{
	static const char	a[] = "a1\na2\na3\na4\n";
	static const char	b[] = "b1 un peu plus long\nb2\nb3 sans newline";
	static const char	c[] = "\nc2\n\nc4\n";
	t_gnl				g[3];

	gnl_init(&g[0], gnl_file(a, sizeof(a) - 1), a, sizeof(a) - 1, "fichier A");
	gnl_init(&g[1], gnl_file(b, sizeof(b) - 1), b, sizeof(b) - 1, "fichier B");
	gnl_init(&g[2], gnl_file(c, sizeof(c) - 1), c, sizeof(c) - 1, "fichier C");
	ROUND_ROBIN(g, 3);
	for (int i = 0; i < 3; i++)
		close(g[i].fd);
}
TEST(GNL_SUITE, "un fichier fini bien avant les autres")
{
	static const char	a[] = "seule ligne\n";
	static const char	b[] = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n";
	t_gnl				g[2];

	gnl_init(&g[0], gnl_file(a, sizeof(a) - 1), a, sizeof(a) - 1, "court");
	gnl_init(&g[1], gnl_file(b, sizeof(b) - 1), b, sizeof(b) - 1, "long");
	ROUND_ROBIN(g, 2);
	close(g[0].fd);
	close(g[1].fd);
}
TEST(GNL_SUITE, "lignes plus longues que BUFFER_SIZE en alternance")
{
	static char	a[3000];
	static char	b[3000];
	size_t		la = 0;
	size_t		lb = 0;
	t_gnl		g[2];

	for (int k = 0; k < 20; k++)
	{
		size_t	n = zrand() % 120;

		for (size_t i = 0; i < n; i++)
		{
			a[la++] = 'A';
			b[lb++] = 'B';
		}
		a[la++] = '\n';
		if (k % 3)
			b[lb++] = '\n';
	}
	gnl_init(&g[0], gnl_file(a, la), a, la, "A (lignes variées)");
	gnl_init(&g[1], gnl_file(b, lb), b, lb, "B (lignes variées)");
	ROUND_ROBIN(g, 2);
	close(g[0].fd);
	close(g[1].fd);
}
TEST(GNL_SUITE, "fd invalides entre deux lectures")
{
	static const char	a[] = "ligne 1\nligne 2\nligne 3\n";
	t_gnl				g;

	gnl_init(&g, gnl_file(a, sizeof(a) - 1), a, sizeof(a) - 1, "fichier A");
	STEP(&g);
	INPUT("get_next_line(-1) entre deux lectures de A");
	CHECK(get_next_line(-1) == NULL, "get_next_line(-1) doit renvoyer NULL");
	INPUT("get_next_line(4242) entre deux lectures de A");
	CHECK(get_next_line(4242) == NULL, "get_next_line(4242) doit renvoyer NULL");
	while (STEP(&g))
		;
	close(g.fd);
}
TEST(GNL_SUITE, "fd élevé (1000) et stdin en alternance")
{
	static const char	a[] = "haut 1\nhaut 2\nhaut 3";
	static const char	b[] = "stdin 1\nstdin 2\n";
	struct rlimit		rl;
	int					fd;
	t_gnl				g[2];

	getrlimit(RLIMIT_NOFILE, &rl);
	fd = gnl_file(a, sizeof(a) - 1);
	if (rl.rlim_cur > 1000 && dup2(fd, 1000) == 1000)
	{
		close(fd);
		fd = 1000;
	}
	gnl_init(&g[0], fd, a, sizeof(a) - 1, "fd élevé");
	{
		int	s = gnl_file(b, sizeof(b) - 1);

		dup2(s, 0);
		close(s);
	}
	gnl_init(&g[1], 0, b, sizeof(b) - 1, "stdin");
	ROUND_ROBIN(g, 2);
	close(fd);
}
TEST(GNL_SUITE, "50 fichiers ouverts en même temps")
{
	static char	data[50][64];
	t_gnl		g[50];

	for (int i = 0; i < 50; i++)
	{
		int	l = snprintf(data[i], sizeof(data[i]), "f%d l1\nf%d l2\n%s", i, i,
				i % 2 ? "f l3 sans newline" : "");

		gnl_init(&g[i], gnl_file(data[i], (size_t)l), data[i], (size_t)l,
			"un des 50 fichiers");
	}
	ROUND_ROBIN(g, 50);
	for (int i = 0; i < 50; i++)
		close(g[i].fd);
}

#endif
