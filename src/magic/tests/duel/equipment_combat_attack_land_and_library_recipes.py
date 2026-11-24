"""Equipment combat attack triggers with exact library and land receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'equipment_combat_attack_land_and_library'
PROFILE = Path(__file__).with_name('data') / 'equipment_combat_attack_land_and_library_wave8_profiles.json'
CARDS = {
    10538: (1, (0, 0), "Whenever equipped creature attacks, look at the top card of your library. If it's a land "
            'card, you may put it onto the battlefield tapped.\n'
            'Equip {1} ({1}: Attach to target creature you control. Equip only as a sorcery.)',
            ('take_land', 'nonland', 'empty'), 'decline_land'),
    12104: (2, (0, 0), 'Whenever equipped creature attacks, defending player reveals cards from the top of their '
            'library until they reveal a land card. The creature gets +1/+0 until end of turn for each card '
            'revealed this way. That player puts the revealed cards into their graveyard.\nEquip {2}',
            ('deep_land', 'first_land', 'no_land', 'empty'), None),
    15229: (2, (1, 1), 'Equipped creature gets +1/+1.\nWhenever equipped creature attacks, you may search your '
            'library for a basic land card, put it onto the battlefield tapped, then shuffle.\nEquip {2}',
            ('search_basic', 'fail_to_find'), 'decline_search'),
    15948: (1, (1, 0), 'Equipped creature gets +1/+0.\nWhenever equipped creature attacks, you may mill two cards.\n'
            'Equip {1} ({1}: Attach to target creature you control. Equip only as a sorcery.)',
            ('mill_two', 'mill_short', 'unequipped_attack'), 'decline_mill'),
    15444: (2, (0, 0), "Whenever equipped creature attacks, you may return a land you control to its owner's "
            'hand. If you do, the creature gets +2/+2 until end of turn.\nEquip {2}',
            ('return_own', 'return_borrowed', 'decline', 'only_enemy_land'), None),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0,) * 6
HOST = 'Grizzly Bears'
OTHER = 'Hill Giant'
WALL = 'Wall of Wood'
BOLT = 'Lightning Bolt'
NONBASIC = 'Terramorphic Expanse'
FETCH = 'Island'
WITNESSES = {HOST, OTHER, WALL, BOLT, NONBASIC, FETCH, 'Forest', 'Mountain', 'Plains', 'Swamp'}
GAPS = {
    10538: ['The native AI automatically takes an eligible land. The human menu supplies the voluntary decline control.'],
    15229: ['The native AI auto-enters the search branch. Its voluntary fail-to-find is tested separately from the human decline of the whole search.'],
    15948: ['The native AI automatically takes its mill trigger. The human trigger window supplies the voluntary decline control.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or not record.get('type_line', '').endswith('Artifact — Equipment')):
        raise ValueError('whole normal Equipment required')
    equip, bonus, text, _, _ = CARDS[csvid]
    keywords = {'Equip', 'Mill'} if csvid == 15948 else {'Equip'}
    if record.get('oracle_text') != text or set(record.get('keywords', ())) != keywords:
        raise ValueError('exact Equipment attack rules and costs required')
    return dict(csvid=csvid, spell_fee=cost_from_oracle(record), equip_fee=equip, static_bonus=list(bonus), rules=text)


def variants(shape):
    _, _, _, modes, human_decline = CARDS[shape['csvid']]
    return ([f'{mode}_{actor}' for mode in modes for actor in (0, 1)]
            + ([f'{human_decline}_0'] if human_decline else []))


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported attack-and-library scenarios required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned Equipment primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned complete attack-and-library declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('independent attack-and-library witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CARDS):
        raise ValueError('whole Equipment library cohort required')
    for group, items in pins.items():
        for name, pin in items.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('changed Equipment source or witness: ' + name)
            if group == 'sources':
                if not str(row.get('handler', '')).startswith('manalinkeh_card_'):
                    raise ValueError('implemented Equipment primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[NONBASIC]['type_line'] != 'Land' or not oracle[FETCH]['type_line'].startswith('Basic Land')
            or (oracle[WALL]['power'], oracle[WALL]['toughness']) != ('0', '3')):
        raise ValueError('distinct basic/nonbasic land and combat blocker required')


class Duel:
    def __init__(self, row, player, mode, rows, oracle):
        self.id, self.name, self.player, self.foe, self.mode = row['csvid'], row['name'], player, 1 - player, mode
        self.rows, self.oracle = rows, oracle
        self.cards, self.steps, self.setup = {}, [], []
        self.library = [[], []]
        self.life, self.shuffles = [20, 20], [0, 0]
        self.equipped = None
        self.temporary = 0
        self.bound_land = None
        self.add('source', self.name, player, 'hand')
        self.add('host', HOST, player, 'battlefield')
        self.add('other', OTHER, player, 'battlefield')
        if self.id == 12104 and mode == 'deep_land':
            self.add('blocker', WALL, self.foe, 'battlefield')
        self.populate()

    def add(self, ref, name, player, zone, *, owner=None):
        from recipes import card

        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=False)
        setup = card(name, ref, player, zone)
        if owner is not None:
            if owner != 1 - player or zone != 'battlefield':
                raise ValueError('borrowed-land state requires an opposing owner and actual control')
            self.cards[ref]['owner'] = owner
            setup['state'] = ['opponent_owned']
        self.setup.append(setup)

    def populate(self):
        if self.id == 15444:
            self.add('foe_land', 'Plains', self.foe, 'battlefield')
            if self.mode != 'only_enemy_land':
                self.add('own_land', 'Forest', self.player, 'battlefield')
            if self.mode == 'return_borrowed':
                self.add('borrowed', 'Island', self.player, 'battlefield', owner=self.foe)
            own, enemy = ('Mountain',), ('Swamp',)
        elif self.id == 10538:
            own = () if self.mode == 'empty' else (BOLT, FETCH) if self.mode == 'nonland' else (NONBASIC, BOLT, FETCH)
        elif self.id == 15229:
            own = (NONBASIC, BOLT, FETCH, 'Forest')
        elif self.id == 15948:
            own = (BOLT,) if self.mode == 'mill_short' else (BOLT, 'Forest', 'Swamp')
        else:
            own = ('Mountain', OTHER)
        if self.id == 12104:
            enemy = {'deep_land': (BOLT, HOST, 'Forest', 'Plains', OTHER), 'first_land': (NONBASIC, BOLT),
                     'no_land': (BOLT, HOST), 'empty': ()}[self.mode]
        elif self.id != 15444:
            enemy = ('Plains', HOST)
        for player, names in ((self.player, own), (self.foe, enemy)):
            for offset, name in enumerate(names):
                ref = f'library_{player}' if self.id == 15444 else f'library_{player}_{offset}'
                self.add(ref, name, player, 'library')
                self.library[player].append(ref)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('choose_trigger_source', 'choose_optional_target'):
            self.steps.append(step(op, ref=ref, player=player))
            return
        if op == 'assert_shuffle_count':
            self.steps.append(step(op, player=player, value=value))
            return
        if op == 'assert_forced_target_choices':
            self.steps.append(step(op, value=value))
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

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def csvid(self, ref):
        return self.rows[self.cards[ref]['name']]['csvid']

    def mana(self, op, player, amounts):
        for color, amount in enumerate(amounts):
            self.emit(op, player=player, color=color, value=amount)

    def pools_empty(self):
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)

    def fund(self, fee):
        self.mana('set_mana', self.player, fee)
        self.emit('refresh')

    def stats(self, ref):
        card = self.oracle[self.cards[ref]['name']]
        dp, dt = CARDS[self.id][1] if ref == self.equipped else (0, 0)
        bonus = self.temporary if ref == self.equipped else 0
        return int(card['power']) + dp + bonus, int(card['toughness']) + dt + (bonus if self.id == 15444 else 0)

    def receipt(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.pools_empty()
        for ref in ('host', 'other', 'blocker'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                self.prop(ref, 'in_play', 1)
                for prop, value in zip(('power', 'toughness'), self.stats(ref)):
                    self.prop(ref, prop, value)
                self.prop(ref, 'tapped', int(self.cards[ref]['tapped']))
        if self.bound_land is not None:
            self.prop('resolved_land', 'in_play', 1)
            self.prop('resolved_land', 'tapped', 1)
        known = sorted({self.csvid(ref) for ref in self.cards})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for number, zone in enumerate(ZONES):
                counts = Counter(self.csvid(ref) for ref, card in self.cards.items()
                                 if (card['player'], card['zone']) == (player, zone))
                self.emit('assert_zone', player=player, zone=number, value=sum(counts.values()))
                for csvid in known:
                    self.emit('assert_zone_multiplicity', player=player, zone=number, index=csvid, value=counts[csvid])
                if zone == 'library' and not self.shuffles[player]:
                    for offset, ref in enumerate(self.library[player]):
                        self.emit('assert_zone_card', player=player, zone=number, index=offset, value=self.csvid(ref))

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')

    def setup_equip(self):
        self.emit('turn', player=self.player, value=0)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.fund(cost_from_oracle(self.oracle[self.name]))
        self.emit('cast', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.pools_empty()
        self.resolve()
        self.cards['source']['zone'] = 'battlefield'
        self.prop('source', 'in_play', 1)
        self.receipt()
        # An empty floating pool does not rule out payment from untapped
        # controlled basics. In particular, both Slab Hammer lands can pay {2}.
        available_basic_mana = sum(
            item['player'] == self.player and item['zone'] == 'battlefield' and not item['tapped']
            and self.oracle[item['name']]['type_line'].startswith('Basic Land')
            for item in self.cards.values()
        )
        self.emit('can_activate', 'source', value=int(available_basic_mana >= CARDS[self.id][0]))
        self.fund((CARDS[self.id][0], 0, 0, 0, 0, 0))
        self.emit('can_activate', 'source', value=1)
        self.emit('choose_target', 'host', player=self.player)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_choices_consumed')
        self.pools_empty()
        self.resolve()
        self.equipped = 'host'
        self.receipt()

    def move_library_card(self, player, ref, destination):
        self.library[player].remove(ref)
        self.cards[ref]['zone'] = destination
        if destination == 'battlefield':
            self.cards[ref]['tapped'] = True
            self.bound_land = ref

    def pop(self, player, count):
        for ref in self.library[player][:count]:
            self.move_library_card(player, ref, 'graveyard')

    def reveal_count(self):
        defense = self.library[self.foe]
        for index, ref in enumerate(defense):
            if 'Land' in self.oracle[self.cards[ref]['name']]['type_line'].split(' — ', 1)[0]:
                return index + 1
        return len(defense)

    def input_for_attack(self):
        if self.id == 15444 and self.mode != 'only_enemy_land':
            chosen = 'borrowed' if self.mode == 'return_borrowed' else 'own_land'
            self.emit('choose_optional_target' if self.mode == 'decline' else 'choose_target', chosen, player=self.player)
        elif self.id == 10538 and self.player == 0 and self.mode != 'empty':
            # Land gets a Take/Pass menu. The nonland display is a single
            # acknowledgement and never changes the library.
            self.emit('choose_dialog', player=self.player, value=int(self.mode == 'decline_land'))
        elif self.id == 12104 and self.reveal_count():
            # Real show_deck in the headless build publishes its revealed list.
            self.emit('choose_zone', player=self.player, value=-1)
        elif self.id == 15229 and self.mode != 'decline_search':
            if self.player == 0:
                self.emit('choose_trigger_source', 'source', player=self.player)
            csvid = self.rows[FETCH]['csvid']
            self.emit('observe_search_filters', value=1)
            self.emit('choose_zone_card' if self.mode == 'search_basic' else 'choose_optional_zone',
                      player=self.player, value=csvid)
        elif self.id == 15948 and self.player == 0 and self.mode not in ('decline_mill', 'unequipped_attack'):
            self.emit('choose_trigger_source', 'source', player=self.player)

    def trigger_result(self):
        if self.id == 15444:
            if self.mode == 'decline':
                self.emit('assert_forced_target_choices', value=0)
            elif self.mode in ('return_own', 'return_borrowed'):
                ref = 'borrowed' if self.mode == 'return_borrowed' else 'own_land'
                card = self.cards[ref]
                card['player'] = card.get('owner', self.player)
                card['zone'] = 'hand'
                self.temporary = 2
        if self.id == 10538 and self.mode == 'take_land':
            ref = self.library[self.player][0]
            self.move_library_card(self.player, ref, 'battlefield')
        if self.id == 12104:
            count = self.reveal_count()
            self.emit('assert_offered_count', value=count)
            for index, ref in enumerate(self.library[self.foe][:count]):
                self.emit('assert_offered_card', index=index, value=self.csvid(ref))
            self.temporary = count
            self.pop(self.foe, count)
        if self.id == 15229:
            if self.mode == 'decline_search':
                self.emit('assert_offered_count', value=0)
            else:
                self.emit('assert_offered_count', value=len(self.library[self.player]))
                self.emit('assert_offered_selection_required', value=0)
                for ref in self.library[self.player]:
                    record = self.oracle[self.cards[ref]['name']]
                    self.emit('assert_offered_legal_card', index=self.csvid(ref), value=int(record['type_line'].startswith('Basic Land')))
                self.shuffles[self.player] += 1
                if self.mode == 'search_basic':
                    ref, = [ref for ref in self.library[self.player] if self.cards[ref]['name'] == FETCH]
                    self.move_library_card(self.player, ref, 'battlefield')
        if self.id == 15948 and self.mode in ('mill_two', 'mill_short'):
            self.pop(self.player, 2)
        if self.bound_land is not None:
            self.emit('bind_permanent', 'resolved_land', player=self.player, value=self.csvid(self.bound_land), index=-1)

    def combat(self):
        attacker = 'other' if self.mode == 'unequipped_attack' else 'host'
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.cards[attacker]['tapped'] = True
        self.input_for_attack()
        self.emit('finish_attackers')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.trigger_result()
        self.receipt()  # Effects must already exist before blockers or combat damage.
        self.emit('begin_blockers')
        blocked = 'blocker' in self.cards
        if blocked:
            self.emit('block', 'blocker', attacker, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        damage = self.stats(attacker)[0]
        if blocked:
            assert damage >= self.stats('blocker')[1] and self.stats('blocker')[0] == 0
            self.cards['blocker']['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= damage
        self.receipt()
        self.emit('cleanup')
        self.emit('refresh')
        self.temporary = 0
        self.receipt()

    def build(self):
        self.setup_equip()
        self.combat()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered Equipment primary CSVID required')
    output = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(rows[name], int(actor), mode, rows, oracle)
        game.build()
        output.append(dict(
            name=f'EquipmentCombatAttackLandAndLibraryRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=2660, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return output



