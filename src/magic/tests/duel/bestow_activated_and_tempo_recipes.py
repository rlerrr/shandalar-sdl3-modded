"""Paid Bestow with form-dependent activated, upkeep, flash and blocker riders."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from bestow_stat_aura_recipes import REMINDER, Scene
from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle


FAMILY = 'bestow_activated_and_tempo'
PROFILE = Path(__file__).with_name('data') / 'bestow_activated_and_tempo_wave8_profiles.json'
BOON, EREBOS, EVERFLAME, HERALD, SPIRE = 13392, 13432, 13698, 13730, 13981
MODES = {
    BOON: ('flash_creature', 'flash_bestow', 'flash_then_detach', 'flash_target_lost'),
    EREBOS: ('creature_ability', 'aura_own_ability', 'aura_foreign_ability', 'detach_ability'),
    EVERFLAME: ('creature_ability', 'aura_own_ability', 'aura_foreign_ability', 'detach_ability'),
    HERALD: ('creature', 'aura_own', 'aura_foreign', 'detach'),
    SPIRE: ('creature', 'aura_own', 'aura_foreign', 'detach', 'flying_attacker'),
}
PARAMETERS = {
    BOON: ('{3}{G}{G}', (4, 2), ('Flash',)),
    EREBOS: ('{5}{B}', (3, 3), (
        'Discard a creature card: This creature gets +2/+2 until end of turn. If this permanent is an Aura, '
        'enchanted creature gets +2/+2 until end of turn instead.',)),
    EVERFLAME: ('{2}{R}', (1, 1), (
        "{R}: This creature gets +1/+0 until end of turn. If it's an Aura, enchanted creature gets "
        '+1/+0 until end of turn instead.',)),
    HERALD: ('{3}{B}{B}', (3, 3), ('Flying', 'At the beginning of your upkeep, you lose 1 life.')),
    SPIRE: ('{4}{G}', (4, 1), ('This creature blocks each combat if able.',)),
}
WITNESSES = ('Forest', 'Giant Growth', 'Grizzly Bears', 'Hill Giant', 'Murder', 'Shock', 'Unsummon', 'Wind Drake')

GAPS = {
    BOON: ('Flash is exercised through explicit native spell responses; menu discoverability is outside these cases.',),
    SPIRE: ('A real mandatory blocker selection is exercised; no explicit UI Done/retry diagnostic.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def shape(record, csvid):
    values = model(record, csvid)
    return dict(csvid=csvid, normal_cost=record['mana_cost'], bestow_cost=PARAMETERS[csvid][0],
                bonus=list(values['bonus']), creature=values['creature'])


def variants(params):
    return [f'{mode}_{player}' for player in (0, 1) for mode in MODES[params['csvid']]]


def validate_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(params))):
        raise ValueError('distinct supported Bestow rider variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned whole Bestow primary required')
    params = shape(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), params)):
        raise ValueError('exact typed Bestow rider declaration required')
    validate_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != set(WITNESSES)
            or {value['registry']['csvid'] for value in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact Bestow rider sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('pinned Bestow Oracle or registry drift: ' + name)
            if group == 'sources':
                aura = pin.get('engine_aura_face', {})
                if (row['handler'] == 'card_dummy' or aura.get('csvid') != row['csvid'] + 1
                        or aura.get('handler') != row['handler'] or not exact(rows.get(aura.get('name')), aura)):
                    raise ValueError('exact implemented native creature/Aura forms required: ' + name)
                shape(oracle[name], row['csvid'])


def model(record, csvid):
    price, buff, riders = PARAMETERS[csvid]
    grant = f'Enchanted creature gets +{buff[0]}/+{buff[1]}'
    if csvid == HERALD:
        grant += ' and has flying.'
    elif csvid == SPIRE:
        grant += ' and blocks each combat if able.'
    else:
        grant += '.'
    bestow = ('Bestow ' + price + REMINDER,)
    following = (*riders, *bestow) if csvid == BOON else (*bestow, *riders)
    if (record.get('oracle_text') != '\n'.join((*following, grant)) or record.get('layout') != 'normal'
            or not record.get('type_line', '').startswith('Enchantment Creature — ')
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('exact normal whole-Oracle Bestow rider required')
    return dict(normal=cost_from_oracle(record), bestowed=cost_from_oracle({'mana_cost': price}),
                bonus=buff, creature=[int(record[name]) for name in ('power', 'toughness')])


class TempoScene(Scene):
    def __init__(self, name, owner, mode, registry, oracle, aura):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1 - owner, mode
        self.id, self.aura_id = registry[name]['csvid'], aura['csvid']
        self.model, self.registry, self.oracle = model(oracle[name], self.id), registry, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.mana, self.life = [[0] * 6 for _ in (0, 1)], [20, 20]
        self.form, self.turn = None, owner
        self.boosts = Counter()

    def setup_cards(self):
        self.add('source', self.name, self.owner)
        self.add('probe', 'Murder', self.enemy)
        host = self.enemy if 'foreign' in self.mode else self.owner
        self.add('host', 'Grizzly Bears', host, 'battlefield')
        self.add('other', 'Hill Giant', 1 - host, 'battlefield')
        if self.id == BOON:
            self.add('parent', 'Unsummon' if self.mode == 'flash_then_detach' else
                     'Giant Growth' if self.mode == 'flash_target_lost' else 'Shock', self.enemy)
            if self.mode == 'flash_target_lost':
                self.add('answer', 'Unsummon', self.enemy)
        if self.id in (EREBOS, EVERFLAME, HERALD, SPIRE) and self.mode.startswith('detach'):
            self.add('helper', 'Unsummon', self.enemy)
        if self.id == EREBOS:
            self.add('reject', 'Forest', self.owner)
            self.add('first', 'Grizzly Bears', self.owner)
            self.add('second', 'Grizzly Bears', self.owner)
        if self.id == HERALD and self.mode == 'creature':
            self.add('flyer', 'Wind Drake', self.enemy, 'battlefield')
        if self.id == SPIRE:
            attacker = self.owner if self.mode == 'aura_foreign' else self.enemy
            self.add('raider', 'Wind Drake' if self.mode == 'flying_attacker' else 'Grizzly Bears',
                     attacker, 'battlefield')

    def host_stats(self):
        for ref in ('host', 'other'):
            item = self.objects.get(ref)
            if not item or item['zone'] != 'battlefield':
                continue
            printed = self.oracle[item['name']]
            static = self.model['bonus'] if ref == 'host' and self.form == 'aura' else (0, 0)
            temporary = self.single_boost(ref)
            for index, key in enumerate(('power', 'toughness')):
                self.prop(ref, key, int(printed[key]) + static[index] + temporary[index])

    def single_boost(self, ref):
        times = self.boosts[ref]
        if self.id == EREBOS:
            return (2 * times, 2 * times)
        if self.id == EVERFLAME:
            return (times, 0)
        return (0, 0)

    def source_state(self):
        self.prop('source', 'in_play', 1, required=True)
        aura = self.form == 'aura'
        self.prop('source', 'csvid', self.aura_id if aura else self.id)
        self.emit('can_target_creature', ref='probe', other='source', value=int(not aura))
        if not aura:
            for prop, value, extra in zip(('power', 'toughness'), self.model['creature'],
                                          self.single_boost('source'), strict=True):
                self.prop('source', prop, value + extra)
        self.host_stats()

    def begin(self, player):
        self.turn = player
        self.emit('turn', player=player, value=0)

    def paid_turn(self, player):
        assert player == 1 - self.turn and not any(map(any, self.mana))
        self.boosts.clear()
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('upkeep_step', player=player)
        if self.id == HERALD and player == self.owner:
            self.life[self.owner] -= 1
        self.begin(player)
        self.source_state()
        for seat, life in enumerate(self.life):
            self.emit('assert_life', player=seat, value=life)

    def choices(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def pending_witness(self, ref, *, player_target=None, target=None, root=False, depth=1):
        item = self.objects[ref]
        whom = item['player']
        fee = cost_from_oracle(self.oracle[item['name']])
        self.fund(whom, fee)
        if player_target is not None:
            self.emit('choose_player', player=whom, value=player_target)
        if target:
            self.emit('choose_target', player=whom, ref=target)
        self.emit('cast' if root else 'cast_response', ref=ref, value=1)
        self.emit('assert_stack', value=depth)
        self.choices()
        self.spend(whom, fee)
        if root:
            self.emit('announce_spell', ref=ref)

    def flash(self):
        self.begin(self.enemy)
        if self.mode == 'flash_creature':
            self.pending_witness('parent', player_target=self.owner, root=True)
        else:
            self.pending_witness('parent', target='host', root=True)
        aura = self.mode != 'flash_creature'
        fee = self.model['bestowed' if aura else 'normal']
        self.fund(self.owner, fee)
        if aura:
            self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('choose_target', player=self.owner, ref='host')
        self.emit('cast_response', ref='source', value=1)
        self.emit('assert_stack', value=2)
        self.prop('source', 'csvid', self.aura_id if aura else self.id)
        self.choices()
        self.spend(self.owner, fee)
        if self.mode == 'flash_target_lost':
            self.emit('announce_spell', ref='source')
            self.pending_witness('answer', target='host', depth=3)
            self.resolve(response=True)
            self.objects['answer']['zone'] = 'graveyard'
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
        else:
            self.resolve(response=True)
            if self.mode == 'flash_then_detach':
                self.objects['host']['zone'] = 'hand'
                self.entered('creature')
            elif aura:
                self.entered('aura')
                self.prop('host', 'damage', 2)
            else:
                self.life[self.owner] -= 2
                self.entered('creature')
        self.objects['parent']['zone'] = 'graveyard'

    def eliminate_host(self):
        self.cast_helper('helper', 'host')
        self.objects['host']['zone'] = 'hand'
        self.entered('creature')

    def pump(self, receiver, discard=None):
        if self.id == EVERFLAME:
            paid = (0, 0, 0, 0, 1, 0)
            self.fund(self.owner, paid)
        else:
            paid = (0,) * 6
        self.emit('can_activate', ref='source', value=1)
        if discard:
            self.emit('choose_zone_card', player=self.owner, value=self.registry['Grizzly Bears']['csvid'])
        self.emit('activate', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.choices()
        self.spend(self.owner, paid)
        if discard:
            self.objects[discard]['zone'] = 'graveyard'
            self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard',
                      index=self.registry['Grizzly Bears']['csvid'], value=1 + int(discard == 'second'))
        self.source_state()
        self.resolve()
        self.boosts[receiver] += 1
        self.source_state()

    def activated(self):
        aura = self.mode != 'creature_ability'
        self.cast_source(bestow=aura, choice=0 if aura else None)
        self.resolve()
        self.entered('aura' if aura else 'creature')
        first, second = ('first', 'second') if self.id == EREBOS else (None, None)
        receiver = 'host' if aura else 'source'
        self.pump(receiver, first)
        if self.mode == 'detach_ability':
            self.eliminate_host()
            receiver = 'source'
        self.pump(receiver, second)
        bounced_host_is_discardable = self.id == EREBOS and self.mode == 'detach_ability'
        self.emit('can_activate', ref='source', value=int(bounced_host_is_discardable))
        self.receipts()
        self.paid_turn(self.enemy)
        self.paid_turn(self.owner)
        self.source_state()

    def flying_combat(self, attacker, *, flyer=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='other', other=attacker, value=0)
        if flyer:
            self.emit('block', ref='flyer', other=attacker, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if flyer:
            self.objects['flyer']['zone'] = 'graveyard'
            self.prop(attacker, 'damage', 2)
        else:
            self.life[1 - self.turn] -= 5 if attacker == 'host' else 3
        self.objects[attacker]['tapped'] = True

    def herald(self):
        aura = self.mode != 'creature'
        self.cast_source(bestow=aura, choice=0 if aura else None)
        self.resolve()
        self.entered('aura' if aura else 'creature')
        self.paid_turn(self.enemy)
        self.paid_turn(self.owner)
        if self.mode == 'detach':
            self.eliminate_host()
            self.paid_turn(self.enemy)
            self.paid_turn(self.owner)
        if self.mode == 'aura_foreign':
            self.paid_turn(self.enemy)
        self.flying_combat('host' if self.form == 'aura' else 'source', flyer=self.mode == 'creature')

    def spirespine(self):
        aura = self.mode in ('aura_own', 'aura_foreign', 'detach')
        self.cast_source(bestow=aura, choice=0 if aura else None)
        self.resolve()
        self.entered('aura' if aura else 'creature')
        if self.mode == 'detach':
            self.eliminate_host()
        attacker = self.objects['raider']['player']
        self.paid_turn(self.enemy)
        if attacker == self.owner:
            self.paid_turn(self.owner)
        defender = 'host' if self.form == 'aura' else 'source'
        assert self.objects[defender]['player'] == 1 - attacker
        able = self.mode != 'flying_attacker'
        self.emit('begin_attackers')
        self.emit('attack', ref='raider', value=1)
        self.emit('finish_attackers')
        if able:
            self.emit('choose_target', ref='raider', player=1 - attacker)
        self.emit('begin_blockers')
        if not able:
            self.emit('block', ref=defender, other='raider', value=0)
        self.emit('advance_combat')
        self.choices()
        self.prop(defender, 'blocking_count', int(able))
        self.prop('raider', 'blocked', int(able))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if able:
            self.objects['raider']['zone'] = 'graveyard'
            if defender == 'source':
                self.objects['source']['zone'] = 'graveyard'
                self.form = None
            else:
                self.prop('host', 'damage', 2)
                self.source_state()
        else:
            self.life[1 - attacker] -= 2
            self.source_state()

    def receipts(self):
        # Zone totals and multiplicities do not treat a retired hand slot as a
        # live object after discard or when an Aura changes native identity.
        self.emit('assert_stack', value=0)
        self.choices()
        self.pools()
        self.host_stats()
        for who, life in enumerate(self.life):
            self.emit('assert_life', player=who, value=life)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                counts = Counter(self.aura_id if ref == 'source' and self.form == 'aura'
                                 else self.registry[item['name']]['csvid']
                                 for ref, item in self.objects.items()
                                 if item['player'] == who and item['zone'] == zone)
                self.emit('assert_zone', player=who, zone=zone, value=sum(counts.values()))
                for csvid, value in counts.items():
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=value)

    def build(self):
        self.setup_cards()
        self.begin(self.owner)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=who, color=color, value=0)
        self.emit('refresh')
        if self.id == BOON:
            self.flash()
        elif self.id == HERALD:
            self.herald()
        elif self.id == SPIRE:
            self.spirespine()
        else:
            self.activated()
        self.receipts()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_variants(params, selected)
    validate_witnesses(oracle, rows)
    pinned = profiles()['sources'][name]
    if params['csvid'] != csvid or pinned['registry']['csvid'] != csvid:
        raise ValueError('exact Bestow rider primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = TempoScene(name, int(player), mode, rows, oracle, pinned['engine_aura_face'])
        game.build()
        cases.append(dict(name=f'BestowActivatedTempoRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2609, setup=game.setup, steps=game.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GAPS.get(csvid, ())))))
    return cases
