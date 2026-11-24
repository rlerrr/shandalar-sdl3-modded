"""Oracle-pinned targeted boosts and their actual delayed Rebound casts."""

import copy
import json
import re
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'targeted_rebound'
PROFILE = Path(__file__).with_name('data') / 'targeted_rebound_wave8_profiles.json'
REBOUND = ("Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of "
           "your next upkeep, you may cast this card from exile without paying its mana cost.)")
BOOST = re.compile(r"Target creature gets \+([1-5])/\+([0-5]) until end of turn( and can't be blocked this turn)?\.")
HANDLERS = {'manalinkeh_card_distortion_strike', 'manalinkeh_card_taigams_strike', 'manalinkeh_card_artful_maneuver'}
BEAR, SQUIRE, GIANT = 'Grizzly Bears', 'Squire', 'Hill Giant'
COUNTER, BOUNCE, WRATH = 'Counterspell', 'Unsummon', 'Wrath of God'
LANDS = ('Forest', 'Island')
WITNESSES = {BEAR, SQUIRE, GIANT, COUNTER, BOUNCE, WRATH, *LANDS}
ZERO = [0] * 6
PAIRS = ('return_same', 'retarget_enemy', 'no_target', 'countered', 'fizzle', 'combat', 'snapshot', 'wrong_color')
GAPS = ('The complete real upkeep driver does not expose a separate response window to the recast.',
        'The AI outer policy elects to take Rebound; actual optional decline is exercised by the human seat.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords') != ['Rebound'] or record.get('type_line') not in ('Instant', 'Sorcery')):
        raise ValueError('normal targeted Rebound Instant or Sorcery required')
    boost, *extra = record['oracle_text'].splitlines()
    if extra != [REBOUND] or not (match := BOOST.fullmatch(boost)):
        raise ValueError('complete reviewed targeted Rebound wording required')
    return dict(power=int(match[1]), toughness=int(match[2]), unblockable=bool(match[3]),
                instant=record['type_line'] == 'Instant', mana=cost_from_oracle(record), keywords=[])


def variants(params):
    pairs = PAIRS
    if params['instant']:
        pairs += ('offturn',)
    if params['unblockable']:
        pairs += ('expired_block',)
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in pairs] + ['decline_0']


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed targeted Rebound variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or profiles()['sources'].get(record['name'], {}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact typed declaration with full pinned targeted Rebound Oracle required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {r['csvid'] for r in json.loads((PROFILE.parent / 'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses', ())) != WITNESSES:
        raise ValueError('reviewed exact targeted Rebound profiles required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row, record = rows.get(name, {}), oracle.get(name, {})
            if (pin != dict(oracle=record, registry=row) or row.get('kind') != 'card'
                    or record.get('layout') != 'normal' or row['csvid'] in skipped or row['handler'] in (None, '0', 'NULL')
                    or row['handler'] == 'card_dummy' and name not in (BEAR, SQUIRE, GIANT)):
                raise ValueError(f'unreviewed targeted Rebound binding: {name}')
            if group == 'sources' and row['handler'] not in HANDLERS:
                raise ValueError(f'unreviewed targeted Rebound handler: {name}')


class ReboundWorld(_AttackResourceWorld):
    def __init__(self, name, params, mode, owner, rows, oracle):
        super().__init__(name, dict(params, effect=FAMILY), owner, rows, oracle)
        self.mode = mode
        self.foe = 1 - owner
        self.configure()

    def configure(self):
        self.add(self.name, 'source', self.owner, 'hand')
        self.add(BEAR, 'first', self.owner)
        if self.mode in ('retarget_enemy', 'snapshot'):
            self.add(SQUIRE, 'other', self.foe)
        elif self.mode == 'expired_block':
            self.add(SQUIRE, 'other', self.owner)
        if self.mode in ('combat', 'expired_block'):
            self.add(GIANT, 'blocker', self.foe)
        if self.mode == 'countered':
            self.add(COUNTER, 'response', self.foe, 'hand')
        if self.mode == 'fizzle':
            self.add(BOUNCE, 'response', self.foe, 'hand')
        if self.mode == 'no_target':
            self.add(WRATH, 'wrath', self.owner, 'hand')
            self.add(SQUIRE, 'recovery', self.owner, 'hand')
        for player in (0, 1):
            for index, name in enumerate(LANDS):
                self.add(name, f'library_{player}_{index}', player, 'library')
        if self.mode == 'offturn':
            self.current = self.foe
        self.emit('turn', player=self.current, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.pool(player, ZERO)
        self.receipts()

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def funds(self):
        for player, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)

    def receipts(self):
        # A deferred human priority choice may legitimately remain queued until
        # the next genuine upkeep. Check it independently at that boundary.
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.funds()
        universe = sorted({card['name'] for card in self.setup})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, both in self.zones.items():
                cards = both[player]
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[name]['csvid'])
                else:
                    for name in universe:
                        if any(card['name'] == name and card['player'] == player for card in self.setup):
                            self.emit('assert_zone_multiplicity', player=player, zone=zone,
                                      index=self.rows[name]['csvid'], value=cards.count(name))
        for ref, entity in self.entities.items():
            if entity['zone'] == 'battlefield':
                self.observe(ref, 'in_play', 1)
                for prop in ('power', 'toughness', 'damage'):
                    self.observe(ref, prop, entity[prop])

    def move_spell(self, name, player, start, end):
        self.zones[start][player].remove(name)
        self.zones[end][player].append(name)

    def boost(self, ref):
        entity = self.entities[ref]
        for stat in ('power', 'toughness'):
            entity[stat] += self.p[stat]

    def paid_cast(self):
        if self.mode == 'wrong_color':
            self.pool(self.owner, [sum(self.p['mana']), 0, 0, 0, 0, 0])
            self.emit('cast', ref='source', value=0)
            self.emit('assert_unused_choices', value=0)
            self.observe('source', 'in_hand', True)
            self.receipts()
            return
        self.pool(self.owner, self.p['mana'])
        self.emit('choose_target', player=self.owner, ref='first')
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        self.observe('source', 'in_hand', 0)
        self.mana[self.owner] = ZERO.copy()
        self.funds()
        self.observe('first', 'power', self.entities['first']['power'])
        self.observe('first', 'toughness', self.entities['first']['toughness'])
        if self.mode in ('countered', 'fizzle'):
            self.respond()
            self.move_spell(self.name, self.owner, 'hand', 'graveyard')
        else:
            if self.owner == 0 and self.mode not in ('decline', 'combat'):
                # The source instance is still valid now. The priority decision
                # records its CSVID and later matches the offered display legacy.
                self.emit('choose_trigger', player=self.owner, ref='source')
            self.emit('resolve')
            self.move_spell(self.name, self.owner, 'hand', 'exile')
            self.boost('first')
        self.receipts()

    def respond(self):
        from recipes import cost_from_oracle

        response = COUNTER if self.mode == 'countered' else BOUNCE
        self.emit('announce_spell', ref='source')
        self.pool(self.foe, cost_from_oracle(self.o[response]))
        if response == BOUNCE:
            self.emit('choose_target', player=self.foe, ref='first')
        self.emit('cast_response', ref='response', value=1)
        self.emit('assert_stack', value=2)
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=0)
        self.mana[self.foe] = ZERO.copy()
        self.move_spell(response, self.foe, 'hand', 'graveyard')
        if response == BOUNCE:
            self.leave('first', 'hand')

    def prepare_upkeep(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('real upkeep requires no floating mana')
        self.emit('advance_turn')
        self.expire()
        for entity in self.entities.values():
            for stat in ('power', 'toughness'):
                entity[stat] = int(self.o[entity['name']][stat])
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        self.receipts()

    def perform_upkeep(self, target=None, observe_choices=True):
        if target:
            self.emit('choose_target', player=self.owner, ref=target)
        self.emit('upkeep_step', player=self.current)
        if observe_choices:
            self.emit('assert_unused_choices', value=0)
        if target:
            self.move_spell(self.name, self.owner, 'exile', 'graveyard')
            self.boost(target)
        self.receipts()
        self.emit('turn', player=self.current, value=0)

    def no_target_witness(self):
        from recipes import cost_from_oracle

        self.pool(self.owner, cost_from_oracle(self.o[WRATH]))
        self.emit('cast', ref='wrath', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.mana[self.owner] = ZERO.copy()
        self.move_spell(WRATH, self.owner, 'hand', 'graveyard')
        self.leave('first', 'graveyard')
        self.receipts()

    def restore_legal_target(self):
        from recipes import cost_from_oracle

        self.pool(self.owner, cost_from_oracle(self.o[SQUIRE]))
        self.emit('cast', ref='recovery', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.mana[self.owner] = ZERO.copy()
        self.arrive('recovery')
        self.receipts()

    def fight(self, blocked, bonus):
        self.emit('begin_attackers')
        self.emit('attack', ref='first', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='blocker', other='first', value=int(blocked))
        self.observe('blocker', 'blocking_count', blocked)
        if not blocked:
            self.observe('first', 'blocked', 0)
        self.emit('advance_combat')
        if blocked:
            self.observe('first', 'blocked', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker, defender = self.entities['first'], self.entities['blocker']
        if blocked:
            attacker['damage'] = defender['power']
            defender['damage'] = attacker['power']
            for ref in ('first', 'blocker'):
                entity = self.entities[ref]
                if entity['damage'] >= entity['toughness']:
                    self.leave(ref, 'graveyard')
        else:
            self.life[self.foe] -= attacker['power']
        if bonus and not self.p['unblockable'] and attacker['zone'] != 'battlefield':
            raise ValueError('stat boost combat must save the legal attacker')
        self.receipts()


def scenario(world):
    mode = world.mode
    world.paid_cast()
    if mode == 'wrong_color':
        return None
    if mode == 'combat':
        world.fight(blocked=not world.p['unblockable'], bonus=True)
        return None
    if mode == 'no_target':
        world.no_target_witness()
    if world.current == world.owner:
        world.prepare_upkeep()
        deferred = world.owner == 0 and mode not in ('countered', 'fizzle', 'decline')
        world.perform_upkeep(observe_choices=not deferred)
    world.prepare_upkeep()
    cut = len(world.steps) if mode == 'snapshot' else None
    target = None
    if mode in ('return_same', 'offturn'):
        target = 'first'
    elif mode in ('retarget_enemy', 'snapshot', 'expired_block'):
        target = 'other'
    world.perform_upkeep(target)
    if mode == 'expired_block':
        world.fight(blocked=True, bonus=False)
    if mode == 'no_target':
        world.restore_legal_target()
    if mode in ('return_same', 'retarget_enemy', 'no_target', 'snapshot', 'decline', 'offturn'):
        for _ in range(2):
            world.prepare_upkeep()
            world.perform_upkeep()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact targeted Rebound source CSVID required')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = ReboundWorld(name, params, mode, int(owner), rows, oracle)
        cut = scenario(world)
        steps = world.steps
        if cut is not None:
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'TargetedReboundRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=163,
                          setup=world.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                              params=declaration['params'], oracle_text=declaration['oracle_text'],
                              untested_components=list(GAPS))))
    return cases

