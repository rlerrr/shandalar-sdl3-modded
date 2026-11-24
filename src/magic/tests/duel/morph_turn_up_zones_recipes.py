"""Paid Morph actions and complete printed zone-changing turn-up effects."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN, MORPH
from morph_turn_up_target_recipes import Scene as MorphScene


FAMILY = 'morph_turn_up_zones'
PROFILE = Path(__file__).parent / 'data/morph_turn_up_zones_wave8_profiles.json'
PREFIX = 'When this creature is turned face up, '
CLAUSES = {
    'you may search your library for a Dragon card, reveal it, put it into your hand, then shuffle.': 'dragon_search',
    "return all Zombie cards from all graveyards to their owners' hands.": 'zombie_return',
    'discard two cards, then draw three cards.': 'discard_draw',
    'you may put a creature card from your hand onto the battlefield.': 'creature_entry',
    "return all other tapped creatures to their owners' hands.": 'tapped_return',
}
COMMON = ('hidden', 'faceup', 'hidden_choice', 'hidden_short', 'flip', 'flip_short', 'flip_wrong',
          'offturn', 'pending_spell', 'replay')
EXTRA = {
    'dragon_search': ('no_match', 'fail_to_find', 'empty_library', 'opponent_only', 'changeling_offer',
                      'decline_search', 'flying_ground', 'flying_reach', 'hidden_combat'),
    'zombie_return': ('only_foe', 'only_own', 'empty_graves', 'nonmatching_zones', 'graveyard_shroud', 'kindred'),
    'discard_draw': ('empty_hand', 'one_card', 'retain_one'),
    'creature_entry': ('no_match', 'decline', 'empty_hand', 'opponent_only', 'entry_draw'),
    'tapped_return': ('no_tapped_creatures', 'tapped_source', 'foreign_owner', 'flying_ground',
                      'flying_reach', 'hidden_combat'),
}
DECK = ('Forest', 'Island', 'Mountain', 'Darksteel Relic')
WITNESSES = set(DECK) | {
    'Bazaar Trader', 'Crib Swap', 'Dragon Whelp', 'Elvish Visionary', 'Festering Goblin', 'Giant Spider',
    'Goblin Tinkerer', 'Scathe Zombies', 'Shivan Dragon', 'Silent Gravestone', 'Twiddle', 'Unsummon',
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record
            or not record['type_line'].startswith('Creature — ')):
        raise ValueError('whole normal Morph creature required')
    lines = record['oracle_text'].splitlines()
    flying = lines[:1] == ['Flying']
    clauses = lines[int(flying):]
    morph = MORPH.fullmatch(clauses[0]) if len(clauses) == 2 else None
    effect = CLAUSES.get(clauses[1].removeprefix(PREFIX)) if morph and clauses[1].startswith(PREFIX) else None
    if (not effect or flying != (effect in ('dragon_search', 'tapped_return')) or
            set(record['keywords']) != ({'Morph', 'Flying'} if flying else {'Morph'})):
        raise ValueError('complete Morph, supported zone clause and reviewed passive required')
    return dict(effect=effect, flying=flying, cast_mana=cost_from_oracle(record),
                flip_mana=cost_from_oracle(dict(record, mana_cost=morph[1])),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in COMMON + EXTRA[params['effect']]
            if owner == 0 or mode != 'decline_search']


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(pins()['sources'].get(record['name'], {}).get('oracle'), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model)):
        raise ValueError('exact complete Morph zone declaration required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('distinct reviewed Morph zone variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if not exact(data['hidden_observer'], rows.get('Face-Down Creature')) or data['hidden_observer']['csvid'] != FACE_DOWN:
        raise ValueError('native face-down observer changed')
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('complete Morph zone witness inventory required')
    for name, pin in (data['sources'] | data['witnesses']).items():
        if (not exact(pin, {'oracle': oracle.get(name), 'registry': rows.get(name)}) or
                oracle[name]['layout'] != 'normal' or rows[name]['kind'] != 'card'):
            raise ValueError(f'Oracle or registry identity changed for {name}')
        if name != 'Giant Spider' and rows[name]['handler'] in (None, 'NULL', '0', 'card_dummy'):
            raise ValueError(f'production handler missing for {name}')


def has_type(record, name):
    return name in record['type_line'].partition(' — ')[0].split()


def has_subtype(record, name):
    return name in record['type_line'].partition(' — ')[2].split() or 'Changeling' in record['keywords']


class ZoneScene(MorphScene):
    def deck(self, owner):
        return [ref for ref, obj in self.entities.items() if obj['zone'] == 'library' and obj['owner'] == owner]

    def cards_in_hand(self, owner):
        return [ref for ref, obj in self.entities.items() if obj['zone'] == 'hand' and obj['owner'] == owner]

    def type_of(self, ref, kind):
        return has_type(self.oracle[self.entities[ref]['name']], kind)

    def subtype_of(self, ref, kind):
        return has_subtype(self.oracle[self.entities[ref]['name']], kind)

    def setup_cards(self):
        self.add(self.name, 'source', self.owner, 'hand')
        if self.mode == 'pending_spell':
            self.add('Unsummon', 'parent', self.foe, 'hand')
        self.deck_shuffles = [0, 0]
        self.search_offer = None
        self.selected = None
        self.expected_discards = []
        self.ordered_deck = self.effect != 'dragon_search'
        self.add_decks()
        setup = {
            'dragon_search': self.setup_search,
            'zombie_return': self.setup_graves,
            'discard_draw': self.setup_discard,
            'creature_entry': self.setup_entry,
            'tapped_return': self.setup_tapped,
        }[self.effect]
        setup()
        if self.mode in ('flying_ground', 'flying_reach', 'hidden_combat'):
            self.add('Giant Spider' if self.mode == 'flying_reach' else 'Goblin Tinkerer', 'blocker', self.foe)

    def add_decks(self):
        own = DECK
        foe = ('Island', 'Forest')
        if self.effect == 'dragon_search':
            if self.mode == 'empty_library':
                own = ()
            elif self.mode in ('no_match', 'opponent_only'):
                own = DECK
            else:
                own = ('Forest', 'Shivan Dragon', 'Goblin Tinkerer', 'Dragon Whelp')
                if self.mode == 'changeling_offer':
                    own += ('Crib Swap',)
            if self.mode == 'opponent_only':
                foe = ('Shivan Dragon', 'Dragon Whelp')
        for who, deck in ((self.owner, own), (self.foe, foe)):
            for i, name in enumerate(deck):
                self.add(name, f'deck_{who}_{i}', who, 'library')

    def setup_search(self):
        self.add('Mountain', 'existing', self.owner, 'hand')

    def setup_graves(self):
        if self.mode == 'graveyard_shroud':
            self.add('Silent Gravestone', 'shroud', self.foe)
        for who in (self.owner, self.foe):
            if self.mode == 'empty_graves':
                continue
            self.add('Goblin Tinkerer', f'other_{who}', who, 'graveyard')
            owns_match = self.mode != 'only_foe' or who == self.foe
            owns_match &= self.mode != 'only_own' or who == self.owner
            if owns_match and self.mode != 'nonmatching_zones':
                prefix = f'zombie_{who}'
                self.add('Scathe Zombies', prefix, who, 'graveyard')
                self.add('Festering Goblin', prefix + '_two', who, 'graveyard')
                if self.mode == 'kindred':
                    self.add('Crib Swap', prefix + '_kindred', who, 'graveyard')
            if self.mode == 'nonmatching_zones':
                self.add('Festering Goblin', f'exiled_{who}', who, 'exile')
                self.add('Scathe Zombies', f'held_{who}', who, 'hand')

    def setup_discard(self):
        count = {'empty_hand': 0, 'one_card': 1, 'retain_one': 3}.get(self.mode, 2)
        cards = ('Darksteel Relic', 'Goblin Tinkerer', 'Shivan Dragon') if self.owner == 0 else ('Darksteel Relic',) * 3
        for i, name in enumerate(cards[:count]):
            self.add(name, f'discard_{i}', self.owner, 'hand')
        self.add('Dragon Whelp', 'foe_hand', self.foe, 'hand')

    def setup_entry(self):
        if self.mode in ('empty_hand', 'opponent_only'):
            if self.mode == 'opponent_only':
                self.add('Shivan Dragon', 'foe_creature', self.foe, 'hand')
            return
        self.add('Darksteel Relic', 'ineligible', self.owner, 'hand')
        if self.mode != 'no_match':
            target = 'Elvish Visionary' if self.mode == 'entry_draw' else 'Shivan Dragon'
            self.add(target, 'entrant', self.owner, 'hand')
            self.add('Goblin Tinkerer', 'other_creature', self.owner, 'hand')

    def setup_tapped(self):
        tapped = self.mode != 'no_tapped_creatures'
        self.add('Goblin Tinkerer', 'victim', self.owner, tapped=tapped)
        self.add('Scathe Zombies', 'foe_tapped', self.foe, tapped=tapped)
        self.add('Festering Goblin', 'foe_untapped', self.foe)
        self.add('Darksteel Relic', 'tapped_artifact', self.owner, tapped=True)
        if self.mode == 'foreign_owner':
            self.add('Bazaar Trader', 'trader', self.owner)
        if self.mode == 'tapped_source':
            self.add('Twiddle', 'tap_spell', self.foe, 'hand')

    def receipts(self):
        super().receipts()
        for who in (0, 1):
            self.emit('assert_shuffle_count', player=who, value=self.deck_shuffles[who])
            if self.ordered_deck:
                for i, ref in enumerate(self.deck(who)):
                    self.emit('assert_zone_card', player=who, zone='library', index=i,
                              value=self.rows[self.entities[ref]['name']]['csvid'])
        for ref, obj in self.entities.items():
            if obj['zone'] == 'battlefield':
                continue
            name, owner, zone = obj['name'], obj['owner'], obj['zone']
            if not any(o['name'] == name and o['owner'] == owner and o['zone'] == 'battlefield'
                       for o in self.entities.values()):
                self.emit('assert_zone_multiplicity', player=owner, zone='battlefield',
                          index=self.rows[name]['csvid'], value=0)

    def select(self, refs, *, selected, suitable):
        if not refs:
            return
        self.search_offer = [(self.rows[self.entities[ref]['name']]['csvid'], int(suitable(ref))) for ref in refs]
        chosen = selected or refs[0]
        self.emit('choose_zone_card' if selected else 'choose_optional_zone', player=self.owner,
                  value=self.rows[self.entities[chosen]['name']]['csvid'])

    def plan_trigger(self):
        if self.effect == 'dragon_search':
            accepted = self.mode != 'decline_search'
            if self.owner == 0:
                self.emit('choose_dialog', player=0, value=int(not accepted))
            if not accepted:
                return
            refs = self.deck(self.owner)
            if self.mode not in ('no_match', 'opponent_only', 'fail_to_find', 'empty_library'):
                self.selected = next(ref for ref in refs if self.entities[ref]['name'] == 'Shivan Dragon')
            self.select(refs, selected=self.selected, suitable=lambda ref: self.subtype_of(ref, 'Dragon'))
            self.deck_shuffles[self.owner] += 1
        elif self.effect == 'creature_entry':
            refs = self.cards_in_hand(self.owner)
            if 'entrant' in refs and self.mode != 'decline':
                self.selected = 'entrant'
            self.select(refs, selected=self.selected, suitable=lambda ref: self.type_of(ref, 'Creature'))
        elif self.effect == 'discard_draw':
            self.expected_discards = self.cards_in_hand(self.owner)[:2]
            if self.owner == 0:
                for ref in self.expected_discards:
                    self.emit('choose_zone_card', player=0, value=self.rows[self.entities[ref]['name']]['csvid'])

    def apply_trigger(self):
        if self.search_offer is not None:
            self.emit('assert_offered_count', value=len(self.search_offer))
            self.emit('assert_offered_selection_required', value=0)
            self.emit('assert_forced_zone_choices', value=0)
            for csvid, legal in sorted(set(self.search_offer)):
                self.emit('assert_offered_legal_card', index=csvid, value=legal)
        if self.effect == 'dragon_search' and self.selected:
            self.leave(self.selected, 'hand')
        elif self.effect == 'creature_entry' and self.selected:
            self.leave(self.selected, 'battlefield')
            self.bind(self.selected)
            if self.mode == 'entry_draw':
                self.leave(self.deck(self.owner)[0], 'hand')
        elif self.effect == 'zombie_return':
            for ref, obj in self.entities.items():
                if obj['zone'] == 'graveyard' and self.subtype_of(ref, 'Zombie'):
                    self.leave(ref, 'hand')
        elif self.effect == 'discard_draw':
            for ref in self.expected_discards:
                self.leave(ref, 'graveyard')
            for ref in self.deck(self.owner)[:3]:
                self.leave(ref, 'hand')
        elif self.effect == 'tapped_return':
            for ref, obj in self.entities.items():
                if ref != 'source' and obj['zone'] == 'battlefield' and obj['tapped'] and self.type_of(ref, 'Creature'):
                    self.leave(ref, 'hand')

    def flip(self):
        fee = self.model['flip_mana']
        funds = list(fee)
        denied = self.mode in ('flip_short', 'flip_wrong')
        if denied:
            color = next(i for i, amount in enumerate(fee) if i and amount)
            funds[color] -= 1
            if self.mode == 'flip_wrong':
                funds[next(i for i in range(1, 6) if i != color)] += 1
        self.fund(self.owner, funds)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate', ref='source', value=int(not denied))
        if not denied:
            self.plan_trigger()
        self.emit('activate', ref='source', value=int(not denied))
        self.emit('assert_unused_choices', value=0)
        if denied:
            self.receipts()
            return
        self.pay(self.owner, fee)
        self.hidden = False
        self.pools()
        self.source_traits()
        pending = self.mode == 'pending_spell'
        self.resolve(pending)
        self.apply_trigger()
        if pending:
            self.leave('parent', 'graveyard')
            self.leave('source', 'hand')
        self.receipts()

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked = self.hidden or self.mode == 'flying_reach'
        self.emit('block', ref='blocker', other='source', value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        attacker = self.entities['source']
        attacker['tapped'] = True
        if blocked:
            blocker = self.entities['blocker']
            blocker['damage'] = self.stats('source')[0]
            attacker['damage'] = self.stats('blocker')[0]
            if blocker['damage'] >= self.stats('blocker')[1]:
                self.leave('blocker', 'graveyard')
        else:
            self.life[self.foe] -= self.stats('source')[0]
        self.receipts()

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        self.emit('observe_search_filters', value=1)
        for who in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=who, color=color, value=0)
        self.emit('refresh')
        if not self.cast_source() or self.mode in ('hidden', 'hidden_choice', 'faceup'):
            return
        if self.mode in ('offturn', 'pending_spell'):
            self.advance()
        if self.mode in ('hidden_combat', 'flying_ground', 'flying_reach'):
            self.advance()
            self.advance()
        if self.mode == 'hidden_combat':
            self.combat()
            return
        if self.mode == 'foreign_owner':
            self.donate('victim')
        if self.mode == 'tapped_source':
            self.cast_spell('tap_spell', 'source')
            self.emit('choose_dialog', player=self.foe, value=0)
            self.resolve()
            self.leave('tap_spell', 'graveyard')
            self.entities['source']['tapped'] = True
            self.receipts()
        if self.mode == 'pending_spell':
            self.cast_spell('parent', 'source')
        self.flip()
        if self.mode in ('flying_ground', 'flying_reach'):
            self.combat()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('unreserved Morph zone source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        scene = ZoneScene(name, params, mode, int(owner), rows, oracle)
        scene.play()
        script = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            script = script[:cut] + [step('capture')] + script[cut:] + [step('record_state'), step('restore')] + script[cut:] + [step('assert_recorded_state')]
        gaps = {
            'dragon_search': ['Independent receipt for public reveal of the selected card.'],
            'discard_draw': ['Game loss from attempting to draw beyond the remaining library.'],
        }.get(params['effect'], [])
        cases.append(dict(name=f'MorphTurnUpZonesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=script, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
