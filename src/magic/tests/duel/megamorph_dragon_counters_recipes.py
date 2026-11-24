"""Real Megamorph, independent Dragon counter receipts, and native keywords."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN
from morph_turn_up_target_recipes import HIDDEN_FEE
from morph_turn_up_team_recipes import TeamScene


FAMILY = 'megamorph_dragon_counters'
PROFILE = Path(__file__).parent / 'data/megamorph_dragon_counters_wave8_profiles.json'
MEGAMORPH = re.compile(r'Megamorph (\{5\}(\{[BUGRW]\})\2) \(You may cast this card face down as a 2/2 creature for \{3\}\. Turn it face up any time for its megamorph cost and put a \+1/\+1 counter on it\.\)')
OTHER_DRAGONS = 'When this creature is turned face up, put a +1/+1 counter on each other Dragon creature you control.'
NATIVE_KEYWORDS = {
    'Flying, deathtouch': ('Deathtouch', 'deathtouch'),
    'Flying, hexproof': ('Hexproof', 'hexproof'),
    'Flying, trample': ('Trample', 'trample'),
    'Flying, lifelink': ('Lifelink', 'lifelink'),
    'Flying, first strike': ('First strike', 'first_strike'),
}
COMMON = ('hidden', 'faceup', 'hidden_choice', 'hidden_short', 'reveal', 'flip_short', 'flip_wrong',
          'offturn', 'no_ally', 'hidden_dragon', 'donated', 'acquired', 'late_dragon', 'recast', 'replay',
          'flying', 'ground_hidden')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or
            'card_faces' in record or record.get('type_line') != 'Creature — Dragon'):
        raise ValueError('whole normal Dragon creature required')
    lines = record['oracle_text'].splitlines()
    if len(lines) != 3 or lines[0] not in NATIVE_KEYWORDS or lines[2] != OTHER_DRAGONS:
        raise ValueError('complete supported printed Megamorph Dragon clauses required')
    morph = MEGAMORPH.fullmatch(lines[1])
    keyword, mechanism = NATIVE_KEYWORDS[lines[0]]
    if not morph or set(record['keywords']) != {'Flying', keyword, 'Megamorph'}:
        raise ValueError('independent Megamorph cost and printed passive required')
    return dict(effect='dragon_counters', keyword=mechanism, cast_mana=cost_from_oracle(record),
                flip_mana=cost_from_oracle(dict(record, mana_cost=morph[1])),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(model):
    keyword = model['keyword']
    contrary = 'hexproof_pending' if keyword == 'hexproof' else keyword + '_hidden'
    return [f'{mode}_{actor}' for actor in (0,1) for mode in COMMON + (keyword, contrary)]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    pin = pins()['sources'].get(record['name'], {})
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY or
            type(declaration.get('version')) is not int or declaration['version'] != 2 or
            declaration.get('oracle_text') != record['oracle_text'] or
            not exact(declaration.get('params'), model) or type(selected) is not list or not selected or
            any(type(v) is not str for v in selected) or len(set(selected)) != len(selected) or
            set(selected) - set(variants(model))):
        raise ValueError('exact indexed Megamorph Dragon declaration and typed variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if not exact(data['hidden_observer'], rows.get('Face-Down Creature')) or data['hidden_observer']['csvid'] != FACE_DOWN:
        raise ValueError('live face-down characteristics observer changed')
    for group in ('sources', 'witnesses'):
        for name,pin in data[group].items():
            row = rows.get(name, {})
            if (not exact(pin, dict(oracle=oracle.get(name), registry=row)) or row.get('kind') != 'card' or
                    row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError(f'implemented original Dragon or witness changed: {name}')


class DragonScene(TeamScene):
    def __init__(self, name, model, mode, owner, rows, oracle):
        super().__init__(name, model, mode, owner, rows, oracle)
        self.counters = {}

    def add(self, name, ref, owner, zone='battlefield', tapped=False):
        super().add(name, ref, owner, zone, tapped)
        self.counters[ref] = 0

    def stats(self, ref):
        return tuple(amount + self.counters[ref] for amount in super().stats(ref))

    def leave(self, ref, zone):
        super().leave(ref, zone)
        self.counters[ref] = 0

    def counter_receipt(self, ref):
        self.emit('assert_counter', ref=ref, counter_kind='plus1', value=self.counters[ref])

    def receipts(self):
        super().receipts()
        for ref in self.active_creatures():
            self.counter_receipt(ref)
            controller = self.entities[ref]['controller']
            self.emit('assert_matching_permanents', player=controller, index=self.observed_id(ref),
                      property='subtype_Dragon', value=int(self.creature_type(ref, 'Dragon')))

    def setup_cards(self):
        self.add(self.name, 'source', self.owner, 'hand')
        mode = self.mode
        if mode != 'no_ally':
            self.add('Shivan Dragon', 'own_dragon', self.owner)
        if mode in ('reveal', 'offturn', 'recast', 'replay', 'late_dragon'):
            self.add('Dragon Whelp', 'other_dragon', self.owner, 'hand' if mode=='late_dragon' else 'battlefield')
        combat_dragon = 'Shivan Dragon' if mode in ('deathtouch', 'deathtouch_hidden') else 'Dragon Whelp'
        self.add(combat_dragon, 'foe_dragon', self.foe)
        self.add('Goblin Tinkerer', 'non_dragon', self.owner)
        if mode in ('flying', 'ground_hidden'):
            self.add('Goblin Tinkerer', 'ground', self.foe)
        if mode in ('first_strike', 'first_strike_hidden'):
            self.add('Serra Angel', 'flying_blocker', self.foe)
        if mode == 'hidden_dragon':
            self.add('Imperial Hellkite', 'secondary', self.owner, 'hand')
        if mode in ('donated', 'acquired'):
            actor = self.owner if mode=='donated' else self.foe
            self.add('Bazaar Trader', 'trader', actor)
        if mode in ('hexproof', 'hexproof_pending'):
            self.add('Unsummon', 'opposing_spell', self.foe, 'hand')
        if mode in ('recast', 'hexproof'):
            self.add('Unsummon', 'return_spell', self.owner, 'hand')

    def transfer(self):
        own_gift = self.mode == 'donated'
        actor = self.owner if own_gift else self.foe
        ref = 'own_dragon' if own_gift else 'foe_dragon'
        self.emit('require_can_activate', ref='trader', value=1)
        self.emit('choose_player', player=actor, value=1-actor)
        self.emit('choose_target', player=actor, ref=ref)
        self.emit('activate', ref='trader', value=1)
        self.emit('assert_stack', value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        self.entities[ref]['controller'] = 1-actor
        self.bind(ref)
        self.receipts()

    def flip_dragon(self, pending=False):
        fee = self.model['flip_mana']
        denied = self.mode in ('flip_short', 'flip_wrong')
        pool = list(fee)
        if denied:
            color = next(i for i,n in enumerate(fee) if i and n)
            pool[color] -= 1
            if self.mode == 'flip_wrong':
                pool[3 if color!=3 else 2] += 1
        self.fund(self.owner, pool)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate', ref='source', value=int(not denied))
        self.emit('activate', ref='source', value=int(not denied))
        self.emit('assert_choices_consumed')
        if denied:
            self.receipts()
            return
        self.pay(self.owner, fee)
        self.hidden = False
        self.counters['source'] += 1
        for ref in self.active_creatures():
            if ref != 'source' and self.entities[ref]['controller'] == self.owner and self.creature_type(ref, 'Dragon'):
                self.counters[ref] += 1
        self.emit('refresh')
        self.source_traits()
        self.pools()
        self.counter_receipt('source')
        # First inspect the actual paid reveal. Drain the synchronous native
        # turn-up proxy, and the real pending instant when present.
        self.resolve(parent=pending)
        if pending:
            self.leave('opposing_spell', 'graveyard')
        self.receipts()

    def bounce(self):
        self.cast_spell('return_spell', 'source')
        self.resolve()
        self.leave('return_spell', 'graveyard')
        self.leave('source', 'hand')
        self.receipts()

    def target_check(self, revealed):
        self.emit('can_target_creature', ref='opposing_spell', other='source', value=int(not revealed))
        self.emit('can_target_creature', ref='return_spell', other='source', value=1)

    def keyword_combat(self):
        mode = self.mode
        self.advance()
        self.advance()
        hidden = mode.endswith('_hidden')
        if not hidden:
            self.flip_dragon()
        blocker = ('ground' if mode in ('flying', 'ground_hidden') else
                   'foe_dragon' if mode.removesuffix('_hidden') in ('deathtouch', 'trample') else
                   'flying_blocker' if mode.removesuffix('_hidden') == 'first_strike' else None)
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', ref=blocker, other='source', value=int(mode!='flying'))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power = self.stats('source')[0]
        if blocker is None or mode=='flying':
            self.life[self.foe] -= power
            if not self.hidden and self.model['keyword']=='lifelink':
                self.life[self.owner] += power
        else:
            block_power,block_toughness = self.stats(blocker)
            spill = max(0, power-block_toughness) if mode=='trample' else 0
            self.entities[blocker]['damage'] += power-spill
            self.life[self.foe] -= spill
            if mode=='deathtouch':
                self.leave(blocker, 'graveyard')
            if mode!='first_strike' or power<block_toughness:
                self.entities['source']['damage'] += block_power
        self.reap(damage=True)
        self.receipts()

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        if not self.cast_source() or self.mode in ('hidden', 'faceup', 'hidden_choice'):
            return
        mode = self.mode
        if mode in ('flying', 'ground_hidden', 'deathtouch', 'deathtouch_hidden', 'trample', 'trample_hidden',
                    'lifelink', 'lifelink_hidden', 'first_strike', 'first_strike_hidden'):
            self.keyword_combat()
            return
        if mode in ('offturn', 'hexproof_pending'):
            self.advance()
        if mode in ('donated', 'acquired'):
            self.transfer()
        if mode=='hidden_dragon':
            self.cast_secondary()
        if mode=='hexproof':
            self.target_check(revealed=False)
        if mode=='hexproof_pending':
            self.cast_spell('opposing_spell', 'source')
        self.flip_dragon(pending=mode=='hexproof_pending')
        if mode=='late_dragon':
            self.cast_spell('other_dragon', None)
            self.resolve()
            self.entities['other_dragon']['zone'] = 'battlefield'
            self.receipts()
        if mode=='hexproof':
            self.target_check(revealed=True)
            self.bounce()
        if mode=='recast':
            self.bounce()
            self.emit('bind_hand_copy', ref='source', player=self.owner, index=0, value=self.csvid)
            self.cast_source()
            self.flip_dragon()
        if mode not in ('flip_short', 'flip_wrong'):
            self.cleanup()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if csvid != pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved Megamorph Dragon source')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_', 1)
        scene = DragonScene(name, model, mode, int(actor), rows, oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [
                step('record_state'), step('ai_roundtrip'), step('restore')] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=['Independent resolution priority inside the native turn-up callback', 'Multiplayer'])
        cases.append(dict(name=f'MegamorphDragonCounterRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=steps, _recipe=metadata))
    return cases
