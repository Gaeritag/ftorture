# 🔥 ftorture — libft tester (subject v19.3)

🇫🇷 [Version française](README.fr.md)

> *Your libft will talk.*

**ftorture** puts your libft to the question: every function runs in its own process,
face to face with the **real** libc function. It watches how it crashes, what it leaks,
and what it reads outside its memory.

Inspired by [Caesarovich/zft](https://github.com/Caesarovich/zft), rewritten in C:
nothing to download, you only need `cc`, `make` and `nm`. Built for **Linux** (42 machines).

## Installation

```bash
git clone https://github.com/HorusExE69/ftorture.git ~/ftorture
cd ~/ftorture && bash install.sh
source ~/.zshrc        # or ~/.bashrc
```

`install.sh` adds the **`torture`** command to bash, zsh and fish.

## Usage

```bash
torture                       # inside your libft folder
torture ~/libft               # or give the path
torture -f split,itoa -v      # a few functions, with every detail
torture -p 3                  # only part 3 (linked lists)
torture -s                    # strict mode: ▲ count as errors
```

| Option | Effect |
|---|---|
| `-s`, `--speculative` | speculative errors (▲) make the run fail |
| `-v`, `--verbose` | show the details of every failure |
| `-f`, `--func LIST` | `ft_split,itoa,strlcat` (the `ft_` prefix is optional) |
| `-p`, `--part N` | part 1, 2 or 3 |
| `--no-checks` | skip Makefile / norm / nm / prototype checks |
| `--no-norm` | skip norminette |
| `--seed N` | fuzzing seed (default 42) |
| `--timeout S` | per-test timeout (default 3 s) |
| `--no-color` | no colors |

A full plain-text report is written to `ftorture.log`, in the ftorture folder. Nothing
is ever written inside your libft, so no extra files end up in your submission.

> The terminal output is in French.

## What gets checked

| Area | Details |
|---|---|
| **Makefile** | rules `$(NAME) all clean fclean re`, `cc`, `-Wall -Wextra -Werror` on every file (read with `make -nB`, even with `@`), `ar` and not libtool, no `-std=c99`, **relink**, `clean` / `fclean` |
| **Files** | `libft.h` and `ft_*.c` at the root, extra files, `README.md` (italic first line, Description / Instructions / Resources sections) |
| **Code** | norminette (if installed), **forbidden external functions** (anything but `malloc`, `free`, `write`, with the offending file), **global variables**, non-`static` helper functions, all 43 functions present |
| **Prototypes** | every prototype in your `libft.h` compared by the compiler against the **Linux man pages** / subject v19.3, exact `t_list` struct, `libft.h` compiles on its own |
| **Tests** | about 400 tests, plus fuzzing against the real glibc (or BSD for `strlcpy`, `strlcat`, `strnstr`) |

## The torture tools

- **One process per test** (`fork`): a crash doesn't stop the other tests.
- **Guarded memory**: test strings sit right before a forbidden page, so any read past
  the end crashes (`strlcat`, `strnstr`, `substr`, `memchr`…).
- **Instrumented malloc** (`-Wl,--wrap=malloc,--wrap=free`):
  - one byte written outside a block: crash
  - use after `free`: crash; **double free**: reported
  - **leaks**: any block not freed at the end of the test
  - allocation size checked (`strdup`, `itoa`, `split`…)
  - fresh blocks filled with `0xA5`: a `calloc` that doesn't zero shows up
  - **malloc protection** (`M`): the test is replayed with the 1st, 2nd, 3rd… malloc
    failing. Your function must return `NULL` with no crash and no leak (essential for
    `ft_split` and `ft_lstmap`).

## Your function vs the real one

For twisted inputs (NULL, `n = 0`, overflows…), ftorture runs **your** function and the
**real** one in two separate processes, then compares:

| yours | the real one | verdict |
|---|---|---|
| crashes | crashes | ✔ OK: same behavior as the original |
| crashes | doesn't crash | ✘ **error**, even without `-s` (e.g. `ft_memcpy(NULL, NULL, 0)`) |
| doesn't crash | crashes | ▲ **speculative**: you protect where the libc doesn't |
| doesn't crash | doesn't crash | results must be identical |

Parts 2 and 3 have no libc original: the reference is then a simple implementation
with no NULL protection.

▲ are warnings: they don't make ftorture fail, except with `-s`. It's up to you to
defend them during your evaluation.

## Legend

| | |
|---|---|
| `✔` | ok |
| `✔` (cyan) | crashed exactly like the libc (accepted) |
| `✘` | error |
| `S` | crash (segfault, bus error, abort) |
| `T` | timeout, probably an infinite loop |
| `L` | memory leak |
| `M` | malloc protection test |
| `▲` | speculative |

## Adding a test

Tests live in `src/t_part1.c`, `src/t_part2.c` and `src/t_part3.c`.

```c
TEST(ft_strlen, "my test")
{
	EQ_SZ(ft_strlen("abc"), 3);
}

VS(ft_strlen, "compared to the libc")
{
	OUT("%zu", PICK(ft_strlen, r_strlen)("abc"));
}

MTEST(ft_strdup, "protected malloc")
{
	ARM();
	char *d = ft_strdup("x");
	DISARM();
	if (FAILING)
		CHECK(!d, "must return NULL");
	free(d);
}
```

- **Assertions**: `CHECK`, `EQ_INT`, `EQ_SZ`, `EQ_PTR`, `EQ_STR`, `EQ_MEM`
- **Helpers**: `gstr`, `gmem`, `gbuf` (guarded memory), `alloc_size`,
  `cap_open` / `cap_read` (fd output), `INPUT` (describes the input on failure)
- `VS_SPEC`: like `VS`, but a different result is only speculative

## License

GPL-3.0, like the original project.
