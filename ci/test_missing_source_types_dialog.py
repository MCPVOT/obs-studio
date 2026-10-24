#!/usr/bin/env -S uv run --script
"""The Missing Source Types warning must not block startup (#2161).

Usage:
    uv run python -m unittest ci/test_missing_source_types_dialog.py

#1883 showed the warning with OBSMessageBox::warning(), a modal exec(),
from inside LoadData(). When a scene collection used a source type no
module provides, the main thread waited on the dialog before OBS finished
loading, so the frontend never became ready: the release run's packaged
launch check (win-dshow.dll removed, run 40097160881) timed out instead of
reporting the missing camera module. The warning now opens after the
current event, without exec(), as the Missing Files dialog does.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "frontend/widgets/OBSBasic_SceneCollections.cpp.hpp"


def body(text: str, signature: str) -> str:
    start = text.index(signature)
    depth = 0
    for i in range(text.index("{", start), len(text)):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        if depth == 0:
            return text[start:i + 1]
    raise AssertionError(f"unterminated {signature}")


class MissingSourceTypesDialogTest(unittest.TestCase):
    def setUp(self):
        self.show = body(SOURCE.read_text(encoding="utf-8"),
                         "void OBSBasic::ShowMissingSourceTypes(")

    def test_warning_does_not_run_a_nested_modal_loop(self):
        self.assertNotIn("OBSMessageBox::", self.show)
        self.assertNotRegex(self.show, r"\.exec\(|->exec\(")

    def test_warning_opens_after_loading_and_frees_itself(self):
        self.assertIn("QTimer::singleShot(0,", self.show)
        self.assertIn("->show()", self.show)
        self.assertRegex(self.show, r"setAttribute\(Qt::WA_DeleteOnClose")

    def test_still_logs_and_stays_quiet_in_safe_mode(self):
        self.assertIn("which no loaded module provides", self.show)
        self.assertLess(self.show.index("if (safe_mode)"), self.show.index("QTimer::singleShot"))


if __name__ == "__main__":
    unittest.main()
