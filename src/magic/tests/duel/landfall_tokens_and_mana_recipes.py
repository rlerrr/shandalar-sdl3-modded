"""Landfall resources with real land plays, token costs, and lifecycles."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from landfall_case_support import LandfallBoard, landfall_lines
from library_search_recipes import wave6_exact as exact


FAMILY = 'landfall_tokens_and_mana'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
LANDFALL = 'Landfall — Whenever a land you control enters, '
RULES = {
    (LANDFALL + 'create a 2/2 green Elemental creature token.',): 'roil',
    (LANDFALL + 'add one mana of any color.',): 'cobra',
    (LANDFALL + 'create a 5/5 red and green Elemental creature token.',
     'Whenever Omnath or another Elemental you control dies, Omnath deals 3 damage to any target.'): 'omnath',
    (LANDFALL + 'investigate. (Create a Clue token. It\'s an artifact with "{2}, Sacrifice this token: Draw a card.")',
     'Whenever you sacrifice a Clue, put a +1/+1 counter on this creature.'): 'tracker',
    (LANDFALL + 'you get {E} (an energy counter).',
     'Pay eight {E}: Create a 6/6 colorless Beast artifact creature token.'): 'architect',
    ('Flying', LANDFALL + 'create a 0/2 red Dragon Egg creature token with defender and '
     '"When this token dies, create a 2/2 red Dragon creature token with flying and \'{R}: This token gets +1/+0 until end of turn.\'"'): 'dragon',
    (LANDFALL + 'you may pay {2}{R}. If you do, create a 3/1 red Elemental creature token with trample and haste. '
     'Exile that token at the beginning of the next end step.',): 'stonewaker',
}
RIDER_MODES = {
    'roil': ('replay',),
    'cobra': ('spend_red', 'blue', 'mandatory', 'replay'),
    'omnath': ('token_dies', 'source_dies', 'own_non_elemental', 'foreign_elemental'),
    'tracker': ('spend_clue', 'short_clue', 'replay'),
    'architect': ('energy_eight', 'energy_short', 'replay'),
    'dragon': ('egg_defender', 'dragon_hatches', 'source_flying'),
    'stonewaker': ('unfunded', 'decline', 'attack', 'expires'),
}
# The creator's complete Oracle clause supplies the traits. These names bind physical engine token identities.
TOKENS = {
    'roil': ('Elemental', 'Elemental', 2, 2, 8, 0),
    'omnath': ('Elemental', 'Elemental', 5, 5, 24, 0),
    'tracker': ('Clue', 'Clue', None, None, 0, 1),
    'beast': ('Beast', 'Beast', 6, 6, 0, 1),
    'dragon': ('Dragon Egg', 'Dragon', 0, 2, 16, 0),
    'hatch': ('Dragon', 'Dragon', 2, 2, 16, 0),
    'stonewaker': ('Elemental', 'Elemental', 3, 1, 16, 0),
}
LANDS = ('Swamp', 'Forest', 'Island', 'Evolving Wilds')
WITNESSES = {*LANDS, *(token[0] for token in TOKENS.values()), 'Lightning Bolt', 'Shock', 'Murder',
             'Coal Stoker', 'Goblin Tinkerer'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = landfall_lines(record)
    effect = RULES.get(tuple(lines))
    expected = {'Landfall'}
    if effect == 'dragon':
        expected.add('Flying')
    if effect == 'tracker':
        expected.add('Investigate')
    if not effect or set(record['keywords']) != expected:
        raise ValueError('Exact whole Landfall origin and printed rider required')
    return dict(effect=effect, price=cost_from_oracle(record), clauses=lines)


def variants(model):
    modes = ('own', 'opponent', 'repeat', 'late_source', *RIDER_MODES[model['effect']])
    return [f'{mode}_{seat}' for mode in modes for seat in (0, 1)
            if mode not in ('decline', 'mandatory') or seat == 0]


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Landfall card required')
    model = classify(record)
    picked = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(picked, list) or not picked
            or any(type(v) is not str for v in picked) or len(set(picked)) != len(picked)
            or set(picked) - set(variants(model))):
        raise ValueError('Exact Landfall origin and distinct scoped variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Complete independent Landfall witness pins required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            token_names = {'Elemental', 'Beast', 'Clue', 'Dragon'}
            if (not exact(pin, {'oracle': record, 'registry': row}) or row.get('kind') != 'card'
                    or record.get('layout') != ('token' if name in token_names else 'normal')
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy')):
                raise ValueError(f'Pinned implemented Landfall source or witness changed: {name}')


class TokenLandfallBoard(LandfallBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.energy = 0
        self.counters = 0
        self.token_refs = {}
        self.snapshot = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    @property
    def effect(self):
        return self.shape['effect']

    def build(self):
        mode, effect = self.mode, self.effect
        land = 'Evolving Wilds' if mode in ('spend_clue', 'short_clue') else 'Swamp'
        self.add(land, 'land', self.foe if mode == 'opponent' else self.owner, 'hand')
        if mode == 'repeat':
            self.add('Forest', 'again', self.owner, 'hand')
        if mode in ('energy_eight', 'energy_short'):
            for index in range(1, 8 if mode == 'energy_eight' else 7):
                self.add(LANDS[index % len(LANDS)], f'land_{index}', self.owner, 'hand')
        if effect == 'cobra' and mode in ('repeat', 'spend_red'):
            self.add('Lightning Bolt', 'spend', self.owner, 'hand')
        if mode in ('spend_clue', 'short_clue'):
            self.add('Island', 'drawn', self.owner, 'library')
        if mode in ('token_dies', 'source_dies', 'own_non_elemental', 'foreign_elemental'):
            self.add('Murder', 'removal', self.foe, 'hand')
        if mode == 'own_non_elemental':
            self.add('Goblin Tinkerer', 'victim', self.owner)
        if mode == 'foreign_elemental':
            self.add('Coal Stoker', 'victim', self.foe)
        if mode == 'source_dies':
            self.add('Goblin Tinkerer', 'victim', self.foe)
        if mode == 'dragon_hatches':
            self.add('Shock', 'removal', self.foe, 'hand')
        if mode in ('dragon_hatches', 'source_flying', 'attack'):
            self.add('Goblin Tinkerer', 'blocker', self.foe)

    def add_token(self, role):
        name, *_ = TOKENS[role]
        ref = f'token_{len(self.token_refs)}'
        index = sum(item['name'] == name and item['player'] == self.owner and item['zone'] == 'battlefield'
                    for item in self.cards.values())
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=index, value=self.rows[name]['csvid'])
        self.cards[ref] = dict(name=name, player=self.owner, zone='battlefield', tapped=False, damage=0, boost=0)
        self.token_refs[ref] = role
        return ref

    def stats(self, ref):
        item = self.cards[ref]
        if ref in self.token_refs:
            _, _, power, toughness, _, _ = TOKENS[self.token_refs[ref]]
            return (power + item['boost'], toughness) if power is not None else None
        row = self.oracle[item['name']]
        if 'power' in row:
            plus = self.counters if ref == 'source' else 0
            return int(row['power']) + plus, int(row['toughness']) + plus
        return None

    def checks(self):
        quantities = Counter((c['player'], c['zone'], self.rows[c['name']]['csvid']) for c in self.cards.values()
                              if c['zone'] != 'gone')
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            self.emit('assert_energy', player=who, value=self.energy if who == self.owner else 0)
            for zone in ('battlefield', 'hand', 'library', 'graveyard', 'exile'):
                total = sum(n for (seat, place, _), n in quantities.items() if seat == who and place == zone)
                self.emit('assert_zone', player=who, zone=zone, value=total)
        for (who, zone, csvid), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=who, zone=zone, index=csvid, value=value)
        for ref, item in self.cards.items():
            if item['zone'] != 'battlefield':
                continue
            stats = self.stats(ref)
            if stats:
                for property_name, value in zip(('power', 'toughness'), stats):
                    self.emit('assert_card', ref=ref, property=property_name, value=value)
                self.emit('assert_card', ref=ref, property='damage', value=item['damage'])
            self.emit('assert_card', ref=ref, property='tapped', value=int(item['tapped']))
        for role in dict.fromkeys((self.effect, *self.token_refs.values())):
            if role not in TOKENS:
                continue
            name, subtype, _, _, color, artifact = TOKENS[role]
            live = [ref for ref, token_role in self.token_refs.items()
                    if token_role == role and self.cards[ref]['zone'] == 'battlefield']
            csvid = self.rows[name]['csvid']
            self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=csvid, value=len(live))
            if live:
                for attribute, value in (('token', 1), ('subtype_' + subtype, 1), ('color', color), ('artifact', artifact)):
                    self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=attribute, value=value)
        if self.effect == 'tracker':
            self.emit('assert_card', ref='source', property='plus1_counters', value=self.counters)
        self.observe_mana()
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def reward_choices(self, board):
        if self.effect == 'cobra':
            if self.owner == 0 and self.mode != 'mandatory':
                board.emit('choose_trigger_source', player=self.owner, ref='source')
            board.emit('choose_color', player=self.owner, value=2 if self.mode == 'blue' else 4)
        elif self.effect == 'stonewaker' and self.owner == 0 and self.mode not in ('unfunded', 'decline'):
            board.emit('choose_trigger_source', player=self.owner, ref='source')

    def own_land(self, ref='land'):
        mode, effect = self.mode, self.effect
        if effect == 'stonewaker' and mode != 'unfunded':
            self.fund(self.owner, [2, 0, 0, 0, 1, 0])
        if mode == 'mandatory':
            # A correct Oracle-mandatory trigger offers a color even after ordinary priority passes.
            self.reward_choices(self)
            self.emit('play_land', ref=ref, value=1)
            self.cards[ref]['zone'] = 'battlefield'
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_unused_choices', value=0)
        else:
            self.play_land(ref, choices=self.reward_choices)
        if effect == 'cobra':
            self.pools[self.owner][2 if mode == 'blue' else 4] += 1
        elif effect == 'architect':
            self.energy += 1
        elif effect in TOKENS and mode not in ('unfunded', 'decline'):
            if effect == 'stonewaker':
                self.pools[self.owner] = [0] * 6
            self.add_token(effect)
        self.checks()

    def spend_red(self):
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('cast', ref='spend', value=1)
        self.emit('assert_stack', value=1)
        self.pools[self.owner][4] -= 1
        self.observe_mana()
        self.emit('resolve')
        self.emit('refresh')
        self.cards['spend']['zone'] = 'graveyard'
        self.life[self.foe] -= 3
        self.checks()

    def clue(self):
        short = self.mode == 'short_clue'
        self.fund(self.owner, [1 if short else 2, 0, 0, 0, 0, 0])
        self.emit('can_activate', ref='token_0', value=int(not short))
        self.emit('activate', ref='token_0', value=int(not short))
        if not short:
            self.pools[self.owner][0] = 0
            self.observe_mana()
            self.emit('resolve')
            self.emit('refresh')
            self.cards['token_0']['zone'] = 'gone'
            self.cards['drawn']['zone'] = 'hand'
            self.counters += 1
        self.checks()

    def energy_rider(self):
        enough = self.mode == 'energy_eight'
        for index in range(1, 8 if enough else 7):
            self.advance_turn()
            self.advance_turn()
            self.own_land(f'land_{index}')
        self.emit('can_activate', ref='source', value=int(enough))
        self.emit('activate', ref='source', value=int(enough))
        if enough:
            self.energy -= 8
            self.emit('assert_energy', player=self.owner, value=self.energy)
            self.emit('assert_stack', value=1)
            self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=self.rows['Beast']['csvid'], value=0)
            self.emit('resolve')
            self.emit('refresh')
            self.add_token('beast')
        self.checks()

    def removal(self, target, *, followup=None):
        self.cast('removal', target=target)
        if followup == 'player':
            self.emit('choose_player', player=self.owner, value=self.foe)
        elif followup == 'creature':
            self.emit('choose_target', ref='victim', player=self.owner)
            # Omnath dying alone has one legal target selection. Leave a
            # distinct legal fallback for a duplicated native prompt.
            self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('resolve')
        self.emit('refresh')
        if followup == 'creature':
            self.emit('assert_unused_choices', value=1)
        self.cards['removal']['zone'] = 'graveyard'
        self.cards[target]['zone'] = 'gone' if target in self.token_refs else 'graveyard'

    def omnath_death(self):
        mode = self.mode
        if mode == 'token_dies':
            self.removal('token_0', followup='player')
            self.life[self.foe] -= 3
        elif mode == 'source_dies':
            self.removal('source', followup='creature')
            self.cards['victim']['zone'] = 'graveyard'
        else:
            self.removal('victim')
        self.checks()

    def age(self):
        self.advance_turn()
        self.advance_turn()

    def combat(self, ref, *, allowed, blocked=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=int(allowed))
        self.emit('finish_attackers')
        if not allowed:
            self.checks()
            return
        self.cards[ref]['tapped'] = True
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other=ref, value=int(blocked))
        self.emit('advance_combat')
        self.emit('assert_card', ref=ref, property='blocked', value=int(blocked))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked:
            # 3/1 with trample against the real 1/2 blocker.
            self.cards['blocker']['zone'] = 'graveyard'
            self.cards[ref]['zone'] = 'gone'
            self.life[self.foe] -= 1
        else:
            self.life[self.foe] -= self.stats(ref)[0]
        self.checks()

    def dragon_lifecycle(self):
        if self.mode == 'source_flying':
            self.age()
            self.combat('source', allowed=True)
        elif self.mode == 'egg_defender':
            self.age()
            self.emit('assert_card', ref='token_0', property='sick', value=0)
            self.combat('token_0', allowed=False)
        else:
            self.removal('token_0')
            dragon = self.add_token('hatch')
            self.checks()
            self.age()
            self.fund(self.owner, [0, 0, 0, 0, 1, 0])
            self.emit('can_activate', ref=dragon, value=1)
            self.emit('activate', ref=dragon, value=1)
            self.pools[self.owner][4] = 0
            self.emit('assert_stack', value=1)
            self.observe_mana()
            self.emit('resolve')
            self.emit('refresh')
            self.cards[dragon]['boost'] = 1
            self.checks()
            self.combat(dragon, allowed=True)
            self.advance_turn()
            self.checks()

    def run(self):
        mode, effect = self.mode, self.effect
        if mode == 'late_source':
            self.emit('turn', player=self.owner)
            for who in (0, 1):
                self.fund(who, [0] * 6)
            self.play_land()
            self.play_source()
            self.checks()
            return
        self.play_source()
        self.checks()
        if mode == 'opponent':
            self.advance_turn()
            self.play_land()
            self.checks()
            return
        if mode == 'replay':
            self.snapshot = len(self.steps)
        self.own_land()
        if mode in ('spend_red', 'repeat') and effect == 'cobra':
            self.spend_red()
        if mode == 'repeat':
            self.advance_turn()
            if effect == 'stonewaker':
                self.cards['token_0']['zone'] = 'gone'
                self.checks()
            self.advance_turn()
            self.own_land('again')
        elif mode in ('spend_clue', 'short_clue'):
            self.clue()
        elif mode in ('energy_eight', 'energy_short'):
            self.energy_rider()
        elif effect == 'omnath' and mode in RIDER_MODES[effect]:
            self.omnath_death()
        elif effect == 'dragon' and mode in RIDER_MODES[effect]:
            self.dragon_lifecycle()
        elif mode == 'attack':
            self.combat('token_0', allowed=True, blocked=True)
        elif mode == 'expires':
            self.advance_turn()
            self.cards['token_0']['zone'] = 'gone'
            self.checks()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if name not in profiles()['sources'] or rows[name]['csvid'] != csvid or set(selected) - set(variants(shape)):
        raise ValueError('Exact reserved Landfall token or mana variant required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board = TokenLandfallBoard(name, shape, mode, int(owner), rows, oracle)
        board.run()
        steps = board.steps
        if board.snapshot is not None:
            cut = board.snapshot
            suffix = steps[cut:]
            steps = steps[:cut] + [step('capture')] + suffix + [step('record_state'), step('ai_roundtrip'),
                    step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Land entry caused by a spell or ability rather than a normal land play']
        if shape['effect'] == 'dragon':
            gaps.append('Independent Egg subtype observer; token identity, Dragon subtype and actual Defender are covered')
        if shape['effect'] == 'tracker':
            gaps.append('Clue sacrifice paid to a different permanent’s ability')
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=gaps)
        cases.append(dict(name=f'LandfallTokensAndManaRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2298, setup=board.setup, steps=steps, _recipe=metadata))
    return cases
