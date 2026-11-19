#!/usr/bin/env python3
"""Install a direct ~/.local/bin launcher; optionally update from upstream."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import re

if sys.version_info < (3, 10):
    raise SystemExit("ftorture requires Python 3.10 or newer")

UPSTREAM = "https://github.com/HorusExE69/ftorture.git"
MARKER = "# ftorture managed launcher"
PATH_MARKER = "# ftorture: ~/.local/bin on PATH"


def configure_path(home: Path) -> None:
    """Append a PATH entry once, preserving existing shell configuration."""
    shell = Path(os.environ.get("SHELL", "")).name
    configurations = [(home / ".bashrc", 'export PATH="$HOME/.local/bin:$PATH"', shell == "bash"),
                      (home / ".zshrc", 'export PATH="$HOME/.local/bin:$PATH"', shell == "zsh")]
    fish = home / ".config/fish/config.fish"
    configurations.append((fish, "fish_add_path -g $HOME/.local/bin", bool(shutil.which("fish"))))
    for path, line, create in configurations:
        if not path.exists() and not create:
            continue
        content = path.read_text() if path.exists() else ""
        if any(re.search(r"(?:\$HOME|\$\{HOME\}|~)/\.local/bin", existing)
               for existing in content.splitlines() if not existing.lstrip().startswith("#")):
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a") as output:
            output.write(f"\n{PATH_MARKER}\n{line}\n")
        print(f"PATH: ~/.local/bin added to {path}")


def managed(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    try:
        with path.open() as source:
            return source.readline().startswith("#!") and source.readline().strip() == MARKER
    except (OSError, UnicodeError):
        return False


def update(root: Path) -> None:
    def git(*args: str) -> str:
        return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()
    if git("rev-parse", "--show-toplevel") != str(root) or git("branch", "--show-current") != "main":
        raise RuntimeError("Update requires a Git checkout on the main branch.")
    if git("status", "--porcelain", "--untracked-files=normal"):
        raise RuntimeError("Update refused: the checkout has local changes.")
    subprocess.run(["git", "-C", str(root), "fetch", UPSTREAM, "main"], check=True)
    subprocess.run(["git", "-C", str(root), "merge", "--ff-only", "FETCH_HEAD"], check=True)


def install(root: Path, home: Path) -> Path:
    target = home / ".local/bin/torture"
    applications = home / ".local/applications"
    if target.is_symlink():
        previous = Path(os.readlink(target))
        if not previous.is_absolute():
            previous = target.parent / previous
        if previous.parent != applications or not managed(previous):
            raise RuntimeError(f"Refusing to replace an existing command: {target}")
    elif target.exists() and not managed(target):
        raise RuntimeError(f"Refusing to replace an existing command: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    # repr() quotes spaces, Unicode and embedded quote characters correctly.
    content = (f"#!/usr/bin/env python3\n{MARKER}\n"
               "import os\nimport sys\n"
               f"script = {str(root / 'torture.py')!r}\n"
               "os.execv(sys.executable, [sys.executable, script, *sys.argv[1:]])\n")
    name = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=target.parent, prefix=".torture-",
                                         delete=False) as output:
            name = Path(output.name)
            output.write(content)
        name.chmod(0o755)
        os.replace(name, target)
    finally:
        if name is not None:
            name.unlink(missing_ok=True)
    # Remove only tagged old launchers, regardless of filename or interpreter.
    if applications.is_dir():
        for previous in applications.iterdir():
            if managed(previous):
                previous.unlink()
    return target


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true", help="fast-forward a clean main checkout from upstream")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    try:
        if args.update:
            update(root)
            # A newer installer may have changed: rerun it after the fast-forward.
            os.execv(sys.executable, [sys.executable, str(root / "install.py")])
        target = install(root, Path.home())
        configure_path(Path.home())
        print(f"Installed {target}")
        if shutil.which("torture") != str(target):
            print("Add ~/.local/bin to PATH if needed. Remove any old torture alias from your shell config.")
        print("Update from the original repository: python3 install.py --update")
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"install: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
