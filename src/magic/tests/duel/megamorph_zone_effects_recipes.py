"""Whole Megamorph effects exercised through real zones, payments and turn-up."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN
from morph_turn_up_target_recipes import HIDDEN_FEE, Scene as MorphScene
from morph_turn_up_zones_recipes import has_type


FAMILY = 'megamorph_zone_effects'
PROFILE = Path(__file__).parent / 'data/megamorph_zone_effects_wave8_profiles.json'
MEGA = re.compile(r'Megamorph ((?:\{[1-9]\})?(?:\{[WUBRG]\}){1,2}) '
                  r'\(You may cast this card face down as a 2/2 creature for \{3\}\. Turn it face up any time '
                  r'for its megamorph cost and put a \+1/\+1 counter on it\.\)')
MENACE = "Menace (This creature can't be blocked except by two or more creatures.)"
CLAUSES = {
    ('Deathtouch',
     'Whenever a permanent you control is turned face up, you may return this card from your graveyard to the battlefield face up or face down.'):
        ('raptor', {'Deathtouch'}),
    ("Creatures with power less than this creature's power can't block it.",
     'When this creature is turned face up, return target card from your graveyard to your hand.'):
        ('den', set()),
    (MENACE, 'When this creature is turned face up, exile the top card of your library. Until end of turn, you may play that card.'):
        ('ire', {'Menace'}),
    ('When this creature is turned face up, return target noncreature, nonland card from your graveyard to your hand.',):
        ('restricted', set()),
    ("Whenever another permanent you control is turned face up, if it's a creature, put two +1/+1 counters on it.",):
        ('ambush', set()),
    ("{U}: Exile this creature, then return it to the battlefield face down under its owner's control.",
     '{1}: This creature gets +1/-1 or -1/+1 until end of turn.'):
        ('shore', set()),
}
COMMON = ('hidden', 'faceup', 'hidden_choice', 'hidden_short', 'flip', 'flip_short', 'flip_wrong', 'offturn', 'replay')
EXTRA = {
    'raptor': ('return_up', 'return_down', 'return_down_reveal', 'return_decline', 'opponent_flip', 'grave_shroud',
               'deathtouch', 'hidden_damage'),
    'den': ('recover_land', 'recover_spell', 'empty_grave', 'foe_grave', 'shroud', 'lower_blocker', 'equal_blocker',
            'higher_blocker', 'hidden_blocker', 'printed_blocker'),
    'restricted': ('artifact_target', 'no_match', 'empty_grave', 'foe_grave', 'shroud'),
    'ire': ('play_land', 'play_spell', 'spent_land_drop', 'wrong_player', 'wrong_card', 'expiry', 'offturn_land',
            'offturn_spell', 'short_spell', 'empty_library', 'menace_single', 'menace_pair', 'hidden_blocker'),
    'ambush': ('own_flip', 'opponent_flip', 'two_watchers', 'hidden_watcher', 'entry_no_flip',
               'counter_persistence', 'megamorph_stack'),
    'shore': ('blink', 'blink_reset', 'blink_owner', 'pump_up', 'pump_down', 'pump_cleanup', 'lethal_shrink'),
}
DECK = ('Island', 'Mountain', 'Darksteel Relic')
RAPTOR_RETURN = ('return_up', 'return_down', 'return_down_reveal', 'return_decline', 'opponent_flip', 'grave_shroud')
AMBUSH_WITNESS = set(EXTRA['ambush'])
IRE_PERMISSION = set(EXTRA['ire'][:9])
COMBAT = set(EXTRA['den'][-5:]) | set(EXTRA['ire'][-3:]) | {'deathtouch', 'hidden_damage'}
VANILLA = {'Grizzly Bears', 'Hill Giant', 'Craw Wurm', 'Wall of Stone'}
WITNESSES = set(DECK) | VANILLA | {
    'Bazaar Trader', 'Forest', 'Glacial Stalker', 'Goblin Tinkerer', 'Murder', 'Reach Through Mists',
    'Sandstorm Charger', 'Silent Gravestone', 'Unsummon',
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record
            or not record['type_line'].startswith('Creature — ')
            or any(not re.fullmatch('[0-9]+', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('numeric whole normal creature required')
    lines = record['oracle_text'].splitlines()
    forms = [(i, match[1]) for i, line in enumerate(lines) if (match := MEGA.fullmatch(line))]
    if len(forms) != 1:
        raise ValueError('one exact printed Megamorph clause required')
    index, cost = forms[0]
    family = CLAUSES.get(tuple(lines[:index] + lines[index+1:]))
    if not family or set(record['keywords']) != {'Megamorph', *family[1]}:
        raise ValueError('whole supported Megamorph composition required')
    return dict(effect=family[0], cast_mana=cost_from_oracle(record),
                flip_mana=cost_from_oracle(dict(record, mana_cost=cost)),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + EXTRA[params['effect']]]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    source = pins()['sources'].get(record['name'], {})
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(source.get('oracle'), record) or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model)):
        raise ValueError('exact whole Oracle Megamorph composition required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('distinct reviewed Megamorph variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data['witnesses']) != WITNESSES or not exact(data['hidden_observer'], rows.get('Face-Down Creature')):
        raise ValueError('exact witness and face-down identities required')
    if data['hidden_observer']['csvid'] != FACE_DOWN:
        raise ValueError('native face-down observer changed')
    for name, expected in (data['sources'] | data['witnesses']).items():
        if (not exact(expected, {'oracle': oracle.get(name), 'registry': rows.get(name)})
                or oracle[name]['layout'] != 'normal' or rows[name]['kind'] != 'card'):
            raise ValueError(f'Oracle or registry identity changed for {name}')
        if name not in VANILLA and rows[name]['handler'] in ('card_dummy', 'NULL', '0', None):
            raise ValueError(f'production handler missing for {name}')


class MegaScene(MorphScene):
    def add(self, name, ref, owner, zone='battlefield', tapped=False):
        super().add(name, ref, owner, zone, tapped)
        self.entities[ref].update(down=False, counters=0)

    def set_face(self, ref, down):
        self.entities[ref]['down'] = down
        if ref == 'source':
            self.hidden = down

    def stats(self, ref):
        piece = self.entities[ref]
        base = (2, 2) if piece['down'] else piece['base']
        return tuple(value + delta + piece['counters'] for value, delta in zip(base, piece['bonus']))

    def observed_id(self, ref):
        piece = self.entities[ref]
        return FACE_DOWN if piece['down'] else self.rows[piece['name']]['csvid']

    def deck(self, owner):
        return [ref for ref, piece in self.entities.items() if piece['owner'] == owner and piece['zone'] == 'library']

    def leave(self, ref, zone):
        super().leave(ref, zone)
        self.set_face(ref, False)
        self.entities[ref]['counters'] = 0

    def receipts(self):
        super().receipts()
        for ref, piece in self.entities.items():
            if piece['zone'] == 'battlefield' and has_type(self.oracle[piece['name']], 'Creature'):
                self.emit('assert_counter', ref=ref, counter_kind='plus1', value=piece['counters'])
                self.prop(ref, 'csvid', self.observed_id(ref))
        for player in (0, 1):
            for i, ref in enumerate(self.deck(player)):
                self.emit('assert_zone_card', player=player, zone='library', index=i,
                          value=self.rows[self.entities[ref]['name']]['csvid'])
            self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=FACE_DOWN,
                      value=sum(e['zone'] == 'battlefield' and e['controller'] == player and e['down']
                                for e in self.entities.values()))

    def exact_fund(self, player, budget):
        if any(self.mana[player]):
            raise ValueError('all earlier actions must consume their funded pools')
        self.fund(player, budget)

    def cast_body(self, ref, *, down=False, denial=False, choice=False):
        from recipes import cost_from_oracle

        player = self.entities[ref]['owner']
        printed = cost_from_oracle(self.oracle[self.entities[ref]['name']])
        cost = list(HIDDEN_FEE if down else printed)
        budget = list(cost)
        if denial:
            budget[0] = 2
        elif choice:
            budget = list(printed)
            budget[0] = max(3, budget[0])
        self.exact_fund(player, budget)
        if choice or (not down and sum(cost) >= 3):
            self.emit('choose_dialog', player=player, value=int(not down))
        self.emit('cast', ref=ref, value=int(not denial))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(not denial))
        self.prop(ref, 'in_hand', denial)
        if not denial:
            self.pay(player, cost)
            self.set_face(ref, down)
            self.prop(ref, 'csvid', self.observed_id(ref))
            self.pools()
            self.resolve()
            self.entities[ref]['zone'] = 'battlefield'
        self.receipts()

    def plan_grave(self):
        if self.mode == 'shroud':
            return None, []
        refs = [ref for ref, entity in self.entities.items()
                if entity['owner'] == self.owner and entity['zone'] == 'graveyard']
        eligible = [ref for ref in refs if self.effect == 'den'
                    or not any(has_type(self.oracle[self.entities[ref]['name']], t) for t in ('Land', 'Creature'))]
        chosen = 'land' if self.mode == 'recover_land' else 'artifact' if self.mode == 'artifact_target' else (
            'spell' if self.mode == 'recover_spell' or self.effect == 'restricted' else 'creature')
        if not eligible:
            return None, []
        assert chosen in eligible
        self.emit('choose_zone_card', player=self.owner, value=self.rows[self.entities[chosen]['name']]['csvid'])
        return chosen, [(self.rows[self.entities[ref]['name']]['csvid'], int(ref in eligible)) for ref in refs]

    def reveal(self, ref='source', *, ordinary_fee=None, grant_counters=0, recover=None):
        actor = self.entities[ref]['controller']
        price = list(self.model['flip_mana'] if ref == 'source' else ordinary_fee)
        budget = list(price)
        denied = ref == 'source' and self.mode in ('flip_short', 'flip_wrong')
        if denied:
            colored = next(i for i in range(1, 6) if budget[i])
            budget[colored] -= 1
            if self.mode == 'flip_wrong':
                budget[next(i for i in range(1, 6) if i != colored)] += 1
        self.exact_fund(actor, budget)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate', ref=ref, value=int(not denied))
        pick, offers = None, []
        if not denied and ref == 'source' and self.effect in ('den', 'restricted'):
            pick, offers = self.plan_grave()
        if recover is not None:
            self.emit('choose_dialog', player=actor, value=recover)
        self.emit('activate', ref=ref, value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        if denied:
            self.receipts()
            return
        self.pay(actor, price)
        self.set_face(ref, False)
        real_mega = 'Megamorph' in self.oracle[self.entities[ref]['name']]['keywords']
        self.entities[ref]['counters'] += int(real_mega) + grant_counters
        self.emit('refresh')
        self.prop(ref, 'csvid', self.observed_id(ref))
        self.emit('assert_counter', ref=ref, counter_kind='plus1', value=self.entities[ref]['counters'])
        self.pools()
        self.resolve()
        if offers:
            self.emit('assert_offered_count', value=len(offers))
            self.emit('assert_offered_selection_required', value=1)
            for csvid, legal in offers:
                self.emit('assert_offered_legal_card', index=csvid, value=legal)
        if pick:
            self.leave(pick, 'hand')
        if recover in (0, 1):
            self.leave('source', 'battlefield')
            self.set_face('source', recover == 1)
            self.bind('source')
        if ref == 'source' and self.effect == 'ire' and self.deck(actor):
            self.exiled = self.deck(actor)[0]
            self.leave(self.exiled, 'exile')
        self.receipts()

    def kill_source(self):
        self.cast_spell('removal', 'source')
        self.resolve()
        self.leave('removal', 'graveyard')
        self.leave('source', 'graveyard')
        self.receipts()

    def do_raptor_return(self):
        self.cast_body('source')
        if self.mode == 'opponent_flip':
            self.advance()
        self.cast_body('trigger', down=True)
        self.kill_source()
        choice = None if self.mode == 'opponent_flip' else (
            2 if self.mode == 'return_decline' else 1 if 'down' in self.mode else 0)
        self.reveal('trigger', ordinary_fee=[4, 0, 1, 0, 0, 0], recover=choice)
        if self.mode == 'return_down_reveal':
            self.reveal()

    def do_ambush(self):
        self.cast_body('source', down=self.mode == 'hidden_watcher')
        if self.mode == 'two_watchers':
            self.cast_body('colleague')
        if self.mode == 'opponent_flip':
            self.advance()
        self.cast_body('trigger', down=self.mode != 'entry_no_flip')
        if self.mode == 'entry_no_flip':
            return
        trigger_fee = [4, 0, 0, 0, 0, 1] if self.mode == 'megamorph_stack' else [4, 0, 1, 0, 0, 0]
        grant = {'opponent_flip': 0, 'hidden_watcher': 0, 'two_watchers': 4}.get(self.mode, 2)
        self.reveal('trigger', ordinary_fee=trigger_fee, grant_counters=grant)
        if self.mode == 'counter_persistence':
            self.cleanup()

    def land_from_hand(self):
        self.emit('play_land', ref='land_drop', value=1)
        self.leave('land_drop', 'battlefield')
        self.receipts()

    def permission(self, ref, allowed, *, actor=None, use=True):
        who = self.owner if actor is None else actor
        card_name = self.entities[ref]['name']
        index = self.rows[card_name]['csvid']
        self.emit('can_play_exiled', player=who, index=index, value=int(allowed))
        if not use:
            return
        self.emit('play_exiled', player=who, index=index, value=int(allowed))
        if allowed:
            if has_type(self.oracle[card_name], 'Land'):
                self.leave(ref, 'battlefield')
                self.bind(ref)
            else:
                self.pay(who, [0, 0, 1, 0, 0, 0])
                self.leave(ref, 'graveyard')
                self.leave(self.deck(who)[0], 'hand')
            self.emit('resolve')
            self.emit('refresh')
        self.receipts()

    def do_permission(self):
        mode = self.mode
        if mode in ('play_spell', 'offturn_spell'):
            self.exact_fund(self.owner, [0, 0, 1, 0, 0, 0])
        if mode == 'expiry':
            self.permission(self.exiled, True, use=False)
            self.cleanup()
            self.advance()
            self.advance()
            self.emit('turn', player=self.owner, value=0)
            self.permission(self.exiled, False)
        elif mode == 'wrong_card':
            self.permission('foreign_exile', False)
            self.permission(self.exiled, True)
        elif mode == 'wrong_player':
            self.permission(self.exiled, False, actor=self.foe)
            self.permission(self.exiled, True)
        elif mode in ('spent_land_drop', 'offturn_land', 'short_spell'):
            self.permission(self.exiled, False)
        else:
            self.permission(self.exiled, True)
            self.permission(self.exiled, False)

    def shore_activation(self, *, blink=False, boost=None):
        source = self.entities['source']
        actor = source['controller']
        fee = [0, 0, 1, 0, 0, 0] if blink else [1, 0, 0, 0, 0, 0]
        self.exact_fund(actor, fee)
        self.emit('require_can_activate', ref='source', value=1)
        self.emit('choose_activation_mode', ref='source', player=actor,
                  mode_label='Exile and return' if blink else '+1/-1 or -1/+1',
                  fallback_color=0, allow_auto_nonmana=False)
        self.emit('activate', ref='source', value=1)
        self.emit('require_activation_mode', value=1)
        self.emit('assert_stack', value=1)
        self.pay(actor, fee)
        self.pools()
        if not blink:
            self.emit('choose_dialog', player=actor, value=int(boost < 0))
        self.resolve()
        if blink:
            self.leave('source', 'battlefield')
            self.set_face('source', True)
            self.bind('source')
        else:
            power, toughness = source['bonus']
            source['bonus'] = power + boost, toughness - boost
            if self.stats('source')[1] <= 0:
                self.leave('source', 'graveyard')
        self.receipts()

    def do_shore(self):
        if self.mode == 'blink_reset':
            self.cast_body('source', down=True)
            self.reveal()
            self.shore_activation(boost=1)
            self.shore_activation(blink=True)
            self.reveal()
            return
        self.cast_body('source')
        if self.mode == 'blink_owner':
            self.donate('source')
        if self.mode in ('blink', 'blink_owner'):
            self.shore_activation(blink=True)
        elif self.mode == 'lethal_shrink':
            for _ in range(3):
                self.shore_activation(boost=1)
        else:
            self.shore_activation(boost=-1 if self.mode == 'pump_down' else 1)
            if self.mode == 'pump_cleanup':
                self.cleanup()

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        mode = self.mode
        can_block = mode not in ('lower_blocker', 'menace_single')
        self.emit('block', ref='blocker', other='source', value=int(can_block))
        if mode == 'menace_pair':
            self.emit('block', ref='second_blocker', other='source', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', can_block)
        self.entities['source']['tapped'] = True
        if mode == 'menace_pair':
            for ref in ('blocker', 'second_blocker'):
                self.prop(ref, 'blocking_count', 1)
            for ref in ('source', 'blocker', 'second_blocker'):
                self.emit('remove_from_combat', ref=ref)
            self.cleanup()
            return
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if not can_block:
            self.life[self.foe] -= self.stats('source')[0]
        else:
            self.entities['blocker']['damage'] = self.stats('source')[0]
            self.entities['source']['damage'] = self.stats('blocker')[0]
            if mode == 'deathtouch' or self.entities['blocker']['damage'] >= self.stats('blocker')[1]:
                self.leave('blocker', 'graveyard')
            if self.entities['source']['damage'] >= self.stats('source')[1]:
                self.leave('source', 'graveyard')
        self.receipts()

    def setup_grave(self):
        if self.mode in ('shroud',):
            self.add('Silent Gravestone', 'shroud', self.foe)
        if self.mode == 'empty_grave':
            return
        owner = self.foe if self.mode == 'foe_grave' else self.owner
        self.add('Forest', 'land', owner, 'graveyard')
        self.add('Goblin Tinkerer', 'creature', owner, 'graveyard')
        if self.mode != 'no_match':
            self.add('Unsummon', 'spell', owner, 'graveyard')
            self.add('Darksteel Relic', 'artifact', owner, 'graveyard')

    def setup_cards(self):
        self.exiled = None
        mode = self.mode
        self.add(self.name, 'source', self.owner, 'hand')
        own = DECK
        if self.effect == 'ire':
            top = 'Reach Through Mists' if mode in ('play_spell', 'offturn_spell', 'short_spell') else 'Forest'
            own = () if mode == 'empty_library' else (top, *DECK)
        for player, deck in ((self.owner, own), (self.foe, DECK)):
            for i, name in enumerate(deck):
                self.add(name, f'library_{player}_{i}', player, 'library')
        if self.effect in ('den', 'restricted'):
            self.setup_grave()
        if mode in RAPTOR_RETURN and self.effect == 'raptor':
            self.add('Glacial Stalker', 'trigger', self.foe if mode == 'opponent_flip' else self.owner, 'hand')
            self.add('Murder', 'removal', self.foe, 'hand')
            if mode == 'grave_shroud':
                self.add('Silent Gravestone', 'shroud', self.foe)
        if mode in AMBUSH_WITNESS and self.effect == 'ambush':
            self.add('Sandstorm Charger' if mode == 'megamorph_stack' else 'Glacial Stalker',
                     'trigger', self.foe if mode == 'opponent_flip' else self.owner, 'hand')
            if mode == 'two_watchers':
                self.add(self.name, 'colleague', self.owner, 'hand')
        if mode == 'blink_owner':
            self.add('Bazaar Trader', 'trader', self.owner)
        if mode == 'spent_land_drop':
            self.add('Mountain', 'land_drop', self.owner, 'hand')
        if mode == 'wrong_card':
            self.add('Island', 'foreign_exile', self.owner, 'exile')
        if mode in COMBAT:
            blocker = {'equal_blocker': 'Hill Giant', 'higher_blocker': 'Craw Wurm',
                       'deathtouch': 'Wall of Stone', 'hidden_damage': 'Wall of Stone'}.get(mode, 'Grizzly Bears')
            self.add(blocker, 'blocker', self.foe)
            if mode == 'menace_pair':
                self.add('Hill Giant', 'second_blocker', self.foe)

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        if self.effect == 'raptor' and self.mode in RAPTOR_RETURN:
            self.do_raptor_return()
            return
        if self.effect == 'ambush' and self.mode in AMBUSH_WITNESS:
            self.do_ambush()
            return
        if self.effect == 'shore' and self.mode in EXTRA['shore']:
            self.do_shore()
            return
        mode = self.mode
        self.cast_body('source', down=mode not in ('faceup', 'printed_blocker'), denial=mode == 'hidden_short',
                       choice=mode == 'hidden_choice')
        if mode in ('hidden', 'faceup', 'hidden_short', 'hidden_choice'):
            return
        if mode == 'spent_land_drop':
            self.land_from_hand()
        if mode in ('offturn', 'offturn_land', 'offturn_spell'):
            self.advance()
        if mode in COMBAT:
            self.advance()
            self.advance()
        if mode not in ('hidden_blocker', 'hidden_damage', 'printed_blocker'):
            self.reveal()
        if self.effect == 'ire' and mode in IRE_PERMISSION:
            self.do_permission()
        if mode in COMBAT:
            self.combat()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('unreserved Megamorph source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        scene = MegaScene(name, params, mode, int(owner), rows, oracle)
        scene.play()
        script = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            tail = script[cut:]
            script = script[:cut] + [step('capture')] + tail + [step('record_state'), step('restore')] + tail + [step('assert_recorded_state')]
        gaps = {'ire': ['Menace pair verifies both declared blockers before division of damage.'],
                'raptor': ['Distinct simultaneous Deathmist Raptor copies held for a separately calibrated trigger order.'],
                'ambush': ['Noncreature turn-up control requires a separately calibrated noncreature turning face up.']}.get(params['effect'], [])
        cases.append(dict(name=f'MegamorphZoneEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=script, _recipe=dict(family=FAMILY, version=2, variant=variant,
                             params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
