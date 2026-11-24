"""Fight triggers from real entries, turn-up actions and delayed end steps."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from morph_creature_recipes import FACE_DOWN
from recipes import cost_from_oracle

FAMILY = 'fight_triggered_sources'
PROFILE = Path(__file__).with_name('data') / 'fight_triggered_sources_wave8_profiles.json'
CARDS = {
    12966: ('Whenever this creature or another creature you control enters, that creature fights target creature '
            'an opponent controls.', {'Fight'}, ('self_entry', 'other_entry', 'no_target', 'mutual_death')),
    14555: ('As this enchantment enters, choose Khans or Dragons.\n'
            '• Khans — At the beginning of each of your main phases, add {G}{G}.\n'
            '• Dragons — Whenever a creature you control with flying enters, you may have it fight target creature '
            "you don't control.", {'Fight'}, ('dragon_entry', 'dragon_decline', 'khans_entry', 'enemy_flier')),
    14644: ('When this creature enters, manifest the top card of your library. (Put that card onto the battlefield '
            'face down as a 2/2 creature. Turn it face up any time for its mana cost if it\'s a creature card.)\n'
            'Whenever a permanent you control is turned face up, if it\'s a creature, you may have it fight target '
            "creature you don't control.", {'Manifest', 'Fight'}, ('manifest_fight', 'foe_faceup', 'empty_library',
                                                                  'faceup_uses_revealed_pt', 'manifest_pass')),
    14916: ("Flying\nWhen this creature enters, you may have it fight target creature you don't control.\n"
            'Whenever a creature you control fights, put two +1/+1 counters on it at the beginning of the next end '
            'step.', {'Flying', 'Fight'}, ('self_fight', 'decline', 'other_fight', 'persists_after_source')),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0,) * 6
BEAR = 'Grizzly Bears'
GIANT = 'Hill Giant'
FLIER = 'Air Elemental'
HIDDEN = 'Lightning Bolt'
MORPH = 'Glacial Stalker'
FIGHT = 'Prey Upon'
MORPH_FEE = '{4}{U}'
WURM = 'Craw Wurm'
WITNESSES = {BEAR, GIANT, FLIER, HIDDEN, MORPH, FIGHT, WURM, 'Forest', 'Murder'}
GAPS = {
    14555: ['The driver does not dispatch actual main-phase entry. Khans is observed as the paid switch '
            'away from the Dragons trigger, and its main-phase mana is untested.'],
    14644: ['The printed manifest event and a separate real paid Morph turn-up are observed. Activation '
            'of the non-Morph manifest legacy has no visible exact activation reference in this fixture.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CARDS or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face')):
        raise ValueError('whole normal Fight source required')
    text, keywords, _ = CARDS[csvid]
    if record.get('oracle_text') != text or set(record.get('keywords', ())) != keywords:
        raise ValueError('complete printed fight source, costs and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), rules=text)


def variants(shape):
    return [f'{mode}_{actor}' for mode in CARDS[shape['csvid']][2] for actor in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported Fight trigger scenarios required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned triggered Fight primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole triggered Fight declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses', 'native_face_down_observer'}
            or set(pins['witnesses']) != WITNESSES
            or not exact(rows.get('Face-Down Creature'), pins['native_face_down_observer'])
            or pins['native_face_down_observer']['csvid'] != FACE_DOWN):
        raise ValueError('exact independent Fight witnesses and face-down observer required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CARDS):
        raise ValueError('complete triggered Fight cohort required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('changed triggered Fight source or witness: ' + name)
            if group == 'sources':
                if not str(row.get('handler', '')).startswith('manalinkeh_card_'):
                    raise ValueError('implemented triggered Fight primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[FLIER]['oracle_text'] != 'Flying' or not oracle[MORPH]['oracle_text'].startswith('Morph ' + MORPH_FEE)
            or not oracle[FIGHT]['oracle_text'].startswith("Target creature you control fights target creature you don't control.")
            or oracle['Murder']['oracle_text'] != 'Destroy target creature.'):
        raise ValueError('exact activated flip, Flying, removal and source fight witnesses required')


class Duel:
    def __init__(self, row, player, mode, rows, oracle):
        self.id, self.name = row['csvid'], row['name']
        self.player, self.foe, self.mode, self.active = player, 1 - player, mode, player
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.add('source', self.name, player)

    def add(self, ref, name, owner, zone='hand'):
        from recipes import card

        self.cards[ref] = dict(name=name, zone=zone, owner=owner, hidden=False, damage=0, counters=0)
        self.setup.append(card(name, ref, owner, zone))

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'assert_forced_target_choices':
            self.steps.append(step(op, value=value))
            return
        if op in ('choose_trigger_source', 'choose_optional_target'):
            self.steps.append(step(op, ref=ref, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, values):
        for color, value in enumerate(values):
            self.emit(op, player=player, value=value, color=color)

    def empty_mana(self):
        for player in (0, 1):
            self.mana('assert_mana', player, ZERO)

    def fund(self, player, cost):
        self.mana('set_mana', player, cost)
        self.emit('refresh')

    def turn(self):
        self.emit('turn', player=self.active, value=0)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone
        if zone != 'battlefield':
            self.cards[ref].update(hidden=False, damage=0, counters=0)

    def csvid(self, ref):
        entry = self.cards[ref]
        return FACE_DOWN if entry['hidden'] else self.rows[entry['name']]['csvid']

    def stats(self, ref):
        entry = self.cards[ref]
        record = self.oracle[entry['name']]
        p, t = (2, 2) if entry['hidden'] else (int(record['power']), int(record['toughness']))
        return p + entry['counters'], t + entry['counters']

    def receipt(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.empty_mana()
        for ref, entry in self.cards.items():
            if entry['zone'] != 'battlefield':
                continue
            self.prop(ref, 'in_play', 1)
            if entry['hidden'] or 'Creature' in self.oracle[entry['name']]['type_line'].split(' — ', 1)[0]:
                self.prop(ref, 'csvid', self.csvid(ref))
                self.prop(ref, 'damage', entry['damage'])
                for prop, value in zip(('power', 'toughness'), self.stats(ref)):
                    self.prop(ref, prop, value)
                self.emit('assert_counter', ref, prop='plus1', value=entry['counters'])
        known = {self.rows[entry['name']]['csvid'] for entry in self.cards.values()} | {FACE_DOWN}
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for index, zone in enumerate(ZONES):
                seen = Counter(self.csvid(ref) for ref, entry in self.cards.items()
                               if (entry['zone'], entry['owner']) == (zone, player))
                self.emit('assert_zone', player=player, zone=index, value=sum(seen.values()))
                for csvid in sorted(known):
                    self.emit('assert_zone_multiplicity', player=player, zone=index, index=csvid, value=seen[csvid])
                if zone == 'library':
                    contents = [self.csvid(ref) for ref, entry in self.cards.items()
                                if (entry['zone'], entry['owner']) == (zone, player)]
                    for offset, csvid in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=offset, value=csvid)

    def start(self):
        self.turn()
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana('set_mana', player, ZERO)
        self.emit('refresh')

    def advance(self):
        # advance_turn invokes the production end-turn trigger, then start-turn gates.
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.turn()
        for entry in self.cards.values():
            entry['damage'] = 0

    def do_fight(self, first, second):
        p1, t1 = self.stats(first)
        p2, t2 = self.stats(second)
        self.cards[first]['damage'] += p2
        self.cards[second]['damage'] += p1
        if self.cards[first]['damage'] >= t1:
            self.move(first, 'graveyard')
        if self.cards[second]['damage'] >= t2:
            self.move(second, 'graveyard')

    def cast(self, ref, *, targets=(), facedown=False):
        entry = self.cards[ref]
        player = entry['owner']
        assert entry['zone'] == 'hand'
        assert player == self.active or self.oracle[entry['name']]['type_line'].startswith('Instant')
        fee = [3, 0, 0, 0, 0, 0] if facedown else cost_from_oracle(self.oracle[entry['name']])
        self.fund(player, fee)
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.empty_mana()
        if facedown:
            self.prop(ref, 'csvid', FACE_DOWN)

    def resolve(self, *, strict=True):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed' if strict else 'assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def enter(self, ref, *, facedown=False, trigger=None):
        self.cast(ref, facedown=facedown)
        if trigger is not None:
            trigger()
        self.resolve()
        self.move(ref, 'battlefield')
        self.cards[ref]['hidden'] = facedown

    def mandatory(self, ref):
        self.emit('choose_target', ref, player=self.player)

    def optional(self, ref, *, accept, entering_source=False):
        if self.player == 0:
            if accept:
                op = 'choose_trigger' if entering_source else 'choose_trigger_source'
                self.emit(op, 'source', player=self.player)
                self.mandatory(ref)
        else:
            self.emit('choose_target' if accept else 'choose_optional_target', ref, player=self.player)

    def ragebeast(self):
        if self.mode != 'no_target':
            self.add('victim', WURM if self.mode == 'mutual_death' else BEAR, self.foe, 'battlefield')
        else:
            self.add('friendly', BEAR, self.player, 'battlefield')
        if self.mode == 'other_entry':
            self.add('enemy_enters', BEAR, self.foe)
            self.add('ally_enters', GIANT, self.player)
        self.start()
        self.enter('source', trigger=(lambda: self.mandatory('victim')) if self.mode != 'no_target' else None)
        if self.mode != 'no_target':
            self.do_fight('source', 'victim')
        self.receipt()
        if self.mode == 'other_entry':
            self.advance()
            self.enter('enemy_enters')
            self.receipt()
            self.advance()
            self.enter('ally_enters', trigger=lambda: self.mandatory('enemy_enters'))
            self.do_fight('ally_enters', 'enemy_enters')
            self.receipt()

    def siege(self):
        self.add('target', BEAR, self.foe, 'battlefield')
        self.add('flier', FLIER, self.foe if self.mode == 'enemy_flier' else self.player)
        if self.mode == 'dragon_entry':
            self.add('ground', GIANT, self.player)
        dragons = self.mode != 'khans_entry'
        self.start()
        self.enter('source', trigger=lambda: self.emit('choose_dialog', player=self.player, value=int(dragons)))
        self.receipt()
        if self.mode == 'enemy_flier':
            self.advance()
            self.enter('flier')
            self.receipt()
            return
        if self.mode == 'dragon_entry':
            self.enter('ground')
            self.receipt()
        accept = self.mode == 'dragon_entry'
        self.enter('flier', trigger=(lambda: self.optional('target', accept=accept)) if dragons else None)
        if accept:
            self.do_fight('flier', 'target')
        elif dragons:
            self.emit('assert_forced_target_choices', value=0)
        self.receipt()

    def manifest(self):
        if self.mode != 'empty_library':
            self.add('manifested', HIDDEN, self.player, 'library')
            self.add('next_card', 'Forest', self.player, 'library')
            actor = self.foe if self.mode == 'foe_faceup' else self.player
            self.add('turner', MORPH, actor)
            target = GIANT if self.mode == 'faceup_uses_revealed_pt' else BEAR
            self.add('target', target, self.foe, 'battlefield')
        self.start()
        self.enter('source')
        if self.mode == 'empty_library':
            self.receipt()
            return
        # The library reference is a zone position. Bind the resulting exact
        # native face-down permanent before touching its live characteristics.
        self.move('manifested', 'battlefield')
        self.cards['manifested']['hidden'] = True
        self.emit('bind_permanent', 'manifested', player=self.player, value=FACE_DOWN, index=-1)
        self.receipt()
        if actor == self.foe:
            self.advance()
        self.enter('turner', facedown=True)
        self.receipt()
        fee = cost_from_oracle({'mana_cost': MORPH_FEE})
        self.fund(actor, fee)
        self.emit('can_activate', 'turner', value=1)
        accept = actor == self.player and self.mode != 'manifest_pass'
        if actor == self.player:
            # The actual face-up helper presents its own explicit Fight/Pass
            # menu. The legal target is selected only after the Fight choice.
            self.emit('choose_dialog', player=actor, value=int(not accept))
            if accept:
                self.mandatory('target')
        self.emit('activate', 'turner', value=1)
        self.resolve(strict=actor != self.player)
        self.cards['turner']['hidden'] = False
        if accept:
            self.do_fight('turner', 'target')
        self.receipt()

    def airborne(self):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground', 'source', value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.life[self.foe] -= self.stats('source')[0]

    def regent(self):
        other = self.mode in ('other_fight', 'persists_after_source')
        if other:
            self.add('ally', GIANT, self.player, 'battlefield')
            self.add('target', BEAR, self.foe)
            self.add('fight', FIGHT, self.player)
            if self.mode == 'persists_after_source':
                self.add('remove', 'Murder', self.foe)
        else:
            self.add('target', BEAR, self.foe, 'battlefield')
            if self.mode == 'self_fight':
                self.add('ground', GIANT, self.foe, 'battlefield')
        accept = self.mode == 'self_fight'
        self.start()
        self.enter('source', trigger=(lambda: self.optional('target', accept=accept, entering_source=True))
                   if not other else None)
        if accept:
            self.do_fight('source', 'target')
        elif not other:
            self.emit('assert_forced_target_choices', value=0)
        self.receipt()
        if other:
            self.advance()
            self.enter('target')
            self.receipt()
            self.advance()
            self.cast('fight', targets=('ally', 'target'))
            self.resolve()
            self.move('fight', 'graveyard')
            self.do_fight('ally', 'target')
            self.receipt()
            if self.mode == 'persists_after_source':
                self.cast('remove', targets=('source',))
                self.resolve()
                self.move('remove', 'graveyard')
                self.move('source', 'graveyard')
                self.receipt()
        self.advance()
        if other or accept:
            self.cards['ally' if other else 'source']['counters'] += 2
        self.receipt()
        if accept:
            self.advance()
            self.airborne()
            self.receipt()

    def build(self):
        {12966: self.ragebeast, 14555: self.siege, 14644: self.manifest, 14916: self.regent}[self.id]()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact triggered Fight primary CSVID required')
    output = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(rows[name], int(actor), mode, rows, oracle)
        game.build()
        output.append(dict(
            name=f'FightTriggeredSourcesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
            seed=2633, setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return output



