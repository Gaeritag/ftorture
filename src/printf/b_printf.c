/* ************************************************************************** */
/*  Efficiency workload of ft_printf (--efficiency), mandatory conversions.  */
/*  The EXPECTED ft_printf below is plain and sensible: one write() per run  */
/*  of plain text and one per conversion (not one per character), no         */
/*  allocation. Compiled like the tester (-O0). 100% = as fast as it.       */
/* ************************************************************************** */
#include <stdarg.h>
#include <stdint.h>
#include "pf.h"

static int	put(const char *s, size_t n)
{
	return (write(1, s, n) < 0 ? -1 : (int)n);
}

static int	put_num(uintmax_t v, unsigned base, int upper, const char *pre,
				int neg)
{
	char		b[40];
	int			i = 40;
	const char	*dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	size_t		k;

	do
	{
		b[--i] = dig[v % base];
		v /= base;
	} while (v);
	for (k = strlen(pre); k > 0; k--)
		b[--i] = pre[k - 1];
	if (neg)
		b[--i] = '-';
	return (put(b + i, (size_t)(40 - i)));
}

static int	conv(char c, va_list *ap)
{
	if (c == 'c')
	{
		char	ch = (char)va_arg(*ap, int);

		return (put(&ch, 1));
	}
	if (c == 's')
	{
		const char	*s = va_arg(*ap, const char *);

		if (!s)
			s = "(null)";
		return (put(s, strlen(s)));
	}
	if (c == 'p')
	{
		void	*p = va_arg(*ap, void *);

		if (!p)
			return (put("(nil)", 5));
		return (put_num((uintptr_t)p, 16, 0, "0x", 0));
	}
	if (c == 'd' || c == 'i')
	{
		long	v = va_arg(*ap, int);

		return (put_num((uintmax_t)(v < 0 ? -v : v), 10, 0, "", v < 0));
	}
	if (c == 'u')
		return (put_num(va_arg(*ap, unsigned), 10, 0, "", 0));
	if (c == 'x' || c == 'X')
		return (put_num(va_arg(*ap, unsigned), 16, c == 'X', "", 0));
	if (c == '%')
		return (put("%", 1));
	return (0);
}

static int	e_printf(const char *fmt, ...)
{
	va_list		ap;
	int			n = 0;
	int			r;
	const char	*run;

	va_start(ap, fmt);
	while (*fmt)
	{
		run = fmt;
		while (*fmt && *fmt != '%')
			fmt++;
		if (fmt > run && (r = put(run, (size_t)(fmt - run))) >= 0)
			n += r;
		if (*fmt == '%' && fmt[1])
		{
			r = conv(fmt[1], &ap);
			if (r < 0)
				return (va_end(ap), -1);
			n += r;
			fmt += 2;
		}
		else if (*fmt)
			fmt++;
	}
	va_end(ap);
	return (n);
}

BENCH(ft_printf)
{
	static int	x = 42;

	BENCH_LOOP
	{
		SINK(PICK(ft_printf, e_printf)("Hello %s, you are %d years old!\n",
				"world", 42));
		SINK(PICK(ft_printf, e_printf)("%c%c%c %i %u %x %X %%\n", 'a', 'b',
				'c', -2147483647 - 1, 4294967295u, 0xdeadbeefu, 255u));
		SINK(PICK(ft_printf, e_printf)("ptr %p and %p, %s\n", (void *)&x,
				(void *)0, (char *)0));
		SINK(PICK(ft_printf, e_printf)("a fairly long line of plain text "
				"without any conversion, like most of a real output\n"));
	}
}
