/* ************************************************************************** */
/*  get_next_line — MANDATORY PART (subject v14.3)                            */
/*  Built once per BUFFER_SIZE; every test lands in the suite GNL_SUITE      */
/*  (one report line per BUFFER_SIZE). Each line returned is compared with   */
/*  the expected one, must be a malloc block, and is freed; after the last   */
/*  line get_next_line must return NULL (twice). Leaks are checked.           */
/* ************************************************************************** */
#include <fcntl.h>
#include <sys/wait.h>
#include "gnl.h"

/* BUFFER_SIZE used to build lines around the buffer size */
#define BS (Z_BS > 0 ? Z_BS : 42)
/* sizes that stay reasonable even with BUFFER_SIZE=10000000 */
#define SMALL_BS (Z_BS > 0 && Z_BS <= 100000)

#define FILE_TEST(NAME, DATA) \
TEST(GNL_SUITE, NAME) \
{ \
	static const char	d[] = DATA; \
	int					fd = gnl_file(d, sizeof(d) - 1); \
	EXPECT_FD(fd, d, sizeof(d) - 1, NAME); \
	close(fd); \
}

#if Z_BS == 0

/* BUFFER_SIZE=0 makes no sense: only "no crash, no leak" is checked (▲) */
TEST(GNL_SUITE, "BUFFER_SIZE=0 : NULL, sans crash ni fuite")
{
	int		fd = gnl_file("abc\ndef\n", 8);
	char	*r = get_next_line(fd);

	INPUT("get_next_line(fd) compilé avec -D BUFFER_SIZE=0");
	CHECK(r == NULL, "avec BUFFER_SIZE=0, get_next_line devrait renvoyer NULL");
	free(r);
	close(fd);
}

#else

/* ---------------------------------------------------------- contents -- */
FILE_TEST("fichier vide", "")
FILE_TEST("une ligne avec \\n", "hello\n")
FILE_TEST("une ligne sans \\n", "hello")
FILE_TEST("un seul \\n", "\n")
FILE_TEST("que des \\n", "\n\n\n\n\n")
FILE_TEST("un seul caractère", "x")
FILE_TEST("plusieurs lignes", "ligne 1\nligne 2\n\nligne 4\n")
FILE_TEST("dernière ligne sans \\n", "a\nb\nc")
FILE_TEST("lignes vides au milieu et à la fin", "a\n\n\nb\n\n")
FILE_TEST("caractères spéciaux et non-ASCII", "\t tab\r\n\x7f\x01\n\xe9t\xe9 \xff\n")
FILE_TEST("texte réaliste", "Lorem ipsum dolor sit amet,\nconsectetur adipiscing "
	"elit.\n\nSed do eiusmod tempor incididunt\nut labore et dolore magna "
	"aliqua.\nUt enim ad minim veniam")

/* lines of length n (+ '\n' or not) */
static void	run_len(const char *file, int line, size_t n, int nl, int count)
{
	size_t	tot = (n + (size_t)nl) * (size_t)count;
	char	*d = gbuf(tot ? tot : 1, 0);
	size_t	k = 0;
	int		fd;
	char	what[96];

	for (int c = 0; c < count; c++)
	{
		for (size_t i = 0; i < n; i++)
			d[k++] = (char)('a' + (i + (size_t)c) % 26);
		if (nl)
			d[k++] = '\n';
	}
	snprintf(what, sizeof(what), "%d ligne(s) de %zu car.%s", count, n,
		nl ? " + \\n" : " sans \\n");
	fd = gnl_file(d, tot);
	gnl_expect(file, line, fd, d, tot, what);
	close(fd);
}
#define LEN(n, nl, count) run_len(__FILE__, __LINE__, n, nl, count)

TEST(GNL_SUITE, "lignes de BUFFER_SIZE - 1, BUFFER_SIZE, BUFFER_SIZE + 1")
{
	if (!SMALL_BS)
	{
		LEN(100, 1, 3);
		return ;
	}
	if (BS > 1)
		LEN(BS - 1, 1, 3);
	LEN(BS, 1, 3);
	LEN(BS + 1, 1, 3);
}
TEST(GNL_SUITE, "pareil, sans \\n final")
{
	if (!SMALL_BS)
	{
		LEN(100, 0, 1);
		return ;
	}
	if (BS > 1)
		LEN(BS - 1, 0, 1);
	LEN(BS, 0, 1);
	LEN(BS + 1, 0, 1);
}
TEST(GNL_SUITE, "lignes de 2 x BUFFER_SIZE et plus")
{
	if (!SMALL_BS || BS > 20000)
	{
		LEN(5000, 1, 2);
		return ;
	}
	LEN(2 * BS, 1, 2);
	LEN(3 * BS + 7, 1, 2);
}
TEST(GNL_SUITE, "ligne longue (10 000 caractères)")   { LEN(10000, 1, 2); LEN(10000, 0, 1); }
TEST(GNL_SUITE, "ligne très longue (100 000)")        { LEN(100000, 1, 1); }
TEST(GNL_SUITE, "5 000 lignes d'un caractère")        { LEN(1, 1, 5000); }
TEST(GNL_SUITE, "gros fichier (~80 Ko, longueurs variées)")
{
	size_t	cap = 90000;
	char	*d = gbuf(cap, 0);
	size_t	k = 0;
	int		fd;

	while (k < 80000)
	{
		size_t	l = zrand() % 160;

		for (size_t i = 0; i < l; i++)
			d[k++] = (char)(' ' + zrand() % 95);
		d[k++] = '\n';
	}
	fd = gnl_file(d, k);
	EXPECT_FD(fd, d, k, "gros fichier");
	close(fd);
}

/* ------------------------------------------------------- other fds ---- */
TEST(GNL_SUITE, "entrée standard (fd 0)")
{
	static const char	d[] = "lu depuis stdin\nligne 2\nfin sans newline";
	int					fd = gnl_file(d, sizeof(d) - 1);

	dup2(fd, 0);
	close(fd);
	EXPECT_FD(0, d, sizeof(d) - 1, "stdin");
}
TEST(GNL_SUITE, "pipe alimenté par petits morceaux")
{
	static char	d[6000];
	size_t		len = 0;
	int			p[2];
	pid_t		pid;

	while (len < 5500)
	{
		size_t	l = zrand() % 80;

		for (size_t i = 0; i < l; i++)
			d[len++] = (char)('a' + zrand() % 26);
		d[len++] = '\n';
	}
	d[len++] = 'z';
	if (pipe(p) < 0)
		zft_failf(__FILE__, __LINE__, "pipe a échoué (tester)");
	pid = fork();
	if (pid == 0)
	{
		size_t	o = 0;

		close(p[0]);
		while (o < len)
		{
			size_t	k = 1 + zrand() % 37;

			if (k > len - o)
				k = len - o;
			if (write(p[1], d + o, k) <= 0)
				_exit(1);
			o += k;
			if (zrand() % 8 == 0)
				usleep(200);
		}
		_exit(0);
	}
	close(p[1]);
	EXPECT_FD(p[0], d, len, "pipe (écritures de 1 à 37 octets)");
	close(p[0]);
	waitpid(pid, NULL, 0);
}
TEST(GNL_SUITE, "fd invalides : -1, non ouvert, fermé")
{
	int		fd = gnl_file("abc\n", 4);
	char	*r;

	close(fd);
	INPUT("get_next_line(-1)");
	r = get_next_line(-1);
	CHECK(r == NULL, "get_next_line(-1) doit renvoyer NULL");
	INPUT("get_next_line(4242) (fd jamais ouvert)");
	r = get_next_line(4242);
	CHECK(r == NULL, "get_next_line(4242) doit renvoyer NULL");
	INPUT("get_next_line(fd fermé)");
	r = get_next_line(fd);
	CHECK(r == NULL, "get_next_line sur un fd fermé doit renvoyer NULL");
}
TEST(GNL_SUITE, "un dossier (read échoue avec EISDIR)")
{
	int		fd = open("/tmp", O_RDONLY);
	char	*r;

	INPUT("get_next_line(open(\"/tmp\"))");
	r = get_next_line(fd);
	CHECK(r == NULL, "lire un dossier doit renvoyer NULL");
	close(fd);
}
TEST(GNL_SUITE, "un fd invalide puis un fichier normal")
{
	static const char	d[] = "premier\nsecond\n";
	int					fd;

	free(get_next_line(-1));
	fd = gnl_file(d, sizeof(d) - 1);
	EXPECT_FD(fd, d, sizeof(d) - 1, "après get_next_line(-1)");
	close(fd);
}

/* --------------------------------------------------------- errors ----- */
#if SMALL_BS
TEST(GNL_SUITE, "read() échoue au milieu : NULL, puis repart de zéro")
{
	/* line 2 is longer than 2 x BUFFER_SIZE, so it needs more read() calls */
	size_t	l2 = (size_t)BS * 2 + 10;
	size_t	tot = 2 + l2 + 1;
	char	*d = gbuf(tot, 'x');
	int		fd;
	char	*r;
	t_gnl	g;
	static const char	d2[] = "nouveau\nfichier\n";

	d[0] = 'a';
	d[1] = '\n';
	d[tot - 1] = '\n';
	fd = gnl_file(d, tot);
	gnl_init(&g, fd, d, tot, "read() renverra -1 après la 1re ligne");
	STEP(&g);
	g_rd_fail_at = g_rd_calls + 1;
	INPUT("2e appel : le prochain read() renvoie -1");
	r = get_next_line(fd);
	g_rd_fail_at = 0;
	if (r && strlen(r) == l2 + 1)
	{
		free(r);	/* the line was already in the stash: no read needed */
		close(fd);
		return ;
	}
	CHECK(r == NULL, "quand read() renvoie -1, get_next_line doit renvoyer "
		"NULL (reçu %s)", zft_str(r));
	close(fd);
	fd = gnl_file(d2, sizeof(d2) - 1);
	EXPECT_FD(fd, d2, sizeof(d2) - 1,
		"nouveau fichier après l'erreur (le reste doit avoir été libéré)");
	close(fd);
}
TEST(GNL_SUITE, "lit le minimum (pas tout le fichier d'un coup)")
{
	size_t	tot = 6 + 300000 + 1;
	char	*d = gbuf(tot, 'x');
	int		fd;
	char	*r;
	size_t	bound = ((6 + (size_t)BS - 1) / (size_t)BS) * (size_t)BS;

	memcpy(d, "short\n", 6);
	d[tot - 1] = '\n';
	fd = gnl_file(d, tot);
	g_rd_bytes = 0;
	INPUT("1er appel sur \"short\\n\" + 300 000 caractères");
	r = get_next_line(fd);
	CHECK(r && !strcmp(r, "short\n"), "1re ligne attendue \"short\\n\", reçu %s",
		zft_str(r));
	CHECK(g_rd_bytes <= bound, "%zu octets lus pour renvoyer une ligne de 6 "
		"octets (max attendu : %zu, soit des read() de BUFFER_SIZE jusqu'au "
		"premier \\n)", g_rd_bytes, bound);
	free(r);
	while ((r = get_next_line(fd)))
		free(r);
	close(fd);
}
#endif

MTEST(GNL_SUITE, "malloc échoue : pas de crash ni de fuite")
{
	static const char	d[] = "abc\ndefgh\nij";
	int					fd = gnl_file(d, sizeof(d) - 1);
	char				*l;

	ARM();
	while ((l = get_next_line(fd)))
		free(l);
	DISARM();
	close(fd);
}

#endif
