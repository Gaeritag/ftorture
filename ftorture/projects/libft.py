from .archive import ArchiveProject
from ..checks import legacy_checks
from ..runtime import Context


class LibftProject(ArchiveProject):
    def checks(self, ctx: Context) -> None:
        legacy_checks(ctx)

    def run(self, ctx: Context) -> int:
        self.build_archive(ctx)
        executable = self.link(ctx)
        args = ["-p", str(ctx.options.part)] if ctx.options.part else []
        return ctx.tester(executable, args, checks=True)


PROJECT = LibftProject(key="libft", name="libft", header="libft.h", subject="19.3",
                       parts=(1, 2, 3), archive="libft.a", source_dir="libft", check_tools=("bash",))
