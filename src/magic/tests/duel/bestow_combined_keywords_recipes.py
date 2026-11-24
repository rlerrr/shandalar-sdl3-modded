"""Whole Oracle Bestow with actual Aura, creature and combat transitions."""
from functools import lru_cache
import json
from pathlib import Path
import re

from bestow_stat_aura_recipes import REMINDER, Scene
from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'bestow_combined_keywords'
PROFILE = Path(__file__).with_name('data') / 'bestow_combined_keywords_wave8_profiles.json'
ARCHON, CHROMANTICORE, GHOSTBLADE, OREAD, HOPEFUL = 13403, 13678, 13720, 13557, 13459
PROFILES = {
    ARCHON: ('{5}{W}{W}', 4, 'Flying, first strike', 'flying and first strike'),
    CHROMANTICORE: ('{2}{W}{U}{B}{R}{G}', 4, 'Flying, first strike, vigilance, trample, lifelink',
                    'flying, first strike, vigilance, trample, and lifelink'),
    GHOSTBLADE: ('{5}{W}', 1, 'Double strike (This creature deals both first-strike and regular combat damage.)', 'double strike'),
    OREAD: ('{5}{R}', 2, 'First strike', 'first strike'),
    HOPEFUL: ('{3}{W}', 1, 'Lifelink (Damage dealt by this creature also causes you to gain that much life.)', 'lifelink'),
}
SHAPES = ('regular_combat', 'bestowed_combat', 'detach_combat', 'pending_host_lost')
PREFIX = 'BestowCombinedKeywordsRecipe'
SEED = 123
BEAR, AIR, DRAKE, GIANT = 'Grizzly Bears', 'Air Elemental', 'Wind Drake', 'Hill Giant'
MURDER, UNSUMMON = 'Murder', 'Unsummon'
WITNESSES = {BEAR, AIR, DRAKE, GIANT, MURDER, UNSUMMON}



@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (csvid not in PROFILES or not pin or pin['registry']['csvid'] != csvid or not exact(pin['oracle'], record)
            or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record['type_line'].startswith('Enchantment Creature — ')):
        raise ValueError('whole pinned normal Bestow primary required')
    posted, size, keyword_line, granted = PROFILES[csvid]
    fee, intrinsic, attached = record['oracle_text'].splitlines()
    expected_keywords = set(intrinsic.split(' (', 1)[0].lower().split(', ')) | {'bestow'}
    bonus = re.fullmatch(r'Enchanted creature gets \+([1-4])/\+([1-4]) and has (.*)\.', attached)
    if (fee != f'Bestow {posted}{REMINDER}' or intrinsic != keyword_line or not bonus
            or tuple(bonus.groups()) != (str(size), str(size), granted)
            or {key.lower() for key in record['keywords']} != expected_keywords):
        raise ValueError('exact printed Bestow cost and keyword grant required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), bestowed=cost_from_oracle({'mana_cost': posted}),
                bonus=[int(bonus[1]), int(bonus[2])], creature=[int(record['power']), int(record['toughness'])])


def variants(shape):
    csvid = shape['csvid']
    extra = ('flight',) if csvid in (ARCHON, CHROMANTICORE) else ('foe_link', 'creature_link') if csvid == HOPEFUL else ()
    return [f'{mode}_{seat}' for mode in (*SHAPES, *extra) for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Bestow variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('reserved Bestow source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact declared Bestow parameters required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent Bestow witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != PROFILES.keys():
        raise ValueError('exact five-source Bestow profile required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('exact Bestow source or witness changed: ' + name)
            if group == 'sources':
                aura = pin['engine_aura_face']
                if (row['handler'] == 'card_dummy' or not exact(rows.get(aura['name']), aura)
                        or aura['csvid'] != row['csvid'] + 1 or aura['handler'] != row['handler']):
                    raise ValueError('paired implemented Bestow identity required: ' + name)
                classify(oracle[name], row['csvid'])


class CombinedScene(Scene):
    def __init__(self, name, owner, mode, model, rows, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.model, self.registry, self.oracle = model, rows, oracle
        self.id = rows[name]['csvid']
        self.aura_id = rows[name + ' Bestowed']['csvid']
        self.setup, self.steps, self.objects = [], [], {}
        self.mana = [[0] * 6 for _ in (0, 1)]
        self.life = [20, 20]
        self.form = None

    def setup_cards(self):
        self.add('source', self.name, self.owner)
        self.add('probe', MURDER, self.enemy)
        host_controller = self.enemy if self.mode == 'foe_link' else self.owner
        self.add('host', BEAR, host_controller, 'battlefield')
        if self.mode in ('pending_host_lost', 'detach_combat'):
            self.add('helper', UNSUMMON, self.enemy)
        if self.mode == 'pending_host_lost':
            return
        if self.mode == 'flight':
            self.add('blocker', GIANT, self.enemy, 'battlefield')
        elif self.mode == 'creature_link':
            self.add('blocker', BEAR, self.enemy, 'battlefield')
        else:
            blocker = {ARCHON: AIR, CHROMANTICORE: DRAKE, OREAD: BEAR}.get(self.id)
            if blocker:
                self.add('blocker', blocker, self.enemy, 'battlefield')

    def combat(self, attacker):
        keywords = {value.lower() for value in self.oracle[self.name]['keywords']}
        flying = 'flying' in keywords
        vigilance = 'vigilance' in keywords
        double_strike = 'double strike' in keywords
        trample = 'trample' in keywords
        lifelink = 'lifelink' in keywords
        assert self.mode != 'flight' or flying
        power = self.model['creature'][0] if attacker == 'source' else (
            int(self.oracle[self.objects[attacker]['name']]['power']) + self.model['bonus'][0])
        blocked = 'blocker' in self.objects and self.mode != 'flight'
        defender_toughness = int(self.oracle[self.objects['blocker']['name']]['toughness']) if blocked else 0
        nominal = power * (2 if double_strike else 1)
        if blocked:
            assert 'first strike' in keywords and power >= defender_toughness
        player_damage = max(0, nominal - defender_toughness) if blocked and trample else 0 if blocked else nominal
        creature_damage = min(power, defender_toughness) if blocked else 0
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', not vigilance)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if 'blocker' in self.objects:
            self.emit('block', ref='blocker', other=attacker, value=int(blocked))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocked))
        self.emit('combat_damage')
        self.resolve()
        self.prop(attacker, 'in_play', 1)
        self.prop(attacker, 'damage', 0)
        controller = self.objects[attacker]['player']
        self.life[1-controller] -= player_damage
        if lifelink:
            self.life[controller] += player_damage + creature_damage
        if blocked:
            self.objects['blocker']['zone'] = 'graveyard'
        self.emit('end_combat')
        self.objects[attacker]['tapped'] = not vigilance
        self.prop(attacker, 'tapped', not vigilance)

    def creature_link(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other='source', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        self.prop('blocker', 'damage', self.model['creature'][0])
        self.life[self.owner] += self.model['creature'][0]
        self.objects['source']['zone'] = 'graveyard'
        self.form = None
        self.emit('end_combat')

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            for index in range(6):
                self.emit('set_mana', player=player, color=index, value=0)
        self.emit('refresh')
        normal = self.mode in ('regular_combat', 'creature_link')
        self.cast_source(bestow=not normal, choice=None if normal else 0)
        if self.mode == 'pending_host_lost':
            self.cast_helper('helper', 'host', response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
            return
        self.resolve()
        self.entered('creature' if normal else 'aura')
        if self.mode in ('regular_combat', 'detach_combat', 'creature_link'):
            self.age_turns()
        if self.mode == 'detach_combat':
            self.cast_helper('helper', 'host')
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        if self.mode == 'foe_link':
            self.emit('advance_turn')
            self.emit('untap_step', player=self.enemy)
            self.emit('upkeep_step', player=self.enemy)
            self.emit('turn', player=self.enemy, value=0)
            self.source_state()
        if self.mode == 'creature_link':
            self.creature_link()
        else:
            self.combat('source' if self.form == 'creature' else 'host')


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat = variant.rsplit('_', 1)
    scene = CombinedScene(name, int(seat), mode, shape, rows, oracle)
    scene.play()
    scene.receipts()
    return dict(name=f'{PREFIX}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                setup=scene.setup, steps=scene.steps,
                _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                             oracle_text=declaration['oracle_text'], untested_components=[]))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered Bestow source required')
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    return [build(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
