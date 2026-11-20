---
name: ftorture-42-projects
description: Run ftorture against 42 libft, ft_printf, and get_next_line projects; interpret results and report actionable failures without changing student code.
---

# Test a 42 project with ftorture

Use this skill when asked to test, diagnose, or explain results for a 42 **libft**, **ft_printf**, or **get_next_line** submission. Other projects are not supported yet. The tester targets Linux and the subject versions documented in `README.md`; check the student's subject/version before treating a mismatch as a defect.

## Before running

1. Locate the student's project and verify its expected header (`libft.h`, `ft_printf.h`, or `get_next_line.h`). libft and ft_printf also need a Makefile; GNL does not.
2. Check the student's working tree and ask before running if build artifacts or uncommitted work must be preserved. The tester runs `make fclean`, builds `all`/`bonus`, and runs `make clean` for libft/printf. It may replace archives and remove object files. GNL is built in ftorture's `build/` directory instead.
3. From the ftorture checkout use `bash /path/to/ftorture/torture.sh …`; if installed, use `torture …`. Dependencies: `cc`, `make`, `nm`; `norminette` is optional. No network or elevated privileges are needed to run tests.
4. `bash install.sh` links `~/.local/bin/torture` and appends `export PATH="$HOME/.local/bin:$PATH"` to `~/.zshrc` / `~/.bashrc` (and `fish_add_path` for fish) when missing. If `torture` is "not found" right after installing, the current shell simply has not reloaded its startup file: open a new terminal or `source ~/.zshrc`; do not tell the user to edit `PATH` by hand. `bash install.sh --update` fast-forwards a clean `main` checkout.

## Run targeted tests first

```bash
torture libft  /path/to/libft -f strlen,split -v
torture printf /path/to/ft_printf -p 1 -f %d,%s -v
torture gnl    /path/to/get_next_line -p 1 -f 1,42
```

Omit `-f` and `-p` for the full run. With no arguments in a terminal, the interactive menu asks for project, path, and options. `-f` filters function names for libft, conversions/flags for printf, and comma-separated `BUFFER_SIZE` values for GNL (`def` means the default size). `-p 1` selects mandatory; `-p 2` selects bonus (libft also has part 3). Use `--no-checks` to skip subject checks, `--no-norm` to skip only norminette, `--no-hidden` to ignore dot-files and dot-folders (`.vscode`, `.DS_Store`…) in the file and include checks, `--seed N` for reproducible fuzzing, `--timeout S` for slow cases, `-e [N]` for the efficiency gauge (target N%, default 80), `--lang en|fr` for the output language, and `--no-color` for captured output. The menu has a box for `-s`, `-v`, `-e`, checks, norminette and `--no-hidden`. See `torture --help` for the exact options supported by this checkout.

## Interpret and report

- `✔` passes; cyan `✔` means a crash matching the reference and is accepted. `✘` fails; `S` is a crash, `T` a timeout, `L` a leak, and `M` a malloc-failure protection failure.
- `▲` is speculative (undefined or subject-dependent behavior). It is a warning by default; `-s` makes it fail. Never present a speculative result as a guaranteed subject violation without checking the specification.
- Check whether a failure is in **checks** (Makefile, submission files, prototypes, norm, symbols) or in a **function test**. Use `-v` and inspect `ftorture.log` in the ftorture checkout for full uncolored details; build/link diagnostics are in `build/<project>/`.
- For printf, `bonus` requires a Makefile bonus rule. For GNL, bonus requires all `_bonus` sources. An absent bonus is not a tested pass. If building fails, report the compiler/Makefile error before drawing conclusions about behavior.
- **Extra files are KO** (not a warning): anything that is not a file to submit, hidden files except `.git`/`.gitignore`, unexpected extensions (`.py`, `.cpp`, `.txt`, `.o`, `a.out`, `.swp`…), empty folders. ft_printf may keep a `libft/` folder (`*.c`, `*.h`, `Makefile`). If the only KO is an editor folder the student will not submit, rerun with `--no-hidden` to confirm the rest.
- **Unused `#include` are KO**: a `<lib.h>` must give the file at least one name it uses (function, type, macro, enum constant, as the preprocessor sees the header); a project `"x.h"` must give one of its own declarations or of the libs it includes; a header's includes also count for the files that include it. `NULL`, `size_t`, `ssize_t`, `va_list`… only justify the **first** include of a file that gives them, so `<stdio.h>` kept only for `NULL` is reported. Duplicate includes are KO too.
- **GNL with `BUFFER_SIZE=1`**: the long-line tests (#16, 100 000 chars; #25, `"short\n"` + 300 000 chars) end in `T` with the default GNL timeout (10 s) when the stash is rebuilt by a `strjoin` after every `read()` (quadratic). That is a timeout, not a crash: rerun with `--timeout 480` to show the result is correct, and explain the quadratic cost. (An old memtrack false positive, "free() d'un pointeur au milieu d'un bloc malloc" after ~300 000 mallocs, is fixed; if it ever reappears, suspect the tracker before the student.)
- **Efficiency (`-e`)**: each function line gets `⚡ N%` and the result box ends with a gauge (geometric mean, each value clamped to [1%, 200%]) with a `┃` mark at the target and the functions below it. N% = expected time / student time, where "expected" is a plain, sensible version compiled `-O0` like the tester (not the glibc): one pass, no work done twice, one `write()` per string/number/text run (not per character), a linear GNL stash. It is information only and never makes a run KO; runs differ by under ~5%. Typical findings: `ft_putstr_fd`/`ft_printf` writing char by char (1–20%), an `ft_lstmap` or `ft_split` that rescans (quadratic), a GNL `strjoin` stash (1–2%). GNL is not measured without `-D BUFFER_SIZE`. `FTORTURE_BENCH_DEBUG=1` prints raw times per repetition. Present a low score as a performance remark, never as a subject violation.
- Include the command, project path, relevant subject version, failing check/test and input, expected versus actual result, whether it is speculative, and the smallest reproducible rerun. Do not edit the student's code unless explicitly asked.
