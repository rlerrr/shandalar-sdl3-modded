"""Real Aura grants with sacrifice, draw, and reciprocal damage."""

from copy import deepcopy
from functools import lru_cache
import hashlib
import json
from pathlib import Path

from aura_granted_damage_recipes import DamageAuraScene
from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle, step


FAMILY = 'additional_damage_auras'
PROFILE = Path(__file__).with_name('data') / 'additional_damage_auras_wave8_profiles.json'
CLAUSES = {
    'sacrifice': ('Enchant creature you control\n'
                  'Enchanted creature has "{T}: This creature deals 1 damage to any target."\n'
                  'Sacrifice this Aura: It deals 1 damage to any target.'),
    'draw': ('Enchant creature\n'
             'Enchanted creature has "{T}: This creature deals 1 damage to target player or planeswalker. You draw a card."'),
    'reciprocal': ('Enchant creature\n'
                   'Enchanted creature has "{T}: This creature deals damage equal to its power to target creature. '
                   'That creature deals damage equal to its power to this creature."'),
}
COMMON = ('short', 'wrong_color', 'no_creature', 'foreign_gate', 'replay', 'pending_replay', 'aura_removed_pending')
MODES = {
    'sacrifice': ('player', 'self_player', 'creature', 'walker', 'host_lifelink', 'source_colors', 'lost_control',
                  'sacrifice_ready', 'sacrifice_tapped', 'sacrifice_fresh', 'sacrifice_after_tap', 'sacrifice_walker',
                  'sacrifice_target_leaves', 'repeat'),
    'draw': ('player', 'self_player', 'walker', 'walker_lethal', 'walker_leaves', 'creature_denied',
             'changed_control', 'host_lifelink', 'fresh', 'tapped', 'repeat'),
    'reciprocal': ('normal', 'double_lethal', 'double_lifelink', 'self_target', 'land_denied', 'target_leaves',
                   'power_up', 'power_down', 'zero_power', 'foreign_host', 'fresh', 'tapped'),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Enchantment — Aura' or record.get('keywords') != ['Enchant']):
        raise ValueError('whole original Aura required')
    effect = next((name for name, clause in CLAUSES.items() if record.get('oracle_text') == clause), None)
    if effect is None:
        raise ValueError('complete reviewed granted damage and additional clause required')
    return dict(effect=effect, cast_mana=cost_from_oracle(record))


def variants(model):
    return [f'{mode}_{actor}' for mode in COMMON + MODES[model['effect']] for actor in (0, 1)]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    pin = profiles()['sources'].get(record['name'], {})
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))):
        raise ValueError('pinned damage Aura and distinct typed scenarios required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unused = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented = {entry['csvid'] for entry in unused.values()}
    root = Path(__file__).resolve().parents[4]
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(pin['oracle'], oracle.get(name)) or not exact(pin['registry'], row) or row.get('kind') != 'card'
                or row['csvid'] in unimplemented or row['handler'] in (None, 'NULL', '0', 'card_dummy')
                or oracle[name]['layout'] != 'normal'):
            raise ValueError('implemented Aura or action witness changed: ' + name)
        source = pin.get('compiled_source')
        if source is not None and hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('compiled Aura callback changed: ' + name)


class AdditionalAuraScene(DamageAuraScene):
    def setup_cards(self):
        self.add(self.name, 'source', self.owner, 'hand')
        mode = self.mode
        if mode == 'no_creature':
            self.add('Forest', 'land', self.foe, tapped=True)
            return
        if mode == 'foreign_gate':
            self.add('Serra Angel', 'host', self.foe)
            return

        host = {
            'host_lifelink': 'Child of Night', 'source_colors': 'Child of Night',
            'double_lifelink': 'Child of Night', 'self_target': 'Nessian Asp', 'zero_power': 'Wall of Tanglecord',
        }.get(mode, 'Serra Angel')
        zone = 'hand' if mode in ('fresh', 'sacrifice_fresh') else 'battlefield'
        actor = self.foe if mode == 'foreign_host' else self.owner
        self.add(host, 'host', actor, zone, tapped=mode in ('tapped', 'sacrifice_tapped'))

        if self.effect == 'reciprocal' and mode != 'self_target':
            victim = {'double_lethal': 'Serra Angel', 'double_lifelink': 'Vampire Nighthawk',
                      'zero_power': 'Mogg Fanatic'}.get(mode, 'Shivan Dragon')
            self.add(victim, 'victim', 1-actor)
        if mode in ('creature', 'creature_denied', 'sacrifice_target_leaves'):
            self.add('Mogg Fanatic', 'victim', self.foe)
        if mode in ('walker', 'walker_lethal', 'walker_leaves', 'sacrifice_walker'):
            self.add('Jace Beleren', 'victim', self.foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        if mode == 'walker_lethal':
            self.add('Shock', 'wound', self.owner, 'hand')
        if mode == 'land_denied':
            self.add('Forest', 'land', self.foe, tapped=True)
        if mode == 'source_colors':
            self.add('White Knight', 'victim', self.foe)
        if mode in ('lost_control', 'changed_control'):
            self.add('Bazaar Trader', 'trader', self.owner)
        if mode == 'repeat':
            self.add('Twiddle', 'untap', self.owner, 'hand')
        reply = {'aura_removed_pending': 'Disenchant', 'walker_leaves': "Hero's Downfall",
                 'sacrifice_target_leaves': 'Unsummon', 'target_leaves': 'Unsummon',
                 'power_up': 'Giant Growth', 'power_down': 'Disfigure'}.get(mode)
        if reply:
            self.add(reply, 'answer', self.owner if mode == 'power_up' else self.foe, 'hand')

        if self.effect == 'draw':
            for player, deck in enumerate((('Forest', 'Island', 'Mountain'), ('Plains', 'Mountain', 'Island'))):
                for index, name in enumerate(deck):
                    self.add(name, f'library_{player}_{index}', player, 'library')

    def receipts(self):
        super().receipts()
        if self.effect == 'draw':
            for player in (0, 1):
                remaining = [e for e in self.entities.values() if e['owner'] == player and e['zone'] == 'library']
                for index, e in enumerate(remaining):
                    self.emit('assert_zone_card', player=player, zone='library', index=index,
                              value=self.rows[e['name']]['csvid'])

    def cast_aura(self):
        fee = self.model['cast_mana']
        budget = list(fee)
        mode = self.mode
        if mode == 'short':
            budget[0] -= 1
        if mode == 'wrong_color':
            missing = next(i for i in range(1, 6) if fee[i])
            wrong = next(i for i in range(1, 6) if not fee[i])
            budget[missing] -= 1
            budget[wrong] += 1
        self.fund(self.owner, budget)
        if mode == 'foreign_gate':
            self.emit('can_cast', ref='source', value=int(self.effect != 'sacrifice'))
            self.receipts()
            return False
        allowed = mode not in ('short', 'wrong_color', 'no_creature')
        if allowed:
            self.emit('choose_target', player=self.owner, ref='host')
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.prop('source', 'in_hand', 0)
        self.pay(self.owner, fee)
        self.pools()
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.attachment = True
        self.receipts()
        return True

    def transfer_host(self):
        self.emit('choose_player', player=self.owner, value=self.foe)
        self.emit('choose_target', player=self.owner, ref='host')
        self.emit('activate', ref='trader', value=1)
        self.emit('assert_stack', value=1)
        self.resolve()
        self.entities['trader']['tapped'] = True
        self.entities['host']['controller'] = self.foe
        self.bind('host')
        if self.effect == 'sacrifice':
            self.leave('source', 'graveyard')
        self.receipts()

    def begin_damage(self, target, *, sacrifice=False, available=True):
        if not self.attachment:
            raise ValueError('granted activation needs an actual host')
        self.emit('can_activate', ref='source', value=int(available))
        if not available:
            self.emit('activate', ref='source', value=0)
            self.receipts()
            return
        modal = self.effect == 'sacrifice'
        if modal:
            host = self.entities['host']
            auto = sacrifice and (host['tapped'] or self.mode == 'sacrifice_fresh')
            self.emit('choose_activation_mode', ref='source', player=self.owner, fallback_color=0,
                      mode_label='Sac Fire Whip' if sacrifice else 'Tap enchanted creature', allow_auto_nonmana=auto)
        chooser = self.owner if sacrifice else self.host_actor()
        if self.effect == 'reciprocal' and chooser != self.owner:
            # The native Aura selector may prompt its controller. Match the
            # legal recipient regardless of which seat gets the presentation.
            chooser = -1
        if isinstance(target, int):
            self.emit('choose_player', player=chooser, value=target)
        elif target is not None:
            self.emit('choose_target', player=chooser, ref=target)
        else:
            raise ValueError('legal damage target required')
        self.emit('activate', ref='source', value=1)
        if modal:
            self.emit('require_activation_mode', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        if sacrifice:
            self.leave('source', 'graveyard')
            self.prop('source', 'in_play', 0)
            self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard', index=self.csvid, value=1)
        else:
            self.entities['host']['tapped'] = True
            self.prop('source', 'tapped', 0)
        self.prop('host', 'tapped', self.entities['host']['tapped'])
        for player, life in enumerate(self.life):
            self.emit('assert_life', player=player, value=life)
        if isinstance(target, str):
            if self.loyalty is not None:
                self.emit('assert_counter', ref=target, counter_kind='loyalty', value=self.loyalty)
            else:
                self.prop(target, 'damage', self.entities[target]['damage'])

    def answer_pending(self):
        mode = self.mode
        if 'answer' not in self.entities:
            return
        target = 'source' if mode == 'aura_removed_pending' else 'host' if mode == 'power_up' else 'victim'
        # The existing bridge allows the ordinary instant cast above an
        # activation; the activation is the real parent for stack resolution.
        self.begin_spell('answer', target=target, depth=2)
        self.leave('answer', 'graveyard')
        if mode in ('power_up', 'power_down'):
            self.entities[target]['bonus'] = (3, 3) if mode == 'power_up' else (-2, -2)
        else:
            self.leave(target, 'hand' if mode in ('target_leaves', 'sacrifice_target_leaves') else 'graveyard')

    def has_keyword(self, ref, keyword):
        return keyword in self.oracle[self.entities[ref]['name']]['keywords']

    def strike(self, source, target, value):
        if value <= 0:
            return
        if isinstance(target, int):
            self.life[target] -= value
        elif self.loyalty is not None:
            self.loyalty -= value
            if self.loyalty <= 0:
                self.leave(target, 'graveyard')
        else:
            self.entities[target]['damage'] += value
        if self.has_keyword(source, 'Lifelink'):
            self.life[self.entities[source]['controller']] += value

    def settle_creatures(self, packets):
        lethal = set()
        for source, target, value in packets:
            self.strike(source, target, value)
            if value > 0 and (self.entities[target]['damage'] >= self.stats(target)[1]
                              or self.has_keyword(source, 'Deathtouch')):
                lethal.add(target)
        for ref in sorted(lethal):
            self.leave(ref, 'graveyard')
        if 'host' in lethal and self.entities['source']['zone'] == 'battlefield':
            self.leave('source', 'graveyard')

    def finish_damage(self, target, actor, *, sacrifice=False):
        self.resolve()
        if isinstance(target, str) and self.entities[target]['zone'] != 'battlefield':
            self.receipts()
            return
        source = 'source' if sacrifice else 'host'
        if self.effect == 'reciprocal':
            outgoing = max(0, self.stats('host')[0])
            returning = max(0, self.stats(target)[0])
            self.settle_creatures([('host', target, outgoing), (target, 'host', returning)])
        elif isinstance(target, int) or self.loyalty is not None:
            self.strike(source, target, 1)
        else:
            self.settle_creatures([(source, target, 1)])
        if self.effect == 'draw':
            top = next(ref for ref, e in self.entities.items() if e['owner'] == actor and e['zone'] == 'library')
            self.leave(top, 'hand')
        self.receipts()

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        mode = self.mode
        if mode in ('fresh', 'sacrifice_fresh'):
            self.begin_spell('host')
            self.resolve()
            self.entities['host']['zone'] = 'battlefield'
        if not self.cast_aura():
            return
        if mode in ('lost_control', 'changed_control'):
            self.transfer_host()
            if mode == 'lost_control':
                return
            self.advance()
            self.emit('refresh')
        if mode in ('fresh', 'tapped'):
            self.begin_damage(None, available=False)
            return
        if mode == 'walker_lethal':
            self.begin_spell('wound', target='victim')
            self.resolve()
            self.leave('wound', 'graveyard')
            self.loyalty -= 2
            self.receipts()
        if mode in ('creature_denied', 'land_denied', 'source_colors'):
            probe = 'land' if mode == 'land_denied' else 'victim'
            self.emit('probe_activation_target', ref='source', other=probe, index=0 if mode == 'source_colors' else -1, value=0)
        actor = self.host_actor()
        target = 1-actor
        if self.effect == 'reciprocal':
            target = 'host' if mode == 'self_target' else 'victim'
        elif mode == 'self_player':
            target = actor
        elif mode in ('creature', 'walker', 'walker_lethal', 'walker_leaves', 'sacrifice_walker',
                      'sacrifice_target_leaves', 'source_colors'):
            target = 'victim'
        sacrifice = mode == 'source_colors' or (mode.startswith('sacrifice_') and mode != 'sacrifice_after_tap')
        if mode == 'replay':
            self.replay = len(self.steps)
        self.begin_damage(target, sacrifice=sacrifice)
        if mode == 'pending_replay':
            self.replay = len(self.steps)
        self.answer_pending()
        self.finish_damage(target, actor, sacrifice=sacrifice)

        if mode == 'repeat':
            if self.effect != 'sacrifice':
                self.begin_damage(None, available=False)
            self.begin_spell('untap', target='host')
            self.emit('choose_dialog', player=self.owner, value=1)
            self.resolve()
            self.leave('untap', 'graveyard')
            self.entities['host']['tapped'] = False
            self.receipts()
            self.begin_damage(target)
            self.finish_damage(target, actor)
        if mode == 'sacrifice_after_tap':
            self.begin_damage(target, sacrifice=True)
            self.finish_damage(target, actor, sacrifice=True)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if (not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))
            or csvid != profiles()['sources'][name]['registry']['csvid']):
        raise ValueError('unreserved damage Aura or unreviewed scenario')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        scene = AdditionalAuraScene(name, model, mode, int(actor), rows, oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            tail = steps[cut:]
            steps = steps[:cut] + [step('capture')] + tail + [
                step('record_state'), step('ai_roundtrip'), step('restore')] + deepcopy(tail) + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=['Independent opponent UI authority on the grant and multiplayer.'])
        cases.append(dict(name=f'AdditionalDamageAurasRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=steps, _recipe=metadata))
    return cases
