"""Strive removal, non-target model selection and temporary copies."""

from collections import Counter
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from strive_target_rewards_recipes import STRIVE, StriveBoard


FAMILY = 'strive_removal_copy_rewards'
PROFILE = Path(__file__).parent / 'data/strive_removal_copy_rewards_wave8_profiles.json'
EFFECTS = {
    'Destroy any number of target artifacts and/or enchantments.': 'destroy',
    'Choose a creature on the battlefield. Any number of target creatures you control each become a copy of that creature until end of turn.': 'temporary',
    'Exile any number of target creatures and all Auras attached to them.': 'exile',
    "Choose any number of target creatures you control. For each of them, create a token that's a copy of that creature, except it has haste. Exile those tokens at the beginning of the next end step.": 'tokens',
    "Return any number of target enchantments to their owners' hands.": 'bounce',
}
COSTS = {
    'destroy': ('{2}{G}', '{2}{G}'),
    'temporary': ('{2}{U}', '{1}{U}'),
    'exile': ('{2}{B}{B}', '{2}{B}'),
    'tokens': ('{1}{R}', '{2}{R}'),
    'bounce': ('{U}', '{U}'),
}
COMMON = ('one', 'two_mixed', 'three', 'zero', 'stop_one', 'short_color',
          'all_unavailable', 'wrong_base_color', 'late_first', 'countered', 'replay')
EXTRA = {
    'destroy': ('short_generic', 'offturn', 'opposing', 'indestructible', 'artifact_creature'),
    'temporary': ('short_generic', 'offturn', 'shrouded_model', 'shrouded_recipient', 'copy_expires', 'foreign_only'),
    'exile': ('short_generic', 'offturn', 'opposing', 'attached_auras', 'foreign_auras', 'late_auras'),
    'tokens': ('short_generic', 'same_turn_haste', 'expires', 'foreign_only', 'different_copies'),
    'bounce': ('offturn', 'opposing', 'foreign_owner', 'artifact_only'),
}
STOCK = ('Forest', 'Island', 'Mountain', 'Plains')
STATIC = {'Grizzly Bears', 'Centaur Courser', 'Hill Giant', 'Ornithopter'}
WITNESSES = set(STOCK) | STATIC | {
    'Jalum Tome', 'Fervor', 'Sylvan Library', 'Concordant Crossroads', 'Darksteel Relic',
    'Blurred Mongoose', 'Flight', 'Spirit Link', 'Pacifism', 'Boomerang', 'Counterspell', 'Unsummon',
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    match = STRIVE.fullmatch(lines[0]) if len(lines) == 2 else None
    effect = EFFECTS.get(lines[1]) if match else None
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['keywords'] != ['Strive'] or not effect
            or record['type_line'] != ('Sorcery' if effect == 'tokens' else 'Instant')
            or (record['mana_cost'], match[1]) != COSTS[effect]):
        raise ValueError('complete reviewed Strive removal or copy clause required')
    base = cost_from_oracle(record)
    extra = cost_from_oracle({'mana_cost': match[1]})
    color, = (i for i in range(1, 6) if extra[i])
    return dict(effect=effect, base=base, extra=extra, color=color)


def variants(params):
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in (*COMMON, *EXTRA[params['effect']])]


def spec(declaration, record):
    params = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2
            or not exact(profile()['sources'].get(record['name'], {}).get('oracle'), record)
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), params)):
        raise ValueError('exact pinned Strive cost and whole Oracle required')
    if (not isinstance(selected, list) or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Strive removal/copy variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES or len(pins['sources']) != len(EFFECTS):
        raise ValueError('complete Strive removal/copy witness inventory required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows[name]
        if not exact(oracle[name], pin['oracle']) or not exact(row, pin['registry']):
            raise ValueError(f'Oracle or registry drift for {name}')
        if row['kind'] != 'card' or (name not in STATIC and row['handler'] in ('card_dummy', '0', 'NULL', None)):
            raise ValueError(f'expected registered ordinary witness for {name}')


class ExtensionBoard(StriveBoard):
    unavailable = ('all_unavailable', 'foreign_only', 'artifact_only')

    def __init__(self, name, params, variant, rows, oracle):
        self.tokens = []
        self.imitating = {}
        self.auras = {}
        super().__init__(name, params, variant, rows, oracle)

    def seed_board(self):
        mode, effect = self.mode, self.reward
        if mode in self.unavailable:
            name = 'Jalum Tome' if mode == 'artifact_only' else 'Forest'
            self.add(name, 'invalid', self.owner)
            if mode == 'foreign_only':
                self.add('Grizzly Bears', 'foe_creature', self.foe)
            return

        first, second, third = {
            'destroy': ('Jalum Tome', 'Fervor', 'Ornithopter'),
            'temporary': ('Grizzly Bears', 'Hill Giant', 'Centaur Courser'),
            'exile': ('Grizzly Bears', 'Centaur Courser', 'Hill Giant'),
            'tokens': ('Grizzly Bears', 'Hill Giant', 'Centaur Courser'),
            'bounce': ('Fervor', 'Sylvan Library', 'Concordant Crossroads'),
        }[effect]
        same_owner = effect in ('temporary', 'tokens')
        if mode == 'indestructible':
            first = 'Darksteel Relic'
        if mode == 'shrouded_recipient':
            third = 'Blurred Mongoose'
        self.add(first, 'first', self.owner)
        self.add(second, 'second', self.owner if same_owner else self.foe)
        self.add(third, 'third', self.owner)
        if same_owner:
            self.add('Jalum Tome', 'invalid', self.owner)
            if effect == 'temporary':
                choice = 'Blurred Mongoose' if mode in ('shrouded_model', 'copy_expires') else 'Ornithopter'
                self.add(choice, 'model', self.foe)
                if mode in ('shrouded_model', 'copy_expires', 'shrouded_recipient'):
                    self.add('Unsummon', 'shroud_probe', self.owner, 'hand')
            else:
                self.add('Blurred Mongoose', 'foe_creature', self.foe)
        elif effect == 'bounce':
            self.add('Jalum Tome', 'invalid', self.foe)
        elif effect == 'destroy':
            self.add('Grizzly Bears', 'invalid', self.foe)
        else:
            self.add('Jalum Tome', 'invalid', self.foe)
        if mode in ('late_first', 'late_auras'):
            self.add('Boomerang', 'response', self.foe, 'hand')
        elif mode == 'countered':
            self.add('Counterspell', 'response', self.foe, 'hand')
        if mode == 'foreign_owner':
            next(entry for entry in self.setup if entry['ref'] == 'first')['state'].append('opponent_owned')
            self.true_owners['first'] = self.foe
        if mode in ('attached_auras', 'foreign_auras', 'late_auras'):
            owner = self.foe if mode == 'foreign_auras' else self.owner
            for ref, name, controller, target in (
                ('aura_one', 'Flight', owner, 'first'),
                ('aura_two', 'Spirit Link', 1-owner, 'first'),
                ('aura_three', 'Pacifism', self.owner, 'third'),
            ):
                self.add(name, ref, controller, 'hand')
                self.auras[ref] = target

    def selection(self):
        if self.mode in self.unavailable:
            return ()
        if self.mode == 'artifact_creature':
            return ('first', 'third')
        if self.mode in ('attached_auras', 'foreign_auras', 'indestructible', 'late_auras'):
            return ('first', 'second')
        if self.mode in ('shrouded_model', 'shrouded_recipient', 'copy_expires', 'different_copies', 'expires'):
            return ('first', 'second')
        if self.mode in ('foreign_owner', 'same_turn_haste'):
            return ('first',)
        return super().selection()

    def receipt(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for ref, entry in self.entities.items():
            if entry['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', entry['tapped'])
            stats = self.imitating.get(ref, entry['stats'])
            if stats is not None:
                for field, value in zip(('power', 'toughness', 'damage'), (*stats, entry['damage'])):
                    self.observe(ref, field, value)
        names = sorted({entry['name'] for entry in self.entities.values()})
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone, seats in self.zones.items():
                cards = seats[who]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[name]['csvid'])
                elif zone != 'battlefield' or not self.imitating:
                    counts = Counter(cards)
                    for name in names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone,
                                  index=self.rows[name]['csvid'], value=counts[name])

    def move_to_owner(self, ref, zone):
        entry = self.entities[ref]
        controller = entry['player']
        owner = self.true_owners.get(ref, controller)
        if controller == owner:
            self.move(ref, zone)
            return
        self.zones[entry['zone']][controller].remove(entry['name'])
        self.zones[zone][owner].append(entry['name'])
        entry.update(player=owner, zone=zone)

    def attach_auras(self):
        for aura, host in self.auras.items():
            self.witness_spell(aura, target=host)
            self.move(aura, 'battlefield')
        if self.auras:
            self.receipt()

    def resolve_source(self, targets):
        from recipes import cost_from_oracle

        has_response = self.mode.startswith('late_') or self.mode == 'countered'
        if has_response:
            self.emit('announce_spell', ref='source')
            response = self.entities['response']
            fee = cost_from_oracle(self.oracle[response['name']])
            actor = response['player']
            self.fund(actor, fee)
            # Counterspell automatically selects the only pending parent spell.
            if self.mode != 'countered':
                self.emit('choose_target', player=actor, ref=targets[0])
            self.emit('cast_response', ref='response', value=1)
            self.spend(actor, fee)
            self.emit('assert_stack', value=2)
            self.pools()
        if self.reward == 'temporary' and self.mode != 'countered':
            # The resolution chooser is separate from all paid cast targets.
            self.emit('choose_target', player=self.owner, ref='model')
        self.emit('resolve_response' if has_response else 'resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.move('source', 'graveyard')
        if has_response:
            self.move('response', 'graveyard')
            if self.mode == 'countered':
                targets = ()
            else:
                bounced, *targets = targets
                self.move_to_owner(bounced, 'hand')
                for aura, host in self.auras.items():
                    if host == bounced:
                        self.move_to_owner(aura, 'graveyard')
        self.apply_reward(targets)
        self.receipt()

    def apply_reward(self, targets):
        if self.reward == 'temporary':
            self.imitating = {ref: self.entities['model']['stats'] for ref in targets}
            if self.mode in ('shrouded_model', 'copy_expires'):
                self.emit('can_target_creature', ref='shroud_probe', other='model', value=0)
                for ref in targets:
                    self.emit('can_target_creature', ref='shroud_probe', other=ref, value=0)
            return
        if self.reward == 'tokens':
            for original in targets:
                entry = self.entities[original]
                token = f'token_{original}'
                self.entities[token] = dict(entry, tapped=False, damage=0)
                self.tokens.append(token)
                self.zones['battlefield'][self.owner].append(entry['name'])
                if self.mode in ('same_turn_haste', 'different_copies'):
                    self.emit('bind_permanent_copy', player=self.owner, ref=token, index=1,
                              value=self.rows[entry['name']]['csvid'])
                    # Effective sickness is cleared by the token's Haste ability.
                    self.observe(token, 'sick', 0)
                else:
                    # The count is safe to assert even when no usable copy was created.
                    del self.entities[token]
            return
        zone = {'destroy': 'graveyard', 'exile': 'exile', 'bounce': 'hand'}[self.reward]
        for ref in targets:
            if self.reward == 'destroy' and self.entities[ref]['name'] == 'Darksteel Relic':
                continue
            self.move_to_owner(ref, zone)
            if self.reward == 'exile':
                for aura, host in self.auras.items():
                    if host == ref:
                        self.move_to_owner(aura, zone)

    def expire_copies(self):
        self.fund(self.owner, [0]*6)
        self.emit('cleanup')
        self.emit('refresh')
        self.imitating.clear()
        for ref in self.tokens:
            original = ref.removeprefix('token_')
            name = self.entities[original]['name']
            self.entities.pop(ref, None)
            self.zones['battlefield'][self.owner].remove(name)
        if self.mode == 'copy_expires':
            for ref in ('first', 'second'):
                self.emit('can_target_creature', ref='shroud_probe', other=ref, value=1)
        self.receipt()

    def haste_combat(self):
        token = 'token_first'
        self.emit('begin_attackers')
        self.emit('attack', ref=token, value=1)
        self.entities[token]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[self.foe] -= self.entities[token]['stats'][0]
        self.receipt()

    def actions(self):
        from recipes import step

        self.emit('turn', player=self.active, value=0)
        self.fund(self.owner, [0]*6)
        self.attach_auras()
        if self.mode in self.unavailable:
            self.fund(self.owner, self.params['base'])
            self.emit('can_cast', ref='source', value=1)
            self.receipt()
            return
        if self.mode in ('shrouded_model', 'copy_expires'):
            self.emit('can_target_creature', ref='shroud_probe', other='model', value=0)
        if self.mode == 'shrouded_recipient':
            self.emit('can_target_creature', ref='shroud_probe', other='third', value=0)
        targets = self.selection()
        if self.begin_spell(targets):
            self.resolve_source(targets)
            if self.mode == 'same_turn_haste':
                self.haste_combat()
            if self.reward == 'tokens' or self.mode in ('copy_expires', 'shrouded_model', 'replay'):
                self.expire_copies()
        else:
            self.receipt()
        if self.replay is not None:
            tail = self.steps[self.replay:]
            self.steps[self.replay:] = [step('capture'), *tail, step('record_state'), step('ai_roundtrip'),
                                        step('restore'), *tail, step('assert_recorded_state')]


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or not selected or set(selected) - set(variants(params)):
        raise ValueError('unbound Strive removal/copy source or invalid variant')
    metadata = dict(family=FAMILY, version=2, params=declaration['params'], oracle_text=declaration['oracle_text'],
                    untested_components=['more than three distinct targets', 'the priority interval between end step and cleanup',
                                         'multiplayer and replacement effects'])
    result = []
    for variant in selected:
        case = ExtensionBoard(name, params, variant, rows, oracle)
        case.actions()
        result.append(dict(name=f'StriveRemovalCopyRewardRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                           seed=2008, setup=case.setup, steps=case.steps, _recipe=dict(metadata, variant=variant)))
    return result
