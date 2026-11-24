"""Real creature casts, entry sacrifices and their complete pinned rewards."""

import copy
import json
import re
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld

FAMILY = 'creature_entry_sacrifices'
PROFILE = Path(__file__).with_name('data') / 'creature_entry_sacrifices_wave8_profiles.json'
BLUE_CONDITION = 'When this creature enters, sacrifice it unless {U} was spent to cast it.'
UNDYING = ("Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the "
           "battlefield under its owner's control with a +1/+1 counter on it.)")
KICKER = 'Kicker {B} (You may pay an additional {B} as you cast this spell.)'
EDICT = 'When this creature enters, if it was kicked, target player sacrifices a creature of their choice.'
EXILE = 'When this creature enters, exile it unless you sacrifice another creature.'
LIFE_DRAW = ("When this creature enters, sacrifice another creature. You gain X life and draw X cards, "
             "where X is that creature's power.")
ARTIFACT_DAMAGE = ("When this creature enters, you may sacrifice an artifact. If you do, this creature deals 4 damage "
                   "divided as you choose among any number of targets.")
HERALD = "This creature can't be blocked.\nWhen this creature enters, you gain 4 life.\n" + BLUE_CONDITION
VIPER = 'Flying\nWhen this creature enters, create two 1/1 green and blue Snake creature tokens.\n' + BLUE_CONDITION
ORACLE_TEXTS = {HERALD:('blue_life', []), VIPER:('blue_tokens',['Flying']),
                'Flying\n'+EXILE+'\n'+UNDYING:('undying',['Flying','Undying']),
                LIFE_DRAW:('sacrifice_power',[]), KICKER+'\n'+EDICT:('kicker',['Kicker']),
                ARTIFACT_DAMAGE:('artifact_damage',[])}
HANDLERS = {'blue_life':'manalinkeh_card_azorius_herald', 'blue_tokens':'manalinkeh_card_patagia_viper',
            'undying':'manalinkeh_card_demonlord_of_ashmouth', 'sacrifice_power':'manalinkeh_card_disciple_of_bolas',
            'kicker':'manalinkeh_card_gatekeeper_of_malakir', 'artifact_damage':'manalinkeh_card_kuldotha_flamefiend'}
BEAR, GIANT, SQUIRE = 'Grizzly Bears', 'Hill Giant', 'Squire'
RELIC, MYR = 'Darksteel Relic', 'Darksteel Myr'
COUNTER, GROWTH, KILL, BLINK = 'Counterspell', 'Giant Growth', 'Murder', 'Cloudshift'
DECK = ('Forest','Island','Plains','Swamp','Mountain','Forest','Island')
WITNESSES = {BEAR, GIANT, SQUIRE, RELIC, MYR, COUNTER, GROWTH, KILL, BLINK, *DECK}
BOTH = {
    'blue_life': ('ordinary', 'blue', 'blink', 'combat'),
    'blue_tokens': ('ordinary', 'blue', 'blink', 'combat', 'plain_token_combat'),
    'undying': ('paid', 'unavailable', 'combat', 'undying_paid_twice', 'undying_unpaid'),
    'sacrifice_power': ('paid', 'unavailable', 'zero_power', 'boosted_power'),
    'kicker': ('paid', 'ordinary', 'no_victim'),
    'artifact_damage': ('split', 'artifact_creature', 'no_artifact'),
}
HUMAN = {'blue_life':('blue_left_unspent',), 'blue_tokens':('blue_left_unspent',),
         'undying':('decline',), 'sacrifice_power':('choose_different_power',),
         'kicker':('decline_kicker','self_target'), 'artifact_damage':('decline',)}
GAPS = ('Entry and its callbacks currently drain together. A separate entry-trigger response window is not modeled.',
        'Flamefiend currently asks for sacrifice before allocation. The pilot verifies actual damage and optional cost, without claiming an independent target-announcement/order witness.',
        'Voluntary human decline uses a real offered choice. AI discretionary refusal and non-forced sacrifice selection are not scripted.')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line','').startswith('Creature — ')):
        raise ValueError('whole normal entry-sacrifice creature required')
    if record['oracle_text'] not in ORACLE_TEXTS:
        return classify_typed(record)
    mode, keywords = ORACLE_TEXTS[record['oracle_text']]
    if record.get('keywords') != keywords:
        raise ValueError('whole printed entry-sacrifice keywords required')
    return dict(mode=mode, mana=cost_from_oracle(record), keywords=keywords,
                power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    mode = params['mode']
    if mode=='typed_unless':
        return typed_variants(params)
    return ([f'{case}_{player}' for player in (0,1) for case in BOTH[mode]+('countered','wrong_color','snapshot')]
            + [case+'_0' for case in HUMAN[mode]])


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(x) is not str for x in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(params))):
        raise ValueError('distinct supported entry-sacrifice variants required')


def spec(declaration, record):
    params = classify(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or profiles()['sources'].get(record['name'],{}).get('oracle') != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'),sort_keys=True) != json.dumps(params,sort_keys=True)):
        raise ValueError('exact typed entry-sacrifice declaration and full pinned Oracle required')
    check_variants(params, declaration.get('variants'))
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    skipped = {v['csvid'] for v in json.loads((PROFILE.parent/'unimplemented.json').read_text(
        encoding='utf8'))['cases'].values()}
    if (data.get('version') != 1 or not data.get('sources') or set(data.get('witnesses',())) != WITNESSES | TYPED_WITNESSES
            or set(data.get('tokens',())) != {'Snake'}):
        raise ValueError('entry-sacrifice profile or token identity changed')
    for group in ('sources','witnesses','tokens'):
        for name, pin in data[group].items():
            r, o = rows.get(name,{}), oracle.get(name,{})
            if pin != dict(registry=r,oracle=o):
                raise ValueError(f'exact entry-sacrifice input pin required: {name}')
            if group == 'tokens':
                # The generic identity is shared with other creators. The
                # Patagia token descriptor comes from the Patagia card text.
                if r['csvid']!=18821 or o['layout']!='token':
                    raise ValueError('reviewed physical Snake identity required')
                continue
            if (r.get('kind')!='card' or o.get('layout')!='normal' or r['csvid'] in skipped
                    or r['handler'] in (None,'NULL','0','card_dummy') and name not in (BEAR,GIANT,SQUIRE,'Giant Spider',*DUAL_LAND.values())):
                raise ValueError(f'normal implemented entry-sacrifice binding required: {name}')
            if group=='sources':
                mode = classify(o)['mode']
                allowed = TYPED_HANDLERS if mode=='typed_unless' else {HANDLERS[mode]}
                if r['handler'] not in allowed:
                    raise ValueError(f'entry-sacrifice production handler changed: {name}')


class EntryWorld(_AttackResourceWorld):
    def __init__(self, name, params, case, owner, rows, oracle):
        super().__init__(name, dict(params,effect=FAMILY), owner, rows, oracle)
        self.case = case
        self.model = params['mode']
        self.foe = 1-owner
        self.target_player = owner if case == 'self_target' else self.foe
        self.expects_recurrence_counter = False
        self.green_blue = self.model.startswith('blue_')
        self.blue_paid = self.green_blue and case in ('blue','blink','combat','snapshot')
        self.kicked = self.model=='kicker' and case not in ('ordinary','decline_kicker','countered','wrong_color')
        self.configure()

    def configure(self):
        case = self.case
        self.add(self.name,'source',self.owner,'hand',source=True)
        if self.model in ('undying','sacrifice_power') and case!='unavailable':
            self.add(MYR if case=='zero_power' else GIANT,'victim',self.owner)
            if case=='choose_different_power':
                self.add(SQUIRE,'chosen',self.owner)
        if self.model=='kicker' and case!='no_victim':
            self.add(BEAR,'victim',self.target_player)
        if self.model=='artifact_damage':
            if case!='no_artifact':
                self.add(MYR if case=='artifact_creature' else RELIC,'artifact',self.owner)
            self.add(RELIC,'foe_artifact',self.foe)
            self.add(GIANT,'foe_creature',self.foe)
        elif case in ('combat','plain_token_combat','unavailable'):
            self.add(GIANT,'foe_creature',self.foe)
        if case=='countered':
            self.add(COUNTER,'counter',self.foe,'hand')
        if case=='boosted_power':
            self.add(GROWTH,'growth',self.owner,'hand')
        if case=='blink':
            self.add(BLINK,'blink',self.owner,'hand')
        if case in ('undying_paid_twice','undying_unpaid'):
            self.add(KILL,'kill_first',self.foe,'hand')
            if case=='undying_paid_twice':
                self.add(KILL,'kill_second',self.foe,'hand')
                self.add(SQUIRE,'second_victim',self.owner,'hand')
        for p in (0,1):
            for index,name in enumerate(DECK):
                self.add(name,f'library_{p}_{index}',p,'library')
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            self.pool(p,[0]*6)
        self.receipts()

    def observe(self, ref, prop, value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def mana_receipts(self):
        for p,pool in enumerate(self.mana):
            for color,value in enumerate(pool):
                self.emit('assert_mana', player=p,color=color,value=value)

    def receipts(self):
        self.emit('refresh')
        start = len(self.steps)
        super().receipts()
        if self.expects_recurrence_counter:
            for step in self.steps[start:]:
                if step.get('ref')=='source' and step.get('property')=='plus1_counters':
                    step['value']=1
        if self.model=='blue_tokens':
            count = self.zones['battlefield'][self.owner].count('Snake')
            for player in (0,1):
                for zone in ('battlefield','graveyard','exile'):
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=18821,
                              value=count if player==self.owner and zone=='battlefield' else 0)
            if count:
                for prop,value in (('token',1),('power',1),('toughness',1),('color',12),
                                   ('artifact',0),('subtype_Snake',1)):
                    self.emit('assert_matching_permanents',player=self.owner,index=18821,property=prop,value=value)

    def pay_from_oracle(self, name, player):
        from recipes import cost_from_oracle
        self.pool(player,cost_from_oracle(self.o[name]))

    def regular_cast(self, ref, name, player, target=None):
        self.pay_from_oracle(name,player)
        if target:
            self.emit('choose_target',player=player,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.mana[player]=[0]*6

    def move_spell(self, name, player):
        self.zones['hand'][player].remove(name)
        self.zones['graveyard'][player].append(name)

    def source_pending(self):
        from recipes import COLORS
        b,u = COLORS['B'],COLORS['U']
        denied = self.case=='wrong_color'
        spend = list(self.p['mana'])
        if self.blue_paid:
            spend[0] -= 1
            spend[u] += 1
        if self.kicked:
            spend[b] += 1
        funded = [sum(spend),0,0,0,0,0] if denied else spend.copy()
        if self.case=='blue_left_unspent':
            funded[u] += 1
        if self.case=='decline_kicker':
            funded[b] += 1
        self.pool(self.owner,funded)
        if self.green_blue and self.owner==0 and funded[u]:
            self.emit('choose_dialog',player=self.owner,value=int(self.blue_paid))
        if self.kicked or self.case=='decline_kicker':
            self.emit('choose_dialog',player=self.owner,value=int(not self.kicked))
        self.emit('cast',ref='source',value=int(not denied))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(not denied))
        self.observe('source','in_hand',denied)
        self.observe('source','in_play',0)
        if not denied:
            self.mana[self.owner]=[initial-fee for initial,fee in zip(funded,spend)]
        self.mana_receipts()
        if denied:
            self.receipts()
        return not denied

    def blueprint_blue(self, paid):
        if self.model=='blue_life':
            self.life[self.owner] += 4
        else:
            self.zones['battlefield'][self.owner] += ['Snake']*2
        if not paid:
            self.leave('source','graveyard')

    def choose_sacrifice(self, ref, who, decline=False):
        if who==0:
            op = 'choose_optional_target' if decline else 'choose_target'
            self.emit(op, player=who, ref=ref)

    def sacrifice_victim(self, ref):
        if ref in self.entities:
            self.leave(ref,'graveyard')
        else:
            row = next(row for row in self.setup if row['ref']==ref)
            self.zones['battlefield'][row['player']].remove(row['name'])
            self.zones['graveyard'][row['player']].append(row['name'])

    def choose_entry(self):
        model,case = self.model,self.case
        if model=='undying' and case!='unavailable':
            self.choose_sacrifice('victim',self.owner,decline=case=='decline')
        if model=='sacrifice_power':
            if case=='unavailable':
                # If a mis-filtered mandatory selection offers the just-entered
                # Disciple, expose that receipt and its actual downstream effect.
                # Correct Oracle supplies no chooser and leaves this intent unused.
                self.emit('choose_optional_target',player=self.owner,ref='source')
            else:
                self.choose_sacrifice('chosen' if case=='choose_different_power' else 'victim',self.owner)
        if model=='kicker' and self.kicked:
            self.emit('choose_player',player=self.owner,value=self.target_player)
            if case=='self_target':
                self.choose_sacrifice('source',self.target_player)
            elif case!='no_victim':
                self.choose_sacrifice('victim',self.target_player)
        if model=='artifact_damage':
            if self.owner==0:
                # Snapshot the real currently pending source's identity and
                # elect its optional callback only when actually offered.
                self.emit('choose_trigger',player=self.owner,ref='source')
            if case!='no_artifact':
                self.choose_sacrifice('artifact',self.owner,decline=case=='decline')
            if case not in ('no_artifact','decline'):
                if case=='artifact_creature':
                    pips = [self.foe]*4
                else:
                    self.emit('choose_target',player=self.owner,ref='foe_creature')
                    pips = [self.foe]*3
                for p in pips:
                    self.emit('choose_player',player=self.owner,value=p)

    def oracle_entry(self):
        self.arrive('source')
        model,case = self.model,self.case
        if self.green_blue:
            self.blueprint_blue(self.blue_paid)
        elif model=='undying':
            if case in ('decline','unavailable'):
                self.leave('source','exile')
            else:
                self.sacrifice_victim('victim')
        elif model=='sacrifice_power' and case!='unavailable':
            ref = 'chosen' if case=='choose_different_power' else 'victim'
            amount = self.entities[ref]['power']
            self.sacrifice_victim(ref)
            self.life[self.owner] += amount
            for _ in range(amount):
                self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))
        elif model=='kicker' and self.kicked and case!='no_victim':
            self.sacrifice_victim('source' if case=='self_target' else 'victim')
        elif model=='artifact_damage' and case not in ('decline','no_artifact'):
            self.sacrifice_victim('artifact')
            if case=='artifact_creature':
                self.life[self.foe] -= 4
            else:
                self.entities['foe_creature']['damage'] += 1
                self.life[self.foe] -= 3

    def settle(self):
        model,case = self.model,self.case
        if case=='countered':
            self.emit('announce_spell',ref='source')
            self.pay_from_oracle(COUNTER,self.foe)
            self.emit('cast_response',ref='counter',value=1)
            self.emit('assert_stack',value=2)
            self.emit('resolve_response')
            self.mana[self.foe]=[0]*6
            self.move_spell(COUNTER,self.foe)
            self.leave('source','graveyard')
        elif case=='boosted_power':
            self.emit('announce_spell',ref='source')
            self.pay_from_oracle(GROWTH,self.owner)
            self.emit('choose_target',player=self.owner,ref='victim')
            self.emit('cast_response',ref='growth',value=1)
            self.emit('assert_stack',value=2)
            self.choose_entry()
            self.emit('resolve_response')
            self.mana[self.owner]=[0]*6
            self.move_spell(GROWTH,self.owner)
            for stat in ('power','toughness'):
                self.entities['victim'][stat] += 3
            self.oracle_entry()
        else:
            self.choose_entry()
            self.emit('resolve')
            self.oracle_entry()
        self.emit('assert_unused_choices',value=int(model=='sacrifice_power' and case=='unavailable'))
        if case=='decline' or model=='sacrifice_power' and case=='unavailable':
            self.emit('assert_forced_target_choices',value=0)
        self.receipts()

    def blink_source(self):
        self.regular_cast('blink',BLINK,self.owner,'source')
        self.emit('resolve')
        self.move_spell(BLINK,self.owner)
        self.blueprint_blue(False)
        self.receipts()

    def recurrence(self):
        second = self.case=='undying_paid_twice'
        if second:
            self.regular_cast('second_victim',SQUIRE,self.owner)
            self.emit('resolve')
            self.arrive('second_victim')
            self.receipts()
        self.regular_cast('kill_first',KILL,self.foe,'source')
        if second:
            self.choose_sacrifice('second_victim',self.owner)
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.move_spell(KILL,self.foe)
        if second:
            self.sacrifice_victim('second_victim')
            self.emit('bind_permanent',ref='source',player=self.owner,value=self.rows[self.name]['csvid'])
            self.entities['source'].update(power=self.p['power']+1,toughness=self.p['toughness']+1,sick=True)
            self.expects_recurrence_counter=True
            self.receipts()
            self.regular_cast('kill_second',KILL,self.foe,'source')
            self.emit('resolve')
            self.move_spell(KILL,self.foe)
            self.leave('source','graveyard')
        else:
            self.leave('source','exile')
        self.receipts()

    def combat(self):
        token = self.case=='plain_token_combat'
        ref = 'token' if token else 'source'
        if token:
            self.emit('bind_permanent_copy',ref=ref,player=self.owner,value=18821,index=0)
        self.age()
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='foe_creature',other=ref,value=int(token))
        self.observe('foe_creature','blocking_count',token)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('cleanup')
        if token:
            # A plain 1/1 Snake dies to a 3/3, without killing it.
            self.zones['battlefield'][self.owner].remove('Snake')
        else:
            self.entities['source']['tapped']=True
            self.life[self.foe] -= self.p['power']
        self.receipts()


def scenario(world):
    if not world.source_pending():
        return None
    cut = len(world.steps) if world.case=='snapshot' else None
    world.settle()
    if world.case=='blink':
        world.blink_source()
    elif world.case in ('undying_paid_twice','undying_unpaid'):
        world.recurrence()
    elif world.case in ('combat','plain_token_combat'):
        world.combat()
    return cut


TYPED_CLAUSE = re.compile(
    r'When this creature enters, sacrifice it unless you sacrifice (a Forest|two Swamps|'
    r'three Forests|two Mountains|a land|an artifact)\.')
TYPED_PAYMENTS = {
    'a Forest': ('Forest', 1), 'two Swamps': ('Swamp', 2), 'three Forests': ('Forest', 3),
    'two Mountains': ('Mountain', 2), 'a land': ('land', 1), 'an artifact': ('artifact', 1),
}
TYPED_RIDERS = {
    "Flying (This creature can't be blocked except by creatures with flying or reach.)": 'Flying',
    "Shroud (This creature can't be the target of spells or abilities.)": 'Shroud',
}
TYPED_HANDLERS = {
    'manalinkeh_card_mold_demon', 'manalinkeh_card_plant_elemental', 'manalinkeh_card_primeval_force',
    'manalinkeh_card_rathi_dragon', 'manalinkeh_card_rogue_elephant', 'manalinkeh_card_scythe_tiger',
    'manalinkeh_card_vulshok_war_boar',
}
DUAL_LAND = {'Forest': 'Bayou', 'Swamp': 'Badlands', 'Mountain': 'Taiga', 'land': 'Bayou'}
TYPED_WITNESSES = {'Giant Spider', *DUAL_LAND.values()}
TYPED_GAPS = ('Entry and its callbacks drain together. No independently pending entry-trigger priority observer.',
              'Human decline uses the real available choice. AI selects its available sacrifice by its normal policy.')


def classify_typed(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    match = TYPED_CLAUSE.fullmatch(lines[-1]) if lines else None
    rider = TYPED_RIDERS.get(lines[0]) if len(lines)==2 else None
    if not match or len(lines)!=1+int(bool(rider)) or record.get('keywords')!=([rider] if rider else []):
        raise ValueError('whole reviewed typed entry sacrifice and printed keyword required')
    resource, quantity = TYPED_PAYMENTS[match[1]]
    return dict(mode='typed_unless', resource=resource, quantity=quantity,
                mana=cost_from_oracle(record), keywords=record['keywords'],
                power=int(record['power']), toughness=int(record['toughness']))


def typed_variants(params):
    tests = ('paid','unavailable','snapshot','countered','wrong_color')
    if params['resource']=='artifact':
        tests += ('artifact_creature',)
    else:
        tests += ('nonbasic',)
    if 'Flying' in params['keywords']:
        tests += ('combat','reach')
    if 'Shroud' in params['keywords']:
        tests += ('shroud',)
    variants = [f'{case}_{p}' for p in (0,1) for case in tests] + ['decline_0']
    if params['quantity']>1:
        variants.append('short_1')
    if params['quantity']==3:
        variants.append('short_0')
    return variants


class TypedWorld(EntryWorld):
    def configure(self):
        self.resource = self.p['resource']
        self.quantity = self.p['quantity']
        self.victims = []
        case, seat = self.case, self.owner
        self.add(self.name,'source',seat,'hand',source=True)
        name = {'land':'Island','artifact':RELIC}.get(self.resource,self.resource)
        # Negative controls include an equally named opponent permanent.
        self.add(name,'foe_match',self.foe)
        qty = 0 if case=='unavailable' else self.quantity-int(case=='short')
        for index in range(qty):
            ref = f'matching_{index}'
            witness = name
            if case=='nonbasic' and index==0:
                witness = DUAL_LAND[self.resource]
            elif case=='artifact_creature':
                witness = MYR
            self.add(witness,ref,seat)
            self.victims.append(ref)
        other = { 'Forest':'Island','Swamp':'Plains','Mountain':'Forest','land':RELIC,'artifact':BEAR }
        self.add(other[self.resource],'own_nonmatch',seat)
        if case=='countered':
            self.add(COUNTER,'counter',self.foe,'hand')
        if case=='shroud':
            for p in (0,1):
                self.add(GROWTH,f'probe_{p}',p,'hand')
            self.add(BEAR,'shroud_control',seat)
        if case in ('combat','reach'):
            self.add('Giant Spider' if case=='reach' else GIANT,'foe_creature',self.foe)
        for p in (0,1):
            for i,n in enumerate(DECK):
                self.add(n,f'library_{p}_{i}',p,'library')
        if case=='wrong_color':
            for card in self.setup:
                if card['zone']=='battlefield' and 'Land' in self.o[card['name']]['type_line']:
                    card['state']=['tapped']
        self.emit('turn',player=seat,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            self.pool(p,[0]*6)
        self.receipts()

    def typed_intent(self):
        if not self.victims:
            return
        decline = self.case=='decline'
        short = self.case=='short'
        # Mold Demon presents a separate real accept/decline dialog only
        # when the complete two-Swamp option is offered.
        if self.resource=='Swamp' and not short:
            self.emit('choose_dialog',player=self.owner,value=int(decline))
            if decline:
                return
        if self.owner==0:
            if decline:
                self.emit('choose_optional_target',player=0,ref=self.victims[0])
            else:
                for ref in self.victims:
                    self.emit('choose_target',player=0,ref=ref)

    def settle(self):
        if self.case=='countered':
            return super().settle()
        self.typed_intent()
        self.emit('resolve')
        self.arrive('source')
        if self.case in ('unavailable','decline','short'):
            self.leave('source','graveyard')
        else:
            for ref in self.victims:
                self.sacrifice_victim(ref)
        # When Primeval Force offers payment with just two Forests, only
        # actual calls consume these intents. Oracle supplies no legal cost.
        expected_unused = len(self.victims) if self.case=='short' and self.owner==0 else 0
        self.emit('assert_unused_choices',value=expected_unused)
        if self.case=='decline':
            self.emit('assert_forced_target_choices',value=0)
        self.receipts()

    def reach_block(self):
        self.age()
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='foe_creature',other='source',value=1)
        self.observe('foe_creature','blocking_count',1)
        self.emit('advance_combat')
        self.observe('source','blocked',1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.entities['source']['damage'] = self.entities['foe_creature']['power']
        self.entities['source']['tapped'] = True
        self.leave('foe_creature','graveyard')
        self.emit('end_combat')
        self.receipts()

    def shroud(self):
        for p in (0,1):
            self.emit('can_target_creature',ref=f'probe_{p}',other='source',value=0)
            self.emit('can_target_creature',ref=f'probe_{p}',other='shroud_control',value=1)


def typed_scenario(world):
    if not world.source_pending():
        return None
    cut = len(world.steps) if world.case=='snapshot' else None
    world.settle()
    if world.case=='combat':
        world.combat()
    elif world.case=='reach':
        world.reach_block()
    elif world.case=='shroud':
        world.shroud()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step
    check_variants(params,selected)
    validate_witnesses(oracle,rows)
    if profiles()['sources'][name]['registry']['csvid']!=csvid:
        raise ValueError('exact entry-sacrifice source CSVID required')
    cases=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1)
        typed = params['mode']=='typed_unless'
        kind = TypedWorld if typed else EntryWorld
        world=kind(name,params,mode,int(owner),rows,oracle)
        cut=typed_scenario(world) if typed else scenario(world)
        steps=world.steps
        if cut is not None:
            tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+\
                  copy.deepcopy(tail)+[step('assert_recorded_state')]
        cases.append(dict(name=f'CreatureEntrySacrificesRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=173,setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                      params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=list(TYPED_GAPS if typed else GAPS))))
    return cases

