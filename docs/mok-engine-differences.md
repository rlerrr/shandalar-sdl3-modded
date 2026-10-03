# MOK engine comparison, 2026-10-03

This pass compares the surviving engine against MOK's `C:/dev/shandalar/Magic.exe`
(SHA-256 `71609906df4d3e5f4aa004034b21cfa362fd2c5522d23c57c91d96d6ca7d7025`).
Card implementations and card-specific helpers are excluded. The recompilation
and PDB come from `C:/dev/shandalar-decomp/out/build/nmake-matching/src/magic`.

Use the local fork explicitly from the decomp repository:

```powershell
$env:PYTHONPATH = 'C:\dev\reccmp'
$env:PYTHONUTF8 = '1'
python -c "import reccmp; print(reccmp.__file__)"
python -m reccmp.tools.asmcmp --target MOK --offset-addresses --no-color --verbose 0x00436740
```

The flag is `--offset-addresses`. Assembly logs and full JSON reports from this
pass are in the SDL repository's ignored `temp/` directory. In particular,
`mok-report-annotated.json` includes the updated CSV annotations.

## Adopted changes

| Engine function / MOK location | Original executable behavior | SDL change |
| --- | --- | --- |
| `run_duel_turn`, `0x439966` | Calls veneer `0x200b240`, `untap_phase`. | Enter the Manalink wrapper, which handles begin-turn events, phasing, skipped untap steps, and untap notifications. |
| `untap_phase_exe`, `0x43a70c` and `0x43ac27` | Jumps over its old entry checks and final untap/reset/refresh/mana-burn work. | Keep the original preparation and choice loop; let the wrapper own the final work. Both changes are necessary together. |
| `resolve_top_card_on_stack`, `0x436854` | Calls veneer `0x200b76d`, `untap_card`, on the proxy's parent. | Use the hook so effects can observe other cards untapping; retain the parent upkeep-flag reset. |
| `add_card_to_hand`, `0x479656` | Calls veneer `0x200b272`, `create_card_instance`. | Route initialization through Manalink's full instance template, extended identity/color fields, and mana-source handling. |
| `is_combat_damage_ability_in_step`, `0x4b39e3` | Returns true for `0x40000000` (double strike) before testing first strike. | Double strike participates in both damage steps, including when the first-strike bit is also set. |
| `resolve_combat_damage`, `0x4b2554` and `0x4b2667` | Calls veneer `0x200284d`, `get_attack_power`, for attackers and blockers. | Honor effects that replace power with toughness when assigning combat damage. |
| `resolve_combat_damage`, `0x4b2649` | Tests damage-step eligibility without excluding tapped blockers. | Tapping a blocker after declaration no longer suppresses its combat damage. |
| `cleanup_combat_state`, `0x4b3a52` | If still blocking, sets `0x4000` before clearing `0x8`. | Preserve `STATE_BLOCKED` for later effects, without marking creatures that have already stopped blocking. |
| `process_killed_card`, `0x477b57` | `STATUS_CANNOT_LEAVE_PLAY` jumps past both death and leave-play notifications, then reaps the instance. | Persistent effects that detach when their object leaves play bypass those notifications and zone moves. |
| `move_card_to_exile`, `0x477f67` and `0x477f9e` | Reads the original identity from the supplied controller/card instance; skips adding tokens to exile. | Use the MOK three-argument call shape (owner, card, controller), preserving the original identity after type changes and the correct owner after control changes. |

Manalink is permanent in this fork, so these changes are unconditional and the
old vanilla alternatives have been removed. They are calls and behavior inside
engine functions, beyond the five-byte JMP reroutes already represented by
`mok_manalink_redirects.h`.

## Annotation improvements in the decomp repository

Ten function entries were added to `Ghidra/mok.csv`, with byte sizes verified
against the executable:

| Address | Name | Size |
| --- | --- | ---: |
| `0x401a80` | `get_card_instance` (register ABI; no recomp counterpart) | 38 |
| `0x438980` | `displayed_card_indices_invalid` | 29 |
| `0x4389a0` | `get_displayed_card_special_counters` | 45 |
| `0x438e80` | `get_displayed_card_counter_power_toughness` | 62 |
| `0x439130` | `get_displayed_card_base_internal_id` | 48 |
| `0x439160` | `get_displayed_card_parent` | 65 |
| `0x4391b0` | `get_displayed_card_original_internal_id` (tail jump to `0x439130`) | 5 |
| `0x45a040` | `init_sound_dll` | 286 |
| `0x47d8e0` | `update_hand_window_title` | 86 |
| `0x4961f0` | `run_duel_timer_tick` | 61 |

The existing `0x438ad0` row was corrected from
`displayed_card_indices_invalid` to `get_displayed_card_internal_id`. Its code
loads the instance's `internal_card_id` at offset `0x6c`; the actual validator
at `0x438980` rejects player indices outside 0–1 and card indices outside 0–149.
Rerunning the fork's MOK report successfully matched the new entries that have
recompiled counterparts.

## Validation and remaining leads

`mok_engine_test` compiles selected production function definitions, with the
Manalink and UI boundaries mocked. It checks first strike versus double strike,
blocker history, card creation, ownership and original identity on exile, token
exile, persistent-effect death notifications, untap proxy resolution, and the
division of work between legacy untap preparation and the Manalink wrapper.
The source-selection step uses the existing FUNCTION annotations and fails on
missing or ambiguous definitions; tests do not contain copies of engine logic.

The native Clang x86 and WebAssembly `shandalar` builds both succeeded. The
native CTest run passed five of six tests, including `mok_engine_test`.
`winuser_compat_test` failed at line 231 on `GetWindow(parent, GW_CHILD) == child`
in unchanged platform code; the run also reported missing font files. That
failure was not investigated as part of this engine comparison.

These tests exercise the surviving engine with mocked hooks. They do not verify
the full combat assignment path, the Manalink implementations themselves, or a
live duel. In particular, the tapped-blocker and attack-power changes have
binary evidence and build validation, but no dedicated runtime coverage yet.

The second pass below follows the previously identified UI and save hooks.

Low reccmp similarity alone is not evidence of a gameplay change. MOK has many
register-ABI assembly rewrites, dead-code bypasses, and data-access optimizations;
inspect reachable control flow, constants, and call targets before porting.

## Second pass: summaries and UI integration

| MOK location | Finding and resulting change |
| --- | --- |
| `0x435e4d` calls `0x4024f0` | The register-argument helper tests all seven color bits and accumulates power/toughness for each matching bit. Battlefield summaries now include multicolored creatures in each matching color, while combined totals count each creature once. |
| `0x464a3f` | Call `initialize_ability_tooltip_names` while `ABILITYWORDS` is loaded. This initializes the expanded ability and counter names and maps the original 17 ability slots. The static bridge's mapped table is copied to the SDL engine's legacy string table. |
| `0x494d3d` | Call `set_smallcard_size` instead of hard-coding window width divided by eight, honoring the configured absolute or proportional card size. |
| `0x4397fd`, `0x4d1da7`, `0x4d24a4` | Use `legacy_name` for dialog titles, full-card titles, and legacy rules text. Numbered alternatives and callback-generated names/text now reach the surviving engine UI. The static host identifies its relocated rules-text buffer by pointer identity, replacing the original literal-address test. |
| `0x4d216e` | Use `effect_card_text_replace_pipe_n` for legacy power/toughness placeholders, including repeated occurrences and signed zero/negative modifiers. |
| `0x4a0841` | Call `after_load_game` after a mode-3 load attempt, preserving its result and reapplying rules-engine settings as the binary does. |

These are engine and shared display changes; no individual card handlers were
modified. The legacy text helper received only the host adaptation needed to
recognize the relocated rules-text buffer.

Calling conventions matter when reading this binary. For example, `is_in_play`
leaves its predicate in flags and the instance in ESI; callers test ZF directly.
The color-summary helper receives player in EBX, color bits in AL, power in ECX,
and toughness in EDX. A normal C comparison can therefore have almost no
assembly similarity while implementing the same logical operation.

There is also a suspicious stride in that color helper: its destinations use a
128-byte player stride, although its caller clears the original 32-byte rows.
This appears to be a MOK pointer-arithmetic bug. The SDL change uses its typed
arrays, with tests for independent totals for both players.

Seven more functions were annotated, with boundaries verified against the
executable and the four large UI functions matched through the local fork:

| Address | Name | Size |
| --- | --- | ---: |
| `0x4024f0` | `accumulate_creature_power_and_toughness_by_color` (register ABI) | 51 |
| `0x438e50` | `get_displayed_card_eot_toughness` | 45 |
| `0x4390d0` | `get_displayed_card_display_flags` | 45 |
| `0x495dc0` | `apply_duel_backdrop_art` | 1066 |
| `0x4a9eb0` | `populate_pick_card_list` | 480 |
| `0x4d1950` | `draw_displayed_card_overlaid_full_card` | 618 |
| `0x4d1bc0` | `draw_special_effect_full_card` | 2567 |

Two annotations were corrected:

- `0x468750`: `destroy_MAGICGAME_LibraryClass` became
  `invalidate_player_territory_if_display_enabled`. It takes player in EAX,
  checks AI/display state, and calls `InvalidateRect` on the player's territory.
- `0x439250`: `get_displayed_poison_total` became
  `get_displayed_player_counters`. Its returned dword includes poison in byte 0,
  experience in byte 1, and energy in byte 2 in MOK.

Second-pass assembly and validation logs are in `temp/mok2-*`. Native regression
coverage was extended for multicolor summaries, phased-out exclusion, and the
ability-initialization boundary. `mok_legacy_text_test` compiles the actual
imported text functions with a mock displayed instance and buffer classifier;
it exercises name/text event selection and buffer limits, numbered alternatives,
repeated numeric substitutions, and signed zero modifiers. UI rendering and a
live loaded duel still need runtime verification.

The final native and WebAssembly game builds succeeded. All three focused
native tests passed: `mok_engine_test`, `mok_legacy_text_test`, and
`global_strings_test`. The unrelated platform-test failure from the first pass
was not rerun or changed in this pass.

## Remaining engine work identified in the second pass

The save supplement at `0x49f4ed` and `0x4a02af` serializes graveyard source IDs,
turn count, color prompting, mulligan state, challenge state, and counters added.
The SDL engine does not yet call it. Inserting it into the existing file layout
would move the following mode-specific data and break previously written branch
saves, so this needs an explicit format version/migration. MOK's load repair
also preserves dummy/multiblocker callbacks at `0x4a0273`–`0x4a0281`; the static
host's saved raw function pointers need a relocation-aware equivalent.

The packed-counter gap identified here is addressed in the third pass below.
The supplemental save format and callback relocation remain outstanding.

## Third pass: packed player counters and complete cache boundaries

MOK uses the original two four-byte poison slots at `0x4ef530` for
`PlayerCounters`: poison, experience, energy, and a reserved byte, in that order.
The imported counter functions already use this layout, but the static host had
allocated a separate array for energy/experience while poison stayed in the
native engine's integer array. This caused the two newer counter types to escape
reset, native AI rollback, save/load, and display refresh.

The engine now owns one typed `player_counters[2]` array shared with Manalink.
Both save formats and both native AI snapshot paths copy all eight bytes. The
new-duel reset clears each complete entry. Victory checks read only poison, so
energy and experience cannot be mistaken for lethal poison. Three existing
poison writes in legacy card files were mechanically redirected to the poison
byte to keep the storage migration compiling; no card behavior was investigated
or otherwise changed.

The display cache copies the packed values under its existing render lock.
`get_displayed_player_counters` exposes those cached values. Life-panel
registration now installs `wndproc_LifeClass_hook`, as at MOK `0x49711a`, and
its paint handler calls `paint_player_counters`, as at `0x497a17`. This connects
the imported mixed-counter icon layout and the poison/energy/experience hover
text. The native handler also routes direct hover calls through that hook.

The eight-byte save slot stays at its existing position and size. Ordinary
poison-only branch saves (poison values 0�255) have zero upper bytes, which load
as zero experience and energy. This change does not add the separate MOK save
supplement or solve saved callback relocation.

The cache annotation was also severely truncated: `0x40b920` had size 18,
covering only its prologue and a detour into a patch at `0x403093`. That patch
returns to `0x40b932`; the function's body continues through its return at
`0x40bca2`. Its correct contiguous size is 899. The initial comparison/copy covers
the range starting at `0x4ef4e0`, including the counters, into the cache starting
at `0x543bb0`; the corresponding packed slots are `0x543c00` and `0x543c04`.
This is another reason to check boundaries before treating short diffs as simple
reroutes.

Third-pass annotation changes in `Ghidra/mok.csv`:

- Correct `refresh_duel_display_cache` size from 18 to 899.
- Rename the `0x4ef530` global from `g_poison_counters` to `player_counters`,
  with explicit eight-byte size.
- Add the two four-byte cache globals at `0x543c00` and `0x543c04`, retaining
  their decompiled symbol names for matching.
- Add four missing engine shell functions, with complete boundaries:

| Address | Name | Size | reccmp similarity |
| --- | --- | ---: | ---: |
| `0x4cfa30` | `shell_set_map_mode` | 118 | 84.91% |
| `0x4cfab0` | `shell_logical_rect_to_device` | 121 | 84.62% |
| `0x4cfb30` | `shell_invalidate_logical_rect` | 66 | 93.75% |
| `0x4cfb80` | `shell_enable_animation` | 77 | 70.97% |

The shell helpers show equivalent control flow and API calls through MOK's
import thunks. No behavior port was needed for these helpers. All comparisons
used the local reccmp fork with `--offset-addresses`, sequentially. The corrected
cache comparison now includes its actual epilogue and reports 18.90% similarity;
its optimized bulk comparison differs substantially from the decompiled C.
Assembly logs are in `temp/mok3-*`.

The engine harness now compiles the actual reset, AI backup/restore, save-format,
cache-refresh, and displayed-counter functions, plus their production snapshot
and cache declarations. It checks independent packed counters for both players,
rollback after speculation, invalid player access, refresh after energy-only and
experience-only changes, both save/load slots, old poison-only slots, and reset
between duels. File I/O and unrelated engine services are mocked, so these checks
do not establish full save-file or live UI compatibility.

The final native game and WebAssembly game builds succeeded. All three focused
native tests passed with the freshly linked harness, including a stable-cache
baseline before the energy-only and experience-only invalidation checks:
`mok_engine_test`, `mok_legacy_text_test`, and `global_strings_test`. Both
repositories passed `git diff --check`; the annotation CSV has no duplicate
addresses. Logs: `temp/mok3-native-final.log`,
`temp/mok3-native-final-2.log`, and `temp/mok3-wasm-final.log`.

## Startup regression: default Manalink text source

The ability initialization hook exposed an incorrect host adaptation of
`load_text(NULL, ...)`: it selected the legacy `UIStrings.txt`, whose ability
section has 17 entries, rather than the extended `Text.res` section with 75.
The initializer copied stale rows beyond entry 17 and hit its name-length
assertion in the WebAssembly startup reported by the user. The earlier loader
test mocked that initializer, so it did not exercise this resource mismatch.

Default Manalink reads now prefer `Text.res` and fall back to the legacy UI
file for missing sections. Explicit filenames keep their original behavior.
Startup requests the default ability section before invoking the imported
initializer, while a 17-entry legacy section is copied in its original order.
The shared buffer and all host declarations now hold 300 rows; the loader
validates against that capacity rather than rejecting counts above 225. This
also admits the installed 254-entry creature-type section. Empty, shorter,
or failed loads discard stale rows, and newline stripping preserves the last
character of an unterminated final line.

`global_strings_test` now compiles the actual ability, counter, and subtype
initializers from the imported source. Its fixtures cover legacy and extended
ability ordering, Text.res precedence, explicit filenames, fallback sections,
the 254-entry subtype section, full-buffer capacity, oversized counts, and
stale-row clearing. Its optional asset-directory argument exercises the real
installed assets; running it against `C:/dev/assets_modded` successfully loaded
all 75 abilities, 142 counters, and 254 creature rows. The native and WebAssembly game builds succeeded, and all three focused
tests passed. Logs: `temp/mok-ui-text-native-3.log` and
`temp/mok-ui-text-wasm.log`. The installed-asset initializer run also exited
successfully; a live browser startup was not verified in this pass.

## Small-card title context

The engine drew the title twice: once inside the modded DrawSmallCard, using
its selected frame configuration, then through the short DrawSmallCardTitle
host API. That wrapper discarded the card context, so the renderer selected
CFG_BASE for the second pass. The installed classic frame configuration uses
TitleLeft=8 and TitleWidth=776, while the base configuration uses
TitleLeft=32 and TitleWidth=736. The second pass could shift the lettering
and choose a different fitted font, producing the reported double impression.

The normal, activation, and effect rendering paths now pass the same local card
data to a context-aware title wrapper, preserving frame selection and display
color. Highlight changes invalidate the card for a complete repaint rather than
drawing another title through the short API. Card-back titles retain the short
API because there is no card-specific frame or earlier title pass.

## Campaign startup: imported eligibility and packed IDs

The name-entry hang reproduced in a headless run of the full native game.
The stack was inside the starter-land loop in AddRandomStarterDeckCards.
All five basic lands had expansion=0, so the campaign's expansion & 0xc1
condition rejected every candidate forever. These fields were already zero
in ct_all.csv; they are unused in Manalink, rather than cleared at runtime.

The importer now merges campaign expansion masks and the campaign-only extra
flags (0xf80) from the original Shandalar table at 0x594208 by CSV ID. The
reviewed mapping lives in tools/carddata_campaign_metadata.json. Engine order,
card handlers, and dynamic effect records are retained; 655 playable imported
records receive their original campaign metadata. This also restores dungeon
reward and pricing flags that were absent from the imported table.

Many original campaign cards now have internal IDs above 4095 (Mesa Pegasus
is 6072). Adventure deck extraction, sorting, display, ante, removal, rebuilding,
and CSV-ID conversion now use the 14-bit ADVENTURE_DECK_CARD_ID_MASK. The market
and selection flags at bits 14 and 15, plus the new-card flag at bit 16, retain
their existing values. Copy counting ignores selection/new-card flags and
continues excluding market cards. The importer rejects a campaign card that
cannot fit this field, and insertion rejects IDs overlapping the flags. Slot
widths and the save format are unchanged; this does not reconstruct IDs already
truncated by earlier builds.

campaign_new_game_test compiles the actual imported records, production starter
selection, deck insertion/rebuilding, rarity loading, and CSV-ID conversion.
Only card callbacks and journal output are omitted. It covers all four
difficulties and five colors, expected deck sizes, basic lands, large IDs,
identity/flag preservation through rebuilding, and flagged copy limits. Its
native and WebAssembly-under-Node runs passed using the installed campaign
assets. The full native headless diagnostic passed shared startup, starter
generation, world-map generation, dungeon setup, and the world-entry checkpoint,
stopping before autosave. Both game builds and the four focused native tests
passed. A live browser world entry was not checked. Logs are in
 temp/campaign-newgame-* and temp/newgame-probe-fixed-run.log.
