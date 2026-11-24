"""Temporary unblockability spells with real combat and response receipts."""

FAMILY = 'targeted_evasion_spells'
TARGET = "Target creature can't be blocked this turn."
TWO_TARGETS = "Up to two target creatures can't be blocked this turn."
TIMING = 'Cast this spell only during the declare attackers step.'
DEVOID = 'Devoid (This card has no color.)'
LIBRARY = ('Mountain', 'Plains')
ATTACKERS = (('Hill Giant', 3, 3), ('Giant Spider', 2, 4))
REPLIES = {
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Negate': ('{1}{U}', 'Counter target noncreature spell.'),
    'Red Elemental Blast': ('{R}', 'Choose one —\n• Counter target blue spell.\n• Destroy target blue permanent.'),
}


def parameters(record):
    from recipes import cost_from_oracle

    if (record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] not in ('Instant', 'Sorcery')):
        raise ValueError('expected a normal instant or sorcery')
    text = record['oracle_text']
    shapes = {
        TARGET: (1, False, False, False),
        TARGET + '\nDraw a card.': (1, False, True, False),
        DEVOID + '\n' + TARGET + '\nDraw a card.': (1, False, True, True),
        TIMING + '\n' + TARGET: (1, True, False, False),
        TWO_TARGETS: (2, False, False, False),
    }
    if text not in shapes:
        raise ValueError('expected complete temporary unblockability rules')
    targets, attack_only, draw, devoid = shapes[text]
    instant = record['type_line'] == 'Instant'
    if (record['keywords'] != (['Devoid'] if devoid else []) or record['colors'] not in ([], ['U'], ['G'])
            or devoid != (not record['colors']) or attack_only and not instant or targets == 2 and instant):
        raise ValueError('unblockability card characteristics and rules disagree')
    return dict(targets=targets, attack_only=attack_only, draw=draw, devoid=devoid, instant=instant,
                cost=cost_from_oracle(record))


def spec(declaration, record):
    model = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('params') != model
            or declaration.get('oracle_text') != record['oracle_text']):
        raise ValueError('unblockability declaration differs from the complete printed effect')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, registry):
    for name, (cost, text) in REPLIES.items():
        if (registry[name]['kind'] != 'card' or registry[name]['handler'] in (None, '0', 'NULL', 'card_dummy')
                or (oracle[name]['mana_cost'], oracle[name]['oracle_text']) != (cost, text)):
            raise ValueError('response witness changed: ' + name)
    for name, power, toughness in (*ATTACKERS, ('Wall of Stone', 0, 8)):
        if (registry[name]['kind'] != 'card' or not oracle[name]['type_line'].startswith('Creature — ')
                or (oracle[name]['power'], oracle[name]['toughness']) != (str(power), str(toughness))):
            raise ValueError('combat witness changed: ' + name)
    for land in LIBRARY:
        if registry[land]['kind'] != 'card' or oracle[land]['type_line'] != f'Basic Land — {land}':
            raise ValueError('draw witness changed: ' + land)


def variants(model):
    modes = ('effect', 'expiry', 'fizzle', 'countered', 'short', 'replay')
    if model['instant']:
        modes += ('offturn',)
    if model['attack_only']:
        modes += ('phase',)
    if model['targets'] == 2:
        modes += ('none', 'two', 'one_leaves')
    if model['draw']:
        modes += ('color_gate',)
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def expand(name, csvid, declaration, model, selected, registry, oracle):
    row = registry[name]
    if (row['csvid'] != csvid or row['kind'] != 'card' or row['handler'] in (None, '0', 'NULL', 'card_dummy')
            or not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('expected an implemented spell and unique variants')
    return [EvasionCase(name, csvid, declaration, model, variant, registry).build() for variant in selected]


class EvasionCase:
    def __init__(self, name, csvid, declaration, model, variant, registry):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.observe, self.step = card, observe, step
        self.name, self.csvid, self.declaration, self.model, self.variant = name, csvid, declaration, model, variant
        self.registry = registry
        self.mode, seat = variant.rsplit('_', 1)
        self.owner = int(seat)
        self.foe = 1 - self.owner
        self.active = self.foe if self.mode == 'offturn' else self.owner
        self.defender = 1 - self.active
        self.casting_in_combat = self.mode == 'offturn' or model['attack_only']
        self.reply = {'fizzle': 'Unsummon', 'one_leaves': 'Unsummon', 'countered': 'Negate',
                      'color_gate': 'Red Elemental Blast'}.get(self.mode)
        self.cost = list(model['cost'])
        self.pool = self.cost if self.mode != 'short' else [self.cost[0], 0, 0, 0, 0, 0]
        self.reply_mana = cost_from_oracle({'mana_cost': REPLIES[self.reply][0]}) if self.reply else [0] * 6
        self.setup = [card(name, 'source', self.owner, 'hand')]
        for index, (attacker, _, _) in enumerate(ATTACKERS):
            self.setup.extend([card(attacker, f'attacker_{index}', self.active),
                               card('Wall of Stone', f'blocker_{index}', self.defender)])
        self.setup.extend(card(land, f'library_{index}', self.owner, 'library') for index, land in enumerate(LIBRARY))
        if self.reply:
            self.setup.append(card(self.reply, 'reply', self.foe, 'hand'))
        self.steps = []

    def emit(self, op, **fields):
        self.steps.append(self.step(op, **fields))

    def multiplicity(self, player, location, name, value):
        return self.step('assert_zone_multiplicity', player=player, zone=location,
                         index=self.registry[name]['csvid'], value=value)

    def attackers(self, indices):
        self.emit('begin_attackers')
        for index in indices:
            self.emit('attack', ref=f'attacker_{index}', value=1)
        self.emit('finish_attackers')

    def combat(self, removed, escaped, life):
        self.emit('begin_blockers')
        for index in range(len(ATTACKERS)):
            if index not in removed:
                self.emit('block', ref=f'blocker_{index}', other=f'attacker_{index}', value=int(index not in escaped))
        self.emit('advance_combat')
        for index in range(len(ATTACKERS)):
            if index not in removed:
                self.steps.append(self.observe(f'attacker_{index}', 'blocked', int(index not in escaped)))
        self.steps.extend(self.step(op) for op in ('combat_damage', 'resolve', 'end_combat'))
        self.emit('assert_life', player=self.defender, value=life - sum(ATTACKERS[i][1] for i in escaped))

    def cast_and_resolve(self):
        targets = {'none': (), 'two': (0, 1), 'one_leaves': (0, 1)}.get(self.mode, (0,))
        for index in targets:
            self.emit('choose_target', player=self.owner, ref=f'attacker_{index}')
        if self.model['targets'] == 2 and len(targets) < 2:
            self.emit('choose_cancel_target', player=self.owner)
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        if self.reply:
            self.emit('announce_spell', ref='source')
        removed = {0} if self.reply == 'Unsummon' else set()
        counter = self.reply == 'Negate' or self.mode == 'color_gate' and not self.model['devoid']
        reply_accepted = self.reply is not None and (self.mode != 'color_gate' or counter)
        if self.reply == 'Unsummon':
            self.emit('choose_target', player=self.foe, ref='attacker_0')
        if self.reply:
            self.emit('cast_response', ref='reply', value=int(reply_accepted))
            self.emit('assert_stack', value=2 if reply_accepted else 1)
        escaped = set() if counter else set(targets) - removed
        drew = int(self.model['draw'] and bool(escaped))
        resolve = self.step('resolve_response' if reply_accepted else 'resolve')
        receipts = [resolve, self.step('refresh'), self.step('assert_stack', value=0),
                    self.multiplicity(self.owner, 'graveyard', self.name, 1),
                    self.multiplicity(self.owner, 'hand', self.name, 0),
                    self.step('assert_zone', player=self.owner, zone='library', value=len(LIBRARY) - drew),
                    self.step('assert_zone_card', player=self.owner, zone='library', index=0,
                              value=self.registry[LIBRARY[drew]]['csvid']),
                    self.multiplicity(self.owner, 'hand', LIBRARY[0], drew)]
        if self.reply:
            receipts += [self.multiplicity(self.foe, 'graveyard', self.reply, int(reply_accepted)),
                         self.multiplicity(self.foe, 'hand', self.reply, int(not reply_accepted))]
        if removed:
            receipts.append(self.multiplicity(self.active, 'hand', ATTACKERS[0][0], 1))
        if self.mode == 'replay':
            self.steps += [self.step('capture'), *receipts, self.step('record_state'), self.step('ai_roundtrip'),
                           self.step('restore'), *[dict(item) for item in receipts], self.step('assert_recorded_state')]
        else:
            self.steps += receipts
        for player, amounts in ((self.owner, [0] * 6), (self.foe, [0] * 6 if reply_accepted else self.reply_mana)):
            for color, value in enumerate(amounts):
                self.emit('assert_mana', player=player, color=color, value=value)
        return removed, escaped

    def play(self):
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player, amounts in ((self.owner, self.pool), (self.foe, self.reply_mana)):
            for color, value in enumerate(amounts):
                self.emit('set_mana', player=player, color=color, value=value)
        if self.mode == 'phase':
            self.emit('can_cast', ref='source', value=0)
            self.attackers(range(len(ATTACKERS)))
            self.emit('can_cast', ref='source', value=1)
            self.emit('begin_blockers')
            self.emit('can_cast', ref='source', value=0)
            self.steps.append(self.multiplicity(self.owner, 'hand', self.name, 1))
            return
        if self.casting_in_combat:
            self.attackers(range(len(ATTACKERS)))
        if self.mode == 'short':
            self.emit('cast', ref='source', value=0)
            self.emit('assert_stack', value=0)
            self.steps.append(self.multiplicity(self.owner, 'hand', self.name, 1))
            for color, value in enumerate(self.pool):
                self.emit('assert_mana', player=self.owner, color=color, value=value)
            return
        removed, escaped = self.cast_and_resolve()
        if not self.casting_in_combat:
            self.attackers(i for i in range(len(ATTACKERS)) if i not in removed)
        self.combat(removed, escaped, 20)
        if self.mode == 'expiry':
            self.emit('advance_turn')
            self.emit('advance_turn')
            self.emit('untap_step', player=self.active)
            self.emit('turn', player=self.active, value=0)
            self.attackers(range(len(ATTACKERS)))
            self.combat(set(), set(), 20 - sum(ATTACKERS[i][1] for i in escaped))

    def build(self):
        self.play()
        return dict(name=f'TargetedEvasionSpellsRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=263, setup=self.setup, steps=self.steps,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[]))
