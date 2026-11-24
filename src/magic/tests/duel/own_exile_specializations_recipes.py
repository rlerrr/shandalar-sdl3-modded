"""Whole-Oracle owner-exile modes with actual paid and free play."""

import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from play_own_exile_effects_recipes import ExileBoard


FAMILY = 'own_exile_specializations'
PROFILE = Path(__file__).parent / 'data/own_exile_specializations_wave8_profiles.json'
SPELL = 'Reach Through Mists'
DECK = ('Forest', SPELL, 'Grizzly Bears', 'Island', 'Plains', 'Hill Giant', 'Walking Corpse', 'Swamp', 'Mountain')
WITNESSES = set(DECK) | {'Act on Impulse', 'Air Elemental', 'Counterspell', 'Unsummon'}
HANDLED = {SPELL, 'Act on Impulse', 'Counterspell', 'Unsummon'}
PATTERNS = {
    ('Sorcery', '{7}{R}{R}{R}', 'Exile the top seven cards of your library. Until end of turn, you may cast spells from among them.\n'
     'If this spell was cast from your hand, add ten mana of any one color.'): 'apex',
    ('Sorcery', '{R}', "Choose target creature. Exile the top card of your library. You may have Spark of Creativity deal damage to that creature equal to the exiled card's mana value. If you don't, you may play that card until end of turn."): 'spark',
    ('Artifact', '{4}', '{2}, {T}: Exile the top card of your library. Until end of turn, you may play that card. Put a brick counter on this artifact.\n'
     '{T}: Exile the top card of your library. Until end of turn, you may play that card without paying its mana cost. Activate only if there are three or more brick counters on this artifact.'): 'vault',
}
SPELL_COMMON = ('empty', 'wrong_source_mana', 'countered', 'expiry')
MODES = {
    'apex': (*SPELL_COMMON, 'full_prefix', 'shallow', 'paid_spell_blue', 'wrong_spell_color', 'lands_forbidden',
             'wrong_seat', 'from_exile', 'replay', 'black', 'green', 'white'),
    'spark': (*SPELL_COMMON, 'damage', 'lethal', 'zero_damage', 'own_target', 'removed_target', 'no_target',
              'play_land', 'paid_spell', 'wrong_spell_color', 'wrong_seat', 'spent_land_drop', 'replay'),
    'vault': ('paid_spell', 'paid_land', 'wrong_spell_color', 'wrong_seat', 'affordability', 'tapped', 'empty',
              'expiry', 'free_funded', 'free_zero', 'free_land', 'paid_at_three', 'empty_third', 'replay'),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    key = record['type_line'], record.get('mana_cost'), record['oracle_text']
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['keywords'] or key not in PATTERNS):
        raise ValueError('supported whole-Oracle specialized exile grant required')
    model = PATTERNS[key]
    settings = {'apex': dict(count=7, mana=10), 'spark': dict(count=1),
                'vault': dict(count=1, paid_fee=2, brick_required=3)}
    return dict(model=model, cost=cost_from_oracle(record), **settings[model])


def variants(params):
    return [f'{mode}_{who}' for who in (0, 1) for mode in MODES[params['model']]]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), params)
            or not exact(profile()['sources'].get(record['name']), record)):
        raise ValueError('complete pinned Oracle and typed own-exile specialization required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct supported own-exile variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('exact source and witness inventory required')
    for name, expected in records.items():
        row = rows[name]
        if not exact(oracle[name], expected) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('0', 'NULL', None, 'card_dummy'):
            raise ValueError(f'live production handler required for {name}')
    if (oracle[SPELL]['oracle_text'], oracle[SPELL]['mana_cost']) != ('Draw a card.', '{U}'):
        raise ValueError('exiled spell requires a real one-blue draw witness')


class Specialization(ExileBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model, self.active = params['model'], self.owner
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.loyalty = self.replay = None
        mode = self.mode
        if mode == 'from_exile':
            self.add('Act on Impulse', 'portal', self.owner, 'hand')
        else:
            seed_tapped = mode == 'tapped'
            self.add(name, 'source', self.owner, 'battlefield' if seed_tapped else 'hand',
                     state='tapped' if seed_tapped else 'untapped')
        if mode == 'countered':
            self.add('Counterspell', 'reply', self.enemy, 'hand')
        if mode == 'removed_target':
            self.add('Unsummon', 'reply', self.enemy, 'hand')
        if self.model == 'spark' and mode != 'no_target':
            self.add('Hill Giant', 'target', self.owner if mode == 'own_target' else self.enemy)
        if mode == 'spent_land_drop':
            self.add('Mountain', 'land_drop', self.owner, 'hand')
        suffix = list(DECK)
        if self.model == 'apex':
            if mode == 'shallow':
                suffix = suffix[:2]
            elif mode == 'from_exile':
                suffix = [name, 'Swamp', 'Mountain'] + suffix[:7] + ['Air Elemental']
        elif self.model == 'spark':
            top = {'damage': 'Grizzly Bears', 'own_target': 'Grizzly Bears', 'lethal': 'Air Elemental',
                   'paid_spell': SPELL, 'wrong_spell_color': SPELL, 'wrong_seat': SPELL}.get(mode, 'Forest')
            suffix = [top] + [n for n in suffix if n != top]
        else:
            if mode in ('free_funded', 'free_zero', 'paid_at_three', 'replay'):
                suffix = ['Forest', 'Island', 'Plains', SPELL, 'Hill Giant']
            elif mode == 'free_land':
                suffix = ['Island', 'Plains', 'Swamp', 'Forest', 'Hill Giant']
            elif mode == 'empty_third':
                suffix = ['Forest', 'Island']
            elif mode in ('paid_spell', 'wrong_spell_color', 'wrong_seat'):
                suffix = [SPELL] + [n for n in suffix if n != SPELL]
        if mode == 'empty':
            suffix = []
        for i, item in enumerate(suffix):
            self.add(item, f'deck_{i}', self.owner, 'library')
        self.library_ids = set(suffix)

    def play_witness(self, name, allow, *, free=False, actor=None):
        who = self.owner if actor is None else actor
        index = self.rows[name]['csvid']
        self.emit('can_play_exiled', player=who, index=index, value=int(allow))
        self.emit('play_exiled', player=who, index=index, value=int(allow))
        if allow:
            self.zones['exile'][who].remove(name)
            is_spell = name == SPELL
            self.zones['graveyard' if is_spell else 'battlefield'][who].append(name)
            if is_spell:
                if not free:
                    self.mana[who][2] -= 1
                self.draw(who)
            self.emit('resolve')
            self.emit('refresh')
        self.receipt()

    def finish_countered(self):
        self.emit('announce_spell', ref='source')
        self.start_cast('reply', response=True)
        self.emit('resolve_response')
        self.move('reply', 'graveyard')
        self.move('source', 'graveyard')
        self.receipt()

    def expiry(self, item):
        index = self.rows[item]['csvid']
        self.emit('can_play_exiled', player=self.owner, index=index, value=1)
        self.emit('cleanup')
        self.emit('turn', player=self.owner, value=0)
        self.play_witness(item, False)

    def apex(self):
        mode = self.mode
        color = {'black': 1, 'green': 3, 'wrong_spell_color': 4, 'white': 5}.get(mode, 2)
        if mode == 'from_exile':
            self.start_cast('portal')
            self.emit('resolve')
            self.move('portal', 'graveyard')
            self.grant(3)
            self.receipt()
            self.fund(self.owner, self.params['cost'])
            self.emit('can_play_exiled', player=self.owner, index=self.csvid, value=1)
        else:
            denied = mode == 'wrong_source_mana'
            self.start_cast('source', budget=[sum(self.params['cost']), 0, 0, 0, 0, 0] if denied else None, denied=denied)
            if denied:
                self.receipt()
                return
            if mode == 'countered':
                self.finish_countered()
                return
        display = self.zones['library'][self.owner][:self.params['count']]
        if display:
            self.emit('choose_zone', player=self.owner, value=-1)
        self.emit('choose_color', player=self.owner, value=color)
        if mode == 'from_exile':
            self.emit('play_exiled', player=self.owner, index=self.csvid, value=1)
            # A real color is available if the engine wrongly treats the nested
            # cast as from hand. The Oracle-correct path leaves this one intent.
            self.emit('assert_unused_choices', value=1)
            # The permission has cast and resolved Apex inside its activation.
            # Retire the outer activation before the next permission query.
            self.emit('resolve')
            self.zones['exile'][self.owner].remove(self.name)
            self.zones['graveyard'][self.owner].append(self.name)
            self.mana[self.owner] = [0] * 6
        else:
            self.emit('resolve')
            self.move('source', 'graveyard')
            self.mana[self.owner][color] = self.params['mana']
        self.grant(self.params['count'])
        if display:
            self.visibility(display)
        self.receipt()
        if mode == 'lands_forbidden':
            for name in ('Forest', 'Island', 'Plains'):
                self.play_witness(name, False)
        if mode == 'wrong_seat':
            self.fund(self.enemy, [0, 0, 1, 0, 0, 0])
            self.play_witness(SPELL, False, actor=self.enemy)
        if mode in ('paid_spell_blue', 'replay', 'wrong_seat', 'wrong_spell_color'):
            if mode == 'replay':
                self.replay = len(self.steps)
            self.play_witness(SPELL, mode != 'wrong_spell_color')
        if mode == 'expiry':
            self.expiry(SPELL)
        if mode == 'from_exile':
            self.play_witness('Forest', False)
            self.fund(self.owner, [0, 0, 1, 0, 0, 0])
            self.play_witness(SPELL, True)

    def spark(self):
        from recipes import cost_from_oracle

        mode = self.mode
        denied = mode in ('wrong_source_mana', 'no_target')
        self.start_cast('source', target=None if denied else 'target', denied=denied,
                        budget=[1, 0, 0, 0, 0, 0] if mode == 'wrong_source_mana' else None)
        if denied:
            self.receipt()
            return
        if mode == 'countered':
            self.finish_countered()
            return
        if mode == 'removed_target':
            self.emit('announce_spell', ref='source')
            self.start_cast('reply', target='target', response=True)
            self.emit('resolve_response')
            self.move('reply', 'graveyard')
            self.move('target', 'hand')
            self.move('source', 'graveyard')
            self.receipt()
            return
        damage = mode in ('damage', 'lethal', 'zero_damage', 'own_target')
        library = self.zones['library'][self.owner]
        item = library[0] if library else None
        if item:
            self.emit('choose_dialog', player=self.owner, value=0)  # Acknowledge reveal.
            self.emit('choose_dialog', player=self.owner, value=int(not damage))
        self.emit('resolve')
        self.move('source', 'graveyard')
        if item:
            self.grant(1)
        if damage:
            victim = self.entities['target']
            victim['damage'] = int(self.oracle[item]['cmc'])
            if victim['damage'] >= victim['stats'][1]:
                self.move('target', 'graveyard')
        self.receipt()
        if not item:
            return
        if mode == 'expiry':
            self.expiry(item)
        else:
            if mode == 'replay':
                self.replay = len(self.steps)
            if mode in ('paid_spell', 'wrong_spell_color', 'wrong_seat'):
                self.fund(self.owner, [1, 0, 0, 0, 0, 0] if mode == 'wrong_spell_color' else [0, 0, 1, 0, 0, 0])
            if mode == 'wrong_seat':
                self.fund(self.enemy, [0, 0, 1, 0, 0, 0])
                self.play_witness(item, False, actor=self.enemy)
            if mode == 'spent_land_drop':
                self.emit('play_land', ref='land_drop', value=1)
                self.move('land_drop', 'battlefield')
            if damage and item != 'Forest':
                self.fund(self.owner, cost_from_oracle(self.oracle[item]))
            self.play_witness(item, not (damage or mode in ('spent_land_drop', 'wrong_spell_color')))

    def vault_activation(self, *, paid, budget, offer=False):
        self.fund(self.owner, [budget, 0, 0, 0, 0, 0])
        self.emit('can_activate', ref='source', value=1)
        if offer:
            self.emit('choose_dialog', player=self.owner, value=int(not paid))
        self.emit('activate', ref='source', value=1)
        if offer:
            self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.entities['source']['tapped'] = True
        self.observe('source', 'tapped', 1)
        self.mana[self.owner][0] -= self.params['paid_fee'] if paid else 0
        self.pools()
        self.emit('resolve')
        exiled = self.grant(1)
        self.receipt()
        return exiled

    def vault(self):
        mode = self.mode
        fee = self.params['paid_fee']
        if mode == 'tapped':
            self.fund(self.owner, [fee, 0, 0, 0, 0, 0])
        else:
            self.start_cast('source')
            self.emit('resolve')
            self.move('source', 'battlefield')
        if mode in ('tapped', 'affordability'):
            if mode == 'affordability':
                self.fund(self.owner, [fee - 1, 0, 0, 0, 0, 0])
            self.emit('can_activate', ref='source', value=0)
            self.emit('activate', ref='source', value=0)
            self.receipt()
            return
        self.receipt()
        advanced = mode in ('free_funded', 'free_zero', 'free_land', 'paid_at_three', 'empty_third', 'replay')
        if advanced:
            for _ in range(self.params['brick_required']):
                self.emit('can_activate', ref='source', value=0)
                self.vault_activation(paid=True, budget=fee, offer=self.owner == 1)
                self.advance()
                self.advance()
            if mode == 'replay':
                self.replay = len(self.steps)
            paid = mode == 'paid_at_three'
            budget = 0 if mode == 'free_zero' else fee
            names = self.vault_activation(paid=paid, budget=budget, offer=self.owner == 1 or budget == fee)
            if names:
                if paid:
                    self.play_witness(names[0], False)
                    self.fund(self.owner, [0, 0, 1, 0, 0, 0])
                self.play_witness(names[0], True, free=not paid)
            return
        names = self.vault_activation(paid=True, budget=fee, offer=self.owner == 1)
        self.emit('can_activate', ref='source', value=0)
        if not names:
            return
        item = names[0]
        if mode == 'expiry':
            self.expiry(item)
            return
        if mode == 'wrong_seat':
            self.fund(self.enemy, [0, 0, 1, 0, 0, 0])
            self.play_witness(item, False, actor=self.enemy)
        if mode in ('paid_spell', 'wrong_spell_color', 'wrong_seat'):
            self.fund(self.owner, [1, 0, 0, 0, 0, 0] if mode == 'wrong_spell_color' else [0, 0, 1, 0, 0, 0])
        self.play_witness(item, mode != 'wrong_spell_color')

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.owner, [0] * 6)
        {'apex': self.apex, 'spark': self.spark, 'vault': self.vault}[self.model]()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved own-exile specialization')
    cases = []
    for variant in selected:
        board = Specialization(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        actions = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = actions[cut:]
            actions = actions[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The live exile observer requires a unique owner/card ID permission.']
        if params['model'] == 'vault':
            gaps.append('Actual repeated payments and the free mode threshold witness brick accumulation. The driver has no direct brick counter receipt.')
        cases.append(dict(name=f'OwnExileSpecializationsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=actions, _recipe=dict(family=FAMILY, version=2,
                            variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=gaps)))
    return cases
