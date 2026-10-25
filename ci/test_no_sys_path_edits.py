"""No Python file in this repo edits sys.path.

`sys.path.insert(0, ...)` (or append, extend, assignment) makes an import
work only from one launch path: it hides a script that is not run through
`uv run` (the script's own directory is already sys.path[0] there, and
`unittest discover -s ci` puts ci/ on it), and it lets a module reach into
another directory without saying so. Run scripts with `uv run <file>`, import
siblings by name, and load a module from another directory by its path with
importlib (see ci/test_conversion_measure.py). Part of ./lint through the
ci/ self-tests.

    uv run python -m unittest discover -s ci -p test_no_sys_path_edits.py
"""

import ast
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Vendored or upstream trees this repo does not edit.
EXCLUDED = ("deps/", "plugins/obs-websocket/", "plugins/obs-browser/")
METHODS = {"insert", "append", "extend", "remove", "clear", "pop", "__setitem__"}


def is_sys_path(node: ast.AST) -> bool:
    return (isinstance(node, ast.Attribute) and node.attr == "path"
            and isinstance(node.value, ast.Name) and node.value.id == "sys")


def edits(tree: ast.AST) -> list[int]:
    """Line numbers of every statement that changes sys.path."""
    lines = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
                and node.func.attr in METHODS and is_sys_path(node.func.value):
            lines.append(node.lineno)
        elif isinstance(node, (ast.Assign, ast.AugAssign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                base = target.value if isinstance(target, ast.Subscript) else target
                if is_sys_path(base):
                    lines.append(node.lineno)
    return sorted(lines)


def python_files() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z", "--", "*.py"], cwd=ROOT,
                         capture_output=True, text=True, check=True).stdout
    return [p for p in out.split("\0") if p and not p.startswith(EXCLUDED)]


class NoSysPathEditsTest(unittest.TestCase):
    def test_the_detector(self):
        bad = ("import sys\nsys.path.insert(0, 'x')\nsys.path.append('y')\n"
               "sys.path += ['z']\nsys.path = ['w']\nsys.path[0:0] = ['v']\n")
        self.assertEqual(edits(ast.parse(bad)), [2, 3, 4, 5, 6])
        fine = "import sys\n# sys.path.insert(0, 'x')\nprint(sys.path)\npaths = list(sys.path)\n"
        self.assertEqual(edits(ast.parse(fine)), [])

    def test_no_file_edits_sys_path(self):
        found = []
        for rel in python_files():
            path = ROOT / rel
            if not path.is_file():
                continue
            try:
                tree = ast.parse(path.read_text(encoding="utf-8"), rel)
            except SyntaxError:
                continue
            found += [f"{rel}:{line}" for line in edits(tree)]
        self.assertEqual(found, [], "sys.path edited (run through uv run; import by name or by path "
                                    "with importlib):\n" + "\n".join(found))


if __name__ == "__main__":
    unittest.main()
