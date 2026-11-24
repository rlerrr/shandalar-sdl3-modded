"""Exercise paid Devoid spells, encounter choices, and observable zone effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counted_battlefield_damage_recipes import Game, exact_pin


FAMILY = 'devoid_zone_and_spell_choices'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
DEVOID = 'Devoid (This card has no color.)'
INGEST = 'Ingest (Whenever this creature deals combat damage to a player, that player exiles the top card of their library.)'
BOUNCE = "Return target spell or creature to its owner's hand."
BRUTAL_DAMAGE = ('Brutal Expulsion deals 2 damage to target creature or planeswalker. '
                 'If that creature or planeswalker would die this turn, exile it instead.')
FIGHT = ('Target creature you control fights target creature an opponent controls. '
         'If the creature an opponent controls would die this turn, exile it instead.')
DEATH_LOOT = 'When this creature dies, you may draw a card. If you do, discard a card.'
SIRE = ("Whenever a land an opponent controls enters, that player exiles the top two cards of their library and you draw two cards.")
TRANSGRESS = ('Target player reveals their hand. You choose a card from it with mana value 3 or greater '
              'and exile that card.')
RUIN = ('Search your library for a basic land card and reveal it. You may put that card onto the battlefield '
        'tapped if you control a land named Wastes. Otherwise, put that card into your hand. Then shuffle.')
WITNESS = 'Target opponent exiles two cards from their hand and loses 2 life.'
SPIKE = ('Serpentine Spike deals 2 damage to target creature, 3 damage to another target creature, and 4 damage '
         'to a third target creature. If a creature dealt damage this way would die this turn, exile it instead.')
INVERT = ('When this creature enters, exile all cards from your library face down, '
          'then shuffle all cards from your graveyard into your library.')
MODELS = {
    ('Choose one or both —', '• ' + BOUNCE, '• ' + BRUTAL_DAMAGE): ('modal', 'brutal_expulsion', ()),
    (INGEST, DEATH_LOOT): ('loot', 'salvage_drone', ('Ingest',)),
    (SIRE,): ('land', 'sire_of_stagnation', ()),
    (TRANSGRESS,): ('threshold', 'transgress_the_mind', ()),
    (FIGHT,): ('fight', 'unnatural_aggression', ()),
    (RUIN,): ('search', 'ruin_on_their_wake', ()),
    (WITNESS,): ('two', 'witness_the_end', ()),
    (SPIKE,): ('spike', 'serpentine_spike', ()),
    ('Flying', INVERT): ('invert', 'inverter_of_truth', ('Flying',)),
}
SCENES = {
    'modal': ('both', 'bounce', 'stack_bounce', 'planeswalker', 'survivor', 'later_death', 'expiry', 'countered', 'short', 'replay'),
    'loot': ('unblocked', 'blocked', 'death_loot', 'short', 'countered', 'replay'),
    'land': ('opposing_land', 'own_land', 'opposing_nonland', 'opposing_short_library', 'short', 'countered', 'replay'),
    'threshold': ('boundary', 'alternate_card', 'no_qualifying', 'empty_hand', 'own_target', 'short', 'countered', 'replay'),
    'fight': ('both_die', 'enemy_survives', 'later_death', 'expiry', 'no_own', 'no_enemy', 'short', 'countered', 'replay'),
    'search': ('to_hand', 'wastes_tapped', 'wastes_decline', 'enemy_wastes', 'no_basic', 'fail_find', 'short', 'countered', 'replay'),
    'two': ('choose_two', 'one_card', 'empty_hand', 'short', 'countered', 'replay'),
    'spike': ('triple', 'lost_middle', 'later_death', 'expiry', 'too_few', 'short', 'countered', 'replay'),
    'invert': ('pool_exchange', 'empty_grave', 'empty_library', 'public_exile_control', 'flying', 'short', 'countered', 'replay'),
}
SUPPORT = {'Plains', 'Island', 'Forest', 'Swamp', 'Mountain', 'Grizzly Bears', 'Hill Giant', 'Divination',
           'Counterspell', 'Wastes', 'Jace Beleren', 'Murder', 'Darksteel Citadel', 'Giant Spider',
           'Dazzling Ramparts', 'Unsummon', 'Oracle of Dust', 'Wind Drake'}
VANILLA = {'Plains', 'Island', 'Forest', 'Swamp', 'Mountain', 'Grizzly Bears', 'Hill Giant', 'Giant Spider', 'Wind Drake'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face') or record['colors'] or lines[:1] != [DEVOID]:
        raise ValueError('expected a whole printed colorless Devoid card')
    matched = MODELS.get(tuple(lines[1:]))
    if not matched:
        raise ValueError('unsupported complete Devoid rules text')
    model, _, keywords = matched
    actual = record['keywords']
    if 'Devoid' not in actual or set(actual) != {'Devoid', *keywords, *({'Fight'} if model == 'fight' and 'Fight' in actual else ())}:
        raise ValueError('unexpected keyword in pinned Devoid card')
    card_type = record['type_line'].split(' — ')[0]
    if card_type not in ('Creature', 'Instant', 'Sorcery'):
        raise ValueError('unexpected Devoid source type')
    return dict(model=model, mana=cost_from_oracle(record), kind=card_type.lower())


def handler(record):
    return 'manalinkeh_card_' + MODELS[tuple(record['oracle_text'].splitlines()[1:])][1]


def variants(shape):
    options = [f'{mode}_{owner}' for mode in SCENES[shape['model']] for owner in (0, 1)]
    if shape['model'] == 'loot':
        options.append('decline_0')
    return options


def spec(decl, record):
    shape = classify(record)
    source = profiles()['sources'].get(record['name'])
    chosen = decl.get('variants')
    if (not source or not exact_pin(source['oracle'], record) or decl.get('family') != FAMILY
            or type(decl.get('version')) is not int or decl['version'] != 2
            or decl.get('oracle_text') != record['oracle_text'] or not exact_pin(decl.get('params'), shape)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('pinned full Devoid source and approved variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pinned = profiles()
    if pinned['version'] != 1 or not pinned['sources'] or set(pinned['witnesses']) != SUPPORT:
        raise ValueError('complete Devoid witness pins required')
    for group in ('sources', 'witnesses'):
        for name, pin in pinned[group].items():
            record, row = oracle[name], rows[name]
            if (not exact_pin(pin, dict(oracle=record, registry=row)) or record['layout'] != 'normal'
                    or row['kind'] != 'card' or row['handler'] in (None, '0', 'NULL')
                    or (row['handler'] == 'card_dummy' and name not in VANILLA)):
                raise ValueError(f'mismatched pinned Devoid fixture card {name}')
            if group == 'sources' and row['handler'] != handler(record):
                raise ValueError(f'mismatched implemented primary {name}')


class ChoiceBoard(Game):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.active = owner
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        model, mode, owner, foe = self.shape['model'], self.mode, self.owner, self.foe
        if model == 'modal':
            if mode == 'stack_bounce':
                self.add('Divination', 'parent', foe, 'hand')
            else:
                self.add('Grizzly Bears', 'first', foe)
                self.add('Jace Beleren' if mode == 'planeswalker' else 'Hill Giant' if mode in
                         ('survivor', 'later_death', 'expiry') else 'Grizzly Bears', 'second', foe,
                         'hand' if mode == 'planeswalker' else 'battlefield')
                if mode in ('later_death', 'expiry'):
                    self.add('Murder', 'removal', owner, 'hand')
        elif model == 'fight':
            if mode != 'no_own':
                self.add('Grizzly Bears', 'first', owner)
            if mode != 'no_enemy':
                self.add('Grizzly Bears' if mode == 'both_die' else 'Giant Spider', 'second', foe)
            if mode in ('later_death', 'expiry'):
                self.add('Murder', 'removal', owner, 'hand')
        elif model in ('threshold', 'two'):
            letters = {
                'threshold': ('Grizzly Bears', 'Divination', 'Hill Giant'),
                'two': ('Plains', 'Grizzly Bears', 'Divination'),
            }
            hand = () if mode == 'empty_hand' else ('Mountain', 'Grizzly Bears') if mode == 'no_qualifying' else (
                ('Island',) if mode == 'one_card' else letters[model])
            target = owner if mode == 'own_target' else foe
            for index, name in enumerate(hand):
                self.add(name, f'candidate_{index}', target, 'hand')
            self.add('Swamp', 'kept', 1-target, 'hand')
        elif model == 'land':
            self.add('Forest', 'entry', owner if mode == 'own_land' else foe,
                     'library' if mode == 'opposing_nonland' else 'hand')
            if mode == 'opposing_nonland':
                self.add('Grizzly Bears', 'nonland', foe, 'hand')
            self.library(owner, 'Island', 'Plains', 'Mountain')
            self.library(foe, *('Swamp',) if mode == 'opposing_short_library' else ('Swamp', 'Forest', 'Island'))
        elif model == 'loot':
            if owner == 0 or mode not in ('blocked', 'death_loot'):
                self.add('Forest', 'discard', owner, 'hand')
            self.library(owner, 'Island', 'Plains', 'Mountain')
            self.library(foe, 'Swamp', 'Grizzly Bears', 'Island')
            if mode == 'blocked':
                self.add('Grizzly Bears', 'blocker', foe)
        elif model == 'search':
            if mode in ('wastes_tapped', 'wastes_decline', 'replay', 'enemy_wastes'):
                self.add('Wastes', 'wastes', foe if mode == 'enemy_wastes' else owner)
            self.library(owner, 'Darksteel Citadel', 'Hill Giant', *(() if mode == 'no_basic' else ('Forest',)))
        elif model == 'spike':
            alive = mode in ('later_death', 'expiry')
            self.add('Hill Giant' if alive else 'Grizzly Bears', 'second', foe)
            self.add('Giant Spider' if alive else 'Hill Giant', 'third', owner)
            if mode != 'too_few':
                self.add('Dazzling Ramparts' if alive else 'Giant Spider', 'fourth', foe)
            if mode in ('later_death', 'expiry'):
                self.add('Murder', 'removal', owner, 'hand')
            if mode == 'lost_middle':
                self.add('Unsummon', 'bounce', foe, 'hand')
        elif model == 'invert':
            if mode not in ('empty_library', 'public_exile_control'):
                self.library(owner, 'Island', 'Plains', 'Mountain')
            if mode != 'empty_grave':
                self.add('Swamp', 'grave_one', owner, 'graveyard')
                self.add('Forest', 'grave_two', owner, 'graveyard')
            self.library(foe, 'Mountain', 'Island', 'Plains')
            if mode in ('pool_exchange', 'public_exile_control', 'replay'):
                self.add('Oracle of Dust', 'processor', foe)
            if mode == 'public_exile_control':
                self.add('Island', 'ordinary_exile', owner, 'exile')
            if mode == 'flying':
                self.add('Grizzly Bears', 'ground', foe)
                self.add('Wind Drake', 'air', foe)
        if mode == 'countered':
            self.add('Counterspell', 'counter', foe, 'hand')

    def library(self, seat, *cards):
        for index, name in enumerate(cards):
            self.add(name, f'library_{seat}_{index}', seat, 'library')

    def in_zone(self, seat, zone):
        return [ref for ref, card in self.cards.items() if card['player'] == seat and card['zone'] == zone]

    def select_zone(self, seat, ref):
        self.emit('choose_zone_card', player=seat, value=self.rows[self.cards[ref]['name']]['csvid'])

    def cast_card(self, ref, *, targets=(), mode=None, response=False, allowed=True, denied=False):
        from recipes import cost_from_oracle

        item = self.cards[ref]
        who = item['player']
        fee = cost_from_oracle(self.oracle[item['name']])
        pool = [0] * 6 if denied else fee
        self.fund(who, pool)
        if mode is not None:
            self.emit('choose_dialog', player=who, value=mode)
        for target in targets:
            if type(target) is int:
                self.emit('choose_player', player=who, value=target)
            else:
                self.emit('choose_target', player=who, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed) + int(response))
        if allowed:
            self.pools[who] = [0] * 6
            item['zone'] = 'stack'
            if ref == 'source':
                self.assert_property(ref, 'color', 0)
        self.observe_mana()

    def resolve_card(self, ref, destination='graveyard', *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_unused_choices', value=0)
        self.cards[ref]['zone'] = destination

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def assert_property(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def count_card(self, seat, zone, name, value):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=value)

    def facts(self, *, ordered=True):
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.observe_mana()
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                refs = self.in_zone(seat, zone)
                tally = Counter(self.cards[ref]['name'] for ref in refs)
                self.emit('assert_zone', player=seat, zone=zone, value=len(refs))
                # Zone size plus multiplicities checks exact content, including duplicates.
                for name, count in sorted(tally.items()):
                    self.count_card(seat, zone, name, count)
                if zone == 'library' and ordered:
                    for index, ref in enumerate(refs):
                        self.emit('assert_zone_card', player=seat, zone=zone, index=index,
                                  value=self.rows[self.cards[ref]['name']]['csvid'])
            self.emit('assert_poison', player=seat, value=0)
        source = self.cards['source']
        if source['zone'] == 'battlefield':
            self.emit('assert_matching_permanents', player=self.owner, index=self.rows[source['name']]['csvid'],
                      property='color', value=0)
            for prop in ('power', 'toughness'):
                self.assert_property('source', prop, int(self.oracle[source['name']][prop]))

    def advance(self):
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        self.emit('turn', player=self.active, value=0)

    def destroy(self, target, dest):
        self.emit('destroy', ref=target)
        self.move(target, dest)
        self.emit('assert_unused_choices', value=0)

    def finish_damage(self, first, second, incoming, outgoing, destination):
        for ref, amount, dest in ((first, incoming, 'graveyard'), (second, outgoing, destination)):
            if not ref:
                continue
            toughness = int(self.oracle[self.cards[ref]['name']]['toughness'])
            if amount >= toughness:
                self.move(ref, dest)
            else:
                self.assert_property(ref, 'damage', amount)

    def lethality_tail(self):
        if self.mode not in ('later_death', 'expiry'):
            return
        if self.mode == 'expiry':
            self.emit('cleanup')
            self.emit('refresh')
            self.assert_property('second', 'damage', 0)
        self.cast_card('removal', targets=('second',))
        self.resolve_card('removal')
        self.move('second', 'graveyard' if self.mode == 'expiry' else 'exile')

    def modal_targets(self):
        if self.mode in ('both', 'replay'):
            return (2, ('first', 'second'))
        if self.mode in ('bounce', 'stack_bounce'):
            return (0, ('parent' if self.mode == 'stack_bounce' else 'first',))
        return (1, ('second',))

    def modal(self):
        mode, targets = self.modal_targets()
        if self.mode == 'stack_bounce':
            self.advance()
            self.cast_card('parent')
            self.emit('announce_spell', ref='parent')
            # Only bounce is legal without any creature/planeswalker on board.
            self.cast_card('source', targets=targets, mode=mode, response=True)
            self.resolve_card('source', response=True)
            self.move('parent', 'hand')
            return
        if self.mode == 'planeswalker':
            self.advance()
            self.cast_card('second')
            self.resolve_card('second', 'battlefield')
            self.assert_property('second', 'in_play', 1)
            self.emit('assert_counter', ref='second', counter_kind='loyalty', value=3)
        self.cast_card('source', targets=targets, mode=mode)
        self.resolve_card('source')
        if mode in (0, 2):
            self.move('first', 'hand')
        if mode in (1, 2):
            if self.mode == 'planeswalker':
                self.emit('assert_counter', ref='second', counter_kind='loyalty', value=1)
            else:
                self.finish_damage(None, 'second', 0, 2, 'exile')
        self.lethality_tail()

    def fight(self):
        if self.mode in ('no_own', 'no_enemy'):
            self.fund(self.owner, self.shape['mana'])
            self.emit('can_cast', ref='source', value=0)
            return
        self.cast_card('source', targets=('first', 'second'))
        self.resolve_card('source')
        power = int(self.oracle[self.cards['second']['name']]['power'])
        self.finish_damage('first', 'second', power, 2, 'exile')
        self.lethality_tail()

    def spike(self):
        if self.mode == 'too_few':
            self.fund(self.owner, self.shape['mana'])
            self.emit('can_cast', ref='source', value=0)
            return
        self.cast_card('source', targets=('second', 'third', 'fourth'))
        lost = self.mode == 'lost_middle'
        if lost:
            self.emit('announce_spell', ref='source')
            self.cast_card('bounce', targets=('third',), response=True)
            self.resolve_card('bounce', response=True)
            self.move('third', 'hand')
        self.resolve_card('source')
        for ref, damage in zip(('second', 'third', 'fourth'), (2, 3, 4)):
            if not (lost and ref == 'third'):
                self.finish_damage(None, ref, 0, damage, 'exile')
        self.lethality_tail()

    def invert(self):
        if 'processor' in self.cards and self.mode != 'public_exile_control':
            # Record the actual availability before the ETB to isolate any
            # mistakenly published cards after the effect.
            self.fund(self.foe, [2, 0, 0, 0, 0, 0])
            self.emit('can_activate', ref='processor', value=0)
        self.cast_card('source')
        self.resolve_card('source', 'battlefield')
        for ref in self.in_zone(self.owner, 'library'):
            # Hidden exile has no entry in the public RFG zone. The opposing
            # processor supplies an independent behavioral visibility check.
            self.move(ref, 'hidden_exile')
        grave = self.in_zone(self.owner, 'graveyard')
        for ref in grave:
            self.move(ref, 'library')
        self.emit('assert_shuffle_count', player=self.owner, value=int(bool(grave)))
        if 'processor' in self.cards:
            self.fund(self.foe, [2, 0, 0, 0, 0, 0])
            self.emit('can_activate', ref='processor', value=int(self.mode == 'public_exile_control'))
        if self.mode == 'flying':
            self.advance()
            self.advance()
            self.emit('begin_attackers')
            self.emit('attack', ref='source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', ref='ground', other='source', value=0)
            self.emit('block', ref='air', other='source', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.move('air', 'graveyard')
            self.assert_property('source', 'damage', 2)

    def hand_spell(self):
        target = self.owner if self.mode == 'own_target' else self.foe
        threshold = self.shape['model'] == 'threshold'
        # The explicit target-player spell chooses. Witness's legacy
        # GS_CAN_ONLY_TARGET_OPPONENT path sets the sole opponent directly.
        self.emit('observe_search_filters', value=1)
        self.cast_card('source', targets=(target,) if threshold else ())
        offered = self.in_zone(target, 'hand')
        if threshold and offered:
            valid = [ref for ref in offered if self.oracle[self.cards[ref]['name']]['cmc'] >= 3]
            if valid:
                choice = valid[-1] if self.mode == 'alternate_card' else valid[0]
                self.select_zone(self.owner, choice)
                self.move(choice, 'exile')
            else:
                self.emit('choose_optional_zone', player=self.owner, value=self.rows[self.cards[offered[0]]['name']]['csvid'])
        if not threshold:
            # The affected player chooses, including when the caster is seat 0.
            order = offered[-2:][::-1]
            for ref in order:
                self.select_zone(target, ref)
                self.move(ref, 'exile')
            self.life[target] -= 2
        self.resolve_card('source')
        self.emit('assert_forced_zone_choices', value=0)
        if threshold and offered:
            self.emit('assert_offered_count', value=len(offered))
            self.emit('assert_offered_selection_required', value=int(bool(valid)))
            for ref in offered:
                self.emit('assert_offered_legal_card', index=self.rows[self.cards[ref]['name']]['csvid'],
                          value=int(ref in valid))

    def search(self):
        offered = self.in_zone(self.owner, 'library')
        valid = [ref for ref in offered if self.oracle[self.cards[ref]['name']]['type_line'].startswith('Basic Land')]
        self.emit('observe_search_filters', value=1)
        self.cast_card('source')
        found = bool(valid) and self.mode != 'fail_find'
        if found:
            target = valid[0]
            self.select_zone(self.owner, target)
        else:
            self.emit('choose_zone', player=self.owner, value=-1)
        own_wastes = self.mode in ('wastes_tapped', 'wastes_decline', 'replay')
        deploy = self.mode in ('wastes_tapped', 'replay')
        if found and own_wastes:
            if self.owner == 1:
                # The legacy TUTOR_HAND search announces the chosen card
                # before the Wastes decision. A pending seat-1 dialog intent
                # would otherwise be consumed by this informational notice.
                self.emit('choose_dialog', player=self.owner, value=0)
            self.emit('choose_dialog', player=self.owner, value=int(not deploy))
        self.resolve_card('source')
        self.emit('assert_shuffle_count', player=self.owner, value=1)
        self.emit('assert_offered_count', value=len(offered))
        self.emit('assert_offered_selection_required', value=0)
        for ref in offered:
            self.emit('assert_offered_legal_card', index=self.rows[self.cards[ref]['name']]['csvid'],
                      value=int(ref in valid))
        if found:
            self.move(target, 'battlefield' if deploy else 'hand')
            if deploy:
                self.emit('bind_permanent', ref=target, player=self.owner, value=self.rows[self.cards[target]['name']]['csvid'], index=-1)
                self.assert_property(target, 'tapped', 1)

    def land_trigger(self):
        self.cast_card('source')
        self.resolve_card('source', 'battlefield')
        self.facts()
        if self.mode != 'own_land':
            self.advance()
        if self.mode == 'opposing_nonland':
            self.cast_card('nonland')
            self.resolve_card('nonland', 'battlefield')
            return
        self.emit('play_land', ref='entry', value=1)
        self.emit('resolve')
        entrant = self.cards['entry']['player']
        self.move('entry', 'battlefield')
        if entrant == self.foe:
            for ref in self.in_zone(self.foe, 'library')[:2]:
                self.move(ref, 'exile')
            for ref in self.in_zone(self.owner, 'library')[:2]:
                self.move(ref, 'hand')

    def loot(self):
        self.cast_card('source')
        self.resolve_card('source', 'battlefield')
        self.facts()
        if self.mode in ('unblocked', 'blocked', 'replay'):
            self.advance()
            self.advance()
            block = self.mode == 'blocked'
            self.emit('begin_attackers')
            self.emit('attack', ref='source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            if block:
                self.emit('block', ref='blocker', other='source', value=1)
                self.death_choices()
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            if block:
                self.move('source', 'graveyard')
            else:
                self.life[self.foe] -= 1
                self.move(self.in_zone(self.foe, 'library')[0], 'exile')
            self.emit('assert_unused_choices', value=0)
        else:
            self.death_choices()
            self.destroy('source', 'graveyard')

    def death_choices(self):
        if self.mode == 'decline':
            return
        if self.owner == 0:
            self.emit('choose_trigger', player=self.owner, ref='source')
            self.emit('discard_policy', value=0)
        draw = self.in_zone(self.owner, 'library')[0]
        self.move(draw, 'hand')
        if self.owner == 0:
            self.select_zone(self.owner, 'discard')
            self.move('discard', 'graveyard')
        else:
            # The stock AI discard handler chooses without a zone menu.
            # The drawn card is its only eligible discard.
            self.move(draw, 'graveyard')

    def run(self):
        if self.mode == 'short':
            self.cast_card('source', allowed=False, denied=True)
        elif self.mode == 'countered':
            model = self.shape['model']
            targets = ('first', 'second') if model == 'fight' else (self.foe,) if model == 'threshold' else ()
            dialog = None
            if model == 'modal':
                dialog, targets = 0, ('first',)
            if model == 'spike':
                targets = ('second', 'third', 'fourth')
            self.cast_card('source', targets=targets, mode=dialog)
            self.emit('announce_spell', ref='source')
            self.cast_card('counter', response=True)
            self.resolve_card('counter', response=True)
            self.move('source', 'graveyard')
        else:
            action = {'modal': self.modal, 'fight': self.fight, 'threshold': self.hand_spell,
                      'two': self.hand_spell, 'search': self.search, 'land': self.land_trigger,
                      'loot': self.loot, 'spike': self.spike, 'invert': self.invert}[self.shape['model']]
            action()
        self.facts(ordered=self.shape['model'] not in ('search', 'invert'))


def expand(name, scvid, declaration, shape, selected, rows, oracle):
    out = []
    for label in selected:
        mode, actor = label.rsplit('_', 1)
        world = ChoiceBoard(name, shape, mode, int(actor), rows, oracle)
        world.emit('turn', player=world.owner, value=0)
        world.emit('present_ai_dialogs', value=1)
        world.emit('discard_policy', value=1)
        for seat in (0, 1):
            world.fund(seat, [0] * 6)
        start = len(world.steps)
        world.run()
        if mode == 'replay':
            from recipes import step
            pre, sequence = world.steps[:start], world.steps[start:]
            world.steps = pre + [step('capture')] + sequence + [
                step('record_state'), step('restore'), step('ai_roundtrip'),
            ] + sequence + [step('assert_recorded_state')]
        coverage = []
        if shape['model'] == 'search':
            coverage.append('public reveal of the hand-destination search selection is not directly observable')
        if shape['model'] == 'loot':
            coverage.append('AI policy controls whether to decline the optional death trigger for seat 1')
        out.append(dict(name=f'DevoidZoneAndSpellChoicesRecipe_{scvid}_{label}', card=name, fixture='empty_main', seed=123,
                        setup=world.setup, steps=world.steps, _recipe=dict(
                            family=FAMILY, version=2, variant=label, params=declaration['params'],
                            oracle_text=declaration['oracle_text'], untested_components=coverage)))
    return out
