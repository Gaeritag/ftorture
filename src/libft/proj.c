/* libft project descriptor (subject v19.3) */
#include "zft.h"
#include "project.h"
#include "libft_decl.h"

#define F(n, p) { #n, NULL, p, (void *)(n) }

static t_func	g_libft_funcs[] = {
	F(ft_isalpha, 1), F(ft_isdigit, 1), F(ft_isalnum, 1), F(ft_isascii, 1),
	F(ft_isprint, 1), F(ft_strlen, 1), F(ft_memset, 1), F(ft_bzero, 1),
	F(ft_memcpy, 1), F(ft_memmove, 1), F(ft_strlcpy, 1), F(ft_strlcat, 1),
	F(ft_toupper, 1), F(ft_tolower, 1), F(ft_strchr, 1), F(ft_strrchr, 1),
	F(ft_strncmp, 1), F(ft_memchr, 1), F(ft_memcmp, 1), F(ft_strnstr, 1),
	F(ft_atoi, 1), F(ft_calloc, 1), F(ft_strdup, 1),
	F(ft_substr, 2), F(ft_strjoin, 2), F(ft_strtrim, 2), F(ft_split, 2),
	F(ft_itoa, 2), F(ft_strmapi, 2), F(ft_striteri, 2), F(ft_putchar_fd, 2),
	F(ft_putstr_fd, 2), F(ft_putendl_fd, 2), F(ft_putnbr_fd, 2),
	F(ft_lstnew, 3), F(ft_lstadd_front, 3), F(ft_lstsize, 3),
	F(ft_lstlast, 3), F(ft_lstadd_back, 3), F(ft_lstdelone, 3),
	F(ft_lstclear, 3), F(ft_lstiter, 3), F(ft_lstmap, 3),
};

t_project	g_proj = {
	"libft", "libft.a", "libft.h",
	"LIBFT · SUJET v19.3 · LINUX",
	"> ta libft va parler",
	g_libft_funcs,
	(int)(sizeof(g_libft_funcs) / sizeof(g_libft_funcs[0])),
	3,
	{"", "Fonctions de la libc", "Fonctions supplémentaires",
		"Listes chaînées"},
	{"", "libc", "supplémentaires", "listes chaînées"},
};
