#!/usr/bin/env python3
"""Import ct_all engine records, joining Manalink.csv by full card name.

DLL code addresses resolve through ManalinkEh.asm's veneer list. Original exe
handlers use the explicitly reviewed address map beside this script. Unknown
addresses or missing names are errors, never silently replaced with vanilla.
"""

import argparse
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENEER_RE = re.compile(
    r"^\s*jmp\s+near\s+_(\S+)\s*;\s*(?:0x)?(0?2[0-9a-f]{6})\b", re.M | re.I
)


def read_csv(path):
    with path.open(encoding="cp1252", newline="") as source:
        return list(csv.reader(source, delimiter=";"))


def number(value):
    return int(value or "0")


def signed_byte(value):
    return (number(value) + 128) % 256 - 128


def bits(row, start, count):
    return sum((1 if number(row[start + i]) else 0) << (count - i - 1)
               for i in range(count))


def c_string(value):
    # Emit byte escapes, keeping imported strings independent of compiler locale.
    return '"' + ''.join(chr(b) if 32 <= b < 127 and chr(b) not in '\\"'
                         else f"\\{b:03o}" for b in value.encode("cp1252")) + '"'


def load_handler_overrides(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("version") != 1 or not isinstance(data.get("cards"), dict):
        raise ValueError("Unsupported card handler override schema")
    overrides = {}
    for key, record in data["cards"].items():
        csvid = int(key)
        if str(csvid) != key or csvid < 0 or not record.get("name") or not record.get("reason"):
            raise ValueError(f"Invalid card handler override: {key}")
        if not re.fullmatch(r"manalinkeh_card_[a-z0-9_]+", record.get("handler", "")):
            raise ValueError(f"Invalid override handler for {key}")
        if int(record["source_address"], 16) != 0x00401000:
            raise ValueError(f"Override {key} must replace the explicit dummy address")
        if record.get("new_field_or", 0) not in (0, 1):
            raise ValueError(f"Unsupported cost-hook flags for {key}")
        overrides[csvid] = record
    return overrides


def override_handler(overrides, csvid, name, address, handler):
    record = overrides.get(csvid)
    if record is None:
        return handler
    if name != record["name"] or address != int(record["source_address"], 16) or handler != "card_dummy":
        raise ValueError(f"Stale handler override for CSVID {csvid}: {name} at {address:#x} ({handler})")
    return record["handler"]


def generate(source_dir):
    ct_path, ml_path = source_dir / "ct_all.csv", source_dir / "Manalink.csv"
    all_ct = [row for row in read_csv(ct_path)[1:] if row[0].isdigit()]
    # Zero code addresses denote unimplemented cards. They must not enter the
    # engine's draw/reward pool, where a card handler is called unconditionally.
    ct = [row for row in all_ct if int(row[34], 16) != 0]
    ml = read_csv(ml_path)
    cards = {row[1]: row for row in ml[1:] if row[0].isdigit()}
    veneers = {int(address, 16): "manalinkeh_" + name.rstrip(";")
               for name, address in VENEER_RE.findall(
                   (ROOT / "src/manalinkeh/ManalinkEh.asm").read_text())}
    fallback = {int(address, 16): name for address, name in json.loads(
        (ROOT / "tools/carddata_legacy_handlers.json").read_text()).items()}
    overrides_path = ROOT / "tools/carddata_handler_overrides.json"
    overrides = load_handler_overrides(overrides_path)
    applied_overrides = set()
    # Manalink leaves the campaign fields unused. Restore their original
    # meanings by CSV ID without changing engine order or card handlers.
    campaign = {int(csvid): values for csvid, values in json.loads(
        (ROOT / "tools/carddata_campaign_metadata.json").read_text())["cards"].items()}
    rows, handlers, effect_ids = [], set(), {}
    for iid, row in enumerate(ct):
        card = None if row[2].startswith("#") else cards[row[2]]
        csvid = int(row[2][1:]) if card is None else int(card[0])
        address = int(row[34], 16)
        handler = veneers.get(address) or fallback.get(address)
        if handler is None:
            raise ValueError(f"unmapped handler {row[34]} for {row[2]}")
        handler = override_handler(overrides, csvid, row[2], address, handler)
        if csvid in overrides:
            applied_overrides.add(csvid)
        if handler.startswith("manalinkeh_"):
            handlers.add(handler)
        if card is None:
            effect_ids[row[1]] = iid
        campaign_fields = campaign.get(csvid) if card is not None else None
        extra_abilities = bits(row, 67, 32)
        expansion = number(row[108]) & 255
        if campaign_fields is not None:
            if iid > 0x3fff:
                raise ValueError(f"campaign card {row[2]} exceeds adventure deck ID capacity")
            expansion = campaign_fields[0]
            extra_abilities = (extra_abilities & ~0xf80) | campaign_fields[1]
        fields = [str(255 if csvid == -1 else 0), c_string(row[1][:17]),
                  str(csvid), f"0x{bits(row, 3, 8):02x}", str(number(row[11])),
                  str((bits(row, 12, 8) + 128) % 256 - 128),
                  "{ %d, %d, %d }" % (signed_byte(row[21]), signed_byte(row[20]), signed_byte(row[22])),
                  str(number(row[23])), str(number(row[24])),
                  str(bits(row, 25, 8) | overrides.get(csvid, {}).get("new_field_or", 0)), str(number(row[33])), handler,
                  f"0x{bits(row, 35, 32):08x}u", f"0x{extra_abilities:08x}u",
                  str(number(row[99]) & 255), str(bits(row, 100, 8)),
                  # Byte 0x43 is padding; ct2exe writes the signed engine rating
                  # at 0x44. Manalink's AI Base Value belongs to Cards.dat instead.
                  str(expansion), "0", str(number(row[109]))]
        rows.append("  { " + ", ".join(fields) + " }, /* %d: %s */" %
                    (iid, row[2].replace("*/", "* /")))
    if applied_overrides != overrides.keys():
        raise ValueError(f"Handler overrides absent from source: {sorted(overrides.keys() - applied_overrides)}")
    sentinel = next(i for i, row in enumerate(ct) if row[2] == "#-1")
    assert len(ct) - sentinel == 16, "the engine requires sixteen dynamic slots"
    damage = effect_ids["Damage"]
    assert effect_ids["Activation"] == damage + 19
    assert effect_ids["Generic"] == damage + 15
    # The DLL creates these effect types itself. Give the static host permanent
    # records so they neither alias real cards nor consume dynamic copy slots.
    custom = sentinel
    activated = sentinel + 1
    custom_rows = []
    for iid, name, base, old_handler, handler in (
        (custom, "Custom Legacy", "Generic", "card_generic", "manalinkeh_legacy_effect_custom"),
        (activated, "Activated Legacy", "Asp Sting", "manalinkeh_legacy_effect_activated",
         "manalinkeh_legacy_effect_activated"),
    ):
        handlers.add(handler)
        record = rows[effect_ids[base]].replace(c_string(base), c_string(name), 1)
        record = record.replace(f', {old_handler},', f', {handler},', 1)
        custom_rows.append(re.sub(r'/\*.*\*/$', f'/* {iid}: static host effect */', record))
    rows[sentinel:sentinel] = custom_rows
    # Renumber only the trailing dynamic placeholders; all asset IDs stay put.
    for iid in range(sentinel + 2, len(rows)):
        rows[iid] = re.sub(r"/\* \d+:", f"/* {iid}:", rows[iid])
    sentinel += 2
    # Preserve the host's activation and generic-effect offsets.
    prefix = ("/* Generated by tools/import_carddata.py; do not edit by hand.\n"
              f" * ct_all.csv SHA256: {hashlib.sha256(ct_path.read_bytes()).hexdigest()}\n"
              f" * Manalink.csv SHA256: {hashlib.sha256(ml_path.read_bytes()).hexdigest()}\n */\n")
    prefix = prefix.replace("\n */", f"\n * Handler overrides SHA256: {hashlib.sha256(overrides_path.read_bytes()).hexdigest()}\n */")
    output = ROOT / "src/magic/src"
    (output / "carddata.c").write_text(prefix + '#include "cards.h"\n'
        '#include "carddata_generated.h"\n\n'
        + ''.join(f"extern int {name}(int, int, event_t);\n" for name in sorted(handlers))
        + '\nint card_no_blocking(int, int, event_t);\n'
        'int card_permanent_color(int, int, event_t);\n\n'
        'card_data_t global_cards_data[SHANDALAR_CARD_DATA_COUNT] = {\n'
        + '\n'.join(rows) + '\n};\n', encoding="utf-8")
    (output / "carddata_generated.h").write_text(prefix +
        '#ifndef SHANDALAR_CARD_DATA_GENERATED_H\n#define SHANDALAR_CARD_DATA_GENERATED_H\n'
        f'#define SHANDALAR_CARD_DATA_COUNT {len(rows)}\n'
        f'#define SHANDALAR_CARD_DATA_ACTIVE_COUNT {sentinel}\n'
        f'#define SHANDALAR_DAMAGE_CARD_IID {damage}\n'
        f'#define SHANDALAR_TRANSMUTE_CARD_IID {effect_ids["TransmuteFX"]}\n'
        f'#define SHANDALAR_CUSTOM_LEGACY_CARD_IID {custom}\n'
        f'#define SHANDALAR_ACTIVATED_LEGACY_CARD_IID {activated}\n'
        '#endif\n', encoding="utf-8")
    print(f"Imported {len(rows)} engine entries; {len(handlers)} distinct Manalink handlers; "
          f"{len(all_ct) - len(ct)} uncoded CSV entries excluded.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_dir", type=Path)
    generate(parser.parse_args().source_dir)
