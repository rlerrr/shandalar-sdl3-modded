"""Upkeep recursion, real land payment, modal entry, and temporary returns."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'graveyard_return_cost_modes'
PROFILE = Path(__file__).parent / 'data/graveyard_return_cost_modes_wave8_profiles.json'
UPKEEP = 'At the beginning of your upkeep, '
PATTERNS = {
    UPKEEP + 'if this creature is in your graveyard, you may pay {2}{G}. If you do, return target creature card from your graveyard to your hand.':
        ('genesis', '{4}{G}', 'Creature — Incarnation', (), (4, 4)),
    ('As this enchantment enters, choose Khans or Dragons.\n• Khans — ' + UPKEEP
     + 'return target creature card from your graveyard to your hand.\n• Dragons — ' + UPKEEP
     + 'each opponent loses 2 life and you gain 2 life.'):
        ('siege', '{3}{B}{B}', 'Enchantment', (), None),
    ("Swampwalk (This creature can't be blocked as long as defending player controls a Swamp.)\n" + UPKEEP
     + 'return target creature card from your graveyard to the battlefield.\n'
     "At the beginning of each opponent's upkeep, that player sacrifices a creature of their choice."):
        ('sheoldred', '{5}{B}{B}', 'Legendary Creature — Phyrexian Praetor', ('Landwalk', 'Swampwalk'), (6, 6)),
    (UPKEEP + 'you lose 1 life.\n' + UPKEEP + 'you may return target creature card from your graveyard to the battlefield. '
     'That creature gains haste until end of turn. Exile it at the beginning of the next end step.'):
        ('dawn', '{2}{B}{B}{B}', 'Enchantment', (), None),
}
GRAVE = ('Grizzly Bears', 'Hill Giant', 'Reach Through Mists')
DECK = ('Island', 'Mountain')
WITNESSES = set(GRAVE + DECK) | {'Forest', 'Wastes', 'Swamp', 'Bayou', 'Darksteel Relic', 'Silent Gravestone',
                               'Murder', 'Counterspell', 'Naturalize', 'Unsummon'}
HANDLED = {'Murder', 'Counterspell', 'Naturalize', 'Unsummon', 'Silent Gravestone'}
COMMON = ('effect', 'alternate', 'opponent_grave', 'shroud', 'replay', 'repeat')
EXTRA = {
    'genesis': ('decline', 'short', 'wrong_color', 'opposing_funds', 'outside_grave', 'cast_then_dies', 'self_return', 'opponent_upkeep'),
    'siege': ('empty', 'opponent_upkeep', 'dragons', 'dragons_repeat', 'dragons_empty', 'dragons_opponent', 'countered', 'underpaid'),
    'sheoldred': ('empty', 'opponent_upkeep', 'sacrifice_alt', 'artifact_only', 'walk_swamp', 'walk_dual', 'walk_open', 'countered', 'underpaid', 'cross_owned'),
    'dawn': ('empty', 'opponent_upkeep', 'decline', 'haste_exile', 'source_gone', 'target_bounced', 'countered', 'underpaid', 'cross_owned'),
}
DAWN_RETURN = ('haste_exile', 'source_gone', 'target_bounced')
WALK = ('walk_swamp', 'walk_dual', 'walk_open')


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    text = record['oracle_text']
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or text not in PATTERNS:
        raise ValueError('supported normal whole-Oracle grave return required')
    model, fee, card_type, keywords, stats = PATTERNS[text]
    if (record['mana_cost'], record['type_line'], tuple(record['keywords']), (record.get('power'), record.get('toughness'))) != (
            fee, card_type, keywords, tuple(str(n) for n in stats) if stats else (None, None)):
        raise ValueError('exact printed payment, type, keywords and stats required')
    return dict(model=model, cost=cost_from_oracle(record),
                destination='hand' if model in ('genesis', 'siege') else 'battlefield')


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + EXTRA[params['model']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('exact pinned whole Oracle and typed grave-return fixture required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed grave-return variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('complete grave-return source and witness inventory required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(oracle[name], expected) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected live production action for {name}')


class ReturnModes(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.owner
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model = params['model']
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.replay = self.loyalty = None
        self.dawn_returns = []
        self.producers = []
        if self.model == 'genesis' and self.mode not in ('outside_grave', 'cast_then_dies'):
            source_grave = self.enemy if self.mode == 'opponent_grave' else self.owner
            self.add(name, 'grave_source', source_grave, 'graveyard')
        else:
            self.add(name, 'source', self.owner, 'battlefield' if self.mode == 'cross_owned' else 'hand')
            if self.mode == 'cross_owned':
                self.setup[-1]['state'].append('opponent_owned')
        self.setup_witnesses()
        for who in (0, 1):
            for i, card in enumerate(DECK):
                self.add(card, f'lib_{who}_{i}', who, 'library')
        self.library_ids = set(DECK) | set(self.zones['graveyard'][0] + self.zones['graveyard'][1])

    def setup_witnesses(self):
        mode = self.mode
        if mode not in ('empty', 'dragons_empty', *WALK):
            grave_owner = self.enemy if mode == 'opponent_grave' else self.owner
            for i, card in enumerate(GRAVE):
                self.add(card, f'grave_{i}', grave_owner, 'graveyard')
            if mode != 'opponent_grave':
                self.add(GRAVE[0], 'other_grave', self.enemy, 'graveyard')
        if mode == 'shroud':
            self.add('Silent Gravestone', 'shroud', self.enemy)
        if self.model == 'genesis':
            budget_owner = self.enemy if mode == 'opposing_funds' else self.owner
            for i in range(2 if mode == 'short' else 3):
                ref = f'producer_{i}'
                self.add('Wastes' if mode == 'wrong_color' else 'Forest', ref, budget_owner)
                self.producers.append(ref)
        if self.model == 'sheoldred':
            self.add('Darksteel Relic', 'noncreature', self.enemy)
            if mode in WALK:
                self.add('Grizzly Bears', 'blocker', self.enemy, 'hand')
                if mode != 'walk_open':
                    self.add('Bayou' if mode == 'walk_dual' else 'Swamp', 'defending_swamp', self.enemy)
            elif mode != 'artifact_only':
                if self.enemy == 0:
                    self.add('Grizzly Bears', 'sacrifice', self.enemy)
                    self.add('Hill Giant', 'alternative', self.enemy)
                else:
                    self.add('Hill Giant' if mode == 'sacrifice_alt' else 'Grizzly Bears', 'sacrifice', self.enemy)
        witness = {'cast_then_dies': 'Murder', 'countered': 'Counterspell', 'source_gone': 'Naturalize',
                   'target_bounced': 'Unsummon'}.get(mode)
        if witness:
            self.add(witness, 'reply', self.enemy, 'hand')

    def deploy(self):
        mode = self.mode
        if 'source' not in self.entities or mode == 'cross_owned':
            return True
        budget = list(self.params['cost'])
        if mode == 'underpaid':
            budget[0] -= 1
        self.start_cast('source', budget=budget, denied=mode == 'underpaid')
        if mode == 'underpaid':
            self.receipt()
            return False
        if mode == 'countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('reply', response=True)
            self.emit('resolve_response')
            self.move('reply', 'graveyard')
            self.move('source', 'graveyard')
            self.receipt()
            return False
        if self.model == 'siege':
            self.emit('choose_dialog', player=self.owner, value=int(mode.startswith('dragons')))
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.receipt()
        if mode == 'cast_then_dies':
            self.cast_removal('source', 'graveyard')
        return True

    def cast_removal(self, target, destination):
        self.start_cast('reply', target=target)
        self.emit('resolve')
        self.move('reply', 'graveyard')
        self.move(target, destination)
        self.receipt()

    def choose_tutor(self, *, decline=False):
        grave = self.zones['graveyard'][self.owner]
        choices = [name for name in grave if self.is_creature(name)]
        if not choices or self.mode == 'shroud':
            return None, None
        if self.mode == 'self_return':
            chosen = self.name
        else:
            chosen = GRAVE[1] if self.mode == 'alternate' else GRAVE[0]
            if chosen not in choices:
                chosen = choices[0]
        self.emit('choose_optional_zone' if decline else 'choose_zone_card', player=self.owner, value=self.rows[chosen]['csvid'])
        return (None if decline else chosen), list(grave)

    def is_creature(self, name):
        return 'Creature' in self.oracle[name]['type_line'].partition(' — ')[0].split()

    def genesis_plan(self):
        available = [ref for ref in self.producers if self.entities[ref]['player'] == self.owner
                     and self.entities[ref]['name'] == 'Forest' and not self.entities[ref]['tapped']]
        if self.name not in self.zones['graveyard'][self.owner] or len(available) < 3 or self.mode == 'shroud':
            return None, None
        pay = self.mode != 'decline'
        self.emit('choose_dialog', player=self.owner, value=int(not pay))
        if not pay:
            return None, None
        for ref in available[:3]:
            if self.owner == 0:
                self.emit('choose_mana_source', ref=ref)
            self.entities[ref]['tapped'] = True
        return self.choose_tutor()

    def sacrifice_plan(self):
        choices = [ref for ref in ('sacrifice', 'alternative') if ref in self.entities
                   and self.entities[ref]['zone'] == 'battlefield']
        if not choices:
            return None
        target = choices[-1] if self.mode == 'sacrifice_alt' else choices[0]
        if self.enemy == 0:
            self.emit('choose_target', player=self.enemy, ref=target)
        return target

    def next_turn(self):
        self.emit('advance_turn')
        self.settle_dawn_expiry()
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['player'] == self.active and entity['zone'] == 'battlefield':
                entity['tapped'] = False

    def retain_return(self, chosen):
        destination = self.params['destination']
        self.zones['graveyard'][self.owner].remove(chosen)
        self.zones[destination][self.owner].append(chosen)
        if self.model == 'dawn':
            self.dawn_returns.append(chosen)
        if self.model == 'dawn' and self.mode in DAWN_RETURN:
            record = self.oracle[chosen]
            self.emit('bind_permanent_copy', ref='returned', player=self.owner, value=self.rows[chosen]['csvid'], index=0)
            self.entities['returned'] = dict(name=chosen, player=self.owner, zone='battlefield',
                stats=(int(record['power']), int(record['toughness'])), tapped=False, damage=0)

    def upkeep(self, *, capture=False):
        self.next_turn()
        if capture:
            self.replay = len(self.steps)
        chosen, offered, sacrificed = None, None, None
        if self.active == self.owner:
            if self.model == 'genesis':
                chosen, offered = self.genesis_plan()
            elif self.model == 'siege' and self.mode.startswith('dragons'):
                self.life[self.owner] += 2
                self.life[self.enemy] -= 2
            else:
                if self.model == 'dawn':
                    self.life[self.owner] -= 1
                chosen, offered = self.choose_tutor(decline=self.mode == 'decline')
        elif self.model == 'sheoldred':
            sacrificed = self.sacrifice_plan()
        self.emit('upkeep_step', player=self.active)
        if sacrificed:
            self.move(sacrificed, 'graveyard')
        if offered is not None:
            self.emit('assert_offered_count', value=len(offered))
            for name in sorted(set(offered)):
                self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(self.is_creature(name)))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        if chosen:
            self.retain_return(chosen)
        self.receipt()
        self.emit('turn', player=self.active, value=0)

    def fight(self, ref, blocker=None, *, blocks=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=1)
        self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=ref, value=int(blocks))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        attacker = self.entities[ref]
        if blocks:
            other = self.entities[blocker]
            attacker['damage'] = other['stats'][0]
            self.move(blocker, 'graveyard')
        else:
            self.life[self.enemy] -= attacker['stats'][0]
        self.receipt()

    def dawn_lifecycle(self):
        if self.mode == 'source_gone':
            self.cast_removal('source', 'graveyard')
        elif self.mode == 'target_bounced':
            self.cast_removal('returned', 'hand')
        if self.mode != 'target_bounced':
            self.fight('returned')
        self.emit('cleanup')
        self.settle_dawn_expiry()
        self.receipt()

    def play(self):
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        self.fund(self.owner, [0] * 6)
        if self.model == 'genesis':
            self.emit('prepare_graveyard_abilities')
        self.receipt()
        if not self.deploy():
            return
        self.upkeep()
        if self.mode in ('opponent_upkeep', 'dragons_opponent'):
            return
        if self.mode in WALK:
            self.start_cast('blocker')
            self.emit('resolve')
            self.move('blocker', 'battlefield')
            self.receipt()
        self.upkeep(capture=self.mode == 'replay')
        if self.mode in ('repeat', 'dragons_repeat'):
            self.upkeep()
            self.upkeep()
        if self.mode in WALK:
            self.fight('source', 'blocker', blocks=self.mode == 'walk_open')
        if self.model == 'dawn':
            if self.mode in DAWN_RETURN:
                self.dawn_lifecycle()
            else:
                self.expire_dawn_returns()

    def expire_dawn_returns(self):
        # Ordinary Dawn cases also observe the actual next end step. Each
        # returning body is gone before any subsequent same-family upkeep.
        self.emit('cleanup')
        self.settle_dawn_expiry()
        self.receipt()

    def settle_dawn_expiry(self):
        for name in self.dawn_returns:
            if 'returned' in self.entities:
                if self.entities['returned']['zone'] == 'battlefield':
                    self.move('returned', 'exile')
            else:
                self.zones['battlefield'][self.owner].remove(name)
                self.zones['exile'][self.owner].append(name)
        self.dawn_returns.clear()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved grave-return source')
    cases = []
    for variant in selected:
        board = ReturnModes(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The actual upkeep wrapper resolves its trigger callbacks as one step. Separate responses after the grave target choice need a narrower priority boundary.']
        if params['model'] == 'sheoldred':
            gaps.append('A unique legal AI sacrifice isolates the forced choice while the human has two distinct legal choices.')
        cases.append(dict(name=f'GraveyardReturnCostModesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
