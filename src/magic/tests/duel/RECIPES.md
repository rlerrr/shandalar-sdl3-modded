# Faster Oracle-backed card authoring

Recipes generate ordinary behavior cases for the real duel engine. They are
distinct from the older card-ID templates. A reviewed recipe costs authoring and
review work once; matching cards get its scenarios without model-written JSON.
Existing authored cases remain in their card files alongside the declaration.

```json
{
  "version": 1,
  "card": "Divination",
  "csvid": 10405,
  "recipes": [{"family": "draw_cards", "version": 1}]
}
```

Version 1 declarations derive amounts and costs from exact pinned Oracle wording.
Version 2 declarations supply explicit effect parameters, reviewed against Oracle,
and still derive mana costs from structured Oracle data. The registry supplies identity.
Dispatch hints help prioritize work, never supply expectations. `card_dummy`
does not automatically mean a vanilla creature is broken: characteristics can
work through the core engine without a card handler.

## Reviewed families

| Family | Exact supported Oracle text | Scenarios |
| --- | --- | --- |
| `draw_cards` | `Draw a card.` or `Draw two/three/four cards.` | Ordered library, draw identities and multiplicities, kept hand card, payment, insufficient mana, snapshot replay |
| `gain_life` | `You gain N life.` (1-20) | Both life totals, injured controller, payment, insufficient mana, snapshot replay |
| `pump_creature` | `Target creature gets +P/+T until end of turn.` (0-10 each, nonzero combined) | Own and opposing targets, unselected witness, cleanup, no creature, payment, insufficient mana, snapshot replay |
| `damage_any_target` | `NAME deals N damage to any target.` (1-4) | Surviving and lethal creature damage, both players, lost target while pending, unselected witness, payment, insufficient mana, snapshot replay |

Automatic version 1 enrollment accepts only normal-layout Instants and Sorceries with no keywords and a fixed, ordinary
mana cost of 1-10 qualify. Full text must match: extra clauses, restrictions,
optional effects, alternative costs, and multi-face cards go to manual authoring.
Unsupported cards receive an inventory reason; an explicitly mismatched recipe
fails generation. Witness characteristics and fixture contents are checked before
generation. Amount ranges are deliberate review boundaries, not approximations.

These scenarios are partial coverage. Damage recipes do not yet exercise
planeswalkers or redirected/prevented damage; pumps do not exercise granted
abilities or every possible target becoming illegal. Extend reviewed scenarios
when those interactions matter rather than counting a card as fully covered.

## Creature components

`creature_keywords` is a version 2 whole-text recipe for normal single-face
creatures with numeric power/toughness, positive toughness and ordinary fixed
mana costs. Its only effect parameter is an explicit, sorted `keywords` array.
Printed power/toughness and costs come from structured pinned Oracle fields.
Rules text must consist entirely of the selected unconditional keywords and
reviewed reminder strings; Oracle's keyword tags alone never establish that an
ability is unconditional. Any additional clause rejects automatic enrollment.

```json
{
  "family": "creature_keywords",
  "version": 2,
  "params": {"keywords": ["Flying", "Vigilance"]},
  "oracle_text": "Flying\nVigilance",
  "variants": ["cast", "payment", "insufficient_mana", "snapshot",
               "fresh_attack", "unblocked", "block_ground", "block_flying",
               "tapped_attacker", "tapped_blocker", "flying_evasion", "flying_reach"]
}
```

Every creature gets real casting, exact/insufficient payment, populated-zone
snapshot replay after AI search, fresh-cast attack eligibility, unblocked combat,
grounded/flying blocking and tapped attacker/blocker rejection. Attack declaration
also checks vigilance's tap obligation. Positive combat cases advance through
attacker and blocker declarations before dealing damage. Additional components
exercise flying evasion and reach, first-strike survival, deathtouch against a
larger creature, trample against a flying blocker, and complete single/pair Menace
declarations. Unblocked damage checks lifelink and double strike when present.
Menace block selection is provisional: legality is asserted after advancing the
entire declaration, rather than rejecting the first selection of a valid pair.

These remain partial scenarios: Flash timing is explicitly marked untested in
generated provenance, because the existing cast helper bypasses the game's
priority/timing permission path. Hybrid/zero costs, nonnumeric characteristics,
zero toughness, additional abilities, lost/granted keywords and arbitrary keyword
interactions require other recipes or authored cases. First-strike, deathtouch
and trample component cases need positive power. The generator exposes those
limits instead of inferring missing expectations from implementation code.

The offline enrollment tool inventories exact supported candidates, preserves
existing authored cases, and reports distinct cards, new homes, case counts,
covered keyword components and deferred reasons:

```powershell
python src/magic/tests/duel/creature_recipe_tools.py --output out/duel-tests/creature-inventory.json
python src/magic/tests/duel/creature_recipe_tools.py --write --output out/duel-tests/creature-enrollment.json
```

Vanilla `card_dummy` records qualify because the core engine supplies their
characteristics. Keyword-only no-op records are deferred for an implementation
review. This avoids labeling every no-op creature missing or silently treating
every keyword no-op as implemented. Neither dispatch choice supplies rules
expectations.

`variants` can select a nonempty subset of the names in `VARIANTS`. Omitting it
generates all reviewed scenarios. Unknown options fail validation. Names use
CSVID, family, and variant, so moving a file changes neither names nor order.
Versions make recipe revisions deliberate; Oracle refreshes still require review
of the pinned corpus diff.

Version 2 declarations may supply `resolve_zones`, a map from selected variant
names to ordered zone-choice indices (`-1` finishes an optional choice). Choices
are queued immediately before the source spell resolves and repeated on snapshot
replay. They do not apply to setup spells or illegal-target variants. For example,
`"resolve_zones": {"snapshot": [-1, -1]}` keeps both scryed cards on top in their
original order. This scripts presentation decisions; the assertions still come
from reviewed Oracle behavior. Unknown variants, invalid indices, and plans for
variants that never resolve are rejected.

## Signed stat reductions

`reduce_creature` declares signed `power` and `toughness` adjustments, with at
least one negative component. Amounts are explicit, not extracted from prose.
Optional `draw`, `gain_life`, `lose_life`, and `tap: true` describe coupled effects.
Scaled Wurm is the surviving witness, Llanowar Elves exercises zero-toughness
death, and Wall of Stone exercises genuinely negative power. Witness statistics
are checked against the pinned Oracle corpus.

Friendly/opposing cleanup, lethal toughness, negative power, illegal sole targets,
empty boards, wrong types, payment, insufficient mana, and snapshot replay are
separate variants. Pre-resolution assertions check that characteristics and life
have not changed during casting. Post-resolution checks inspect both players'
zones, complete ordered libraries, unselected creatures, life, tapped state, and
absence of damage. Draw/life effects still happen when a legal target dies from
the reduction; they do not happen when that target was illegal on resolution.
Only declare variants consistent with a card's conditional clauses: Blood Lust
uses its high-toughness branch, Tragic Slip starts without a prior death, and
Domain uses explicit tapped support lands.

## Permanent movement components

`move_permanent` uses explicit `targets` (artifact/enchantment/creature/land),
`destination` (`hand`, `library_top`, `library_bottom`, `library_second`), and optional
`controller` (`any`, `self`, `opponent`). Reviewed `draw`, `gain_life`,
`target_gain_life`, and `target_lose_life` amounts describe coupled effects.
`target_tapped: true` restricts legal targets; `wrong_state` then tests an untapped
creature. `wrong_controller` checks controller restrictions, while `target_TYPE`
scenarios check every additionally declared permanent type.

Scenarios check that the target remains in play before resolution, then inspect
both players' hands, graveyards, exile, life, and the complete ordered libraries.
Libraries begin populated, so top, bottom, and second-position insertions are
observable. Target removal before resolution suppresses every declared effect.
These witnesses have matching owner/controller; stolen permanents need authored
cases that establish control-changing effects legally.

`cast_dialogs` maps specific positive variants to reviewed zero-based choices
queued before casting. This accommodates normal Cyclonic Rift casts that decline
Overload when it is affordable; exact-payment variants need no overload dialog.
A snapshot replays those same choices. Cast-query negatives for Cyclonic Rift are
omitted because its overloaded mode permits casting without legal targets.
Additional modes and alternate-cost behavior need separate cases.

## Explicit components for more complex spells

`destroy_permanent` is an explicit-only family. Its `params.targets` lists reviewed
types (`artifact`, `enchantment`, `creature`, `land`). Declare the actual scope;
document any intentionally covered subset such as only Dreadbore's creature
branch. Its scenarios check opposing and owned victims, an eligible unselected
permanent, an empty board, an ineligible type, exact payment, no available mana,
an illegal sole target, and snapshot replay. Optional `target_TYPE` variants
exercise each additional declared type.

Destruction life parameters are `gain_life`, `lose_life`, `self_damage`,
`target_gain_life`, `target_lose_life`, and `target_damage` (1-20). Both players'
life totals are checked, including friendly targets and no secondary effects
when the sole target becomes illegal. Variable amounts must match the witnesses:
Divine Offering and Crumble use Sol Ring's mana value of one; Devour in Shadow
uses Grizzly Bears' toughness of two. Regeneration, indestructible, other mana
values/toughnesses, and undeclared clauses require dedicated cases.

```json
{
  "family": "destroy_permanent",
  "version": 2,
  "params": {"targets": ["artifact", "enchantment"], "target_gain_life": 4},
  "oracle_text": "Destroy target artifact or enchantment. Its controller gains 4 life.",
  "variants": ["opponent", "friendly", "lost_target", "empty", "wrong_target", "payment", "insufficient_mana", "snapshot", "target_enchantment"]
}
```

Version 2 pump/damage/draw/life declarations can add `setup` with explicit named
battlefield support cards. Entries require `name`, a unique `support_` reference,
`player`, and `zone: "battlefield"`; optional `state: ["tapped"]` prevents a mana
source from paying for an intentionally unaffordable cast. Other zones remain
in shared fixtures or authored cases so recipe zone accounting remains explicit.
For example, Domain tests establish Forest/Island/Plains and declare +3/+3;
Artificer's Epiphany establishes a tapped Sol Ring to avoid its discard branch.
Authors must account for support permanents' abilities in every selected variant.

Use version 2 to test a shared component without parsing free-form rules text.
For example, Adamant Will can reuse the pump scenarios:

```json
{
  "family": "pump_creature",
  "version": 2,
  "params": {"power": 2, "toughness": 2},
  "oracle_text": "Target creature gets +2/+2 and gains indestructible until end of turn. (Damage and effects that say \"destroy\" don't destroy it.)",
  "variants": ["friendly_cleanup", "opponent", "no_creature", "payment", "insufficient_mana", "snapshot"]
}
```

Incinerate uses `damage_any_target` with `params: {"amount": 3}` and its full
pinned Oracle text. Draw and life recipes likewise take `amount`; pumps take
`power` and `toughness`. Existing reviewed numeric ranges still apply.

Pump and damage parameters can also declare coupled `gain_life` (1-20),
`self_damage` (1-4), `lose_life` (1-10), and `draw` (1-4). Life-gain recipes can
declare `draw`. For example, Lightning Helix uses
`{"amount": 3, "gain_life": 3}`; Orcish Cannonade uses
`{"amount": 2, "self_damage": 3, "draw": 1}`; Wildsize uses
`{"power": 2, "toughness": 2, "draw": 1}`. These are authored Oracle expectations,
not parameters inferred from handlers.

Coupled effects check life totals, self targeting, ordered draw identities and
multiplicities, remaining library contents, mana payment, and snapshot replay.
Uncastable or unaffordable spells must not produce secondary effects. A
single-target spell whose target becomes illegal must not produce its secondary
effects either. Draw scenarios provision an ordered seven-card library in empty
fixtures and use the stocked library for snapshot scenarios.

Version 2 pumps additionally support the opt-in `lost_target` variant: cast while
the creature is legal, destroy it while the spell is pending, then resolve.
This fits single-target pumps; multi-target cards need separate authored plans.
It is not added to default version 1 scenarios.

`resolve_dialogs` optionally supplies reviewed zero-based dialog choices during
successful resolution. For example, Volt Charge uses `[0]` for Automatic
proliferate on fixtures with no eligible counters. Choices are queued after the
cast and before resolution, repeated during snapshot replay, and omitted for an
illegal sole target. Unexpected or unconsumed choices still fail. Dialog positions
describe presentation; Oracle text and explicit parameters supply expectations.

Authors must select `variants` explicitly after checking that the card's target
restrictions, extra choices, costs, and side effects fit each scenario. The
generator checks parameter shape/ranges and the full text pin, but deliberately
does **not** infer whether those authored expectations agree with Oracle. Review
that agreement independently of implementation. Normal single-face Instant or
Sorcery and ordinary mana cost restrictions remain; keywords are allowed in this
explicit mode because the author reviews their effects.

Keep separate authored cases in `cases` for the unique abilities: indestructible
and its expiration for Adamant Will, regeneration prevention and its expiration
for Incinerate. A pump or damage declaration alone does not cover those abilities.
Additional effects can invalidate shared assertions (for example drawing an extra
card changes zone counts); choose compatible scenarios or author dedicated cases.

The full `oracle_text` pin is readable review evidence and a refresh guard, not a
parser or a correctness proof. A global wording change requires updating that pin
after review, without changing parameters or recipe grammar. Automatic enrollment
continues using version 1; explicit version 2 components are authored deliberately.

## Batch workflow

Run from the repository root in PowerShell. Inventory is offline and distinguishes
registry categories and shared Oracle identities; identity overlap is not a
first-printing classification.

```powershell
python src/magic/tests/duel/recipe_tools.py inventory --output out/duel-tests/recipe-catalog.json
```

Select a family or an explicit card list. Enrollment defaults to a dry-run plan:

```powershell
python src/magic/tests/duel/recipe_tools.py enroll --family draw_cards --uncovered --limit 100
python src/magic/tests/duel/recipe_tools.py enroll --family draw_cards --uncovered --limit 100 --write
# Or select named cards, including ones with existing authored cases:
python src/magic/tests/duel/recipe_tools.py enroll --cards Divination Harmonize --write
```

Enrollment is idempotent. It uses an existing home or creates the canonical CSVID
card file, preserving authored cases and existing variant selections. Inventory
JSON contains family counts and per-record reasons; give agents selected manual
records, not the entire catalog.

Build using [README.md](README.md), then run recipes through the baseline-aware
runner. Each case has its own process, crash boundary and timeout:

```powershell
python src/magic/tests/duel/run_cases.py `
  --binary out/bin/shandalar-clang-x86/duel_card_tests.exe `
  --manifest out/build/shandalar-clang-x86/src/shandalar/duel_tests/generated/manifest.json `
  --baseline src/magic/tests/duel/data/known_failures.json `
  --filter 'Behavior.Recipe_*' --jobs 4 --quiet --output out/duel-tests/recipe-native
```

For Wasm, use its `.js` binary and manifest, add `--node` with your Node executable,
and choose another output directory. `--jobs` defaults to 1; four is a useful
starting point, subject to memory. `--quiet` prints counts while retaining every
case's log, XML, classification, and observable state. CTest labels these cases
`behavior`, `recipe`, and `recipe_FAMILY`.

Create a small review packet:

```powershell
python src/magic/tests/duel/recipe_tools.py review `
  out/duel-tests/recipe-native/summary-shard-0.json `
  --brief --output out/duel-tests/recipe-review.json
```

Passing and recognized known failures are omitted. New failures and unexpected
passes retain card identity, Oracle ID, recipe/version, exact failure count,
operation names, sample assertions, and full log/XML links. Omit `--brief` for all
assertions. Review buckets are triage aids, not proof of a shared cause. This tool
does not rewrite tests, fix production, or create known-failure entries.

Execution-budget exhaustion has its own `budget` failure kind. Reviewed engine
loops may have exact budget baselines; missing choices and other harness errors
remain unbaselineable harness failures. A baseline must identify the reproducible
case and exhaustion marker; different failures still fail the gate.

## Adding a family

Start with pinned Oracle and meaningful scenarios. Reuse `cast`, `exact_mana`, and
observation builders to avoid repeating choice/stack plumbing. Match whole text,
reject unreviewed grammar and parameter ranges, and add negative parser examples
with restrictions or extra effects. Calibrate against existing authored cards
before enrolling a wider family.

Keep implementation edits out of this workflow. Diagnose failures after
expectations are established; preserve correct assertions and have the integrating
reviewer classify exact failures. Unusual cards continue to use ordinary authored
cases. Never silently discard unsupported wording or infer a rule from a handler.

```powershell
python src/magic/tests/duel/test_recipes.py
python src/magic/tests/duel/test_case_files.py
```

The initial rollout calibrated 69 cases on 12 cards, then generated 267 cases for
43 matching cards. Both runtimes passed 236; 31 exact assertion baselines cover
five spells with missing `card_dummy` implementations. Eighteen card homes were
newly covered. This reduces card-by-card authoring, not the need to review new
recipes or the missing implementation backlog.

The first explicit-component batch adds 631 cases across 100 cards (82 new card
homes): 57 pumps, 29 damage spells, nine draw spells, and five life-gain spells.
Native and Wasm agree: 548 pass, and 83 reviewed failures cover 14 missing
implementations, four overly restrictive target handlers, and six exact-mana
payment loops. Card files mark this as component coverage; additional clauses
still need independent authored cases. Exile-on-death spells omit the shared
lethal variant because its graveyard assertion does not apply to those spells.

The next batch adds 332 cases across 49 more cards (46 new homes), including
coupled life changes and draws, plus opt-in illegal-target pump scenarios.
Both runtimes pass 234. The 98 reviewed failures cover 16 missing implementations,
three more exact-mana payment loops, and Orcish Cannonade remaining in play when
its only target becomes illegal. Its damage and draw are correctly suppressed;
the spell must also leave play for the graveyard.

The destruction/setup batch adds 598 behavior cases across 73 cards (50 new
card homes). Both runtimes pass 551; 47 reviewed failures cover eight missing
spell implementations and five exact-mana payment loops. Destruction variants
exercise target types, ownership, payment, fizzling and snapshot replay, with
explicit life changes where relevant. Domain and Forest-count pumps, plus
artifact-conditioned draw spells, use reviewed support permanents; support mana
sources begin tapped.

Dismantle uses authored scenarios because its handler requests the cast target
twice. Both choices name the same legal artifact; the counter-transfer branch
still needs separate cases. Might of Alara remains unenrolled because its legacy
challenge-unlock gate blocks the current player-zero fixture. Testing its Domain
effect needs explicit unlocked fixture state, rather than a false rules baseline.

The permanent-movement batch adds 509 behavior cases across 52 cards (49 new
card homes). Both runtimes pass 320. The 189 precisely reviewed failures include
the shared slot-versus-internal-ID library insertion bug, eleven missing spell
handlers, source cleanup failures, Wipe Away moving its target during casting,
Cyclonic Rift charging twice when Overload is declined, and four exact-mana
payment loops. Pre-resolution assertions ensure that effects wait for resolution;
the engine's hand flags are deliberately inspected only after resolution because
its pending stack cards can retain their hand flags.

The stat-reduction batch adds 292 cases across 30 cards (18 new homes). Native
and Wasm agree: 234 pass, with 58 reviewed failures. Of these, 23 expose the
shared negative-power query clamp, 26 cover four missing handlers, seven catch
Sudden Death applying its reduction during casting, one catches Blood Lust
remaining in play after its target becomes illegal, and one is Auger Spree's
exact-mana payment loop. The negative-power failures retain signed Oracle
expectations; they identify the common accessor divergence rather than 23
independent card implementation bugs.

The shared-engine fixes add 21 authored regressions for normal and two-generic
hybrid payment, stolen permanents and tokens moving into libraries, and signed
power at combat, life-gain and sacrifice boundaries. Tower Above exercises
`{2/G}{2/G}{2/G}` with six generic mana, three green mana, mixed combinations,
explicit colored-pool clicks toward generic costs, and insufficient payments.
Slitherhead exercises `{B/G}` payment choices and wrong-color rejection.

Library movement now distinguishes instance slots from raw internal IDs, and
top-of-library tutors shuffle the remaining library before placing their selected
cards. Casting consumes the computed six-color cost, and Manalink's produced
mana and declared source-capacity mappings match the host. Power queries retain
negative values; damage, life gain and Berserk's computed bonus keep zero as
their lower bound. AI snapshot restoration also restores the host random stream
without refunding its execution budget. Broader checks exposed and corrected
Seeds of Strength's target loop, Recoil's discard ownership, and normal payment
for awaken spells with no eligible land.

These changes retire 179 known-failure entries. Dramatic Rescue's exact-payment
case now reaches its existing spell-cleanup bug, and Sudden Death's negative
power case still exposes its effect occurring during casting. Their baselines
describe those remaining defects precisely. The Mtenda Herder flanking test
now correctly expects Wall of Stone to have -1 power and deal zero damage.

Both runtimes complete 4,741 behavior cases: 4,334 pass and 407 remain known
failures, with no new failures or unexpected passes under the updated baseline.
All 4,740 completed state observations match between native and Wasm; the
remaining case is the previously recorded Paralyze crash. The 21 new cases pass
in repeated shuffled runs, and all three seeded AI scenarios and all three
harness checks pass on each runtime. Both production game targets also build.

The restricted-destruction batch adds 177 cases using existing recipes across
23 cards, plus 40 authored eligibility checks; 12 card homes are new. All 217
pass in native and Wasm, with identical observable states and no new baselines.
Restriction cases check both controllers: black and artifact creatures, Swamps,
creature size, and Vampire/Werewolf/Zombie exclusions. Normal casting, costs,
lost targets, ownership, life changes, and snapshot replay reuse the shared
recipes. Card files document gaps such as regeneration, cycling, madness,
flashback, threshold, and additional-land permissions.

The automatic exact-text inventory is exhausted: all 43 matches are enrolled.
That inventory reports version 1 matches, not every potential version 2
component. Further expansion requires reviewed explicit parameters or new
recipe mechanics; the remaining Oracle text cannot be enrolled indiscriminately.

The next component batch adds 137 recipe cases and 17 authored cases across 20
cards, including 18 new homes. Variable draws use explicit support permanents
for owned Islands, owned creatures, Birds on both sides, owned Humans, and the
greatest owned permanent mana value. Zero-count cases keep stocked zones and
opposing permanents, checking that unrelated cards do not inflate the draw.
Destruction supplements cover artifact creatures, black creatures, and high
mana values. Storm, conspire, convoke, alternate costs, split second, and counter
immunity remain separate where the card's coverage note says so.

The batch initially exposed five native casting crashes in Mogg Salvage: its
null prompt reached the text loader. Mogg Salvage now supplies its artifact
prompt, generic spell targeting supplies a default for absent prompts, and the
text loader rejects null or empty section names. Two Dismantling Blow cases
exercise the shared fallback directly with exact printed-cost mana. The five
native-only baselines are retired; all 156 cases pass in both runtimes.

The scry batch adds 72 recipe cases and 64 authored cases across ten new cards:
Battlewise Valor, Ferocious Charge, Stand Firm, Bolt of Keranos, Magma Jet, Spark
Jolt, Lose Hope, Lost in a Labyrinth, Opt, and Serum Visions. Main effects reuse
existing recipes. Authored cases check keeping cards, bottoming one or all,
two-card reordering, short/empty libraries where legal, illegal sole targets,
temporary-effect cleanup, and snapshot replay. Opt scries before drawing;
Serum Visions draws before scrying. All 136 cases pass in native and Wasm.

Destruction components accept an explicit `creature_witness` from a reviewed
whitelist and a boolean `target_tapped`. The generator validates witness power,
toughness, colors and relevant keywords against the pinned corpus. These
parameters select legal fixture subjects; they do not infer restrictions from
free-form Oracle text. Authored cases supply the ineligible subjects.

The predicate-destruction batch adds 121 recipe cases and 44 authored cases
across fifteen cards (nine new card homes). It covers flying, defender, black
creatures, tapped/untapped state, power thresholds, and mana-value restrictions.
Power-four, power-three and power-two boundaries have explicit witnesses.
Nine real-response cases change power or tapped state before resolution and
verify the now-illegal target survives. Three Eaten by Spiders cases activate
Equip legally, then check attached Equipment destruction, survival of unattached
Equipment, ownership, and the sole target disappearing before resolution.
Cycling, regeneration and further altered-characteristic scenarios remain
separate where documented by individual card files.

All 165 new cases pass in native and Wasm. Rerunning the 808 existing
destruction recipe cases yields 766 passes and the same 42 separately recorded
unimplemented cases on each runtime. All 973 observed states agree across
platforms; no failure or unimplemented baselines changed.

The creature sweep adds 4,509 generated cases plus three authored Shrine Keeper
payment cases across 437 cards: 299 vanilla and 138 keyword creatures, with 415
new card homes. Both runtimes produce 4,467 passes and 45 precisely recorded
assertion failures: 36 fresh-cast haste cases, six Leatherback Baloth unlock-gate
cases, one Raging Poltergeist flying-block case, and two Shrine Keeper WW-versus-1W
cost cases. Temporary paging-file/process failures pass on low-concurrency retry
and are absent from the baselines. All 4,512 observed states match across
platforms; the existing case manifest entries remain unchanged.

Shrine Keeper's exact payment is authored with a white-pool click to pay the
incorrect imported generic requirement using its legal WW pool. This preserves
a successful cast scenario while separate assertions catch the incorrect color
charge and insufficient-color acceptance. Remove that temporary choice when the
metadata is repaired. Twenty Flash creatures have an explicit timing gap; hybrid
and zero-cost creatures and 231 eligible-syntax keyword no-op records remain
in the deferred inventory. The full suite now has 10,053 behavior cases.
