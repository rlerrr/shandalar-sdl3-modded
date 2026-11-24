"""Optional card draw from actual combat damage by an attached creature."""
import copy
import json
import re
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'attachment_combat_draw'
PROFILE = Path(__file__).parent / 'data/attachment_combat_draw_wave8_profiles.json'
FEAR_REMINDER = " (It can't be blocked except by artifact creatures and/or black creatures.)"
EQUIP_REMINDER = ' ({{{cost}}}: Attach to target creature you control. Equip only as a sorcery.)'


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face')):
        raise ValueError('normal whole attachment record required')
    lines = record.get('oracle_text','').splitlines()
    if not lines:
        raise ValueError('complete attachment clauses required')
    types = record['type_line'].split(' — ')
    equip = 0
    if types == ['Artifact', 'Equipment']:
        cost = re.fullmatch(r'Equip \{([1-9])\}(.*)', lines.pop())
        if not cost or cost[2] not in ('', EQUIP_REMINDER.format(cost=cost[1])) or record['keywords'] != ['Equip']:
            raise ValueError('complete printed Equip cost required')
        attach = 'equipment'
        equip = int(cost[1])
        subject = 'equipped'
    elif types == ['Enchantment', 'Aura'] and lines.pop(0) == 'Enchant creature' and record['keywords'] == ['Enchant']:
        attach = 'aura'
        subject = 'enchanted'
    else:
        raise ValueError('reviewed Equipment or enchant creature Aura required')
    fear_line = subject.capitalize()+' creature has fear.'+FEAR_REMINDER
    fear = bool(lines and lines[0] == fear_line)
    if fear:
        lines.pop(0)
    if lines == [f'Whenever {subject} creature deals combat damage to a player, you may draw a card.']:
        recipient, ability = 'player', 'attachment'
    elif lines == [f'Whenever {subject} creature deals combat damage to an opponent, you may draw a card.']:
        recipient, ability = 'opponent', 'attachment'
    elif attach == 'aura' and lines == ['Enchanted creature has "Whenever this creature deals combat damage to a player, you may draw a card."']:
        recipient, ability = 'player', 'granted'
    else:
        raise ValueError('complete optional attached combat draw clause required')
    return dict(schema=1, attachment=attach, equip=equip, fear=fear, recipient=recipient, ability=ability)


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2 or
            declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), expected)):
        raise ValueError('typed whole attachment draw declaration required')
    model = dict(expected, family=FAMILY, mana=cost_from_oracle(record))
    selection = declaration.get('variants')
    if not isinstance(selection, list) or not selection or len(set(selection)) != len(selection) or set(selection)-set(variants(model)):
        raise ValueError('explicit unique supported attachment draw variants required')
    return model


def variants(model):
    modes = ['hit','blocked','other_attacker','noncombat','source_removed','unaffordable_cast',
             'ready_snapshot','damage_snapshot','foreign_owned_host']
    modes += (['unattached','equip_insufficient','equip_no_own','moved'] if model['equip'] else
              ['opposing_host'])
    if model['fear']:
        modes += ['fear_green','fear_artifact']
    return [f'{mode}_{p}' for p in (0,1) for mode in modes]+['decline_0']


def validate_witnesses(oracle, registry):
    data = profiles()
    for name,pin in data['witnesses'].items():
        if not exact(oracle.get(name),pin['oracle']) or not exact(registry.get(name),pin['registry']):
            raise ValueError('attachment draw witness changed: '+name)
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(oracle.get(name),pin['oracle']) or not exact(row,pin['registry']) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented whole attachment draw source changed: '+name)
        parameters(oracle[name])


class AttachmentWorld(World):
    def __init__(self, name, owner, model, registry, oracle, mode, input_source):
        super().__init__(name,owner,model,registry,oracle)
        self.mode = mode
        self.input_source = input_source
        self.host_controller = 1-owner if mode in ('opposing_host','equip_no_own') else owner
        self.host = None
        self.add(name,'source',owner,'hand')
        self.add('Grizzly Bears','host',self.host_controller,foreign=mode=='foreign_owned_host')
        if mode in ('other_attacker','moved'):
            self.add('Hill Giant','other_host',owner)
        # Keep two distinct legal Aura targets so a planned input corresponds to
        # an actual choice for either controller, including the opposing host.
        if not model['equip']:
            self.add('Eager Cadet','other_target',1-self.host_controller)
        if mode in ('blocked','fear_green','fear_artifact'):
            blocker = {'blocked':'Wall of Bone','fear_green':'Runeclaw Bear','fear_artifact':'Wall of Tanglecord'}[mode]
            self.add(blocker,'blocker',1-self.host_controller)

    def effects(self):
        checks = [step for key,e in self.entities.items() if e['alive'] for step in self.stats(key)]
        return checks+self.mana_checks()

    def cast_attachment(self):
        from recipes import step

        player = self.controller
        if self.mode=='unaffordable_cast':
            return [step('cast',ref='source',value=0),step('assert_unused_choices',value=0)]
        sequence = self.fund(player,self.model['mana'])
        if not self.model['equip']:
            sequence += [step('choose_target',player=player,ref='host')]
            self.host = 'host'
        sequence += [step('cast',ref='source',value=1),step('assert_stack',value=1),step('assert_unused_choices',value=0),
                     step('resolve'),step('assert_stack',value=0),step('assert_unused_choices',value=0)]
        self.zones['hand'][player].remove(self.name)
        self.entities['source']['alive'] = True
        self.mana[player] = [0]*6
        return sequence+self.effects()

    def equip(self, target='host'):
        from recipes import step

        player = self.controller
        payable = self.mode!='equip_insufficient'
        legal = payable and self.mode!='equip_no_own'
        run = self.fund(player,[self.model['equip'] if payable else 0,0,0,0,0,0])
        run += [step('require_can_activate',ref='source',value=int(legal))]
        if legal:
            run += [step('choose_target',player=player,ref=target),step('activate',ref='source',value=1),
                    step('assert_stack',value=1),step('assert_unused_choices',value=0),step('resolve'),
                    step('refresh'),step('assert_stack',value=0),step('assert_unused_choices',value=0)]
            self.mana[player] = [0]*6
            self.host = target
        else:
            run += [step('activate',ref='source',value=0),step('assert_unused_choices',value=0)]
        return run+self.effects()

    def detach(self):
        from recipes import step

        run = self.spell('Naturalize','removal',('source',))
        self.leave('source')
        self.host = None
        return run+[step('resolve'),step('assert_unused_choices',value=0)]+self.effects()

    def declare(self, attacker):
        from recipes import observe, step

        player = self.entities[attacker]['player']
        run = [step('turn',player=player,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),
               observe(attacker,'tapped',1),step('finish_attackers'),step('begin_blockers')]
        self.entities[attacker]['tapped'] = 1
        blocked = self.mode in ('blocked','fear_artifact')
        if blocked or self.mode=='fear_green':
            run += [step('block',ref='blocker',other=attacker,value=int(blocked)),
                    observe('blocker','blocking_count',int(blocked))]
        return run+[step('advance_combat'),observe(attacker,'blocked',int(blocked))],blocked

    def damage(self, attacker, blocked):
        from recipes import step

        attacker_entity = self.entities[attacker]
        actor = attacker_entity['player']
        defender = 1-actor
        run = []
        unused = 0
        power = int(self.oracle[attacker_entity['name']]['power'])
        if blocked:
            blocker_entity = self.entities['blocker']
            attacker_entity['damage'] = int(self.oracle[blocker_entity['name']]['power'])
            blocker_entity['damage'] = power
        else:
            self.life[defender] -= power
            trigger_controller = actor if self.model['ability']=='granted' else self.controller
            qualifies = self.host==attacker and power>0 and (self.model['recipient']=='player' or defender!=trigger_controller)
            if qualifies and self.mode!='decline':
                # Mask/Robe highlight the attachment; Gloves highlights the
                # damage source. The opposing-host grant is an independent
                # controller probe and queues the creature's actual controller.
                if trigger_controller==0:
                    priority_ref = attacker if self.mode=='opposing_host' and self.model['ability']=='granted' else self.input_source
                    run += [step('choose_trigger_source',player=0,ref=priority_ref)]
                self.zones['hand'][trigger_controller].append(self.zones['library'][trigger_controller].pop(0))
            elif self.host==attacker and self.model['recipient']=='opponent' and defender==trigger_controller==0:
                # The Robe's controller was hit by the opposing enchanted
                # creature. An optional response may not be offered here.
                run += [step('choose_trigger_source',player=0,ref='source')]
                unused = 1
        return run+[step('combat_damage'),step('resolve'),step('assert_unused_choices',value=unused),step('end_combat')]+self.effects()

    def noncombat(self):
        from recipes import step

        player = self.controller
        run = self.spell('Hermetic Study','grant',('host',),caster=player)
        run += [step('resolve'),step('assert_unused_choices',value=0),step('require_can_activate',ref='grant',value=1),
                step('choose_player',player=player,value=1-player),step('activate',ref='grant',value=1),
                step('assert_stack',value=1),step('resolve'),step('assert_unused_choices',value=0)]
        self.entities['host']['tapped'] = 1
        self.life[1-player] -= 1
        return run+self.effects()

    def receipts(self):
        return super().receipts()+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved attachment draw source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        w = AttachmentWorld(name,int(player),model,registry,oracle,mode,pin['priority_ref'])
        actions = w.cast_attachment()
        if model['equip'] and mode not in ('unaffordable_cast','unattached'):
            actions += w.equip()
        if mode=='moved':
            actions += w.equip('other_host')
        if mode=='source_removed':
            actions += w.detach()
        if mode=='noncombat':
            actions += w.noncombat()
        cut = 0
        if mode not in ('unaffordable_cast','equip_insufficient','equip_no_own','noncombat'):
            attacker = 'other_host' if mode=='other_attacker' else 'host'
            run,blocked = w.declare(attacker)
            actions += run
            if mode=='damage_snapshot':
                cut = len(actions)
            actions += w.damage(attacker,blocked)
        tail = actions+w.receipts()
        if mode in ('ready_snapshot','damage_snapshot'):
            replay = tail[cut:]
            tail = tail[:cut]+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),
                   step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        result.append(dict(name=f'AttachmentCombatDrawRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+tail,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=[
                               'AI discretionary decline requires a real observed UI path',
                               'multiplayer opponent and damage-recipient distinctions require a multiplayer fixture'])))
    return result
