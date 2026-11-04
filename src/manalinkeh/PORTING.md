# ManalinkEh static integration status

This directory is a source-level fork of ManalinkEh for the
`modded-shandalar-manalink` target.  It does not use the old injected DLL or
absolute MOK executable addresses.

## Shared header migration

The definitions in `src/` are the canonical shared definitions. Preserve numeric
IDs and the layouts consumed by binary asset loaders. Existing saves are not a
compatibility constraint for this migration.
Matching names or total record sizes do not establish matching field meanings,
widths, or offsets.

- `card_id.h`, `Expansions.h`, and `stypes.h` forward to the main headers. Their
  definitions were identical before consolidation; the forwarding headers keep
  existing include paths working without maintaining duplicate declarations.
- `subtypes.h` forwards to the main header, which includes the imported subtype
  IDs and expanded creature, supertype, artifact, and planeswalker range limits.
  Existing IDs and the ordinary enum representation are preserved. The imported
  header's alternate `SHANDALAR` branch (a 16-bit C++ enum and separate constants)
  is not part of this host's type model.
- `src/defs.h` owns the shared enums, flags, helper types, macros, and binary
  record declarations. `src/manalinkeh/defs.h` is a compatibility wrapper, with
  no duplicate enum or record declarations. Runtime definitions do not change
  with `SHANDALAR` or the include path; the wrapper only adapts static-host
  poison accessors.
- Common constants use one set of values. Counters, icons, DFC symbols, and
  sounds cover the complete modded asset ranges. Targeting and player flags use
  the host's masks. Extended triggers have distinct IDs and independent enable
  bits, including a separate energy-counter enable flag. The unused duration
  status bit is separate from the active basic-land text status bit.
- Comparison helpers evaluate their arguments once. The C++ enum operators are
  shared too; `modded_compat.h` now forwards to `defs.h`.
- `iid_t` and `csvid_t` each have one shared definition: distinct wrappers around
  an `int` in C++, and integer typedefs in C. The C++ wrappers retain the host's
  implicit `int` constructors and map the 16-bit `0xffff` sentinel to `-1`.
- `card_ptr_t` is shared directly. A union exposes both the original aggregate
  subtype word and the imported split fields; AI values use individual bytes.
  The seven-entry `types` array occupies the original two subtype words and ten
  ability bytes; the legacy deck reader handles that old byte interpretation
  locally. Casting costs use the shared `req` member. ID and picture count use
  the host field types.
- `stack_data_t` is shared directly, with an unsigned event byte matching the
  host's byte extraction and imported event values. Its four-byte size, field
  offsets, and host alignment are unchanged; imported arrays use that same
  alignment. `defs_abi_test` checks the saved stack dwords in both directions.
- `card_data_t` is shared directly, retaining its 72-byte layout and host
  alignment. IDs use the complete host field; the imported high-half clearing
  write is unnecessary. The signed engine rating is `ai_base_value` at 0x44,
  separate from the reserved `unused0x43` byte and the Cards.dat AI score.
  Mana costs retain the host's signed X sentinel (-1); imported calculations
  that require unsigned bytes cast explicitly. `defs_abi_test` checks those
  record bytes and full-width ID writes.
- `card_instance_t` is shared directly, retaining the host's 300-byte layout
  and alignment. Counter unions expose the host words (`special_counters` and
  `counters`) and imported bytes (`special_counters_byte`, `counters_byte`, and
  `counters2` through `counters5`). `blocking` and `damage_on_card` are unsigned;
  blocking indices use 255 for no assignment. The display accessor translates
  that sentinel to -1 for its existing callers. The instance template uses designated
  initializers so these views cannot shift positional values. `defs_abi_test`
  checks counter-byte isolation, unsigned damage, and high blocking indices.
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

## October 2022 migration

The recovered October 2022 source is integrated into the static target. MOK
annotations resolve EXE references to source functions and shared storage. AI
snapshots save named host and extension state; no contiguous EXE memory ranges
are copied. Card extensions and display extensions have independent banks, and
live card/activation copies preserve the destination's extension pointer.

Haste has one canonical keyword bit, `SP_KEYWORD_HASTE`. Attack and tap-cost
permissions are derived separately so effects granting only one permission work.
The host's underlying continuous-control flag lives in the extension: its old
state bit is `STATE_STIFLED` in the recovered engine and cannot be shared.

The host's control-change duration uses a separate flag above the native pump
flags, and stores its original target controller outside the new subtype/color
slots. Suspend grants the same haste keyword through that duration mechanism.
Event range and action-sentinel comparisons explicitly use unsigned arithmetic
so native MSVC-compatible enum behavior matches WebAssembly.

### Validation, November 4, 2026

- Native x86 and WebAssembly application/test builds link successfully.
- The same 139 focused engine/card cases pass on both targets, including all
  summoning-sickness scenarios, suspend control loss, AI snapshots, handler
  overrides, and the repaired Skyclave Basilica binding.
- All 177 behavior cases whose former dummy handlers are recovered pass natively.
  Their obsolete unimplemented expectations were removed.
- Behavior shard 0 of 100 (849 cases) reports 843 passes, five known failures,
  one unimplemented card (Pirate's Prize), and no unexpected failures. Seven
  obsolete known-failure entries were removed after verified passes.
- The 871 recipe-generator tests and nine card-data importer tests pass.
- The compiled-source pointer audit preprocesses 169 imported translation units
  and finds no absolute executable-address pointer casts, including indexed
  arithmetic references. Excluded historical source aggregates are not runtime
  code and are outside this audit.
- Native ABI, extension-copy/snapshot, dynamic-dialog-template, and GDI checks
  pass. The separate `winuser_compat_test` currently fails its window-ordering
  assertion at `GetWindow(parent, GW_CHILD) == child`; that remains unresolved.

These are sampled rules checks, not a complete rules certification. The full
84,820-case behavior suite and 24,458-case catalog suite have not been completed.
New high-IID cards are available to duels, but campaign eligibility remains
limited by its existing 14-bit card-ID field. The storm reimplementation remains
preserved on `codex/storm-reimplementation-2020`, outside this integration branch.

See README.md for the source revision and card-data reconstruction command.
