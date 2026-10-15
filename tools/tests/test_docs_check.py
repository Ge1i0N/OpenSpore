import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.docs_check import missing_links


class DocsCheckTest(unittest.TestCase):
    def test_links_and_exclusions(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/"exists file.md").write_text("# heading", encoding="utf-8")
            doc = root/"index.md"
            doc.write_text('[valid](exists%20file.md#heading)\n[missing](gone.md)\n'
                           '![image](gone.png)\n[external](https://example.invalid/no-fetch)\n'
                           '[anchor](#not-checked)\n```powershell\n[example](ignored.md)\n```\n',
                           encoding="utf-8")
            errors = list(missing_links(doc))
            self.assertEqual(len(errors), 2)
            self.assertIn("gone.md", errors[0])
            self.assertIn("gone.png", errors[1])


if __name__ == "__main__":
    unittest.main()
