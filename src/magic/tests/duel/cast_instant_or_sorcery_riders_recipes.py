"""Observe effects of actual instant and sorcery casts and their printed riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from cast_trigger_case_support import CastTriggerBoard, types
from library_search_recipes import wave6_exact


FAMILY = 'cast_instant_or_sorcery_riders'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
WHEN = 'Whenever you cast an instant or sorcery spell, '
SCRY_TWO = 'scry 2. (Look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.)'
INVESTIGATE = 'investigate. (Create a Clue token. It\'s an artifact with "{2}, Sacrifice this token: Draw a card.")'
MODELS = {
    (WHEN + 'this creature gets +1/+1 until end of turn. Untap it.',):
        ('untap', 'blistercoil_weird', ()),
    (WHEN + 'you may pay {1}. If you do, ' + SCRY_TWO,):
        ('paid_scry', 'eyes_of_the_watcher', ('Scry',)),
    ('Flying', WHEN + "you may switch this creature's power and toughness until end of turn."):
        ('switch', 'fluxcharger', ('Flying',)),
    (WHEN + 'you may put a charge counter on this artifact. If you do, this artifact deals damage equal to the number of charge counters on it to any target.',):
        ('charge_damage', 'sphinx_bone_wand', ()),
    (WHEN + INVESTIGATE,): ('clue', 'trail_of_evidence', ('Investigate',)),
}

COPY = 'copy that spell. You may choose new targets for the copy.'
RIKU_ENTRY = ("Whenever another nontoken creature you control enters, you may pay {G}{U}. "
              "If you do, create a token that's a copy of that creature.")
FOLLOW_MODELS = {
    ('At the beginning of your upkeep, return an instant or sorcery card at random from your graveyard to your hand.',
     WHEN + 'this creature gets +4/+0 until end of turn.'): ('devil', 'charmbreaker_devils', ()),
    (WHEN + 'you may pay {3}. If you do, ' + COPY,): ('mirari', 'mirari2', ()),
    ('Defender', WHEN + "this creature gets +3/+0 until end of turn and can attack this turn as though it didn't have defender."):
        ('cyclops', 'nivix_cyclops', ('Defender',)),
    ('Flying', WHEN + 'scry 1. (Look at the top card of your library. You may put that card on the bottom.)'):
        ('scry_one', 'prescient_chimera', ('Flying', 'Scry')),
    (WHEN + 'you may pay {U}{R}. If you do, ' + COPY, RIKU_ENTRY): ('riku', 'riku_of_two_reflections', ()),
    (WHEN + 'you may put a charge counter on this creature.',
     'Whenever this creature deals combat damage to a player, you may draw X cards, where X is the number of charge counters on it.'):
        ('spellblade', 'surrakar_spellblade', ()),
    (WHEN + "you may put three +1/+1 counters on target land you control. If you do, that land becomes a 0/0 Elemental creature with haste that's still a land.",):
        ('animate', 'noyan_dar_roil_shaper', ()),
    ('Flying', 'Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)',
     WHEN + "tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step."):
        ('freeze', 'niblis_of_frost', ('Flying', 'Prowess')),
    (WHEN + 'you get {E}{E} (two energy counters).', '{T}, Pay {E}{E}{E}{E}{E}: This artifact deals 3 damage to any target.'):
        ('tower', 'dynavolt_tower', ()),
    (WHEN + "create an X/X colorless Construct artifact creature token, where X is that spell's mana value.",
     '{3}{U}{U}, Exile this enchantment: Return all instant and sorcery cards from your graveyard to your hand. Activate only if you control six or more artifacts.'):
        ('construct', 'metallurgic_summonings', ()),
    (WHEN + 'you may ' + COPY,): ('swarm_copy', 'swarm_intelligence', ()),
    ('Flying, haste', WHEN + 'Wizards you control get +1/+1 until end of turn.'):
        ('wizard', 'adeliz_the_cinder_wind', ('Flying', 'Haste')),
    (WHEN + "copy it for each other instant and sorcery spell you've cast before it this turn. You may choose new targets for the copies.",):
        ('storm', 'thousand_year_storm', ()),
}
COPY_MODELS = {'mirari', 'riku', 'swarm_copy', 'storm'}
FOLLOW_OPTIONAL = {'mirari', 'riku', 'spellblade', 'animate', 'swarm_copy'}
FOLLOW_SCENES = {
    'devil': ('upkeep', 'upkeep_wrong_type', 'expiry'),
    'mirari': ('fee_short', 'retarget'),
    'cyclops': ('attack_enabled', 'defender_only', 'attack_expiry'),
    'scry_one': ('flying',),
    'riku': ('fee_short', 'retarget', 'copy_entry', 'entry_short'),
    'spellblade': ('scaled_combat', 'blocked_draw'),
    'animate': ('fresh_land_attack', 'later_land_mana', 'no_own_land'),
    'freeze': ('freeze_expires', 'artifact_prowess', 'flying'),
    'tower': ('spend_energy', 'short_energy'),
    'construct': ('reclaim', 'below_reclaim'),
    'swarm_copy': ('retarget',),
    'wizard': ('expiry', 'flying_haste'),
    'storm': ('storm_three', 'storm_reset', 'storm_retarget'),
}
FOLLOW_SUPPORT = {'Lightning Bolt', 'Icy Manipulator', 'Incandescent Soulstoke', 'Darksteel Relic',
                  'Construct', 'Prodigal Sorcerer'}

COMMON = ('instant', 'sorcery', 'own_creature', 'enemy_instant', 'countered', 'replay')
SCENES = {
    'untap': ('double', 'expiry', 'hybrid_red'),
    'paid_scry': ('unfunded',),
    'switch': ('double', 'expiry', 'flying'),
    'charge_damage': ('double', 'creature_target', 'walker_target'),
    'clue': ('double', 'clue_paid', 'clue_short'),
}
OPTIONAL = {'paid_scry', 'switch', 'charge_damage'}
LIBRARY = ('Giant Growth', 'Divination', 'Grizzly Bears', 'Counterspell', 'Wind Drake',
           'Island', 'Plains', 'Forest', 'Swamp', 'Mountain')
SUPPORT = set(LIBRARY) | {'Reach Through Mists', 'Twiddle', 'Clue', 'Jace Beleren', 'Hill Giant'} | FOLLOW_SUPPORT
VANILLA = {'Island', 'Plains', 'Forest', 'Swamp', 'Mountain', 'Grizzly Bears', 'Wind Drake', 'Hill Giant'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import COLORS, cost_from_oracle

    match = (MODELS | FOLLOW_MODELS).get(tuple(record['oracle_text'].splitlines()))
    if record['layout'] != 'normal' or record.get('card_faces') or record.get('selected_face') or not match:
        raise ValueError('unrecognized whole-card cast trigger')
    effect, _, keywords = match
    if set(record['keywords']) != set(keywords):
        raise ValueError('cast trigger keywords do not match the complete Oracle model')
    mana_cost = record['mana_cost']
    hybrid = ('U', 'R') if effect == 'untap' and mana_cost == '{U/R}' else ()
    mana = [int(color == COLORS['U']) for color in range(6)] if hybrid else cost_from_oracle(record)
    power = [int(record[key]) for key in ('power', 'toughness')] if 'Creature' in types(record) else []
    return dict(effect=effect, mana=mana, hybrid=list(hybrid), stats=power, keywords=list(keywords))


def handler(record):
    return 'manalinkeh_card_' + (MODELS | FOLLOW_MODELS)[tuple(record['oracle_text'].splitlines())][1]


def variants(shape):
    effect = shape['effect']
    both = [f'{scenario}_{owner}' for scenario in (*COMMON, *(SCENES | FOLLOW_SCENES)[effect]) for owner in (0, 1)]
    return both + (['decline_0'] if effect in OPTIONAL | FOLLOW_OPTIONAL else [])


def spec(declaration, record):
    shape = classify(record)
    pin = profiles()['sources'].get(record['name'])
    selected = declaration.get('variants')
    if (not pin or not wave6_exact(pin['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not wave6_exact(declaration.get('params'), shape)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('actual Cast Trigger family and complete Oracle pin required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pinned = profiles()
    if pinned['version'] != 1 or set(pinned['witnesses']) != SUPPORT:
        raise ValueError('complete pinned actual-cast witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in pinned[group].items():
            record, row = oracle[name], rows[name]
            if (not wave6_exact(pin, dict(oracle=record, registry=row)) or
                    record['layout'] != ('token' if name in ('Clue', 'Construct') else 'normal') or
                    row['kind'] not in ('card', 'token') or row['handler'] in (None, '0', 'NULL') or
                    (row['handler'] == 'card_dummy' and name not in VANILLA)):
                raise ValueError(f'changed reviewed cast witness or source {name}')
            if group == 'sources' and row['handler'] != handler(record):
                raise ValueError(f'changed implemented primary {name}')


class TriggerBoard(CastTriggerBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.life = [20, 20]
        self.active = owner
        self.stats = list(shape['stats'])
        self.charges = 0
        self.life_hits = 0
        self.clues = []
        self.libraries = [[], []]
        super().__init__(name, shape, mode, owner, rows, oracle)

    @property
    def effect(self):
        return self.shape['effect']

    def build(self):
        owner, foe, mode = self.owner, self.foe, self.mode
        for seat in (0, 1):
            for index, name in enumerate(LIBRARY):
                ref = f'library_{seat}_{index}'
                self.add(name, ref, seat, 'library')
                self.libraries[seat].append(ref)
        if mode == 'enemy_instant':
            self.add('Reach Through Mists', 'spell', foe, 'hand')
        elif mode == 'own_creature':
            self.add('Grizzly Bears', 'spell', owner, 'hand')
        else:
            self.add('Divination' if mode == 'sorcery' else 'Reach Through Mists', 'spell', owner, 'hand')
        if mode in ('double', 'replay'):
            self.add('Reach Through Mists', 'second_spell', owner, 'hand')
        if mode == 'countered':
            self.add('Counterspell', 'counter', foe, 'hand')
        if self.effect == 'untap':
            self.add('Twiddle', 'tap_spell', foe, 'hand')
        if self.effect == 'switch' and mode == 'flying':
            self.add('Hill Giant', 'ground', foe)
            self.add('Wind Drake', 'air', foe)
        if mode == 'creature_target':
            self.add('Grizzly Bears', 'victim', foe)
        if mode == 'walker_target':
            self.add('Jace Beleren', 'victim', foe, 'hand')

    def stat(self, ref, prop, amount):
        self.emit('assert_card', ref=ref, property=prop, value=int(amount))

    def count(self, seat, zone, name, value):
        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[name]['csvid'], value=value)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def draw_cards(self, seat, amount):
        if len(self.libraries[seat]) < amount:
            raise ValueError('bounded witness draw exceeds prepared library')
        refs = self.libraries[seat][:amount]
        del self.libraries[seat][:amount]
        for ref in refs:
            self.move(ref, 'hand')

    def age(self, *, upkeep=False):
        self.active = 1-self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=self.active)
        if upkeep:
            self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active)
        self.emit('refresh')

    def plain_resolve(self, ref, new_zone='graveyard', *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.move(ref, new_zone)

    def play_actual_source(self):
        if not self.shape['hybrid']:
            self.play_source()
            return
        from recipes import COLORS
        color = 'R' if self.mode == 'hybrid_red' else 'U'
        pool = [int(c == COLORS[color]) for c in range(6)]
        self.fund(self.owner, pool)
        self.emit('cast', ref='source', value=1)
        self.pools[self.owner] = [0]*6
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='color', value=(1 << COLORS['U']) | (1 << COLORS['R']))
        self.observe_mana()
        self.plain_resolve('source', 'battlefield')
        self.stat('source', 'in_play', 1)

    def decl_choices(self, ref):
        if self.effect in OPTIONAL and self.mode != 'decline' and self.owner == 0:
            self.emit('choose_trigger_source', player=self.owner, ref='source')
        if self.mode == 'decline':
            return
        if self.effect == 'paid_scry' and self.fee():
            self.prepare_scry(2)
        elif self.effect == 'charge_damage':
            if self.mode in ('creature_target', 'walker_target'):
                self.emit('choose_target', player=self.owner, ref='victim')
            else:
                self.emit('choose_player', player=self.owner, value=self.foe)

    def fee(self):
        return int(self.effect == 'paid_scry' and self.mode not in ('unfunded', 'decline', 'own_creature', 'enemy_instant'))

    def prepare_scry(self, amount):
        """Offer legal top nonlands to both legacy selectors and bottom all N."""
        self.emit('record_library', player=self.owner)
        for ref in self.libraries[self.owner][:amount]:
            self.emit('choose_zone_card', player=self.owner, value=self.rows[self.cards[ref]['name']]['csvid'])

    def value_scry(self, amount):
        self.emit('assert_scry_result', player=self.owner, value=amount)
        refs = self.libraries[self.owner]
        refs[:] = refs[amount:] + refs[:amount]

    def add_clue(self):
        ref = f'clue_{len(self.clues)}'
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=len(self.clues), value=self.rows['Clue']['csvid'])
        self.cards[ref] = dict(name='Clue', player=self.owner, zone='battlefield', tapped=False)
        self.clues.append(ref)
        for attribute, amount in (('token', 1), ('artifact', 1), ('subtype_Clue', 1), ('color', 0)):
            self.emit('assert_matching_permanents', player=self.owner, index=self.rows['Clue']['csvid'],
                      property=attribute, value=amount)

    def reward(self):
        if self.mode == 'decline':
            return
        effect = self.effect
        if effect == 'untap':
            self.stats = [p+1 for p in self.stats]
            self.cards['source']['tapped'] = False
        elif effect == 'switch':
            self.stats.reverse()
        elif effect == 'paid_scry' and self.fee():
            self.pools[self.owner][0] -= 1
            self.value_scry(2)
        elif effect == 'charge_damage':
            self.charges += 1
            if self.mode == 'creature_target':
                self.stat('victim', 'damage', 1)
            elif self.mode == 'walker_target':
                self.emit('assert_counter', ref='victim', counter_kind='loyalty', value=2)
            else:
                self.life[self.foe] -= self.charges
        elif effect == 'clue':
            self.add_clue()

    def reward_receipts(self):
        if self.cards['source']['zone'] == 'battlefield':
            for name, amount in zip(('power', 'toughness'), self.stats):
                self.stat('source', name, amount)
            if self.effect == 'untap':
                self.stat('source', 'tapped', self.cards['source']['tapped'])
            if self.effect == 'charge_damage':
                self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charges)
        if self.effect == 'clue':
            self.count(self.owner, 'battlefield', 'Clue', sum(self.cards[ref]['zone']=='battlefield' for ref in self.clues))
            self.count(self.foe, 'battlefield', 'Clue', 0)

    def payment_for(self, ref, qualifies):
        from recipes import cost_from_oracle
        cost = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        cost[0] += self.fee() if qualifies else 0
        return cost

    def witness(self, ref, *, countered=False):
        item = self.cards[ref]
        who = item['player']
        kinds = set(types(self.oracle[item['name']]))
        qualifies = who == self.owner and bool(kinds & {'Instant', 'Sorcery'})
        pool = self.payment_for(ref, qualifies)
        plan = (lambda board: board.decl_choices(ref)) if qualifies else None
        self.cast_trigger_spell(ref, funding=pool, choices=plan, resolve=False)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        if qualifies:
            self.reward()
        self.reward_receipts()
        self.observe_mana()
        if countered:
            self.cast('counter', response=True)
            self.plain_resolve('counter', response=True)
            self.move(ref, 'graveyard')
            self.emit('assert_stack', value=0)
        else:
            self.plain_resolve(ref, 'battlefield' if 'Creature' in kinds else 'graveyard')
            amount = {'Divination': 2, 'Reach Through Mists': 1}.get(item['name'], 0)
            self.draw_cards(who, amount)

    def facts(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.observe_mana()
        self.reward_receipts()
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                names = Counter(piece['name'] for piece in self.cards.values()
                                if piece['player'] == seat and piece['zone'] == zone)
                self.emit('assert_zone', player=seat, zone=zone, value=names.total())
                for name, value in sorted(names.items()):
                    self.count(seat, zone, name, value)
            for index, ref in enumerate(self.libraries[seat]):
                self.emit('assert_zone_card', player=seat, zone='library', index=index,
                          value=self.rows[self.cards[ref]['name']]['csvid'])

    def clue_use(self):
        ref = self.clues[0]
        paid = self.mode == 'clue_paid'
        self.fund(self.owner, [2 if paid else 1, 0, 0, 0, 0, 0])
        self.emit('can_activate', ref=ref, value=int(paid))
        self.emit('activate', ref=ref, value=int(paid))
        self.emit('assert_stack', value=int(paid))
        if paid:
            self.pools[self.owner] = [0]*6
            self.move(ref, 'gone')
            self.count(self.owner, 'battlefield', 'Clue', 0)
            self.draw_cards(self.owner, 1)
            self.emit('resolve')

    def combat(self, ref, power, *, ground=None, flyer=None, allowed=True):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=int(allowed))
        if allowed:
            self.stat(ref, 'attacking', 1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if ground:
            self.emit('block', ref=ground, other=ref, value=0)
        if flyer:
            self.emit('block', ref=flyer, other=ref, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        if allowed and not flyer:
            self.life[1-self.active] -= power

    def run(self):
        self.play_actual_source()
        if self.effect == 'untap':
            self.cast_trigger_spell('tap_spell', target='source',
                                    choices=lambda board: board.emit('choose_dialog', player=self.foe, value=0))
            self.cards['source']['tapped'] = True
            self.stat('source', 'tapped', 1)
        if self.mode in ('flying', 'walker_target'):
            self.age()
            if self.mode == 'walker_target':
                self.cast_trigger_spell('victim')
                self.emit('assert_counter', ref='victim', counter_kind='loyalty', value=3)
            self.age()
        self.witness('spell', countered=self.mode == 'countered')
        if 'second_spell' in self.cards:
            self.witness('second_spell')
        if self.mode == 'expiry':
            self.age()
            self.stats = list(self.shape['stats'])
        if self.mode == 'flying':
            self.combat('source', self.stats[0], ground='ground', flyer='air')
            self.move('air', 'graveyard')
            self.move('source', 'graveyard')
        if self.mode in ('clue_paid', 'clue_short'):
            self.clue_use()
        self.facts()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    result = []
    for label in selected:
        mode, seat = label.rsplit('_', 1)
        board_type = FollowerBoard if shape['effect'] in FOLLOW_SCENES else TriggerBoard
        board = board_type(name, shape, mode, int(seat), rows, oracle)
        board.emit('turn', player=board.owner)
        board.emit('present_ai_dialogs', value=1)
        for actor in (0, 1):
            board.fund(actor, [0]*6)
        start = len(board.steps)
        board.run()
        if mode == 'replay':
            opening, body = board.steps[:start], board.steps[start:]
            board.steps = opening + [step('capture')] + body + [
                step('record_state'), step('restore'), step('ai_roundtrip'),
            ] + body + [step('assert_recorded_state')]
        omissions = ['The ordinary AI policy decides whether to decline optional triggers.'] if shape['effect'] in OPTIONAL | FOLLOW_OPTIONAL else []
        result.append(dict(name=f'CastInstantOrSorceryRidersRecipe_{csvid}_{label}', card=name, fixture='empty_main',
                           seed=123, setup=board.setup, steps=board.steps, _recipe=dict(family=FAMILY, version=2,
                           variant=label, params=declaration['params'], oracle_text=declaration['oracle_text'],
                           untested_components=omissions)))
    return result

class FollowerBoard(TriggerBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.energy = self.animated = self.spells_seen = 0
        self.tokens = []
        self.victim_tapped = False
        self.wizard_increment = 0
        self.current_spell = None
        super().__init__(name, shape, mode, owner, rows, oracle)

    def build(self):
        super().build()
        owner, foe, mode, effect = self.owner, self.foe, self.mode, self.effect
        if mode in ('retarget', 'storm_retarget'):
            self.cards['spell']['name'] = 'Lightning Bolt'
            self.setup[next(i for i, obj in enumerate(self.setup) if obj['ref'] == 'spell')]['name'] = 'Lightning Bolt'
        if mode in ('storm_three', 'storm_reset', 'storm_retarget', 'scaled_combat', 'blocked_draw', 'spend_energy'):
            if 'second_spell' not in self.cards:
                self.add('Reach Through Mists', 'second_spell', owner, 'hand')
            if effect in ('storm', 'tower'):
                self.add('Reach Through Mists', 'third_spell', owner, 'hand')
        if effect == 'devil' and mode in ('upkeep', 'upkeep_wrong_type'):
            self.add('Grizzly Bears', 'wrong_grave', owner, 'graveyard')
            self.add('Giant Growth', 'opposing_grave', foe, 'graveyard')
            if mode == 'upkeep':
                self.add('Divination', 'own_grave', owner, 'graveyard')
        if mode in ('defender_only', 'artifact_prowess', 'copy_entry', 'entry_short'):
            name = 'Icy Manipulator' if mode == 'artifact_prowess' else 'Grizzly Bears'
            self.cards['spell']['name'] = name
            next(obj for obj in self.setup if obj['ref'] == 'spell')['name'] = name
        if effect == 'wizard':
            self.add('Prodigal Sorcerer', 'wizard', owner)
            self.add('Grizzly Bears', 'nonwizard', owner)
            self.add('Prodigal Sorcerer', 'enemy_wizard', foe)
        if effect == 'freeze':
            self.add('Grizzly Bears', 'victim', foe)
        if mode in ('flying', 'flying_haste'):
            self.add('Hill Giant', 'ground', foe)
            if effect != 'wizard':
                self.add('Wind Drake', 'air', foe)
        if mode == 'blocked_draw':
            self.add('Hill Giant', 'blocker', foe)
        if effect == 'animate':
            self.add('Island', 'wrong_land', foe)
            self.add('Incandescent Soulstoke', 'lord', owner)
            if mode != 'no_own_land':
                self.add('Forest', 'land', owner, 'hand' if mode == 'fresh_land_attack' else 'battlefield')
        if effect == 'construct' and mode in ('reclaim', 'below_reclaim'):
            for index in range(5 if mode == 'reclaim' else 4):
                self.add('Darksteel Relic', f'artifact_{index}', owner)
            for index, name in enumerate(('Grizzly Bears', 'Divination', 'Giant Growth')):
                self.add(name, f'grave_{index}', owner, 'graveyard')
            self.add('Divination', 'foe_grave', foe, 'graveyard')

    def copy_fee(self):
        if self.mode in ('fee_short', 'decline'):
            return [0]*6
        if self.effect == 'mirari':
            return [3,0,0,0,0,0]
        if self.effect == 'riku':
            return [0,0,1,0,1,0]
        return [0]*6

    def payment_for(self, ref, qualifies):
        cost = super().payment_for(ref, qualifies)
        if qualifies and self.effect in COPY_MODELS:
            cost = [a+b for a,b in zip(cost, self.copy_fee())]
        return cost

    def decl_choices(self, ref):
        effect = self.effect
        if effect in FOLLOW_OPTIONAL and self.mode != 'decline' and self.owner == 0:
            self.emit('choose_trigger_source', player=self.owner, ref='source')
        if self.mode == 'decline':
            return
        if effect == 'scry_one':
            self.prepare_scry(1)
        if effect == 'animate' and self.mode != 'no_own_land':
            self.emit('choose_target', player=self.owner, ref='land')
        if effect == 'freeze':
            self.emit('choose_target', player=self.owner, ref='victim')
        if effect in COPY_MODELS and self.cards[ref]['name']=='Lightning Bolt':
            copies = self.spells_seen if effect == 'storm' else int(self.mode != 'fee_short')
            for _ in range(copies):
                self.emit('choose_player', player=self.owner, value=self.owner)

    def token(self, name, stats, label, *, subtype=None):
        ref = f'{label}_{len(self.tokens)}'
        ordinal = sum(c['name']==name and c['player']==self.owner and c['zone']=='battlefield'
                      for c in self.cards.values())
        self.emit('bind_permanent_copy', player=self.owner, value=self.rows[name]['csvid'], index=ordinal, ref=ref)
        self.cards[ref] = dict(name=name, player=self.owner, zone='battlefield', tapped=False)
        self.tokens.append(ref)
        for key, value in zip(('power', 'toughness'), stats):
            self.stat(ref, key, value)
        if subtype:
            for key, value in (('artifact', 1), ('token', 1), ('color', 0), ('subtype_' + subtype, 1)):
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows[name]['csvid'], property=key, value=value)

    def reward(self):
        effect = self.effect
        if self.mode == 'decline':
            return
        if effect == 'devil':
            self.stats[0] += 4
        elif effect == 'cyclops':
            self.stats[0] += 3
        elif effect == 'scry_one':
            self.value_scry(1)
        elif effect == 'spellblade':
            self.charges += 1
        elif effect == 'animate' and self.mode != 'no_own_land':
            self.animated += 3
        elif effect == 'freeze':
            self.stats = [n+1 for n in self.stats]
            self.victim_tapped = True
        elif effect == 'tower':
            self.energy += 2
        elif effect == 'construct':
            value = {'Reach Through Mists': 1, 'Divination': 3}[self.cards[self.current_spell]['name']]
            self.token('Construct', (value, value), 'construct', subtype='Construct')
        elif effect == 'wizard':
            self.stats = [n+1 for n in self.stats]
            self.wizard_increment += 1
        elif effect in COPY_MODELS and self.mode != 'fee_short':
            copies = self.spells_seen if effect == 'storm' else 1
            self.spells_seen += 1
            self.pools[self.owner] = [a-b for a,b in zip(self.pools[self.owner], self.copy_fee())]
            name = self.cards[self.current_spell]['name']
            if name == 'Lightning Bolt':
                self.life[self.owner] -= 3*copies
            else:
                self.draw_cards(self.owner, copies * (2 if name=='Divination' else 1))

    def reward_receipts(self):
        super().reward_receipts()
        effect = self.effect
        if effect == 'spellblade' and self.cards['source']['zone'] == 'battlefield':
            self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charges)
        if effect == 'tower':
            self.emit('assert_energy', player=self.owner, value=self.energy)
            self.emit('assert_energy', player=self.foe, value=0)
            self.stat('source', 'tapped', self.cards['source']['tapped'])
        if effect == 'freeze':
            self.stat('victim', 'tapped', self.victim_tapped)
        if effect == 'animate' and self.animated:
            self.emit('assert_counter', ref='land', counter_kind='plus1', value=self.animated)
            self.emit('assert_counter', ref='wrong_land', counter_kind='plus1', value=0)
            self.stat('land', 'power', self.animated + 1)
            self.stat('land', 'toughness', self.animated + 1)
        if effect == 'wizard':
            for ref, power, toughness in (('wizard', 1+self.wizard_increment, 1+self.wizard_increment),
                                          ('nonwizard', 2, 2), ('enemy_wizard', 1, 1)):
                self.stat(ref,'power',power)
                self.stat(ref,'toughness',toughness)

    def witness(self, ref, *, countered=False):
        self.current_spell = ref
        name = self.cards[ref]['name']
        if name == 'Icy Manipulator':
            self.cast_trigger_spell(ref, resolve=False)
            self.emit('refresh')
            self.stats = [n+1 for n in self.stats]
            self.reward_receipts()
            self.plain_resolve(ref, 'battlefield')
            return
        if name == 'Lightning Bolt':
            self.emit('choose_player', player=self.owner, value=self.foe)
        super().witness(ref, countered=countered)
        if name == 'Lightning Bolt' and not countered:
            self.life[self.foe] -= 3

    def two_real_turns(self):
        self.age()
        self.age()

    def run_upkeep(self):
        self.age(upkeep=True)
        self.age(upkeep=True)
        if 'own_grave' in self.cards:
            self.move('own_grave', 'hand')

    def riku_entry(self):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.oracle['Grizzly Bears'])
        paid = self.mode == 'copy_entry'
        if paid:
            fee[2] += 1
            fee[3] += 1
        self.cast_trigger_spell('spell', funding=fee, resolve=False)
        if self.owner == 0:
            self.emit('choose_trigger_source', player=self.owner, ref='source')
        self.plain_resolve('spell', 'battlefield')
        if paid:
            self.pools[self.owner][2] -= 1
            self.pools[self.owner][3] -= 1
            self.token('Grizzly Bears', (2,2), 'bear_copy')
            self.count(self.owner, 'battlefield', 'Grizzly Bears', 2)
            self.emit('destroy', ref='spell')
            self.move('spell', 'graveyard')
            for prop, value in (('color',8), ('token',1), ('subtype_Bear',1)):
                self.emit('assert_matching_permanents', player=self.owner, index=self.rows['Grizzly Bears']['csvid'],
                          property=prop, value=value)

    def allow_wizard_flying(self):
        self.combat('source', self.stats[0], ground='ground')

    def finish_flying(self):
        self.combat('source', self.stats[0], ground='ground', flyer='air')
        self.move('air', 'graveyard')
        self.stat('source','damage',2)

    def spellblade_combat(self):
        block = self.mode == 'blocked_draw'
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if block:
            self.emit('block',ref='blocker',other='source',value=1)
        self.emit('advance_combat')
        if not block and self.owner == 0:
            self.emit('choose_trigger_source', player=0, ref='source')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if block:
            self.move('source','graveyard')
            self.stat('blocker','damage',2)
        else:
            self.life[self.foe]-=self.stats[0]
            self.draw_cards(self.owner,self.charges)

    def tower_use(self):
        paid = self.mode == 'spend_energy'
        self.emit('can_activate',ref='source',value=int(paid))
        if paid:
            self.emit('choose_player',player=self.owner,value=self.foe)
        self.emit('activate',ref='source',value=int(paid))
        if paid:
            self.energy -= 5
            self.cards['source']['tapped'] = True
            self.emit('assert_energy',player=self.owner,value=self.energy)
            self.emit('resolve')
            self.life[self.foe]-=3
            self.emit('can_activate',ref='source',value=0)

    def reclaim(self):
        paid = self.mode == 'reclaim'
        self.fund(self.owner,[3,0,2,0,0,0])
        self.emit('can_activate',ref='source',value=int(paid))
        self.emit('activate',ref='source',value=int(paid))
        if paid:
            self.pools[self.owner] = [0]*6
            self.move('source','exile')
            self.count(self.owner,'exile',self.cards['source']['name'],1)
            self.emit('resolve')
            for ref in ('spell','grave_1','grave_2'):
                self.move(ref,'hand')

    def run(self):
        self.play_actual_source()
        mode, effect = self.mode, self.effect
        if mode in ('copy_entry', 'entry_short'):
            self.riku_entry()
            self.facts()
            return
        if mode in ('upkeep', 'upkeep_wrong_type'):
            self.run_upkeep()
            self.facts()
            return
        if mode in ('flying','scaled_combat','blocked_draw', 'attack_enabled','defender_only','attack_expiry'):
            self.two_real_turns()
        if mode == 'fresh_land_attack':
            self.emit('play_land',ref='land',value=1)
            self.move('land','battlefield')
        if mode == 'storm_retarget':
            self.witness('second_spell')
        self.witness('spell',countered=mode=='countered')
        if 'second_spell' in self.cards and mode != 'storm_retarget':
            self.witness('second_spell')
        if 'third_spell' in self.cards:
            if mode == 'storm_reset':
                self.two_real_turns()
                self.spells_seen = 0
            self.witness('third_spell')
        if mode in ('expiry','attack_expiry','freeze_expires'):
            self.age()
            self.stats = list(self.shape['stats'])
            self.wizard_increment = 0
        if mode == 'freeze_expires':
            self.stat('victim','tapped',1)
            self.two_real_turns()
            self.victim_tapped = False
            self.stat('victim','tapped',0)
        if mode in ('defender_only','attack_expiry','attack_enabled'):
            if mode == 'attack_expiry':
                self.age()
            self.combat('source', self.stats[0], allowed=mode=='attack_enabled')
        if mode == 'fresh_land_attack':
            self.combat('land', self.animated + 1)
        if mode == 'later_land_mana':
            self.two_real_turns()
            self.emit('can_activate',ref='land',value=1)
            self.emit('activate',ref='land',value=1)
            self.pools[self.owner][3] = 1
            self.stat('land','tapped',1)
        if mode == 'flying':
            self.finish_flying()
        if mode == 'flying_haste':
            self.allow_wizard_flying()
        if mode in ('scaled_combat','blocked_draw'):
            self.spellblade_combat()
        if mode in ('spend_energy','short_energy'):
            self.tower_use()
        if mode in ('reclaim','below_reclaim'):
            self.reclaim()
        self.facts()
