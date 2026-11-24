"""Bestow evasion and blocking through real paid casts and declaration."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER, Scene
from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle, step


FAMILY = 'bestow_evasion_and_blocking'
PROFILE = Path(__file__).with_name('data') / 'bestow_evasion_and_blocking_wave8_profiles.json'
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')

LAMPAD = 13401
EIDOLON = 13714
SCARHIDE = 13896
EMISSARY = 13526
PRINTED = {
    LAMPAD: ('{3}{B}', '{5}{B}', 'Nymph', ['Bestow', 'Intimidate'], (2, 2), (2, 2),
             "Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)",
             'Enchanted creature gets +2/+2 and has intimidate.'),
    EIDOLON: ('{1}{U}', '{5}{U}', 'Spirit', ['Bestow'], (1, 1), (1, 1),
              "This creature can't be blocked.", "Enchanted creature gets +1/+1 and can't be blocked."),
    SCARHIDE: ('{B}', '{3}{B}', 'Minotaur', ['Bestow'], (2, 1), (2, 1),
               "This creature can't block.", "Enchanted creature gets +2/+1 and can't block."),
    EMISSARY: ('{3}{R}', '{6}{R}', 'Ox', ['Menace', 'Bestow'], (3, 3), (3, 3),
               "Menace (This creature can't be blocked except by two or more creatures.)",
               'Enchanted creature gets +3/+3 and has menace.'),
}
MODES = {
    LAMPAD: ('regular_wrong', 'regular_match', 'bestowed_match', 'bestowed_wrong', 'bestowed_artifact', 'pending_fallback'),
    EIDOLON: ('regular', 'bestowed', 'detach', 'pending_fallback'),
    SCARHIDE: ('regular', 'bestowed', 'foreign_host', 'removed_aura', 'pending_fallback'),
    EMISSARY: ('regular_lone', 'regular_pair', 'bestowed_lone', 'bestowed_pair', 'pending_fallback'),
}

DRYAD = 13474
FLIER = 'Warden of Evos Isle'
PRINTED[DRYAD] = ('{1}{G}', '{3}{G}', 'Nymph Dryad', ['Reach', 'Bestow'], (2, 2), (2, 2),
                 'Reach', 'Enchanted creature gets +2/+2 and has reach.')
MODES[DRYAD] = ('regular', 'bestowed', 'pending_fallback', 'removed_aura')
NEW_WITNESSES = {FLIER}
WITNESSES = {'Cuombajj Witches', 'Darksteel Myr', 'Dazzling Ramparts', 'Forest', 'Island',
             'Merfolk Mesmerist', 'Mountain', 'Murder', 'Naturalize', 'Orcish Artillery',
             'Plains', 'Prodigal Sorcerer', 'Unsummon'} | NEW_WITNESSES
PAIR_GAP = 'Both legal blockers are verified at the completed declaration. Paired damage allocation is outside this scope.'


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in PRINTED or not record or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal Bestow restriction primary required')
    cost, bestowed, subtype, keywords, stats, bonus, inherent, aura = PRINTED[csvid]
    text = '\n'.join(('Bestow ' + bestowed + REMINDER, inherent, aura))
    if (record.get('layout') != 'normal' or record.get('type_line') != 'Enchantment Creature — ' + subtype
            or record.get('oracle_text') != text or record.get('keywords') != keywords
            or record.get('mana_cost') != cost
            or (record.get('power'), record.get('toughness')) != tuple(map(str, stats))):
        raise ValueError('complete printed Bestow cost, stats, restriction and granted rider required')
    return dict(csvid=csvid, normal=cost_from_oracle(record), bestowed=cost_from_oracle({'mana_cost': bestowed}),
                creature=list(stats), bonus=list(bonus))


def variants(shape):
    return [f'{mode}_{owner}' for mode in MODES[shape['csvid']] for owner in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(m) is not str for m in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Bestow restriction variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', ''))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned Bestow restriction source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole version 2 Bestow restriction declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Bestow source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned Bestow source or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])
            form = pin['engine_aura_face']
            if (not exact(rows.get(form['name']), form) or form['name'] != name + ' Bestowed'
                    or form['csvid'] != row['csvid'] + 1 or form['handler'] != row['handler']
                    or form['name'] in oracle):
                raise ValueError('exact non-primary Bestow Aura identity required: ' + name)
    flier = oracle[FLIER]
    if ((flier['power'], flier['toughness']) != ('2', '2') or flier['keywords'] != ['Flying']
            or flier['oracle_text'] != 'Flying\nCreature spells with flying you cast cost {1} less to cast.'):
        raise ValueError('reviewed non-ETB 2/2 Flying witness required')


class Duel(Scene):
    def __init__(self, name, owner, mode, model, rows, oracle, aura_id):
        # Reuse the established Bestow cast, response, and turn operations.
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.model, self.registry, self.oracle = model, rows, oracle
        self.id, self.aura_id = rows[name]['csvid'], aura_id
        self.setup, self.steps, self.objects = [], [], {}
        self.mana = [[0] * 6 for _ in (0, 1)]
        self.life = [20, 20]
        self.form = None
        self.current = owner
        self.add('source', name, owner)
        self.add('probe', 'Murder', self.enemy)
        self.add('host', 'Merfolk Mesmerist', self.enemy if mode == 'foreign_host' else owner, 'battlefield')
        if mode in ('pending_fallback', 'detach'):
            self.add('helper', 'Unsummon', self.enemy)
        if mode == 'removed_aura':
            self.add('helper', 'Naturalize', self.enemy)
        for who in (0, 1):
            for index, name in enumerate(('Forest', 'Island') if who == owner else ('Plains', 'Mountain')):
                self.add(f'library_{who}_{index}', name, who, 'library')
        if self.id == SCARHIDE:
            self.add('swing', 'Orcish Artillery', owner if mode == 'foreign_host' else self.enemy, 'battlefield')
        elif self.id == DRYAD:
            self.add('swing', FLIER, self.enemy, 'battlefield')
        elif self.id == LAMPAD:
            blocker = {
                'regular_match': 'Cuombajj Witches',
                'regular_wrong': 'Dazzling Ramparts',
                'bestowed_match': 'Prodigal Sorcerer',
                'bestowed_wrong': 'Cuombajj Witches',
                'bestowed_artifact': 'Darksteel Myr',
                'pending_fallback': 'Dazzling Ramparts',
            }[mode]
            self.add('blocker', blocker, self.enemy, 'battlefield')
        else:
            self.add('blocker', 'Dazzling Ramparts', self.enemy, 'battlefield')
            if mode.endswith('_pair'):
                self.add('second', 'Dazzling Ramparts', self.enemy, 'battlefield')

    def emit(self, op, **fields):
        super().emit('can_cast' if op == 'require_can_cast' else op, **fields)

    def prop(self, ref, key, value, required=False):
        self.emit('assert_card', ref=ref, property=key, value=int(value))

    def exact_state(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        self.host_stats()
        for ref, item in self.objects.items():
            if item['zone'] in ('hand', 'battlefield'):
                self.prop(ref, 'in_play', item['zone'] == 'battlefield')
        names = {entry['name'] for entry in self.objects.values()}
        known_ids = sorted({self.aura_id, *[self.registry[name]['csvid'] for name in names]})
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in ZONES:
                contents = [
                    self.aura_id if ref == 'source' and self.form == 'aura' else self.registry[item['name']]['csvid']
                    for ref, item in self.objects.items() if item['player'] == who and item['zone'] == zone
                ]
                self.emit('assert_zone', player=who, zone=zone, value=len(contents))
                if zone == 'library':
                    for index, csvid in enumerate(contents):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=csvid)
                else:
                    mult = Counter(contents)
                    for csvid in known_ids:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=mult[csvid])

    def visit_enemy(self):
        assert self.current == self.owner and not any(map(any, self.mana))
        self.current = self.enemy
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('upkeep_step', player=self.enemy)
        self.emit('turn', player=self.enemy)

    def stats(self, ref):
        item = self.objects[ref]
        record = self.oracle[item['name']]
        delta = self.model['bonus'] if ref == 'host' and self.form == 'aura' else (0, 0)
        return tuple(int(record[key]) + bonus for key, bonus in zip(('power', 'toughness'), delta))

    def block_damage(self, ref, incoming, toughness):
        record = self.oracle[self.objects[ref]['name']]
        if incoming >= toughness and 'Indestructible' not in record['keywords']:
            self.objects[ref]['zone'] = 'graveyard'
        else:
            self.prop(ref, 'damage', incoming)

    def combat(self, attacker, attempted, accepted):
        assert self.objects[attacker]['player'] == self.current
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref in attempted:
            self.emit('block', ref=ref, other=attacker, value=int(ref in accepted))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', bool(accepted))
        for ref in attempted:
            self.prop(ref, 'blocking_count', ref in accepted)
        if len(accepted) == 2:
            # The actual completed declaration proves the pair qualifies.
            # Neither seat must choose a particular damage allocation here.
            self.exact_state()
            return
        power, toughness = self.stats(attacker)
        self.emit('combat_damage')
        self.emit('resolve')
        if accepted:
            blocker, = accepted
            block_power, block_toughness = self.stats(blocker)
            self.block_damage(blocker, power, block_toughness)
            self.block_damage(attacker, block_power, toughness)
        else:
            self.life[1 - self.current] -= power
        self.emit('end_combat')
        self.exact_state()

    def play_fixture(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=who, color=color, value=0)
        self.emit('refresh')
        self.exact_state()
        alternate = not self.mode.startswith('regular')
        self.cast_source(bestow=alternate, choice=0 if alternate else None)
        if self.mode == 'pending_fallback':
            self.cast_helper('helper', 'host', response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        else:
            self.resolve()
            self.entered('aura' if alternate else 'creature')
        self.exact_state()
        if self.mode in ('detach', 'removed_aura'):
            self.cast_helper('helper', 'host' if self.mode == 'detach' else 'source')
            if self.mode == 'detach':
                self.objects['host']['zone'] = 'hand'
                self.entered('creature')
            else:
                self.form = None
                self.objects['source']['zone'] = 'graveyard'
                self.host_stats()
            self.exact_state()

    def build(self):
        self.play_fixture()
        if self.form:
            self.age_turns()
        if self.id == SCARHIDE:
            if self.mode != 'foreign_host':
                self.visit_enemy()
            blocker = 'source' if self.form == 'creature' else 'host'
            self.combat('swing', (blocker,), (blocker,) if self.mode == 'removed_aura' else ())
            return
        if self.id == DRYAD:
            self.visit_enemy()
            blocker = 'source' if self.form == 'creature' else 'host'
            self.combat('swing', (blocker,), (blocker,) if self.form else ())
            return
        attacker = 'source' if self.form == 'creature' else 'host'
        if self.id == EMISSARY and self.mode.endswith('_pair'):
            self.combat(attacker, ('blocker', 'second'), ('blocker', 'second'))
            return
        valid = self.id == LAMPAD and self.mode in ('regular_match', 'bestowed_match', 'bestowed_artifact')
        self.combat(attacker, ('blocker',), ('blocker',) if valid else ())


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    primary = profiles()['sources'][name]
    if primary['registry']['csvid'] != csvid:
        raise ValueError('exact registered Bestow primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        world = Duel(name, int(seat), mode, shape, rows, oracle, primary['engine_aura_face']['csvid'])
        world.build()
        cases.append(dict(
            name=f'BestowEvasionBlockingRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2609,
            setup=world.setup, steps=world.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=[PAIR_GAP] if mode.endswith('_pair') else []),
        ))
    return cases
