#include "manalink.h"

// Functions

void make_servo(int player, int card, int amount)
{
	generate_tokens_by_id(player, card, CARD_ID_SERVO, amount);
}

static void make_thopter(int player, int card, int amount)
{
	token_generation_t token;
	default_token_definition(player, card, CARD_ID_THOPTER, &token);
	token.pow = token.tou = 1;
	token.key_plus = KEYWORD_FLYING;
	token.qty = amount;
	generate_token(&token);
}

static void fabricate(int player, int card, int amount, int priority_counters, int priority_servo)
{
	/*Add "amount" +1/+1 counters on this creature or create "amount" 1/1 colorless Servo artifact creature token*/
	enum{
		CHOICE_COUNTERS = 1,
		CHOICE_SERVO
	};

	char msg1[100];
	char msg2[100];
	scnprintf(msg1, 100, "Add %d +1/+1 counters", amount);
	scnprintf(msg2, 100, "Create %d Servo", amount);

	int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL,
						msg1, 1, priority_counters,
						msg2, 1, priority_servo);

	if( choice == CHOICE_COUNTERS ){
		add_counters(player, card, COUNTER_P1_P1, amount);
	}

	if( choice == CHOICE_SERVO ){
		make_servo(player, card, amount);
	}
}

static void etb_fabricate(int player, int card, event_t event, int amount, int priority_counters, int priority_servo){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		fabricate(player, card, amount, priority_counters, priority_servo);
	}
}

static int can_crew(int player, int card){
	return ! call_card_function(player, card, EVENT_CANT_CREW);
}

static const char* this_creature_can_crew_a_vehicle(int who_chooses, int player, int card)
{
	if( can_crew(player, card) ){
		return NULL;
	}

	return "this creature can't Crew a vehicle";
}

int vehicle(int player, int card, event_t event, int crew)
{
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		int tot_pow = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_play(player, i) && is_what(player, i, TYPE_CREATURE) && ! is_tapped(player, i) && can_crew(player, i) ){
				tot_pow += get_power(player, i);
				if( tot_pow >= crew ){
					return 1;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.illegal_state = TARGET_STATE_TAPPED;
		td.extra = (int32_t)this_creature_can_crew_a_vehicle;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

		int sel_pow = 0;
		int targs = 0;

		while( sel_pow < crew ){
				char buffer[100];
				scnprintf(buffer, 100, "Select a creature for Crew with power %d or more.", crew-sel_pow);
				if( new_pick_target(&td, buffer, targs, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[targs].player, instance->targets[targs].card, 1);
					sel_pow += get_power(instance->targets[targs].player, instance->targets[targs].card);
					targs++;
				}
				else{
					break;
				}
		}

		int i;
		for(i=0; i<targs; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			if( sel_pow >= crew ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
				int csvid = get_id(instance->targets[i].player, instance->targets[i].card);
				switch( csvid ){
						case CARD_ID_GEARSHIFT_ACE:
							pump_ability_until_eot(instance->targets[i].player, instance->targets[i].card, player, card,
													0, 0, KEYWORD_FIRST_STRIKE, 0);
							break;

						case CARD_ID_SPEEDWAY_FANATIC:
							pump_ability_until_eot(instance->targets[i].player, instance->targets[i].card, player, card,
													0, 0, 0, SP_KEYWORD_HASTE);
							break;

						case CARD_ID_VETERAN_MOTORIST:
							pump_until_eot_merge_previous(instance->targets[i].player, instance->targets[i].card, player, card, 1, 1);
							break;

						default:
							break;
				}
			}
		}
		instance->number_of_targets = 0;

		if( sel_pow < crew ){
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		turn_into_creature(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 0, 0);
	}

	return 0;
}

void etb_energy(int player, int card, event_t event, int amount){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		add_energy(player, amount);
	}
}

int attack_trigger_with_energy(int player, int card, event_t event, int amount){

	if( ENERGY_COUNTERS(player) >= amount && declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card) ){
		return 1;
	}

	return 0;
}

static void boost_if_you_control_an_artifact(int player, int card, event_t event, int pow, int tou, int key, int s_key){

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) &&
		in_play(player, card) && ! is_humiliated(player, card) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) && !is_humiliated(player, card))
		{
			switch( event ){
					case EVENT_POWER:
						event_result += pow;
						break;
					case EVENT_TOUGHNESS:
						event_result += tou;
						break;
					case EVENT_ABILITIES:
					{
						event_result |= key;
						if( s_key ){
							special_abilities(player, card, event, s_key, player, card);
						}
						break;
					}
					default:
						break;
			}
		}
	}
}


// Cards

// White

int card_acrobatic_maneuver(int player, int card, event_t event){
/*
Acrobatic Maneuver |2|W	0x200f10b
Instant
Exile target creature you control, then return that card to the battlefield under its owner's control.
Draw a card.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
			draw_cards(player, 1);
			kill_card(player, card, KILL_DESTROY );
		}
	}

	return 0;
}

/*
Aerial Responder |1|W|W	0x2001fec (Sphinx of the Steel Wind)
Creature - Dwarf Soldier
Flying, vigilance, lifelink
2/3
*/

int card_aetherstorm_roc(int player, int card, event_t event){
/*
Aetherstorm Roc |2|W|W	0x200f110
Creature - Bird
Flying
Whenever Aetherstorm Roc or another creature enters the battlefield under your control, you get |E (an energy counter).
Whenever Aetherstorm Roc attacks, you may pay |E|E. If you do, put a +1/+1 counter on it and tap up to one target creature
defending player controls.
3/3
*/

     etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	 if( event == EVENT_ETB_ABILITY ){
	    add_energy(player, 1);
     }

	if( attack_trigger_with_energy(player, card, event, 2) ){
		remove_energy(player, 2);
		add_1_1_counter(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", 0, GS_LITERAL_PROMPT) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_angel_of_invention(int player, int card, event_t event){
/*
Angel of Invention |3|W|W	0x200f115
Creature - Angel
Flying, vigilance, lifelink
Fabricate 2 (When this creature enters the battlefield, put two +1/+1 counters on it or create two 1/1 colorless Servo artifact creature tokens.)
Other creatures you control get +1/+1.
2/1
*/
	vigilance(player, card, event);
	lifelink(player, card, event);
	etb_fabricate(player, card, event, 2, count_subtype(player, TYPE_CREATURE, -1)*5, 20);
	boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY);
	return 0;
}

int card_authority_of_the_consuls(int player, int card, event_t event){
/*
Authority of the Consuls |W	0x200f11a
Enchantment
Creatures your opponents control enter the battlefield tapped.
Whenever a creature enters the battlefield under an opponent's control, you gain 1 life.
*/

   if( in_play(player, card)  && ! is_humiliated(player, card)){
        permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_CREATURE, NULL);
   }

	etb_ability_global(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, 0, player, card);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_aviary_mechanic(int player, int card, event_t event){
/*
Aviary Mechanic |1|W	0x200f11f
Creature - Dwarf Artificer
When Aviary Mechanic enters the battlefield, you may return another permanent you control to its owner's hand.
2/2
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = td.preferred_controller = player;
		td.special = TARGET_SPECIAL_NOT_ME;
		td.illegal_abilities = 0;


		if( can_target(&td) && new_pick_target(&td, "Select another permanent you control.", 0, GS_LITERAL_PROMPT) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}

		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_built_to_last(int player, int card, event_t event){
/*
Built to Last |W	0x200f124
Instant
Target creature gets +2/+2 until end of turn. If it's an artifact creature, it gains indestructible until end of turn. (Damage and effects that say "destroy" don't destroy it.)
*/

	if( event == EVENT_CAN_CAST || event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, 0);
		if( spell_fizzled != 1 && instance->number_of_targets == 1 ){
			if( is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ){
				ai_modifier += 15;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int s_key = is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ? SP_KEYWORD_INDESTRUCTIBLE : 0;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2, 0, s_key);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/*
Captured by the Consulate |3|W
Enchantment - Aura
Enchant creature you don't control
Enchanted creature can't attack.
Whenever an opponent casts a spell, if it has a single target, change the target to enchanted creature if able.
*/

int card_cataclysmic_gearhulk(int player, int card, event_t event){
/*
Cataclysmic Gearhulk |3|W|W	0x200f129
Artifact Creature - Construct
Vigilance
When Cataclysmic Gearhulk enters the battlefield, each player chooses an artifact, a creature, an enchantment,
and a planeswalker from among the nonland permanents he or she controls, then sacrifices the rest.
4/5
*/
	vigilance(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		tragic_arrogance_effect(player, card, ANYBODY);
	}

	return 0;
}

int card_consulate_surveillance(int player, int card, event_t event){
/*
Consulate Surveillance |3|W	0x200f12e
Enchantment
When Consulate Surveillance enters the battlefield, you get |E|E|E|E (four energy counters).
Pay |E|E: Prevent all damage that would be dealt to you this turn by a source of your choice.
*/
	etb_energy(player, card, event, 4);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.extra = damage_card;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot = 0;
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0,
									GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2), &td, "TARGET_DAMAGE");
}

int card_consuls_shieldguard(int player, int card, event_t event){
/*
Consul's Shieldguard |3|W	0x200f133
Creature - Dwarf Soldier
When Consul's Shieldguard enters the battlefield, you get |E|E (two energy counters).
Whenever Consul's Shieldguard attacks, you may pay |E. If you do, another target attacking creature gains indestructible until end of turn.
3/4
*/

	etb_energy(player, card, event, 2);

	if( attack_trigger_with_energy(player, card, event, 1) ){
		remove_energy(player, 1);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.required_state = TARGET_STATE_ATTACKING;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target another target attacking creature.", 0, GS_LITERAL_PROMPT) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_eddytrail_hawk(int player, int card, event_t event){
/*
Eddytrail Hawk |1|W	0x200f138
Creature - Bird
Flying
When Eddytrail Hawk enters the battlefield, you get |E|E (two energy counters).
Whenever Eddytrail Hawk attacks, you may pay |E. If you do, another target attacking creature gains flying until end of turn.
1/2
*/
	etb_energy(player, card, event, 2);

	if( attack_trigger_with_energy(player, card, event, 1) ){
		remove_energy(player, 1);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.required_state = TARGET_STATE_ATTACKING;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target another target attacking creature.", 0, GS_LITERAL_PROMPT) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

/*
Fairgrounds Warden |2|W	0x200aa61 (Banisher Priest)
Creature - Dwarf Soldier
When Fairgrounds Warden enters the battlefield, exile target creature an opponent controls until Fairgrounds Warden leaves the battlefield.
1/3
*/

int card_fragmentize(int player, int card, event_t event){
/*
Fragmentize |W	0x200f13d
Sorcery
Destroy target artifact or enchantment with converted mana cost 4 or less.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);
	td.extra = 4;
	td.special = TARGET_SPECIAL_CMC_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						"Select target artifact or enchantment with converted mana cost 4 or less.", 1, NULL);
}

int card_fumigate(int player, int card, event_t event){
/*
Fumigate |3|W|W	0x200f142
Sorcery
Destroy all creatures. You gain 1 life for each creature destroyed this way.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		int result = 0;
		APNAP(p, { result += new_manipulate_all(player, card, p, &this_test, KILL_BURY);};);
		gain_life(player, result);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


/*
Gearshift Ace |1|W	0x401000 (vanilla, the effect is in "crew")
Creature - Dwarf Pilot
First strike
Whenever Gearshift Ace crews a Vehicle, that Vehicle gains first strike until end of turn.
2/1
*/

int card_glint_sleeve_artisan(int player, int card, event_t event){
	/*
	  Glint-Sleeve Artisan |2|W	0x200f147
	  Creature - Dwarf Artificer
	  Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
	  2/2
	*/
	// Used externally
	etb_fabricate(player, card, event, 1, count_subtype(player, TYPE_CREATURE, -1)*5, 20);
	return 0;
}

int card_herald_of_the_fair(int player, int card, event_t event){
/*
Herald of the Fair |2|W	0x200f14c
Creature - Human
When Herald of the Fair enters the battlefield, target creature you control gets +1/+1 until end of turn.
3/2
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;

		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT) ){
			card_instance_t *instance = get_card_instance(player, card);
			pump_until_eot_merge_previous(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}

		get_card_instance(player, card)->number_of_targets = 0;
	}

	return 0;
}

int card_impeccable_timing(int player, int card, event_t event){
/*
Impeccable Timing |1|W	0x200f151
Instant
Impeccable Timing deals 3 damage to target attacking or blocking creature.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td)){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target attacking or blocking creature.", 1, NULL);
}

// Inspired Charge: reprint M11

int card_master_trinketeer(int player, int card, event_t event){
/*
Master Trinketeer |2|W	0x200f156
Creature - Dwarf Artificer
Servos and Thopters you control get +1/+1.
|3|W: Create a 1/1 colorless Servo artifact creature token.
3/2
*/
	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_SERVO) ||
				has_subtype(affected_card_controller, affected_card, SUBTYPE_THOPTER) )
			{
				event_result++;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		make_servo(instance->parent_controller, instance->parent_card, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(3, 1), 0, NULL, NULL);
}

int card_ninth_bridge_patrol(int player, int card, event_t event){
/*
Ninth Bridge Patrol |1|W	0x200f15b
Creature - Dwarf Soldier
Whenever another creature you control leaves the battlefield, put a +1/+1 counter on Ninth Bridge Patrol.
1/1
*/
	if( trigger_condition == TRIGGER_LEAVE_PLAY && in_play(player, card) && ! is_humiliated(player, card) && affect_me(player, card)){
		if( affect_me(player, card) && reason_for_trigger_controller == player ){
			if( trigger_cause_controller == player && is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) &&
				    ! ( trigger_cause == card) )
			{
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						add_1_1_counter(player, card);
				}
			}
		}
	}

	return 0;
}

// Pressure Point: reprint FRF

/*
Propeller Pioneer |3|W	0x200f147 (Glint-Sleeve Artisan)
Creature - Human Artificer
Flying
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
2/1
*/

/*
Refurbish |3|W	0x2003293 (Argivian Restoration)
Sorcery
Return target artifact card from your graveyard to the battlefield.
*/

int card_revoke_privileges(int player, int card, event_t event){
/*
Revoke Privileges |2|W	0x200f160
Enchantment - Aura
Enchant creature
Enchanted creature can't attack, block, or crew Vehicles.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		int p = get_card_instance(player, card)->damage_target_player;
		int c = get_card_instance(player, card)->damage_target_card;
		if( event == EVENT_CANT_CREW && affect_me(p, c) ){
			return 1;
		}
	}

	return card_pacifism(player, card, event);
}

int card_servo_exhibition(int player, int card, event_t event){
/*
Servo Exhibition |1|W	0x200f165
Sorcery
Create two 1/1 colorless Servo artifact creature tokens.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		make_servo(player, card, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/*
Skyswirl Harrier |4|W	0x401000 (vanilla)
Creature - Bird
Flying
3/4
*/

int card_skywhalers_shot(int player, int card, event_t event){
/*
Skywhaler's Shot |2|W	0x200f16a
Instant
Destroy target creature with power 3 or greater. Scry 1.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 3 | TARGET_PT_GREATER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with power 3 or greater.", 1, NULL);
}


/*
Tasseled Dromedary |W	0x401000 (vanilla)
Creature - Camel
0/4
*/

int card_thriving_ibex(int player, int card, event_t event){
/*
Thriving Ibex |3|W	0x200f16f
Creature - Goat
When Thriving Ibex enters the battlefield, you get |E|E (two energy counters).
Whenever Thriving Ibex attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
2/4
*/
/*
Scrapper Champion	|3|R
Creature - Human Artificer 2/2
Double strike (This creature deals both first-strike and regular combat damage.)
When ~ enters the battlefield, you get |E|E (two energy counters).
Whenever ~ attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
*/

	etb_energy(player, card, event, 2);

	if( attack_trigger_with_energy(player, card, event, 2) ){
		remove_energy(player, 2);
		add_1_1_counter(player, card);
	}

	return 0;
}

int card_toolcraft_exemplar(int player, int card, event_t event){
/*
Toolcraft Exemplar |W	0X200f174
Creature - Dwarf Artificer
At the beginning of combat on your turn, if you control an artifact, Toolcraft Exemplar gets +2/+1 until end of turn.
If you control three or more artifacts, it also gains first strike until end of turn.
1/1
*/
	if( beginning_of_combat(player, card, event, player, card) ){
		int amount = count_subtype(player, TYPE_ARTIFACT, -1);
		if( amount ){
		alternate_legacy_text(1, player, pump_ability_until_eot(player, card, player, card, 2, 1, 0, 0));
        }
		if (amount >= 3){
		alternate_legacy_text(2, player, pump_ability_until_eot(player, card, player, card, 0, 0, (amount >= 3 ? KEYWORD_FIRST_STRIKE : 0), 0));
		   }
	    }
	return 0;
}

/*
Trusty Companion |1|W	0x0000000
Creature - Hyena
Vigilance
Trusty Companion can't attack alone.
3/2
*/

int card_visionary_augmenter(int player, int card, event_t event){
/*
Visionary Augmenter |2|W|W	0x200f179
Creature - Dwarf Artificer
Fabricate 2 (When this creature enters the battlefield, put two +1/+1 counters on it or create
			two 1/1 colorless Servo artifact creature tokens.)
2/1
*/
	etb_fabricate(player, card, event, 2, count_subtype(player, TYPE_CREATURE, -1)*5, 20);
	return 0;
}

int card_wispweaver_angel(int player, int card, event_t event){
/*
Wispweaver Angel |4|W|W	0x200f17e
Creature - Angel
Flying
When Wispweaver Angel enters the battlefield, you may exile another target creature you control,
then return that card to the battlefield under its owner's control.
4/4
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && new_pick_target(&td, "Select another target creature you control.", 0, GS_LITERAL_PROMPT) ){
			blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}


// Blue

static const char* is_creature_or_vehicle(int who_chooses, int player, int card)
{
	if( is_what(player, card, TYPE_CREATURE) ){
		return NULL;
	}
	if( has_subtype(player, card, SUBTYPE_VEHICLE) ){
		return NULL;
	}
	return "must be a creature or a Vehicle";
}

int card_aether_meltdown(int player, int card, event_t event){
/*
Aether Meltdown |1|U	0x200f183
Enchantment - Aura
Flash (You may cast this spell any time you could cast an instant.)
Enchant creature or Vehicle
When Aether Meltdown enters the battlefield, you get |E|E (two energy counters).
Enchanted permanent gets -4/-0.
*/
	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		int p = get_card_instance(player, card)->damage_target_player;
		int c = get_card_instance(player, card)->damage_target_card;
		if( event == EVENT_POWER && affect_me(p, c) ){
			event_result -= 4;
		}
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			if( is_creature_or_vehicle(-1, p, c) != NULL ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	etb_energy(player, card, event, 2);

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)is_creature_or_vehicle;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature or Vehicle.");
}

int card_aether_theorist(int player, int card, event_t event){
/*
Aether Theorist |1|U	0x200f188
Creature - Vedalken Rogue
When Aether Theorist enters the battlefield, you get |E|E|E (three energy counters).
|T, Pay |E: Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.)
1/3
*/
	etb_energy(player, card, event, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		scry(get_card_instance(player, card)->parent_controller, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, (GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1)), NULL, NULL);
}

int card_aether_tradewinds(int player, int card, event_t event){
/*
Aether Tradewinds |2|U	0x200f18d
Instant
Return target permanent you control and target permanent you don't control to their owners' hands.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_PERMANENT);
	td2.allowed_controller = td2.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) && can_target(&td2) ? 1 : 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target permanent you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target permanent you don't control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}
		if( validate_target(player, card, &td2, 1) ){
			action_on_target(player, card, 1, ACT_BOUNCE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}


int card_aethersquall_ancient(int player, int card, event_t event){
/*
Aethersquall Ancient |5|U|U	0x200f192
Creature - Leviathan
Flying
At the beginning of your upkeep, you get |E|E|E (three energy counters).
Pay |E|E|E|E|E|E|E|E: Return all other creatures to their owners' hands. Activate this ability only any time you could cast a sorcery.
6/6
*/
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_energy(player, 3);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		APNAP(p, {new_manipulate_all(instance->parent_controller, instance->parent_card, p, &this_test, ACT_BOUNCE);};);

	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, (GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 8)), NULL, NULL);
}

int card_ceremonious_rejection(int player, int card, event_t event){
/*
Ceremonious Rejection |U	0x200f197
Instant
Counter target colorless spell.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_ANY_COLORED);

	return counterspell(player, card, event, &td, 0);
}

static const char* ai_helper_function_for_confiscation_coup(int who_chooses, int player, int card)
{
	if( get_cmc(player, card) <= ENERGY_COUNTERS(player)+4 ){
		return NULL;
	}
	return "AI will pick only permanents with CMC <= ENERGY_COUNTERS()+4";
}

int card_confiscation_coup(int player, int card, event_t event){
/*
Confiscation Coup |3|U|U	0x200f19c
Sorcery
Choose target artifact or creature. You get |E|E|E|E (four energy counters), then you may pay an amount of |E equal
to that permanent's converted mana cost. If you do, gain control of it.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
	if( player == AI ){
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = (int32_t)ai_helper_function_for_confiscation_coup;
	}

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_energy(player, 4);
			int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
			int amount = MIN(ENERGY_COUNTERS(player), cmc);
			if( player == AI && amount < cmc ){
				amount = 0;
			}
			if( player == HUMAN ){
				amount = choose_a_number(player, "How much Energy you'll spend?", amount);
			}
			remove_energy(player, amount);
			if( amount <= cmc ){
				gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or creature.", 1, NULL);
}

/*
Curio Vendor |1|U	0x401000 (vanilla)
Creature - Vedalken
2/1
*/

int card_disappearing_act(int player, int card, event_t event){
/*
Disappearing Act |1|U|U	0x200f1a1
Instant
As an additional cost to cast Disappearing Act, return a permanent you control to its owner's hand.
Counter target spell.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	if( event == EVENT_CAN_CAST && count_subtype(player, TYPE_PERMANENT, -1)){
			return counterspell(player, card, event, NULL, 0);
		}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
	    target_definition_t td;
	    default_target_definition(player, card, &td, TYPE_PERMANENT);
	    td.allowed_controller = td.preferred_controller = player;
	    td.illegal_abilities = 0;

		get_card_instance(player, card)->number_of_targets = 0;
		if( new_pick_target(&td, "Select a permanent you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
			get_card_instance(player, card)->number_of_targets = 0;
			return counterspell(player, card, event, NULL, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_dramatic_reversal(int player, int card, event_t event){
/*
Dramatic Reversal |1|U	0x200f1a6
Instant
Untap all nonland permanents you control.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_ENCHANTMENT | TARGET_TYPE_PLANESWALKER);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_era_of_innovation(int player, int card, event_t event){
/*
Era of Innovation |1|U	0x200f1ab
Enchantment
Whenever an artifact or Artificer enters the battlefield under your control, you may pay |1. If you do, you get |E|E (two energy counters).
Pay |E|E|E|E|E|E, Sacrifice Era of Innovation: Draw three cards.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		if( (is_what(trigger_cause_controller, trigger_cause, TYPE_ARTIFACT) ||
			has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_ARTIFICER)) &&
			trigger_cause_controller == player && has_mana(player, COLOR_ANY, 1) )
		{
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test);
			while( result && has_mana(player, COLOR_ANY, 1) ){
					int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
										"Get 2 Energy counters", 1, 5,
										"Decline", 1, 1);
					if( choice == 1 ){
						charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
						if( spell_fizzled != 1 ){
							add_energy(player, 2);
						}
					}
					result--;
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, (GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 6)), NULL, NULL);
}

int card_experimental_aviator(int player, int card, event_t event){
/*
Experimental Aviator |3|U|U	0x200f1b0
Creature - Human Artificer
Flying
When Experimental Aviator enters the battlefield, create two 1/1 colorless Thopter artifact creature tokens with flying.
0/3
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		make_thopter(player, card, 2);
	}

	return 0;
}

int card_failed_inspection(int player, int card, event_t event){
/*
Failed Inspection |2|U|U	0x200f1b5
Instant
Counter target spell. Draw a card, then discard a card.
*/
	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CAN_COUNTER ){
		return counterspell(player, card, event, NULL, 0);
	}

	if (event == EVENT_RESOLVE_SPELL) {
		card_instance_t* instance = get_card_instance(player, card);
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
			discard(player, 0, player);
		}
		instance->number_of_targets = 0;
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}


int card_gearseeker_serpent(int player, int card, event_t event){
/*
Gearseeker Serpent |5|U|U	0x200f1ba
Creature - Serpent
Gearseeker Serpent costs |1 less to cast for each artifact you control.
|5|U: Gearseeker Serpent can't be blocked this turn.
5/6
*/
	affinity(player, card, event, TYPE_ARTIFACT, 0);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(5, 1), 0, NULL, NULL);
}

int card_glimmer_of_genius(int player, int card, event_t event ){
/*
Glimmer of Genius |3|U	0x200f1bf
Instant
Scry 2, then draw two cards. You get |E|E (two energy counters).
*/

	if( event == EVENT_RESOLVE_SPELL ){
		scry(player, 2);
		draw_cards(player, 2);
		add_energy(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_glint_nest_crane(int player, int card, event_t event){
/*
Glint-Nest Crane |1|U	0x200f1c4
Creature - Bird
Flying
When Glint-Nest Crane enters the battlefield, look at the top four cards of your library.
You may reveal an artifact card from among them and put it into your hand. Put the rest on the bottom of your library in any order.
1/3
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = MIN(4, count_deck(player));
		if( amount ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
			test.create_minideck = amount;
			test.no_shuffle = 1;
			if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test) != -1 ){
				amount--;
			}
			if( amount ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
	}

	return 0;
}

int card_hightide_hermit(int player, int card, event_t event){
/*
Hightide Hermit |4|U	0x200f1c9
Creature - Crab
Defender
When Hightide Hermit enters the battlefield, you get |E|E|E|E (four energy counters).
Pay |E|E: Hightide Hermit can attack this turn as though it didn't have defender.
4/4
*/

	etb_energy(player, card, event, 4);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		create_targetted_legacy_effect(player, instance->parent_card, &effect_defender_can_attack_until_eot, player, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, (GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2)), NULL, NULL);
}

int card_insidious_will(int player, int card, event_t event){
/*
Insidious Will |2|U|U	0x200f1ce
Instant
Choose one -
* Counter target spell.
* You may choose new targets for target spell.
* Copy target instant or sorcery spell. You may choose new targets for the copy.
*/
	if( event == EVENT_CAN_CAST || event == EVENT_CAN_COUNTER ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_COPY
	};

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priority_counter = 5;
			int priority_copy = 1;
			if( is_what(card_on_stack_controller, card_on_stack, TYPE_SPELL) ){
				card_instance_t* spell = get_card_instance(card_on_stack_controller, card_on_stack);
				int i;
				for(i=0; i<spell->number_of_targets; i++ ){
					if( spell->targets[i].player == 1-player ){
						priority_copy = 10;
						break;
					}
				}
			}

			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								"Counter", 1, priority_counter,
								"Copy", is_what(card_on_stack_controller, card_on_stack, TYPE_SPELL), priority_copy);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		return counterspell(player, card, event, NULL, 0);
	}

	if (event == EVENT_RESOLVE_SPELL) {
		card_instance_t* instance = get_card_instance(player, card);
		if( counterspell_validate(player, card, NULL, 0) ){
			if( instance->info_slot == CHOICE_COUNTER ){
				real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			if( instance->info_slot == CHOICE_COPY ){
				copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
			}
		}
		instance->number_of_targets = 0;
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_janjeet_sentry(int player, int card, event_t event){
/*
Janjeet Sentry |2|U	0x200f1d3
Creature - Vedalken Soldier
When Janjeet Sentry enters the battlefield, you get |E|E (two energy counters).
|T, Pay |E|E: You may tap or untap target artifact or creature.
2/3
*/

	etb_energy(player, card, event, 2);

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_ACTIVATE && player == AI && cancel != 1 ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0,
									(GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2)), &td, "Select target artifact or creature.");
		if( spell_fizzled != 1 ){
			ai_modifier_twiddle(player, card, 0);
		}
		return 0;
	}
	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		twiddle(player, card, 0);
	}


	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0,
									(GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2)), &td, "Select target artifact or creature.");
}

/*
Long-Finned Skywhale |2|U|U	0x2004049 (Cloud Sprite)
Creature - Whale
Flying
Long-Finned Skywhale can block only creatures with flying.
4/3
*/

int card_malfunction(int player, int card, event_t event){
/*
Malfunction |3|U	0x200f1d8
Enchantment - Aura
Enchant artifact or creature
When Malfunction enters the battlefield, tap enchanted permanent.
Enchanted permanent doesn't untap during its controller's untap step.
*/
	if( comes_into_play(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			tap_card( instance->damage_target_player, instance->damage_target_card );
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	if( ! IS_AURA_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
    td.preferred_controller = 1-player;

	return disabling_targeted_aura(player, card, event, &td, "Select target artifact or creature.", 0);
}

int card_metallurgic_summonings(int player, int card, event_t event){
/*
Metallurgic Summonings |3|U|U	0x200f1dd
Enchantment
Whenever you cast an instant or sorcery spell, create an X/X colorless Construct artifact creature token,
where X is that spell's converted mana cost.
|3|U|U, Exile Metallurgic Summonings: Return all instant and sorcery cards from your graveyard to your hand.
Activate this ability only if you control six or more artifacts.
*/
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_CONSTRUCT, &token);
			token.pow = token.tou = get_cmc(trigger_cause_controller, trigger_cause);
			generate_token(&token);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_subtype(player, TYPE_ARTIFACT, -1) < 6 ){
			return 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int count = count_graveyard(player)-1;
		while( count > -1 ){
				if( is_what(-1, get_grave(player)[count], TYPE_SPELL) ){
					from_grave_to_hand(player, count, TUTOR_HAND);
				}
				count--;
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XU(3, 2), 0, NULL, NULL);
}

int card_minister_of_inquiries(int player, int card, event_t event){
/*
Minister of Inquiries |U	0x200f1e2
Creature - Vedalken Advisor
When Minister of Inquiries enters the battlefield, you get |E|E (two energy counters).
|T, Pay |E: Target player puts the top three cards of his or her library into his or her graveyard.
1/2
*/

	etb_energy(player, card, event, 2);

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance( player, card);
		mill(instance->targets[0].player, 3);
	}


	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
									(GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1)), &td, "TARGET_PLAYER");
}


/*
Nimble Innovator |3|U	0x2003c4d (Carven Caryatid)
Creature - Vedalken Artificer
When Nimble Innovator enters the battlefield, draw a card.
2/2
*/

int card_padeem_consul_of_innovation(int player, int card, event_t event){
/*
Padeem, Consul of Innovation |3|U	0x200f1e7
Legendary Creature - Vedalken Artificer
Artifacts you control have hexproof.
At the beginning of your upkeep, if you control the artifact with the highest converted mana cost or tied for the highest converted mana cost, draw a card.
1/4
*/

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) )
		{
			hexproof(affected_card_controller, affected_card, event);
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int my_cmc = check_battlefield_for_special_card(player, card, player, CBFSC_GET_MAX_CMC, &test);
		int ai_cmc = check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_MAX_CMC, &test);
		if( my_cmc >= ai_cmc ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

static const char* nonland_nontoken_permanent(int who_chooses, int player, int card){

	if( is_token(player, card) ){
		return "can't target a token permanent";
	}

	if( is_what(player, card, TYPE_LAND) ){
		return "can't target a land";
	}

	return NULL;
}

int card_paradoxical_outcome(int player, int card, event_t event){
/*
Paradoxical Outcome |3|U	0x200f1ec
Instant
Return any number of target nonland, nontoken permanents you control to their owners' hands.
Draw a card for each card returned to your hand this way.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = td.preferred_controller = player;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)nonland_nontoken_permanent;
	td.allow_cancel = 2; // "Done" button

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int t = 0;
		while( can_target(&td) && t < 10 ){
				if( new_pick_target(&td, "Select target nonland, nontoken permanent.", t, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[t].player, instance->targets[t].card, 1);
					t++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<t; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int cards_to_draw = 0;
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				if( get_owner(instance->targets[i].player, instance->targets[i].card) == player ){
					cards_to_draw++;
				}
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		draw_cards(player, cards_to_draw);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_revolutionary_rebuff(int player, int card, event_t event){
/*
Revolutionary Rebuff |1|U	0x200f1f1
Instant
Counter target nonartifact spell unless its controller pays |2.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_ARTIFACT;

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CAN_COUNTER ){
		return counterspell(player, card, event, &td, 0);
	}

	if (event == EVENT_RESOLVE_SPELL) {
		counterspell_resolve_unless_pay_x(player, card, &td, 0, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_saheelis_artistry(int player, int card, event_t event){
/*
Saheeli's Artistry |4|U|U	0x200f1f6
Sorcery
Choose one or both -
* Create a token that's a copy of target artifact.
* Create a token that's a copy of target creature, except it's an artifact in addition to its other types.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_COPY_ARTIFACT = 1,
		CHOICE_COPY_CREATURE,
		CHOICE_BOTH
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	if( player == AI ){
		td.illegal_type = TYPE_CREATURE;
	}
	td.preferred_controller = ANYBODY;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	if( player == AI ){
		td2.illegal_type = TYPE_ARTIFACT;
	}
	td2.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_target(&td) || can_target(&td2) ? 1 : 0;
		}
	}

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Copy artifact", can_target(&td), 5,
								"Copy creature", can_target(&td2), 10,
								"Do both", can_target(&td) && can_target(&td2), 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		int base_target = 0;
		if( instance->info_slot == CHOICE_COPY_ARTIFACT || instance->info_slot == CHOICE_BOTH ){
			if( new_pick_target(&td, "TARGET_ARTIFACT", 0, 1) ){
				base_target++;
			}
			else{
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_COPY_CREATURE || instance->info_slot == CHOICE_BOTH ){
			new_pick_target(&td2, "TARGET_CREATURE", base_target, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;

		token_generation_t token_artifact;
		default_token_definition(player, card, CARD_ID_SWAMP, &token_artifact);

		token_generation_t token_creature;
		default_token_definition(player, card, CARD_ID_SWAMP, &token_creature);
		if( choice == CHOICE_BOTH ){
			if( validate_target(player, card, &td, 0) ){
				copy_token_definition(player, card, &token_artifact, instance->targets[0].player, instance->targets[0].card);
			}
			if( validate_target(player, card, &td2, 1) ){
				copy_token_definition(player, card, &token_creature, instance->targets[1].player, instance->targets[1].card);
			}
		}
		if( choice == CHOICE_COPY_ARTIFACT ){
			if( validate_target(player, card, &td, 0) ){
				copy_token_definition(player, card, &token_artifact, instance->targets[0].player, instance->targets[0].card);
			}
		}
		if( choice == CHOICE_COPY_CREATURE ){
			if( validate_target(player, card, &td2, 0) ){
				copy_token_definition(player, card, &token_creature, instance->targets[0].player, instance->targets[0].card);
			}
		}
		if( token_artifact.id != CARD_ID_SWAMP ){
			generate_token(&token_artifact);
		}
		if( token_creature.id != CARD_ID_SWAMP ){
			token_creature.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
			generate_token(&token_creature);
		}
		instance->number_of_targets = 0;
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_select_for_inspection(int player, int card, event_t event){
/*
Select for Inspection |U	0x200f1fb
Instant
Return target tapped creature to its owner's hand.
Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.)
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target tapped creature.", 1, NULL);
}


int card_shrewd_negotiation(int player, int card, event_t event){
/*
Shrewd Negotiation |4|U	0x200f200
Sorcery
Exchange control of target artifact you control and target artifact or creature you don't control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_ARTIFACT|TYPE_CREATURE);
	td2.allowed_controller = td.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td) && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target artifact you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target artifact or creature controlled by opponent.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
													instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_tezzerets_ambition(int player, int card, event_t event ){
/*
Tezzeret's Ambition |3|U|U	0x200f205
Sorcery
Draw three cards. If you control no artifacts, discard a card.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);
		if( ! count_subtype(player, TYPE_ARTIFACT, -1) ){
			discard(player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


/*
Thriving Turtle |U	0x200f16f (Thriving Ibex)
Creature - Turtle
When Thriving Turtle enters the battlefield, you get |E|E (two energy counters).
Whenever Thriving Turtle attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
0/3
*/

int card_torrential_gearhulk(int player, int card, event_t event){
/*
Torrential Gearhulk |4|U|U	0x200f20a
Artifact Creature - Construct
Flash
When Torrential Gearhulk enters the battlefield, you may cast target instant card from your graveyard without paying its mana cost.
If that card would be put into your graveyard this turn, exile it instead.
5/6
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_INSTANT | TYPE_INTERRUPT, "Select an instant card to play for free.");
		if( new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ){
			int grave_backup[2][count_graveyard(player)];
			int gbc = 0;
			int i;
			for(i=0; i<count_graveyard(player); i++){
				if( is_what(-1, get_grave(player)[i], TYPE_INSTANT | TYPE_INTERRUPT) ){
					int iid = get_grave(player)[i];
					if( can_legally_play_iid(player, iid) ){
						grave_backup[0][gbc] = iid;
						grave_backup[1][gbc] = i;
						gbc++;
					}
				}
			}
			if( gbc ){
				int selected = select_card_from_zone(player, player, grave_backup[0], gbc, 0, AI_MAX_CMC, -1, &test);
				if( selected != -1 ){
					play_card_in_grave_for_free_and_exile_it(player, player, grave_backup[1][selected]);
				}
			}
		}
	}

	return flash(player, card, event);
}


/*
Vedalken Blademaster |2|U	0x200ce74 (Jeskai Windscout)
Creature - Vedalken Soldier
Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)
2/3
*/

int card_weldfast_wingsmith(int player, int card, event_t event){
/*
Weldfast Wingsmith |3|U	0x200f20f
Creature - Human Artificer
Whenever an artifact enters the battlefield under your control, Weldfast Wingsmith gains flying until end of turn.
3/3
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			pump_ability_until_eot(player, card, player, card, 0, 0, KEYWORD_FLYING, 0);
		}
	}

	return 0;
}

// Wind Drake: reprint TMP

// Black

int card_aetherborn_marauder(int player, int card, event_t event){
/*
Aetherborn Marauder |3|B	0x200f214
Creature - Aetherborn Rogue
Flying, lifelink
When Aetherborn Marauder enters the battlefield, move any number of +1/+1 counters from other permanents you control
onto Aetherborn Marauder.
2/2
*/
    lifelink(player, card, event);
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		/* Carefully not moving counters that are added as a result of removing counters from other creatures
		 * from some horrid combination of death triggers */
		int num_counters[151] = {0};
		int c;
		for(c = 0; c < active_cards_count[player]; ++c){
			if( in_play(player, c) && is_what(player, c, TYPE_PERMANENT) ){
				num_counters[c] = count_counters(player, c, COUNTER_P1_P1);
			}
		}

		num_counters[card] = 0;

		for(c = 0; c < active_cards_count[player]; ++c){
			if( num_counters[c] ){
				int amount = num_counters[c];
				/* AI will try to preserve its own creatures, so it won't remove counters if that will reduce the toughness
				 * of the creature to less than 1 */
				if( is_what(player, c, TYPE_CREATURE) ){
					int t = get_toughness(player, c)-get_card_instance(player, c)->damage_on_card;
					while( amount && (t - amount < 1) ){
							amount--;
					}
				}
				if( player != AI ){
					int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_SMALLCARD(player, c),
										"Move +1/+1 counters from this", 1, 1,
										"Decline", 1, 1);
					if( choice == 1 ){
						amount = choose_a_number(player, "How many +1/+1 counters you'll move?", amount);
					}
				}
				if( amount > 0 && amount <= num_counters[c] ){
					move_counters(player, card, player, c, COUNTER_P1_P1, amount);
				}
			}
		}
	}

	return 0;
}

/*
Ambitious Aetherborn |4|B	0x200f147 (Glint-Sleeve Artisan)
Creature - Aetherborn Artificer
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
4/3
*/

static int demon_of_dark_schemes_legacy(int player, int card, event_t event){

	//In case something is copying Demon of Dark Schemes and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_DEMON_OF_DARK_SCHEMES ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						return 0;
					}
				}
				count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			add_energy(player, amount);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	return 0;
}

int card_demon_of_dark_schemes(int player, int card, event_t event){
/*
Demon of Dark Schemes |3|B|B|B	0x200f219
Creature - Demon
Flying
When Demon of Dark Schemes enters the battlefield, all other creatures get -2/-2 until end of turn.
Whenever another creature dies, you get |E (an energy counter).
|2|B, Pay |E|E|E|E: Put target creature card from a graveyard onto the battlefield under your control tapped.
5/5
*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)demon_of_dark_schemes_legacy ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &demon_of_dark_schemes_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;

		APNAP(p, {pump_creatures_until_eot_merge_pt(player, card, p, -2, -2, &this_test);};);
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			add_energy(player, amount);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 1), GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 4), NULL, NULL) ){
			int i;
			for(i=0; i<2; i++){
				if( has_dead_creature(i) && ! graveyard_has_shroud(i) ){
					return 1;
				}
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(2, 1)) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			if( select_target_from_either_grave(player, card, 0, AI_MAX_CMC, AI_MAX_CMC, &this_test, 0, 1) != -1 ){
				remove_energy(player, 4);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent( instance->parent_controller, instance->parent_card, instance->targets[0].player, selected, REANIMATE_TAP);
		}
	}

	return 0;
}

int card_dhund_operative(int player, int card, event_t event){
/*
Dhund Operative |1|B	0x200f21e
Creature - Human Rogue
As long as you control an artifact, Dhund Operative gets +1/+0 and has deathtouch.
(Any amount of damage it deals to a creature is enough to destroy it.)
2/2
*/
	boost_if_you_control_an_artifact(player, card, event, 1, 0, 0, SP_KEYWORD_DEATHTOUCH);

	return 0;
}

// Diabolic Tutor: reprint ODY

static const char* ai_helper_function_for_die_young(int who_chooses, int player, int card)
{
	if( get_toughness(player, card)-get_card_instance(player, card)->damage_on_card <= ENERGY_COUNTERS(player)+2 ){
		return NULL;
	}
	return "AI will pick only permanents with TOU <= ENERGY_COUNTERS()+2";
}

int card_die_young(int player, int card, event_t event){
/*
Die Young |1|B	0x200f223
Sorcery
Choose target creature. You get |E|E (two energy counters), then you may pay any amount of |E.
The creature gets -1/-1 until end of turn for each |E paid this way.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)ai_helper_function_for_die_young;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_energy(player, 2);
			int tou = get_toughness(player, card)-get_card_instance(player, card)->damage_on_card;
			int amount = MIN(ENERGY_COUNTERS(player), tou);
			if( player == AI && amount < tou ){
				amount = 0;
			}
			if( player == HUMAN ){
				amount = choose_a_number(player, "How much Energy you'll spend?", amount);
			}
			remove_energy(player, amount);
			if( amount ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_dukhara_scavenger(int player, int card, event_t event){
/*
Dukhara Scavenger |5|B	0x200f228
Creature - Crocodile
When Dukhara Scavenger enters the battlefield, you may put target artifact or creature card from your graveyard on top of your library.
4/6
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT, "Select target artifact or creature card.");
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

int card_eliminate_the_competition(int player, int card, event_t event){
/*
Eliminate the Competition |4|B	0x200f22d
Sorcery
As an additional cost to cast Eliminate the Competition, sacrifice X creatures.
Destroy X target creatures.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event); //You could sacrifice 0 creatures and thus choosing 0 targets
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int max_sac = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) &&
										can_sacrifice_this_as_cost(p, c) )
									{
										max_sac++;
									}
								};
		);

		int t = 0;
		while( t < max_sac ){
				if( new_pick_target(&td, "TARGET_CREATURE", t, 0) ){
					state_untargettable(instance->targets[t].player, instance->targets[t].card, 1);
					t++;
					if( t >= 3 && player == AI && internal_rand(50)+1 >= 25 ){
						break;
					}
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<t; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}

		int sac[151];
		int sc = 0;
		while( sc < t ){
				sac[sc] = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
				if( sac[sc] > -1 ){
					sc++;
				}
				else{
					break;
				}
		}
		for(i=0; i<sc; i++){
			kill_card(BYTE2(sac[i]), BYTE3(sac[i]), KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				action_on_target(player, card, i, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_embraal_bruiser(int player, int card, event_t event){
/*
Embraal Bruiser |1|B	0x200f232
Creature - Human Warrior
Embraal Bruiser enters the battlefield tapped.
Embraal Bruiser has menace as long as you control an artifact.
3/1
*/
	comes_into_play_tapped(player, card, event);

	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_ARTIFACT);

	if( in_play(player, card) && ! is_humiliated(player, card) &&
                                check_battlefield_for_special_card(player, card, player, 0, &this_test) ){

		menace(player, card, event);
		}

	return 0;
}

int card_essence_extraction(int player, int card, event_t event){
/*
Essence Extraction |1|B|B	0x200f237
Instant
Essence Extraction deals 3 damage to target creature and you gain 3 life.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_fortuitous_find(int player, int card, event_t event){
/*
Fortuitous Find |2|B	0x200f23c
Sorcery
Choose one or both -
* Return target artifact card from your graveyard to your hand.
* Return target creature card from your graveyard to your hand.
*/

  return spell_return_one_or_two_cards_from_gy_to_hand(player, card, event, TYPE_ARTIFACT, TYPE_CREATURE);
}

int card_foundry_screecher(int player, int card, event_t event){
/*
Foundry Screecher |2|B	0x200f241
Creature - Bat
Flying
Foundry Screecher gets +1/+0 as long as you control an artifact.
2/1
*/
	boost_if_you_control_an_artifact(player, card, event, 1, 0, 0, 0);

	return 0;
}

int card_fretwork_colony(int player, int card, event_t event){
/*
Fretwork Colony |1|B	0x200f246
Creature - Insect
Fretwork Colony can't block.
At the beginning of your upkeep, put a +1/+1 counter on Fretwork Colony and you lose 1 life.
1/1
*/
	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_1_1_counter(player, card);
		lose_life(player, 1);
	}

	cannot_block(player, card, event);

	return 0;
}

int card_gonti_lord_of_luxury(int player, int card, event_t event){
/*
Gonti, Lord of Luxury |2|B|B	0x200f24b
Legendary Creature - Aetherborn Rogue
Deathtouch
When Gonti, Lord of Luxury enters the battlefield, look at the top four cards of target opponent's library,
exile one of them face down, then put the rest on the bottom of that library in a random order.
For as long as that card remains exiled, you may look at it, you may cast it, and you may spend mana as though it were mana of
any type to cast it.
2/3
*/
	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int amount = MIN(4, count_deck(1-player));
			if( amount ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
				if( player == AI ){
					test.type = TYPE_LAND;
					test.type_flag = DOESNT_MATCH;
				}
				test.create_minideck = amount;
				test.no_shuffle = 1;
				int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, (player == HUMAN ? 1 : 0), AI_MAX_VALUE, -1, &test);
				if( selected == -1 ){
					new_default_test_definition(&test, TYPE_ANY, "Select a card to exile.");
					selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 1, AI_MAX_VALUE, -1, &test);
				}
				int iid = deck_ptr[1-player][selected];
				rfg_card_in_deck(1-player, selected);
				create_may_play_card_from_exile_effect(player, card, 1-player, cards_data[iid].id, MPCFE_FOR_CMC);
				amount--;
				if( amount ){
					put_top_x_on_bottom_in_random_order(1-player, amount);
				}
			}
		}
	}

	deathtouch(player, card, event);

	return 0;
}

int card_harsh_scrutiny(int player, int card, event_t event){
/*
Harsh Scrutiny |B	0x200f250
Sorcery
Target opponent reveals his or her hand. You choose a creature card from it. That player discards that card. Scry 1.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			this_test.zone = TARGET_ZONE_HAND;

			int can_cancel = check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_CHECK_ONLY, &this_test);
			int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_HAND, can_cancel, AI_MAX_VALUE, -1, &this_test);
			if( selected != -1 ){
				new_discard_card(instance->targets[0].player, selected, 0, player);
			}
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_PLAYER", 1, NULL);
}

int card_lawless_broker(int player, int card, event_t event ){
/*
Lawless Broker |2|B	0x200f255
Creature - Aetherborn Rogue
When Lawless Broker dies, put a +1/+1 counter on target creature you control.
3/2
*/

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return 0;
}

int card_live_fast(int player, int card, event_t event ){
/*
Live Fast |2|B	0x200f25a
Sorcery
You draw two cards, lose 2 life, and get |E|E (two energy counters).
*/

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		lose_life(player, 2);
		add_energy(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}


int card_lost_legacy(int player, int card, event_t event){
/*
Lost Legacy |1|B|B	0x200f25f
Sorcery
Name a nonartifact, nonland card. Search target player's graveyard, hand, and library for any number of cards
with that name and exile them. That player shuffles his or her library, then draws a card for each card exiled from hand this way.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int csvid = -1;
			int amount = 0;
			if( player == AI ){
				int turns = 0;
				int count = internal_rand(count_deck(instance->targets[0].player));
				while( turns < 2 ){
						int iid = deck_ptr[instance->targets[0].player][count];
						if( iid != -1 ){
							if( ! is_what(-1, iid, TYPE_ARTIFACT | TYPE_LAND) ){
								csvid = cards_data[iid].id;
								break;
							}
							count++;
						}
						else{
							count = 0;
							turns++;
						}
				}
			}
			else{
				csvid = card_from_list(player, 3, TYPE_LAND | TYPE_ARTIFACT, DOESNT_MATCH, 0, 0, 0, 0, 0, 0, -1, 0);
			}
			if( csvid > -1 ){
				amount = lobotomy_effect(player, instance->targets[0].player, csvid, LOBOTOMY_MUST_FIND_ALL_COPIES);
			}
			draw_cards(instance->targets[0].player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_make_obsolete(int player, int card, event_t event ){
/*
Make Obsolete |2|B	0x200f264
Instant
Creatures your opponents control get -1/-1 until end of turn.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, 1-player, -1, -1, -1, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int marionette_master_legacy(int player, int card, event_t event){

	//In case something is copying Demon of Dark Schemes and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MARIONETTE_MASTER ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						instance->targets[1].player = get_power(p, c);
					}
				}
				count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount && opponent_is_valid_target(player, card) ){
			lose_life(1-player, amount * get_card_instance(player, card)->targets[1].player);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	return 0;
}

int card_marionette_master(int player, int card, event_t event){
/*
Marionette Master |4|B|B	0x200f269
Creature - Human Artificer
Fabricate 3 (When this creature enters the battlefield, put three +1/+1 counters on it or create three 1/1 colorless Servo artifact creature tokens.)
Whenever an artifact you control is put into a graveyard from the battlefield, target opponent loses life equal to Marionette Master's power.
1/3
*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)marionette_master_legacy ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &marionette_master_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	etb_fabricate(player, card, event, 3, count_subtype(player, TYPE_ARTIFACT, -1)*5, 15);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && affect_me(player, card) && opponent_is_valid_target(player, card) ){
			int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
			if( amount ){
				lose_life(1-player, get_power(player, card)*amount);
			}
		}
	}

	return 0;
}


int card_maulfist_squad(int player, int card, event_t event){
/*
Maulfist Squad |3|B	0x200f26e
Creature - Human Artificer
Menace
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
3/1
*/
	menace(player, card, event);
	return card_glint_sleeve_artisan(player, card, event);
}

int card_midnight_oil(int player, int card, event_t event){
/*
Midnight Oil |2|B|B	0x200f273
Enchantment
Midnight Oil enters the battlefield with seven hour counters on it.
At the beginning of your draw step, draw an additional card and remove two hour counters from Midnight Oil.
Your maximum hand size is equal to the number of hour counters on Midnight Oil.
Whenever you discard a card, you lose 1 life.
*/

	enters_the_battlefield_with_counters(player, card, event, COUNTER_HOUR, 7);

	if( current_turn == player && event == EVENT_DRAW_PHASE && ! is_humiliated(player, card) ){
		event_result++;
		int amount = MIN(2, count_counters(player, card, COUNTER_HOUR));
		remove_counters(player, card, COUNTER_HOUR, amount);
	}

	if( current_turn == player && event == EVENT_MAX_HAND_SIZE && ! is_humiliated(player, card) ){
		event_result = count_counters(player, card, COUNTER_HOUR);
	}

	if( discard_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, 0) ){
		lose_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

// Mind Rot: reprint POR

int card_morbid_curiosity(int player, int card, event_t event){
/*
Morbid Curiosity |1|B|B	0x200f278
Sorcery
As an additional cost to cast Morbid Curiosity, sacrifice an artifact or creature.
Draw cards equal to the converted mana cost of the sacrificed permanent.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE | TYPE_ARTIFACT, "Select an artifact or creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_PERMANENT_AS_COST, NULL, NULL, 1, &test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->info_slot = get_cmc(BYTE2(sac), BYTE3(sac));
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	}


	if ( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, instance->info_slot);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_night_market_lookout(int player, int card, event_t event){
/*
Night Market Lookout |B	0x200f27d
Creature - Human Rogue
Whenever Night Market Lookout becomes tapped, each opponent loses 1 life and you gain 1 life.
1/1
*/
	if( event == EVENT_TAP_CARD && affect_me(player, card) && ! is_humiliated(player, card) ){
		lose_life(1-player, 1);
		gain_life(player, 1);
	}

	return 0;
}

int card_noxious_gearhulk(int player, int card, event_t event){
/*
Noxious Gearhulk |4|B|B	0x200f282
Artifact Creature - Construct
Menace
When Noxious Gearhulk enters the battlefield, you may destroy another target creature.
If a creature is destroyed this way, you gain life equal to its toughness.
5/4
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select another target creature.", 0, GS_LITERAL_PROMPT) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			action_on_target(player, card, 0, KILL_DESTROY);
			gain_life(player, amount);
		}

		instance->number_of_targets = 0;
	}

	menace(player, card, event);

	return 0;
}

/*
Ovalchase Daredevil |3|B		0x401000 (vanille, the real effect is in "static_graveyard_abilities")
Creature - Human Pilot
Whenever an artifact enters the battlefield under your control, you may return Ovalchase Daredevil from your graveyard to your hand.
4/2
*/

/*
Prakhata Club Security |3|B		0x401000 (vanille)
Creature - Aetherborn Warrior
3/4
*/

int card_rush_of_vitality(int player, int card, event_t event){
/*
Rush of Vitality |1|B	0x200f287
Instant
Target creature gets +1/+0 and gains lifelink and indestructible until end of turn. (Damage dealt by that creature also causes its controller to gain that much life, and it can't be destroyed by damage or effects that say "destroy.")
*/
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 0, 0, SP_KEYWORD_LIFELINK | SP_KEYWORD_INDESTRUCTIBLE);
}

int card_subtle_strike(int player, int card, event_t event){
/*
Subtle Strike |1|B	0x200f28c
Instant
Choose one or both -
* Target creature gets -1/-1 until end of turn.
* Put a +1/+1 counter on target creature.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_WEAKEN = 1,
		CHOICE_PUMP,
		CHOICE_BOTH
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			if( can_target(&td) || can_target(&td2) ){
				return 1;
			}
		}
	}

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"-1/-1 to target creature", can_target(&td), 10,
								"+1/+1 to target creature", can_target(&td2), 5,
								"Do both", can_target(&td) && can_target(&td2) ? 1 : 0, 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		int base_target = 0;
		if( instance->info_slot == CHOICE_WEAKEN || instance->info_slot == CHOICE_BOTH ){
			if( ! new_pick_target(&td, "TARGET_CREATURE", base_target, 1) ){
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_PUMP || instance->info_slot == CHOICE_BOTH ){
			new_pick_target(&td2, "TARGET_CREATURE", base_target, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;
		int base_target = 0;
		if( choice == CHOICE_BOTH || choice == CHOICE_WEAKEN ){
			if( validate_target(player, card, &td, base_target) ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -1);
			}
			base_target++;
		}
		if( choice == CHOICE_BOTH || choice == CHOICE_PUMP ){
			if( validate_target(player, card, &td2, base_target) ){
				add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_syndicate_trafficker(int player, int card, event_t event){
/*
Syndicate Trafficker |1|B	0x200f291
Creature - Aetherborn Rogue
|1, Sacrifice an artifact: Put a +1/+1 counter on Syndicate Trafficker. It gains indestructible until end of turn.
3/1
*/
	if( event != EVENT_RESOLVE_ACTIVATION ){
		return generic_husk(player, card, event, TYPE_ARTIFACT, 1, 1, 0, SP_KEYWORD_INDESTRUCTIBLE);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
	}

	return 0;
}

/*
Thriving Rats |1|B	0x200f16f (Thriving Ibex)
Creature - Rat
When Thriving Rats enters the battlefield, you get |E|E (two energy counters).
Whenever Thriving Rats attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
1/2
*/

int card_tidy_conclusion(int player, int card, event_t event){
/*
Tidy Conclusion |3|B|B	0x200f296
Instant
Destroy target creature. You gain 1 life for each artifact you control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, count_subtype(player, TYPE_ARTIFACT, -1));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_underhanded_designs(int player, int card, event_t event){
/*
Underhanded Designs |1|B	0x200f29b
Enchantment
Whenever an artifact enters the battlefield under your control, you may pay |1. If you do, each opponent loses 1 life and you gain 1 life.
|1|B, Sacrifice Underhanded Designs: Destroy target creature. Activate this ability only if you control two or more artifacts.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && has_mana(player, COLOR_ANY, 1) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test);
		while( result && has_mana(player, COLOR_ANY, 1) ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
									"Suck life", 1, 5,
									"Decline", 1, 1);
				if( choice == 1 ){
					charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						lose_life(1-player, 1);
						gain_life(player, 1);
					}
				}
				else{
					break;
				}
				result--;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_ACTIVATE && count_subtype(player, TYPE_ARTIFACT, -1) >= 2 ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XB(1, 1), 0,
										&td, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XB(1, 1), 0,
										&td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_weaponcraft_enthusiast(int player, int card, event_t event){
/*
Weaponcraft Enthusiast |2|B	0x200f2a0
Creature - Aetherborn Artificer
Fabricate 2 (When this creature enters the battlefield, put two +1/+1 counters on it or create two 1/1 colorless Servo artifact creature tokens.)
0/1
*/
	etb_fabricate(player, card, event, 2, count_subtype(player, TYPE_CREATURE, -1)*5, 15);
	return 0;
}


// Red

int card_aethertorch_renegade(int player, int card, event_t event){
	/* Aethertorch Renegade	|2|R	0x200f2a5
	 * Creature - Human Rogue 1/2
	 * When ~ enters the battlefield, you get |E|E|E|E.
	 * |T, Pay |E|E: ~ deals 1 damage to target creature.
	 * |T, Pay |E|E|E|E|E|E|E|E: ~ deals 6 damage to target player or planeswalker. */
	etb_energy(player, card, event, 4);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_DAMAGE_CREATURE = 1,
		CHOICE_DAMAGE_PLAYER
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2),
			&td, NULL) )
		{
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 8),
			&td2, NULL) )
		{
			return 1;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		int can_damage_creature = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
															GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2), &td, NULL);
		int can_damage_player = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
															GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 8), &td2, NULL);
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"1 damage to target creature", can_damage_creature, 5,
							"6 damage to target player", can_damage_player, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DAMAGE_CREATURE ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
											GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2), &td, "TARGET_CREATURE");
		}
		if( choice == CHOICE_DAMAGE_PLAYER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
											GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 8), &td2, "TARGET_PLAYER_OR_PLANESWALKER");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_DAMAGE_CREATURE && valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		if( choice == CHOICE_DAMAGE_PLAYER && valid_target(&td2) ){
			damage_target0(player, card, 6);
		}
	}

	return 0;
}

/*
Brazen Scourge |1|R|R	0x200bb46 (Raging Goblin)
Creature - Gremlin
Haste
3/3
*/

int card_built_to_smash(int player, int card, event_t event){
/*
Built to Smash |R	0x200f2aa
Instant
Target attacking creature gets +3/+3 until end of turn. If it's an artifact creature, it gains trample until end of turn.
*/

	if( ! IS_GS_EVENT(player, card, event) && event != EVENT_CHECK_PUMP ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_state = TARGET_STATE_ATTACKING;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_CHECK_PUMP )
	{
		if( ! has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) ||
			! can_target(&td) )
		{
			return 0;
		}
		pumpable_power[player] += 3;
		pumpable_toughness[player] +=3;
	}

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target attacking creature.", 0, 1 | GS_LITERAL_PROMPT) ){
			if( is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ){
				ai_modifier += 15;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int key = is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ? KEYWORD_TRAMPLE : 0;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3, key, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cathartic_reunion(int player, int card, event_t event){
/*
Cathartic Reunion |1|R	0x200f2af
Sorcery
As an additional cost to cast Cathartic Reunion, discard two cards.
Draw three cards.
*/
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event) && hand_count[player] > 2 ? 1 : 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		new_multidiscard(player, 2, 0, player);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_chandra_torch_of_defiance(int player, int card, event_t event){
	/* Chandra, Torch of Defiance	|2|R|R	0x200f2b4
	 * Legendary Planeswalker - Chandra (4)
	 * +1: Exile the top card of your library. You may cast that card. If you don't, ~ deals 2 damage to each opponent.
	 * +1: Add |R|R.
	 * -3: ~ deals 4 damage to target creature.
	 * -7: You get an emblem with "Whenever you cast a spell, this emblem deals 5 damage to any target." */
	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 4);
	}

	enum{
		CHOICE_EXILE_TOP_CARD = 1,
		CHOICE_MANA,
		CHOICE_DAMAGE_CREATURE,
		CHOICE_EMBLEM
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;

		int priority_exile = 35 - (hand_count[player]*5);
		int priority_mana = 25 - (count_subtype(player, TYPE_LAND, -1)*5);
		int priority_damage = count_counters(player, card, COUNTER_LOYALTY)*3;
		if( priority_damage > priority_exile && priority_damage > priority_mana ){
			td.toughness_requirement = 4 | TARGET_PT_LESSER_OR_EQUAL;
			if( count_subtype(player, TYPE_CREATURE, -1) < (count_subtype(1-player, TYPE_CREATURE, -1) - (can_target(&td) ? 1 : 0)) ){
				priority_damage-=10;
			}
			default_target_definition(player, card, &td, TYPE_CREATURE);
		}


		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Exile top card", 					1, 					priority_exile, 		+1,
							"Produce RR", 						1, 					priority_mana, 			+1,
							"4 damage to target creature", 		can_target(&td), 	priority_damage, 		-3,
							"Emblem", 							1, 					20, 					-7);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_DAMAGE_CREATURE ){
			new_pick_target(&td, "TARGET_CREATURE", 0, 1);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		int p = instance->parent_controller;
		if( choice == CHOICE_EXILE_TOP_CARD ){
			if( deck_ptr[p][0] != -1 ){
				int iid = deck_ptr[p][0];
				reveal_card_iid(player, card, iid);
				rfg_top_card_of_deck(p);
				if( ! is_what(-1, iid, TYPE_LAND) && can_legally_play_iid(p, iid) ){
					char buffer [100];
					scnprintf(buffer, 100, "Play %s", cards_ptr[cards_data[iid].id]->name);
					int play_this = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_SMALLCARD_ID(iid), DLG_WHO_CHOOSES(p),
										buffer, 1, 10,
										"Decline", 1, 1);
					if( play_this == 1 ){
						create_may_play_card_from_exile_effect(p, card, p, cards_data[iid].id, MPCFE_UNTIL_EOT);
					}
					else damage_player(1-player, 2, player, card);
				}
				if( is_what(-1, iid, TYPE_LAND)){
					damage_player(1-player, 2, player, card);
			    }
		     }
		}
		if( choice == CHOICE_MANA ){
			produce_mana(p, COLOR_RED, 2);
		}

		if( choice == CHOICE_DAMAGE_CREATURE ){
			if( valid_target(&td) ){
				damage_target0(player, card, 4);
			}
		}

		if( choice == CHOICE_EMBLEM ){
			generate_token_by_id(player, card, CARD_ID_CHANDRA_TORCH_OF_DEFIANCE_EMBLEM);
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_chandra_torch_of_defiance_emblem(int player, int card, event_t event){
	/* Chandra, Torch of Defiance Emblem	""	0x200f2b9
	 * Emblem - Chandra
	 * Whenever you cast a spell, this emblem deals 5 damage to any target. */
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		get_card_instance(player, card)->number_of_targets = 0;

		if( new_specific_spell_played(player, card, event, player, (can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0), NULL) ){
			if( new_pick_target(&td, "TARGET_ANY", 0, 0) ){
				damage_target0(player, card, 5);
			}
		}
	}

	return 0;
}

/*
Chandra's Pyrohelix |1|R	0x20085b8 (Forked Bolt)
Instant
Chandra's Pyrohelix deals 2 damage divided as you choose among one or two target creatures and/or players.
*/

int card_combustible_gearhulk(int player, int card, event_t event){
/*
Combustible Gearhulk |4|R|R	0x200f2be
Artifact Creature - Construct
First strike
When Combustible Gearhulk enters the battlefield, target opponent may have you draw three cards.
If the player doesn't, put the top three cards of your library into your graveyard, then Combustible Gearhulk deals damage to
that player equal to the total converted mana cost of those cards.
6/6
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td, 1-player, -1) ){
			int priority_draw = 5;
			int priority_mill = life[player] <= 6 ? 10 : 1;
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_WHO_CHOOSES(1-player),
								"Opponent draws 3 cards", 1, priority_draw,
								"Opponent mill 3 & damages you", 1, priority_mill);
			if( choice == 1 ){
				draw_cards(player, 3);
			}
			if( choice == 2 ){
				special_mill(player, card, CARD_ID_COMBUSTIBLE_GEARHULK, player, 3);
				int amount = get_card_instance(player, card)->info_slot;
				if( amount ){
					damage_player(1-player, player, card, amount);
				}
			}
		}
	}

	menace(player, card, event);

	return 0;
}

// Demolish: reprint ODY

int card_fateful_showdown(int player, int card, event_t event){
	/* Fateful Showdown	|2|R|R	0x200f2c3
	 * Instant
	 * ~ deals damage to any target equal to the number of cards in your hand. Discard all the cards in your hand, then draw that many cards. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = hand_count[player];
			damage_target0(player, card, amount);
			new_discard_all(player, player);
			draw_cards(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_furious_reprisal(int player, int card, event_t event){
	/* Furious Reprisal	|3|R	0x200f2c8
	 * Sorcery
	 * ~ deals 2 damage to each of two targets. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	target_definition_t td_player;
	default_target_definition(player, card, &td_player, 0);
	td_player.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( player == AI ){
			if( would_validate_arbitrary_target(&td_player, 1-player, -1) ){
				if( life[1-player] <= 3 && life[player] >= 9 && would_validate_arbitrary_target(&td_player, player, -1) ){
					return 1;
				}
				return can_target(&td_creature) ? 1 : 0;
			}
			return target_available(player, card, &td_creature) > 1 ? 1 : 0;
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 2, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == AI ){
			int bt = 0;
			if( would_validate_arbitrary_target(&td_player, 1-player, -1) && life[1-player] <= 6 ){
				instance->targets[0].player = 1-player;
				instance->targets[0].card = -1;
				instance->number_of_targets = 1;
				bt = 1;
			}
			if( ! can_target(&td_creature) && would_validate_arbitrary_target(&td_player, player, -1) && life[player] >= 9 ){
				instance->targets[1].player = player;
				instance->targets[1].card = -1;
				instance->number_of_targets = 2;
				bt = 2;
			}
			while( bt < 2 ){
					if( new_pick_target(&td_creature, "TARGET_CREATURE", bt, 1) ){
						state_untargettable(instance->targets[bt].player, instance->targets[bt].card, 1);
					}
					bt++;
			}
			int i;
			for(i=0; i<2; i++){
				if( instance->targets[i].card != -1 ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				}
			}
		}
		else{
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 2, NULL);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				damage_creature(instance->targets[i].player, instance->targets[i].card, 2, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 2, NULL);
}

int card_giant_spectacle(int player, int card, event_t event){
/*
Giant Spectacle |1|R	0x200f2cd
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+1 and has menace.
*/
	return generic_aura(player, card, event, player, 2, 1, 0, SP_KEYWORD_MENACE, 0, 0, 0);
}

static const char* ai_helper_function_for_harness_lightning(int who_chooses, int player, int card)
{
	if( get_toughness(player, card)-get_card_instance(player, card)->damage_on_card <= ENERGY_COUNTERS(player)+2 ){
		return NULL;
	}
	return "AI will pick only permanents with TOU <= ENERGY_COUNTERS()+2";
}

int card_harness_lightning(int player, int card, event_t event){
/*
Harnessed Lightning |1|R	0x200f2d2
Instant
Choose target creature. You get |E|E|E (three energy counters), then you may pay any amount of |E.
Harnessed Lightning deals that much damage to that creature.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = (int32_t)ai_helper_function_for_harness_lightning;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			add_energy(player, 3);
			int tou = get_toughness(player, card)-get_card_instance(player, card)->damage_on_card;
			int amount = MIN(ENERGY_COUNTERS(player), tou);
			if( player == AI && amount < tou ){
				amount = 0;
			}
			if( player == HUMAN ){
				amount = choose_a_number(player, "How much Energy you'll spend?", amount);
			}
			remove_energy(player, amount);
			if( amount ){
				damage_target0(player, card, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_hijack(int player, int card, event_t event){
/*
Hijack |1|R|R	0x200f2d7
Sorcery
Gain control of target artifact or creature until end of turn. Untap it. It gains haste until end of turn.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( instance->targets[0].player != player ){
				effect_act_of_treason( player, card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact or creature.", 1, NULL);
}

int card_incendiary_sabotage(int player, int card, event_t event){
/*
Incendiary Sabotage |2|R|R	0x200f2dc
Instant
As an additional cost to cast Incendiary Sabotage, sacrifice an artifact.
Incendiary Sabotage deals 3 damage to each creature.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {new_damage_all(player, card, p, 3, NDA_ALL_CREATURES, NULL);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_PERMANENT_AS_COST, NULL, NULL, 1, &test);
}

int card_inventors_apprentice(int player, int card, event_t event){
/*
Inventor's Apprentice |R	0x200f2e1
Creature - Human Artificer
Inventor's Apprentice gets +1/+1 as long as you control an artifact.
1/2
*/
	boost_if_you_control_an_artifact(player, card, event, 1, 1, 0, 0);
	return 0;
}

int card_lathnu_hellion(int player, int card, event_t event){
/*
Lathnu Hellion |2|R	0x200f2e6
Creature - Hellion
Haste
When Lathnu Hellion enters the battlefield, you get |E|E (two energy counters).
At the beginning of your end step, sacrifice Lathnu Hellion unless you pay |E|E.
4/4
*/
	etb_energy(player, card, event, 2);

	if( current_turn == player && ! is_humiliated(player, card) && eot_trigger(player, card, event) ){
		int sac_me = 1;
		if( ENERGY_COUNTERS(player) ){
			int priority_pay_energy = 10;
			if( (get_abilities(player, card, EVENT_ABILITIES, -1) & KEYWORD_DEFENDER) ||
				check_special_flags2(player, card, SF2_COULD_NOT_UNTAP) )
			{
				priority_pay_energy = 1;
			}

			int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
								"Pay 2 Energy", 1, priority_pay_energy,
								"Decline", 1, 5);
			if( choice == 1 ){
				remove_energy(player, 2);
				sac_me = 0;
			}
		}
		if( sac_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_madcap_experiment(int player, int card, event_t event ){
/*
Madcap Experiment |3|R	0x200f2eb
Sorcery
Reveal cards from the top of your library until you reveal an artifact card.
Put that card onto the battlefield and the rest on the bottom of your library in a random order.
Madcap Experiment deals damage to you equal to the number of cards revealed this way.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		int amount = reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_PUT_TEST_INTO_PLAY |
														RCUYRT_PUT_REST_ON_BOTTOM_RANDOM | RCUYRT_RETURN_REVEALED_CARDS);
		damage_player(player, amount, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_maulfist_doorbuster(int player, int card, event_t event){
/*
Maulfist Doorbuster |3|R	0x200f2f0
Creature - Human Warrior
When Maulfist Doorbuster enters the battlefield, you get |E|E (two energy counters).
Whenever Maulfist Doorbuster attacks, you may pay |E. If you do, target creature can't block this turn.
4/2
*/
	etb_energy(player, card, event, 2);

	store_attackers(player, card, event, 0, player, card, NULL);

	if (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card) && reason_for_trigger_controller == player)
	{
		if( ENERGY_COUNTERS(player) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);

			int trig_mode = RESOLVE_TRIGGER_OPTIONAL;
			if( player == AI ){
				trig_mode = can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0;
			}

			if( resolve_declare_attackers_trigger(player, card, event, trig_mode) ){
				remove_energy(player, 1);

				card_instance_t *instance = get_card_instance(player, card);

				if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
											0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
				}
				instance->number_of_targets = 0;
			}
		}
	}

	return 0;
}

int card_pia_nalaar(int player, int card, event_t event){
/*
Pia Nalaar |2|R	0x200f2f5
Legendary Creature - Human Artificer
When Pia Nalaar enters the battlefield, create a 1/1 colorless Thopter artifact creature token with flying.
|1|R: Target artifact creature gets +1/+0 until end of turn.
|1, Sacrifice an artifact: Target creature can't block this turn.
2/2
*/

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		make_thopter(player, card, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP = 1,
		CHOICE_REMOVE_BLOCKER,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, NULL) ){
			return 1;
		}
		if( can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, NULL);
		}
	}

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int can_pump = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, NULL);
		int can_remove_blocker = can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT) &&
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, NULL)
								? 1 : 0;

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Pump artifact creature", can_pump, current_phase == PHASE_AFTER_BLOCKING ? 10 : 1,
							"Sac artifact and remove blockers", can_remove_blocker, current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_PUMP ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XR(1, 1), 0,
											&td, "Select target artifact creature.");
		}
		if( choice == CHOICE_REMOVE_BLOCKER ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac){
				cancel = 1;
				return 0;
			}
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_PUMP ){
			if( valid_target(&td) ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 1, 0);
			}
		}
		if( choice == CHOICE_REMOVE_BLOCKER ){
			if( valid_target(&td2) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			}
		}
	}

	return 0;
}

int card_quicksmith_genius(int player, int card, event_t event){
/*
Quicksmith Genius |2|R	0x200f2fa
Creature - Human Artificer
Whenever an artifact enters the battlefield under your control, you may discard a card. If you do, draw a card.
3/2
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && hand_count[player] ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test);
		while( result && hand_count[player] ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
									"Discard & draw", 1, count_deck(player) > 20 ? 10 : 1,
									"Decline", 1, 5);
				if( choice == 1 ){
					discard(player, 0, player);
					draw_cards(player, 1);
				}
				else{
					break;
				}
				result--;
		}
	}

	return 0;
}

int card_reckless_fireweaver(int player, int card, event_t event){
/*
Reckless Fireweaver |1|R	0x200f2ff
Creature - Human Artificer
Whenever an artifact enters the battlefield under your control, Reckless Fireweaver deals 1 damage to each opponent.
1/3
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test);
		while( result && hand_count[player] ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
									"Damage opponent", 1, 10,
									"Decline", 1, 1);
				if( choice == 1 ){
					damage_player(1-player, 1, player, card);
				}
				else{
					break;
				}
				result--;
		}
	}

	return 0;
}

int card_renegade_tactics(int player, int card, event_t event){
/*
Renegade Tactics |R	0x200f304
Sorcery
Target creature can't block this turn.
Draw a card.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_ruinous_goblin(int player, int card, event_t event){
/*
Ruinous Gremlin |R	0x200f309
Creature - Gremlin
|2|R, Sacrifice Ruinous Gremlin: Destroy target artifact.
1/1
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XR(2, 1), 0, &td, "TARGET_ARTIFACT");
}

int card_salivating_gremlins(int player, int card, event_t event){
/*
Salivating Gremlins |2|R	0x200f30e
Creature - Gremlin
Whenever an artifact enters the battlefield under your control, Salivating Gremlins gets +2/+0 and gains trample until end of turn.
2/3
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test);
		if( result ){
			pump_ability_until_eot(player, card, player, card, result*2, 0, KEYWORD_TRAMPLE, 0);
		}
	}
	return 0;
}

int card_skyship_stalker(int player, int card, event_t event){
/*
Skyship Stalker |2|R|R	0x200f313
Creature - Dragon
Flying
|R: Skyship Stalker gets +1/+0 until end of turn.
|R: Skyship Stalker gains first strike until end of turn.
|R: Skyship Stalker gains haste until end of turn.
3/3
*/

    if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( current_turn == player ){
			return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP = 1,
		CHOICE_FIRST_STRIKE,
		CHOICE_HASTE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
	}

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int priority_pump = current_phase == PHASE_AFTER_BLOCKING ? 10 : 1;
		int priority_first_strike = 1;
		if( current_phase == PHASE_AFTER_BLOCKING && player == AI ){
			if( (instance->targets[1].player > -1 && !(instance->targets[1].player & KEYWORD_FIRST_STRIKE)) ||
				instance->targets[1].player == -1 )
			{
				if( current_turn == player ){
					int toughness_of_blockers = 0;
					int power_of_blockers = 0;
					CYCLE_ALL_CARDS(p, c, 	{
												if( p == 1-player && in_play(p, c) && get_card_instance(p, c)->blocking == card ){
													toughness_of_blockers += get_toughness(p, c);
													power_of_blockers += get_power(p, c);
												}
											}
					);
					if( power_of_blockers >= get_toughness(player, card) ){
						if( get_power(player, card) + (has_mana(player, COLOR_RED, -1)-1) >= toughness_of_blockers ){
							priority_first_strike = 15;
						}
					}
				}
				else{
					int blocker = instance->blocking;
					if( blocker > 0 ){
						int toughness_of_blockers = get_toughness(1-player, blocker);
						int power_of_blockers = get_power(1-player, blocker);
						if( power_of_blockers >= get_toughness(player, card) ){
							if( get_power(player, card) + (has_mana(player, COLOR_RED, -1)-1) >= toughness_of_blockers ){
								priority_first_strike = 15;
							}
						}
					}
				}
			}
		}
		int priority_haste = current_phase < PHASE_DECLARE_ATTACKERS ? 5 : 1;

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"+1/+0 until EOT", 1, priority_pump,
							"Fist Strike until EOT", 1, priority_first_strike,
							"Haste until EOT", 1, priority_haste);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		// AI
		if( choice != CHOICE_PUMP ){
			generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
			instance->targets[1].player = 0;
			instance->targets[1].player |= (choice == CHOICE_FIRST_STRIKE ? KEYWORD_FIRST_STRIKE : SP_KEYWORD_HASTE);
		}
		else generic_activated_ability(player, card, event, 0, MANACOST_R(1), 0, NULL, NULL);
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		int pow = choice == CHOICE_PUMP ? 1 : 0;
		int key = choice == CHOICE_FIRST_STRIKE ? KEYWORD_FIRST_STRIKE : 0;
		int s_key = choice == CHOICE_HASTE ? SP_KEYWORD_HASTE : 0;
		if( pow ){
			alternate_legacy_text(1, instance->parent_controller, pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 1, 0));
		}
		if(key){
		   alternate_legacy_text(2, instance->parent_controller, pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->parent_controller, instance->parent_card, 0, 0, key, 0));
		}
		if(s_key){
		   alternate_legacy_text(3, instance->parent_controller, pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->parent_controller, instance->parent_card, 0, 0, 0, s_key));
	    }
	}
	return 0;
}


int card_spark_of_creativity(int player, int card, event_t event){
/*
Spark of Creativity |R	0x200f313
Sorcery
Choose target creature. Exile the top card of your library.
You may have Spark of Creativity deal damage to that creature equal to the exiled card's converted mana cost.
If you don't, you may play that card until end of turn.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) && deck_ptr[player][0] != -1 ){
			card_instance_t *instance = get_card_instance( player, card);

			int iid = deck_ptr[player][0];
			reveal_card_iid(player, card, iid);
			rfg_top_card_of_deck(player);
			int dmg = get_cmc_by_internal_id(iid);
			char buf1 [100];
			char buf2 [100];
			scnprintf(buf1, 100, "Deal %d damage to target creature", dmg);
			scnprintf(buf2, 100, "You may play %s", cards_ptr[cards_data[iid].id]->name);
			int priority_play = 5;
			int priority_damage = get_toughness(instance->targets[0].player, instance->targets[0].card) <= dmg ? 10 : 1;
			if( ! is_what(-1, iid, TYPE_LAND) && can_legally_play_iid(player, iid) && has_mana_to_cast_iid(player, iid, event) ){
				priority_play = 15;
			}

			int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_SMALLCARD_ID(iid),
								buf1, 1, priority_damage,
								buf2, 1, priority_play);
			if( choice == 1 ){
				damage_target0(player, card, dmg);
			}
			else{
				create_may_play_card_from_exile_effect(player, card, player, cards_data[iid].id, MPCFE_UNTIL_EOT);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


/*
Speedway Fanatic |1|R	0x200bb46 (Raging Goblin, the rest of the effect is in "vehicle")
Creature - Human Pilot
Haste
Whenever Speedway Fanatic crews a Vehicle, that Vehicle gains haste until end of turn.
2/1
*/

int card_spireside_infiltrator(int player, int card, event_t event){
/*
Spireside Infiltrator |2|R	0x200f31d
Creature - Human Rogue
Whenever Spireside Infiltrator becomes tapped, it deals 1 damage to each opponent.
3/2
*/
	if( event == EVENT_TAP_CARD && affect_me(player, card) && ! is_humiliated(player, card) ){
		damage_player(1-player, 1, player, card);
	}

	return 0;
}

int card_spontaneous_artist(int player, int card, event_t event){
/*
Spontaneous Artist |3|R	0x200f322
Creature - Human Rogue
When Spontaneous Artist enters the battlefield, you get |E (an energy counter).
Pay |E: Target creature gains haste until end of turn.
3/3
*/

	etb_energy(player, card, event, 1);

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance( player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, 0, SP_KEYWORD_HASTE);
	}


	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, (GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1)),
									&td, "TARGET_CREATURE");
}

int card_start_your_engines(int player, int card, event_t event ){
/*
Start Your Engines |3|R	0x200f327
Sorcery
Vehicles you control become artifact creatures until end of turn. Creatures you control get +2/+0 until end of turn.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		int marked[151];
		int mc = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_PERMANENT) && has_subtype(player, c, SUBTYPE_VEHICLE) &&
				! is_what(player, c, TYPE_CREATURE) )
			{
				marked[mc] = c;
				mc++;
			}
		}
		for(c=mc-1; c>-1; c--){
			turn_into_creature(player, marked[c], player, marked[c], 1, 0, 0);
		}
		pump_subtype_until_eot(player, card, player, -1, 2, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_territorial_gorge(int player, int card, event_t event)
{
/*
Territorial Gorger |3|R	0x200f32c
Creature - Gremlin
Trample
Whenever you get one or more |E (energy counters), Territorial Gorger gets +2/+2 until end of turn.
2/2
*/
	enable_xtrigger_flags |= XTRIGGER_COUNTER_ENERGY;
	if( xtrigger_condition() == XTRIGGER_COUNTER_ENERGY && affect_me(player, card) && trigger_cause_controller == player &&
		! is_humiliated(player, card))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;
	  if (event == EVENT_RESOLVE_TRIGGER)
		pump_until_eot(player, card, player, card, 2, 2);
	   }

  return 0;
}

/*
Terror of the Fairgrounds |3|R		0x401000 (vanilla)
Creature - Gremlin
5/2
*/

/*
Thriving Grubs	0x200f16f	(Thriving Ibex)
Creature - Gremlin
When Thriving Grubs enters the battlefield, you get |E|E (two energy counters).
Whenever Thriving Grubs attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
2/1
*/

/*
Wayward Giant |4|R	0x2009977 (Ripscale Predators)
Creature - Giant
Menace
4/5
*/

int card_welding_sparks(int player, int card, event_t event){
/*
Welding Sparks |2|R	0x200f331
Instant
Welding Sparks deals X damage to target creature, where X is 3 plus the number of artifacts you control.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3+count_subtype(player, TYPE_ARTIFACT, -1));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}


// Green

int card_appetite_for_the_unnatural(int player, int card, event_t event ){
/*
Appetite for the Unnatural |2|G	0x200f336
Instant
Destroy target artifact or enchantment. You gain 2 life.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_arborback_stomper(int player, int card, event_t event){
/*
Arborback Stomper |3|G|G	0x200f33b
Creature - Beast
Trample
When Arborback Stomper enters the battlefield, you gain 5 life.
5/4
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 5);
	}

	return 0;
}

int card_architect_of_the_untamed(int player, int card, event_t event){
/*
Architect of the Untamed |2|G	0x200f340
Creature - Elf Artificer Druid
Whenever a land enters the battlefield under your control, you get |E (an energy counter).
Pay |E|E|E|E|E|E|E|E: Create a 6/6 colorless Beast artifact creature token.
2/3
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && has_mana(player, COLOR_ANY, 1) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test);
		if( result ){
			add_energy(player, result);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_BEAST, &token);
		token.pow = token.tou = 6;
		token.color_forced = COLOR_TEST_COLORLESS;
		token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 8), NULL, NULL);
}

int card_armorcraft_judge(int player, int card, event_t event){
/*
Armorcraft Judge |3|G	0x200f345
Creature - Elf Artificer
When Armorcraft Judge enters the battlefield, draw a card for each creature you control with a +1/+1 counter on it.
3/3
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
										amount += count_1_1_counters(p, c) ? 1 : 0;
									}
								};
		);
		draw_cards(player, amount);
	}

	return 0;
}


int card_attune_with_aether(int player, int card, event_t event ){
/*
Attune with Aether |G	0x200f34a
Sorcery
Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. You get |E|E (two energy counters).
*/

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_land(player, 0, 0);
		add_energy(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_blossoming_defense(int player, int card, event_t event ){
/*
Blossoming Defense |G	0x200f34f
Instant
Target creature you control gets +2/+2 and gains hexproof until end of turn.
*/
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, 0, SP_KEYWORD_HEXPROOF);
}

int card_bristling_hydra(int player, int card, event_t event){
/*
Bristling Hydra |2|G|G	0x200f354
Creature - Hydra
When Bristling Hydra enters the battlefield, you get |E|E|E (three energy counters).
Pay |E|E|E: Put a +1/+1 counter on Bristling Hydra. It gains hexproof until end of turn.
4/3
*/
	etb_energy(player, card, event, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_HEXPROOF);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 3), NULL, NULL);
}

/*
Commencement of Festivities |1|G	0x200b006 (Defend the Heart)
Instant
Prevent all combat damage that would be dealt to players this turn.
*/

/*
Cowl Prowler |4|G|G	0x401000 (vanilla)
Creature - Wurm
6/6
*/

// Creeping Mold: reprint MIR

int card_cultivator_of_blades(int player, int card, event_t event){
/*
Cultivator of Blades |3|G|G	0x200f359
Creature - Elf Artificer
Fabricate 2 (When this creature enters the battlefield, put two +1/+1 counters on it or create two 1/1 colorless Servo
			artifact creature tokens.)
Whenever Cultivator of Blades attacks, you may have other attacking creatures get +X/+X until end of turn,
where X is Cultivator of Blades's power.
1/1
*/

	etb_fabricate(player, card, event, 2, count_subtype(player, TYPE_CREATURE, -1)*5, 15);

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card) ){
		int pump = get_power(player, card);

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING;
		test.not_me = 1;
		pump_creatures_until_eot(player, card, player, 0, pump, pump, 0, 0, &test);
	}

	return 0;
}

int card_dubious_challenge(int player, int card, event_t event ){
/*
Dubious Challenge |3|G	0x200f35e
Sorcery
Look at the top ten cards of your library, exile up to two creature cards from among them, then shuffle your library.
Target opponent may choose one of the exiled cards and put it onto the battlefield under his or her control.
Put the rest onto the battlefield under your control.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = MIN(10, count_deck(player));
		if( amount ){
			int exiled[2] = {-1};
			int ec = 0;

			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");
			test.create_minideck = amount;
			test.no_shuffle = 1;
			int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &test);
			if( selected != -1 ){
				exiled[0] = deck_ptr[player][selected];
				rfg_card_in_deck(player, selected);
				ec++;
				amount--;
			}
			if( amount ){
				test.create_minideck = amount;
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &test);
				if( selected != -1 ){
					exiled[ec] = deck_ptr[player][selected];
					rfg_card_in_deck(player, selected);
					amount--;
					ec++;
				}
			}
			shuffle(player);
			if( ec ){
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
				selected = select_card_from_zone(1-player, 1-player, exiled, ec, 0, AI_MAX_CMC, -1, &test);
				if( selected != -1 ){
					remove_card_from_rfg(player, cards_data[exiled[selected]].id);
					int card_added = add_card_to_hand(1-player, exiled[selected]);
					exiled[selected] = -1;
					if( 1-player == AI ){
						remove_state(1-player, card_added, STATE_OWNED_BY_OPPONENT);
					}
					else{
						add_state(1-player, card_added, STATE_OWNED_BY_OPPONENT);
					}
					put_into_play(1-player, card_added);
				}
				int i;
				for(i=0; i<ec; i++){
					if( exiled[i] > -1 ){
						remove_card_from_rfg(player, cards_data[exiled[i]].id);
						int card_added = add_card_to_hand(player, exiled[i]);
						put_into_play(player, card_added);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_durable_handicraft(int player, int card, event_t event){
/*
Durable Handicraft |1|G	0x200f363
Enchantment
Whenever a creature enters the battlefield under your control, you may pay |1. If you do, put a +1/+1 counter on that creature.
|5|G, Sacrifice Durable Handicraft: Put a +1/+1 counter on each creature you control.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && has_mana(player, COLOR_ANY, 1) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test);
		while( result && has_mana(player, COLOR_ANY, 1) ){
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
									DLG_SMALLCARD(trigger_cause_controller, trigger_cause),
									"Add +1/+1 counter on this", 1, 5,
									"Decline", 1, 1);
				if( choice == 1 ){
					charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						add_1_1_counter(trigger_cause_controller, trigger_cause);
					}
				}
				result--;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, player, &this_test, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(5, 1), 0, NULL, NULL);
}

int card_elegant_edgecrafters(int player, int card, event_t event){
/*
Elegant Edgecrafters |4|G|G	0x200f368
Creature - Elf Artificer
Elegant Edgecrafters can't be blocked by creatures with power 2 or less.
Fabricate 2 (When this creature enters the battlefield, put two +1/+1 counters on it or create two 1/1 colorless Servo artifact creature tokens.)
3/4
*/
	etb_fabricate(player, card, event, 2, count_subtype(player, TYPE_CREATURE, -1)*5, 10);

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == player && attacking_card == card ){
			if( get_power(affected_card_controller, affected_card) <= 2 ){
				event_result = 1;
			}
		}
	}
	return 0;
}

int card_fairgrounds_trumpeter(int player, int card, event_t event){
/*
Fairgrounds Trumpeter |2|G
Creature - Elephant
At the beginning of each end step, if a +1/+1 counter was placed on a permanent under your control this turn,
put a +1/+1 counter on Fairgrounds Trumpeter.
2/2
*/
	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) ){
		if( get_trap_condition(player, TRAP_P1_P1_COUNTER_PLACED) && ! is_humiliated(player, card) && eot_trigger(player, card, event) ){
			add_1_1_counter(player, card);
		}
	}

	return 0;
}

int ghirapur_guide_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == p && attacking_card == c ){
			if( get_power(affected_card_controller, affected_card) <= 2 ){
				event_result = 1;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_ghirapur_guide(int player, int card, event_t event){
/*
Ghirapur Guide |2|G	0x200f372
Creature - Elf Scout
|2|G: Target creature you control can't be blocked by creatures with power 2 or less this turn.
3/2
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance(player, card);
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &ghirapur_guide_effect,
										instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_CREATURE");
}


int card_highspire_artisan(int player, int card, event_t event){
/*
Highspire Artisan |2|G	0x200f377
Creature - Elf Artificer
Reach (This creature can block creatures with flying.)
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
0/3
*/
	etb_fabricate(player, card, event, 1, count_subtype(player, TYPE_CREATURE, -1)*5, 10);
	return 0;
}

// Hunt the Weak: reprint M14

int card_kujar_seedsculptor(int player, int card, event_t event ){
/*
Kujar Seedsculptor |1|G	0x200f37c
Creature - Elf Druid
When Kujar Seedsculptor enters the battlefield, put a +1/+1 counter on target creature you control.
1/2
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, GS_LITERAL_PROMPT) ){
			add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_larger_than_life(int player, int card, event_t event ){
/*
Larger Than Life |1|G	0x200f381
Sorcery
Target creature gets +4/+4 and gains trample until end of turn.
*/
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 4, 4, KEYWORD_TRAMPLE, 0);
}

int card_longtusk_cub(int player, int card, event_t event){
/*
Longtusk Cub |1|G	0x200f386
Creature - Cat
Whenever Longtusk Cub deals combat damage to a player, you get |E|E (two energy counters).
Pay |E|E: Put a +1/+1 counter on Longtusk Cub.
2/2
*/

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_PLAYER) ){
		add_energy(player, 2);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 2), NULL, NULL);
}

int card_natures_way(int player, int card, event_t event ){
/*
Nature's Way |1|G	0x200f38b
Sorcery
Target creature you control gains vigilance and trample until end of turn.
It deals damage equal to its power to target creature you don't control.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = td2.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select target creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			new_pick_target(&td2, "Select target creature you don't control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if (validate_target(player, card, &td, 0)){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									0, 0, KEYWORD_TRAMPLE, SP_KEYWORD_VIGILANCE);
			if (validate_target(player, card, &td2, 1)){
				int pow = get_power(instance->targets[0].player, instance->targets[0].card);
				damage_creature(instance->targets[1].player, instance->targets[1].card, pow,
								instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_nissa_vital_force(int player, int card, event_t event){
/*
Nissa, Vital Force |3|G|G	0x200f390
Planeswalker - Nissa
+1: Untap target land you control. Until your next turn, it becomes a 5/5 Elemental creature with haste. It's still a land.
-3: Return target permanent card from your graveyard to your hand.
-6: You get an emblem with "Whenever a land enters the battlefield under your control, you may draw a card."
5
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 5);
	}

	enum{
		CHOICE_UNTAP_AND_ANIMATE = 1,
		CHOICE_RETURN_PERMANENT,
		CHOICE_EMBLEM
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = td.preferred_controller = player;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select target permanent card.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( planeswalker(player, card, event, 5) ){
			int loy = count_counters(player, card, COUNTER_LOYALTY);
			if( loy >= 6 ){
				return 1;
			}
			if( loy >= 3 && new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ){
				return 1;
			}
			if( can_target(&td) ){
				return 1;
			}
		}
	}

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int can_return_permanent = new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ? 1 : 0;
		int priority_untap_animate = 25-(count_subtype(player, TYPE_CREATURE, -1)*5);
		int priority_return_permanent = count_counters(player, card, COUNTER_LOYALTY)*4;

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Untap & animate", 					can_target(&td), 		priority_untap_animate, 		+1,
							"Return permanent from grave", 		can_return_permanent, 	priority_return_permanent, 		-3,
							"Emblem", 							1, 						30, 							-6);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_UNTAP_AND_ANIMATE ){
			if( ! new_pick_target(&td, "TARGET_LAND", 0, 1) ){
				return 0;
			}
		}
		if( choice == CHOICE_RETURN_PERMANENT ){
			if(	select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0) == -1 ){
				spell_fizzled = 1;
				return 0;
			}
		}
		return planeswalker(player, card, event, 5);
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_UNTAP_AND_ANIMATE ){
			if( valid_target(&td) ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
				int legacy = turn_into_creature(player, card, instance->targets[0].player, instance->targets[0].card, 1, 5, 5);
				get_card_instance(player, legacy)->targets[8].card = SP_KEYWORD_HASTE;
			}
		}

		if( choice == CHOICE_RETURN_PERMANENT ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
			}
		}

		if( choice == CHOICE_EMBLEM ){
			generate_token_by_id(player, card, CARD_ID_NISSA_VITAL_FORCE_EMBLEM);
		}
	}

	return 0;
}

int card_nissa_vital_force_emblem(int player, int card, event_t event){
/*
Nissa, Vital Force Emblem	0x200f395
Emblem - Nissa
Whenever a land enters the battlefield under your control, you may draw a card.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_ornamental_courage(int player, int card, event_t event){
/*
Ornamental Courage |G	0x200f39a
Instant
Untap target creature. It gets +1/+3 until end of turn.
*/
	if( event == EVENT_CHECK_PUMP ){
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 1, 3, 0, 0);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_oviya_pashiri_sage_lifecrafter(int player, int card, event_t event){
/*
Oviya Pashiri, Sage Lifecrafter |G	0x200f39f
Legendary Creature - Human Artificer
|2|G, |T: Create a 1/1 colorless Servo artifact creature token.
|4|G, |T: Create an X/X colorless Construct artifact creature token, where X is the number of creatures you control.
1/2
*/

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_SERVO = 1,
		CHOICE_CONSTRUCT,
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(2, 1), 0, NULL, NULL);
	}

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int p_construct = count_subtype(player, TYPE_CREATURE, -1)*5;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create Servo", has_mana_for_activated_ability(player, card, MANACOST_XG(2, 1)), 20,
							"Create Construct", has_mana_for_activated_ability(player, card, MANACOST_XG(4, 1)), p_construct);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SERVO ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(2, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_CONSTRUCT ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(4, 1), 0, NULL, NULL);

		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_SERVO ){
			make_servo(instance->parent_controller, instance->parent_card, 1);
		}
		if( choice == CHOICE_CONSTRUCT ){
			token_generation_t token;
			default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_CONSTRUCT, &token);
			token.pow = token.tou = count_subtype(player, TYPE_CREATURE, -1);
			generate_token(&token);
		}
	}

	return 0;
}


/*
Peema Outrider |2|G|G	0x200f147 (Glint-Sleeve Artisan)
Creature - Elf Artificer
Trample
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
3/3
*/

int card_riparian_tiger(int player, int card, event_t event){
/*
Riparian Tiger |3|G|G	0x200f3a4
Creature - Cat
Trample
When Riparian Tiger enters the battlefield, you get |E|E (two energy counters).
Whenever Riparian Tiger attacks, you may pay |E|E. If you do, it gets +2/+2 until end of turn.
4/4
*/
	etb_energy(player, card, event, 2);

	if( attack_trigger_with_energy(player, card, event, 2) ){
		remove_energy(player, 2);
		pump_until_eot(player, card, player, card, 2, 2);
	}

	return 0;
}

int card_sage_of_shailas_claim(int player, int card, event_t event){
/*
Sage of Shaila's Claim |1|G	0x200f3a9
Creature - Elf Druid
When Sage of Shaila's Claim enters the battlefield, you get |E|E|E (three energy counters).
2/1
*/
	etb_energy(player, card, event, 3);
	return 0;
}

int card_servant_of_the_conduit(int player, int card, event_t event){
/*
Servant of the Conduit |1|G	0x200f3ae
Creature - Elf Druid
When Servant of the Conduit enters the battlefield, you get |E|E (two energy counters).
|T, Pay |E: Add one mana of any color to your mana pool.
2/2
*/
    etb_energy(player, card, event, 2);

	if( event == EVENT_CAN_ACTIVATE || (event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		if( ENERGY_COUNTERS(player) ){
			return mana_producing_creature_all_one_color(player, card, event, 5, COLOR_TEST_ANY_COLORED, 1);
		}
	}

	if( event == EVENT_ACTIVATE ){
		mana_producing_creature_all_one_color(player, card, event, 5, COLOR_TEST_ANY_COLORED, 1);
		if( cancel != 1 ){
			remove_energy(player, 1);
		}
	}

	return 0;
}

int card_take_down(int player, int card, event_t event){
/*
Take Down |G	0x200f3b3
Sorcery
Choose one -
* Take Down deals 4 damage to target creature with flying.
* Take Down deals 1 damage to each creature with flying.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	enum{
		CHOICE_4_DAMAGE_TO_TARGET = 1,
		CHOICE_1_DAMAGE_TO_ALL
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int priority_1_damage_to_all = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) &&
											(get_abilities(player, card, EVENT_ABILITIES, -1) & KEYWORD_FLYING) &&
											get_toughness(p, c) == 1 )
										{
											priority_1_damage_to_all += (p == player ? -5 : 5);
										}
									};
			);
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"4 damage to target creature with flying", can_target(&td), 5,
								"1 damage to each creature with flying", 1, priority_1_damage_to_all);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_4_DAMAGE_TO_TARGET ){
			if ( ! new_pick_target(&td, "TARGET_FLYING", 0, 1)){
			return 0;
		    }
	    }
	}
	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;

		if( choice == CHOICE_4_DAMAGE_TO_TARGET ){
			if( validate_target(player, card, &td, 0) ){
				damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
			}
		}

		if( choice == CHOICE_1_DAMAGE_TO_ALL ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.keyword = KEYWORD_FLYING;
			new_damage_all(player, card, ANYBODY, 1, 0, &test);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}


/*
Thriving Rhino |2|G	0x200f16f (Thriving Ibex)
Creature - Rhino
When Thriving Rhino enters the battlefield, you get |E|E (two energy counters).
Whenever Thriving Rhino attacks, you may pay |E|E. If you do, put a +1/+1 counter on it.
2/3
*/

int card_verdurous_gearhulk(int player, int card, event_t event){
/*
Verdurous Gearhulk |3|G|G	0x200f3b8
Artifact Creature - Construct
Trample
When Verdurous Gearhulk enters the battlefield, distribute four +1/+1 counters among any number of target creatures you control.
4/4
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			int marked[4] = {0};
			int i;
			for(i=0; i<4; i++){
				if( pick_target(&td, "TARGET_CREATURE") ){
					SET_BYTE0(marked[i]) |= instance->targets[0].player;
					SET_BYTE1(marked[i]) |= instance->targets[0].card;
					add_state(instance->targets[0].player, instance->targets[0].card, STATE_TARGETTED);
				}
				instance->number_of_targets = 0;
			}
			for(i=0; i<4; i++){
				add_1_1_counter(BYTE0(marked[i]), BYTE1(marked[i]));
				remove_state(BYTE0(marked[i]), BYTE1(marked[i]), STATE_TARGETTED);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_wild_wanderer(int player, int card, event_t event){
/*
Wild Wanderer |3|G	0x200f3bd
Creature - Elf Druid
When Wild Wanderer enters the battlefield, you may search your library for a basic land card,
put it onto the battlefield tapped, then shuffle your library.
3/2
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		tutor_basic_land(player, 1, 1);
	}

	return 0;
}

int card_wildest_dreams(int player, int card, event_t event){
/*
Wildest Dreams |X|X|G	0x200f3c2
Sorcery
Return X target cards from your graveyard to your hand. Exile Wildest Dreams.
*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to return to your hand.");

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL | GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		set_special_flags2(player, card, SF2_X_SPELL);
		if( ! played_for_free(player, card) && ! is_token(player, card) ){
			instance->info_slot = 0;
			if( player == HUMAN ){
				int result = charge_mana_for_double_x(player, COLOR_ANY);
				set_x_for_x_spells(player, card, event, result);
			}
			else{
				int mana_to_charge = MIN(count_graveyard(player)*2, hand_count[player]*2);
				while( ! has_mana(player, COLOR_ANY, mana_to_charge) ){
						mana_to_charge -= 2;
				}
				if( mana_to_charge == 0 ){
					ai_modifier-=50;
				}
				charge_mana(player, COLOR_ANY, mana_to_charge);
				if( spell_fizzled != 1 ){
					set_x_for_x_spells(player, card, event, mana_to_charge);
				}
			}
		}
		int max_targets = instance->info_slot / 2;
		select_multiple_cards_from_graveyard(player, player, 1, AI_MAX_VALUE, &this_test, max_targets, &instance->targets[0]);
	}

	if (event == EVENT_RESOLVE_SPELL){
		int i, num_validated = 0;
		for (i = 0; i < instance->info_slot / 2; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++num_validated;
			}
		}

		if (num_validated == 0 && instance->info_slot > 0){
			spell_fizzled = 1;
			kill_card(player, card, KILL_DESTROY);
		} else {
			kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}


int card_wily_bandar(int player, int card, event_t event){
/*
Wily Bandar |G	0x200f3c7
Creature - Cat Monkey
|2|G: Wily Bandar gains indestructible until end of turn. (Damage and effects that say "destroy" don't destroy it.)
1/1
*/
	return generic_shade(player, card, event, 0, MANACOST_XG(2, 1), 0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
}

// Gold

int card_cloudblazer(int player, int card, event_t event){
/*
Cloudblazer |3|W|U	0x200f3cc
Creature - Human Scout
Flying
When Cloudblazer enters the battlefield, you gain 2 life and draw two cards.
2/2
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
		draw_cards(player, 2);
	}

	return 0;
}

int card_contraband_kingpin(int player, int card, event_t event){
/*
Contraband Kingpin |U|B	0x200f3d1
Creature - Aetherborn Rogue
Lifelink
Whenever an artifact enters the battlefield under your control, scry 1.
1/4
*/
	lifelink(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		int result = new_specific_cip(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, &test);
		if( result ){
			while( result ){
					scry(player, 1);
					result--;
			}
		}
	}

	return 0;
}

static int is_vehicle_or_dwarf(int iid){
	if( has_subtype_by_id(cards_data[iid].id, SUBTYPE_DWARF) || has_subtype_by_id(cards_data[iid].id, SUBTYPE_VEHICLE) ){
		return 1;
	}
	return 0;
}

int card_depala_pilot_exemplar(int player, int card, event_t event){
/*
Depala, Pilot Exemplar |1|R|W	0x200f3d6
Legendary Creature - Dwarf Pilot
Other Dwarves you control get +1/+1.
Each Vehicle you control gets +1/+1 as long as it's a creature.
Whenever Depala, Pilot Exemplar becomes tapped, you may pay |X. If you do, reveal the top X cards of your library,
	put all Dwarf and Vehicle cards from among them into your hand, then put the rest on the bottom of your library in a random order.
3/3
*/
	check_legend_rule(player, card, event);

	boost_creature_type(player, card, event, SUBTYPE_DWARF, 1, 1, 0, BCT_CONTROLLER_ONLY);

	boost_creature_type(player, card, event, SUBTYPE_VEHICLE, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( event == EVENT_TAP_CARD && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( has_mana(player, COLOR_ANY, 1) ){
			int choice = player == HUMAN ? do_dialog(player, player, card, -1, -1, " Reveal x cards\n Pass", 0) : 0;
			if( choice == 0 ){
				charge_mana_while_resolving(player, card, event, player, COLOR_ANY, -1);
				if( spell_fizzled != 1){
					int amount = MIN(count_deck(player), x_value);
					if( amount ){
						int deck_copy[amount];
						int dcc = 0;
						int i;
						for(i=0; i<amount; i++){
							deck_copy[dcc] = deck_ptr[player][0];
							remove_card_from_deck(player, 0);
							dcc++;
						}
						APNAP(p, {show_deck(p, deck_copy, amount, "Cards revealed by Depala, Pilot Exemplar.", 0, 0x7375B0);};);
						for(i=amount-1; i > -1; i--){
							if( is_vehicle_or_dwarf(deck_copy[i]) ){
								add_card_to_hand(player, deck_copy[i]);
								dcc--;
							}
							else{
								raw_put_iid_on_top_of_deck(player, deck_copy[i]);
							}
						}
						if( dcc ){
							put_top_x_on_bottom_in_random_order(player, dcc);
						}
					}
				}
			}
		}
	}

	return 0;
}

int card_dovin_baan(int player, int card, event_t event){
/*
Dovin Baan |2|W|U	0x200f3db
Planeswalker - Dovin
+1: Until your next turn, up to one target creature gets -3/-0 and its activated abilities can't be activated.
-1: You gain 2 life and draw a card.
-7: You get an emblem with "Your opponents can't untap more than two permanents during their untap steps."
3
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 3);
	}

	enum{
		CHOICE_DISABLE_CREATURE = 1,
		CHOICE_GAIN_AND_DRAW,
		CHOICE_EMBLEM
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.allow_cancel = 3;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int priority_gain_draw = 20-life[player];
		priority_gain_draw += (14-(hand_count[player]*2));

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Disable target creature", 	1, 		10, 						+1,
							"Gain 2 life & draw", 		1, 						priority_gain_draw, 		-1,
							"Emblem", 					1, 						30, 						-7);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

       if( choice == CHOICE_DISABLE_CREATURE ){
				instance->number_of_targets = 0;
				new_pick_target(&td, "TARGET_CREATURE", 0, 0);
				if (instance->targets[0].card == -1){
					cancel = 1;
				}
	   }

	}
	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_DISABLE_CREATURE ){
			if( valid_target(&td) ){
				disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, 1);

				pump_ability_t pump;
				default_pump_ability_definition(player, card, &pump, -3, 0, 0, 0);
				pump.paue_flags = PAUE_END_AT_THE_BEGINNING_OF_YOUR_NEXT_TURN | PAUE_ENABLE_ALL_ACTIVATED_ABILITIES_AT_EOT;
				pump.eot_removal_method = KILL_DESTROY;
				pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
			}
		}

		if( choice == CHOICE_GAIN_AND_DRAW ){
			gain_life(player, 2);
			draw_cards(player, 1);
		}

		if( choice == CHOICE_EMBLEM ){
			generate_token_by_id(player, card, CARD_ID_DOVIN_BAAN_EMBLEM);
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_dovin_baan_eblem(int player, int card, event_t event){
/*
Dovin Baan Emblem	0x200f3e0
Emblem - Dovin
Your opponents can't untap more than two permanents during their untap steps.
*/
	if( current_turn == 1-player ){
		return card_static_orb(player, card, event);
	}

	return 0;
}

int card_empyreal_voyager(int player, int card, event_t event){
/*
Empyreal Voyager |1|G|U	0x200f3e5
Creature - Vedalken Scout
Flying, trample
Whenever Empyreal Voyager deals combat damage to a player, you get that many |E (energy counters).
2/3
*/

	if( damage_dealt_by_me(player, card, event, DDBM_REPORT_DAMAGE_DEALT | DDBM_MUST_DAMAGE_OPPONENT | DDBM_MUST_BE_COMBAT_DAMAGE) )
	{
		card_instance_t *instance = get_card_instance( player, card );
		add_energy(player, instance->targets[16].player);
		instance->targets[16].player = 0;
	}

	return 0;
}

int card_engineered_might(int player, int card, event_t event){
/*
Engineered Might |3|G|W	0x200f3ea
Sorcery
Choose one -
* Target creature gets +5/+5 and gains trample until end of turn.
* Creatures you control get +2/+2 and gain vigilance until end of turn.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_PUMP_TARGET = 1,
		CHOICE_PUMP_ALL,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(	event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"+5/+5 and trample to target creature", can_target(&td), 10,
								"+2/+2 and vigilance to all your crits", 1, 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_PUMP_TARGET ){
			if( ! pick_target(&td, "TARGET_CREATURE") ){
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;
		if( choice == CHOICE_PUMP_ALL ){
			int c;
			for(c=active_cards_count[player]; c > -1; c--){
				if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
				   alternate_legacy_text(2, player,  pump_ability_until_eot(player, card, player, c, 2, 2, 0, SP_KEYWORD_VIGILANCE));
			    }
			}
		}
		 if( choice == CHOICE_PUMP_TARGET ){
			if( valid_target(&td) ){
			   alternate_legacy_text(1, player, pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 5, 5, KEYWORD_TRAMPLE, 0));
			}
		 }
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

static int have_no_counter(int unused1, int unused2, int player, int card){
	return count_counters(player, card, COUNTER_invalid)== 0 ? 1 : 0;
}

int card_hazardous_conditions(int player, int card, event_t event){
/*
Hazardous Conditions |2|B|G	0x200f3ef
Sorcery
Creatures with no counters on them get -2/-2 until end of turn.
*/

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.special_selection_function = &have_no_counter;

		APNAP(p, {pump_creatures_until_eot(player, card, p, 0, -2, -2, 0, 0, &this_test);};);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_kambal_consul_of_allocation(int player, int card, event_t event){
/*
Kambal, Consul of Allocation |1|W|B	0x200f3f4
Legendary Creature - Human Advisor
Whenever an opponent casts a noncreature spell, that player loses 2 life and you gain 2 life.
2/3
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.type_flag = DOESNT_MATCH;
		if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_MANDATORY, &this_test) ){
			lose_life(1-player, 2);
			gain_life(player, 2);
		}
	}

	return 0;
}

int card_rashmi_eternities_crafter(int player, int card, event_t event){
/*
Rashmi, Eternities Crafter |2|G|U	0x200f3f9
Legendary Creature - Elf Druid
Whenever you cast your first spell each turn, reveal the top card of your library.
If it's a nonland card with converted mana cost less than that spell's, you may cast it without paying its mana cost.
If you don't cast the revealed card, put it into your hand.
2/3
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && get_specific_storm_count(player) == 1 &&
		deck_ptr[player][0] != -1 )
	{
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			int iid = deck_ptr[player][0];
			reveal_card_iid(player, card, iid);
			int add_to_hand = 1;
			if( ! is_what(-1, iid, TYPE_LAND) && can_legally_play_iid(player, iid) ){
				char msg [100];
				scnprintf(msg, 100, "Play %s", cards_ptr[cards_data[iid].id]->name);
				int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_SMALLCARD_ID(iid),
									msg, 		1, 10,
									"Decline", 	1, 5);
				if( choice == 1 ){
					play_card_in_deck_for_free(player, player, 0);
					add_to_hand = 0;
				}
			}
			if( add_to_hand ){
				add_card_to_hand(player, iid);
				remove_card_from_deck(player, 0);
			}
		}
	}

	return 0;
}

int card_restoration_gearsmith(int player, int card, event_t event){
/*
Restoration Gearsmith |2|W|B	0x200f3fe
Creature - Human Artificer
When Restoration Gearsmith enters the battlefield, return target artifact or creature card from your graveyard to your hand.
3/3
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ARTIFACT, "Select target artifact or creature card.");
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

static int test_not_in_array3(int iid, int arrptr, int player, int card)
{
  int* arr = (int*)arrptr;
  return iid != arr[0] && iid != arr[1] && iid != arr[2];
}

int card_saheeli_rai(int player, int card, event_t event){
/*
Saheeli Rai |1|U|R	0x200f403
Planeswalker - Saheeli
+1: Scry 1. Saheeli Rai deals 1 damage to each opponent.
-2: Create a token that's a copy of target artifact or creature you control, except it's an artifact in addition to its other types.
	That token gains haste. Exile it at the beginning of the next end step.
-7: Search your library for up to three artifact cards with different names, put them onto the battlefield, then shuffle your library.
3
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 3);
	}

	enum{
		CHOICE_SCRY_AND_DAMAGE = 1,
		CHOICE_COPY,
		CHOICE_THREE_ARTIFACTS
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_ARTIFACT);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int priority_copy = (count_subtype(player, TYPE_CREATURE, -1)-count_subtype(1-player, TYPE_CREATURE, -1))*5;

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Scry & damage", 						1,		15, 				+1,
							"Copy creature or artifact", 			can_target(&td), 						priority_copy, 		-2,
							"Tutor 3 artifacts", 					1, 						30, 				-7);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_COPY ){
			if( ! new_pick_target(&td, "Select target artifact or creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				return 0;
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_SCRY_AND_DAMAGE ){
			scry(player, 1);
			damage_player(1-player, 1, player, card);
		}

		if( choice == CHOICE_COPY ){
			if( valid_target(&td) ){
				token_generation_t token;
				default_token_definition(instance->parent_controller, instance->parent_card,
										get_id(instance->targets[0].player, instance->targets[0].card), &token);
				if( ! is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ARTIFACT) ){
					token.action = TOKEN_ACTION_CONVERT_INTO_ARTIFACT;
				}
				token.special_code_on_generation = &haste_and_sacrifice_token_at_end_of_next_turn;
				generate_token(&token);
			}
		}

		if( choice == CHOICE_THREE_ARTIFACTS ){
			int chosen[3] = {-1, -1, -1};

			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select up to three artifact cards with different names.");
			test.value_for_special_selection_function = (int)chosen;
			test.special_selection_function = test_not_in_array3;
			test.qty = 3;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_unlicensed_disintegration(int player, int card, event_t event){
/*
Unlicensed Disintegration |1|B|R	0x200f408
Instant
Destroy target creature. If you control an artifact, Unlicensed Disintegration deals 3 damage to that creature's controller.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( count_subtype(player, TYPE_ARTIFACT, -1) ){
				damage_player(instance->targets[0].player, 3, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/*
Veteran Motorist |R|W	0x200ae58 (Horizon Scholar, the rest of the effect is in "vehicle")
Creature - Dwarf Pilot
When Veteran Motorist enters the battlefield, scry 2.
Whenever Veteran Motorist crews a Vehicle, that Vehicle gets +1/+1 until end of turn.
3/1
*/

int card_voltaic_brawler(int player, int card, event_t event){
/*
Voltaic Brawler |R|G	0x200f40d
Creature - Human Warrior
When Voltaic Brawler enters the battlefield, you get |E|E (two energy counters).
Whenever Voltaic Brawler attacks, you may pay |E. If you do, it gets +1/+1 and gains trample until end of turn.
3/2
*/
	etb_energy(player, card, event, 2);

	if( attack_trigger_with_energy(player, card, event, 1) ){
		remove_energy(player, 1);
		pump_ability_until_eot(player, card, player, card, 1, 1, KEYWORD_TRAMPLE, 0);
	}

	return 0;
}

int card_whirler_virtuoso(int player, int card, event_t event){
/*
Whirler Virtuoso |1|U|R	0x200f412
Creature - Vedalken Artificer
When Whirler Virtuoso enters the battlefield, you get |E|E|E (three energy counters).
Pay |E|E|E: Create a 1/1 colorless Thopter artifact creature token with flying.
2/3
*/
	etb_energy(player, card, event, 3);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		make_thopter(instance->parent_controller, instance->parent_card, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 3), NULL, NULL);
}

// Artifact

/*
Accomplished Automaton |7	0x200f147 (Glint-Sleeve Artisan)
Artifact Creature - Construct
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
5/7
*/

int card_aetherflux_reservoir(int player, int card, event_t event){
	/* Aetherflux Reservoir	|4	0x200f417
	 * Artifact
	 * Whenever you cast a spell, you gain 1 life for each spell you've cast this turn.
	 * Pay 50 life: ~ deals 50 damage to any target. */
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) )
	{
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			gain_life(player, get_specific_storm_count(player));
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 50);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 50, &td, "TARGET_ANY");
}

static int aetherworks_marvel_legacy(int player, int card, event_t event){

	//In case something is copying Demon of Dark Schemes and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_AETHERWORKS_MARVEL ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
					}
				}
				count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			add_energy(player, amount);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_aetherworks_marvel(int player, int card, event_t event){
/*
Aetherworks Marvel |4
Legendary Artifact
Whenever a permanent you control is put into a graveyard, you get |E (an energy counter).
|T, Pay |E|E|E|E|E|E: Look at the top six cards of your library. You may cast a card from among them without paying its mana cost. Put the rest on the bottom of your library in a random order.
*/

	check_legend_rule(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)aetherworks_marvel_legacy ){
					if( (inst->damage_target_player == player && inst->damage_target_card == card) ||
						(inst->targets[0].player == player && inst->targets[0].card == card) )
					{
						found = 1;
						break;
					}
				}
			}
		}
		if( ! found ){
			int legacy = create_targetted_legacy_effect(player, card, &aetherworks_marvel_legacy, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( amount ){
			add_energy(player, amount);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = MIN(6, count_deck(player));
		if( amount ){
			APNAP(p, {show_deck(p, deck_ptr[player], amount, "Cards revealed by Aetherworks Marvels", 0, 0x7375B0 );};);
			int playable[2][amount];
			int pc = 0;
			int i;
			for(i=0; i<amount; i++){
				if( ! is_what(-1, deck_ptr[player][i], TYPE_LAND) && can_legally_play_iid(player, deck_ptr[player][i]) ){
					playable[0][pc] = deck_ptr[player][i];
					playable[1][pc] = i;
					pc++;
				}
			}
			if( pc ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Select a card to cast for free.");
				int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_CMC, -1, &test);
				if( selected != -1 ){
					play_card_in_deck_for_free(player, player, playable[1][selected]);
					pc--;
				}
				if( pc ){
					put_top_x_on_bottom_in_random_order(player, pc);
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 6), NULL, NULL);
}

int card_animation_module(int player, int card, event_t event)
{
/*
Animation Module |1	0x200f41c
Artifact
Whenever one or more +1/+1 counters are placed on a permanent you control, you may pay |1. If you do, create a 1/1 colorless Servo artifact creature token.
|3, |T: Choose a counter on target permanent or player. Give that permanent or player another counter of that kind.
*/

	enable_xtrigger_flags |= ENABLE_XTRIGGER_1_1_COUNTERS;
	if( xtrigger_condition() == XTRIGGER_1_1_COUNTERS && affect_me(player, card) && player == reason_for_trigger_controller &&
		trigger_cause_controller == player && !is_humiliated(player, card) && has_mana(player, COLOR_ANY, 1))
	{
		if (event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_AI(player);
		}
		if (event == EVENT_RESOLVE_TRIGGER){
			charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1){
				make_servo(player, card, 1);
			}
		}
	}

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
		td.extra = 0;
		SET_BYTE0(td.extra) = COUNTER_invalid;
		if( ! count_player_counters(0, PCOUNTER_invalid) || ! count_player_counters(1, PCOUNTER_invalid) ){
			td.zone = TARGET_ZONE_IN_PLAY;	// no targettable players
		}
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			enum{
				CHOICE_AUTO = 1,
				CHOICE_MANUAL
			};
			int choice = CHOICE_AUTO;
			if( player != AI ){
				choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL,
								"Automatic", 1, 1,
								"Manual", 1, 1);

			}
			if( choice == CHOICE_AUTO )
			{
				proliferate_auto_single_permanent_or_player(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			if( choice == CHOICE_MANUAL )
			{
				proliferate_manual_single_permanent_or_player(player, card, instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0,
									&td, "Select target permanent or player.");
}


int card_aradara_express(int player, int card, event_t event){
/*
Aradara Express |5	0x200f421
Artifact - Vehicle
Menace
Crew 4 (Tap any number of creatures you control with total power 4 or more: This Vehicle becomes an artifact creature until end of turn.)
8/6
*/
	menace(player, card, event);
	return vehicle(player, card, event, 4);
}

int card_ballista_charger(int player, int card, event_t event){
	/* Ballista Charger	|5	0x200f426
	 * Artifact - Vehicle 6/6
	 * Whenever ~ attacks, it deals 1 damage to any target.
	 * Crew 3 */
	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		get_card_instance(player, card)->number_of_targets = 0;

		if( pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 1);
		}
	}

	return vehicle(player, card, event, 3);
}

/*
Bastion Mastodon |5	0x200448b (Bladed Sentinel)
Artifact Creature - Elephant
|W: Bastion Mastodon gains vigilance until end of turn.
4/5
*/

int card_bomat_bazaar_barge(int player, int card, event_t event){
/*
Bomat Bazaar Barge |4	0x200f42b
Artifact - Vehicle
When Bomat Bazaar Barge enters the battlefield, draw a card.
Crew 3 (Tap any number of creatures you control with total power 3 or more: This Vehicle becomes an artifact creature until end of turn.)
5/5
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return vehicle(player, card, event, 3);
}

int card_bomat_courier(int player, int card, event_t event){
/*
Bomat Courier |1	0x200f430
Artifact Creature - Construct
Haste
Whenever Bomat Courier attacks, exile the top card of your library face down. (You can't look at it.)
|R, Discard your hand, Sacrifice Bomat Courier: Put all cards exiled with Bomat Courier into their owners' hands.
1/1
*/
	haste(player, card, event);

	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		int iid = deck_ptr[player][0];
		int leg;
		int idx;
		exiledby_remember(player, card, player, iid, &leg, &idx);
		rfg_top_card_of_deck(player);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		new_discard_all(player, player);
		get_card_instance( player, card )->targets[1].card = exiledby_detach(player, card);
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		int leg = 0;
		int idx = 0;
		int* loc;
		while ((loc = exiledby_find_any(player-2, instance->targets[1].card, &leg, &idx)) != NULL){
				int owner = (*loc & 0x80000000) ? 1 : 0;
				int iid = *loc & ~0x80000000;
				add_card_to_hand(owner, iid);
				remove_card_from_rfg(owner, cards_data[iid].id);
				*loc = -1;
		}
	}

	return 0;
}

// Chief of the Foundry: reprint ORI

int card_cogworks_puzzleknot(int player, int card, event_t event){
/*
Cogworker's Puzzleknot |2	0x200f435
Artifact
When Cogworker's Puzzleknot enters the battlefield, create a 1/1 colorless Servo artifact creature token.
|1|W, Sacrifice Cogworker's Puzzleknot: Create a 1/1 colorless Servo artifact creature token.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || event == EVENT_RESOLVE_ACTIVATION ){
		make_servo(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XW(1, 1), 0, NULL, NULL);
}

/*
Consulate Skygate |2	0x401000 (vanilla)
Artifact Creature - Wall
Defender
Reach (This creature can block creatures with flying.)
0/4
*/

int card_cultivators_caravan(int player, int card, event_t event){
/*
Cultivator's Caravan |3	0x200f43a
Artifact - Vehicle
|T: Add one mana of any color to your mana pool.
Crew 3 (Tap any number of creatures you control with total power 3 or more: This Vehicle becomes an artifact creature until end of turn.)
5/5
*/
	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		return mana_producer(player, card, event);
	}

	if( ! IS_GAA_EVENT(event)  ){
		return 0;
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_VEHICLE
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( vehicle(player, card, event, 3) ){
			return 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		int can_produce_mana = mana_producer(player, card, EVENT_CAN_ACTIVATE);
		int can_transform = vehicle(player, card, EVENT_CAN_ACTIVATE, 3);
		int priority_vehicle = (current_phase < PHASE_DECLARE_ATTACKERS && current_turn == player) ||
								(current_phase < PHASE_DECLARE_BLOCKERS && current_turn == 1-player) ? 10 : 1;
		if( ! paying_mana() ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", can_produce_mana, 5,
							"Animate as Vehicle", can_transform, priority_vehicle);
			if( ! choice ){
				cancel = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_VEHICLE ){
			return vehicle(player, card, event, 3);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_VEHICLE ){
			return vehicle(player, card, event, 3);
		}
	}

	return 0;
}

static int re_enable_abilities_at_eot(int player, int card, event_t event){

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			disable_all_activated_abilities(instance->damage_target_player, instance->damage_target_card, ALL_AB_ENABLE);
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_deadlock_trap(int player, int card, event_t event){
/*
Deadlock Trap |3	0x200f43f
Artifact
Deadlock Trap enters the battlefield tapped.
When Deadlock Trap enters the battlefield, you get |E|E (two energy counters).
|T, Pay |E: Tap target creature or planeswalker. Its activated abilities can't be activated this turn.
*/

	comes_into_play_tapped(player, card, event);

	etb_energy(player, card, event, 2);

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);

	if(event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance(player, card);
		tap_card(instance->targets[0].player, instance->targets[0].card);
		disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, ALL_AB_DISABLE);
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &re_enable_abilities_at_eot,
										instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0,
									GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), &td, "Select target creature or planeswalker.");
}

int card_decoction_module(int player, int card, event_t event){
/*
Decoction Module |2	0x200f444
Artifact
Whenever a creature enters the battlefield under your control, you get |E (an energy counter).
|4, |T: Return target creature you control to its owner's hand.
*/
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		int result = new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test);
		if( result ){
			add_energy(player, result);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		bounce_permanent(instance->targets[0].player, instance->targets[0].card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(4), 0,
									&td, "Select target creature you control.");
}

int card_demolition_stomper(int player, int card, event_t event){
/*
Demolition Stomper |6	0x200f449
Artifact - Vehicle
Demolition Stomper can't be blocked by creatures with power 2 or less.
Crew 5 (Tap any number of creatures you control with total power 5 or more: This Vehicle becomes an artifact creature until end of turn.)
10/7
*/

	if( ! is_humiliated(player, card) ){
		if( event == EVENT_BLOCK_LEGALITY && attacking_card_controller == player && attacking_card == card ){
			if( get_power(affected_card_controller, affected_card) <= 2 ){
				event_result = 1;
			}
		}
	}

	return vehicle(player, card, event, 5);
}


/*
Dukhara Peafowl |4	0x2004512 (Gust Skimmer)
Artifact Creature - Bird
|U: Dukhara Peafowl gains flying until end of turn.
2/4
*/

int card_dynavolt_tower(int player, int card, event_t event){
	/* Dynavolt Tower	|3	0x200f44e
	 * Artifact
	 * Whenever you cast an instant or sorcery spell, you get |E|E.
	 * |T, Pay |E|E|E|E|E: ~ deals 3 damage to any target. */
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &this_test) ){
			add_energy(player, 2);
		}
	}

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		damage_target0(player, card, 3);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
									GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 5), &td, "TARGET_ANY");
}


int card_eager_construct(int player, int card, event_t event){
/*
Eager Construct |2	0x200f453
Artifact Creature - Construct
When Eager Construct enters the battlefield, each player may scry 1.
(To scry 1, look at the top card of your library, then you may put that card on the bottom of your library.)
2/2
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		APNAP(p, {scry(p, 1);};);
	}

	return 0;
}

int card_electrostatic_pummerler(int player, int card, event_t event){
/*
Electrostatic Pummeler |3	0x200f458
Artifact Creature - Construct
When Electrostatic Pummeler enters the battlefield, you get |E|E|E (three energy counters).
Pay |E|E|E: Electrostatic Pummeler gets +X/+X until end of turn, where X is its power.
1/1
*/
	etb_energy(player, card, event, 3);

	if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST){
		int initial_power = get_power(player, card);
		if (initial_power > 0){
			int final_power, num_times_can_activate = ENERGY_COUNTERS(player)/3;
			for (final_power = initial_power; num_times_can_activate > 0; --num_times_can_activate){
				final_power *= 2;
			}
			return final_power - initial_power;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return card_yew_spirit(player, card, event); //This gets +X/+X until end of turn, where X is its power.
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 3), NULL, NULL);
}

int card_fabrication_module(int player, int card, event_t event)
{
/*
Fabrication Module |3	0x200f458
Artifact
Whenever you get one or more |E (energy counters), put a +1/+1 counter on target creature you control.
|4, |T: You get |E.
*/
	enable_xtrigger_flags |= XTRIGGER_COUNTER_ENERGY;
	if( xtrigger_condition() == XTRIGGER_COUNTER_ENERGY && affect_me(player, card) && trigger_cause_controller == player &&
		reason_for_trigger_controller == player && ! is_humiliated(player, card))
	{
		if (event == EVENT_TRIGGER){
		    event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
	    if (event == EVENT_RESOLVE_TRIGGER){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		   if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE_YOU_CONTROL", 0, 0) ){
			  add_1_1_counter(instance->targets[0].player, instance->targets[0].card);
		   }
		   instance->number_of_targets = 0;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		add_energy(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_filigree_familiar(int player, int card, event_t event)
{
/*
Filigree Familiar |3	0x200f462
Artifact Creature - Fox
When Filigree Familiar enters the battlefield, you gain 2 life.
When Filigree Familiar dies, draw a card.
2/2
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 2);
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_fireforgers_puzzleknot(int player, int card, event_t event){
	/* Fireforger's Puzzleknot	|2	0x200f467
	 * Artifact
	 * When ~ enters the battlefield, it deals 1 damage to any target.
	 * |2|R, Sacrifice ~: It deals 1 damage to any target. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( ! IS_GAA_EVENT(event) && event != EVENT_ETB_ABILITY ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	if( event == EVENT_ETB_ABILITY ){
		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XR(2, 1), 0,
									&td, "TARGET_ANY");
}

int card_fleetwheel_cruiser(int player, int card, event_t event){
/*
Fleetwheel Cruiser |4	0x200f46c
Artifact - Vehicle
Trample, haste
When Fleetwheel Cruiser enters the battlefield, it becomes an artifact creature until end of turn.
Crew 2 (Tap any number of creatures you control with total power 2 or more: This Vehicle becomes an artifact creature until end of turn.)
5/3
*/
	if( comes_into_play(player, card, event) ){
		turn_into_creature(player, card, player, card, 1, 0, 0);
	}

	haste(player, card, event);

	return vehicle(player, card, event, 2);
}

int card_foundry_inspector(int player, int card, event_t event){
/*
Foundry Inspector |3	0x200f471
Artifact Creature - Construct
Artifact spells you cast cost |1 less to cast.
3/2
*/
	if (event == EVENT_MODIFY_COST_GLOBAL)
	{
		if( (affected_card_controller == -1 ? event_result : affected_card_controller) == player &&
			is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
			in_play(player, card) && ! is_humiliated(player, card) )
		{
			COST_COLORLESS--;
		}
		return 0;
	}

	return 0;
}

int card_ghirapur_orrery(int player, int card, event_t event){
/*
Ghirapur Orrery |4	0x200f476
Artifact
Each player may play an additional land on each of his or her turns.
At the beginning of each player's upkeep, if that player has no cards in hand, that player draws three cards.
*/
	check_playable_lands(player);

	if( upkeep_trigger_mode(player, card, event, hand_count[current_turn] == 0 ? RESOLVE_TRIGGER_MANDATORY : 0) ){
		draw_cards(current_turn, 3);
	}

	return 0;
}

int card_glassblowers_puzzleknot(int player, int card, event_t event){
/*
Glassblower's Puzzleknot |2	0x200f47b
Artifact
When Glassblower's Puzzleknot enters the battlefield, scry 2, then you get |E|E. (You get two energy counters. To scry 2, look at the top two cards of your library, then put any number of them on the bottom of your library and the rest on top in any order.)
|2|U, Sacrifice Glassblower's Puzzleknot: Scry 2, then you get |E|E.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || event == EVENT_RESOLVE_ACTIVATION ){
		scry(player, 2);
		add_energy(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XU(2, 1), 0, NULL, NULL);
}

int card_inventors_goggles(int player, int card, event_t event){
/*
Inventor's Goggles |1	0x200f480
Artifact - Equipment
Equipped creature gets +1/+2.
Whenever an Artificer enters the battlefield under your control, you may attach Inventor's Goggles to it.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.)
*/

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_ARTIFICER;

		int trig_mode = player == HUMAN ? RESOLVE_TRIGGER_OPTIONAL : 0;
		if( player == AI ){
			if( is_equipping(player, card) ){
				card_instance_t *instance = get_card_instance( player, card );
				int p = instance->damage_target_player;
				int c = instance->damage_target_card;
				if( get_base_value(p, c) < get_base_value(trigger_cause_controller, trigger_cause) ){
					trig_mode = RESOLVE_TRIGGER_MANDATORY;
				}
			}
		}

		if( new_specific_cip(player, card, event, player, trig_mode, &test) ){
			equip_target_creature(player, card, trigger_cause_controller, trigger_cause);
		}
	}

	return vanilla_equipment(player, card, event, 2, 1, 2, 0, 0);
}

int card_iron_league_steed(int player, int card, event_t event){
/*
Iron League Steed |4	0x200f485
Artifact Creature - Construct
Haste
Fabricate 1 (When this creature enters the battlefield, put a +1/+1 counter on it or create a 1/1 colorless Servo artifact creature token.)
2/2
*/
	haste(player, card, event);
	etb_fabricate(player, card, event, 1, count_subtype(player, TYPE_CREATURE, -1)*5, 10);
	return 0;
}

int card_key_to_the_city(int player, int card, event_t event){
/*
Key to the City |2	0x200f48a
Artifact
|T, Discard a card: Up to one target creature can't be blocked this turn.
Whenever Key to the City becomes untapped, you may pay |2. If you do, draw a card.
*/
	if( inspired(player, card, event) && has_mana(player, COLOR_ANY, 2) &&
		DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, "Draw a card", 1, 1) &&
		charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 2) )
	{
		draw_cards(player, 1);
	}

	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		add_state(player, card, STATE_TAPPED);
		instance->number_of_targets = 0;
		discard(player, 0, player);
		new_pick_target(&td, "TARGET_CREATURE", 0, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->number_of_targets == 1 && valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return 0;
}

int card_metalspinners_puzzleknot(int player, int card, event_t event){
/*
Metalspinner's Puzzleknot |2	0x200f48f
Artifact
When Metalspinner's Puzzleknot enters the battlefield, you draw a card and you lose 1 life.
|2|B, Sacrifice Metalspinner's Puzzleknot: You draw a card and you lose 1 life.
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
		lose_life(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XB(2, 1), 0, NULL, NULL);
}


int card_metalwork_colossus(int player, int card, event_t event){
/*
Metalwork Colossus |11	0x200f494
Artifact Creature - Construct
Metalwork Colossus costs |X less to cast, where X is the total converted mana cost of noncreature artifacts you control.
Sacrifice two artifacts: Return Metalwork Colossus from your graveyard to your hand.
10/10
*/
	if( event == EVENT_MODIFY_COST ){
		int rc = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_ARTIFACT) && ! is_what(p, c, TYPE_CREATURE) ){
										rc += get_cmc(p, c);
									}
								};
		);
		COST_COLORLESS-=rc;
	}

	if( event == EVENT_GRAVEYARD_ABILITY ){
		if( can_sacrifice_type_as_cost(player, 2, TYPE_ARTIFACT) ){
			return GA_RETURN_TO_HAND;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
		int sac1 = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_AS_COST | SAC_RETURN_CHOICE, &this_test);
		if( sac1 ){
			int sac2 = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_AS_COST | SAC_RETURN_CHOICE, &this_test);
			if( sac2 ){
				kill_card(BYTE2(sac1), BYTE3(sac1), KILL_SACRIFICE);
				kill_card(BYTE2(sac2), BYTE3(sac2), KILL_SACRIFICE);
				return GAPAID_REMOVE;
			}
			if( !sac2 ){
				state_untargettable(BYTE2(sac1), BYTE3(sac1), 0);
				cancel = 1;
				return 0;
			}
		}
		else{
			cancel = 1;
		}
	}

	return 0;
}

int card_multiform_wonder(int player, int card, event_t event){
/*
Multiform Wonder |5	0x200f499
Artifact Creature - Construct
When Multiform Wonder enters the battlefield, you get |E|E|E (three energy counters).
Pay |E: Multiform Wonder gains your choice of flying, vigilance, or lifelink until end of turn.
Pay |E: Multiform Wonder gets +2/-2 or -2/+2 until end of turn.
3/3
*/
	etb_energy(player, card, event, 3);

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card)->targets[1].player = 0;
	}

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( ENERGY_COUNTERS(player) ){
			if( current_turn == player && is_attacking(player, card) ){
				return generic_shade(player, card, event, 0, MANACOST0, 2, -2, 0, 0);
			}
			if( current_turn == 1-player && get_card_instance(player, card)->blocking > 0 ){
				return generic_shade(player, card, event, 0, MANACOST0, -2, 2, 0, 0);
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card);

	enum{
		CHOICE_ABILITY,
		CHOICE_PUMP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_PLAYER_COUNTERS(PCOUNTER_ENERGY, 1), NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int priority_use_ability = 0;
		if( player == AI ){
			// Luckily enought, the three abilities you could add doesn't overlap, being in two different sets
			int keyword_already_added = instance->targets[1].player ;
			if( !( keyword_already_added & KEYWORD_FLYING) ){
				if( current_phase < PHASE_DECLARE_BLOCKERS ){
					priority_use_ability += 5;
				}
			}
			if( !( keyword_already_added & SP_KEYWORD_VIGILANCE) ){
				if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
					priority_use_ability += 5;
				}
			}
			if( !( keyword_already_added & SP_KEYWORD_LIFELINK) ){
				if( life[player] < 6 ){
					priority_use_ability += 10;
				}
			}
		}
		int priority_pump = 0;
		if( player == AI ){
			if( (current_turn == player && is_attacking(player, card)) || (current_turn == 1-player && instance->blocking > 0) )
			{
				priority_pump += 10;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Give ability", 1, priority_use_ability,
							"Pump", 1, priority_pump);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		remove_energy(player, 1);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_ABILITY ){
			enum{
					C2_FLYING = 1,
					C2_VIGILANCE,
					C2_LIFELINK
			};
			int key = 0;
			int s_key = 0;
			int priority_flying = 0;
			int priority_vigilance = 0;
			int priority_lifelink = 0;
			if( player == AI ){
				int keyword_already_added = instance->targets[1].player;
				if( !( keyword_already_added & SP_KEYWORD_VIGILANCE) ){
					if( current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ){
						priority_vigilance = 10;
					}
				}
				if( !( keyword_already_added & KEYWORD_FLYING) ){
					if( current_phase < PHASE_DECLARE_BLOCKERS ){
						priority_vigilance = 0;
						priority_flying = 10;
					}
				}
				if( !( keyword_already_added & SP_KEYWORD_LIFELINK) ){
					if( life[player] < 6 ){
						priority_flying = 0;
						priority_vigilance = 0;
						priority_lifelink = 10;
					}
				}
			}
			int choice2 = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE
								"Flyng", 1, priority_lifelink,
								"Vigilance", 1, priority_flying,
								"Lifelink", 1, priority_vigilance);

			switch( choice2 ){
					case C2_FLYING:
						key = KEYWORD_FLYING;
						break;
					case C2_VIGILANCE:
						s_key = SP_KEYWORD_VIGILANCE;
						break;
					case C2_LIFELINK:
						s_key = SP_KEYWORD_LIFELINK;
						break;
			}
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, key, s_key);
			if( get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player == -1 ){
				get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player = 0;
			}
			get_card_instance(instance->parent_controller, instance->parent_card)->targets[1].player |= (key | s_key);
		}
		if( choice == CHOICE_PUMP ){
			enum{
					C2_P2_M2,
					C2_M2_P2,
			};
			int priority_P2_M2 = 0;
			int priority_M2_P2 = 0;
			if( player == AI ){
				if( current_turn == player && is_attacking(player, card) )
				{
					priority_P2_M2 = 10;
				}
				if( current_turn == 1-player && instance->blocking > 0 )
				{
					priority_M2_P2 = 10;
				}
			}
			int choice2 = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE
								"+2/-2", 1, priority_P2_M2,
								"-2/+2", 1, priority_M2_P2);
			int pow = 0;
			int tou = 0;
			switch( choice2 ){
					case C2_P2_M2:
					{
						pow = 2;
						tou = -2;
						break;
					}
					case C2_M2_P2:
					{
						pow = -2;
						tou = 2;
						break;
					}
			}
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, pow, tou);
		}
	}

	return 0;
}

int card_narnam_cobra(int player, int card, event_t event){
/*
Narnam Cobra |2	0x200f49e
Artifact Creature - Snake
|G: Narnam Cobra gains deathtouch until end of turn. (Any amount of damage it deals to a creature is enough to destroy it.)
2/1
*/
	return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_ovalchase_dragster(int player, int card, event_t event){
/*
Ovalchase Dragster |4	0x200f4a3
Artifact - Vehicle
Trample, haste
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
6/1
*/
	haste(player, card, event);
	return vehicle(player, card, event, 1);
}

/*
Panharmonicon |4	0x401000 (vanilla, the effect is handled in "etb_ability" and "new_specific_cip")
Artifact
If an artifact or creature entering the battlefield causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time.
*/

int card_perpetual_timepiece(int player, int card, event_t event){
/*
Perpetual Timepiece |2	0x200f4a8
Artifact
|T: Put the top two cards of your library into your graveyard.
|2, Exile Perpetual Timepiece: Shuffle any number of target cards from your graveyard into your library.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_MILL = 1,
		CHOICE_SHUFFLE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Mill 2", 1, 15,
							"Shuffle grave into deck", 1, count_graveyard(player)*2);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MILL ){
			ai_modifier += 15;
		}
		SET_BYTE0(instance->info_slot) += choice;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			switch( choice ){
					case CHOICE_MILL:
						tap_card(player, card);
						break;
					case CHOICE_SHUFFLE:
					{
						test_definition_t test;
						new_default_test_definition(&test, TYPE_ANY, "Select target card to shuffle into deck.");

						int max_targ = player == HUMAN ? count_graveyard(player) : internal_rand(count_graveyard(player))+1;

						int amount = select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &test, max_targ,
																			&instance->targets[0]);
						SET_BYTE1(instance->info_slot) += amount;

						kill_card(player, card, KILL_EXILE);
						break;
					}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = BYTE0(instance->info_slot);
		if( choice == CHOICE_MILL ){
			mill(player, 2);
		}
		if( choice == CHOICE_SHUFFLE ){
			int i;
			for(i=BYTE1(instance->info_slot)-1; i > -1; i--){
				int selected = validate_target_from_grave_source(player, card, player, i);
				if( selected != -1 ){
					from_graveyard_to_deck(player, selected, 1);
				}
			}
			shuffle(player);
		}
	}

	return 0;
}

int card_prakhata_pillar_bug(int player, int card, event_t event){
/*
Prakhata Pillar-Bug |3	0x200f4ad
Artifact Creature - Insect
|B: Prakhata Pillar-Bug gains lifelink until end of turn. (Damage dealt by this creature also causes you to gain that much life.)
2/3
*/
	return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_LIFELINK);
}


// Prophetic Prism: reprint ROE

int card_renegade_freighter(int player, int card, event_t event){
/*
Renegade Freighter |3	0x200f4b2
Artifact - Vehicle
Whenever Renegade Freighter attacks, it gets +1/+1 and gains trample until end of turn.
Crew 2 (Tap any number of creatures you control with total power 2 or more: This Vehicle becomes an artifact creature until end of turn.)
4/3
*/
	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card) ){
		pump_ability_until_eot(player, card, player, card, 1, 1, KEYWORD_TRAMPLE, 0);
	}

	return vehicle(player, card, event, 2);
}

int card_scrapheap_scrounger(int player, int card, event_t event){
/*
Scrapheap Scrounger |2	0x200f4b7
Artifact Creature - Construct
Scrapheap Scrounger can't block.
|1|B, Exile another creature card from your graveyard: Return Scrapheap Scrounger from your graveyard to the battlefield.
3/2
*/

	cannot_block(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 1 ){
			return GA_RETURN_TO_PLAY;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XB(1, 1));
		if( spell_fizzled != 1 ){
			if( gather_body_parts(player, card, 1, 1) ){
				return GAPAID_REMOVE;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	return 0;
}

int card_self_assembler(int player, int card, event_t event){
/*
Self-Assembler |5	0x200f4bc
Artifact Creature - Assembly-Worker
When Self-Assembler enters the battlefield, you may search your library for an Assembly-Worker creature card, reveal it,
put it into your hand, then shuffle your library.
4/4
*/
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select an Assembly-Worker creature card.");
		test.subtype = SUBTYPE_ASSEMBLY_WORKER;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_sky_stiff(int player, int card, event_t event){
/*
Sky Skiff |2	0x200f4c1
Artifact - Vehicle
Flying
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
2/3
*/
/*
Sleek Schooner	|3
Artifact - Vehicle 4/3
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
*/
	return vehicle(player, card, event, 1);
}

static void skysovereign_consul_flagship_ability(int player, int card){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
	td.allowed_controller = td.preferred_controller = 1-player;

	if( can_target(&td) && new_pick_target(&td, "Select target creature or planeswalker an opponent controls.", 0, GS_LITERAL_PROMPT) ){
		damage_target0(player, card, 3);
	}
	get_card_instance(player, card)->number_of_targets = 0;
}

int card_skysovereign_consul_flagship(int player, int card, event_t event){
/*
Skysovereign, Consul Flagship |5	0x200f4c6
Legendary Artifact - Vehicle
Flying
Whenever Skysovereign, Consul Flagship enters the battlefield or attacks, it deals 3 damage to target creature or planeswalker
	an opponent controls.
Crew 3 (Tap any number of creatures you control with total power 3 or more: This Vehicle becomes an artifact creature until end of turn.)
6/5
*/
	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.allowed_controller = td.preferred_controller = 1-player;

		if( can_target(&td) ){
			etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		skysovereign_consul_flagship_ability(player, card);
	}

	store_attackers(player, card, event, 0, player, card, NULL);

	if (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card) && reason_for_trigger_controller == player)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.allowed_controller = td.preferred_controller = 1-player;

		if( can_target(&td) && resolve_declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			skysovereign_consul_flagship_ability(player, card);
		}
	}

	return vehicle(player, card, event, 3);
}

int card_smugglers_copter(int player, int card, event_t event){
/*
Smuggler's Copter |2	0x200f4cb
Artifact - Vehicle
Flying
Whenever Smuggler's Copter attacks or blocks, you may draw a card. If you do, discard a card.
Crew 1 (Tap any number of creatures you control with total power 1 or more: This Vehicle becomes an artifact creature until end of turn.)
3/3
*/
	if( declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_CHECK_DECK_COUNT(player, 1), player, card) ||
		blocking(player, card, event) )
	{
		draw_cards(player, 1);
		discard(player, 0, player);
	}

	return vehicle(player, card, event, 1);
}

/*
Snare Thopter |4	0x200bb46 (Raging Goblin)
Artifact Creature - Thopter
Flying, haste
3/2
*/

int card_torch_gauntlet(int player, int card, event_t event){
/*
Torch Gauntlet |2	0x200f4d0
Artifact - Equipment
Equipped creature gets +2/+0.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.)
*/
	return vanilla_equipment(player, card, event, 2, 2, 0, 0, 0);
}

int card_weldafast_monitor(int player, int card, event_t event){
/*
Weldfast Monitor |3	0x200f4d5
Artifact Creature - Lizard
|R: Weldfast Monitor gains menace until end of turn.
3/2
*/
	return generic_shade(player, card, event, 0, MANACOST_R(1), 0, 0, 0, SP_KEYWORD_MENACE);
}

int card_whirlemaker(int player, int card, event_t event){
/*
Whirlermaker |3	0x200f4da
Artifact
|4, |T: Create a 1/1 colorless Thopter artifact creature token with flying.
*/

	if (event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		make_thopter(instance->parent_controller, instance->parent_card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
}

int card_woodweavers_puzzleknot(int player, int card, event_t event){
/*
Woodweaver's Puzzleknot |2	0x200f4df
Artifact
When Woodweaver's Puzzleknot enters the battlefield, you gain 3 life and get |E|E|E (three energy counters).
|2|G, Sacrifice Woodweaver's Puzzleknot: You gain 3 life and get |E|E|E.
*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY || event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
		add_energy(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(2, 1), 0, NULL, NULL);
}


/*
Workshop Assistant |3	0x2003b5d (Junk Diver)
Artifact Creature - Construct
When Workshop Assistant dies, return another target artifact card from your graveyard to your hand.
1/2
*/

// Land

int card_aether_hub(int player, int card, event_t event){
/*
Aether Hub	0x200f4e4
Land
When Aether Hub enters the battlefield, you get |E (an energy counter).
|T: Add |C to your mana pool.
|T, Pay |E: Add one mana of any color to your mana pool.
*/

	etb_energy(player, card, event, 1);

	if( event != EVENT_COUNT_MANA && event != EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) ){
			if( ENERGY_COUNTERS(player) ){
				declare_mana_available(player, COLOR_ANY, 1);
			}
			else{
				declare_mana_available(player, COLOR_COLORLESS, 1);
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		color_test_t colors = ENERGY_COUNTERS(player) ? get_card_instance(player, card)->mana_color : COLOR_TEST_COLORLESS;
		produce_mana_tapped_all_one_color(player, card, colors, 1);
		if( chosen_colors & ~COLOR_TEST_COLORLESS ){
			remove_energy(player, 1);
		}
	}

	return 0;
}

/*
Blooming Marsh	0x2003c0c (card_generic_som_tapland)
Land
Blooming Marsh enters the battlefield tapped unless you control two or fewer other lands.
|T: Add |B or |G to your mana pool.
*/

/*
Botanical Sanctum	0x2003c0c (card_generic_som_tapland)
Land
Botanical Sanctum enters the battlefield tapped unless you control two or fewer other lands.
|T: Add |G or |U to your mana pool.
*/

/*
Concealed Courtyard	0x2003c0c (card_generic_som_tapland)
Land
Concealed Courtyard enters the battlefield tapped unless you control two or fewer other lands.
|T: Add |W or |B to your mana pool.
*/

/*
Inspiring Vantage	0x2003c0c (card_generic_som_tapland)
Land
Inspiring Vantage enters the battlefield tapped unless you control two or fewer other lands.
|T: Add |R or |W to your mana pool.
*/

int card_inventors_fair(int player, int card, event_t event){
/*
Inventors' Fair	0x200f4e4
Legendary Land
At the beginning of your upkeep, if you control three or more artifacts, you gain 1 life.
|T: Add |C to your mana pool.
|4, |T, Sacrifice Inventors' Fair: Search your library for an artifact card, reveal it, put it into your hand,
	then shuffle your library. Activate this ability only if you control three or more artifacts.
*/

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_TUTOR_ARTIFACT
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		if( ! paying_mana() ){
			int can_tutor_artifact = count_subtype(player, TYPE_ARTIFACT, -1) >= 3 &&
									generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME,
																MANACOST_X(5), 0, NULL, NULL) ? 1 : 0;
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Tutor artifact", can_tutor_artifact, 10);
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_TUTOR_ARTIFACT ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(5)) ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_TUTOR_ARTIFACT ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}


int card_sequestered_stash(int player, int card, event_t event){
/*
Sequestered Stash	0x200f4ee
Land
|T: Add |C to your mana pool.
|4, |T, Sacrifice Sequestered Stash: Put the top five cards of your library into your graveyard.
	Then you may put an artifact card from your graveyard on top of your library.
*/

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_MILL_RETURN_ARTIFACT
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		instance->info_slot = choice;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME,
																MANACOST_X(4), 0, NULL, NULL) ? 1 : 0){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Mill & put artifact on deck", count_graveyard_by_type(player, TYPE_ARTIFACT) ? 10 : 1);
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_MILL_RETURN_ARTIFACT ){
			add_state(player, card, STATE_TAPPED);
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(4)) ){
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
					untap_card_no_event(player, card);
			}

		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_MILL_RETURN_ARTIFACT ){
			mill(player, 5);

			if( count_graveyard_by_type(player, TYPE_ARTIFACT) ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &test);
			}
		}
	}

	return 0;
}

/*
Spirebluff Canal	0x2003c0c (card_generic_som_tapland)
Land
Spirebluff Canal enters the battlefield tapped unless you control two or fewer other lands.
|T: Add |U or |R to your mana pool.
*/

// Special cards (only on pre-constructed decks)

int card_chandra_pyrogenius(int player, int card, event_t event){
	/* Chandra, Pyrogenius	|4|R|R	0x200f4f3
	 * Legendary Planeswalker - Chandra (5)
	 * +2: ~ deals 2 damage to each opponent.
	 * -3: ~ deals 4 damage to target creature.
	 * -10: ~ deals 6 damage to target player or planeswalker and each creature that player or that planeswalker's controller controls. */
	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 5);
	}

	enum{
		CHOICE_2_DAMAGE_OPP = 1,
		CHOICE_4_DAMAGE_CREATURE,
		CHOICE_6_DAMAGE_ALL,
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"2 damages to opponent", 				1, 						30-life[1-player], 	+2,
							"4 damage to target creature", 			can_target(&td), 		10,					-3,
							"6 damage to opp and opp creatures", 	can_target(&td2), 		40, 				-10);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_4_DAMAGE_CREATURE ){
			new_pick_target(&td, "TARGET_CREATURE", 0, 1);
		}
		if( choice == CHOICE_6_DAMAGE_ALL ){
			new_pick_target(&td2, "TARGET_PLAYER_OR_PLANESWALKER", 0, 1);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_2_DAMAGE_OPP ){
			damage_player(1-player, 2, player, card);
		}

		if( choice == CHOICE_4_DAMAGE_CREATURE ){
			if( valid_target(&td) ){
				damage_target0(player, card, 4);
			}
		}

		if( choice == CHOICE_6_DAMAGE_ALL ){
			if( valid_target(&td2) ){
				damage_target0(player, card, 6);
				new_damage_all(player, card, instance->targets[0].player, 6, NDA_ALL_CREATURES, NULL);
			}
		}
	}

	return planeswalker(player, card, event, 5);
}

/*
Flame Lash |3|R	0x200cce4 (Lightning Blast)
Instant
Flame Lash deals 4 damage to target creature or player.
*/

int card_liberating_combustion(int player, int card, event_t event){
/*
Liberating Combustion |4|R	0x200f4f8
Sorcery
Liberating Combustion deals 6 damage to target creature. You may search your library and/or graveyard for a card named
	Chandra, Pyrogenius, reveal it, and put it into your hand. If you search your library this way, shuffle it.
*/
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 6);
			if( DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, "Tutor Chandra, Pyrogenius", 1, 10,
						"Decline", 1, 1) == 1 )
			{
				test_definition_t test;
				default_test_definition(&test, TARGET_TYPE_PLANESWALKER);
				test.id = CARD_ID_CHANDRA_PYROGENIUS;

				scnprintf(test.message, 100, "Select a card named %s.", cards_ptr[test.id]->name);
				int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
			    if( result == -1 ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
			    }
		   }
		   kill_card(player, card, KILL_DESTROY);
	   }
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_renegade_firebrand(int player, int card, event_t event){
/*
Renegade Firebrand |2|R	0x200f4fd
Creature - Human Warrior
As long as you control a Chandra planeswalker, Renegade Firebrand gets +1/+0 and has first strike.
(It deals combat damage before creatures without first strike.)
3/2
*/
	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) &&
		in_play(player, card) && ! is_humiliated(player, card) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_CHANDRA;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) && !is_humiliated(player, card))
		{
			switch( event ){
					case EVENT_POWER:
						event_result ++;
						break;
					case EVENT_TOUGHNESS:
						event_result ++;
						break;
					case EVENT_ABILITIES:
					{
						event_result |= KEYWORD_FIRST_STRIKE;
						break;
					}
					default:
						break;
			}
		}
	}
	return 0;
}

// Stone Quarry: reprint SOI

int card_nissa_natures_artisan(int player, int card, event_t event){
/*
Nissa, Nature's Artisan |4|G|G	0x200f502
Planeswalker - Nissa
+3: You gain 3 life.
-4: Reveal the top two cards of your library. Put all land cards from among them onto the battlefield and the rest into your hand.
-12: Creatures you control get +5/+5 and gain trample until end of turn.
5
*/

	if( ! IS_GAA_EVENT(event) ){
		return planeswalker(player, card, event, 5);
	}

	enum{
		CHOICE_GAIN_LIFE = 1,
		CHOICE_REVEAL,
		CHOICE_5_5_TRAMPLE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int priority_reveal = (30-(count_subtype(player, TYPE_LAND, -1)*5)) + (35-(hand_count[player]*5));
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
							"Gain 3 life", 							1, 10, 											+3,
							"Reveal top 2 & get lands", 			1, priority_reveal, 							-4,
							"+5/+5 and trample to all your crits", 	1, count_subtype(player, TYPE_CREATURE, -1)*3,	-12);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;

		if( choice == CHOICE_GAIN_LIFE ){
			gain_life(player, 3);
		}

		if( choice == CHOICE_REVEAL ){
			int max = MIN(2, count_deck(player));
			if( max ){
				APNAP(p, {show_deck(p, deck_ptr[player], max, "Cards revealed by Nissa, Nature's Artisan.", 0, 0x7375B0 );};);
			}
			int i;
			for(i=0; i<max; i++){
				int card_added = add_card_to_hand(player, deck_ptr[player][0]);
				remove_card_from_deck(player, 0);
				if( is_what(player, card_added, TYPE_LAND) ){
					put_into_play(player, card_added);
				}
			}
		}

		if( choice == CHOICE_5_5_TRAMPLE ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 5, 5, KEYWORD_TRAMPLE, 0);
		}
	}

	return planeswalker(player, card, event, 5);
}

int card_guardian_of_the_great_conduit(int player, int card, event_t event){
/*
Guardian of the Great Conduit |3|G	0x200f507
Creature - Elemental
Reach (This creature can block creatures with flying.)
As long as you control a Nissa planeswalker, Guardian of the Great Conduit gets +2/+0 and has vigilance. (Attacking doesn't cause it to tap.)
2/4
*/
	if( (event == EVENT_POWER || event == EVENT_ABILITIES) && affect_me(player, card) &&
		in_play(player, card) && ! is_humiliated(player, card) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_NISSA;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) && !is_humiliated(player, card))
		{
			switch( event ){
					case EVENT_POWER:
						event_result+=2;
						break;
					case EVENT_ABILITIES:
						vigilance(player, card, event);
						break;
					default:
						break;
			}
		}
	}
	return 0;
}


/*
Terrain Elemental |1|G (vanilla)
Creature - Elemental
3/2
*/

int card_verdant_crescendo(int player, int card, event_t event){
/*
Verdant Crescendo |3|G	0x200f50c
Sorcery
Search your library for a basic land card and put it onto the battlefield tapped.
Search your library and graveyard for a card named Nissa, Nature's Artisan, reveal it,
and put it into your hand. Then shuffle your library.
*/

	if(event == EVENT_RESOLVE_SPELL ){
		tutor_basic_land(player, 1, 1);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card named Nissa, Nature's Artisan");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/*
Servo	0x20076a4 (Rhino token, the "basic" token form)
Token Artifact Creature - Servo
1/1
*/
