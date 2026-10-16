# ManalinkEh static integration status

This directory is a source-level fork of ManalinkEh for the
`modded-shandalar-manalink` target.  It does not use the old injected DLL or
absolute MOK executable addresses.

## Shared header migration

The definitions in `src/` are the canonical shared definitions. Consolidate the
imported headers incrementally, preserving numeric IDs and the layouts consumed
by existing save files and binary asset loaders. Matching names or total record
sizes do not establish matching field meanings, widths, or offsets.

- `card_id.h`, `Expansions.h`, and `stypes.h` forward to the main headers. Their
  definitions were identical before consolidation; the forwarding headers keep
  existing include paths working without maintaining duplicate declarations.
- `subtypes.h` forwards to the main header, which includes the imported subtype
  IDs and expanded creature, supertype, artifact, and planeswalker range limits.
  Existing IDs and the ordinary enum representation are preserved. The imported
  header's alternate `SHANDALAR` branch (a 16-bit C++ enum and separate constants)
  is not part of this host's type model.
- Migrate `defs.h` a type or enum at a time. Reconcile missing names and semantic
  differences explicitly. For binary records, verify member offsets, widths,
  signedness, packing, and serialization before sharing a declaration.
- The unused engine API declarations in the root `src/manalink.h` have been
  removed. Its former card-art and rendering consumers include `defs.h` and
  standard library headers directly. `src/manalinkeh/manalink.h` remains the
  imported engine API, including its static bridge adaptations.

In particular, Manalink's `STATE_SUMMON_SICK` combines uses that the host splits
between attack and activation restrictions. Migrate its callers according to
their purpose rather than introducing a blanket alias. Full haste and permission
to activate abilities as though a creature had haste must remain distinct.

## Current boundary

- The imported sources compile into `manalinkeh_static`.
- The host replaces the 73 functions rerouted by the MOK executable through
  `mok_manalink_redirects.h`.
- 61 corresponding original function bodies are excluded from the modded
  target while their declarations remain available to callers.
- Imported functions and helpers are namespaced as `manalinkeh_*`, avoiding
  duplicate definitions with the decompiled engine.
- The host bridge maps shared globals, interior fields, renamed functions,
  initialized UI strings, and a small amount of Manalink-owned bookkeeping
  state.
- A normal C trampoline replaces `functions/call_card_fn.asm`.  The ESI
  compatibility behavior was only needed by original assembly card handlers;
  the static target calls C handlers.

The target now compiles and links successfully with no unresolved symbols
(down from 205 at the start of the bridge work).
The latest diagnostic is generated with:

```text
clang.bat shandalar > temp\manalink_link_complete2.txt 2>&1
```

Files in `temp/` are diagnostics and are not part of the source checkpoint.

## Closed link boundary

The last dependencies were resolved by source-level or typed host adapters:

1. Legacy/effect titles and hunting subtype names use bounded host accessors
   instead of incompatible raw table aliases.
2. Faerie Dragon's old permanent-color calls use Manalink's source-level
   `change_color()` implementation.
3. `check_mana_multi` uses an integral capacity solver over fixed pools,
   special mana, flexible-color entries, and color-conversion mappings.  This
   prevents one flexible source from satisfying multiple colored costs.
4. The obsolete external `LaunchMCu.exe` launcher is excluded from the static
   target, removing its legacy sound-system shutdown dependency.
5. `game_number` remains explicit Manalink-owned state because the SDL
   single-process duel has no network match-number owner.

Resolve future dependencies with a host alias when storage or behavior is
genuinely shared, a typed adapter when signatures differ, or explicit
Manalink-owned state when the SDL engine has no owner.  Do not add no-op linker
stubs.

## Known runtime hazard

The imported AI save/restore code still contains raw MOK address assumptions.
The target is not runtime-ready until those snapshots are converted to named
host state or removed from the selected dependency closure.
