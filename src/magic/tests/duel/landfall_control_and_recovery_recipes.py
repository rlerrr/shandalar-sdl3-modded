"""Actual landfall, linked exile, local life totals, and changing control."""

from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from landfall_case_support import LandfallBoard, landfall_lines


FAMILY = 'landfall_control_and_recovery'
PROFILE = Path(__file__).parent / 'data/landfall_control_and_recovery_wave8_profiles.json'
MODES = {
    'Admonition Angel': ('linked', 'own_target', 'release_two', 'only_self', 'flying'),
    'Bloodghast': ('return', 'exiled', 'haste', 'no_haste', 'cannot_block'),
    'Courser of Kruphix': ('top_land', 'top_nonland', 'empty_top', 'land_used', 'offturn_top'),
    'Eternity Vessel': ('from_below', 'from_above', 'new_count'),
    'Roil Elemental': ('steal', 'leave', 'lose_control', 'owner_hand', 'flying'),
}
COMMON = ('own_land', 'opposing_land', 'repeat', 'replay')
WITNESSES = {
    'Forest', 'Mountain', 'Plains', 'Darksteel Relic', 'Spellbook', 'Grizzly Bears', 'Runeclaw Bear',
    'Murder', 'Unsummon', 'Boomerang', 'Lightning Bolt', "Chaplain's Blessing", 'Mind Control',
    'Swords to Plowshares',
}


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    landfall_lines(record)
    entry = profile()['sources'].get(record.get('name'))
    if entry is None or not exact(record, entry['oracle']):
        raise ValueError('exact whole Oracle Landfall source required')
    return dict(source=record['name'])


def variants(model):
    name = model['source']
    result = [f'{mode}_{owner}' for mode in (*COMMON, *MODES[name]) for owner in (0, 1)]
    if name != 'Courser of Kruphix':
        result.append('decline_0')
    return result


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model)):
        raise ValueError('whole pinned version-two Landfall declaration required')
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(model))):
        raise ValueError('distinct compatible mirrored Landfall variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profile()
    if set(pins['sources']) != MODES.keys() or set(pins['witnesses']) != WITNESSES:
        raise ValueError('Landfall pin scope changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = registry.get(name, {})
        vanilla = name in {'Grizzly Bears', 'Runeclaw Bear'} and pin['oracle']['oracle_text'] == ''
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                or row.get('handler') == 'card_dummy' and not vanilla):
            raise ValueError('exact implemented Oracle or registry record changed for ' + name)


class RecoveryBoard(LandfallBoard):
    def build(self):
        self.name = self.cards['source']['name']
        self.life = [20, 20]
        self.charge = None
        self.library = [[], []]
        self.replay = None
        self.linked = []
        self.target_home = self.owner if self.mode == 'own_target' else self.foe
        if self.name != 'Courser of Kruphix' or self.mode != 'empty_top':
            top = 'Darksteel Relic' if self.mode == 'top_nonland' else 'Plains'
            self.library[self.owner] = [top, 'Mountain', 'Spellbook']
        self.library[self.foe] = ['Mountain', 'Darksteel Relic']
        for player, cards in enumerate(self.library):
            for index, card in enumerate(cards):
                self.add(card, f'library_{player}_{index}', player, 'library')
        land_actor = self.foe if self.mode == 'opposing_land' else self.owner
        self.add('Forest', 'land', land_actor, 'hand')
        if self.mode in ('repeat', 'release_two'):
            self.add('Mountain', 'next_land', self.owner, 'hand')
        if self.name == 'Admonition Angel' and self.mode not in ('only_self', 'flying'):
            self.add('Spellbook', 'subject', self.target_home)
            if self.mode in ('repeat', 'release_two'):
                self.add('Darksteel Relic', 'second_subject', self.owner)
        if self.name == 'Roil Elemental' and self.mode != 'flying':
            self.add('Grizzly Bears', 'subject', self.foe)
            if self.mode == 'repeat':
                self.add('Runeclaw Bear', 'second_subject', self.foe)
        if self.mode in ('release_two', 'leave'):
            self.add('Murder', 'remove', self.foe, 'hand')
        if self.mode == 'owner_hand':
            self.add('Unsummon', 'bounce', self.owner, 'hand')
        if self.mode == 'lose_control':
            self.add('Mind Control', 'mind', self.foe, 'hand')
        if self.name == 'Bloodghast':
            if self.mode == 'exiled':
                self.add('Swords to Plowshares', 'remove', self.foe, 'hand')
            elif self.mode not in ('haste', 'no_haste', 'cannot_block'):
                self.add('Murder', 'remove', self.foe, 'hand')
                if self.mode == 'repeat':
                    self.add('Murder', 'remove_again', self.foe, 'hand')
        if self.mode in ('flying', 'cannot_block'):
            self.add('Grizzly Bears', 'opposing_creature', self.foe)
        if self.name == 'Eternity Vessel':
            if self.mode == 'from_above':
                self.add("Chaplain's Blessing", 'life_spell', self.owner, 'hand')
            else:
                self.add('Lightning Bolt', 'life_spell', self.foe, 'hand')
                if self.mode == 'repeat':
                    self.add('Lightning Bolt', 'life_spell_again', self.foe, 'hand')
        if self.mode == 'new_count':
            self.add('Boomerang', 'bounce', self.owner, 'hand')

    def move(self, ref, zone, player=None):
        entry = self.cards[ref]
        entry['zone'] = zone
        if player is not None:
            entry['player'] = player

    def multiplicity(self, name, who, zone, amount):
        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=amount)

    def observe_subject(self, ref):
        entry = self.cards[ref]
        for player in (self.owner, self.foe):
            for zone in ('battlefield', 'graveyard', 'exile', 'hand'):
                self.multiplicity(entry['name'], player, zone, int((player, zone) == (entry['player'], entry['zone'])))

    def observe_life(self):
        for player, total in enumerate(self.life):
            self.emit('assert_life', player=player, value=total)

    def observe_library(self):
        for player, cards in enumerate(self.library):
            self.emit('assert_zone', player=player, zone='library', value=len(cards))
            for index, name in enumerate(cards):
                self.emit('assert_zone_card', player=player, zone='library', index=index, value=self.rows[name]['csvid'])

    def source_stats(self):
        record = self.oracle[self.name]
        self.observe_subject('source')
        if 'power' in record:
            for prop in ('power', 'toughness'):
                self.emit('assert_card', ref='source', property=prop, value=int(record[prop]))
        if self.charge is not None:
            self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charge)

    def spell(self, ref, target=None, player_target=None):
        # Player-target intents are consumed during casting, just like named targets.
        if player_target is not None:
            self.emit('choose_player', player=self.cards[ref]['player'], value=player_target)
        self.cast(ref, target=target)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        permanent = self.cards[ref]['name'] == 'Mind Control'
        self.move(ref, 'battlefield' if permanent else 'graveyard')
        if permanent:
            self.emit('assert_card', ref=ref, property='in_play', value=1)

    def remove_bloodghast(self, ref):
        self.spell(ref, target='source')
        exiled = self.mode == 'exiled'
        self.move('source', 'exile' if exiled else 'graveyard')
        if exiled:
            self.life[self.owner] += int(self.oracle[self.name]['power'])
        else:
            self.emit('prepare_graveyard_abilities')
        self.observe_subject('source')
        self.observe_life()

    def life_spell(self, again=False):
        if self.mode == 'from_above':
            self.spell('life_spell')
            self.life[self.owner] += 5
        else:
            self.spell('life_spell_again' if again else 'life_spell', player_target=self.owner)
            self.life[self.owner] -= 3
        self.observe_life()

    def enter_land(self, ref='land', target='subject'):
        name, mode = self.name, self.mode
        own = self.cards[ref]['player'] == self.owner
        wants = own and mode != 'decline'
        if name == 'Bloodghast':
            available = own and self.cards['source']['zone'] == 'graveyard'
            if available and self.owner == 0:
                self.emit('choose_dialog', player=0, value=int(not wants))
            if available and wants:
                self.move('source', 'battlefield')
        elif name == 'Courser of Kruphix':
            self.life[self.owner] += int(own)
        else:
            # Human optional trigger priority is accepted at the real land entry.
            # The AI accepts gains when favorable and may legally decline lowering its life.
            if name == 'Eternity Vessel' and self.owner == 1 and self.life[self.owner] > self.charge:
                wants = False
            if wants and self.owner == 0:
                self.emit('choose_trigger', player=0, ref='source')
            if wants and name in ('Admonition Angel', 'Roil Elemental') and mode != 'only_self':
                self.emit('choose_target', player=self.owner, ref=target)
                if name == 'Admonition Angel':
                    self.move(target, 'exile')
                    self.linked.append(target)
                else:
                    self.move(target, 'battlefield', self.owner)
            if wants and name == 'Eternity Vessel':
                self.life[self.owner] = self.charge
        self.play_land(ref)
        self.observe_life()
        self.observe_subject('source')
        for candidate in ('subject', 'second_subject'):
            if candidate in self.cards:
                self.observe_subject(candidate)
        if self.charge is not None:
            self.emit('assert_counter', ref='source', counter_kind='charge', value=self.charge)

    def next_own_turn(self):
        self.advance_turn()
        self.advance_turn()

    def fight(self):
        own_attack = self.mode != 'cannot_block'
        attacking = self.mode != 'no_haste'
        attacker = 'source' if own_attack else 'opposing_creature'
        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=int(attacking))
        self.emit('assert_card', ref=attacker, property='attacking', value=int(attacking))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.mode in ('cannot_block', 'flying'):
            blocker = 'opposing_creature' if own_attack else 'source'
            self.emit('block', ref=blocker, other=attacker, value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        if attacking:
            self.life[self.foe if own_attack else self.owner] -= int(self.oracle[self.cards[attacker]['name']]['power'])
        self.observe_life()
        self.observe_subject('source')

    def top_permission(self):
        allowed = self.mode == 'top_land'
        self.observe_library()
        self.emit('can_activate', ref='source', value=int(allowed))
        self.emit('activate', ref='source', value=int(allowed))
        if allowed:
            self.emit('resolve')
            self.emit('refresh')
            top = self.library[self.owner].pop(0)
            self.life[self.owner] += 1
            self.multiplicity(top, self.owner, 'battlefield', 1)
            self.multiplicity(top, self.owner, 'hand', 0)
            self.emit('can_activate', ref='source', value=0)
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.observe_library()
        self.observe_life()

    def run(self):
        mode, name = self.mode, self.name
        if name == 'Eternity Vessel':
            self.life[self.owner] = 12
            self.emit('set_life', player=self.owner, value=12)
            self.charge = 12
        if mode in ('haste', 'no_haste'):
            self.life[self.foe] = 10 if mode == 'haste' else 11
            self.emit('set_life', player=self.foe, value=self.life[self.foe])
        self.observe_life()
        self.play_source()
        self.source_stats()
        if mode in ('flying', 'cannot_block', 'haste', 'no_haste'):
            if mode == 'cannot_block':
                self.advance_turn()
            elif mode == 'flying':
                self.next_own_turn()
            self.fight()
            return
        if name == 'Bloodghast':
            self.remove_bloodghast('remove')
        if name == 'Eternity Vessel':
            self.life_spell()
            if mode == 'new_count':
                self.spell('bounce', target='source')
                self.move('source', 'hand')
                self.emit('bind_hand_copy', player=self.owner, value=self.rows[name]['csvid'], ref='source', index=0)
                self.observe_subject('source')
                self.charge = self.life[self.owner]
                self.play_source()
                self.source_stats()
        if mode in ('opposing_land', 'offturn_top'):
            self.advance_turn()
        if mode in ('top_land', 'top_nonland', 'empty_top', 'offturn_top'):
            self.top_permission()
            return
        if mode == 'replay':
            self.replay = len(self.steps)
        self.enter_land()
        if mode == 'land_used':
            self.top_permission()
        if mode in ('repeat', 'release_two'):
            self.next_own_turn()
            if name == 'Bloodghast':
                # The first true return generated a new object.
                self.bind('source')
                self.remove_bloodghast('remove_again')
            if name == 'Eternity Vessel':
                self.life_spell(again=True)
            self.enter_land('next_land', 'second_subject' if 'second_subject' in self.cards else 'subject')
        if mode in ('leave', 'release_two'):
            self.spell('remove', target='source')
            self.move('source', 'graveyard')
            self.observe_subject('source')
            for target in self.linked:
                self.move(target, 'battlefield')
                self.observe_subject(target)
            if mode == 'leave':
                self.move('subject', 'battlefield', self.foe)
                self.observe_subject('subject')
        if mode == 'lose_control':
            self.advance_turn()
            self.spell('mind', target='source')
            self.move('source', 'battlefield', self.foe)
            self.move('subject', 'battlefield', self.foe)
            self.observe_subject('source')
            self.observe_subject('subject')
        if mode == 'owner_hand':
            self.bind('subject')
            self.spell('bounce', target='subject')
            self.move('subject', 'hand', self.foe)
            self.observe_subject('subject')


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle, registry)
    if (model['source'] != name or registry[name]['csvid'] != csvid
            or not selected or set(selected) - set(variants(model))):
        raise ValueError('only exact registered Landfall cases may expand')
    gaps = {
        'Courser of Kruphix': ['The top-reveal UI is a card-name legacy without a typed public-visibility observer. Top identity and actual legal play are covered.'],
        'Admonition Angel': ['No between-entry-and-trigger priority window is exercised.'],
    }.get(name, [])
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = RecoveryBoard(name, model, mode, int(seat), registry, oracle)
        board.run()
        steps = board.steps
        if board.replay is not None:
            prefix, suffix = steps[:board.replay], steps[board.replay:]
            steps = [*prefix, step('capture'), *suffix, step('record_state'), step('restore'),
                     *suffix, step('assert_recorded_state')]
        cases.append(dict(name=f'LandfallControlRecoveryRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2298, setup=board.setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant,
                                       params=declaration['params'], oracle_text=declaration['oracle_text'],
                                       untested_components=gaps)))
    return cases
