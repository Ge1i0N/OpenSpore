import importlib.util
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parents[1]))
spec = importlib.util.spec_from_file_location('gge_userdata_correlate', Path(__file__).parents[1] / 'gge_userdata_correlate.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class CorrelationTests(unittest.TestCase):
    def test_exact_keys_duplicates_and_readability_are_separate(self):
        rows = {f: [(3, 2, 1, 9, 8, 7)] * 2 for f in mod.TABLES}
        records = [{'index': 0, 'key': (3, 2, 1), 'packed': 20}]
        r = mod.correlate(rows, records)['07db67db']
        self.assertEqual(r['left']['counts'], {'unique_directory_entry_payload_not_checked': 2})
        self.assertEqual(r['right']['counts'], {'absent_in_selected_directory': 2})
        records.append({'index': 1, 'key': (3, 2, 1), 'packed': 0})
        self.assertEqual(mod.correlate(rows, records)['07db67db']['left']['counts'], {'ambiguous': 2})

    def test_zero_size_not_usable_and_no_word_permutation(self):
        rows = {f: [(3, 2, 1, 1, 2, 3)] for f in mod.TABLES}
        r = mod.correlate(rows, [{'index': 0, 'key': (3, 2, 1), 'packed': 0}])['07db67db']
        self.assertIn('unique_directory_entry_unreadable_or_zero_size', r['left']['counts'])
        self.assertIn('absent_in_selected_directory', r['right']['counts'])

    def test_related_type_is_not_exact_match(self):
        rows = {f: [(3, 2, 1, 9, 8, 7)] for f in mod.TABLES}
        r = mod.correlate(rows, [{'index': 0, 'key': (3, 99, 1), 'packed': 10}])['07db67db']['left']
        self.assertEqual(r['counts'], {'absent_in_selected_directory': 1})
        self.assertEqual(r['rows_with_same_group_instance_other_type'], 1)
        self.assertEqual(r['other_type_entry_counts'], {'00000063': 1})

    def test_inventory_complete_ordinals_required(self):
        header = 'EXPLICIT DBPF INVENTORY ONLY path=p records=1 kind=4 groups=1\n'
        row = '[0] instance=0x1 type=0x2 group=0x3 packed=4 memory=4 compression=0x0\n'
        self.assertEqual(mod.parse_inventory(header + row)[0]['key'], (3, 2, 1))
        for text in (header, header + row.replace('[0]', '[1]'), header + row + row,
                     header + row.replace('packed=4', 'packed=?')):
            with self.assertRaises(ValueError):
                mod.parse_inventory(text)


if __name__ == '__main__':
    unittest.main()
