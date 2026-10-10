"""Reviewed temporary ability grants, verified through real actions and combat."""
from collections import Counter

KEYWORDS = ("Flying", "Reach", "Trample", "First strike", "Double strike", "Lifelink",
            "Indestructible", "Haste", "Hexproof", "Shroud", "Deathtouch")
LIBRARY = ("Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt")
GRAVE = ("Grizzly Bears", "Giant Growth", "Forest", "Sol Ring")
EXILE = ("Island", "Llanowar Elves")
HAND = ("Grizzly Bears", "Giant Growth", "Lightning Bolt", "Plains")


def spec(p, mana, family="grant_creature"):
    if family == "aura_control":
        required={"power", "toughness", "keywords", "scope"}
        optional={"no_attack", "no_block", "no_activate", "mana_exception", "combat_prevent", "return_aura", "gain_life"}
        if not isinstance(p,dict) or not required <= set(p) or set(p)-required-optional:
            raise ValueError("unknown or missing Aura control parameters")
        if any(type(p[k]) is not int or not -13 <= p[k] <= 3 for k in ("power","toughness")) or p["keywords"] != []:
            raise ValueError("Aura control requires bounded signed stats and no granted keywords")
        if p["scope"] not in ("creature","permanent","nonland","artifact_or_creature"):
            raise ValueError("unsupported Aura control target scope")
        for key in optional-{"gain_life"}:
            if key in p and p[key] is not True:raise ValueError("Aura control flags require explicit true values")
        if "gain_life" in p and (type(p["gain_life"]) is not int or not 1 <= p["gain_life"] <= 4):
            raise ValueError("Aura entry life requires a bounded positive amount")
        if p.get("mana_exception") and not p.get("no_activate"):
            raise ValueError("mana exception requires an activation restriction")
        if p["toughness"] < -3 or p["power"] > 0 and p["toughness"] >= 0:
            raise ValueError("Aura control stat range requires reviewed surviving and lethal witnesses")
        return dict(family=family,mana=mana,**p)
    aura=family=="aura_creature"
    required = {"power", "toughness", "keywords"}
    supported_keywords=KEYWORDS+("Vigilance",) if aura else KEYWORDS
    optional = {"controller", "draw", "untap", "gain_toughness", "death_loss", "prevent_damage", "must_be_blocked"}
    if aura: optional |= {"return_aura", "gain_life", "lose_life"}
    if not isinstance(p, dict) or not required <= set(p) or set(p)-required-optional:
        raise ValueError("unknown or missing ability-grant parameters")
    if any(type(p[k]) is not int or not 0 <= p[k] <= (10 if aura else 7) for k in ("power", "toughness")):
        raise ValueError("ability-grant stats require bounded nonnegative integers")
    if (not isinstance(p["keywords"], list) or not p["keywords"] and not aura
            or any(not isinstance(k, str) or k not in supported_keywords for k in p["keywords"])
            or len(set(p["keywords"])) != len(p["keywords"])):
        raise ValueError("ability grants require reviewed unique keywords")
    if p.get("controller", "any") not in ("any", "self"):
        raise ValueError("unsupported ability-grant controller")
    for key in ("untap", "gain_toughness", "prevent_damage", "must_be_blocked", "return_aura"):
        if key in p and p[key] is not True: raise ValueError("ability-grant flags require explicit true values")
    for key in ("draw", "death_loss", "gain_life", "lose_life"):
        if key in p and (type(p[key]) is not int or not 1 <= p[key] <= 4):
            raise ValueError("ability-grant amounts require bounded positive integers")
    if p.get("death_loss") and "Deathtouch" not in p["keywords"]:
        raise ValueError("death-loss component requires the reviewed deathtouch scenario")
    return dict(family=family, mana=mana, **p)


def variants(p):
    if p["family"]=="aura_control":
        result=["friendly_cleanup","opponent_cleanup","payment","insufficient_mana","empty","lost_target",
                "snapshot","tapped","large_target","unblocked","opponent_unblocked","empty_zones",
                "aura_removed","aura_bounced","host_bounced","host_destroyed","removed_attack","block_ground","removed_block"]
        if p["scope"] != "permanent":result.append("wrong_type")
        if p["toughness"]<0:result.append("lethal")
        elif p["power"]<0:result.append("small_survivor")
        if p.get("combat_prevent"):result.append("prevent_block_damage")
        if p.get("no_activate"):
            result += ["activate","removed_activate","opponent_activate","mana_activate","removed_mana_activate","opponent_mana_activate"]
        return result
    keys = set(p["keywords"])
    result = ["friendly_cleanup", "payment", "insufficient_mana", "empty", "wrong_type", "lost_target",
              "snapshot", "tapped", "large_target", "unblocked"]
    if p.get("controller", "any") == "any": result += ["opponent_cleanup", "opponent_unblocked"]
    else: result.append("wrong_controller")
    if not p.get("draw"): result.append("empty_zones")
    if "Flying" in keys: result += ["flying_evasion", "flying_reach", "expired_flying"]
    if keys & {"Flying", "Reach"}: result += ["block_flying", "expired_block_flying"]
    if "Trample" in keys: result += ["trample", "expired_trample"]
    if p.get("must_be_blocked"): result.append("forced_block")
    if keys & {"First strike", "Double strike"}: result += ["first_strike", "expired_first_strike"]
    if "Double strike" in keys: result.append("expired_double_strike")
    if "Lifelink" in keys: result.append("expired_lifelink")
    if "Deathtouch" in keys: result += ["deathtouch", "expired_deathtouch"]
    if "Indestructible" in keys:
        if p["toughness"] > 2: raise ValueError("indestructibility damage/zero-toughness probes require a small recipient")
        result += ["destroy", "lethal_damage", "zero_toughness", "expired_destroy"]
    if p.get("prevent_damage"): result += ["prevent_damage", "expired_damage"]
    if keys & {"Hexproof", "Shroud"}: result += ["opponent_targeting", "owner_targeting", "expired_targeting"]
    if "Haste" in keys: result += ["fresh_grant", "fresh_without_grant", "haste_removed"]
    # Opposing attacks after cleanup exercise expiry on the next player's turn.
    if p.get("controller") == "self":
        result = [v for v in result if v not in ("expired_flying", "expired_trample", "expired_double_strike", "expired_lifelink")]
    if p["family"]=="aura_creature":
        result += ["aura_removed", "aura_bounced"]
        if "Shroud" not in keys: result.append("host_bounced")
        if "Indestructible" not in keys: result.append("host_destroyed")
    return result


def validate_witnesses(o, registry):
    if not isinstance(o, dict): raise ValueError("ability grants require pinned Oracle witnesses")
    for name, power, toughness, keywords in (("Grizzly Bears", "2", "2", []), ("Hill Giant", "3", "3", []),
            ("Scaled Wurm", "7", "6", []), ("Suntail Hawk", "1", "1", ["Flying"]),
            ("Giant Spider", "2", "4", ["Reach"]), ("Llanowar Elves", "1", "1", [])):
        r=o.get(name,{})
        if name not in registry or (r.get("power"),r.get("toughness"),r.get("keywords")) != (power,toughness,keywords):
            raise ValueError(f"ability-grant creature witness changed: {name}")
    for name,cost,text in (("Unsummon","{U}","Return target creature to its owner's hand."),
            ("Flame Slash","{R}","Flame Slash deals 4 damage to target creature."),
            ("Grasp of Darkness","{B}{B}","Target creature gets -4/-4 until end of turn."),
            ("Humble","{1}{W}","Until end of turn, target creature loses all abilities and has base power and toughness 0/1.")):
        if name not in registry or (o.get(name,{}).get("mana_cost"),o.get(name,{}).get("oracle_text")) != (cost,text):
            raise ValueError(f"ability-grant action witness changed: {name}")


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana
    result=[]
    control=p["family"]=="aura_control"
    aura=p["family"] in ("aura_creature","aura_control")
    for variant in selected:
        empty=variant=="empty_zones"; expired=variant.startswith("expired_")
        owner=int(variant in ("opponent_cleanup","opponent_unblocked","wrong_controller","expired_flying","expired_trample",
                             "expired_double_strike","expired_lifelink","opponent_activate","opponent_mana_activate"))
        fresh=variant in ("fresh_grant","fresh_without_grant","haste_removed")
        negative=variant in ("empty","wrong_type","wrong_controller","insufficient_mana")
        eligibility=variant in declaration.get("eligibility_only", [])
        isolated=eligibility or variant in ("opponent_targeting","owner_targeting","expired_targeting")
        target="Scaled Wurm" if variant=="large_target" else "Sol Ring" if variant=="wrong_type" else "Grizzly Bears"
        activation=control and variant in ("activate","removed_activate","opponent_activate","mana_activate","removed_mana_activate","opponent_mana_activate")
        mana_activation=activation and "mana_activate" in variant
        if control:
            target=("Suntail Hawk" if variant=="lethal" else "Wirewood Elf" if mana_activation and p["toughness"]<0 else "Llanowar Elves" if mana_activation else "Furnace Whelp" if activation
                    else "Forest" if variant=="wrong_type" and p["scope"] in ("nonland","artifact_or_creature")
                    else "Sol Ring" if variant=="wrong_type" else "Scaled Wurm" if variant=="large_target"
                    else "Grizzly Bears" if variant=="small_survivor" or p["toughness"]>=0 else "Scaled Wurm")
        base_power=int(oracle[target].get("power",0));base_toughness=int(oracle[target].get("toughness",0))
        power=base_power+p["power"];toughness=base_toughness+p["toughness"]
        if activation and toughness<=0:raise ValueError("activation probe requires a surviving Aura host")
        hands=[list(() if empty else HAND) for _ in (0,1)];libraries=[list(() if empty else LIBRARY) for _ in (0,1)]
        graves=[list(() if empty else GRAVE) for _ in (0,1)];exiles=[list(() if empty else EXILE) for _ in (0,1)]
        hands[0].append(name);setup=[card(name,"source",zone="hand")];entities=[]
        if aura: entities.append(dict(name=name,ref="source",player=0,alive=False))
        def add(n,ref,player=0,tapped=False,zone="battlefield"):
            c=card(n,ref,player,zone)
            if zone=="battlefield":
                c["state"]=["tapped"] if tapped else ["untapped"]
                entities.append(dict(name=n,ref=ref,player=player,alive=True))
            elif zone=="hand": hands[player].append(n)
            setup.append(c)
        if not empty:
            fixture="stocked_zones"
        else: fixture="empty_main"
        if variant!="empty":
            add(target,"recipient",owner,variant=="tapped","hand" if fresh else "battlefield")
        if not negative and not isolated:
            add("Hill Giant","unselected",0);add("Hill Giant","enemy_unselected",1,True)
        if activation:add("Llanowar Elves" if mana_activation else "Furnace Whelp","activation_control",owner)
        attacker=blocker=None
        if variant in ("block_flying","expired_block_flying"):
            attacker="Suntail Hawk";add(attacker,"attacker",1)
        if variant in ("first_strike","expired_first_strike"):
            attacker="Llanowar Elves";add(attacker,"attacker",1)
        if variant=="prevent_block_damage":
            attacker="Llanowar Elves";add(attacker,"attacker",1)
        if variant in ("block_ground","removed_block"):
            attacker="Hill Giant";add(attacker,"attacker",1)
        if variant in ("deathtouch","expired_deathtouch"):
            attacker="Scaled Wurm";add(attacker,"attacker",1)
        if not eligibility and variant in ("flying_evasion","expired_flying","flying_reach","trample","expired_trample","forced_block"):
            blocker="Giant Spider" if variant=="flying_reach" else "Suntail Hawk" if "trample" in variant or variant=="forced_block" else "Grizzly Bears"
            add(blocker,"blocker",1-owner)
        response=variant=="lost_target"
        if response: add("Unsummon","response",0,zone="hand")
        removal=aura and (expired or variant in ("aura_removed","removed_attack","removed_block","removed_activate","removed_mana_activate"))
        if removal: add("Disenchant","removal",0,zone="hand")
        bounce="Unsummon" if variant=="host_bounced" else "Disperse" if variant=="aura_bounced" else None
        if bounce: add(bounce,"bounce",0,zone="hand")
        probe=variant in ("opponent_targeting","owner_targeting","expired_targeting")
        probe_actor=0 if variant=="owner_targeting" else 1
        if probe: add("Unsummon","probe",probe_actor,zone="hand")
        action_spell=("Flame Slash" if variant in ("lethal_damage","prevent_damage","expired_damage") else
                      "Grasp of Darkness" if variant=="zero_toughness" else "Humble" if variant=="haste_removed" else None)
        action_actor=0 if variant=="haste_removed" else 1
        if action_spell: add(action_spell,"action_spell",action_actor,zone="hand")
        mana=[60,60];life=[20,20];pre=[]
        if variant=="opponent_unblocked":
            if not aura and not oracle[name]["type_line"].startswith("Instant"):
                raise ValueError("opposing-turn combat grants require an instant")
            if not aura:pre.append(step("turn",player=1,value=0))
        if fresh:
            pre += [step("cast",ref="recipient",value=1),step("resolve"),step("assert_choices_consumed"),
                    observe("recipient","in_play",1),observe("recipient","sick",1)]
            hands[0].remove(target);mana[0]-=2;entities.append(dict(name=target,ref="recipient",player=0,alive=True))
        if variant=="payment": pre+=exact_mana(p["mana"]);mana[0]=sum(p["mana"])
        if variant=="insufficient_mana": pre+=exact_mana([0]*6);mana[0]=0
        effect=[];resolved=not negative and not response and not eligibility and variant!="fresh_without_grant"
        if negative:
            effect += [step("cast" if variant=="insufficient_mana" else "can_cast",ref="source",value=0),observe("source","in_hand",1)]
        elif eligibility:
            # An isolated legal opposing target makes the targeting defect an
            # eligibility assertion, without submitting an illegal host choice.
            effect += [step("can_cast",ref="source",value=1),observe("source","in_hand",1)]
        elif variant!="fresh_without_grant":
            effect += [step("can_cast",ref="source",value=1)]
            decisions=[step("choose_dialog",player=0,value=d) for d in declaration.get("cast_dialogs",{}).get(variant,[])]
            target_choice=step("choose_target",player=0,ref="recipient")
            effect += [target_choice]+decisions if declaration.get("cast_target_first") else decisions+[target_choice]
            if aura:effect += [dict(target_choice) for _ in range(declaration.get("cast_target_count",1)-1)]
            if response:
                effect += [step("choose_priority",ref="response"),step("choose_target",player=-1,ref="recipient")]
                hands[0].remove("Unsummon");graves[0].append("Unsummon");hands[owner].append(target);mana[0]-=1
                next(e for e in entities if e["ref"]=="recipient")["alive"]=False
            effect += [step("cast",ref="source",value=1),step("assert_stack",value=1)]
            if not response:
                effect += [observe("recipient","power",base_power),observe("recipient","toughness",base_toughness)]
            effect += [step("assert_life",player=i,value=20) for i in (0,1)]
            effect += [step("resolve"),step("assert_choices_consumed")]
            hands[0].remove(name);mana[0]-=sum(p["mana"])
            if aura and resolved:entities[0]["alive"]=True
            else:graves[0].append(name)
            if resolved:
                if variant=="lethal":
                    next(e for e in entities if e["ref"]=="recipient")["alive"]=False
                    entities[0]["alive"]=False;graves[owner].append(target);graves[0].append(name)
                    effect += [observe("recipient","in_play",0),observe("source","in_play",0)]
                else:effect += [observe("recipient","power",power),observe("recipient","toughness",toughness)]
                if p.get("gain_toughness"): life[0]+=toughness
                life[0]+=p.get("gain_life",0)-p.get("lose_life",0)
                for _ in range(p.get("draw",0)): hands[0].append(libraries[0].pop(0))
                if variant=="tapped":effect.append(observe("recipient","tapped",int(not p.get("untap"))))
        else:effect.append(observe("source","in_hand",1))
        follow=[]
        def aura_left():
            entities[0]["alive"]=False
            (hands[0] if p.get("return_aura") else graves[0]).append(name)
        if removal:
            follow += [step("choose_target",player=0,ref="source"),step("cast",ref="removal",value=1),step("resolve"),step("assert_choices_consumed"),observe("source","in_play",0),observe("recipient","power",base_power),observe("recipient","toughness",base_toughness)]
            hands[0].remove("Disenchant");graves[0].append("Disenchant");mana[0]-=2;aura_left()
            power=base_power;toughness=base_toughness
        if not eligibility and (variant in ("friendly_cleanup","opponent_cleanup") or expired):
            follow += [step("cleanup"),observe("recipient","power",power if aura else base_power),observe("recipient","toughness",toughness if aura else base_toughness)]
            if not aura:power=base_power;toughness=base_toughness
            if expired:follow.append(step("turn",player=1,value=0))
        keys=set() if expired or removal or not resolved else set(p["keywords"])
        def died(ref):
            e=next(e for e in entities if e["ref"]==ref);e["alive"]=False;graves[e["player"]].append(e["name"])
            if aura and ref=="recipient" and entities[0]["alive"]:aura_left()
        if variant=="host_destroyed":
            follow += [step("destroy",ref="recipient"),observe("recipient","in_play",0),observe("source","in_play",0)];died("recipient")
        if bounce:
            ref="recipient" if variant=="host_bounced" else "source"
            follow += [step("choose_target",player=0,ref=ref),step("cast",ref="bounce",value=1),step("resolve"),step("assert_choices_consumed"),observe(ref,"in_play",0)]
            hands[0].remove(bounce);graves[0].append(bounce);mana[0]-=1 if bounce=="Unsummon" else 2
            if ref=="recipient":
                next(e for e in entities if e["ref"]==ref)["alive"]=False;hands[owner].append(target);aura_left()
            else:
                entities[0]["alive"]=False;hands[0].append(name)
                follow += [observe("recipient","power",base_power),observe("recipient","toughness",base_toughness)]
        if probe:
            legal=int(expired or variant=="owner_targeting" and "Shroud" not in keys)
            follow.append(step("can_cast",ref="probe",value=legal))
            if legal:
                follow += [step("choose_target",player=probe_actor,ref="recipient"),step("cast",ref="probe",value=1),step("resolve"),step("assert_choices_consumed")]
                hands[probe_actor].remove("Unsummon");graves[probe_actor].append("Unsummon");mana[probe_actor]-=1
                next(e for e in entities if e["ref"]=="recipient")["alive"]=False;hands[owner].append(target)
                if aura and entities[0]["alive"]:aura_left()
                follow.append(observe("recipient","in_play",0))
        if variant in ("destroy","expired_destroy"):
            follow += [step("destroy",ref="recipient"),observe("recipient","in_play",int(not expired))]
            if expired:died("recipient")
            else:
                follow += [step("destroy",ref="unselected"),observe("unselected","in_play",0)];died("unselected")
        if action_spell:
            follow += [step("choose_target",player=action_actor,ref="recipient"),step("cast",ref="action_spell",value=1),step("resolve"),step("assert_choices_consumed")]
            hands[action_actor].remove(action_spell);graves[action_actor].append(action_spell)
            mana[action_actor]-=2 if action_spell in ("Grasp of Darkness","Humble") else 1
            if action_spell=="Humble":
                # Base-setting effects do not remove additive stat modifiers.
                power=p["power"];toughness=1+p["toughness"]
                follow += [observe("recipient","power",power),observe("recipient","toughness",toughness)]
            else:
                dead=variant in ("zero_toughness","expired_damage")
                follow.append(observe("recipient","in_play",int(not dead)))
                if dead:died("recipient")
                else:follow.append(observe("recipient","damage",0 if p.get("prevent_damage") else 4))
        if attacker:
            follow += [step("turn",player=1,value=0),step("begin_attackers"),step("attack",ref="attacker",value=1),
                       step("finish_attackers"),step("begin_blockers")]
            legal=int(variant!="expired_block_flying" and not (control and p.get("no_block") and not removal))
            follow += [step("block",ref="recipient",other="attacker",value=legal),observe("recipient","blocking_count",legal),step("advance_combat")]
            if control and variant=="block_ground" and p.get("no_block"):
                # A second, unaffected creature must still be able to block.
                follow[-1:-1]=[step("block",ref="unselected",other="attacker",value=1),observe("unselected","blocking_count",1)]
            if variant not in ("block_flying","expired_block_flying","block_ground","removed_block"):
                follow.append(step("combat_damage"))
                is_touch=variant in ("deathtouch","expired_deathtouch")
                first=bool(keys & {"First strike","Double strike"})
                recipient_dead=is_touch and not first and "Indestructible" not in keys and toughness<=7
                prevented=control and p.get("combat_prevent") and not removal
                attacker_dead=not prevented and (not is_touch or "Deathtouch" in keys or power>=6)
                follow += [observe("recipient","in_play",int(not recipient_dead)),observe("attacker","in_play",int(not attacker_dead))]
                if recipient_dead:died("recipient")
                else:follow.append(observe("recipient","damage",0 if first else 7 if is_touch else 1))
                if attacker_dead:
                    died("attacker")
                    if is_touch and not expired:life[1]-=p.get("death_loss",0)
                else:follow.append(observe("attacker","damage",0 if prevented else power))
        if activation:
            legal=int(bool(not p.get("no_activate") or removal or mana_activation and p.get("mana_exception")))
            follow += [step("can_activate",ref="recipient",value=legal),step("can_activate",ref="activation_control",value=1)]
            for ref in (("recipient","activation_control") if legal else ("activation_control",)):
                follow += [step("activate",ref=ref,value=1),step("resolve"),step("assert_choices_consumed")]
                if mana_activation:
                    mana[owner]+=1;follow.append(observe(ref,"tapped",1))
                else:
                    mana[owner]-=1
                    follow.append(observe(ref,"power",(power if ref=="recipient" else 2)+1))
        attacking=not eligibility and variant in ("unblocked","opponent_unblocked","flying_evasion","flying_reach","expired_flying","trample","expired_trample","forced_block",
                             "expired_double_strike","expired_lifelink","fresh_grant","fresh_without_grant","haste_removed","removed_attack","small_survivor")
        if attacking:
            if aura and variant=="opponent_unblocked":follow.append(step("turn",player=1,value=0))
            can_attack=int(variant not in ("fresh_without_grant","haste_removed") and not (control and p.get("no_attack") and not removal))
            follow += [step("begin_attackers"),step("attack",ref="recipient",value=can_attack),observe("recipient","attacking",can_attack),
                       observe("recipient","tapped",int(can_attack and "Vigilance" not in keys))]
            control_attack=control and variant=="unblocked" and p.get("no_attack")
            if control_attack:follow += [step("attack",ref="unselected",value=1),observe("unselected","attacking",1)]
            follow.append(step("finish_attackers"))
            forced=bool(blocker and p.get("must_be_blocked") and not expired)
            if forced: follow.append(step("choose_target",player=1-owner,ref="blocker"))
            follow.append(step("begin_blockers"))
            if blocker:
                legal=int(variant!="flying_evasion")
                if not forced: follow.append(step("block",ref="blocker",other="recipient",value=legal))
                follow.append(observe("blocker","blocking_count",legal))
            follow.append(step("advance_combat"))
            if variant not in ("flying_reach","expired_flying"):
                follow.append(step("combat_damage"))
                total=max(0,power)*(2 if "Double strike" in keys else 1) if can_attack and not (control and p.get("combat_prevent") and not removal) else 0
                dealt=total if not blocker or variant=="flying_evasion" else max(0,total-1) if "Trample" in keys else 0
                life[1-owner]-=dealt
                if control_attack:life[1]-=3
                if "Lifelink" in keys:life[owner]+=total
                blocked=bool(blocker and variant!="flying_evasion")
                follow += [observe("recipient","in_play",1),observe("recipient","damage",0 if not blocked or keys & {"First strike","Double strike"} else 1)]
                if blocked:
                    follow.append(observe("blocker","in_play",0));died("blocker")
        checks=[step("assert_stack",value=0),step("assert_choices_consumed")]
        for e in entities:
            checks.append(observe(e["ref"],"in_play",int(e["alive"])))
            if e["alive"] and e["ref"] in ("unselected","enemy_unselected"):
                control_tapped=control and variant=="unblocked" and p.get("no_attack") and e["ref"]=="unselected"
                checks += [observe(e["ref"],"power",3),observe(e["ref"],"toughness",3),observe(e["ref"],"tapped",int(bool(e["player"]==1 or control_tapped)))]
        for player in (0,1):
            checks += [step("assert_life",player=player,value=life[player]),step("assert_mana_total",player=player,value=mana[player]),
                       step("assert_zone",player=player,zone="battlefield",value=sum(e["alive"] and e["player"]==player for e in entities))]
            for n in dict.fromkeys(hands[player]+graves[player]):
                checks.append(step("assert_hand_graveyard_total",player=player,index=registry[n]["csvid"],value=hands[player].count(n)+graves[player].count(n)))
            for zone,names in (("hand",hands[player]),("graveyard",graves[player]),("library",libraries[player]),("exile",exiles[player])):
                checks.append(step("assert_zone",player=player,zone=zone,value=len(names)))
                if zone=="library":checks += [step("assert_zone_card",player=player,zone=zone,index=i,value=registry[n]["csvid"]) for i,n in enumerate(names)]
                else:checks += [step("assert_zone_contains",player=player,zone=zone,value=registry[n]["csvid"]) for n in Counter(names)]
        actions=pre+effect+follow+checks
        if variant=="snapshot":actions=pre+[step("capture")]+effect+follow+checks+[step("record_state"),step("ai_roundtrip"),step("restore")]+effect+follow+checks+[step("assert_recorded_state")]
        provenance=dict(family=p["family"],version=2,variant=variant,params=declaration["params"],oracle_text=declaration["oracle_text"])
        if variant in declaration.get("cast_dialogs",{}):provenance["cast_dialogs"]=declaration["cast_dialogs"][variant]
        if declaration.get("cast_target_first"):provenance["cast_target_first"]=True
        if eligibility:provenance["eligibility_only"]=True
        if "cast_target_count" in declaration:provenance["cast_target_count"]=declaration["cast_target_count"]
        result.append(dict(name=f"{'AuraControlRecipe' if control else 'AuraRecipe' if aura else 'GrantRecipe'}_{csvid}_{variant}",card=name,fixture=fixture,seed=123,setup=setup,steps=actions,_recipe=provenance))
    return result
