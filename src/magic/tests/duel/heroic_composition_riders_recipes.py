"""Heroic compositions for sacrifice, animation, copying, discard and linked exile."""

from collections import Counter
import copy
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from heroic_case_support import HeroicBoard, heroic_rules


FAMILY = 'heroic_composition_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
PREFIX = 'Heroic — Whenever you cast a spell that targets '
MODELS = {
    ('Deathtouch', PREFIX + 'this creature, each opponent sacrifices a creature of their choice.'): 'sacrifice',
    (PREFIX + "Anthousa, up to three target lands you control each become 2/2 Warrior creatures until end of turn. They're still lands.",): 'lands',
    (PREFIX + 'this creature, you may have this creature become a copy of target creature, except it has this ability.',): 'copy',
    (PREFIX + 'this creature, each opponent discards a card.',): 'discard',
    (PREFIX + 'this creature, look at the top three cards of your library. You may reveal an enchantment card from among them and put it into your hand. Put the rest on the bottom of your library in any order.',): 'selection',
    (PREFIX + "this creature, exile the top three cards of target player's library.",
     '{U}{U}, Sacrifice this creature: Counter target spell with the same name as a card exiled with this creature.'): 'linked',
}
COMMON = ('own', 'enemy', 'other_target', 'replay')
MODES = {
    'sacrifice': ('no_victim', 'deathtouch'),
    'lands': ('fewer', 'zero', 'warrior'),
    'copy': ('decline', 'copy_again'),
    'discard': ('empty', 'two_cards'),
    'selection': ('decline', 'no_enchantment', 'short_library'),
    'linked': ('own_library', 'short_library', 'counter', 'nonmatch', 'short_payment', 'unlinked'),
}
WITNESSES = {'Defiant Strike', 'Grizzly Bears', 'Giant Spider', 'Hill Giant', 'Chief of the Edge',
             'Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Fervor', 'Mind Stone',
             'Think Twice', 'Visions of Beyond'}
STATIC = {'Grizzly Bears', 'Giant Spider', 'Hill Giant'}
BASICS = ('Forest', 'Mountain', 'Plains', 'Island')
WATCHED_ZONES = ('battlefield', 'hand', 'graveyard', 'library', 'exile')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    model = MODELS.get(tuple(heroic_rules(record)))
    stats = {'sacrifice': ('3', '2'), 'lands': ('4', '5'), 'copy': ('1', '1'),
             'discard': ('1', '3'), 'selection': ('1', '3'), 'linked': ('2', '1')}.get(model)
    keywords = {'Heroic', 'Deathtouch'} if model == 'sacrifice' else {'Heroic'}
    if model is None or stats != (record.get('power'), record.get('toughness')) or set(record['keywords']) != keywords:
        raise ValueError('All printed Heroic clauses, keywords and stats are required')
    return dict(model=model, cost=cost_from_oracle(record))


def variants(params):
    return [f'{mode}_{who}' for who in (0, 1) for mode in COMMON + MODES[params['model']]]


def spec(declaration, record):
    model = parameters(record)
    pins = profiles()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(pins.get('oracle'), record)
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(model, declaration.get('params'))):
        raise ValueError('The exact whole pinned primary Oracle record is required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('Explicit distinct reviewed Heroic variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if (set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES
            or len(data['sources']) != len(MODELS)):
        raise ValueError('Complete Heroic source and witness pins required')
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows[name]
        if (not exact(oracle[name], pin['oracle']) or not exact(row, pin['registry'])
                or row['kind'] != 'card' or row['handler'] in ('0', 'NULL', None)
                or (row['handler'] == 'card_dummy' and name not in STATIC)):
            raise ValueError('Heroic source or witness changed: ' + name)
    if (oracle['Chief of the Edge']['oracle_text'] != 'Other Warrior creatures you control get +1/+0.'
            or oracle['Defiant Strike']['oracle_text'] != 'Target creature gets +1/+0 until end of turn.\nDraw a card.'):
        raise ValueError('Independent Warrior and paid spell witnesses changed')


class Composition(HeroicBoard):
    def build(self):
        self.model = self.shape['model']
        self.libraries = [[], []]
        self.form = None
        self.boosts = Counter()
        self.replay = None
        self.animated = []
        self.land_refs = []
        self.riders = self.mode not in ('enemy', 'other_target')
        own_deck = ['Island', 'Plains', 'Mountain']
        foe_deck = ['Mountain', 'Swamp', 'Island', 'Plains', 'Forest']

        if self.mode == 'other_target':
            self.add('Grizzly Bears', 'other', self.owner)
        if self.model == 'sacrifice':
            self.add('Grizzly Bears', 'ally', self.owner)
            if self.mode != 'no_victim':
                self.add('Giant Spider' if self.mode == 'deathtouch' else 'Grizzly Bears', 'victim', self.foe)
                if self.mode == 'own':
                    self.add('Grizzly Bears', 'other_victim', self.foe)
        elif self.model == 'lands':
            self.add('Swamp', 'foreign_land', self.foe)
            for i, name in enumerate(BASICS):
                ref = f'land_{i}'
                self.add(name, ref, self.owner)
                self.land_refs.append(ref)
            if self.mode == 'warrior':
                self.add('Chief of the Edge', 'warrior_lord', self.owner)
        elif self.model == 'copy':
            self.add('Hill Giant', 'copy_target', self.foe)
            self.add('Giant Spider', 'copy_next', self.owner)
            if self.mode == 'copy_again':
                # Select the original card image on the still-live attached trigger.
                self.add(self.cards['source']['name'], 'printed_identity', self.owner, 'hand')
        elif self.model == 'discard':
            if self.mode != 'empty':
                self.add('Forest', 'victim_card', self.foe, 'hand')
            if self.mode == 'two_cards':
                self.add('Forest', 'other_victim_card', self.foe, 'hand')
        elif self.model == 'selection':
            own_deck = {
                'no_enchantment': ['Hill Giant', 'Mind Stone', 'Forest', 'Island', 'Plains'],
                'short_library': ['Fervor', 'Island'],
            }.get(self.mode, ['Hill Giant', 'Fervor', 'Forest', 'Island', 'Plains', 'Mountain'])
        elif self.model == 'linked':
            if self.mode == 'own_library':
                own_deck += ['Forest', 'Swamp']
            if self.mode == 'short_library':
                foe_deck = ['Forest', 'Swamp']
            elif self.mode != 'unlinked':
                foe_deck = ['Think Twice', *foe_deck]
            if self.mode == 'unlinked':
                self.add('Think Twice', 'unrelated_exile', self.foe, 'exile')
            if self.mode in ('counter', 'short_payment', 'nonmatch', 'unlinked'):
                spell = 'Visions of Beyond' if self.mode == 'nonmatch' else 'Think Twice'
                self.add(spell, 'incoming', self.foe, 'hand')
        for who, deck in ((self.owner, own_deck), (self.foe, foe_deck)):
            for i, name in enumerate(deck):
                ref = f'library_{who}_{i}'
                self.add(name, ref, who, 'library')
                self.libraries[who].append(ref)
        if self.mode == 'copy_again':
            self.add('Defiant Strike', 'trigger', self.owner, 'hand')
            self.add('Defiant Strike', 'second', self.owner, 'hand')

    def identity(self, ref):
        if ref == 'source' and self.form and self.cards[ref]['zone'] == 'battlefield':
            return self.form
        return self.cards[ref]['name']

    def draw(self, actor):
        ref = self.libraries[actor].pop(0)
        self.cards[ref]['zone'] = 'hand'

    def stat(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def ptr(self, ref):
        record = self.oracle[self.identity(ref)]
        power, toughness = int(record['power']), int(record['toughness'])
        if ref == 'source' and self.model == 'lands' and self.mode == 'warrior':
            power += 1
        return power + self.boosts[ref], toughness

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        identities = {self.identity(ref) for ref in self.cards}
        identities.add(self.cards['source']['name'])
        quantities = Counter((entry['player'], entry['zone'], self.identity(ref))
                             for ref, entry in self.cards.items())
        for who in (0, 1):
            self.emit('assert_life', player=who, value=20)
            for zone in WATCHED_ZONES:
                self.emit('assert_zone', player=who, zone=zone,
                          value=sum(n for (seat, area, _), n in quantities.items() if (seat, area) == (who, zone)))
                for name in sorted(identities):
                    self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'],
                              value=quantities[who, zone, name])
            for i, ref in enumerate(self.libraries[who]):
                self.emit('assert_zone_card', player=who, zone='library', index=i,
                          value=self.rows[self.identity(ref)]['csvid'])
        for ref in ('source', 'other'):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                power, toughness = self.ptr(ref)
                self.stat(ref, 'power', power)
                self.stat(ref, 'toughness', toughness)

    def after_announce(self, target):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        power, toughness = self.ptr(target)
        self.stat(target, 'power', power)
        self.stat(target, 'toughness', toughness)

    def sacrifice_choice(self):
        if self.mode == 'no_victim':
            return
        victim = 'other_victim' if self.mode == 'own' and self.foe == 0 else 'victim'
        if self.foe == 0:
            self.emit('choose_target', ref=victim, player=self.foe)
        self.cards[victim]['zone'] = 'graveyard'

    def deathtouch_combat(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner)
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', ref='victim', other='source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.cards['source']['zone'] = self.cards['victim']['zone'] = 'graveyard'

    def land_choices(self):
        count = {'fewer': 2, 'zero': 0}.get(self.mode, 3)
        self.animated = self.land_refs[:count]
        for ref in self.animated:
            self.emit('choose_target', ref=ref, player=self.owner)
        if count < 3:
            self.emit('choose_cancel_target', player=self.owner)

    def land_receipts(self):
        for ref in [*self.land_refs, 'foreign_land']:
            animated = ref in self.animated
            self.emit('can_target_creature', ref='source', other=ref, value=int(animated))
            if animated:
                self.stat(ref, 'power', 3 if self.mode == 'warrior' else 2)
                self.stat(ref, 'toughness', 2)

    def land_window(self):
        self.land_receipts()
        if self.mode == 'warrior':
            # The first animated land is the established Forest on our battlefield.
            self.stat('land_0', 'csvid', self.rows['Forest']['csvid'])
            self.emit('activate', ref='land_0', value=1)
            self.pools[self.owner][3] += 1
            self.observe_mana()
            self.stat('land_0', 'tapped', 1)
        elif self.animated:
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.boosts.clear()
            self.animated.clear()
            self.land_receipts()
            self.receipts()

    def copy_choice(self, again):
        if self.mode == 'decline':
            if self.owner == 1:
                self.emit('choose_cancel_target', player=self.owner)
            return
        target = 'copy_next' if again else 'copy_target'
        if self.owner == 0:
            self.emit('choose_trigger', player=self.owner, ref='printed_identity' if again else 'source')
        self.emit('choose_target', player=self.owner, ref=target)
        self.form = self.cards[target]['name']

    def discard_choice(self):
        if self.mode == 'empty':
            return
        if self.foe == 0:
            self.emit('choose_zone_card', player=self.foe, value=self.rows['Forest']['csvid'])
        self.cards['victim_card']['zone'] = 'graveyard'

    def selection_choices(self):
        deck = self.libraries[self.owner]
        window = deck[:3]
        chosen = next((ref for ref in window if self.cards[ref]['name'] == 'Fervor'), None)
        if self.mode == 'decline':
            chosen = None
        self.emit('observe_search_filters', value=1)
        if chosen:
            self.emit('choose_zone_card', player=self.owner, value=self.rows['Fervor']['csvid'])
            if self.owner == 0:
                self.emit('choose_dialog', player=self.owner, value=0)
            self.cards[chosen]['zone'] = 'hand'
        else:
            self.emit('choose_optional_zone', player=self.owner, value=self.rows[self.cards[window[0]]['name']]['csvid'])
        remainder = list(reversed([ref for ref in window if ref != chosen]))
        for ref in remainder[:-1]:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[self.cards[ref]['name']]['csvid'])
        deck[:] = deck[len(window):] + remainder

    def linked_choices(self):
        who = self.owner if self.mode == 'own_library' else self.foe
        self.emit('choose_player', player=self.owner, value=who)
        deck = self.libraries[who]
        for ref in deck[:3]:
            self.cards[ref]['zone'] = 'exile'
        del deck[:3]

    def linked_response(self):
        mode = self.mode
        if 'incoming' not in self.cards:
            return
        self.cast('incoming')
        self.emit('announce_spell', ref='incoming')
        self.fund(self.owner, [0, 0, 1 if mode == 'short_payment' else 2, 0, 0, 0])
        counters = mode == 'counter'
        self.emit('activate_response', ref='source', value=int(counters))
        if counters:
            self.cards['source']['zone'] = 'graveyard'
            self.pools[self.owner][2] -= 2
            self.stat('source', 'in_play', 0)
            self.emit('assert_stack', value=2)
        else:
            self.stat('source', 'in_play', 1)
            self.emit('assert_stack', value=1)
        self.observe_mana()
        # The counter must resolve inside the real parent spell's context.
        self.emit('resolve_response' if counters else 'resolve')
        self.cards['incoming']['zone'] = 'graveyard'
        if not counters:
            self.draw(self.foe)
        self.receipts()

    def choices(self, board, *, again=False):
        if self.model == 'sacrifice':
            self.sacrifice_choice()
        elif self.model == 'lands':
            self.land_choices()
        elif self.model == 'copy':
            self.copy_choice(again)
        elif self.model == 'discard':
            self.discard_choice()
        elif self.model == 'selection':
            self.selection_choices()
        else:
            self.linked_choices()

    def driver(self, *, again=False):
        from functools import partial

        target = 'other' if self.mode == 'other_target' else 'source'
        self.heroic_spell(self.mode != 'enemy', target=target, spell='second' if again else 'trigger',
                          choices=partial(self.choices, again=again) if self.riders else None,
                          before_resolve=lambda board: board.after_announce(target))
        self.draw(self.foe if self.mode == 'enemy' else self.owner)
        self.boosts[target] += 1
        self.receipts()
        if self.model == 'selection':
            self.emit('assert_forced_zone_choices', value=0)

    def run(self):
        self.emit('present_ai_dialogs', value=1)
        self.play_source()
        if self.mode == 'deathtouch':
            self.deathtouch_combat()
            self.receipts()
            return
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.driver()
        if self.model == 'lands':
            self.land_window()
        if self.mode == 'copy_again':
            self.driver(again=True)
        if self.model == 'linked':
            self.linked_response()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params)):
        raise ValueError('Only explicitly reviewed registered Heroic variants may expand')
    out = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        game = Composition(name, params, mode, int(player), rows, oracle)
        game.run()
        steps = game.steps
        if game.replay is not None:
            prefix, suffix = steps[:game.replay], steps[game.replay:]
            steps = prefix + [step('capture')] + suffix + [
                step('record_state'), step('restore'), *copy.deepcopy(suffix), step('assert_recorded_state')]
        out.append(dict(name=f'HeroicCompositionRiderRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                        setup=game.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                        params=declaration['params'], oracle_text=declaration['oracle_text'],
                        untested_components=['Multiplayer, targeting the same creature multiple times in a single spell, and stacked Heroic sources.'])))
    return out
