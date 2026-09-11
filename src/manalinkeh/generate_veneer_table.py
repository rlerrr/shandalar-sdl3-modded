#!/usr/bin/env python3
"""Generate a native function registry from ManalinkEh's ordered veneers."""

import re
import sys
from pathlib import Path


VENEER_RE = re.compile(
    r"^\s*jmp\s+near\s+_(\S+)\s*;\s*(?:0x)?(0?2[0-9a-f]{6})\b",
    re.IGNORECASE,
)
IDENTIFIER_RE = re.compile(r"[A-Za-z_]\w*\Z")
STDCALL_RE = re.compile(r"([A-Za-z_]\w*)@(\d+)\Z")


def read_veneers(path):
    entries = []
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        match = VENEER_RE.match(line)
        if match:
            # A single veneer spells the NASM comment delimiter immediately
            # after the symbol.  The others separate it with whitespace.
            name = match.group(1).rstrip(";")
            entries.append((int(match.group(2), 16), name, line_number))
    if not entries:
        raise ValueError(f"no veneers found in {path}")
    for index, (address, _name, line_number) in enumerate(entries):
        expected = entries[0][0] + index * 5
        if address != expected:
            raise ValueError(
                f"non-contiguous veneer at {path}:{line_number}: "
                f"expected 0x{expected:08x}, found 0x{address:08x}"
            )
    return entries


def write_header(path, base, count):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        """#ifndef MANALINKEH_VENEER_TABLE_H
#define MANALINKEH_VENEER_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef void (*manalinkeh_opaque_fn)(void);

#define MANALINKEH_VENEER_BASE 0x%08xu
#define MANALINKEH_VENEER_STRIDE 5u
#define MANALINKEH_VENEER_COUNT %du

extern const manalinkeh_opaque_fn
    manalinkeh_veneer_table[MANALINKEH_VENEER_COUNT];

manalinkeh_opaque_fn manalinkeh_resolve_veneer(uint32_t legacy_address);

#endif
"""
        % (base, count),
        encoding="utf-8",
    )


def write_source(path, entries):
    valid_names = sorted(
        {name for _address, name, _line in entries if IDENTIFIER_RE.fullmatch(name)}
    )
    lines = [
        '#include "manalinkeh_veneer_table.h"',
        "",
        "/* Deliberately opaque: the veneer list contains several signatures. */",
    ]
    lines.extend(f"extern void {name}(void);" for name in valid_names)
    stdcall_names = sorted(
        {
            (match.group(1), int(match.group(2)) // 4)
            for _address, name, _line in entries
            if (match := STDCALL_RE.fullmatch(name))
        }
    )
    for name, argument_count in stdcall_names:
        arguments = ", ".join("uint32_t" for _ in range(argument_count))
        lines.append(f"extern void __stdcall {name}({arguments});")
    lines.extend(
        [
            "",
            "const manalinkeh_opaque_fn",
            "manalinkeh_veneer_table[MANALINKEH_VENEER_COUNT] = {",
        ]
    )
    for address, name, _line in entries:
        stdcall_match = STDCALL_RE.fullmatch(name)
        value = stdcall_match.group(1) if stdcall_match else name
        if not IDENTIFIER_RE.fullmatch(value):
            value = "0"
        lines.append(f"  (manalinkeh_opaque_fn){value}, /* 0x{address:08x} */")
    lines.extend(
        [
            "};",
            "",
            "manalinkeh_opaque_fn",
            "manalinkeh_resolve_veneer(uint32_t legacy_address)",
            "{",
            "  uint32_t offset;",
            "  uint32_t index;",
            "",
            "  if (legacy_address < MANALINKEH_VENEER_BASE)",
            "    return 0;",
            "  offset = legacy_address - MANALINKEH_VENEER_BASE;",
            "  if (offset % MANALINKEH_VENEER_STRIDE != 0)",
            "    return 0;",
            "  index = offset / MANALINKEH_VENEER_STRIDE;",
            "  if (index >= MANALINKEH_VENEER_COUNT)",
            "    return 0;",
            "  return manalinkeh_veneer_table[index];",
            "}",
            "",
        ]
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main():
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: generate_veneer_table.py MANALINKEH_ASM OUTPUT_C OUTPUT_H"
        )
    entries = read_veneers(Path(sys.argv[1]))
    write_source(Path(sys.argv[2]), entries)
    write_header(Path(sys.argv[3]), entries[0][0], len(entries))


if __name__ == "__main__":
    main()
