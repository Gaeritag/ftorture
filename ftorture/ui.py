"""Small line-based menu, usable over SSH and without terminal libraries."""
from pathlib import Path

from .i18n import text
from .projects import PROJECTS, detect
from .runtime import RunnerError


def interactive(lang: str) -> tuple[str, Path]:
    guess = detect(Path.cwd())
    print("ftorture · " + text(lang, "Select a project", "Choisir un projet"))
    for index, project in enumerate(PROJECTS, 1):
        print(f"  {index}. {project.name} · {project.subject}")
    default = str(PROJECTS.index(guess) + 1) if guess else "1"
    choice = input(text(lang, f"Project [{default}] (q to quit): ",
                        f"Projet [{default}] (q pour quitter) : ")).strip() or default
    if choice.lower() == "q":
        raise EOFError
    if not choice.isdigit() or not 1 <= int(choice) <= len(PROJECTS):
        raise RunnerError(text(lang, "Invalid project selection.", "Projet invalide."))
    project = PROJECTS[int(choice) - 1]
    path = input(text(lang, "Project directory [.]: ", "Dossier du projet [.] : ")).strip() or "."
    return project.key, Path(path).expanduser().resolve()
