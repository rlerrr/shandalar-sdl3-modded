"""Paid Bestow modes for dynamic stats, player control and targeting riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER
from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'bestow_dynamic_and_control'
PROFILE = Path(__file__).with_name('data') / 'bestow_dynamic_and_control_wave8_profiles.json'
CARDS = {
    13689: ('{2}{W}{W}', ('This creature and enchanted creature each get +1/+1 for each creature you '
                        'control and +1/+1 for each Aura you control.',),
            ('regular_growth', 'foreign_dynamic_detach', 'pending_host_lost', 'enemy_aura_ignored')),
    13490: ('{2}{B}{B}', ('This creature and enchanted creature each get +X/+X, where X is the number '
                        'of creature cards in all graveyards.',),
            ('regular_graves', 'bestowed_graves', 'empty_graves', 'last_grave_exiled')),
    13912: ('{5}{U}{U}', ('Flying', 'You control enchanted creature.', 'Enchanted creature gets +1/+1 and has flying.'),
            ('regular_flying', 'steal_fly_release', 'pending_host_lost', 'stolen_dies')),
    13861: ('{3}{U}{U}', ('When this permanent becomes the target of a spell or ability, sacrifice it.',
                        'Enchanted creature gets +4/+4 and has "When this creature becomes the target of a spell '
                        'or ability, sacrifice it."'), ('regular_ability', 'bestowed_host_spell', 'bestowed_source_spell',
                                                       'regular_nontap_ability')),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0,) * 6
BEAR = 'Grizzly Bears'
GIANT = 'Hill Giant'
HOLY = 'Holy Strength'
CRYPT = "Tormod's Crypt"
PRODIGAL = 'Prodigal Sorcerer'
OVERSEER = 'Flowstone Overseer'
WITNESSES = {BEAR, GIANT, HOLY, CRYPT, PRODIGAL, OVERSEER, 'Mountain', 'Murder', 'Naturalize',
             'Unsummon', 'Giant Growth', 'Boomerang'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or not record.get('type_line', '').startswith('Enchantment Creature — ')):
        raise ValueError('whole normal Bestow creature required')
    price, riders, _ = CARDS[csvid]
    if (record.get('oracle_text', '').splitlines() != ['Bestow ' + price + REMINDER, *riders]
            or set(record.get('keywords', ())) != ({'Bestow', 'Flying'} if csvid == 13912 else {'Bestow'})
            or any(not str(record.get(field, '')).isdigit() for field in ('power', 'toughness'))):
        raise ValueError('exact printed Bestow modes and dynamic rules required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), bestowed=cost_from_oracle({'mana_cost': price}),
                creature=[int(record['power']), int(record['toughness'])], riders=list(riders))


def variants(shape):
    return [f'{mode}_{actor}' for mode in CARDS[shape['csvid']][2] for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported dynamic Bestow scenarios required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Bestow primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole dynamic Bestow declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('independent dynamic Bestow witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CARDS):
        raise ValueError('complete dynamic Bestow source cohort required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('dynamic Bestow source or witness changed: ' + name)
            if group == 'sources':
                face = pin['engine_aura_face']
                if (not str(row.get('handler', '')).startswith('manalinkeh_card_')
                        or not exact(rows.get(face['name']), face) or face['csvid'] != row['csvid'] + 1
                        or face['handler'] != row['handler']):
                    raise ValueError('exact implemented Bestow creature and Aura identities required')
                classify(oracle[name], row['csvid'])
    if (oracle[HOLY]['oracle_text'] != 'Enchant creature\nEnchanted creature gets +1/+2.'
            or oracle[CRYPT]['oracle_text'] != "{T}, Sacrifice this artifact: Exile target player's graveyard."
            or oracle[OVERSEER]['oracle_text'] != '{R}{R}: Target creature gets +1/-1 until end of turn.'
            or not oracle[PRODIGAL]['oracle_text'].endswith('deals 1 damage to any target.')):
        raise ValueError('exact Aura, graveyard and paired targeting witnesses required')


class Duel:
    def __init__(self, row, owner, mode, rows, oracle):
        self.id, self.name, self.owner, self.foe, self.mode = row['csvid'], row['name'], owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.active = owner
        self.steps, self.setup, self.cards = [], [], {}
        self.life = [20, 20]
        self.form = None
        self.holy_host = None
        self.add('source', self.name, owner)
        self.add('probe', 'Murder', self.foe)

    def add(self, ref, name, owner, zone='hand'):
        from recipes import card

        assert ref not in self.cards
        self.cards[ref] = dict(name=name, owner=owner, controller=owner, zone=zone)
        self.setup.append(card(name, ref, owner, zone))

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def prop(self, ref, name, value):
        self.emit('assert_card', ref, prop=name, value=value)

    def move(self, ref, zone):
        card = self.cards[ref]
        card['zone'] = zone
        card['controller'] = card['owner']

    def pool(self, op, who, amounts):
        for color, n in enumerate(amounts):
            self.emit(op, player=who, color=color, value=n)

    def spent(self):
        for who in (0, 1):
            self.pool('assert_mana', who, ZERO)

    def family_type(self, ref):
        if ref == 'source' and self.form == 'aura' and self.cards[ref]['zone'] == 'battlefield':
            return 'Enchantment — Aura'
        return self.oracle[self.cards[ref]['name']]['type_line']

    def dynamic(self):
        if self.id == 13689:
            return sum(int('Creature' in self.family_type(ref)) + int('Aura' in self.family_type(ref))
                       for ref, entry in self.cards.items()
                       if entry['controller'] == self.owner and entry['zone'] == 'battlefield')
        if self.id == 13490:
            return sum('Creature' in self.family_type(ref) for ref, entry in self.cards.items()
                       if entry['zone'] == 'graveyard')
        return 0

    def stats(self, ref):
        record = self.oracle[self.cards[ref]['name']]
        boost = 0
        if ref == 'source':
            boost = self.dynamic()
        if ref == 'host' and self.form == 'aura':
            boost = {13689: self.dynamic(), 13490: self.dynamic(), 13912: 1, 13861: 4}[self.id]
        for index, key in enumerate(('power', 'toughness'), start=1):
            self.prop(ref, key, int(record[key]) + boost + (index if ref == self.holy_host else 0))

    def receipts(self, *, observe_host=True):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.spent()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            ids = {self.rows[item['name']]['csvid'] for item in self.cards.values()} | {self.id + 1}
            for index, zone in enumerate(ZONES):
                entries = [self.id + 1 if ref == 'source' and self.form == 'aura' and zone == 'battlefield'
                           else self.rows[item['name']]['csvid'] for ref, item in self.cards.items()
                           if item['controller'] == player and item['zone'] == zone]
                self.emit('assert_zone', player=player, zone=index, value=len(entries))
                counts = Counter(entries)
                for csvid in sorted(ids):
                    self.emit('assert_zone_multiplicity', player=player, zone=index, index=csvid, value=counts[csvid])
        if self.cards['source']['zone'] == 'battlefield':
            self.prop('source', 'in_play', 1)
            self.prop('source', 'csvid', self.id + int(self.form == 'aura'))
            self.emit('can_target_creature', 'probe', 'source', value=int(self.form == 'creature'))
            if self.form == 'creature':
                self.stats('source')
        for ref in ('host', 'spare', 'ground', 'victim', 'grown'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield' and (observe_host or ref != 'host'):
                self.stats(ref)

    def select_phase(self):
        self.emit('turn', player=self.active, value=0)

    def start(self):
        self.select_phase()
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.pool('set_mana', who, ZERO)
        self.emit('refresh')

    def cast_source(self, bestow):
        fee = cost_from_oracle({'mana_cost': CARDS[self.id][0]}) if bestow else cost_from_oracle(self.oracle[self.name])
        self.pool('set_mana', self.owner, fee)
        self.emit('refresh')
        self.emit('can_cast', 'source', value=1)
        if bestow:
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('choose_target', 'host', player=self.owner)
        self.emit('cast', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.prop('source', 'in_hand', 0)
        self.prop('source', 'csvid', self.id + int(bestow))
        self.spent()

    def resolve(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def helper(self, ref, target=None, *, destination='graveyard', response=False):
        actor = self.cards[ref]['controller']
        self.pool('set_mana', actor, cost_from_oracle(self.oracle[self.cards[ref]['name']]))
        self.emit('refresh')
        if response:
            self.emit('announce_spell', 'source')
        if target:
            self.emit('choose_target', target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_stack', value=2 if response else 1)
        self.emit('assert_choices_consumed')
        self.spent()
        self.resolve(response=response)
        self.move(ref, destination)

    def enter(self, form):
        self.form = form
        self.move('source', 'battlefield')

    def rebind_host(self, controller):
        self.cards['host']['controller'] = controller
        self.receipts(observe_host=False)
        self.emit('bind_permanent', 'host', player=controller, value=self.rows[BEAR]['csvid'], index=-1)
        self.stats('host')

    def exile_grave(self, target):
        self.emit('can_activate', 'crypt', value=1)
        self.emit('choose_player', player=self.owner, value=target)
        self.emit('activate', 'crypt', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.move('crypt', 'graveyard')
        self.resolve()
        for ref, entry in self.cards.items():
            if entry['zone'] == 'graveyard' and entry['owner'] == target:
                self.move(ref, 'exile')

    def after_pending_bounce(self):
        self.cast_source(bestow=True)
        self.helper('bounce', 'host', response=True)
        self.move('host', 'hand')
        self.enter('creature')
        self.receipts()

    def eidolon(self):
        if self.mode == 'enemy_aura_ignored':
            self.add('grown', BEAR, self.owner, 'battlefield')
            self.add('holy', HOLY, self.foe)
            self.start()
            self.cast_source(False)
            self.resolve()
            self.enter('creature')
            self.receipts()
            self.advance()
            self.helper('holy', 'grown', destination='battlefield')
            self.holy_host = 'grown'
            self.receipts()
            return
        if self.mode == 'regular_growth':
            self.add('grown', BEAR, self.owner)
            self.add('holy', HOLY, self.owner)
            self.add('remove', 'Naturalize', self.foe)
            self.add('ground', GIANT, self.foe, 'battlefield')
            self.start()
            self.cast_source(False)
            self.resolve()
            self.enter('creature')
            self.receipts()
            self.helper('grown', destination='battlefield')
            self.receipts()
            self.helper('holy', 'grown', destination='battlefield')
            self.holy_host = 'grown'
            self.receipts()
            self.helper('remove', 'holy')
            self.move('holy', 'graveyard')
            self.holy_host = None
            self.receipts()
            return
        self.add('host', BEAR, self.foe, 'battlefield')
        self.add('bounce', 'Unsummon', self.foe)
        if self.mode == 'pending_host_lost':
            self.start()
            self.after_pending_bounce()
            return
        self.add('spare', GIANT, self.owner, 'battlefield')
        self.add('holy', HOLY, self.owner)
        self.start()
        self.cast_source(True)
        self.resolve()
        self.enter('aura')
        self.receipts()
        self.helper('holy', 'spare', destination='battlefield')
        self.holy_host = 'spare'
        self.receipts()
        self.helper('bounce', 'host')
        self.move('host', 'hand')
        self.enter('creature')
        self.receipts()

    def nighthowler(self):
        self.add('land_grave', 'Mountain', self.foe, 'graveyard')
        if self.mode != 'empty_graves':
            self.add('own_grave', BEAR, self.owner, 'graveyard')
            if self.mode != 'last_grave_exiled':
                self.add('foe_grave', GIANT, self.foe, 'graveyard')
            self.add('crypt', CRYPT, self.owner, 'battlefield')
        bestowed = self.mode == 'bestowed_graves'
        if bestowed:
            self.add('host', BEAR, self.owner, 'battlefield')
            self.add('victim', BEAR, self.foe, 'battlefield')
            self.add('kill', 'Murder', self.owner)
            self.add('bounce', 'Unsummon', self.foe)
        self.start()
        self.cast_source(bestowed)
        self.resolve()
        if self.mode == 'empty_graves':
            self.move('source', 'graveyard')
            self.receipts()
            return
        self.enter('aura' if bestowed else 'creature')
        self.receipts()
        if bestowed:
            self.helper('kill', 'victim')
            self.move('victim', 'graveyard')
            self.receipts()
        self.exile_grave(self.foe if bestowed else self.owner)
        if self.mode == 'last_grave_exiled':
            self.move('source', 'graveyard')
        self.receipts()
        if bestowed:
            self.helper('bounce', 'host')
            self.move('host', 'hand')
            self.enter('creature')
            self.receipts()

    def advance(self):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.select_phase()

    def flying_attack(self, ref):
        self.advance()
        self.advance()
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', ref, value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[self.foe] -= 1 if ref == 'source' else 3
        self.receipts()

    def siren(self):
        if self.mode != 'regular_flying':
            self.add('host', BEAR, self.foe, 'battlefield')
        if self.mode == 'pending_host_lost':
            self.add('bounce', 'Unsummon', self.foe)
            self.start()
            self.after_pending_bounce()
            return
        self.add('ground', GIANT, self.foe, 'battlefield')
        bestowed = self.mode in ('steal_fly_release', 'stolen_dies')
        if self.mode == 'stolen_dies':
            self.add('kill', 'Murder', self.foe)
        if self.mode == 'steal_fly_release':
            self.add('remove', 'Naturalize', self.foe)
            self.add('bounce', 'Unsummon', self.owner)
        self.start()
        self.cast_source(bestowed)
        self.resolve()
        self.enter('aura' if bestowed else 'creature')
        if bestowed:
            self.rebind_host(self.owner)
        else:
            self.receipts()
        if self.mode == 'stolen_dies':
            self.helper('kill', 'host')
            self.move('host', 'graveyard')
            self.enter('creature')
            self.receipts()
            return
        self.flying_attack('host' if bestowed else 'source')
        if bestowed:
            self.helper('remove', 'source')
            self.form = None
            self.move('source', 'graveyard')
            self.rebind_host(self.foe)
            self.helper('bounce', 'host')
            self.move('host', 'hand')
            self.receipts()

    def nautilus(self):
        bestowed = self.mode.startswith('bestowed_')
        nontap = self.mode == 'regular_nontap_ability'
        if bestowed:
            self.add('host', BEAR, self.owner, 'battlefield')
        if not bestowed:
            self.add('pinger', OVERSEER if nontap else PRODIGAL, self.foe, 'battlefield')
        else:
            self.add('target_spell', 'Giant Growth' if self.mode == 'bestowed_host_spell' else 'Boomerang', self.foe)
        self.start()
        self.cast_source(bestowed)
        self.resolve()
        self.enter('aura' if bestowed else 'creature')
        self.receipts()
        if not bestowed:
            if nontap:
                self.pool('set_mana', self.foe, cost_from_oracle({'mana_cost': '{R}{R}'}))
                self.emit('refresh')
            self.emit('can_activate', 'pinger', value=1)
            self.emit('choose_target', 'source', player=self.foe)
            self.emit('activate', 'pinger', value=1)
            self.emit('assert_stack', value=1)
            self.emit('assert_choices_consumed')
            self.prop('pinger', 'tapped', int(not nontap))
            if nontap:
                self.spent()
            self.resolve()
            self.move('source', 'graveyard')
        elif self.mode == 'bestowed_source_spell':
            self.helper('target_spell', 'source')
            self.form = None
            self.move('source', 'graveyard')
        else:
            self.helper('target_spell', 'host')
            self.move('host', 'graveyard')
            self.enter('creature')
        self.receipts()

    def build(self):
        if self.id == 13689:
            self.eidolon()
        elif self.id == 13490:
            self.nighthowler()
        elif self.id == 13912:
            self.siren()
        else:
            self.nautilus()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered dynamic Bestow primary CSVID required')
    output = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(rows[name], int(actor), mode, rows, oracle)
        game.build()
        output.append(dict(
            name=f'BestowDynamicAndControlRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=2609, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[]),
        ))
    return output



