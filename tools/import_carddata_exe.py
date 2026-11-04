#!/usr/bin/env python3
"""Reconstruct the October 2022 engine table from Magic.exe and matching Cards.dat.

The executable is read as data. Original internal IDs, null bindings, and the
sixteen dynamic slots are retained; only two static-host effect records are
inserted immediately before those dynamic slots.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from import_carddata import ROOT, VENEER_RE, c_string, load_handler_overrides


def extract(exe):
    data = exe.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError('Expected a PE executable')
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    if machine != 0x14c or struct.unpack_from('<H', data, pe + 24)[0] != 0x10b:
        raise ValueError('Expected an x86 PE32 executable')
    sections = []
    for index in range(count):
        pos = pe + 24 + optional_size + index * 40
        name = data[pos:pos + 8].rstrip(b'\0')
        _, rva, size, offset = struct.unpack_from('<4I', data, pos + 8)
        if name == b'.cards':
            sections.append(data[offset:offset + size])
    if len(sections) != 1 or sections[0][:16] != b'*' * 16:
        raise ValueError('Missing or unrecognized .cards section')
    raw = sections[0]
    records = []
    for pos in range(16, len(raw) - 71, 72):
        record = raw[pos:pos + 72]
        if record == bytes(72):
            break
        name = record[1:36].split(b'\0')[0].decode('cp1252')
        if not name:
            raise ValueError(f'Empty card name at IID {len(records)}')
        records.append(dict(iid=len(records), raw=record, name=name,
                            csvid=struct.unpack_from('<i', record, 36)[0],
                            address=struct.unpack_from('<I', record, 52)[0]))
    if len(records) < 16 or any(r['csvid'] != -1 or r['raw'][0] != 255 for r in records[-16:]):
        raise ValueError('Missing sixteen trailing dynamic slots')
    if any(r['csvid'] == -1 for r in records[:-16]):
        raise ValueError('Unexpected dynamic slot inside card table')
    return data, records


def database_names(path):
    data = path.read_bytes()
    count, text_size = struct.unpack_from('<II', data)
    text_start = 8 + count * 152
    if text_start + text_size + 4 > len(data):
        raise ValueError('Truncated Cards.dat')
    text = data[text_start:text_start + text_size]
    names = {}
    for index in range(count):
        pos = 8 + index * 152
        csvid, offset = struct.unpack_from('<II', data, pos)
        if offset >= text_size:
            raise ValueError(f'Invalid name offset in Cards.dat record {index}')
        end = text.find(b'\0', offset)
        if end < 0:
            raise ValueError(f'Unterminated name in Cards.dat record {index}')
        names[csvid] = text[offset:end].decode('cp1252')
    return data, names


def correct_bindings(records, source_hash, corrections, veneers):
    if corrections.get('version') != 1 or corrections.get('source_sha256') != source_hash:
        raise ValueError('Correction manifest does not match source executable')
    by_id = {r['csvid']: r for r in records if r['csvid'] >= 0}
    applied = []
    for key, correction in corrections['cards'].items():
        row = by_id[int(key)]
        expected = (correction['name'], correction['source_iid'], int(correction['source_address'], 16))
        if (row['name'], row['iid'], row['address']) != expected:
            raise ValueError(f'Stale binding correction for CSVID {key}')
        address = int(correction['corrected_address'], 16)
        if veneers.get(address) != correction['handler']:
            raise ValueError(f'Unexpected corrected handler for CSVID {key}')
        row['effective_address'] = address
        applied.append(int(key))
    return applied


def generate(exe, cards_dat, output, veneer_path=None):
    image, records = extract(exe)
    database, names = database_names(cards_dat)
    source_hash = hashlib.sha256(image).hexdigest()
    veneers = {int(a, 16): 'manalinkeh_' + name.rstrip(';') for name, a in
               VENEER_RE.findall((veneer_path or ROOT / 'src/manalinkeh/ManalinkEh.asm').read_text())}
    correction_path = ROOT / 'tools/carddata_2022_corrections.json'
    applied = correct_bindings(records, source_hash, json.loads(correction_path.read_text()), veneers)
    fallback = {int(a, 16): n for a, n in json.loads((ROOT / 'tools/carddata_legacy_handlers.json').read_text()).items()}
    overrides = load_handler_overrides(ROOT / 'tools/carddata_handler_overrides.json')
    campaign = json.loads((ROOT / 'tools/carddata_campaign_metadata.json').read_text())['cards']
    rows, handlers, effects, recovered = [], set(), {}, []
    for r in records:
        raw, csvid, iid = r['raw'], r['csvid'], r['iid']
        address = r.get('effective_address', r['address'])
        handler = veneers.get(address) or fallback.get(address)
        if handler is None:
            raise ValueError(f'Unmapped address {address:#x}: IID {iid}, {r["name"]}')
        is_effect = bool(raw[40] & 0x80) or raw[40] == 0
        full_name = '#' + str(csvid) if is_effect or csvid == -1 else names[csvid]
        override = overrides.get(csvid)
        new_field = raw[50]
        if override and address == int(override['source_address'], 16):
            if full_name != override['name'] or handler != 'card_dummy':
                raise ValueError(f'Stale dummy recovery for CSVID {csvid}')
            handler = override['handler']
            new_field |= override.get('new_field_or', 0)
            recovered.append(csvid)
        if handler.startswith('manalinkeh_'):
            handlers.add(handler)
        expansion, extra = raw[66], struct.unpack_from('<I', raw, 60)[0]
        if not is_effect and str(csvid) in campaign:
            if iid > 0x3fff:
                raise ValueError(f'Campaign card {full_name} exceeds the adventure ID field')
            expansion, flags = campaign[str(csvid)]
            extra = (extra & ~0xf80) | flags
        if is_effect:
            effects[r['name']] = iid
        fields = [str(raw[0]), c_string(r['name']), str(csvid), str(raw[40]),
                  str(struct.unpack_from('<b', raw, 41)[0]), str(struct.unpack_from('<b', raw, 42)[0]),
                  '{ %d, %d, %d }' % struct.unpack_from('<3b', raw, 43),
                  str(struct.unpack_from('<h', raw, 46)[0]), str(struct.unpack_from('<h', raw, 48)[0]),
                  str(new_field), str(raw[51]), handler,
                  f'0x{struct.unpack_from("<I", raw, 56)[0]:08x}u', f'0x{extra:08x}u',
                  str(raw[64]), str(raw[65]), str(expansion), str(raw[67]),
                  str(struct.unpack_from('<i', raw, 68)[0])]
        rows.append('  { ' + ', '.join(fields) + ' }, /* %d: %s */' % (iid, full_name.replace('*/', '* /')))
    sentinel = len(records) - 16
    custom, activated = sentinel, sentinel + 1
    custom_rows = []
    for iid, name, base, old_handler, handler in (
        (custom, 'Custom Legacy', 'Generic', 'card_generic', 'manalinkeh_legacy_effect_custom'),
        (activated, 'Activated Legacy', 'Asp Sting', 'manalinkeh_legacy_effect_activated', 'manalinkeh_legacy_effect_activated')):
        handlers.add(handler)
        row = rows[effects[base]].replace(c_string(base), c_string(name), 1)
        row = row.replace(', ' + old_handler + ',', ', ' + handler + ',', 1)
        custom_rows.append(re.sub(r'/\*.*\*/$', f'/* {iid}: static host effect */', row))
    rows[sentinel:sentinel] = custom_rows
    for iid in range(sentinel + 2, len(rows)):
        rows[iid] = re.sub(r'/\* \d+:', f'/* {iid}:', rows[iid])
    prefix = ('/* Generated by tools/import_carddata_exe.py; do not edit by hand.\n'
              f' * Magic.exe SHA256: {source_hash}\n'
              f' * Cards.dat SHA256: {hashlib.sha256(database).hexdigest()}\n'
              f' * Corrections SHA256: {hashlib.sha256(correction_path.read_bytes()).hexdigest()}\n */\n')
    output.mkdir(parents=True, exist_ok=True)
    (output / 'carddata.c').write_text(prefix + '#include "cards.h"\n#include "carddata_generated.h"\n\n' +
        ''.join(f'extern int {name}(int, int, event_t);\n' for name in sorted(handlers)) +
        '\nint card_no_blocking(int, int, event_t);\nint card_permanent_color(int, int, event_t);\n\n'
        'card_data_t global_cards_data[SHANDALAR_CARD_DATA_COUNT] = {\n' + '\n'.join(rows) + '\n};\n', encoding='utf-8')
    (output / 'carddata_generated.h').write_text(prefix + '#ifndef SHANDALAR_CARD_DATA_GENERATED_H\n#define SHANDALAR_CARD_DATA_GENERATED_H\n' +
        '\n'.join(f'#define {name} {value}' for name, value in (
            ('SHANDALAR_CARD_DATA_COUNT', len(rows)), ('SHANDALAR_CARD_DATA_ACTIVE_COUNT', sentinel + 2),
            ('SHANDALAR_DAMAGE_CARD_IID', effects['Damage']), ('SHANDALAR_TRANSMUTE_CARD_IID', effects['TransmuteFX']),
            ('SHANDALAR_CUSTOM_LEGACY_CARD_IID', custom), ('SHANDALAR_ACTIVATED_LEGACY_CARD_IID', activated))) +
        '\n#endif\n', encoding='utf-8')
    print(f'Reconstructed {len(rows)} records; {len(handlers)} handlers; corrections {applied}; {len(recovered)} dummy recoveries.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--cards-dat', type=Path)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'src/magic/src')
    args = parser.parse_args()
    generate(args.exe, args.cards_dat or args.exe.with_name('Cards.dat'), args.output_dir)
