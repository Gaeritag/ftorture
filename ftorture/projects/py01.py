"""python-module-01 (Code Cultivation, v3.2): Object-Oriented Garden Systems."""
import ast
from pathlib import Path

from .python import (PyOutcome, PySuite, PyTestCase, PythonProject,
                     eval_module_code, expect_json_outcome, run_script_file)
from ..runtime import Context
from ..subject.python import ExerciseSpec

SPECS: tuple[ExerciseSpec, ...] = (
    ExerciseSpec("ex0", "ex0", ("ft_garden_intro.py",),
                 authorized=frozenset({"print"}),
                 allow_main_block=True, require_main_block=True, require_type_hints=True),
    ExerciseSpec("ex1", "ex1", ("ft_garden_data.py",),
                 authorized=frozenset({"print"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant",)),
    ExerciseSpec("ex2", "ex2", ("ft_plant_growth.py",),
                 authorized=frozenset({"print", "range", "round"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant",)),
    ExerciseSpec("ex3", "ex3", ("ft_plant_factory.py",),
                 authorized=frozenset({"print", "range", "round"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant",)),
    ExerciseSpec("ex4", "ex4", ("ft_garden_security.py",),
                 authorized=frozenset({"print", "range", "round"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant",)),
    ExerciseSpec("ex5", "ex5", ("ft_plant_types.py",),
                 authorized=frozenset({"super", "print", "range", "round"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant", "Flower", "Tree", "Vegetable")),
    ExerciseSpec("ex6", "ex6", ("ft_garden_analytics.py",),
                 authorized=frozenset({"super", "print", "range", "round", "staticmethod", "classmethod"}),
                 allow_classes=True, allow_main_block=True, require_type_hints=True,
                 expected_classes=("Plant", "Flower", "Tree", "Seed")),
)


def _script_runs_test(rel_path: str, *, min_lines: int = 1, must_contain: tuple[str, ...] = ()):
    def runner(ctx: Context, directory: Path, timeout: int) -> PyOutcome:
        status, stdout, stderr = run_script_file(ctx, directory, rel_path, timeout=timeout)
        if status == 124:
            return PyOutcome(False, f"timeout ({timeout}s)", input_desc=f"python3 {rel_path}", timed_out=True)
        if status != 0:
            err = (stderr.strip().splitlines() or [f"exit {status}"])[-1]
            return PyOutcome(False, err, input_desc=f"python3 {rel_path}", actual=stderr.strip())
        lines = [line for line in stdout.splitlines() if line.strip()]
        if len(lines) < min_lines:
            return PyOutcome(False, f"expected at least {min_lines} output line(s)",
                             input_desc=f"python3 {rel_path}", expected=f">= {min_lines} lines",
                             actual=f"{len(lines)} lines")
        low = stdout.lower()
        missing = [word for word in must_contain if word.lower() not in low]
        if missing:
            return PyOutcome(False, "missing expected keywords: " + ", ".join(missing),
                             input_desc=f"python3 {rel_path}", expected=", ".join(must_contain),
                             actual=stdout.strip()[:200])
        return PyOutcome(True)
    return runner


def _import_silent_test(rel_path: str):
    def runner(ctx: Context, directory: Path, timeout: int) -> PyOutcome:
        status, stdout, stderr = eval_module_code(ctx, directory, rel_path, "", timeout=timeout)
        if status == 124:
            return PyOutcome(False, f"timeout ({timeout}s)", timed_out=True)
        if status != 0:
            return PyOutcome(False, (stderr.strip().splitlines() or [f"exit {status}"])[-1])
        if stdout.strip():
            return PyOutcome(False, "output printed on import (guard with if __name__ == '__main__')",
                             expected="(no output)", actual=stdout.strip()[:200])
        return PyOutcome(True)
    return runner


def _ex1_plant_test():
    code = """
import io, json, inspect, contextlib
Plant = getattr(mod, 'Plant', None)
if not inspect.isclass(Plant):
    print(json.dumps({'ok': False, 'detail': 'missing Plant class'}))
elif not callable(getattr(Plant, 'show', None)):
    print(json.dumps({'ok': False, 'detail': 'Plant missing show() method'}))
else:
    sig = inspect.signature(Plant.__init__)
    params = [p for p in list(sig.parameters.values())[1:] if p.default is inspect.Parameter.empty]
    if len(params) == 0:
        p = Plant()
        for attr, val in (('name', 'Rose'), ('height', 25), ('age', 30)):
            if not hasattr(p, attr):
                setattr(p, attr, val)
    elif len(params) == 3:
        p = Plant('Rose', 25, 30)
    else:
        print(json.dumps({'ok': False, 'detail': f'unexpected Plant.__init__ params: {sig}'}))
        raise SystemExit(0)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        p.show()
    out = buf.getvalue().strip()
    print(json.dumps({'ok': bool(out), 'detail': '' if out else 'show() printed nothing', 'actual': out}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex1/ft_garden_data.py", code, timeout=timeout, input_desc="Plant + show()"
    )


def _ex2_growth_test():
    code = """
import json, inspect
Plant = getattr(mod, 'Plant', None)
if not inspect.isclass(Plant):
    print(json.dumps({'ok': False, 'detail': 'missing Plant class'}))
else:
    missing = [m for m in ('grow', 'age', 'show') if not callable(getattr(Plant, m, None))]
    if missing:
        print(json.dumps({'ok': False, 'detail': 'Plant missing: ' + ', '.join(missing)}))
    else:
        sig = inspect.signature(Plant.__init__)
        req = [p for p in list(sig.parameters.values())[1:] if p.default is inspect.Parameter.empty]
        p = Plant('Rose', 25.0, 30) if len(req) == 3 else Plant()
        before = dict(p.__dict__)
        p.grow()
        p.age()
        after = dict(p.__dict__)
        changed = (before != after)
        print(json.dumps({'ok': changed, 'detail': '' if changed else 'grow()/age() did not change plant state'}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex2/ft_plant_growth.py", code, timeout=timeout, input_desc="Plant.grow() & Plant.age()"
    )


def _ex3_factory_test():
    code = """
import io, json, inspect, contextlib
Plant = getattr(mod, 'Plant', None)
if not inspect.isclass(Plant):
    print(json.dumps({'ok': False, 'detail': 'missing Plant class'}))
else:
    try:
        p = Plant('Oak', 200.0, 365)
    except Exception as exc:
        print(json.dumps({'ok': False, 'detail': f'Plant("Oak", 200.0, 365) failed: {exc}'}))
    else:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            p.show()
        out = buf.getvalue().strip()
        ok = ('Oak' in out)
        print(json.dumps({'ok': ok, 'detail': '' if ok else 'show() did not display initialized plant name', 'actual': out}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex3/ft_plant_factory.py", code, timeout=timeout, input_desc="Plant('Oak', 200.0, 365)"
    )


def _ex4_security_test():
    code = """
import io, json, inspect, contextlib
Plant = getattr(mod, 'Plant', None)
if not inspect.isclass(Plant):
    print(json.dumps({'ok': False, 'detail': 'missing Plant class'}))
else:
    req = ('set_height', 'set_age', 'get_height', 'get_age')
    missing = [m for m in req if not callable(getattr(Plant, m, None))]
    if missing:
        print(json.dumps({'ok': False, 'detail': 'Plant missing: ' + ', '.join(missing)}))
    else:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            p = Plant('Rose', 15.0, 10)
            mangled = [k for k in p.__dict__ if k.startswith('_Plant__')]
            protected = [k for k in p.__dict__ if k.startswith('_') and not k.startswith('__')]
            p.set_height(25)
            h1 = p.get_height()
            p.set_age(30)
            a1 = p.get_age()
            err_buf = io.StringIO()
            with contextlib.redirect_stdout(err_buf):
                p.set_height(-5)
                p.set_age(-10)
            h2 = p.get_height()
            a2 = p.get_age()
        if mangled:
            print(json.dumps({'ok': False, 'detail': 'use protected _attr, not __name mangling', 'actual': ', '.join(mangled)}))
        elif not protected:
            print(json.dumps({'ok': False, 'detail': 'expected protected _attributes on Plant'}))
        elif h1 != 25 or a1 != 30:
            print(json.dumps({'ok': False, 'detail': 'valid set_height/set_age failed', 'expected': '25, 30', 'actual': f'{h1}, {a1}'}))
        elif h2 != 25 or a2 != 30:
            print(json.dumps({'ok': False, 'detail': 'negative value was not rejected', 'expected': '25, 30', 'actual': f'{h2}, {a2}'}))
        elif not err_buf.getvalue().strip():
            print(json.dumps({'ok': False, 'detail': 'rejected update must print an error message'}))
        else:
            print(json.dumps({'ok': True}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex4/ft_garden_security.py", code, timeout=timeout,
        input_desc="set_height/set_age valid & negative"
    )


def _ex5_inheritance_test():
    code = """
import io, json, inspect, contextlib
names = ('Plant', 'Flower', 'Tree', 'Vegetable')
classes = {n: getattr(mod, n, None) for n in names}
missing = [n for n, c in classes.items() if not inspect.isclass(c)]
if missing:
    print(json.dumps({'ok': False, 'detail': 'missing class(es): ' + ', '.join(missing)}))
else:
    Plant, Flower, Tree, Vegetable = (classes[n] for n in names)
    bad_sub = [n for n in ('Flower', 'Tree', 'Vegetable') if not issubclass(classes[n], Plant)]
    if bad_sub:
        print(json.dumps({'ok': False, 'detail': 'must inherit from Plant: ' + ', '.join(bad_sub)}))
    elif not callable(getattr(Flower, 'bloom', None)):
        print(json.dumps({'ok': False, 'detail': 'Flower missing bloom()'}))
    elif not callable(getattr(Tree, 'produce_shade', None)):
        print(json.dumps({'ok': False, 'detail': 'Tree missing produce_shade()'}))
    else:
        print(json.dumps({'ok': True}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex5/ft_plant_types.py", code, timeout=timeout,
        input_desc="Plant <- Flower/Tree/Vegetable"
    )


def _uses_super_test(rel_path: str):
    def runner(ctx: Context, directory: Path, timeout: int) -> PyOutcome:
        try:
            tree = ast.parse((directory / rel_path).read_text(encoding="utf-8"))
        except (OSError, UnicodeError, SyntaxError) as exc:
            return PyOutcome(False, str(exc))
        has_super = any(
            isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == "super"
            for n in ast.walk(tree)
        )
        return PyOutcome(has_super, "" if has_super else "no super() call found")
    return runner


def _ex6_analytics_test():
    code = """
import json, inspect
Plant = getattr(mod, 'Plant', None)
Flower = getattr(mod, 'Flower', None)
Tree = getattr(mod, 'Tree', None)
Seed = getattr(mod, 'Seed', None)
missing = [n for n, c in (('Plant', Plant), ('Flower', Flower), ('Tree', Tree), ('Seed', Seed)) if not inspect.isclass(c)]
if missing:
    print(json.dumps({'ok': False, 'detail': 'missing class(es): ' + ', '.join(missing)}))
elif not issubclass(Seed, Flower):
    print(json.dumps({'ok': False, 'detail': 'Seed must inherit from Flower'}))
else:
    has_static = any(isinstance(v, staticmethod) for v in Plant.__dict__.values())
    has_classmethod = any(isinstance(v, classmethod) for v in Plant.__dict__.values())
    has_nested = any(inspect.isclass(v) for v in Plant.__dict__.values())
    has_standalone = any(
        inspect.isfunction(v) and v.__module__ == mod.__name__
        for k, v in vars(mod).items() if not k.startswith('_')
    )
    problems = []
    if not has_static:
        problems.append('Plant missing @staticmethod')
    if not has_classmethod:
        problems.append('Plant missing @classmethod')
    if not has_nested:
        problems.append('Plant missing nested stats class')
    if not has_standalone:
        problems.append('missing standalone stats display function')
    print(json.dumps({'ok': not problems, 'detail': '; '.join(problems)}))
"""
    return lambda ctx, directory, timeout: expect_json_outcome(
        ctx, directory, "ex6/ft_garden_analytics.py", code, timeout=timeout,
        input_desc="Seed(Flower), staticmethod, classmethod, nested class"
    )


SUITES: tuple[PySuite, ...] = (
    PySuite("ex0", "ex0 · ft_garden_intro", ("ex0/ft_garden_intro.py",), aliases=("0", "intro"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex0/ft_garden_intro.py")),
        PyTestCase("runs directly", "exécution directe",
                   _script_runs_test("ex0/ft_garden_intro.py", min_lines=3, must_contain=("plant", "height", "age"))),
    )),
    PySuite("ex1", "ex1 · ft_garden_data", ("ex1/ft_garden_data.py",), aliases=("1", "data"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex1/ft_garden_data.py")),
        PyTestCase("Plant class & show()", "classe Plant & show()", _ex1_plant_test()),
        PyTestCase("displays >= 3 plants", "affiche >= 3 plantes",
                   _script_runs_test("ex1/ft_garden_data.py", min_lines=3)),
    )),
    PySuite("ex2", "ex2 · ft_plant_growth", ("ex2/ft_plant_growth.py",), aliases=("2", "growth"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex2/ft_plant_growth.py")),
        PyTestCase("grow() & age() mutate state", "grow() & age() modifient l'état", _ex2_growth_test()),
        PyTestCase("simulates week cleanly", "simule une semaine sans erreur",
                   _script_runs_test("ex2/ft_plant_growth.py", min_lines=5)),
    )),
    PySuite("ex3", "ex3 · ft_plant_factory", ("ex3/ft_plant_factory.py",), aliases=("3", "factory"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex3/ft_plant_factory.py")),
        PyTestCase("Plant.__init__ ready", "Plant.__init__ opérationnel", _ex3_factory_test()),
        PyTestCase("displays >= 5 plants", "affiche >= 5 plantes",
                   _script_runs_test("ex3/ft_plant_factory.py", min_lines=5)),
    )),
    PySuite("ex4", "ex4 · ft_garden_security", ("ex4/ft_garden_security.py",), aliases=("4", "security"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex4/ft_garden_security.py")),
        PyTestCase("encapsulation & validation", "encapsulation & validation", _ex4_security_test()),
        PyTestCase("runs cleanly", "s'exécute sans erreur",
                   _script_runs_test("ex4/ft_garden_security.py", min_lines=3)),
    )),
    PySuite("ex5", "ex5 · ft_plant_types", ("ex5/ft_plant_types.py",), aliases=("5", "types"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex5/ft_plant_types.py")),
        PyTestCase("inheritance & methods", "héritage & méthodes", _ex5_inheritance_test()),
        PyTestCase("uses super()", "utilise super()", _uses_super_test("ex5/ft_plant_types.py")),
        PyTestCase("runs cleanly", "s'exécute sans erreur",
                   _script_runs_test("ex5/ft_plant_types.py", min_lines=5)),
    )),
    PySuite("ex6", "ex6 · ft_garden_analytics", ("ex6/ft_garden_analytics.py",), aliases=("6", "analytics"), cases=(
        PyTestCase("silent on import", "silencieux à l'import", _import_silent_test("ex6/ft_garden_analytics.py")),
        PyTestCase("OOP analytics structure", "structure OOP analytics", _ex6_analytics_test()),
        PyTestCase("uses super()", "utilise super()", _uses_super_test("ex6/ft_garden_analytics.py")),
        PyTestCase("runs cleanly", "s'exécute sans erreur",
                   _script_runs_test("ex6/ft_garden_analytics.py", min_lines=5)),
    )),
)

PROJECT = PythonProject(
    key="py01",
    name="python-module-01",
    header="",
    subject="3.2",
    aliases=("python01", "python-module-01", "code_cultivation"),
    specs=SPECS,
    suites=SUITES,
    check_naming_conventions=True,
)
