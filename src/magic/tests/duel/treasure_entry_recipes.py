"""Whole-Oracle Treasure entry and consumption through production actions."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from global_attack_tax_recipes import Board as Ledger


FAMILY = 'treasure_entry'
PROFILE = Path(__file__).parent / 'data/treasure_entry_wave8_profiles.json'
TREASURE = 'Treasure'
ABILITY = '{T}, Sacrifice this token: Add one mana of any color.'
ENTRIES = {
    f'When this creature enters, create a Treasure token. (It\'s an artifact with "{ABILITY}")': 1,
    f'When this creature enters, create two Treasure tokens. (They\'re artifacts with "{ABILITY}")': 2,
}
MANA_MODES = {'mana_black': 'B', 'mana_blue': 'U', 'mana_green': 'G', 'mana_red': 'R', 'mana_white': 'W'}
COMMON = (
    'entry', 'board_no_entry', 'unrelated_entry', 'wrong_color', 'short_cost', 'countered',
    'two_entries', 'both_players', 'bounce_recast', 'blink', 'source_removed', 'tap_untap',
    'spend_blue', 'wrong_player_cannot_spend', *MANA_MODES,
)
DOUBLE = ('double_colors', 'pay_red_pair')
WITNESSES = {
    TREASURE, 'Grizzly Bears', 'Darksteel Relic', 'Counterspell', 'Unsummon', 'Cloudshift',
    'Murder', 'Twiddle', 'Reach Through Mists', 'Forest', 'Island', 'Wily Goblin',
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify_entry(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record
            or not record['type_line'].startswith('Creature — ') or record['keywords'] != ['Treasure']
            or record['oracle_text'] not in ENTRIES or not re.fullmatch(
                r'(?:\{[1-9]\})?(?:\{[WUBRG]\}){1,2}', record['mana_cost'])
            or any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('complete fixed Treasure entry creature required')
    return dict(cost=cost_from_oracle(record), count=ENTRIES[record['oracle_text']],
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    if 'composition' in params:
        modes = LAND_MODES if params['composition'] == 'lands' else DEATH_MODES
        return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    modes = COMMON + (DOUBLE if params['count'] == 2 else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(pins()['sources'].get(record['name']), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)):
        raise ValueError('exact typed values and whole Oracle required for the Treasure entry')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Treasure entry variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(records) != set(data['registry']):
        raise ValueError('complete Treasure entry source and witness inventory required')
    for name, record in records.items():
        if (not exact(oracle.get(name), record) or not exact(rows.get(name), data['registry'][name])
                or record.get('layout') != ('token' if name in (TREASURE, 'Goblin') else 'normal')
                or rows[name]['kind'] != 'card'):
            raise ValueError(f'Oracle or registry identity changed for {name}')
        if name != 'Grizzly Bears' and rows[name]['handler'] in (None, 'card_dummy', 'NULL', '0'):
            raise ValueError(f'production callback missing for {name}')
    token = records[TREASURE]
    if (rows[TREASURE]['csvid'] != 18840 or token['type_line'] != 'Token Artifact — Treasure'
            or token['colors'] or token['oracle_text'] != ABILITY or 'power' in token or 'toughness' in token):
        raise ValueError('canonical colorless noncreature Treasure pin required')


class TreasureBoard(Ledger):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.foe = int(seat), 1 - int(seat)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life, self.loyalty = [[0] * 6, [0] * 6], [20, 20], None
        for player in (0, 1):
            self.add('Forest', f'library_{player}_0', player, 'library')
            self.add('Island', f'library_{player}_1', player, 'library')
            self.add('Island', f'grave_{player}', player, 'graveyard')
            self.add('Forest', f'exile_{player}', player, 'exile')
        on_board = self.mode == 'board_no_entry' or (params.get('composition') == 'death'
            and self.mode not in ('death_before_entry', 'death_then_spend'))
        self.add(name, 'source', self.owner, 'battlefield' if on_board else 'hand')
        self.setup_witnesses()
        if self.mode == 'board_no_entry':
            # Battlefield fixture placement uses real entry before the script.
            # Treat those Treasures as the starting board for the no-repeat check.
            self.zones['battlefield'][self.owner].extend([TREASURE] * params['count'])

    def setup_witnesses(self):
        if self.mode == 'unrelated_entry':
            self.add('Grizzly Bears', 'unrelated', self.owner, 'hand')
        if self.mode in ('two_entries', 'both_players'):
            self.add(self.name, 'second', self.foe if self.mode == 'both_players' else self.owner, 'hand')
        if self.mode == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        if self.mode in ('blink', 'bounce_recast', 'source_removed'):
            witness, player = {'blink': ('Cloudshift', self.owner), 'bounce_recast': ('Unsummon', self.foe),
                               'source_removed': ('Murder', self.foe)}[self.mode]
            self.add(witness, 'change', player, 'hand')
        if self.mode == 'tap_untap':
            self.add('Twiddle', 'tap_spell', self.foe, 'hand')
            self.add('Twiddle', 'untap_spell', self.owner, 'hand')
        if self.mode == 'spend_blue':
            self.add('Reach Through Mists', 'spending', self.owner, 'hand')
        if self.mode == 'wrong_player_cannot_spend':
            self.add('Reach Through Mists', 'spending', self.foe, 'hand')
        if self.mode == 'pay_red_pair':
            self.add('Wily Goblin', 'spending', self.owner, 'hand')
        if self.mode == 'entry':
            self.add('Darksteel Relic', 'non_treasure', self.owner)

    def treasure_count(self, player, zone='battlefield'):
        self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[TREASURE]['csvid'],
                  value=self.zones[zone][player].count(TREASURE))

    def receipt(self):
        super().receipt()
        for player in (0, 1):
            for zone in self.zones:
                self.treasure_count(player, zone)
            if TREASURE in self.zones['battlefield'][player]:
                for prop, value in (('token', 1), ('artifact', 1), ('color', 0), ('power', 0), ('toughness', 0)):
                    self.emit('assert_matching_permanents', player=player, index=self.rows[TREASURE]['csvid'],
                              property=prop, value=value)

    def bind_treasure(self):
        self.emit('bind_permanent_copy', ref='treasure', player=self.owner,
                  value=self.rows[TREASURE]['csvid'], index=0)

    def pending(self, ref, *, funding=True, target=None, denied=None, response=False):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['player']
        fee = cost_from_oracle(self.oracle[entity['name']])
        if funding:
            amounts = list(fee)
            if denied == 'wrong_color':
                amounts = [sum(fee), 0, 0, 0, 0, 0]
            if denied == 'short_cost':
                amounts[next(i for i in range(1, 6) if amounts[i])] -= 1
            self.fund(player, amounts)
        if target:
            self.emit('choose_target', ref=target, player=player)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(denied is None))
        self.observe(ref, 'in_hand', denied is not None)
        self.observe(ref, 'in_play', 0)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0 if denied else 2 if response else 1)
        if denied is None:
            self.mana[player] = [pool - cost for pool, cost in zip(self.mana[player], fee)]
            assert not any(self.mana[player])
        self.pools()
        for seat in (0, 1):
            self.treasure_count(seat)

    def create_treasures(self, ref):
        entity = self.entities[ref]
        count = classify(self.oracle[entity['name']])['count']
        self.add_treasures(entity['player'], count)

    def add_treasures(self, player, count):
        self.zones['battlefield'][player].extend([TREASURE] * count)
        if count:
            self.emit('assert_matching_permanents', player=player, index=self.rows[TREASURE]['csvid'],
                      property='tapped', value=0)

    def enter(self, ref, *, funding=True, treasure=True):
        self.pending(ref, funding=funding)
        self.emit('resolve')
        self.move(ref, 'battlefield')
        if treasure:
            self.create_treasures(ref)
        self.emit('refresh')
        self.receipt()

    def deny_entry(self):
        self.pending('source', denied=self.mode)
        self.receipt()

    def counter_entry(self):
        self.pending('source')
        self.emit('announce_spell', ref='source')
        self.pending('counter', response=True)
        self.emit('resolve_response')
        self.move('source', 'graveyard')
        self.move('counter', 'graveyard')
        self.receipt()

    def change_source(self):
        self.pending('change', target='source')
        self.emit('resolve')
        self.move('change', 'graveyard')
        if self.mode == 'bounce_recast':
            self.move('source', 'hand')
            self.emit('bind_hand_copy', ref='source', player=self.owner, value=self.csvid, index=0)
        elif self.mode == 'source_removed':
            self.move('source', 'graveyard')
        else:
            self.emit('bind_permanent_copy', ref='source', player=self.owner, value=self.csvid, index=0)
            self.create_treasures('source')
        self.emit('refresh')
        self.receipt()

    def twiddle(self, ref, *, tapped):
        self.pending(ref, target='treasure')
        self.emit('choose_dialog', player=self.entities[ref]['player'], value=int(not tapped))
        self.emit('resolve')
        self.move(ref, 'graveyard')
        self.emit('refresh')
        self.observe('treasure', 'tapped', tapped)
        self.receipt()

    def activate_treasure(self, color, *, bind=True, retry=True):
        from recipes import COLORS

        if bind:
            self.bind_treasure()
        self.observe('treasure', 'tapped', 0)
        self.emit('require_can_activate', ref='treasure', value=1)
        self.emit('choose_color', player=self.owner, value=COLORS[color])
        self.emit('activate', ref='treasure', value=1)
        self.zones['battlefield'][self.owner].remove(TREASURE)
        self.mana[self.owner][COLORS[color]] += 1
        self.observe('treasure', 'in_play', 0)
        self.emit('refresh')
        self.receipt()
        if retry:
            self.emit('can_activate', ref='treasure', value=0)
            self.emit('activate', ref='treasure', value=0)
            self.receipt()

    def draw_with_treasure(self):
        self.activate_treasure('U')
        self.pending('spending', funding=False)
        self.emit('resolve')
        self.move('spending', 'graveyard')
        self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))
        self.receipt()

    def episode(self):
        self.emit('turn', player=self.owner, value=0)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        if self.mode == 'board_no_entry':
            self.emit('advance_turn')
            self.receipt()
            return
        if self.mode == 'unrelated_entry':
            self.enter('unrelated', treasure=False)
            return
        if self.mode in ('wrong_color', 'short_cost'):
            self.deny_entry()
            return
        if self.mode == 'countered':
            self.counter_entry()
            return
        self.enter('source')
        if self.mode == 'entry':
            self.emit('can_activate', ref='non_treasure', value=0)
        elif self.mode in ('two_entries', 'both_players'):
            if self.mode == 'both_players':
                self.emit('advance_turn')
            self.enter('second')
        elif self.mode in ('blink', 'bounce_recast', 'source_removed'):
            self.change_source()
            if self.mode == 'bounce_recast':
                self.enter('source')
            if self.mode == 'source_removed':
                self.activate_treasure('G')
        elif self.mode == 'tap_untap':
            self.emit('present_ai_dialogs', value=1)
            self.bind_treasure()
            self.twiddle('tap_spell', tapped=True)
            self.emit('can_activate', ref='treasure', value=0)
            self.emit('activate', ref='treasure', value=0)
            self.receipt()
            self.twiddle('untap_spell', tapped=False)
            self.activate_treasure('W', bind=False)
        elif self.mode in MANA_MODES:
            self.activate_treasure(MANA_MODES[self.mode])
        elif self.mode == 'spend_blue':
            self.draw_with_treasure()
        elif self.mode == 'wrong_player_cannot_spend':
            self.pending('spending', funding=False, denied='unfunded')
            self.receipt()
        elif self.mode in DOUBLE:
            for color in ('R', 'U') if self.mode == 'double_colors' else ('R', 'R'):
                self.activate_treasure(color)
            if self.mode == 'pay_red_pair':
                self.enter('spending', funding=False)

    def case(self):
        from recipes import step

        self.emit('capture')
        self.episode()
        run = self.steps + [step('record_state'), step('restore')] + self.steps[1:] + [step('assert_recorded_state')]
        return dict(name=f'TreasureEntryRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=run, _recipe=dict(
                        family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                        oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Treasure subtype is pinned by exact token identity. The driver has no independent Treasure-subtype probe.',
                            'Mana is floated by the real Treasure ability before the spending spell. Automatic source selection during payment is separate.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    scene = {'lands': LandTreasureBoard, 'death': DeathTreasureBoard}.get(params.get('composition'), TreasureBoard)
    return [scene(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]

LAND_TEXT = f'For each land you control, create a Treasure token. (It\'s an artifact with "{ABILITY}")'
DEATH_TEXT = f'Whenever another creature you control dies, create a Treasure token. (It\'s an artifact with "{ABILITY}")'
LAND_MODES = (
    'zero', 'one', 'three', 'dual_land', 'tapped_land', 'opposing_land', 'nonland', 'played_land',
    'response_land_loss', 'countered', 'short_cost', 'spend_blue',
)
DEATH_MODES = (
    'own_death', 'foe_death', 'self_death', 'artifact_destruction', 'own_exiled', 'own_bounced',
    'two_deaths', 'simultaneous_source', 'own_dies_off_turn', 'borrowed_creature',
    'death_before_entry', 'death_after_source', 'token_destruction', 'death_then_spend',
)
COMPOSITION_WITNESSES = {
    'Control Magic', 'Dragon Fodder', 'Fissure', 'Goblin', 'Icy Manipulator', 'Naturalize',
    'Pyroclasm', 'Swords to Plowshares', 'Tropical Island', 'Wrath of God',
}


def classify_composition(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record
            or record['keywords'] != ['Treasure']):
        raise ValueError('normal whole Treasure source required')
    text, typ = record['oracle_text'], record['type_line']
    if text == LAND_TEXT and typ == 'Sorcery' and re.fullmatch(r'\{[1-9]\}\{R\}', record['mana_cost']):
        return dict(cost=cost_from_oracle(record), composition='lands')
    if (text == DEATH_TEXT and typ.startswith('Creature — ') and re.fullmatch(r'\{[1-9]\}\{B\}', record['mana_cost'])
            and all(re.fullmatch(r'[0-9]+', record.get(stat, '')) for stat in ('power', 'toughness'))):
        return dict(cost=cost_from_oracle(record), composition='death', power=int(record['power']),
                    toughness=int(record['toughness']))
    raise ValueError('complete land count or controlled creature death Treasure clause required')


class LandTreasureBoard(TreasureBoard):
    def setup_witnesses(self):
        for i, land in enumerate({
                'one': ('Forest',), 'three': ('Forest', 'Island', 'Forest'),
                'dual_land': ('Tropical Island',), 'tapped_land': ('Forest',),
                'response_land_loss': ('Tropical Island', 'Island'),
                'spend_blue': ('Forest',),
        }.get(self.mode, ())):
            self.add(land, f'land_{i}', self.owner, state='tapped' if self.mode == 'tapped_land' else 'untapped')
        if self.mode == 'opposing_land':
            self.add('Forest', 'foe_land', self.foe)
            self.add('Tropical Island', 'foe_dual', self.foe)
        if self.mode == 'nonland':
            self.add('Darksteel Relic', 'artifact', self.owner)
            self.add('Grizzly Bears', 'creature', self.owner)
        if self.mode == 'played_land':
            self.add('Forest', 'land_0', self.owner, 'hand')
        if self.mode == 'response_land_loss':
            self.add('Fissure', 'response_spell', self.foe, 'hand')
        if self.mode == 'countered':
            self.add('Counterspell', 'counter', self.foe, 'hand')
        if self.mode == 'spend_blue':
            self.add('Reach Through Mists', 'spending', self.owner, 'hand')

    def episode(self):
        self.emit('turn', player=self.owner, value=0)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        if self.mode == 'countered':
            self.counter_entry()
            return
        if self.mode == 'short_cost':
            self.deny_entry()
            return
        if self.mode == 'played_land':
            self.emit('play_land', ref='land_0', value=1)
            self.move('land_0', 'battlefield')
            self.receipt()
        self.pending('source')
        if self.mode == 'response_land_loss':
            self.emit('announce_spell', ref='source')
            self.pending('response_spell', target='land_0', response=True)
            self.emit('resolve_response')
            self.move('land_0', 'graveyard')
            self.move('response_spell', 'graveyard')
        else:
            self.emit('resolve')
        self.move('source', 'graveyard')
        count = sum('Land' in self.oracle[name]['type_line'].partition(' — ')[0].split()
                    for name in self.zones['battlefield'][self.owner])
        self.add_treasures(self.owner, count)
        self.receipt()
        if self.mode == 'spend_blue':
            self.draw_with_treasure()


class DeathTreasureBoard(TreasureBoard):
    def setup_witnesses(self):
        self.death_target = None
        mode = self.mode
        if mode not in ('self_death', 'token_destruction'):
            player = self.foe if mode in ('foe_death', 'borrowed_creature') else self.owner
            self.add('Icy Manipulator' if mode == 'artifact_destruction' else 'Grizzly Bears', 'victim', player)
            self.death_target = 'victim'
        if mode == 'self_death':
            self.death_target = 'source'
        if mode in ('two_deaths', 'simultaneous_source'):
            self.add('Grizzly Bears', 'second_victim', self.owner)
            self.add('Grizzly Bears', 'foe_victim', self.foe)
        spells = {
            'artifact_destruction': 'Naturalize', 'own_exiled': 'Swords to Plowshares',
            'own_bounced': 'Unsummon', 'two_deaths': 'Pyroclasm', 'simultaneous_source': 'Wrath of God',
        }
        actor = self.owner if mode in ('two_deaths', 'simultaneous_source', 'token_destruction') else self.foe
        self.add(spells.get(mode, 'Murder'), 'kill', actor, 'hand')
        if mode == 'death_after_source':
            self.add('Murder', 'initial_removal', self.foe, 'hand')
        if mode == 'borrowed_creature':
            self.add('Control Magic', 'control', self.owner, 'hand')
        if mode == 'token_destruction':
            self.add('Dragon Fodder', 'token_spell', self.owner, 'hand')
        if mode == 'death_then_spend':
            self.add('Reach Through Mists', 'spending', self.owner, 'hand')

    def spell(self, ref, target=None):
        self.pending(ref, target=target)
        self.emit('resolve')
        self.move(ref, 'graveyard')
        self.emit('refresh')

    def take_control(self):
        self.pending('control', target='victim')
        self.emit('resolve')
        self.move('control', 'battlefield')
        self.zones['battlefield'][self.foe].remove('Grizzly Bears')
        self.zones['battlefield'][self.owner].append('Grizzly Bears')
        self.entities['victim']['player'] = self.owner
        self.emit('bind_permanent_copy', ref='victim', player=self.owner, value=self.rows['Grizzly Bears']['csvid'], index=0)
        self.receipt()

    def make_token_victim(self):
        self.spell('token_spell')
        self.zones['battlefield'][self.owner].extend(['Goblin', 'Goblin'])
        goblin = self.rows['Goblin']['csvid']
        self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=goblin, value=2)
        for prop, value in (('token', 1), ('color', 16), ('subtype_Goblin', 1), ('power', 1), ('toughness', 1)):
            self.emit('assert_matching_permanents', player=self.owner, index=goblin, property=prop, value=value)
        self.emit('bind_permanent_copy', ref='victim', player=self.owner, value=goblin, index=0)
        self.death_target = 'victim'

    def death_resolution(self):
        mode = self.mode
        if mode in ('two_deaths', 'simultaneous_source'):
            self.spell('kill')
            for ref in ('victim', 'second_victim', 'foe_victim'):
                self.move(ref, 'graveyard')
            if mode == 'simultaneous_source':
                self.move('source', 'graveyard')
            else:
                self.entities['source']['damage'] = 2
            return 2

        if mode == 'token_destruction':
            self.make_token_victim()
        target = self.death_target
        self.spell('kill', target)
        if mode == 'own_exiled':
            self.move(target, 'exile')
            self.life[self.owner] += self.entities[target]['stats'][0]
            return 0
        if mode == 'own_bounced':
            self.move(target, 'hand')
            return 0
        if mode == 'borrowed_creature':
            self.zones['battlefield'][self.owner].remove('Grizzly Bears')
            self.zones['graveyard'][self.foe].append('Grizzly Bears')
            self.entities[target]['zone'] = 'graveyard'
            self.move('control', 'graveyard')
            return 1
        if mode == 'token_destruction':
            self.zones['battlefield'][self.owner].remove('Goblin')
            for zone, count in (('battlefield', 1), ('graveyard', 0)):
                self.emit('assert_zone_multiplicity', player=self.owner, zone=zone,
                          index=self.rows['Goblin']['csvid'], value=count)
            return 1

        victim = self.entities[target]
        triggers = (self.entities['source']['zone'] == 'battlefield' and target != 'source'
                    and victim['player'] == self.owner and victim['stats'] is not None)
        self.move(target, 'graveyard')
        return int(triggers)

    def episode(self):
        self.emit('turn', player=self.owner, value=0)
        self.fund(self.owner, [0] * 6)
        self.receipt()
        if self.mode == 'death_then_spend':
            self.enter('source', treasure=False)
        if self.mode == 'own_dies_off_turn':
            self.emit('advance_turn')
        if self.mode == 'borrowed_creature':
            self.take_control()
        if self.mode == 'death_after_source':
            self.spell('initial_removal', 'source')
            self.move('source', 'graveyard')
            self.receipt()
        count = self.death_resolution()
        self.add_treasures(self.owner, count)
        self.receipt()
        if self.mode == 'death_before_entry':
            self.enter('source', treasure=False)
        elif self.mode == 'death_then_spend':
            self.draw_with_treasure()



WITNESSES |= COMPOSITION_WITNESSES


def classify(record):
    try:
        return classify_entry(record)
    except ValueError:
        return classify_composition(record)
