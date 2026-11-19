"""Explicit project registry: adding a project requires no CLI changes."""
from pathlib import Path

from .base import Project
from .gnl import PROJECT as GNL
from .libft import PROJECT as LIBFT
from .printf import PROJECT as PRINTF

PROJECTS: tuple[Project, ...] = (GNL, PRINTF, LIBFT)


def find(name: str) -> Project | None:
    return next((project for project in PROJECTS
                 if name in (project.key, *project.aliases)), None)


def detect(directory: Path) -> Project | None:
    return next((project for project in PROJECTS if (directory / project.header).is_file()), None)
