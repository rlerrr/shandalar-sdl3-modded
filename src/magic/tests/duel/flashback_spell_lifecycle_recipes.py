"""Fixed-cost Flashback spells through the production hand and graveyard paths."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin

FAMILY = 'flashback_spell_lifecycle'
DATA = Path(__file__).parent / 'data/flashback_spell_lifecycle_wave8_profiles.json'
FLASHBACK = re.compile(r'Flashback ((?:\{[1-9BUGRW]\})+) \(You may cast this card from your graveyard for its flashback cost\. Then exile it\.\)')
EFFECTS = {
    "Create a token that's a copy of target creature you control.": 'own_creature_copy',
    "For each creature token on the battlefield, its controller creates a token that's a copy of that creature.": 'copy_all_creature_tokens',
    'Each player chooses a creature they control. Destroy the rest.': 'choose_survivors',
    'Reveal the top four cards of your library. Put a creature card from among them into your hand and the rest into your graveyard.': 'reveal_creature',
}
DEFAULT_LIBRARY = ('Forest', 'Grizzly Bears', 'Hill Giant', 'Island', 'Plains', 'Suntail Hawk', 'Grizzly Bears', 'Mountain', 'Island')
WITNESSES = set(DEFAULT_LIBRARY) | {'Ornithopter', 'Darksteel Myr', 'Counterspell', "Tormod's Crypt", 'Silent Gravestone',
                                   'Soldier', 'Servo', 'Clue', 'Dovescape', 'Bird'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') not in ('Instant', 'Sorcery') or record.get('keywords') != ['Flashback']):
        raise ValueError('normal whole Flashback spell required')
    lines = record.get('oracle_text', '').splitlines()
    if len(lines) != 2 or lines[0] not in EFFECTS or not (match := FLASHBACK.fullmatch(lines[1])):
        raise ValueError('unmodeled whole Flashback or effect text')
    return dict(effect=EFFECTS[lines[0]], mana=cost_from_oracle(record),
                flashback_mana=cost_from_oracle({'mana_cost': match[1]}), instant=record['type_line'] == 'Instant')


def variants(params):
    modes = ['hand_effect', 'grave_effect', 'same_card_recast', 'hand_countered', 'grave_countered',
             'hand_insufficient', 'grave_insufficient', 'grave_wrong_color', 'grave_printed_cost_only',
             'grave_off_turn', 'grave_shrouded', 'grave_absent_after_exile', 'hand_pending_replay']
    if params['effect'] == 'own_creature_copy':
        modes += ['hand_artifact_copy', 'grave_artifact_copy', 'hand_copy_tapped', 'grave_copy_stolen',
                  'hand_no_own_target', 'grave_no_own_target', 'hand_target_lost', 'grave_copied_token']
    elif params['effect'] == 'copy_all_creature_tokens':
        modes += ['hand_no_tokens', 'grave_no_tokens', 'grave_nontoken_excluded', 'grave_noncreature_token', 'grave_several_tokens']
    elif params['effect'] == 'choose_survivors':
        modes += ['hand_other_survivors', 'grave_other_survivors', 'grave_empty_opponent',
                  'grave_indestructible', 'grave_actual_owner', 'grave_no_creatures']
    else:
        modes += ['hand_other_pick', 'grave_other_pick', 'grave_no_eligible', 'grave_short_library', 'grave_empty_library']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('whole pinned source Oracle and exact recipe version required')
    params = classify(record)
    if not exact(declaration.get('params'), params):
        raise ValueError('parameters must match complete Oracle clauses')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(value, str) for value in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Flashback variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('exact Flashback fixture witness set required')
    records = dict(data['witnesses'], **data['sources'])
    if set(records) != set(data['handlers']):
        raise ValueError('complete source and witness handler pin required')
    for name, expected in records.items():
        row = rows[name]
        if (not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]
                or row['handler'] in ('0', 'NULL')):
            raise ValueError(f'Oracle or production handler changed for {name}')
        if name in data['sources'] and (row['kind'] != 'card' or row['handler'] == 'card_dummy'):
            raise ValueError(f'implemented normal source required for {name}')


class FlashbackCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import step

        self.name, self.csvid, self.declaration, self.params = name, csvid, declaration, params
        self.mode, who = variant.rsplit('_', 1)
        self.owner = int(who)
        self.other = 1 - self.owner
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.grave = self.mode.startswith('grave_')
        self.connected = self.mode == 'same_card_recast'
        self.active = self.other if self.mode == 'grave_off_turn' else self.owner
        self.countered = self.mode.endswith('_countered')
        self.grave_countered = self.mode == 'grave_countered'
        self.effect = params['effect']
        self.permanents, self.steps, self.setup = {}, [], []
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana = [[0] * 6, [0] * 6]
        self.mana[self.owner] = self.cost_to_seed()
        if self.grave_countered:
            self.seed('Dovescape', 'dovescape', self.other)
        elif self.countered:
            self.mana[self.other][2] = 2
            self.seed('Counterspell', 'counter', self.other, 'hand')
        self.grave_index = self.connected_index() if self.connected else 1
        self.grave_ref = 'grave_cast_slot' if self.connected else 'source'
        for player in (0, 1):
            library = self.library() if player == self.owner else DEFAULT_LIBRARY
            for index, card in enumerate(library):
                self.seed(card, f'lib_{player}_{index}', player, 'library')
            self.seed('Island', f'exile_{player}', player, 'exile')
            if player == self.owner and self.connected:
                for index in range(self.grave_index + 1):
                    ref = self.grave_ref if index == self.grave_index else f'grave_pad_{index}'
                    self.seed('Plains', ref, player, 'graveyard')
            else:
                self.seed('Plains', f'grave_{player}', player, 'graveyard')
        self.seed(name, 'source', self.owner, 'graveyard' if self.grave else 'hand')
        self.seed_effect()
        if self.connected:
            self.seed("Tormod's Crypt", 'crypt', self.other)
        if self.mode == 'grave_shrouded':
            self.seed('Silent Gravestone', 'shroud', self.other)
        names = {card['name'] for card in self.setup}
        if self.grave_countered:
            names.add('Bird')
        self.names = sorted(names)
        self.before = [step('turn', player=self.active, value=0), step('present_ai_dialogs', value=1)]
        self.before += [step('set_mana', player=player, color=color, value=amount)
                        for player in (0, 1) for color, amount in enumerate(self.mana[player])]
        self.before += [step('refresh'), step('observe_search_filters', value=1)]
        self.replay_cut = 0

    def library(self):
        if self.mode == 'grave_empty_library':
            return []
        if self.mode == 'grave_short_library':
            return ['Forest', 'Grizzly Bears']
        if self.mode == 'grave_no_eligible':
            return ['Forest', 'Island', 'Plains', 'Mountain', 'Hill Giant']
        return DEFAULT_LIBRARY

    def connected_index(self):
        # Reckoning is processed into the graveyard before the destroyed
        # creatures complete deferred graveyard-order processing.
        return {'own_creature_copy': 0, 'copy_all_creature_tokens': 0,
                'choose_survivors': 0, 'reveal_creature': 3}[self.effect]

    def cost_to_seed(self):
        mana = list(self.params['flashback_mana' if self.grave else 'mana'])
        if self.mode.endswith('_insufficient'):
            mana[next(color for color, amount in enumerate(mana) if amount)] -= 1
        elif self.mode == 'grave_wrong_color':
            mana = [sum(mana), 0, 0, 0, 0, 0]
        elif self.mode == 'grave_printed_cost_only':
            mana = list(self.params['mana'])
        return mana

    def seed_effect(self):
        if self.effect == 'own_creature_copy':
            self.seed('Suntail Hawk', 'opposing', self.other)
            if self.mode.endswith('no_own_target'):
                return
            name = 'Ornithopter' if 'artifact_copy' in self.mode else 'Soldier' if self.mode == 'grave_copied_token' else 'Grizzly Bears'
            self.seed(name, 'target', self.owner, token=name == 'Soldier', tapped=self.mode == 'hand_copy_tapped',
                      foreign=self.mode == 'grave_copy_stolen')
        elif self.effect == 'copy_all_creature_tokens':
            self.seed('Grizzly Bears', 'ordinary', self.owner)
            self.seed('Hill Giant', 'opposing', self.other)
            if not self.mode.endswith('no_tokens'):
                self.seed('Soldier', 'token_own', self.owner, token=True)
                self.seed('Servo', 'token_other', self.other, token=True)
            if self.mode == 'grave_noncreature_token':
                self.seed('Clue', 'clue', self.owner, token=True)
            if self.mode == 'grave_several_tokens':
                self.seed('Soldier', 'token_extra', self.owner, token=True, tapped=True)
        elif self.effect == 'choose_survivors':
            if self.mode == 'grave_no_creatures':
                return
            self.seed('Grizzly Bears', 'first_own', self.owner)
            own = 'Darksteel Myr' if self.mode == 'grave_indestructible' else 'Hill Giant'
            self.seed(own, 'second_own', self.owner, foreign=self.mode == 'grave_actual_owner')
            if self.mode != 'grave_empty_opponent':
                self.seed('Grizzly Bears', 'first_other', self.other)
                self.seed('Suntail Hawk', 'second_other', self.other)

    def seed(self, name, ref, player, zone='battlefield', token=False, tapped=False, foreign=False):
        from recipes import card

        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['tapped' if tapped else 'untapped']
            if token:
                entry['state'].append('token')
            if foreign:
                entry['state'].append('opponent_owned')
            self.permanents[ref] = dict(name=name, player=player, owner=1 - player if foreign else player,
                                        token=token, alive=True, tapped=tapped)
        self.setup.append(entry)
        self.zones[zone][player].append(name)

    def emit(self, op, **kwargs):
        from recipes import step

        self.steps.append(step(op, **kwargs))

    def move(self, name, player, start, finish):
        self.zones[start][player].remove(name)
        self.zones[finish][player].append(name)

    def destroy_in_model(self, ref):
        p = self.permanents[ref]
        p['alive'] = False
        self.zones['battlefield'][p['player']].remove(p['name'])
        if not p['token']:
            self.zones['graveyard'][p['owner']].append(p['name'])

    def spend(self, owner, cost):
        self.mana[owner] = [amount - price for amount, price in zip(self.mana[owner], cost)]

    def receipt(self, stack=0, raw_hand_pending=False):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=20)
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone, pair in self.zones.items():
                names = pair[player]
                if zone == 'hand' and raw_hand_pending and player == self.owner:
                    names = names + [self.name]
                self.emit('assert_zone', player=player, zone=zone, value=len(names))
                counts = Counter(names)
                for name in self.names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[name]['csvid'])

    def target_choice(self):
        if self.effect == 'own_creature_copy':
            self.emit('choose_target', player=self.owner, ref='target')

    def resolution_choices(self):
        if self.effect == 'choose_survivors':
            suffix = 'second' if self.mode.endswith('other_survivors') else 'first'
            for player in (self.active, 1 - self.active):
                group = 'own' if player == self.owner else 'other'
                ref = f'{suffix}_{group}'
                if ref in self.permanents and self.permanents[ref]['alive']:
                    self.emit('choose_target', player=player, ref=ref)
        elif self.effect == 'reveal_creature':
            cards = self.zones['library'][self.owner][:4]
            if cards:
                name = self.picked_creature(cards)
                if name:
                    self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
                else:
                    # This mandatory observed selector accepts empty result only
                    # where the offered prefix has no legal creature.
                    self.emit('choose_zone', player=self.owner, value=-1)

    def picked_creature(self, cards):
        choices = [name for name in cards if 'Creature' in self.oracle[name]['type_line'].partition(' — ')[0].split()]
        if not choices:
            return None
        return choices[-1] if self.mode.endswith('other_pick') else choices[0]

    def apply_effect(self):
        if self.effect == 'own_creature_copy':
            self.copy_permanent('target')
        elif self.effect == 'copy_all_creature_tokens':
            original = list(self.permanents.items())
            for ref, p in original:
                if p['alive'] and p['token'] and 'Creature' in self.oracle[p['name']]['type_line'].partition(' — ')[0].split():
                    self.copy_permanent(ref)
        elif self.effect == 'choose_survivors':
            kept = 'second' if self.mode.endswith('other_survivors') else 'first'
            for ref, p in self.permanents.items():
                if ref.startswith(('first_', 'second_')) and not ref.startswith(kept) and p['alive'] and p['name'] != 'Darksteel Myr':
                    self.destroy_in_model(ref)
        else:
            cards = self.zones['library'][self.owner][:4]
            picked = self.picked_creature(cards)
            if picked:
                self.move(picked, self.owner, 'library', 'hand')
                cards.remove(picked)
            for name in cards:
                self.move(name, self.owner, 'library', 'graveyard')

    def copy_permanent(self, ref):
        source = self.permanents[ref]
        owner, name = source['player'], source['name']
        ordinal = self.zones['battlefield'][owner].count(name)
        copy_ref = f'{ref}_copy_{ordinal}'
        self.permanents[copy_ref] = dict(name=name, player=owner, owner=owner, token=True, alive=True, tapped=False)
        self.zones['battlefield'][owner].append(name)
        if self.mode == 'hand_copy_tapped':
            # This fixture's only tapped matching permanent is the original.
            self.emit('assert_card', ref=ref, property='tapped', value=1)

    def token_receipts(self):
        for player in (0, 1):
            tokens = {p['name'] for p in self.permanents.values() if p['alive'] and p['player'] == player and p['token']}
            nontokens = {p['name'] for p in self.permanents.values() if p['alive'] and p['player'] == player and not p['token']}
            for name in sorted(tokens):
                record = self.oracle[name]
                if 'Creature' in record['type_line'].partition(' — ')[0].split():
                    for key in ('power', 'toughness'):
                        self.emit('assert_matching_permanents', player=player, index=self.rows[name]['csvid'],
                                  property=key, value=int(record[key]))
            for name in sorted(tokens - nontokens):
                self.emit('assert_matching_permanents', player=player, index=self.rows[name]['csvid'], property='token', value=1)
            if self.mode == 'hand_copy_tapped' and 'Grizzly Bears' in tokens - nontokens:
                for key, value in [('tapped', 0), ('sick', 1)]:
                    self.emit('assert_matching_permanents', player=player, index=self.rows['Grizzly Bears']['csvid'], property=key, value=value)
            if self.mode == 'grave_artifact_copy' and 'Ornithopter' in tokens - nontokens:
                self.emit('assert_matching_permanents', player=player, index=self.rows['Ornithopter']['csvid'], property='artifact', value=1)

    def hand_cast(self):
        if self.mode in ('hand_no_own_target', 'hand_insufficient'):
            self.emit('cast', ref='source', value=0)
            return
        self.target_choice()
        self.emit('cast', ref='source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('require_card', ref='source', property='in_hand', value=0)
        self.spend(self.owner, self.params['mana'])
        self.zones['hand'][self.owner].remove(self.name)
        self.receipt(stack=1, raw_hand_pending=True)
        if self.mode == 'hand_pending_replay':
            self.replay_cut = len(self.steps)
        if self.countered:
            self.emit('announce_spell', ref='source')
            self.emit('cast_response', ref='counter', value=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.spend(self.other, [0, 0, 2, 0, 0, 0])
            self.move('Counterspell', self.other, 'hand', 'graveyard')
        else:
            if self.mode == 'hand_target_lost':
                self.emit('destroy', ref='target')
                self.destroy_in_model('target')
            self.resolution_choices()
            self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        if not self.countered and self.mode != 'hand_target_lost':
            self.apply_effect()
        self.zones['graveyard'][self.owner].append(self.name)
        self.receipt()

    def grave_cast(self):
        deny_modes = ('grave_insufficient', 'grave_wrong_color', 'grave_printed_cost_only', 'grave_no_own_target')
        denied = self.mode in deny_modes or (self.mode == 'grave_off_turn' and not self.params['instant'])
        self.emit('assert_zone_card', player=self.owner, zone='graveyard', index=self.grave_index, value=self.csvid)
        self.emit('prepare_graveyard_abilities')
        self.emit('require_can_cast_graveyard', ref=self.grave_ref, value=int(not denied))
        if not denied:
            self.target_choice()
            if not self.grave_countered:
                self.resolution_choices()
        self.emit('cast_graveyard', ref=self.grave_ref, value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        if denied:
            return
        self.spend(self.owner, self.params['flashback_mana'])
        self.zones['graveyard'][self.owner].remove(self.name)
        if self.grave_countered:
            # Dovescape responds to the actual cast announcement without a
            # priority choice. Its token quantity uses printed mana value.
            self.zones['battlefield'][self.owner].extend(['Bird'] * int(self.oracle[self.name]['cmc']))
        else:
            self.apply_effect()
        self.zones['exile'][self.owner].append(self.name)
        self.receipt()
        if self.grave_countered:
            # Dovescape supplies these characteristics; the generic Bird ID
            # can represent other printed tokens in other contexts.
            white_blue = 0x20 | 0x04
            for key, value in [('token', 1), ('subtype_Bird', 1), ('power', 1), ('toughness', 1), ('color', white_blue)]:
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows['Bird']['csvid'], property=key, value=value)

    def clear_grave_anchor(self):
        self.emit('choose_player', player=self.other, value=self.owner)
        self.emit('activate', ref='crypt', value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.destroy_in_model('crypt')
        for name in self.zones['graveyard'][self.owner][:]:
            self.move(name, self.owner, 'graveyard', 'exile')
        self.receipt()

    def case(self):
        from recipes import step

        if self.connected:
            self.clear_grave_anchor()
        if not self.grave:
            self.hand_cast()
        if self.connected:
            self.mana[self.owner] = list(self.params['flashback_mana'])
            for color, amount in enumerate(self.mana[self.owner]):
                self.emit('set_mana', player=self.owner, color=color, value=amount)
            self.emit('refresh')
        if self.grave or self.connected:
            self.grave_cast()
        if self.mode == 'grave_absent_after_exile':
            # The positional reference must no longer designate an eligible spell.
            self.emit('require_can_cast_graveyard', ref=self.grave_ref, value=0)
            self.emit('cast_graveyard', ref=self.grave_ref, value=0)
        if self.mode in ('hand_copy_tapped', 'grave_copy_stolen', 'grave_artifact_copy'):
            # Separate the original from its identical CSVID to prove the new
            # permanent is a token and the original reaches its own graveyard.
            self.emit('destroy', ref='target')
            self.emit('refresh')
            self.destroy_in_model('target')
        self.token_receipts()
        self.receipt()
        replay = self.steps[self.replay_cut:]
        sequence = self.before + self.steps[:self.replay_cut] + [step('capture')] + replay
        sequence += [step('record_state'), step('restore')] + replay + [step('assert_recorded_state')]
        return dict(name=f'FlashbackSpellLifecycleRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=sequence,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[
                                     'Flashback resolves in the production nested cast. A separate test-side pending Flashback spell is not manufactured.',
                                     'Connected recast uses an independently checked fixed graveyard slot after actual hand resolution.',
                                 ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params)):
        raise ValueError('unsupported Flashback lifecycle variants')
    return [FlashbackCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
