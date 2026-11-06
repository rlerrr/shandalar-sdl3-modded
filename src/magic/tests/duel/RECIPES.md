# Faster Oracle-backed card authoring

Recipes generate ordinary behavior cases for the real duel engine. They are
distinct from the older card-ID templates. A reviewed recipe costs authoring and
review work once; matching cards get its scenarios without model-written JSON.
Existing authored cases remain in their card files alongside the declaration.

For parallel authoring, configure the shared native/Wasm engines once with
`SHANDALAR_DUEL_AUTHORING_BUILD=ON` and
`SHANDALAR_DUEL_EXTENDED_TESTS=OFF`. This uses `generate_cases.py --driver-only`
to emit only compiled fixtures, templates, smoke tables and handler bindings.
It skips behavior expansion and catalog CTest registration. Authors then generate
complete selected card homes into separate directories with `--csvid` or
`--case-file` and run their archives against the immutable engine using
`run_cases.py --case-data ... --manifest ...`. The same archive works on native
and Node/Wasm. See the [independent authoring workflow](README.md#independent-authoring)
for commands and output details. The integrator owns engine rebuilds; switch
authoring mode off for full-catalog generation and integration validation.

Start a new adapter with four to eight cards covering its distinct profiles.
Freeze the module and declarations, then run both controllers, payment,
responses and snapshot replay through the central native/Wasm queue before
enrolling a larger group. Record hashes of the selected declarations and adapter
inputs during generation. Correct fixture errors before expanding the pattern;
only reviewed Oracle mismatches belong in the known-failure baseline.

Older expanders may reuse the same step dictionaries before and after snapshot
restore. When mirroring players or adjusting receipts, process each object once
by identity, or copy each occurrence independently. Guard both replay payment
receipts and decision owners; checking only source ownership misses double flips.

Explicit pending-spell responses can opt into `cast_response` and
`resolve_response` when the ordinary nested response UI is not driving the case.
The first casts a real response against the pending top spell; the second drains
the response and parent stack in the parent's casting context. Both reject
missing spell context, and ordinary costs, targeting and resolution still run.
`resolve_response` is currently validated only for one response above its parent
spell; do not use it for deeper response chains.
Do not substitute them for ordinary `cast`/`resolve` globally. Assert the actual
pending stack and consumed choices. Pending spell cards remain physically counted
in hand until resolution, so intermediate zone models must retain them there.

Recipes depending on this-turn death history opt into `reset_death_history` as
the first step, with an empty stack, before any authored deaths. It initializes
the same Deadbox history that normal turn startup clears, avoiding reserved-token
metadata being mistaken for a dead card. Destroy real battlefield witnesses
afterward and keep older graveyard cards as negative controls. Do not reset after
those deaths, on refresh, or when replay must preserve captured history.

For nonmana `GA_BASIC` abilities and creature Unearth seeded in a graveyard, begin
with `prepare_graveyard_abilities` on an empty stack. Normal startup registers
RulesEngine needs from decks and active cards; a graveyard-only fixture has not
passed through that lifecycle. This opt-in operation uses the ordinary card flag
query and RulesEngine registration. It does not pay costs or perform effects.
`can_activate_graveyard` and `require_can_activate_graveyard` take the setup's
graveyard-index ref and preserve the full state and decision queue while querying.
`activate_graveyard` uses the real RulesEngine menu; queue its actual option,
assert pending stack/cost/exile receipts, then resolve normally. The helper
verifies the resulting activation's source and available mode. Unearth uses the
real paid removal and RulesEngine return/haste/exile lifecycle; it does not cast
the card. Other graveyard casting and return/play modes are unsupported. The
Unearth query honors the real creature-return cage restriction. Graveyard indexes can
shift when cards move, so do not reuse a stale index for another activation.
These queries describe the engine's actual menu availability, not independent
Oracle legality. Card tests must retain Oracle expectations if that menu is
wrong, including Unearth's mana-only cost leaving its source in the graveyard
until resolution. The helper does not repair premature engine zone movement.
If a handler chooses targets too late, preserve a separate pending-selection
assertion and end that scenario pending rather than clearing a choice plan and
then forcing an unscripted resolution.

`etb_damage_creature`, `death_damage_creature` and `variable_sweep` version 2
cover fixed creature entry/death damage and ordinary single-X creature damage
sweeps. Each declaration must match the complete pinned Oracle text; passive
keywords, cycling and other accepted inactive components remain explicit gaps.
Both controllers exercise real casts, payment, zone receipts, selected and
unselected recipients, lethal damage, indestructibility, countered casts and
snapshot replay. Entry tests distinguish re-entry from other creatures entering
or dying. Death tests distinguish destruction, lethal damage and combat from
bounce, blink and exile, including optional target cancellation and both hybrid
payment colors where printed. Sweeps cover zero and positive X, flying filters,
both players, mixed boards and cleanup. Damage is checked before cleanup heals
survivors. Planeswalker targets, prevention/replacement, control changes and
multiplayer remain separate components; each case records its specific gaps.

Scripted inputs apply to actual resolution. Nested AI damage-prevention searches
use ordinary AI choices, preserving the caller's queued response while engine
snapshots are replayed. The damage-caused death cases exercise this boundary.

`variable_value` and `variable_damage` version 2 cover complete ordinary
single-X instant and sorcery effects. Parameters must match the entire pinned
Oracle text and printed base cost. Value operations include ordered draw,
discard, mill and life changes, twice X, X plus a constant, and life gained from
the actual life loss. Damage scopes distinguish creatures from any target;
declared life gain and black-only X payment have separate receipts.

Both controllers choose X = 0, 1, 2, 3, 5 and 8 through the real payment engine.
Cases check base costs at X=0, exact colored payment, legal targets, pending
target protection/departure, lethal damage, regeneration, short hands and mill
libraries, countered spells, timing, cleanup and snapshot replay as applicable.
The lost-target damage fixture casts a real Unsummon onto the pending spell
and checks the two-spell stack. Random discards permit any legal identities
while preserving exact conservation and zone receipts.

`choose_x` is an explicit queued input with a controller and a nonnegative
integer value. The engine consumes it only for an actual variable payment;
fixed costs and ordinary AI searches do not consume it. Snapshots restore the
input queue. Actual cost computation, mana spending, restrictions and spell
resolution remain in the engine. Tests reject unconsumed inputs.

Multiple-X card handlers, alternative costs, planeswalker targets, damage
replacement/prevention, draw beyond an empty library, randomness distribution,
trigger interactions and multiplayer remain separate coverage gaps. A direct
engine control verifies repeated-X payment without claiming such card coverage.

`static_stats` version 2 covers one to three complete continuous creature-stat
clauses on a normal standalone permanent. Parameters pin signed bonuses,
controller/other restrictions, combat/tapping states, and color, subtype,
artifact, token, legendary or flying predicates. Source keywords and inactive
activated clauses are explicit; additional conditions or triggered clauses are
rejected. Two-color hybrid sources use reviewed payment colors, with alternate
and mixed exact-payment scenarios.

Both controllers face mixed creature boards, changelings and token copies.
Separate clauses add independently when colors or creature types overlap.
Tests inspect characteristics before the source resolves, after entry,
source removal/re-entry, additional copies, late arrivals, tapping changes,
combat and cleanup. Marked damage followed by source bounce tests state-based
death when a toughness bonus disappears. Exact zones distinguish a dead card
from a token that vanishes, and snapshots replay actual pending spells.
Late arrivals avoid duplicate legends and unscripted alternative casting modes.
Declaration-only combat participants are removed before cleanup; separate
combat cases resolve actual damage and end combat normally.

Full-pool AI hybrid casts may choose any legal allocation, so those cases check
the total payment; exact-payment cases still check each color. World sources
and legendary sources have different duplicate-fixture limits. Source keyword
combat/timing, inactive activated abilities, live color/type/control changes,
replacement effects and multiplayer remain explicit component gaps.

`removal_activation` version 2 pins one complete targeted destruction or exile
ability on a normal permanent. Explicit fields describe the removal predicate,
ordinary mana/tap/discard/life/self-sacrifice or exile costs, passive keywords,
printed tapped entry and an optional colorless mana ability. Unreviewed costs,
additional abilities and changes to the pinned source or witnesses fail closed.

Both controllers exercise casting the source, actual activation costs,
insufficient resources, fresh/tapped sources, opposing turns, Suppression Field,
target predicates, lost or changed targets, source removal, regeneration,
indestructibility, repeated activations and snapshot replay where applicable.
Aura and enchanted-creature targets use real cast Auras. Combat witnesses use
real attack/block declarations; land creatures must be untapped before attacking.
Each fixture checks exact zone counts and the identities of every card it uses,
including nonselected duplicates, ordered libraries and hand/grave conservation.

`probe_activation_target` inspects the handler's actual target definition for a
named candidate, independently of other legal targets such as the source itself.
It snapshots all engine/input state, observes the real query or selection path,
then restores costs, random state and the caller's planned input. An absent or
ambiguous definition is a test failure. Successful removals still run through
normal activation and resolution. Exact-payment tests for mana producers skip
this probe and exercise the real activation directly. Optional per-controller
`activation_dialogs` annotations describe mode presentation, not target legality.

Mana-producing siblings can mask ability availability, so insufficient-removal
query cases are omitted for them; exact payment and resulting stack/effects are
still checked. Source keyword combat, alternate abilities/land play, protection,
live numeric/control changes, replacement/triggered effects and multiplayer
remain explicit component gaps.

`mass_removal` version 2 pins complete ordinary destruction, exile and bounce
effects from spells and permanent activations. Explicit parameters describe
type/color/state/characteristic filters, controller or owner selection, source
exclusion, regeneration clauses, destruction-derived draws/life and discard-all
receipts. Mana, tapping, self-sacrifice/exile, discard and life costs are kept
separate from resolution; printed tapped entry is checked by real source casts.

Both controllers face mixed boards with intersecting permanent types,
indestructible creatures/artifacts/lands, token copies and real cast Auras.
Stolen-card fixtures distinguish ownership from control and send bounced or
destroyed cards to their owner's zones. An Aura whose creature leaves the
battlefield enters the graveyard even when the Aura itself was not selected.
Actual attack declarations and offered regeneration responses exercise those
rules. Exact payments, single-zone multiplicities, combined hand/grave totals,
ordered libraries and caller-owned snapshots check complete receipts. AI
discard costs allow any legal subset; human minimum/maximum identity policies
exercise actual offered choices. Fixture mana sources stay tapped so they
cannot hide insufficient pools or alter exact payment receipts.

Alternative abilities/costs, entry/death removal triggers, variable limits,
late arrivals/state changes while pending, conditional replacement/triggered
effects, control changes during resolution and multiplayer remain separate
components. Preset stolen-card ownership tests are covered.

`value_flow` version 2 covers fixed nondiscard draw, mill and life effects from
ordinary spells, creature entry/death triggers and permanent activations. Its
whole-text validator uses the same ordered operation and cost machinery as
`hand_exchange`, with separate family names and explicit source/recipient
parameters. `value_flow` rejects every discard effect or cost;
`hand_exchange` still requires one.
Prior generated hand-exchange cases remain unchanged.

Both controllers check exact costs, zone multiplicities, ordered libraries,
controller/recipient life deltas, repeat activations, source removal/re-entry,
countered spells, pending target protection and snapshot replay where applicable.
Exact-library fixtures exercise drawing the last cards without drawing beyond
the library. Short/empty-library mill fixtures require milling only the cards
remaining. Higher starting life totals keep repeated loss effects within a
live duel. Draw attempts beyond an empty library, source keyword combat,
alternative abilities/costs, replacement/triggered effects, control changes and
multiplayer remain named coverage gaps.

`hand_exchange` version 2 covers ordered draw/discard operations from ordinary
spells, creature entry/death triggers and permanent activations. Parameters
must match the complete pinned effect, recipient and costs, including fixed
mill/life effects and draw amounts derived from earlier discards. Additional
discard costs are checked before resolution. Flashback, madness, cycling,
miracle and the separate mana ability of a looting land are named component
gaps rather than inferred coverage.

Both controllers exercise empty, short, uneven, duplicate and large hands,
exact costs, source removal/re-entry, counters and snapshot replay where
applicable. Human discard policies select the lowest or highest CSVID actually
offered by the engine. AI and random discards allow any legal subset; exact
hand/graveyard totals for every identity, zone counts, upper bounds on remaining
hand copies and ordered libraries detect lost, duplicated or unrelated cards
without assuming a particular random outcome. These tests do not establish
randomness distribution or cover discard/replacement triggers, control changes
or multiplayer.

Pending-spell cost receipts account for the legacy active-zone counter, which
includes the spell's physical slot in its hand count until resolution. Creature
casts use the normal cast path directly; their handlers need not implement the
instant/sorcery `EVENT_CAN_CAST` query.

Reviewed `target_choices` and `activation_dialogs` annotations describe the
handler's input presentation separately from the Oracle effect. Sole-opponent
handlers can select implicitly; Wistful Thinking currently asks twice for the
same target. `present_ai_dialogs` opts in to presenting normally hidden AI mode
dialogs, preserving the engine's disabled-option checks and snapshot state.
AI searches retain their usual internal mode selection.

`target_removal` version 2 covers ordinary single-target destruction or exile.
The complete effect must match explicit type/color/subtype, controller, combat
or tapped-state, and fixed mana value/power/toughness restrictions. Secondary
fixed draw, life, damage and mana receipts are explicit too. Cycling, flashback,
madness and splice remain named component gaps; they are not exercised by
ordinary casts from hand. Other clauses and casting mechanics fail closed.

An independent normal-card palette exercises eligibility one candidate at a
time, including artifact lands, artifact/enchantment creatures, planeswalkers,
legendary creatures, multicolored and colorless cards, changelings, Vehicles,
and indestructible permanents. Negative queries have no additional legal target
that could mask an incorrect predicate. Positive tests use real targeting and
payment for both controllers. Identical extra targets verify single-target
removal; exact zone multiplicities distinguish battlefield, hand, graveyard and
exile copies. Countering, target loss/bounce, live tap-state changes and real
attacker/blocker declarations test resolution legality and secondary effects.

Regeneration attempts select only actual engine-offered responses. A blocked
attempt remains queued, and `assert_unused_choices` checks and retires exactly
that many choices. This proves that exile or a no-regeneration clause prevents
regeneration without submitting illegal input. Ordinary destruction consumes
the planned regeneration and checks its payment and surviving creature.
`require_can_cast` stops a scenario at an incorrect positive permission query,
allowing a card bug to retain a precise assertion instead of causing a later
invalid target-input error. Snapshots replay through the real engine and verify
complete observable results. Alternative source abilities, multiple targets,
pre-existing regeneration shields, protection/hexproof/shroud, numeric target
changes while pending, replacement effects, control changes and multiplayer
need separate scenarios.

`library_search` version 2 covers single-card tutors cast as spells, triggered
by creature entry, or activated from a permanent. Reviewed parameters pin the
entire Oracle text, search scope and any fixed mana value/power/toughness limit,
destination, optional trigger, exact costs, source keywords, named cards and
separate life loss. Additional abilities remain explicit component gaps.

Mixed, Oracle-pinned witnesses check the actual selector's accepted and rejected
cards: type unions/intersections, subtypes and changelings, legendary status,
color, names and numeric boundaries. Both controllers exercise payment, exact
zone receipts, empty/duplicate libraries, declining qualified searches, repeated
searches, snapshots, countering spells and entry-source removal/reentry where
applicable. Unrestricted searches must select a card when one exists; hidden
quality searches may fail to find. `assert_offered_selection_required` observes
the actual selection policy separately from its eligibility predicate.

Optional trigger/menu annotations script presentation only. In scripted tests,
Bogbrew Witch, Treefolk Harbinger and Gatecreeper Vine/District Guide expose the
full legal choices instead of their AI-preferred subset. Normal gameplay and
speculative AI retain their preferences. If a named choice is rejected by an
observed optional selector, the host legally declines it, leaving eligibility
and exact outcome assertions to report the defect. Required or unobserved
selectors retain strict input errors. This does not override the engine filter.
If a required observed selector has no legal candidates, the scripted selector
returns no card as the ordinary AI path does. Its incorrect required policy
and forced-choice count remain visible to assertions. A missing optional
fallback can select another actually legal offered card; it never admits an
illegal candidate. These paths allow exact failures for impossible searches.

`bind_hand_copy` selects a CSVID's zero-based occurrence in hand, analogous to
`bind_permanent_copy`. Namesake tutors use it when bounce/recast leaves both
the fetched copy and bounced source in hand. Fetched Aura attachment, extra
triggers, source keyword combat, other activated abilities, replacement effects,
multiplayer and shuffle distribution require separate scenarios.
For Blightspeaker, per-option unaffordable tutor queries are also deferred:
its separate free life-loss ability keeps the card-level activation query true.
Exact funded tutor payments and shared tap/sickness restrictions remain tested.

Generation streams card files through a bounded LRU cache while retaining the
existing globally sorted case order. This avoids keeping every expanded recipe
in memory during configure. The eager loader remains available to authoring
tools and unit checks. Streaming and eager generation produced 96 byte-identical
outputs before the library-search presentation updates. The enlarged Wasm test
binary starts with 768 MiB; this setting is confined to `duel_card_tests`.

`landcycling` version 2 declares `cycling_cost`, `basic`, and `types`. The
complete pinned Oracle text must contain exactly the reviewed basic-landcycling
or one/two-type cycling clause. Additional cycling/discard triggers fail closed.
Other spell, battlefield, combat and upkeep/graveyard rules are retained as
explicit untested text; the component never casts its source card.

The shared search scenarios activate the real hand-ability proxy, pay mana and
discard before resolution, and inspect actual basic/type eligibility in mixed
libraries. They cover both controllers, opposing-turn/combat timing, canceling
the activation, declining the search, empty/duplicate libraries, two sources,
Fluctuator reductions, Suppression Field taxes, mandatory colored mana, cleanup
with a pending ability, and snapshots. Reclaim and Tormod's Crypt can move the
discarded card while its search is pending; the ability still resolves. The
existing 2,880 ordinary search scenarios remain semantically unchanged.

`land_search` version 2 covers an ordinary single-land search spell or activated
permanent ability. Explicit parameters describe basic/snow restrictions, a union
of printed land types, hand/battlefield destination, entry tapping, ordinary
mana/tap/sacrifice/life costs, and an optional separate colorless mana ability.
The complete Oracle text must match. Optional `activation_dialogs` are menu
annotations for each controller, not effect expectations.

A mixed library contains all five basics, snow basics, typed original duals,
an untyped artifact land, a nonbasic snow land, nonlands, and a duplicate. Spells and activations pay
real costs; sacrifice is checked before the search resolves. Both controllers
exercise exact payments, failure to find, empty/short libraries, duplicates,
repeat searches, destination state, taxes, legal untapping, countering, and
snapshot replay where applicable. Sources that enter tapped have a distinct
entry-state case; ordinary activation fixtures represent ready permanents.

`observe_search_filters` opts into read-only copies of the actual engine
selector predicate results. `assert_offered_legal_card` checks every offered
copy of a named card, allowing library order to change after shuffling. This
verifies exclusions as well as successful selection; voluntarily declining a
search alone does not prove its filter. Observation is disabled for other
cases, normal gameplay and speculative AI. Its flag and recorded offers are
preserved by test snapshots. Shuffle distribution, replacement effects, source
casting/combat for activated components, and additional triggers remain gaps.

`land_entry_mana` version 2 requires explicit `entry`, `outputs`, `production`,
and `life` parameters matching the complete Oracle land text. Supported entry
forms are life-payment shock lands, bounce lands, Lairs, scry, life gain,
two-player Battlebond lands, and unconditional tapped entry. Cycling may remain
a separately declared component. Snow/legendary lands retain their printed
types; simultaneous-copy scenarios are excluded for legendary sources.

Walking Atlas puts the actual card from hand onto the battlefield. No land is
cast as a spell. Scenarios cover entry decisions, both controllers, tapped and
untapped return choices, returning the source itself where legal, repeated
entry, other land arrivals, cleanup, empty/populated zones, mana abilities under
Suppression Field, actual spending, and snapshot replay. Returned cards are
rebound to their new hand slots before observation. Human scry explicitly keeps
or bottoms the viewed card; AI scripts permit a legal optional selection to be
skipped. `record_library` and `assert_scry_result` verify card multiplicities and
preservation of the uninspected suffix, allowing either legal scry outcome.
Empty and one-card libraries are included. Land play allowances/timing, other
replacement effects, trigger ordering, and multiplayer counts remain separate.

`entry_stats_creature` covers unconditional creature entry triggers that give a
target or the controller's team a temporary positive bonus, or put fixed +1/+1
counters on a target creature. Version 2 parameters are `effect` (`target_pump`,
`team_pump`, or `counter`), `power`, `toughness`, `amount`, and explicit source
`keywords`. Counter recipes use zero temporary stats; pump recipes use zero
counter amount. The full Oracle clause and all remaining passive text must match.

Its scenarios check casting and exact payment, pre-resolution state, both
controllers, self and opposing targets, unchanged creatures/noncreatures,
cleanup, later arrivals, re-entry, source removal, beneficiary combat, and
snapshot replay. Temporary team bonuses affect the resolution cohort; permanent
counters survive cleanup. Source keyword combat mechanics, target loss after
trigger targeting, replacement effects, and trigger ordering remain separate.

Once-per-turn self-pumps include a real turn advance before testing reuse.
Three-color mana creatures check each offered output. Entry-value recipes also
support pure life-loss triggers. Optional `activation_dialogs` on value abilities
maps selected variants to explicit presentation choices, including AI single-mode
menus; it supplies no rules expectations. Scripted activation queries ignore AI
desirability while retaining every legality check. Gameplay and AI search retain
their preference filtering.

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
still needs separate cases. Might of Alara was deferred in this batch because its
legacy challenge-unlock gate blocked the player-zero fixture. The later passive
keyword batch supplies explicit in-memory availability for loaded fixture cards;
its Domain effect still needs a reviewed declaration rather than a false rules
baseline.

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


### Continuous creature boosts

`lord_creature` uses explicit `predicate` (`all`, `subtype`, `color`, or `artifact`),
`value`, `scope` (`own` or `both`), `other`, positive `power`/`toughness` amounts,
and source `keywords`. Full pinned passive Oracle text must agree exactly; this
first scope supports one unconditional boost clause and ordinary mana costs.
Additional printed activated abilities are recorded as coverage gaps. Additional
passive restrictions, triggers, conditional boosts, negative modifiers and
multiple boost clauses fail closed. Numeric stats and reviewed witnesses are
required. Legendary sources cannot have two copies under one controller.

Boards include matching and nonmatching creatures controlled by both players.
Tests cover self-inclusion/exclusion, controller scope, stacked and opposing
sources, tapped/summoning-sick sources, casting and exact mana payment, source
bounce/recast, destroy and blink, witness blink, additive Giant Growth followed
by cleanup, unblocked combat damage, and snapshot replay after AI backups.
Blink and bounce explicitly rebind the new object before inspecting it.
Populated hands, graveyards, exile and ordered libraries are checked throughout;
empty-zone variants are deliberate. Source keywords (including flying and Flash), printed activated abilities,
type/color/control changes, enchantment sources and multiplayer remain gaps.

The first enrollment contains 1,856 cases across 46 card records. Fail-closed
unit checks also validate parameters, passive clauses, witness characteristics,
self-exclusion, controller scope, payment, snapshot sequencing and references.

Validation: native x86 and Node/Wasm each report 1,821 passes and 35 exact known
failures. All 1,856 observable states agree, as do 45 existing controls (34 passes,
11 prior known failures). The new failures expose one defect: Jacques le Vert
omits its own Oracle +0/+2 because its color-boost helper lacks `BCT_INCLUDE_SELF`.
Correct assertions remain intact; production fixes are deferred. Basal Thrull is
explicitly tapped in the missing-color negative so its sacrifice mana cannot make
the supposedly illegal cast legal. There are 241 passing Python guard checks and
25 successful CTest gates per platform. Build options and all prior cases/baselines
are preserved. Details: `out/duel-tests/recipe-lords/validation.json`.

### Entry counters and bloodthirst

`entry_counter_creature` has explicit `mechanic` (`fixed` or `bloodthirst`),
`amount` (1–7), and `keywords` parameters. The complete pinned passive text must
agree with the declaration; ordinary costs and numeric base stats come from
Oracle. Fixed counters support base 0/0 creatures that survive their entry.
Graft, modular, variable amounts, other passive/triggered clauses, nonstandard
costs and alternate faces fail closed. Printed activated abilities and keywords
are recorded as gaps; these cases cover the entry component, not the whole card.

The inspector reads actual +1/+1 counters as well as power/toughness. Both
controllers exercise casts, exact payment and illegal payment, countered spells,
blink, bounce/recast, removal, cleanup, independent copies, and populated zones.
Giant Growth and Glorious Anthem change stats without changing the counter count.
Bloodthirst cases distinguish opponent damage from self/creature damage and actual
life loss (a resolved Death of a Thousand Stings). They cover combat damage,
multiple damage events, damage while the creature spell is pending, earlier-turn
damage, damaged-turn re-entry and new-turn re-entry. Snapshot replay includes paid
pending spells, pending blinks and damage history, with intervening AI backups.
Expensive repeated casts receive a larger colorless fixture pool; dedicated
payment tests still supply exactly the printed cost. An entry `require_card`
prerequisite stops a case if the creature dies unexpectedly, before queuing
dependent targets. An exit observer records the resulting board even after a
fatal assertion, so prerequisite failures can be compared across platforms.

The new `advance_turn` action runs cleanup and the game's start-turn gate before
selecting the next player's main phase. This performs skip-turn housekeeping,
per-turn trap reset and sickness clearing. It deliberately omits untap, upkeep
and draw, preserving fixture mana and ordered zones; it is not a full turn
simulation. A skipped turn or nonempty stack fails explicitly. Earlier scripts
that only selected a new turn omitted the trap reset and produced false
bloodthirst failures. Start-turn autosave is suppressed through the existing
network flag while the real rules dispatch runs. Production gameplay is unchanged.

Batch validation: 1,945 cases across 39 new card homes. Native x86 and Node/Wasm
each report 1,789 passes and 156 exact known failures. These represent four defect
groups: Spike Breeder enters with two counters instead of three (37 cases);
Festercreep uses a −1/−1 counter and dies at entry (37); Bloodscale Prowler,
Ghor-Clan Savage and Rabble-Rouser lack the metadata that enables damage history
(60); and the shared bloodthirst helper checks damage at casting rather than
entry (22 across the other eleven bloodthirst cards). Correct assertions remain
intact. Festercreep cases stop at the entry prerequisite, without accepting
unused choices or invalid follow-up actions. Production fixes are deferred.

All 1,945 observable results agree across platforms, including prerequisite
failures. The 49 older controls report 37 passes and 12 prior known failures on
both platforms. There are 269 passing Python guards and 48 successful CTest gates
per platform. A native recipe-guard timeout during overlapping runs passed when
the checks were rerun without that overlap. All 49,135 earlier manifest entries
and previous baselines are preserved. The suite now contains 33,866 behavior
cases across 2,146 card records. Detailed evidence and grouped diagnoses:
`out/duel-tests/recipe-entry-counters/`.

### Ordinary cycling

`cycling` pins the complete Oracle text and an explicit `cycling_cost` parameter.
It accepts ordinary fixed generic/colored costs only; cycling/discard triggers,
other hand/graveyard mechanics and alternate faces require dedicated cases.
Printed spell effects, battlefield abilities and other keywords are visible gaps.
This recipe covers cycling, not those other components of a card.

The `can_activate_hand`, `require_can_activate_hand` and `activate_hand` actions
use the engine's existing rules-engine proxy and scripted hand-ability menu.
Eligibility, payment, discard and stack resolution run through production code.
Tests inspect the paid cost and discarded source before allowing the draw.
Missing proxy metadata is a fatal prerequisite failure, before any decision is
queued; it cannot be mistaken for a successful direct event invocation.

Both controllers exercise ordered draws, exact payment, insufficient/wrong-color
mana, cancellation, two copies, combat and opposing-turn activation, stocked and
intentionally sparse zones, and last-card/duplicate-card libraries. Fluctuator
and Suppression Field exercise reductions, taxes and controller scope. Reclaim
can move the discarded source onto the library while cycling is pending; Tormod's
Crypt can exile it. Those abilities must still draw on resolution. Crypt uses the
human controller for either graveyard so AI strategic self-target refusal does
not create a rules failure. Pending and resolved snapshots replay after AI
backups and compare observable state. A cleanup test checks that ending temporary
effects does not cancel a pending ability; it does not simulate full priority
handling in the cleanup step.

Batch validation: 2,022 cases across 45 card records, adding 30 new homes and
extending 15 existing ones. Native x86 and Node/Wasm each report 1,690 passes and
332 exact known failures, with no harness or decision failures accepted. Three
defect groups explain them: opposing Suppression Fields are omitted from cycling
costs (164 cases across 41 cards); Cast Out, Djeru's Resolve and Forsake the Worldly
lack the metadata that exposes their implemented cycling handlers (126); and
Repopulate's spell handler omits its cycling branch (42). Correct Oracle
assertions remain intact. Production fixes are deferred.

All 2,022 observable results agree across platforms. The 180 older controls
report 164 passes and 16 prior known failures on each. There are 293 passing
Python guards and 32 successful CTest checks per platform. An overlapping native
recipe-guard run timed out; rerunning CTest serially passed. All 51,080 earlier
manifest entries, prior authored cases and previous baselines are preserved.
The suite now contains 35,888 behavior cases across 2,176 card records. Detailed
validation and grouped diagnoses: `out/duel-tests/recipe-cycling/`.

### Mana-producing lands and artifacts

`permanent_mana` pins the whole Oracle text and explicit `activation_cost`,
`mana_outputs`, `production`, `enters_tapped`, `entry_life`, and `indestructible`
parameters. It supports ordinary noncreature lands/artifacts with a tap cost and
an optional one-to-three generic input. Fixed output is at most three mana;
choices support two/three single colors or one color from all five, including
two/three mana of that one color. Basic-land reminder text is recognized.
Conditional/restricted production, additional activated abilities and unreviewed
entry/passive/triggered clauses fail closed. Cycling remains a visible gap.
Artifact spell costs come from structured Oracle data; zero-cost Moxen are valid.
Lands enter via the real fixture put-into-play path. These cases do not test land
play permissions or emulate playing lands by casting spells.

Both controllers exercise entry state/life, explicit mana pools, separate taps,
repeated activation refusal, input costs, opponent turns, combat, populated/empty
zones, spending produced mana, removal and snapshots. Colored spending uses
one-mana creature witnesses for each produced color. Generic spending supplies
only the shortfall for Phyrexian Hulk. Human payments script the colored-pool
clicks; AI payments use the engine's automatic payment path.
Twiddle legally untaps sources; Suppression Field is legally cast before checking
that it leaves mana abilities untaxed. Artifact casts cover immediate activation,
insufficient payment, pending snapshots and Negate in a real priority window.
Snapshots replay after AI backups. `require_can_activate` stops before queuing
color decisions if eligibility is broken.

The new `refresh` action updates cached characteristics using the production
recalculation routine between actions. Artifact resolution is followed by this
normal refresh before activation. The headless host selects a sole legal color
when no explicit color choice is queued, and acknowledges only the six exact AI
informational "to produce ... mana." notices, preserving explicit scripted
confirmations when present. Those notices
report an already selected output; gameplay decisions and other dialogs still
require scripts. No card implementations are changed by this recipe.

Empty-input fixtures refresh mana availability after overriding the raw pools.
Otherwise a filter land can retain the mana counted while the fixture had its
initial funding. A fatal eligibility assertion stops negative tests before
dependent activation choices if the engine still reports the ability available.

The isolated runner registers only the requested test for exact GoogleTest
filters. Wildcard, multi-case and exclusion filters keep full registration.
Listing tests works without initializing the engine or loading assets.

Batch validation: 7,086 cases across 143 card records, adding 108 new homes and
extending 35 existing ones. All new cases pass on native x86 and Node/Wasm, with
identical observable results and no added known failures. The 864 older controls
retain 782 passes and 82 known failures on each platform. There are 329 passing
Python guards and 39 successful CTest checks per platform.

Full catalog discovery and nine representative exact/wildcard/multi-case/exclusion
filters agree with the manifest on both platforms. Native exact-case registration
measured about 23 ms, down from about 187 ms before the shortcut; these timings are
informational, not a performance gate. All 53,102 earlier manifest entries, 73
authored cases in the extended homes, and previous baselines are preserved.
The suite now contains 42,974 behavior cases across 2,284 card records. Validation
and harness diagnoses: `out/duel-tests/recipe-permanent-mana/`.

### Ordinary Equipment (`equipment`, version 2)

Explicit parameters describe the permanent equipped-creature bonus separately
from the source Equipment's own indestructibility:

```json
{
  "family": "equipment",
  "version": 2,
  "oracle_text": "Equipped creature gets +2/+0.\nEquip {1}",
  "params": {
    "power": 2,
    "toughness": 0,
    "keywords": [],
    "equip_cost": "{1}",
    "indestructible": false
  },
  "variants": ["attach_0", "attach_1", "move_0", "pending_snapshot_1"]
}
```

The whole pinned Oracle text must match one unconditional bonus and one ordinary
generic equip cost. Source indestructibility and reviewed reminder text are
allowed; extra triggers, scaling, alternative attachment methods, and targeting
restrictions require dedicated cases. Granted keywords are read from the
explicit parameters and checked against the effect clause, since Scryfall's
keyword tags describe the Equipment itself. Normal mana costs still come from
structured Oracle data. Zero casting/equip costs and negative toughness bonuses
are supported.

Both controllers exercise real equip activations, payment, pending-stack checks,
moving attachments, invalid or lost targets, source and host departures,
cleanup, and snapshots restored after AI backups. Populated zones retain exact
contents, ownership, and ordered libraries, with a deliberate empty-zone variant.
Human generic payments using red mana script payment before target selection.
Conditional variants test combat keywords and source-versus-host
indestructibility; menace checks the complete blocker declaration rather than
rejecting the first of two legal blockers. Hexproof/shroud probes use the
production target validator without asking whether the AI wants to cast
Unsummon on its own creature.

These cases do not cover equip timing permissions, multiplayer, or multi-blocker
damage ordering. Assertions remain independent of card implementations; engine
bugs uncovered by the batch are documented rather than repaired here. Native
hardware faults caught by GoogleTest are classified as crashes, matching Wasm
traps, rather than counted as card-rules assertions. The larger generated test
catalog requires a 256 MB initial Wasm test allocation, with growth enabled.
Earlier explicit native SEH baselines are adapted in memory without changing
their stored records. A rules assertion before a hardware fault or Wasm trap
still fails the gate. Equipping a sick creature does not require preserving the
engine's sickness flags; the separate attack scenarios check haste permission.

Batch validation: 2,648 cases across 43 new card records. Native x86 and
Node/Wasm each retain 2,458 passes and 190 strict known failures: 86 pending-source
departure crashes, 86 duplicated equip-tax checks, 14 granted-haste attack
failures, and four excess lifelink gains during trample. Correct Oracle
expectations remain in place. All 2,562 completed observable states agree across
platforms; the 86 crashes follow the same attachment path and cannot produce a
completed state observation.

The 74 older controls retain their expected outcomes. Their 63 completed states
agree across platforms; 11 older native-only hardware faults remain recognized
while the corresponding Wasm cases pass. Each platform passes 39 CTest checks
and two shuffled shared-process runs. There are 364 passing Python guards, and
full discovery plus nine representative filters match the generated manifest.
All 60,188 previous manifest entries and 810 previous baseline records are
preserved. The suite now has 45,622 behavior cases across 2,327 card records.
Validation and diagnoses: `out/duel-tests/recipe-equipment/`.

### Fixed draw and life activations (`value_activation`, version 2)

This family tests ordinary standalone permanents with one fixed self-draw or
life-gain ability. Explicit parameters specify the effect and amount, ordinary
mana cost, tap cost, self-sacrifice, life payment, and any alternate tap-for-mana
outputs. Validation reconstructs the complete pinned Oracle text; extra triggers,
keywords, target choices, discard costs, hand-size conditions, and variable costs
require dedicated tests. Casting costs and numeric creature characteristics come
from structured Oracle fields. Tokens are excluded.

Both controllers use normal casting, activation, cost payment, and stack
resolution. Cases check that costs happen before drawing or gaining life, then
inspect both players' populated zones, ordered libraries, card multiplicities,
mana pools, life, source state, and stack. Other scenarios exercise fresh and
summoning-sick creatures, tapped sources, the opponent's turn, combat, repeated
and stacked activations, independent copies, Suppression Field, source removal
while an ability is pending, and ready/pending/resolved snapshots after nested
AI backup/restore. Bare-zone cases intentionally retain recognizable libraries.

Optional ordinary mana abilities get separate output cases for each color and
owner. The recipe scripts presentation mode indices and color choices; these
choices do not supply the rules expectations. When the draw branch is
unaffordable but its mana alternative remains legal, negative-pool cases execute
the legal mana branch and verify that no draw or sacrifice occurs. These cases
prove the fallback behavior; they do not prove that the UI disables the draw
choice. Empty-library loss, land-play permissions, and creature combat
characteristics remain outside this family's scope.

Example declaration for Jayemdae Tome:

```json
{
  "family": "value_activation",
  "version": 2,
  "oracle_text": "{4}, {T}: Draw a card.",
  "params": {
    "effect": "draw", "amount": 1, "mana_cost": "{4}",
    "tap": true, "sacrifice": false, "pay_life": 0, "mana_outputs": []
  },
  "variants": ["value_0", "payment_1", "pending_snapshot_1"]
}
```

Heart Warden, Dreamstone Hedron, Hedron Archive, and Blighted Cataract were
reviewed but deferred from this enrollment. Their AI-dependent mode routing and
choice presentation need dedicated decision scripts. AI refusals and missing or
unused scripted choices are not recorded as known rules failures. Cleanup cases
verify paid costs before dispatching cleanup; this helper does not simulate
priority/phase-boundary mana-pool emptying.

The initial enrollment expands 40 card records (33 new homes and seven existing
homes) into 1,686 behavior cases. Every new case passes natively and in Wasm,
with identical observable states and no new known-failure records. The 86 older
controls retain their expected outcomes, including earlier Equipment regressions
and native-only faults. Both platforms pass 39 CTest checks, two shuffled
shared-process runs, and ten discovery filters. There are 398 passing Python
guards. All 62,836 prior manifest entries and 1,000 baseline records are
preserved. The suite now contains 47,308 behavior cases across 2,360 card records.
Validation artifacts: `out/duel-tests/recipe-value-activation/`.

## Fixed library milling

`fixed_mill` uses explicit `mode` (`spell`, `activation`, or `entry`), `amount`,
`recipient` (`any`, `opponent`, `self`, or `both`), `draw`, `draw_first`,
`mana_cost`, `tap`, and `sacrifice` parameters. The complete pinned Oracle text
must match these parameters. Normal single-face instants, sorceries, standalone
creatures/artifacts/enchantments, and simple creature entry triggers are
supported. Fixed amounts are
bounded to ten milled cards and three drawn cards. Additional abilities,
keywords, variable amounts, storm, flashback, splice, and conditional milling
require dedicated cases. Scryfall's `Mill` action keyword is allowed; it does
not introduce an additional ability.

The initial group includes Millstone, Ambassador Laquatus, Whetstone,
Ghoulcaller's Bell, Morgue Thrull, Thought Scour, Mental Note, Glimpse the
Unthinkable, Vessel of Paramnesia, and simple entry creatures. Both controllers
receive populated hands, graveyards, exile, and ordered libraries containing
duplicates. Assertions verify complete graveyard and remaining library order,
hand/graveyard multiplicities, mana payment, tap/sacrifice costs, stack contents,
life, and source presence. Effects resolve through the engine. Source removal,
countering through the real casting response window, cleanup, independent
copies, untap/repeat, genuinely stacked activations, and ready/pending/resolved
snapshots after AI backup/restore provide regression scenarios.

Short, exactly sized, and empty libraries are deliberately separate cases:
milling an exhausted library is not drawing from it and should not cause a
loss. Cantrip loss from an empty library needs a separate game-loss fixture.
For targeted milling, self-target cases use controller 0 because the legacy
AI's target validator applies strategic preferences for controller 1. Those AI
refusals are not rules failures. Both controllers still exercise opponent,
untargeted self, and global milling as applicable. Creature combat abilities
remain outside this recipe's scope.

Graveyard order checks pin the engine's deterministic default ordering as a
regression expectation; they do not imply that Oracle requires a unique order
for simultaneously milled cards. Legendary sources omit same-controller copy
fixtures, which would need a separately scripted legend-rule decision.

Pending spell slots are included by the engine's hand inspector, so semantic
hand assertions run after resolution; pending libraries and graveyards are
still checked before the effect. Costs and stack depth are checked while the
ability is pending. Cleanup assertions do not model mana-pool emptying at phase
boundaries.

```json
{
  "family": "fixed_mill",
  "version": 2,
  "oracle_text": "{2}, {T}: Target player mills two cards.",
  "params": {
    "mode": "activation", "amount": 2, "recipient": "any",
    "draw": 0, "draw_first": false, "mana_cost": "{2}",
    "tap": true, "sacrifice": false
  },
  "variants": ["effect_0", "payment_1", "pending_snapshot_1", "self_target_0"]
}
```

Validation artifacts: `out/duel-tests/recipe-fixed-mill/`.

The initial enrollment adds 873 behavior cases across 25 card records (24 new
homes and one existing home): seven spells, nine activated abilities, and nine
entry creatures. All 873 pass natively and in Wasm with identical observable
states and no new known failures. Both platforms pass 39 CTest checks, two
shuffled shared-process runs, and ten discovery filters. There are 430 passing
Python guards. The 86 older controls retain their expected outcomes; all 64,522
prior catalog entries and 1,000 baseline records are preserved. The suite now
contains 48,181 behavior cases across 2,384 card records. Production engine code
is unchanged, and the original build settings are restored.


## Group stat spells with combat and both-controller replay

`mass_stats` (version 2) supplements the existing `sweep` component cases with
whole-text-pinned instant/sorcery scenarios for both controllers. Explicit
parameters select `scope` (`own`, `opponents`, `both`), `state` (`all`,
`attacking`, `blocking`), a creature `predicate` (`all`, `color`, `not_color`,
`not_subtype`), its `value`, signed `power` and `toughness`, zero/one `draw`,
and boolean `untap`. The reviewed subtype exclusion is Elf. Oracle's `+0` and
`-0` both mean zero; the complete original text still remains pinned. Extra
keywords, conditions, alternative costs, or additional effects require separate
cases. Arcane casting is supported without claiming splice coverage.

The mixed board has vanilla creatures of every color, a colorless artifact
creature, an Elf, a large combat body, and a defender, on each side. Darksteel
Relic and Maze of Ith check that noncreatures remain unaffected without
supplying hidden mana in the insufficient-payment variants. Hands, ordered
libraries, graveyards, and owned exile remain populated. Bare-zone and
empty-battlefield variants deliberately remove their respective contents.

Real attacker/blocker declarations exercise attacking-only and blocking-only
cohorts, blocked damage, and unblocked damage. Sorceries resolve in main phase
before combat starts; instants use the combat response window. Other cases
cover exact/wrong-color/insufficient payment, tapped and sick creatures,
sequential and simultaneously stacked copies, source spells countered through
priority, a creature destroyed or bounced before/after resolution, cleanup,
and a legally cast flash creature entering after the effect. That late entrant
keeps its printed stats, because the effect's creature cohort is fixed on
resolution. Snapshots replay before casting, with a paid spell pending, and
after resolution around AI backups. Pending spells are excluded from semantic
hand assertions until resolution because the engine hand inspector includes
stack slots.

Declarations preserve existing authored and `sweep` cases. These scenarios
strengthen combat, both-controller, and pending-stack coverage; a new recipe
on an already covered card does not count as another card home. Multiplayer,
Arcane interactions, empty-library draw loss, and unmentioned rules components
remain outside this family.

```json
{
  "family": "mass_stats",
  "version": 2,
  "params": {
    "scope": "own", "state": "all", "predicate": "all", "value": "",
    "power": 1, "toughness": 1, "draw": 0, "untap": false
  },
  "oracle_text": "Creatures you control get +1/+1 until end of turn.",
  "variants": ["effect_0", "effect_1", "combat_blocked_0", "pending_snapshot_1"]
}
```

The first group-stat batch adds 1,700 scenarios across 38 card records: Rally
and Hydrolash are two new homes, while 36 existing homes retain their earlier
cases and gain deeper coverage. Native x86 and Node/Wasm agree on 1,666 passes
and 34 strict extensions of Army of Allah's existing swapped-stat failure
(+0/+2 instead of +2/+0). All 1,700 observable states agree. Both runtimes also
pass 39 representative CTest checks, ten discovery filters, and two shuffled
shared-process orders; 465 Python guards pass. Eighty-eight older controls retain
their expected outcomes, including the documented native-only faults. No
production code was changed. The suite reaches 2,386 card records and 49,881
behavior scenarios.


## Attacking creatures' own temporary stats

`attack_pump_creature` (version 2) pins a normal single-face creature's complete
Oracle text. Explicit `power` and `toughness` specify its nonzero attacking
bonus, and `keywords` pins its reviewed unconditional passive abilities. The
text must contain exactly one unconditional "Whenever this creature attacks,
it gets ... until end of turn" trigger, with no additional abilities. Printed
numeric characteristics and ordinary casting costs come from structured Oracle
fields. Conditional triggers, variable characteristics, alternative costs,
additional abilities, and unreviewed keyword combinations are rejected.

Both controllers use complete attacker and blocker declarations. Scenarios
check printed stats before commitment, the bonus after declaration, unaffected
other attackers, independent same-card attackers, attacks by other creatures,
blocking, tapped and sick sources, actual fresh casts, blocked/unblocked damage,
trample overflow, and cleanup. Removing a creature from combat must preserve
the turn-long bonus. Returning or destroying it after declaration prevents its
combat damage and checks the owner's hand/graveyard. Exact, insufficient and
wrong-color casting costs are exercised independently. Populated hands,
ordered libraries, graveyards and exile accompany the normal boards; bare-zone
variants deliberately empty them. Snapshots replay a ready board, a provisional
attacker declaration, and the resolved modifier around AI backups. Additional
combat phases, multiplayer, Flash timing, and Flying/Reach blocker restrictions
remain separate coverage gaps.

```json
{
  "family": "attack_pump_creature",
  "version": 2,
  "params": {"power": 1, "toughness": 1, "keywords": []},
  "oracle_text": "Whenever this creature attacks, it gets +1/+1 until end of turn.",
  "variants": ["attack_0", "attack_1", "declaration_snapshot_0", "cleanup_1"]
}
```

The first attack-bonus batch adds 702 scenarios for 16 newly covered cards.
All 702 pass natively and under Node/Wasm, with identical observable states
and no new failure expectations. The 67,095 earlier manifest entries and 1,034
baseline entries remain unchanged. Both platforms pass 39 representative CTest
checks, ten discovery filters, and two shuffled shared-process orders; 497
Python guards pass. Ninety-four earlier regression controls retain their
expected outcomes. Native and Wasm build caches have been recreated, their
generated tables match independent generation, and the usual build flags have
been restored. No production engine code changed. The suite reaches 2,402 card
records and 50,583 behavior scenarios.


`trigger_pump_creature` covers whole-text prowess and unconditional landfall
self bonuses, with explicit `trigger`, `power`, `toughness`, and passive
`keywords` parameters. Pinned Oracle text validates the selected trigger and
bonus; structured characteristics supply printed stats and mana costs.

Real artifact/instant casts exercise prowess; Rampant Growth and Nature's Lore
put searched lands onto the battlefield for landfall. Both controllers receive
positive, repeated, and multiple-source cases, alongside opponent-controlled
spells, creature spells, failed searches, nonland entry, late-arriving sources,
and destroyed sources. Combat checks use the resulting bonus and real cleanup.
Ready, pending-spell, and resolved snapshots replay after an AI save/restore.
Populated hands, graveyards, exile and libraries remain checked; searched
libraries are checked for contents and size rather than a rules-prescribed
shuffle order. Bare-zone cases retain searchable lands deliberately.

These recipes leave countered triggering spells, mixed trigger ordering,
ordinary land-play actions, and multiplayer for dedicated cases. They preserve
real gameplay failures instead of adjusting expectations to card handlers.


Verified trigger-pump enrollment: 27 new card records and 1,290 cases.
Native x86 and Wasm each pass 1,286 cases; four freshly cast haste cases for
Monastery Swiftspear and Stormchaser Mage reproduce the existing
`STATE_SUMMONSICK_NOATTACK` defect. Correct expectations remain in place, with
five-assertion signatures and case-specific patterns in the known-failure
manifest. All 1,290 observable states agree across platforms. Older regression
controls, CTest, discovery, shuffled order, and independent generation checks
pass without production engine changes.


Defensive recipes now accept reviewed two- and three-color protection lists.
Two-color hybrid costs require explicit `payment_colors`, one legal color per
Oracle hybrid symbol. Exact payment, opposite-color payment and mixed payment
use real casting; the human payment menu is selected by its offered mana
vector instead of an option number. Ordinary mana parsing remains strict.
Both defensive and combat-keyword recipes support mirrored second-controller
base scenarios, including replayed snapshots without shared-step aliasing.

Combat-keyword components can explicitly combine protection with bushido or
exalted. Protection prevents the relevant blocker from making a legal block;
a rejected block must not trigger bushido. Indestructible bushido has a lethal
10/10 Colossus of Akros blocker case, exercising Konda's survival and cleanup.
Other activated abilities remain recorded as untested components.


Verified protection/combat expansion: eight new records and 418 cases for
Slippery Bogle, Stillmoon Cavalier, Oversoul of Dusk, Hand of Cruelty, Hand of
Honor, Knight of Glory, Knight of Infamy, and Konda, Lord of Eiganjo. All pass
natively and in Wasm with identical observable states. All 69,087 previous
manifest entries and 1,038 failure baselines remain unchanged. Both platforms
pass 39 representative CTest checks, ten discovery filters and two shuffled
four-case orders; 534 Python guards pass. The 116 older controls retain their
expected outcomes. No production engine code changed.


The shared hybrid creature layer accepts explicit `payment_colors` on supported
version-two creature recipes. Each choice corresponds to one two-color hybrid
symbol in the pinned Oracle mana cost. It delegates text, characteristics and
ability checks to the original recipe; the ordinary mana parser still rejects
hybrid costs. This does not add support for two-generic hybrid, Phyrexian mana,
variable costs, or hybrid activation costs.

`hybrid_payment_other` chooses the other color of each symbol;
`hybrid_payment_mixed` alternates the declared and opposite colors. Single-symbol
cards necessarily duplicate an exact payment in the mixed variant. Casts with
multiple affordable payment vectors script the real human payment dialog;
exact single-option payments and AI choices remain unscripted. Pure keyword
creatures now use stocked zones, including mirrored controller and snapshot
cases, and assert the retained zone contents and ordered libraries.


Verified hybrid-creature enrollment: 14 new card records and 262 cases.
Each platform passes 260 cases; Rakdos Shred-Freak's freshly cast attacks under
both controllers reproduce the existing haste defect. Correct attack, tapped
and attacking assertions remain in place with strict three-assertion signatures.
All 262 observable states agree natively and in Wasm. Inkfathom Witch's fear
component is exercised; its separate activated ability remains outside this
recipe. No production card or engine code changed.


### Infect and wither creature components

`counter_damage_creature` requires explicit `mechanic` (`Infect` or `Wither`)
and unconditional `keywords`, plus a whole Oracle text pin. It admits normal,
single-face creatures with bounded numeric stats and those passive clauses;
additional ordinary activated abilities are recorded as untested. Extra entry,
death, attack, upkeep, or damage triggers and replacement effects fail closed.
Trample, double strike, deathtouch and menace combinations need dedicated cases.

Both controllers receive actual casts, exact and insufficient payment, fresh
attacks, blocked and unblocked combat, defending ground and flying attackers,
pumped damage, cleanup, source removal, empty-zone variants, and ready/combat/
resolved snapshot replay after an AI round trip. Zero-power witnesses prevent
unrelated retaliation while testing surviving and lethal counter damage.
Infect keeps life unchanged and gives poison to the damaged player; wither
reduces life normally. Lifelink applies to either form of damage. Surviving
creatures retain -1/-1 counters and stat reductions after cleanup, with no
marked damage. Direct poison and counter observations participate in snapshots.
Noncombat damage, counter cancellation, multiple blockers and Flash timing
remain explicit gaps. Supplemental hybrid-payment cases use the existing
explicit color layer; wrong-color cases supply a color outside both alternatives.


Verified counter-damage enrollment: 18 new records and 780 cases. Native x86
and Wasm each pass 772 cases. Eight fresh/sick haste cases for Blackcleave
Goblin and Boggart Ram-Gang reproduce the existing no-attack sickness-bit defect;
five-assertion signatures preserve attack, tapped/attacking state and damage or
poison expectations. All 780 observable states agree across platforms. Headless
poison and -1/-1 counter inspection is new; no production engine code changed.
AI cast cases use exact pools instead of assuming a hybrid color preference.

### Creature entry removal and bounce

`etb_removal_creature` takes explicit `effect`, `targets`, `optional`, and
`keywords`, checked against the whole pinned Oracle text. It covers a single
unconditional entry clause on a normal creature with numeric stats. Additional
ordinary activated abilities and passive keywords are listed as untested;
conditional entry abilities and additional triggers require dedicated cases.

Both controllers cast through the real stack and select friendly or opposing
targets. Cases check exact and insufficient payment, each allowed permanent
type, optional decline, entry without other targets, countering the creature,
cleanup, source removal, bounce/recast, blink, and snapshot replay after AI
searches. Mandatory creature bounce can return the source itself. Stocked
hands, graveyards, exile and ordered libraries are checked alongside the effect;
empty zones are explicit variants. Returned cards are rebound to their new hand
instances. Targets leaving after selection, trigger ordering, alternative costs,
and additional abilities remain gaps.

`trigger_priority: true` describes an optional trigger's host input path rather
than an expected rules result. It scripts selecting the pending effect after
earlier response windows; declining instead passes that optional trigger.

Verified entry-removal enrollment: 25 new card records and 1,030 cases. All
pass natively and in Wasm with identical observable states, including optional
decline and mandatory self-return. No new card failure baseline was added.
Headless input now selects optional triggers through earlier response windows,
honors legal friendly targets for either controller, and restores named-instance
bindings with snapshots. Production gameplay and AI targeting policy are unchanged.

### Creature entry recovery to hand

`etb_recovery_creature` takes explicit `targets`, `type_match`, `exclude_types`,
`optional`, and `keywords`. The full Oracle text must contain exactly the
reviewed, unconditional entry recovery clause. Union types, an enchantment
creature intersection, any card, and noncreature/nonland exclusions are
distinct declarations. Other triggers, replacement effects, variable stats,
restricted colors/subtypes, and alternative costs need dedicated cases.
Ordinary activated abilities and passive keywords are reported as untested.

Both controllers use real casts and graveyard selection with several legal
alternatives. Cases include each permitted type, exact payment, insufficient
or wrong mana, empty and opposing graveyards, wrong types, optional decline,
Ground Seal protection, countering, cleanup, source destruction, bounce/recast,
blink and ready/pending/resolved snapshot replay after AI searches. Populated
hands, libraries, graveyards and exile are checked, including duplicate and
absent card counts in each hand/graveyard separately and ordered libraries.
Returning to the battlefield, graveyard targets moving after selection, trigger
ordering and additional abilities remain explicit gaps.

Verified entry-recovery enrollment: 18 new card records plus Gravedigger receive
886 cases. Native x86 and Wasm each pass 732 and reproduce 154 rules failures
with identical observable states. A shared entry-callback guard prevents
Anarchist, Cartographer, Scrivener, Treasure Hunter and Stoic Builder from
recovering cards; Auramancer and Mnemonic Wall force an Oracle-optional choice.
Strict failure signatures preserve all 1,048 prior baselines. Headless input
observes whether a zone choice is mandatory and tests count duplicates and
absent cards separately in each zone. Production rules remain unchanged.

### Fixed creature death receipts

`death_value_creature` takes explicit `draw`, `gain_life`, `optional` and
`keywords`. It admits a single fixed death clause plus reviewed unconditional
keywords on a normal creature with bounded numeric stats. Extra entry/upkeep
triggers, variable receipts, replacement effects and additional abilities fail
closed. The whole Oracle text is pinned; keywords are reported as untested
components rather than implied whole-card coverage.

Both controllers cast the source, then exercise destruction, Murder, lethal
damage, zero toughness and combat death where applicable. Casting, countering,
exile, bounce, blink and another creature dying must not produce the death
receipt. Cases cover exact/insufficient/wrong mana, injured controllers, two
copies, cleanup, death while tapped by a real spell, surviving damage, stocked
or bare zones, short ordered libraries, and snapshot replay after AI searches.
Exact per-zone multiplicities check both receipt and absence. Human optional
decline is separate from the AI's normal accepted-draw policy; the AI is not
expected to refuse a beneficial optional trigger without a decision hook.
Sacrifice costs, control changes, trigger ordering, replacement effects and
empty-library draw loss remain explicit gaps.

Verified death-receipt enrollment: 15 new card records plus Onulet receive 783
cases. All pass natively and in Wasm with identical observable states. No
failure baseline or production rules change was needed. The AI's decision to
accept a beneficial optional draw is retained as policy, while human decline
uses the real pending trigger path.

### Passive keywords beside activated abilities

`passive_keyword_creature` supplies explicit `keywords` and verbatim
`activated_abilities`. The whole Oracle text is pinned. Every nonactivated
clause must be an unconditional keyword already reviewed by `creature_keywords`;
conditional keywords, other triggers, static effects, variable characteristics,
alternative costs and modal continuation lines require another recipe. At
least one passive keyword and one ordinary activated clause are required.

The existing keyword combat scenarios run for both controllers, with stocked
hands, libraries, graveyards and exile. Exact per-zone counts include absent
cards; libraries retain their order. Snapshot replay checks the same zones
before recording and after an AI round trip. All activated clauses are reported
as untested and are not intentionally activated by these scenarios. This is
passive-component coverage, not a certification of those separate abilities.
Flash timing, gaining or losing keywords, control changes and additional combat
interactions remain explicit gaps. Enrollment avoids duplicating cards whose
existing creature recipes already exercise this passive layer.

Fixture cards are considered available in the test collection. Adding a card
grants only its mapped challenge unlock in memory before invoking its handler;
it does not create unlock files or alter production challenge rules. The test
snapshot preserves these availability flags. Dedicated cases mutate and restore
Kiki-Jiki's flag and verify that a restored fixture can cast the real card.

The first enrollment adds 4,902 behavior cases across 217 records (212 new card
homes). Native and Wasm both pass 4,874 cases; 28 retain correct fresh-attack
expectations for 14 haste creatures affected by the shared no-attack sickness
flag. Their baselines require exact assertion counts and traces. These cases
exercise the passive keyword component, not the recorded activated abilities.

Two additional availability scenarios cover fixture casting and caller-owned
snapshot restoration, including AI searches. The availability correction also
makes six existing Leatherback Baloth cases pass on both platforms; their old
challenge-lock fixture baselines are retired. Production challenge and card rules
are unchanged. The suite now has 60,936 behavior cases across 2,739 cards.

### Targeted temporary stat abilities

`target_stats_activation` requires explicit source `keywords`, signed `power`
and `toughness` changes, `targets`, `mana_cost`, `tap`, and `sacrifice` costs.
The whole Oracle text must contain exactly one reviewed ability plus supported
unconditional keywords. Other abilities, triggers, variable amounts and complex
restrictions require separate cases. Supported target scopes cover creatures,
flying or green creatures, Dwarves, Soldiers, attackers, blockers, either combat
role, and attacking Humans. Self-sacrifice refers to the source creature only.

Both controllers use stocked zones and exact per-zone multiplicities, including
absent cards, with ordered libraries. Cases check activation and casting costs,
wrong mana, tapping and sickness, Suppression Field, legal friendly/opposing
targets, shroud, lost targets and sources, stacking, two selected recipients,
repeated activations after untapping, cleanup, negative power and zero-toughness
death. Costs take effect before resolution while the recipient retains its base
stats. Combat restrictions use normal declarations and `advance_combat`; combat
damage and cleanup verify the temporary effect's gameplay consequence and expiry.
Ready, pending and resolved snapshots preserve the effect and zone assertions
after AI round trips. Self-targeting, blink identity, control changes, extra
keywords on recipients and multiple restricted combat targets remain explicit
gaps. Source keyword mechanics have their separate passive-component cases.

The initial batch adds 1,586 cases across 26 cards (19 new homes). Every case
passes natively and in Wasm with identical observable states; the 1,224 existing
failure baselines remain unchanged. The suite now has 62,522 behavior cases
across 2,758 cards.

Reviewed registry identity classifications live in `data/registry_kinds.json`.
CSVID 910, named Assembly-Worker, is Mishra's Factory's animated artifact creature
land form. Its casting and mana abilities differ from the printed `{3}` creature
with the same name. The record is classified as an alternate; ordinary Oracle
recipes and normal-card smoke enrollment exclude it. Authored Factory scenarios
can still reference the form. Classification entries require an exact existing
name, a reviewed nonprimary kind and a reason; stale IDs or names fail closed.
The discarded printed-card assumptions are not rules failure baselines.

`target_keyword_activation` covers a single fixed temporary keyword ability on
a normal creature with optional unconditional source keywords. Its explicit
parameters are `keywords` (source), `grant`, `targets`, `mana_cost`, and `tap`.
The complete pinned Oracle clause must match the reviewed cost, scope, effect
and reminder text; additional effects or abilities require separate cases.
Costs use structured mana symbols rather than inferred prose. Reviewed scopes
include any creature, your creature, specified colors, Beasts, Humans, and power
5 or greater. Each selected variant runs independently for both controllers.

The probes use normal combat, destruction and concrete target validation:
flying evasion, first/double strike survival, excess trample damage, deathtouch
deaths, lifelink life gain, vigilance tapping, fresh-creature haste attacks,
indestructible destruction and shroud against both players' targeting sources.
Haste expectations check actual attacks and tap activations; they do not require
raw summoning-sickness flags to be cleared.
Unselected recipients retain their original behavior. Expiry probes repeat the
behavior after cleanup, except haste expiry across turns, which remains a gap.
Casting, costs, wrong mana, sickness, tapping, Suppression Field, lost sources
and targets, stacked activations and exact zone receipts are also checked.
Ready, pending and resolved snapshots replay after real AI backup/restore.

Repeat probes remove alternative recipients before checking availability and
queuing another selection. Pixie Queen's player-1 repeat-selection variant is
deferred: its AI flying filter rejects the already flying legal choice while
aggregate availability remains true. Do not baseline that harness diagnostic
as a rules assertion. Its player-0 repetition and player-1 shroud permission
regression remain covered. Self-targeting, multiple recipients, control changes,
blink identity, recipient keyword interactions, indestructibility against lethal damage and zero toughness
and source passive-keyword mechanics remain explicit coverage gaps.

The Wasm test target reserves 384 MiB initially: its generated case tables have
grown beyond the previous 256 MiB linker minimum. This is separate from the
game's memory settings; runtime memory growth remains enabled.

The first keyword-activation batch adds 1,455 cases across 24 cards, including
17 new homes. Native and Wasm each have 1,222 passes and 233 precisely baselined
assertion cases, with identical recorded states. The assertions expose four
issues: granted haste's attack gate, the Mage controller restriction, Bloodthorn
Taunter's extra sickness restriction and Pixie Queen's AI shroud mask. The 1,224
prior baselines remain unchanged; no production rules were changed. The suite
now has 63,977 behavior cases across 2,775 cards. Validation includes 651 Python
guards, 52 CTest checks per platform, 158 existing control cases, registration
and shuffled-order checks, and independent generated-data hashes.

`self_keyword_activation` covers one ordinary mana ability that grants its
creature a fixed keyword until end of turn. Parameters are `keywords` (the
source's unconditional keywords), `grants` (an explicit list), and `mana_cost`.
The complete pinned Oracle clause, reminder and structured cost must match.
Reviewed grants are flying, first strike, deathtouch, lifelink, vigilance,
reach, shroud, hexproof, indestructible and menace, plus the explicit vigilance
and lifelink combination. Sources may have flying or defender; other source
keyword combinations require separate combat expectations. Additional costs,
restrictions, abilities and effects fail closed.

The ability uses the real activation and stack paths without a target choice.
Both controllers receive independent cases for exact payment, insufficient and
wrong-colored mana, Suppression Field, tapped and summoning-sick sources,
freshly cast sources, opposing turns, repeated and stacked activations, source
loss before resolution, an unaffected creature and cleanup expiry. A legal
Robe of Mirrors setup checks that shroud does not prevent a nontargeted ability;
if its creature dies, the Aura's graveyard receipt is checked too.

Gameplay probes check evasion and reach, first strike through return damage,
deathtouch through both deaths, lifelink and vigilance through life and tapping,
and shroud versus hexproof through concrete targeting by both players.
Indestructibility cases distinguish destruction, lethal damage, zero toughness,
exile and bounce. Menace uses completed blocker declarations, including a
two-blocker variant; that variant deliberately stops before damage assignment.
Stocked and empty zones, exact multiplicities, library order and ready, pending
and resolved snapshots after AI round trips are covered. Control changes,
blink identity, extra keyword interactions, source passive mechanics and
multiple simultaneous damage assignments remain explicit gaps.

The first self-keyword batch adds 1,948 cases across 37 cards (28 new homes).
All cases pass natively and in Wasm with identical observable states. The 1,457
existing failure baselines remain unchanged; no production rules were changed.
Validation covers 668 Python guards, 78 CTest checks per platform, 166 existing
control cases, discovery and shuffled-order checks, and independent generation
hashes. The suite now has 65,925 behavior cases across 2,803 cards.

`token_creation` covers a fixed group of identical creature tokens made by an
ordinary instant/sorcery or a creature's entry trigger. Explicit parameters are
the source's `keywords` and a `token` definition containing `name`, `count`,
`power`, `toughness`, `colors`, `subtypes`, `artifact` and `keywords`. The entire
pinned creation clause must match. Token characteristics come from that clause;
the same token name in the bulk corpus may represent a different variant.
The registry supplies its CSVID identity. Counts and characteristics are bounded;
additional clauses, variable counts and mixed token groups fail closed.

Both controllers receive cases for casting, exact payment, insufficient and
wrong-colored mana, countering, repeated casts, unrelated creature entry,
cleanup, stocked and empty zones and ready, pending-spell and resolved snapshots
after AI round trips. Creature makers also cover destruction, bounce, re-entry,
blink and blink snapshots. Battlefield fixture loading uses the real
`put_into_play` path: its entry trigger creates tokens without a spell or mana
payment. Tokens persist when their maker leaves; they cease to exist after
their own destruction, bounce or exile. Wrath checks distinguish nontoken
graveyard receipts from vanished tokens.

Every copy receives stat, color, artifact-status, subtype and token-status
checks. Fresh attacks test haste through actual declarations, without asserting
raw sickness flags. Ready attacks use normal turn advancement. Combat probes
cover ground and flying blockers, flying evasion and reach, trample excess,
deathtouch deaths and vigilance tapping. Only one token participates in each
damage scenario. Exact zone counts and multiplicities include token absence in
hands, graveyards and exile, plus ordered libraries and both players' life and
mana. Test-only operations bind a particular duplicate permanent or inspect all
permanents matching a CSVID; missing matches fail rather than passing vacuously.

Source passive abilities, a separate priority window for the entry trigger,
control changes, copying, token doublers, multiple simultaneous damage
assignments and additional card types remain explicit gaps. Conditional counts,
Stangg's linked token, mixed groups, delayed token removal and Spectral
Reserves' additional life gain remain outside this recipe.

The first token-creation batch adds 1,512 cases across 33 newly covered cards.
All pass natively and in Wasm with identical recorded states. The 1,457 existing
failure baselines remain unchanged. Validation includes 689 Python guards,
70 CTest checks per platform, 174 existing controls, discovery, shuffled order
and independent generation hashes. The suite now has 67,437 behavior cases
across 2,836 cards. Ten Wasm processes required serial retries after memory
pressure: seven explicit instantiation OOMs and three no-output Node fail-fast
exits. These diagnostics are archived separately, never treated as rules
failures. Keep full data generation separate from Node test execution.

### Fixed tap and untap abilities

`tap_activation` accepts a single complete `Tap target …` or `Untap target …`
ability on an ordinary artifact, enchantment, or creature with reviewed static
Flying/Defender keywords. Other source keywords fail closed until reviewed.
Version 2 declarations pin the whole Oracle text and explicitly give
`keywords`, `effect` (`tap` or `untap`), `targets`, `mana_cost`, and a boolean
`tap` cost. For example, Seeker of Skybreak uses `effect: "untap"`,
`targets: "creature"`, `mana_cost: ""`, `tap: true`, and `keywords: []`.
Extra clauses and separate mana modes fail closed.

Reviewed targets include creatures, flying/nonflying creatures, green and
non-Human creatures, power/toughness thresholds, Walls, artifacts, artifact
creatures, enemy artifacts, lands, Forests, Gates, any permanent, and the
artifact/creature/land union. Target witnesses and response spells are pinned
independently of engine handlers. Both controllers receive legal friendly or
enemy targets, redundant tap/untap operations, unselected alternatives, exact
payments, tax and insufficient-payment checks, source readiness and casting,
repeat activations, removal or shroud in response, cleanup persistence, stocked
and empty zones, and ready/pending/resolved snapshots with intervening AI work.
Selected scopes also exercise self-targeting and actual mana production after
untapping a Forest or Sol Ring.

Combat uses normal declarations and damage. Tapping before declarations prevents
an attack or block; untapping permits it. Tapping an already-declared attacker
or blocker does not remove it from combat or suppress its damage. Walls receive
blocking probes rather than illegal attacks. Every final board checks exact zone
counts and multiplicities, ordered libraries, both players' mana and life, and
the selected and unselected permanents.

Negative availability cases protect an eligible creature source with a legally
cast Robe of Mirrors. A broad artifact source can still target itself, so scopes
without a reviewed way to remove that alternative omit those negative probes.
Additional effects, alternate mana modes, noncreature-target shroud, changing
power/toughness restrictions while an ability is pending, control changes,
restricted-scope self-targeting, and independent source keyword behavior remain
explicit gaps. Maze of Ith and the other tap/untap cards with additional effects
require separate recipes or authored cases.

Gate fixtures run the real entry path, including the Guildgate's tapped entry.
The already-ready probe untaps both Gates with legal Twiddle spells first.
The repeated Gate probe retaps its target with Twiddle before the second
activation, so it tests reuse without forcing an AI-ineligible choice.
Greenside Watcher's redundant-untap probe remains active for player 0; the
player 1 version is explicitly deferred because its AI policy filters ready
Gates. This is a decision-policy gap, not a known rules failure.

The first tap/untap batch adds 3,047 cases across 45 cards, including 37 new
card files. Native and Wasm observations, outcome kinds and assertion counts
match for every case: 3,035 pass and 12 expose shroud defects in Cephalid
Retainer (8 cases) and Flood (4). These failures retain correct assertions and
exact signatures; production rules are unchanged. All 1,457 prior baselines
and 84,650 prior manifest entries are preserved. The suite now contains 70,484
behavior cases across 2,873 cards.

Validation includes 714 Python guards, 94 CTest checks per platform, 182 prior
controls, 12 discovery filters per platform, two shuffled four-case orders per
platform, and independent generation hashes. The fixture audit proves 3,044
scenarios unchanged, three legally corrected Gate setups, and one documented
deferred AI-only probe. Both build caches are restored to typed OFF/ON/OFF
settings. The detailed report is
`out/duel-tests/recipe-tap-activation/validation.json`.

### Fixed targeted damage abilities

`damage_activation` accepts one complete fixed-damage ability on an ordinary
artifact, enchantment, or creature with reviewed Flying, Defender, or Haste
keywords. Version 2 pins the whole Oracle text and explicitly supplies
`keywords`, `damage`, `targets`, `mana_cost`, `tap`, `sacrifice`, `pay_life`,
`damage_you`, `damage_self`, `controller_life_loss`, and `tap_target`.
Boolean costs and effects must be booleans; damage and life quantities must be
integers. Ordinary mana, tapping, paying life, and sacrificing the source are
supported costs. At most one reviewed collateral effect is allowed.

For example, Prodigal Sorcerer uses `damage: 1`, `targets: "any"`,
`mana_cost: ""`, `tap: true`, `sacrifice: false`, `keywords: []`, and zero/false
for every remaining effect or cost. Brothers of Fire instead uses
`mana_cost: "{1}{R}{R}"`, `tap: false`, and `damage_you: 1`. Mogg Fanatic uses
`sacrifice: true` and `tap: false`. Expectations use these explicit values and
the independent Oracle pin, never the handler's implementation.

Reviewed target scopes are any target, creature, flying creature, attacking or
blocking creature, attacking flying creature, player, and opponent. Player and
opponent wording may mention planeswalkers, but planeswalker targeting remains
an explicit coverage gap. Variable damage or costs, discard, sacrificing other
permanents, sweep damage, random choices, modal abilities, extra clauses, and
unreviewed source keywords fail closed.

Cases cover both controllers, selected and unselected alternatives, friendly
targets and self-targeting where legal, lethal and indestructible targets,
source readiness, actual casting, exact payment, wrong colors, insufficient
mana or life, Suppression Field, repeated or stacked activations, legal untap
and reuse, and source or target removal. Costs are checked before resolution.
Safe Passage tests damage prevention; Gilded Light tests player shroud and the
flash aura Alexi's Cloak tests creature shroud in response. Preventing damage
does not prevent a separately specified life loss
or tapping effect. Losing every legal target prevents collateral effects too.
Haste is checked through actual activation permission rather than assuming the
engine clears its raw summoning-sickness flag.

Combat-restricted targets use normal attacker/blocker declarations and phase
advancement. Damage to an attacker is combined with real combat damage; killing
a declared blocker does not make its attacker unblocked. Cleanup clears marked
damage without untapping. Both players have stocked hands, graveyards, exile,
and ordered libraries; deliberately empty variants remain separate. Every
final board checks mana, life, permanent state, exact zone counts and card
multiplicities. Ready, pending, and resolved snapshots are replayed around AI
backup work and compared for equivalent state.

Regeneration, color protection, damage redirection, planeswalkers, pending
target-characteristic changes, independent static keyword behavior, alternate
modes, and additional costs or effects require separate cases. High-damage
survivor Krosan Cloudscraper is used only in scenarios that do not advance
through its upkeep; its upkeep and morph rules are not asserted here.

Rakdos Ickspitter's positive targeting drafts are kept in
`data/pending_cases/cards/082/08269-rakdos-ickspitter-damage.json`: its missing
targeting flag leaves scripted target decisions unconsumed. These 60 drafts
are not active tests or new rule baselines. Four negative-availability probes
remain active, alongside its previously authored regression. Restore the
pending probes when the targeting path can consume their legal choices.

The first fixed-damage batch adds 3,512 active cases across 58 cards, including
27 new card files. Every native/Wasm observation matches: 3,452 pass and 60
expose reviewed defects. Elite Archers' literal-prompt flag is passed in a mana
argument, adding two white mana to its printed tap-only cost (50 cases).
Shivan Hellkite (4 cases) and Razortip Whip (2) resolve damage against targets
that gained shroud. Rakdos Ickspitter (4) permits activation without a legal
creature target. Exact assertion signatures are baselined; production fixes
remain separate. All 1,469 prior baselines and 87,697 prior manifest entries
are preserved. The suite now contains 73,996 behavior cases across 2,900 cards.

The fixture review replaces 100 non-flash shroud responses with legal Alexi's
Cloak responses and its Oracle mana cost; 3,412 other scenarios are unchanged.
The review also records an audit follow-up for the same shortcut in older
recipes. Pending targeting drafts remain outside the active catalog.

Validation includes 737 Python guards, 120 CTest checks per platform, 198 prior
controls, 12 discovery filters per platform, two shuffled four-case orders per
platform, and independent generation hashes. The controls retain 185 matching
recorded states, two matching pre-existing crashes, and 11 pre-existing
native-only faults whose Wasm counterparts pass. Both build caches are restored
to typed OFF/ON/OFF settings. The detailed report is
`out/duel-tests/recipe-damage-activation/validation.json`.

### Mana production with life consequences

`mana_consequence` uses a whole normal-card Oracle pin and explicit version 2
parameters: `production`, `outputs`, `colored_damage`, `fixed_damage`,
`life_payment`, `opponent_life`, and `enters_tapped`. It admits ordinary
noncreature lands and artifacts with exactly one bounded life consequence.
The reviewed production forms are `colorless_choice`, `colorless_any`, `fixed`,
and `any_color`. Every ordinary output is listed explicitly and ordered as in
the printed ability. Extra clauses, keywords, variable costs, and unsupported
production forms fail closed.

For example, Adarkar Wastes uses `outputs: ["{C}", "{W}", "{U}"]`,
`production: "colorless_choice"`, and `colored_damage: 1`; colorless production
does not cause damage. Ancient Tomb uses one `{C}{C}` output and
`fixed_damage: 2`. Mana Confluence uses all five ordinary colors and
`life_payment: 1`. Grove of the Burnwillows uses `opponent_life: 1` for colored
production. Other consequence quantities are explicitly zero.

Both controllers exercise every output, immediate mana production with an
empty payment pool, prevention, low but nonlethal life totals, taxes, repeat
activation, legal untap and reuse, independent sources, and exact mana spending
on real colored or generic-cost spells. Safe Passage prevents damage; it
does not prevent paying life or an opponent gaining life. Incidental tapping,
including retapping after colored production, must not repeat a consequence
that belongs to the mana ability's effect. Source removal leaves produced mana
in the pool. Artifacts also receive casting, exact casting payment, insufficient
mana, and fresh-activation probes; their tap abilities do not acquire creature
summoning-sickness restrictions.

Tapped-entry lands use the real entry path and legal Twiddle spells to become
ready. Combat probes use normal declarations and phase advancement. Ready and
post-production snapshots replay around AI backup work; mana abilities resolve
immediately rather than creating a pending stack object. Both players' hands,
graveyards, exile, and ordered libraries receive exact contents checks, with
separate empty-zone variants.

Land play permissions, automatic payment-source selection, lethal life totals,
multiplayer, damage redirection, counterspell responses to artifact casting,
and cards with additional costs or clauses require separate cases. City of
Brass' separate triggered damage clause is outside this recipe.

The first mana-consequence batch adds 2,582 cases across 25 newly covered cards.
All pass natively and in Wasm with identical recorded states. The same batch
replaces 178 pending non-flash shroud responses across the targeted-stat,
targeted-keyword, and tap/untap families with legal Alexi's Cloak responses.
The audit covers 89 card records: 174 corrected cases pass and four retain the
existing Flood/Cephalid Retainer failures, with matching states, outcome kinds,
and assertion counts. The other 5,910 scenarios in those families are unchanged.

The generic-payment fixture review corrects 114 cases that expected a negative
colorless balance after spending colored mana on a generic cost. A complete
six-mana pool pays Phyrexian Hulk's `{6}` cost and leaves every pool empty;
2,468 other new scenarios are unchanged. These draft expectation failures
were not added to the failure manifest. All 1,529 existing baselines and
91,209 prior catalog entries are preserved. The catalog now has 93,791 entries,
including 76,578 behavior cases across 2,925 cards. Production rules and the
headless engine interface are unchanged.

The mana-consequence audit includes 764 Python guards, 60 CTest checks per
platform, 262 prior controls, 12 discovery filters per platform, two shuffled
four-case orders per platform, and independent generation hashes. Controls
retain 249 matching recorded states, two matching pre-existing crashes, and
11 pre-existing native-only faults whose Wasm counterparts pass. Both build
caches are restored to typed OFF/ON/OFF settings. The detailed report is
`out/duel-tests/recipe-mana-consequence/validation.json`.

### Conditional-entry mana lands

`conditional_mana_land` pins a whole normal-card Oracle record and explicitly
supplies `outputs`, `condition`, `limit`, and `types`. Its entry predicates are
`at_most_other_lands` with `limit: 2`, `at_least_basics` with `limit: 2`, and
`land_types` with `limit: 0` and two named qualifying basic land types. The two
ordinary colored outputs are listed in their printed order. The complete
Oracle text must contain exactly the reviewed entry rule and mana ability;
additional clauses, faces, keywords, and unreviewed predicates fail closed.

Fast-land fixtures cover zero through four other lands, basic and nonbasic
lands, nonland permanents, opponent-controlled lands, and sequential entry at
the third/fourth-land boundary. Battle-land fixtures distinguish basic
supertypes from basic land subtypes, count two copies of the same basic name,
and reject mixed basic/nonbasic pairs. Check-land fixtures accept either named
type independently and a nonbasic dual with qualifying types, while rejecting
wrong and untyped lands. Matching cards in hands, graveyards, exile, and
libraries do not satisfy a battlefield condition.

Fixture order controls which lands exist when the source enters through the
real engine path. Ordinary cases do not override the source's tapped state.
Late qualifying lands must not untap an already-entered source; removing
qualifiers does not retap a source that entered ready. Reducing the land count
does not untap a fast land that entered tapped. Production cases first check
entry state, then use a legal Twiddle when untapping is necessary.

Both controllers exercise both outputs on every entry board. Additional cases
cover empty payment pools, taxes, noncreature sickness flags, opposing turns,
combat, repeat activation, independent sources, incidental tapping, source
removal, and actual colored/generic spell payment. Entry, ready, and
post-production snapshots replay around AI backup work. Final assertions cover
exact battlefield and zone multiplicities, ordered libraries, mana, life, and
permanent state. Separate variants deliberately leave the nonbattlefield zones
empty.

Land play permissions, re-entry and control changes, type-changing effects,
interacting entry replacement effects, automatic payment-source selection,
and multiplayer require separate cases. Optional life payment, revealing a
card as a land enters, and filtering mana are outside this recipe.

Ordered fixture placement refreshes the engine's cached land counts between
permanents. Fast lands query that cache during entry, so refreshing only after
the complete board was loaded produced 28 misleading failures in the initial
434-case pilot. The narrow headless-host correction resolved those failures
without changing the card handler or overriding tapping. The previous mana
batch is replayed to check compatibility with that host change.

Generated setup arrays, action arrays, and case descriptors now live in bounded
C++ data shards rather than one large translation unit. The registration
table retains the same case pointers and order. A definition-hash audit
compares every case with the original monolithic output, and the build links
only the generated source inventory. Unchanged shards keep their timestamps;
stale generated files are left out of the build. This changes test-data storage
without changing rules, assertions, or the engine testing interface.

This batch adds 3,620 cases across 25 newly covered lands. Every case passes
natively and in Wasm with identical observable state. All 2,582 prior
mana-consequence cases pass again on each platform, and their 5,164 recorded
observations are unchanged. The 1,529-entry failure manifest and 93,791 prior
catalog entries are preserved. The suite now contains 80,198 behavior cases
across 2,950 cards, with 97,411 total catalog entries.

Validation includes 794 Python guards, 61 CTest checks per platform, 268 prior
controls, 12 discovery filters per platform, and two shuffled four-case orders
per platform. Controls retain 255 matching states, two matching pre-existing
crashes, and 11 pre-existing native-only faults whose Wasm counterparts pass.
All 61 generated files match independent generation. The sharding audit
preserves every one of 80,198 case definitions and the full manifest; 55 data
shards each stay below 8 MiB. Both build caches are restored to typed
OFF/ON/OFF settings. The report is
`out/duel-tests/recipe-conditional-mana/validation.json`.

### Continuous self-stat bonuses from counts

`conditional_stats` pins a whole normal creature record and explicitly supplies
`keywords`, `condition` (`metalcraft` or `threshold`), `power`, and `toughness`.
The complete text must contain exactly the reviewed self-stat clause and
supported unconditional Flying, Reach, Shroud, or Haste keywords. Additional
activated or triggered abilities, conditional keyword grants, other static
effects, faces, and unreviewed reminders fail closed. Werebear's separate mana
ability remains a documented deferred candidate.

Metalcraft boards cover zero through four controlled artifacts and count an
artifact-creature source itself. Threshold boards contain zero, one, five,
six, seven, or eight real graveyard cards. Both controllers receive casting,
effect, actual unblocked combat, snapshot, and repeated-recalculation cases on
every count board. Opponent-only and offboard cases distinguish ownership and
zone scope; duplicate names still represent multiple cards. Empty nonbattlefield
zones remain deliberate variants. Recalculation must not stack the bonus, and
cleanup must not expire a continuous effect whose condition is still true.

Transitions cross a boundary by casting Sol Ring or Twiddle, by a creature
dying, by an artifact being destroyed, or by Morningtide exiling graveyards.
Morningtide itself reaches its owner's graveyard after its effect. Tremor
tests loss of a toughness bonus with marked damage already present, including
state-based death. These spells do not target a shrouded source. Independent
sources share the count while retaining their own state. All final boards
check life, mana, permanent state, exact zone multiplicities, and ordered
libraries; snapshots restore them around AI backup work.

Independent static keyword rules, type/control changes, other continuous
modifiers, additional abilities or triggers, graveyard replacement effects,
and multiplayer require separate cases. The recipe constructs boundary
graveyards as real initial zone cards from the empty reusable fixture and
fills both players' other zones explicitly.

The first continuous-stat batch adds 780 cases for ten newly covered creatures.
Every case passes natively and in Wasm with identical states, outcome kinds,
and assertion counts. The 1,529 existing baselines and 97,411 prior catalog
entries are preserved. The suite now has 80,978 behavior cases across 2,960
cards and 98,191 total catalog entries. Card rules and the headless engine
interface are unchanged.

Validation includes 818 Python guards, 31 CTest checks per platform, 272 prior
controls, 12 discovery filters per platform, and two shuffled four-case orders
per platform. Controls retain 259 matching states, two matching pre-existing
crashes, and 11 pre-existing native-only faults whose Wasm counterparts pass.
All 62 generated files match independent generation, and data shards remain
bounded below 8 MiB. Both build caches are restored to typed OFF/ON/OFF
settings. The report is
`out/duel-tests/recipe-conditional-stats/validation.json`.

### Continuous conditional combat keywords

`conditional_keyword` supplies explicit `keywords`, `condition` (`metalcraft`
or `threshold`), `power`, `toughness`, and `grant` parameters, alongside the
whole Oracle pin and an explicit variant list. The first scope supports Flying,
Double strike, or Trample, with either no stat bonus or a positive paired bonus.
An unconditional Vigilance clause is also supported. For example, Auriok
Sunchaser declares `keywords: []`, `condition: "metalcraft"`, `power: 2`,
`toughness: 2`, and `grant: "Flying"`. Additional clauses and protection rules
require dedicated cases; Mystic Crusader and Mystic Enforcer are deferred.

The recipe reuses the continuous-stat fixture builder for real artifact and
graveyard counts. Each ready-board, casting, count-transition, recalculation,
and cleanup scenario then probes the granted ability through a legal attack,
blocker declaration, and normal combat damage. Ground, flying, and reach
blockers check evasion; unblocked, large-blocker, and first-strike scenarios
check both damage steps of double strike. Trample checks lethal damage assigned
to a blocker and excess damage dealt to its controller. Vigilance checks the
attacker's tapped state. Casting scenarios first reject a fresh attack, then
use the normal turn gate twice before testing combat.

Snapshots capture the ready board, execute combat, run AI backup work, restore,
and execute the same combat again. Every final board checks ownership, zone
multiplicities, ordered libraries, mana, life, and permanent state. The combat
expectations use pinned Oracle characteristics without consulting handlers or
injecting ability events. Defending with the source, multiple blockers,
control/type changes, other continuous modifiers, additional abilities,
graveyard replacement effects, and multiplayer remain explicit coverage gaps.

The first conditional-keyword batch adds 758 cases across eight newly covered
cards. Every case passes natively and in Wasm with identical observable states.
The 1,529 existing baselines and 98,191 prior catalog entries are unchanged.
The suite now has 81,736 behavior cases across 2,968 cards and 98,949 total
catalog entries. Production rules and the engine testing interface are unchanged.

Validation includes 843 Python guards, 27 CTest checks per platform, 276 prior
controls, 12 discovery filters per platform, and two shuffled four-case orders
per platform. Controls preserve 263 matching states, two paired old crashes,
and 11 old native-only faults whose Wasm counterparts pass. All 62 generated
files match independent hashes; the 56 data shards stay below 8 MiB. Both build
caches are restored to typed OFF/ON/OFF settings. Lossless NTFS compression of
generated data and test artifacts recovered build space without deleting files.
The report is `out/duel-tests/recipe-conditional-keywords/validation.json`.

### Multiple tap-for-mana modes

`mana_modes` declares two or three complete printed modes on a normal Land or
Artifact. Each mode supplies `cost`, `production`, `outputs`, `choices`, and
`fallback_choices`. The first mode is the ordinary free `{T}: Add {C}.`
fallback. Other costs admit one to five generic mana or one ordinary two-color
hybrid symbol plus tapping. Outputs admit one to five explicit ordinary mana
symbols, fixed alternatives, or one mana of any color. The full Oracle text
must match every declared mode; restrictions, triggers, other costs, entry
effects, and additional clauses require separate cases.

For example, Mystic Gate's second mode declares `cost: "{W/U}, {T}"`,
`production: "choice"`, and `outputs: ["{W}{W}", "{W}{U}", "{U}{U}"]`.
Both printed input colors fund every output, with exact payment assertions.
The decision scripts are UI bindings, separate from those rules expectations:
`choices[output][controller][payment]` selects a bounded color/dialog script,
and `fallback_choices[controller]` handles the unfunded colorless path. Human
filter-land menus couple payment and output choices; the AI path requests the
two output colors separately. Crystal Quarry's free mode has its own colorless
subchoice. These bindings are validated without deriving expected costs or
outputs from card handlers.

Both controllers exercise output production with reserves, exact payment,
immediate resolution, repeated-tap rejection, a real Twiddle and repeated
activation, taxes, colored/generic spending, ready and produced-mana snapshots,
and owned populated zones. Common cases cover entry, incidental tapping,
noncreature sickness, combat, opposing turns, source loss, deliberate bare
zones, and the zero-pool colorless fallback. Artifact sources also receive
casting, exact casting cost, insufficient mana, and fresh activation cases.
All final boards check life, six mana amounts, permanent state, exact zone
multiplicities, and ordered libraries.

The AI filter-land path prioritizes filtering when colored funding exists.
Its free-mode fixtures therefore supply only C, while human free-mode fixtures
also test selecting C with both modes funded. Generic affordability considers
colored mana too. Repeated-activation cases assert the first output, legally
cast Twiddle, and install the next explicit payment pool before activating
again. Automatic payment-source selection, mode-specific eligibility queries,
offered-color restrictions, AI selection of free C with colored funding,
hybrid input choice with both colors simultaneously available, multiple-source
chaining, land-play permissions, and multiplayer remain coverage gaps.

The first multi-mode mana batch adds 2,368 cases across 19 newly covered cards.
All pass natively and in Wasm with identical observable states. The 1,529 prior
baselines and 98,949 prior catalog entries are preserved. The suite now has
84,104 behavior cases across 2,987 cards and 101,317 total catalog entries.
Production rules and the engine testing interface are unchanged.

The initial 430-case pilot exposed decision-binding and fixture-affordability
mistakes. Correcting those scripts produced 430 passing pilots without changing
the pinned Oracle costs or outputs or baselining draft failures. The review is
`out/duel-tests/recipe-mana-modes/pilot-review.json`.

Validation includes 870 Python guards, 48 CTest checks per platform, 280 prior
controls, 12 discovery filters per platform, and two shuffled four-case orders
per platform. Controls retain 267 matching states, two paired old crashes, and
11 old native-only faults whose Wasm counterparts pass. All 65 generated files
match independent hashes; 59 data shards stay below 8 MiB. Both build caches
are restored to typed OFF/ON/OFF settings. Lossless compression of prior audit
snapshots recovered space after a disk-full Wasm link; the retry and all Wasm
execution checks passed. The report is
`out/duel-tests/recipe-mana-modes/validation.json`.

## Printed creature entry and attack conditions

`creature_conditions` version 2 uses explicit `keywords`, `enters_tapped`, and
`attack_land` parameters. The latter is either null or an object specifying
`type` (Island or Mountain) and `scope` (defender or battlefield). Parameters
must match the complete pinned Oracle text of a normal creature with numeric
power and toughness. Additional abilities fail closed; this family supports
only the reviewed Flying/Trample keywords and entry/attack clauses.

Both controllers exercise real casting and payment, entry tapping, sickness,
legal Twiddle untapping, ground/flying blocks, combat damage, empty-zone
variants, cleanup, and snapshot restoration after AI backup searches. Land
restrictions additionally cover ownership, wrong/missing/offboard types,
tapped lands, snow and dual lands, removal before/after attack declaration,
and a legal Spreading Seas or Blood Moon that supplies the required type.
Spreading Seas includes its draw in expected hand and library contents.

Boards check life, all mana pools, named permanent state, zone counts, exact
card multiplicities, and ordered libraries. Battlefield enumeration exposes
current type-change CSVIDs. For the changed qualifying land only, identity
checks use its stable reference and zone count; all other identities and
zones remain checked. Printed identity under these type changes is an explicit
gap, alongside land-play timing, other type-changing effects, control changes,
multiple blockers, trample overflow, additional clauses, and multiplayer.

The first creature-condition batch adds 702 cases across 13 newly covered
cards. All pass natively and in Wasm with identical observable states. Coverage
now totals 84,806 behavior cases across 3,000 cards and 102,019 catalog entries.
The 1,529 existing failure baselines remain unchanged. Production rules and the
headless engine interface are unchanged. The pilot's six draft identity-check
failures and their correction are recorded in
`out/duel-tests/recipe-creature-conditions/pilot-review.json`.

Validation includes 899 Python guards, 36 CTest checks per platform, 284 prior
controls, 12 discovery filters per platform, and two shuffled four-case orders
per platform. Controls retain 271 matching states, two paired old crashes, and
11 old native-only faults with passing Wasm counterparts. All 101,317 prior
catalog entries are preserved. The 66 generated outputs match independent
hashes, with 60 data shards below 8 MiB. Typed OFF/ON/OFF build settings are
restored. The report is
`out/duel-tests/recipe-creature-conditions/validation.json`.


`untap_step` takes only an explicit `player` (0 or1). Call it after
`advance_turn`, with that player current, an empty stack and zero floating
mana for both players. It invokes the complete real untap wrapper, including
begin-turn notification, untap restrictions, actual untaps and end-of-untap
expiry; it does not dispatch upkeep or draw. Each prepared turn permits one
call. Snapshots preserve both prepared and consumed states. A `turn` setter
invalidates preparation and cannot create another untap. Only untap UI stop
preferences/requests are temporarily suppressed and restored; rules choices
remain subject to the ordinary headless input contract.


`can_cast_graveyard`, `require_can_cast_graveyard`, and `cast_graveyard`
use a graveyard-index reference for the bounded noncreature Flashback path.
Call `prepare_graveyard_abilities` after seeding the graveyard. Entry requires
an empty stack; this interface does not model Flashback responses. The query
preserves game state and scripted decisions. The real RulesEngine menu selects
the named source (and its occurrence among identical copies); callers still
supply targets, payment choices and any inner dialogs. Do not queue a numeric
choice for the outer Flashback menu. Its native callback pays costs, casts and
resolves the spell, so success returns after full resolution and draining the
real proxy activation. There is no synthetic pending-spell checkpoint. Retrace,
Aftermath, Escape and creature casting are outside this initial interface.

`assert_shuffle_count` accepts only `player` (integer 0 or 1) and `value`
(a nonnegative integer). It observes actual calls to the shared library-shuffle
entry, indexed by the library's owner rather than the computer making the
choice. Empty and one-card libraries still count. This receipt proves entry
and recipient, not a uniform random distribution or delivery of shuffle
triggers. Counts start at zero on reset and follow test snapshots and both AI
rollback paths; RNG work, time and execution budgets are not refunded by AI
counter restoration. Retained calls made under AI policies still count.

`assert_matching_permanents` also accepts `subtype_Wolf`, `subtype_Ape`,
`subtype_Frog` and `subtype_Lizard`. These read the matching permanent's real
subtypes and are not `assert_card` properties. A Frog Lizard has both its Frog
and Lizard observations set; the keys are independent.
