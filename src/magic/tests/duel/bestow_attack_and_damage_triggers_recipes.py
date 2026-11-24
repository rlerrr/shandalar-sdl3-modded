"""Bestow creature/Aura identity, real attacks and damage riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER, Scene
from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'bestow_attack_and_damage_triggers'
PROFILE = Path(__file__).with_name('data') / 'bestow_attack_and_damage_triggers_wave8_profiles.json'
HELIOD, SPITE, THASSA, KESTIA = (13456, 13812, 13575, 19100)
RULES = {
    HELIOD: ('{6}{W}', (
        'Whenever this creature or enchanted creature attacks, tap target creature an opponent controls.',
        'Enchanted creature gets +3/+3.',
    ), 3),
    SPITE: ('{3}{B}', (
        'Whenever this creature or enchanted creature attacks, defending player loses 2 life.',
        'Enchanted creature gets +1/+1.',
    ), 1),
    THASSA: ('{5}{U}', (
        'Whenever this creature or enchanted creature deals combat damage to a player, draw a card.',
        'Enchanted creature gets +3/+3.',
    ), 3),
    KESTIA: ('{3}{G}{W}{U}', (
        'Enchanted creature gets +4/+4.',
        'Whenever an enchanted creature or enchantment creature you control attacks, draw a card.',
    ), 4),
}
COMMON = ('regular_attack', 'bestowed_attack', 'foreign_attack', 'detach_attack')
MODES = {
    HELIOD: (*COMMON, 'no_legal_target'),
    SPITE: (*COMMON, 'unrelated_attacker'),
    THASSA: (*COMMON, 'blocked', 'noncombat_damage'),
    KESTIA: (*COMMON, 'team', 'opponent_enchantment'),
}
LIBRARY = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp')
ZA = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
WITNESSES = {
    *LIBRARY, 'Murder', 'Grizzly Bears', 'Wall of Stone', 'Unsummon', 'Hill Giant',
    'Nyxborn Shieldmate', 'Holy Strength', "Soul's Fire",
}
AURA_FORMS = {
    HELIOD: "Heliod's Emissary Bestowed",
    SPITE: 'Spiteful Returned Bestowed',
    THASSA: "Thassa's Emissary Bestowed",
}






@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in RULES or record.get('layout') != 'normal' or record.get('selected_face')
            or record.get('card_faces') or record.get('keywords') != ['Bestow']
            or 'Enchantment Creature' not in record.get('type_line', '')):
        raise ValueError('complete normal Bestow creature required')
    price, clauses, bonus = RULES[csvid]
    if record.get('oracle_text') != '\n'.join(('Bestow ' + price + REMINDER, *clauses)):
        raise ValueError('complete printed Bestow, stat and combat clauses required')
    return dict(csvid=csvid, normal=cost_from_oracle(record),
                bestowed=cost_from_oracle({'mana_cost': price}),
                creature=[int(record['power']), int(record['toughness'])], bonus=[bonus, bonus])


def variants(shape):
    return [f'{mode}_{owner}' for mode in MODES[shape['csvid']] for owner in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported Bestow combat cases required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact whole printed Bestow combat source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned Bestow combat declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    profile = pins()
    if (set(profile) != {'sources', 'witnesses', 'engine_aura_forms'}
            or set(profile['witnesses']) != WITNESSES):
        raise ValueError('complete independently pinned Bestow combat fixtures required')
    for group in ('sources', 'witnesses'):
        for name, pin in profile[group].items():
            row = rows.get(name, {})
            if (not exact(row, pin['registry']) or not exact(oracle.get(name), pin['oracle'])
                    or row.get('kind') != 'card' or row.get('handler') in ('NULL', '0', None)):
                raise ValueError('exact source or witness changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented normal Bestow primary required')
                cid = row['csvid']
                classify(oracle[name], cid)
                observed = profile['engine_aura_forms'][name]
                if cid == KESTIA:
                    wanted = dict(convention='same-csvid dynamic type', registry=row)
                else:
                    aura = rows.get(AURA_FORMS[cid], {})
                    if aura.get('csvid') != cid + 1 or aura.get('handler') != row['handler']:
                        raise ValueError('exact engine Bestowed identity required')
                    wanted = dict(convention='paired', registry=aura)
                if not exact(observed, wanted):
                    raise ValueError('paired or dynamic Aura implementation identity changed')
    if (oracle['Holy Strength']['oracle_text'] != 'Enchant creature\nEnchanted creature gets +1/+2.'
            or oracle['Wall of Stone']['power'] != '0' or oracle['Wall of Stone']['toughness'] != '8'
            or not oracle['Nyxborn Shieldmate']['type_line'].startswith('Enchantment Creature')
            or oracle["Soul's Fire"]['oracle_text'] != 'Target creature you control deals damage equal to its power to any target.'):
        raise ValueError('exact neutral, enchanted and noncombat damage controls required')


class Game(Scene):
    def __init__(self, row, actor, mode, rows, oracle):
        self.name, self.owner, self.enemy, self.mode = row['name'], actor, 1 - actor, mode
        self.id = row['csvid']
        self.model = classify(oracle[self.name], self.id)
        self.registry, self.oracle = rows, oracle
        self.aura_id = self.id if self.id == KESTIA else rows[self.name + ' Bestowed']['csvid']
        self.setup, self.steps, self.objects = [], [], {}
        self.mana = [[0] * 6 for _ in (0, 1)]
        self.life, self.form = [20, 20], None
        self.library = {p: list(LIBRARY) for p in (0, 1)}
        self.attacking_player = self.enemy if mode in ('foreign_attack', 'opponent_enchantment') else self.owner
        self.blocked = mode == 'blocked' or (self.id == SPITE and mode == 'bestowed_attack')
        self.attackers = ('enchanted', 'both', 'host') if mode == 'team' else (
            ('host',) if mode in ('bestowed_attack', 'foreign_attack', 'blocked', 'unrelated_attacker') else ('source',))
        self.add('source', self.name, self.owner)
        self.add('probe', 'Murder', self.enemy)
        host = 'Nyxborn Shieldmate' if mode == 'opponent_enchantment' else 'Grizzly Bears'
        self.add('host', host, self.attacking_player, 'battlefield')
        if mode == 'opponent_enchantment':
            self.attackers = ('host',)
        if self.id == HELIOD and mode != 'no_legal_target':
            self.add('tap_target', 'Wall of Stone', self.enemy, 'battlefield')
        if self.blocked:
            self.add('blocker', 'Wall of Stone', 1 - self.attacking_player, 'battlefield')
        if mode == 'detach_attack':
            self.add('helper', 'Unsummon', self.enemy)
        if mode == 'noncombat_damage':
            self.add('damage_spell', "Soul's Fire", self.owner)
        if mode == 'team':
            self.add('enchanted', 'Hill Giant', self.owner, 'battlefield')
            self.add('both', 'Nyxborn Shieldmate', self.owner, 'battlefield')
            self.add('aura_one', 'Holy Strength', self.owner)
            self.add('aura_two', 'Holy Strength', self.owner)
        for p, names in self.library.items():
            for i, name in enumerate(names):
                self.add(f'library_{p}_{i}', name, p, 'library')

    def move_draws(self, n):
        for _ in range(n):
            name = self.library[self.owner].pop(0)
            matching, = [e for e in self.objects.values() if e['player'] == self.owner
                         and e['zone'] == 'library' and e['name'] == name]
            matching['zone'] = 'hand'

    def checkpoints(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.pools()
        for p in (0, 1):
            self.emit('assert_life', player=p, value=self.life[p])
            for zone in ZA:
                counts = Counter(
                    self.aura_id if ref == 'source' and self.form == 'aura' else self.registry[e['name']]['csvid']
                    for ref, e in self.objects.items() if (e['player'], e['zone']) == (p, zone)
                )
                self.emit('assert_zone', player=p, zone=zone, value=sum(counts.values()))
                for cid in sorted({self.id, self.aura_id, *(self.registry[e['name']]['csvid']
                                                          for e in self.objects.values())}):
                    self.emit('assert_zone_multiplicity', player=p, zone=zone, index=cid, value=counts[cid])
                if zone == 'library':
                    for index, name in enumerate(self.library[p]):
                        self.emit('assert_zone_card', player=p, zone=zone, index=index,
                                  value=self.registry[name]['csvid'])

    def cast_team_aura(self, ref, host):
        self.fund(self.owner, cost_from_oracle(self.oracle['Holy Strength']))
        self.emit('choose_target', player=self.owner, ref=host)
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.spend(self.owner, cost_from_oracle(self.oracle['Holy Strength']))
        self.resolve()
        self.objects[ref]['zone'] = 'battlefield'
        self.prop(host, 'power', int(self.oracle[self.objects[host]['name']]['power']) + 1)
        self.prop(host, 'toughness', int(self.oracle[self.objects[host]['name']]['toughness']) + 2)

    def advance_to_attack(self):
        if self.attacking_player == self.owner:
            self.age_turns()
            return
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('upkeep_step', player=self.enemy)
        self.emit('turn', player=self.enemy, value=0)
        self.source_state()

    def attacker_power(self, ref):
        if ref == 'source':
            return self.model['creature'][0]
        base = int(self.oracle[self.objects[ref]['name']]['power'])
        return base + (self.model['bonus'][0] if self.form == 'aura' and ref == 'host' else (
            1 if self.mode == 'team' and ref in ('enchanted', 'both') else 0))

    def combat(self):
        defender = 1 - self.attacking_player
        for ref in self.attackers:
            self.prop(ref, 'sick', 0)
        if 'tap_target' in self.objects:
            self.prop('tap_target', 'tapped', 0)
        self.emit('begin_attackers')
        for ref in self.attackers:
            self.emit('attack', ref=ref, value=1)
            self.prop(ref, 'attacking', 1)
        if 'tap_target' in self.objects:
            self.emit('choose_target', player=self.owner, ref='tap_target')
        self.emit('finish_attackers')
        self.emit('assert_unused_choices', value=0)
        if 'tap_target' in self.objects:
            self.prop('tap_target', 'tapped', 1)
        elif self.id == SPITE and self.mode != 'unrelated_attacker':
            self.life[defender] -= 2
        elif self.id == KESTIA and self.attacking_player == self.owner:
            self.move_draws(2 if self.mode == 'team' else 1)
        self.checkpoints()
        self.emit('begin_blockers')
        if self.blocked:
            target, = self.attackers
            self.emit('block', ref='blocker', other=target, value=1)
        self.emit('advance_combat')
        if self.blocked:
            self.prop(self.attackers[0], 'blocked', 1)
        self.emit('combat_damage')
        self.resolve()
        if self.blocked:
            self.prop('blocker', 'damage', self.attacker_power(self.attackers[0]))
        else:
            self.life[defender] -= sum(self.attacker_power(ref) for ref in self.attackers)
            if self.id == THASSA:
                self.move_draws(1)
        self.checkpoints()
        self.emit('end_combat')
        self.emit('assert_choices_consumed')
        self.checkpoints()

    def noncombat_damage(self):
        price = cost_from_oracle(self.oracle["Soul's Fire"])
        self.fund(self.owner, price)
        self.emit('choose_target', ref='source', player=self.owner)
        self.emit('choose_player', player=self.owner, value=self.enemy)
        self.emit('cast', ref='damage_spell', value=1)
        self.emit('assert_stack', value=1)
        self.spend(self.owner, price)
        self.resolve()
        self.objects['damage_spell']['zone'] = 'graveyard'
        self.life[self.enemy] -= self.attacker_power('source')
        self.checkpoints()

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for p in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=p, color=color, value=0)
        self.emit('refresh')
        aura = self.mode in ('bestowed_attack', 'foreign_attack', 'detach_attack', 'blocked')
        self.cast_source(bestow=aura, choice=0 if aura else None)
        self.resolve()
        self.entered('aura' if aura else 'creature')
        if self.mode == 'noncombat_damage':
            self.noncombat_damage()
            return
        if self.mode == 'team':
            self.cast_team_aura('aura_one', 'enchanted')
            self.cast_team_aura('aura_two', 'both')
        self.advance_to_attack()
        if self.mode == 'detach_attack':
            self.cast_helper('helper', 'host')
            self.objects['host']['zone'] = 'hand'
            # Stop after the actual identity discrepancy before attempting to use a wrong slot.
            if self.id == KESTIA:
                self.prop('source', 'csvid', self.id, required=True)
            self.entered('creature')
        self.checkpoints()
        self.combat()



def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Bestow combat CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = Game(rows[name], int(owner), mode, rows, oracle)
        game.play()
        cases.append(dict(
            name=f'BestowAttackAndDamageTriggersRecipe_{csvid}_{variant}', card=name,
            fixture='empty_main', seed=2609, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=[]),
        ))
    return cases
