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

## Pending behavior scenarios

Permission queries and printed-stat checks are partial coverage. For each Oracle
ability, author at least a successful effect/zone transition plus relevant legal
alternatives, restrictions, and expiry or ownership checks. Multiple modes need
their own effect cases. If a harness hook is missing, record the missing behavior
explicitly; do not substitute a permission query and call the ability tested.

The October 7 batch review expanded Blaze of Glory and Drafna's Restoration
from permission queries to effect cases. Blaze checks mandatory multiblocking,
flying and tapped restrictions, summoning sickness, a target leaving the
battlefield before resolution,
and casting timing. Drafna checks zero/subset/multiple selections, both library
orders, opponent ownership, empty/nonartifact graveyards, and snapshot replay.
Four Drafna scenarios expose a shared library-return adapter bug: a card slot
is treated as an internal card ID. Their correct Oracle assertions remain known
failures. Artifact targets being selected at casting time, and becoming illegal
before resolution, still need coverage; current selection occurs at resolution.

Lure now exercises required blocking with multiple legal alternatives and an
ineligible tapped/flying restriction. Lifeforce and both Elemental Blasts now
counter real spells. This does not make the whole batch complete: Juggernaut's
mandatory attack still needs the real attacker-selection pipeline; Oasis and
Argivian Blacksmith currently check legacy activation restrictions, without a
successful damage-prevention window. Existing query/stat cases do not cover
those effects. Blaze of Glory's multiblocker combat damage, its cleanup followed
by another combat, defending-player casting, and Wall targeting also remain
unverified. Track these gaps when scheduling future authoring batches.

Shock, Giant Growth, and Unsummon need cases where their creature target leaves
the battlefield before resolution. The intended expectation is that the spell
has no effect on that target and goes to its owner's graveyard; paid mana is
not refunded. These are not counted as passing behavior coverage.

The current `cast` interface exposes the legacy casting response window, which
rejected both Lightning Bolt and a permanent-targeting Red Elemental Blast as
opponent responses in the attempted scripts. A normal instant-response window
or a verified pause after casting and before resolution is needed to express
these scenarios. Destroying the target after an already-resolved `cast` is not
an acceptable substitute. This is an authoring/harness gap, not a confirmed
card implementation failure.

Nature's Spiral currently restricts graveyard targets to creatures. Isolated
permission cases retain the Oracle expectation that lands, artifacts,
enchantments, and planeswalkers are eligible. Full noncreature return and
destination scenarios remain pending until the restriction is fixed; these
permission cases do not claim to verify their resolution.

Shrink, Hydrosurge, and Sensory Deprivation retain negative-power assertions.
Manalink's shared power computation currently clamps negative results to zero.
The known failures track that observable rules difference, not a claim that the
underlying power modifier is absent. Rancor/Wildsize trample damage and Rally the
Forces first-strike damage still need combat-resolution cases; stat bonuses alone
do not verify those keywords.

Colossal Might and Unnatural Predation now check real trample damage, Daring Leap
checks first-strike survival, and Mighty Leap, Aim High, and Spidery Grasp check
flying/reach block legality and expiration. These cases do not replace the
pending cases for the other cards listed above.

Bright Reprisal, Lich's Caress, and Puncturing Blow currently bind to `card_dummy`.
Their running cases retain the Oracle expectation that an eligible creature
allows casting. Full destruction, draw/life, indestructibility, and exile
replacement scripts are preserved under `data/pending_cases/cards/` and are
excluded from execution and passing coverage until real handlers are available.
Mighty Leap similarly retains an opposing-creature permission assertion; its
opposing-target resolution script remains in that pending tree while the
handler incorrectly restricts targets to creatures its caster controls.

Ambush Viper's current cases cover casting, printed stats, and deathtouch combat.
Flash timing remains pending: the scripted casting interface does not yet expose
a verified opponent-priority window for casting a creature with flash.

Thunder Strike and Skillful Lunge share a handler that restricts targets to its
caster's creatures. Permission cases retain the Oracle expectation that opposing
creatures are eligible; full opposing-target resolution/expiry scripts remain
under `data/pending_cases/cards/` until that restriction is fixed.

Shelter verifies chosen-color prevention, draw, ownership, and expiry in normal
creature combat. A separate protection-plus-trample scenario remains pending.
The current interface cannot script the exact division of trample damage between
a blocker and the defending player. The legacy damage pipeline applies prevention
before computing overflow; its observed zero player damage alone does not prove
an illegal modern assignment, since assigning all damage to a blocker is permitted.
An explicit assignment hook is needed before gating the intended two-to-blocker,
six-to-player outcome against Comprehensive Rule 702.19b.

Unmake's exile scenarios also check the scripted BBB and WWW hybrid payments.
The real casting path currently charges its printed BBB cost in the core engine
and charges the selected hybrid cost again in the card handler. These cases retain
the three-mana Oracle expectation and the successful exile/ownership assertions;
known failures track only the extra mana charge. Pit Fight similarly retains a
permission check requiring two distinct creatures, rather than allowing its one
friendly creature to satisfy both target definitions.

Death Ward, Drudge Skeletons, and Living Wall are recorded in
`data/legacy_exceptions.json`, not as bugs: Shandalar uses pre-Sixth Edition
timing, where regeneration could only be used at destruction time in the
damage-prevention window and proactive shields did not exist. Their cases assert
the old-rules-correct rejections (no main-phase shield) as passing. Death
Ward's pending file is explicitly a modern-rule shield reference, excluded by
that exception. It is not a destruction-window script or a request to modernize
the handler. Legacy destruction-window regeneration success remains untested.

Hypnotic Specter's random discard uses the engine's seeded RNG. The dialog when
the opponent discards is a notification after selection, acknowledged with
`choose_dialog 0`; it does not select the discarded card. Cases cover both
players, populated and empty hands, blocked combat, and repeated random selection
after snapshot restoration and an AI backup/restore. Merchant Ship covers empty
and multiple blocker declarations, blocked attacks, and snapshot restoration.
Jade Statue's combat animation and Orcish Oriflamme's attacking bonus pass in the
after-blocking response window. Obelisk remains activatable with an opposing
creature present because it can legally target itself; its case proves the
self-target return rather than asserting that the entire ability is unavailable.
Healing Salve remains outside this batch; its prevention mode requests a zone
choice the harness does not map.

The next reviewed batch adds 124 cases across 36 cards: player-target draw,
discard and life spells; mass bounce, damage and temporary bonuses; mana removal;
tap/untap and pump abilities; flying, first strike, landwalk, attack/block
restrictions, and characteristic-dependent power/toughness. Hibernation and
Lhurgoyf replay snapshots after the AI backup/restore path. Treasure Trove uses
the stocked shared board; Hibernation also checks preserved hand, library,
graveyard and exile witnesses belonging to both players.

Fixture initialization uses explicit libraries for both players. The legacy
opponent-library index of -1 generates random encounter cards on draw, bypassing
the supplied library, so headless fixtures initialize the normal library mode.
Mind Rot scripts the human player's card selection; the opponent's AI chooses
its discards with the seeded RNG. Opponent cases assert counts and conservation
of every available card, rather than prescribing an AI choice. Opponent discard
notifications require `choose_dialog` with `player: 1`.

Mana Short taps the correct lands but leaves all six spendable mana pools intact.
Its handler writes `g_special_mana_pool` through the legacy `mana_pool` mapping,
while payment and inspection use `g_raw_mana_available`. Three cases preserve
the correct Oracle assertions with exact six-assertion baselines. The production
fix remains separate. Remaining coverage gaps include draw-from-empty game-loss
inspection, Lava Axe planeswalker targets, Plague Wind's regeneration prohibition,
and Puppeteer's optional choice to leave the target unchanged. The short/empty
draw cases check zone results and do not claim to verify the game-loss rule.


The following reviewed batch adds 135 cases for 36 more cards. It covers
multi-target destruction and exile, additional sacrifice costs, mass land
destruction by subtype, modal spells, graveyard retrieval, mana abilities,
combat-only targeting, protection, temporary stat changes, trample, vigilance,
first strike, and conditional characteristics. Archivist, Book of Rass, Ashes
to Ashes, Rain of Salt, and Beast of Burden exercise populated zones and snapshot
restoration, including the AI backup/restore path.

The test host now refreshes derived characteristics after changing the active
turn or removing a creature from combat. Combat removal calls the real engine
helper, and block declarations reject blockers controlled by the attacking
player. These prevent cached stats and impossible blocks from misleading rules
tests. Angelic Page tests use the scripted human player for both friendly and
opposing targets; the AI's preference for friendly targets is not a targeting
restriction in the rules.

Dwarven Thaumaturgist correctly switches Craw Wurm from 6/4 to 4/6, but its
legacy effect survives cleanup and the stats remain switched. The exact two
failing expiry assertions are recorded in the known-failure manifest; the
production fix remains separate. Remaining gaps include game-loss inspection
after drawing from an empty library, Beasts of Bogardan's white token condition
and protection targeting/Aura clauses, and planeswalker damage targets. These
authored tests do not establish a line/branch coverage percentage.

After this batch, the authored behavior gate has 1,278 cases per runtime:
1,239 pass and 39 have exact known-failure baselines. Native and Wasm observable
states agree for every case. All 134 passing new cases also pass two shuffled
iterations in each runtime.

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
