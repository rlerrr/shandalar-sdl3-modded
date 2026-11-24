"""Real Madness costs and independent combat triggers."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle, step


FAMILY = 'madness_combat_triggers'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
MADNESS = ' (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONE_NAMES = {number: name for name, number in ZONE.items()}
GOBLIN = 'Rummaging Goblin'
GAP = ('The actual discard callback casts the hand instance with an exile marker. The legacy exposes neither '
       'temporary exile residency nor a separate Madness trigger response window.')

CARDS = {
    8565: ('{B}{B}', ("Whenever this creature blocks or becomes blocked by a nonblack creature, destroy that creature at end of combat.",)),
    15752: ('{1}{R}', ('Whenever this creature deals combat damage to a player, put a +1/+1 counter on it.',)),
    15970: ('{2}{R}', ('Attacking creatures you control have first strike.',
                      'Whenever a Vampire you control deals combat damage to a player, put a +1/+1 counter on it.')),
    16204: ('{1}{R}', ('Trample', 'Whenever this creature deals combat damage to a player, exile the top card of '
                      'your library. Until end of turn, you may play that card.')),
}
BASE_MODES = ('paid', 'declined', 'normal', 'short')
EXTRA = {8565: ('blocks', 'black_blocker'), 15752: ('blocked',),
         15970: ('foreign',), 16204: ('expires', 'zero_spill')}
NOVEL = ('offturn_paid', 'normal_countered', 'replay')
WITNESSES = {GOBLIN, 'Mountain', 'Plains', 'Island', 'Wall of Roots', 'Hill Giant', 'Black Oak of Odunos',
             'Vampire Interloper', 'Goblin Tinkerer', 'Elvish Mystic', 'Ornithopter', 'Counterspell'}
STATIC_DUMMY_WITNESSES = {'Ornithopter', 'Hill Giant'}
ZERO = [0] * 6


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    fee, riders = CARDS[csvid]
    clauses = [*riders, 'Madness ' + fee + MADNESS]
    expected_keywords = {'Madness'} | ({'Trample'} if csvid == 16204 else set())
    matches_type = record['type_line'] == 'Enchantment' if csvid == 15970 else record['type_line'].startswith('Creature — ')
    if (record['oracle_text'] != '\n'.join(clauses) or record.get('layout') != 'normal'
            or not matches_type or record.get('selected_face') or record.get('card_faces')
            or set(record['keywords']) != expected_keywords):
        raise ValueError('Exact whole Madness combat card required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), madness=cost_from_oracle({'mana_cost': fee}),
                clauses=clauses)


def variants(model):
    modes = BASE_MODES + EXTRA.get(model['csvid'], ()) + NOVEL
    return [f'{mode}_{actor}' for mode in modes for actor in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != CARDS.keys()):
        raise ValueError('Exact independent Madness source and witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            handler = row.get('handler')
            if (not exact(pin, {'oracle': record, 'registry': row}) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or handler in (None, 'NULL', '0')
                    or handler == 'card_dummy' and name not in STATIC_DUMMY_WITNESSES):
                raise ValueError(f'Pinned Madness source or witness changed: {name}')
    if (oracle[GOBLIN]['oracle_text'] != '{T}, Discard a card: Draw a card.'
            or rows[GOBLIN]['handler'] != 'manalinkeh_card_rummaging_goblin'):
        raise ValueError('Actual selected discard and draw witness required')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Madness primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Madness costs, clauses and scoped variants required')
    return dict(model, family=FAMILY)

class MadnessBoard:
    def add(self, ref, player, name, zone):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, zone=None, **fields):
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if zone is not None:
            fields['zone'] = ZONE_NAMES[zone]
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

    def selected_discard(self):
        from recipes import cost_from_oracle

        self.cast_paid('looter', cost_from_oracle(self.oracle[GOBLIN]))
        self.age()
        if self.mode == 'offturn_paid':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
        self.observe('looter', 'in_play', 1)
        self.observe('looter', 'tapped', 0)
        self.observe('looter', 'sick', 0)
        self.receipts()
        self.emit('discard_policy', value=1)
        self.emit('present_ai_dialogs', value=1)
        affordable = self.mode != 'short'
        pool = self.madness_cost if affordable else [sum(self.madness_cost), 0, 0, 0, 0, 0]
        self.mana('set_mana', self.actor, pool)
        self.emit('refresh')
        cut = len(self.steps) if self.mode == 'replay' else None
        self.emit('can_activate', 'looter', value=1)
        if self.actor == 1:
            # The native AI selects its sole eligible discard and acknowledges it.
            self.emit('choose_dialog', player=self.actor, value=0)
        paid = self.mode in ('paid', 'offturn_paid', 'replay')
        self.emit('choose_dialog', player=self.actor, value=int(affordable and not paid))
        self.emit('activate', 'looter', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        if self.actor == 0:
            self.emit('assert_discard_choices', value=1)
        self.cards['source']['zone'] = 'battlefield' if paid else 'graveyard'
        self.observe('looter', 'tapped', 1)
        self.mana('assert_mana', self.actor, [0] * 6 if paid else pool)
        self.mana('assert_mana', self.foe, [0] * 6)
        self.receipts(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[f'd_{self.actor}_0']['zone'] = 'hand'
        del self.library[self.actor][0]
        self.receipts()
        return cut

    def counter_normal_cast(self):
        from recipes import cost_from_oracle

        self.cast_pending('source', self.normal_cost)
        self.emit('announce_spell', 'source')
        self.mana('set_mana', self.foe, cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response', 'counter', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_choices_consumed')
        self.mana('assert_mana', self.foe, [0] * 6)
        self.cards['source']['zone'] = self.cards['counter']['zone'] = 'graveyard'
        self.emit('refresh')
        self.receipts()



class TriggerDuel(MadnessBoard):
    def __init__(self, csvid, record, actor, scenario, rows, oracle):
        self.csvid, self.record, self.actor, self.foe = csvid, record, actor, 1 - actor
        self.scenario = scenario
        self.mode = scenario if scenario in BASE_MODES + NOVEL else 'paid'
        self.rows, self.oracle = rows, oracle
        model = classify(record, csvid)
        self.normal_cost, self.madness_cost = model['normal'], model['madness']
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Plains', 'Island']}
        self.add('source', actor, record['name'], 'hand')
        if self.mode == 'normal_countered':
            self.add('counter', self.foe, 'Counterspell', 'hand')
        elif self.mode != 'normal':
            self.add('looter', actor, GOBLIN, 'hand')
        for player, deck in self.library.items():
            for i, name in enumerate(deck):
                self.add(f'd_{player}_{i}', player, name, 'library')
        if self.mode == 'normal_countered':
            return
        if csvid == 8565:
            name = 'Hill Giant' if self.gorgon_blocks() else (
                'Black Oak of Odunos' if scenario == 'black_blocker' else 'Wall of Roots'
            )
            self.add('witness', self.foe, name, 'battlefield')
        elif csvid == 15752 and scenario == 'blocked':
            self.add('witness', self.foe, 'Wall of Roots', 'battlefield')
        elif csvid == 15970:
            self.masquerade_setup()
        elif csvid == 16204:
            self.add('witness', self.foe, 'Wall of Roots' if scenario == 'zero_spill' else 'Ornithopter', 'battlefield')

    def masquerade_setup(self):
        if self.scenario == 'foreign':
            self.add('vampire', self.foe, 'Vampire Interloper', 'battlefield')
            self.add('tinkerer', self.foe, 'Goblin Tinkerer', 'battlefield')
            self.add('elf', self.actor, 'Elvish Mystic', 'battlefield')
        else:
            self.add('vampire', self.actor, 'Vampire Interloper', 'battlefield')
            self.add('tinkerer', self.actor, 'Goblin Tinkerer', 'battlefield')
            self.add('elf', self.actor, 'Elvish Mystic', 'battlefield')
            self.add('witness', self.foe, 'Elvish Mystic', 'battlefield')

    def enter_rider(self):
        self.emit('bind_permanent_copy', 'played', player=self.actor, index=0, value=self.csvid)
        self.observe('played', 'in_play', 1)
        if self.csvid != 15970:
            self.observe('played', 'power', int(self.record['power']))
            self.observe('played', 'toughness', int(self.record['toughness']))
            self.observe('played', 'sick', 1)
        opposing_combat = self.gorgon_blocks() or self.scenario == 'foreign'
        if self.mode == 'offturn_paid':
            if not opposing_combat:
                self.next_turn(self.actor)
                if self.csvid != 15970:
                    self.observe('played', 'sick', 0)
        elif opposing_combat:
            self.next_turn(self.foe)
        else:
            self.age()
            if self.csvid != 15970:
                self.observe('played', 'sick', 0)
        action = {8565: self.gorgon, 15752: self.bloodmad,
                  15970: self.masquerade, 16204: self.occultist}[self.csvid]
        action()
        self.receipts()

    def next_turn(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)

    def begin_fight(self, attackers, *, blocker=None, blocked_by=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
            self.observe(ref, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, blocked_by, value=1)
        self.emit('advance_combat')
        for ref in attackers:
            self.observe(ref, 'blocked', int(ref == blocked_by))

    def damage(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_choices_consumed')

    def finish_combat(self):
        self.emit('end_combat')
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def gorgon_blocks(self):
        return self.csvid == 8565 and self.scenario in ('blocks', 'offturn_paid')

    def gorgon(self):
        blocks = self.gorgon_blocks()
        attacker, blocker = ('witness', 'played') if blocks else ('played', 'witness')
        self.begin_fight([attacker], blocker=blocker, blocked_by=attacker)
        self.damage()
        self.observe('witness', 'in_play', 1)
        self.observe('witness', 'damage', 2)
        power = int(self.oracle[self.cards['witness']['name']]['power'])
        self.observe('played', 'damage', power)
        # Combat damage itself could not destroy the witness. The change must
        # happen at the real end-of-combat trigger, and only for a nonblack card.
        self.receipts()
        self.finish_combat()
        if 'B' not in self.oracle[self.cards['witness']['name']]['colors']:
            self.cards['witness']['zone'] = 'graveyard'
        else:
            self.observe('witness', 'in_play', 1)
            self.observe('witness', 'damage', 2)

    def bloodmad(self):
        blocked = self.scenario == 'blocked'
        self.begin_fight(['played'], blocker='witness' if blocked else None,
                         blocked_by='played' if blocked else None)
        self.damage()
        self.finish_combat()
        counters = int(not blocked)
        self.observe('played', 'plus1_counters', counters)
        self.observe('played', 'power', 4 + counters)
        self.observe('played', 'toughness', 1 + counters)
        if blocked:
            self.observe('witness', 'damage', 4)
        else:
            self.life[self.foe] -= 4

    def masquerade(self):
        foreign = self.scenario == 'foreign'
        attackers = ['vampire', 'tinkerer'] if foreign else ['vampire', 'tinkerer', 'elf']
        self.begin_fight(attackers, blocker='elf' if foreign else 'witness', blocked_by='tinkerer')
        self.damage()
        self.finish_combat()
        if foreign:
            self.life[self.actor] -= 2
            self.cards['elf']['zone'] = 'graveyard'
            self.observe('tinkerer', 'damage', 1)
        else:
            self.life[self.foe] -= 3
            self.cards['witness']['zone'] = 'graveyard'
            self.observe('tinkerer', 'damage', 0)
            self.observe('elf', 'plus1_counters', 0)
        counters = int(not foreign)
        self.observe('vampire', 'plus1_counters', counters)
        self.observe('vampire', 'power', 2 + counters)
        self.observe('vampire', 'toughness', 1 + counters)
        self.observe('tinkerer', 'plus1_counters', 0)

    def occultist(self):
        top = self.library[self.actor][0]
        top_id = self.rows[top]['csvid']
        self.emit('assert_zone_card', player=self.actor, zone=ZONE['library'], index=0, value=top_id)
        self.begin_fight(['played'], blocker='witness', blocked_by='played')
        self.damage()
        zero_spill = self.scenario == 'zero_spill'
        if zero_spill:
            self.observe('witness', 'damage', 3)
        else:
            self.cards['witness']['zone'] = 'graveyard'
            self.life[self.foe] -= 1
            ref, = (r for r, c in self.cards.items() if c['player'] == self.actor and
                    c['name'] == top and c['zone'] == 'library')
            self.library[self.actor].pop(0)
            self.cards[ref]['zone'] = 'exile'
        self.receipts()
        self.finish_combat()
        self.emit('can_play_exiled', player=self.foe, index=top_id, value=0)
        self.emit('can_play_exiled', player=self.actor, index=top_id, value=int(not zero_spill))
        if self.scenario == 'expires':
            self.age()
            self.emit('can_play_exiled', player=self.actor, index=top_id, value=0)
            self.emit('play_exiled', player=self.actor, index=top_id, value=0)
        elif not zero_spill:
            self.emit('play_exiled', player=self.actor, index=top_id, value=1)
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
            self.cards[ref]['zone'] = 'battlefield'

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.foe, ZERO)
        if self.mode == 'normal_countered':
            self.counter_normal_cast()
            return
        cut = None
        if self.mode == 'normal':
            self.cast_paid('source', self.normal_cost)
            self.receipts()
        else:
            cut = self.selected_discard()
        if self.mode in ('normal', 'paid', 'offturn_paid', 'replay'):
            self.enter_rider()
        if cut is not None:
            suffix = self.steps[cut:]
            self.steps[cut:] = [step('capture')] + suffix + [
                step('record_state'), step('ai_roundtrip'), step('restore')
            ] + suffix + [step('assert_recorded_state')]


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Madness primary and named variants required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = TriggerDuel(csvid, oracle[name], int(actor), mode, rows, oracle)
        game.build()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[] if mode in ('normal', 'normal_countered') else [GAP])
        cases.append(dict(name=f'MadnessCombatTriggersRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2471, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases

