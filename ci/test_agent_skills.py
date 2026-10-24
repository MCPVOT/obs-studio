#!/usr/bin/env -S uv run --script
"""Every agent skill is reachable from AGENTS.md (#2075).

Agents without skill support read AGENTS.md and follow its links, so a skill
that AGENTS.md does not link is invisible to them. Each skill also needs the
frontmatter name its directory has, and the worker loop skill must link
SECURITY.md like the other worker documents.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent.parent
SKILLS = ROOT / ".claude/skills"


class AgentSkillsTest(unittest.TestCase):
    def test_worker_loop_skill_exists(self):
        self.assertTrue((SKILLS / "obs-worker-loop/SKILL.md").is_file())

    def test_agents_md_links_every_skill(self):
        agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
        for skill in sorted(p.parent.name for p in SKILLS.glob("*/SKILL.md")):
            with self.subTest(skill=skill):
                self.assertIn(f"(.claude/skills/{skill}/SKILL.md)", agents)

    def test_frontmatter_name_matches_directory(self):
        for path in sorted(SKILLS.glob("*/SKILL.md")):
            with self.subTest(skill=path.parent.name):
                m = re.match(r"---\nname: (\S+)\n", path.read_text(encoding="utf-8"))
                self.assertIsNotNone(m, "missing frontmatter name")
                self.assertEqual(m.group(1), path.parent.name)

    def test_worker_skills_forbid_new_clones_and_worktrees(self):
        # A new clone or worktree starts every build cache cold (zccache,
        # cargo target, CMake trees, bosn volumes).
        for skill in ("obs-worker", "obs-worker-loop"):
            with self.subTest(skill=skill):
                text = (SKILLS / skill / "SKILL.md").read_text(encoding="utf-8")
                self.assertIn("no new clones or worktrees", text.lower())
                self.assertNotIn("branch or worktree", text)
                self.assertNotIn("One clone per concurrent branch", text)

    def test_coordinator_decides_or_escalates_open_decisions(self):
        # The coordinator researches each `decision-needed` issue: it answers
        # with `Decided:` when the rules, docs or code settle it, and moves
        # the rest to `human-review-needed` for the maintainer.
        text = (SKILLS / "obs-coordinator/SKILL.md").read_text(encoding="utf-8")
        self.assertIn("decision-needed", text)
        self.assertIn("human-review-needed", text)
        self.assertIn("Decided:", text)
        self.assertNotIn("you don't\ndecide them", text)
        self.assertNotIn("You collect them; you don't", text)

    def test_worker_loop_links_security_md(self):
        text = (SKILLS / "obs-worker-loop/SKILL.md").read_text(encoding="utf-8")
        self.assertRegex(text, r"\]\([./]*SECURITY\.md[#)]")


if __name__ == "__main__":
    unittest.main()
