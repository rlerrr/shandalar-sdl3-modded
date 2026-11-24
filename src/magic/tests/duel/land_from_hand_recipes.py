"""Whole cards that put an optional land from hand onto the battlefield."""
import json
import re
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'land_from_hand'
DATA = Path(__file__).parent / 'data/land_from_hand_wave8_profiles.json'
PLACE = '{T}: You may put a land card from your hand onto the battlefield.'
SORCERY = ' Activate only as a sorcery.'
SPELL_PLACE = PLACE.removeprefix('{T}: ')
SPELLS = {
    ('Instant', 'Draw a card. ' + SPELL_PLACE): 'draw',
    ('Instant', 'Target creature gets +2/+2 until end of turn. ' + SPELL_PLACE): 'pump',
    ('Sorcery', 'Destroy target artifact or enchantment. ' + SPELL_PLACE): 'destroy',
}
TRIGGERS = {
    ('Enchantment', 'Whenever an opponent plays a land, you may put a land card from your hand onto the battlefield.'):
        'opponent_land',
    ('Creature — Spirit',
     'Whenever you cast a Spirit or Arcane spell, you may put a land card from your hand onto the battlefield tapped.'):
        'spirit_or_arcane',
}
WITNESSES = {'Forest', 'Mountain', 'Island', 'Darksteel Citadel', 'Kabira Crossroads',
             'Shock', 'Grizzly Bears', 'Fog', 'Boomerang', 'Control Magic', 'Counterspell',
             'Fervor', 'Spellbook', 'Plains', 'Sakura-Tribe Scout', 'Wandering Ones', 'Reach Through Mists'}
VANILLA_WITNESSES = {'Grizzly Bears', 'Wandering Ones'}
MODES = ('cast_short', 'fresh_then_ready', 'choose_basic', 'choose_artifact', 'decline', 'empty', 'nonland_only',
         'tapped_entry', 'immediate_mana', 'before_normal', 'after_normal', 'next_turn_slot', 'opposing_turn',
         'combat', 'second_main', 'stack_busy', 'source_leaves_pending', 'foreign_controller', 'pending_replay')
SPELL_MODES = ('choose_land', 'decline', 'nonland_only', 'tapped_entry', 'before_normal', 'after_normal',
               'cast_short', 'wrong_color', 'countered', 'pending_replay')
SPELL_EXTRA = {
    'draw': ('drawn_land', 'drawn_declined', 'opposing_turn'),
    'pump': ('empty', 'foe_target', 'target_gone', 'no_legal_target', 'opposing_turn'),
    'destroy': ('empty', 'target_enchantment', 'own_target', 'target_indestructible', 'target_gone', 'no_legal_target'),
}
TRIGGER_MODES = {
    'opponent_land': ('source_short', 'opponent_play', 'decline', 'empty', 'nonland_only', 'own_play',
                      'opponent_put', 'opponent_second_denied', 'tapped_entry', 'ready_replay'),
    'spirit_or_arcane': ('source_short', 'spirit', 'arcane', 'decline', 'empty_spirit', 'arcane_draw_no_land',
                        'nonland_only', 'enemy_spirit', 'non_spirit', 'non_arcane', 'cast_countered',
                        'after_normal', 'before_normal', 'artifact_land', 'tapped_entry', 'ready_replay'),
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('keywords')):
        raise ValueError('normal whole card with reviewed land placement required')
    text = record['oracle_text']
    trigger = TRIGGERS.get((record['type_line'], text))
    if trigger:
        model = dict(cast=cost_from_oracle(record), trigger=trigger)
        if trigger == 'spirit_or_arcane':
            if any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness')):
                raise ValueError('complete numeric Spirit source required')
            model.update(artifact=False, power=int(record['power']), toughness=int(record['toughness']))
        return model
    spell = SPELLS.get((record['type_line'], text))
    if spell:
        mana = cost_from_oracle(record)
        if not any(mana[1:]) or all(mana[1:]):
            raise ValueError('reviewed ordinary spell with an independent wrong-color budget required')
        return dict(cast=mana, spell=spell)
    if (not record.get('type_line', '').startswith(('Creature — ', 'Artifact Creature — '))
            or any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness'))):
        raise ValueError('normal numeric creature with only the reviewed tap ability required')
    if text not in (PLACE, PLACE + SORCERY):
        raise ValueError('entire optional land-placement ability and timing must match Oracle')
    return dict(cast=cost_from_oracle(record), artifact=record['type_line'].startswith('Artifact Creature — '),
                sorcery=text.endswith(SORCERY), power=int(record['power']), toughness=int(record['toughness']))


def spec(declaration, record):
    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(model, params['mechanism'])):
        raise ValueError('complete independently pinned land-placement record and declaration required')
    return dict(model, family=FAMILY, csvid=source['csvid'])


def variants(model):
    if model.get('trigger'):
        return [f'{mode}_{owner}' for owner in (0, 1) for mode in TRIGGER_MODES[model['trigger']]] + ['trigger_decline_0']
    spell = model.get('spell')
    modes = SPELL_MODES + SPELL_EXTRA[spell] if spell else MODES
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    profile = pins()
    if set(profile) != {'sources', 'witnesses'} or set(profile['witnesses']) != WITNESSES:
        raise ValueError('exact land-placement source and witness pins required')
    for group, records in profile.items():
        for name, pin in records.items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row.get('handler') in (None, 'NULL', '0') or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented whole land-placement card changed: ' + name)
            if row['handler'] == 'card_dummy' and (
                    group != 'witnesses' or name not in VANILLA_WITNESSES or oracle[name]['oracle_text']):
                raise ValueError('only the exact vanilla combat witness uses the generic card callback')


class LandCase:
    def __init__(self, model, owner, active, rows, oracle):
        self.model, self.owner, self.active = model, owner, active
        self.rows, self.oracle = rows, oracle
        self.setup, self.actions = [], []
        self.cards, self.owners, self.zones, self.controls = {}, {}, {}, {}
        self.tapped = set()
        self.pools = [[0] * 6 for _ in (0, 1)]
        self.life = [20, 20]

    def add(self, name, ref, player, zone='battlefield'):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref], self.zones[ref] = name, zone
        self.controls[ref] = self.owners[ref] = player

    def emit(self, op, **values):
        from recipes import step

        self.actions.append(step(op, **values))

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=value)

    def fund(self, player, amount):
        self.pools[player] = list(amount)
        for color, value in enumerate(amount):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def receipt(self, stack=0):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=stack)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=0)
            for color, value in enumerate(self.pools[player]):
                self.emit('assert_mana', player=player, color=color, value=value)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                cards = Counter(self.rows[name]['csvid'] for ref, name in self.cards.items()
                                if self.zones[ref] == zone and self.controls[ref] == player)
                self.emit('assert_zone', player=player, zone=zone, value=sum(cards.values()))
                for csvid, count in sorted(cards.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=csvid, value=count)
        for ref, zone in self.zones.items():
            if zone == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(ref in self.tapped))
        if self.zones['source'] == 'battlefield' and 'power' in self.model:
            player = self.controls['source']
            self.observe('source', 'power', self.model['power'])
            self.observe('source', 'toughness', self.model['toughness'])
            self.emit('assert_matching_permanents', player=player, index=self.model['csvid'], property='artifact',
                      value=int(self.model['artifact']))

    def bind(self, ref, op, player):
        self.emit(op, player=player, ref=ref, index=0, value=self.rows[self.cards[ref]]['csvid'])

    def cast(self, ref, *, target=None, success=True, budget=None, use_pool=False, response=False):
        from recipes import cost_from_oracle

        player = self.controls[ref]
        price = cost_from_oracle(self.oracle[self.cards[ref]])
        if not use_pool:
            self.fund(player, price if budget is None else budget)
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(success))
        if success:
            self.pools[player] = [have - need for have, need in zip(self.pools[player], price)]
            if min(self.pools[player]) < 0:
                raise ValueError('real cast fixture needs the exact printed card price')
            self.zones[ref] = 'stack'
            self.observe(ref, 'in_hand', 0)

    def enter_land(self, ref, enters_tapped=False):
        self.zones[ref] = 'battlefield'
        crossroads = self.cards[ref] == 'Kabira Crossroads'
        if enters_tapped or crossroads:
            self.tapped.add(ref)
        if crossroads:
            self.life[self.controls[ref]] += 2

    def play_land(self, ref, allowed=True):
        self.emit('play_land', ref=ref, value=int(allowed))
        if allowed:
            self.enter_land(ref)
        self.receipt()

    def begin_activation(self, allowed=True, *, pending=0):
        for op in ('can_activate', 'activate'):
            self.emit(op, ref='source', value=int(allowed))
        if allowed:
            self.tapped.add('source')
        self.emit('assert_stack', value=pending + int(allowed))
        self.observe('source', 'tapped', int('source' in self.tapped))
        for player in (0, 1):
            for color, value in enumerate(self.pools[player]):
                self.emit('assert_mana', player=player, color=color, value=value)

    def selection(self, ref=None, *, action=None, enters_tapped=False):
        player = self.controls['source']
        offered = [other for other, zone in self.zones.items() if zone == 'hand' and self.controls[other] == player]
        if ref and ref not in offered:
            raise ValueError('land must physically be in the current controller hand at resolution')
        if offered:
            fallback = ref or offered[0]
            self.emit('choose_zone_card' if ref else 'choose_optional_zone', player=player,
                      value=self.rows[self.cards[fallback]]['csvid'])
        if action is None:
            self.emit('resolve')
        else:
            self.actions.append(action)
        self.emit('assert_offered_count', value=len(offered))
        if offered:
            self.emit('assert_offered_selection_required', value=0)
            for item in offered:
                record = self.oracle[self.cards[item]]
                is_land = 'Land' in record['type_line'].partition(' — ')[0].split()
                self.emit('assert_offered_legal_card', index=self.rows[self.cards[item]]['csvid'], value=int(is_land))
        self.emit('assert_forced_zone_choices', value=0)
        if ref:
            self.enter_land(ref, enters_tapped)
            self.bind(ref, 'bind_permanent_copy', player)

    def move_to_owner_hand(self, ref):
        self.zones[ref] = 'hand'
        self.controls[ref] = self.owners[ref]
        self.tapped.discard(ref)
        self.bind(ref, 'bind_hand_copy', self.owners[ref])

    def advance(self):
        if any(any(pool) for pool in self.pools):
            raise ValueError('actual next turn fixture starts from empty pools')
        self.emit('advance_turn')
        self.active = 1-self.active

    def next_own_main(self, player):
        self.advance()
        if self.active != player:
            self.advance()
        self.emit('untap_step', player=player)
        self.tapped.difference_update(ref for ref, controller in self.controls.items() if controller == player)
        self.emit('turn', player=player, value=0)

    def begin_combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='attacker', value=1)
        self.tapped.add('attacker')
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')

    def finish_combat(self):
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[1-self.active] -= int(self.oracle[self.cards['attacker']]['power'])
        self.receipt()


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    active = foe if mode == 'opposing_turn' else owner
    w = LandCase(model, owner, active, rows, oracle)
    w.add(name, 'source', owner, 'hand' if mode in ('cast_short', 'fresh_then_ready') else 'battlefield')
    if mode not in ('empty', 'nonland_only'):
        land = {'choose_artifact': 'Darksteel Citadel', 'tapped_entry': 'Kabira Crossroads'}.get(mode, 'Forest')
        w.add(land, 'land', owner, 'hand')
    if mode in ('choose_basic', 'choose_artifact', 'decline', 'nonland_only'):
        w.add('Shock', 'nonland', owner, 'hand')
    if mode in ('choose_basic', 'choose_artifact', 'decline'):
        w.add('Island', 'alternate', owner, 'hand')
    if mode in ('before_normal', 'after_normal', 'next_turn_slot'):
        w.add('Mountain', 'normal', owner, 'hand')
        w.add('Island', 'second', owner, 'hand')
        w.add('Shock', 'nonland', owner, 'hand')
    if mode in ('immediate_mana', 'stack_busy'):
        w.add('Fog', 'instant', owner, 'hand')
    if mode in ('source_leaves_pending', 'foreign_controller'):
        w.add('Boomerang', 'bounce', foe if mode == 'source_leaves_pending' else owner, 'hand')
    if mode == 'foreign_controller':
        w.add('Control Magic', 'control', foe, 'hand')
        w.add('Mountain', 'foreign_land', foe, 'hand')
    if mode in ('combat', 'second_main'):
        w.add('Grizzly Bears', 'attacker', owner)
    if mode != 'foreign_controller':
        w.add('Mountain', 'foe_land', foe, 'hand')
    for player in (0, 1):
        w.add('Island', f'library_{player}', player, 'library')

    w.emit('turn', player=active, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.emit('observe_search_filters', value=1)
    w.fund(0, [0]*6)
    w.fund(1, [0]*6)
    replay_from = None
    if mode == 'cast_short':
        price = model['cast'].copy()
        price[next(i for i, n in enumerate(price) if n)] -= 1
        w.cast('source', success=False, budget=price)
        w.receipt()
    else:
        if mode == 'fresh_then_ready':
            w.cast('source')
            w.emit('assert_stack', value=1)
            w.emit('resolve')
            w.zones['source'] = 'battlefield'
            w.observe('source', 'sick', 1)
            w.begin_activation(False)
            w.receipt()
            w.next_own_main(owner)
        if mode == 'foreign_controller':
            w.advance()
            w.cast('control', target='source')
            w.emit('resolve')
            w.zones['control'] = 'battlefield'
            w.controls['source'] = foe
            w.bind('source', 'bind_permanent_copy', foe)
            w.begin_activation(False)
            w.receipt()
            w.next_own_main(foe)
        if mode in ('after_normal', 'next_turn_slot'):
            w.play_land('normal')
            w.play_land('second', False)
        if mode in ('combat', 'second_main'):
            w.begin_combat()
            if mode == 'second_main':
                w.finish_combat()
        if mode == 'stack_busy':
            w.cast('instant')
            w.emit('assert_stack', value=1)
        if mode == 'opposing_turn':
            w.play_land('land', False)
        allowed = not (model['sorcery'] and mode in ('opposing_turn', 'combat', 'stack_busy'))
        w.begin_activation(allowed, pending=int(mode == 'stack_busy'))
        if mode == 'pending_replay':
            replay_from = len(w.actions)
        if allowed:
            if mode == 'source_leaves_pending':
                w.cast('bounce', target='source')
                w.emit('assert_stack', value=2)
                # Boomerang resolves before the land ability scans the hand.
                # The returned creature is displayed as an ineligible card.
                w.zones['source'] = 'hand'
            selected = {'decline': None, 'empty': None, 'nonland_only': None,
                        'foreign_controller': 'foreign_land'}.get(mode, 'land')
            w.selection(selected)
        elif mode == 'stack_busy':
            w.emit('resolve')
        if mode == 'stack_busy':
            w.zones['instant'] = 'graveyard'
        if mode == 'source_leaves_pending':
            w.zones['bounce'] = 'graveyard'
            w.move_to_owner_hand('source')
        w.receipt()
        if allowed and mode not in ('source_leaves_pending', 'foreign_controller'):
            w.begin_activation(False)
            w.receipt()
        if mode == 'combat':
            w.finish_combat()
        if mode == 'before_normal':
            w.play_land('normal')
            w.play_land('second', False)
        if mode == 'next_turn_slot':
            w.next_own_main(owner)
            w.play_land('second')
        if mode == 'tapped_entry':
            w.emit('can_activate', ref='land', value=0)
        if mode == 'immediate_mana':
            w.emit('can_activate', ref='land', value=1)
            w.emit('activate', ref='land', value=1)
            w.tapped.add('land')
            w.pools[owner][3] = 1
            w.receipt()
            w.cast('instant', use_pool=True)
            w.emit('resolve')
            w.zones['instant'] = 'graveyard'
            w.receipt()
        if mode == 'foreign_controller':
            w.cast('bounce', target='foreign_land')
            w.emit('resolve')
            w.zones['bounce'] = 'graveyard'
            w.move_to_owner_hand('foreign_land')
            w.receipt()
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip')] + tail
                   + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=[declaration['oracle_text']],
                    untested_components=['Optional selector ordering with independent simultaneous nested triggers.'])
    return dict(name=f'LandFromHandRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


def _spell_case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    spell = model['spell']
    active = foe if mode == 'opposing_turn' else owner
    w = LandCase(model, owner, active, rows, oracle)
    w.add(name, 'source', owner, 'hand')
    if mode not in ('nonland_only', 'empty', 'drawn_land', 'drawn_declined'):
        land = 'Kabira Crossroads' if mode == 'tapped_entry' else 'Forest'
        w.add(land, 'land', owner, 'hand')
    if mode in ('decline', 'nonland_only', 'choose_land'):
        w.add('Shock', 'nonland', owner, 'hand')
    if mode in ('before_normal', 'after_normal'):
        w.add('Mountain', 'normal', owner, 'hand')
        w.add('Island', 'second', owner, 'hand')
    if spell == 'draw':
        drawn = 'Kabira Crossroads' if mode in ('drawn_land', 'drawn_declined') else 'Fog'
        w.add(drawn, 'drawn', owner, 'library')
    w.add('Plains', 'library_tail', owner, 'library')
    w.add('Island', 'other_library', foe, 'library')
    w.add('Mountain', 'other_hand', foe, 'hand')
    if mode == 'countered':
        w.add('Counterspell', 'response', foe, 'hand')
    elif mode == 'target_gone':
        w.add('Boomerang', 'response', foe, 'hand')
    target = None
    if spell != 'draw':
        if mode == 'no_legal_target':
            w.add('Island' if spell == 'pump' else 'Grizzly Bears', 'wrong_type', foe)
        else:
            chosen = 'Grizzly Bears' if spell == 'pump' else {
                'target_enchantment': 'Fervor', 'target_indestructible': 'Darksteel Citadel',
            }.get(mode, 'Spellbook')
            w.add(chosen, 'target', owner if (spell == 'pump' and mode != 'foe_target') or mode == 'own_target' else foe)
            target = 'target'
    w.emit('turn', player=active, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.emit('observe_search_filters', value=1)
    w.fund(0, [0]*6)
    w.fund(1, [0]*6)
    if mode == 'after_normal':
        w.play_land('normal')
        w.play_land('second', False)
    price = model['cast'].copy()
    if mode == 'cast_short':
        price[next(i for i, amount in enumerate(price) if amount)] -= 1
    elif mode == 'wrong_color':
        wrong = next(i for i in range(1, 6) if not price[i])
        colored = sum(price[1:])
        price[1:] = [colored if i == wrong else 0 for i in range(1, 6)]
    allowed = mode not in ('cast_short', 'wrong_color', 'no_legal_target')
    w.cast('source', target=target if allowed else None, success=allowed, budget=price)
    replay_from = None
    if allowed:
        w.emit('assert_stack', value=1)
        if mode == 'pending_replay':
            replay_from = len(w.actions)
        if mode in ('target_gone', 'countered'):
            w.emit('announce_spell', ref='source')
            w.cast('response', target=target if mode == 'target_gone' else None, response=True)
            w.emit('assert_stack', value=2)
            w.emit('resolve_response')
            w.zones['response'] = 'graveyard'
            if mode == 'target_gone':
                w.move_to_owner_hand('target')
                # If an engine incorrectly presents a picker after losing the
                # only target, decline normally and assert it was never offered.
                w.emit('choose_optional_zone', player=owner, value=rows['Forest']['csvid'])
                w.emit('resolve')
            w.emit('assert_offered_count', value=0)
        else:
            if spell == 'draw':
                w.zones['drawn'] = 'hand'
            elif spell == 'destroy' and mode != 'target_indestructible':
                w.zones['target'] = 'graveyard'
            selected = {'decline': None, 'nonland_only': None, 'empty': None, 'drawn_declined': None,
                        'drawn_land': 'drawn'}.get(mode, 'land')
            w.selection(selected)
        w.zones['source'] = 'graveyard'
    w.receipt()
    resolved = allowed and mode not in ('target_gone', 'countered')
    if resolved and spell == 'draw':
        w.emit('assert_zone_card', player=owner, zone='library', index=0, value=rows['Plains']['csvid'])
    if resolved and spell == 'pump':
        for field in ('power', 'toughness'):
            w.observe('target', field, int(oracle['Grizzly Bears'][field])+2)
    if mode == 'before_normal':
        w.play_land('normal')
        w.play_land('second', False)
    if resolved and spell == 'pump':
        w.emit('cleanup')
        for field in ('power', 'toughness'):
            w.observe('target', field, int(oracle['Grizzly Bears'][field]))
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=[declaration['oracle_text']],
                    untested_components=['Simultaneous triggers from multiple independent land entries.'])
    return dict(name=f'LandFromHandRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


def _trigger_setup(name, model, mode, owner, rows, oracle):
    foe = 1-owner
    w = LandCase(model, owner, owner, rows, oracle)
    w.add(name, 'source', owner, 'hand')
    if mode not in ('empty', 'empty_spirit', 'nonland_only', 'arcane_draw_no_land'):
        land = {'tapped_entry': 'Kabira Crossroads', 'artifact_land': 'Darksteel Citadel'}.get(mode, 'Forest')
        w.add(land, 'land', owner, 'hand')
    if mode in ('opponent_play', 'spirit', 'arcane', 'decline', 'nonland_only'):
        w.add('Shock', 'nonland', owner, 'hand')
    drawn = 'Kabira Crossroads' if mode == 'arcane_draw_no_land' else 'Plains'
    w.add(drawn, 'owner_library', owner, 'library')
    if mode in ('arcane', 'arcane_draw_no_land'):
        w.add('Island', 'owner_library_tail', owner, 'library')
    w.add('Island', 'foe_library', foe, 'library')
    w.add('Mountain', 'foe_land', foe, 'hand')
    return w


def _opponent_land(w, mode, owner):
    from recipes import step

    foe = 1-owner
    if mode == 'own_play':
        w.play_land('own_land')
    else:
        w.advance()
        if mode == 'opponent_put':
            for op in ('can_activate', 'activate'):
                w.emit(op, ref='scout', value=1)
            w.tapped.add('scout')
            w.emit('assert_stack', value=1)
            w.emit('choose_zone_card', player=foe, value=w.rows['Mountain']['csvid'])
            w.emit('resolve')
            w.enter_land('foe_land')
            w.bind('foe_land', 'bind_permanent_copy', foe)
            w.receipt()
        elif mode == 'trigger_decline':
            w.play_land('foe_land')
        else:
            if owner == 0:
                w.emit('choose_trigger', player=owner, ref='source')
            selected = None if mode in ('empty', 'nonland_only', 'decline') else 'land'
            w.selection(selected, action=step('play_land', ref='foe_land', value=1))
            w.enter_land('foe_land')
            w.receipt()
            if mode == 'opponent_second_denied':
                w.play_land('extra_foe_land', False)


def _spirit_or_arcane(w, mode, owner, card):
    from recipes import step

    if mode == 'enemy_spirit':
        w.advance()
    if mode == 'after_normal':
        w.play_land('ordinary_land')
        w.play_land('extra_land', False)
    w.cast('cause')
    w.emit('assert_stack', value=1)
    event = step('announce_spell', ref='cause')
    triggered = mode not in ('enemy_spirit', 'non_spirit', 'non_arcane', 'trigger_decline')
    if triggered:
        if owner == 0:
            w.emit('choose_trigger', player=owner, ref='source')
        selected = None if mode in ('decline', 'empty_spirit', 'arcane_draw_no_land', 'nonland_only') else 'land'
        w.selection(selected, action=event, enters_tapped=True)
        if selected:
            w.observe(selected, 'in_play', 1)
            w.observe(selected, 'tapped', 1)
    else:
        w.actions.append(event)
        w.emit('assert_offered_count', value=0)
    w.emit('assert_stack', value=1)
    if mode == 'cast_countered':
        w.cast('counter', response=True)
        w.emit('assert_stack', value=2)
        w.emit('resolve_response')
        w.zones['cause'] = w.zones['counter'] = 'graveyard'
    else:
        w.emit('resolve')
        if card in ('Reach Through Mists', 'Fog'):
            w.zones['cause'] = 'graveyard'
        else:
            w.zones['cause'] = 'battlefield'
        if card == 'Reach Through Mists':
            w.zones['owner_library'] = 'hand'
            w.emit('assert_zone_card', player=owner, zone='library', index=0, value=w.rows['Island']['csvid'])
    w.receipt()
    if mode == 'before_normal':
        w.play_land('ordinary_land')
        w.play_land('extra_land', False)


def _trigger_case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    trigger = model['trigger']
    w = _trigger_setup(name, model, mode, owner, rows, oracle)
    if trigger == 'opponent_land':
        if mode == 'own_play':
            w.add('Plains', 'own_land', owner, 'hand')
        elif mode == 'opponent_put':
            w.add('Sakura-Tribe Scout', 'scout', foe)
        elif mode == 'opponent_second_denied':
            w.add('Island', 'extra_foe_land', foe, 'hand')
            w.add('Plains', 'retained_land', owner, 'hand')
    else:
        cause = {'arcane': 'Reach Through Mists', 'arcane_draw_no_land': 'Reach Through Mists',
                 'non_spirit': 'Grizzly Bears', 'non_arcane': 'Fog'}.get(mode, 'Wandering Ones')
        if mode != 'source_short':
            w.add(cause, 'cause', foe if mode == 'enemy_spirit' else owner, 'hand')
        if mode == 'cast_countered':
            w.add('Counterspell', 'counter', foe, 'hand')
        if mode in ('before_normal', 'after_normal'):
            w.add('Mountain', 'ordinary_land', owner, 'hand')
            w.add('Plains', 'extra_land', owner, 'hand')

    w.emit('turn', player=owner, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.emit('observe_search_filters', value=1)
    w.fund(0, [0]*6)
    w.fund(1, [0]*6)
    price = model['cast'].copy()
    allowed = mode != 'source_short'
    if not allowed:
        price[next(i for i, amount in enumerate(price) if amount)] -= 1
    w.cast('source', success=allowed, budget=price)
    if allowed:
        w.emit('assert_stack', value=1)
        w.emit('resolve')
        w.zones['source'] = 'battlefield'
    w.receipt()
    replay_from = len(w.actions) if mode == 'ready_replay' else None
    if allowed:
        if trigger == 'opponent_land':
            _opponent_land(w, mode, owner)
        else:
            _spirit_or_arcane(w, mode, owner, cause)
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=[declaration['oracle_text']],
                    untested_components=['Ordering multiple simultaneous independent triggers.',
                                         'Whole-trigger decline is only offered by the native human policy.'])
    return dict(name=f'LandFromHandRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('distinct explicit reviewed land-placement variants required')
    validate_witnesses(oracle, rows)
    render = _trigger_case if model.get('trigger') else _spell_case if model.get('spell') else _case
    return [render(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
