/* ************************************************************************** */
/*  Project descriptor: which suites exist, in which part, and how the       */
/*  project is named. One proj.c per project (src/libft, src/printf).        */
/* ************************************************************************** */
#ifndef PROJECT_H
# define PROJECT_H

typedef struct s_func
{
	const char	*name;		/* key used by TEST(name, ...)                  */
	const char	*label;		/* shown in the report (NULL = name)            */
	int			part;		/* 1..3                                         */
	void		*addr;		/* function under test, NULL = missing          */
}	t_func;

typedef struct s_project
{
	const char	*name;			/* "libft", "ft_printf"                     */
	const char	*archive;		/* "libft.a", "libftprintf.a"               */
	const char	*header;		/* "libft.h", "ft_printf.h"                 */
	const char	*subtitle;		/* banner line                              */
	const char	*tagline;		/* "> ta libft va parler"                   */
	t_func		*funcs;
	int			nfuncs;
	int			nparts;
	const char	*part_names[4];	/* section titles                           */
	const char	*short_names[4];/* summary labels (<= 16 visible chars)     */
}	t_project;

extern t_project	g_proj;

#endif
