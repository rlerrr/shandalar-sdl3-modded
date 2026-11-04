#include "manalink.h"

static char *you_own(int who_chooses, int player, int card, int targeting_player, int targeting_card);

/* Canonical recovered helper declarations. */
static int can_use_stolen_ability_with_havengul_lich(int player, int card);
static int can_cast_creature_in_grave_with_havengul_lich(int player, int card, int t_player);

// Functions
static int fateful_hour(int player){
	if( life[player] < 6 ){
		return 1;
	}
	return 0;
}

int undying(int player, int card, event_t event)
{
  if (count_1_1_counters(player, card) <= 0)
	{
	  nice_creature_to_sacrifice(player, card);

	  int owner, position;
	  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)
		  && find_in_owners_graveyard(player, card, &owner, &position))
		reanimate_permanent(owner, -1, owner, position, REANIMATE_PLUS1_PLUS1_COUNTER);	// -1 for card doesn't abort since no legacy is added
	}

  return 0;
}

// Cards

// WHITE
int card_archangels_light(int player, int card, event_t event){
/* CARD_ID_ARCHANGELS_LIGHT	12134
Archangel's Light	|7|W
Sorcery
You gain 2 life for each card in your graveyard, then shuffle your graveyard into your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, count_graveyard(player)*2);
		reshuffle_grave_into_deck(player, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_bar_the_door(int player, int card, event_t event){
/* CARD_ID_BAR_THE_DOOR	12137
Bar the Door	|2|W
Instant
Creatures you control get +0/+4 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 0, 4, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_break_of_day(int player, int card, event_t event)
{
  /* Break of Day	|1|W
   * Instant
   * Creatures you control get +1/+1 until end of turn.
   * Fateful hour - If you have 5 or less life, those creatures gain indestructible until end of turn. */

   if (event == EVENT_RESOLVE_SPELL)
	{
	  pump_creatures_until_eot(player, card, player, 0, 1,1, 0,(fateful_hour(player) ? SP_KEYWORD_INDESTRUCTIBLE : 0), NULL);
	  kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_burden_of_guilt(int player, int card, event_t event){
/* CARD_ID_BURDEN_OF_GUILT	12144
Burden of Guilt	|W
Enchantment - Aura
Enchant creature
|1: Tap enchanted creature. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_RESOLVE_ACTIVATION ){
				tap_card(instance->damage_target_player, instance->damage_target_card);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

int card_curse_of_echoes(int player, int card, event_t event){
/* CARD_ID_CURSE_OF_ECHOES	12157
Curse of Echoes	|4|U
Enchantment - Aura Curse
Enchant player
Whenever enchanted player casts an instant or sorcery spell, each other player may copy that spell and may choose new targets for the copy he or she controls.*/

	if (in_play(player, card) && !is_humiliated(player, card, AL_TRIG)){
		card_instance_t* instance = get_card_instance(player, card);
		int enchanted_player = instance->targets[0].player;
		sp_ability_trigger(enchanted_player, event, RESOLVE_TRIGGER_AI(player), TYPE_SPELL, NULL, player, card);
		if (event == EVENT_SPA){
			copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
		}
	}
	return curse(player, card, event);
}

int card_curse_of_exhaustion(int player, int card, event_t event){
/* CARD_ID_CURSE_OF_EXHAUSTION	12158
Curse of Exhaustion	|2|W|W
Enchantment - Aura Curse
Enchant player
Enchanted player can't cast more than one spell each turn. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance( player, card );
		int cursed_player = instance->targets[0].player;

		if( event == EVENT_CAST_SPELL && affected_card_controller == cursed_player ){
			if( ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
				instance->targets[1].player = 66;
			}
		}

		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == cursed_player && instance->targets[1].player == 66 && !is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			infinite_casting_cost();
		}

	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return curse(player, card, event);
}

int card_elgaud_inquisitor(int player, int card, event_t event){
	/* Elgaud Inquisitor	|3|W
	 * Creature - Human Cleric 2/2
	 * Lifelink
	 * When ~ dies, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	lifelink(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);
	}

	return 0;
}

static int faiths_shield_legacy(int player, int card, event_t event){

	if( event == EVENT_ABILITIES && affected_card_controller == get_card_instance(player, card)->targets[0].player ){
		card_instance_t *instance = get_card_instance(player, card);
		if( is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
			in_play(affected_card_controller, affected_card) )
		{
			event_result |= instance->targets[1].player;
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_faiths_shield(int player, int card, event_t event){
  /* Faith's Shield |W
   * Instant
   * Target permanent you control gains protection from the color of your choice until end of turn.
   * Fateful hour — If you have 5 or less life, instead you and each permanent you control gain protection from the color of
   * 				your choice until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return fateful_hour(player) ? 	basic_spell(player, card, event) :
										generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( ! fateful_hour(player) ){
			if( valid_target(&td) ){
				int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
										0, 0, keyword, 0);
			}
		}
		else{
			int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			int legacy = create_legacy_effect(player, card, &faiths_shield_legacy);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[1].player = keyword;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_gather_the_townsfolk(int player, int card, event_t event){
	/* Gather the Townsfolk	|1|W
	 * Sorcery
	 * Put two 1/1 |Swhite Human creature tokens onto the battlefield.
	 * Fateful hour - If you have 5 or less life, put five of those tokens onto the battlefield instead. */

	 if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HUMAN, &token);
		token.qty = fateful_hour(player) ? 5 : 2;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_gavony_ironwright(int player, int card, event_t event){
/* CARD_ID_GAVONY_IRONWRIGHT	12186
Gavony Ironwright	|2|W
Creature - Human Soldier 1/4
Fateful hour - As long as you have 5 or less life, other creatures you control get +1/+4. */

	if( fateful_hour(player) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		return boost_creature_type(player, card, event, -1, 1, 4, 0, BCT_CONTROLLER_ONLY);
	}
	return 0;
}

int card_hollowhenge_spirit(int player, int card, event_t event){
/* CARD_ID_HOLLOWHENGE_SPIRIT	12211
Hollowhenge Spirit	|3|W
Creature - Spirit 2/2
Flash (You may cast this spell any time you could cast an instant.)
Flying
When ~ enters the battlefield, remove target attacking or blocking creature from combat. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_IN_COMBAT;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ATTACKING_BLOCKING_CREATURE") ){
			get_card_instance(instance->targets[0].player, instance->targets[0].card)->blocking = 0xff;
			remove_state(instance->targets[0].player, instance->targets[0].card, STATE_BLOCKING | STATE_ATTACKING);
		}

	}

	return flash(player, card, event);
}

int card_increasing_devotion(int player, int card, event_t event){
	/* Increasing Devotion	|3|W|W
	 * Sorcery
	 * Put five 1/1 |Swhite Human creature tokens onto the battlefield.
	 * If ~ was cast from a graveyard, put ten of those tokens onto the battlefield instead.
	 * Flashback |7|W|W */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HUMAN, &token);
		token.qty = get_flashback(player, card) ? 10 : 5;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);

		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return do_flashback(player, card, event, MANACOST_XW(7, 2));
}

int card_lingering_souls(int player, int card, event_t event){
	/* Lingering Souls	|2|W
	 * Sorcery
	 * Put two 1/1 |Swhite Spirit creature tokens with flying onto the battlefield.
	 * Flashback |1|B */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.qty = 2;
		token.pow = token.tou = 1;
		token.key_plus = KEYWORD_FLYING;
		token.color_forced = COLOR_TEST_WHITE;
		generate_token(&token);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XB(1, 1));
}

static int loyal_cathar_legacy(int player, int card, event_t event)
{

  if (eot_trigger(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int owner = instance->targets[0].player;
    int iid = instance->targets[0].card;
    int count = count_graveyard(owner) - 1;
    while (count > -1)
    {
      if (get_grave(owner)[count] == iid)
      {
        reanimate_permanent(player, card, owner, count, REANIMATE_RETURN_TO_PLAY_TRANSFORMED);
        break;
      }
      count--;
    }
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_loyal_cathar(int player, int card, event_t event)
{
  /* CARD_ID_LOYAL_CATHAR	12227
  Loyal Cathar	|W|W
  Creature - Human Soldier 2/2
  Vigilance
  When ~ dies, return it to the battlefield transformed under your control at the beginning of the
  next end step. (Unhallowed Cathar - 2/1) Unhallowed Cathar can't block. */

  vigilance(player, card, event);
  double_faced_card(player, card, event);

  nice_creature_to_sacrifice(player, card);

  if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) && in_play(player, card) &&
      !is_humiliated(player, card, AL_TRIG))
  {
    if (is_this_dying(player, card) && !check_for_death_trigger_removal(player, card))
    {
      int legacy = create_legacy_effect(player, card, &loyal_cathar_legacy);
      card_instance_t *instance = get_card_instance(player, legacy);
      instance->targets[0].player = get_owner(player, card);
      instance->targets[0].card = get_original_internal_card_id(player, card);
    }
  }

  return 0;
}

int card_unhallowed_cathar(int player, int card, event_t event){
/* CARD_ID_UNHALLOWED_CATHAR	12228
Unhallowed Cathar	""
Creature - Zombie Soldier 2/1
~ can't block. */

	double_faced_card(player, card, event);

	cannot_block(player, card, event);

	return 0;
}

int card_midnight_guard(int player, int card, event_t event)
{
  /* CARD_ID_MIDNIGHT_GUARD	12231
Midnight Guard	|2|W
Creature - Human Soldier 2/3
Whenever another creature enters the battlefield, untap ~. */

  if (!is_humiliated(player, card, AL_TRIG) && trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.not_me = 1;
    etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_ETB)
    untap_card(player, card);
  return 0;
}

int card_niblis_of_the_mist(int player, int card, event_t event){
/* CARD_ID_NIBLIS_OF_THE_MIST	12240
Niblis of the Mist	|2|W
Creature - Spirit 2/1
Flying
When ~ enters the battlefield, you may tap target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static int requiem_angel_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_REQUIEM_ANGEL)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
            return 0;
          }
        }
        test_definition_t this_test;
        default_test_definition(&this_test, TYPE_CREATURE);
        this_test.type = TYPE_LAND | TYPE_CREATURE;
        this_test.subtype = SUBTYPE_SPIRIT;
        this_test.subtype_flag = DOESNT_MATCH;
        count_for_gfp_ability(player, card, event, player, 0, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);

    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SPIRIT, &token);
    token.key_plus = KEYWORD_FLYING;
    token.color_forced = COLOR_TEST_WHITE;
    token.qty = amount;
    generate_token(&token);

    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_requiem_angel(int player, int card, event_t event){
/* Requiem Angel	|5|W
 * Creature - Angel 5/5
 * Flying
 * Whenever another non-Spirit creature you control dies, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)requiem_angel_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &requiem_angel_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.subtype_flag = DOESNT_MATCH;
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = COLOR_TEST_WHITE;
			token.qty = amount;
			generate_token(&token);
		}
	}

	return 0;
}

int card_seance(int player, int card, event_t event){
/* CARD_ID_SEANCE	12256
Seance	|2|W|W
Enchantment
At the beginning of each upkeep, you may exile target creature card from your graveyard.
If you do, create a token that's a copy of that card, except it's a Spirit in addition to its other types. Exile it at the beginning of the next end step.*/

	upkeep_trigger_ability_mode(player, card, event, ANYBODY, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(player) ){
			char msg[100] = "Select a creature to exile.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			int selected = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 ){
				const int *grave = get_grave(player);
				int csvid = cards_data[grave[selected]].id;
				rfg_card_from_grave(player, selected);

				token_generation_t token;
				default_token_definition(player, card, csvid, &token);
				token.legacy = 1;
				token.special_code_for_legacy = &remove_at_eot;
				token.action = TOKEN_ACTION_ADD_SUBTYPE;
				token.action_argument = SUBTYPE_SPIRIT;
				generate_token(&token);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_sudden_disappearance(int player, int card, event_t event){
/* CARD_ID_SUDDEN_DISAPPEARANCE	12273
Sudden Disappearance	|5|W
Sorcery
Exile all nonland permanents target player controls. Return the exiled cards to the battlefield under their owner's control at the beginning of the next end step.*/

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.type_flag = DOESNT_MATCH;
			new_manipulate_all(player, card, instance->targets[0].player, &test, ACT_RFG_UNTIL_EOT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_thraben_doomsayer(int player, int card, event_t event){
	/* Thraben Doomsayer	|1|W|W
	 * Creature - Human Cleric 2/2
	 * |T: Put a 1/1 |Swhite Human creature token onto the battlefield.
	 * Fateful hour - As long as you have 5 or less life, other creatures you control get +2/+2. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_HUMAN);
	}

	if( fateful_hour(player) ){
		boost_creature_type(player, card, event, -1, 2, 2, 0, BCT_CONTROLLER_ONLY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_thalia_guardian_of_thraben(int player, int card, event_t event){
/* CARD_ID_THALIA_GUARDIAN_OF_THRABEN	12275
Thalia, Guardian of Thraben	|1|W
Legendary Creature - Human Soldier 2/1
First strike
Noncreature spells cost |1 more to cast. */

	check_legend_rule(player, card, event);

	return card_thorn_of_amethyst(player, card, event);
}

int card_thraben_heretic(int player, int card, event_t event){
/* CARD_ID_THRABEN_HERETIC	12278
Thraben Heretic	|1|W
Creature - Human Wizard 2/2
|T: Exile target creature card from a graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
    test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);
		}
	}
	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL,
	                                                                 TYPE_CREATURE, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}
// BLUE
int card_artful_dodge(int player, int card, event_t event){
/* CARD_ID_ARTFUL_DODGE	12135
Artful Dodge	|U
Sorcery
Target creature can't be blocked this turn.
Flashback |U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if (IS_GS_EVENT(player, card, event)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		card_instance_t* instance = get_card_instance(player, card);
		if (event == EVENT_RESOLVE_SPELL){
			if (valid_target(&td)){
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
			}
			kill_card(player, card, get_flashback(player, card) ? KILL_EXILE : KILL_DESTROY);
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}
	return do_flashback(player, card, event, MANACOST_U(1));
}

int card_beguiler_of_wills(int player, int card, event_t event){
  /* Beguiler of Wills |3|U|U
   * Creature — Human Wizard 1/1
   * {T}: Gain control of target creature with power less than or equal to the number of creatures you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = count_subtype(player, TYPE_CREATURE, -1) | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_bone_to_ash(int player, int card, event_t event){
/* CARD_ID_BONE_TO_ASH	12141
Bone to Ash	|2|U|U
Instant
Counter target creature spell.
Draw a card. */

	if(event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		counterspell_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t *instance = get_card_instance(player, card);
		if( counterspell_validate(player, card, &td, 0) ){
			draw_cards(player, 1);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	else{
		return card_remove_soul(player, card, event);
	}
	return 0;
}

int card_call_to_the_kindred(int player, int card, event_t event){
  /* Call to the Kindred |3|U
   * Enchantment — Aura
   * Enchant creature
   * At the beginning of your upkeep, you may look at the top five cards of your library.
   * If you do, you may put a creature card that shares a creature type with enchanted creature from among them onto the battlefield,
   * 	then you put the rest of those cards on the bottom of your library in any order. */

	card_instance_t *instance = get_card_instance(player, card);

	if( in_play(player, card) && instance->damage_target_player != -1  ){

		upkeep_trigger_ability_mode(player, card, event, player, duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			card_instance_t *target = get_card_instance( instance->damage_target_player, instance->damage_target_card);

			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
			int to_look = MIN(5, count_deck(player));
			test.create_minideck = to_look;
			test.no_shuffle = 1;

			int selected = -1;

			if( player == HUMAN ){
				selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &test);
			}
			else{
				int i;
				int par = 0;
				for(i=0; i<to_look; i++){
					if( is_what(-1, deck_ptr[player][i], TYPE_CREATURE) &&
						shares_creature_subtype(-1, deck_ptr[player][i], -1, target->internal_card_id) )
					{
						if( get_cmc_by_id(cards_data[deck_ptr[player][i]].id) > par ){
							par = get_cmc_by_id(cards_data[deck_ptr[player][i]].id);
							selected = i;
						}
					}
				}
			}
			if( selected != -1 && shares_creature_subtype(-1, deck_ptr[player][selected], -1, target->internal_card_id) ){
				int card_added = add_card_to_hand(player, deck_ptr[player][selected]);
				remove_card_from_deck(player, selected);
				set_special_flags(player, card_added, SF_PLAYED_FROM_DECK);
				put_into_play(player, card_added);
				to_look--;
			}
			put_top_x_on_bottom(player, player, to_look);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return vanilla_aura(player, card, event, player);
}

int card_chant_of_skifsang(int player, int card, event_t event){
/* CARD_ID_CHANT_OF_THE_SKIFSANG	12149
Chant of the Skifsang	|2|U
Enchantment - Aura
Enchant creature
Enchanted creature gets -13/-0. */

	return generic_aura(player, card, event, 1-player, -13, 0, 0, 0, 0, 0, 0);
}

int card_chill_of_foreboding(int player, int card, event_t event){
/* CARD_ID_CHILL_OF_FOREBODING	12150
Chill of Foreboding	|2|U
Sorcery
Each player puts the top five cards of his or her library into his or her graveyard.
Flashback |7|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {mill(p, 5);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return do_flashback(player, card, event, MANACOST_XU(7, 1));
}

int card_counterlash(int player, int card, event_t event){
  /* Counterlash |4|U|U
   * Instant
   * Counter target spell. You may cast a nonland card in your hand that shares a card type with that spell without paying its mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return counterspell(player, card, event, &td, 0);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, &td, 0) ){
			int type = get_type(instance->targets[0].player, instance->targets[0].card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			if( hand_count[player] > 0 ){
				int playable[2][hand_count[player]];
				int pc = 0;
				int i;
				for(i=0; i<active_cards_count[player]; i++){
					if( in_hand(player, i) && is_what(player, i, type) && ! is_what(player, i, TYPE_LAND) ){
						int iid = get_original_internal_card_id(player, i);
						if( can_legally_play_iid(player, iid) ){
							playable[0][pc] = iid;
							playable[1][pc] = i;
							pc++;
						}
					}
				}
				if( pc ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, type, "Select a card to play.");
					int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_CMC, -1, &this_test);
					if( selected != -1 ){
						play_card_in_hand_for_free(player, playable[1][selected]);
					}
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int dungeon_geist_effect(int player, int card, event_t event){

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
		if( ! in_play(instance->targets[0].player, instance->targets[0].card) ){
			kill_card(player, card, KILL_EXILE);
		}
	    if( event == EVENT_CARDCONTROLLED && affect_me(instance->targets[0].player, instance->targets[0].card) ){
			kill_card(player, card, KILL_EXILE);
        }
	}

	return 0;
}

int card_dungeon_geist(int player, int card, event_t event){
/* CARD_ID_DUNGEON_GEISTS	12168
Dungeon Geists	|2|U|U
Creature - Spirit 3/3
Flying
When ~ enters the battlefield, tap target creature an opponent controls.
That creature doesn't untap during its controller's untap step for as long as you control ~. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = 1-player;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			int legacy = create_targetted_legacy_effect(player, card, &dungeon_geist_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
			leg->number_of_targets = 1;
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_geralfs_mindcrusher(int player, int card, event_t event){
/* CARD_ID_GERALFS_MINDCRUSHER	12188
Geralf's Mindcrusher	|4|U|U
Creature - Zombie Horror 5/5
When ~ enters the battlefield, target player puts the top five cards of his or her library into his or her graveyard.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
      td.allow_cancel = 0;
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			mill(instance->targets[0].player, 5);
		}
		instance->number_of_targets = 0;
	}

	return undying(player, card, event);
}

int card_griptide(int player, int card, event_t event){
/* CARD_ID_GRIPTIDE	12196
Griptide	|3|U
Instant
Put target creature on top of its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_havengul_runebinder(int player, int card, event_t event){
/* Havengul Runebinder	|2|U|U
 * Creature - Human Wizard 2/2
 * |2|U, |T, Exile a creature card from your graveyard:
 *			Put a 2/2 |Sblack Zombie creature token onto the battlefield, then put a +1/+1 counter on each Zombie creature you control. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(2, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XU(2, 1)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card to exile.");
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MIN_VALUE, &test) > -1 ){
				add_state(player, card, STATE_TAPPED);
				tapped_for_mana_color = -1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_ZOMBIE);

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_ZOMBIE;
		new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
	}

	return 0;
}

int card_headless_skaab(int player, int card, event_t event){
/* CARD_ID_HEADLESS_SKAAB	12202
Headless Skaab	|2|U
Creature - Zombie Warrior 3/6
As an additional cost to cast ~, exile a creature card from your graveyard.
~ enters the battlefield tapped. */

	skaabs(player, card, event, 1);

	comes_into_play_tapped(player, card, event);

	return 0;
}

int card_increasing_confusion(int player, int card, event_t event){
/* Increasing Confusion	|X|U
 * Sorcery
 * Target player puts the top X cards of his or her library into his or her graveyard.
 * If ~ was cast from a graveyard, that player puts twice that many cards into his or her graveyard instead.
 * Flashback |X|U */

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana(player, COLOR_BLUE, 1) && can_sorcery_be_played(player, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		if( can_target(&td) ){
			return GA_PLAYABLE_FROM_GRAVE;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi(player, MANACOST_XU(-1, 1));
		if( spell_fizzled != 1 ){
			set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
			return GAPAID_EXILE;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) && instance->info_slot > 0 ){
			int amount = instance->info_slot;
			if( get_flashback(player, card) ){
				amount*=2;
			}
			mill(instance->targets[0].player, amount);
		}
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_PLAYER", 1, NULL);
}

int card_mystic_retrieval(int player, int card, event_t event){
	/* Mystic Retrieval	|3|U
	 * Sorcery
	 * Return target instant or sorcery card from your graveyard to your hand.
	 * Flashback |2|R */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return do_flashback(player, card, event, MANACOST_XR(2, 1));
	}

	return card_call_to_mind(player, card, event);
}

int card_niblis_of_the_breath(int player, int card, event_t event){
/* CARD_ID_NIBLIS_OF_THE_BREATH	12239
Niblis of the Breath	|2|U
Creature - Spirit 2/1
Flying
|U, |T: You may tap or untap target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(1), 0, &td, "TARGET_CREATURE");
		ai_modifier_twiddle(player, card, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			twiddle(player, card, 0);
		}
	}

	return 0;
}

int card_relentless_skaabs(int player, int card, event_t event){
/* CARD_ID_RELENTLESS_SKAABS	12247
Relentless Skaabs	|3|U|U
Creature - Zombie 4/4
As an additional cost to cast ~, exile a creature card from your graveyard.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	skaabs(player, card, event, 1);

	return undying(player, card, event);
}

int card_saving_grasp(int player, int card, event_t event){
/* CARD_ID_SAVING_GRASP	12251
Saving Grasp	|U
Instant
Return target creature you own to your hand.
Flashback |W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = 1-player;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = (int32_t)you_own;

		card_instance_t *instance = get_card_instance(player, card);

		if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			if( get_flashback(player, card) ){
				kill_card(player, card, KILL_EXILE);
			}
			else{
				kill_card(player, card, KILL_DESTROY);
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_W(1));
}

int card_screeching_skaab(int player, int card, event_t event ){
/* CARD_ID_SCREECHING_SKAAB	12255
Screeching Skaab	|1|U
Creature - Zombie 2/1
When ~ enters the battlefield, put the top two cards of your library into your graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		mill(player, 2);
	}

	return 0;
}

int card_secrets_of_the_dead(int player, int card, event_t event){
  /* Secrets of the Dead |2|U
   * Enchantment
   * Whenever you cast a spell from your graveyard, draw a card. */

	if (in_play(player, card) && !is_humiliated(player, card, AL_TRIG) && trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player){
		if (check_special_flags(trigger_cause_controller, trigger_cause, SF_PLAYED_FROM_GRAVE) && get_owner(trigger_cause_controller, trigger_cause) == player){
			sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
		}
	}
	if (event == EVENT_SPA){ draw_cards(player, 1); }
	return global_enchantment(player, card, event);
}

int card_shriekgeist(int player, int card, event_t event){
/* CARD_ID_SHRIEKGEIST	12259
Shriekgeist	|1|U
Creature - Spirit 1/1
Flying
Whenever ~ deals combat damage to a player, that player puts the top two cards of his or her library into his or her graveyard. */

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						mill(p, 2);
					}
				}
		);
	}

	return 0;
}

int card_stormbound_geist(int player, int card, event_t event){
/* CARD_ID_STORMBOUND_GEIST	12270
Stormbound Geist	|1|U|U
Creature - Spirit 2/2
Flying
~ can block only creatures with flying.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	if( event == EVENT_BLOCK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( ! check_for_ability(attacking_card_controller, attacking_card, KEYWORD_FLYING) ){
			event_result = 1;
		}
	}

	return undying(player, card, event);
}

int card_thought_scour(int player, int card, event_t event){
/* CARD_ID_THOUGHT_SCOUR	12276
Thought Scour	|U
Instant
Target player puts the top two cards of his or her library into his or her graveyard.
Draw a card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		mill(instance->targets[0].player, 2);
		draw_cards(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_tower_geist(int player, int card, event_t event ){
/* CARD_ID_TOWER_GEIST	12280
Tower Geist	|3|U
Creature - Spirit 2/2
Flying
When ~ enters the battlefield, look at the top two cards of your library. Put one of them into your hand and the other into your graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		select_one_and_mill_the_rest(player, player, 2, TYPE_ANY);
	}

	return 0;
}

// BLACK
int card_black_cat(int player, int card, event_t event ){
/* CARD_ID_BLACK_CAT	12139
Black Cat	|1|B
Creature - Zombie Cat 1/1
When ~ dies, target opponent discards a card at random. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;

		if( would_validate_target(player, card, &td, 0) ){
			discard(1-player, DISC_RANDOM, player);
		}
	}

	return 0;
}

int card_chosen_of_markov(int player, int card, event_t event){
/* CARD_ID_CHOSEN_OF_MARKOV	12151
Chosen of Markov	|2|B
Creature - Human 2/2
|T, Tap an untapped Vampire you control: Transform ~.
(Markov's Servant - 4/4) */

	double_faced_card(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_VAMPIRE;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( new_pick_target(&td, "Select an untapped Vampire you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
				tap_card(player, card);
			}
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		transform(instance->parent_controller, instance->parent_card);
	}

	return 0;
}

static int curse_already_enchanting_player(int csvid, int attached_player){
	int p, c;
	for(p=0; p<2; p++){
		for(c = 0; c < active_cards_count[p]; c++ ){
			if( in_play(p, c) && has_subtype(p, c, SUBTYPE_CURSE) && get_id(p, c) == csvid ){
				if( get_card_instance(p, c)->targets[0].player == attached_player ){
					return 1;
				}
			}
		}
	}
	return 0;
}

int card_curse_of_misfortunes(int player, int card, event_t event)
{
  /* CARD_ID_CURSE_OF_MISFORTUNES	12159
  Curse of Misfortunes	|4|B
  Enchantment - Aura Curse
  Enchant player
  At the beginning of your upkeep, you may search your library for a Curse card that doesn't have
  the same name as a Curse attached to enchanted player, put it onto the battlefield attached to
  that player, then shuffle your library. */

  if (get_card_instance(player, card)->targets[0].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);

    int *deck = deck_ptr[player];
    if (deck[0] != -1)
    {
      upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

      if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
      {
        char msg[100] = "Select a Curse card.";
        test_definition_t this_test;
        new_default_test_definition(&this_test, TYPE_ENCHANTMENT, msg);
        this_test.subtype = SUBTYPE_CURSE;

        int curse_array[count_deck(player)];

        int i = 0;
        while (deck[i] != -1)
        {
          if (has_subtype_by_id(player, cards_data[deck[i]].id, SUBTYPE_CURSE) &&
              !curse_already_enchanting_player(cards_data[deck[i]].id, instance->targets[0].player))
          {
            curse_array[i] = 1;
          }
          else
          {
            curse_array[i] = 0;
          }
          i++;
        }
        int selected = -1;
        if (player == HUMAN)
        {
          selected =
              new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &this_test);
        }
        else
        {
          i = 0;
          int max_cmc = -1;
          while (deck[i] != -1)
          {
            if (curse_array[i])
            {
              int this_cmc = get_cmc_by_internal_id(deck_ptr[player][i]);
              if (this_cmc > max_cmc)
              {
                selected = i;
                max_cmc = this_cmc;
              }
            }
            i++;
          }
        }
        if (selected != -1 && curse_array[selected])
        {
          int card_added = add_card_to_hand(player, deck_ptr[player][selected]);
          remove_card_from_deck(player, selected);
          set_special_flags(player, card_added, SF_TARGETS_ALREADY_SET);
          card_instance_t *added = get_card_instance(player, card_added);
          added->targets[0] = instance->targets[0];
          put_into_play(player, card_added);
        }
      }
    }
  }

  return curse(player, card, event);
}

int card_curse_of_thirst(int player, int card, event_t event){
/* CARD_ID_CURSE_OF_THIRST	12160
Curse of Thirst	|4|B
Enchantment - Aura Curse
Enchant player
At the beginning of enchanted player's upkeep, ~ deals damage to that player equal to the number of Curses attached to him or her. */

	if( in_play(player, card) && get_card_instance(player, card)->targets[0].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		upkeep_trigger_ability(player, card, event, instance->targets[0].player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			int count = 0;
			int amount = 0;
			while( count < active_cards_count[player] ){
					if( in_play(player, count) && is_what(player, count, TYPE_ENCHANTMENT) &&
						has_subtype(player, count, SUBTYPE_CURSE)
					  ){
						card_instance_t *target = get_card_instance(player, count);
						if( target->targets[0].player == instance->targets[0].player ){
							amount++;
						}
					}
					count++;
			}
			if( amount > 0 ){
				damage_player(instance->targets[0].player, amount, player, card);
			}
		}
	}

	return curse(player, card, event);
}

int card_deadly_allure(int player, int card, event_t event){
/* CARD_ID_DEADLY_ALLURE	12162
Deadly Allure	|B
Sorcery
Target creature gains deathtouch until end of turn and must be blocked this turn if able.
Flashback |G (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if(event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
										0, 0, 0, SP_KEYWORD_DEATHTOUCH + SP_KEYWORD_MUST_BE_BLOCKED);
			}
			if( get_flashback(player, card) ){
				kill_card(player, card, KILL_EXILE);
			}
			else{
				kill_card(player, card, KILL_DESTROY);
			}
		}

		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_G(1));
}

int card_deaths_caress(int player, int card, event_t event){
/* CARD_ID_DEATHS_CARESS	12163
Death's Caress	|3|B|B
Sorcery
Destroy target creature. If that creature was a Human, you gain life equal to its toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 0;
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_HUMAN) ){
				amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			}
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_falkenrath_torturer(int player, int card, event_t event)
{
  /* CARD_ID_FALKENRATH_TORTURER	12177
   * Falkenrath Torturer	|2|B
   * Creature - Vampire 2/1
   * Sacrifice a creature: ~ gains flying until end of turn. If the sacrificed creature was a Human, put a +1/+1 counter on ~. */

  card_instance_t *instance;
  int result;
  unsigned int sacrifice_choice;
  unsigned int player_00;
  test_definition_t test;
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if (result != 0)
      {
        new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
        sacrifice_choice = new_sacrifice(player, card, player,
                                         SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (sacrifice_choice == 0)
        {
          spell_fizzled = 1;
        }
        else
        {
          player_00 = (int)sacrifice_choice >> 0x10 & 0xff;
          result = has_subtype(player_00, sacrifice_choice >> 0x18, SUBTYPE_HUMAN);
          instance->info_slot = result;
          if (result == 1)
          {
            ai_modifier = ai_modifier + 0xf;
          }
          kill_card(player_00, sacrifice_choice >> 0x18, KILL_SACRIFICE);
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->parent_controller, instance->parent_card, 0, 0, 0x20, 0);
      if (instance->info_slot == 1)
      {
        add_counters(player, card, instance->parent_controller, instance->parent_card,
                     COUNTER_P1_P1, 1);
      }
    }
    return 0;
  }
  return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x200, 0, 0, 0, 0, 0, 0, 0,
                                   NULL, NULL);
}

int card_farbog_boneflinger(int player, int card, event_t event){
/* CARD_ID_FARBOG_BONEFLINGER	12178
Farbog Boneflinger	|4|B
Creature - Zombie 2/2
When ~ enters the battlefield, target creature gets -2/-2 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE")){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -2, -2);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_fiend_of_the_shadows(int player, int card, event_t event)
{
/* Fiend of the Shadows	|3|B|B
 * Creature - Vampire Wizard 3/3
 * Flying
 * Whenever ~ deals combat damage to a player, that player exiles a card from his or her hand.
 * 		You may play that card for as long as it remains exiled.
 * Sacrifice a Human: Regenerate ~. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;

		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select a card to exile.");

		APNAP(p,{
					if( times_damaged[p] ){
						int selected = new_select_a_card(p, p, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
						if( selected > -1 ){
						int csvid = get_original_id(p, selected);
						rfg_card_in_hand(p, selected);
						create_may_play_card_from_exile_effect(player, card, p, csvid, MPCFE_ATTACH);
						}
					}
				}
		);
	}

	if (!IS_ACTIVATING(event))
		return 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Human to sacrifice.");
    test.subtype = SUBTYPE_HUMAN;
	if (event == EVENT_CAN_ACTIVATE && regeneration(player, card, event, MANACOST0) && new_can_sacrifice_as_cost(player, card, &test))
		return 99;

	if (event == EVENT_ACTIVATE && regeneration(player, card, event, MANACOST0) && spell_fizzled != 1)
	{
		new_sacrifice(player, card, player, SAC_AS_COST, &test);
	}

	if (event == EVENT_RESOLVE_ACTIVATION)
		regeneration(player, card, event, MANACOST0);

	return 0;
}

int card_geralfs_messanger(int player, int card, event_t event){

	/* Geralf's Messenger	|B|B|B
	 * Creature - Zombie 3/2
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, target opponent loses 2 life.
	 * Undying */

	comes_into_play_tapped(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( target_opponent(player, card) ){
			lose_life(1-player, 2);
		}
	}

	return undying(player, card, event);
}

int card_gravecrawler(int player, int card, event_t event){
	/* Gravecrawler	|B
	 * Creature - Zombie 2/1
	 * ~ can't block.
	 * You may cast ~ from your graveyard as long as you control a Zombie. */

	cannot_block(player, card, event);

	if(event == EVENT_GRAVEYARD_ABILITY){
		if( can_sorcery_be_played(player, event) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.subtype = SUBTYPE_ZOMBIE;
			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				if( has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, card)) ){
					return GA_PLAYABLE_FROM_GRAVE;
				}
			}
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		if( charge_mana_from_id(player, -1, event, get_id(player, card)) ){
			return GAPAID_REMOVE;
		}
	}

	return 0;
}

int card_gruesome_discovery(int player, int card, event_t event){
/* Gruesome Discovery	|2|B|B
 * Sorcery
 * Target player discards two cards.
 * Morbid - If a creature died this turn, instead that player reveals his or her hand, you choose two cards from it,
 * 		then that player discards those cards. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( morbid() ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);

				ec_definition_t this_definition;
				default_ec_definition(instance->targets[0].player, player, &this_definition);
				this_definition.qty = 2;
				new_effect_coercion(&this_definition, &this_test);
			}
			else{
				new_multidiscard(instance->targets[0].player, 2, 0, player);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_harrowing_journey(int player, int card, event_t event){
/* CARD_ID_HARROWING_JOURNEY	12198
Harrowing Journey	|4|B
Sorcery
Target player draws three cards and loses 3 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 3);
			lose_life(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_highborn_ghoul(int player, int card, event_t event){
/* CARD_ID_HIGHBORN_GHOUL	12207
Highborn Ghoul	|B|B
Creature - Zombie 2/1
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.) */

	intimidate(player, card, event);
	return 0;
}

int card_increasing_ambition(int player, int card, event_t event){
/* CARD_ID_INCREASING_AMBITION	12216
Increasing Ambition	|4|B
Sorcery
Search your library for a card and put that card into your hand. If ~ was cast from a graveyard,
instead search your library for two cards and put those cards into your hand. Then shuffle your library.
Flashback |7|B (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card.");
		if( get_flashback(player, card) ){
			this_test.qty = 2;
			kill_card(player, card, KILL_EXILE);
		}
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return do_flashback(player, card, event, MANACOST_XB(7, 1));
}

void mikaeus_undying(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	for(i=1; i < 10; i++){
		if( instance->targets[i].player > -1 ){
			int owner = instance->targets[i].player;
			int iid = instance->targets[i].card;
			instance->targets[i].player = instance->targets[i].card = -1;
			if( cards_data[iid].id != CARD_ID_MIKAEUS_THE_UNHALLOWED ){
				int cd = count_graveyard(owner)-1;
				while( cd > -1 ){
						if( get_grave(owner)[cd] == iid ){
							reanimate_permanent(owner, -1, owner, cd, REANIMATE_PLUS1_PLUS1_COUNTER);
							break;
						}
						cd--;
				}
			}
		}
	}
}

static int mikaeus_the_unhallowed_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_MIKAEUS_THE_UNHALLOWED)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY &&
      is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
      !count_1_1_counters(affected_card_controller, affected_card) &&
      !has_subtype(affected_card_controller, affected_card, SUBTYPE_HUMAN))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
              instance->targets[11].player = 0;
            SET_BYTE2(instance->targets[11].player) = 1;
            return 0;
          }
        }
        count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL,
                                               GFPC_TRACK_DEAD_CREATURES | GFPC_STORE_OWNER, 1);
      }
    }
  }

  if (!check_state(player, card, STATE_INVISIBLE))
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      mikaeus_undying(player, card);
      kill_card(player, card, KILL_EXILE);
    }
  }
  else
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;
    if (instance->targets[11].player > 0 && trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY &&
        p == reason_for_trigger_controller && affect_me(p, c) && trigger_cause_controller == p &&
        trigger_cause == c)
    {
      if (event == EVENT_TRIGGER)
      {
        event_result |= RESOLVE_TRIGGER_MANDATORY;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        instance->targets[11].player = 0;
        mikaeus_undying(player, card);
      }
      else if (event == EVENT_END_TRIGGER)
      {
        instance->targets[11].player = 0;
      }
    }
  }

  return 0;
}

int card_mikaeus_the_unhallowed(int player, int card, event_t event)
{
  /* Mikaeus, the Unhallowed	|3|B|B|B
   * Legendary Creature - Zombie Cleric 5/5
   * Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
   * Whenever a Human deals damage to you, destroy it.
   * Other non-Human creatures you control get +1/+1 and have undying. (When a creature with undying dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.) */

  card_instance_t *instance;
  int card_00;
  card_instance_t *damage_or_candidate;
  int legacy_or_target_index;
  int target_player_or_result;
  card_instance_t *source_or_legacy;
  check_legend_rule(player, card, event);
  intimidate(player, card, event);
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    bool legacy_found = false;
    if (0 < active_cards_count[player])
    {
      legacy_or_target_index = 0;
      do
      {
        damage_or_candidate = in_play(player, legacy_or_target_index);
        if ((((damage_or_candidate != NULL) &&
              (target_player_or_result = is_what(player, legacy_or_target_index, 0x80),
               target_player_or_result != 0)) &&
             (damage_or_candidate = get_card_instance(player, legacy_or_target_index),
              damage_or_candidate->info_slot_fn == mikaeus_the_unhallowed_effect)) &&
            (((damage_or_candidate->damage_target_player == player &&
               (damage_or_candidate->damage_target_card == card)) ||
              ((damage_or_candidate->targets[0].player == player &&
                (damage_or_candidate->targets[0].card == card))))))
        {
          legacy_found = true;
          break;
        }
        legacy_or_target_index = legacy_or_target_index + 1;
      } while (legacy_or_target_index < active_cards_count[player]);
    }
    if (!legacy_found)
    {
      legacy_or_target_index =
          create_targetted_legacy_effect(player, card, mikaeus_the_unhallowed_effect, player, card);
      instance = get_card_instance(player, legacy_or_target_index);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      add_status(player, legacy_or_target_index, 0x10000);
    }
  }
  damage_or_candidate = in_play(player, card);
  if ((damage_or_candidate != NULL) &&
      (legacy_or_target_index = is_humiliated(player, card, AL_TRIG), legacy_or_target_index == 0))
  {

    if (((event == EVENT_DEAL_DAMAGE) &&
         ((((damage_or_candidate = get_card_instance(affected_card_controller, affected_card),
             damage_or_candidate->internal_card_id == damage_card &&
                 (damage_or_candidate->damage_target_card == -1)) &&
            (damage_or_candidate->damage_target_player == player)) &&
           ((legacy_or_target_index = damage_is_to_planeswalker(damage_or_candidate),
             legacy_or_target_index == 0 &&
                 (legacy_or_target_index =
                      has_subtype((int)damage_or_candidate->damage_source_player,
                                  damage_or_candidate->damage_source_card, SUBTYPE_HUMAN),
                  legacy_or_target_index != 0)))))) &&
        ((0 < damage_or_candidate->info_slot ||
          (source_or_legacy = get_card_instance((int)damage_or_candidate->damage_source_player,
                                                damage_or_candidate->damage_source_card),
           source_or_legacy->targets[0x10].player == 1))))
    {
      source_or_legacy = get_card_instance(player, card);
      if (source_or_legacy->info_slot < 0)
      {
        source_or_legacy->info_slot = 0;
      }
      legacy_or_target_index = source_or_legacy->info_slot;
      source_or_legacy->targets[legacy_or_target_index].player =
          (int)damage_or_candidate->damage_source_player;
      source_or_legacy->targets[legacy_or_target_index].card =
          damage_or_candidate->damage_source_card;
      source_or_legacy->info_slot = legacy_or_target_index + 1;
    }
    instance = get_card_instance(player, card);
    if (((0 < instance->info_slot) && (trigger_condition == TRIGGER_DEAL_DAMAGE)) &&
        ((card == affected_card &&
          ((player == affected_card_controller && (reason_for_trigger_controller == player))))))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        instance = get_card_instance(player, card);
        if (0 < instance->info_slot)
        {
          legacy_or_target_index = 0;
          do
          {
            target_player_or_result = instance->targets[legacy_or_target_index].player;
            if (((target_player_or_result != -1) &&
                 (card_00 = instance->targets[legacy_or_target_index].card, card_00 != -1)) &&
                (source_or_legacy = in_play(target_player_or_result, card_00),
                 source_or_legacy != NULL))
            {
              kill_card(instance->targets[legacy_or_target_index].player,
                        instance->targets[legacy_or_target_index].card, KILL_DESTROY);
            }
            legacy_or_target_index = legacy_or_target_index + 1;
          } while (legacy_or_target_index < instance->info_slot);
        }
        instance->info_slot = 0;
      }
    }
  }
  instance = in_play(player, card);
  if (((instance != NULL) && (legacy_or_target_index = is_humiliated(player, card, AL_STATIC),
                              legacy_or_target_index == 0)) &&
      (((((uint32_t)(event - EVENT_POWER) < 2 &&
          ((affected_card_controller == player && (card != affected_card)))) &&
         (legacy_or_target_index = is_what(player, affected_card, 2),
          legacy_or_target_index != 0)) &&
        (legacy_or_target_index =
             has_subtype(affected_card_controller, affected_card, SUBTYPE_HUMAN),
         legacy_or_target_index == 0))))
  {
    event_result = event_result + 1;
  }
  return 0;
}

int card_ravenous_demon(int player, int card, event_t event){
  /* Ravenous Demon |3|B|B
   * Creature — Demon 4/4, 3BB (5)
   * Sacrifice a Human: Transform Ravenous Demon. Activate this ability only any time you could cast a sorcery. */

	double_faced_card(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select an Human to sacrifice.");
	test.subtype = SUBTYPE_HUMAN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		transform(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, NULL, NULL,
															0, &test, 0);
}

int card_archdemon_of_greed(int player, int card, event_t event){
  /* Archdemon of Greed	""
   * Creature - Demon 9/9
   * Flying, trample
   * At the beginning of your upkeep, sacrifice a Human. If you can't, tap this creature and it deals 9 damage to you. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int result = target_player_sacrifices_a_subtype(player, card, TYPE_PERMANENT, SUBTYPE_HUMAN, 0);
		if( result == 0 ){
			tap_card(player, card);
			damage_player(player, 9, player, card);
		}
	}

	return 0;
}

int card_reap_the_seagraf(int player, int card, event_t event){
	/* Reap the Seagraf	|2|B
	 * Sorcery
	 * Put a 2/2 |Sblack Zombie creature token onto the battlefield.
	 * Flashback |4|U */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		generate_token_by_id(player, card, CARD_ID_ZOMBIE);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XU(4, 1));
}

int card_skirsdag_flayer(int player, int card, event_t event){
  /* Skirsdag Flayer |1|B
   * Creature — Human Cleric 1/1
   * {3}{B}, {T}, Sacrifice a Human: Destroy target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select an Human to sacrifice.");
	test.subtype = SUBTYPE_HUMAN;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
		    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XB(3, 1), 0,
															&td, "TARGET_CREATURE", 0, &test, 0);
}

int card_sightless_ghoul(int player, int card, event_t event){
/* CARD_ID_SIGHTLESS_GHOUL	12260
Sightless Ghoul	|3|B
Creature - Zombie Soldier 2/2
~ can't block.
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	cannot_block(player, card, event);
	return undying(player, card, event);
}

int card_spiteful_shadows(int player, int card, event_t event){
  /* Spiteful Shadows |1|B
   * Enchantment — Aura
   * Enchant creature
   * Whenever enchanted creature is dealt damage, it deals that much damage to its controller. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_card != -1 && ! is_humiliated(player, card, AL_TRIG) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == c && damage->damage_target_player == p ){
					int good = 0;
					if( damage->info_slot > 0 ){
						good = damage->info_slot;
					}
					else{
						card_instance_t *trg = get_card_instance(damage->damage_source_player, damage->damage_source_card);
						if( trg->targets[16].player > 0 ){
							good = trg->targets[16].player;
						}
					}
					instance->info_slot+=good;
				}
			}
		}

		if( instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				damage_player(p, instance->info_slot, p, c);
				instance->info_slot = 0;
			}
		}

	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

int card_tragic_slip(int player, int card, event_t event){
/* Tragic Slip	|B
 * Instant
 * Target creature gets -1/-1 until end of turn.
 * Morbid - That creature gets -13/-13 until end of turn instead if a creature died this turn. */

	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, morbid() ? -13 : -1, morbid() ? -13 : -1, 0, 0);
}

static int undying_evil_legacy(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;
    if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) && !is_token(p, c) &&
        count_counters(p, c, COUNTER_P1_P1) < 1)
    {
      if (!check_for_death_trigger_removal(p, c) && is_this_dying(p, c))
      {
        instance->targets[0].player = get_owner(p, c);
        instance->targets[0].card = get_original_internal_card_id(p, c);
        instance->targets[11].player = 1;
        instance->damage_target_player = instance->damage_target_card = -1;
      }
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int owner = instance->targets[0].player;
    int iid = instance->targets[0].card;
    seek_grave_for_id_to_reanimate(owner, -1, owner, cards_data[iid].id,
                                   REANIMATE_PLUS1_PLUS1_COUNTER);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_undying_evil(int player, int card, event_t event){
/* CARD_ID_UNDYING_EVIL	12284
Undying Evil	|B
Instant
Target creature gains undying until end of turn.
(When it dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.) */

	if(event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &undying_evil_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	else{
		return vanilla_instant_pump(player, card, event, ANYBODY, player, 0, 99, 0, 0);
	}

	return 0;
}

int card_wakedancer(int player, int card, event_t event){
/* Wakedancer	|2|B
 * Creature - Human Shaman 2/2
 * Morbid - When ~ enters the battlefield, if a creature died this turn, put a 2/2 |Sblack Zombie creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( morbid() ){
			generate_token_by_id(player, card, CARD_ID_ZOMBIE);
		}
	}

	return 0;
}

int card_zombie_apocalypse(int player, int card, event_t event){
/* CARD_ID_ZOMBIE_APOCALYPSE	12297
Zombie Apocalypse	|3|B|B|B
Sorcery
Return all Zombie creature cards from your graveyard to the battlefield tapped, then destroy all Humans. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_ZOMBIE;
		new_reanimate_all(player, card, player, &test, REANIMATE_TAP);

		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_HUMAN;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// RED

// Afflicted Deserter => Gastaf Shepherd
static int effect_werewolf_ransacker(int player, int card, event_t event)
{
  if (effect_put_into_a_graveyard_this_way(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int amount =
        is_what(instance->damage_target_player, instance->damage_target_card, TYPE_CREATURE)
            ? card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1
            : 1;
    do
    {
      damage_player(instance->targets[0].player, 3, instance->damage_source_player,
                    instance->damage_source_card);
    } while (--amount != 0);
  }

  return 0;
}
int card_werewolf_ransacker(int player, int card, event_t event)
{
  /* Werewolf Ransacker	"" [Afflicted Deserter]
   * Creature - Werewolf 5/4
   * Whenever this creature transforms into ~, you may destroy target artifact. If that artifact is put into a graveyard this way, ~ deals 3 damage to that
   * artifact's controller.
   * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

  werewolf_moon_phases(player, card, event);

  if (event == EVENT_TRANSFORMED)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_ARTIFACT);

	  instance->number_of_targets = 0;
	  if (can_target(&td) && pick_target(&td, "TARGET_ARTIFACT"))
		{
		  create_targetted_legacy_effect(player, card, effect_werewolf_ransacker, instance->targets[0].player, instance->targets[0].card);
		  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

  return 0;
}

int card_alpha_brawl(int player, int card, event_t event){
  /* Alpha Brawl |6|R|R
   * Sorcery
   * Target creature an opponent controls deals damage equal to its power to each other creature that player controls,
   * then each of those creatures deals damage equal to its power to that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int my_damage = get_power(instance->targets[0].player, instance->targets[0].card);
			if( my_damage > 0 ){
				test_definition_t test;
				default_test_definition(&test, TYPE_CREATURE);
				test.not_me = 1;
				new_damage_all(instance->targets[0].player, instance->targets[0].card, instance->targets[0].player, my_damage, 0, &test);
			}
			int count = active_cards_count[instance->targets[0].player];
			while(count > -1 ){
					if( count != instance->targets[0].card && in_play(instance->targets[0].player, count) && is_what(instance->targets[0].player, count, TYPE_CREATURE) ){
						int dmg = get_power(instance->targets[0].player, count);
						if( dmg > 0 ){
							damage_creature(instance->targets[0].player, instance->targets[0].card, dmg,
											instance->targets[0].player, count);
						}
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_burning_oil(int player, int card, event_t event){
/* CARD_ID_BURNING_OIL	12145
Burning Oil	|1|R
Instant
~ deals 3 damage to target attacking or blocking creature.
Flashback |3|W (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_IN_COMBAT;

		if( event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td)){
				damage_target0(player, card, 3);
			}
			if( get_flashback(player, card) ){
				kill_card(player, card, KILL_EXILE);
			}
			else{
				kill_card(player, card, KILL_DESTROY);
			}
		}

		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT,
							&td, "Select target attacking or blocking creature.", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_XW(3, 1));
}

int card_blood_feud(int player, int card, event_t event){
	/* Blood Feud	|4|R|R
	 * Sorcery
	 * Target creature fights another target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td, 1) ){
			fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_curse_of_bloodletting(int player, int card, event_t event)
{
/* CARD_ID_CURSE_OF_BLOODLETTING	12156
Curse of Bloodletting	|3|R|R
Enchantment - Aura Curse
Enchant player
If a source would deal damage to enchanted player, it deals double that damage to that player instead. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance( player, card );
		int cursed_player = instance->targets[0].player;
		card_instance_t* damage = damage_being_dealt(event);
		if( damage && damage->damage_target_card == -1 &&
			damage->damage_target_player == cursed_player &&
			! damage_is_to_planeswalker(damage) )
		{
			damage->info_slot *= 2;
		}
	}

	return curse(player, card, event);
}

int card_faithless_looting(int player, int card, event_t event){
/* CARD_ID_FAITHLESS_LOOTING	12174
Faithless Looting	|R
Sorcery
Draw two cards, then discard two cards.
Flashback |2|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		multidiscard(player, 2, 0);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XR(2, 1));
}

int card_fires_of_undeath(int player, int card, event_t event){
	/* Fires of Undeath	|2|R	0x2004ee5
	 * Instant
	 * ~ deals 2 damage to any target.
	 * Flashback |5|B */
	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		if( event == EVENT_RESOLVE_SPELL){
			if( valid_target(&td)){
				damage_target0(player, card, 2);
			}
			if( get_flashback(player, card) ){
				kill_card(player, card, KILL_EXILE);
			}
			else{
				kill_card(player, card, KILL_DESTROY);
			}
		}

		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_XB(5, 1));
}

int card_forge_devil(int player, int card, event_t event){
/* CARD_ID_FORGE_DEVIL	12184
Forge Devil	|R
Creature - Devil 1/1
When ~ enters the battlefield, it deals 1 damage to target creature and 1 damage to you. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
		}
		instance->number_of_targets = 0;
		damage_player(player, 1, player, card);
	}

	return 0;
}

int card_flayer_of_the_hatebound(int player, int card, event_t event){
	/* Flayer of the Hatebound	|5|R	0x200d5cc
	 * Creature - Devil 4/2
	 * Undying
	 * Whenever ~ or another creature enters the battlefield from your graveyard, that creature deals damage equal to its power to any target. */
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( check_special_flags3(trigger_cause_controller, trigger_cause, SF3_REANIMATED) &&
			get_owner(trigger_cause_controller, trigger_cause) == player )
		{
			etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
		}
	}

	if( event == EVENT_ETB_ABILITY ){
		get_card_instance(trigger_cause_controller, trigger_cause)->regen_status |= KEYWORD_RECALC_POWER|KEYWORD_RECALC_TOUGHNESS;
		pandemonium_effect(trigger_cause_controller, trigger_cause);
	}

	return undying(player, card, event);
}

int card_heckling_fiend(int player, int card, event_t event){
/* CARD_ID_HECKLING_FIENDS	12204
Heckling Fiends	|2|R
Creature - Devil 2/2
|2|R: Target creature attacks this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_MUST_ATTACK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(2, 1), 0, &td, "TARGET_CREATURE");
}

int card_hinterland_scourge(int player, int card, event_t event){
/* CARD_ID_HINTERLAND_SCOURGE	12209
Hinterland Scourge	""
Creature - Werewolf 3/2
~ must be blocked if able.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);
	must_be_blocked(player, card, event);
	return 0;
}

int card_hellrider(int player, int card, event_t event)
{
	/* Hellrider	|2|R|R	0x2004d23
	 * Creature - Devil 3/3
	 * Haste
	 * Whenever a creature you control attacks, ~ deals 1 damage to the player or planeswalker it's attacking. */

	haste(player, card, event);
	store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot-1; i >= 0; --i){
			int attacking = planeswalker_its_attacking(player, attackers[i]);
			if (attacking != -2){
				damage_creature(1-current_turn, attacking, 1, player, card);
			}
		}
	}
	return 0;
}

int card_markov_warlord(int player, int card, event_t event){
/* CARD_ID_MARKOV_WARLORD	12230
Markov Warlord	|5|R
Creature - Vampire Warrior 4/4
Haste
When ~ enters the battlefield, up to two target creatures can't block this turn. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) ){
			if( new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				if( can_target(&td) ){
					new_pick_target(&td, "TARGET_CREATURE", 1, 0);
				}
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_moonveil_dragon(int player, int card, event_t event){
/* CARD_ID_MOONVEIL_DRAGON	12235
Moonveil Dragon	|3|R|R|R
Creature - Dragon 5/5
Flying
|R: Each creature you control gets +1/+0 until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 0, 0, 0);
	}
	else{
		return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
	}

	return 0;
}

int card_nearheath_stalker(int player, int card, event_t event){
/* CARD_ID_NEARHEATH_STALKER	12237
Nearheath Stalker	|4|R
Creature - Vampire Rogue 4/1
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	return undying(player, card, event);
}

static int legacy_2_minimum_blockers(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	int t_player = instance->targets[0].player;
	int t_card = instance->targets[0].card;

	if( t_player > -1 ){
		menace(t_player, t_card, event);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_pyreheart_wolf(int player, int card, event_t event){

	/* Pyreheart Wolf	|2|R
	 * Creature - Wolf 1/1
	 * Whenever ~ attacks, creatures you control gain menace until end of turn.
	 * Undying */

	int count;
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		for (count = active_cards_count[player]-1; count >= 0; --count){
			if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
				create_targetted_legacy_effect(player, card, &legacy_2_minimum_blockers, player, count);
			}
		}
	}

	return undying(player, card, event);
}

int card_scorch_the_fields(int player, int card, event_t event){
/* CARD_ID_SCORCH_THE_FIELDS	12252
Scorch the Fields	|4|R
Sorcery
Destroy target land. ~ deals 1 damage to each Human creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.subtype = SUBTYPE_HUMAN;
			new_damage_all(player, card, ANYBODY, 1, 0, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

int card_shattered_perception(int player, int card, event_t event){
/* CARD_ID_SHATTERED_PERCEPTION	12258
Shattered Perception	|2|R
Sorcery
Discard all the cards in your hand, then draw that many cards.
Flashback |5|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		int amount = hand_count[player];
		discard_all(player);
		draw_cards(player, amount);

		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return do_flashback(player, card, event, MANACOST_XR(5, 1));
}

int card_talons_of_falkenrath(int player, int card, event_t event){
/* CARD_ID_TALONS_OF_FALKENRATH	12274
Talons of Falkenrath	|1|R
Enchantment - Aura
Flash (You may cast this spell any time you could cast an instant.)
Enchant creature
Enchanted creature has "|1|R: This creature gets +2/+0 until end of turn." */

	if (IS_GAA_EVENT(event)){
		card_instance_t *instance = get_card_instance(player, card);

		if( instance->damage_target_player > -1 ){
			int t_player = instance->damage_target_player;
			int t_card = instance->damage_target_card;
			if( event == EVENT_RESOLVE_ACTIVATION ){
				pump_until_eot_merge_previous(player, card, t_player, t_card, 2, 0);
			}
			return granted_generic_activated_ability(player, card, t_player, t_card, event, 0,
													 MANACOST_XR(1, 1), 0, NULL, NULL);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_tovolars_magehunters(int player, int card, event_t event)
{
  /* CARD_ID_TOVOLARS_MAGEHUNTER	12234
Tovolar's Magehunter	""
Creature - Werewolf 5/5
Whenever an opponent casts a spell, ~ deals 2 damage to that player.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

  werewolf_moon_phases(player, card, event);

  sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
    damage_player(1-player, 2, player, card);
  return 0;
}

int card_torch_fiend(int player, int card, event_t event){
/* CARD_ID_TORCH_FIEND	12279
Torch Fiend	|1|R
Creature - Devil 2/1
|R, Sacrifice ~: Destroy target artifact. */

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

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, "TARGET_ARTIFACT");
}

int card_wrack_with_madness(int player, int card, event_t event){
/* CARD_ID_WRACK_WITH_MADNESS	12295
Wrack with Madness	|3|R
Sorcery
Target creature deals damage to itself equal to its power. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, amount, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

// GREEN
int card_briarpack_alpha(int player, int card, event_t event){
/* CARD_ID_BRIARPACK_ALPHA	12143
Briarpack Alpha	|3|G
Creature - Wolf 3/3
Flash (You may cast this spell any time you could cast an instant.)
When ~ enters the battlefield, target creature gets +2/+2 until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_clinging_mists(int player, int card, event_t event){
/* CARD_ID_CLINGING_MISTS	12153
Clinging Mists	|2|G
Instant
Prevent all combat damage that would be dealt this turn.
Fateful hour - If you have 5 or less life, tap all attacking creatures. Those creatures don't untap during their controller's next untap step. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL) {
		fog_effect(player, card);
		if( fateful_hour(player) ){
			int p = current_turn;
			int count = active_cards_count[p]-1;
			while( count > -1 ){
					if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) && is_attacking(p, count) ){
						tap_card(p, count);
						effect_frost_titan(player, card, p, count);
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static const char* is_artifact_or_creature_with_flying(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	if( is_what(player, card, TYPE_ARTIFACT) ){
		return NULL;
	}
	if( is_what(player, card, TYPE_CREATURE) ){
		if ( check_for_ability(player, card, KEYWORD_FLYING) ){
			return NULL;
		}
		return gs_target_error_abilities;	// ",abilities"
	}
	return gs_target_error_type;	// ",type"
}

int card_crushing_vines(int player, int card, event_t event){
/* CARD_ID_CRUSHING_VINES	12155
Crushing Vines	|2|G
Instant
Choose one -
* Destroy target creature with flying.
* Destroy target artifact. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.extra = target_predicate_extra(is_artifact_or_creature_with_flying);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET|GS_LITERAL_PROMPT, &td, "Select target artifact or creature with flying.",
						1, NULL);
}

int card_dawntrader_elk(int player, int card, event_t event){
/* CARD_ID_DAWNTREADER_ELK	12161
Dawntreader Elk	|1|G
Creature - Elk 2/2
|G, Sacrifice ~: Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_G(1), 0, NULL, NULL);
}

int card_deranged_outcast(int player, int card, event_t event)
{
  /* Deranged Outcast	|1|G
   * Creature - Human Rogue 2/1
   * |1|G, Sacrifice a Human: Put two +1/+1 counters on target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  new_default_test_definition(&test, 0x47, "Select an Human to sacrifice.");
  test.subtype = 0x5b;
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                             COUNTER_P1_P1, 2);
  }
  result = generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET, 1, 0, 0, 1, 0, 0, 0, &td, "TARGET_CREATURE", TYPE_NONE,
      &test, SARV_0);
  return result;
}

int card_favor_of_the_woods(int player, int card, event_t event){
/* CARD_ID_FAVOR_OF_THE_WOODS	12179
Favor of the Woods	|2|G
Enchantment - Aura
Enchant creature
Whenever enchanted creature blocks, you gain 3 life. */

	if( in_play(player, card) && !is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_DECLARE_BLOCKERS &&
			blocking(instance->damage_target_player, instance->damage_target_card, event) ){
			gain_life(player, 3);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return disabling_aura(player, card, event);
}

int card_feed_the_pack(int player, int card, event_t event){
/* Feed the Pack	|5|G
 * Enchantment
 * At the beginning of your end step, you may sacrifice a nontoken creature.
 * If you do, put X 2/2 |Sgreen Wolf creature tokens onto the battlefield, where X is the sacrificed creature's toughness. */

	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a nontoken creature to sacrifice.");
		test.type_flag = F1_NO_TOKEN;

		if( new_can_sacrifice(player, card, player, &test) ){
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_RETURN_CHOICE, &test);
			if( sac ){
				int amount = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				generate_tokens_by_id(player, card, CARD_ID_WOLF, amount);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_ghoultreee(int player, int card, event_t event){
/* CARD_ID_GHOULTREE	12189
Ghoultree	|7|G
Creature - Zombie Treefolk 10/10
~ costs |1 less to cast for each creature card in your graveyard. */

	if( event == EVENT_MODIFY_COST ){
		COST_COLORLESS-=count_graveyard_by_type(player, TYPE_CREATURE);
	}

	return 0;
}

int card_gravetiller_wurm(int player, int card, event_t event){
/* Gravetiller Wurm	|5|G
 * Creature - Wurm 4/4
 * Trample
 * Morbid - ~ enters the battlefield with four +1/+1 counters on it if a creature died this turn. */

	if( (event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE ){
		if( morbid() ){
			enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);
		}
	}

	return 0;
}

int card_grim_flowering(int player, int card, event_t event){
/* CARD_ID_GRIM_FLOWERING	12195
Grim Flowering	|5|G
Sorcery
Draw a card for each creature card in your graveyard. */

	if( event == EVENT_RESOLVE_SPELL){
		draw_cards(player, count_graveyard_by_type(player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_hunger_of_the_howlpack(int player, int card, event_t event){

	/* Hunger of the Howlpack	|G
	 * Instant
	 * Put a +1/+1 counter on target creature.
	 * Morbid - Put three +1/+1 counters on that creature instead if a creature died this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = 1;
			if( morbid() ){
				amount+=2;
			}
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_increasing_savagery(int player, int card, event_t event){
/* CARD_ID_INCREASING_SAVAGERY	12219
Increasing Savagery	|2|G|G
Sorcery
Put five +1/+1 counters on target creature. If ~ was cast from a graveyard, put ten +1/+1 counters on that creature instead.
Flashback |5|G|G (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if(event == EVENT_RESOLVE_SPELL ){
			if( get_flashback(player, card) ){
				add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 10);
				kill_card(player, card, KILL_EXILE);
			}
			else{
				add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 5);
				kill_card(player, card, KILL_DESTROY);
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_XG(5, 2));
}

int card_krallenhorde_killer(int player, int card, event_t event){
/* CARD_ID_KRALLENHORDE_KILLER	12293
Krallenhorde Killer	""
Creature - Werewolf 2/2
|3|G: ~ gets +4/+4 until end of turn. Activate this ability only once each turn.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_shade(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XG(3, 1), 4, 4, 0, 0) ){
			return 4;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_XG(3, 1), 4, 4, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_XG(3, 1), 0, NULL, NULL);
}

int card_lost_in_the_woods(int player, int card, event_t event)
{
  /* CARD_ID_LOST_IN_THE_WOODS	12226
   * Lost in the Woods	|3|G|G
   * Enchantment
   * Whenever a creature attacks you or a planeswalker you control, reveal the top card of your library.
   * If it's |Ha Forest card, remove that creature from combat. Then put the revealed card on the bottom of your library. */

  int result;
  card_instance_t *instance;
  card_instance_t *attacker;
  int attacker_index;
  uint8_t *attackers;
  store_attackers(player, card, event, DAT_TRACK, 1 - player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
  {
    instance = get_card_instance(player, card);
    attacker_index = (int32_t)((uint32_t)instance->info_slot - 1U);
    attackers = (uint8_t *)&instance->targets[2].player;
    if (attacker_index >= 0)
    {
      do
      {
        if (*deck_ptr[player] == -1)
          break;
        attacker = in_play(current_turn, (unsigned int)attackers[attacker_index]);
        if (attacker != NULL)
        {
          reveal_card_iid(player, card, *deck_ptr[player]);
          result = has_subtype_by_id(player, (unsigned int)cards_data[*deck_ptr[player]].id,
                                     SUBTYPE_FOREST);
          if (result != 0)
          {
            remove_from_combat(current_turn, (unsigned int)attackers[attacker_index]);
          }
          put_top_card_of_deck_to_bottom(player);
        }
        --attacker_index;
      } while (attacker_index != -1);
    }
  }
  return global_enchantment(player, card, event);
}

int card_predator_ooze(int player, int card, event_t event){
/* CARD_ID_PREDATOR_OOZE	12242
Predator Ooze	|G|G|G
Creature - Ooze 1/1
Indestructible
Whenever ~ attacks, put a +1/+1 counter on it.
Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~. */

	indestructible(player, card, event);

	// Whenever ~ attacks, put a +1/+1 counter on it.
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY | DAT_STORE_IN_TARGETS_3, 0);
	if (event == EVENT_DAT_ABILITY){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	// Whenever a creature dealt damage by ~ this turn dies, put a +1/+1 counter on ~.
	if( sengir_vampire_trigger(player, card, event, 2) ){
		card_instance_t* instance = get_card_instance(player, card);
		add_counters(player, card, player, card, COUNTER_P1_P1, instance->targets[11].card);
		instance->targets[11].card = 0;
	}

	return 0;
}

int card_scorned_villager(int player, int card, event_t event){
	/* Scorned Villager	|1|G
	 * Creature - Human Werewolf 1/1
	 * |T: Add |G to your mana pool.
	 * At the beginning of each upkeep, if no spells were cast last turn, transform ~.
	 * (Moonscarred Werewolf - 2/2) Vigilance
	 * |T: Add |G|G to your mana pool.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform Moonscarred Werewolf. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	return mana_producing_creature(player, card, event, 24, COLOR_GREEN, 1);
}

int card_moonscarred_werewolf(int player, int card, event_t event){
	/* Scorned Villager	|1|G
	 * Creature - Human Werewolf 1/1
	 * |T: Add |G to your mana pool.
	 * At the beginning of each upkeep, if no spells were cast last turn, transform ~.
	 * (Moonscarred Werewolf - 2/2) Vigilance
	 * |T: Add |G|G to your mana pool.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform Moonscarred Werewolf. */

	vigilance(player, card, event);

	werewolf_moon_phases(player, card, event);

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		return 0;	// Don't make extra-reluctant to attack, due to vigilance
	}
	return mana_producing_creature(player, card, event, 0, COLOR_GREEN, 2);
}

int card_silverpelt_werewolf(int player, int card, event_t event){
/* CARD_ID_SILVERPELT_WEREWOLF	12224
Silverpelt Werewolf	""
Creature - Werewolf 4/5
Whenever ~ deals combat damage to a player, draw a card.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */

	werewolf_moon_phases(player, card, event);

	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_wolfbitten_captive(int player, int card, event_t event){
/* CARD_ID_WOLFBITTEN_CAPTIVE	12292
Wolfbitten Captive	|G
Creature - Human Werewolf 1/1
|1|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn.
At the beginning of each upkeep, if no spells were cast last turn, transform ~.
(Krallenhorde Killer - 2/2) |3|G: Krallenhorde Killer gets +4/+4 until end of turn. Activate this ability only once each turn.
At the beginning of each upkeep, if a player cast two or more spells last turn, transform Krallenhorde Killer. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	return card_basking_rootwalla(player, card, event);
}

int card_strangleroor_geist(int player, int card, event_t event){
/* CARD_ID_STRANGLEROOT_GEIST	12271
Strangleroot Geist	|G|G
Creature - Spirit 2/1
Haste
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	haste(player, card, event);

	return undying(player, card, event);
}

int card_trackers_instict(int player, int card, event_t event){
/* CARD_ID_TRACKERS_INSTINCTS	12281
Tracker's Instincts	|1|G
Sorcery
Reveal the top four cards of your library. Put a creature card from among them into your hand and the rest into your graveyard.
Flashback |2|U (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		reveal_top_cards_of_library_and_choose_type(player, card, player, 4, 1, TUTOR_HAND, 0, TUTOR_GRAVE, 1, TYPE_CREATURE);
		if( get_flashback(player, card) ){
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return do_flashback(player, card, event, MANACOST_XU(2, 1));
}

int card_ulvenwald_bear(int player, int card, event_t event)
{
  /* Ulvenwald Bear	|2|G
   * Creature - Bear 2/2
   * Morbid - When ~ enters the battlefield, if a creature died this turn, put two +1/+1 counters on target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if ((event == EVENT_ETB) && (0 < creatures_dead_this_turn))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = pick_target(&td, "TARGET_CREATURE");
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 2);
      }
    }
    instance->number_of_targets = '\0';
    return 0;
  }
  return 0;
}

int card_village_survivors(int player, int card, event_t event){
/* CARD_ID_VILLAGE_SURVIVORS	12287
Village Survivors	|4|G
Creature - Human 4/5
Vigilance
Fateful hour - As long as you have 5 or less life, other creatures you control have vigilance. */

	vigilance(player, card, event);

	if( event == EVENT_ABILITIES && fateful_hour(player) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if(affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE)){
			vigilance(affected_card_controller, affected_card, event);
		}
	}

	return 0;
}

int card_vorapede(int player, int card, event_t event){
/* CARD_ID_VORAPEDE	12288
Vorapede	|2|G|G|G
Creature - Insect 5/4
Vigilance, trample
Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.)*/

	vigilance(player, card, event);

	return undying(player, card, event);
}

int card_wild_hunger(int player, int card, event_t event){
/* CARD_ID_WILD_HUNGER	12291
Wild Hunger	|2|G
Instant
Target creature gets +3/+1 and gains trample until end of turn.
Flashback |3|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

	if( IS_GS_EVENT(player, card, event) ){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 1, KEYWORD_TRAMPLE, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	return do_flashback(player, card, event, MANACOST_XR(3, 1));
}

// GOLD
static int diregraf_captain_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_DIREGRAF_CAPTAIN)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE2(instance->targets[11].player) = 1;
            return 0;
          }
        }
        test_definition_t this_test;
        default_test_definition(&this_test, TYPE_PERMANENT);
        this_test.subtype = SUBTYPE_ZOMBIE;
        count_for_gfp_ability(player, card, event, player, 0, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;
    while (amount > 0)
    {
      if (would_validate_arbitrary_target(&td, 1 - player, -1))
        lose_life(1 - player, 1);
      --amount;
    }

    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_diregraf_captain(int player, int card, event_t event){
  /* Diregraf Captain |1|U|B
   * Creature — Zombie Soldier 2/2
   * Deathtouch
   * Other Zombie creatures you control get +1/+1.
   * Whenever another Zombie you control dies, target opponent loses 1 life. */

	deathtouch(player, card, event);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)diregraf_captain_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &diregraf_captain_effect, player, card);
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
		this_test.subtype = SUBTYPE_ZOMBIE;
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, player, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.zone = TARGET_ZONE_PLAYERS;
			while (amount > 0){
				if (would_validate_arbitrary_target(&td, 1-player, -1)){ lose_life(1-player, 1); }
				--amount;
			}
		}
	}

	return boost_creature_type(player, card, event, SUBTYPE_ZOMBIE, 1, 1, 0, BCT_CONTROLLER_ONLY);
}

int card_drogskol_captain(int player, int card, event_t event)
{
/* CARD_ID_DROGSKOL_CAPTAIN	12166
Drogskol Captain	|1|W|U
Creature - Spirit Soldier 2/2
Flying
Other Spirit creatures you control get +1/+1 and have hexproof. (They can't be the targets of spells or abilities your opponents control.) */

	boost_subtype(player, card, event, SUBTYPE_SPIRIT, 1, 1, 0,SP_KEYWORD_HEXPROOF, BCT_CONTROLLER_ONLY);
	return 0;
}

int card_drogskol_reaver(int player, int card, event_t event)
{
  /* CARD_ID_DROGSKOL_REAVER	12167
Drogskol Reaver	|5|W|U
Creature - Spirit 3/5
Flying, double strike, lifelink
Whenever you gain life, draw a card. */

  lifelink(player, card, event);
  if (trigger_gain_life(player, card, event, player, RESOLVE_TRIGGER_MANDATORY) && in_play(player, card))
    draw_a_card(player);
  return 0;
}

int card_falkenrath_aristocrat(int player, int card, event_t event){
  /* Falkenrath Aristocrat |2|B|R
   * Creature — Vampire 4/1
   * Flying, haste
   * Sacrifice a creature: Falkenrath Aristocrat gains indestructible until end of turn.
   * 	If the sacrificed creature was a Human, put a +1/+1 counter on Falkenrath Aristocrat. */

	haste(player, card, event);

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_RETURN_CHOICE, &test);
			if( sac ){
				instance->info_slot = has_subtype(BYTE2(sac), BYTE3(sac), SUBTYPE_HUMAN);
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		generic_shade(player, card, event, 0, MANACOST0, 0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
		if( instance->info_slot == 1 ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
	}

	return 0;
}

extern int havengul_lich_hack;

/* ManalinkEh 2022: 0x210a2b0; original cards/dark_ascension.c:1169. */
static char *you_own(int who_chooses, int player, int card, int targeting_player,
                     int targeting_card)
{
  int result;
  char *rejection;
  result = get_owner(player, card);
  rejection = NULL;
  if (result != targeting_player)
  {
    rejection = "owner";
  }
  return rejection;
}

/* ManalinkEh 2022: 0x2112500; original cards/dark_ascension.c:2641. */
int card_increasing_vengeance(int player, int card, event_t event)
{
  /* Increasing Vengeance	|R|R
   * Instant
   * Copy target instant or sorcery spell you control. If this spell was cast from a graveyard, copy that spell twice instead. You may choose new targets for the copies.
   * Flashback |3|R|R (You may cast this card from your graveyard for its flashback cost. Then exile it.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  counterspell_target_definition(player, card, &td, 0x38);
  td.preferred_controller = player;
  td.allowed_controller = player;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = counterspell_validate(player, card, &td, 0);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
        result = get_flashback(player, card);
        if (result != 0)
        {
          copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
          kill_card(player, card, KILL_EXILE);
        }
      }
      kill_card(player, card, KILL_DESTROY);
    }
    result = do_flashback(player, card, event, 3, 0, 0, 0, 2, 0);
    return result;
  }
  result = twincast(player, card, 0x74, &td, NULL);
  return result;
}

/* ManalinkEh 2022: 0x210a600; original cards/dark_ascension.c:3707. */
static int havengul_lich_legacy(int player, int card, event_t event)
{
  if (event == EVENT_STATIC_EFFECTS && card == affected_card && player == affected_card_controller)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player < 0)
      kill_card(player, card, KILL_EXILE);
    if (!check_status(player, card, 0x10000))
      add_status(player, card, 0x10000);
  }
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player < 0)
  {
    if (event == EVENT_CLEANUP)
      kill_card(player, card, KILL_EXILE);
    return 0;
  }
  if (!effect_follows_control_of_attachment(player, card, event) && event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}
static int can_cast_creature_in_grave(int player, int card, int t_player){
	int i;
	for(i=0; i<2; i++){
		if( i == t_player || t_player == 2 ){
			int cg = count_graveyard(i);
			const int *grave = get_grave(i);
			while( cg > -1 ){
					if( is_what(-1, grave[cg], TYPE_CREATURE) ){
						int result = has_mana_to_cast_id(player, EVENT_CAN_CAST, cards_data[grave[cg]].id);
						if( result ){
							return 1;
						}
					}
					cg--;
			}
		}
	}
	return 0;
}

static int can_cast_this(int iid, int unused, int player, int card)
{
	card_instance_t *instance = get_card_instance(player, card);
	return has_mana_to_cast_iid(instance->targets[0].player, EVENT_CAN_CAST, iid);
}

int card_huntmaster_of_the_fells(int player, int card, event_t event){
/* Huntmaster of the Fells	|2|R|G [Ravager of the Fells]
 * Creature - Human Werewolf 2/2
 * Whenever this creature enters the battlefield or transforms into ~, put a 2/2 |Sgreen Wolf creature token onto
 *  	the battlefield and you gain 2 life.
 * At the beginning of each upkeep, if no spells were cast last turn, transform ~. */

	double_faced_card(player, card, event);

	human_moon_phases(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_TRANSFORMED || event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_WOLF);
		gain_life(player, 2);
	}

	return 0;
}

int card_ravager_of_the_fells(int player, int card, event_t event)
{
	/* Ravager of the Fells	"" [Huntmaster of the Fells]	0x200ba7e
	 * Creature - Werewolf 4/4
	 * Trample
	 * Whenever this creature transforms into ~, it deals 2 damage to target opponent or planeswalker and 2 damage to up to one target creature that player or that planeswalker's controller controls.
	 * At the beginning of each upkeep, if a player cast two or more spells last turn, transform ~. */
  werewolf_moon_phases(player, card, event);

  if (event == EVENT_TRANSFORMED)
	{
	  target_definition_t td_opw;
	  opponent_or_pw_target_definition(player, card, &td_opw);
	  if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER"))
		{
		  card_instance_t* instance = get_card_instance(player, card);

		  target_definition_t td_creature;
		  default_target_definition(player, card, &td_creature, TYPE_CREATURE);
		  td_creature.allowed_controller = td_creature.preferred_controller = instance->targets[0].player;

		  if (can_target(&td_creature) && new_pick_target(&td_creature, "TARGET_CREATURE", 1, 0))
			{
			  damage_target0(player, card, 2);
			  damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player, card);
			}
		  else
			damage_target0(player, card, 2);

		  instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_immerwolf(int player, int card, event_t event){

	/* Immerwolf	|1|R|G
	 * Creature - Wolf 2/2
	 * Intimidate
	 * Other Wolf and Werewolf creatures you control get +1/+1.
	 * Non-Human Werewolves you control can't transform. */

	intimidate(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( affected_card_controller == player && ! affect_me(player, card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			in_play(affected_card_controller, affected_card) )
		{
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_WOLF) ||
				has_subtype(affected_card_controller, affected_card, SUBTYPE_WEREWOLF) )

			{
				event_result++;
			}
		}
	}

	return 0;
}

int card_sorins_emblem(int player, int card, event_t event){
  /* Sorin, Lord of Innistrad Emblem	""
   * Emblem - Sorin
   * Creatures you control get +1/+0. */

	return boost_creature_type(player, card, event, -1, 1, 0, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
}

// ARTIFACTS
static int sorin_lord_effect(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance= get_card_instance(player, card);
		int good = 0;
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( affected_card_controller == instance->targets[i].player && affected_card == instance->targets[i].card ){
				good = 1;
				break;
			}
		}
		if( good == 1 ){
			count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_PERMANENT, NULL, GFPC_TRACK_DEAD_CREATURES, 3);
		}
	}

	int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( result ){
		int i;
		card_instance_t *instance= get_card_instance(player, card);
		for(i=3; i<result+3; i++){
			int owner = instance->targets[i].player;
			int iid = instance->targets[i].card;
			seek_grave_for_id_to_reanimate(player, -1, owner, cards_data[iid].id, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_sorin_lord_of_innistrad(int player, int card, event_t event){
/* Sorin, Lord of Innistrad	|2|W|B
 * Planeswalker - Sorin (3)
 * +1: Put a 1/1 |Sblack Vampire creature token with lifelink onto the battlefield.
 * -2: You get an emblem with "Creatures you control get +1/+0."
 * -6: Destroy up to three target creatures and/or other planeswalkers. Return each card put into a graveyard this way to the
 * 		battlefield under your control. */
	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.special = TARGET_SPECIAL_NOT_ME;
		if (player == AI){
			td.allowed_controller = 1-player;
		}

		card_instance_t *instance = get_card_instance( player, card );

		enum{
			CHOICE_VAMPIRE = 1,
			CHOICE_EMBLEM,
			CHOICE_KILL_PERMANENTS
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Generate a Vampire", 1, 10, 1,
						"Emblem", 1, ! check_battlefield_for_id(player, CARD_ID_SORINS_EMBLEM) ? count_subtype(player, TYPE_CREATURE, -1)*3 : 0, -2,
						"Kill permanents", can_target(&td), 15, -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
			instance->number_of_targets = 0;
			switch (choice)
			{
				case CHOICE_VAMPIRE:
				case CHOICE_EMBLEM:
					break;

				case CHOICE_KILL_PERMANENTS:
				{
					int i = 0;
					while( i < 3 && can_target(&td) ){
							if( new_pick_target(&td, "Select another target creature or planeswalker.", i, GS_LITERAL_PROMPT) ){
								state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
								i++;
							}
							else{
								break;
							}
					}
					int k;
					for(k=0; k<i; k++){
						state_untargettable(instance->targets[k].player, instance->targets[k].card, 0);
					}
				}
				break;
			}
		}
		else{	// event == EVENT_RESOLVE_ACTIVATION
			switch (choice)
			{
				case CHOICE_VAMPIRE:
				{
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_VAMPIRE, &token);
					token.s_key_plus = SP_KEYWORD_LIFELINK;
					token.pow = 1;
					token.tou = 1;
					generate_token(&token);
				}
				break;

				case CHOICE_EMBLEM:
					generate_token_by_id(player, card, CARD_ID_SORINS_EMBLEM);
					break;

				case CHOICE_KILL_PERMANENTS:
				{
					if( instance->number_of_targets ){
						int legacy = create_legacy_effect(player, instance->parent_card, &sorin_lord_effect);
						card_instance_t *leg = get_card_instance(player, legacy);
						int i;
						for(i=0; i<instance->number_of_targets; i++){
							if( validate_target(player, card, &td, i) ){
								leg->targets[i] = instance->targets[i];
								leg->number_of_targets++;
								kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
							}
						}
					}
					break;
				}

			}
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_stromkirk_captain(int player, int card, event_t event){
/* CARD_ID_STROMKIRK_CAPTAIN	12272
Stromkirk Captain	|1|B|R
Creature - Vampire Soldier 2/2
First strike
Other Vampire creatures you control get +1/+1 and have first strike. */

	return boost_creature_type(player, card, event, SUBTYPE_VAMPIRE, 1, 1, KEYWORD_FIRST_STRIKE, BCT_CONTROLLER_ONLY);
}

int card_avacyns_collar(int player, int card, event_t event){
	/* Avacyn's Collar	|1
	 * Artifact - Equipment
	 * Equipped creature gets +1/+0 and has vigilance.
	 * Whenever equipped creature dies, if it was a Human, put a 1/1 |Swhite Spirit creature token with flying onto the battlefield.
	 * Equip |2 */

	if (attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		card_instance_t* instance = get_card_instance(player, card);
		if (has_subtype(instance->damage_target_player, instance->damage_target_card, SUBTYPE_HUMAN)){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.pow = token.tou = 1;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = COLOR_TEST_WHITE;
			generate_token(&token);
		}
	}
	return vanilla_equipment(player, card, event, 2, 1, 0, 0, SP_KEYWORD_VIGILANCE);
}

int card_chalice_of_life(int player, int card, event_t event){
/* CARD_ID_CHALICE_OF_LIFE	12147
Chalice of Life	|3
Artifact
|T: You gain 1 life. Then if you have at least 10 life more than your starting life total, transform ~.
(Chalice of Death) |T: Target player loses 5 life. */

	double_faced_card(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[8].card = get_starting_life_total(player);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_life(player, 1);
		if( life[player] > (instance->targets[8].card + 9) ){
			transform(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_chalice_of_death(int player, int card, event_t event){
/* CARD_ID_CHALICE_OF_DEATH	12148
Chalice of Death	""
Artifact
|T: Target player loses 5 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 5);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_elbrus_the_binding_blade(int player, int card, event_t event){

	/* Elbrus, the Binding Blade	|7 [Withengar Unbound]
	 * Legendary Artifact - Equipment
	 * Equipped creature gets +1/+0.
	 * When equipped creature deals combat damage to a player, unattach ~, then transform it.
	 * Equip |1 */

	check_legend_rule(player, card, event);

	double_faced_card(player, card, event);

	if( equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER) ){
		unattach(player, card);
		transform(player, card);
	}

	return vanilla_equipment(player, card, event, 1, 1, 0, 0, 0);
}

int card_withengar_unbound(int player, int card, event_t event){
/* CARD_ID_WITHENGAR_UNBOUND	12170
Withengar Unbound	""
Legendary Creature - Demon 13/13
Flying, intimidate, trample (A creature with intimidate can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Whenever a player loses the game, put thirteen +1/+1 counters on ~. */

	check_legend_rule(player, card, event);

	intimidate(player, card, event);

	return 0;
}

int card_executioners_hood(int player, int card, event_t event){
/* CARD_ID_EXECUTIONERS_HOOD	12173
Executioner's Hood	|2
Artifact - Equipment
Equipped creature has intimidate. (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	card_instance_t *instance= get_card_instance(player, card);

	if( is_equipping(player, card) ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		intimidate(p, c, event);
	}

	return basic_equipment(player, card, event, 2);
}

int card_heavy_mattock(int player, int card, event_t event){
/* CARD_ID_HEAVY_MATTOCK	12203
Heavy Mattock	|3
Artifact - Equipment
Equipped creature gets +1/+1.
As long as equipped creature is a Human, it gets an additional +1/+1.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	card_instance_t *instance= get_card_instance(player, card);

	if( is_equipping(player, card) ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		int amount = has_subtype(p, c, SUBTYPE_HUMAN) ? 2 : 1;
		modify_pt_and_abilities(p, c, event, amount, amount, 0);
	}

	return basic_equipment(player, card, event, 2);
}

int card_helvault(int player, int card, event_t event){
  /* Helvault |3
   * Legendary Artifact
   * {1}, {T}: Exile target creature you control.
   * {7}, {T}: Exile target creature you don't control.
   * When Helvault is put into a graveyard from the battlefield, return all cards exiled with it to the battlefield under their owners' control. */

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
	    if( is_this_dying(player, card) ){
		    int leg = 0;
		    int idx = 0;
		    int* loc;
		    while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
				    int owner = (*loc & 0x80000000) ? 1 : 0;
				    int iid = *loc & ~0x80000000;
				    *loc = -1;
				    int card_added = add_card_to_hand(owner, iid);
					remove_card_from_rfg(owner, cards_data[iid].id);
				    put_into_play(owner, card_added);
		   }
	    }
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = player;
	td2.preferred_controller = player;
	if( player == AI ){
		td2.required_state = TARGET_STATE_DESTROYED;
	}

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE);
	td3.allowed_controller = 1-player;
	td3.preferred_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_EXILE_YOURS = 1,
		CHOICE_EXILE_OPP,
		CHOICE_SHOW_EXILED
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( player == HUMAN &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL) )
		{
			return 1;
		}
		if( player == AI &&
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_REGENERATION, MANACOST_X(1), 0, &td2, NULL) )
		{
			return 99;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED |GAA_CAN_TARGET, MANACOST_X(7), 0, &td3, NULL) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abils[4] = {
						0,
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED |GAA_CAN_TARGET, MANACOST_X(7), 0,
												&td3, NULL),
						player == HUMAN && exiledby_count(player, card, ANYBODY) ? 1 : 0
		};
		if( player == HUMAN ){
			abils[1] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0,
												&td, NULL);
		}
		else{
			abils[1] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_REGENERATION,
												MANACOST_X(1), 0, &td2, NULL);
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Exile a creature you control", 	abils[1], 5,
							"Exile opponent's creature", 		abils[2], 10,
							"Show exiled creatures", 			abils[3], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EXILE_YOURS ){
			if( player == HUMAN ){
				return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0,
												&td, "Select target creature you control.");
			}
			else{
				generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_REGENERATION, MANACOST_X(1), 0,
											&td2, NULL);
			}
		}
		if( choice == CHOICE_EXILE_OPP ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED |GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_X(7), 0,
											&td3, "Select target creature you don't control.");
		}
		if( choice == CHOICE_SHOW_EXILED ){
			exiledby_choose(player, card, CARD_ID_HELVAULT, EXBY_CHOOSE, 0, "Creature", 0);
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int good = 0;
		if( instance->info_slot == CHOICE_EXILE_YOURS && player == HUMAN && valid_target(&td) ){
			good = 1;
		}
		if( instance->info_slot == CHOICE_EXILE_YOURS && player == AI && valid_target(&td2) ){
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
			good = 1;
		}
		if( instance->info_slot == CHOICE_EXILE_OPP && valid_target(&td3) ){
			good = 1;
		}
		if( good ){
			exile_card_and_remember_it_on_exiledby(instance->parent_controller, instance->parent_card,
													instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

// LANDS
int card_jar_of_eyeballs(int player, int card, event_t event)
{
  /* Jar of Eyeballs	|3
   * Artifact
   * Whenever a creature you control dies, put two eyeball counters on ~.
   * |3, |T, Remove all eyeball counters from ~: Look at the top X cards of your library, where X is the number of eyeball counters removed
   * 			 this way. Put one of them into your hand and the rest on the bottom of your library in any order. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
    count_for_gfp_ability(player, card, 0x77, player, 2, NULL);
  result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0 && event == EVENT_GRAVEYARD_FROM_PLAY)
    return 0;
  if (result != 0)
    add_counters(player, card, player, card, COUNTER_EYEBALL, result * 2);
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 3, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    return result;
  }
  if (event == EVENT_ACTIVATE)
  {
    result = count_counters(player, card, COUNTER_EYEBALL);
    if (result < 3)
    {
      ai_modifier = ai_modifier + -0x32;
    }
    else
    {
      result = count_counters(player, card, COUNTER_EYEBALL);
      ai_modifier = ai_modifier + result * 5 + -10;
    }
    charge_mana_for_activated_ability(player, card, 3, 0, 0, 0, 0, 0);
    if (spell_fizzled != 1)
    {
      instance = get_card_instance(player, card);
      result = count_counters(player, card, COUNTER_EYEBALL);
      instance->info_slot = result;
      remove_all_counters(player, card, player, card, COUNTER_EYEBALL);
      tap_card(player, card);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    impulse_effect(player, instance->info_slot, 0);
    return 0;
  }
  return 0;
}

int card_warden_of_the_wall(int player, int card, event_t event){
/* CARD_ID_WARDEN_OF_THE_WALL	12290
Warden of the Wall	|3
Artifact
~ enters the battlefield tapped.
|T: Add |C to your mana pool.
As long as it's not your turn, ~ is a 2/3 Gargoyle artifact creature with flying. */

	comes_into_play_tapped(player, card, event);

	if( event == EVENT_ABILITIES && affect_me(player, card) && current_turn != player &&
		in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) )
	{
		event_result |= KEYWORD_FLYING;
	}

	if( event == EVENT_CHANGE_TYPE && affect_me(player, card) && current_turn != player && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->targets[12].card == -1 ){
			int newtype = create_a_card_type(instance->internal_card_id);
			cards_at_7c7000[newtype]->type |= (cards_data[instance->internal_card_id].type | TYPE_CREATURE);
			cards_at_7c7000[newtype]->power = 2;
			cards_at_7c7000[newtype]->toughness = 3;
			instance->targets[12].card = newtype;
		}
		event_result = instance->targets[12].card;
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[12].card = -1;
	}

	return mana_producer(player, card, event);
}

int card_wolfhunters_quiver(int player, int card, event_t event){
	/* Wolfhunter's Quiver	|1	0x2004f49
	 * Artifact - Equipment
	 * Equipped creature has "|T: This creature deals 1 damage to any target" and "|T: This creature deals 3 damage to target Werewolf creature."
	 * Equip |5 */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	target_definition_t td2;
	if( is_equipping(player, card) ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		default_target_definition(p, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		default_target_definition(p, c, &td2, TYPE_CREATURE);
		td2.required_subtype = SUBTYPE_WEREWOLF;
	}

	enum{
		CHOICE_EQUIP = 1,
		DAMAGE_TARGET,
		CHOICE_DAMAGE_WEREWOLF
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( generic_activated_ability(p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
				return 1;
			}
			if( generic_activated_ability(p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td2, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 5);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[4] = {0, can_activate_basic_equipment(player, card, event, 5), 0, 0};
		int p = -1;
		int c = -1;
		if( is_equipping(player, card) ){
			p = instance->damage_target_player;
			c = instance->damage_target_card;
			abils[2] = generic_activated_ability(p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
			abils[3] = generic_activated_ability(p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td2, NULL);
		}

		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip",				abils[1], 5,
							"Damage any target",	abils[2], 1,
							"Damage Werewolf",		abils[3], 1);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 5);
		}
		if( choice == DAMAGE_TARGET ){
			return generic_activated_ability(p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
		}
		if( choice == CHOICE_DAMAGE_WEREWOLF ){
			return generic_activated_ability(p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
											&td2, "Select target Werewolf.");
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		int choice = instance->info_slot;
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 5);
		}
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
		int p = parent->damage_target_player;
		int c = parent->damage_target_card;
		if( p > -1 && c > -1 ){
			if( choice == DAMAGE_TARGET ){
				if( validate_target(p, c, &td, 0) ){
					damage_target0(p, c, 1);
				}
			}
			if( choice == CHOICE_DAMAGE_WEREWOLF ){
				if( validate_target(p, c, &td2, 0) ){
					damage_target0(p, c, 3);
				}
			}
		}
	}

	return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_grim_backwoods(int player, int card, event_t event){
  /* Grim Backwoods	""
   * Land
   * |T: Add |C.
   * |2|B|G, |T, Sacrifice a creature: Draw a card. */

/*
Grim Backwoods
Land
{T}: Add {C} to your mana pool.
{2}{B}{G}, {T}, Sacrifice a creature: Draw a card.
*/

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_AND_DRAW
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST_XBG(3, 1, 1), 0,
									NULL, NULL) )
		{
			int priority_sac = (count_subtype(player, TYPE_CREATURE, -1)*2)+(35-(hand_count[player]*5));
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Get mana", 1, 5,
							"Sac crit & draw", 1, priority_sac);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAC_AND_DRAW ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST_XBG(2, 1, 1), 0, NULL, NULL);
			instance->info_slot = CHOICE_SAC_AND_DRAW;
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
			tapped_for_mana_color = -1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_AND_DRAW ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_haunted_fengraf(int player, int card, event_t event){
  /* Haunted Fengraf	""
   * Land
   * |T: Add |C.
   * |3, |T, Sacrifice this land: Return a creature card at random from your graveyard to your hand. */

/*
Haunted Fengraf
Land
{T}: Add {C} to your mana pool.
{3}, {T}, Sacrifice Haunted Fengraf: Return a creature card at random from your graveyard to your hand.
*/
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_RETURN_RANDOM,
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(4), 0, NULL, NULL) )
		{
			int priority_sac = (count_subtype(player, TYPE_LAND, -1)*2)+(count_graveyard_by_type(player, TYPE_CREATURE)*2);
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Get mana", 1, 5,
							"Sac & return random crit", 1, priority_sac);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_RETURN_RANDOM ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL);
			instance->info_slot = CHOICE_RETURN_RANDOM;
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_RETURN_RANDOM ){
			tutor_random_permanent_from_grave(player, card, player, TUTOR_HAND, TYPE_CREATURE, 1, REANIMATE_DEFAULT);
		}
	}

	return 0;
}

int card_vault_of_the_archangel(int player, int card, event_t event){
  /* Vault of the Archangel	""
   * Land
   * |T: Add |C.
   * |2|W|B, |T: Creatures you control gain deathtouch and lifelink until end of turn. */

/*
Vault of the Archangel
Land
{T}: Add {C} to your mana pool.
{2}{W}{B}, {T}: Creatures you control gain deathtouch and lifelink until end of turn.
*/
	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	enum{
		CHOICE_MANA = 1,
		CHOICE_PUMP,
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_XBW(3, 1, 1), 0,
									NULL, NULL) )
		{
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Get mana", 1, 5,
							"Give deathtouch and lifelink", 1, current_phase == PHASE_AFTER_BLOCKING ? 15 : 5);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_PUMP ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, 0, MANACOST_XBW(2, 1, 1), 0, NULL, NULL);
			instance->info_slot = CHOICE_PUMP;
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
			tapped_for_mana_color = -1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1,
									0, 0, 0, SP_KEYWORD_LIFELINK+SP_KEYWORD_DEATHTOUCH);
		}
	}

	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x210b2a0; original cards/dark_ascension.c:3693. */
static int find_attached_havengul_legacy(int player, int card,
                                         int (*func_ptr)(int, int, event_t),
                                         int target_player, int target_card)
{
  for (int eff = 0; eff < active_cards_count[player]; ++eff)
  {
    card_instance_t *inst = in_play(player, eff);
    if (inst && inst->internal_card_id == LEGACY_EFFECT_CUSTOM &&
        inst->info_slot_fn == func_ptr && inst->damage_source_player == player &&
        inst->damage_source_card == card && inst->damage_target_player == target_player &&
        inst->damage_target_card == target_card)
      return eff;
  }
  return -1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x210b320; original cards/dark_ascension.c:3728. */
static int spell_has_flashback_legacy_for_hl(int player, int card, event_t event)
{
  if (event == EVENT_CLEANUP || event == EVENT_STATIC_EFFECTS)
  {
    if (event == EVENT_STATIC_EFFECTS && get_card_instance(player, card)->targets[1].card != -1)
      return 0;
    kill_card(player, card, KILL_EXILE);
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    int pos = find_in_graveyard_by_source(instance->targets[2].card, instance->targets[1].player,
                                          instance->targets[1].card);
    instance->targets[1].card = pos;
    if (pos != -1)
    {
      if ((instance->targets[2].player & 1) == 0)
      {
        if (!has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE, instance->targets[0].card))
          return 0;
        if (has_no_mana_cost(-1, instance->targets[0].card))
          return 0;
      }
      int rval = can_legally_play_iid(player, instance->targets[0].card);
      if (rval != 0 && ((cards_data[instance->targets[0].card].type & 0x30) != 0 ||
                        can_sorcery_be_played(player, EVENT_CAN_ACTIVATE)))
        return rval;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    uint32_t mode = instance->targets[2].player;
    int iid = instance->targets[0].card;
    if ((mode & 1) == 0)
    {
      if (!charge_mana_from_id(player, -1, EVENT_ACTIVATE, (uint16_t)cards_data[iid].id))
        return 0;
      iid = instance->targets[0].card;
      mode = instance->targets[2].player;
    }
    int hlp = instance->targets[3].player, hlc = instance->targets[3].card;
    int source = instance->targets[1].player, hint = instance->targets[1].card,
        gyp = instance->targets[2].card;
    kill_card(player, card, KILL_EXILE);
    int pos = find_in_graveyard_by_source(gyp, source, hint);
    if ((mode & 2) == 0)
      play_card_in_grave_for_free_and_exile_it(player, gyp, pos);
    else
    {
      int played = play_card_in_grave_for_free(player, gyp, pos);
      if (player != gyp)
        get_card_instance(player, played)->state |= 0x1000;
      int legacy = find_attached_havengul_legacy(hlp, hlc, havengul_lich_legacy, hlp, hlc);
      card_instance_t *leg =
          legacy < 0 ? get_card_instance(hlp, hlc) : get_card_instance(player, legacy);
      if ((cards_data[iid].extra_ability & 0x1001) != 0)
      {
        for (int i = 1; i != 19; ++i)
        {
          if (leg->targets[i].card == -1)
          {
            leg->targets[i].card = iid;
            cant_be_responded_to = 1;
            return 0;
          }
          if (leg->targets[i].player == -1)
          {
            leg->targets[i].player = iid;
            cant_be_responded_to = 1;
            return 0;
          }
        }
      }
    }
    cant_be_responded_to = 1;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
static void create_spell_has_flashback_legacy_for_hl(int player, int card, int graveyard_pos,
                                                    flashback_legacy_t mode)
{
  if (graveyard_pos < 0)
    return;
  int legacy = create_legacy_activate(player, card, spell_has_flashback_legacy_for_hl);
  card_instance_t *instance = get_card_instance(player, legacy);
  int graveyard_player = get_card_instance(player, card)->targets[0].player;
  const int *grave = get_grave(graveyard_player);
  instance->targets[0].card = grave[graveyard_pos];
  instance->targets[1].player = graveyard_source[graveyard_player][graveyard_pos];
  instance->targets[1].card = graveyard_pos;
  instance->targets[2].player = mode;
  instance->targets[2].card = graveyard_player;
  instance->targets[3].player = player;
  instance->targets[3].card = card;
  create_card_name_legacy(player, legacy, cards_data[grave[graveyard_pos]].id);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x210b160; original cards/dark_ascension.c:3829. */
/* The release specializes t_player to 2; only that domain is recovered here. */
static int can_cast_creature_in_grave_with_havengul_lich(int player, int card, int t_player)
{
  if (generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                          GAA_NONE, 1, 0, 0, 0, 0, 0, 0, NULL, NULL,
                                                          TYPE_CREATURE, NULL, 0x1000))
  {
    for (int p = 0; p != 2; ++p)
    {
      int cg = count_graveyard(p);
      const int *grave = get_grave(p);
      for (; cg >= 0; --cg)
      {
        if (is_what(-1, grave[cg], 2))
        {
          ++havengul_lich_hack;
          int result =
              has_mana_to_cast_id(player, EVENT_CAN_CAST, (uint16_t)cards_data[grave[cg]].id);
          --havengul_lich_hack;
          if (result)
            return 1;
        }
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x210b650; original cards/dark_ascension.c:3854. */
static int can_use_stolen_ability_with_havengul_lich(int player, int card)
{
  int legacy = find_attached_havengul_legacy(player, card, havengul_lich_legacy, player, card);
  card_instance_t *leg = get_card_instance(player, legacy < 0 ? card : legacy);
  if (player != 1)
  {
    for (int i = 1; i != 19; ++i)
    {
      if (leg->targets[i].card != -1 &&
          cards_data[leg->targets[i].card].code_pointer(player, card, EVENT_CAN_ACTIVATE))
        return 1;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x210a700; original cards/dark_ascension.c:3873. */
static int can_cast_this_havengul(int iid, int unused, int player, int card)
{
  return has_mana_to_cast_iid(player, EVENT_CAN_CAST, iid);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */
/* ManalinkEh 2022: 0x2114ea0; original cards/dark_ascension.c:3876. */
int card_havengul_lich(int player, int card, event_t event)
{
  /* CARD_ID_HAVENGUL_LICH	12200
   * Havengul Lich	|3|U|B
   * Creature - Zombie Wizard 4/4
   * |1: You may cast target creature card in a graveyard this turn.
   * When you cast that card this turn, ~ gains all activated abilities of that card until end of turn. */

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (affect_me(player, card))
    {
      int found = 0;
      for (int c = 0; c < active_cards_count[player]; ++c)
      {
        if (in_play(player, c) && is_what(player, c, 0x80))
        {
          card_instance_t *inst = get_card_instance(player, c);
          if (inst->info_slot_fn == havengul_lich_legacy &&
              ((inst->damage_target_player == player && inst->damage_target_card == card) ||
               (inst->targets[0].player == player && inst->targets[0].card == card)))
          {
            found = 1;
            break;
          }
        }
      }
      if (!found)
      {
        int legacy =
            create_targetted_legacy_effect(player, card, havengul_lich_legacy, player, card);
        card_instance_t *inst = get_card_instance(player, legacy);
        inst->targets[0].player = player;
        inst->targets[0].card = card;
        inst->number_of_targets = 1;
        add_status(player, legacy, 0x10000);
      }
    }
    get_card_instance(player, card);
    test_definition_t test;
    new_default_test_definition(&test, 2, "Select target creature card.");
    return 0;
  }
  card_instance_t *instance = get_card_instance(player, card);
  test_definition_t test;
  new_default_test_definition(&test, 2, "Select target creature card.");
  test.ai_selection_mode = 0x101;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (!can_use_activated_abilities(player, card))
      return 0;
    if (player != 1)
    {
      if (generic_activated_ability_with_target_in_grave(player, card, EVENT_CAN_ACTIVATE,
                                                              GAA_NONE, 1, 0, 0, 0, 0, 0, 0, NULL,
                                                              NULL, TYPE_NONE, &test, 0x1000))
        return 1;
      return can_use_stolen_ability_with_havengul_lich(player, card) != 0;
    }
    if (!has_mana_for_activated_ability(1, card, 1, 0, 0, 0, 0, 0))
      return 0;
    if (!graveyard_has_shroud(2))
      return can_cast_creature_in_grave_with_havengul_lich(1, card, 2);
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    uint32_t mode = instance->eot_toughness;
    if (mode == 0x42)
    {
      int selected =
          validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
      if (selected >= 0)
      {
        int hlp = instance->parent_controller, hlc = instance->parent_card;
        int legacy = create_legacy_activate(hlp, hlc, spell_has_flashback_legacy_for_hl);
        card_instance_t *leg = get_card_instance(hlp, legacy);
        card_instance_t *parent = get_card_instance(hlp, hlc);
        int gyp = parent->targets[0].player;
        const int *grave = get_grave(gyp);
        leg->targets[0].card = grave[selected];
        leg->targets[1].card = selected;
        leg->targets[1].player = graveyard_source[gyp][selected];
        leg->targets[2].player = 2;
        leg->targets[2].card = gyp;
        leg->targets[3].player = hlp;
        leg->targets[3].card = hlc;
        create_card_name_legacy(hlp, legacy, (uint16_t)cards_data[grave[selected]].id);
      }
      mode = instance->eot_toughness;
    }
    if ((int32_t)mode < 0 && mode != 0x80000000)
    {
      cards_data[mode & 0x0fffffff].code_pointer(player, card, EVENT_RESOLVE_ACTIVATION);
      instance->eot_toughness = 0;
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE)
    return 0;
  int use_grave = 0;
  if (player == 0)
  {
    if (generic_activated_ability_with_target_in_grave(0, card, EVENT_CAN_ACTIVATE, GAA_NONE,
                                                            1, 0, 0, 0, 0, 0, 0, NULL, NULL,
                                                            TYPE_NONE, &test, 0x1000))
    {
      if (!can_use_stolen_ability_with_havengul_lich(0, card))
        use_grave = 1;
      else
      {
        int choice = do_dialog(0, 0, card, -1, -1,
                               " Cast a creature from grave\n Use a stolen ability\n Cancel", 0);
        if (choice == 0)
          use_grave = 1;
        else if (choice != 1)
        {
          spell_fizzled = 1;
          return 0;
        }
      }
    }
  }
  else if (player == 1 && can_cast_creature_in_grave_with_havengul_lich(1, card, 2))
  {
    if (!can_use_stolen_ability_with_havengul_lich(1, card))
      use_grave = 1;
    else
    {
      int choice = do_dialog(1, 1, card, -1, -1,
                             " Cast a creature from grave\n Use a stolen ability\n Cancel", 0);
      if (choice == 0)
        use_grave = 1;
      else if (choice != 1)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    if (use_grave)
      test.special_selection_function = can_cast_this_havengul;
  }
  if (use_grave)
  {
    generic_activated_ability_with_target_in_grave(player, card, EVENT_ACTIVATE, GAA_NONE, 1,
                                                        0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                        &test, 0x1000);
    if (spell_fizzled != 1)
      instance->eot_toughness = 0x42;
    return 0;
  }
  int legacy = find_attached_havengul_legacy(player, card, havengul_lich_legacy, player, card);
  card_instance_t *leg = legacy < 0 ? instance : get_card_instance(player, legacy);
  int act_array[8];
  int aa_count = 0;
  for (int i = 1; i != 19; ++i)
  {
    if (leg->targets[i].card != -1 &&
        cards_data[leg->targets[i].card].code_pointer(player, card, EVENT_CAN_ACTIVATE))
      act_array[aa_count++] = leg->targets[i].card;
    if (leg->targets[i].player != -1 &&
        cards_data[leg->targets[i].player].code_pointer(player, card, EVENT_CAN_ACTIVATE))
      act_array[aa_count++] = leg->targets[i].player;
  }
  test_definition_t this_test;
  new_default_test_definition(&this_test, 2, "Select an ability to activate.");
  int selected =
      select_card_from_zone(player, player, act_array, aa_count, 0, 1, -1, &this_test);
  if (selected == -1)
  {
    spell_fizzled = 1;
    return 0;
  }
  cards_data[act_array[selected]].code_pointer(player, card, EVENT_ACTIVATE);
  if (spell_fizzled != 1)
    instance->eot_toughness = (uint32_t)act_array[selected] | 0x80000000;
  return 0;
}
#endif
