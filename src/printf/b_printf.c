/* ************************************************************************** */
/*  Efficiency workload of ft_printf (--efficiency), mandatory conversions.  */
/*  COST() is the minimal work of one repetition (4 calls): read the format  */
/*  once, produce the digits, copy the output (~200 bytes) into a buffer and */
/*  send it with ONE write() per call. The engine prices it on this machine. */
/* ************************************************************************** */
#include "pf.h"

BENCH(ft_printf)
{
	static int	x = 42;

	/* format: 31 + 22 + 18 + 88 chars; 9 conversions; 44 digits (decimal,
	   hex, the address); output: 35 + 41 + ~35 + 88 bytes                   */
	COST(.calls = 4, .scan = 159, .ops = 45, .digits = 44, .copy = 200,
		.sys = 4);
	BENCH_LOOP
	{
		SINK(ft_printf("Hello %s, you are %d years old!\n", "world", 42));
		SINK(ft_printf("%c%c%c %i %u %x %X %%\n", 'a', 'b', 'c',
				-2147483647 - 1, 4294967295u, 0xdeadbeefu, 255u));
		SINK(ft_printf("ptr %p and %p, %s\n", (void *)&x, (void *)0,
				(char *)0));
		SINK(ft_printf("a fairly long line of plain text without any "
				"conversion, like most of a real output\n"));
	}
}
