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

## Run targeted tests first

```bash
torture libft  /path/to/libft -f strlen,split -v
torture printf /path/to/ft_printf -p 1 -f %d,%s -v
torture gnl    /path/to/get_next_line -p 1 -f 1,42
```

Omit `-f` and `-p` for the full run. With no arguments in a terminal, the interactive menu asks for project, path, and options. `-f` filters function names for libft, conversions/flags for printf, and comma-separated `BUFFER_SIZE` values for GNL (`def` means the default size). `-p 1` selects mandatory; `-p 2` selects bonus (libft also has part 3). Use `--no-checks` to skip subject checks, `--no-norm` to skip only norminette, `--seed N` for reproducible fuzzing, `--timeout S` for slow cases, and `--no-color` for captured output. See `torture --help` for the exact options supported by this checkout.

## Interpret and report

- `✔` passes; cyan `✔` means a crash matching the reference and is accepted. `✘` fails; `S` is a crash, `T` a timeout, `L` a leak, and `M` a malloc-failure protection failure.
- `▲` is speculative (undefined or subject-dependent behavior). It is a warning by default; `-s` makes it fail. Never present a speculative result as a guaranteed subject violation without checking the specification.
- Check whether a failure is in **checks** (Makefile, submission files, prototypes, norm, symbols) or in a **function test**. Use `-v` and inspect `ftorture.log` in the ftorture checkout for full uncolored details; build/link diagnostics are in `build/<project>/`.
- For printf, `bonus` requires a Makefile bonus rule. For GNL, bonus requires all `_bonus` sources. An absent bonus is not a tested pass. If building fails, report the compiler/Makefile error before drawing conclusions about behavior.
- Include the command, project path, relevant subject version, failing check/test and input, expected versus actual result, whether it is speculative, and the smallest reproducible rerun. Do not edit the student's code unless explicitly asked.
