---
name: ftorture-new-testers
description: Design and add reliable ftorture suites for new 42 projects, with subject-aware checks, C tests, safe fixtures, and end-to-end validation.
---

# Add a tester for another 42 project

Use this skill when extending **ftorture itself**, not when testing a student's project. A new project needs integration throughout the runner; adding a `TEST` macro alone will not make it selectable. First read the exact 42 subject/version and the existing implementations in `torture.sh`, `gnl.sh`, `src/engine.c`, `src/project.h`, `src/zft.h`, and the closest `src/<project>/` directory. Do not assume libft's archive, Makefile, allowed symbols, or reference functions apply to the new subject.

## Design and integration

1. Record mandatory/bonus deliverables, prototypes, compiler flags, allowed external functions, undefined behavior, resource limits, and expected exit behavior **from the subject**. Keep mandatory and bonus checks separate. Specify when a warning is speculative instead of required.
2. Add project detection, CLI aliases, interactive selection, directory validation, build strategy, checks, and part/bonus selection to the shell runner. Follow the archive-based `torture.sh` path for Makefile projects or the source-based `gnl.sh` path when the subject has no Makefile. Resolve all paths relative to the ftorture checkout; write tester artifacts to `build/<project>/`, not the student's repository. Do not delete user files outside the project's documented build/cleanup rules.
3. Define exactly one `g_proj` descriptor for that tester executable in `src/<project>/proj.c` (see `src/project.h`). Give each suite a stable key, display label, part number, and function address; weak declarations allow a missing student symbol to be reported rather than crashing during link. Add subject-derived prototypes to `protos/<project>.txt` if prototype checks apply; adapt checks instead of copying another project's rules.
4. Link `src/engine.c`, `src/memtrack.c`, the new suite sources, and student sources/archive with the necessary include paths. Existing suites use `-Wl,--wrap=malloc -Wl,--wrap=free`; add other wrappers only when justified by the subject. Preserve the runner's report, log, exit-status, cleanup, and mandatory/bonus behavior.

## Write trustworthy C tests

- Put focused tests in `src/<project>/t_*.c`, using `TEST(suite, "case")`, `SPEC_TEST` only for non-required behavior, `VS`/`VS_SPEC` for a justified reference, and `MTEST` for allocation-failure resilience. Tests register through constructors in `src/zft.h`; suite keys must match the descriptor. The engine forks each run, so communicate comparison values with `OUT(...)` on **both** paths of `VS`, and use `PICK(student_fn, reference_fn)` for reference selection. A reference implementation must be independent of the student code; libc is not a valid oracle for every subject behavior.
- Add `INPUT(...)` before a failure-prone call, then assert with `CHECK`, `EQ_INT`, `EQ_SZ`, `EQ_STR`, or `EQ_MEM`. Compare exact output bytes and return values for output projects (see `src/printf/pf.h`), not just visible text. For size/ownership requirements use `alloc_size`, `gstr`/`gmem`/`gbuf`, and clean up allocations and file descriptors. Use `XFREE` for memory originating from either the student or reference path.
- For `MTEST`, bracket relevant allocations with `ARM()`/`DISARM()`; the engine replays each allocation failure and treats crashes/leaks as errors. Keep randomized cases deterministic with `zrand()` and `--seed`; avoid flaky timing and assumptions beyond the Linux/subject contract. Mark undefined or implementation-dependent cases speculative rather than demanding one arbitrary result.

## Validate before shipping

Run shell syntax checks (`bash -n torture.sh` and any new shell file), compile the tester, then run focused and full mandatory/bonus suites against a **known-good** implementation and deliberately broken fixtures. Confirm both real failures and missing symbols report correctly; no bonus is reported as skipped, not passed; `-f`/`-p` select the right cases; `-s` affects only speculative verdicts; build/link errors fail clearly; and the student's repository is left with the expected cleanup. Update `README.md` and `README.fr.md` with supported subject version, installation/usage, scope, and limitations. Never weaken tests just to make a particular student implementation pass.
