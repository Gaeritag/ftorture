"""push_swap: build the student's binaries with make, run the C suites on them."""
from dataclasses import dataclass

from .archive import ArchiveProject
from ..subject.push_swap import push_swap_checks
from ..runtime import Context, RunnerError


@dataclass(frozen=True)
class PushSwapProject(ArchiveProject):
    def validate(self, ctx: Context) -> None:
        # The subject imposes no header name: only the Makefile is required.
        if not (ctx.options.directory / "Makefile").is_file():
            raise RunnerError(f"{ctx.options.directory}: Makefile " + ctx.message("not found", "introuvable"))
        if ctx.options.part is not None and ctx.options.part not in self.parts:
            raise RunnerError(ctx.message("Invalid part for", "Partie invalide pour") + f" {self.name}")

    def checks(self, ctx: Context) -> None:
        push_swap_checks(ctx, self)

    def build(self, ctx: Context, rule: str = "all") -> None:
        """make all -> ./push_swap; make bonus -> ./checker (on top of it)."""
        binary = "checker" if rule == "bonus" else "push_swap"
        if rule != "bonus":
            self.make(ctx, "fclean", log="fclean.log")
        if self.make(ctx, rule) or not (ctx.options.directory / binary).is_file():
            raise RunnerError(f"make {rule}: " + ctx.message("see", "voir") + f" {ctx.build / 'make.log'}")

    def run(self, ctx: Context) -> int:
        self.build(ctx)
        bonus = ctx.options.part != 1 and self.has_bonus(ctx)
        note = None
        failed = 0
        if bonus:
            try:
                self.build(ctx, "bonus")
            except RunnerError:
                bonus, failed = False, 1
                note = ctx.message("Bonus not tested: make bonus gave no ./checker.",
                                   "Bonus non testé : make bonus ne donne pas de ./checker.")
        elif ctx.options.part != 1:
            note = ctx.message("Bonus not tested: no bonus rule.", "Bonus non testé : pas de règle bonus.")
        if ctx.options.part == 2 and not bonus:
            raise RunnerError(note or "")
        executable = ctx.build / "tester"
        ctx.require([*ctx.cc, *ctx.engine_flags("push_swap"), ctx.root / "src/engine.c",
                     ctx.root / "src/memtrack.c", *sorted((ctx.root / "src/push_swap").glob("*.c")),
                     "-Wl,--wrap=malloc", "-Wl,--wrap=free", "-lm", "-o", executable],
                    ctx.build / "link.log")
        args: list[str] = ["-p", "1"] if not bonus else (["-p", str(ctx.options.part)] if ctx.options.part else [])
        if ctx.options.timeout is None:
            args.extend(["--timeout", "20"])
        if note:
            args.extend(["--note", note])
        return max(failed, ctx.tester(executable, args, checks=True))

    def cleanup(self, ctx: Context) -> None:
        if self.make(ctx, "fclean", log="fclean.log") == 0:
            print("   ▶ " + ctx.message("make fclean: object files and binaries removed",
                                        "make fclean : .o et binaires supprimés") + "\n", flush=True)


PROJECT = PushSwapProject(key="push_swap", name="push_swap", aliases=("ps",), header="push_swap.h",
                          subject="1.2", archive="push_swap", source_dir="push_swap")
