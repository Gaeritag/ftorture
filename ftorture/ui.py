"""Original phosphor-green TUI, implemented with the Linux standard library."""
from contextlib import contextmanager
from dataclasses import replace
import glob
import os
from pathlib import Path
import readline
import select
import sys
import termios
import tty

from .i18n import text
from .projects import PROJECTS, detect
from .runtime import Options

FONT = {
    "F": "###:#..:##.:#..:#..", "T": "###:.#.:.#.:.#.:.#.",
    "O": "###:#.#:#.#:#.#:###", "R": "##.:#.#:##.:#.#:#.#",
    "U": "#.#:#.#:#.#:#.#:###", "E": "###:#..:##.:#..:###",
}


class Terminal:
    def __init__(self, lang: str, no_color: bool = False):
        self.lang = lang
        self.color = sys.stdout.isatty() and not no_color and not os.environ.get("NO_COLOR")
        self.ph = self.escape("38;5;46")
        self.medium = self.escape("38;5;34")
        self.dark = self.escape("38;5;22")
        self.bold = self.escape("1")
        self.reset = self.escape("0")
        self.red = self.escape("38;5;203")
        self.inverse = self.escape("48;5;46;38;5;16;1")

    def escape(self, code: str) -> str:
        return f"\033[{code}m" if self.color else ""

    def t(self, en: str, fr: str) -> str:
        return text(self.lang, en, fr)

    def write(self, value: str) -> None:
        print(value, end="", flush=True)

    def clear(self) -> None:
        if sys.stdout.isatty():
            self.write("\033[H\033[2J\033[3J")

    def cursor(self, visible: bool) -> None:
        if sys.stdout.isatty():
            self.write("\033[?25h" if visible else "\033[?25l")

    def banner(self, project=None) -> None:
        grid = {(r, i * 4 + c) for i, letter in enumerate("FTORTURE")
                for r, row in enumerate(FONT[letter].split(":"))
                for c, pixel in enumerate(row) if pixel == "#"}
        self.write("\n")
        for r, scan in enumerate((46, 40, 46, 40, 46, 22)):
            line = "   "
            for c in range(32):
                if (r, c) in grid:
                    line += self.escape(f"38;5;{scan}") + "██"
                elif (r - 1, c - 1) in grid:
                    line += self.dark + "▒▒"
                else:
                    line += "  "
            self.write(line + self.reset + "\n")
        subtitle = "42 TESTER · LIBFT · FT_PRINTF · GET_NEXT_LINE"
        tag = self.t("> your code will talk", "> ton code va parler")
        if project:
            subtitle = f"{project.name.upper()} · {self.t('SUBJECT', 'SUJET')} v{project.subject} · LINUX"
            tags = {"libft": ("> your libft will talk", "> ta libft va parler"),
                    "printf": ("> your printf will talk", "> ton printf va parler"),
                    "gnl": ("> your gnl will talk", "> ta gnl va parler")}
            tag = self.t(*tags.get(project.key, ("> your code will talk", "> ton code va parler")))
        self.write(f"   {self.medium}▚▚ {subtitle} ▞▞{self.reset}\n")
        self.write(f"   {self.ph}{tag}{self.reset}{self.escape('5')}{self.ph}_{self.reset}\n\n")

    def heading(self, en: str, fr: str, project=None) -> None:
        self.clear()
        self.banner(project)
        self.write(f"   {self.bold}{self.ph}{self.t(en, fr)}{self.reset}\n\n")


@contextmanager
def terminal_session(terminal: Terminal):
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    terminal.cursor(False)
    try:
        yield
    finally:
        termios.tcsetattr(fd, termios.TCSANOW, saved)
        terminal.cursor(True)


def read_key() -> str:
    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    try:
        # Keep ISIG enabled: Ctrl-C still reaches the normal cleanup path.
        tty.setcbreak(fd, termios.TCSANOW)
        key = os.read(fd, 1)
        if not key or key == b"\x04":
            raise EOFError
        if key == b"\x1b":
            for _ in range(2):
                if not select.select([fd], [], [], 0.05)[0]:
                    break
                key += os.read(fd, 1)
        if key in (b"\x1b[A", b"\x1bOA", b"k", b"K"):
            return "up"
        if key in (b"\x1b[B", b"\x1bOB", b"j", b"J"):
            return "down"
        if key in (b"\r", b"\n"):
            return "enter"
        if key == b" ":
            return "space"
        if key in (b"q", b"Q", b"\x1b"):
            return "quit"
        return key.decode("ascii", errors="ignore")
    finally:
        termios.tcsetattr(fd, termios.TCSANOW, saved)


def choose_project(terminal: Terminal):
    guess = detect(Path.cwd())
    selected = PROJECTS.index(guess) if guess else 0
    descriptions = {
        "libft": terminal.t("libft.a · 43 functions · parts 1, 2, 3", "libft.a · 43 fonctions · parties 1, 2, 3"),
        "printf": terminal.t("libftprintf.a · cspdiuxX% · bonus -0. width # +", "libftprintf.a · cspdiuxX% · bonus -0. largeur # +"),
        "gnl": "get_next_line · 14 BUFFER_SIZE · bonus multi-fd",
    }
    while True:
        terminal.heading("WHICH PROJECT TO TEST?", "QUEL PROJET ON TORTURE ?")
        labels = [(p.key.upper() if p.key != "printf" else "FT_PRINTF", descriptions.get(p.key, p.name))
                  for p in PROJECTS] + [(terminal.t("QUIT", "QUITTER"), "")]
        for index, (label, description) in enumerate(labels):
            if index == selected:
                terminal.write(f"   {terminal.inverse}▶ {label:<12}{terminal.reset} {terminal.ph}{description}{terminal.reset}\n")
            else:
                terminal.write(f"     {terminal.medium}{label:<12}{terminal.reset} {terminal.dark}{description}{terminal.reset}\n")
        terminal.write(f"\n   {terminal.dark}" + terminal.t(
            "↑ ↓ select · Enter confirm · 1 2 3 shortcuts · q quit",
            "↑ ↓ choisir · Entrée valider · 1 2 3 raccourcis · q quitter") + terminal.reset + "\n")
        if guess:
            terminal.write(f"   {terminal.dark}" + terminal.t(
                f"(current directory: {guess.key} detected)", f"(dossier courant : {guess.key} détecté)") + terminal.reset + "\n")
        key = read_key()
        if key == "up":
            selected = (selected - 1) % len(labels)
        elif key == "down":
            selected = (selected + 1) % len(labels)
        elif key.isdigit() and 1 <= int(key) <= len(PROJECTS):
            return PROJECTS[int(key) - 1]
        elif key == "quit" or (key == "enter" and selected == len(PROJECTS)):
            terminal.clear()
            raise EOFError
        elif key == "enter":
            return PROJECTS[selected]


def complete_path(value: str, state: int):
    matches = sorted(glob.glob(glob.escape(os.path.expanduser(value)) + "*"))
    matches = [path + (os.sep if os.path.isdir(path) else "") for path in matches]
    # Preserve the user's ~ spelling in the editable input.
    if value.startswith("~/"):
        matches = ["~" + path[len(str(Path.home())):] for path in matches]
    return matches[state] if state < len(matches) else None


def path_input(terminal: Terminal, default: str) -> str:
    completer, delimiters = readline.get_completer(), readline.get_completer_delims()
    readline.set_completer(complete_path)
    readline.set_completer_delims("\n")
    readline.parse_and_bind("tab: complete")
    readline.set_startup_hook(lambda: readline.insert_text(default))
    terminal.cursor(True)
    try:
        # Readline needs nonprinting markers to calculate the cursor position.
        prompt = f"   \001{terminal.ph}\002▶\001{terminal.reset}\002 " + terminal.t("path : ", "chemin : ")
        return input(prompt)
    finally:
        readline.set_startup_hook(None)
        readline.set_completer(completer)
        readline.set_completer_delims(delimiters)
        terminal.cursor(False)


def choose_path(terminal: Terminal, project) -> Path:
    home_project = Path.home() / project.name
    default = Path.cwd() if (Path.cwd() / project.header).is_file() else home_project
    if not (default / project.header).is_file():
        default = Path.cwd()
    error = ""
    while True:
        terminal.heading(f"WHERE IS YOUR {project.name} ?", f"OÙ EST TON {project.name} ?", project)
        if error:
            terminal.write(f"   {terminal.red}✘ {error}{terminal.reset}\n\n")
        terminal.write(f"   {terminal.dark}" + terminal.t("Tab to complete · Enter to confirm", "Tab pour compléter · Entrée pour valider") + terminal.reset + "\n\n")
        value = path_input(terminal, str(default))
        directory = Path(value).expanduser()
        if not directory.is_dir():
            error = terminal.t("directory not found: ", "dossier introuvable : ") + value
        elif project.key != "gnl" and not (directory / "Makefile").is_file():
            error = terminal.t("no Makefile in ", "pas de Makefile dans ") + value
        elif not (directory / project.header).is_file():
            error = terminal.t(f"no {project.header} in ", f"pas de {project.header} dans ") + value
        else:
            return directory.resolve()
        default = directory


def choose_options(terminal: Terminal, project, options: Options) -> Options:
    labels = [
        terminal.t("strict mode (-s): ▲ count as errors", "mode strict (-s) : les ▲ comptent comme erreurs"),
        terminal.t("verbose (-v): all failure details", "verbose (-v) : tous les détails des échecs"),
        terminal.t("checks (Makefile, nm, prototypes, README…)", "vérifications (Makefile, nm, prototypes, README…)"),
        "norminette", terminal.t("ignore hidden files (--no-hidden)", "ignorer les fichiers cachés (--no-hidden)"),
    ]
    values = [options.speculative, options.verbose, options.checks, options.norm, options.ignore_hidden]
    selected = len(labels)
    while True:
        terminal.clear()
        terminal.banner(project)
        terminal.write(f"   {terminal.bold}{terminal.ph}OPTIONS{terminal.reset}  {terminal.dark}{project.name} · {options.directory}{terminal.reset}\n\n")
        for index, label in enumerate(labels):
            box = "[■]" if values[index] else "[ ]"
            if index == selected:
                terminal.write(f"   {terminal.ph}{terminal.bold}▶ {box} {label}{terminal.reset}\n")
            else:
                terminal.write(f"     {terminal.medium}{box}{terminal.reset} {terminal.dark}{label}{terminal.reset}\n")
        launch = terminal.t("START TESTING", "LANCER LA TORTURE")
        terminal.write(f"\n   {terminal.inverse} ▶ {launch} {terminal.reset}\n" if selected == len(labels)
                       else f"\n     {terminal.medium}▶ {launch}{terminal.reset}\n")
        terminal.write(f"\n   {terminal.dark}" + terminal.t(
            "↑ ↓ navigate · Space toggle · Enter start · q quit",
            "↑ ↓ naviguer · Espace cocher · Entrée lancer · q quitter") + terminal.reset + "\n")
        key = read_key()
        if key == "up":
            selected = (selected - 1) % (len(labels) + 1)
        elif key == "down":
            selected = (selected + 1) % (len(labels) + 1)
        elif key in ("space", "enter") and selected < len(labels):
            values[selected] = not values[selected]
        elif key == "enter":
            return replace(options, speculative=values[0], verbose=values[1], checks=values[2],
                           norm=values[3], ignore_hidden=values[4])
        elif key == "quit":
            terminal.clear()
            raise EOFError


def interactive(options: Options):
    terminal = Terminal(options.lang, options.no_color)
    with terminal_session(terminal):
        project = choose_project(terminal)
        options = replace(options, directory=choose_path(terminal, project))
        options = choose_options(terminal, project, options)
    terminal.clear()
    return project, options


def run_header(project, options: Options) -> None:
    terminal = Terminal(options.lang, options.no_color)
    terminal.banner(project)
    mode = terminal.t("strict (-s): ▲ count as errors", "strict (-s) : les ▲ comptent comme erreurs") if options.speculative else terminal.t(
        "normal: ▲ are warnings (-s to count them)", "normal : ▲ = avertissements (-s pour les compter)")
    for label, value in ((terminal.t("project", "projet"), project.name),
                         (terminal.t("path", "chemin"), options.directory), ("mode  ", mode), ("seed  ", options.seed)):
        terminal.write(f"   {terminal.dark}{label}{terminal.reset} {value}\n")
