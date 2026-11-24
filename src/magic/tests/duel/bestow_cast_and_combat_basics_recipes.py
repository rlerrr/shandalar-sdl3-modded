"""Printed Bestow combat keywords and real creature/Aura lifecycles."""

from functools import lru_cache
import json
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER, Scene as BestowScene
from library_search_recipes import wave6_exact as exact
from recipes import cost_from_oracle, step


FAMILY = 'bestow_cast_and_combat_basics'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
MODELS = {
    13384: ('{1}{B}', '{4}{B}', (1, 1),
            'Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)', 'Deathtouch'),
    13492: ('{2}{U}', '{4}{U}', (2, 2), 'Flying', 'Flying'),
    13498: ('{3}{G}', '{5}{G}', (3, 3), 'Trample', 'Trample'),
    13501: ('{2}{W}', '{4}{W}', (2, 2), 'Vigilance', 'Vigilance'),
}
MODES = ('creature_combat', 'bestow_combat', 'foreign_combat', 'detached_combat', 'pending_host_lost')
SEED = 2609
HOST = 'Elvish Mystic'
WALL = 'Wall of Roots'
FLYER = "Judge's Familiar"
WITNESSES = {HOST, WALL, 'Murder', 'Unsummon', FLYER, 'Naturalize'}
NEW_MODES = {13384: 'pending_replay', 13492: 'flying_blocker',
             13498: 'destroy_aura', 13501: 'vigilance_defends'}
GAPS = ['External cost modifiers and casting without paying the mana cost.',
        'Aura exchanges and multi-controller changes while attached.']



class CombatScene(BestowScene):
    def __init__(self, name, owner, mode, model, rows, oracle):
        # Preserve the proven base family's methods; the pilot has independent
        # whole-card and paired native face pins.
        self.name, self.owner, self.enemy, self.mode = name, owner, 1 - owner, mode
        self.model, self.registry, self.oracle = model, rows, oracle
        self.id = rows[name]['csvid']
        self.aura_id = rows[name + ' Bestowed']['csvid']
        self.setup, self.steps, self.objects = [], [], {}
        self.mana, self.life = [[0] * 6 for _ in (0, 1)], [20, 20]
        self.form = None
        self.snapshot = None

    def setup_cards(self):
        host_seat = self.enemy if self.mode == 'foreign_combat' else self.owner
        self.add('source', self.name, self.owner)
        self.add('probe', 'Murder', self.enemy)
        self.add('host', HOST, host_seat, 'battlefield')
        if self.mode in ('detached_combat', 'pending_host_lost', 'pending_replay'):
            self.add('helper', 'Unsummon', self.enemy)
        if self.mode == 'destroy_aura':
            self.add('helper', 'Naturalize', self.enemy)
        if self.mode == 'vigilance_defends':
            self.add('retaliator', HOST, self.enemy, 'battlefield')
        if self.mode == 'pending_host_lost' or self.model['ability'] == 'Vigilance':
            return
        if self.mode == 'flying_blocker':
            blocker = FLYER
        else:
            blocker = WALL if self.model['ability'] == 'Deathtouch' else HOST
        self.add('blocker', blocker, 1 - host_seat, 'battlefield')

    def fight(self, actor, attacker, *, aura_removed=False):
        ability = None if aura_removed else self.model['ability']
        if attacker == 'host':
            attacker_power = 1 if aura_removed else self.model['bonus'][0] + 1
        else:
            attacker_power = self.model['creature'][0]
        defender = 1 - actor
        blocked = ability in ('Deathtouch', 'Trample') or self.mode in ('flying_blocker', 'destroy_aura')
        tapped = ability != 'Vigilance'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', tapped)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if 'blocker' in self.objects:
            self.emit('block', ref='blocker', other=attacker, value=int(blocked))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', blocked)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.objects[attacker]['tapped'] = tapped
        if aura_removed:
            self.objects[attacker]['zone'] = 'graveyard'
        else:
            self.prop(attacker, 'tapped', tapped)
        damage = attacker_power
        if blocked:
            blocked_toughness = int(self.oracle[self.objects['blocker']['name']]['toughness'])
            damage = attacker_power - blocked_toughness if ability == 'Trample' else 0
            if ability == 'Deathtouch':
                assert 0 < attacker_power < blocked_toughness
            self.objects['blocker']['zone'] = 'graveyard'
        self.life[defender] -= damage


    def defend_next_turn(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('upkeep_step', player=self.enemy)
        self.emit('turn', player=self.enemy, value=0)
        self.prop('host', 'tapped', 0)
        self.emit('begin_attackers')
        self.emit('attack', ref='retaliator', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='host', other='retaliator', value=1)
        self.emit('advance_combat')
        self.prop('retaliator', 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.objects['retaliator']['zone'] = 'graveyard'
        self.prop('host', 'in_play', 1)
        self.source_state()

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        ordinary = self.mode == 'creature_combat'
        self.cast_source(bestow=not ordinary, choice=None if ordinary else 0)
        if self.mode in ('pending_host_lost', 'pending_replay'):
            if self.mode == 'pending_replay':
                self.snapshot = len(self.steps)
            self.cast_helper('helper', 'host', response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
            if self.mode == 'pending_replay':
                self.age_turns()
                self.fight(self.owner, 'source')
            self.receipts()
            return
        self.resolve()
        self.entered('creature' if ordinary else 'aura')
        if self.mode in ('creature_combat', 'detached_combat'):
            self.age_turns()
        if self.mode == 'detached_combat':
            self.cast_helper('helper', 'host')
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        actor = self.owner
        if self.mode == 'foreign_combat':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.enemy)
            self.emit('upkeep_step', player=self.enemy)
            self.emit('turn', player=self.enemy, value=0)
            self.source_state()
            actor = self.enemy
        if self.mode == 'destroy_aura':
            self.cast_helper('helper', 'source')
            self.form = None
            self.objects['source']['zone'] = 'graveyard'
            self.host_stats()
        self.fight(actor, 'source' if self.form == 'creature' else 'host',
                   aura_removed=self.mode == 'destroy_aura')
        if self.mode == 'vigilance_defends':
            self.defend_next_turn()
        self.receipts()



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    normal, alternate, stats, printed, ability = MODELS[csvid]
    power, toughness = stats
    clauses = ['Bestow ' + alternate + REMINDER, printed,
               f'Enchanted creature gets +{power}/+{toughness} and has {ability.lower()}.']
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != normal
            or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Enchantment Creature — ')
            or set(record.get('keywords', ())) != {'Bestow', ability}
            or tuple(record.get(prop) for prop in ('power', 'toughness')) != tuple(map(str, stats))):
        raise ValueError('Exact normal creature and full printed Bestow keyword clause required')
    return dict(csvid=csvid, normal_cost=normal, bestow_cost=alternate, creature=list(stats),
                bonus=list(stats), ability=ability, clauses=clauses)


def variants(model):
    return [f'{mode}_{owner}' for mode in (*MODES, NEW_MODES[model['csvid']]) for owner in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {value['registry']['csvid'] for value in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact Bestow source and witness scope required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            expected = dict(oracle=record, registry=row)
            if group == 'sources':
                aura = rows.get(name + ' Bestowed', {})
                if (aura.get('csvid') != row['csvid'] + 1 or aura.get('iid') != row['iid'] + 1
                        or aura.get('handler') != row['handler'] or aura.get('kind') != 'card'):
                    raise ValueError(f'Changed native Bestow Aura identity: {name}')
                expected['engine_aura_face'] = aura
            if (not exact(pin, expected) or record.get('layout') != 'normal' or row.get('kind') != 'card'
                    or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented Bestow source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Bestow primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Bestow clauses, paid modes, and scoped variants required')
    return dict(model, family=FAMILY, normal=cost_from_oracle(record),
                bestowed=cost_from_oracle({'mana_cost': model['bestow_cost']}))


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Bestow primary and variants required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        scene = CombatScene(name, int(owner), mode, model, rows, oracle)
        scene.play()
        actions = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = actions[cut:]
            actions = ([*actions[:cut], step('capture'), *tail, step('record_state'),
                        step('restore'), step('ai_roundtrip'), *tail, step('assert_recorded_state')])
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=GAPS)
        cases.append(dict(name=f'BestowCastCombatRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=scene.setup, steps=actions, _recipe=metadata))
    return cases
