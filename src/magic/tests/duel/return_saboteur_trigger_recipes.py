"""Whole-Oracle saboteurs with real battlefield/graveyard return destinations.

The expected zone ledger is independent of the card handlers.  Handlers select
only presentation profiles: ordinary target/zone menus, the RulesEngine hand
menu, and printed optional payment menus.  All movement uses real game actions.
"""
import copy
import json
import re
from pathlib import Path

from combat_value_trigger_recipes import _AdjacentBoard, _mirror
from grant_recipes import LIBRARY
from graveyard_recovery_extension_recipes import eligible as recovery_eligible

FAMILY = 'return_saboteur_trigger'
PROFILE_PATH = Path(__file__).parent / 'data/return_saboteur_trigger_wave8_profiles.json'


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line', '')
            or any(not re.fullmatch(r'[1-8]', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('return saboteur requires a fixed normal creature')
    aliases = {'this creature', record['name'], record['name'].split(',')[0]}
    name = '(?:' + '|'.join(re.escape(n) for n in sorted(aliases)) + ')'
    text = record['oracle_text']; keywords = []; cycling = None; ninjutsu = None; regeneration = None
    lines = text.splitlines(); effect_lines = []
    for line in lines:
        if line in ('Flying', 'Trample', 'Flying, trample'):
            keywords += [v.strip().capitalize() for v in line.split(',')]; continue
        m = re.fullmatch(r'Cycling ((?:\{[0-9BUGRW]+\})+) \(\1, Discard this card: Draw a card\.\)', line)
        if m:
            if cycling: raise ValueError('duplicate cycling')
            cycling = m[1]; keywords.append('Cycling'); continue
        m = re.fullmatch(r'Ninjutsu ((?:\{[0-9BUGRW]+\})+) \(\1, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking\.\)', line)
        if m:
            if ninjutsu: raise ValueError('duplicate ninjutsu')
            ninjutsu = m[1]; keywords.append('Ninjutsu'); continue
        m = re.fullmatch(r'((?:\{[0-9BUGRW]+\})+): Regenerate ' + name + r'\.', line)
        if m:
            if regeneration: raise ValueError('duplicate regeneration')
            regeneration = m[1]; keywords.append('Regenerate'); continue
        effect_lines.append(line)
    body = '\n'.join(effect_lines)
    trigger = re.fullmatch(r'Whenever ' + name + r' deals (combat )?damage to (a player|an opponent), (.+)', body, re.S)
    if not trigger: raise ValueError('one complete printed return damage trigger required')
    clause = trigger[3]; cost = None; scope = None; optional = False
    if clause == "return up to that many target permanents that player controls to their owner's hand.":
        effect = 'bounce_damage'; scope = 'permanent'; optional = True
    elif clause == "choose one —\n• That player returns a permanent they control to its owner's hand.\n• That player discards a card.":
        effect = 'modal_bounce_discard'; scope = 'permanent'
    else:
        m = re.fullmatch(r"you may pay ((?:\{[0-9BUGRW]+\})+)\. If you do, choose a color, then return all creatures of that color to their owners' hands\.", clause)
        hand = re.fullmatch(r'you may return target (card|Arcane card) from your graveyard to your hand\.', clause)
        play = re.fullmatch(r"you may (?:(?:pay ((?:\{[0-9BUGRW]+\})+)\. If you do, )?put target creature card from (that player's graveyard|a graveyard) onto the battlefield under your control)\.", clause)
        bounce = re.fullmatch(r"you may return target creature that player controls to its owner's hand\.", clause)
        if m: effect = 'color_bounce'; cost = m[1]; scope = 'creature'; optional = True
        elif hand: effect = 'grave_hand'; scope = 'arcane' if hand[1] == 'Arcane card' else 'card'; optional = True
        elif play: effect = 'grave_play'; scope = 'any_grave' if play[2] == 'a graveyard' else 'damaged_grave'; cost = play[1]; optional = True
        elif bounce: effect = 'bounce_one'; scope = 'creature'; optional = True
        else: raise ValueError('unreviewed return destination or selector')
    if len(keywords) != len(set(keywords)) or set(keywords) != set(record.get('keywords', [])):
        raise ValueError('complete return source text and keyword metadata disagree')
    return dict(effect=effect, selector=scope, scope='combat' if trigger[1] else 'any',
                recipient='player' if trigger[2] == 'a player' else 'opponent', optional=optional,
                trigger_cost=cost, cycling=cycling, ninjutsu=ninjutsu, regeneration=regeneration,
                keywords=sorted(keywords))


def spec(declaration, oracle):
    from recipes import cost_from_oracle
    p = parameters(oracle); profile = profiles()['sources'].get(oracle['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != oracle['oracle_text'] or not profile
            or profile['oracle'] != oracle or profile['params'] != p
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('complete pinned return source and exact typed parameters required')
    p.update(family=FAMILY, mana=cost_from_oracle(oracle), power=int(oracle['power']), toughness=int(oracle['toughness']),
             legendary='Legendary' in oracle['type_line'], input_profile=profile['input_profile'])
    chosen = declaration.get('variants')
    if not isinstance(chosen, list) or not chosen or len(chosen) != len(set(chosen)) or any(v not in variants(p) for v in chosen):
        raise ValueError('explicit distinct return variants required')
    return p


def variants(p):
    modes = ['unblocked', 'cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack', 'blocked',
             'pump', 'cleanup', 'two_hits', 'ready_snapshot', 'combat_snapshot', 'noncombat_player',
             'noncombat_self', 'source_bounce']
    if p['effect'] in ('grave_hand', 'grave_play'):
        modes += ['empty_grave', 'other_grave', 'declined']
        if p['selector'] != 'card': modes += ['wrong_type']
        if p['effect'] == 'grave_hand' and p['selector'] == 'card': modes += ['land_return', 'artifact_return']
        if p['effect'] == 'grave_play': modes += ['reanimated_bounce']
        if p['selector'] == 'any_grave': modes += ['both_graves']
    if p['effect'] == 'bounce_damage': modes += ['zero_returns', 'one_return', 'partial_returns', 'land_return', 'artifact_return']
    if p['effect'] == 'modal_bounce_discard': modes += ['discard_mode', 'empty_hand', 'shroud_return']
    if p['effect'] == 'color_bounce': modes += ['declined', 'trigger_payment', 'trigger_insufficient', 'trigger_wrong_color', 'self_color', 'multicolor']
    if p['effect'] == 'grave_play' and p['trigger_cost']: modes += ['trigger_payment', 'trigger_insufficient', 'trigger_wrong_color']
    if p['cycling']: modes += ['cycling', 'cycling_payment', 'cycling_insufficient', 'cycling_pending_snapshot']
    if p['ninjutsu']: modes += ['ninjutsu', 'ninjutsu_payment', 'ninjutsu_insufficient', 'ninjutsu_before_blocks', 'ninjutsu_blocked', 'ninjutsu_pending_snapshot']
    if p['regeneration']: modes += ['regenerate', 'regenerate_insufficient', 'regenerate_cannot', 'ninjutsu_regenerate']
    # The AI auto-payment path has no payment cancellation UI. Keep that
    # particular voluntary-decline observation in the private pending package;
    # an unfunded/shortage case would not be an honest substitute.
    return [v + '_' + str(owner) for owner in (0, 1) for v in modes
            if not (v == 'declined' and owner == 1 and p['effect'] == 'grave_play' and p['trigger_cost'])]


def validate_witnesses(oracle, registry):
    data = profiles()
    for name, pin in data['witnesses'].items():
        row = registry.get(name, {})
        passive = name in data['native_characteristics_only_witnesses'] and row.get('handler') == 'card_dummy'
        if oracle.get(name) != pin or row.get('kind') != 'card' or (not passive and row.get('handler') in (None, 'NULL', 'card_dummy', 'manalinkeh_card_dummy')):
            raise ValueError('return witness Oracle/implementation changed: ' + name)
    for name, profile in data['sources'].items():
        row = registry.get(name, {})
        if oracle.get(name) != profile['oracle'] or row.get('kind') != 'card' or row.get('handler') != profile['handler']:
            raise ValueError('return source implementation changed: ' + name)


class ReturnBoard(_AdjacentBoard):
    """Real action emitter with separate original-owner/current-controller fields."""
    def __init__(self, name, csvid, declaration, p, variant, rows, oracle):
        from recipes import card, step, observe, cost_from_oracle
        self.card = card; self.step = step; self.observe = observe; self.cost = cost_from_oracle
        self.name = name; self.csvid = csvid; self.d = declaration; self.p = p; self.rows = rows; self.oracle = oracle
        self.mode, owner = variant.rsplit('_', 1); self.owner = int(owner); self.requested = variant
        self.e = dict(draw=0, discard=0, mill=False, self_counters=0); self.keys = set(p['keywords'])
        self.steps = []; self.setup = []; self.entities = {}; self.tokens = []; self.bound = set()
        self.zones = [{z: [] for z in ('hand', 'graveyard', 'exile', 'library')} for _ in (0, 1)]
        self.life = [20, 20]; self.mana = [60, 60]; self.pools = [[10]*6, [10]*6]
        self.boundary = self.mode.startswith('noncombat_'); self.optional = False; self.discard_nondeterministic = False
        for player in (0, 1):
            self.zones[player].update(hand=['Forest', 'Plains'], graveyard=['Forest'], exile=['Mountain'], library=list(LIBRARY))
        effect = p['effect']; mode = self.mode
        self.grave_player = 0 if effect == 'grave_hand' or mode in ('other_grave', 'both_graves') else 1
        self.return_card = 'Reach Through Mists' if effect == 'grave_hand' else 'Elvish Visionary'
        if mode == 'land_return' and effect == 'grave_hand': self.return_card = 'Forest'
        if mode == 'artifact_return' and effect == 'grave_hand': self.return_card = 'Ornithopter'
        if effect in ('grave_hand', 'grave_play'):
            if mode == 'other_grave': self.grave_player = 1 if effect == 'grave_hand' else 0
            if mode == 'other_grave' and effect == 'grave_hand' and p['selector'] == 'card': self.zones[0]['graveyard'] = []
            eligible = mode not in ('empty_grave', 'wrong_type')
            if mode == 'empty_grave': self.zones[self.grave_player]['graveyard'] = []
            elif eligible:
                if self.return_card == 'Forest': self.zones[self.grave_player]['graveyard'] = ['Forest']
                else: self.zones[self.grave_player]['graveyard'].append(self.return_card)
            if mode == 'both_graves':
                self.return_card = 'Grizzly Bears'; self.zones[0]['graveyard'].append(self.return_card)
                self.zones[1]['graveyard'].append('Elvish Visionary')
        for player in (0, 1):
            if mode == 'empty_hand' and player == 1: self.zones[player]['hand'] = []
            for zone, names in self.zones[player].items():
                self.setup += [card(n, f'stock_{zone}_{player}_{i}', player, zone) for i, n in enumerate(names)]
        self.hand_mode = mode.startswith(('cycling', 'ninjutsu'))
        self.casting = mode in ('cast', 'payment', 'insufficient_mana', 'wrong_color', 'fresh_attack')
        self.add(name, 'source', 0, 'hand' if self.casting or self.hand_mode else 'battlefield')
        self.add('Grizzly Bears', 'idle'); self.add('Wall of Denial', 'wall', 1)
        self.add('Grizzly Bears', 'victim', 1); self.add('Forest', 'land', 1); self.add('Ornithopter', 'artifact', 1)
        if effect == 'color_bounce':
            self.add('Abyssal Specter', 'black_creature', 1)
            self.add('Grizzly Bears', 'friendly_green', 0)
        helper = 'Giant Growth' if mode in ('pump', 'partial_returns') else 'Unsummon' if mode in ('source_bounce', 'reanimated_bounce') else None
        if helper: self.add(helper, 'helper', 0, 'hand')
        if self.boundary: self.add('Hermetic Study', 'grant', 0, 'hand')
        if mode.startswith('regenerate') or mode == 'ninjutsu_regenerate': self.add('Terminate' if mode == 'regenerate_cannot' else 'Murder', 'fatal', 0, 'hand')
        if mode == 'declined' and p['effect'] == 'grave_play' and p['trigger_cost'] and self.owner == 0:
            # Floating mana is automatically spent before any cancellation
            # input is offered. Real untapped lands preserve affordability
            # while opening the ordinary human payment-selection window.
            payment = self.cost(dict(mana_cost=p['trigger_cost']))
            lands = ['Forest']*payment[0]
            for color, land in enumerate(('Swamp','Island','Forest','Mountain','Plains'), 1): lands += [land]*payment[color]
            for index, land in enumerate(lands): self.add(land, 'payment_land_' + str(index))
        self.emit('turn', player=0, value=0); self.emit('discard_policy', value=1); self.emit('present_ai_dialogs', value=1)
        if mode == 'payment': self.pools[0] = list(p['mana'])
        if mode in ('insufficient_mana', 'trigger_insufficient', 'cycling_insufficient', 'ninjutsu_insufficient'): self.pools[0] = [0]*6
        if mode in ('wrong_color', 'trigger_wrong_color'): self.pools[0] = [60, 0, 0, 0, 0, 0]
        for prefix, cost in (('trigger', p['trigger_cost']), ('cycling', p['cycling']), ('ninjutsu', p['ninjutsu'])):
            if mode == prefix + '_payment': self.pools[0] = self.cost(dict(mana_cost=cost))
        if mode == 'regenerate_insufficient': self.pools[0] = [2, 2, 0, 0, 0, 0]  # Murder payable, regeneration black unavailable after cast.
        if mode == 'declined' and p['effect'] == 'grave_play' and p['trigger_cost']: self.pools[0] = [0]*6
        for player in (0, 1): self.set_pool(player, self.pools[player])
        self.emit('observe_search_filters', value=1); self.ready = len(self.steps)

    def add(self, name, ref, player=0, zone='battlefield', state=None):
        super().add(name, ref, player, zone, state)
        self.entities[ref]['owner'] = player

    def move(self, ref, dest):
        e = self.entities[ref]; original = e['owner']
        if e['zone'] not in ('battlefield', 'gone'): self.zones[original][e['zone']].remove(e['name'])
        if dest != 'battlefield': self.zones[original][dest].append(e['name'])
        e['zone'] = dest
        if dest != 'battlefield': e.update(player=original, tapped=False, boost=0, counters=0, damage=0)

    def set_pool(self, player, values):
        self.pools[player] = list(values); self.mana[player] = sum(values)
        for color, value in enumerate(values): self.emit('set_mana', player=player, color=color, value=value)

    def spend(self, text, player=0):
        cost = self.cost(dict(mana_cost=text))
        # Every cast/payment fixture supplies colorless generic mana, so this
        # ledger never assumes an arbitrary colored-mana spend policy.
        if any(a < b for a, b in zip(self.pools[player], cost)): raise ValueError('fixture cannot pay printed cost')
        self.pools[player] = [a-b for a, b in zip(self.pools[player], cost)]
        self.mana[player] = sum(self.pools[player])

    def spell(self, ref, name, target=None, entity=False):
        self.spend(self.oracle[name]['mana_cost'])
        # The shared emitter maintains total mana itself; precompensate its
        # expected total while retaining the exact per-color independent pool.
        self.mana[0] += sum(self.cost(self.oracle[name]))
        super().spell(ref, name, target, entity)

    def eligible_grave(self):
        if self.p['effect'] == 'grave_hand':
            return [n for n in self.zones[0]['graveyard'] if recovery_eligible(self.oracle[n], dict(scope=self.p['selector'], value_max=None))]
        allowed = (0, 1) if self.p['selector'] == 'any_grave' else (1,)
        return [n for player in allowed for n in self.zones[player]['graveyard'] if 'Creature' in self.oracle[n]['type_line'].split(' — ')[0].split()]

    def bounce(self, ref): self.move(ref, 'hand')

    def return_grave(self):
        p = self.p; mode = self.mode; choices = self.eligible_grave()
        if not choices:
            # The legacy Unspeakable helper opens a selector with wrong types;
            # Scion/Ink-Eyes/Teneb count eligible creatures first.
            if p['effect'] == 'grave_hand' and self.zones[0]['graveyard']:
                self.emit('choose_optional_zone', player=0, value=self.rows[self.return_card]['csvid'])
            return
        if p['input_profile']['outer_optional'] and self.owner == 0:
            if mode == 'declined': return
            self.emit('choose_trigger_source', player=0, ref='source')
        if mode == 'declined':
            if p['trigger_cost']: self.emit('choose_cancel_mana', player=0); return
            self.emit('choose_optional_zone', player=0, value=self.rows[self.return_card]['csvid']); return
        if p['trigger_cost']:
            payable = all(a >= b for a, b in zip(self.pools[0], self.cost(dict(mana_cost=p['trigger_cost']))))
            if not payable: return
            self.spend(p['trigger_cost'])
        grave_player = 0 if p['effect'] == 'grave_hand' else self.grave_player
        if p['effect'] == 'grave_play' and p['selector'] == 'any_grave':
            available = [pl for pl in (0, 1) if any('Creature' in self.oracle[n]['type_line'].split(' — ')[0].split() for n in self.zones[pl]['graveyard'])]
            if grave_player not in available: grave_player = available[0]
            if len(available) == 2: self.emit('choose_player', player=0, value=grave_player)
        local = [n for n in self.zones[grave_player]['graveyard'] if n in choices]
        chosen = self.return_card if self.return_card in local else local[0]
        self.emit('choose_zone_card', player=0, value=self.rows[chosen]['csvid'])
        self.zones[grave_player]['graveyard'].remove(chosen)
        if p['effect'] == 'grave_hand': self.zones[0]['hand'].append(chosen)
        else:
            ref = 'returned_' + str(sum(k.startswith('returned_') for k in self.entities))
            r = self.oracle[chosen]
            self.entities[ref] = dict(name=chosen, owner=grave_player, player=0, zone='battlefield', creature=True,
                                     power=int(r['power']), toughness=int(r['toughness']), counters=0, boost=0, damage=0, tapped=False, token=False)
            self.bound.add(ref)
            # Binding occurs after the real trigger has resolved, in receipts.
            self.entities[ref]['needs_bind'] = True
            if chosen == 'Elvish Visionary': self.draw(0, 1)

    def return_effect(self, amount, target=1, combat=True):
        p = self.p; mode = self.mode
        if amount <= 0 or not (combat or p['scope'] == 'any') or (target != 1 and p['recipient'] == 'opponent'): return
        effect = p['effect']
        if effect in ('grave_hand', 'grave_play'): self.return_grave(); return
        if effect == 'color_bounce':
            payable = all(a >= b for a, b in zip(self.pools[0], self.cost(dict(mana_cost=p['trigger_cost']))))
            if not payable: return
            self.emit('choose_dialog', player=0, value=1 if mode == 'declined' else 0)
            if mode == 'declined': return
            self.spend(p['trigger_cost']); color = 'U' if mode == 'self_color' else 'B' if mode == 'multicolor' else 'G'
            self.emit('choose_color', player=0, value={'B':1, 'U':2, 'G':3, 'R':4, 'W':5}[color])
            for ref, e in self.entities.items():
                if e['zone'] == 'battlefield' and e['creature'] and color in self.oracle[e['name']].get('colors', []): self.bounce(ref)
            return
        if effect == 'modal_bounce_discard':
            discard = mode in ('discard_mode', 'empty_hand')
            self.emit('choose_dialog', player=0, value=0 if discard else 1)
            if discard:
                if self.zones[target]['hand']:
                    self.discard_nondeterministic = True
                    self.discard_initial = list(self.zones[target]['hand'] + self.zones[target]['graveyard'])
                    self.zones[target]['graveyard'].append(self.zones[target]['hand'].pop(0))
                return
            ref = 'wall' if mode == 'shroud_return' else next((r for r in ('victim', 'land', 'artifact', 'wall') if self.entities[r]['zone'] == 'battlefield'), None)
            if ref is None: return
            self.emit('choose_target', player=target, ref=ref); self.bounce(ref); return
        targets = [r for r in ('victim', 'land', 'artifact', 'wall') if self.entities[r]['zone'] == 'battlefield']
        if mode == 'land_return': targets = ['land']
        elif mode == 'artifact_return': targets = ['artifact']
        else: targets = targets[:1 if mode in ('one_return', 'partial_returns') or effect == 'bounce_one' else amount]
        if mode in ('zero_returns', 'declined'): targets = []
        # Wall of Denial is an intentionally untargetable contrary permanent.
        targets = [r for r in targets if r != 'wall']
        for ref in targets: self.emit('choose_target', player=0, ref=ref); self.bounce(ref)
        remaining = [r for r in ('victim', 'land', 'artifact') if self.entities[r]['zone'] == 'battlefield']
        if len(targets) < (amount if effect == 'bounce_damage' else 1) and remaining:
            # The reviewed legacy menu asks separately for every damage slot;
            # canceling one selection does not terminate its surrounding loop.
            # Each repeated Done is an ordinary optional choice with a still
            # legal fallback, never a forced illegal target or extra return.
            stops = amount-len(targets) if p['input_profile'].get('up_to_stop_policy') == 'each_remaining_damage_slot' else 1
            for _ in range(stops): self.emit('choose_optional_target', player=0, ref=remaining[0])

    def receipts(self):
        if self.boundary:
            # Missing real noncombat triggers must leave ordinary observable
            # input/effect failures. Drain their actual unused decision plan
            # before the immutable parent's strict empty-plan receipt.
            self.emit('assert_unused_choices', value=0)
        for ref, e in self.entities.items():
            if e.pop('needs_bind', False):
                prior = list(self.entities).index(ref)
                index = sum(x['name'] == e['name'] and x['player'] == e['player'] and x['zone'] == 'battlefield' for x in list(self.entities.values())[:prior])
                self.emit('bind_permanent_copy', player=e['player'], value=self.rows[e['name']]['csvid'], index=index, ref=ref)
                self.emit('assert_card', ref=ref, property='sick', value=1)
        start = len(self.steps); super().receipts()
        if self.discard_nondeterministic:
            # Any card is a legal nonrandom choice. Preserve exact identities
            # across hand+grave without asserting which legal card an AI chose.
            self.steps[start:] = [s for s in self.steps[start:] if not (s['op'] == 'assert_zone_multiplicity' and s.get('player') == 1 and s.get('zone') in ('hand', 'graveyard'))]
            for name in sorted(set(self.discard_initial)):
                total = self.discard_initial.count(name)
                self.emit('assert_hand_graveyard_total', player=1, index=self.rows[name]['csvid'], value=total)
                for zone in ('hand', 'graveyard'): self.emit('assert_zone_multiplicity_max', player=1, zone=zone, index=self.rows[name]['csvid'], value=total)
        for player in (0, 1):
            for color, value in enumerate(self.pools[player]): self.emit('assert_mana', player=player, color=color, value=value)
        self.emit('assert_forced_zone_choices', value=0); self.emit('assert_forced_target_choices', value=0)

    def combat(self):
        mode = self.mode; self.emit('begin_attackers')
        legal = mode != 'fresh_attack'; self.emit('attack', ref='source', value=int(legal))
        if legal: self.entities['source']['tapped'] = True
        self.emit('finish_attackers'); cut = len(self.steps); self.emit('begin_blockers')
        if mode == 'blocked': self.emit('block', ref='wall', other='source', value=1)
        self.emit('advance_combat')
        amount = self.power('source') if legal else 0
        if mode == 'blocked':
            self.entities['wall']['damage'] += amount
            if self.entities['wall']['damage'] >= 8: self.move('wall', 'graveyard')
            amount = 0  # Source maximum6; Wall8 prevents trample excess here.
        self.life[1] -= amount; self.return_effect(amount)
        self.emit('combat_damage'); self.emit('resolve'); self.emit('end_combat'); return cut

    def next_own_turn(self):
        if self.p.get('effect') == 'bounce_one' and self.mode == 'two_hits':
            # The first legal creature target is now in its owner's hand.
            # Recast it on that player's actual intervening turn, rather than
            # scripting a noncreature fallback for the next combat trigger.
            victim = self.entities['victim']; player = victim['owner']
            if player != 1 or victim['zone'] != 'hand':
                raise ValueError('repeated creature bounce requires the real bounced victim')
            for pl in (0, 1): self.set_pool(pl, [0]*6)
            self.emit('advance_turn'); self.emit('untap_step', player=player)
            self.emit('turn', player=player, value=0)
            for e in self.entities.values():
                e.update(damage=0, boost=0)
                if e['player'] == player: e['tapped'] = False
            payment = self.cost(self.oracle[victim['name']])
            self.set_pool(player, payment)
            self.emit('bind_hand', player=player, value=self.rows[victim['name']]['csvid'], ref='victim')
            self.emit('assert_card', ref='victim', property='in_hand', value=1)
            self.emit('cast', ref='victim', value=1)
            self.spend(self.oracle[victim['name']]['mana_cost'], player)
            self.emit('assert_stack', value=1)
            self.emit('assert_card', ref='victim', property='in_hand', value=0)
            self.emit('assert_mana_total', player=player, value=self.mana[player])
            for color, value in enumerate(self.pools[player]):
                self.emit('assert_mana', player=player, color=color, value=value)
            self.emit('resolve'); self.move('victim', 'battlefield')
            self.emit('assert_card', ref='victim', property='sick', value=1)
            self.receipts()
            self.emit('advance_turn'); self.emit('untap_step', player=0)
            self.emit('turn', player=0, value=0)
            for e in self.entities.values(): e.update(tapped=False, damage=0, boost=0)
            for pl in (0, 1): self.set_pool(pl, [10]*6)
            return
        super().next_own_turn()
        for player in (0, 1): self.set_pool(player, [10]*6)

    def cycling(self):
        mode = self.mode; legal = mode != 'cycling_insufficient'
        self.emit('can_activate_hand', ref='source', value=int(legal))
        if not legal: return len(self.steps)
        self.emit('choose_dialog', player=0, value=1); self.emit('activate_hand', ref='source', value=1)
        self.spend(self.p['cycling']); self.move('source', 'graveyard'); self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='in_play', value=0)
        self.emit('assert_zone_multiplicity', player=0, zone='graveyard', index=self.csvid, value=1)
        self.emit('assert_mana_total', player=0, value=self.mana[0]); cut = len(self.steps)
        self.draw(0, 1); self.emit('resolve'); return cut

    def ninjutsu(self):
        mode = self.mode; self.emit('can_activate_hand', ref='source', value=0)
        self.emit('begin_attackers'); self.emit('attack', ref='idle', value=1); self.entities['idle']['tapped'] = True
        self.emit('finish_attackers')
        if mode == 'ninjutsu_before_blocks': self.emit('can_activate_hand', ref='source', value=0)
        self.emit('begin_blockers')
        if mode == 'ninjutsu_blocked': self.emit('block', ref='victim', other='idle', value=1)
        self.emit('advance_combat'); legal = mode not in ('ninjutsu_insufficient', 'ninjutsu_blocked')
        self.emit('can_activate_hand', ref='source', value=int(legal))
        cut = len(self.steps)
        if legal:
            self.emit('choose_dialog', player=0, value=1); self.emit('choose_target', player=0, ref='idle')
            self.emit('activate_hand', ref='source', value=1); self.emit('assert_stack', value=1)
            self.spend(self.p['ninjutsu']); self.bounce('idle'); cut = len(self.steps)
            self.emit('resolve'); self.move('source', 'battlefield'); self.entities['source']['tapped'] = True
            self.emit('assert_card', ref='source', property='in_hand', value=0)
            for property in ('tapped', 'attacking', 'sick'): self.emit('assert_card', ref='source', property=property, value=1)
            self.emit('assert_zone_multiplicity', player=0, zone='hand', index=self.rows['Grizzly Bears']['csvid'], value=1)
            amount = self.power('source'); self.life[1] -= amount; self.return_effect(amount)
        elif mode == 'ninjutsu_blocked':
            self.move('idle', 'graveyard'); self.move('victim', 'graveyard')
        else: self.life[1] -= self.power('idle')
        self.emit('combat_damage'); self.emit('resolve'); self.emit('end_combat'); return cut

    def regenerate(self):
        mode = self.mode; self.emit('choose_target', player=0, ref='source'); self.emit('cast', ref='fatal', value=1)
        self.spend(self.oracle[self.entities['fatal']['name']]['mana_cost']); self.move('fatal', 'graveyard')
        cut = len(self.steps)
        if mode in ('regenerate', 'ninjutsu_regenerate'):
            self.emit('choose_regeneration', ref='source'); self.spend(self.p['regeneration'])
            self.entities['source']['tapped'] = True
        else: self.move('source', 'graveyard')
        self.emit('resolve'); return cut

    def run(self):
        mode = self.mode; cut = self.ready
        if self.casting:
            if mode in ('insufficient_mana', 'wrong_color'):
                self.emit('cast', ref='source', value=0); self.emit('assert_card', ref='source', property='in_hand', value=1)
            else: self.spell('source', self.name, entity=True)
        if mode.startswith('cycling'): cut = self.cycling()
        elif mode.startswith('ninjutsu'):
            cut = self.ninjutsu()
            if mode == 'ninjutsu_regenerate': self.receipts(); self.regenerate()
        elif mode.startswith('regenerate'): cut = self.regenerate()
        elif self.boundary:
            self.spell('grant', 'Hermetic Study', 'source', True); cut = len(self.steps)
            target = 0 if mode == 'noncombat_self' else 1
            self.emit('choose_player', player=0, value=target); self.emit('activate', ref='grant', value=1)
            self.entities['source']['tapped'] = True; self.emit('assert_card', ref='source', property='tapped', value=1)
            self.emit('assert_card', ref='grant', property='tapped', value=0)
            self.life[target] -= 1; self.return_effect(1, target, False); self.emit('resolve')
        elif not self.casting or mode == 'fresh_attack':
            if mode in ('pump', 'partial_returns'):
                self.spell('helper', 'Giant Growth', 'source'); self.entities['source']['boost'] += 3
            cut = self.combat()
            if mode == 'two_hits': self.receipts(); self.next_own_turn(); self.combat()
            if mode == 'source_bounce': self.receipts(); self.spell('helper', 'Unsummon', 'source'); self.bounce('source')
            if mode == 'reanimated_bounce':
                self.receipts(); ref = next(r for r in self.entities if r.startswith('returned_'))
                self.spell('helper', 'Unsummon', ref); self.bounce(ref)
            if mode in ('pump', 'partial_returns', 'cleanup'):
                self.receipts()
                for player in (0, 1): self.set_pool(player, [0]*6)
                self.emit('cleanup'); self.emit('refresh')
                for e in self.entities.values(): e.update(boost=0, damage=0)
        self.receipts(); steps = self.steps
        if mode in ('ready_snapshot', 'combat_snapshot', 'cycling_pending_snapshot', 'ninjutsu_pending_snapshot'):
            point = self.ready if mode == 'ready_snapshot' else cut; replay = steps[point:]
            steps = copy.deepcopy(steps[:point]) + [self.step('capture')] + copy.deepcopy(replay) + [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')] + copy.deepcopy(replay) + [self.step('assert_recorded_state')]
        case = dict(name=f'ReturnSaboteurTriggerRecipe_{self.csvid}_{self.requested}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=self.requested,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'], untested_components=[
                        'Multiplayer, simultaneous multiple sources, damage replacement/prevention and extra combat steps.',
                        'No fabricated priority window: target loss between damage-trigger announcement and resolution is not claimed.',
                        'Generic any-permanent/graveyard forbidden-target permission observations without a faithful availability probe remain separate work.']))
        if self.owner: _mirror(case)
        return case


def expand(name, csvid, declaration, p, selected, registry, oracle):
    validate_witnesses(oracle, registry)
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported return variants required')
    return [ReturnBoard(name, csvid, declaration, p, v, registry, oracle).run() for v in selected]
