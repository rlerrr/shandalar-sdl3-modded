"""Paid creature casts with both printed halves of the Monument cycle."""

import copy
import json
import re
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'monument_cast_rewards'
PROFILE = Path(__file__).parent / 'data/monument_cast_rewards_wave8_profiles.json'
DISCOUNT = re.compile(r'(Black|Red|Blue|White|Green) creature spells you cast cost \{1\} less to cast\.')
EFFECTS = {
    'Whenever you cast a creature spell, each opponent loses 1 life and you gain 1 life.':
        ('B', 'drain', 'bontus_monument'),
    'Whenever you cast a creature spell, you may discard a card. If you do, draw a card.':
        ('R', 'loot', 'hazorets_monument'),
    "Whenever you cast a creature spell, target creature an opponent controls doesn't untap during its controller's next untap step.":
        ('U', 'freeze', 'kefnets_monument'),
    'Whenever you cast a creature spell, create a 1/1 white Warrior creature token with vigilance.':
        ('W', 'warrior', 'oketras_monument'),
    'Whenever you cast a creature spell, target creature you control gets +2/+2 and gains trample until end of turn.':
        ('G', 'trample', 'rhonass_monument'),
}
COLOR_NAMES = dict(Black='B', Red='R', Blue='U', White='W', Green='G')
MATCH = dict(B='Walking Corpse', R='Goblin Piker', U='Coral Merfolk', W='Silvercoat Lion', G='Grizzly Bears')
ONE = dict(B='Carrion Feeder', R='Goblin Balloon Brigade', U='Merfolk of the Pearl Trident',
           W='Savannah Lions', G='Nimble Mongoose')
MIXED = dict(B='Shambling Remains', R='Shambling Remains', U='Esper Cormorants',
             W='Esper Cormorants', G='Rhox Brute')
NONCREATURE = dict(B='Bad Moon', R='Goblin War Drums', U='Levitation', W='Honor of the Pure', G='Fecundity')
WITNESSES = set(MATCH.values()) | set(ONE.values()) | set(MIXED.values()) | set(NONCREATURE.values()) | {
    'Hill Giant', 'Gladecover Scout', 'Island', 'Forest', 'Mountain', 'Boomerang', 'Counterspell', 'Twiddle', 'Safe Passage',
}
DUMMIES = set(MATCH.values()) | {'Hill Giant', 'Merfolk of the Pearl Trident', 'Savannah Lions', 'Esper Cormorants', 'Rhox Brute'}
COMMON = ('matching', 'printed_budget', 'wrong_color', 'wrong_spell_color', 'mixed_color', 'floor', 'noncreature',
          'opposing_match', 'countered', 'short_source', 'source_removed', 'replay')
EXTRA = {
    'drain': ('repeat', 'through_prevention'),
    'loot': ('empty_hand',),
    'freeze': ('repeat', 'freeze_once', 'untapped_then_tap', 'no_target'),
    'warrior': ('repeat', 'vigilance', 'fresh_no_attack'),
    'trample': ('repeat', 'trample_damage', 'expiry', 'no_target'),
}
WARRIOR = 'Warrior'


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] != 'Legendary Artifact' or record['keywords'] or len(lines) != 2):
        raise ValueError('normal complete two-clause Legendary Monument required')
    discount = DISCOUNT.fullmatch(lines[0])
    if not discount or lines[1] not in EFFECTS:
        raise ValueError('unsupported printed Monument discount or cast reward')
    color, effect, _ = EFFECTS[lines[1]]
    cost = cost_from_oracle(record)
    if COLOR_NAMES[discount[1]] != color or cost != [3,0,0,0,0,0]:
        raise ValueError('printed Monument color/effect/cost mismatch')
    return dict(color=color, mode=effect, cost=cost, reduction=1, keywords=[])


def handler(record):
    return 'manalinkeh_card_' + EFFECTS[record['oracle_text'].splitlines()[1]][2]


def variants(params):
    mode = params['mode']
    names = [f'{scene}_{seat}' for scene in (*COMMON, *EXTRA[mode]) for seat in (0, 1)]
    return names if mode != 'loot' else [*names, 'decline_trigger_0', 'decline_discard_0']


def spec(declaration, record):
    p = classify(record)
    names = declaration.get('variants')
    pinned = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pinned or pinned['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('exact pinned Oracle discount and reward required')
    if (type(names) is not list or not names or any(type(item) is not str for item in names)
            or len(set(names)) != len(names) or not set(names) <= set(variants(p))):
        raise ValueError('distinct approved Monument variants required')
    return dict(p, family=FAMILY, effect=FAMILY)


def is_creature(record):
    return 'Creature' in record['type_line'].partition(' — ')[0].split()


def validate_witnesses(oracle, registry):
    p = profiles()
    if p.get('version') != 1 or set(p['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned Monument witnesses required')
    for group in ('sources','witnesses'):
        for name, pin in p[group].items():
            row, record = registry.get(name, {}), oracle.get(name)
            native = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != record or pin['registry'] != row or row.get('kind') != 'card'
                    or record['layout'] != 'normal' or row.get('handler') in (None,'NULL') or row['handler'] == 'card_dummy' and not native):
                raise ValueError(f'changed Monument source or witness: {name}')
            if group == 'sources' and (not classify(record) or row['handler'] != handler(record)):
                raise ValueError(f'changed Monument handler: {name}')
    token = p['warrior_engine_identity']
    if token != registry.get(WARRIOR) or token['csvid'] != 18852 or token['handler'] != 'manalinkeh_card_rhino_token':
        raise ValueError('changed native Warrior implementation')
    if p['warrior_oracle'] != oracle.get(WARRIOR) or oracle[WARRIOR]['layout'] != 'token':
        raise ValueError('changed Warrior token Oracle identity')
    for color in MATCH:
        for palette in (MATCH, ONE, MIXED, NONCREATURE):
            record = oracle[palette[color]]
            if (color not in record['colors'] or is_creature(record) == (palette is NONCREATURE)
                    or (len(record['colors']) > 1) != (palette is MIXED)):
                raise ValueError('different color or spell-kind witness')


class _World(_SpellWorld):
    def __init__(self, name, params, owner, registry, oracle, scene):
        super().__init__(name, params, owner, registry, oracle)
        self.scene = scene
        self.freeze = None
        self.pumps = {}
        self.warriors = {}

    def active(self):
        return self.cards['source'][2] == 'battlefield'

    def fund_cast(self, ref, *, discounted=True, limit=None, allowed=True):
        from recipes import cost_from_oracle

        name, actor, _ = self.cards[ref]
        record = self.o[name]
        fee = cost_from_oracle(record)
        qualifies = self.active() and actor == self.owner and is_creature(record) and self.p['color'] in record['colors']
        if qualifies:
            fee[0] = max(0, fee[0] - self.p['reduction'])
        budget = list(fee if discounted else cost_from_oracle(record)) if limit is None else list(limit)
        self.pool(actor, budget)
        self.emit('refresh')
        self.emit('cast', ref=ref, value=int(allowed))
        self.emit('assert_stack', value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.observe(ref, 'in_hand', not allowed)
        if allowed:
            self.mana[actor] = [paid - due for paid, due in zip(budget, fee)]
        self.wallets()

    def wallets(self):
        for seat in (0, 1):
            for color, amount in enumerate(self.mana[seat]):
                self.emit('assert_mana', player=seat, color=color, value=amount)

    def new_warrior(self):
        tid = self.rows[WARRIOR]['csvid']
        ref = f'warrior_{len(self.warriors)}'
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=len(self.warriors), value=tid)
        self.warriors[ref] = dict(tapped=False, sick=True)
        self.zones['battlefield'][self.owner].append(WARRIOR)
        self.warrior_receipts()

    def warrior_receipts(self):
        if self.p['mode'] != 'warrior':
            return
        from recipes import COLORS

        tid = self.rows[WARRIOR]['csvid']
        for seat in (0,1):
            self.emit('assert_zone_multiplicity', player=seat, zone='battlefield', index=tid,
                      value=len(self.warriors) if seat == self.owner else 0)
        for ref, status in self.warriors.items():
            self.observe(ref,'power',1)
            self.observe(ref,'toughness',1)
            for prop, val in status.items():
                self.observe(ref,prop,val)
        if self.warriors:
            for prop, value in (('token',1),('subtype_Warrior',1),('color',1 << COLORS['W'])):
                self.emit('assert_matching_permanents', player=self.owner, index=tid, property=prop, value=value)

    def receipts(self):
        super().receipts()
        self.warrior_receipts()

    def choose_loot(self):
        available = self.zones['hand'][self.owner].count('Island')
        deck = self.zones['library'][self.owner]
        if self.owner != 0:
            return
        if self.scene == 'decline_trigger':
            return
        self.emit('choose_trigger_source', ref='source', player=0)
        if self.scene in ('decline_discard','empty_hand'):
            fallback = 'Island' if available else deck[0]
            self.emit('choose_optional_zone', player=0, value=self.rows[fallback]['csvid'])
        else:
            self.emit('choose_zone_card', player=0, value=self.rows['Island']['csvid'])

    def reward(self, ref):
        name, actor, _ = self.cards[ref]
        qualifies = self.active() and actor == self.owner and is_creature(self.o[name])
        mode = self.p['mode']
        target = 'effect_target' if 'effect_target' in self.cards else None
        if qualifies and mode == 'loot':
            self.choose_loot()
        if qualifies and mode in ('freeze','trample') and target:
            # Only a real legal recipient is scripted. No-target cases prove the absent offer.
            self.emit('choose_target', ref=target, player=self.owner)
        self.emit('announce_spell', ref=ref)
        self.emit('assert_stack', value=1)
        self.emit('assert_unused_choices', value=0)
        if not qualifies:
            return
        if mode == 'drain':
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
            for seat, value in enumerate(self.life):
                self.emit('assert_life', player=seat, value=value)
        elif mode == 'loot':
            self.loot_result()
        elif mode == 'warrior':
            self.new_warrior()
        elif mode == 'freeze' and target:
            self.freeze = target
            self.observe(target,'tapped',self.entities[target]['tapped'])
        elif mode == 'trample' and target:
            self.emit('refresh')
            self.pumps[target] = self.pumps.get(target,0) + 2
            for prop in ('power','toughness'):
                self.entities[target][prop] += 2
                self.observe(target,prop,self.entities[target][prop])

    def loot_result(self):
        hand = self.zones['hand'][self.owner]
        deck = self.zones['library'][self.owner]
        take = self.scene not in ('decline_trigger','decline_discard','empty_hand')
        if self.owner == 0 and self.scene != 'decline_trigger':
            if take:
                self.emit('assert_offered_count', value=len(hand)-1)
            else:
                self.emit('assert_forced_zone_choices', value=0)
        if take:
            hand.remove('Island')
            self.zones['graveyard'][self.owner].append('Island')
            hand.append(deck.pop(0))
        self.emit('assert_zone', player=self.owner, zone='library', value=len(deck))
        for index, card in enumerate(deck):
            self.emit('assert_zone_card', player=self.owner, zone='library', index=index, value=self.rows[card]['csvid'])
        for witness in ('Island','Forest','Mountain'):
            for zone in ('hand','graveyard'):
                self.emit('assert_zone_multiplicity', player=self.owner, zone=zone, index=self.rows[witness]['csvid'],
                          value=self.zones[zone][self.owner].count(witness))

    def next_main(self):
        if any(map(any, self.mana)):
            raise ValueError('actual untap requires cleared pools')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        for ref, amount in self.pumps.items():
            for prop in ('power','toughness'):
                self.entities[ref][prop] -= amount
        self.pumps.clear()
        self.emit('untap_step', player=self.current)
        for ref, creature in self.entities.items():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature['sick'] = False
                if ref != self.freeze:
                    creature['tapped'] = False
        if self.freeze and self.current == self.foe:
            self.freeze = None
        if self.current == self.owner:
            for status in self.warriors.values():
                status.update(tapped=False, sick=False)
        self.emit('turn', player=self.current, value=0)
        self.receipts()

    def twiddle(self, ref, *, tap):
        actor = self.cards['twiddle'][1]
        self.cast('twiddle', target=ref)
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=actor, value=int(not tap))
        self.finish('twiddle', 'graveyard')
        self.emit('present_ai_dialogs', value=0)
        self.entities[ref]['tapped'] = tap
        self.receipts()

    def combat_step(self, attacker, *, blocker=None, allowed=True):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed))
        if attacker in self.warriors:
            self.observe(attacker,'tapped',0)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other=attacker, value=1)
            self.observe(blocker,'blocking_count',1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')

    def trample_combat(self):
        self.combat_step('effect_target',blocker='combat_foe')
        bearer = self.entities['effect_target']
        bearer.update(damage=2,tapped=True)
        self.move('combat_foe','graveyard')
        self.life[self.foe] -= 2
        self.receipts()

    def vigilance_combat(self, fresh=False):
        ref = next(iter(self.warriors))
        if not fresh:
            self.next_main()
            self.next_main()
        self.combat_step(ref, allowed=not fresh)
        if not fresh:
            self.life[self.foe] -= 1
        self.receipts()
        if not fresh:
            self.next_main()
            self.combat_step('combat_foe',blocker=ref)
            self.entities['combat_foe'].update(tapped=True,damage=1)
            self.zones['battlefield'][self.owner].remove(WARRIOR)
            del self.warriors[ref]
            self.receipts()


def fixture(w):
    scene, owner, foe, color, mode = w.scene, w.owner, w.foe, w.p['color'], w.p['mode']
    w.add(w.name,'source',owner,'hand')
    member = MATCH[color]
    if scene == 'wrong_spell_color':
        member = MATCH['R' if color == 'G' else 'G']
    elif scene == 'floor':
        member = ONE[color]
    elif scene == 'mixed_color':
        member = MIXED[color]
    elif scene == 'noncreature':
        member = NONCREATURE[color]
    actor = foe if scene == 'opposing_match' else owner
    if scene != 'short_source':
        w.add(member,'member',actor,'hand')
    if scene == 'repeat':
        w.add(MATCH[color],'second',owner,'hand')
    if scene in ('source_removed','countered'):
        w.add('Boomerang' if scene == 'source_removed' else 'Counterspell','reply',foe,'hand')
    if mode == 'loot':
        if scene != 'empty_hand':
            w.add('Island','spare',owner,'hand')
        w.add('Forest','top',owner,'library')
        w.add('Mountain','next',owner,'library')
    if mode == 'freeze':
        w.add('Hill Giant','friendly',owner)
        if scene == 'no_target':
            w.add('Gladecover Scout','protected',foe)
        else:
            w.add('Hill Giant','effect_target',foe)
            if scene != 'untapped_then_tap':
                w.setup[-1]['state'] = ['tapped']
                w.entities['effect_target']['tapped'] = True
        if scene == 'untapped_then_tap':
            w.add('Twiddle','twiddle',owner,'hand')
    if mode == 'trample':
        w.add('Grizzly Bears','combat_foe',foe)
        if scene != 'no_target':
            w.add('Grizzly Bears','effect_target',owner)
    if mode == 'warrior' and scene == 'vigilance':
        w.add('Grizzly Bears','combat_foe',foe)
    if scene == 'through_prevention':
        w.add('Safe Passage','prevention',foe,'hand')


def cast_episode(w):
    from recipes import COLORS, cost_from_oracle

    scene = w.scene
    if scene == 'opposing_match':
        w.next_main()
    if scene == 'source_removed':
        w.cast('reply',target='source')
        w.finish('reply','graveyard')
        w.move('source','hand')
        w.receipts()
    if scene == 'through_prevention':
        w.cast('prevention')
        w.finish('prevention','graveyard')
    record = w.o[w.cards['member'][0]]
    printed = cost_from_oracle(record)
    if scene in ('noncreature','opposing_match','source_removed','wrong_spell_color'):
        low = list(printed)
        low[0] -= 1
        w.fund_cast('member', limit=low, allowed=False)
        w.receipts()
    elif scene in ('wrong_color','floor'):
        low = list(printed)
        color = COLORS[w.p['color']]
        low[0] = sum(low) - 1 if scene == 'wrong_color' else 1
        low[color] = 0
        w.fund_cast('member',limit=low,allowed=False)
        w.receipts()
        if scene == 'wrong_color':
            return
    w.fund_cast('member', discounted=scene != 'printed_budget')
    w.reward('member')
    if scene == 'countered':
        w.cast('reply',parent='member')
        w.finish('reply','graveyard',response=True)
        w.move('member','graveyard')
    else:
        w.finish('member','battlefield')
    w.receipts()
    if scene == 'repeat':
        w.fund_cast('second')
        w.reward('second')
        w.finish('second','battlefield')
        w.receipts()
    elif scene == 'freeze_once' or scene == 'untapped_then_tap':
        if scene == 'untapped_then_tap':
            w.twiddle('effect_target',tap=True)
        w.next_main()
        w.next_main()
        w.next_main()
    elif scene in ('vigilance','fresh_no_attack'):
        w.vigilance_combat(fresh=scene == 'fresh_no_attack')
    elif scene in ('trample_damage','expiry'):
        if scene == 'trample_damage':
            w.trample_combat()
        w.next_main()


def build_variant(name, p, scene, owner, registry, oracle):
    from recipes import step

    w = _World(name,p,owner,registry,oracle,scene)
    fixture(w)
    w.emit('turn',player=owner,value=0)
    for seat in (0,1):
        w.pool(seat,[0]*6)
    w.receipts()
    w.cast('source',budget=[2,0,0,0,0,0] if scene == 'short_source' else None,denied=scene == 'short_source')
    if scene == 'short_source':
        w.receipts()
        return w.setup,w.steps
    w.finish('source','battlefield')
    w.receipts()
    if scene == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    cast_episode(w)
    if scene == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'),step('ai_roundtrip'),step('restore'),*tail,step('assert_recorded_state')])
    return w.setup,w.steps


def expand(name, csvid, declaration, p, selected, registry, oracle):
    row, record = registry[name], oracle[name]
    pin = profiles()['sources'].get(name)
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(record):
        raise ValueError(f'unpinned Monument source: {name}')
    cases = []
    for v in selected:
        scene, owner = v.rsplit('_',1)
        setup,steps = build_variant(name,p,scene,int(owner),registry,oracle)
        cases.append(dict(name=f'MonumentCastRewardsRecipe_{csvid}_{v}',card=name,fixture='empty_main',seed=123,
                          setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=v,params=declaration['params'],
                          oracle_text=declaration['oracle_text'],untested_components=['multiplayer','AI discard preference','text-changing effects'])))
    return cases
