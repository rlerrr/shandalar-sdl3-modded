"""Landfall responses whose printed effects change attachments or real combat."""

from collections import Counter
from pathlib import Path
import json
import re

from counter_condition_recurrence_recipes import exact
from landfall_case_support import LandfallBoard, landfall_lines


FAMILY = 'landfall_combat_and_animation'
PROFILE = Path(__file__).parent / 'data/landfall_combat_and_animation_wave8_profiles.json'
TRIGGER = 'Landfall — Whenever a land you control enters, '
SHROUD = "Shroud (This creature can't be the target of spells or abilities.)"
MENACE = "Menace (This creature can't be blocked except by two or more creatures.)"
DEATHTOUCH = 'Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)'
INTIMIDATE = ("this creature gains intimidate until end of turn. "
              "(It can't be blocked except by artifact creatures and/or creatures that share a color with it.)")
ANIMATE = ('you may have target land you control become a 3/3 Elemental creature with haste until end of turn. '
           "It's still a land.")
WITNESSES = frozenset({
    'Forest', 'Island', 'Plains', 'Walking Atlas', 'Saltfield Recluse', 'Prodigal Sorcerer',
    'Royal Assassin', 'Drift of Phantasms', 'Darksteel Myr', 'Icy Manipulator', 'Dryad Arbor', 'Elvish Mystic', 'Murder', 'Giant Growth',
})
TYPED_SOURCE = {
    'gear': 'Artifact — Equipment',
    'switch': 'Creature — Turtle',
    'intimidate': 'Creature — Surrakar',
    'lure': 'Creature — Basilisk',
    'menace': 'Creature — Elemental',
    'animate': 'Creature — Elemental',
}
MODES = {
    'gear': ('equip_land', 'unattached_land', 'two_entries', 'reattach', 'no_own_equip', 'short_equip'),
    'switch': ('switch', 'double_switch', 'shroud_own', 'shroud_foe'),
    'intimidate': ('intimidate', 'matching_color', 'artifact_blocker', 'before_land', 'expired_block'),
    'lure': ('forced_block', 'tapped_target', 'before_land', 'expired_block'),
    'menace': ('menace_one', 'menace_two', 'two_entries', 'first_entry_damage'),
    'animate': ('animate_combat', 'animate_old_land', 'land_mana', 'own_land_creature',
                'foe_land_creature', 'nonland_creature', 'intrinsic_keyword'),
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = landfall_lines(record)
    trigger, = [line for line in lines if line.startswith(TRIGGER)]
    body = trigger.removeprefix(TRIGGER)
    others = [line for line in lines if line != trigger]
    equip = re.fullmatch(r'Equip \{([1-9][0-9]*)\} \(\{\1\}: Attach to target creature you control\. Equip only as a sorcery\.\)',
                         others[0]) if len(others) == 1 else None
    pump = re.fullmatch(r'(equipped creature|this creature) gets \+([1-9][0-9]*)/\+([1-9][0-9]*) until end of turn\.', body)
    if pump and pump[1] == 'equipped creature' and equip:
        shape = dict(effect='gear', equip=int(equip[1]), power=int(pump[2]), toughness=int(pump[3]))
        keywords = {'Equip'}
    elif body == 'you may switch this creature\'s power and toughness until end of turn.' and others == [SHROUD]:
        shape, keywords = dict(effect='switch'), {'Shroud'}
    elif body == INTIMIDATE and not others:
        shape, keywords = dict(effect='intimidate'), set()
    elif body == 'you may have target creature block this creature this turn if able.' and others == [DEATHTOUCH]:
        shape, keywords = dict(effect='lure'), {'Deathtouch'}
    elif pump and pump[1] == 'this creature' and others == [MENACE]:
        shape = dict(effect='menace', power=int(pump[2]), toughness=int(pump[3]))
        keywords = {'Menace'}
    elif body == ANIMATE and len(others) == 2 and others[0] in ('Trample', 'Vigilance') and others[1] == (
            f'Land creatures you control have {others[0].lower()}.'):
        shape, keywords = dict(effect='animate', grant=others[0].lower()), {others[0]}
    else:
        raise ValueError('Unsupported full Landfall combat composition')
    keywords.add('Landfall')
    printed = record['keywords']
    if (set(printed) != keywords or len(printed) != len(keywords)
            or record['type_line'] != TYPED_SOURCE[shape['effect']]):
        raise ValueError('Landfall effect does not match the printed card types and keywords')
    if shape['effect'] != 'gear' and any(not re.fullmatch(r'[1-9][0-9]*', record[k]) for k in ('power', 'toughness')):
        raise ValueError('Fixed numeric creature stats required')
    return shape


def variants(shape):
    modes = ('opponent_land', 'cleanup', *MODES[shape['effect']])
    variants = [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]
    if shape['effect'] in ('switch', 'lure', 'animate'):
        variants.append('decline_0')
    return variants


def spec(declaration, record):
    shape = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('Exact whole Landfall declaration required')
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('Distinct supported Landfall variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    if set(data['witnesses']) != WITNESSES:
        raise ValueError('Independent Landfall witness set changed')
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = rows.get(name, {})
        if (row.get('kind') != 'card' or row.get('handler') in (None, 'card_dummy', 'NULL', '0')
                or not exact(row, pin['registry']) or not exact(oracle.get(name), pin['oracle'])):
            raise ValueError(f'Whole implemented Oracle or registry pin changed: {name}')
    return data


class CombatLandfallBoard(LandfallBoard):
    def build(self):
        self.life = [20, 20]
        self.effect = self.shape['effect']
        self.land_target = 'old_land' if self.mode == 'animate_old_land' else 'land'
        self.host = None
        self.switch = False
        self.animation = None
        self.boosts = {}
        self.replay = None
        if self.mode not in ('short_equip', 'no_own_equip', 'before_land', 'shroud_own', 'shroud_foe',
                             'own_land_creature', 'foe_land_creature', 'nonland_creature', 'intrinsic_keyword'):
            self.add('Forest', 'land', self.foe if self.mode == 'opponent_land' else self.owner, 'hand')
        if self.mode in ('two_entries', 'double_switch'):
            self.add('Island', 'second_land', self.owner, 'hand')
            self.add('Walking Atlas', 'atlas', self.owner)
        if self.effect == 'gear':
            self.add('Saltfield Recluse', 'host', self.foe if self.mode == 'no_own_equip' else self.owner)
            if self.mode == 'reattach':
                self.add('Prodigal Sorcerer', 'second_host', self.owner)
        if self.mode in ('shroud_own', 'shroud_foe'):
            self.add('Murder', 'targeter', self.owner if self.mode == 'shroud_own' else self.foe, 'hand')
        if self.effect == 'intimidate' and self.mode not in ('opponent_land', 'cleanup'):
            self.add({'matching_color': 'Royal Assassin', 'artifact_blocker': 'Darksteel Myr'}.get(
                self.mode, 'Saltfield Recluse'), 'blocker', self.foe)
        if self.effect == 'lure' and self.mode != 'opponent_land':
            self.add('Drift of Phantasms', 'blocker', self.foe)
            if self.mode == 'tapped_target':
                self.add('Icy Manipulator', 'tapper', self.owner)
        if self.mode in ('menace_one', 'menace_two'):
            self.add('Drift of Phantasms', 'blocker', self.foe)
            if self.mode == 'menace_two':
                self.add('Darksteel Myr', 'second_blocker', self.foe)
        if self.effect == 'animate':
            self.add('Plains', 'enemy_land', self.foe)
            if self.mode == 'animate_old_land':
                self.add('Island', 'old_land', self.owner)
            if self.mode in ('own_land_creature', 'foe_land_creature'):
                self.add('Dryad Arbor', 'field_creature', self.owner if self.mode == 'own_land_creature' else self.foe)
            if self.mode == 'nonland_creature':
                self.add('Saltfield Recluse', 'field_creature', self.owner)
            if self.shape['grant'] == 'trample' and self.mode in ('animate_combat', 'intrinsic_keyword', 'nonland_creature'):
                self.add('Elvish Mystic', 'blocker', self.foe)
            if self.shape['grant'] == 'trample' and self.mode in ('own_land_creature', 'foe_land_creature'):
                self.add('Darksteel Myr', 'blocker', self.foe if self.mode == 'own_land_creature' else self.owner)
            if self.shape['grant'] == 'trample' and self.mode in ('own_land_creature', 'foe_land_creature', 'nonland_creature'):
                self.add('Giant Growth', 'growth', self.foe if self.mode == 'foe_land_creature' else self.owner, 'hand')
            if self.mode in ('animate_combat', 'land_mana', 'animate_old_land'):
                self.add('Saltfield Recluse', 'other_creature', self.owner)

    def stat(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def stats(self, ref):
        power, toughness = (3, 3) if ref == self.animation else (
            int(self.oracle[self.cards[ref]['name']][key]) for key in ('power', 'toughness'))
        if self.switch and ref == 'source':
            power, toughness = toughness, power
        p, t = self.boosts.get(ref, (0, 0))
        return power + p, toughness + t

    def observed_subtype(self, ref, active):
        item = self.cards[ref]
        self.emit('assert_matching_permanents', player=item['player'], index=self.rows[item['name']]['csvid'],
                  property='subtype_Elemental', value=int(active))

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield', 'hand', 'graveyard', 'library', 'exile'):
                cards = Counter(self.rows[e['name']]['csvid'] for e in self.cards.values()
                                if e['player'] == player and e['zone'] == zone)
                self.emit('assert_zone', player=player, zone=zone, value=sum(cards.values()))
                for csvid, count in sorted(cards.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=csvid, value=count)
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.stat(ref, 'in_play', 1)
            self.stat(ref, 'tapped', card['tapped'])
            if ref == self.animation or self.oracle[card['name']].get('power') is not None:
                power, toughness = self.stats(ref)
                self.stat(ref, 'power', power)
                self.stat(ref, 'toughness', toughness)
                self.stat(ref, 'damage', card['damage'])
        if self.effect == 'animate' and self.land_target in self.cards:
            target = self.cards[self.land_target]
            if target['zone'] == 'battlefield':
                self.observed_subtype(self.land_target, self.animation == self.land_target)
            if self.cards['enemy_land']['zone'] == 'battlefield':
                self.observed_subtype('enemy_land', False)

    def equip(self, target=None, *, payable=True):
        fee = self.shape['equip']
        self.fund(self.owner, [fee if payable else 0, 0, 0, 0, 0, 0])
        self.emit('can_activate', ref='source', value=int(target is not None))
        if target is not None:
            self.emit('choose_target', player=self.owner, ref=target)
            self.emit('activate', ref='source', value=1)
            self.emit('assert_stack', value=1)
            self.pools[self.owner][0] -= fee
            self.host = target
            self.emit('resolve')
            self.emit('refresh')
        self.receipts()

    def land_choices(self):
        optional = self.effect in ('switch', 'lure', 'animate')
        accept = self.mode != 'decline'
        if not optional or not accept:
            return
        if self.owner == 0:
            self.emit('choose_trigger', player=self.owner, ref='source')
        targets = {'lure': 'blocker', 'animate': self.land_target}
        if self.effect in targets:
            self.emit('choose_target', player=self.owner, ref=targets[self.effect])

    def result_of_land(self):
        if self.mode == 'decline':
            return
        if self.effect == 'switch':
            self.switch = not self.switch
        if self.effect == 'animate':
            self.animation = self.land_target
        ref = self.host if self.effect == 'gear' else 'source' if self.effect == 'menace' else None
        if ref:
            p, t = self.boosts.get(ref, (0, 0))
            self.boosts[ref] = p + self.shape['power'], t + self.shape['toughness']

    def entry(self):
        if self.mode == 'opponent_land':
            self.advance_turn()
            self.play_land()
        else:
            self.play_land(choices=lambda board: board.land_choices())
            self.result_of_land()
        self.receipts()

    def additional_entry(self):
        self.emit('can_activate', ref='atlas', value=1)
        self.emit('activate', ref='atlas', value=1)
        self.emit('assert_stack', value=1)
        self.emit('choose_zone_card', player=self.owner, value=self.rows['Island']['csvid'])
        self.land_choices()
        self.emit('resolve')
        self.emit('refresh')
        self.cards['atlas']['tapped'] = True
        self.cards['second_land']['zone'] = 'battlefield'
        self.result_of_land()
        self.receipts()

    def cleanup(self):
        self.emit('cleanup')
        self.emit('refresh')
        self.animation = None
        self.switch = False
        self.boosts.clear()
        for entry in self.cards.values():
            entry['damage'] = 0
        self.receipts()

    def tap_blocker(self):
        # The defending seat has already taken an untap since the source's cast.
        self.fund(self.owner, [1, 0, 0, 0, 0, 0])
        self.emit('choose_target', player=self.owner, ref='blocker')
        self.emit('activate', ref='tapper', value=1)
        self.emit('assert_stack', value=1)
        self.pools[self.owner][0] -= 1
        self.emit('resolve')
        self.emit('refresh')
        self.cards['tapper']['tapped'] = True
        self.cards['blocker']['tapped'] = True
        self.receipts()

    def return_to_main(self):
        self.advance_turn()
        self.advance_turn()

    def damage_to(self, ref, amount, deathtouch=False):
        item = self.cards[ref]
        if amount >= self.stats(ref)[1] or (amount and deathtouch):
            if item['name'] != 'Darksteel Myr':
                item['zone'] = 'graveyard'
                return
        item['damage'] += amount

    def combat(self, attacker='source', *, blockers=(), denied=(), forced=None, lethal=False,
               minimum=1, vigilance=False, trample=False, leave_at_declaration=False, idle_blocker=None):
        card = self.cards[attacker]
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        card['tapped'] = not vigilance
        self.stat(attacker, 'tapped', not vigilance)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for ref in blockers + denied:
            self.emit('block', ref=ref, other=attacker, value=int(ref not in denied))
        assigned = (*blockers, forced) if forced else blockers
        if len(assigned) < minimum:
            assigned = ()
        self.emit('advance_combat')
        self.stat(attacker, 'blocked', bool(assigned))
        if idle_blocker:
            self.stat(idle_blocker, 'blocking_count', 0)
        for ref in dict.fromkeys(blockers + denied + ((forced,) if forced else ())):
            self.stat(ref, 'blocking_count', ref in assigned)
        self.receipts()
        if leave_at_declaration:
            return
        if len(assigned) > 1:
            raise ValueError('Two-blocker geometry is tested at the actual declaration boundary')
        power = self.stats(attacker)[0]
        defender = 1 - card['player']
        if assigned:
            blocker, = assigned
            block_power, block_toughness = self.stats(blocker)
            self.damage_to(blocker, min(power, block_toughness) if trample else power, lethal)
            self.damage_to(attacker, block_power)
            if trample:
                self.life[defender] -= max(0, power - block_toughness)
        else:
            self.life[defender] -= power
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('refresh')
        self.receipts()

    def consume_land_mana(self):
        ref = self.land_target
        self.emit('activate', ref=ref, value=1)
        self.cards[ref]['tapped'] = True
        self.pools[self.owner][{'Forest': 3, 'Island': 2}[self.cards[ref]['name']]] += 1
        self.receipts()

    def attack_animation(self):
        if self.mode in ('own_land_creature', 'foe_land_creature', 'nonland_creature'):
            attacker = 'field_creature'
        elif self.mode == 'intrinsic_keyword':
            attacker = 'source'
        else:
            attacker = self.land_target
        item = self.cards[attacker]
        if item['player'] != self.active:
            self.advance_turn()
        if 'growth' in self.cards:
            self.cast_trigger_spell('growth', target=attacker)
            self.boosts[attacker] = (3, 3)
        grants = (attacker == 'source' or
                  (item['player'] == self.owner and (attacker == self.animation or item['name'] == 'Dryad Arbor')))
        trample = grants and self.shape['grant'] == 'trample'
        vigilance = grants and self.shape['grant'] == 'vigilance'
        blockers = ('blocker',) if 'blocker' in self.cards else ()
        self.combat(attacker, blockers=blockers, trample=trample, vigilance=vigilance)

    def run(self):
        self.play_source()
        self.receipts()
        if self.effect in ('intimidate', 'lure', 'menace') or self.mode == 'intrinsic_keyword':
            self.return_to_main()
        if self.effect == 'gear' and self.mode != 'unattached_land':
            payable = self.mode != 'short_equip'
            target = 'host' if payable and self.mode != 'no_own_equip' else None
            self.equip(target, payable=payable)
            if self.mode == 'reattach':
                self.equip('second_host')
            if target is None:
                return
        if self.mode.startswith('shroud_'):
            self.emit('can_target_creature', ref='targeter', other='source', value=0)
            self.emit('can_cast', ref='targeter', value=0)
            self.receipts()
            return
        if self.mode in ('equip_land', 'switch', 'intimidate', 'forced_block', 'menace_two', 'animate_combat'):
            self.replay = len(self.steps)
        if 'land' in self.cards:
            self.entry()
        if self.mode in ('double_switch', 'two_entries'):
            self.additional_entry()
        if self.mode == 'tapped_target':
            self.tap_blocker()
        if self.mode == 'cleanup':
            self.cleanup()
        if self.mode == 'expired_block':
            self.return_to_main()
        if self.effect == 'intimidate' and 'blocker' in self.cards:
            denied = self.mode == 'intimidate'
            self.combat(blockers=() if denied else ('blocker',), denied=('blocker',) if denied else ())
        if self.effect == 'lure' and self.mode != 'opponent_land':
            if self.mode == 'cleanup':
                self.return_to_main()
            forced = 'blocker' if self.mode == 'forced_block' else None
            self.combat(forced=forced, lethal=bool(forced), idle_blocker=None if forced else 'blocker')
            if not forced:
                self.stat('blocker', 'in_play', 1)
        if self.effect == 'menace':
            if self.mode == 'menace_one':
                # The defending seat has no second creature and cannot declare this block.
                self.combat(denied=('blocker',), minimum=2)
            elif self.mode == 'menace_two':
                self.combat(blockers=('blocker', 'second_blocker'), minimum=2, leave_at_declaration=True)
            elif self.mode == 'first_entry_damage':
                self.combat()
        if self.effect == 'animate':
            if self.mode in ('animate_combat', 'animate_old_land', 'intrinsic_keyword', 'own_land_creature',
                             'foe_land_creature', 'nonland_creature'):
                self.attack_animation()
            elif self.mode == 'land_mana':
                self.consume_land_mana()
            elif self.mode in ('decline', 'cleanup'):
                self.emit('turn', player=self.owner)
                self.emit('begin_attackers')
                self.emit('attack', ref='land', value=0)
                self.stat('land', 'tapped', 0)
                self.emit('finish_attackers')
                self.emit('begin_blockers')
                self.emit('advance_combat')
                self.emit('combat_damage')
                self.emit('end_combat')
                self.receipts()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    data = validate_witnesses(oracle, rows)
    if (name not in data['sources'] or rows[name]['csvid'] != csvid or not selected
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('Selected variant must match a pinned Landfall source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        board = CombatLandfallBoard(name, shape, mode, int(owner), rows, oracle)
        board.run()
        steps = board.steps
        if board.replay is not None:
            first, tail = steps[:board.replay], steps[board.replay:]
            steps = [*first, step('capture'), *tail, step('record_state'), step('restore'),
                     step('ai_roundtrip'), *tail, step('assert_recorded_state')]
        gaps = ['Multiplayer, simultaneous independent trigger ordering, and response targeting across legacy ETB dispatch.']
        if shape['effect'] in ('switch', 'lure', 'animate'):
            gaps.append('Optional AI trigger choice is automatic; explicit decline is exercised only with the human controller.')
        if shape['effect'] == 'lure':
            gaps.append('Same-controller targets cannot demonstrate the required block in ordinary two-seat combat. No passive ETB target eligibility probe.')
        cases.append(dict(name=f'LandfallCombatAnimationRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2298, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       covered_clauses=declaration['oracle_text'].splitlines(),
                                       untested_components=gaps)))
    return cases
