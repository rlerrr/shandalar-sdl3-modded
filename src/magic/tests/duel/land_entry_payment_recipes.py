"""Land entry and its interaction with later activated mana."""
import json
from collections import Counter
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'land_entry_payment'
DATA = Path(__file__).parent / 'data/land_entry_payment_wave8_profiles.json'
TAPPED = 'This land enters tapped.'
PAY = 'When this land enters, sacrifice it unless you pay {1}.'
ANY = '{T}: Add one mana of any color.'
ADD = 'When this land enters, add one mana of any color.'
COLORLESS = '{T}: Add {C}. ({C} represents colorless mana.)'
HUB = ['When this land enters, you get {E} (an energy counter).', '{T}: Add {C}.',
       '{T}, Pay {E}: Add one mana of any color.']
COLORS = 'CBUGRW'
SPENDERS = {'B': 'Festering Goblin', 'U': 'Mothdust Changeling', 'G': 'Elvish Mystic',
            'R': 'Monastery Swiftspear', 'W': 'Soul Warden', 'C': 'Bonesplitter'}
WITNESSES = {'Forest', 'Mountain', 'Island', 'Wastes', 'Bloodbriar', 'Azorius Guildgate',
             'Saruli Gatekeepers', 'Sol Ring', 'Sakura-Tribe Scout', 'Boomerang', "Woodweaver's Puzzleknot",
             *SPENDERS.values()}
COMMON = ('slot_taken', 'off_turn', 'scout_put', 'next_turn_slot', 'ready_replay')
PAYMENT = ('unfunded', 'tapped_provider', 'opposing_provider', 'pay_wastes', 'pay_forest',
           'floating_forest', 'sacrifice_witness', 'gate_witness')
COLOR_MODES = tuple('color_' + symbol.lower() for symbol in COLORS[1:])
HUB_MODES = ('entry', 'free_colorless', 'spent_then_colorless', 'colored_cast', 'reenter', 'opposing_energy',
             'own_reserve', 'scout_entry', 'off_turn', 'ready_replay') + COLOR_MODES


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('mana_cost') or record.get('keywords') or record['type_line'] not in ('Land', 'Land — Gate')):
        raise ValueError('whole normal printed land required')
    clauses = record['oracle_text'].splitlines()
    if clauses == [TAPPED, PAY, ANY]:
        return dict(kind='fee', gate=record['type_line'] == 'Land — Gate', fee=1,
                    enter_tapped=True, entry_colors='', tap_colors=COLORS[1:])
    if clauses == [TAPPED, ADD, COLORLESS] and record['type_line'] == 'Land':
        return dict(kind='mana', gate=False, fee=0, enter_tapped=True, entry_colors=COLORS[1:], tap_colors='C')
    if clauses == HUB and record['type_line'] == 'Land':
        return dict(kind='energy', enter_tapped=False, entry_energy=1, energy_fee=1, tap_colors=COLORS)
    raise ValueError('whole printed tap, enter and payment or mana instructions required')


def spec(declaration, record):
    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(model, params['mechanism'])):
        raise ValueError('complete independently pinned land entry rules required')
    return dict(model, family=FAMILY, csvid=source['csvid'])


def variants(model):
    if model['kind'] == 'energy':
        return [f'{mode}_{owner}' for owner in (0, 1) for mode in HUB_MODES]
    fee = model['kind'] == 'fee'
    modes = COMMON + COLOR_MODES + (PAYMENT if fee else ('later_colorless',))
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes] + (['decline_0'] if fee else [])


def validate_witnesses(oracle, rows):
    from recipes import cost_from_oracle

    profile = pins()
    if set(profile) != {'sources', 'witnesses'} or set(profile['witnesses']) != WITNESSES:
        raise ValueError('exact land entry source and witness pins required')
    for group in profile.values():
        for name, pin in group.items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != pin['csvid'] or row.get('handler') != pin['handler']
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented whole land entry identity changed: ' + name)
    for color, name in SPENDERS.items():
        cost = cost_from_oracle(oracle[name])
        if sum(cost) != 1 or cost[COLORS.index(color)] != 1:
            raise ValueError('cast witness must consume the exact one-unit production: ' + name)


class EntryCase:
    def __init__(self, model, owner, rows, oracle):
        self.model, self.owner, self.rows, self.oracle = model, owner, rows, oracle
        self.setup, self.actions = [], []
        self.cards, self.zones, self.tapped = {}, {}, set()
        self.pool = [[0]*6 for _ in (0, 1)]
        self.life = [20, 20]
        self.sacrifices = 0

    def add(self, name, ref, player, zone='battlefield', *, tapped=False):
        from recipes import card

        desc = card(name, ref, player, zone)
        if tapped:
            desc['state'] = ['tapped']
            self.tapped.add(ref)
        self.setup.append(desc)
        self.cards[ref], self.zones[ref] = desc, zone

    def emit(self, op, **values):
        from recipes import step

        self.actions.append(step(op, **values))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=value)

    def zero_pools(self):
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=0)
            for color, amount in enumerate(self.pool[player]):
                self.emit('assert_mana', player=player, color=color, value=amount)
            for zone in ('hand', 'battlefield', 'graveyard', 'exile', 'library'):
                identities = Counter(self.rows[desc['name']]['csvid'] for ref, desc in self.cards.items()
                                     if self.zones[ref] == zone and desc['player'] == player)
                self.emit('assert_zone', player=player, zone=zone, value=sum(identities.values()))
                for identity, count in sorted(identities.items()):
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, index=identity, value=count)
        for ref, zone in self.zones.items():
            if zone == 'battlefield':
                self.observe(ref, 'in_play', 1)
                self.observe(ref, 'tapped', int(ref in self.tapped))
        if 'witness' in self.cards:
            self.observe('witness', 'plus1_counters', self.sacrifices)

    def use_mana(self, ref, color, *, choose=False, amount=1):
        player = self.cards[ref]['player']
        self.emit('require_can_activate', ref=ref, value=1)
        if choose:
            self.emit('choose_color', player=player, value=COLORS.index(color))
        self.emit('activate', ref=ref, value=1)
        self.tapped.add(ref)
        self.pool[player][COLORS.index(color)] += amount
        self.receipt()

    def spend(self, ref, life_gain=0):
        from recipes import cost_from_oracle

        player = self.cards[ref]['player']
        need = cost_from_oracle(self.oracle[self.cards[ref]['name']])
        self.pool[player] = [have - spent for have, spent in zip(self.pool[player], need)]
        if min(self.pool[player]) < 0:
            raise ValueError('cast must spend the actual previously produced mana')
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.zones[ref] = 'battlefield'
        self.life[player] += life_gain
        self.receipt()

    def plain_play(self, ref, allowed=True):
        self.emit('play_land', ref=ref, value=int(allowed))
        if allowed:
            self.zones[ref] = 'battlefield'
        self.receipt()

    def next_own_main(self):
        if any(any(pool) for pool in self.pool):
            raise ValueError('real untap requires actual prior expenditures')
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.tapped.difference_update(ref for ref, desc in self.cards.items() if desc['player'] == self.owner)
        self.emit('turn', player=self.owner, value=0)
        self.receipt()

    def source_entry(self, *, pay, color, floating=False, scout=False):
        fee = self.model['kind'] == 'fee'
        if scout:
            self.emit('require_can_activate', ref='scout', value=1)
            self.emit('activate', ref='scout', value=1)
            self.emit('assert_stack', value=1)
            self.tapped.add('scout')
            self.emit('choose_zone_card', player=self.owner, value=self.model['csvid'])
        if fee and self.owner == 0:
            if not pay:
                self.emit('choose_cancel_mana', player=self.owner)
            elif floating:
                self.emit('choose_mana', player=self.owner, color=COLORS.index('G'))
            else:
                self.emit('choose_mana_source', ref='payer')
        if not fee:
            self.emit('choose_color', player=self.owner, value=COLORS.index(color))
        if scout:
            self.emit('resolve')
        else:
            self.emit('play_land', ref='source', value=1)
        if fee and not pay:
            self.zones['source'] = 'graveyard'
            self.sacrifices += 1
        else:
            self.zones['source'] = 'battlefield'
            self.tapped.add('source')
            if scout:
                self.emit('bind_permanent_copy', ref='source', player=self.owner, index=0, value=self.model['csvid'])
            if fee:
                if floating:
                    self.pool[self.owner][COLORS.index('G')] -= self.model['fee']
                else:
                    self.tapped.add('payer')
            else:
                self.pool[self.owner][COLORS.index(color)] += 1
        self.receipt()
        if self.zones['source'] == 'battlefield':
            self.emit('can_activate', ref='source', value=0)
            self.emit('activate', ref='source', value=0)
            self.receipt()


def _case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    w = EntryCase(model, owner, rows, oracle)
    fee = model['kind'] == 'fee'
    absent = mode in ('slot_taken', 'off_turn')
    paid = fee and not absent and mode not in ('unfunded', 'tapped_provider', 'opposing_provider', 'decline', 'sacrifice_witness')
    color_mode = mode.startswith('color_')
    color = mode[-1].upper() if color_mode else 'G'
    w.add(name, 'source', owner, 'hand')
    if paid or mode in ('tapped_provider', 'opposing_provider', 'decline'):
        giver = 'Forest' if mode in ('pay_forest', 'floating_forest', 'tapped_provider', 'opposing_provider', 'decline') else 'Wastes'
        w.add(giver, 'payer', foe if mode == 'opposing_provider' else owner, tapped=mode == 'tapped_provider')
    if mode in ('slot_taken', 'scout_put', 'next_turn_slot', 'unfunded'):
        w.add('Mountain', 'ordinary', owner, 'hand')
    if mode == 'scout_put':
        w.add('Island', 'extra', owner, 'hand')
        w.add('Sakura-Tribe Scout', 'scout', owner)
    if mode == 'sacrifice_witness':
        w.add('Bloodbriar', 'witness', owner)
    if mode == 'gate_witness':
        w.add('Azorius Guildgate', 'first_gate', owner, tapped=True)
        w.add('Sol Ring', 'generic_mana', owner, tapped=True)
        w.add('Forest', 'green_mana', owner, tapped=True)
        w.add('Saruli Gatekeepers', 'gatekeepers', owner, 'hand')
    if color_mode or (not fee and not absent):
        w.add(SPENDERS[color], 'spender', owner, 'hand')
    if mode == 'later_colorless':
        w.add(SPENDERS['C'], 'colorless_spender', owner, 'hand')
    for player in (0, 1):
        w.add('Island', f'library_{player}', player, 'library')
    w.emit('reset_death_history')
    w.emit('turn', player=foe if mode == 'off_turn' else owner, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.zero_pools()
    replay_from = len(w.actions) if mode == 'ready_replay' else None
    if absent:
        if mode == 'slot_taken':
            w.plain_play('ordinary')
        w.plain_play('source', False)
    else:
        if mode == 'floating_forest':
            w.use_mana('payer', 'G')
        w.source_entry(pay=paid, color=color, floating=mode == 'floating_forest', scout=mode == 'scout_put')
        if not fee:
            w.spend('spender')
        if mode in ('next_turn_slot', 'unfunded'):
            w.plain_play('ordinary', False)
        if mode == 'scout_put':
            w.plain_play('ordinary')
            w.plain_play('extra', False)
        if mode in ('later_colorless', 'gate_witness', 'next_turn_slot') or (fee and color_mode):
            w.next_own_main()
        if mode == 'next_turn_slot':
            w.plain_play('ordinary')
        elif mode == 'later_colorless':
            w.use_mana('source', 'C')
            w.emit('can_activate', ref='source', value=0)
            w.spend('colorless_spender')
        elif fee and color_mode:
            w.use_mana('source', color, choose=True)
            w.emit('can_activate', ref='source', value=0)
            w.spend('spender')
        elif mode == 'gate_witness':
            w.use_mana('generic_mana', 'C', amount=2)
            w.use_mana('payer', 'C')
            w.use_mana('green_mana', 'G')
            w.spend('gatekeepers', life_gain=7 if model['gate'] else 0)
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                    untested_components=['The mandatory color palette exposes five real choices. Invalid host inputs terminate the script.',
                                         'The automatic AI decides whether an available fee is paid.'])
    return dict(name=f'LandEntryPaymentRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


class HubCase(EntryCase):
    def __init__(self, model, owner, rows, oracle):
        super().__init__(model, owner, rows, oracle)
        self.energy = [0, 0]

    def receipt(self):
        super().receipt()
        for player in (0, 1):
            self.emit('assert_energy', player=player, value=self.energy[player])

    def enter_hub(self, scout=False):
        if scout:
            self.emit('require_can_activate', ref='scout', value=1)
            self.emit('activate', ref='scout', value=1)
            self.tapped.add('scout')
            self.emit('assert_stack', value=1)
            self.emit('choose_zone_card', player=self.owner, value=self.model['csvid'])
            self.emit('resolve')
            self.emit('bind_permanent_copy', ref='source', player=self.owner, index=0, value=self.model['csvid'])
        else:
            self.emit('play_land', ref='source', value=1)
        self.zones['source'] = 'battlefield'
        self.energy[self.owner] += self.model['entry_energy']
        self.receipt()

    def tap_hub(self, color, spender):
        has_energy = self.energy[self.owner] >= self.model['energy_fee']
        if color != 'C':
            if not has_energy:
                raise ValueError('colored mana requires an actual energy counter')
            self.energy[self.owner] -= self.model['energy_fee']
        self.use_mana('source', color, choose=has_energy)
        self.emit('can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)
        self.spend(spender)

    def provide_energy(self):
        player = self.cards['donor']['player']
        self.use_mana('ring', 'C', amount=2)
        self.energy[player] += 3
        self.spend('donor', life_gain=3)

    def bounce(self):
        self.use_mana('first_island', 'U')
        self.use_mana('second_island', 'U')
        self.emit('choose_target', player=self.owner, ref='source')
        self.emit('cast', ref='bounce', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.pool[self.owner][COLORS.index('U')] -= 2
        self.zones['bounce'], self.zones['source'] = 'graveyard', 'hand'
        self.emit('bind_hand_copy', ref='source', player=self.owner, index=0, value=self.model['csvid'])
        self.receipt()

    def pay_cast_from_hub(self):
        if self.owner == 0:
            self.emit('choose_mana_source', ref='source')
        # The actual cast charge selects its one required color through Hub.
        self.energy[self.owner] -= self.model['energy_fee']
        self.pool[self.owner][COLORS.index('R')] += 1
        self.tapped.add('source')
        self.spend('spender')


def _hub_case(name, csvid, declaration, model, variant, rows, oracle):
    from recipes import step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    foe = 1-owner
    w = HubCase(model, owner, rows, oracle)
    w.add(name, 'source', owner, 'hand')
    color = mode[-1].upper() if mode in COLOR_MODES else 'R'
    if mode not in ('entry', 'free_colorless', 'off_turn', 'scout_entry'):
        w.add(SPENDERS[color], 'spender', owner, 'hand')
    if mode in ('free_colorless', 'spent_then_colorless', 'opposing_energy'):
        w.add(SPENDERS['C'], 'free_spender', owner, 'hand')
    if mode in ('spent_then_colorless', 'opposing_energy', 'own_reserve'):
        w.add(SPENDERS['W'], 'white_spender', owner, 'hand')
    if mode in ('opposing_energy', 'own_reserve'):
        donor = foe if mode == 'opposing_energy' else owner
        w.add("Woodweaver's Puzzleknot", 'donor', donor, 'hand')
        w.add('Sol Ring', 'ring', donor)
    if mode == 'reenter':
        w.add('Island', 'first_island', owner)
        w.add('Island', 'second_island', owner)
        w.add('Boomerang', 'bounce', owner, 'hand')
    if mode == 'scout_entry':
        w.add('Sakura-Tribe Scout', 'scout', owner)
        w.add('Mountain', 'ordinary', owner, 'hand')
        w.add('Forest', 'extra', owner, 'hand')
    for player in (0, 1):
        w.add('Island', f'library_{player}', player, 'library')
    w.emit('turn', player=foe if mode in ('off_turn', 'opposing_energy') else owner, value=0)
    w.emit('present_ai_dialogs', value=1)
    w.zero_pools()
    w.receipt()
    replay_from = len(w.actions) if mode == 'ready_replay' else None
    if mode == 'off_turn':
        w.plain_play('source', False)
    else:
        if mode in ('opposing_energy', 'own_reserve'):
            w.provide_energy()
            if mode == 'opposing_energy':
                w.emit('advance_turn')
        w.enter_hub(scout=mode == 'scout_entry')
        if mode == 'free_colorless':
            w.tap_hub('C', 'free_spender')
        elif mode == 'colored_cast':
            w.pay_cast_from_hub()
        elif mode == 'reenter':
            w.bounce()
            w.plain_play('source', False)
            w.next_own_main()
            w.enter_hub()
            w.tap_hub('R', 'spender')
        elif mode in ('spent_then_colorless', 'opposing_energy'):
            w.tap_hub('R', 'spender')
            w.next_own_main()
            w.emit('cast', ref='white_spender', value=0)
            w.receipt()
            w.tap_hub('C', 'free_spender')
        elif mode == 'own_reserve':
            w.tap_hub('R', 'spender')
            w.next_own_main()
            w.tap_hub('W', 'white_spender')
        elif mode == 'scout_entry':
            w.plain_play('ordinary')
            w.plain_play('extra', False)
        elif mode in COLOR_MODES or mode == 'ready_replay':
            w.tap_hub(color, 'spender')
    actions = w.actions
    if replay_from is not None:
        tail = actions[replay_from:]
        actions = (actions[:replay_from] + [step('capture')] + tail
                   + [step('record_state'), step('restore'), step('ai_roundtrip'), step('present_ai_dialogs', value=1)]
                   + tail + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                    untested_components=['External mana-ability replacement effects and control change.'])
    return dict(name=f'LandEntryPaymentRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=123, setup=w.setup, steps=actions, _recipe=metadata)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid != model['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(model)):
        raise ValueError('distinct explicit reviewed land entry variants required')
    validate_witnesses(oracle, rows)
    render = _hub_case if model['kind'] == 'energy' else _case
    return [render(name, csvid, declaration, model, variant, rows, oracle) for variant in selected]
