"""A single registry interface for bundled projects and user-owned test suites."""
from pathlib import Path
import re

from .projects import PROJECTS
from .projects.base import Project
from .runtime import RunnerError


class Registry:
    def __init__(self, projects=PROJECTS):
        self.projects = tuple(projects)
        names = set()
        for project in self.projects:
            for name in (project.key, *project.aliases):
                if name in names:
                    raise RunnerError(f"Duplicate project name or alias: {name}")
                names.add(name)

    def find(self, name: str | None) -> Project | None:
        return next((project for project in self.projects if name in (project.key, *project.aliases)), None)

    def detect(self, directory: Path) -> Project | None:
        # push_swap first: its folder often holds a libft/ or an ft_printf.h too.
        priority = {"push_swap": -1, "gnl": 0, "printf": 1, "libft": 2}
        makefile = directory / "Makefile"
        if makefile.is_file() and re.search(r"^\s*NAME\s*:?=\s*push_swap\s*$",
                                            makefile.read_text(errors="replace"), re.M):
            return next((project for project in self.projects if project.key == "push_swap"), None)
        return next((project for project in sorted(self.projects, key=lambda p: priority.get(p.key, 3))
                     if not project.custom and project.find_header(directory)), None)


def load_registry(custom_directory: Path | None = None) -> Registry:
    # Lazy import keeps the bundled registry independent of the custom strategy.
    from .projects.custom import discover
    return Registry((*PROJECTS, *discover(custom_directory)))
