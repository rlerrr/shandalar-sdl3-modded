"""Whole-card top-of-library exile grants and their real play permissions."""

import json
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'play_own_exile_effects'
PROFILE = Path(__file__).parent / 'data/play_own_exile_effects_wave8_profiles.json'
TURN = 'Until end of turn, you may play '
NEXT = 'Until the end of your next turn, you may play '
PROWESS = 'Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)'
SPECTACLE = 'Spectacle {R} (You may cast this spell for its spectacle cost rather than its mana cost if an opponent lost life this turn.)'
SIEGE = ('As this enchantment enters, choose Khans or Dragons.\n'
         '• Khans — At the beginning of your upkeep, exile the top card of your library. '
         + TURN + 'that card.\n'
         '• Dragons — Whenever a creature you control leaves the battlefield, this enchantment deals 1 damage to any target.')
MODELS = {
    ('Sorcery', '{2}{R}', 'Exile the top three cards of your library. ' + TURN + 'those cards. '
     '(If you cast a spell this way, you still pay its costs. You can play a land this way only if you have an available land play remaining.)', ()): ('fixed', 3, False),
    ('Creature — Human Monk', '{1}{R}', PROWESS + '\nWhen this creature enters, exile the top card of your library. '
     + TURN + 'that card.', ('Prowess',)): ('prowess', 1, False),
    ('Instant', '{X}{R}{R}', 'Exile the top X cards of your library. ' + NEXT + 'those cards.', ()): ('variable', 2, True),
    ('Sorcery', '{2}{R}', SPECTACLE + '\nExile the top two cards of your library. ' + NEXT + 'those cards.', ('Spectacle',)): ('spectacle', 2, True),
    ('Enchantment', '{3}{R}', SIEGE, ()): ('siege', 1, False),
}

DECK = ('Forest', 'Island', 'Grizzly Bears', 'Plains', 'Hill Giant')
SPELL = 'Reach Through Mists'
WITNESSES = set(DECK) | {SPELL, 'Cloudshift', 'Counterspell', 'Spellbook', 'Jace Beleren', 'Mountain',
                        'Murder', 'Naturalize', 'Ornithopter', 'Searing Blaze', 'Shock', 'Swords to Plowshares', 'Unsummon'}
HANDLED = {SPELL, 'Cloudshift', 'Counterspell', 'Jace Beleren', 'Murder', 'Naturalize', 'Shock',
           'Spellbook', 'Swords to Plowshares', 'Unsummon'}
COMMON = ('grant', 'empty', 'play_land', 'play_spell', 'wrong_spell_mana', 'wrong_seat', 'wrong_card',
          'spent_land_drop', 'expiry', 'offturn_land', 'replay', 'source_wrong_mana', 'source_countered')
DRAGON = ('dragons_death', 'dragons_bounce', 'dragons_exile', 'dragons_creature', 'dragons_walker',
          'dragons_enemy', 'dragons_noncreature', 'dragons_upkeep')
DRAGON_PING = {'dragons_death', 'dragons_bounce', 'dragons_exile', 'dragons_creature', 'dragons_walker'}
EXTRA = {'fixed': ('shallow', 'second_land'),
         'prowess': ('own_cast', 'own_creature', 'enemy_cast', 'blink'),
         'variable': ('shallow', 'second_land', 'zero_x', 'one_x', 'offturn_grant'),
         'spectacle': ('shallow', 'second_land', 'spectacle', 'normal_when_eligible', 'spectacle_enabled', 'own_life_loss', 'old_life_loss'),
         'siege': ('khans_scope', 'khans_departure', *DRAGON)}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    key = record['type_line'], record.get('mana_cost'), record['oracle_text'], tuple(record['keywords'])
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or key not in MODELS:
        raise ValueError('supported whole-Oracle top exile grant required')
    model, amount, next_turn = MODELS[key]
    if model == 'prowess' and (record.get('power'), record.get('toughness')) != ('2', '1'):
        raise ValueError('the Prowess composition requires the printed 2/1 source')
    base = dict(record, mana_cost=record['mana_cost'].removeprefix('{X}'))
    return dict(model=model, count=amount, next_turn=next_turn, cost=cost_from_oracle(base))


def variants(params):
    modes = (*COMMON, *EXTRA[params['model']])
    if params['next_turn']:
        modes += ('next_turn_play', 'offturn_spell')
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(profile()['sources'].get(record['name']), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(params, declaration.get('params'))):
        raise ValueError('exact full Oracle and typed exile-grant recipe required')
    if (not isinstance(chosen, list) or not chosen or any(not isinstance(v, str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(params))):
        raise ValueError('distinct reviewed exile-grant variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    records = pins['sources'] | pins['witnesses']
    if set(pins['witnesses']) != WITNESSES or set(records) != set(pins['registry']):
        raise ValueError('complete exile-grant source and witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if not exact(oracle[name], record) or not exact(row, pins['registry'][name]) or row['kind'] != 'card':
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in pins['sources'].keys() | HANDLED and row['handler'] in ('card_dummy', '0', 'NULL', None):
            raise ValueError(f'expected production action handler for {name}')


class ExileBoard(Board):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.name, self.csvid, self.variant = name, csvid, variant
        self.declaration, self.params, self.rows, self.oracle = declaration, params, rows, oracle
        self.model = params['model']
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.loyalty = self.replay = None
        self.active = self.enemy if self.mode in ('offturn_grant', 'dragons_walker') else self.owner
        self.add(name, 'source', self.owner, 'hand')
        deck = list(DECK)
        if self.mode in ('play_spell', 'wrong_spell_mana', 'offturn_spell'):
            deck.insert(0, SPELL)
        if self.mode == 'shallow':
            deck = ['Forest']
        if self.mode == 'empty':
            deck = []
        for player, pile in ((self.owner, deck), (self.enemy, DECK[-2:])):
            for i, card in enumerate(pile):
                self.add(card, f'library_{player}_{i}', player, 'library')
        if self.mode == 'spent_land_drop':
            self.add('Mountain', 'land_drop', self.owner, 'hand')
        if self.mode == 'wrong_card':
            self.add('Mountain', 'other_exile', self.owner, 'exile')
        if self.mode == 'source_countered':
            self.add('Counterspell', 'counter', self.enemy, 'hand')
        if self.mode in ('spectacle', 'normal_when_eligible', 'spectacle_enabled', 'own_life_loss', 'old_life_loss'):
            self.add('Shock', 'shock', self.owner, 'hand')
        if self.mode == 'spectacle_enabled':
            # Real registered card in the deck activates the same production
            # Rules Engine that tracks Shock's actual opponent life loss.
            self.add('Searing Blaze', 'trap_registration', self.enemy, 'hand')
        if self.mode in ('own_cast', 'enemy_cast', 'own_creature', 'blink'):
            witness = {'own_creature': 'Grizzly Bears', 'blink': 'Cloudshift'}.get(self.mode, SPELL)
            self.add(witness, 'witness', self.enemy if self.mode == 'enemy_cast' else self.owner, 'hand')
        if self.mode in DRAGON or self.mode == 'khans_departure':
            if self.mode != 'dragons_upkeep':
                victim = 'Spellbook' if self.mode == 'dragons_noncreature' else 'Grizzly Bears'
                victim_owner = self.enemy if self.mode == 'dragons_enemy' else self.owner
                self.add(victim, 'victim', victim_owner)
                removal = {'dragons_bounce': 'Unsummon', 'dragons_exile': 'Swords to Plowshares',
                           'dragons_noncreature': 'Naturalize'}.get(self.mode, 'Murder')
                self.add(removal, 'removal', 1 - victim_owner, 'hand')
            if self.mode == 'dragons_creature':
                self.add('Hill Giant', 'recipient', self.enemy)
            elif self.mode == 'dragons_walker':
                self.add('Jace Beleren', 'walker', self.enemy, 'hand')
        if self.mode in DRAGON_PING:
            self.add('Ornithopter', 'damage_boundary', self.owner)
        self.library_ids = set(deck) | set(DECK[-2:]) | ({'Mountain'} if self.mode == 'wrong_card' else set())

    def receipt(self):
        super().receipt()
        names = self.library_ids - {entity['name'] for entity in self.entities.values()}
        for zone in ('hand', 'battlefield', 'graveyard', 'exile'):
            for who, cards in enumerate(self.zones[zone]):
                counts = Counter(cards)
                for name in sorted(names):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def advance(self, *, upkeep=False):
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        for entity in self.entities.values():
            entity['damage'] = 0
            if entity['zone'] == 'battlefield' and entity['player'] == self.active:
                entity['tapped'] = False
        if self.model == 'prowess' and self.entities['source']['zone'] == 'battlefield':
            self.entities['source']['stats'] = (2, 1)
        if upkeep:
            self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active, value=0)

    def start_cast(self, ref, *, fee=None, budget=None, target=None, player_target=None, x=None, dialog=None, denied=False, response=False):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        actor = entity['player']
        fee = list(fee if fee is not None else cost_from_oracle(self.oracle[entity['name']]))
        self.fund(actor, fee if budget is None else budget)
        if x is not None:
            self.emit('choose_x', player=actor, value=x)
        if dialog is not None:
            self.emit('choose_dialog', player=actor, value=dialog)
        if target is not None:
            self.emit('choose_target', player=actor, ref=target)
        if player_target is not None:
            self.emit('choose_player', player=actor, value=player_target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(not denied))
        self.observe(ref, 'in_hand', denied)
        self.emit('assert_stack', value=0 if denied else 2 if response else 1)
        if not denied:
            self.mana[actor] = [amount - cost for amount, cost in zip(self.mana[actor], fee)]
        self.pools()

    def visibility(self, names):
        self.emit('assert_offered_count', value=len(names))
        for i, name in enumerate(names):
            self.emit('assert_offered_card', index=i, value=self.rows[name]['csvid'])

    def grant(self, amount):
        library = self.zones['library'][self.owner]
        names = library[:amount]
        del library[:amount]
        self.zones['exile'][self.owner].extend(names)
        return names

    def draw(self, who):
        self.zones['hand'][who].append(self.zones['library'][who].pop(0))

    def prowess(self):
        entity = self.entities['source']
        entity['stats'] = tuple(value + 1 for value in entity['stats'])

    def permission(self, name, allowed, *, actor=None, use=True):
        who = self.owner if actor is None else actor
        identity = self.rows[name]['csvid']
        self.emit('can_play_exiled', player=who, index=identity, value=int(allowed))
        if not use:
            return
        self.emit('play_exiled', player=who, index=identity, value=int(allowed))
        if allowed:
            spell = name == SPELL
            self.zones['exile'][who].remove(name)
            self.zones['graveyard' if spell else 'battlefield'][who].append(name)
            if spell:
                self.mana[who][2] -= 1
                self.draw(who)
                if self.model == 'prowess':
                    self.prowess()
            self.emit('resolve')
            self.emit('refresh')
        self.receipt()

    def start_source(self):
        mode, model = self.mode, self.model
        fee = list(self.params['cost'])
        x = None
        denied = mode in ('source_wrong_mana', 'own_life_loss', 'old_life_loss')
        if model == 'variable':
            fee[0] = {'zero_x': 0, 'one_x': 1, 'shallow': 3}.get(mode, self.params['count'])
            x = None if denied else fee[0]
        budget = [sum(fee), 0, 0, 0, 0, 0] if denied else list(fee)
        option = None
        if mode in ('spectacle', 'normal_when_eligible', 'spectacle_enabled', 'own_life_loss', 'old_life_loss'):
            victim = self.owner if mode == 'own_life_loss' else self.enemy
            self.start_cast('shock', player_target=victim)
            self.emit('resolve')
            self.move('shock', 'graveyard')
            self.life[victim] -= 2
            self.receipt()
            if mode == 'old_life_loss':
                self.advance()
                self.advance()
            if denied:
                budget = [0, 0, 0, 0, 1, 0]
            else:
                option = int(mode == 'normal_when_eligible')
                if mode in ('spectacle', 'spectacle_enabled'):
                    fee[0] = 0
        self.start_cast('source', fee=fee, budget=budget, x=x, dialog=option, denied=denied)
        if option is not None:
            # Report a missing Spectacle offer while leaving the separate real
            # informational zone acknowledgement available to the next callback.
            self.emit('assert_unused_choices', value=0)
        return denied, self.params['count'] if x is None else x

    def resolve_source(self, amount):
        mode, model = self.mode, self.model
        if mode == 'source_countered':
            self.emit('announce_spell', ref='source')
            self.start_cast('counter', response=True)
            self.emit('resolve_response')
            self.move('counter', 'graveyard')
            self.move('source', 'graveyard')
            return
        if model == 'siege':
            self.emit('choose_dialog', player=self.owner, value=int(mode in DRAGON))
        if model == 'spectacle' and self.zones['library'][self.owner]:
            self.emit('choose_zone', player=self.owner, value=-1)
        self.emit('resolve')
        self.move('source', 'battlefield' if model in ('siege', 'prowess') else 'graveyard')
        if model != 'siege':
            names = self.grant(amount)
            if model == 'spectacle' and names:
                self.visibility(names)

    def complete_damage_boundary(self):
        # The leave-play callback creates an actual damage packet. The scripted
        # spell drain may return before processing it. A legal unblocked attack
        # by a zero-power creature reaches normal prevention without adding damage.
        self.emit('assert_choices_consumed')
        self.emit('begin_attackers')
        self.emit('attack', ref='damage_boundary', value=1)
        self.entities['damage_boundary']['tapped'] = True
        for op in ('finish_attackers', 'begin_blockers', 'advance_combat', 'combat_damage', 'end_combat'):
            self.emit(op)

    def play_siege(self):
        mode = self.mode
        if mode == 'dragons_upkeep':
            self.advance(upkeep=True)
            self.advance(upkeep=True)
            self.receipt()
        elif mode in DRAGON or mode == 'khans_departure':
            self.start_cast('removal', target='victim')
            ping = mode in DRAGON_PING
            if mode in ('dragons_creature', 'dragons_walker'):
                self.emit('choose_target', player=self.owner, ref='walker' if mode == 'dragons_walker' else 'recipient')
            elif ping:
                self.emit('choose_player', player=self.owner, value=self.enemy)
            self.emit('resolve')
            self.move('removal', 'graveyard')
            self.move('victim', {'dragons_bounce': 'hand', 'dragons_exile': 'exile'}.get(mode, 'graveyard'))
            if ping:
                self.complete_damage_boundary()
            if mode == 'dragons_exile':
                self.life[self.owner] += 2
            if mode == 'dragons_creature':
                self.entities['recipient']['damage'] += 1
            elif mode == 'dragons_walker':
                self.loyalty -= 1
            elif ping:
                self.life[self.enemy] -= 1
            self.receipt()
        else:
            self.advance(upkeep=True)
            self.receipt()
            self.advance(upkeep=True)
            self.grant(1)
            self.receipt()
            self.play_grant()

    def play_prowess(self):
        mode = self.mode
        self.start_cast('witness', target='source' if mode == 'blink' else None)
        self.emit('resolve')
        self.move('witness', 'battlefield' if mode == 'own_creature' else 'graveyard')
        if mode in ('own_cast', 'enemy_cast'):
            self.draw(self.owner if mode == 'own_cast' else self.enemy)
        if mode == 'blink':
            self.emit('bind_permanent_copy', ref='source', player=self.owner, value=self.csvid, index=0)
            self.grant(1)
        elif mode == 'own_cast':
            self.prowess()
        self.receipt()
        if mode == 'own_cast':
            self.advance()
            self.receipt()

    def play_grant(self):
        mode = self.mode
        if mode in ('grant', 'shallow', 'empty', 'zero_x', 'one_x', 'offturn_grant', 'spectacle', 'normal_when_eligible', 'spectacle_enabled', 'khans_scope'):
            return
        if mode == 'replay':
            self.replay = len(self.steps)
        if mode == 'spent_land_drop':
            self.emit('play_land', ref='land_drop', value=1)
            self.move('land_drop', 'battlefield')
        if mode == 'wrong_card':
            self.permission('Mountain', False)
        if mode == 'wrong_seat':
            self.permission('Forest', False, actor=self.enemy)
        if mode == 'expiry':
            self.permission('Forest', True, use=False)
        if mode in ('expiry', 'next_turn_play', 'offturn_land', 'offturn_spell'):
            self.advance()
            if mode in ('expiry', 'next_turn_play'):
                self.advance()
            if mode == 'expiry' and self.params['next_turn']:
                self.permission('Forest', True, use=False)
                self.advance()
                self.advance()
        name = SPELL if mode in ('play_spell', 'offturn_spell', 'wrong_spell_mana') else 'Forest'
        if name == SPELL:
            self.fund(self.owner, [0, 0, 1, 0, 0, 0] if mode != 'wrong_spell_mana' else [0, 0, 0, 0, 1, 0])
        self.permission(name, mode not in ('expiry', 'spent_land_drop', 'offturn_land', 'wrong_spell_mana'))
        if mode == 'second_land':
            self.permission('Island', False)
        if mode == 'play_spell' and self.model == 'prowess':
            self.advance()
            self.receipt()

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.fund(self.owner, [0] * 6)
        if self.mode == 'dragons_walker':
            self.cast('walker')
            self.advance()
        denied, amount = self.start_source()
        if not denied:
            self.resolve_source(amount)
        self.receipt()
        if denied or self.mode == 'source_countered':
            return
        if self.model == 'siege':
            self.play_siege()
        elif self.model == 'prowess' and self.mode in EXTRA['prowess']:
            self.play_prowess()
        else:
            self.play_grant()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if profile()['registry'][name]['csvid'] != csvid:
        raise ValueError('unreserved own-exile grant source')
    cases = []
    for variant in selected:
        board = ExileBoard(name, csvid, declaration, params, variant, rows, oracle)
        board.play()
        steps = board.steps
        if board.replay is not None:
            cut = board.replay
            tail = steps[cut:]
            steps = steps[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = ['The permission observer requires a unique live owner/card ID match. Multiple active permissions with identical IDs are outside this fixture.']
        if params['model'] == 'spectacle':
            gaps.append('Alternative and normal payments are both tested after life loss with both payments available. A red-only Spectacle prompt is held for separate UI calibration.')
        cases.append(dict(name=f'PlayOwnExileEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=board.setup, steps=steps, _recipe=dict(family=FAMILY, version=2,
                            variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=gaps)))
    return cases
