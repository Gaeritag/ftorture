/* ************************************************************************** */
/*  push_swap suites: run the student's binaries, replay the operations on   */
/*  a built-in stack simulator, generate inputs.                             */
/* ************************************************************************** */
#ifndef PS_H
# define PS_H

# include "zft.h"

# define PS_MAX 1024		/* largest input a suite generates               */

/* one execution of ./push_swap or ./checker                                */
typedef struct s_proc
{
	char	*out;		/* stdout, NUL-terminated                            */
	size_t	outlen;
	char	*err;		/* stderr, NUL-terminated                            */
	size_t	errlen;
	int		status;		/* exit status, or -1 when killed                    */
	int		sig;		/* signal that killed it, 0 if none                  */
}	t_proc;

/* result of replaying an operation list                                    */
typedef struct s_replay
{
	int		count;		/* operations read                                   */
	int		bad;		/* 1: a line is not an operation                     */
	int		bad_line;	/* 1-based number of that line                       */
	char	bad_text[48];
	int		no_final_nl;/* output does not end with '\n'                     */
	int		sorted;		/* a sorted ascending and b empty                    */
	int		b_left;		/* elements left in b                                */
}	t_replay;

/* runs dir/prog with args (NULL-terminated), stdin = in (may be NULL)       */
void		ps_exec(const char *prog, const char *const *args, const char *in,
				size_t inlen, t_proc *p);
void		ps_free(t_proc *p);
/* replays ops on v[0..n-1] (v[0] = top of a)                               */
void		ps_replay(const int *v, int n, const char *ops, size_t len,
				t_replay *r);

/* inputs                                                                   */
void		ps_random(int *v, int n);			/* distinct, shuffled        */
void		ps_sorted(int *v, int n);			/* distinct, ascending       */
void		ps_disorder(int *v, int n, double lo, double hi);
double		ps_disorder_of(const int *v, int n);
/* argv built from flags (may be NULL) followed by the numbers             */
const char	**ps_args(const char *flag1, const char *flag2, const int *v,
				int n);
/* input shown when a test fails                                            */
void		ps_input(const char *flag, const int *v, int n);

/* runs push_swap, fails the test unless the output sorts v; returns the
   number of operations. flag / flag2 may be NULL.                         */
int			ps_sorts(const char *flag, const char *flag2, const int *v, int n);
/* same, keeps the process output (free it with ps_free)                    */
int			ps_sorts_keep(const char *flag, const char *flag2, const int *v,
				int n, t_proc *p);
/* runs push_swap with raw arguments, fails unless it prints exactly
   "Error\n" on stderr and nothing on stdout                                */
void		ps_error(const char *const *args);
/* ./checker v... with ops on stdin                                         */
void		ps_checker(const char *const *args, const char *ops, t_proc *p);
/* official checker from FTORTURE_PS_CHECKER: 1 OK, 0 KO, -1 none         */
int			ps_official(const int *v, int n, const char *ops, size_t len);
/* ps_sorts on the default strategy, then also checked by checker_linux     */
int			ps_sorts_official(const int *v, int n);

/* French / English message                                                 */
# define T(fr, en) (zft_en() ? (en) : (fr))

#endif
