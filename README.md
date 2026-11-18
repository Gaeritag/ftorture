# 🔥 ftorture — 42 tester: libft & ft_printf

🇫🇷 [Version française](README.fr.md)

> *Your code will talk.*

**ftorture** puts your project to the question: every test runs in its own process,
face to face with the **real** libc function. It watches how it crashes, what it leaks,
and what it reads outside its memory.

| Project | Subject | What is tested |
|---|---|---|
| **libft** | v19.3 | all 43 functions (parts 1, 2 and 3), ~400 tests + fuzzing |
| **ft_printf** | v12.1 | `cspdiuxX%` + bonus `-0.`, width, `# +`, ~22,000 calls compared with `printf` |

Inspired by [Caesarovich/zft](https://github.com/Caesarovich/zft), rewritten in C:
nothing to download, you only need `cc`, `make` and `nm`. Built for **Linux** (42 machines).

## Installation

```bash
git clone https://github.com/HorusExE69/ftorture.git ~/ftorture
cd ~/ftorture && bash install.sh
source ~/.zshrc        # or ~/.bashrc
```

`install.sh` adds the **`torture`** command to bash, zsh and fish.

> The terminal output is in French.

## Usage

### Interactive menu

```bash
torture
```

The logo stays at the top of every screen:

1. **project**: `LIBFT` or `FT_PRINTF` (`↑ ↓` or `1` `2`, Enter). If the current folder
   contains a `libft.h` or a `ft_printf.h`, it is preselected.
2. **path** to the project (Tab completes).
3. **options** to tick (Space): strict mode, verbose, checks, norminette, then
   `▶ LANCER LA TORTURE`.

### Command line

```bash
torture libft ~/libft                # a libft
torture printf ~/ft_printf           # a ft_printf (mandatory + bonus)
torture ~/ft_printf -s               # project detected automatically
torture libft -f split,itoa -v       # a few functions, with every detail
torture printf -f %d,"flag -"        # a few conversions / flags
torture printf -p 1                  # mandatory part only
```

| Option | Effect |
|---|---|
| `-s`, `--speculative` | speculative errors (▲) make the run fail |
| `-v`, `--verbose` | show the details of every failure |
| `-f`, `--func LIST` | libft: `ft_split,itoa` · printf: `%d,%s,flag -,précision .` |
| `-p`, `--part N` | libft: 1, 2 or 3 · printf: 1 (mandatory) or 2 (bonus) |
| `--no-checks` | skip Makefile / norm / nm / prototype checks |
| `--no-norm` | skip norminette |
| `--seed N` | fuzzing seed (default 42) |
| `--timeout S` | per-test timeout (default 3 s) |
| `--no-color` | no colors |

At the end, ftorture runs your project's `make clean`, even after an error or a Ctrl-C:
no `.o` file is left behind. A full plain-text report is written to `ftorture.log`, in
the ftorture folder, never inside your project.

## Checks (both projects)

| Area | Details |
|---|---|
| **Makefile** | rules `$(NAME) all clean fclean re` (+ `bonus` for printf), `cc`, `-Wall -Wextra -Werror` on every file (read with `make -nB`, even with `@`), `ar` and not libtool, no `-std=c99`, **relink** (including `make bonus`), `clean` / `fclean` |
| **Files** | files to submit, extra files, `README.md` (italic first line, Description / Instructions / Resources, + algorithm explanation for printf) |
| **Code** | norminette (if installed), **forbidden external functions** (anything but `malloc`, `free`, `write`, with the offending file), **global variables**, non-`static` helpers (libft), functions present in the archive |
| **Prototypes** | every prototype in the header compared by the compiler against the **Linux man pages** / subject, exact `t_list` (libft), header compiles on its own |

## libft

- **Guarded memory**: test strings sit right before a forbidden page, so any read past
  the end crashes (`strlcat`, `strnstr`, `substr`, `memchr`…).
- **Instrumented malloc** (`-Wl,--wrap=malloc,--wrap=free`):
  - one byte written outside a block: crash; use after `free`: crash
  - **double free**: reported; **leaks**: any block not freed at the end of the test
  - allocation size checked (`strdup`, `itoa`, `split`…)
  - fresh blocks filled with `0xA5`: a `calloc` that doesn't zero shows up
  - **malloc protection** (`M`): the test is replayed with the 1st, 2nd, 3rd… malloc
    failing; it must return `NULL` with no crash and no leak (`ft_split`, `ft_lstmap`)
- **Fuzzing** against glibc (or BSD for `strlcpy`, `strlcat`, `strnstr`).

## ft_printf

Each call runs **your `ft_printf`**, then the **real `printf`** with the same
arguments, each with its output redirected to a file. ftorture compares the **output
byte by byte** (including the `\0` of `%c`) and the **return value**.

- **Mandatory part** (archive from `make`): every conversion `c s p d i u x X %`
  (limit values, `NULL` → `(null)` / `(nil)`, all 256 characters, `INT_MIN`,
  `UINT_MAX`, 64-bit pointers…), mixes, 24 arguments, outputs over 10,000 bytes, and
  fuzzing.
- **Bonus** (archive from `make bonus`, tested in a second pass): each flag (`-`, `0`,
  `.`, width, `#`, space, `+`), their interactions (`'-'` cancels `'0'`, `'+'` beats
  `' '`, `.0` with value 0…) and **5,000 random combinations** that are valid in C.
- **Undefined cases** (`%05s`, `%.3c`, `%#d`, `%+u`, `%s` NULL with precision, flags on
  `%%`…): ▲ only, when they differ from glibc.
- **Memory**: leaks detected, and failing mallocs (no crash, no leak).
- **`NULL` format**: compared with the real function's crash (rule below).

If your Makefile has no `bonus` rule, only the mandatory part is tested and the summary
says so.

## Your function vs the real one

For twisted inputs (NULL, `n = 0`, overflows…), ftorture runs **your** function and the
**real** one in two separate processes, then compares:

| yours | the real one | verdict |
|---|---|---|
| crashes | crashes | ✔ OK: same behavior as the original |
| crashes | doesn't crash | ✘ **error**, even without `-s` (e.g. `ft_memcpy(NULL, NULL, 0)`) |
| doesn't crash | crashes | ▲ **speculative**: you protect where the libc doesn't |
| doesn't crash | doesn't crash | results must be identical |

For libft parts 2 and 3 (no libc original), the reference is a simple implementation
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

```
src/engine.c, memtrack.c, zft.h   shared engine (fork, guarded memory, report)
src/libft/                        libft tests (t_part1-3.c) + references
src/printf/                       ft_printf tests (t_printf.c, t_printf_bonus.c)
protos/                           expected prototypes
```

**libft:**

```c
TEST(ft_strlen, "my test")
{
	EQ_SZ(ft_strlen("abc"), 3);
}

VS(ft_strlen, "compared to the libc")
{
	OUT("%zu", PICK(ft_strlen, r_strlen)("abc"));
}
```

**ft_printf** (`pf_d`, `pf_s`, `pf_minus`…: the suite the test belongs to):

```c
TEST(pf_d, "my test")
{
	PF("%d|%d", 42, -42);       /* same output + same return value as printf */
}

SPEC_TEST(pf_undef, "undefined case")
{
	PF("%05s", "ab");           /* ▲ only if it differs from glibc */
}
```

- **Assertions**: `CHECK`, `EQ_INT`, `EQ_SZ`, `EQ_PTR`, `EQ_STR`, `EQ_MEM`, `PF`
- **Variants**: `SPEC_TEST` (▲), `VS` / `VS_SPEC` (compared to the real function),
  `MTEST` (failing malloc, between `ARM()` and `DISARM()`)
- **Helpers**: `gstr`, `gmem`, `gbuf` (guarded memory), `alloc_size`,
  `cap_open` / `cap_read`, `INPUT` (describes the input on failure)

## License

GPL-3.0, like the original project.
