"""Soulbond links formed by real entries and demonstrated by their printed grants."""
import json
import re
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'soulbond_creature'
DATA = Path(__file__).parent / 'data/soulbond_creature_wave8_profiles.json'
SOULBOND = ('Soulbond (You may pair this creature with another unpaired creature when either enters. '
            'They remain paired for as long as you control both of them.)')
PAIRED = 'As long as this creature is paired with another creature, '
STATS = re.compile(r'each of those creatures gets \+([0-9]+)/\+([0-9]+)\.')
KEYWORDS = {'reach', 'first strike', 'haste', 'lifelink', 'deathtouch', 'trample', 'double strike', 'vigilance', 'flying'}
HEXPROOF = "both creatures have hexproof. (They can't be the targets of spells or abilities your opponents control.)"
PREPAIRS = ("Druid's Familiar", 'Trusted Forcemage')
GENERIC = {'Grizzly Bears': '', 'Hill Giant': '', 'Goblin Piker': '', 'Suntail Hawk': 'Flying',
           'Deadly Insect': "Shroud (This creature can't be the target of spells or abilities.)",
           'Giant Spider': 'Reach (This creature can block creatures with flying.)'}
WITNESSES = {*GENERIC, *PREPAIRS, 'Unsummon', 'Counterspell', 'Control Magic', 'Ray of Command',
             'Dazzling Ramparts', 'Elvish Mystic', 'Insurrection'}
MODES = ('source_entry', 'partner_entry', 'source_decline', 'no_partner', 'enemy_entry',
         'already_paired_entrant', 'already_paired_only', 'partner_leave', 'partner_reentry', 'source_reentry',
         'control_partner', 'control_source', 'shroud_partner', 'insufficient_mana', 'wrong_color',
         'countered', 'pending_replay')
EXTRA_MODES = {
    'double strike': ('double_sole_blocker', 'double_two_hits'),
    'flying': ('flying_reach',),
    'hexproof': ('hexproof_rejects_control', 'hexproof_mass_control'),
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or not record.get('type_line', '').startswith('Creature — ')
            or any(not re.fullmatch(r'[0-9]+', record.get(key, '')) for key in ('power', 'toughness'))
            or set(record.get('keywords', [])) != {'Soulbond'}):
        raise ValueError('normal whole numeric Soulbond creature required')
    clauses = record['oracle_text'].splitlines()
    if len(clauses) != 2 or clauses[0] != SOULBOND or not clauses[1].startswith(PAIRED):
        raise ValueError('complete Soulbond rule and one continuous grant required')
    grant = clauses[1][len(PAIRED):]
    stats = STATS.fullmatch(grant)
    keyword = 'hexproof' if grant == HEXPROOF else next((key for key in KEYWORDS if grant == f'both creatures have {key}.'), None)
    if not stats and not keyword:
        raise ValueError('unreviewed Soulbond grant')
    mana = cost_from_oracle(record)
    if not any(mana[1:]) or all(mana[1:]):
        raise ValueError('colored Soulbond creature cost with an independent wrong color required')
    return dict(mana=mana, stats=[int(stats[1]), int(stats[2])] if stats else [0, 0],
                keyword=keyword, clauses=clauses, power=int(record['power']), toughness=int(record['toughness']))


def spec(declaration, record):
    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(model, params['mechanism'])):
        raise ValueError('complete independently pinned Soulbond record and exact declaration required')
    return dict(model, family=FAMILY, csvid=source['csvid'])


def variants(model):
    modes = MODES
    if model['keyword'] == 'hexproof':
        modes = tuple(mode for mode in modes if mode not in ('control_source', 'control_partner'))
    modes += EXTRA_MODES.get(model['keyword'], ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + ['partner_decline_0']


def validate_witnesses(oracle, rows):
    profile = pins()
    if set(profile) != {'sources', 'witnesses'} or set(profile['witnesses']) != WITNESSES:
        raise ValueError('exact Soulbond source and witness pins required')
    for group, pins_by_name in profile.items():
        for name, pin in pins_by_name.items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row.get('handler') in (None, 'NULL', '0') or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented complete Soulbond source or witness changed: ' + name)
            if row['handler'] == 'card_dummy' and (
                    group != 'witnesses' or name not in GENERIC or oracle[name]['oracle_text'] != GENERIC[name]):
                raise ValueError('generic callback must be an independently pinned simple combat witness')


class PairCase:
    def __init__(self, model, owner, rows, oracle):
        self.model, self.owner = model, owner
        self.foe, self.active = 1-owner, owner
        self.rows, self.oracle = rows, oracle
        self.keyword = model['keyword']
        self.setup, self.actions = [], []
        self.cards, self.zones, self.controls = {}, {}, {}
        self.fresh, self.tapped, self.damage = set(), set(), {}
        self.link, self.previous_link = None, None
        self.pools = [[0] * 6 for _ in (0, 1)]
        self.life = [20, 20]

    def add(self, name, ref, player, zone='battlefield'):
        from recipes import card

        self.setup.append(card(name, ref, player, zone))
        self.cards[ref], self.zones[ref], self.controls[ref] = name, zone, player

    def emit(self, op, **values):
        from recipes import step

        self.actions.append(step(op, **values))

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=value)

    def fund(self, player, amount):
        self.pools[player] = list(amount)
        for color, value in enumerate(amount):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def price(self, ref):
        from recipes import cost_from_oracle

        return cost_from_oracle(self.oracle[self.cards[ref]])

    def cast(self, ref, *, target=None, answer=True, budget=None, response=False):
        player = self.controls[ref]
        price = self.price(ref)
        self.fund(player, price if budget is None else budget)
        if target:
            self.emit('choose_target', ref=target, player=player)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(answer))
        if answer:
            self.pools[player] = [have - need for have, need in zip(self.pools[player], price)]
            self.zones[ref] = 'stack'
            self.emit('assert_stack', value=2 if response else 1)
            self.observe(ref, 'in_hand', 0)

    def enter(self, ref):
        self.zones[ref] = 'battlefield'
        self.fresh.add(ref)

    def pair_choice(self, trigger, entrant, participant, *, decline=False):
        # The generic creature picker is only the UI for source entry; it
        # deliberately ignores Shroud because Soulbond is not targeting.
        if self.owner == 0:
            self.emit('choose_trigger', ref=trigger, player=0)
        if trigger == entrant:
            self.emit('choose_optional_target' if decline else 'choose_target', ref=participant, player=self.owner)

    def source_resolve(self, participant=None, *, decline=False):
        if participant:
            self.pair_choice('source', 'source', participant, decline=decline)
            if not decline:
                self.link = participant
        self.emit('resolve')
        self.enter('source')
        self.receipt()
        if decline:
            self.emit('assert_forced_target_choices', value=0)

    def other_enters(self, ref, *, pair=False):
        self.cast(ref)
        if pair:
            self.pair_choice('source', ref, ref)
            self.link = ref
        self.emit('resolve')
        self.enter(ref)
        self.receipt()

    def next_turn(self):
        assert not any(any(pool) for pool in self.pools)
        self.emit('advance_turn')
        self.active = 1-self.active
        self.fresh.difference_update(ref for ref, player in self.controls.items() if player == self.active)
        self.damage.clear()

    def bind(self, ref, op, player):
        self.emit(op, ref=ref, player=player, index=0, value=self.rows[self.cards[ref]]['csvid'])

    def bounce(self, ref):
        self.cast('response', target=ref)
        self.emit('resolve')
        self.zones['response'], self.zones[ref] = 'graveyard', 'hand'
        self.tapped.discard(ref)
        self.fresh.discard(ref)
        self.damage.pop(ref, None)
        self.link = None
        self.bind(ref, 'bind_hand_copy', self.owner)
        self.receipt()

    def transfer(self, ref):
        if self.cards['response'] == 'Control Magic':
            self.next_turn()
        self.cast('response', target=ref)
        self.emit('resolve')
        self.zones['response'] = 'battlefield' if self.cards['response'] == 'Control Magic' else 'graveyard'
        self.controls[ref] = self.foe
        self.fresh.add(ref)
        self.link = None
        self.bind(ref, 'bind_permanent_copy', self.foe)
        self.receipt()

    def mass_transfer(self):
        self.next_turn()
        self.cast('response')
        self.emit('resolve')
        self.zones['response'] = 'graveyard'
        self.link = None
        for ref in ('source', 'partner'):
            self.controls[ref] = self.foe
            self.bind(ref, 'bind_permanent_copy', self.foe)
        self.receipt()
        self.hexproof_probe('source')
        self.emit('cleanup')
        for ref in ('source', 'partner'):
            self.controls[ref] = self.owner
            self.bind(ref, 'bind_permanent_copy', self.owner)
        self.receipt()

    def stats(self, ref):
        record = self.oracle[self.cards[ref]]
        power, toughness = int(record['power']), int(record['toughness'])
        if self.link is not None and ref in ('source', self.link):
            power += self.model['stats'][0]
            toughness += self.model['stats'][1]
        if self.previous_link and ref in ('bond_witness', 'partner'):
            change = mechanism(self.oracle[self.cards['bond_witness']])['stats']
            power += change[0]
            toughness += change[1]
        return power, toughness

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for color, amount in enumerate(self.pools[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile'):
                cards = Counter(self.rows[name]['csvid'] for ref, name in self.cards.items()
                                if self.zones[ref] == zone and self.controls[ref] == player)
                self.emit('assert_zone', player=player, zone=zone, value=sum(cards.values()))
                for csvid, count in sorted(cards.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=csvid, value=count)
        for ref, zone in self.zones.items():
            if zone == 'battlefield' and self.oracle[self.cards[ref]]['type_line'].startswith('Creature — '):
                power, toughness = self.stats(ref)
                for field, value in (('in_play', 1), ('tapped', int(ref in self.tapped)), ('power', power),
                                     ('toughness', toughness), ('damage', self.damage.get(ref, 0)),
                                     ('plus1_counters', 0), ('minus1_counters', 0)):
                    self.observe(ref, field, value)

    def combat_start(self, attacker, allowed=True, *, vigilance=False):
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(allowed))
        if allowed and not vigilance:
            self.tapped.add(attacker)
        self.emit('finish_attackers')
        self.emit('begin_blockers')

    def block(self, blocker, attacker, allowed):
        self.emit('block', ref=blocker, other=attacker, value=int(allowed))

    def combat_end(self):
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.receipt()

    def is_linked(self, ref):
        return self.link is not None and ref in ('source', self.link)

    def hexproof_probe(self, ref):
        for creature in dict.fromkeys(('source', 'partner', ref)):
            if self.zones.get(creature) != 'battlefield':
                continue
            shroud = 'Shroud' in self.oracle[self.cards[creature]]['keywords']
            for spell in ('owner_probe', 'foe_probe'):
                enemy_spell = self.controls[spell] != self.controls[creature]
                self.emit('can_target_creature', ref=spell, other=creature,
                          value=int(not shroud and not (enemy_spell and self.is_linked(creature))))

    def resolve_blocked_damage(self, attacker, *, keyword, linked):
        power, toughness = self.stats(attacker)
        return_power, blocker_toughness = self.stats('blocker')
        spill = max(0, power-blocker_toughness) if keyword == 'trample' and linked else 0
        dealt = power-spill
        if keyword == 'double strike' and linked:
            if dealt >= blocker_toughness:
                return_power = 0
            else:
                dealt += power
        if dealt >= blocker_toughness or (keyword == 'deathtouch' and linked and dealt > 0):
            self.zones['blocker'] = 'graveyard'
        if return_power >= toughness:
            self.zones[attacker] = 'graveyard'
        self.damage[attacker] = return_power
        self.damage['blocker'] = dealt
        self.life[1-self.controls[attacker]] -= spill

    def grant_probe(self, ref):
        if not self.keyword:
            return
        if self.keyword == 'hexproof':
            self.hexproof_probe(ref)
            return
        linked = self.is_linked(ref)
        if self.keyword == 'reach':
            if self.active != self.foe:
                self.next_turn()
            self.combat_start('flyer')
            self.block(ref, 'flyer', linked)
            if linked:
                self.zones['flyer'] = 'graveyard'
                self.damage[ref] = 1
            else:
                self.life[self.owner] -= 1
        else:
            player = self.controls[ref]
            if self.keyword == 'haste':
                if self.active != player or ref not in self.fresh:
                    raise ValueError('haste requires a creature that actually entered this turn')
                self.combat_start(ref, linked)
                if linked:
                    self.life[1-player] -= self.stats(ref)[0]
            else:
                while self.active != player or ref in self.fresh:
                    self.next_turn()
                self.combat_start(ref, vigilance=self.keyword == 'vigilance' and linked)
                power, toughness = self.stats(ref)
                if self.keyword == 'first strike':
                    self.block('blocker', ref, True)
                    blocker_power, blocker_toughness = self.stats('blocker')
                    self.damage['blocker'] = power
                    if power >= blocker_toughness:
                        self.zones['blocker'] = 'graveyard'
                    if not linked or power < blocker_toughness:
                        self.damage[ref] = blocker_power
                        if blocker_power >= toughness:
                            self.zones[ref] = 'graveyard'
                elif self.keyword == 'lifelink':
                    self.life[1-player] -= power
                    if linked:
                        self.life[player] += power
                elif self.keyword == 'flying':
                    reach = 'Reach' in self.oracle[self.cards['blocker']]['keywords']
                    blocked = reach or not linked
                    self.block('blocker', ref, blocked)
                    if blocked:
                        self.resolve_blocked_damage(ref, keyword=self.keyword, linked=linked)
                    else:
                        self.life[1-player] -= power
                elif 'blocker' in self.cards:
                    self.block('blocker', ref, True)
                    self.resolve_blocked_damage(ref, keyword=self.keyword, linked=linked)
                else:
                    self.life[1-player] -= power * (2 if self.keyword == 'double strike' and linked else 1)
        self.combat_end()


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    w = PairCase(model, owner, rows, oracle)
    poor = mode in ('insufficient_mana', 'wrong_color')
    uncast = poor or mode == 'countered'
    alone = mode in ('no_partner', 'partner_entry', 'partner_decline', 'enemy_entry')
    fresh_partner = model['keyword'] == 'haste' and mode == 'control_source'
    if not alone:
        w.add('Deadly Insect' if mode == 'shroud_partner' else 'Grizzly Bears', 'partner',
              owner, 'hand' if fresh_partner else 'battlefield')
    elif mode in ('partner_entry', 'partner_decline'):
        w.add('Grizzly Bears', 'partner', owner, 'hand')
    w.add(name, 'source', owner, 'hand')
    if mode in ('source_entry', 'already_paired_entrant', 'enemy_entry'):
        w.add('Hill Giant', 'newcomer', foe if mode == 'enemy_entry' else owner,
              'battlefield' if mode == 'source_entry' else 'hand')
    if mode == 'already_paired_only':
        other = next(witness for witness in PREPAIRS if witness != name)
        w.add(other, 'bond_witness', owner, 'hand')
    witness = {
        'partner_leave': 'Unsummon', 'partner_reentry': 'Unsummon', 'source_reentry': 'Unsummon',
        'countered': 'Counterspell',
    }.get(mode)
    if mode in ('control_partner', 'control_source'):
        witness = 'Ray of Command' if model['keyword'] == 'haste' else 'Control Magic'
    if mode == 'hexproof_rejects_control':
        witness = 'Control Magic'
    elif mode == 'hexproof_mass_control':
        witness = 'Insurrection'
    if witness:
        responder = owner if model['keyword'] == 'hexproof' and witness == 'Unsummon' else foe
        w.add(witness, 'response', responder, 'hand')
    if not uncast and model['keyword'] == 'reach':
        w.add('Suntail Hawk', 'flyer', foe)
    if not uncast and model['keyword'] == 'first strike':
        w.add('Goblin Piker', 'blocker', foe)
    if not uncast and model['keyword'] in ('deathtouch', 'trample', 'flying'):
        blocker = {'deathtouch': 'Dazzling Ramparts', 'trample': 'Elvish Mystic',
                   'flying': 'Giant Spider' if mode == 'flying_reach' else 'Goblin Piker'}[model['keyword']]
        w.add(blocker, 'blocker', foe)
    if mode in ('double_sole_blocker', 'double_two_hits'):
        w.add('Elvish Mystic' if mode == 'double_sole_blocker' else 'Dazzling Ramparts', 'blocker', foe)
    if model['keyword'] == 'hexproof':
        w.add('Unsummon', 'owner_probe', owner, 'hand')
        w.add('Unsummon', 'foe_probe', foe, 'hand')

    w.emit('reset_death_history')
    w.emit('turn', player=owner, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.fund(0, [0]*6)
    w.fund(1, [0]*6)
    if fresh_partner:
        w.other_enters('partner')
    if mode == 'already_paired_only':
        w.cast('bond_witness')
        w.pair_choice('bond_witness', 'bond_witness', 'partner')
        w.emit('resolve')
        w.enter('bond_witness')
        w.previous_link = 'partner'
        w.receipt()
    mana = model['mana'].copy()
    if mode == 'insufficient_mana':
        mana[next(i for i, amount in enumerate(mana) if amount)] -= 1
    elif mode == 'wrong_color':
        wrong = next(i for i in range(1, 6) if not mana[i])
        mana[1:] = [sum(mana[1:]) if i == wrong else 0 for i in range(1, 6)]
    w.cast('source', answer=not poor, budget=mana)
    replay_from = len(w.actions) if mode == 'pending_replay' else None
    if poor:
        w.receipt()
    elif mode == 'countered':
        w.emit('announce_spell', ref='source')
        w.cast('response', response=True)
        w.emit('resolve_response')
        w.zones['source'] = w.zones['response'] = 'graveyard'
        w.receipt()
    else:
        participant = None if alone or mode == 'already_paired_only' else 'partner'
        w.source_resolve(participant, decline=mode == 'source_decline')
        probe = 'source'
        if mode in ('partner_entry', 'partner_decline'):
            w.other_enters('partner', pair=mode == 'partner_entry')
            probe = 'partner'
        elif mode == 'enemy_entry':
            w.next_turn()
            w.other_enters('newcomer')
            if model['keyword'] == 'haste':
                probe = 'newcomer'
        elif mode == 'already_paired_entrant':
            w.other_enters('newcomer')
            probe = 'newcomer'
        elif mode in ('partner_leave', 'partner_reentry'):
            w.bounce('partner')
            if mode == 'partner_reentry':
                w.other_enters('partner', pair=True)
                probe = 'partner'
        elif mode == 'source_reentry':
            w.bounce('source')
            w.cast('source')
            w.source_resolve('partner')
        elif mode in ('control_partner', 'control_source'):
            victim = 'partner' if mode == 'control_partner' else 'source'
            w.transfer(victim)
            probe = 'source' if mode == 'control_partner' else 'partner'
        elif mode == 'hexproof_rejects_control':
            w.next_turn()
            w.cast('response', answer=False)
            w.receipt()
        elif mode == 'hexproof_mass_control':
            w.mass_transfer()
        w.grant_probe(probe)
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                    untested_components=['Decline on a later entry for the native mandatory AI resolution policy.',
                                         'Priority inside the atomic trigger resolver, multiple simultaneous entrants.'])
    return dict(name=f'SoulbondCreatureRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('distinct explicit reviewed Soulbond variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
