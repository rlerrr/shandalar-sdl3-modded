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


`counter_spell` version 2 accepts explicit Oracle-reviewed parameters. `targets`
lists fixture spell categories (`creature`, `instant`, `sorcery`, `artifact`,
`enchantment`, `aura`); include `aura` alongside `enchantment` when both are legal.
`colors` optionally restricts to W/U/B/R/G, `colorless`, or `multicolored`.
`value_min`, `value_max`, and `value_exact` restrict mana value. These are authored
parameters, not inferred restrictions from Oracle prose. The full Oracle text pin
requires re-review after an upstream wording change.

Destinations are `graveyard` (default), `exile`, `hand`, `library_top`, and
`library_bottom`. `self_destination: library_bottom` covers Spell Crumple's own
placement. Immediate components are `draw`, `target_draw`, `gain_life`,
`target_lose_life`, `mill`, `scry`, `target_life_from_value`, and
`target_damage_from_power`. `tax` adds decline, pay, exact available payment, and
unaffordable payment scenarios. Draw/life/scry after a tax counter are still
asserted when its controller pays and the targeted spell resolves.

The counter recipe supplies real priority responses while casting the original
spell. It does not dispatch card events manually. A countered original cast
returns false in this engine; a successful tax payment lets that cast complete.
Witness mana costs, colors, types, and mana values come from the pinned Oracle
corpus; engine registry values supply identity only. Generation rejects changed
witness characteristics or the fixture effects used for outcome assertions.

Cases verify a supported spell of every eligible fixture category, own and
opposing spells, restriction negatives, exact source payment, no stack, empty
mana, ordered destinations, immediate effects, and populated-zone snapshot
replay after an AI roundtrip. Dedicated cases are still needed for uncounterable
spells, removing the target during a response, multiple selectable stack
objects, exceptional spell types, and optional/alternative costs. The recipe's
category coverage is a reviewed sample, not proof of every interaction.

The initial counterspell batch adds 529 cases across 54 cards, including 49 new
card homes. Native x86 and Node/Wasm each produce 525 passes and four strict
known-failure results for Halt Order: its handler counters and draws correctly
but leaves the resolving Instant on the battlefield. Assertions retain normal
graveyard placement. All 529 observable outcomes match between runtimes.
The full suite now contains 10,582 behavior cases across 1,431 card records.


`recover_card` version 2 declares `targets` (creature, artifact, enchantment,
land, instant, sorcery), `destination` (hand, battlefield, library_top), and
optionally `graveyard: any` (default: self). Hand and library destinations use
the selected card's owner; battlefield entry uses the caster's control. Optional
restrictions are `colors` (W/U/B/R/G or multicolored) and `value_max`.
Battlefield declarations reject instant/sorcery targets.

Immediate components are `gain_life`, `draw`, `lose_life_from_value`,
`self_destination: exile`, and `counters` (+1/+1 counters). `bonus_subtype` may
specify Angel, Dragon, or Zombie, paired with `bonus_counters` or `bonus_draw`.
Subtype, color, mana value, power/toughness, and mana cost expectations come from
structured pinned Oracle records. Explicit parameters and the full Oracle text
pin still require author review.

Each case stocks both libraries, hands, exile zones, and graveyards with
recognizable alternatives. Negative variants deliberately remove the eligible
cards. Recovery selection uses the full graveyard index, including ineligible
entries. Ordered graveyard/library checks and hand/graveyard multiplicities
cover duplicate identities without depending on hand display slot order.

A real Reclaim response removes the original target before resolution in
`lost_target`, or moves an earlier card in `shifted_target`. The latter checks
that recovery follows the selected card as its index changes. No direct card
events or invented graveyard mutations supply those interactions. Snapshot
replay uses the populated shared fixture, restores after an AI roundtrip, and
repeats the same choices and observations.

Reanimation checks untapped entry, numeric creature characteristics, summoning
sickness, and a later death reaching the owner's graveyard. Reanimate's
opponent-card variant checks that the opponent remains the owner while the
caster gains control. Empty zones, wrong types/colors/values, wrong graveyard,
exact payment, and insufficient mana are checked when applicable. Optional
abilities and alternate costs, multi-target recovery, graveyard shroud,
replacement effects, and exceptional card types require separate cases.


`target_witness` optionally selects an eligible primary witness by full card name
from the reviewed whitelist. It changes fixture selection, not the declared
Oracle restrictions. Every applicable `eligible_type_*` case also checks casting
with only that legal card type available in the graveyard. Nature's Spiral and
Treasured Find currently reject noncreature choices, so their noncreature
resolution variants are replaced by these isolated eligibility assertions until
the card bugs are fixed. Creature resolution scenarios still cover their normal
costs, destinations, responses, and snapshots.

An all-targets-illegal spell does not execute its own self-exile instruction.
The lost-target expectation for Treasured Find therefore puts the spell in its
graveyard, while valid resolutions exile it. This expectation is independent of
its current handler's unconditional self-exile.


The initial recovery batch adds 534 cases across 41 cards (23 new card homes).
Native x86 and Node/Wasm each produce 524 passes and ten strict assertion
expectations: three confirmations of the existing Nature's Spiral permanent-type
restriction, five Treasured Find noncreature-target failures, one Treasured Find
self-exile failure when its sole target leaves, and one Call to the Netherworld
missing black-color restriction. No scripted-choice, crash, or resource failures
are accepted as baselines. All 534 observable outcomes match across platforms.

Rerunning the 70 existing authored cases for these cards produces 66 passes and
the same four pre-existing Nature's Spiral failure expectations in both runtimes.
The ten new expectations also pass their strict classification checks in fresh
processes. Original manifest entries remain unchanged. The full suite now has
11,116 behavior cases across 1,454 card records.

### Targeted damage components (`damage_spell`, version 2)

Declarations pin the full Oracle text and supply explicit damage amount and
creature/player target classes. Optional components describe color exclusions
or alternatives, flying, opponent ownership, attacking/blocking status,
life gain, controller damage, scry (including your-turn-only scry), increased
artifact damage, same-name damage, and exile-on-death duration. Printed normal
mana payment comes from structured Oracle data. New cards require an explicit
reviewed declaration; this family does not infer parameters from prose.

Every board stocks both players' hands, ordered libraries, graveyards, and exile.
Exact payment, insufficient mana, unselected witnesses, legality, lethal and
surviving damage, cleanup, and AI/snapshot replay are checked where applicable.
An actual Unsummon response removes the last creature target before resolution;
coupled life gain, damage, and scry must then fizzle. Combat-only targets are
declared through the normal attacker/blocker sequence. A creature removed from
combat before resolution becomes illegal. Additional cases check artifact damage,
duplicate names on both sides, both player targets, scry to the bottom, and
exile for a later death in the same turn versus ordinary death after cleanup.
Indestructible witnesses must survive lethal damage without being exiled.

Regeneration follows `data/legacy_exceptions.json`: a scripted activation during
the destruction window must tap the creature, heal damage, and replace death.
Exile-on-death effects do not prohibit this replacement. `prevent_regeneration`
omits the ordinary regeneration success scenario and adds a cleanup-expiration
scenario using a legally pumped Wall of Bone. The latter checks the creature
survives cleanup and loses its pump and damage before testing regeneration again.
It does **not** establish prohibition during the affected turn; that requires a
separate scenario that observes the legal responses in the destruction window.

These are component tests. Planeswalker targets, flashback, cycling, landcycling,
madness, surge and other alternate costs, uncounterability, damage prevention,
and attachment destruction remain explicit gaps where present. Passing a recipe
is not a claim that every Oracle clause or every timing interaction works.

The first targeted-damage batch adds 417 cases across 41 cards (36 new card
homes). Native x86 and Node/Wasm each produce 416 passes and one strict
regression expectation: after Rage of Purphoros's duration expires, Wall of Bone
survives cleanup but dies despite activating regeneration in the later
destruction window. Engulfing Flames passes the equivalent scenario. Correct
Oracle expectations remain in the case; the production fix is deferred.
The suite now contains 11,533 behavior cases across 1,490 card records.

All 417 observations agree across runtimes, and all 14 existing authored cases
for these cards still pass. The generated native/Wasm inputs match, and every
pre-existing manifest entry is unchanged. Recipe, loader, and runner guard tests
also pass (39, seven, and nine cases respectively).

### Mixed-board sweeps (`sweep`, version 2)

Explicit parameters select `damage` or `destroy`, affected permanent types,
controller scope, and the amount of damage. Reviewed predicates include colors,
excluded colors/types, flying, tapped state, basic-land subtypes, and nonbasic
lands. Damage may affect all players or opponents, gain life from player damage,
scry, and replace deaths with exile for the turn. Destruction may gain or lose
life per creature actually destroyed, including losses applied to each creature's
controller. Full Oracle pins require re-review when the source text changes.

Both players receive the same 23-permanent board: vanilla creatures in all five
colors; flying and indestructible creatures; artifact, enchantment, and land
creatures; mana rocks; enchantments in all five colors; basic/nonbasic lands; and
an indestructible artifact land. There are no passive power/toughness bonuses or
damage-prevention abilities on this board. Hands, ordered libraries, graveyards,
and exile are stocked independently. Additional variants use one side only,
empty battlefields, intentionally empty zones, uniformly tapped/untapped boards,
exact payment, insufficient mana, cleanup, and AI/snapshot restoration.

Type overlaps are counted once. The assertions distinguish damage from
destruction, preserve indestructible witnesses, and compute death-dependent
life changes from actual Oracle-eligible, destroyable creatures. A real Lightning
Bolt supplies pre-existing damage in injury scenarios. Exile variants destroy a
surviving damaged creature later in the same turn and after cleanup to test the
replacement's duration. Graveyard identities and multiplicities are checked
without assuming an arbitrary order for simultaneous deaths; exile contents and
ownership are also checked, while library ordering is asserted exactly.

Insufficient-mana variants omit battlefield mana sources so automatic payment
cannot legitimately fund the spell. Reviewed `cast_dialogs` choose the normal
cost for awaken and alternate-cost prompts, and are recorded in provenance and
replayed after restoration. A sole legal opponent is selected automatically;
the script does not invent a player-selection prompt for Simoon or Rain of
Daggers. Player hexproof and target cancellation require separate scenarios.

The full boards exceed the ordinary 200,000-work-unit test allowance during
finite simultaneous-damage processing. Sweep cases explicitly use the bounded
`set_budget` action with 2,000,000 units. Generation and the C test interface
reject values outside 200,000–2,000,000. It preserves RNG, time, and decision
state; snapshots capture the allowance too. Existing cases keep their original
limit, and process timeouts remain in force. Budget failures are not accepted as
card-regression expectations for this batch.

This batch adds 687 cases across 60 cards (45 new card homes). Native x86 and
Node/Wasm agree on 663 passes and 24 exact assertion expectations. Those 24
scenarios expose one shared symptom in Anger of the Gods and Yamabushi's Storm:
lethal marked damage wrongly exiles indestructible creatures. Correct survival,
characteristic, battlefield-count, and exile-count assertions remain. Ordinary
damage sweep controls preserve these creatures. Production fixes are deferred.

These are reviewed components, not whole-card certification. Regeneration
prohibition, uncounterability, flashback/cycling, awaken and alternate-cost
effects, planeswalker destruction, replacement/prevention effects, and unusual
timing restrictions require additional authored cases where applicable. The
suite now contains 12,220 behavior cases across 1,535 card records.

All 43 pre-existing authored cases for these cards pass in both runtimes.
Magma Spray and Reduce to Ashes still pass their indestructible controls, and
the earlier Rage of Purphoros cleanup failure retains its exact expected
signature. All 687 new and 46 regression/control observations agree across
platforms. Generated inputs match, all prior manifest entries are unchanged,
and recipe/loader/runner guard tests pass (45, seven, and nine).

### Board-wide temporary stats (`sweep`, version 2)

`effect: "stats"` requires explicit signed `power` and `toughness` changes and
creature-only targets. Scope may be all creatures, yours, opponents', or a
target player's. Color predicates and the reviewed non-Elf predicate use pinned
Oracle characteristics. Separate `opponent_power`/`opponent_toughness` values
describe asymmetric effects such as Zealous Persecution. Optional components
cover untapping, drawing, scrying, life gained per death, and turn-long exile
replacement. Full Oracle text remains pinned; these parameters describe a
reviewed component rather than infer expectations from implementation code.

The shared mixed board checks exact signed stats before and after resolution,
unchanged noncreatures and excluded creatures, lethal toughness reductions,
and pre-existing damage. Indestructible creatures survive lethal damage but
still die at zero toughness. Cleanup restores surviving creatures' printed stats
and removes damage. Two casts test cumulative bonuses/reductions and cleanup;
creatures legally cast afterward must enter with their printed stats. Additional
death scenarios distinguish replacements/triggers applying throughout the turn
from stat changes affecting only the creatures present at resolution. Populated
owned zones, exact payment, insufficient mana, and AI/snapshot replay remain
part of these cases. Draw variants retain a nonempty library; they do not claim
coverage of losing the game by drawing from an empty library.

Normal casting also supports single-face Arcane instants/sorceries. Reviewed
`cast_dialogs` decline optional kicker/splice or choose the printed awaken cost.
`repeat_cast_dialogs` can override choices for the second stacking cast, since
the first cast may offer splicing the other copy but the second cannot. Generic
mana for two expensive casts is explicitly stocked from their printed Oracle
costs rather than relying on manual conversion of colored mana pools. Kicker
checks include available battlefield mana sources when deciding whether a
decline prompt will appear, even in exact-payment variants.

This batch adds 803 cases across 57 cards, including 49 new card homes. Native
x86 and Node/Wasm agree on 791 passes and 12 strict assertion expectations:
11 Zealous Persecution scenarios show opposing creatures receiving +1/+1
instead of -1/-1; one Flaying Tendrils scenario shows a creature entering later
going to the graveyard instead of exile when it dies that turn. Correct Oracle
assertions remain; production fixes are deferred. All 22 pre-existing authored
cases for these cards pass. Three earlier damage controls also retain their
results, including the strict Rage of Purphoros regeneration failure.

These cases do not certify keyword grants/combat, alternate costs, kicker
effects, cycling, flashback, madness, awaken, spell mastery, or vehicle animation
where present. Such clauses need separate authored cases. All 803 observations
and 25 existing/control observations agree across runtimes; generated inputs
match and all 29,434 prior manifest entries remain unchanged. Recipe, loader,
and runner guard tests pass (52, seven, and nine). The suite now contains
13,023 behavior cases across 1,584 card records.

### Temporary creature abilities (`grant_creature`, version 2)

Explicit `power`, `toughness`, and `keywords` describe a reviewed temporary
grant. The entire Oracle text is pinned. Optional components declare controller
restrictions, drawing, untapping, life gained from toughness, damage prevention,
death-trigger life loss, and mandatory blocking. Expectations come from those
reviewed declarations and pinned Oracle witnesses; registry handlers supply
identity only. This family currently supports nonnegative stat changes.

Combat scenarios use casting, stack resolution, attacker declaration, blocker
declaration, combat advancement, and damage. Flying is checked against ground
and reach blockers; flying/reach recipients must also block flyers. Trample,
first/double strike, lifelink, and deathtouch check actual damage and deaths.
Indestructibility distinguishes destruction, lethal damage, and zero toughness.
Hexproof/shroud distinguish opposing and owner targeting. Haste cases legally
cast a creature first, then compare grants, no grant, and ability removal.
Expiry controls repeat relevant interactions after cleanup on the next turn.

Fixtures retain populated owned hands, libraries, graveyards, and exile, with
exact payment, illegal targets, empty zones, and snapshot/AI replay checks.
Lost-target cases cast Unsummon as a real priority response; a spell with every
target illegal must also skip its draw component. Mandatory blockers are chosen
through the effect's trigger before damage, without assigning the blocker twice.
Optional costs use reviewed `cast_dialogs`; `cast_target_first: true` describes
a handler that requests its target before that dialog.

For an existing targeting defect, `eligibility_only` explicitly identifies
opposing-target variants that only assert casting eligibility with one legal
opposing creature. This retains the correct Oracle assertion without submitting
an illegal host choice. Resolution/combat coverage for those variants remains a
documented gap until the defect is fixed. It is not a general failure bypass:
only selected opposing-target grant variants accept this field.

Extra clauses such as ferocious trample, alternate costs, overload, flashback,
kicked bonuses, madness, and split-second response restrictions require separate
authored cases. These recipes certify components, not whole cards.

This batch adds 971 cases across 63 cards, with 16 first card homes and ability
coverage for 47 existing homes. Native x86 and Node/Wasm produce equivalent
observable states: 943 passes and 28 strictly matched assertion failures.
Four fresh-creature haste grants cannot attack; seven cards have incorrect
controller restrictions; Stonewood Invocation applies its bonus during casting
rather than resolution. Correct expectations remain and production fixes are
deferred. The restrictions comprise nine positive eligibility failures and three
negative eligibility failures; Stonewood contributes 12 timing scenarios.

All 323 earlier cases and controls for this selection retain their results
(314 passes, nine known failures), with matching observations across runtimes.
All 30,237 prior manifest entries and all prior card cases/recipes are preserved.
The suite now has 13,994 behavior cases across 1,600 card records. Recipe guards,
including 11 grant-specific checks, and loader/runner guard tests pass.

### Continuous creature Auras (`aura_creature`, version 2)

This family shares combat and owned-zone checks with temporary grants, while
explicitly modeling an Aura's different lifetime. It requires a normal creature
Aura, full pinned Oracle text, explicit nonnegative stats and keywords, and
reviewed scenarios. Empty keyword lists allow pure stat Auras. Snow Auras with
ordinary printed casting costs are supported; snow activation costs are not.

The Aura is cast from hand and resolves attached to the chosen creature.
Bonuses persist through cleanup. Disenchant removal and Disperse bounce must
immediately remove the bonus. Bouncing or destroying the host puts the Aura
into its owner's graveyard; `return_aura: true` instead expects its printed
graveyard-to-hand trigger. A target lost in response makes the Aura spell go
to the graveyard without entering, drawing, gaining life, or firing that return
trigger. These use real casting, priority responses, death processing, and
zone transitions, rather than assigning attachment state directly.

`draw`, `gain_life`, and `lose_life` describe reviewed entry effects. Vigilance
checks the actual attack tap obligation. Other declared keywords use the same
combat probes as temporary grants. `expired_*` Aura cases remove the Aura with
Disenchant before cleanup and later combat; cleanup alone does not expire an
Aura. Opposing combat cases cast on the caster's main phase before moving to
the enchanted creature controller's turn, preserving Aura ownership and
lifelink ownership separately. Fresh haste recipients are legally cast first.

These cases certify the declared continuous and entry/return components.
Additional activations, alternative costs, cycling, dredge, madness, aura swap,
and graveyard-return activations need separate authored cases. Umbra armor,
conditional/scaling bonuses, tokens, and other automatic triggers require
additional modeling and are excluded from this initial selection.

`cast_target_count: 2` scripts Fearsome Temper's current duplicate casting
prompt with the same intended host twice. The handler invokes two Aura helpers;
this presentation defect is documented in its card file. The effect assertions
remain Oracle-based. Other cards retain the default single target choice, and
the field only accepts a reviewed Aura sequence of one or two prompts.

This batch adds 1,430 cases across 76 cards, including 64 first card homes.
Native x86 and Node/Wasm agree on 1,405 passes and 25 strict known failures:
six fresh-creature haste grants, thirteen Pursuit of Flight stat/combat cases,
one Unflinching Courage trample/lifelink case, and five lost-target aborts.
Pursuit's always-true event condition bypasses its +2/+2 handling. The blocked
4/4 with Courage gains seven life instead of four. Savage Silhouette, Holy
Armor, Lightning Prowess, Stonehands, and Rime Transfusion abort with
`get_card_instance(-1, -1)` after their target is bounced in response.
Correct assertions remain; production fixes are deferred. Crash cases retain
their fizzle assertions beyond the fatal error and match the exact engine
diagnostic, not any arbitrary trap.

All 1,425 completed-case observations match across platforms, as do the five
fatal engine diagnostics. All 45 earlier Aura/shared controls retain their
results (41 passes, four known failures). All 31,208 prior manifest entries
and all prior authored cases/recipes are unchanged. The suite now contains
15,424 behavior cases across 1,664 card records. Nine Aura, eleven grant,
52 general recipe, seven loader, and nine runner guard tests pass.

`aura_control` v2 adds signed continuous stat changes and attack, block, and
activation restrictions. Parameters are explicit `power`, `toughness`, empty
`keywords`, and `scope` (`creature`, `permanent`, `nonland`, or
`artifact_or_creature`). Optional true flags are `no_attack`, `no_block`,
`no_activate`, `mana_exception`, `combat_prevent`, and `return_aura`;
`gain_life` declares an entry life amount. The full Oracle text and enchant
scope are pinned; costs still come from structured Oracle data.

Negative toughness uses a surviving Scaled Wurm and a separate lethal Suntail
Hawk. Negative power remains signed in observations and deals zero combat
damage. Restrictions use real attacker/blocker declaration and activation,
then legal Disenchant removal proves the action is permitted again. Unaffected
creatures are positive controls. Furnace Whelp tests nonmana activation;
Llanowar Elves tests mana activation, including Faith's Fetters' exception;
negative-toughness Auras use a surviving Wirewood Elf instead.
Both players' attachments, populated zones, target loss through a real priority
response, cleanup persistence, Aura/host bounce and death, exact payment, and
snapshot replay are checked. Threshold, upkeep triggers, transformations,
retargeting, and undeclared color/subtype effects remain explicit coverage gaps.

CTest's `duel.recipes` discovers every `test_*recipes.py` guard module, including
the grant and Aura families, so those authoring safeguards also run in CI.

The first restriction batch adds **880 cases across 40 cards**, including
**34 new card homes**. Native x86 and Node/Wasm agree on **868 passes and 12
strict known engine crashes** after the sole Aura target is bounced in response:
Pacifism, Bound in Silence, Cage of Hands, Forced Worship, Kirtar's Desire,
Demonic Torment, Pillory of the Sleepless, Suppression Bonds, Bound by Moonsilver,
Choking Restraints, Revoke Privileges, and Caught in the Brights. Each baseline
requires its case identity and exact `get_card_instance(-1, -1)` engine error;
correct target-loss and zone assertions remain. Engine fixes are deferred.

All 868 completed-case states and all 12 fatal diagnostics match across
platforms. All 30 earlier authored/shared controls retain their results (25
passes, five known failures), and all 32,638 prior manifest entries and prior
authored cases/recipes are preserved. The suite contains **16,304 behavior
cases across 1,698 card records**. The 81 recipe, seven loader, and nine runner
guards pass. Coverage remains limited to the declared components.

`self_pump_creature` v2 covers creatures with unconditional supported keywords
and activated abilities, with an explicitly declared mana-only self-pump mode.
Parameters are `keywords`, `power`, `toughness`, and `activation_cost` (a mana
symbol string), plus optional `activation_limit` of two or three. Base stats and
casting costs come from structured Oracle fields. The full text is pinned and
the selected simple pump clause must agree with the declared parameters. Other
passive/triggered clauses require separate scenarios; alternate self-reference
wording requires review. Other activated modes remain documented gaps.

Casting, keyword combat, fresh attackers, and all activation scenarios use
stocked zones. Activation tests check payment immediately, unchanged stats
while pending, bonuses after resolution, individual and queued repetitions,
five activations when unlimited, tapped and freshly cast hosts, opposing-turn
and in-combat activation, source bounce in a real priority response, cleanup
expiry, and limit/reset where printed. Resolved dynamic effects are compared
after snapshot/AI backup restoration and replay. Unselected creatures and both
players' ordered libraries, graveyards, exile, hands, life, mana, and battlefield
counts provide controls.

`activation_dialogs` maps selected variants to reviewed zero-based menu indices;
each activation replays that choice. Exact-payment variants can omit a menu
when only one mode is affordable and the handler auto-selects it. A different
printed ability may allow activation even without enough mana for the tested
pump, so global affordability negatives are limited to single-ability cards.
These decision scripts adapt presentation only, never expected Oracle effects.

`activation_cancel_mana` names reviewed exact-payment variants that cancel an
erroneous payment prompt. It is currently used only for Windreaver's request
for blue mana when its printed pump costs white. The case still expects a
successful activation, white payment, and +0/+1. Remove this script when fixing
the handler; unused choices fail explicitly. The headless host uses the normal
no-target cancellation contract, and fixtures start with no UI phase shortcut.

The first self-pump batch adds **1,917 cases across 82 implemented cards**,
including **69 new card homes**. Native x86 has **1,889 passes and 28 strict
known failures**; Node/Wasm has **1,900 passes and 17 strict known failures**.
Shared failures cover five fresh haste attackers, Truefire Paladin's missing
vigilance, and Windreaver's one-based menu result being treated as zero-based
for payment and resolution. Correct Oracle assertions remain in every case.

Unyaro Bees has **11 native-only activation exceptions** with SEH code
`0xc0000005`. A missing comma in its variadic `DIALOG` call merges the first
option label into a flag and leaves a legality integer where the parser expects
a pointer. All 11 equivalent Wasm scenarios pass. These expectations are
explicitly native-only and match the precise exception, case identity, and
activation step; an arbitrary crash or Wasm failure cannot satisfy them.
Card/engine fixes are deferred.

All **1,906 completed shared-case states** match across platforms. The 56 earlier
authored/shared controls retain their results: 51 passes and five known failures
on each platform. All 33,518 prior manifest entries and prior authored
cases/recipes are preserved. The suite contains **18,221 behavior cases across
1,767 card records**. The 91 recipe, seven loader, and nine runner guards pass.
Coverage remains limited to declared components, including populated zones,
activation timing and payment, stack resolution, repetition, cleanup, combat,
and snapshot replay; other printed abilities require separate cases.

`evasion_creature` v2 declares explicit `keywords` and optional true
`unblockable`. The full Oracle text is pinned. It accepts supported unconditional
keywords plus fear, intimidate, shadow, horsemanship, and the five basic landwalk
abilities; other activated paragraphs remain documented gaps. Unreviewed passive
or triggered clauses and combinations needing menace, defender, trample, or
deathtouch combat scenarios are rejected. Base stats, casting costs, and colors
come from structured Oracle fields. Engine handlers never supply expectations.

The blocker matrix includes all five colors, a colorless artifact, a colored
artifact, a colorless nonartifact, flying, reach, shadow, flying plus shadow, and
horsemanship. Defending scenarios check the opposite direction of these rules.
For example, horsemanship limits what can block the attacking creature, while
shadow limits both directions. Unblockability does not prevent its creature
from blocking a normal attacker. Normal combat declaration, damage against a
denied blocker, opposing ownership, persistence after cleanup, casting/payment,
and combat snapshot/AI replay all use populated zones.

Basic landwalk scenarios distinguish defending lands from lands in hands or the
attacking player's battlefield. Matching, wrong, tapped, attacker-owned, and
opposing-player setups are checked. A real Boomerang cast removes the defending
land before blockers and must restore legal blocking. Nonbasic land types,
land-type changes, other activated modes, and flash timing require further cases.

The first evasion batch adds **1,871 cases across 58 implemented cards**,
including **54 new card homes**. All 1,871 pass on native x86 and Node/Wasm,
with identical observable states and no new known-failure entries. All 35
earlier authored/shared controls retain their results on both platforms: 28
passes and seven known failures. All 35,435 prior manifest entries and the 15
prior authored cases in the selected card files are unchanged.

The suite contains **20,092 behavior cases across 1,821 card records**. The 101
recipe, seven loader, and nine runner guards pass, along with 13 representative
CTest entry points on each platform. Native, Wasm, and fresh generated files
are identical. These remain component tests, with the limitations above.


`defensive_creature` v2 declares explicit sorted `keywords` and, when present,
a sorted `protection` array. Supported protection categories are the five colors,
creatures, artifacts, monocolored, and multicolored. Full Oracle text is pinned;
unreviewed passive/triggered clauses, subtype protection, compound protection
clauses, and combined mechanics needing further combat scenarios fail closed.
Other activated paragraphs are recorded as gaps, rather than certified by these
component tests. Base characteristics, colors, and mana costs use structured
Oracle fields. Witness characteristics and complete rules text are pinned too.

```json
{
  "family": "defensive_creature",
  "version": 2,
  "params": {"keywords": ["First strike"], "protection": ["black"]},
  "oracle_text": "First strike (This creature deals combat damage before creatures without first strike.)\nProtection from black (This creature can't be blocked, targeted, dealt damage, or enchanted by anything black.)",
  "variants": ["target_shrink_0", "defend_black", "wrath_1"]
}
```

Targeting probes use six real spells covering all five colors and a multicolored
spell. The suffix on `target_<effect>_<owner>` selects the creature's controller;
the scripted human player casts in both cases. This separates friendly from
opposing targeting without asking the AI to make strategically undesirable
plays. Hexproof permits friendly targeting; shroud rejects both. Protection
checks the spell's colors, independently from whether the spell pumps, bounces,
shrinks, damages, exiles, or destroys. Illegal targeting leaves the spell in hand
and mana untouched. Legal casts check payment, stack presence before resolution,
resolution, ownership, zone contents, and temporary-effect expiry.

Combat probes distinguish blockers of each color, artifacts, multicolored
creatures, and colorless nonartifacts. A flying artifact supplies a separate
flying blocker. Protected creatures can block matching attackers; their incoming
damage is prevented. Hexproof and shroud do not prevent combat damage. Damage
probes account for first strike, indestructibility, creature deaths and cleanup.
Wrath of God and Pyroclasm exercise untargeted destruction and damage with
additional creatures on both sides. Indestructible hosts also receive repeated
Disfigure casts until toughness reaches zero. A combat snapshot replays after an
AI backup/restore round trip. Every case uses populated owned zones and ordered
libraries.

Protection against enchanting, targeted creature/artifact abilities, sacrificing,
dynamic gain/loss of defensive abilities, flash timing, and untested activated
modes remain gaps. These are component tests, not whole-card certification.

The first defensive batch adds **2,055 cases across 47 implemented cards**,
including **45 new card homes**. Native x86 and Node/Wasm each report **2,043
passes and 12 known failures**, with all 2,055 observable states identical.
The failures expose missing targeting restrictions on Guardian of the Guildpact
(ten monocolored-spell permission probes) and Enemy of the Guildpact (two
Terminate permission probes). Their handlers implement blocking and damage
protection but omit targeting. Correct assertions remain, with exact case,
step, assertion-count, and actual/expected value matching. The forbidden casts
are not attempted, and production fixes are deferred.

All 36 prior authored/shared controls retain their results on both platforms:
29 passes and seven known failures. All 37,306 prior manifest entries and the
14 authored cases in the selected card homes are preserved. The suite contains
**22,147 behavior cases across 1,866 card records**. The 111 recipe, seven loader,
and nine runner guards pass, as do 15 representative CTest entry points per
platform. Native, Wasm, and fresh generated files are identical.


`mana_creature` v2 declares explicit `keywords`, `activation_cost`,
`mana_outputs`, and `production` (`fixed`, `choice`, or `any_color`). Outputs
are strings of one to three ordinary mana symbols; simultaneous production is
one output string, while a choice supplies distinct alternatives. Any-color
production declares exactly the five colors, excluding colorless. Full Oracle
text is pinned. Only a single tap-for-mana ability with an optional ordinary
mana input, supported unconditional keywords, numeric base characteristics,
and a normal single-face creature layout qualifies. Extra abilities, triggers,
variable quantities, restrictive spending clauses, and unreviewed costs reject
enrollment. Expectations use Oracle parameters, not engine handlers.

```json
{
  "family": "mana_creature",
  "version": 2,
  "params": {
    "keywords": [],
    "activation_cost": "{1}, {T}",
    "mana_outputs": ["{R}{G}{W}"],
    "production": "fixed"
  },
  "oracle_text": "{1}, {T}: Add {R}{G}{W}.",
  "variants": ["payment_0", "spend_0", "spend_generic_0", "mana_snapshot_0"]
}
```

Each output gets exact pool assertions for both players, immediate resolution
with no stack residue, exact input payment, rejection of a second use while
tapped, opposing-turn activation, tapping a declared blocker without removing
its block, spending on colored and generic-cost creatures, real Vitalize
untapping and reuse, and caller-owned snapshot replay after AI backups. The
spending fixtures begin short of the spell's cost: newly produced mana must
fund a successful cast, and pools are checked before and after payment.
Colored mana used for generic costs has explicit mana-pool click decisions.
Vanilla witness creatures use normal core casting rather than interpreting
their no-op handler's EVENT_CAN_CAST return as a permission result.

Separate cases check tapped and sick hosts, sickness after an actual fresh cast,
insufficient and wrong-color input costs, opposing ownership for fixed outputs,
and vigilance allowing activation after attack declaration where present.
Vitalize must untap the source and a friendly witness while leaving an opposing
tapped witness alone. Zones remain populated and ownership and library order
are checked. Ordinary casting/payment/snapshot and basic combat components are
retained as well.

Other printed keyword interactions, restricted mana, replacement and trigger
interactions, variable output, additional abilities, flash timing, and AI color
selection remain gaps. These are component tests, not whole-card certification.

The first mana-creature batch adds **1,266 cases across 41 implemented cards**,
including **30 new card homes**. All 1,266 pass on native x86 and Node/Wasm,
with identical observable states and no new known-failure entries. All 96
prior authored/shared controls retain their results on both platforms: 88
passes and eight known failures. All 39,361 prior manifest entries and the 36
authored cases in the selected card homes are preserved.

The suite contains **23,413 behavior cases across 1,896 card records**. The 122
recipe, seven loader, and nine runner guards pass, along with 15 representative
CTest entry points on each platform. Native, Wasm, and fresh generated files
are identical. Coverage remains limited to the declared components above.


### Mana-only self regeneration

`regenerate_creature` pins the whole Oracle text and explicitly declares the
ordinary activation cost, unconditional keywords, and intentional legacy timing:

```json
{
  "family": "regenerate_creature",
  "version": 2,
  "params": {
    "keywords": ["Defender", "Reach"],
    "activation_cost": "{1}{B}{G}",
    "timing": "pre_sixth_destruction_window"
  },
  "oracle_text": "Defender, reach\n{1}{B}{G}: Regenerate this creature.",
  "variants": ["regeneration_payment", "combat_block", "regeneration_snapshot"]
}
```

The gate rejects extra abilities, conditions, alternate costs, triggers,
unsupported reminders, and cards without fixed numeric stats. It accepts the
reviewed regeneration reminder and both named and `this creature` self-reference.
Scryfall's `Heal` and `Regenerate` tags are checked separately from unconditional
combat keywords; they do not supply expectations.

`choose_regeneration` scripts a normal priority activation only when the real
legacy destruction window opens. It passes earlier spell-response and damage
prevention windows without consuming the queued choice. This is a headless
host decision, not a forced event or state mutation. The decision and its timing
constraint are included in caller-owned snapshots. Existing `choose_priority`
behavior is unchanged.

Cases check exact payment (including mixed colors and generic costs), repeated
uses, a second destruction after spending the last mana, tapped and sick hosts,
opposing-turn use, missing or wrong-colored mana, and destruction without a
response. Lightning Bolt casts create lethal damage; Shock creates nonlethal
and pre-existing damage. Regeneration must tap, heal, preserve the creature,
and remove it from combat. Two legal Giant Growth casts construct large combat
witnesses that survive first strike and are lethal to every admitted source;
ground attackers and flying blockers avoid accidental evasion mismatches.
Terminate tests prohibition, and Last Gasp tests non-destruction death at zero
toughness where applicable. Snapshot cases capture a real pending lethal spell
and its unconsumed destruction-window decision, then resolve, exercise AI
backups, restore, and resolve again with identical results. Cleanup reuse is
covered, with populated zones, ownership, and ordered libraries checked.

Modern proactive shields differ from this engine's intentional pre-Sixth
Edition timing. Every enrolled card references that precise exception in
`data/legacy_exceptions.json`; the legacy main-phase rejection case is identified
there. Survival, costs, healing, and removal from combat retain Oracle behavior.
Other keyword interactions, flash timing, hexproof targeting, control changes,
sacrifice, exile, and AI response policy remain gaps. These are component tests.


The first regeneration batch adds **1,296 cases across 44 implemented cards**,
including **35 new card homes**. Native x86 and Node/Wasm each report 1,295
passes and one known failure: Manor Skeleton reproduces the existing fresh-cast
haste attack rejection. Correct Oracle attack, attacking-state, and tap
assertions remain, with an exact three-assertion baseline. No production fix is
included. All 109 prior authored/shared controls retain their results on both
platforms: 101 passes and eight known failures. All 40,627 prior manifest entries
and 40 authored cases in the selected homes are preserved.

The suite contains **24,709 behavior cases across 1,931 card records**. The 134
recipe, seven loader, and nine runner guards pass, as do 15 representative CTest
entries on each platform. Observable states match for all new cases and controls;
native, Wasm, and fresh generated files are identical. The 44 snapshot cases
also replay pending lethal spells and queued regeneration decisions across AI
backups. Coverage remains limited to the declared components above.


### Entry draw and life effects

`etb_value_creature` uses explicit `keywords`, `draw`, `gain_life`, and
`lose_life` amounts with a pinned whole Oracle text. It admits one unconditional
entry trigger and reviewed unconditional keywords, with ordinary casting costs
and fixed numeric stats. Additional triggers, modes, optional effects, alternate
costs, hybrid payment, and other mechanics require dedicated cases. Named or
`this creature` self-reference and the older `enters the battlefield` wording
are supported without inferring amounts from implementation code.

```json
{
  "family": "etb_value_creature",
  "version": 2,
  "params": {"keywords": [], "draw": 2, "gain_life": 0, "lose_life": 2},
  "oracle_text": "When this creature enters, you draw two cards and you lose 2 life.",
  "variants": ["cast", "two_copies", "blink_snapshot", "countered"]
}
```

Cases exercise ordinary and exact-payment casts, wrong-colored and missing
mana, injured life totals, both controllers, two copies, and summoning sickness.
Entry triggers must draw the top ordered cards or change the correct life total.
Hands and graveyards are checked for card multiplicity, so pre-existing copies
cannot hide an incorrect draw. Both players keep meaningful hands, graveyards,
exile cards, and ordered libraries. Explicit empty-hand, empty-graveyard,
empty-exile, and final-card-library variants test zone boundaries.

Real Unsummon and recasting, and real Cloudshift exile and return, must each
create a new entry. `bind_hand` finds the returned card by CSVID without assuming
that bounce reuses its original instance slot. Like `bind_permanent`, it rejects
ambiguous matches and supports an explicit zone index. The opponent-bounce
variant uses a human-controlled caster to avoid AI strategic refusals of legal
self-bounce. Creature counter cases queue an interrupt before casting opens the
response window; the existing counter helper selects the top spell, and the
countered cast returns failure with no entry effect.

Leaving via bounce or death, another creature entering, and cleanup must not
repeat the source's entry effect. Pre-cast and pending-creature-spell snapshots,
and pending blink spells, replay after AI backups with identical observable
results. Battlefield fixture loading runs entry effects too; combat cases assert
that setup receipt before declarations and verify unchanged zones and entry
life effects afterward. Other printed keyword interactions, optional or variable effects,
multiple simultaneous triggers, draw replacements/prevention, empty-library
loss, alternate costs, and AI policy remain gaps. These are component tests.


The first entry-value batch adds **1,079 cases across 36 implemented cards**,
including **34 new card homes**. All cases pass on native x86 and Node/Wasm,
with identical observable states and no added known failures. All 38 prior
controls retain their results on both platforms: 29 passes and nine known
failures. All 41,923 prior manifest entries and eight authored cases in the
selected homes are preserved.

The suite contains **25,788 behavior cases across 1,965 card records**. The 147
recipe, seven loader, and nine runner guards pass, along with 15 representative
CTest entries per platform. Native, Wasm, and fresh generated files are
identical. General-test build flags are restored to their previous settings.
Coverage remains limited to the declared components above.


### Blocking restrictions

`blocking_creature` declares `keywords`, `restriction`, and `power_limit`
explicitly, alongside the pinned whole Oracle text and reviewed variants.
`restriction` accepts `none` (no blocking allowed), `flying_only`,
`power_at_least`, or `power_at_most`. The latter two prohibit blocking at and
beyond the stated inclusive boundary; the other modes require a zero limit.
Normal Flying/Reach blocking rules still apply. Reach on an attacker does not
make it a flying attacker.

```json
{
  "family": "blocking_creature",
  "version": 2,
  "params": {"keywords": [], "restriction": "power_at_least", "power_limit": 2},
  "oracle_text": "This creature can't block creatures with power 2 or greater.",
  "variants": ["defend_ground_one_0", "defend_ground_two_1", "shrink_before_block", "combat_snapshot"]
}
```

Cases use real attacker and blocker declarations for both controllers, with
ground, flying, and Reach witnesses across powers zero through four. Giant
Growth and Disfigure change an attacker's current power before blocking; legal
alternatives survive the spell so death cannot masquerade as a restriction.
Tapped blockers and summoning-sick blockers are distinct cases. Combat damage
checks rejected blocks against life totals and legal blocks against damage,
death, and graveyard ownership. Cleanup must preserve the restriction.

Casting, exact payment, missing and wrong-colored mana, summoning sickness,
unblocked attacks, populated owned zones, and deliberate empty zones supplement
the blocking cases. Combat snapshots capture actual declared attackers before
blockers, then replay declarations across AI backups and restoration. Every
blocking-specific case checks stack and decision completion. Additional activated abilities are
recorded in `untested_abilities`; conditional restrictions, entry/death/attack
triggers, combined evasion mechanics, multiple attackers/blockers, forced
attacks, control changes, and Flash timing need separate cases. These tests
cover the declared component rather than certifying every ability on a card.

The first blocking-restriction batch adds **1,626 cases across 44 implemented
cards**, including **37 new card homes**. Every new case passes on native x86
and Node/Wasm, with all 1,626 observable states matching and no added known
failures. The 59 existing controls retain 50 passes and nine known failures on
both platforms, with matching states. All 43,002 prior manifest entries and 26
authored cases in selected homes are preserved.

The suite contains **27,414 behavior cases across 2,002 card records**. All 163
recipe guards, seven loader guards, nine runner guards, and 15 representative
CTest entries per platform pass. Native and Wasm generated files are identical
and match the hashes of an independent fresh generation. The fresh generation
copy was removed after hashing to conserve disk space; review logs, summaries,
and validation results remain in `out/duel-tests/recipe-blocking-creatures`.
General-test build flags are restored to their prior settings.


### Tap damage creatures

`tap_damage_creature` pins the whole Oracle text and requires explicit
`keywords`, `activation_cost`, `damage`, `targets`, `damage_you`, `damage_self`,
`controller_life_loss`, and `tap_target` parameters. Target scopes are `any`,
`creature`, or `flying_creature`. Ordinary mana plus tapping and at most one
printed side effect are supported. Extra abilities, conditions, random choices,
infect, damage lifelink/deathtouch combinations, variable amounts, hybrid costs,
and additional nonmana costs require separate cases.

```json
{
  "family": "tap_damage_creature",
  "version": 2,
  "params": {
    "keywords": [], "activation_cost": "{T}", "damage": 2,
    "targets": "any", "damage_you": 3, "damage_self": 0,
    "controller_life_loss": 0, "tap_target": false
  },
  "oracle_text": "{T}: This creature deals 2 damage to any target and 3 damage to you.",
  "variants": ["survivor", "self_player", "source_bounced", "pending_snapshot"]
}
```

Cases observe mana payment and tapping before resolving the pending activation,
and require zero damage before resolution. Both controllers, friendly creatures,
self-targets and players, lethal damage, indestructibility, printed backlash,
controller life loss, and target tapping have explicit outcomes. Exact payment,
missing/wrong-color mana, tapped and sick sources, fresh legal casting with and
without haste, opposing-turn activation, cancellation, and denied repeated taps
cover costs and permission. Vitalize provides real untapping and reuse; cleanup
removes damage without untapping. Owned populated zones and deliberate empty
zones are checked, including ordered libraries and graveyard ownership.

Unsummon responses and the real destruction pipeline remove either the target
or source before resolution. An illegal sole target cancels all resolution
effects; a departed source leaves its pending ability intact. Snapshots capture
paid pending activations, then replay resolution after AI backup and restoration.
Creature-only no-target cases legally cast Robe of Mirrors onto the only
creature, independently verify shroud with Unsummon's cast permission, and then
require activation to be unavailable. Flying-only shooters also have ground-only
negative boards when the source itself is not flying.

Rakdos Ickspitter's handler omits `GAA_CAN_TARGET`. Its enrollment retains basic
casting/combat, tap/sickness checks, and the isolated no-target assertion; its
damage-resolution cases remain pending. Draft unused-choice failures are not
accepted as baselines. Four haste shooters also reproduce the existing attack
gate defect despite succeeding at immediate tap activation. Correct Oracle
assertions remain for both defects. Flash timing, planeswalkers, prevention,
protection, redirection, countering activated abilities, and multiplayer remain
coverage gaps.

The first tap-damage batch adds **731 cases across 22 implemented cards**,
including **18 new card homes**. Native x86 and Node/Wasm each report 726 passes
and five exact known failures: four instances of the existing fresh-cast haste
attack defect and one isolated Rakdos Ickspitter targeting defect. All 731
observable states match. No production fixes are included. The 19 draft
Ickspitter damage scenarios that could not obtain a target are deferred, with
their diagnostic logs retained rather than accepted as baselines.

All 51 existing controls retain 42 passes and nine known failures on both
platforms, with matching observable states. All 44,628 prior manifest entries,
15 authored cases in selected homes, and existing failure baselines are
preserved. The 180 recipe, seven loader, and nine runner guards pass, along with
15 representative CTest entries per platform. Native and Wasm generated files
match each other and independent fresh-generation hashes. General-test build
settings are restored to their previous values.

The suite contains **28,145 behavior cases across 2,020 card records**. Results,
exact baseline review, draft diagnostics, and state comparisons are retained in
`out/duel-tests/recipe-tap-damage-creatures`. Coverage remains limited to the
declared components and gaps above.


### Combat keyword creature components (version 2)

`combat_keyword_creature` uses explicit `keywords`, `mechanic` (`bushido`, `flanking`, or `exalted`), and `amount` parameters. Bushido supports reviewed integer bonuses 1–5; flanking and exalted require amount 1. The declaration pins the full Oracle text and an explicit variant list. The whole passive text must consist of that mechanic plus reviewed unconditional keywords. Separately printed activated abilities are recorded as `untested_abilities`, not credited as tested. Numeric stats and ordinary casting costs come from the pinned Oracle corpus. Conditional/extra triggered rules, multiple mechanics, unreviewed reminders, double strike, lifelink and deathtouch fail closed; trample is allowed only on exalted sources and its damage allocation remains a gap. Legendary duplicates require dedicated legend-rule scenarios and are excluded from generic two-source fixtures.

The scenarios complete real attacker and blocker declarations instead of sending isolated card events. Both controllers exercise lone and multiple attackers, other attacking creatures, blocking versus becoming blocked, tapped and sick sources, flying legality, temporary bonuses/penalties, combat damage, first strike, cleanup, owned populated zones, and empty-zone alternatives. Bushido is applied once when an attacker becomes blocked by multiple creatures. Flanking affects each non-flanking blocker, leaves flanking blockers unchanged, and can kill a 1/1 blocker without allowing combat damage through to the player. Exalted buffs only a single declared attacker, stacks across sources, works from tapped/sick sources, survives subsequent source removal, and does not trigger retroactively when a second attacker is bounced after declaration. Human simultaneous exalted triggers use an explicit priority choice; the remaining mandatory trigger then resolves automatically. Snapshot replay captures provisional attackers (exalted) or blockers (bushido/flanking), completes the same declaration, runs AI backups, restores, and checks equivalent results.

The harness inspector now reads the core `STATE_ISBLOCKED` history flag. Counting surviving blockers incorrectly described an attacker as unblocked after flanking killed its sole blocker, even though core combat damage correctly remained blocked. No production engine/card fixes were made in this batch. Flash timing, separately printed activated abilities, trample allocation, creatures blocking multiple attackers, trigger counters, and multiplayer remain gaps. This is component coverage, not whole-card certification.

Batch validation: 1,920 cases across 43 implemented cards (14 bushido / 603 cases, 13 flanking / 572 cases, 16 exalted / 745 cases), adding 42 card homes. Native x86 and Node/Wasm each report 1,907 passes and 13 exact known failures: ten Opal-Eye scenarios expose its non-GAA early return before bushido; three fresh-cast scenarios reproduce the existing haste attack gate on Duskmantle Prowler, Ronin Houndmaster and Suq’Ata Lancer. Correct Oracle assertions remain intact. Noble Hierarch’s simultaneous-trigger scenario passes with explicit ordering and is not baselined. All 1,920 observable results match across platforms, along with 43 older controls (32 pass / 11 known), two older legal/illegal menace inspection cases, 202 recipe guards, seven loader guards, nine runner guards, and 27 CTest checks per platform. All 45,359 earlier manifest entries and three authored cases in existing homes are preserved. The suite now has 30,065 behavior cases across 2,062 card records. Detailed evidence: `out/duel-tests/recipe-combat-keywords/`.
