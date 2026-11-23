"""--repro tokens and the replay prefix the engine prints with -v."""
from pathlib import Path
import shlex
import unittest

from ftorture.repro import helpers, parse
from ftorture.runtime import Context, Options, RunnerError

ROOT = Path(__file__).resolve().parent.parent


class ReproTokenTests(unittest.TestCase):
    def test_tokens(self):
        token = parse("t_part1.c#37")
        self.assertEqual((token.file, token.idx, token.fail, token.suite, token.size),
                         ("t_part1.c", 37, 0, "", ""))
        token = parse("t_part2.c#12m3@ft_split")
        self.assertEqual((token.idx, token.fail, token.suite), (12, 3, "ft_split"))
        token = parse("t_gnl.c#5@bbs42=42")
        self.assertEqual((token.file, token.suite, token.size), ("t_gnl.c", "bbs42", "42"))
        self.assertEqual(parse("t_gnl.c#2@bsdef=def").size, "def")

    def test_bad_tokens(self):
        for raw in ("", "part1.c#3", "t_part1.c", "t_part1.c#x", "../t_x.c#1", "t_a.c#1;rm"):
            with self.subTest(raw=raw), self.assertRaises(RunnerError):
                parse(raw)

    def test_helpers_skip_tests_benchmarks_and_descriptor(self):
        names = {path.name for path in helpers(ROOT / "src/libft")}
        self.assertIn("refs.c", names)
        self.assertFalse({name for name in names if name.startswith(("t_", "b_"))} | names & {"proj.c"})
        self.assertEqual({path.name for path in helpers(ROOT / "src/printf")}, {"pf.c"})

    def test_engine_gets_a_replay_prefix(self):
        path = Path("/tmp/my libft")
        ctx = Context(ROOT, "libft", Options(path, lang="fr"))
        args = [str(arg) for arg in ctx.engine_args()]
        self.assertEqual(args[args.index("--repro-cmd") + 1], f"torture libft '{path}'")
        ctx = Context(ROOT, "gnl", Options(Path("/x"), lang="en", seed=7))
        self.assertEqual(ctx.repro_prefix(),
                         shlex.join(["torture", "gnl", str(Path("/x")), "--lang", "en", "--seed", "7"]))


if __name__ == "__main__":
    unittest.main()
