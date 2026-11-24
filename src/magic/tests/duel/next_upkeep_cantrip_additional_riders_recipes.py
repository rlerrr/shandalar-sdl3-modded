"""Combat riders paired with a draw at the next actual upkeep."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from next_upkeep_cantrip_riders_recipes import DRAW, LIBRARY_FOE, LIBRARY_OWNER, _World as DelayedWorld

FAMILY = 'next_upkeep_cantrip_additional_riders'
PROFILE = Path(__file__).with_name('data') / 'next_upkeep_cantrip_additional_riders_wave8_profiles.json'
MENACE = ('Cast this spell only during combat before blockers are declared.\n'
          "All creatures gain menace until end of turn. (They can't be blocked except by two or more creatures.)")
BANDING = ('Target creature gains banding until end of turn. (Any creatures with banding, and up to one without, '
           'can attack in a band. Bands are blocked as a group. If any creatures with banding a player controls '
           'are blocking or being blocked by a creature, that player divides that creature\'s combat damage, '
           "not its controller, among any of the creatures it's being blocked by or is blocking.)")
COIN = ('Cast this spell only during combat after blockers are declared.\n'
        'Flip a coin. If you win the flip, target creature gets +1/+1 until end of turn.')
VIEW = "Look at target player's hand."
SHAPES = {
    'menace': ('manalinkeh_card_gorilla_war_cry', MENACE),
    'banding': ('manalinkeh_card_formation', BANDING),
    'coin': ('manalinkeh_card_aleatory', COIN),
    'view': ('manalinkeh_card_clairvoyance', VIEW),
}
SCENES = {
    'menace': ('one_block', 'pair_blocks', 'offturn', 'expiry', 'main_denied', 'after_blocks'),
    'banding': ('assign_holder', 'assign_ally', 'expired', 'replay', 'short'),
    'coin': ('win', 'lose', 'offturn_win', 'replay', 'main_denied', 'before_blocks'),
    'view': ('effect', 'own_hand', 'empty_hand', 'offturn', 'replay', 'short'),
}
BEAR = 'Grizzly Bears'
ONE = "Mons's Goblin Raiders"
GIANT = 'Hill Giant'
WITNESSES = {*LIBRARY_OWNER, *LIBRARY_FOE, BEAR, ONE, GIANT}
VANILLA = {BEAR, ONE, GIANT}
COIN_SEED = 2087
GAPS = {
    'view': ("The headless reveal helper returns before opening the targeted hand view. "
             'The real target, unaffected hands and next-upkeep draw are observed.',),
    'banding': ('The scripted combat exercises Banding damage assignment. Attacking band formation has a separate UI.',),
    'menace': ('Menace is observed on actual attackers. Nonattacking creatures have no distinct combat receipt in this fixture.',),
}


def xorshift(state):
    state ^= (state << 13) & 0xffffffff
    state ^= state >> 17
    return state ^ ((state << 5) & 0xffffffff)


def actual_call(wins):
    # player_flips_a_coin consumes one sample for the default call and the
    # coin_flip engine consumes the next sample for its actual result.
    result = xorshift(xorshift(COIN_SEED)) % 2
    return result if wins else 1 - result


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Instant' or record.get('keywords')):
        raise ValueError('full normal delayed combat instant required')
    mode = next((key for key, (_, text) in SHAPES.items() if record['oracle_text'] == text + '\n' + DRAW), None)
    if mode is None:
        raise ValueError('exactly modeled combat rider and next-upkeep draw required')
    return dict(mode=mode, instant=True, cost=cost_from_oracle(record), keywords=[])


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in SCENES[params['mode']]]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed delayed combat variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('declaration must equal the whole pinned delayed combat Oracle')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('complete delayed combat fixture pins required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card' or record.get('layout') != 'normal'
                    or row['csvid'] in skipped or (row['handler'] in (None, 'NULL', '0', 'card_dummy') and name not in VANILLA)):
                raise ValueError(f'changed pinned delayed combat source or witness: {name}')
            if group == 'sources' and row['handler'] != SHAPES[classify(record)['mode']][0]:
                raise ValueError(f'changed delayed combat production callback: {name}')


class CombatCantrip(DelayedWorld):
    def __init__(self, name, params, owner, rows, oracle, scene):
        super().__init__(name, params, owner, rows, oracle, scene)
        self.add(name, 'source', owner, 'hand')
        self.seed_library(owner, LIBRARY_OWNER)
        self.seed_library(self.foe, LIBRARY_FOE)
        self.unspecified_damage = set()
        self.current = (1 if params['mode'] == 'banding' else self.foe if scene.startswith('offturn') else owner)

    def setup_combat(self):
        mode = self.p['mode']
        self.add(ONE if mode == 'banding' else BEAR, 'attacker', self.current)
        self.add(GIANT if mode == 'banding' else BEAR if mode == 'coin' else ONE, 'blocker', 1 - self.current)
        if mode == 'banding' or self.scene == 'pair_blocks':
            self.add(BEAR if mode == 'banding' else ONE, 'second_blocker', 1 - self.current)

    def start(self):
        self.emit('turn', player=self.current, value=0)
        for player in (0, 1):
            self.pool(player, [0] * 6)
        self.receipts()

    def begin(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='attacker', value=1)
        self.entities['attacker'].update(tapped=True, attacking=True)
        self.emit('finish_attackers')

    def blockers(self, blocked, pair=False):
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other='attacker', value=int(blocked))
        if pair:
            self.emit('block', ref='second_blocker', other='attacker', value=1)
        self.emit('advance_combat')
        self.observe('attacker', 'blocked', blocked)
        self.observe('blocker', 'blocking_count', int(blocked))
        if pair:
            self.observe('second_blocker', 'blocking_count', 1)

    def combat(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.entities['attacker']['attacking'] = False

    def incoming(self, ref, damage):
        subject = self.entities[ref]
        subject['damage'] += damage
        if subject['damage'] >= subject['toughness']:
            self.move(ref, 'graveyard')

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.unspecified_damage:
            # After expiry the AI may assign the one damage to either blocker.
            self.steps[start:] = [op for op in self.steps[start:]
                                  if not (op.get('property') == 'damage' and op.get('ref') in self.unspecified_damage)]

    def player_hit(self):
        self.combat()
        self.life[1 - self.current] -= self.entities['attacker']['power']
        self.receipts()

    def expire(self):
        super().expire()
        for ref, entity in self.entities.items():
            name = self.cards[ref][0]
            entity.update(power=int(self.o[name]['power']), toughness=int(self.o[name]['toughness']))

    def delayed_draw(self):
        self.next_upkeep(1)
        self.next_upkeep()


def do_coin(w):
    mode = w.scene
    if mode == 'main_denied':
        w.pay('source', allowed=False)
        w.receipts()
        return None
    w.begin()
    if mode == 'before_blocks':
        w.pay('source', allowed=False)
        w.receipts()
        return None
    w.blockers(True)
    cut = len(w.steps) if mode == 'replay' else None
    wins = mode != 'lose'
    w.pay('source', spell_target='attacker')
    w.emit('choose_dialog', player=w.owner, value=actual_call(wins))
    w.finish('source', 'graveyard')
    if wins:
        w.entities['attacker']['power'] += 1
        w.entities['attacker']['toughness'] += 1
    w.receipts()
    w.combat()
    attacker, blocker = w.entities['attacker'], w.entities['blocker']
    w.incoming('attacker', blocker['power'])
    w.incoming('blocker', attacker['power'])
    w.receipts()
    w.delayed_draw()
    return cut


def do_menace(w):
    mode = w.scene
    if mode == 'main_denied':
        w.pay('source', allowed=False)
        w.receipts()
        return None
    w.begin()
    if mode == 'after_blocks':
        w.blockers(True)
        w.pay('source', allowed=False)
        w.receipts()
        return None
    w.pay('source')
    w.finish('source', 'graveyard')
    w.receipts()
    if mode == 'pair_blocks':
        w.blockers(True, True)
        for ref in ('attacker', 'blocker', 'second_blocker'):
            w.emit('remove_from_combat', ref=ref)
        w.combat()
        w.receipts()
    else:
        w.blockers(False)
        w.player_hit()
    w.delayed_draw()
    if mode == 'expiry':
        w.begin()
        w.blockers(True)
        w.combat()
        w.incoming('attacker', 1)
        w.incoming('blocker', 2)
        w.receipts()
    return None


def do_banding(w):
    mode = w.scene
    if mode == 'short':
        w.pay('source', allowed=False, limit=[0, 0, 0, 0, 0, 1])
        w.receipts()
        return None
    cut = len(w.steps) if mode == 'replay' else None
    w.pay('source', spell_target='blocker')
    w.finish('source', 'graveyard')
    w.receipts()
    expired = mode == 'expired'
    if expired:
        w.delayed_draw()
    w.begin()
    w.blockers(True, True)
    recipient = 'second_blocker' if mode == 'assign_ally' else 'blocker'
    # Banding gives the defending player (seat 0) control of the attacking
    # Goblin's one damage point. Both blockers survive either allocation.
    w.emit('choose_target', player=0, ref=recipient)
    w.combat()
    w.emit('assert_unused_choices', value=int(expired))
    w.incoming('attacker', 5)
    if expired:
        w.unspecified_damage.update(('blocker', 'second_blocker'))
    else:
        w.incoming(recipient, 1)
    w.receipts()
    if not expired:
        w.delayed_draw()
    return cut


def do_view(w):
    if w.scene == 'short':
        w.pay('source', allowed=False, limit=[0] * 6)
        w.receipts()
        return None
    cut = len(w.steps) if w.scene == 'replay' else None
    target = w.owner if w.scene == 'own_hand' else w.foe
    w.pay('source', player_target=target)
    w.finish('source', 'graveyard')
    w.receipts()
    w.delayed_draw()
    return cut


RUN = {'menace': do_menace, 'banding': do_banding, 'coin': do_coin, 'view': do_view}


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact delayed combat source CSVID required')
    cases = []
    for variant in selected:
        scene, owner = variant.rsplit('_', 1)
        world = CombatCantrip(name, params, int(owner), rows, oracle, scene)
        if params['mode'] == 'view':
            world.add('Island', 'own_witness', world.owner, 'hand')
            if scene != 'empty_hand':
                world.add('Mountain', 'enemy_witness', world.foe, 'hand')
                world.add('Swamp', 'enemy_second', world.foe, 'hand')
        else:
            world.setup_combat()
        world.start()
        cut = RUN[params['mode']](world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'NextUpkeepCantripAdditionalRidersRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=COIN_SEED, setup=world.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GAPS.get(params['mode'], ())))))
    return cases
