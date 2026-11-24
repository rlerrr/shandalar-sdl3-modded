"""Equipment attack triggers, exact board effects, and actual attachment changes."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'equipment_combat_attack_board_effects'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
GRAFT = 16201
MODELS = {
    11200: (6, 6, (6, 6), (
        'Equipped creature gets +6/+6.',
        'Whenever equipped creature attacks, destroy target permanent.',
        'Equip {6}'),
        ('destroy_own',)),
    12428: (4, 4, (0, 0), (
        'Equipped creature has first strike.',
        'Whenever equipped creature attacks, create a 4/4 white Angel creature token with flying.',
        'Equip {4}'),
        ('angel_flight',)),
    15236: (2, 2, (2, 0), (
        'Equipped creature gets +2/+0.',
        'Whenever equipped creature attacks, you may sacrifice this Equipment. '
        'If you do, this Equipment deals 2 damage to any target.',
        'Equip {2} ({2}: Attach to target creature you control. Equip only as a sorcery.)'),
        ('throw_at_creature', 'human_declines')),
    15566: (2, 1, (1, 0), (
        'Equipped creature gets +1/+0.',
        "Whenever equipped creature attacks, create a 1/1 white Kor Ally creature token that's tapped and attacking.",
        'Equip {1}'),
        ('block_the_token', 'token_later_attacks')),
    GRAFT: (1, 2, (3, 3), (
        'Equipped creature gets +3/+3.',
        "Whenever equipped creature attacks, it doesn't untap during its controller's next untap step.",
        'Whenever this Equipment becomes unattached from a permanent, sacrifice that permanent.',
        'Equip {2}'),
        ('freeze_once', 'tapped_other_attacks', 'reequip_sacrifice', 'destroy_source',
         'bounce_source_recast', 'host_departure', 'short_unattached_removal')),
}
COMMON = ('own_attack', 'other_attack', 'reequip_attack', 'unfunded', 'detached')
HOST = 'Goblin Balloon Brigade'
BACKUP = 'Goblin Tinkerer'
GROUNDER = 'Elvish Mystic'
AMULET = "Traveler's Amulet"
TOKENS = {12428: ('Angel', 18687, (4, 4), 'Angel'), 15566: ('Kor Ally', 18778, (1, 1), 'Ally')}
ZERO = (0,) * 6
SEED = 2660

ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
INDESTRUCTIBLE = 'Darksteel Myr'
GRAFT_SACRIFICE_MODES = {'reequip_sacrifice', 'destroy_source', 'bounce_source_recast'}
WITNESSES = {HOST, BACKUP, GROUNDER, AMULET, 'Forest', 'Unsummon', 'Angel', 'Kor Ally',
             INDESTRUCTIBLE, 'Icy Manipulator', 'Disenchant', 'Boomerang'}
GAPS = {12428: ['The complete combat phase establishes the equipped creature’s first strike; '
                'the observer does not pause between the two damage steps.'],
        15566: ['Ally has a direct subtype query. Kor is pinned by the exact native token identity.']}


class EquipmentBattle:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.other, self.mode = record, csvid, owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.funded, self.life = [ZERO, ZERO], [20, 20]
        self.equipped, self.effect = None, None
        _, self.equip_price, self.bonus, _, _ = MODELS[csvid]
        self.add('source', owner, record['name'], 'hand')
        host = INDESTRUCTIBLE if csvid == GRAFT and mode in GRAFT_SACRIFICE_MODES else HOST
        self.add('host', owner, host)
        self.add('backup', owner, BACKUP)
        if self.csvid == 11200:
            target_player = owner if mode == 'destroy_own' else self.other
            self.add('target', target_player, AMULET if mode == 'destroy_own' else 'Forest')
            self.add('bystander', self.other, AMULET)
        if csvid == 12428 or mode == 'block_the_token':
            self.add('blocker', self.other, GROUNDER)
        if mode == 'throw_at_creature':
            self.add('victim', self.other, BACKUP)
        if mode == 'detached':
            self.add('reply', self.other, 'Unsummon', 'hand')

    def add(self, ref, player, name, zone='battlefield'):
        self.setup.append(card(name, ref, player, zone))
        record = self.oracle[name]
        stats = tuple(int(record[p]) for p in ('power', 'toughness')) if 'power' in record else None
        self.cards[ref] = dict(name=name, player=player, zone=zone, stats=stats,
                               damage=0, tapped=False, attacking=False)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None,
             index=None, color=None):
        values = dict(ref=ref, other=other, property=prop, player=player, value=value, zone=zone,
                      index=index, color=color)
        self.steps.append(step(op, **{name: value for name, value in values.items() if value is not None}))

    def card(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def stats(self, ref):
        base = self.cards[ref]['stats']
        return tuple(x + y for x, y in zip(base, self.bonus)) if ref == self.equipped else base

    def fund(self, player, pool):
        assert not any(self.funded[player])
        self.funded[player] = pool
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        for who, amounts in enumerate(self.funded):
            for color, value in enumerate(amounts):
                self.emit('assert_mana', player=who, color=color, value=value)
            self.emit('assert_life', player=who, value=self.life[who])
            for zone in ZONES:
                located = [c['name'] for c in self.cards.values() if c['player'] == who and c['zone'] == zone]
                self.emit('assert_zone', player=who, zone=zone, value=len(located))
                known = {c['name'] for c in self.cards.values()}
                if self.csvid in TOKENS:
                    known.add(TOKENS[self.csvid][0])
                for name in sorted(known):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'],
                              value=located.count(name))
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.card(ref, 'in_play', 1)
            if card['stats'] is not None:
                power, toughness = self.stats(ref)
                self.card(ref, 'power', power)
                self.card(ref, 'toughness', toughness)
                self.card(ref, 'damage', card['damage'])
                self.card(ref, 'tapped', card['tapped'])
                self.card(ref, 'attacking', card['attacking'])

    def drain(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def cast(self, ref, *, target=None, destination='graveyard'):
        actor, name = (self.cards[ref][k] for k in ('player', 'name'))
        self.fund(actor, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', target, player=actor)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.funded[actor] = ZERO
        self.drain()
        self.cards[ref]['zone'] = destination
        self.receipt()

    def equip(self, target, *, affordable=True):
        pool = (self.equip_price - int(not affordable), 0, 0, 0, 0, 0)
        self.fund(self.owner, pool)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=int(affordable))
        if not affordable:
            self.receipt()
            return
        self.emit('choose_target', target, player=self.owner)
        self.emit('activate', 'source', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.funded[self.owner] = ZERO
        for ref in ('host', 'backup'):
            if self.cards[ref]['zone'] == 'battlefield':
                for prop, value in zip(('power', 'toughness'), self.stats(ref)):
                    self.card(ref, prop, value)
        self.drain()
        previous = self.equipped
        self.equipped = target
        if self.csvid == GRAFT and previous is not None and previous != target:
            self.cards[previous]['zone'] = 'graveyard'
        self.receipt()

    def queue_trigger(self, attacker):
        if attacker != self.equipped:
            return False
        if self.csvid == 11200:
            self.emit('choose_target', 'target', player=self.owner)
        if self.csvid == 15236:
            if self.mode == 'human_declines':
                return False
            if self.owner == 0:
                self.emit('choose_trigger_source', 'source', player=self.owner)
            if self.mode == 'throw_at_creature':
                self.emit('choose_target', 'victim', player=self.owner)
            else:
                self.emit('choose_player', player=self.owner, value=self.other)
        return True

    def create_token(self):
        name, csvid, stats, subtype = TOKENS[self.csvid]
        attacking = self.csvid == 15566
        self.cards['token'] = dict(name=name, player=self.owner, zone='battlefield', stats=stats,
                                   damage=0, tapped=attacking, attacking=attacking)
        self.emit('bind_permanent_copy', 'token', player=self.owner, value=csvid, index=0)
        for prop, value in (('token', 1), ('artifact', 0), ('color', 32), ('subtype_' + subtype, 1)):
            self.emit('assert_matching_permanents', prop=prop, player=self.owner, index=csvid, value=value)

    def trigger_effect(self):
        if self.csvid == 11200:
            self.cards['target']['zone'] = 'graveyard'
            self.effect = dict(destroyed=self.cards['target']['name'], player=self.cards['target']['player'])
        elif self.csvid == 15236:
            self.cards['source']['zone'] = 'graveyard'
            self.equipped = None
            if self.mode == 'throw_at_creature':
                self.cards['victim']['zone'] = 'graveyard'
            else:
                self.life[self.other] -= 2
            self.effect = dict(sacrificed=True, recipient='creature' if self.mode == 'throw_at_creature' else 'player')
        else:
            self.create_token()
            self.effect = dict(created=self.cards['token']['name'], attacking=self.cards['token']['attacking'])

    def begin_attack(self, attacker, *, check_trigger):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.cards[attacker].update(tapped=True, attacking=True)
        self.card(attacker, 'attacking', 1)
        self.card(attacker, 'tapped', 1)
        predict = check_trigger and self.queue_trigger(attacker)
        self.emit('finish_attackers')
        self.drain()
        if predict:
            self.trigger_effect()
        self.receipt()
        return predict

    def finish_combat(self, attacker, *, host_block=False, token_block=False, deny_block=False):
        self.emit('begin_blockers')
        if host_block:
            self.emit('block', 'blocker', attacker, value=1)
        if token_block:
            self.emit('block', 'blocker', 'token', value=1)
        if deny_block:
            self.emit('block', 'blocker', attacker, value=0)
        self.emit('advance_combat')
        self.card(attacker, 'blocked', host_block)
        if token_block:
            self.card('token', 'blocked', 1)
        self.emit('combat_damage')
        self.drain()
        if host_block:
            assert self.csvid == 12428 and self.equipped == attacker
            self.cards['blocker']['zone'] = 'graveyard'
            self.card(attacker, 'damage', 0)
        else:
            self.life[self.other] -= self.stats(attacker)[0]
        if attacker != 'token' and self.cards.get('token', {}).get('attacking'):
            if token_block:
                self.cards['token']['zone'] = 'ceased'
                self.cards['blocker']['zone'] = 'graveyard'
            else:
                self.life[self.other] -= self.stats('token')[0]
        self.emit('end_combat')
        for card in self.cards.values():
            card['attacking'] = False
        self.receipt()

    def next_own_combat(self):
        for seat in (self.other, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
            self.emit('upkeep_step', player=seat)
            self.emit('turn', player=seat)
            for card in self.cards.values():
                if card['zone'] == 'battlefield' and card['player'] == seat:
                    card['tapped'] = False
        self.receipt()
        self.begin_attack('token', check_trigger=False)
        self.finish_combat('token', deny_block=self.mode == 'angel_flight')

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.cast('source', destination='battlefield')
        self.equip('host', affordable=self.mode != 'unfunded')
        if self.mode == 'reequip_attack':
            self.equip('backup')
        if self.mode == 'detached':
            # The equipment remains when the actual host leaves the battlefield.
            self.equipped = None
            self.cards['host']['zone'] = 'hand'
            self.cast('reply', target='host')
        attacker = 'backup' if self.mode in ('other_attack', 'reequip_attack', 'detached') else 'host'
        triggered = self.begin_attack(attacker, check_trigger=True)
        self.finish_combat(attacker, host_block=triggered and self.csvid == 12428 and self.mode != 'angel_flight',
                           token_block=self.mode == 'block_the_token')
        if self.mode in ('angel_flight', 'token_later_attacks'):
            self.next_own_combat()



class GraftBattle(EquipmentBattle):
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        super().__init__(record, csvid, owner, mode, rows, oracle)
        if mode == 'tapped_other_attacks':
            self.add('tapper', self.other, 'Icy Manipulator')
        if mode == 'host_departure':
            self.add('reply', self.other, 'Unsummon', 'hand')
        elif mode == 'bounce_source_recast':
            self.add('reply', self.other, 'Boomerang', 'hand')
        elif mode in ('destroy_source', 'short_unattached_removal'):
            self.add('reply', self.other, 'Disenchant', 'hand')
        self.skip_next_untap = None

    def trigger_effect(self):
        self.skip_next_untap = self.equipped

    def real_untap(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield' or item['player'] != player:
                continue
            if ref == self.skip_next_untap:
                self.skip_next_untap = None
            else:
                item['tapped'] = False
        self.receipt()
        self.emit('upkeep_step', player=player)
        self.receipt()

    def tap_host(self):
        self.fund(self.other, (1, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('can_activate', 'tapper', value=1)
        self.emit('choose_target', 'host', player=self.other)
        self.emit('activate', 'tapper', value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.funded[self.other] = ZERO
        self.drain()
        self.cards['host']['tapped'] = True
        self.cards['tapper']['tapped'] = True
        self.card('tapper', 'tapped', 1)
        self.receipt()

    def source_leaves(self, destination):
        if self.equipped is not None:
            self.cards[self.equipped]['zone'] = 'graveyard'
        self.equipped = None
        self.cards['source']['zone'] = destination
        self.cast('reply', target='source')

    def attack_and_finish(self, ref):
        self.begin_attack(ref, check_trigger=True)
        self.finish_combat(ref)

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.cast('source', destination='battlefield')
        self.equip('host', affordable=self.mode != 'short_unattached_removal')
        if self.mode == 'freeze_once':
            self.attack_and_finish('host')
            for seat in (self.other, self.owner, self.other, self.owner):
                self.real_untap(seat)
        elif self.mode == 'tapped_other_attacks':
            self.tap_host()
            self.attack_and_finish('backup')
            for seat in (self.other, self.owner):
                self.real_untap(seat)
        elif self.mode == 'reequip_sacrifice':
            self.equip('backup')
            self.attack_and_finish('backup')
        elif self.mode == 'host_departure':
            self.equipped = None
            self.cards['host']['zone'] = 'hand'
            self.cast('reply', target='host')
            self.equip('backup')
            self.attack_and_finish('backup')
        elif self.mode == 'bounce_source_recast':
            self.source_leaves('hand')
            self.emit('bind_hand', 'source', player=self.owner, value=GRAFT)
            self.cast('source', destination='battlefield')
            self.equip('backup')
            self.attack_and_finish('backup')
        else:
            self.source_leaves('graveyard')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, equip_price, bonus, clauses, extra = MODELS[csvid]
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != '{' + str(cost) + '}'
            or record.get('type_line') != 'Artifact — Equipment' or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or record.get('keywords') != ['Equip']):
        raise ValueError('Exact whole normal Equipment and its printed costs required')
    return dict(csvid=csvid, cast_mana=cost_from_oracle(record), equip_generic=equip_price, bonus=list(bonus),
                clauses=list(clauses), modes=list(extra) if csvid == GRAFT else [*COMMON, *extra])


def variants(model):
    return [f'{mode}_{owner}' for mode in model['modes']
            for owner in ((0,) if mode == 'human_declines' else (0, 1))]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact Equipment source and witness scope required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            layout = 'token' if name in {token for token, *_ in TOKENS.values()} else 'normal'
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != layout
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or (row['handler'] == 'card_dummy') != (name == 'Kor Ally')):
                raise ValueError(f'Changed Equipment source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Equipment primary required')
    model = classify(record, pin['registry']['csvid'])
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(picked) != len(set(picked))
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Equipment clauses, paid costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Equipment primary and variants required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board = GraftBattle if csvid == GRAFT else EquipmentBattle
        game = board(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=GAPS.get(csvid, []))
        cases.append(dict(name=f'EquipmentAttackBoardRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
