"""Explicit project registry: adding a project requires no CLI changes."""
from pathlib import Path

from .base import Project
from .gnl import PROJECT as GNL
from .libft import PROJECT as LIBFT
from .printf import PROJECT as PRINTF

PROJECTS: tuple[Project, ...] = (LIBFT, PRINTF, GNL)


def find(name: str) -> Project | None:
    return next((project for project in PROJECTS
                 if name in (project.key, *project.aliases)), None)


def detect(directory: Path) -> Project | None:
    # Detection priority is independent of the original menu's 1/2/3 order.
    priority = {"gnl": 0, "printf": 1, "libft": 2}
    return next((project for project in sorted(PROJECTS, key=lambda p: priority.get(p.key, 3))
                 if (directory / project.header).is_file()), None)
