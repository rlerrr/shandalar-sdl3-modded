"""Cumulative upkeep with independent continuous and activated printed effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from cumulative_upkeep_payment_recipes import REMINDER
from recipes import card, cost_from_oracle, step


FAMILY = 'cumulative_upkeep_continuous_and_activated'
PROFILE = Path(__file__).with_name('data') / 'cumulative_upkeep_continuous_and_activated_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
PAY_LAND = ('Wastes', 'Swamp', 'Island', 'Forest', 'Mountain', 'Plains')
ZERO = [0] * 6

BLIZZARD = 1875
ENERGY = 1914
SPIRIT = 2364
TORNADO = 2371
FLYING_RESTRICTION = "Creatures with flying don't untap during their controllers' untap steps."
SOURCE_TEXT = {
    BLIZZARD: ('{G}{G}', '{2}', 'Enchantment',
               ('Cast this spell only if you control a snow land.', 'UPKEEP', FLYING_RESTRICTION)),
    ENERGY: ('{1}{W}', '{1}', 'Enchantment',
             ('UPKEEP', 'Prevent all damage that would be dealt by instant and sorcery spells.', FLYING_RESTRICTION)),
    SPIRIT: ('{1}{W}', '{1}{W}', 'Creature — Angel Spirit',
             ('UPKEEP', 'Damage that would reduce your life total to less than 1 reduces it to 1 instead.')),
    TORNADO: ('{4}{G}', '{G}', 'Enchantment',
              ('UPKEEP', '{2}{G}, Pay 3 life for each velocity counter on this enchantment: Destroy target permanent and put a velocity counter on this enchantment. Activate only once each turn.')),
}
MODES = {
    BLIZZARD: ('age_two', 'flying_decline', 'foe_snow_only'),
    ENERGY: ('age_two', 'flying_decline', 'spell_damage'),
    SPIRIT: ('age_two', 'decline', 'damage_floor'),
    TORNADO: ('age_two', 'decline', 'velocity'),
}

CORROSION = 2684
MUSICIAN = 2044
TERRAIN = 1980
SOURCE_TEXT.update({
    CORROSION: ('{1}{B}{R}', '{1}', 'Enchantment', (
        'UPKEEP',
        "At the beginning of your upkeep, put a rust counter on each artifact target opponent controls. Then destroy each artifact with mana value less than or equal to the number of rust counters on it. Artifacts destroyed this way can't be regenerated.",
        'When this enchantment leaves the battlefield, remove all rust counters from all permanents.',
    )),
    MUSICIAN: ('{2}{U}', '{1}', 'Creature — Human Wizard', (
        'UPKEEP',
        '{T}: Put a music counter on target creature. If it doesn\'t have "At the beginning of your upkeep, destroy this creature unless you pay {1} for each music counter on it," it gains that ability.',
    )),
    TERRAIN: ('{U}{U}', '{2}', 'Enchantment', (
        'UPKEEP', 'As this enchantment enters, choose two basic land types.',
        'Basic lands of the first chosen type are the second chosen type.',
    )),
})
MODES.update({
    CORROSION: ('age_two', 'rust_expiry', 'no_regeneration'),
    MUSICIAN: ('music_unpaid', 'music_indestructible', 'music_paid_after_source_removed'),
    TERRAIN: ('terrain_conversion', 'terrain_expiry', 'terrain_identity'),
})
GAPS = {
    CORROSION: ['The bounded upkeep does not expose an independent ordering choice between the two printed upkeep triggers.'],
    MUSICIAN: ['There is no named music-counter observer. The granted fee and paid, destroyed, and indestructible outcomes are observed via real upkeep.'],
    TERRAIN: ['Changed land subtypes are observed through their real mana abilities. The legacy battlefield CSVID getter may expose the transient replacement type identity.'],
}
NEW_WITNESSES = {'Bonesplitter', 'Prismatic Lens', 'Whispersilk Cloak', 'Naturalize', 'Doom Blade',
                 'Darksteel Myr', 'Sapseep Forest', 'Welding Jar', 'Signal Pest', 'Early Harvest'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in SOURCE_TEXT or not record or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole eligible continuous or activated cumulative upkeep primary required')
    cast, upkeep, type_line, clauses = SOURCE_TEXT[csvid]
    trigger = 'Cumulative upkeep ' + upkeep + REMINDER
    expected = '\n'.join(trigger if clause == 'UPKEEP' else clause for clause in clauses)
    if (record.get('layout') != 'normal' or record.get('type_line') != type_line
            or record.get('keywords') != ['Cumulative upkeep'] or record.get('mana_cost') != cast
            or record.get('oracle_text') != expected):
        raise ValueError('complete printed cumulative upkeep and rider clauses required')
    stats = {SPIRIT: ('0', '3'), MUSICIAN: ('1', '3')}
    if csvid in stats and (record.get('power'), record.get('toughness')) != stats[csvid]:
        raise ValueError('pinned creature stats changed')
    return dict(csvid=csvid, cast=cost_from_oracle(record), upkeep=cost_from_oracle({'mana_cost': upkeep}))


def variants(shape):
    return [f'{mode}_{actor}' for mode in MODES[shape['csvid']] for actor in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(m) is not str for m in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported cumulative upkeep variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', ''))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('whole pinned cumulative upkeep record required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole-card cumulative upkeep declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    required_witnesses = {
        'Wastes', 'Plains', 'Forest', 'Mountain', 'Island', 'Snow-Covered Forest',
        'Shivan Dragon', 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Lightning Bolt',
        'Volcanic Hammer', "Traveler's Amulet",
    } | NEW_WITNESSES
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != required_witnesses
            or {p['registry']['csvid'] for p in pins['sources'].values()} != set(SOURCE_TEXT)):
        raise ValueError('complete source and witness profile required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned source or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        shape = classify(record, csvid)
        self.cast_cost, self.per_age = shape['cast'], shape['upkeep']
        self.setup, self.steps, self.cards = [], [], {}
        self.current = actor
        self.source = 'source'
        self.age_count = 0
        self.velocity = 0
        self.life = [20, 20]
        self.library = {actor: ['Mountain', 'Plains', 'Island'], self.foe: ['Island', 'Plains']}
        self.lands = []
        self.add('source', actor, record['name'], 'hand')
        for seat, cards in self.library.items():
            for i, name in enumerate(cards):
                self.add(f'library_{seat}_{i}', seat, name, 'library')
        self.add_upkeep_lands()
        if mode == 'foe_snow_only':
            self.add('foe_snow', self.foe, 'Snow-Covered Forest', 'battlefield')
        if mode == 'flying_decline':
            for seat in (actor, self.foe):
                self.tapped_creature(f'fly_{seat}', seat, 'Shivan Dragon')
                self.tapped_creature(f'ground_{seat}', seat, 'Prodigal Sorcerer')
        if mode == 'spell_damage':
            self.add('target', self.foe, 'Dazzling Ramparts', 'battlefield')
            self.add('pinger', actor, 'Prodigal Sorcerer', 'battlefield')
            self.add('instant', actor, 'Lightning Bolt', 'hand')
            self.add('sorcery', actor, 'Volcanic Hammer', 'hand')
        if mode == 'damage_floor':
            for i in range(4):
                self.add(f'bolt_{i}', actor, 'Lightning Bolt', 'hand')
        if csvid in (CORROSION, MUSICIAN, TERRAIN):
            self.add_rider_witnesses()
        if mode == 'velocity':
            self.add('extra_wastes_0', actor, 'Wastes', 'battlefield')
            self.add('extra_wastes_1', actor, 'Wastes', 'battlefield')
            self.add('foe_land', self.foe, 'Forest', 'battlefield')
            self.add('foe_artifact', self.foe, "Traveler's Amulet", 'battlefield')
            self.add('foe_creature', self.foe, 'Dazzling Ramparts', 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
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
        type_may_change = {'Forest', 'Mountain'} if self.csvid == TERRAIN else set()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    if zone == 'battlefield' and name in type_may_change:
                        continue
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def tapped_creature(self, ref, seat, name):
        self.add(ref, seat, name, 'battlefield')
        self.setup[-1]['state'] = ['tapped']

    def add_upkeep_lands(self):
        palette = [name for value, name in zip(self.per_age, PAY_LAND) for _ in range(2 * value)]
        if self.csvid == BLIZZARD:
            palette[-1] = 'Forest' if self.mode == 'foe_snow_only' else 'Snow-Covered Forest'
        for i, name in enumerate(palette):
            ref = f'pay_{i}'
            self.add(ref, self.actor, name, 'battlefield')
            self.lands.append(ref)

    def allow_zero_pools(self):
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)

    def live_source(self):
        return self.cards[self.source]['zone'] == 'battlefield'

    def state(self, stack=0):
        self.receipts(stack)
        self.allow_zero_pools()
        if self.live_source():
            self.observe(self.source, 'in_play', 1)
            self.emit('assert_counter', self.source, prop='age', value=self.age_count)
            if self.csvid == TORNADO:
                self.emit('assert_counter', 'source', prop='velocity', value=self.velocity)
            if self.csvid in (SPIRIT, MUSICIAN):
                self.observe(self.source, 'power', int(self.record['power']))
                self.observe(self.source, 'toughness', int(self.record['toughness']))

    def play(self, ref, seat, *, target=None, player=None):
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.mana('set_mana', seat, cost)
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', target, player=seat)
        if player is not None:
            self.emit('choose_player', player=seat, value=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, ZERO)
        if self.csvid == TERRAIN and ref == self.source:
            self.emit('choose_color', player=seat, value=3)
            self.emit('choose_color', player=seat, value=3 if self.mode == 'terrain_identity' else 4)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[ref]['zone'] = 'battlefield' if ref == self.source else 'graveyard'

    def reject_foe_only_snow(self):
        self.mana('set_mana', self.actor, self.cast_cost)
        self.emit('refresh')
        self.emit('can_cast', 'source', value=0)
        self.emit('cast', 'source', value=0)
        self.observe('source', 'in_hand', 1)
        self.mana('assert_mana', self.actor, self.cast_cost)
        self.emit('assert_choices_consumed')
        self.receipts()

    def visit(self, pay=True, before_upkeep=None):
        self.current = 1 - self.current
        self.emit('advance_turn')
        self.emit('untap_step', player=self.current)
        if self.mode == 'flying_decline':
            self.observe(f'fly_{self.current}', 'tapped', int(self.live_source()))
            self.observe(f'ground_{self.current}', 'tapped', 0)
        own_upkeep = self.current == self.actor and self.live_source()
        tapped_receipts = []
        if own_upkeep:
            self.age_count += 1
            self.emit('choose_dialog', player=self.actor, value=int(not pay))
            if pay:
                quantity = self.age_count * sum(self.per_age)
                covered = self.lands[:2 if self.mode == 'velocity' else len(self.lands)]
                chosen = covered[:quantity]
                if self.csvid == SPIRIT and self.age_count == 1:
                    chosen = [covered[0], covered[2]]
                assert len(chosen) == quantity and quantity <= len(covered)
                if self.actor == 0:
                    for ref in chosen:
                        self.emit('choose_mana_source', ref)
                    tapped_receipts = [(ref, int(ref in chosen)) for ref in covered]
                elif quantity == len(covered) and self.mode != 'velocity' and not self.terrain_ready_spell:
                    tapped_receipts = [(ref, 1) for ref in covered]
            else:
                self.cards[self.source]['zone'] = 'graveyard'
                for ref in self.lands:
                    tapped_receipts.append((ref, 0))
        if before_upkeep:
            before_upkeep()
        rust = self.csvid == CORROSION and own_upkeep and pay
        if rust:
            self.corrosion_trigger()
        regen_choice = rust and self.mode == 'no_regeneration'
        self.emit('upkeep_step', player=self.current)
        self.emit('assert_unused_choices', value=int(regen_choice))
        self.emit('assert_choices_consumed')
        self.emit('turn', player=self.current)
        for ref, value in tapped_receipts:
            self.observe(ref, 'tapped', value)
        if regen_choice:
            self.observe('jar', 'in_play', 1)
        self.state()

    def own_upkeep(self, pay=True):
        self.visit()
        self.visit(pay)

    def reveal_spell_prevention(self):
        self.play('instant', self.actor, target='target')
        self.observe('target', 'in_play', 1)
        self.observe('target', 'damage', 0)
        self.state()
        self.play('sorcery', self.actor, player=self.foe)
        self.state()
        self.emit('can_activate', 'pinger', value=1)
        self.emit('choose_player', player=self.actor, value=self.foe)
        self.emit('activate', 'pinger', value=1)
        self.observe('pinger', 'tapped', 1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.life[self.foe] -= 1
        self.state()

    def reveal_damage_floor(self):
        for i, recipient in enumerate((self.actor, self.actor, self.actor, self.foe)):
            self.play(f'bolt_{i}', self.actor, player=recipient)
            lowest = 1 if recipient == self.actor else 0
            self.life[recipient] = max(lowest, self.life[recipient] - 3)
            self.state()

    def use_tornado(self, target):
        fee = cost_from_oracle({'mana_cost': '{2}{G}'})
        self.mana('set_mana', self.actor, fee)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.life[self.actor] -= 3 * self.velocity
        self.emit('assert_life', player=self.actor, value=self.life[self.actor])
        self.mana('assert_mana', self.actor, ZERO)
        self.emit('assert_counter', 'source', prop='velocity', value=self.velocity)
        self.observe(target, 'in_play', 1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards[target]['zone'] = 'graveyard'
        self.velocity += 1
        self.state()

    def check_tornado(self):
        self.use_tornado('foe_land')
        # The two Wastes and both pay lands are real untapped providers.
        for ref in ('extra_wastes_0', 'extra_wastes_1', *self.lands):
            self.observe(ref, 'tapped', 0)
        self.emit('can_activate', 'source', value=0)
        self.own_upkeep()
        self.use_tornado('foe_artifact')

    def add_rider_witnesses(self):
        if self.csvid == CORROSION:
            self.rust_round = 0
            self.add('own_artifact', self.actor, 'Bonesplitter', 'battlefield')
            if self.mode == 'no_regeneration':
                self.add('jar', self.actor, 'Welding Jar', 'battlefield')
                self.add('artifact_creature', self.foe, 'Signal Pest', 'battlefield')
            else:
                self.add('artifact_one', self.foe, 'Bonesplitter', 'battlefield')
                self.add('artifact_two', self.foe, 'Prismatic Lens', 'battlefield')
                self.add('artifact_three', self.foe, 'Whispersilk Cloak', 'battlefield')
                self.add('foe_creature', self.foe, 'Dazzling Ramparts', 'battlefield')
                if self.mode == 'rust_expiry':
                    self.add('remove', self.actor, 'Naturalize', 'hand')
                    self.add('replacement', self.actor, self.record['name'], 'hand')
        elif self.csvid == MUSICIAN:
            target = 'Darksteel Myr' if self.mode == 'music_indestructible' else 'Dazzling Ramparts'
            self.add('subject', self.foe, target, 'battlefield')
            self.add('foe_control', self.foe, 'Prodigal Sorcerer', 'battlefield')
            if self.mode == 'music_paid_after_source_removed':
                self.add('foe_pay_land', self.foe, 'Wastes', 'battlefield')
                self.add('remove', self.actor, 'Doom Blade', 'hand')
        else:
            for seat in (self.actor, self.foe):
                self.add(f'forest_{seat}', seat, 'Forest', 'battlefield')
                self.add(f'island_{seat}', seat, 'Island', 'battlefield')
            self.add('nonbasic_forest', self.foe, 'Sapseep Forest', 'battlefield')
            if self.terrain_ready_spell:
                self.add('ready_lands', self.actor, 'Early Harvest', 'hand')

    def corrosion_trigger(self):
        self.rust_round += 1
        if self.mode == 'no_regeneration':
            self.emit('choose_regeneration', 'jar')
            self.cards['artifact_creature']['zone'] = 'graveyard'
            return
        for ref in ('artifact_one', 'artifact_two', 'artifact_three'):
            entry = self.cards[ref]
            if entry['zone'] == 'battlefield' and int(self.oracle[entry['name']]['cmc']) <= self.rust_round:
                entry['zone'] = 'graveyard'

    def check_corrosion(self):
        self.own_upkeep()
        if self.mode == 'no_regeneration':
            return
        self.own_upkeep()
        if self.mode == 'rust_expiry':
            self.play('remove', self.actor, target=self.source)
            self.cards[self.source]['zone'] = 'graveyard'
            self.state()
            self.source = 'replacement'
            self.age_count = self.rust_round = 0
            self.play(self.source, self.actor)
            self.state()
            self.own_upkeep()
            self.observe('artifact_three', 'in_play', 1)

    def check_musician(self):
        self.own_upkeep()
        self.observe('source', 'sick', 0)
        self.observe('source', 'tapped', 0)
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', 'subject', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.observe('source', 'tapped', 1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.state()
        if self.mode == 'music_paid_after_source_removed':
            self.play('remove', self.actor, target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.state()

            def pay_music():
                self.emit('choose_dialog', player=self.foe, value=0)
                if self.foe == 0:
                    self.emit('choose_mana_source', 'foe_pay_land')

            self.visit(before_upkeep=pay_music)
            self.observe('foe_pay_land', 'tapped', 1)
            return
        if self.mode == 'music_unpaid':
            self.cards['subject']['zone'] = 'graveyard'
        self.visit()

    @property
    def terrain_ready_spell(self):
        return self.csvid == TERRAIN and self.actor == 1 and self.mode in ('terrain_conversion', 'terrain_identity')

    def check_terrain(self):
        self.own_upkeep()
        self.own_upkeep(pay=self.mode != 'terrain_expiry')
        if self.terrain_ready_spell:
            # The AI may use either probe land for the generic upkeep cost.
            self.play('ready_lands', self.actor, player=self.actor)
            self.observe(f'forest_{self.actor}', 'tapped', 0)
            self.observe(f'island_{self.actor}', 'tapped', 0)
            self.state()
        converted = self.mode == 'terrain_conversion'
        for seat in (self.actor, self.foe):
            refs_and_mana = [(f'forest_{seat}', 4 if converted else 3), (f'island_{seat}', 2)]
            if seat == self.foe:
                refs_and_mana.append(('nonbasic_forest', 3))
            expected = [0] * 6
            for ref, color in refs_and_mana:
                self.emit('activate', ref, value=1)
                self.emit('assert_stack', value=0)
                self.emit('assert_choices_consumed')
                self.observe(ref, 'tapped', 1)
                expected[color] += 1
                self.mana('assert_mana', seat, expected)

    def build(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.mode == 'foe_snow_only':
            self.reject_foe_only_snow()
            return
        if self.mode == 'damage_floor':
            self.life[self.actor] = 5
            self.emit('set_life', player=self.actor, value=5)
        self.play(self.source, self.actor)
        self.state()
        if self.csvid == CORROSION:
            self.check_corrosion()
            return
        if self.csvid == MUSICIAN:
            self.check_musician()
            return
        if self.csvid == TERRAIN:
            self.check_terrain()
            return
        if self.mode == 'velocity':
            self.check_tornado()
            return
        self.own_upkeep(pay=self.mode != 'decline')
        if self.mode == 'age_two':
            self.own_upkeep()
        elif self.mode == 'flying_decline':
            self.own_upkeep(pay=False)
            self.own_upkeep()
        elif self.mode == 'spell_damage':
            self.reveal_spell_prevention()
        elif self.mode == 'damage_floor':
            self.reveal_damage_floor()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered primary required')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        world = Duel(csvid, oracle[name], int(actor), mode, rows, oracle)
        world.build()
        cases.append(dict(
            name=f'CumulativeContinuousActivatedRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2503,
            setup=world.setup, steps=world.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return cases
