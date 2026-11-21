/* ************************************************************************** */
/*  Efficiency workload of get_next_line (--efficiency), one per BUFFER_SIZE. */
/*  Reads a whole file (400 lines of 0..120 chars + one line of 20 000).     */
/*  COST() is the minimal work of one pass, for this BUFFER_SIZE: one read() */
/*  per BUFFER_SIZE bytes (+ the one that returns 0), every byte brought,    */
/*  searched for '\n' and copied ONCE into its line, one malloc per line     */
/*  (freed by the harness), one read buffer. A stash rebuilt by a strjoin    */
/*  after every read(), or moved after every line, does more than that.      */
/*  Not measured without -D BUFFER_SIZE (your default value is unknown).     */
/* ************************************************************************** */
#include <fcntl.h>
#include "gnl.h"

#if Z_BS > 0

static size_t	g_len;
static int		g_lines;

static int	bench_fd(void)
{
	static int	fd = -1;
	static char	data[400 * 122 + 20002];
	int			i;
	int			k;

	if (fd >= 0)
		return (fd);
	for (i = 0; i < 400; i++)
	{
		int	l = (int)(zrand() % 121);

		for (k = 0; k < l; k++)
			data[g_len++] = (char)('a' + (i + k) % 26);
		data[g_len++] = '\n';
	}
	for (k = 0; k < 20000; k++)
		data[g_len++] = (char)('A' + k % 26);
	data[g_len++] = '\n';
	g_lines = 401;
	fd = gnl_file(data, g_len);
	return (fd);
}

BENCH(GNL_SUITE)
{
	int		fd = bench_fd();
	double	reads = (double)((g_len + (size_t)Z_BS - 1) / (size_t)Z_BS) + 1;
	char	*l;

	/* + 1 system call: the harness lseek() back to the start */
	COST(.calls = g_lines + 1, .sys = reads + 1, .rbytes = (double)g_len,
		.scan = (double)g_len, .copy = (double)g_len, .allocs = g_lines
		+ (Z_BS < (1 << 20)), .big_allocs = Z_BS >= (1 << 20));
	BENCH_LOOP
	{
		lseek(fd, 0, SEEK_SET);
		while ((l = get_next_line(fd)))
		{
			SINK(l[0]);
			free(l);
		}
	}
}

#endif
