"""Paid Flash entry, typed target selection and temporary team effects."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'flash_entry_tribal_and_team'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
PACK, RATTLE, SHIELDMATE, CHAMPION, HUNT = (15913, 15928, 16082, 16127, 16193)
WOLF = 18856
SEED = 2717
LABEL = 'FlashEntryTribalAndTeamRecipe'
MODES = {
    PACK: ('discard', 'decline', 'nonland_only', 'opponent_land'),
    RATTLE: ('own_spirit', 'foreign_spirit', 'self_flight', 'spirit_permission', 'nonspirit_denied'),
    SHIELDMATE: ('team', 'combat_save', 'only_source'),
    CHAMPION: ('team', 'combat_lifelink', 'intrinsic_lifelink', 'only_source'),
    HUNT: ('mixed_team', 'only_source', 'combat_wolf', 'combat_werewolf'),
}
ZA = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0, 0, 0, 0, 0, 0)
GREEN = 0x08


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'
            or not record.get('type_line', '').startswith('Creature — ')
            or record['oracle_text'].splitlines()[0] != 'Flash'):
        raise ValueError('complete normal tribal/team Flash source required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct supported tribal/team Flash variants required')


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned tribal/team Flash source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned tribal/team Flash declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed tribal/team Flash fixture identity: {name}')


class Scene:
    def __init__(self, source, owner, mode, rows, oracle):
        self.name, self.id = source['name'], source['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {zone: [] for zone in ZA} for p in (0, 1)}
        self.life = [20, 20]
        self.add('source', self.name, owner, 'hand')
        self.add('parent', 'Goblin Piker', self.foe, 'hand')

    def emit(self, op, ref=None, *, prop=None, **values):
        if ref is not None:
            values['ref'] = ref
        if prop is not None:
            values['property'] = prop
        self.steps.append(dict(op=op, **values))

    def add(self, ref, name, who, zone='battlefield', *, tapped=False):
        item = dict(ref=ref, player=who, zone=zone, name=name)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.cards[ref] = (name, who, zone)
        self.zones[who][zone].append(name)

    def move(self, ref, zone):
        name, who, former = self.cards[ref]
        self.zones[who][former].remove(name)
        self.zones[who][zone].append(name)
        self.cards[ref] = name, who, zone

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def stats(self, ref, power_delta=0, toughness_delta=0):
        name, _, _ = self.cards[ref]
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'power', int(self.oracle[name]['power']) + power_delta)
        self.prop(ref, 'toughness', int(self.oracle[name]['toughness']) + toughness_delta)

    def mana(self, who, pool=ZERO, write=False):
        for color, value in enumerate(pool):
            self.emit('set_mana' if write else 'assert_mana', player=who, color=color, value=value)

    def fund(self, ref):
        name, who, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(who, cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')

    def announce_parent(self, ref='parent'):
        self.fund(ref)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana(self.foe)
        self.prop(ref, 'in_hand', 0)
        self.emit('announce_spell', ref)

    def flash(self, plan=None):
        self.announce_parent()
        self.fund('source')
        self.emit('require_can_cast', 'source', value=1)
        self.emit('cast_response', 'source', value=1)
        self.emit('assert_stack', value=2)
        self.prop('source', 'in_hand', 0)
        self.prop('source', 'in_play', 0)
        self.mana(self.owner)
        if plan is not None:
            plan()
        self.emit('resolve_response')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move('source', 'battlefield')
        self.move('parent', 'battlefield')
        self.emit('refresh')
        self.stats('source')
        self.stats('parent')

    def receipts(self, *extra):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        names = set(extra)
        for zones in self.zones.values():
            for cards in zones.values():
                names.update(cards)
        for who, zones in self.zones.items():
            self.emit('assert_life', player=who, value=self.life[who])
            self.mana(who)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                for name in sorted(names):
                    cid = self.rows[name]['csvid']
                    self.emit('assert_zone_multiplicity', player=who, zone=zone,
                              index=cid, value=cards.count(name))

    def pack(self):
        accept = self.mode == 'discard'
        offered_land = self.mode in ('discard', 'decline')
        has_hand = self.mode != 'opponent_land'
        if has_hand:
            self.add('foil_spell', 'Lightning Bolt', self.owner, 'hand')
        if offered_land:
            self.add('own_land', 'Forest', self.owner, 'hand')
        self.add('foe_land', 'Mountain', self.foe, 'hand')

        def choices():
            if has_hand:
                if self.owner == 0:
                    self.emit('choose_trigger', 'source', player=self.owner)
                self.emit('observe_search_filters', value=1)
                candidate = 'Forest' if offered_land else 'Lightning Bolt'
                self.emit('choose_zone_card' if accept else 'choose_optional_zone', player=self.owner,
                          value=self.rows[candidate]['csvid'])

        self.flash(choices)
        if has_hand:
            self.emit('assert_offered_legal_card', player=self.owner, index=self.rows['Lightning Bolt']['csvid'], value=0)
            self.emit('assert_forced_zone_choices', value=0)
            if offered_land:
                self.emit('assert_offered_legal_card', player=self.owner, index=self.rows['Forest']['csvid'], value=1)
        if accept:
            self.move('own_land', 'graveyard')
            self.zones[self.owner]['battlefield'].append('Wolf')
            for prop, value in (('token', 1), ('color', GREEN), ('subtype_Wolf', 1), ('power', 2), ('toughness', 2)):
                self.emit('assert_matching_permanents', player=self.owner, prop=prop, index=WOLF, value=value)
        self.receipts('Wolf')

    def rattle(self):
        own, enemy = self.owner, self.foe
        typed = self.mode != 'self_flight'
        permission = self.mode in ('spirit_permission', 'nonspirit_denied')
        if typed:
            self.add('own_spirit', 'Kami of Ancient Law', own)
            self.add('foe_spirit', 'Kami of Ancient Law', enemy)
            self.add('own_foil', 'Grizzly Bears', own)
            self.add('foe_probe', 'Prodigal Sorcerer', enemy)
            self.add('own_probe', 'Prodigal Sorcerer', own)
        else:
            self.add('ground_blocker', 'Grizzly Bears', enemy)
        if permission:
            self.add('late_parent', 'Walking Corpse', enemy, 'hand')
            self.add('nonspirit_hand', 'Glory Seeker', own, 'hand')
            if self.mode == 'spirit_permission':
                self.add('spirit_hand', 'Lantern Kami', own, 'hand')
        target = 'source' if not typed else 'foe_spirit' if self.mode == 'foreign_spirit' else 'own_spirit'
        if typed:
            self.target_probe('foe_probe' if target == 'own_spirit' else 'own_probe', target, 1)
        self.flash(lambda: self.emit('choose_target', target, player=own))
        if not typed:
            self.advance_owner()
            self.combat(('source',), behavior='flying', blocker='ground_blocker')
            self.life[enemy] -= 2
        else:
            opposite = 'foe_probe' if target == 'own_spirit' else 'own_probe'
            same = 'own_probe' if target == 'own_spirit' else 'foe_probe'
            self.target_probe(opposite, target, 0)
            self.target_probe(same, target, 1)
            self.target_probe('foe_probe', 'own_foil', 1)
            if permission:
                self.spirit_permission()
            self.emit('cleanup')
            self.emit('refresh')
            self.target_probe(opposite, target, 1)
        self.receipts()

    def target_probe(self, source, target, allowed):
        self.emit('can_target_creature', source, other=target, value=allowed)

    def spirit_permission(self):
        accept = self.mode == 'spirit_permission'
        self.announce_parent('late_parent')
        self.fund('spirit_hand' if accept else 'nonspirit_hand')
        self.emit('can_activate', 'source', value=int(accept))
        if accept:
            self.emit('choose_zone_card', player=self.owner, value=self.rows['Lantern Kami']['csvid'])
        self.emit('activate_response', 'source', value=int(accept))
        if accept:
            self.emit('assert_offered_count', value=1)
            self.emit('assert_offered_card', index=0, value=self.rows['Lantern Kami']['csvid'])
            self.move('spirit_hand', 'battlefield')
        else:
            # The declined non-Spirit spell cost remains in the actual pool.
            fee = cost_from_oracle(self.oracle['Glory Seeker'])
            self.mana(self.owner, fee)
            self.prop('nonspirit_hand', 'in_hand', 1)
            self.mana(self.owner, write=True)
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.move('late_parent', 'battlefield')
        self.stats('late_parent')
        if accept:
            self.stats('spirit_hand')
        self.prop('nonspirit_hand', 'in_hand', 1)
        self.mana(self.owner)

    def advance_owner(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('turn', player=self.owner)
        self.emit('refresh')

    def combat(self, attackers, *, behavior='unblocked', blocker=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker is not None:
            self.emit('block', blocker, other=attackers[0], value=int(behavior == 'blocked'))
        self.emit('advance_combat')
        if blocker is not None:
            self.prop(attackers[0], 'blocked', int(behavior == 'blocked'))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('end_combat')

    def shieldmate(self):
        effect = self.mode != 'only_source'
        if effect:
            self.add('ally', 'Grizzly Bears', self.owner)
            self.add('tapped_ally', 'Expedition Envoy', self.owner, tapped=True)
        self.add('foe_attacker', 'Walking Corpse', self.foe)
        self.flash()
        if effect:
            self.stats('ally', 0, 1)
            self.stats('tapped_ally', 0, 1)
            self.prop('tapped_ally', 'tapped', 1)
        self.stats('foe_attacker')
        if self.mode == 'combat_save':
            self.combat(('foe_attacker',), behavior='blocked', blocker='ally')
            self.move('foe_attacker', 'graveyard')
            self.stats('ally', 0, 1)
            self.prop('ally', 'damage', 2)
        self.emit('cleanup')
        self.emit('refresh')
        if effect:
            self.stats('ally')
            self.stats('tapped_ally')
            self.prop('ally', 'damage', 0)
        self.stats('source')
        self.receipts()

    def champion(self):
        effect = self.mode != 'only_source'
        if effect:
            self.add('human', 'Glory Seeker', self.owner)
            self.add('other_human', 'Expedition Envoy', self.owner)
        self.add('nonhuman', 'Grizzly Bears', self.owner)
        self.add('foe_human', 'Glory Seeker', self.foe)
        self.add('foe_attacker', 'Walking Corpse', self.foe)
        self.flash()
        if effect:
            self.stats('human', 1, 1)
            self.stats('other_human', 1, 1)
        for ref in ('nonhuman', 'foe_human', 'foe_attacker'):
            self.stats(ref)
        if self.mode == 'combat_lifelink':
            self.combat(('foe_attacker',), behavior='blocked', blocker='human')
            self.move('foe_attacker', 'graveyard')
            self.stats('human', 1, 1)
            self.prop('human', 'damage', 2)
            self.life[self.owner] += 3
        if self.mode == 'intrinsic_lifelink':
            self.advance_owner()
            self.stats('human')
            self.stats('source')
            self.combat(('source', 'human'))
            self.life[self.owner] += 3
            self.life[self.foe] -= 5
        else:
            self.emit('cleanup')
            self.emit('refresh')
        if effect:
            self.stats('human')
            self.stats('other_human')
        self.stats('source')
        self.receipts()

    def hunt(self):
        self.add('own_foil', 'Grizzly Bears', self.owner)
        self.add('foe_wolf', 'Quilled Wolf', self.foe)
        self.add('foe_werewolf', 'Lesser Werewolf', self.foe)
        eligible = []
        if self.mode in ('mixed_team', 'combat_wolf'):
            self.add('wolf', 'Quilled Wolf', self.owner)
            eligible.append('wolf')
        if self.mode in ('mixed_team', 'combat_werewolf'):
            self.add('werewolf', 'Lesser Werewolf', self.owner, tapped=self.mode == 'mixed_team')
            eligible.append('werewolf')
        if self.mode == 'mixed_team':
            self.add('changeling', 'Mothdust Changeling', self.owner)
            eligible.append('changeling')
        combat_modes = {
            'combat_wolf': ('Hill Giant', 'wolf'),
            'combat_werewolf': ('Craw Wurm', 'werewolf'),
        }
        combat = combat_modes.get(self.mode)
        if combat:
            attacker, _ = combat
            self.add('foe_attacker', attacker, self.foe)
        self.flash()
        unaffected = ('source', 'own_foil', 'foe_wolf', 'foe_werewolf')
        for ref in unaffected:
            self.stats(ref)
        for ref in eligible:
            self.stats(ref, toughness_delta=3)
        if self.mode == 'mixed_team':
            self.prop('werewolf', 'tapped', 1)
        if combat:
            attacker, defender = combat
            self.combat(('foe_attacker',), behavior='blocked', blocker=defender)
            self.stats(defender, toughness_delta=3)
            self.stats('foe_attacker')
            self.prop(defender, 'damage', int(self.oracle[attacker]['power']))
            self.prop('foe_attacker', 'damage', 2)
        self.emit('cleanup')
        self.emit('refresh')
        for ref in (*eligible, *unaffected):
            self.stats(ref)
        if combat:
            self.prop(defender, 'damage', 0)
            self.prop('foe_attacker', 'damage', 0)
        self.receipts()

    def play(self):
        self.emit('turn', player=self.foe)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        {PACK: self.pack, RATTLE: self.rattle, SHIELDMATE: self.shieldmate, CHAMPION: self.champion, HUNT: self.hunt}[self.id]()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    validate_variants(shape, selected)
    if pins()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered tribal/team Flash CSVID required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=[])))
    return cases
