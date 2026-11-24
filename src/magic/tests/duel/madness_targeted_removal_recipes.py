"""Madness removal through paid discard, with distinct targeting and life receipts."""

from collections import Counter
from copy import deepcopy
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


HUNTER = 8745
ASSASSIN = 8617
ERUPTION = 5897
ALMS = 15731
SPECS = {
    HUNTER: ('{1}{B}{B}', '{B}', 'Creature', ('When this creature enters, destroy target creature with power 4 or greater. It can\'t be regenerated.',), 'no_target'),
    ASSASSIN: ('{2}{B}{B}', '{1}{B}', 'Creature', ('First strike', 'When this creature enters, you may reveal X black cards in your hand. If you do, target creature gets -X/-X until end of turn.'), None),
    ERUPTION: ('{1}{R}{R}{R}', '{1}{R}{R}', 'Instant', ('Violent Eruption deals 4 damage divided as you choose among any number of targets.',), 'paid_walker'),
    ALMS: ('{2}{B}', '{B}', 'Sorcery', ('Target opponent loses 3 life and you gain 3 life.',), None),
}

FAMILY = 'madness_targeted_removal'
PROFILE = Path(__file__).with_name('data') / 'madness_targeted_removal_wave8_profiles.json'
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
GOBLIN = 'Rummaging Goblin'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
WITNESSES = {
    GOBLIN, 'Mountain', 'Plains', 'Island', 'Ember Beast', 'Cudgel Troll', 'Elvish Mystic',
    'Doom Blade', 'Murder', 'Dazzling Ramparts', 'Jace Beleren', "Urza's Armor", 'Wrap in Vigor',
}
BOUNDARY_GAP = ('The discard callback uses a hand-instance exile marker. Independent transient exile residency '
                'and a separate Madness trigger response window are unavailable.')
REVEAL_GAP = ('Mixed-hand reveal is exercised through ordinary paid casting. The Madness fixture uses a sole '
              'eligible discard because the legacy AI does not offer a scripted exact selection from mixed hands.')
ZERO = [0] * 6
GREEN = cost_from_oracle({'mana_cost': '{G}'})


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in SPECS or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces')):
        raise ValueError('whole normal Madness removal card required')
    normal, alternative, kind, clauses, _ = SPECS[csvid]
    keywords = {'Madness', 'First strike'} if csvid == ASSASSIN else {'Madness'}
    if (record.get('mana_cost') != normal or record.get('type_line', '').split(' — ')[0] != kind
            or record.get('oracle_text') != '\n'.join((*clauses, 'Madness ' + alternative + MADNESS))
            or set(record.get('keywords', ())) != keywords
            or (kind == 'Creature' and any(not record.get(stat, '').isdigit() or int(record[stat]) < 1
                                           for stat in ('power', 'toughness')))):
        raise ValueError('whole printed Madness cost and removal clauses required')
    return dict(csvid=csvid, kind=kind, normal=cost_from_oracle(record),
                madness=cost_from_oracle({'mana_cost': alternative}))


def variants(shape):
    csvid = shape['csvid']
    extra = SPECS[csvid][-1]
    modes = ('paid', 'declined', 'short', 'normal') + ((extra,) if extra else ())
    modes += ('offturn_paid', 'replay')
    if csvid == HUNTER:
        modes += ('shielded_normal',)
    if csvid == ERUPTION:
        modes += ('uneven_paid',)
    return [f'{mode}_{actor}' for mode in modes for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Madness removal branches required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned Madness removal primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Madness removal declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {entry['registry']['csvid'] for entry in pins['sources'].values()} != set(SPECS)):
        raise ValueError('complete independent Madness removal profile required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                or (row['handler'] == 'card_dummy' and name not in {'Mountain', 'Plains', 'Island'})
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('exact Madness removal primary or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
    if oracle[GOBLIN]['oracle_text'] != '{T}, Discard a card: Draw a card.':
        raise ValueError('actual selected-discard and independent draw witness required')
    if (oracle['Wrap in Vigor']['oracle_text'] != 'Regenerate each creature you control.'
            or oracle['Doom Blade']['oracle_text'] != 'Destroy target nonblack creature.'):
        raise ValueError('actual regeneration-shield and ordinary-destruction witnesses required')


class Duel:
    def __init__(self, csvid, card, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, card, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        shape = classify(card, csvid)
        self.normal_cost, self.madness_cost = shape['normal'], shape['madness']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.paid = mode not in ('declined', 'short')
        self.add('source', actor, card['name'], 'hand')
        if self.is_madness:
            self.add('looter', actor, GOBLIN, 'hand')
        for player, names in self.library.items():
            for i, name in enumerate(names):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if csvid in (HUNTER, ASSASSIN):
            self.add('small', self.foe, 'Ember Beast', 'battlefield')
        if self.hunter_target:
            self.add('large', self.foe, 'Cudgel Troll', 'battlefield')
        if mode == 'shielded_normal':
            self.add('shield', self.foe, 'Wrap in Vigor', 'hand')
            self.add('destroy', self.actor, 'Doom Blade', 'hand')
        if csvid == ASSASSIN:
            self.add('attacker', self.foe, 'Elvish Mystic', 'battlefield')
            if mode == 'normal':
                for i, name in enumerate(('Doom Blade', 'Murder', 'Plains')):
                    self.add(f'reveal_{i}', actor, name, 'hand')
        if csvid == ERUPTION:
            self.add('wall', self.foe, 'Dazzling Ramparts', 'battlefield')
            if mode == 'paid_walker':
                self.add('walker', self.foe, 'Jace Beleren', 'battlefield')
        if csvid == ALMS:
            self.add('armor', self.foe, "Urza's Armor", 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op == 'untap_step':
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def cast_pending(self, ref, cost):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, [0] * 6)

    def cast_paid(self, ref, cost):
        self.cast_pending(ref, cost)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield'

    def age(self):
        for player in (self.foe, self.actor):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            self.emit('turn', player=player)

    @property
    def is_madness(self):
        return self.mode not in ('normal', 'shielded_normal')

    @property
    def eruption_permanent_damage(self):
        return 3 if self.mode == 'uneven_paid' else 2

    @property
    def hunter_target(self):
        return self.csvid == HUNTER and self.mode != 'no_target'

    @property
    def foe_pool(self):
        return GREEN if self.hunter_target and self.paid and self.mode != 'shielded_normal' else ZERO

    def ordinary_targets(self):
        if self.csvid != ERUPTION:
            return
        ref = 'walker' if self.mode == 'paid_walker' else 'wall'
        for _ in range(self.eruption_permanent_damage):
            self.emit('choose_target', ref, player=self.actor)
        for _ in range(4 - self.eruption_permanent_damage):
            self.emit('choose_player', player=self.actor, value=self.foe)

    def entry_choices(self):
        if self.hunter_target:
            self.emit('choose_target', 'large', player=self.actor)
        if self.csvid == ASSASSIN and (self.mode == 'normal' or self.actor == 1):
            if self.actor == 0:
                self.emit('choose_trigger', 'source', player=self.actor)
            self.emit('choose_target', 'small', player=self.actor)
            self.emit('choose_dialog', player=self.actor, value=0)
            if self.mode == 'normal':
                self.emit('choose_zone', player=self.actor, value=-1)

    def effect_receipts(self, stack=0):
        if self.paid:
            if self.csvid == HUNTER and self.hunter_target:
                self.cards['large']['zone'] = 'graveyard'
            if self.csvid == ALMS:
                self.life[self.actor] += 3
                self.life[self.foe] -= 3
            if self.csvid == ERUPTION:
                self.life[self.foe] -= 4 - self.eruption_permanent_damage
        if self.csvid == ERUPTION:
            self.observe('wall', 'damage', self.eruption_permanent_damage if self.paid and self.mode != 'paid_walker' else 0)
            if self.mode == 'paid_walker':
                initial = int(self.oracle['Jace Beleren']['loyalty'])
                self.emit('assert_counter', 'walker', prop='loyalty', value=initial - 2)
        if self.csvid in (HUNTER, ASSASSIN):
            minus = 2 if self.csvid == ASSASSIN and self.mode == 'normal' else 0
            small = self.oracle['Ember Beast']
            self.observe('small', 'power', int(small['power']) - minus)
            self.observe('small', 'toughness', int(small['toughness']) - minus)
        self.receipts(stack)

    def source_destination(self):
        return 'battlefield' if self.paid and self.csvid in (HUNTER, ASSASSIN) else 'graveyard'

    def ordinary_cast(self):
        self.emit('present_ai_dialogs', value=1)
        self.mana('set_mana', self.actor, self.normal_cost)
        self.mana('set_mana', self.foe, self.foe_pool)
        self.emit('refresh')
        self.ordinary_targets()
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.entry_choices()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.mana('assert_mana', self.foe, self.foe_pool)
        self.cards['source']['zone'] = self.source_destination()
        self.effect_receipts()

    def madness_cast(self):
        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        if self.mode == 'offturn_paid':
            self.start_foe_turn()
        self.observe('looter', 'tapped', 0)
        self.observe('looter', 'sick', 0)
        self.receipts()
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        affordable = self.mode != 'short'
        pool = self.madness_cost if affordable else [sum(self.madness_cost), 0, 0, 0, 0, 0]
        self.mana('set_mana', self.actor, pool)
        self.mana('set_mana', self.foe, self.foe_pool)
        self.emit('refresh')
        cut = len(self.steps) if self.mode == 'replay' else None
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            self.emit('choose_dialog', player=self.actor, value=0)
        self.emit('choose_dialog', player=self.actor, value=int(affordable and not self.paid))
        if self.paid:
            self.ordinary_targets()
            self.entry_choices()
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        self.cards['source']['zone'] = self.source_destination()
        self.observe('looter', 'tapped', 1)
        self.mana('assert_mana', self.actor, ZERO if self.paid else pool)
        self.mana('assert_mana', self.foe, self.foe_pool)
        self.effect_receipts(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[f'd_{self.actor}_0']['zone'] = 'hand'
        del self.library[self.actor][0]
        self.receipts()
        return cut

    def check_creature(self):
        self.emit('bind_permanent_copy', 'played', player=self.actor, value=self.csvid)
        self.observe('played', 'in_play', 1)
        self.observe('played', 'power', int(self.record['power']))
        self.observe('played', 'toughness', int(self.record['toughness']))
        if self.csvid != ASSASSIN:
            return
        if self.mode != 'offturn_paid':
            self.start_foe_turn()
        self.emit('refresh')
        small = self.oracle['Ember Beast']
        self.observe('small', 'power', int(small['power']))
        self.observe('small', 'toughness', int(small['toughness']))
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'played', 'attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.cards['attacker']['zone'] = 'graveyard'
        self.observe('played', 'in_play', 1)
        self.observe('played', 'damage', 0)
        self.receipts()


    def start_foe_turn(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('turn', player=self.foe)

    def prepare_regeneration(self):
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Wrap in Vigor']))
        self.emit('refresh')
        self.emit('cast', 'shield', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.foe, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.cards['shield']['zone'] = 'graveyard'
        self.observe('large', 'in_play', 1)
        self.observe('small', 'tapped', 0)
        self.receipts()

    def verify_regeneration(self):
        self.mana('set_mana', self.actor, cost_from_oracle(self.oracle['Doom Blade']))
        self.emit('refresh')
        self.emit('choose_target', 'small', player=self.actor)
        self.emit('cast', 'destroy', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['destroy']['zone'] = 'graveyard'
        self.observe('small', 'in_play', 1)
        self.observe('small', 'tapped', 1)
        self.observe('small', 'damage', 0)
        self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        if self.mode == 'shielded_normal':
            self.prepare_regeneration()
        cut = None
        if self.is_madness:
            cut = self.madness_cast()
        else:
            self.ordinary_cast()
        if self.source_destination() == 'battlefield':
            self.check_creature()
        if self.mode == 'shielded_normal':
            self.verify_regeneration()
        if cut is not None:
            suffix = self.steps[cut:]
            self.steps[cut:] = ([step('capture')] + suffix
                                + [step('record_state'), step('ai_roundtrip'), step('restore')]
                                + deepcopy(suffix) + [step('assert_recorded_state')])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Madness removal CSVID required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
        game.build()
        gaps = [BOUNDARY_GAP] if game.is_madness else []
        if csvid == ASSASSIN and game.is_madness:
            gaps.append(REVEAL_GAP)
        cases.append(dict(
            name=f'MadnessTargetedRemovalRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2471,
            setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=gaps),
        ))
    return cases
