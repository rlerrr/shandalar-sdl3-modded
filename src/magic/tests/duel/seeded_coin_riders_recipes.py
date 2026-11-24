"""Rules receipts for seeded coin calls and the cards' actual payoffs."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from block_restriction_spells_recipes import _World as PermanentWorld
from next_upkeep_cantrip_additional_riders_recipes import COIN_SEED, actual_call

FAMILY = 'seeded_coin_riders'
PROFILE = Path(__file__).with_name('data') / 'seeded_coin_riders_wave8_profiles.json'
SHAPES = {
    'bottle': ('manalinkeh_card_bottle_of_suleiman', 'Artifact',
               '{1}, Sacrifice this artifact: Flip a coin. If you win the flip, create a 5/5 colorless Djinn '
               'artifact creature token with flying. If you lose the flip, this artifact deals 5 damage to you.'),
    'crypt': ('manalinkeh_card_mana_crypt', 'Artifact',
              'At the beginning of your upkeep, flip a coin. If you lose the flip, this artifact deals 3 damage to you.\n'
              '{T}: Add {C}{C}.'),
    'birth': ('manalinkeh_card_molten_birth', 'Sorcery',
              'Create two 1/1 red Elemental creature tokens, then flip a coin. If you win the flip, '
              "return Molten Birth to its owner's hand."),
    'wild': ('manalinkeh_card_wild_wurm', 'Creature',
             "When this creature enters, flip a coin. If you lose the flip, return this creature to its owner's hand."),
    'boom': ('manalinkeh_card_boompile', 'Artifact',
             '{T}: Flip a coin. If you win the flip, destroy all nonland permanents.'),
    'swindle': ('manalinkeh_card_tavern_swindler', 'Creature',
                '{T}, Pay 3 life: Flip a coin. If you win the flip, you gain 6 life.'),
    'scoria': ('manalinkeh_card_scoria_wurm', 'Creature',
               'At the beginning of your upkeep, flip a coin. If you lose the flip, '
               "return this creature to its owner's hand."),
    'strongbox': ('manalinkeh_card_sorcerers_strongbox', 'Artifact',
                  '{2}, {T}: Flip a coin. If you win the flip, sacrifice this artifact and draw three cards.'),
    'orc': ('manalinkeh_card_orcish_captain', 'Creature',
            '{1}: Flip a coin. If you win the flip, target Orc creature gets +2/+0 until end of turn. '
            'If you lose the flip, it gets -0/-2 until end of turn.'),
    'sky': ('manalinkeh_card_winter_sky', 'Sorcery',
            'Flip a coin. If you win the flip, Winter Sky deals 1 damage to each creature and each player. '
            'If you lose the flip, each player draws a card.'),
    'verdict': ('manalinkeh_card_puppets_verdict', 'Instant',
                'Flip a coin. If you win the flip, destroy all creatures with power 2 or less. '
                'If you lose the flip, destroy all creatures with power 3 or greater.'),
    'stitch': ('manalinkeh_card_stitch_in_time', 'Sorcery',
               'Flip a coin. If you win the flip, take an extra turn after this one.'),
}
ACTIVATIONS = {
    'bottle': (1, False, True, 0), 'boom': (0, True, False, 0), 'swindle': (0, True, False, 3),
    'strongbox': (2, True, False, 0), 'orc': (1, False, False, 0),
}
EXTRAS = {
    'bottle': ('unfunded', 'flight'), 'crypt': ('opponent_only', 'mana'), 'birth': ('unfunded',),
    'wild': ('unfunded',), 'boom': ('tapped',), 'swindle': ('low_life', 'fresh'), 'scoria': ('opponent_only',),
    'strongbox': ('unfunded', 'tapped'), 'orc': ('unfunded', 'own_expiry', 'enemy_expiry'),
    'sky': ('unfunded',), 'verdict': ('offturn_win',), 'stitch': ('unfunded',),
}
SPELLS = {'birth', 'sky', 'verdict', 'stitch'}
UPKEEPS = {'scoria', 'crypt'}
WEAK = "Mons's Goblin Raiders"
BEAR = 'Grizzly Bears'
GIANT = 'Hill Giant'
ARTIFACT = 'Darksteel Relic'
ENCHANTMENT = 'Fervor'
BLOCKER = 'Suntail Hawk'
ORC = 'Orcish Artillery'
LIBS = (('Forest', 'Plains', 'Island', 'Mountain'), ('Island', 'Mountain', 'Swamp', 'Forest'))
WITNESSES = {ARTIFACT, ENCHANTMENT, BEAR, GIANT, WEAK, BLOCKER, ORC, 'Elemental', *LIBS[0], *LIBS[1]}
VANILLA = {BEAR, GIANT, WEAK, BLOCKER}
TOKENS = {
    'bottle': ('Djinn', 890, 5, 5, 0, 1),
    'birth': ('Elemental', 18726, 1, 1, 16, 0),
}
GAPS = {
    'bottle': ('The Djinn identity, token, color, artifact type, P/T, and flying are observed. '
               'The token subtype inspector does not expose Djinn.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords')):
        raise ValueError('complete normal seeded-coin Oracle required')
    flag = next((key for key, (_, card_type, text) in SHAPES.items()
                 if text == record['oracle_text'] and card_type == record['type_line'].split(' — ')[0]), None)
    if flag is None:
        raise ValueError('whole supported coin cost and rider required')
    mana = [0] * 6 if record['mana_cost'] == '{0}' else cost_from_oracle(record)
    return dict(shape=flag, mana=mana, keywords=[])


def variants(params):
    flag = params['shape']
    return [f'{scene}_{owner}' for owner in (0, 1) for scene in ('win', 'lose', 'replay') + EXTRAS[flag]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed coin-rider variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('typed declaration must equal whole pinned coin rider Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('exact reviewed seeded-coin witness profile required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or row['csvid'] in skipped
                    or record.get('layout') != ('token' if name == 'Elemental' else 'normal')
                    or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'changed seeded-coin source or witness: {name}')
            if group == 'sources' and row['handler'] != SHAPES[classify(record)['shape']][0]:
                raise ValueError(f'changed seeded-coin callback: {name}')
    if data['native_djinn'] != rows.get('Djinn') or data['native_djinn']['csvid'] != 890:
        raise ValueError('Bottle creates its separately pinned native Djinn identity')
    elemental = oracle['Elemental']
    if (rows['Elemental']['csvid'], elemental['type_line'], elemental['power'], elemental['toughness'], elemental['colors']) != (
            18726, 'Token Creature — Elemental', '1', '1', ['R']):
        raise ValueError('Molten Birth requires the exact native red Elemental identity')


class CoinWorld(PermanentWorld):
    def __init__(self, name, params, owner, rows, oracle, scene):
        super().__init__(name, params, owner, rows, oracle)
        self.scene = scene
        self.shape = params['shape']
        self.wins = scene not in ('lose', 'enemy_expiry')
        self.current = self.foe if scene == 'offturn_win' else self.owner
        self.token_count = 0
        self.sweep = []
        seeded = self.shape in ('swindle', 'orc', 'scoria') and scene != 'fresh' or scene == 'tapped'
        self.add(name, 'source', self.owner, 'battlefield' if seeded else 'hand')
        if scene == 'tapped':
            self.setup[0]['state'] = ['tapped']
        for player in (0, 1):
            for index, card in enumerate(LIBS[int(player != self.owner)]):
                self.add(card, f'library_{player}_{index}', player, 'library')

    def setup_boards(self):
        if self.shape in ('boom', 'verdict', 'sky'):
            for p in (0, 1):
                self.add('Forest', f'land_{p}', p)
                if self.shape == 'boom':
                    group = (('soft', BEAR), ('enchantment', ENCHANTMENT)) if p == self.owner else (('soft', BEAR),)
                elif self.shape == 'verdict':
                    group = (('low', BEAR), ('high', GIANT))
                else:
                    group = (('weak', WEAK), ('strong', BEAR))
                for key, name in group:
                    ref = f'{key}_{p}'
                    self.add(name, ref, p)
                    self.sweep.append(ref)
            if self.shape == 'boom':
                self.add(ARTIFACT, 'indestructible', self.foe)
        if self.scene == 'flight':
            self.add(BEAR, 'ground', self.foe)
            self.add(BLOCKER, 'flier', self.foe)
        if self.shape == 'orc':
            self.add(ORC, 'orc', self.foe if self.scene == 'enemy_expiry' else self.owner)

    def start(self):
        self.emit('turn', player=self.current, value=0)
        for p in (0, 1):
            self.pool(p, [0] * 6)
        self.receipts()

    def receipts(self):
        super().receipts()
        if self.scene == 'tapped' and self.cards['source'][2] == 'battlefield':
            self.observe('source', 'tapped', 1)
        if self.shape not in TOKENS:
            return
        token, csvid, power, toughness, color, artifact = TOKENS[self.shape]
        for p in (0, 1):
            for zone in ('battlefield', 'graveyard', 'exile'):
                self.emit('assert_zone_multiplicity', player=p, zone=zone, index=csvid,
                          value=self.token_count if zone == 'battlefield' and p == self.owner else 0)
        if self.token_count:
            props = [('token', 1), ('artifact', artifact), ('power', power), ('toughness', toughness), ('color', color)]
            if token == 'Elemental':
                props.append(('subtype_Elemental', 1))
            for prop, value in props:
                self.emit('assert_matching_permanents', player=self.owner, index=csvid, property=prop, value=value)

    def upkeep(self, flip=False):
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for creature in self.entities.values():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature.update(tapped=False, sick=False)
        if flip:
            self.call_coin()
        self.emit('upkeep_step', player=self.current)
        if flip:
            self.apply_outcome()
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def call_coin(self):
        self.emit('choose_dialog', player=self.owner, value=actual_call(self.wins))

    def draw(self, player, n):
        for _ in range(n):
            self.zones['hand'][player].append(self.zones['library'][player].pop(0))

    def deal(self, ref, damage):
        creature = self.entities[ref]
        creature['damage'] += damage
        if creature['damage'] >= creature['toughness']:
            self.move(ref, 'graveyard')

    def apply_outcome(self):
        shape, win, owner = self.shape, self.wins, self.owner
        if shape == 'bottle':
            if win:
                self.add_tokens(1)
            else:
                self.life[owner] -= 5
        elif shape == 'birth':
            self.add_tokens(2)
            self.move('source', 'hand' if win else 'graveyard')
        elif shape in ('wild', 'scoria'):
            if not win:
                self.move('source', 'hand')
        elif shape == 'crypt' and not win:
            self.life[owner] -= 3
        elif shape == 'boom' and win:
            for ref in (*self.sweep, 'source'):
                self.move(ref, 'graveyard')
        elif shape == 'swindle' and win:
            self.life[owner] += 6
        elif shape == 'strongbox' and win:
            self.move('source', 'graveyard')
            self.draw(owner, 3)
        elif shape == 'orc':
            target = self.entities['orc']
            if win:
                target['power'] += 2
            else:
                target['toughness'] -= 2
        elif shape == 'sky':
            for player in (0, 1):
                if win:
                    self.life[player] -= 1
                else:
                    self.draw(player, 1)
            if win:
                for ref in self.sweep:
                    self.deal(ref, 1)
        elif shape == 'verdict':
            for ref in self.sweep:
                if (self.entities[ref]['power'] <= 2) == win:
                    self.move(ref, 'graveyard')

    def add_tokens(self, n):
        self.zones['battlefield'][self.owner].extend([TOKENS[self.shape][0]] * n)
        self.token_count += n

    def finish_flying_attack(self):
        for _ in range(2):
            self.upkeep()
        self.emit('bind_permanent', ref='token', player=self.owner, value=TOKENS['bottle'][1])
        self.emit('begin_attackers')
        self.emit('attack', ref='token', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='ground', other='token', value=0)
        self.emit('block', ref='flier', other='token', value=1)
        self.emit('advance_combat')
        self.observe('token', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.move('flier', 'graveyard')
        self.observe('token', 'damage', 1)
        self.receipts()


def spell_or_etb(w):
    if w.scene == 'unfunded':
        w.cast('source', denied=True, budget=[0] * 6)
        w.receipts()
        return None
    w.cast('source')
    cut = len(w.steps) if w.scene == 'replay' else None
    w.call_coin()
    destination = 'battlefield' if w.shape == 'wild' else 'hand' if w.shape == 'birth' else 'graveyard'
    w.finish('source', destination)
    w.apply_outcome()
    w.receipts()
    if w.shape == 'stitch':
        first = w.owner if w.wins else w.foe
        w.emit('advance_scheduled_turn', value=first)
        w.emit('advance_scheduled_turn', value=w.foe if w.wins else w.owner)
    return cut


def upkeep_rider(w):
    if w.cards['source'][2] == 'hand':
        if w.shape != 'crypt' or any(w.p['mana']):
            raise ValueError('the upkeep hand fixture requires the actual printed zero-cost Mana Crypt')
        w.emit('cast', ref='source', value=1)
        w.emit('assert_stack', value=1)
        w.observe('source', 'in_hand', 0)
        w.emit('assert_mana_total', player=w.owner, value=0)
        w.finish('source', 'battlefield')
        w.receipts()
    if w.scene == 'mana':
        w.emit('can_activate', ref='source', value=1)
        w.emit('activate', ref='source', value=1)
        w.emit('can_activate', ref='source', value=0)
        w.observe('source', 'tapped', 1)
        w.mana[w.owner][0] = 2
        w.receipts()
        return None
    w.upkeep()
    if w.scene == 'opponent_only':
        return None
    cut = len(w.steps) if w.scene == 'replay' else None
    w.upkeep(flip=True)
    return cut


def activated(w):
    shape = w.shape
    if w.cards['source'][2] == 'hand':
        w.cast('source')
        w.finish('source', 'battlefield')
        w.receipts()
    generic, taps, sacrifice, life = ACTIVATIONS[shape]
    if w.scene == 'low_life':
        w.emit('set_life', player=w.owner, value=2)
        w.life[w.owner] = 2
    budget = [0 if w.scene == 'unfunded' else generic, 0, 0, 0, 0, 0]
    w.pool(w.owner, budget)
    w.emit('refresh')
    no_cost = w.scene in ('unfunded', 'low_life', 'fresh', 'tapped')
    w.emit('can_activate', ref='source', value=int(not no_cost))
    if shape == 'orc' and not no_cost:
        w.emit('choose_target', ref='orc', player=w.owner)
    w.emit('activate', ref='source', value=int(not no_cost))
    w.emit('assert_unused_choices', value=0)
    w.emit('assert_stack', value=int(not no_cost))
    if no_cost:
        w.receipts()
        return None
    w.mana[w.owner][0] -= generic
    w.life[w.owner] -= life
    w.emit('assert_life', player=w.owner, value=w.life[w.owner])
    w.emit('assert_mana_total', player=w.owner, value=0)
    if sacrifice:
        w.move('source', 'graveyard')
        w.emit('assert_zone_multiplicity', player=w.owner, zone='graveyard', index=w.rows[w.name]['csvid'], value=1)
    if taps:
        w.observe('source', 'tapped', 1)
        if 'source' in w.entities:
            w.entities['source']['tapped'] = True
    cut = len(w.steps) if w.scene == 'replay' else None
    w.call_coin()
    w.emit('resolve')
    w.apply_outcome()
    w.receipts()
    if w.scene == 'flight':
        w.finish_flying_attack()
    if shape == 'orc' and w.scene.endswith('_expiry'):
        subject = w.entities['orc']
        subject.update(power=int(w.o[ORC]['power']), toughness=int(w.o[ORC]['toughness']))
        w.upkeep()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact seeded-coin source CSVID required')
    cases = []
    shape = params['shape']
    for variant in selected:
        scene, owner = variant.rsplit('_', 1)
        world = CoinWorld(name, params, int(owner), rows, oracle, scene)
        world.setup_boards()
        world.start()
        cut = (spell_or_etb if shape in SPELLS or shape == 'wild' else upkeep_rider if shape in UPKEEPS else activated)(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'SeededCoinRidersRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=COIN_SEED,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS.get(shape, ())))))
    return cases
