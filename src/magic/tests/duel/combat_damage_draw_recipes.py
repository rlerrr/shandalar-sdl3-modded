"""Damage-triggered draw, paid companion actions and exact combat receipts."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from morph_creature_recipes import FACE_DOWN, MORPH


FAMILY = 'combat_damage_draw'
PROFILE = Path(__file__).parent / 'data/combat_damage_draw_wave8_profiles.json'
TEAM = 'Whenever a creature you control deals combat damage to '
BIDENT_FEE = '{1}{U}, {T}: Creatures your opponents control attack this turn if able.'
AURA = 'Enchant creature\nWhenever enchanted creature deals damage to an opponent, you may draw a card.'
SELF = 'Whenever this creature deals combat damage to a player, you may draw a card.'
INSIGHT = ('Choose target creature you control. Whenever that creature deals combat damage to a player or '
           'planeswalker this turn, draw that many cards.')
SHRIKE = ('Flying\nWhenever this creature deals combat damage to a player, you may sacrifice it. '
          'If you do, draw three cards.')
MASK = ('Whenever a source an opponent controls deals damage to you, if this artifact is untapped, '
        'you may draw a card.')
EDRIC = 'Whenever a creature deals combat damage to one of your opponents, its controller may draw a card.'
RULES = {
    'bident': (TEAM + 'a player, you may draw a card.\n' + BIDENT_FEE, 'Legendary Enchantment Artifact'),
    'piracy': (TEAM + 'an opponent, you may draw a card.', 'Enchantment'),
    'aura': (AURA, 'Enchantment — Aura'),
    'edric': (EDRIC, 'Legendary Creature — Elf Rogue'),
    'mask': (MASK, 'Artifact'),
    'insight': (INSIGHT, 'Instant'),
    'shrike': (SHRIKE, 'Creature — Phyrexian Bird'),
}
HANDLERS = {
    'bident': 'bident_of_thassa', 'piracy': 'coastal_piracy', 'aura': 'curiosity',
    'edric': 'edric_spymaster_of_trest', 'mask': 'farsight_mask', 'insight': 'hunters_insight',
    'morph': 'hystrodon', 'shrike': 'impaler_shrike',
}
LIBRARY = ('Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Grizzly Bears', 'Hill Giant',
           'Forest', 'Giant Spider', 'Island', 'Plains', 'Mountain')
WITNESSES = set(LIBRARY) | {'Fencing Ace', 'Safe Passage', 'Hermetic Study', 'Shock', 'Naturalize', 'Murder',
                           'Twiddle', "Mons's Goblin Raiders", 'Jace Beleren'}
DUMMIES = {'Grizzly Bears', 'Hill Giant', 'Giant Spider', 'Fencing Ace', "Mons's Goblin Raiders"}
COMMON = ('hit', 'prevented', 'blocked', 'noncombat', 'other_controller', 'source_removed', 'unfunded', 'replay')
EXTRA = {
    'bident': ('two_packets', 'two_steps', 'force_attack', 'force_short', 'force_owner_turn', 'force_expires',
               'force_only_ineligible'),
    'piracy': ('two_packets', 'two_steps'),
    'aura': ('wrong_attacker', 'two_steps', 'detach'),
    'edric': ('two_packets', 'two_steps', 'edric_attacks'),
    'mask': ('two_packets', 'two_steps', 'mask_tapped', 'mask_untapped', 'self_damage'),
    'insight': ('two_steps', 'planeswalker', 'wrong_attacker', 'wrong_target', 'expires'),
    'morph': ('trample_hit', 'hidden_attack', 'morph_reveal', 'morph_short', 'morph_offturn'),
    'shrike': ('flying_evasion',),
}
MULTI = {'bident', 'piracy', 'edric', 'mask'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal damage-draw card required')
    text = record['oracle_text']
    typ = record['type_line']
    mode = next((name for name, signature in RULES.items() if signature == (text, typ)), None)
    morph = None
    if mode is None and typ == 'Creature — Beast':
        lines = text.splitlines()
        match = MORPH.fullmatch(lines[2]) if len(lines) == 3 else None
        if lines[:2] == ['Trample', SELF] and match:
            mode = 'morph'
            morph = cost_from_oracle(dict(record, mana_cost=match[1]))
    expected = {'aura': ['Enchant'], 'morph': ['Morph', 'Trample'], 'shrike': ['Flying']}.get(mode, [])
    creature = 'Creature' in typ
    stats = [record.get('power'), record.get('toughness')]
    if (not mode or sorted(record['keywords']) != expected
            or creature and any(type(n) is not str or not n.isascii() or not n.isdecimal() for n in stats)):
        raise ValueError('unsupported whole damage-draw Oracle or printed characteristics')
    return dict(mode=mode, mana=cost_from_oracle(record), keywords=expected, power=int(stats[0]) if creature else None,
                toughness=int(stats[1]) if creature else None, morph=morph)


def handler(params):
    return 'manalinkeh_card_' + HANDLERS[params['mode']]


def variants(params):
    mode = params['mode']
    result = [f'{value}_{owner}' for value in COMMON + EXTRA[mode] for owner in (0, 1)]
    result += ['decline_0'] if mode != 'insight' else []
    result += ['partial_packets_0'] if mode in MULTI else []
    return result


def spec(declaration, record):
    values = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(values, sort_keys=True)):
        raise ValueError('pinned complete damage-draw Oracle and typed parameters required')
    selected = declaration.get('variants')
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(values))):
        raise ValueError('unknown or duplicate damage-draw variant')
    return dict(values, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    profile = profiles()
    if profile.get('version') != 1 or set(profile['witnesses']) != WITNESSES:
        raise ValueError('complete independent damage-draw witness pins required')
    for group in ('sources', 'witnesses'):
        for name, pin in profile[group].items():
            row = registry.get(name, {})
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row['handler'] in ('NULL', None) or row['handler'] == 'card_dummy' and
                    (name not in DUMMIES or not pin.get('reviewed_native_characteristics'))):
                raise ValueError(f'changed damage-draw witness or binding: {name}')
            if group == 'sources' and row['handler'] != handler(classify(oracle[name])):
                raise ValueError(f'changed damage-draw handler: {name}')
    hidden = profile['face_down']
    if registry.get(hidden['name']) != hidden or hidden['csvid'] != FACE_DOWN:
        raise ValueError('face-down representation changed')


class _World(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle, scenario):
        super().__init__(name, params, owner, registry, oracle)
        self.foe = 1 - owner
        self.mode = params['mode']
        self.scenario = scenario
        self.cards = {}
        self.face_down = False
        self.host = None
        self.insight = False
        self.shield = set()
        self.loyalty = None

    def add(self, name, ref, player=None, zone='hand', source=False, state=None):
        player = self.owner if player is None else player
        super().add(name, ref, player, zone, source)
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=state == 'tapped')
        if state:
            self.setup[-1]['state'] = [state]
            self.entities[ref][state] = True

    def move(self, ref, zone):
        info = self.cards[ref]
        name, player = info['name'], info['player']
        if ref in self.entities:
            if zone == 'battlefield':
                self.arrive(ref)
            else:
                self.leave(ref, zone)
        else:
            self.zones[info['zone']][player].remove(name)
            self.zones[zone][player].append(name)
        info.update(zone=zone, tapped=False)
        if ref == self.host and zone != 'battlefield' and self.mode == 'aura' and self.live('source'):
            self.move('source', 'graveyard')

    def live(self, ref):
        return self.cards[ref]['zone'] == 'battlefield'

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def resources(self, stack):
        self.emit('assert_stack', value=stack)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, amount in enumerate(self.mana[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if self.face_down and self.live('source'):
            source_id = self.rows[self.name]['csvid']
            for row in self.steps[start:]:
                if (row['op'] == 'assert_zone_multiplicity' and row['zone'] == 'battlefield'
                        and row['player'] == self.owner and row['index'] == source_id):
                    row['index'] = FACE_DOWN
            self.observe('source', 'csvid', FACE_DOWN)
        elif self.live('source'):
            self.observe('source', 'csvid', self.rows[self.name]['csvid'])
        for ref, entry in self.cards.items():
            if ref not in self.entities and self.live(ref):
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', entry['tapped'])
        if self.loyalty is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)

    def expire(self):
        super().expire()
        self.insight = False
        self.shield.clear()

    def next_turn(self):
        incoming = 1 - self.current
        for entry in self.cards.values():
            if entry['player'] == incoming and entry['zone'] == 'battlefield':
                entry['tapped'] = False
        super().next_turn()

    def pending(self, ref, *, target=None, creature=None, denied=False, pool=None, choice=None):
        from recipes import cost_from_oracle

        name, player = self.cards[ref]['name'], self.cards[ref]['player']
        price = cost_from_oracle(self.o[name]) if pool is None else pool
        self.pool(player, price)
        self.emit('refresh')
        if creature is not None:
            self.emit('choose_target', player=player, ref=creature)
        if target is not None:
            self.emit('choose_player', player=player, value=target)
        if choice is not None:
            self.emit('choose_dialog', player=player, value=choice)
        self.emit('cast', ref=ref, value=int(not denied))
        if not denied:
            self.mana[player] = [0] * 6
        self.emit('assert_unused_choices', value=0)
        self.resources(int(not denied))
        self.observe(ref, 'in_hand', denied)
        self.observe(ref, 'in_play', 0)

    def resolve_spell(self, ref, zone):
        self.emit('resolve')
        self.move(ref, zone)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def cast_source(self):
        scenario = self.scenario
        denied = scenario in ('unfunded', 'wrong_target')
        self.face_down = scenario in ('hidden_attack', 'morph_reveal', 'morph_short', 'morph_offturn')
        using_creature = self.mode in ('aura', 'insight') and not denied
        if using_creature:
            self.host = 'opposing' if scenario == 'other_controller' and self.mode == 'aura' else 'host'
        self.pending('source', creature=self.host if using_creature else None, denied=denied,
                     pool=[0] * 6 if scenario == 'unfunded' else [3, 0, 0, 0, 0, 0] if self.face_down else None,
                     choice=1 if self.mode == 'morph' and not self.face_down and not denied else None)
        if denied:
            self.receipts()
            return False
        if self.face_down:
            self.observe('source', 'csvid', FACE_DOWN)
            self.entities['source'].update(power=2, toughness=2, keys=set())
        if self.mode == 'insight':
            self.insight = True
        self.resolve_spell('source', 'graveyard' if self.mode == 'insight' else 'battlefield')
        return True

    def reveal(self, allowed=True):
        from recipes import COLORS

        price = list(self.p['morph'])
        if not allowed:
            price[COLORS['G']] -= 1
        self.pool(self.owner, price)
        self.emit('refresh')
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.mana[self.owner] = [0] * 6
            self.face_down = False
            self.entities['source'].update(power=self.p['power'], toughness=self.p['toughness'], keys={'Trample'})
            # Morph changes the permanent in the activation callback. The
            # driver still has to drain the legacy activation bookkeeping.
            self.emit('refresh')
            self.observe('source', 'csvid', self.rows[self.name]['csvid'])
            self.observe('source', 'power', self.p['power'])
            self.observe('source', 'toughness', self.p['toughness'])
            for color, amount in enumerate(self.mana[self.owner]):
                self.emit('assert_mana', player=self.owner, color=color, value=amount)
            self.emit('resolve')
        self.receipts()

    def draw(self, player, n):
        library = self.zones['library'][player]
        if len(library) < n:
            raise ValueError('insufficient library for nonterminal draw fixture')
        self.zones['hand'][player].extend(library[:n])
        del library[:n]

    def qualifies(self, ref, actor, recipient, shape):
        if self.mode == 'insight':
            return self.insight and actor == self.owner and ref == self.host and shape in ('combat', 'walker')
        if not self.live('source') or self.face_down:
            return False
        if self.mode == 'mask':
            return actor == self.foe and recipient == self.owner and shape != 'walker' and not self.cards['source']['tapped']
        if self.mode == 'aura':
            return ref == self.host and recipient == self.foe and shape != 'walker'
        if shape != 'combat' or recipient != self.foe:
            return False
        if self.mode in ('morph', 'shrike'):
            return ref == 'source'
        return actor == self.owner

    def plan_draw(self, packets, actor, recipient, shape='combat'):
        hits = [(ref, damage) for ref, damage in packets if damage > 0 and self.qualifies(ref, actor, recipient, shape)]
        count = len(hits)
        if not count:
            return
        declined = self.scenario == 'decline'
        partial = self.scenario == 'partial_packets'
        mode = self.mode
        if mode == 'insight':
            self.draw(self.owner, sum(damage for _, damage in hits))
            return
        optional = mode in ('bident', 'aura', 'shrike', 'morph', 'piracy', 'edric')
        if optional and self.owner == 0 and (not declined or mode in ('piracy', 'edric')):
            self.emit('choose_trigger_source', ref='source', player=0)
        if mode in ('piracy', 'edric', 'mask') or mode == 'bident' and not declined:
            # Each qualifying damage packet is still counted. The legacy UI
            # presents a single amount for a recipient within a damage step.
            if partial:
                self.emit('choose_dialog', player=self.owner, value=1)
                self.emit('choose_number', player=self.owner, value=1)
            else:
                self.emit('choose_dialog', player=self.owner, value=(1 if count == 1 else 2) if declined else 0)
        if declined:
            return
        draw_count = 1 if partial else count
        if mode == 'shrike':
            draw_count = 3
            self.move('source', 'graveyard')
        self.draw(self.owner, draw_count)

    def prepare_combat(self, actor, attackers, blocking=None, walker=False):
        self.emit('begin_attackers')
        for ref in attackers:
            if walker:
                # The normal declaration selects the actual opposing walker
                # using the already paired attack-tax bridge and UI.
                self.emit('choose_dialog', player=actor, value=1)
            self.emit('attack_with_tax' if walker else 'attack', ref=ref, value=1)
            self.entities[ref].update(tapped=True, attacking=True)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocking is not None:
            blocked = self.scenario != 'flying_evasion'
            self.emit('block', ref='blocker', other=blocking, value=int(blocked))
            self.observe('blocker', 'blocking_count', blocked)
        self.emit('advance_combat')

    def combat_damage(self, actor, attackers, blocking=None, walker=False):
        recipient = 1 - actor
        attack_steps = ([], [])
        for ref in attackers:
            entity = self.entities[ref]
            entity['attacking'] = False
            damage = entity['power']
            if ref == blocking and self.scenario != 'flying_evasion':
                blocker = self.entities['blocker']
                blocker['damage'] += min(damage, blocker['toughness']) if self.has(ref, 'Trample') else damage
                entity['damage'] += blocker['power']
                if blocker['damage'] >= blocker['toughness']:
                    self.move('blocker', 'graveyard')
                if entity['damage'] >= entity['toughness']:
                    self.move(ref, 'graveyard')
                damage = max(0, damage - blocker['toughness']) if self.has(ref, 'Trample') else 0
            if self.has(ref, 'Double strike'):
                attack_steps[0].append((ref, damage))
            attack_steps[1].append((ref, damage))
        for hits in attack_steps:
            if not walker and recipient in self.shield:
                hits = []
            total = sum(damage for _, damage in hits)
            if walker:
                self.loyalty -= total
            else:
                self.life[recipient] -= total
            self.plan_draw(hits, actor, recipient, 'walker' if walker else 'combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')
        self.receipts()

    def shield_before_damage(self, player):
        self.pending('shield')
        self.emit('resolve')
        self.move('shield', 'graveyard')
        self.shield.add(player)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def direct_damage(self):
        scenario = self.scenario
        if self.mode == 'mask':
            actor = self.owner if scenario == 'self_damage' else self.foe
            self.pending('shock', target=self.owner)
            self.life[self.owner] -= 2
            self.plan_draw([('shock', 2)], actor, self.owner, 'noncombat')
            self.resolve_spell('shock', 'graveyard')
            return
        host = 'source' if self.mode in ('morph', 'shrike') else 'host'
        self.pending('grant', creature=host)
        self.resolve_spell('grant', 'battlefield')
        self.emit('require_can_activate', ref='grant', value=1)
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('activate', ref='grant', value=1)
        self.resources(stack=1)
        self.entities[host]['tapped'] = True
        self.life[self.foe] -= 1
        self.plan_draw([(host, 1)], self.owner, self.foe, 'noncombat')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def twiddle(self, ref, tapped):
        self.pending(ref, creature='source')
        self.emit('choose_dialog', player=self.owner, value=int(not tapped))
        self.emit('resolve')
        self.move(ref, 'graveyard')
        self.cards['source']['tapped'] = tapped
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def force_attack(self):
        from recipes import COLORS

        scenario = self.scenario
        if scenario != 'force_owner_turn':
            self.next_turn()
        if scenario == 'force_only_ineligible':
            self.pending('opposing')
            self.resolve_spell('opposing', 'battlefield')
        price = [1, 0, 0, 0, 0, 0]
        price[COLORS['U']] = int(scenario != 'force_short')
        self.pool(self.owner, price)
        self.emit('refresh')
        allowed = scenario != 'force_short'
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.mana[self.owner] = [0] * 6
            self.cards['source']['tapped'] = True
        self.observe('source', 'tapped', allowed)
        self.resources(int(allowed))
        self.emit('resolve')
        self.receipts()
        if scenario == 'force_expires':
            self.age()
        self.emit('begin_required_attackers')
        required = ['opposing', 'second'] if scenario == 'force_attack' else []
        for ref in required:
            self.emit('can_finish_attackers', value=0)
            self.emit('attack', ref=ref, value=1)
            self.entities[ref].update(attacking=True, tapped=True)
        self.emit('can_finish_attackers', value=1)
        self.emit('finish_attackers')
        if required:
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.combat_damage(self.foe, required)
        else:
            self.receipts()


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle, scenario)
    mode, foe = w.mode, w.foe
    w.add(name, 'source', source=True)
    actor = foe if (mode == 'mask') != (scenario == 'other_controller') else owner
    host_name = 'Fencing Ace' if scenario == 'two_steps' else 'Grizzly Bears'
    if scenario != 'wrong_target':
        w.add(host_name, 'host', zone='battlefield')
    w.add(host_name if mode == 'mask' else 'Hill Giant', 'opposing', foe,
          'hand' if scenario == 'force_only_ineligible' else 'battlefield')
    if scenario in ('wrong_attacker', 'source_removed', 'detach'):
        w.add('Hill Giant', 'spare', zone='battlefield')
    if scenario in ('two_packets', 'partial_packets', 'force_attack'):
        w.add('Hill Giant', 'second', foe if scenario == 'force_attack' else actor, 'battlefield')
    if scenario in ('blocked', 'flying_evasion', 'trample_hit', 'morph_reveal', 'morph_offturn'):
        blocker_name = ("Mons's Goblin Raiders" if scenario in ('trample_hit', 'morph_reveal', 'morph_offturn')
                        else 'Hill Giant' if scenario == 'flying_evasion' else 'Giant Spider')
        w.add(blocker_name, 'blocker', 1 - actor, 'battlefield')
    for player in (0, 1):
        for i, card in enumerate(LIBRARY):
            w.add(card, f'lib_{player}_{i}', player, 'library')
    if scenario == 'planeswalker':
        w.add('Jace Beleren', 'walker', foe, 'battlefield')
        w.loyalty = int(oracle['Jace Beleren']['loyalty'])
    if scenario == 'prevented':
        w.add('Safe Passage', 'shield', 1 - actor)
    if scenario in ('mask_tapped', 'mask_untapped'):
        w.add('Twiddle', 'disable')
        if scenario == 'mask_untapped':
            w.add('Twiddle', 'enable')
    if scenario in ('noncombat', 'self_damage'):
        if mode == 'mask':
            w.add('Shock', 'shock', owner if scenario == 'self_damage' else foe)
        else:
            w.add('Hermetic Study', 'grant')
    if scenario in ('source_removed', 'detach'):
        physical_creature = scenario == 'detach' or mode in ('edric', 'morph', 'shrike', 'insight')
        w.add('Murder' if physical_creature else 'Naturalize', 'remove', foe)

    w.emit('turn', player=owner, value=0)
    w.pool(foe, [0] * 6)
    w.emit('present_ai_dialogs', value=1)
    if not w.cast_source():
        return w.setup, w.steps
    if scenario.startswith('force_'):
        w.force_attack()
        return w.setup, w.steps
    if scenario in ('source_removed', 'detach'):
        removed = 'host' if scenario == 'detach' or mode == 'insight' else 'source'
        w.pending('remove', creature=removed)
        w.emit('resolve')
        w.move('remove', 'graveyard')
        w.move(removed, 'graveyard')
        w.emit('assert_unused_choices', value=0)
        w.receipts()
    if scenario in ('mask_tapped', 'mask_untapped'):
        w.twiddle('disable', True)
        if scenario == 'mask_untapped':
            w.twiddle('enable', False)
    if scenario == 'expires':
        w.age()
    if mode in ('morph', 'shrike') and scenario not in ('other_controller', 'source_removed'):
        if scenario == 'morph_offturn':
            w.next_turn()
            w.reveal()
            w.next_turn()
        else:
            w.age()
            if scenario in ('morph_reveal', 'morph_short'):
                w.reveal(allowed=scenario == 'morph_reveal')
    if actor != w.current:
        w.next_turn()
    if scenario in ('noncombat', 'self_damage'):
        w.direct_damage()
        return w.setup, w.steps
    if scenario == 'prevented':
        w.shield_before_damage(1 - actor)

    attacker = 'opposing' if actor == foe else 'host'
    if actor == owner and (mode in ('morph', 'shrike') or scenario == 'edric_attacks'):
        attacker = 'source'
    if scenario in ('wrong_attacker', 'detach') or scenario == 'source_removed' and mode in ('morph', 'shrike', 'insight'):
        attacker = 'spare'
    if scenario == 'edric_attacks':
        w.age()
    attackers = [attacker] + (['second'] if scenario in ('two_packets', 'partial_packets') else [])
    blocking = attacker if 'blocker' in w.cards else None
    walker = scenario == 'planeswalker'
    w.prepare_combat(actor, attackers, blocking, walker)
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    w.combat_damage(actor, attackers, blocking, walker)
    if scenario == 'replay':
        suffix = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *suffix, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    actual = registry[name]
    if not pin or pin['registry'] != actual or actual['csvid'] != csvid or actual['handler'] != handler(params):
        raise ValueError(f'unpinned combat draw source: {name}')
    result = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['multiplayer damage recipients and benefit distribution',
                'priority after combat damage but before the draw trigger is handled']
        if params['mode'] in ('aura', 'morph', 'shrike', 'bident'):
            gaps.append('independent AI strategy for optional trigger refusal')
        result.append(dict(name=f'CombatDamageDrawRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                           setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                           params=declaration['params'], oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return result
