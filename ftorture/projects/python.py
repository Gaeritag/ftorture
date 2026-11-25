"""Reusable isolated Python test runner for 42 Python projects."""
from collections.abc import Callable
from dataclasses import dataclass
import json
import os
from pathlib import Path
import signal
import subprocess
import sys

from .base import Project
from ..runtime import Context, RunnerError
from ..subject.python import ExerciseSpec, python_checks


@dataclass(frozen=True)
class PyOutcome:
    ok: bool
    detail: str = ""
    input_desc: str = ""
    expected: str = ""
    actual: str = ""
    timed_out: bool = False


@dataclass(frozen=True)
class PyTestCase:
    name_en: str
    name_fr: str
    runner: Callable[[Context, Path, int], PyOutcome]
    speculative: bool = False
    timeout: int | None = None


@dataclass(frozen=True)
class PySuite:
    key: str
    label: str
    files: tuple[str, ...]
    cases: tuple[PyTestCase, ...]
    aliases: tuple[str, ...] = ()
    part: int = 1


def run_python_subprocess(ctx: Context, args: list[str], *, cwd: Path,
                          stdin: str | None = None, timeout: int = 5,
                          extra_env: dict[str, str] | None = None) -> tuple[int, str, str]:
    """Run a Python child in its own process group without writing __pycache__."""
    env = {**ctx.env, "PYTHONDONTWRITEBYTECODE": "1", "PYTHONIOENCODING": "utf-8"}
    if extra_env:
        env.update(extra_env)
    with subprocess.Popen(
        args, cwd=cwd, env=env,
        stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        start_new_session=True,
    ) as proc:
        try:
            out, err = proc.communicate(None if stdin is None else stdin.encode("utf-8"), timeout=timeout)
            return proc.returncode, out.decode("utf-8", errors="replace"), err.decode("utf-8", errors="replace")
        except (KeyboardInterrupt, subprocess.TimeoutExpired) as exc:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass
            finally:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
            if isinstance(exc, KeyboardInterrupt):
                raise
            return 124, "", f"Timed out after {timeout}s"


def eval_module_code(ctx: Context, directory: Path, rel_path: str, code: str, *,
                     stdin: str | None = None, timeout: int = 5) -> tuple[int, str, str]:
    """Load student's `rel_path` via importlib and execute `code` with `mod` bound."""
    file_path = directory / rel_path
    harness = (
        "import importlib.util, io, sys, contextlib\n"
        f"_path = {str(file_path)!r}\n"
        "_spec = importlib.util.spec_from_file_location('_student_mod', _path)\n"
        "_mod = importlib.util.module_from_spec(_spec)\n"
        "_spec.loader.exec_module(_mod)\n"
        "mod = _mod\n"
        + code
    )
    return run_python_subprocess(
        ctx, [sys.executable, "-c", harness],
        cwd=file_path.parent, stdin=stdin, timeout=timeout,
    )


def run_script_file(ctx: Context, directory: Path, rel_path: str, *,
                    stdin: str | None = None, timeout: int = 5) -> tuple[int, str, str]:
    """Execute a student Python file directly as `python3 <file>`."""
    file_path = directory / rel_path
    return run_python_subprocess(
        ctx, [sys.executable, str(file_path)],
        cwd=file_path.parent, stdin=stdin, timeout=timeout,
    )


def _render_checks(terminal, checks_file: Path, strict: bool,
                   report_lines: list[str]) -> tuple[int, int, int]:
    """Render checks.txt in the retro TUI style; returns (ok_count, ko_count, warn_count)."""
    if not checks_file.is_file():
        return 0, 0, 0
    ok = ko = warn = 0
    for raw in checks_file.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = raw.split("|")
        if not parts:
            continue
        kind = parts[0]
        if kind == "S" and len(parts) >= 2:
            title = parts[1]
            terminal.section_bar(title)
            report_lines.append(f"\n== {title} ==")
        elif kind == "C" and len(parts) >= 4:
            status, label, detail = parts[1], parts[2], parts[3]
            if status == "OK":
                mark, color = "✔", terminal.ph
                ok += 1
            elif status == "KO":
                mark, color = "✘", terminal.red
                ko += 1
            elif status == "WARN":
                mark, color = "▲", terminal.escape("38;5;221")
                warn += 1
            else:
                mark, color = "·", terminal.dark
            suffix = f"  {terminal.dark}{detail}{terminal.reset}" if detail else ""
            terminal.write(f"   {color}{mark}{terminal.reset} {label:<26}{suffix}\n")
            report_lines.append(f"[{status}] {label}: {detail}" if detail else f"[{status}] {label}")
        elif kind == "D" and len(parts) >= 2:
            terminal.write(f"       {terminal.dark}{parts[1]}{terminal.reset}\n")
            report_lines.append(f"    {parts[1]}")
    return ok, ko, warn


@dataclass(frozen=True)
class PythonProject(Project):
    requires_compiler: bool = False
    requires_makefile: bool = False
    tools: tuple[str, ...] = ()
    nested: bool = False
    parts: tuple[int, ...] = (1,)
    specs: tuple[ExerciseSpec, ...] = ()
    suites: tuple[PySuite, ...] = ()
    check_naming_conventions: bool = False
    default_timeout: int = 5

    @property
    def markers(self) -> tuple[str, ...]:
        return tuple(rel for spec in self.specs for rel in spec.relative_files)

    def matches(self, directory: Path, *, ignore_hidden: bool = False) -> bool:
        return any((directory / rel).is_file() for rel in self.markers)

    def validate(self, ctx: Context) -> None:
        directory = ctx.options.directory
        has_marker = self.matches(directory)
        has_ex_dir = any((directory / spec.directory).is_dir() for spec in self.specs if spec.directory)
        if not has_marker and not has_ex_dir:
            raise RunnerError(
                f"{directory}: " + ctx.message(
                    f"no {self.name} exercise files found",
                    f"aucun fichier d'exercice {self.name} trouvé",
                )
            )
        if ctx.options.part is not None and ctx.options.part not in self.parts:
            raise RunnerError(ctx.message("Invalid part for", "Partie invalide pour") + f" {self.name}")
        self.selected_suites(ctx)

    def selected_suites(self, ctx: Context) -> tuple[PySuite, ...]:
        part = ctx.options.part
        suites = tuple(s for s in self.suites if part is None or s.part == part)
        if ctx.options.functions:
            wanted = [item.strip().lower() for item in ctx.options.functions.split(",") if item.strip()]
            chosen = []
            unknown = []
            for token in wanted:
                matched = [
                    s for s in suites
                    if token == s.key.lower()
                    or token in {a.lower() for a in s.aliases}
                    or any(token == Path(f).stem.lower() or token == Path(f).name.lower() for f in s.files)
                ]
                if not matched:
                    unknown.append(token)
                else:
                    for s in matched:
                        if s not in chosen:
                            chosen.append(s)
            if unknown:
                raise RunnerError(
                    ctx.message("Unknown exercise/filter: ", "Exercice/filtre inconnu : ") + ", ".join(unknown)
                )
            suites = tuple(chosen)
        if not suites:
            raise RunnerError(ctx.message("No exercises selected", "Aucun exercice sélectionné"))
        return suites

    def checks(self, ctx: Context) -> None:
        python_checks(ctx, self.specs, check_naming_conventions=self.check_naming_conventions)

    def run(self, ctx: Context) -> int:
        from ..ui import Terminal
        terminal = Terminal(ctx.options.lang, ctx.options.no_color)
        report_lines = [f"{self.name} (subject v{self.subject})", f"Path: {ctx.options.directory}"]
        check_ok = check_ko = check_warn = 0
        if ctx.options.checks:
            check_ok, check_ko, check_warn = _render_checks(
                terminal, ctx.checks_file, ctx.options.speculative, report_lines
            )

        amber = terminal.escape("38;5;221")
        terminal.write(
            f"\n   {terminal.dark}{ctx.message('Legend', 'Légende')}{terminal.reset}  "
            f"{terminal.ph}✔{terminal.reset} ok   "
            f"{terminal.red}✘{terminal.reset} {ctx.message('error', 'erreur')}   "
            f"{terminal.red}T{terminal.reset} timeout   "
            f"{amber}▲{terminal.reset} {ctx.message('speculative', 'spéculatif')}\n"
        )
        terminal.section_bar(ctx.message("PART 1 · Exercises", "PARTIE 1 · Exercices"))
        report_lines.append("\n== TESTS ==")

        passed = failed = speculative = 0
        suites_ok = suites_missing = 0
        selected = self.selected_suites(ctx)
        directory = ctx.options.directory
        base_timeout = ctx.options.timeout or self.default_timeout

        for suite in selected:
            missing = [rel for rel in suite.files if not (directory / rel).is_file()]
            if missing:
                failed += 1
                suites_missing += 1
                msg = ctx.message("missing: ", "absent : ") + ", ".join(missing)
                terminal.write(f"   {suite.label:<30} {terminal.red}✘ {msg}{terminal.reset}\n")
                report_lines.append(f"[✘] {suite.label}: {msg}")
                continue

            marks: list[str] = []
            failures: list[tuple[str, PyTestCase, PyOutcome]] = []
            s_pass = s_fail = s_spec = 0
            for case in suite.cases:
                timeout = ctx.options.timeout or case.timeout or base_timeout
                outcome = case.runner(ctx, directory, timeout)
                if outcome.ok:
                    passed += 1
                    s_pass += 1
                    marks.append(f"{terminal.ph}✔{terminal.reset}")
                    report_lines.append(f"  [✔] {suite.key}: {ctx.message(case.name_en, case.name_fr)}")
                elif case.speculative:
                    speculative += 1
                    s_spec += 1
                    marks.append(f"{amber}▲{terminal.reset}")
                    failures.append(("▲", case, outcome))
                    report_lines.append(f"  [▲] {suite.key}: {ctx.message(case.name_en, case.name_fr)} — {outcome.detail}")
                else:
                    failed += 1
                    s_fail += 1
                    sym = "T" if outcome.timed_out else "✘"
                    marks.append(f"{terminal.red}{sym}{terminal.reset}")
                    failures.append((sym, case, outcome))
                    report_lines.append(f"  [{sym}] {suite.key}: {ctx.message(case.name_en, case.name_fr)} — {outcome.detail}")

            suite_ok = not s_fail and not (ctx.options.speculative and s_spec)
            if suite_ok:
                suites_ok += 1
            s_total = len(suite.cases)
            spec_note = f" {amber}({s_spec} ▲){terminal.reset}" if s_spec else ""
            terminal.write(f"   {suite.label:<30} {' '.join(marks)}  {s_pass}/{s_total}{spec_note}\n")

            for sym, case, outcome in failures:
                label = ctx.message(case.name_en, case.name_fr)
                if outcome.input_desc:
                    report_lines.append(f"      input:    {outcome.input_desc}")
                if outcome.expected:
                    report_lines.append(f"      expected: {outcome.expected}")
                if outcome.actual:
                    report_lines.append(f"      actual:   {outcome.actual}")
                if ctx.options.verbose:
                    c = amber if sym == "▲" else terminal.red
                    terminal.write(f"       {c}{sym}{terminal.reset} {label}")
                    if outcome.detail:
                        terminal.write(f" — {terminal.dark}{outcome.detail}{terminal.reset}")
                    terminal.write("\n")
                    if outcome.input_desc:
                        terminal.write(f"         {terminal.dark}input   :{terminal.reset} {outcome.input_desc}\n")
                    if outcome.expected:
                        terminal.write(f"         {terminal.dark}expected:{terminal.reset} {outcome.expected}\n")
                    if outcome.actual:
                        terminal.write(f"         {terminal.dark}actual  :{terminal.reset} {outcome.actual}\n")

        bad_checks = check_ko + (check_warn if ctx.options.speculative else 0)
        bad_tests = failed + (speculative if ctx.options.speculative else 0)
        bad = bad_checks + bad_tests
        verdict = "KO" if bad else "OK"
        summary = f"{passed} OK · {failed} KO · {speculative} ▲"
        if ctx.options.checks:
            summary += f"  |  checks: {check_ok} OK · {check_ko} KO · {check_warn} ▲"
        total = passed + failed + speculative
        suites_total = len(selected)
        stats = [
            terminal.progress_row(1, ctx.message("exercises", "exercices"), suites_ok, suites_total),
            "",
            f"{terminal.dark}TESTS {terminal.reset} "
            f"{terminal.ph}{passed} ✔{terminal.reset}  "
            f"{terminal.red if failed else terminal.dark}{failed} ✘{terminal.reset}  "
            f"{amber if speculative else terminal.dark}{speculative} ▲{terminal.reset}  "
            f"{terminal.dark}/ {total}{terminal.reset}",
        ]
        if suites_missing:
            stats.append(
                f"{terminal.dark}{ctx.message('MISSING', 'ABSENT ')}{terminal.reset} "
                f"{terminal.red}{suites_missing} {ctx.message('exercise(s)', 'exercice(s)')}{terminal.reset}"
            )
        if ctx.options.checks:
            stats.append(
                f"{terminal.dark}CHECKS{terminal.reset} "
                f"{terminal.ph}{check_ok} ✔{terminal.reset}  "
                f"{terminal.red if check_ko else terminal.dark}{check_ko} ✘{terminal.reset}  "
                f"{amber if check_warn else terminal.dark}{check_warn} ▲{terminal.reset}"
            )
        headline = (
            ctx.message(f"{suites_ok}/{suites_total} perfect exercises · no errors",
                        f"{suites_ok}/{suites_total} exercices parfaits · aucune erreur")
            if not bad else
            ctx.message(f"{bad} problem(s) · {suites_ok}/{suites_total} exercises OK",
                        f"{bad} problème(s) · {suites_ok}/{suites_total} exercices OK")
        )
        warn_count = speculative + check_warn
        warnings = (
            ctx.message(f"{warn_count} speculative ▲ warnings to review",
                        f"{warn_count} avertissement(s) ▲ à savoir défendre")
            if not ctx.options.speculative and warn_count else ""
        )
        log_path = ctx.root / "ftorture.log"
        report_lines.append(f"\nTOTAL: {summary} -> {verdict}\n")
        log_path.write_text("\n".join(report_lines), encoding="utf-8")
        terminal.result_box(ok=not bad, stats=stats, headline=headline, warnings=warnings, log_path=log_path)
        return int(bool(bad))


def expect_json_outcome(ctx: Context, directory: Path, rel_path: str, code: str, *,
                        stdin: str | None = None, timeout: int = 5,
                        input_desc: str = "") -> PyOutcome:
    """Run `code` which prints a JSON dict {ok, detail, expected, actual} on the last line."""
    status, stdout, stderr = eval_module_code(ctx, directory, rel_path, code, stdin=stdin, timeout=timeout)
    if status == 124:
        return PyOutcome(False, f"timeout ({timeout}s)", input_desc=input_desc, timed_out=True)
    if status != 0:
        err = (stderr.strip() or stdout.strip()).splitlines()
        summary = err[-1] if err else f"exit code {status}"
        return PyOutcome(False, summary, input_desc=input_desc, actual="\n".join(err[-4:]))
    lines = [line for line in stdout.splitlines() if line.strip()]
    if not lines:
        return PyOutcome(False, "no test output", input_desc=input_desc)
    try:
        payload = json.loads(lines[-1])
        return PyOutcome(
            bool(payload.get("ok")),
            str(payload.get("detail", "")),
            input_desc=input_desc or str(payload.get("input", "")),
            expected=str(payload.get("expected", "")),
            actual=str(payload.get("actual", "")),
        )
    except (ValueError, TypeError):
        return PyOutcome(False, "unexpected harness output", input_desc=input_desc, actual=stdout.strip())
