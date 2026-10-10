"""Explicit Oracle counterspell components exercised through real priority windows."""
TYPES = ("creature", "instant", "sorcery", "artifact", "enchantment", "aura")
DESTINATIONS = ("graveyard", "exile", "hand", "library_top", "library_bottom")
WITNESSES = (("Grizzly Bears", "creature", "G", 2), ("Hill Giant", "creature", "R", 4),
             ("Watchwolf", "creature", "GW", 2), ("Giant Growth", "instant", "G", 1),
             ("Lightning Bolt", "instant", "R", 1), ("Ancestral Recall", "instant", "U", 1),
             ("Divination", "sorcery", "U", 3), ("Sol Ring", "artifact", "", 1),
             ("Glorious Anthem", "enchantment", "W", 3), ("Pacifism", "aura", "W", 2))
LIBRARY = ("Forest", "Plains", "Grizzly Bears", "Forest", "Mountain", "Island", "Lightning Bolt")


def validate_witnesses(oracle, registry):
    """Structured Oracle fields validate the independently selected stack spells."""
    if not isinstance(oracle, dict):
        raise ValueError("counterspell expansion requires pinned witness Oracle context")
    for name, category, colors, value in WITNESSES:
        record = oracle.get(name, {})
        if (name not in registry or category.title() not in record.get("type_line", "")
                or set(record.get("colors", [])) != set(colors) or record.get("cmc") != value):
            raise ValueError(f"counterspell witness Oracle characteristics changed: {name}")
    reviewed_text = {
        "Giant Growth": "Target creature gets +3/+3 until end of turn.",
        "Ancestral Recall": "Target player draws three cards.",
        "Divination": "Draw two cards.",
        "Glorious Anthem": "Creatures you control get +1/+1.",
    }
    for name, text in reviewed_text.items():
        if oracle[name].get("oracle_text") != text:
            raise ValueError(f"counterspell witness Oracle effect changed: {name}")
    if (oracle["Watchwolf"].get("power"), oracle["Watchwolf"].get("toughness")) != ("3", "3"):
        raise ValueError("counterspell creature power witness changed: Watchwolf")


def spec(parameters, mana):
    allowed = {"targets", "colors", "value_min", "value_max", "value_exact", "destination", "draw",
               "target_draw", "gain_life", "target_lose_life", "tax", "scry", "mill", "target_life_from_value",
               "target_damage_from_power", "self_destination"}
    if not isinstance(parameters, dict) or set(parameters)-allowed:
        raise ValueError("unknown counterspell parameters")
    targets = parameters.get("targets")
    if not isinstance(targets,list) or not targets or any(not isinstance(t,str) or t not in TYPES for t in targets) or len(set(targets))!=len(targets):
        raise ValueError("counterspell targets require unique reviewed spell types")
    colors = parameters.get("colors", [])
    if not isinstance(colors,list) or any(not isinstance(c,str) or c not in ("G","W","R","U","B","colorless","multicolored") for c in colors) or len(set(colors))!=len(colors):
        raise ValueError("unknown counterspell color restriction")
    for key in ("value_min", "value_max", "value_exact", "draw", "target_draw", "gain_life", "target_lose_life", "tax", "scry", "mill"):
        if key in parameters and (type(parameters[key]) is not int or not 1 <= parameters[key] <= (2 if key=="scry" else 10)):
            raise ValueError("counterspell amounts must be reviewed positive integers")
    if "value_exact" in parameters and ("value_min" in parameters or "value_max" in parameters):
        raise ValueError("counterspell exact value cannot be combined with a range")
    if parameters.get("value_min",1)>parameters.get("value_max",10): raise ValueError("invalid counterspell value range")
    for key in ("target_life_from_value", "target_damage_from_power"):
        if key in parameters and type(parameters[key]) is not bool: raise ValueError("counterspell derived effects require booleans")
    if parameters.get("destination", "graveyard") not in DESTINATIONS or parameters.get("self_destination", "graveyard") not in ("graveyard", "library_bottom"):
        raise ValueError("unknown counterspell destination")
    if parameters.get("draw",0)+parameters.get("target_draw",0)>4 or parameters.get("mill",0)>4:
        raise ValueError("counterspell zone effect exceeds reviewed fixture bounds")
    if parameters.get("target_damage_from_power") and targets != ["creature"]:
        raise ValueError("power-derived counterspell damage requires creature-only targets")
    result = dict(family="counter_spell",mana=mana,**parameters)
    if not any(eligible(w,result) for w in WITNESSES): raise ValueError("counterspell has no reviewed eligible witness")
    return result


def eligible(witness, parameters, ignore=None):
    _,category,colors,value = witness
    allowed_colors = parameters.get("colors", [])
    return ((ignore=="type" or category in parameters["targets"])
        and (ignore=="color" or not allowed_colors or any((not colors if c=="colorless" else len(colors)>1 if c=="multicolored" else c in colors) for c in allowed_colors))
        and (ignore=="value" or parameters.get("value_min",1)<=value<=parameters.get("value_max",10)
             and value==parameters.get("value_exact",value)))


def variants(parameters):
    result = ["opponent", "own_spell", "payment", "insufficient_mana", "no_stack", "snapshot"]
    candidates = [w for w in WITNESSES if eligible(w,parameters)]
    result += ["type_"+t for t in TYPES if t!=candidates[0][1] and any(w[1]==t for w in candidates)]
    for name,ignore in (("wrong_type","type"),("wrong_color","color"),("wrong_value","value")):
        if any(not eligible(w,parameters) and eligible(w,parameters,ignore) for w in WITNESSES):result.append(name)
    for color in parameters.get("colors", []):
        if color in "GWURB" and any(color in w[2] for w in candidates):
            result.append("color_"+color)
    if parameters.get("target_damage_from_power") and any(w[0]=="Hill Giant" for w in candidates):
        result.append("creature_power")
    if parameters.get("tax"):result += ["tax_paid","tax_exact","tax_unaffordable"]
    return result


def expand(name, csvid, declaration, parameters, selected, registry, oracle):
    from recipes import card, step, observe, exact_mana, cost_from_oracle
    cases=[]
    for variant in selected:
        fixture="stocked_zones" if variant=="snapshot" else "empty_main"
        stock=4 if fixture=="stocked_zones" else 0
        owner=int(variant!="own_spell")
        valid=[w for w in WITNESSES if eligible(w,parameters)]
        victim=next((w for w in valid if w[1]==variant[5:]),valid[0]) if variant.startswith("type_") else valid[0]
        if variant.startswith("color_"):
            victim=next(w for w in valid if variant[6:] in w[2])
        if variant=="creature_power":victim=next(w for w in valid if w[0]=="Hill Giant")
        if variant.startswith("wrong_"):
            ignore={"wrong_type":"type","wrong_color":"color","wrong_value":"value"}[variant]
            victim=next(w for w in WITNESSES if not eligible(w,parameters) and eligible(w,parameters,ignore))
        target,category,_,value=victim
        target_id=registry[target]["csvid"]
        target_cost=cost_from_oracle(oracle[target])
        setup=[card(name,"source",zone="hand"),card(target,"spell",player=owner,zone="hand"),card("Scaled Wurm","recipient",player=owner if target=="Giant Growth" else 1-owner if target in ("Lightning Bolt","Pacifism") else 0)]
        if fixture=="empty_main":
            setup += [card(n,f"library_{p}_{i}",player=p,zone="library") for p in (0,1) for i,n in enumerate(LIBRARY)]
        actions=[step("turn",player=owner,value=0)]
        if variant=="snapshot":actions += [step("capture")]
        if variant=="payment":actions += exact_mana(parameters["mana"])
        if variant=="insufficient_mana":actions += exact_mana([0]*6)
        if variant in ("tax_exact","tax_unaffordable"):
            pool=list(target_cost);pool[0]+=parameters["tax"] if variant=="tax_exact" else 0
            actions += [step("set_mana",player=owner,color=i,value=n) for i,n in enumerate(pool)]
        negative=variant in ("no_stack","insufficient_mana","wrong_type","wrong_color","wrong_value")
        def target_choices():
            if target in ("Giant Growth","Lightning Bolt","Pacifism"):return [step("choose_target",player=-1,ref="recipient")]
            if target=="Ancestral Recall":return [step("choose_player",player=-1,value=owner)]
            return []
        effect=[]
        if variant=="no_stack":
            effect=[step("can_cast",ref="source",value=0),observe("source","in_hand",1),step("assert_stack",value=0)]
        elif negative:
            effect=target_choices()+[step("cast",ref="spell",value=1),step("assert_stack",value=1)]
            effect += ([step("cast",ref="source",value=0)] if variant=="insufficient_mana" else [step("can_cast",ref="source",value=0)])
            effect += [observe("source","in_hand",1),step("assert_stack",value=1),step("resolve"),step("assert_choices_consumed")]
        else:
            effect=target_choices()+[step("choose_priority",ref="source")]
            if parameters.get("tax") and variant!="tax_unaffordable":effect += [step("choose_dialog",player=owner,value=0 if variant in ("tax_paid","tax_exact") else 1)]
            if parameters.get("scry"):effect += [step("choose_zone",player=0,value=-1)]*parameters["scry"]
            effect += [step("cast",ref="spell",value=int(variant in ("tax_paid","tax_exact"))),step("resolve"),step("assert_choices_consumed")]
        # The cast opens and completes the normal priority/response window.
        # A countered original cast therefore returns false; no card event is
        # fired directly by the testing recipe.
        countered=not negative and variant not in ("tax_paid","tax_exact")
        resolved=variant!="no_stack" and not countered
        source_resolved=not negative
        hands=[stock,stock];graves=[stock,stock];exiles=[2 if stock else 0]*2
        libraries=[list(LIBRARY),list(LIBRARY)]
        hands[0]+=int(not source_resolved)
        if variant=="no_stack":hands[owner]+=1
        destination=parameters.get("destination","graveyard")
        if source_resolved:
            if parameters.get("self_destination","graveyard")=="graveyard":graves[0]+=1
        if countered:
            if destination=="graveyard":graves[owner]+=1
            elif destination=="exile":exiles[owner]+=1
            elif destination=="hand":hands[owner]+=1
            elif destination=="library_top":libraries[owner].insert(0,target)
            else:libraries[owner].append(target)
        elif resolved and category in ("instant","sorcery"):graves[owner]+=1
        if source_resolved and parameters.get("self_destination")=="library_bottom":libraries[0].append(name)
        drawn=[[],[]]
        if source_resolved:
            for player,count in ((owner,parameters.get("target_draw",0)),(0,parameters.get("draw",0))):
                drawn[player]+=libraries[player][:count];del libraries[player][:count];hands[player]+=count
        milled=[]
        if source_resolved and parameters.get("mill"):
            count=parameters["mill"];milled=libraries[owner][:count];del libraries[owner][:count];graves[owner]+=count
        if resolved and target in ("Divination","Ancestral Recall"):
            count=2 if target=="Divination" else 3
            drawn[owner]+=libraries[owner][:count];del libraries[owner][:count];hands[owner]+=count
        life=[20,20]
        if source_resolved:
            life[0]+=parameters.get("gain_life",0)
            life[owner]-=parameters.get("target_lose_life",0)
            if parameters.get("target_life_from_value"):life[owner]+=value
            if parameters.get("target_damage_from_power"):life[owner]-=int(oracle[target]["power"])
        checks=[step("assert_stack",value=0),observe("source","in_hand",int(not source_resolved)),
                observe("spell","in_play",int(resolved and category not in ("instant","sorcery"))),
                observe("recipient","power",10 if resolved and target=="Giant Growth" else 8 if resolved and target=="Glorious Anthem" and owner==0 else 7),
                observe("recipient","damage",3 if resolved and target=="Lightning Bolt" else 0)]
        for p in (0,1):
            checks += [step("assert_life",player=p,value=life[p]),step("assert_zone",player=p,zone="hand",value=hands[p]),
                       step("assert_zone",player=p,zone="graveyard",value=graves[p]),step("assert_zone",player=p,zone="exile",value=exiles[p]),
                       step("assert_zone",player=p,zone="library",value=len(libraries[p]))]
            checks += [step("assert_zone_card",player=p,zone="library",index=i,value=registry[n]["csvid"]) for i,n in enumerate(libraries[p])]
            checks += [step("assert_zone_contains",player=p,zone="hand",value=registry[n]["csvid"]) for n in drawn[p]]
        if source_resolved and parameters.get("self_destination","graveyard")=="graveyard":checks += [step("assert_zone_contains",player=0,zone="graveyard",value=csvid)]
        if countered and destination in ("graveyard","exile","hand"):checks += [step("assert_zone_contains",player=owner,zone=destination,value=target_id)]
        if resolved:checks += [step("assert_zone_contains",player=owner,zone="graveyard" if category in ("instant","sorcery") else "battlefield",value=target_id)]
        checks += [step("assert_zone_contains",player=owner,zone="graveyard",value=registry[n]["csvid"]) for n in milled]
        expected=0 if variant in ("payment","insufficient_mana") else 60-(sum(parameters["mana"]) if source_resolved else 0)-(sum(target_cost) if owner==0 and variant!="no_stack" else 0)
        if owner==0 and variant in ("tax_paid","tax_exact"):expected-=parameters["tax"]
        checks += [step("assert_mana_total",player=0,value=expected)]
        if owner==1:
            target_mana=0 if variant in ("tax_exact","tax_unaffordable") else 60-(sum(target_cost) if variant!="no_stack" else 0)-(parameters.get("tax",0) if variant=="tax_paid" else 0)
            checks += [step("assert_mana_total",player=owner,value=target_mana)]
        actions += effect+checks
        if variant=="snapshot":actions += [step("record_state"),step("ai_roundtrip"),step("restore")]+effect+checks+[step("assert_recorded_state")]
        cases.append(dict(name=f"CounterRecipe_{csvid}_{variant}",card=name,fixture=fixture,seed=123,setup=setup,steps=actions,
                          _recipe=dict(family="counter_spell",version=2,variant=variant,params=declaration["params"],oracle_text=declaration["oracle_text"])))
    return cases
