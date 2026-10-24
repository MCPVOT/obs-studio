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

    def test_worker_loop_links_security_md(self):
        text = (SKILLS / "obs-worker-loop/SKILL.md").read_text(encoding="utf-8")
        self.assertRegex(text, r"\]\([./]*SECURITY\.md[#)]")


if __name__ == "__main__":
    unittest.main()
