# Duel test organization

Authored behavior tests have one home per card. Each card file contains all of
its positive and negative cases. Shared boards remain in `data/fixtures.json`,
and tests of engine behavior involving multiple cards live under
`data/cases/scenarios/`.

```text
data/
  cases/
    cards/
      027/
        02723-infantry-veteran.json
      070/
        07059-furnace-whelp.json
    scenarios/
      snapshots.json
      continuous-effects.json
  fixtures.json
  oracle.json
  oracle_aliases.json
  known_failures.json
  unimplemented.json
  legacy_exceptions.json
```

Card folders currently group 100 CSVIDs each: `csvid // 100`, padded to three
digits. Filenames start with the five-digit CSVID followed by a readable name.
Only cards with authored cases need files; catalog smoke and wording templates
are generated separately. These folders keep both file lists and agent edit
assignments small.

Oracle-backed X-spell recipes use `choose_x` to supply the requested amount
through actual casting and mana payment. Zero retains the printed base cost;
exact colored receipts check restricted X payments. Ordinary gameplay and AI
policy retain their usual input paths. Both controllers exercise pending-spell
and resolved-state snapshot replay, with all queued inputs checked for use.

For common Oracle wording, use [recipe-based authoring](RECIPES.md). A compact
declaration generates reviewed engine scenarios, and batch tools inventory
unsupported wording and prepare failure-only review reports. Generated recipes
are behavior cases; the existing card-ID templates remain separate.

A card file records the behaviors its cases actually assert; its existence does
not imply complete rules coverage. For example, the current Overrun cases check
the team bonus and cleanup, while trample combat still needs a separate case.
The Lignify cases check base stats and ability removal; creature subtype remains
outside the current inspection interface.

Board-wide stat recipes now cover mixed opposing boards, signed stats, lethal
toughness reductions, stacking, later arrivals, cleanup, owned populated zones,
and snapshot replay. Declared untapping, draw, scry, life-per-death, and exile
components are checked too. Keyword grants and other additional clauses remain
explicit coverage gaps where the card's authored cases do not exercise them.

Temporary keyword recipes now exercise flying/reach blocking, trample damage,
first/double strike, lifelink, deathtouch, indestructibility, hexproof/shroud, and
haste through real casting, responses, and combat. Fresh creatures are legally
cast before the haste check. Cleanup controls exercise expiration. Declared
must-block effects use their blocker-choice triggers. These cover the reviewed
components of a card, with remaining clauses documented in its card file.

Creature Aura recipes check persistent bonuses, removal, bounce, loss of their
host, entry draw/life effects, and returning-Aura triggers, alongside combat
abilities. Ownership of the Aura and its enchanted creature is checked
separately. Cleanup must preserve the continuous effect.

Signed-stat and restriction Auras also cover lethal toughness reduction,
negative power without negative combat damage, normal attack/block restrictions,
and disabled activated abilities. Removal restores those actions; unaffected
creatures provide controls. Mana-ability exceptions use separate witnesses,
with larger bodies when a toughness penalty would kill a 1/1 witness.

Self-pumping creature recipes combine populated-zone casting/combat checks with
real activation costs, pre-resolution stack observations, repeated and queued
bonuses, cleanup, tapped/fresh hosts, source loss, and per-turn activation caps.
Explicit menu decisions select the printed ability; other modes remain separate
coverage gaps. Snapshot replay includes a resolved dynamic effect.

Evasion creature recipes use real combat declarations to check fear, intimidate,
shadow, horsemanship, basic landwalk, and unconditional unblockability. Blocker
colors, artifact status, flying/reach, and both directions of shadow are tested
separately. Landwalk uses the defending player's battlefield, including tapped
lands and land removal before blockers. Other activated modes remain gaps.

Original sets can become a browsing/filter index once we have a reviewed
first-printing mapping. The pinned Oracle corpus currently lacks that mapping;
the set in a Scryfall card URL may describe a reprint. A card's name, CSVID, and
Oracle ID determine its identity. Moving its file into a set folder later does
not change its test names or execution order. Do not duplicate tests for reprints.

Reviewed exceptions to registry identity categories live in
`data/registry_kinds.json`. CSVID 910's Assembly-Worker is the animated Mishra's
Factory land form, which shares a name with a separately printed creature.
Alternate forms, tokens and internal effects require authored scenarios with
the appropriate parent-card Oracle reference; ordinary card recipes reject
those records. The generated inventory retains the classification and rationale.

## Card files

Use a versioned document with a full registry name, CSVID, and cases:

```json
{
  "version": 1,
  "card": "Infantry Veteran",
  "csvid": 2723,
  "cases": [
    {
      "name": "InfantryVeteran_Tapped",
      "fixture": "combat",
      "setup": [
        {
          "name": "Infantry Veteran",
          "ref": "source",
          "player": 0,
          "zone": "battlefield",
          "state": ["tapped"]
        }
      ],
      "steps": [
        {"op": "can_activate", "ref": "source", "value": 0}
      ],
      "seed": 123
    }
  ]
}
```

The file-level card applies to every case. Scenario files omit that header and
specify `card` in each case to identify the Oracle reference. Case names are
globally unique and remain the GoogleTest/CTest identities. Duplicate cases,
duplicate card homes, mismatched names/CSVIDs, and empty case files fail generation.
The loader discovers JSON recursively and sorts cases by name. CMake regenerates
the test inventory when files are added, removed, or edited. Runner results include
each case's source file.

The generated manifest and C++ tables may aggregate the catalog in a build
directory. They are not authoring files. The pinned `oracle.json` is also a generated
reference corpus, refreshed through `tools/import_duel_oracle.py`; agents should
read the relevant entry rather than edit it to satisfy an assertion.

## Independent authoring

Assign an agent specific card files or CSVID ranges. It should:

1. Read the pinned Oracle reference and document any applicable legacy exception.
2. Reuse a shared fixture, adding small case-local overrides and scripted choices.
3. Cover success, illegal actions/targets, relevant empty zones, and effect expiry.
4. Preserve correct assertions when they expose an engine bug.
5. Report missing harness capabilities separately from its case-file changes.

A positive case must assert the effect itself: it should fail if the handler
does nothing. Check printed base stats, colors, land subtypes, source and target
destinations, and initial zone counts against the packet before adding derived
assertions. Include positive controls in restriction/removal cases. Parse selected
packet fields when a full tool output would truncate the Oracle records.

Case names use letters, digits, and underscores, beginning with a letter. Give
variants different actions or board contents; repeating a case with fewer
assertions adds no behavior coverage. A targeted-removal success should include
another eligible permanent that survives, as well as an ineligible witness.

Coordinate changes to shared fixtures, generator code, and failure baselines through
the integrating agent. Validate new cases with the generator, then run their isolated
native and Node/Wasm tests before merging the case files.

During parallel test authoring, agents edit only their assigned card case files.
New recipe families can be assigned separately, with exclusive ownership of
their module, witness data and guards; the integrator owns dispatcher wiring.
They must not change engine/card implementations, shared fixtures, harness code,
Oracle data, or the known-failure manifest to make tests pass. Preserve expectations
derived from Oracle text and report failures with the case name, expected behavior,
actual result, and any suspected harness limitation. The integrating agent reviews
and classifies failures; production fixes are handled separately. Only the integrating
agent rebuilds shared native/Wasm binaries, so concurrent authors cannot invalidate
one another's test runs.

The integrator can build that reusable engine with
`SHANDALAR_DUEL_AUTHORING_BUILD=ON` in each configured native/Wasm build directory:

```powershell
cmake -S . -B out/build/shandalar-clang-x86 -DSHANDALAR_BUILD_TESTS=ON -DSHANDALAR_BUILD_DUEL_TESTS=ON -DSHANDALAR_DUEL_AUTHORING_BUILD=ON -DSHANDALAR_DUEL_EXTENDED_TESTS=OFF
cmake --build out/build/shandalar-clang-x86 --target duel_card_tests -j6
```

Authoring configuration generates only compiled fixtures, templates, smoke tables
and handler bindings. It does not expand behavior recipes or emit their archive,
manifest, inventory or CTest entries. Recipe/card/witness edits therefore do not
reconfigure these shared engines. Fixture, Oracle, registry, generator and runtime
changes still require the integrator to regenerate/rebuild as appropriate.
Supply a worker archive explicitly with the runner's `--case-data`; an authoring
build does not create its default archive. `duel_tests_batched` fails with guidance
in this mode, and `SHANDALAR_DUEL_EXTENDED_TESTS=ON` is incompatible. Default
`SHANDALAR_DUEL_AUTHORING_BUILD=OFF` retains full generation and catalog targets;
use that mode for the final full-catalog integration check.

The equivalent generator-only operation is:

```powershell
python src/magic/tests/duel/generate_cases.py --driver-only --output H:/shandalar-test-scratch/engine/driver
```

It writes `duel_cases.inc`, `duel_handler_bindings.inc` and
`duel_case_sources.cmake`, preserving unchanged timestamps and any existing
behavior archive/catalog files. `--driver-only` cannot be combined with source
selectors or legacy behavior fingerprints. Generate a worker subset separately.

For an authoring pass, select complete source files before expanding recipes:

```powershell
python src/magic/tests/duel/generate_cases.py --output H:/shandalar-test-scratch/worker-a/generated --csvid 2723
python src/magic/tests/duel/run_cases.py --binary H:/shandalar-test-scratch/engine/duel_card_tests.exe --case-data H:/shandalar-test-scratch/worker-a/generated/duel_cases.bin --manifest H:/shandalar-test-scratch/worker-a/generated/manifest.json --baseline src/magic/tests/duel/data/known_failures.json --output H:/shandalar-test-scratch/worker-a/native --batch-size 128 --jobs 1 --timeout 45 --quiet
```

Repeat `--csvid` to select several card homes, or use repeatable `--case-file`
paths relative to `data/cases` (the manifest's `cases/...` spelling also works).
Selectors form a union, and each must match. A subset generates only the case
archive, manifest and inventory; it does not replace the shared build's includes
or CTest catalog. Every case in a selected source file is included.

Each author uses a separate output directory and the same immutable engine.
For Node/Wasm, replace `--binary` with the shared `.js` and add `--node` with the
Node executable. The runner mounts the selected archive's host directory through
NodeFS and keeps the binary directory as the asset working directory. Batches,
restarts and isolated failure checks all use the same archive.
Node's user-data directory uses process-local memory rather than IndexedDB.
The headless target includes Asyncify and IDBFS support required by the shared
file-I/O layer; browser persistence is outside these duel tests.

Subset manifests explicitly identify their selected sources. The runner checks
expectation signatures and selected identities while allowing unrelated baseline
entries. This is a local authoring check: regenerate and run the complete manifest
at integration to detect stale expectations elsewhere in the catalog. Coordinate
process counts across authors; `--jobs` applies to each runner, not to the entire
VM. Prefer one process per author, with shared builds owned by the integrator.

For explicit responses outside a nested casting callback, `cast_response` opts
into the pending top spell's real response context. It requires a pending spell,
not an activation; ordinary casting, legal targeting and mana payment still run.
`resolve_response` requires a response above a pending parent spell and drains
the stack with that parent's response context restored. It does not resolve only
one object. It is currently validated only for one response above its parent
spell; do not use it for deeper response chains. Keep ordinary `cast`/`resolve`
for scenarios that do not need this context. Check stack size, queued choices
and printed costs on either side of
the response. While a spell is pending, the legacy hand-count receipt still
includes its physical slot, although the source's `in_hand` property is false.
Retain that slot in count receipts until resolution; do not assert that a spell
on the stack is in hand.

Cast-history-sensitive explicit responses use `cast`, then `announce_spell`
on the pending parent, then `cast_response` and `resolve_response`.
`announce_spell` runs the engine's actual storm/replicate and cast-trigger stage,
without paying costs again or resolving the spell. Its driver marker is restored
by test snapshots; later resolution executes the remaining engine phase without
announcing the parent twice. Ordinary casts retain their existing path.
The operation requires the top pending spell outside a casting callback and
rejects duplicate announcements. Current response recipes cover a single parent
without prior storm copies. An explicit response before this boundary sees an
unfinished cast history and can misorder a later storm check.

Same-turn histories spanning both players keep the turn unchanged. Use an
instant or a legally cast flash spell for the nonactive player's precursor;
changing the fixture's turn field is not a substitute for legal timing.

History-sensitive cases use `reset_death_history` as their first step, before
any authored destruction or other death. This explicit, empty-stack operation
performs the normal turn-start Deadbox reset omitted by fixture preparation.
It removes reserved-token initialization metadata that could appear as a phantom
death. Never reset after the deaths being tested, during refresh, or during a
snapshot replay that must retain those deaths. Create real battlefield deaths
after the reset; preloaded graveyard cards are not this-turn death history.

The first parallel pilot and complete native/Wasm verification are recorded in
[the pilot audit](../../../../docs/test-audits/parallel-oracle-pilot-2026-11.json).

From the repository root:

```sh
python src/magic/tests/duel/test_case_files.py
python src/magic/tests/duel/generate_cases.py --output out/duel-tests/generated
```

The loader regression tests are also registered as `duel.case_files` in CTest.

Known assertion failures specify an exact `assertion_count` as well as matching
diagnostic patterns. An optional `pattern_counts` list pins the exact number of
matches for each pattern. Use it when repeated token or replay assertions share
the same diagnostic and step, so an unchanged occurrence cannot hide a changed
failure elsewhere. Older entries without counts retain their matching behavior.
Additional or missing failures do not match that baseline;
an unexpected pass also fails the gate. Harness errors cannot be baselined.

`known_failures.json` contains defects in implemented behavior. Missing handlers
live separately in `unimplemented.json`, currently 342 cases across 60 cards.
These are reviewed missing implementations, not every `card_dummy` record:
vanilla creatures can correctly use that handler. The runner discovers this
companion file beside `--baseline`, or accepts an explicit `--unimplemented` path.

Missing cases still execute their unchanged Oracle assertions. Matching results
report `unimplemented` separately from `known_failure` and passing coverage, and
stay out of failure-only recipe review reports. Additional assertions, different
failures, crashes, unscripted choices, and unexpected passes still fail the gate.
Generated manifests include handler identities; a changed handler or card identity
requires reviewing and retiring/reclassifying its missing-implementation entries,
even if its assertions still fail. Removed case names and entries listed in both
files are rejected. When implementing a card, remove its missing entries and run
all its cases; any remaining real defects belong in the known-failure file after
review. Tests and recipes remain in their original card files.


## Recovered handler bindings

The `binding` tier checks that all 46 explicitly recovered records point to their
reviewed, compiled handlers:

```powershell
ctest --test-dir out/build/shandalar-clang-x86 -L binding --output-on-failure
```

Use the Wasm build directory for Node checks, or select `--tier binding` in
`run_cases.py`. These wiring checks are not rules coverage. `Behavior.RestoredBinding_*` cases check focused Oracle components;
each card file records their limits in `binding_recovery_coverage`.

## Current action interface

Known-failure records may specify `platform: "native"` or `platform: "wasm"`
when a reviewed failure occurs on only that runtime. Unscoped records apply to
both. The other runtime must pass normally; an unexpected pass on the affected
runtime still fails the gate. Invalid platform labels are rejected.

When the same case exposes different reviewed assertion failures on the two
runtimes, `platform_signatures` can contain an exact `native` or `wasm`
signature. Each override supplies its own assertion count, reason, and patterns
(plus optional pattern occurrence counts); the top-level signature is the
default. Overrides cannot change card identities, introduce harness failures,
or combine with a single-runtime `platform` restriction. Unexpected passes and
unreviewed diagnostic changes still fail. Passing observations must agree;
reviewed differences in known assertions are reported separately.

Oracle supplies the rules expectations. These details describe how scripts drive
the harness, so test authors can express those expectations correctly:

- Mana pool indices are `0` colorless, `1` black, `2` blue, `3` green, `4` red,
  and `5` white. A generic cost does not specify which pool pays it; assertions
  should not assume a payment policy absent an explicit fixture constraint.
  `assert_mana_total` takes `player` and `value`, and sums the six spendable pools.
  Use it to check generic costs without requiring a particular payment color.
- `assert_energy` takes `player` and `value` and inspects the real engine
  energy counters. Energy also appears in native/Wasm observable state output.
- `assert_card_name` takes `ref` and checks the displayed name against that
  case's card name. `assert_payment_source` checks
  that the normal mana payment caption for the current stack ability includes
  that name. Call it after activating, before resolving the ability.
- `choose_color` uses those same indices and queues a color decision before an
  activation. The harness rejects choices outside the offered color mask.
- `choose_mana` takes `player` and `color` and queues a click on an existing
  mana pool during payment. Use repeated choices to spend colored mana toward
  a generic cost explicitly. This drives the engine's normal payment code;
  clicking an empty pool fails the harness. Hybrid payment dialogs still use
  `choose_dialog` to select a combination before payment begins.
- Battlefield setup states `opponent_owned` and `token` describe ownership and
  token status. `opponent_owned` selects the opposite owner from the fixture's
  controller; it does not change control. These allow library movement cases to
  check the owner's ordered zone and verify that tokens disappear without
  adding a library card.
- Fixtures refresh the engine's cached abilities and mana colors before querying
   actions, and start outside mana payment. An explicit `untapped` setup state
   describes an established permanent, including one that originally entered
   tapped; casting that permanent still tests its normal entry behavior.
   Summoning sickness restricts only creatures: a sick noncreature artifact
   still activates its tap abilities (as in `MindStone_SickArtifactCanTapForMana`),
   while a sick creature does not. Assert `can_activate 1` for the former and
   `0` for the latter; do not copy one shape onto the other.
- For attack-trigger tests, start from first main phase with `begin_attackers`,
  select the entire group with `attack`, then call `finish_attackers` once.
  It runs the engine's global declaration event, attacker counting, mandatory
  attack triggers, and characteristic recalculation before entering the
  before-blocking response window. Cast or activate there, then use
  `begin_blockers` to enter blocker declaration and run the engine's required
  blocking triggers. An empty group is allowed. The `turn` combat shortcut starts
  directly in blocker declaration and does not exercise attack triggers.
  Beginning-of-combat triggers and attack costs still require further harness
  support. `blocked` and `blocking_count` inspect actual block assignments,
  including the core and Manalink engines' multiblocker copies.
- `block` checks legality and records a blocker assignment. After all assignments,
  use `advance_combat` to finish the declaration and enter the after-blocking
  response window. That transition dispatches `EVENT_DECLARE_BLOCKERS` once,
  including when there are no blockers, marks blocked attackers, and refreshes
  combat-dependent abilities and power/toughness through the real engine.
  Cast responses, activate abilities, and inspect continuous bonuses in that
  window; then use `combat_damage` for first strike, trample, and creature deaths.
  Calling damage before the transition or repeating the transition in the same
  combat fails the harness. End-of-combat
  trigger resolution (for example that `Stoning` effect destroying its target)
  still has no harness op: neither `combat_damage` nor `cleanup` fires
  `TRIGGER_END_COMBAT`, so that half of the interaction remains pending. Set
  `turn` to the attacking player before declaring attacks: combat
  damage resolves that player's attackers. Scripted attacks honor the separate
  attack sickness guard used by the main phase and AI, and tap ordinary attackers
  through the engine and leave vigilant creatures untapped. A sick creature
  fails the scripted `attack` guard exactly as the main-phase UI guard fails
  it, so granted haste is observable through `attack`: Instill Energy keeps a
  failing case proving the hasted creature still cannot attack. The `sick`
  observation reports raw engine state bits; the engine clears those bits for
  haste. Keep real fresh attack or activation checks as the rules expectation.
  The combat fixture
  preloads an attacking flag; an explicit `attack` step completes its declaration
  and tap obligation. This helper does not
  yet run the complete attack-cost or timing sequence. Mana activations finish
  their internal stack bookkeeping immediately;
  ordinary activations remain pending until `resolve`.
- `choose_player` queues a player target (value 0 or 1, optional choosing player
  -1/0/1). The engine still validates targeting restrictions. This lets spells
  such as Drafna's Restoration select either player's populated graveyard.
- `choose_zone` indexes the full displayed zone, including cards that fail the
  selection restriction. A choice of `-1` declines an optional selection.
- `choose_cancel_target` declines an optional target. Queue it only when the
  engine offers that choice; selecting the last eligible creature can end an
  optional multi-target selection without a second prompt.
- `choose_damage_target` selects a real pending damage packet by its recipient
  (`ref`); `choose_damage_player` uses recipient player `value`. Both take the
  choosing `player`. Missing or ambiguous packets fail the script. They drive
  damage prevention through the engine's normal response window.
- `assert_offered_count` checks the most recent zone/reveal list size;
  `assert_offered_card` checks its CSVID at `index` against `value`. The list is
  snapshot state, so library-look tests can check exactly which cards were shown.
- `record_state` records the current observable board and ordered zones;
  `assert_recorded_state` compares a later result against it. These complement
  `capture`/`restore` for repeatability checks without assuming which legal card
  a random effect selects. `assert_hand_graveyard_total` checks the combined
  count of a CSVID (`index`) in a player's hand and graveyard against `value`.
- Queue target and zone choices before `cast`: handlers can request them while
  casting or resolving within that call. A queued choice remains available if
  the handler defers its request until `resolve`.
- `can_cast` queries the real card's `EVENT_CAN_CAST` handler without selecting
  targets or paying mana. It tests that handler's permission, not full timing
  or affordability. Use `cast` to exercise payment and resolution.
- `can_target_creature` checks a named `other` target using the production
  unrestricted creature-spell target definition and validator for source `ref`.
  Equipment tests use Unsummon to distinguish hexproof from shroud without the
  AI strategically declining to bounce its own creature. This narrow probe does
  not model card-specific restrictions, timing, costs, or actual resolution.
- `cast` runs the normal response window, which can resolve spells before it
  returns. Queue a `choose_priority` response and its decisions before `cast`
  to test targets becoming illegal before resolution. `cast; destroy; resolve`
  can destroy the target after the spell has already resolved and does not
  reliably test that case without asserting the spell is still pending before
  destroying the target (as in Slay's no-draw case). Use `resolve` to drain
  anything still on the stack. A response's target choices must follow its
  `choose_priority` decision in the queue; a target decision at the front causes
  priority to pass instead of playing that response.
- Named hand/battlefield references describe engine slots. Zone changes can
  create a new slot, so inspect the destination zone's count and card identity
  rather than treating the old slot as a persistent identity.
- Battlefield setup uses real entry calls. Preload libraries before adding a
  creature whose entry draws a card; adding its library afterward cannot undo
  an earlier empty-library loss. Use a real cast for entry-behavior assertions.
- Hand inspection enumerates engine slots, not chronological draw order. With
  several spells preloaded, uncast spells remain in hand and freed slots can be
  reused by drawn cards. Check counts and contents; use a single spell and an
  explicitly ordered library when asserting the exact drawn sequence.
- `destroy` completes the engine's pending regeneration and graveyard processing
  before returning. Queue any required choices before it, and verify a setup
  death before testing mechanics such as morbid.
- Raw `show_deck` lists request `choose_zone`; use `-1` to acknowledge a
  display-only list. The informational GUI block in
  `reveal_top_cards_of_library` is suppressed in headless mode; it preserves
  the revealed count and library, and consumes no selection input. Later card
  selectors and human `reveal_card_iid` dialog acknowledgments still run.
  `select_one_and_put_the_rest_on_bottom` bounds its initial window to the
  target library's size and returns `-1` without input when that window is
  empty. Nonempty selection and bottom ordering still use the normal helpers.
  When ordering rejected cards on the bottom,
  script the requested indices and assert the resulting order, including a card
  below the looked-at window to prove the remainder moved to the bottom.
- Player targets use `choose_player`; a missing timing or presentation hook
  does not change legal targets. For example, Shock has legal player targets
  when the battlefield is empty. Report remaining harness gaps separately.
- `set_budget` sets the remaining work allowance to an integer between 200,000
  and 2,000,000 without resetting RNG, time, or planned choices. Large sweep
  boards use this explicitly for finite simultaneous-event processing. Existing
  cases retain the ordinary limit; snapshots include the selected allowance,
  and isolated-process timeouts still bound execution.


## Running the suite

From **PowerShell**, run this for the full native duel suite, including all catalog smoke checks:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch x86 -HostArch amd64 -SkipAutomaticLocation -NoLogo

cmake -S . -B out/build/shandalar-clang-x86 -DSHANDALAR_BUILD_TESTS:BOOL=ON -DSHANDALAR_BUILD_DUEL_TESTS:BOOL=ON -DSHANDALAR_DUEL_EXTENDED_TESTS:BOOL=ON
cmake --build out/build/shandalar-clang-x86 --target duel_card_tests -j6

ctest --test-dir out/build/shandalar-clang-x86 -L duel --output-on-failure -j1
```

To run the same suite under Node/Wasm:

```powershell
cmake -S . -B out/build/shandalar-wasm -DSHANDALAR_BUILD_TESTS:BOOL=ON -DSHANDALAR_BUILD_DUEL_TESTS:BOOL=ON -DSHANDALAR_DUEL_EXTENDED_TESTS:BOOL=ON
cmake --build out/build/shandalar-wasm --target duel_card_tests -j6

ctest --test-dir out/build/shandalar-wasm -L duel --output-on-failure -j1
```

For a quicker run of just the authored behavior tests:

```powershell
ctest --test-dir out/build/shandalar-clang-x86 -L behavior --output-on-failure -j1
```

CTesT handles the known-failure baseline, so the recognized failures won’t fail the gate; new failures and unexpected passes will.

For the large catalog, batch cases to amortize loading the real card database:

```powershell
cmake --build out/build/shandalar-clang-x86 --target duel_tests_batched
```

This builds the test executable and runs the full catalog in batches of 128 with
four workers, writing results under the build's `src/shandalar/duel_tests/batch-results`.
The same target works in the Wasm build. To customize the run:

```powershell
python src/magic/tests/duel/run_cases.py `
  --binary out/build/shandalar-clang-x86/src/shandalar/duel_tests/duel_card_tests.exe `
  --manifest out/build/shandalar-clang-x86/src/shandalar/duel_tests/generated/manifest.json `
  --baseline src/magic/tests/duel/data/known_failures.json `
  --output temp/duel-full --batch-size 128 --jobs 6 --timeout 30 --quiet
```

The default `--batch-size 1` retains process isolation. Batches use the same
per-case engine reset and strict baseline matching. A failure stops that child;
remaining cases start in a fresh process. A flushed journal preserves completed
results after a crash, and the watchdog bounds each test separately. New failures,
unexpected passes, and interrupted cases also run alone. A different isolated
outcome fails with `failure_kind: isolation`, exposing state leakage rather than
accepting the isolated pass. Rebuild the test executable before using batching.

`summary-shard-0.json` contains every result and total elapsed time;
`progress.json` records completed counts. Passed cases reference shared batch
logs/journals; other cases have individual diagnostic logs and result JSON.
For Node/Wasm, use the `.js` binary plus `--node <path-to-node>`; batching uses
the existing `/test-output` mount. `--tier`, `--filter`, and sharding work in
both modes. CTest's existing individual entries remain isolated.

The [October 2022 Oracle audit](../../../../docs/test-audits/manalink-2022.json)
records the engine corrections, stale fixture assumptions, and retired failure
expectations. Generic token names do not identify a unique Oracle stat line,
so automatic vanilla templates exclude tokens; explicit token-creation cases
retain their characteristic checks. Smoke checks of null-bound placeholder
records establish query/snapshot safety only, not implementation of abilities.

The [November Oracle expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11.json)
records 2,142 additional behavior cases across 84 implemented cards. Existing
recipes supply the scenarios; each new declaration pins the Scryfall Oracle
text and records its coverage limits. Null and no-op handlers were excluded
from this enrollment. Four equipment-tax cases retain their correct assertions
under the existing Suppression Field known-failure mechanism. Hybrid payment
scripts cover countered casts and additional copies as well as successful casts.

The [second expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave2.json)
adds 3,387 cases across another 84 implemented cards, including entry bonuses and
counters, cycling, three-color mana creatures, once-per-turn pumps, and entry
life loss. Partial keyword and lord components explicitly list their untested
activated abilities. Fifty cases retain Oracle expectations under three reviewed
bug classes. Scripted activation queries distinguish legality from AI preference;
AI search and gameplay keep their normal preference filtering.

The [third expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave3.json)
adds 2,384 land entry and mana cases across 44 newly covered cards and five
Deserts with existing cycling tests. Lands enter through Walking Atlas's real
hand-to-battlefield ability. Shock, bounce, Lair, scry, life-gain and two-player
Battlebond entry conditions are exercised alongside mana production/spending.
Coverage of the printed component remains distinct from land-play timing and
multiplayer rules.

The [fourth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave4.json)
adds 2,880 single-land search cases across 38 new cards and four older ramp/search
cards. The recipe exercises fetch lands, search artifacts and creatures, and
simple ramp spells. Oracle-pinned mixed libraries distinguish typed duals,
basic/snow lands, a nonbasic snow land and nonlands. Read-only selector observations check actual
engine eligibility without changing which cards it permits.

The [fifth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave5.json)
adds 2,044 landcycling cases across 21 new cards and two existing spell cards.
These use real hand activation and search-predicate observations, with exact
discard/payment receipts, reductions/taxes, both controllers, combat/opposing
turns, cancellation, pending graveyard responses and snapshot replay. Other
printed abilities remain named component gaps.

The [sixth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave6.json)
adds 6,624 single-card library-search cases across 72 new cards and six existing
card homes. Spells, creature-entry triggers and permanent activations use real
costs and selectors. Mixed Oracle witnesses check accepted and rejected types,
subtypes/changelings, names, color, legendary status and numeric restrictions,
alongside destinations, both controllers, repeated searches and snapshots.
Source removal, bounce/recast and blink are included for entry components.

Correct Oracle assertions remain under 636 exact new baselines across 12 cards:
unrestricted-search selection rules, Trapmaker's Snare's forced selection,
Cateran Summons's missing Mercenary filter, Eerie Procession's Spirit/Arcane
filter mismatch, and Loam Larva's graveyard/library mismatch. Each new failure
reproduces in isolation on native and Wasm. Card rule fixes remain deferred.
The suite reaches 129,196 catalog entries with no unexpected outcomes; all
122,572 prior entries retain their statuses. These expansions now add 343 card
homes and 19,461 behavior cases. A home can cover only a printed component;
remaining abilities and per-option affordability gaps stay explicit.

Generation now streams expansions through a bounded cache. The eager loader's
ordering and validation remain unchanged. Wasm test initial memory is 768 MiB;
the main game's memory setting is unaffected. Silverglade Elemental's unused
fourth callback argument was removed after it trapped in Wasm; its card effect
logic was unchanged.

The [seventh expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave7.json)
adds 7,388 ordinary targeted-removal cases across 48 new cards and 16 existing
card files. Independent Oracle witnesses cover destruction/exile, target type
intersections, color and subtype restrictions, changelings, planeswalkers,
legendary creatures, Vehicles, fixed characteristic limits, and combat/tap
states. Real costs, regeneration, indestructible targets, target loss and
secondary draw/life/mana receipts are checked for both controllers.

All new cases pass natively and in Wasm with identical final states. The full
136,584-entry native suite has zero unexpected outcomes; all 129,196 prior
entries keep their statuses and the 1,005 existing failure records are unchanged.
The cumulative expansion now adds 391 card homes and 26,849 behavior cases.
Alternative source abilities and the interaction gaps named in the audit remain
separate from this ordinary-cast component.

The [eighth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave8.json)
adds 3,658 hand-exchange cases across 72 new cards and ten existing card files.
Oracle-pinned draw/discard pipelines exercise spells, entry/death triggers,
activations, fixed and discard-derived amounts, exact costs, uneven/empty hands,
source removal/re-entry and snapshots. Both controllers check per-card
hand/graveyard conservation and ordered libraries without requiring a specific
AI or random discard outcome.

Native and Wasm each pass 3,614 new cases with identical final observable states.
The remaining 44 retain correct Oracle assertions and exact known-failure
records for Ring of Renewal: its handler incorrectly requires and discards a
card during activation, before the resolving effect. That rule bug is deferred.
The 1,005 previous baseline records and all existing cases and recipes in the
ten expanded files are preserved. The cumulative expansion adds 463 card homes
and 30,507 behavior cases. Missing/no-op handlers remain excluded from new
enrollment; alternative abilities and the audit's interaction gaps remain
separate coverage work.

The full 140,242-entry native suite has zero unexpected outcomes and preserves
every earlier entry's status. Validation also includes 991 recipe guards,
14 case-file checks, six shard checks and 14 engine controls per runtime.
All 108 generated native/Wasm files are byte-identical.

The [ninth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave9.json)
adds 2,178 fixed draw/mill/life cases across 41 new card files and seven existing
files. Both runtimes pass every new case, with exact costs, controller/recipient
life receipts, card multiplicities, ordered libraries, source re-entry/removal
and snapshot replay. Exact-library draws and short/empty mills have dedicated
boundary cases. No new failure baselines or rule fixes were needed.

The new `value_flow` family shares the verified pipeline implementation with
`hand_exchange`; all 115,328 earlier generated case bodies remain unchanged.
Missing/no-op handlers remain excluded. Source keyword combat, alternative
abilities/costs, player loss/win-state boundaries, replacement effects and the
other component gaps in the audit need separate scenarios. The cumulative
expansion now adds 504 card homes and 32,685 behavior cases.

The full 142,420-entry native suite has zero unexpected outcomes; all 140,242
earlier entries retain their statuses and all 1,049 failure records are
byte-identical. Validation includes 1,005 recipe guards, 14 case-file checks,
six shard checks and 14 engine controls per runtime. All 2,178 final states
match across runtimes, as do all 112 generated files.

The [tenth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave10.json)
adds 1,490 mass-destruction/exile/bounce cases across 33 new card files and four
existing files. Mixed boards cover permanent type intersections, colors,
characteristic limits, indestructibility, tokens, actual Auras, owner/controller
differences, regeneration, exact costs, tapped entry, uncounterable spells and
destruction-derived draws/life. Both controllers exercise real spells and
abilities, targeted protection and caller-owned snapshots where applicable.

Both runtimes pass 1,480 cases and match ten exact known failures across five
cards. Hurkyl's Recall selects by controller rather than owner; River's Rebuke
does not revalidate a protected player; Winds of Rath misses player one's
Auras; Kaya's Wrath and Overwhelming Forces count a regenerated creature as
destroyed. Correct Oracle expectations remain, and these rule fixes are
deferred. Missing/no-op handlers remain excluded from new enrollment.
The cumulative expansion adds 537 card homes and 34,175 behavior cases.

The full 143,910-entry native suite has zero unexpected outcomes. All 142,420
earlier statuses and 117,506 generated case bodies are preserved, and all
1,049 previous baseline records are byte-identical. Validation includes 1,025
recipe guards, 14 case-file checks, six shard checks and 14 engine controls per
runtime. All 1,490 final states and all 125 generated files match across runtimes.

The [eleventh expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave11.json)
adds 8,472 activated-removal cases across 48 new card files and ten existing
files. Both controllers cover actual casting/activation costs, source readiness,
target predicates, attached Auras, combat states, responses, indestructibility,
regeneration, repeated activations and caller-owned snapshots where applicable.
The cumulative expansion adds 585 card homes and 42,647 behavior cases.

A snapshot-restored target probe observes each handler's real target definition
for a named candidate. This avoids mistaking another legal target, including
the source itself, for acceptance of that candidate. Successful effects still
use ordinary activation and resolution. Exact fixture receipts replace redundant
checks of absent witness identities, reducing generated data. The test-only
WebAssembly arena starts at 1 GiB.

Both runtimes pass 8,460 new cases and reproduce twelve exact known failures.
Seal of Doom and Notorious Assassin wrongly permit regeneration; Thrashing
Brontodon sacrifices its target instead of destroying it, defeating
indestructibility. Correct Oracle expectations remain and game-rule fixes are
deferred. Missing/no-op handlers are excluded from new enrollment.

The full 152,382-entry native suite finishes in 313 seconds with zero unexpected
outcomes. All 143,910 previous statuses and 118,996 generated case bodies are
preserved, and all 1,059 previous failure records remain byte-identical.
Validation includes 1,039 recipe guards, 14 case-file checks, six shard checks
and 17 engine controls per runtime. All 8,472 final observable states and 141
generated case files match across runtimes.

The [twelfth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave12.json)
adds 2,430 continuous-stat cases across 40 new card files and eight existing
files. Both controllers test signed bonuses, overlapping colors and tribes,
changelings, tokens, source entry/departure, late arrivals, tapping/combat
predicates, hybrid payments and snapshots. Marked damage followed by loss of
a toughness bonus exercises real state-based deaths. The cumulative expansion
adds 625 card homes and 45,077 behavior cases.

Both runtimes pass 2,418 new cases and reproduce twelve exact known failures.
Builder's Blessing and Sprite Noble apply their static toughness bonuses while
their spells are still on the stack. Correct pre-resolution expectations remain;
those card-rule fixes are deferred. Missing/no-op handlers remain excluded.

Crowded-board tests exposed a shared AI evaluator with three scratch arrays
sized for sixteen creatures and a signed byte index. On larger legal boards
the arrays overwrite neighboring locals, producing native crashes. The arrays
and index now cover the actual active-card capacity. This significant testing
blocker is the only production fix in this batch; the new engine regression
evaluates 17, 128 and 140 creatures for each player with snapshot replay.

The full 154,812-entry native suite finishes in 335 seconds with zero unexpected
outcomes. All 152,382 previous statuses, 127,468 generated case bodies and 1,071
previous baseline records are preserved. Validation includes 1,052 recipe guards,
14 case-file checks, six shard checks and 18 engine controls per runtime.
All 2,430 final observable states and 155 generated case files match across
runtimes. The crowded-board regression is part of `duel.characteristics`,
alongside the existing characteristic-value controls.

The [thirteenth expansion audit](../../../../docs/test-audits/oracle-expansion-2026-11-wave13.json)
adds 1,044 single-X spell cases across 18 new card files. Both controllers test
zero and positive X, printed base costs, black-only X payment, ordered value
effects, real damage and responses, exact zones and snapshot replay. The
cumulative expansion adds 643 card homes and 46,121 behavior cases.

Both runtimes pass 1,039 cases and reproduce five exact known failures. In the
AI-controlled Drudge Skeletons fixtures, the offered regeneration response is
consumed without activating: no black mana is spent and the creature dies.
Correct survival, tapping, payment and zone expectations remain. The matching
human-controlled fixtures pass. These shared activation failures are deferred;
no card-rule fix was made. Missing/no-op handlers remain excluded.

Explicit scripted X input uses the actual payment engine and stays queued
through fixed costs and AI searches. Four new characteristic-value controls
check zero/positive X, real entry counters, repeated-X payment and snapshot
replay. Validation includes 1,066 recipe guards, 14 case-file checks, six shard
checks and 22 engine controls per runtime. All 129,898 earlier generated case
bodies and 1,083 previous baseline records are preserved. The 1,044 final
observable states and 157 generated files match between runtimes.

The full 155,856-entry native suite finishes in 333 seconds: 154,603 pass,
1,088 reproduce known failures and 165 retain their existing unimplemented
expectations, with zero unexpected outcomes. All 154,812 prior case identities
and statuses are preserved. No new unimplemented card was enrolled.

Combat scripts inspect historical `blocked` state after `advance_combat` finishes
the declaration. The selector can reject a lone blocker immediately if no second
eligible blocker exists, while final minimum-blocker validation still handles
incomplete assignments. After setting mana pools, use `refresh` before querying
availability so the engine's cached mana options match those pools.

Expanded behavior data lives in `generated/duel_cases.bin`. It contains a shared
UTF-8 string table, an index and independently compressed case records. The
runner reads the index for discovery, then decompresses only the selected case
while it executes. Setup cards, all nine step fields, operation order, names,
Oracle text, seeds and fixture references are preserved. The C++ driver keeps
only the small shared fixtures and catalog tables; no expanded data shards are
compiled or linked. Generator outputs retain their timestamps when unchanged,
so a data-only fixture edit does not force a driver rebuild.

Native runners use their configured generated archive by default. Pass
`--duel-case-data=PATH` to select another archive. Node/Wasm mounts the generated
directory at `/test-cases` using NodeFS, with `DUEL_TEST_CASE_DIR` available to
override its host directory. The archive is not preloaded into Wasm memory.
Keep the generated archive available when moving a test executable. Configure
`SHANDALAR_DUEL_TEST_BINARY_DIR` to relocate the executable; its default archive
location remains the generated directory in that build tree.

Archive format version 1 has a 48-byte little-endian header (`DUELCASE`, version,
case count, string/index offsets, total length and metadata CRCs). Each 24-byte
index entry records a name ID, file offset, compressed/raw lengths and a raw
record CRC. Each record stores five metadata string IDs, seed and setup/step
counts, followed by 20-byte setups and 36-byte steps using string IDs instead
of pointers. Records use standard zlib compression. The reader rejects wrong
versions, malformed bounds, invalid IDs, truncation and checksum failures.
Changes to this layout require a format version bump.

The batched build target defaults to four native workers or two Node/Wasm
workers. Four concurrent Node workers can exhaust this VM's memory when other
validation tools are active. An infrastructure failure must be rerun at lower
concurrency and recorded separately from card-rule failure baselines.

`python src/magic/tests/duel/test_case_archive.py` checks the format, lazy reads,
malformed data and unchanged-output timestamps. The optional generator flag
`--legacy-fingerprints` emits audit hashes compatible with the former C++ data
arrays; normal generation omits that migration-only serialization work.

The [external-data migration audit](../../../../docs/test-audits/external-case-data-2026-11.json)
records the unchanged 130,942 behavior cases and observable states, complete
155,856-entry native/Wasm verification, exact binary/archive sizes, and the
separately recorded VM memory-pressure retry.

## Code coverage

From **PowerShell** (repository root), run the behavior suite under coverage:

```powershell
.\ctestcoverage.bat behavior
```

This configures an instrumented 32-bit `Debug` build in
`out/build/shandalar-llvmcov-x86` (`SHANDALAR_DUEL_COVERAGE=ON`), builds
`duel_card_tests`, and runs the selected CTest label with `LLVM_PROFILE_FILE`
set so every test process writes a raw profile. The script then merges them
with `llvm-profdata` and renders an `llvm-cov` summary plus HTML, all under
`out/coverage/llvmcov/`. Pass a second argument as a `ctest -R` filter to
iterate on a subset, e.g. `.\ctestcoverage.bat behavior AbbeyGargoyles`.

`ctest` has no flag that instruments binaries: `ctest -T Coverage` only
collects results from a pre-instrumented build. The instrumentation lives in
the `shandalar_llvm_coverage` interface target, applied to the duel runtime
libraries and `duel_card_tests` when `SHANDALAR_DUEL_COVERAGE=ON`.

The toolchain matters: coverage needs the 32-bit profile runtime
(`clang_rt.profile-i386.lib`), which 64-bit-only LLVM distributions do not
ship, and the manalinkeh layer assumes 32-bit pointer sizes, so a 64-bit
coverage build is not viable on this branch either. The script uses
`C:\bin\LLVM-21.1.8-win32` by default (`SHANDALAR_LLVM_COV` overrides it);
requesting `-DSHANDALAR_DUEL_COVERAGE=ON` with a toolchain that lacks the
runtime fails configure with a message saying so.


For searches after a shuffle, `choose_zone_card` scripts a CSVID instead of a
raw `choose_zone` index. The headless host chooses the first offered occurrence
of that card; duplicate copies are interchangeable. The engine's usual search
filter still validates that selection. For an observed optional selector, a
missing or rejected named choice becomes a legal decline so correct eligibility
and zone assertions can expose a card bug. Required/unobserved selectors retain
strict input errors. Use `choose_zone` with `value: -1` to explicitly decline.


Generated hybrid-payment cases use `choose_payment` to select a requested mana
vector from the real payment dialog. The host fails on a missing or ambiguous
option. It does not alter costs or choose a replacement payment automatically.
Exact single-option payments and AI payment paths need no dialog script.


Hybrid creature recipes require a reviewed `payment_colors` array, with one
legal color per two-color hybrid symbol. Supplemental payment variants use
explicit pools and assert the remaining mana after a real cast. Existing family
scope still applies: a component recipe does not claim the card's additional
unexercised abilities are covered, and Flash timing still needs dedicated tests.


`assert_poison` observes a player's poison counter total. Card observations
include `minus1_counters` as well as `plus1_counters`; both counters and player
poison appear in observable snapshots used for deterministic replay comparisons.

`assert_counter` reads a named battlefield permanent's actual counters, for
example `{"op":"assert_counter","ref":"source","counter_kind":"charge","value":3}`.
Reviewed kinds are `plus1`, `minus1`, `charge`, `loyalty`, `knowledge`, `elixir`,
`divinity`, `quest`, `blood`, `gold`, and `study`. The action only observes;
fixtures must obtain and spend counters through the card's real entry, trigger,
or activation. Explicit receipts must be repeated after snapshot replay for
counter kinds outside the ordinary plus/minus-one board summary.

`blocking_count` exposes the engine's stored blocker assignment. It is useful
during live combat; surviving pairs can retain that historical assignment after
`end_combat`. Use combat, damage, and zone receipts after that boundary instead
of interpreting this field as a current rules-level blocking status.

`untap_step` leaves the real wrapper's phase value. For a later main-phase
action, select first main with `turn` for the already-current player after the
actual `advance_turn` and `untap_step` operations. This phase handoff must retain
the turn processing, untaps, and history rather than replace them.

Battlefield setup does not establish a previously completed entry trigger. For
an entry-source negative control, cast and resolve that source and its initial
effect before casting an unrelated entrant. Then assert that the later entrant
causes no additional effect. Seeded graveyard, exile, and library references are
zone indices; observe their exact zones rather than using instance properties.

For counted graveyard triggers from an enchantment's destruction, use a real
destruction spell and normal resolution. The standalone `destroy` helper can
move an enchantment to its graveyard and count its death without draining the
counted trigger at a normal action boundary. Femeref Enchantress's Naturalize
controls verify the actual payment, target, graveyard movement, and draw. Raw
helper diagnostics remain testing references rather than card-failure baselines.

`choose_trigger` selects an offered trigger effect by the referenced card's
CSVID. Earlier response windows pass without consuming the planned selection;
an unavailable trigger leaves an unconsumed choice and fails the case. Snapshot
capture/restore also saves the script's named-instance bindings so replay can
rebind cards that moved between the battlefield and hand.

`choose_trigger_source` instead selects the named live instance, with an
explicit `player`. It waits until that instance is a legal offered trigger
response, so a same-display invisible legacy cannot make the choice ambiguous.
Use the original CSVID-based `choose_trigger` for a dying source's legacy
handoff. Exact-source intent and instance timestamps are part of test snapshots.

`choose_optional_target` records cancellation intent with a named legal fallback
and explicit chooser. It cancels only when the actual selector allows it;
otherwise the engine validates the fallback normally and increments
`assert_forced_target_choices`. That receipt and ordinary zone/effect assertions
can expose an incorrectly forced action without bypassing targeting rules.
Counts reset with the fixture and are restored by test snapshots.

`choose_activation_mode` records intent for one real activation of a named live
source. Supply an explicit `player`, exact one-line `mode_label`, legal
`fallback_color`, and boolean `allow_auto_nonmana`. Queue conditional cost
choices after that action, then `activate` the same source and immediately
`require_activation_mode` with `value: 1` before checking effects. Registration
requires an empty ordinary choice queue. The receipt is 1 for the requested
offered mode, 0 for an unselected no-menu path, or a negative fingerprint of an
offered menu missing that label. A different legal mana action cannot satisfy
the receipt. A failed receipt clears only its conditional cost choices;
successful selection retains strict consumption checks.

Enable `allow_auto_nonmana` only after Oracle review establishes exactly one
nonmana activated ability: it can recognize a newly pending activation from that
source when the engine legitimately omits a one-option menu. Multiple value abilities
still require their actual menu protocol. Mode intent, source timestamps,
conditional choice boundaries, and receipts are restored by snapshots; an
unfinished intent is a harness error. Printed costs and resulting effects need
their own independent assertions.

Scripted targeting for either controller uses the rules' allowed controller,
instead of the AI's preferred controller. The actual chooser and all other
legality checks are preserved. Normal gameplay and AI searches retain their
usual targeting policy.

`assert_zone_multiplicity` checks an exact CSVID count in one player's zone,
including zero. Unlike combined hand/graveyard totals, it detects moving the
wrong card when both cards already have copies in both zones.

`choose_optional_zone` declines a graveyard selection if offered, with an
explicit legal fallback CSVID. If the engine requires a selection, the host
chooses that fallback through normal validation and records a forced action.
`assert_forced_zone_choices: 0` and unchanged-zone assertions expose the rules
failure without aborting the process. A skipped optional trigger needs no
selection. All ordinary decisions still require consumption. This input path
does not relax `must_select` or card eligibility. Forced-action counts are part
of the caller-owned test snapshots. Exact public-recovery reveal notices are
acknowledged automatically; gameplay dialogs remain scripted.
