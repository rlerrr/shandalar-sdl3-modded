"""Paid regeneration costs, outcome scope and printed independent riders."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'regeneration_follow_on_sacrifice_and_team'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'

RISE, HIERARCH, SKIN, JAR = 11952, 1368, 7697, 6836
MODELS = {
    RISE: ('{1}{G}', 'Enchantment',
           'Werewolf creatures you control get +1/+0 and have trample.\n'
           'Sacrifice this enchantment: Regenerate all Werewolf creatures you control.',
           ('trample', 'legacy_team', 'proactive_shield', 'without_cost')),
    HIERARCH: ('{2}{G}{W}', 'Creature — Elephant Cleric',
               'When this creature enters, you gain 4 life.\n'
               '{G}{W}, Sacrifice this creature: Regenerate each creature you control.',
               ('team_shields', 'short', 'expires', 'entry')),
    SKIN: ('{2}{G}', 'Enchantment', "Return this enchantment to its owner's hand: Regenerate target creature.",
           ('proactive', 'friendly', 'foreign')),
    JAR: ('{0}', 'Artifact', 'Sacrifice this artifact: Regenerate target artifact.',
          ('proactive', 'friendly', 'foreign', 'wounded', 'forbidden')),
}
WITNESSES = {
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4', '4'),
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1', '2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1', '1'),
    "Traveler's Amulet": ('{1}', '{1}, Sacrifice this artifact: Search your library for a basic land card, reveal it, put it into your hand, then shuffle.'),
    'Yotian Soldier': ('{3}', 'Vigilance', '1', '4'),
    'Shock': ('{R}', 'Shock deals 2 damage to any target.'),
    'Day of Judgment': ('{2}{W}{W}', 'Destroy all creatures.'),
    'Terminate': ('{B}{R}', "Destroy target creature. It can't be regenerated."),
}
FACES = {
    'Tormented Pariah': ('Creature — Human Warrior Werewolf', '3', '2'),
    'Grizzled Outcasts': ('Creature — Human Werewolf', '4', '4'),
}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6
GREEN = (0, 0, 0, 1, 0, 0)
GREEN_WHITE = (0, 0, 0, 1, 0, 1)
SEED = 2964


def label(csvid, mode, actor):
    return f'RegenerationFollowOnSacrificeAndTeamRecipe_{csvid}_{mode}_{actor}'


class Board:
    def __init__(self, record, csvid, mode, actor, rows, oracle, *, host_name=None, contrast_name="Prodigal Sorcerer"):
        self.record, self.csvid, self.mode, self.actor, self.foe = record, csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.add('source', record['name'], actor, 'hand')
        host_owner = self.foe if mode == 'foreign' else actor
        host = 'Serra Angel'
        if csvid == RISE:
            host = 'Tormented Pariah'
            self.add('opposing_werewolf', 'Grizzled Outcasts', self.foe, 'battlefield')
            self.add('ordinary', 'Goblin Tinkerer', actor, 'battlefield')
            if mode == 'legacy_team':
                self.add('second', 'Grizzled Outcasts', actor, 'battlefield')
        elif csvid == HIERARCH:
            self.add('second', 'Goblin Tinkerer', actor, 'battlefield')
        elif csvid == JAR:
            host = 'Yotian Soldier' if mode in ('wounded', 'forbidden') else "Traveler's Amulet"
        self.add('host', host_name or host, host_owner, 'battlefield')
        self.add('contrast', contrast_name, 1-host_owner, 'battlefield')
        if mode in ('legacy_team', 'team_shields'):
            self.add('sweeper', 'Day of Judgment', actor, 'hand')
        if mode == 'forbidden':
            self.add('fatal', 'Terminate', self.foe, 'hand')
        if mode == 'wounded':
            self.add('shock', 'Shock', self.foe, 'hand')

    def add(self, ref, name, player, zone):
        assert ref not in self.cards and player in (0, 1)
        self.cards[ref] = dict(name=name, owner=player, zone=zone)
        state = 8 if zone == 'battlefield' else 0
        entry = card(name, ref, player, zone)
        if state:
            entry['state'] = ['untapped']
        self.setup.append(entry)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'activate_response':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'untap_step':
            self.steps.append(step(op, player=player))
            return
        fields = dict(ref=ref, other=other, property=prop, player=player, value=value,
                      zone=next(name for name, code in ZONES.items() if code == zone), index=index, color=color)
        self.steps.append(step(op, **{key: item for key, item in fields.items() if item != ''}))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, pool):
        self.wallet[player] = tuple(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def money(self):
        for who, pool in enumerate(self.wallet):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=who, color=color, value=amount)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.money()
        for who in (0, 1):
            for zone, code in ZONES.items():
                names = [card['name'] for card in self.cards.values() if card['zone'] == zone and card['owner'] == who]
                self.emit('assert_zone', player=who, zone=code, value=len(names))
                for name in sorted(set(names)):
                    self.emit('assert_zone_multiplicity', player=who, zone=code, index=self.rows[name]['csvid'],
                              value=names.count(name))

    def cast_pending(self, ref, target=None):
        data = self.cards[ref]
        who, name = data['owner'], data['name']
        record = self.oracle[name]
        self.fund(who, ZERO if record['mana_cost'] == '{0}' else cost_from_oracle(record))
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.money()

    def resolve(self, ref, zone='graveyard'):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def cast(self, ref='source', target=None, zone='graveyard'):
        self.cast_pending(ref, target)
        self.resolve(ref, zone)

    def intact(self, ref='host', *, bonus=0, damage=0, tapped=0):
        card = self.oracle[self.cards[ref]['name']]
        stats = card['card_faces'][0] if card['layout'] == 'transform' else card
        self.prop(ref, 'in_play', 1)
        if 'power' in stats:
            self.prop(ref, 'power', int(stats['power']) + bonus)
            self.prop(ref, 'toughness', int(stats['toughness']))
            self.prop(ref, 'damage', damage)
        self.prop(ref, 'tapped', tapped)

    def destroy(self, ref='host', destination=None):
        self.emit('destroy', ref)
        if destination is not None:
            self.move(ref, destination)

    def queue_response(self, target=None):
        self.emit('choose_regeneration', 'source')
        if target is not None:
            self.emit('choose_target', target, player=self.actor)

    def paid_team_activation(self, cost):
        self.fund(self.actor, cost)
        self.emit('activate', 'source', value=1)
        self.wallet[self.actor] = ZERO
        self.emit('assert_stack', value=1)
        self.prop('source', 'in_play', 0)
        self.money()
        self.resolve('source')

    def rise(self):
        self.intact(bonus=1)
        self.intact('opposing_werewolf')
        self.intact('ordinary')
        if self.mode == 'trample':
            self.emit('begin_attackers')
            self.emit('attack', 'host', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'contrast', 'host', value=1)
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.foe] -= 3
            self.move('contrast', 'graveyard')
            self.intact(tapped=1, damage=1, bonus=1)
        elif self.mode == 'legacy_team':
            self.intact('second', bonus=1)
            self.cast_pending('sweeper')
            self.queue_response()
            self.resolve('sweeper')
            for ref in ('source', 'ordinary', 'opposing_werewolf', 'contrast'):
                self.move(ref, 'graveyard')
            self.intact(tapped=1)
            self.intact('second', tapped=1)
        elif self.mode == 'proactive_shield':
            self.paid_team_activation(ZERO)
            self.intact()
            self.receipt()
            self.destroy()
            self.intact(tapped=1)
            self.destroy('ordinary', 'graveyard')
            self.destroy('opposing_werewolf', 'graveyard')
        else:
            self.destroy(destination='graveyard')
            self.prop('source', 'in_play', 1)

    def hierarch(self):
        self.life[self.actor] += 4
        self.intact('source')
        self.receipt()
        if self.mode == 'entry':
            return
        if self.mode == 'short':
            self.fund(self.actor, GREEN)
            self.emit('can_activate', 'source', value=0)
            self.destroy(destination='graveyard')
            self.intact('source')
            self.intact('second')
            return
        self.paid_team_activation(GREEN_WHITE)
        self.receipt()
        if self.mode == 'expires':
            self.emit('cleanup')
            self.destroy(destination='graveyard')
            self.intact('second')
            return
        self.cast('sweeper')
        self.move('contrast', 'graveyard')
        self.intact(tapped=1)
        self.intact('second', tapped=1)
        self.receipt()
        self.destroy('second', 'graveyard')

    def skin(self):
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.intact()
            return
        # EVENT_ACTIVATE lacks a target-selector branch in Molting Skin.
        # Check whether the actual response saves the creature and pays the return;
        # preserve that Oracle expectation without injecting an unoffered target.
        self.queue_response()
        self.destroy()
        self.emit('assert_unused_choices', value=0)
        self.move('source', 'hand')
        self.intact(tapped=1)
        self.intact('contrast')

    def jar(self):
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.intact()
            return
        if self.mode == 'forbidden':
            self.cast('fatal', 'host')
            self.move('host', 'graveyard')
            self.prop('source', 'in_play', 1)
            return
        if self.mode == 'wounded':
            self.cast('shock', 'host')
            self.intact(damage=2)
        self.queue_response('host')
        self.destroy()
        self.move('source', 'graveyard')
        self.intact(tapped=1)
        self.intact('contrast')
        self.receipt()
        self.destroy(destination='graveyard')

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        self.cast(zone='battlefield')
        {RISE: self.rise, HIERARCH: self.hierarch, SKIN: self.skin, JAR: self.jar}[self.csvid]()
        self.receipt()



DEVOTION, VASSAL, HORROR, PTERON, MARTYR = 4689, 11453, 675, 6933, 2651
FOLLOWERS = {
    DEVOTION: ('{2}{W}', 'Enchantment', 'Sacrifice a creature: Regenerate target creature.',
               ('friendly', 'foreign', 'no_fodder', 'proactive')),
    VASSAL: ('{2}{W}', 'Creature — Phyrexian Dog',
             "Sacrifice this creature: Put a -1/-1 counter on target creature. Then if that creature's toughness is 1 or greater, regenerate it.",
             ('survives', 'foreign', 'lethal')),
    HORROR: ('{3}{B}{B}', 'Enchantment',
             'Sacrifice a Swamp: Regenerate target black creature. (The next time that creature would be destroyed this turn, '
             'instead tap it, remove it from combat, and heal all damage on it.)',
             ('friendly', 'foreign', 'wrong_color', 'no_swamp', 'proactive')),
    PTERON: ('{1}{W}', 'Creature — Dinosaur Spirit', 'Flying\nSacrifice this creature: Regenerate target artifact.',
             ('friendly', 'foreign', 'wounded', 'flight', 'proactive')),
    MARTYR: ('{W}', 'Creature — Human Cleric',
             'Sacrifice this creature: Regenerate target creature.\n'
             '{W}{W}, {T}, Sacrifice this creature: Counter target spell that targets an enchantment.',
             ('friendly', 'proactive', 'counter', 'counter_short', 'counter_sick', 'wrong_target')),
}
NEW_WITNESSES = {
    'Swamp': ('', '({T}: Add {B}.)'),
    'Forest': ('', '({T}: Add {G}.)'),
    'Royal Assassin': ('{1}{B}{B}', '{T}: Destroy target tapped creature.', '1', '1'),
    'Disenchant': ('{1}{W}', 'Destroy target artifact or enchantment.'),
    'Rites of Flourishing': ('{2}{G}', "At the beginning of each player's draw step, that player draws an additional card.\nEach player may play an additional land on each of their turns."),
}
KEYWORDS = {HORROR: {'Heal', 'Regenerate'}, PTERON: {'Flying', 'Regenerate'}}
PRIMARY_PT = {HIERARCH: ('4','4'), VASSAL: ('2','1'), PTERON: ('1','1'), MARTYR: ('1','1')}
WHITE = (0, 0, 0, 0, 0, 1)
WHITE_WHITE = (0, 0, 0, 0, 0, 2)


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        host = None
        contrast = 'Prodigal Sorcerer'
        if csvid == DEVOTION and mode == 'foreign':
            # Any actual AI auto-choice from two identical legal own creatures
            # gives the same ledger. The friendly fixture has just one non-dying choice.
            contrast = 'Goblin Tinkerer'
        if csvid == HORROR and mode != 'wrong_color':
            host = 'Royal Assassin'
        if csvid == VASSAL:
            host = 'Prodigal Sorcerer' if mode == 'lethal' else 'Goblin Tinkerer'
        if csvid == PTERON:
            host = 'Yotian Soldier' if mode == 'wounded' else "Traveler's Amulet"
        super().__init__(record, csvid, mode, actor, rows, oracle, host_name=host, contrast_name=contrast)
        if csvid == DEVOTION and mode != 'no_fodder':
            self.add('cost_card', 'Goblin Tinkerer', actor, 'battlefield')
        if csvid == HORROR:
            self.add('cost_card', 'Forest' if mode == 'no_swamp' else 'Swamp', actor, 'battlefield')
            self.add('other_swamp', 'Swamp', self.foe, 'battlefield')
        if csvid == MARTYR and (mode.startswith('counter') or mode == 'wrong_target'):
            eligible = 'Rites of Flourishing' if mode != 'wrong_target' else "Traveler's Amulet"
            self.add('protected', eligible, actor, 'battlefield')
            self.add('disenchant', 'Disenchant', self.foe, 'hand')

    def paid_external_cost(self):
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.intact()
            return
        if self.mode in ('no_fodder', 'wrong_color', 'no_swamp'):
            self.queue_response()
            self.destroy(destination='graveyard')
            self.emit('assert_unused_choices', value=1)
            self.prop('source', 'in_play', 1)
            return
        self.queue_response()
        if self.actor == 0:
            self.emit('choose_target', 'cost_card', player=self.actor)
        self.emit('choose_target', 'host', player=self.actor)
        self.destroy()
        self.move('cost_card', 'graveyard')
        self.intact(tapped=1)
        self.prop('source', 'in_play', 1)

    def vassal(self):
        self.emit('choose_target', 'host', player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.prop('source', 'in_play', 0)
        self.money()
        self.resolve('source')
        if self.mode == 'lethal':
            self.move('host', 'graveyard')
            return
        self.prop('host', 'in_play', 1)
        self.prop('host', 'power', 0)
        self.prop('host', 'toughness', 1)
        self.prop('host', 'tapped', 0)
        self.emit('assert_counter', 'host', prop='minus1', value=1)
        self.destroy()
        self.prop('host', 'in_play', 1)
        self.prop('host', 'power', 0)
        self.prop('host', 'toughness', 1)
        self.prop('host', 'tapped', 1)
        self.emit('assert_counter', 'host', prop='minus1', value=1)
        self.receipt()
        self.destroy(destination='graveyard')

    def age(self, *, untap=True):
        self.emit('advance_turn')
        self.emit('advance_turn')
        if untap:
            self.emit('untap_step', player=self.actor)

    def pteron(self):
        self.intact('source')
        if self.mode != 'flight':
            self.jar()
            return
        self.age(untap=False)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'contrast', 'source', value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= 1
        self.intact('source', tapped=1)
        self.intact('contrast')

    def martyr(self):
        self.intact('source')
        if self.mode == 'proactive':
            self.emit('can_activate', 'source', value=1)
            self.intact()
            return
        if self.mode == 'friendly':
            self.queue_response('host')
            self.destroy()
            self.move('source', 'graveyard')
            self.intact(tapped=1)
            return
        if self.mode != 'counter_sick':
            self.age()
        self.prop('source', 'sick', int(self.mode == 'counter_sick'))
        self.cast_pending('disenchant', 'protected')
        self.fund(self.actor, WHITE if self.mode == 'counter_short' else WHITE_WHITE)
        if self.mode == 'counter':
            self.emit('activate_response', 'source', value=1)
            self.emit('assert_stack', value=2)
            self.prop('source', 'in_play', 0)
            self.wallet[self.actor] = ZERO
            self.move('source', 'graveyard')
        else:
            self.emit('can_activate', 'source', value=0)
            self.move('protected', 'graveyard')
        self.resolve('disenchant')
        if self.mode == 'counter':
            self.prop('protected', 'in_play', 1)
        else:
            self.prop('source', 'in_play', 1)

    def play(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for who in (0, 1):
            self.fund(who, ZERO)
        self.cast(zone='battlefield')
        effect = {DEVOTION: self.paid_external_cost, HORROR: self.paid_external_cost,
                  VASSAL: self.vassal, PTERON: self.pteron, MARTYR: self.martyr}
        effect[self.csvid]()
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    mana, kind, text, modes = (MODELS | FOLLOWERS)[csvid]
    if (record.get('mana_cost'), record.get('type_line'), record.get('oracle_text')) != (mana, kind, text):
        raise ValueError('Exact regeneration cost, type and printed clauses required')
    defaults = {'Regenerate'}
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or set(record.get('keywords', ())) != KEYWORDS.get(csvid, defaults)):
        raise ValueError('Exact single-face regeneration record required')
    if csvid in PRIMARY_PT and (record['power'], record['toughness']) != PRIMARY_PT[csvid]:
        raise ValueError('Printed source characteristics changed')
    return dict(csvid=csvid, printed_cost=mana, choices=[f'{mode}_{seat}' for mode in modes for seat in (0, 1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, rows):
    pins = profiles()
    expects = WITNESSES | NEW_WITNESSES
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != expects.keys() | FACES.keys()
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != MODELS.keys() | FOLLOWERS.keys()):
        raise ValueError('Exact regeneration source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            layout = 'transform' if name in FACES else 'normal'
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != layout
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed regeneration source or witness: {name}')
            if name in expects:
                fields = ('mana_cost', 'oracle_text', 'power', 'toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields) != expects[name]:
                    raise ValueError(f'Unexpected regeneration witness clause: {name}')
            if name in FACES:
                front = record['card_faces'][0]
                if tuple(front[k] for k in ('type_line', 'power', 'toughness')) != FACES[name]:
                    raise ValueError(f'Unexpected Werewolf front: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned regeneration primary required')
    csvid = pin['registry']['csvid']
    model = classify(record, csvid)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact regeneration recipe and distinct scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact regeneration primary and variants required')
    gaps = {SKIN: ['The legacy handler does not offer activation target selection. The source return and survival remain asserted through the actual destruction response.']}
    cases = []
    factory = FollowOn if csvid in FOLLOWERS else Board
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = factory(oracle[name], csvid, mode, int(seat), rows, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps.get(csvid, []))
        cases.append(dict(name=label(csvid, mode, int(seat)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases



