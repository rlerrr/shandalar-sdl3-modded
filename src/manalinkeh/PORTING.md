# ManalinkEh static integration status

This directory is a source-level fork of ManalinkEh for the
`modded-shandalar-manalink` target.  It does not use the old injected DLL or
absolute MOK executable addresses.

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

The target currently reaches the final link with no compile errors and 6
unique unresolved symbols (down from 205 at the start of the bridge work).
The latest diagnostic is generated with:

```text
clang.bat shandalar > temp\manalink_link_ui_aliases.txt 2>&1
```

Files in `temp/` are diagnostics and are not part of the source checkpoint.

## Remaining work

The unresolved symbols are:

1. `cards_txt` and `strs_hunting_subtypenames`, whose imported table layouts
   differ from the host's newer named accessors.
2. `change_color_and_kill_other_change_color_effects` and `check_mana_multi`,
   which still need behavioral adapters rather than linker placeholders.
3. `game_number`, whose owning host duel/challenge state still needs to be
   identified.
4. `TENTATIVE_finalize_sound_system`, pulled in by the obsolete Windows MCU
   launcher at the tail of `functions/windows.c`.

Do not satisfy these with no-op linker stubs.  Add a host alias when storage or
behavior is genuinely shared, a typed adapter when signatures differ, or
explicit Manalink-owned state when the SDL engine has no owner.

## Known runtime hazard

The imported AI save/restore code still contains raw MOK address assumptions.
The target is not runtime-ready until those snapshots are converted to named
host state or removed from the selected dependency closure.
