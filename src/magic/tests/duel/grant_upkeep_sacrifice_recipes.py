"""Whole-Oracle global upkeep grants exercised through real untap and upkeep."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from global_attack_tax_recipes import Board as Ledger


FAMILY = 'grant_upkeep_sacrifice'
PROFILE = Path(__file__).parent / 'data/grant_upkeep_sacrifice_wave8_profiles.json'
CLAUSE = re.compile(
    r'(?P<scope>All|Other) (?P<kind>artifact|creature|enchantment)s have '
    r'"At the beginning of your upkeep, (?P<action>sacrifice|destroy) this '
    r'(?P=kind) unless you pay \{(?P<fee>[1-2])\}\."'
)
SUBJECTS = {'artifact': 'Darksteel Relic', 'creature': 'Grizzly Bears', 'enchantment': 'Fervor'}
DUAL_TYPES = {'artifact': 'Darksteel Myr', 'creature': 'Darksteel Myr', 'enchantment': 'Nyxborn Shieldmate'}
EXCLUDED_TYPES = {'artifact': 'Fervor', 'creature': 'Fervor', 'enchantment': 'Darksteel Relic'}
REMOVAL = {'Creature': 'Murder', 'Enchantment': 'Naturalize', 'Land': 'Stone Rain'}
WITNESSES = set(SUBJECTS.values()) | set(DUAL_TYPES.values()) | set(REMOVAL.values()) | {'Wastes', 'Forest'}
MODES = (
    'paid_other', 'short_other', 'decline_other', 'paid_own', 'short_own', 'scope',
    'source_in_hand', 'source_removed', 'source_enters', 'two_short', 'two_paid',
    'dual_type', 'opposing_funds', 'colored_mana', 'repeat_pay',
)
OWN_UPKEEP = {'paid_own', 'short_own', 'source_removed', 'repeat_pay', 'self_excluded'}
PAID = {'paid_other', 'paid_own', 'two_paid', 'colored_mana', 'repeat_pay'}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def types(record):
    return set(record['type_line'].split(' — ', 1)[0].split())


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or 'card_faces' in record or 'selected_face' in record
            or record['keywords'] or len(types(record) & set(REMOVAL)) != 1):
        raise ValueError('normal whole permanent with the single upkeep grant required')
    match = CLAUSE.fullmatch(record['oracle_text'])
    if not match:
        raise ValueError('complete per-permanent upkeep payment or removal clause required')
    source_type = (types(record) & set(REMOVAL)).pop()
    if source_type == 'Land':
        if record['mana_cost']:
            raise ValueError('ordinary land play required')
    elif not re.fullmatch(r'(?:\{[1-4]\})?\{[WUBRG]\}', record['mana_cost']):
        raise ValueError('supported fixed spell payment required')
    if source_type == 'Creature' and not all(re.fullmatch(r'[0-9]+', record.get(part, ''))
                                             for part in ('power', 'toughness')):
        raise ValueError('numeric source stats required')
    cost = [0] * 6 if source_type == 'Land' else cost_from_oracle(record)
    return dict(cost=cost, source_type=source_type, subject=match['kind'],
                other_only=match['scope'] == 'Other', fee=int(match['fee']), unpaid=match['action'])


def has_automatic_payer(record):
    # Energy Flux uses a separate payment selection for seat 1.
    return record['name'] == 'Energy Flux'


def variants(params):
    modes = MODES + (('self_excluded',) if params['other_only'] else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes
            if not (params.get('automatic_payer') and owner == 0 and mode in ('decline_other', 'two_paid'))]


def spec(declaration, record):
    model = classify(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(pins()['sources'].get(record['name']), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model)):
        raise ValueError('exact whole Oracle upkeep grant required')
    execution = dict(model, family=FAMILY, automatic_payer=has_automatic_payer(record))
    if (not isinstance(selected, list) or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(execution))):
        raise ValueError('distinct reviewed upkeep grant variants required')
    return execution


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(records) != set(data['registry']):
        raise ValueError('complete upkeep grant source and witness pins required')
    for name, record in records.items():
        if (not exact(oracle.get(name), record) or not exact(rows.get(name), data['registry'][name])
                or record.get('layout') != 'normal' or rows[name]['kind'] != 'card'):
            raise ValueError(f'Oracle or registry identity changed for {name}')
        if name != 'Grizzly Bears' and rows[name]['handler'] in (None, 'card_dummy', '0', 'NULL'):
            raise ValueError(f'production callback missing for {name}')


class GrantBoard(Ledger):
    """Reuse the public ledger for card movements, real casts and zone receipts."""

    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner = int(seat)
        self.payer = self.owner if self.mode in OWN_UPKEEP else 1 - self.owner
        self.name, self.csvid, self.variant = name, csvid, variant
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life, self.loyalty = [[0] * 6, [0] * 6], [20, 20], None
        self.producers = []
        self.add(name, 'source', self.owner,
                 'hand' if self.mode in ('source_in_hand', 'source_enters') else 'battlefield')
        self.setup_subjects()
        self.setup_funding()
        for player in (0, 1):
            self.add('Forest', f'seed_grave_{player}', player, 'graveyard')
            self.add('Forest', f'seed_exile_{player}', player, 'exile')
            self.add('Forest', f'seed_library_{player}', player, 'library')

    def grants_to(self, ref, player):
        entity = self.entities[ref]
        return (entity['zone'] == 'battlefield' and entity['player'] == player
                and not (ref == 'source' and self.params['other_only'])
                and self.params['subject'].title() in types(self.oracle[entity['name']]))

    def setup_subjects(self):
        mode, subject = self.mode, self.params['subject']
        witness = SUBJECTS[subject]
        if self.params['automatic_payer'] and self.payer == 1 and mode in PAID:
            # Energy Flux's AI picks only positive-value artifacts. The Relic
            # has zero base value; Darksteel Myr has positive evaluation from its toughness and mana value.
            witness = DUAL_TYPES[subject]
        # Magus is its own sole eligible subject in own-uptkeep cases; a separate
        # witness remains necessary when the grant is removed before upkeep.
        if mode != 'self_excluded' and (mode == 'source_removed' or not self.grants_to('source', self.payer)):
            self.add(DUAL_TYPES[subject] if mode == 'dual_type' else witness, 'subject', self.payer)
        if mode in ('two_short', 'two_paid'):
            self.add(DUAL_TYPES[subject], 'second_subject', self.payer)
        if mode == 'scope':
            self.add(witness, 'inactive_subject', 1 - self.payer)
            self.add(EXCLUDED_TYPES[subject], 'excluded_type', self.payer)
        if mode == 'source_removed':
            self.add(REMOVAL[self.params['source_type']], 'removal', 1 - self.payer, 'hand')

    def setup_funding(self):
        mode, fee = self.mode, self.params['fee']
        if mode in PAID | {'decline_other'}:
            amount = fee * (2 if mode == 'two_paid' else 1)
        elif mode in ('short_other', 'short_own'):
            amount = fee - 1
        else:
            amount = 0
        for i in range(amount):
            ref = f'producer_{i}'
            self.add('Forest' if mode == 'colored_mana' else 'Wastes', ref, self.payer)
            self.producers.append(ref)
        if mode == 'opposing_funds':
            for i in range(fee):
                self.add('Wastes', f'opposing_producer_{i}', 1 - self.payer)

    def upkeep(self, active):
        assert not any(map(any, self.mana))
        self.emit('advance_turn')
        self.emit('untap_step', player=active)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == active:
                entity['tapped'] = False
        self.receipt()
        if self.entities['source']['zone'] == 'battlefield':
            self.granted_choices(active)
        self.emit('upkeep_step', player=active)
        self.emit('assert_unused_choices', value=0)
        self.receipt()
        self.emit('turn', player=active, value=0)

    def granted_choices(self, active):
        options = [ref for ref in self.entities if self.grants_to(ref, active)]
        fee = self.params['fee']
        available = [ref for ref in self.producers
                     if self.entities[ref]['player'] == active and not self.entities[ref]['tapped']]
        auto = active == 1 and self.params['automatic_payer']
        # Multi-subject paid cases make every payment, so their identical
        # funds and choices do not assume a particular order of obligations.
        for ref in options:
            offered = len(available) >= fee
            pay = offered and self.mode != 'decline_other'
            if offered and not auto:
                self.emit('choose_dialog', player=active, value=int(not pay))
            if pay:
                for _ in range(fee):
                    producer = available.pop(0)
                    if active == 0:
                        self.emit('choose_mana_source', ref=producer)
                    self.entities[producer]['tapped'] = True
            elif (self.params['unpaid'] == 'sacrifice'
                  or 'Indestructible' not in self.oracle[self.entities[ref]['name']]['keywords']):
                self.move(ref, 'graveyard')

    def case(self):
        from recipes import step

        self.emit('capture')
        self.emit('turn', player=1 - self.payer, value=0)
        # empty_main starts with ten floating mana of each color for both seats.
        self.fund(1 - self.payer, [0] * 6)
        self.receipt()
        if self.mode == 'source_removed':
            self.cast('removal', target='source')
        elif self.mode == 'source_enters':
            if self.params['source_type'] == 'Land':
                self.emit('play_land', ref='source', value=1)
                self.move('source', 'battlefield')
                self.emit('refresh')
                self.receipt()
            else:
                self.cast('source')
        self.upkeep(self.payer)
        if self.mode == 'repeat_pay':
            self.upkeep(1 - self.payer)
            self.upkeep(self.payer)
        run = self.steps + [step('record_state'), step('restore')] + self.steps[1:] + [step('assert_recorded_state')]
        return dict(name=f'GrantUpkeepSacrificeRecipe_{self.csvid}_{self.variant}', card=self.name,
                    fixture='empty_main', seed=123, setup=self.setup, steps=run,
                    _recipe=dict(family=FAMILY, version=2, variant=self.variant, params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'], untested_components=[
                                     'Multiple grant sources and responses between separate upkeep triggers need focused timing calibration.',
                                     'Energy Flux seat 1 uses the production automatic payment with one eligible artifact.',
                                 ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [GrantBoard(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
