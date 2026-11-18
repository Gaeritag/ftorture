/* ************************************************************************** */
/*  Reference implementations ("la vraie fonction").                          */
/*  Part 1: the real glibc functions, called through volatile pointers so     */
/*  the compiler cannot fold or "optimise away" calls such as strlen(NULL).   */
/*  strlcpy/strlcat/strnstr do not exist in every glibc: faithful copies of   */
/*  the BSD algorithms are used instead (same as <bsd/string.h>).             */
/*  Parts 2/3 have no libc original: the reference is a straightforward,      */
/*  unprotected implementation — it crashes on NULL like a libc would.        */
/* ************************************************************************** */
#ifndef REFS_H
# define REFS_H

# include <stddef.h>
# include "libft_decl.h"

extern int		(*volatile r_isalpha)(int);
extern int		(*volatile r_isdigit)(int);
extern int		(*volatile r_isalnum)(int);
extern int		(*volatile r_isascii)(int);
extern int		(*volatile r_isprint)(int);
extern size_t	(*volatile r_strlen)(const char *);
extern void		*(*volatile r_memset)(void *, int, size_t);
extern void		(*volatile r_bzero)(void *, size_t);
extern void		*(*volatile r_memcpy)(void *, const void *, size_t);
extern void		*(*volatile r_memmove)(void *, const void *, size_t);
extern size_t	(*volatile r_strlcpy)(char *, const char *, size_t);
extern size_t	(*volatile r_strlcat)(char *, const char *, size_t);
extern int		(*volatile r_toupper)(int);
extern int		(*volatile r_tolower)(int);
extern char		*(*volatile r_strchr)(const char *, int);
extern char		*(*volatile r_strrchr)(const char *, int);
extern int		(*volatile r_strncmp)(const char *, const char *, size_t);
extern void		*(*volatile r_memchr)(const void *, int, size_t);
extern int		(*volatile r_memcmp)(const void *, const void *, size_t);
extern char		*(*volatile r_strnstr)(const char *, const char *, size_t);
extern int		(*volatile r_atoi)(const char *);
extern void		*(*volatile r_calloc)(size_t, size_t);	/* libc memory! */
extern char		*(*volatile r_strdup)(const char *);	/* libc memory! */

extern char		*(*volatile r_substr)(char const *, unsigned int, size_t);
extern char		*(*volatile r_strjoin)(char const *, char const *);
extern char		*(*volatile r_strtrim)(char const *, char const *);
extern char		**(*volatile r_split)(char const *, char);
extern char		*(*volatile r_itoa)(int);
extern char		*(*volatile r_strmapi)(char const *, char (*)(unsigned int, char));
extern void		(*volatile r_striteri)(char *, void (*)(unsigned int, char *));
extern void		(*volatile r_putchar_fd)(char, int);
extern void		(*volatile r_putstr_fd)(char *, int);
extern void		(*volatile r_putendl_fd)(char *, int);
extern void		(*volatile r_putnbr_fd)(int, int);

extern t_list		*(*volatile r_lstnew)(void *);
extern void			(*volatile r_lstadd_front)(t_list **, t_list *);
extern unsigned int	(*volatile r_lstsize)(t_list *);
extern t_list		*(*volatile r_lstlast)(t_list *);
extern void			(*volatile r_lstadd_back)(t_list **, t_list *);
extern void			(*volatile r_lstdelone)(t_list *, void (*)(void *));
extern void			(*volatile r_lstclear)(t_list **, void (*)(void *));
extern void			(*volatile r_lstiter)(t_list *, void (*)(void *));
extern t_list		*(*volatile r_lstmap)(t_list *, void *(*)(void *),
						void (*)(void *));

#endif
