---
name: ftorture-42-projects
description: Use the Python-native ftorture CLI to test 42 projects, diagnose build/check/test failures, and report reproducible results without changing student code.
---

# Test a 42 project

Use this skill to run ftorture, not to modify its test framework. The Python CLI delegates to registered project modules and the C engine; do not invoke internal modules or generated binaries as the normal entry point.

## Discover and prepare

```bash
torture --list-projects
torture --help
```

- Current modules support **libft v19.3**, **ft_printf v12.1**, and **get_next_line v14.3**. Verify the student's actual subject before interpreting a mismatch. The registry in `ftorture/projects/__init__.py` is authoritative for this checkout.
- User suites are discovered from `${XDG_CONFIG_HOME:-$HOME/.config}/ftorture/tests/*.json` or `--custom-tests-dir DIR` and listed as `custom-<key>` / `[CUSTOM]`. They are user-owned commands, not official subject checks; only run trusted manifests. See `docs/custom-tests.md`. They require no C tools unless their own commands do.
- Requirements: Linux, Python 3.10+, a C compiler, and the project's build tools (`make`/`nm` for archive projects; `nm` for GNL). Norminette is optional. There are no Python package dependencies.
- Install with `python3 install.py`, then use the installed `torture` command from any directory. The installer writes a Python launcher directly to `~/.local/bin/torture`, detects the running shell and checks its startup file. If PATH setup is missing it shows instructions and asks before editing; declining or non-interactive installation never edits shell configuration. If `torture` is "not found" right after accepting PATH setup, the current shell has not reloaded its startup file yet (open a new terminal or `source` the shell startup file). Keep the checkout in place for the launcher's Python code and C suites. No aliases, shell wrappers, network access, or root permissions are needed for tests; direct Python entry points are development/test interfaces, not the normal user workflow.
- Confirm the student directory and expected header. libft/printf require a Makefile; GNL is compiled from sources. Inspect the student's working tree: archive builds/checks run `make fclean`, `all`/`bonus`, and `clean`, replacing archives and removing objects. Ask first if existing artifacts must be preserved. Do not change student source unless explicitly asked.

## Run and narrow failures

Use explicit project and directory arguments for automation; quote paths with spaces. With no project argument in a terminal, the original retro menu offers project selection (arrows, j/k, or 1/2/3), an editable path with Tab completion, and option checkboxes (`-s`, `-v`, `-e`, checks, norminette, and `--no-hidden`). CLI options initialize those checkboxes; Space toggles them and Enter on the launch row starts testing.

```bash
torture libft /path/to/libft -f strlen,split -v --lang en --no-color
torture printf /path/to/ft_printf -p 1 -f %d,%s -v
torture gnl /path/to/get_next_line -p 1 -f 1,42
```

- Omit `-f`/`-p` for full coverage. `-f` selects libft functions, printf suite keys/conversions/labels, or GNL buffer sizes (`def` means no explicit size). Prefer stable printf keys such as `pf_d,pf_minus` when labels may be translated.
- `-p` selects a part: libft 1–3; printf/GNL 1 mandatory, 2 bonus. Missing bonus support is not a pass. Printf needs a Makefile bonus rule; GNL needs the complete bonus file set.
- Use `--seed N` to reproduce fuzzing, `--timeout S` for per-C-test timeouts, `-e [N]` / `--efficiency [N]` for the efficiency gauge (target N%, default 50), `-t N` / `--target N` to change the target (it turns `-e` on), and `--lang en|fr` to override locale detection. `--no-hidden` ignores dot-files and dot-folders (`.vscode`, `.DS_Store`…) in extra-file and include-use checks. `--no-norm` disables only norminette; `--no-checks` skips all subject checks but still builds/runs tests. A passing unchecked run does not validate the submission structure.
- Run serially per checkout: a lock protects shared artifacts and the report. Copy relevant logs before another run overwrites them. Installation updates are a separate action (`python3 install.py --update`), not a prerequisite for diagnosing student code.
- Uninstall only when requested: `python3 install.py --delete --confirm`. Both flags are mandatory. The installer refuses unrelated commands and preserves shell PATH entries, the checkout, and custom tests.

## Diagnose and report

| Evidence | Location / meaning |
|---|---|
| Native checks | `build/<project>/checks.txt`; file, Makefile, README, norm, include, symbol, prototype verdicts |
| Check diagnostics | `build/<project>/checks/`; numbered tool logs and generated compiler probes |
| Build/link diagnostics | `build/<project>/make.log`, `link.log`, and per-pass subdirectories; use the exact path printed by the runner |
| Detailed test report | `ftorture.log` at the checkout root; `-v` also prints failure details |
| Exit status | `0` selected tests/checks pass; `1` test/check failure; `2` runner/build error; `130` interrupted |

- Distinguish build errors, failed subject checks, and behavioral failures. `✔` passes; cyan `✔` is an accepted reference-matching crash. `✘` fails; `S` crash, `T` timeout, `L` leak, `M` malloc-failure protection failure.
- `▲` is speculative and does not fail by default; `-s` makes it count. Never label undefined/reference-dependent behavior a required subject violation without checking the specification.
- **Extra files are KO** (not a warning): anything that is not a file to submit, hidden files except `.git`/`.gitignore`, unexpected extensions (`.py`, `.cpp`, `.txt`, `.o`, `a.out`, `.swp`…), empty folders. ft_printf may keep a `libft/` folder (`*.c`, `*.h`, `Makefile`). If the only KO is an editor folder the student will not submit, rerun with `--no-hidden` to confirm the rest.
- **Unused `#include` are KO**: a `<lib.h>` must give the file at least one name it uses (function, type, macro, enum constant); a project `"x.h"` must give one of its own declarations or of the libs it includes; a header's includes also count for the files that include it. `NULL`, `size_t`, `ssize_t`, `va_list`… only justify the **first** include of a file that gives them, so `<stdio.h>` kept only for `NULL` is reported. Duplicate includes are KO too. Include-use analysis is heuristic; unresolved headers get no verdict, and complex macros/conditional compilation warrant manual review.
- **GNL with `BUFFER_SIZE=1`**: the long-line tests (#16, 100 000 chars; #25, `"short\n"` + 300 000 chars) end in `T` with the default GNL timeout (10 s) when the stash is rebuilt by a `strjoin` after every `read()` (quadratic). That is a timeout, not a crash: rerun with `--timeout 480` to show the result is correct, and explain the quadratic cost. (An old memtrack false positive, "free() d'un pointeur au milieu d'un bloc malloc" after ~300 000 mallocs, is fixed; if it ever reappears, suspect the tracker before the student.)
- **Efficiency (`-e`)**: each function line gets `⚡ N%` and the result box ends with a gauge (geometric mean, each value clamped to [1%, 200%]) with a `┃` mark at the target, the functions below it and the main machine costs. N% = **perfect time / real time**. There is no reference implementation: each workload declares with `COST()` the minimal work a perfect implementation cannot avoid (bytes scanned/copied once, one `write()` per string, one `read()` per `BUFFER_SIZE`…), and ftorture prices each unit on the current machine (tightest `-O0` loop, L1-sized data, best of 20). A typical clean libft is around 60–100%; a little over 100% is calibration noise. It is information only and never makes a run KO; runs differ by a few %. Typical low scores: `write()` per character (`ft_putstr_fd`, `ft_printf`: 1–5%, a syscall is ~600 ns), length-then-copy (`ft_strlcpy` ~40%), `ft_lstmap` walking to the end for each node (~2%), a GNL `strjoin` stash or a `BUFFER_SIZE` buffer allocated at every call (1–3%). GNL is not measured without `-D BUFFER_SIZE`. `ftorture.log` lists perfect vs real time per function and all machine costs; `FTORTURE_BENCH_DEBUG=1` prints them live. Present a low score as a performance remark, never as a subject violation.
- Report the exact command, project/subject version, exit status, failing check/test, input, expected/actual result, speculative status, relevant log path, and smallest rerun. A filtered pass is not a full-project pass. Do not weaken tests or silently alter the student's submission to improve results.
