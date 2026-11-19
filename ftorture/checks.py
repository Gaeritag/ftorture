"""Migration adapter for the established Bash subject checks.

Only checks run in Bash. Python owns project builds, test passes and cleanup.
New projects can implement Project.checks directly without changing this adapter.
"""
from pathlib import Path

from .runtime import Context, RunnerError


class CheckReport:
    """Write engine check records for a new project's native Python checks.

    Values are single-line fields: pipes/newlines are data, not extra records.
    Collect in memory, then replace checks.txt once validation has finished.
    """

    def __init__(self) -> None:
        self.records: list[str] = []

    def _add(self, *fields: str) -> None:
        self.records.append("|".join(str(value).replace("|", "/").replace("\r", " ").replace("\n", " ")
                                     for value in fields))

    def section(self, title: str) -> None:
        self._add("S", title)

    def check(self, status: str, label: str, detail: str = "") -> None:
        if status not in ("OK", "KO", "WARN", "SKIP"):
            raise ValueError(f"Unknown check status: {status}")
        self._add("C", status, label, detail)

    def detail(self, value: str) -> None:
        self._add("D", value)

    def prototype(self, name: str, status: str, expected: str, found: str) -> None:
        if status not in ("OK", "KO", "WARN"):
            raise ValueError(f"Unknown prototype status: {status}")
        self._add("P", name, status, expected, found)

    def end_prototypes(self) -> None:
        self._add("PS")

    def write(self, path: Path) -> None:
        path.write_text("\n".join(self.records) + "\n", encoding="utf-8")


def legacy_checks(ctx: Context) -> None:
    env = {**ctx.env, "FTORTURE_CHECKS_ONLY": "1"}
    # The legacy checker accepts a single executable for CC, not a command line.
    if len(ctx.cc) != 1:
        raise RunnerError(ctx.message(
            "Subject checks currently require CC to name one compiler executable.",
            "Les vérifications exigent actuellement un seul exécutable dans CC."))
    args = ["bash", str(ctx.root / "torture.sh"), ctx.key, str(ctx.options.directory),
            "--lang", ctx.options.lang, "--no-color"]
    if not ctx.options.norm:
        args.append("--no-norm")
    log = ctx.build / "subject-checks.log"
    status = ctx.command(args, env=env, log=log)
    if status:
        raise RunnerError(ctx.message("Subject checks could not finish; see",
                                      "Vérifications interrompues ; voir") + f" {log}")
    if not ctx.checks_file.is_file():
        raise RunnerError(f"Missing checks file: {ctx.checks_file}")
