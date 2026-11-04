"""Checks for PE card-table ordering and narrowly scoped recovered-pointer repairs."""
import copy
from pathlib import Path
import struct
import unittest

from import_carddata_exe import extract, correct_bindings


class CardDataExeTests(unittest.TestCase):
    def image(self):
        records = []
        for name, csvid, address in [('First', 42, 0), ('Second', 11, 0x2010000)] + [('Dummy', -1, 0)] * 16:
            row = bytearray(72)
            row[0] = 255 if csvid == -1 else 0
            row[1:1 + len(name)] = name.encode()
            struct.pack_into('<i', row, 36, csvid)
            struct.pack_into('<I', row, 52, address)
            records.append(bytes(row))
        section = b'*' * 16 + b''.join(records) + bytes(72)
        image = bytearray(512)
        image[:2] = b'MZ'
        struct.pack_into('<I', image, 0x3c, 0x80)
        image[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HH', image, 0x84, 0x14c, 1)
        struct.pack_into('<H', image, 0x94, 0xe0)
        struct.pack_into('<H', image, 0x98, 0x10b)
        image[0x178:0x180] = b'.cards\0\0'
        struct.pack_into('<4I', image, 0x180, len(section), 0x1000, len(section), 512)
        return bytes(image) + section, records

    def test_original_iids_and_null_handlers_are_preserved(self):
        image, expected = self.image()
        class Image:
            def read_bytes(self): return image
        original, records = extract(Image())
        self.assertEqual(original, image)
        self.assertEqual([r['raw'] for r in records], expected)
        self.assertEqual([(r['iid'], r['csvid'], r['address']) for r in records[:2]],
                         [(0, 42, 0), (1, 11, 0x2010000)])
        self.assertEqual(Image().read_bytes(), image)

    def test_missing_dynamic_slots_rejected(self):
        image, _ = self.image()
        image = bytearray(image)
        struct.pack_into('<i', image, 512 + 16 + 17 * 72 + 36, 999)
        class Image:
            def read_bytes(self): return image
        with self.assertRaisesRegex(ValueError, 'dynamic slots'):
            extract(Image())

    def test_reviewed_correction_preserves_raw_pointer(self):
        row = dict(iid=21922, csvid=21990, name='Skyclave Basilica', address=0x42012f4a)
        correction = dict(version=1, source_sha256='expected', cards={'21990': dict(
            name=row['name'], source_iid=row['iid'], source_address='0x42012f4a',
            corrected_address='0x02012f4a', handler='land')})
        self.assertEqual(correct_bindings([row], 'expected', correction, {0x2012f4a: 'land'}), [21990])
        self.assertEqual(row['address'], 0x42012f4a)
        self.assertEqual(row['effective_address'], 0x2012f4a)
        for key, value in [('name', 'Another card'), ('source_iid', 1), ('source_address', '0x1234')]:
            stale = copy.deepcopy(correction)
            stale['cards']['21990'][key] = value
            with self.assertRaisesRegex(ValueError, 'Stale binding'):
                correct_bindings([row], 'expected', stale, {0x2012f4a: 'land'})
        with self.assertRaisesRegex(ValueError, 'source executable'):
            correct_bindings([row], 'different', correction, {0x2012f4a: 'land'})
        with self.assertRaisesRegex(ValueError, 'corrected handler'):
            correct_bindings([row], 'expected', correction, {0x2012f4a: 'other'})


if __name__ == '__main__':
    unittest.main()
