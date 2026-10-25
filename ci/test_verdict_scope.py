"""Every setup-soldr step in build-project.yaml names its verdict scope.

build-project.yaml is a reusable workflow: push.yaml calls it on master and
ci.yml on pull requests and the merge queue. setup-soldr hashes the calling
workflow into the verdict manifest's scope unless `verdict-scope` replaces
it, so without it the verdicts master publishes never reach a PR, and every
PR re-runs every Clippy and Dylint pass (forge-ci/setup-soldr#604).

    uv run python -m unittest discover -s ci -p test_verdict_scope.py
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "build-project.yaml"
USES = re.compile(r"^(\s*)(?:-\s+)?uses:\s*zackees/setup-soldr@")


def setup_soldr_steps(text: str) -> list[tuple[int, list[str]]]:
    """(line number, step body lines) of every setup-soldr step."""
    lines = text.splitlines()
    steps = []
    for i, line in enumerate(lines):
        match = USES.match(line)
        if not match:
            continue
        indent = len(match.group(1))
        body = []
        for following in lines[i + 1:]:
            stripped = following.lstrip()
            if stripped and (len(following) - len(stripped) < indent or stripped.startswith("- ")):
                break
            body.append(following)
        steps.append((i + 1, body))
    return steps


class VerdictScopeTest(unittest.TestCase):
    def test_every_setup_soldr_step_names_its_scope(self):
        steps = setup_soldr_steps(WORKFLOW.read_text(encoding="utf-8"))
        self.assertGreater(len(steps), 0)
        missing = [line for line, body in steps
                   if not any(re.match(r"\s*verdict-scope:\s*build-project\s*$", b) for b in body)]
        self.assertEqual(missing, [], "setup-soldr steps without `verdict-scope: build-project` at lines "
                                      f"{missing} of {WORKFLOW.relative_to(ROOT)}")

    def test_the_parser_sees_one_step_body(self):
        text = ("      - uses: zackees/setup-soldr@abc\n        with:\n          verdict-scope: build-project\n"
                "      - uses: zackees/setup-soldr@abc\n        with:\n          target: x\n")
        steps = setup_soldr_steps(text)
        self.assertEqual([line for line, _ in steps], [1, 4])
        self.assertIn("          verdict-scope: build-project", steps[0][1])
        self.assertNotIn("          verdict-scope: build-project", steps[1][1])


if __name__ == "__main__":
    unittest.main()
