"""Paid Flash, actual entry selections, and owner-correct battlefield movement."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'flash_entry_battlefield_targeting'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
FLASH = 'Flash (You may cast this spell any time you could cast an instant.)'
BOUNCE = "When this creature enters, you may return another target creature you control to its owner's hand."
BLINK = 'When this creature enters, you may exile target non-Angel creature you control, then return that card to the battlefield under your control.'
SLIVER = "When this creature enters, return target Sliver to its owner's hand."
CYCLE = 'Wizardcycling {3} ({3}, Discard this card: Search your library for a Wizard card, reveal it, put it into your hand, then shuffle.)'
VIZIER_EXILE = ("When this creature enters, you may exile target creature if it attacked or blocked this turn. "
                "Return that card to the battlefield under its owner's control at the beginning of the next end step.")
TRICKSTER_TAP = ("When this creature enters, tap target creature an opponent controls. "
                 "It loses all abilities until end of turn.")
MODELS = {
    13141: ('{W}{U}', (2, 2), (FLASH, BOUNCE), ('own_bounce', 'foreign_owned_bounce', 'no_other', 'short')),
    12448: ('{3}{W}', (3, 4), ('Flash', 'Flying', BLINK), ('blink_reentry', 'no_nonangel', 'flying', 'short')),
    9046: ('{1}{U}', (1, 2), (FLASH, SLIVER, CYCLE), ('own_sliver', 'foe_sliver', 'no_sliver', 'wizardcycling', 'cycle_short')),
    17059: ('{2}{W}', (2, 2), ('Flash', VIZIER_EXILE), ('attacked', 'idle_target', 'blocked', 'foreign_attacker')),
    18464: ('{U}{U}', (2, 2), ('Flash', TRICKSTER_TAP), ('remove_ability', 'already_tapped', 'no_opposing_creature')),
    14573: ('{1}{W}', (0, 4), (FLASH, 'Defender', BOUNCE), ('own_bounce', 'foreign_owned_bounce', 'no_other', 'defender')),
}
DEPUTY, ANGEL, MAGE, WALL, VIZIER, TRICKSTER = 13141, 12448, 9046, 14573, 17059, 18464
PARENT = "Traveler's Amulet"
CREATURE = 'Goblin Balloon Brigade'
ENTRY_WITNESS = 'Lone Missionary'
TAPPER = 'Icy Manipulator'
ANGEL_WITNESS = 'Serra Angel'
SLIVER_WITNESS = 'Venom Sliver'
WIZARD = 'Prodigal Sorcerer'
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
BLOCKER = 'Wall of Roots'
IDLE = 'Elvish Mystic'
WITNESSES = {PARENT, CREATURE, ENTRY_WITNESS, TAPPER, ANGEL_WITNESS, SLIVER_WITNESS, WIZARD, BLOCKER, IDLE, 'Forest'}
TAPPED, UNTAPPED, FOREIGN = 1, 8, 16
ZERO = (0,) * 6
SEED = 2717


def variants(model):
    csvid = model['csvid']
    result = [f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]
    if csvid in (DEPUTY, ANGEL, WALL):
        result.append('decline_0')
    return result


class FlashCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools, self.life, self.shuffles = [ZERO, ZERO], [20, 20], [0, 0]
        self.add('source', record['name'], owner, 'hand')
        self.target = None
        self.cycling = mode in ('wizardcycling', 'cycle_short')
        if self.cycling:
            self.add('wizard', WIZARD, owner, 'library')
            self.add('nonwizard', CREATURE, owner, 'library')
            self.add('forest', 'Forest', owner, 'library')
            self.add('foe_wizard', WIZARD, self.foe, 'library')
        else:
            if self.csvid != VIZIER:
                self.add('parent', PARENT, self.foe, 'hand')
            self.build_battlefield()

    def build_battlefield(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        if self.csvid == TRICKSTER:
            self.add('own_creature', CREATURE, owner, state=UNTAPPED)
            if mode != 'no_opposing_creature':
                self.target = 'foe_creature'
                self.add(self.target, CREATURE, foe, state=TAPPED if mode == 'already_tapped' else UNTAPPED)
            return
        initial = UNTAPPED | (FOREIGN if mode == 'foreign_attacker' else 0)
        self.add('foe_creature', CREATURE, foe, state=initial)
        if self.csvid == VIZIER:
            self.target = 'foe_creature'
            if mode == 'blocked':
                self.target = 'blocker'
                self.add(self.target, BLOCKER, owner, state=UNTAPPED)
            if mode == 'idle_target':
                self.target = 'idle'
                self.add(self.target, IDLE, foe, state=UNTAPPED)
        elif self.csvid == MAGE:
            self.add('wrong_type', CREATURE, owner, state=TAPPED)
            if mode in ('own_sliver', 'foe_sliver'):
                self.target = 'eligible_sliver'
                self.add(self.target, SLIVER_WITNESS, owner if mode == 'own_sliver' else foe, state=TAPPED)
        elif self.csvid == ANGEL:
            self.add('other_angel', ANGEL_WITNESS, owner, state=TAPPED)
            if mode in ('blink_reentry', 'decline'):
                self.target = 'missionary'
                self.add(self.target, ENTRY_WITNESS, owner, 'hand')
                self.add('tapper', TAPPER, owner, state=UNTAPPED)
        elif mode in ('own_bounce', 'foreign_owned_bounce', 'decline', 'short'):
            self.target = 'owned_target'
            state = TAPPED | (FOREIGN if mode == 'foreign_owned_bounce' else 0)
            self.add(self.target, CREATURE, owner, state=state)

    def add(self, ref, name, player, zone='battlefield', state=0):
        entry = card(name, ref, player, zone)
        if state:
            entry['state'] = [label for bit, label in ((TAPPED, 'tapped'), (UNTAPPED, 'untapped'),
                                                       (FOREIGN, 'opponent_owned')) if state & bit]
        self.setup.append(entry)
        self.cards[ref] = dict(name=name, player=player, owner=1-player if state & FOREIGN else player,
                               zone=zone, tapped=bool(state & TAPPED))

    def move(self, ref, zone, *, blinked=False):
        card = self.cards[ref]
        card['zone'] = zone
        card['tapped'] = False
        if not blinked and zone != 'battlefield':
            card['player'] = card['owner']

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        fields = dict(ref=ref, other=other, property=prop, player=player,
                      value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{key: value for key, value in fields.items() if value is not None}))

    def bind(self, ref):
        entity = self.cards[ref]
        self.emit('bind_permanent', ref, player=entity['player'], value=self.rows[entity['name']]['csvid'], index=-1)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def fund(self, player, pool):
        self.pools[player] = tuple(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def observe_mana(self):
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)

    def zone_receipt(self, zone):
        names = sorted({card['name'] for card in self.cards.values()})
        for player in (0, 1):
            visible = [card['name'] for card in self.cards.values()
                       if card['player'] == player and card['zone'] == zone]
            self.emit('assert_zone', player=player, zone=zone, value=len(visible))
            for name in names:
                self.emit('assert_zone_multiplicity', player=player, zone=zone,
                          index=self.rows[name]['csvid'], value=visible.count(name))

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        for zone in ZONES:
            self.zone_receipt(zone)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', card['tapped'])
            record = self.oracle[card['name']]
            if 'power' in record:
                self.observe(ref, 'power', int(record['power']))
                self.observe(ref, 'toughness', int(record['toughness']))

    def cast(self, ref, *, response=False, legal=True, reserve=None):
        name, player = self.cards[ref]['name'], self.cards[ref]['player']
        due = tuple(cost_from_oracle(self.oracle[name]))
        pool = due if reserve is None else reserve
        self.fund(player, pool)
        self.emit('cast_response' if response else 'cast', ref, value=int(legal))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1+int(legal) if response else int(legal))
        if legal:
            self.move(ref, 'pending')
            self.pools[player] = tuple(have-pay for have, pay in zip(pool, due))
        self.observe_mana()
        if legal:
            self.observe(ref, 'in_play', 0)
        if response:
            self.observe('parent', 'in_play', 0)

    def prepare_blink_target(self):
        self.cast(self.target)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.target, 'battlefield')
        self.life[self.owner] += 4
        self.receipt()

        self.fund(self.owner, (1, 0, 0, 0, 0, 0))
        self.emit('choose_target', self.target, player=self.owner)
        self.emit('activate', 'tapper', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.pools[self.owner] = ZERO
        self.cards['tapper']['tapped'] = True
        self.observe_mana()
        self.observe(self.target, 'tapped', 0)
        self.emit('resolve')
        self.emit('refresh')
        self.cards[self.target]['tapped'] = True
        self.receipt()
        self.emit('advance_turn')
        self.emit('turn', player=self.foe)

    def flash_entry(self):
        mode, owner = self.mode, self.owner
        self.cast('parent')
        self.emit('announce_spell', 'parent')
        if self.csvid == TRICKSTER:
            self.fund(self.foe, (0, 0, 0, 0, 1, 0))
            if self.target is not None:
                self.emit('can_activate', self.target, value=1)
        if mode == 'short':
            have = list(cost_from_oracle(self.record))
            color = next(i for i, value in enumerate(have[1:], 1) if value)
            have[color] -= 1
            self.cast('source', response=True, legal=False, reserve=tuple(have))
            self.observe('source', 'in_hand', 1)
            self.emit('resolve')
            self.emit('refresh')
            self.move('parent', 'battlefield')
            self.receipt()
            return
        self.cast('source', response=True)
        if owner == 0 and self.csvid in (ANGEL, WALL):
            self.emit('choose_trigger', 'source', player=owner)
        if self.target is not None:
            choice = 'choose_optional_target' if mode == 'decline' else 'choose_target'
            self.emit(choice, self.target, player=owner)
        self.emit('resolve_response')
        self.emit('refresh')
        self.move('source', 'battlefield')
        self.move('parent', 'battlefield')
        if self.target is not None and mode != 'decline':
            if self.csvid == ANGEL:
                self.move(self.target, 'exile', blinked=True)
                self.move(self.target, 'battlefield', blinked=True)
                self.life[owner] += 4
                self.bind(self.target)
                self.observe(self.target, 'sick', 1)
            elif self.csvid == TRICKSTER:
                self.cards[self.target]['tapped'] = True
                self.emit('can_activate', self.target, value=0)
            else:
                self.move(self.target, 'hand')
        if mode == 'decline':
            self.emit('assert_forced_target_choices', value=0)
        self.receipt()
        if mode in ('flying', 'defender'):
            self.combat()
        if self.csvid == TRICKSTER and self.target is not None:
            self.trickster_expiry()

    def trickster_expiry(self):
        self.fund(self.foe, ZERO)
        self.emit('advance_turn')
        self.emit('refresh')
        self.receipt()
        self.fund(self.foe, (0, 0, 0, 0, 1, 0))
        self.emit('can_activate', self.target, value=1)
        self.emit('activate', self.target, value=1)
        self.emit('assert_stack', value=1)
        self.pools[self.foe] = ZERO
        self.observe_mana()
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()

    def defer_combatant(self):
        target = self.target
        self.emit('begin_attackers')
        self.emit('attack', 'foe_creature', value=1)
        self.emit('finish_attackers')
        self.cards['foe_creature']['tapped'] = True
        if self.mode == 'blocked':
            self.emit('begin_blockers')
            self.emit('block', target, 'foe_creature', value=1)
            self.emit('advance_combat')
            self.observe('foe_creature', 'blocked', 1)

        self.cast('source')
        if self.owner == 0:
            self.emit('choose_trigger', 'source', player=self.owner)
        self.emit('choose_target', target, player=self.owner)
        self.emit('resolve')
        self.emit('refresh')
        self.move('source', 'battlefield')
        exiled = self.mode != 'idle_target'
        if exiled:
            self.move(target, 'exile')
        self.receipt()

        if self.mode != 'blocked':
            self.emit('begin_blockers')
            self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if not exiled:
            self.life[self.owner] -= 1
        self.receipt()
        self.emit('cleanup')
        self.emit('refresh')
        if exiled:
            self.move(target, 'battlefield')
            self.bind(target)
            self.observe(target, 'sick', 1)
        self.receipt()

    def combat(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('upkeep_step', player=self.owner)
        self.emit('turn', player=self.owner)
        for card in self.cards.values():
            if card['player'] == self.owner and card['zone'] == 'battlefield':
                card['tapped'] = False
        attack = self.mode == 'flying'
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=int(attack))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if attack:
            self.emit('block', 'foe_creature', 'source', value=0)
        self.emit('advance_combat')
        if attack:
            self.observe('source', 'blocked', 0)
            self.life[self.foe] -= int(self.record['power'])
            self.cards['source']['tapped'] = True
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.receipt()

    def wizardcycling(self):
        owner = self.owner
        legal = self.mode == 'wizardcycling'
        self.fund(owner, (3 if legal else 2, 0, 0, 0, 0, 0))
        if not legal:
            self.emit('can_activate_hand', 'source', value=0)
            self.emit('activate_hand', 'source', value=0)
            self.receipt()
            return
        self.emit('require_can_activate_hand', 'source', value=1)
        self.emit('choose_dialog', player=owner, value=1)
        self.emit('activate_hand', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.move('source', 'graveyard')
        self.pools[owner] = ZERO
        self.observe_mana()
        self.zone_receipt('graveyard')
        self.zone_receipt('library')
        self.emit('assert_shuffle_count', player=owner, value=0)
        self.emit('choose_zone_card', player=owner, value=self.rows[WIZARD]['csvid'])
        self.emit('resolve')
        self.emit('refresh')
        self.move('wizard', 'hand')
        self.shuffles[owner] += 1
        self.receipt()

    def play(self):
        prelude = self.csvid == ANGEL and self.target is not None
        self.emit('turn', player=self.owner if prelude else self.foe)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        if self.cycling:
            self.wizardcycling()
        elif self.csvid == VIZIER:
            self.defer_combatant()
        else:
            if prelude:
                self.prepare_blink_target()
            self.flash_entry()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    printed_cost, stats, clauses, modes = MODELS[csvid]
    keywords = {'Flash'}
    if csvid == MAGE:
        keywords.update(('Wizardcycling', 'Typecycling', 'Cycling'))
    if csvid == ANGEL:
        keywords.add('Flying')
    if csvid == WALL:
        keywords.add('Defender')
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != printed_cost
            or not record.get('type_line', '').startswith('Creature — ')
            or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or set(record.get('keywords', ())) != keywords
            or tuple(record.get(prop) for prop in ('power', 'toughness')) != tuple(map(str, stats))):
        raise ValueError('Exact whole Flash battlefield source, printed traits and entry clauses required')
    return dict(csvid=csvid, cost=cost_from_oracle(record), stats=list(stats),
                clauses=list(clauses), modes=list(modes))


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact Flash source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented Flash source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Flash Oracle required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact Flash predicates, costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Flash primary and variants required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = FlashCase(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        gaps = ['Wizardcycling reveal presentation'] if csvid == MAGE else []
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=gaps)
        cases.append(dict(name=f'FlashBattlefieldTargetingRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
