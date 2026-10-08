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
  legacy_exceptions.json
```

Card folders currently group 100 CSVIDs each: `csvid // 100`, padded to three
digits. Filenames start with the five-digit CSVID followed by a readable name.
Only cards with authored cases need files; catalog smoke and wording templates
are generated separately. These folders keep both file lists and agent edit
assignments small.

A card file records the behaviors its cases actually assert; its existence does
not imply complete rules coverage. For example, the current Overrun cases check
the team bonus and cleanup, while trample combat still needs a separate case.
The Lignify cases check base stats and ability removal; creature subtype remains
outside the current inspection interface.

Original sets can become a browsing/filter index once we have a reviewed
first-printing mapping. The pinned Oracle corpus currently lacks that mapping;
the set in a Scryfall card URL may describe a reprint. A card's name, CSVID, and
Oracle ID determine its identity. Moving its file into a set folder later does
not change its test names or execution order. Do not duplicate tests for reprints.

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
They must not change engine/card implementations, shared fixtures, harness code,
Oracle data, or the known-failure manifest to make tests pass. Preserve expectations
derived from Oracle text and report failures with the case name, expected behavior,
actual result, and any suspected harness limitation. The integrating agent reviews
and classifies failures; production fixes are handled separately. Only the integrating
agent rebuilds shared native/Wasm binaries, so concurrent authors cannot invalidate
one another's test runs.

From the repository root:

```sh
python src/magic/tests/duel/test_case_files.py
python src/magic/tests/duel/generate_cases.py --output out/duel-tests/generated
```

The loader regression tests are also registered as `duel.case_files` in CTest.

Known assertion failures specify an exact `assertion_count` as well as matching
diagnostic patterns. Additional or missing failures do not match that baseline;
an unexpected pass also fails the gate. Harness errors cannot be baselined.


## Current action interface

Oracle supplies the rules expectations. These details describe how scripts drive
the harness, so test authors can express those expectations correctly:

- Mana pool indices are `0` colorless, `1` black, `2` blue, `3` green, `4` red,
  and `5` white. A generic cost does not specify which pool pays it; assertions
  should not assume a payment policy absent an explicit fixture constraint.
  `assert_mana_total` takes `player` and `value`, and sums the six spendable pools.
  Use it to check generic costs without requiring a particular payment color.
- `choose_color` uses those same indices and queues a color decision before an
  activation. The harness rejects choices outside the offered color mask.
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
  failing case proving the hasted creature still cannot attack. The combat fixture
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
- `cast` runs the normal response window, which can resolve spells before it
  returns. Queue a `choose_priority` response and its decisions before `cast`
  to test targets becoming illegal before resolution. `cast; destroy; resolve`
  can destroy the target after the spell has already resolved and does not
  reliably test that case. Use `resolve` to drain anything still on the stack.
- Named hand/battlefield references describe engine slots. Zone changes can
  create a new slot, so inspect the destination zone's count and card identity
  rather than treating the old slot as a persistent identity.
- Hand inspection enumerates engine slots, not chronological draw order. With
  several spells preloaded, uncast spells remain in hand and freed slots can be
  reused by drawn cards. Check counts and contents; use a single spell and an
  explicitly ordered library when asserting the exact drawn sequence.
- `destroy` completes the engine's pending regeneration and graveyard processing
  before returning. Queue any required choices before it, and verify a setup
  death before testing mechanics such as morbid.
- Informational library reveal windows currently request `choose_zone` too;
  use `-1` to acknowledge them. When ordering rejected cards on the bottom,
  script the requested indices and assert the resulting order, including a card
  below the looked-at window to prove the remainder moved to the bottom.
- Player targets use `choose_player`; a missing timing or presentation hook
  does not change legal targets. For example, Shock has legal player targets
  when the battlefield is empty. Report remaining harness gaps separately.


## Running the suite

From **PowerShell**, run this for the full native duel suite, including all catalog smoke checks:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch x86 -HostArch amd64 -SkipAutomaticLocation -NoLogo

cmake -S . -B out/build/shandalar-clang-x86 -DSHANDALAR_DUEL_EXTENDED_TESTS=ON
cmake --build out/build/shandalar-clang-x86 --target duel_card_tests -j6

ctest --test-dir out/build/shandalar-clang-x86 -L duel --output-on-failure -j1
```

To run the same suite under Node/Wasm:

```powershell
cmake -S . -B out/build/shandalar-wasm -DSHANDALAR_DUEL_EXTENDED_TESTS=ON
cmake --build out/build/shandalar-wasm --target duel_card_tests -j6

ctest --test-dir out/build/shandalar-wasm -L duel --output-on-failure -j1
```

For a quicker run of just the authored behavior tests:

```powershell
ctest --test-dir out/build/shandalar-clang-x86 -L behavior --output-on-failure -j1
```

CTest handles the known-failure baseline, so the recognized failures won’t fail the gate; new failures and unexpected passes will.
