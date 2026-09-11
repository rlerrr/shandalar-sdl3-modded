#include "manalink.h"

// Functions
static void cmc_mod_for_prowl(int player, int card, event_t event, int sub1, int sub2, int colorless, int black, int blue, int green, int red, int white)
{
  if (event == EVENT_MODIFY_COST)
	{
	  card_instance_t* instance = get_card_instance(player, card);
		int c1 = get_updated_casting_cost(player, card, -1, event, colorless);
	  if (has_mana_multi(player, c1, black, blue, green, red, white)
		  && (((sub1 == SUBTYPE_GOBLIN || sub2 == SUBTYPE_GOBLIN) && get_trap_condition(player, TRAP_PROWL_GOBLIN))
			  || ((sub1 == SUBTYPE_ROGUE || sub2 == SUBTYPE_ROGUE) && get_trap_condition(player, TRAP_PROWL_ROGUE))
			  || ((sub1 == SUBTYPE_FAERIE || sub2 == SUBTYPE_FAERIE) && get_trap_condition(player, TRAP_PROWL_FAERIE))))
		{
		  instance->info_slot = 1;
		  null_casting_cost(player, card);
		}
	  else
		instance->info_slot = 0;
	}
}

static void pay_prowl(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	card_instance_t *instance = get_card_instance(player, card);
	if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
		int c1 = get_updated_casting_cost(player, card, -1, event, colorless);
		int choice = 0;
		if( has_mana_multi(player, c1, black, blue, green, red, white) ){
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay Prowl cost\n Play the spell normally\n Cancel", 0);
			}
		}
		else{
			choice = 1;
		}

		if( choice == 0 ){
			charge_mana_multi(player, c1, black, blue, green, red, white);
			if( spell_fizzled != 1 ){
				instance->info_slot |= 2;
			}
		}
		else if( choice == 1 ){
				charge_mana_from_id(player, card, event, get_id(player, card));
		}
		else{
			spell_fizzled = 1;
		}
	}
}

static int prowl(int player, int card, event_t event, int sub1, int sub2, int colorless, int black, int blue, int green, int red, int white){
	cmc_mod_for_prowl(player, card, event, sub1, sub2, colorless, black, blue, green, red, white);
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pay_prowl(player, card, event, colorless, black, blue, green, red, white);
	}
	return 0;
}

static int prowl_paid(int player, int card){
	return get_card_instance(player, card)->info_slot & 2 ? 1 : 0;
}

static int banneret(int player, int card, event_t event, int sub1, int sub2 ){
	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, sub1 ) ||
			has_subtype(affected_card_controller, affected_card, sub2 ) )
		{
			COST_COLORLESS--;
		}
	}
	return 0;
}

static int reinforce(int player, int card, event_t event, int amount_counter, int colorless, int black, int blue, int green, int red, int white){
	if( ! IS_ACTIVATING_FROM_HAND(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE_FROM_HAND  ){
		if( has_mana_multi(player, colorless, black, blue, green, red, white) ){
			return can_target(&td);
		}
	}

	if( event == EVENT_ACTIVATE_FROM_HAND ){
		charge_mana_multi(player, colorless, black, blue, green, red, white);
		if( spell_fizzled != 1 && pick_target(&td, "TARGET_CREATURE") ){
			discard_card(player, card);
			return 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1 ){
		add_1_1_counters(instance->targets[0].player, instance->targets[0].card, amount_counter);
	}
	return 0;
}

static int kinship(int player, int card, event_t event){

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			if( shares_creature_subtype(-1, deck[0], player, card) ){
				if (reveal_card_optional_iid(player, card, deck[0], NULL)){
					return 1;
				}
			}
			else if ( player == HUMAN ){
				reveal_card_iid(player, card, deck[0]);
			}
		}
	}

	return 0;
}

static void auto_attaching_equipment(int player, int card, event_t event, subtype_t subtype){
	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = subtype;
		if( new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			equip_target_creature(player, card, trigger_cause_controller, trigger_cause);
		}
	}
}

static int tribal_spell_played(int player, int card, event_t event, int t_player, int trig_mode, subtype_t subtype){
// AKA "whenever [someone] plays a [subtype] spell..."
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.subtype = subtype;
		if( new_specific_spell_played(player, card, event, t_player, trig_mode, &test) ){
			return 1;
		}
	}
	return 0;
}

static void test_enters_the_battlefield_with_counters(int player, int card, event_t event, int t_player, test_definition_t *test,
	counter_t type, int number)
{
	if( affected_card_controller == t_player ){
		if( new_make_test_in_play(affected_card_controller, affected_card, -1, test) ){
			enters_the_battlefield_with_counters(affected_card_controller, affected_card, event, type, number);
		}
	}
}

// Cards
int card_ambassador_oak(int player, int card, event_t event){
	/* Ambassador Oak	|3|G
	 * Creature - Treefolk Warrior 3/3
	 * When ~ enters the battlefield, put a 1/1 |Sgreen Elf Warrior creature token onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_ELF_WARRIOR);
	}

	return 0;
}

int card_aunties_snitch(int player, int card, event_t event){
/* CARD_ID_AUNTIES_SNITCH	9322
Auntie's Snitch	|2|B
Creature - Goblin Rogue 3/1
~ can't block.
Prowl |1|B (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Goblin or Rogue.)
Whenever a Goblin or Rogue you control deals combat damage to a player, if ~ is in your graveyard, you may return ~ to your hand. */

    cannot_block(player, card, event);
	return prowl(player, card, event, SUBTYPE_GOBLIN, SUBTYPE_ROGUE, MANACOST_XB(1, 1));
}

int card_bullyrush_banneret(int player, int card, event_t event){
/* CARD_ID_BALLYRUSH_BANNERET	9323
Ballyrush Banneret	|1|W
Creature - Kithkin Soldier 2/1
Kithkin spells and Soldier spells you cast cost |1 less to cast. */

	return banneret(player, card, event, SUBTYPE_SOLDIER, SUBTYPE_KITHKIN);
}

int card_battletide_alchemist(int player, int card, event_t event){
/*
Battletide Alchemist |3|W|W
Creature — Kithkin Cleric 3/4
If a source would deal damage to a player, you may prevent X of that damage, where X is the number of Clerics you control.
*/

	if( in_play(player, card) && ! is_humiliated(player, card) && event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && !damage_is_to_planeswalker(damage) && damage->info_slot > 0 ){
				int choice = 0;
				int amount = count_subtype(player, TYPE_PERMANENT, SUBTYPE_CLERIC);
				if( damage->damage_target_player != player ){
					choice = 1;
				}
				if( ! duh_mode(player) ){
					char buffer[100];
					scnprintf(buffer, 500, " Prevent %d damages\n Pass", amount);
					choice = do_dialog(player, player, card, -1, -1, buffer, 0);
				}
				if( choice == 0 ){
					prevent_damage(affected_card_controller, affected_card, amount);
				}
			}
		}
	}
	return 0;
}

int card_bitterblossom(int player, int card, event_t event){
	/* Bitterblossom	|1|B
	 * Tribal Enchantment - Faerie
	 * At the beginning of your upkeep, you lose 1 life and put a 1/1 |Sblack Faerie Rogue creature token with flying onto the battlefield. */

	//  original code : 004DEA12

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_FAERIE_ROGUE);
		lose_life(player, 1);
	}

	return global_enchantment(player, card, event);
}


int card_blightsoil_druid(int player, int card, event_t event){
/* CARD_ID_BLIGHTSOIL_DRUID	9325
Blightsoil Druid	|1|B
Creature - Elf Druid 1/2
|T, Pay 1 life: Add |G to your mana pool. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE && can_pay_life(player, 1) ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		lose_life(player, 1);
		return mana_producer(player, card, event);
	}

	return 0;
}

int card_boldwyr_heavyweights(int player, int card, event_t event){
/* CARD_ID_BOLDWYR_HEAVYWEIGHTS	9326
Boldwyr Heavyweights	|2|R|R
Creature - Giant Warrior 8/8
Trample
When ~ enters the battlefield, each opponent may search his or her library for a creature card and put it onto the battlefield.
Then each player who searched his or her library this way shuffles it. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( do_dialog(1-player, player, card, -1, -1, " Tutor a creature\n Pass", duh_mode(player)) == 0 ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			new_global_tutor(1-player, 1-player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_borderland_behemoth(int player, int card, event_t event){
/* CARD_ID_BORDERLAND_BEHEMOTH	9327
Borderland Behemoth	|5|R|R
Creature - Giant Warrior 4/4
Trample
~ gets +4/+4 for each other Giant you control. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			test.subtype = SUBTYPE_GIANT;
			test.not_me = 1;
			event_result += (4*check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test));
		}
	}

	return 0;
}

int card_bosk_banneret(int player, int card, event_t event){
/* CARD_ID_BOSK_BANNERET	9328
Bosk Banneret	|1|G
Creature - Treefolk Shaman 1/3
Treefolk spells and Shaman spells you cast cost |1 less to cast. */

	return banneret(player, card, event, SUBTYPE_TREEFOLK, SUBTYPE_SHAMAN);
}

int card_bramblewood_paragon(int player, int card, event_t event){
/* CARD_ID_BRAMBLEWOOD_PARAGON	9329
Bramblewood Paragon	|1|G
Creature - Elf Warrior 2/2
Each other Warrior creature you control enters the battlefield with an additional +1/+1 counter on it.
Each creature you control with a +1/+1 counter on it has trample. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( event == EVENT_ABILITIES && player == affected_card_controller){
			if( count_1_1_counters(affected_card_controller, affected_card) > 0 ){
				event_result |= KEYWORD_TRAMPLE;
			}
		}
		if( event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.subtype = SUBTYPE_WARRIOR;
			test.not_me = 1;
			test_enters_the_battlefield_with_counters(player, card, event, player, &test, COUNTER_P1_P1, 1);
		}
	}

	return 0;
}

int card_brightheart_banneret(int player, int card, event_t event){
/* CARD_ID_BRIGHTHEARTH_BANNERET	9330
Brighthearth Banneret	|1|R
Creature - Elemental Warrior 1/1
Elemental spells and Warrior spells you cast cost |1 less to cast.
Reinforce 1-|1|R (|1|R, Discard this card: Put a +1/+1 counter on target creature.) */

	banneret(player, card, event, SUBTYPE_ELEMENTAL, SUBTYPE_WARRIOR);
	return reinforce(player, card, event, 1, MANACOST_XR(1, 1));
}

int card_chameleon_colossus(int player, int card, event_t event){

  /* Chameleon Colossus	|2|G|G
   * Creature - Shapeshifter 4/4
   * Changeling
   * Protection from |Sblack
   * |2|G|G: ~ gets +X/+X until end of turn, where X is its power. */

  changeling_switcher(player, card, event);

  return card_yew_spirit(player, card, event); // Avacyn Restored
}

int card_cloak_and_dagger(int player, int card, event_t event){
/* CARD_ID_CLOAK_AND_DAGGER	9336
Cloak and Dagger	|2
Tribal Artifact - Rogue Equipment
Equipped creature gets +2/+0 and has shroud. (It can't be the target of spells or abilities.)
Whenever a Rogue creature enters the battlefield, you may attach ~ to it.
Equip |3 */

	auto_attaching_equipment(player, card, event, SUBTYPE_ROGUE);

	return vanilla_equipment(player, card, event, 3, 2, 0, KEYWORD_SHROUD, 0);
}

int card_countryside_crusher(int player, int card, event_t event)
{
  /* Countryside Crusher	|1|R|R
   * Creature - Giant Warrior 3/3
   * At the beginning of your upkeep, reveal the top card of your library.
   * If it's a land card, put it into your graveyard and repeat this process.
   * Whenever a land card is put into your graveyard from anywhere, put a +1/+1 counter on ~. */

	upkeep_trigger_ability(player, card, event, player);

	if ( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		while( deck_ptr[player][0] != -1 ){
				reveal_card_iid(player, card, deck_ptr[player][0]);
				if( is_what(-1, deck_ptr[player][0], TYPE_LAND) ){
					mill(player, 1);
				}
				else{
					break;
				}
		}
	}

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		int result = whenever_type_is_put_into_graveyard_from_anywhere(player, card, event, player, TYPE_LAND, NULL);
		if( result ){
			add_1_1_counters(player, card, result);
		}
	}

	return 0;
}

int card_cream_of_the_crop(int player, int card, event_t event){
/* CARD_ID_CREAM_OF_THE_CROP	9339
Cream of the Crop	|1|G
Enchantment
Whenever a creature enters the battlefield under your control, you may look at the top X cards of your library, where X is that creature's power.
If you do, put one of those cards on top of your library and the rest on the bottom of your library in any order. */

 	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		impulse_effect(player, get_power(trigger_cause_controller, trigger_cause), 1);
	}

	return global_enchantment(player, card, event);
}

int card_declaration_of_naught(int player, int card, event_t event){
/*
Declaration of Naught |U|U
Enchantment
As Declaration of Naught enters the battlefield, name a card.
{U}: Counter target spell with the chosen name.
*/
	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		int iid = -1;
		if( IS_AI(player) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			iid = new_card_from_list(player, 5, &this_test);
		}
		else{
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;
			iid = get_internal_card_id_from_csv_id(ai_name_a_random_csvid(player, card, 1-player, 0, &this_test));
		}
		create_card_name_legacy(player, card, cards_data[iid].id);
		instance->targets[1].card = iid;
	}


	if( IS_GAA_EVENT(event) ){
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td1;
		counterspell_target_definition(player, card, &td1, TYPE_ANY);
		td1.extra = instance->targets[1].card;

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( counterspell_validate(player, card, &td1, 0) ){
				real_counter_a_spell(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
			}
		}

		return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_U(1), 0, &td1, NULL);
	}

	return global_enchantment(player, card, event);
}

int card_deglamer(int player, int card, event_t event){
/* CARD_ID_DEGLAMER	9342
Deglamer	|1|G
Instant
Choose target artifact or enchantment. Its owner shuffles it into his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			shuffle_into_library(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_disperse(int player, int card, event_t event){
/* CARD_ID_DISPERSE	9344
Disperse	|1|U
Instant
Return target nonland permanent to its owner's hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent.", 1, NULL);
}

int card_distant_melody(int player, int card, event_t event){
/* CARD_ID_DISTANT_MELODY	9345
Distant Melody	|3|U
Sorcery
Choose a creature type. Draw a card for each permanent you control of that type. */

	if( event == EVENT_RESOLVE_SPELL){
		int selected = select_a_subtype(player, card);
		draw_cards(player, count_subtype(player, TYPE_PERMANENT, selected));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_diviners_wand(int player, int card, event_t event){
/* Diviner's Wand	|3
 * Tribal Artifact - Wizard Equipment
 * Equipped creature has "Whenever you draw a card, this creature gets +1/+1 and gains flying until end of turn" and "|4: Draw a card."
 * Whenever a Wizard creature enters the battlefield, you may attach ~ to it.
 * Equip |3 */

	auto_attaching_equipment(player, card, event, SUBTYPE_WIZARD);

	if( is_equipping(player, card) && card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(player, card, instance->damage_target_player, instance->damage_target_card, 1, 1, KEYWORD_FLYING, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( granted_generic_activated_ability(player, card, p, c, event, 0, MANACOST_X(4), 0, NULL, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 3);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, can_activate_basic_equipment(player, card, event, 3), 0};
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, 0, MANACOST_X(4), 0, NULL, NULL) ){
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", 		abils[1], 1,
							"Draw a card", 	abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( choice == CHOICE_DRAW ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			return granted_generic_activated_ability(player, card, p, c, event, 0, MANACOST_X(4), 0, NULL, NULL);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_door_of_destinies(int player, int card, event_t event){

	/* Door of Destinies	|4
	 * Artifact
	 * As ~ enters the battlefield, choose a creature type.
	 * Whenever you cast a spell of the chosen type, put a charge counter on ~.
	 * Creatures you control of the chosen type get +1/+1 for each charge counter on ~. */

	if( event == EVENT_RESOLVE_SPELL){
		get_card_instance(player, card)->info_slot = select_a_subtype(player, card);
	}

	if( tribal_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, get_card_instance(player, card)->info_slot) ){
		add_counter(player, card, COUNTER_CHARGE);
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player ){
		if( ! is_humiliated(player, card) &&
			has_subtype(affected_card_controller, affected_card, get_card_instance(player, card)->info_slot) )
		{
			event_result += count_counters(player, card, COUNTER_CHARGE);
		}
	}

	return 0;
}

int card_earwig_squad(int player, int card, event_t event){
/* CARD_ID_EARWIG_SQUAD	9349
Earwig Squad	|3|B|B
Creature - Goblin Rogue 5/3
Prowl |2|B (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Goblin or Rogue.)
When ~ enters the battlefield, if its prowl cost was paid, search target opponent's library for three cards and exile them.
Then that player shuffles his or her library. */

	if( prowl_paid(player, card) ){
		etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	}

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ANY);
		this_test.qty = 3;
		new_global_tutor(player, 1-player, TUTOR_FROM_DECK, TUTOR_RFG, 1, AI_MAX_VALUE, &this_test);
	}

	return prowl(player, card, event, SUBTYPE_GOBLIN, SUBTYPE_ROGUE, MANACOST_XB(2, 1));
}

int card_fendeep_summoner(int player, int card, event_t event){
/*
Fendeep Summoner |4|B	0x200f52f
Creature — Treefolk Shaman 3/5
{T}: Up to two target Swamps each become 3/5 Treefolk Warrior creatures in addition to their other types until end of turn.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.allow_cancel = 2; //"Done" button

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int i;
			for(i=0; i<2; i++){
				if( ! new_pick_target(&td, get_hacked_land_text(player, card, "Select target %s.", SUBTYPE_SWAMP), 0, GS_LITERAL_PROMPT) ){
					break;
				}
			}
			add_state(player, card, STATE_TAPPED);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				add_a_subtype(instance->targets[i].player, instance->targets[i].card, SUBTYPE_TREEFOLK);
				land_animation2(instance->parent_controller, instance->parent_card, instance->targets[i].player, instance->targets[i].card,
								1, 3, 5, 0, 0, 0, 0);
			}
		}
	}

	return 0;
}

int card_fertilid(int player, int card, event_t event){
/* CARD_ID_FERTILID	9353
Fertilid	|2|G
Creature - Elemental 0/0
~ enters the battlefield with two +1/+1 counters on it.
|1|G, Remove a +1/+1 counter from ~: Target player searches his or her library for a basic land card and puts it onto the battlefield tapped.
Then that player shuffles his or her library. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
			this_test.subtype = SUBTYPE_BASIC;
			new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY_TAPPED, 0,
							AI_FIRST_FOUND, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(1, 1), GVC_COUNTER(COUNTER_P1_P1), &td, "TARGET_PLAYER");
}

int card_festercreep(int player, int card, event_t event){
/* CARD_ID_FESTERCREEP	9354
Festercreep	|1|B
Creature - Elemental 0/0
~ enters the battlefield with a +1/+1 counter on it.
|1|B, Remove a +1/+1 counter from ~: All other creatures get -1/-1 until end of turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 1);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, ANYBODY, -1, -1, -1, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_1_1_COUNTER, MANACOST_XB(1, 1), 0, NULL, NULL);
}

// giant warrior token --> rhino token

int card_feudkiller_verdict(int player, int card, event_t event){
	/* Feudkiller's Verdict	|4|W|W
	 * Tribal Sorcery - Giant
	 * You gain 10 life. Then if you have more life than an opponent, put a 5/5 |Swhite Giant Warrior creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_SPELL){
		gain_life(player, 10);
		if( life[player] > life[1-player] ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_GIANT_WARRIOR, &token);
			token.pow = token.tou = 5;
			token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

static int forfend_effect(int player, int card, event_t event){

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *source = get_card_instance(affected_card_controller, affected_card);
		if( damage_card == source->internal_card_id && source->info_slot > 0 ){
			if( is_what(source->damage_target_player, source->damage_target_card, TYPE_CREATURE) ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	if( event == EVENT_ATTACK_RATING ){
		ai_defensive_modifier -= 24;
	}

	 if( event == EVENT_BLOCK_RATING ){
		ai_defensive_modifier += 24;
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_forfend(int player, int card, event_t event){
/*
Forfend |1|W
Instant
Prevent all damage that would be dealt to creatures this turn.
*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 8);
	}

	if( event == EVENT_RESOLVE_SPELL){
		create_legacy_effect(player, card, &forfend_effect);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_frogtosser_banneret(int player, int card, event_t event){
/* CARD_ID_FROGTOSSER_BANNERET	9360
Frogtosser Banneret	|1|B
Creature - Goblin Rogue 1/1
Haste
Goblin spells and Rogue spells you cast cost |1 less to cast. */

	haste(player, card, event);
	return banneret(player, card, event, SUBTYPE_GOBLIN, SUBTYPE_ROGUE);
}

// game trail changeling --> vanilla

int card_gilt_leaf_archdruid(int player, int card, event_t event){
/* CARD_ID_GILT_LEAF_ARCHDRUID	9362
Gilt-Leaf Archdruid	|3|G|G
Creature - Elf Druid 3/3
Whenever you cast a Druid spell, you may draw a card.
Tap seven untapped Druids you control: Gain control of all lands target player controls. */

	if( tribal_spell_played(player, card, event, player, RESOLVE_TRIGGER_CHECK_DECK_COUNT(player, 1), SUBTYPE_DRUID) ){
		draw_cards(player, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allow_cancel = 0;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_DRUID;
	td1.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return target_available(player, card, &td1) > 6;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) && tapsubtype_ability(player, card, 7, &td1) ){
			pick_target(&td, "TARGET_PLAYER");
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			new_manipulate_all(player, card, instance->targets[0].player, &this_test, ACT_GAIN_CONTROL);
		}
	}

	return 0;
}

static int graceful_reprieve_legacy(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			if( affect_me(p, c) && is_this_dying(p, c) ){
				instance->damage_target_player = instance->damage_target_card = -1;
				instance->targets[1].player = get_owner(p, c);
				instance->targets[1].card = get_original_internal_card_id(p, c);
				instance->targets[11].player = 66;
			}
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance( player, card );
		int owner = instance->targets[1].player;
		int csvid = cards_data[instance->targets[1].card].id;
		seek_grave_for_id_to_reanimate(owner, -1, owner, csvid, REANIMATE_DEFAULT);
		kill_card(player, card, KILL_EXILE);
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_graceful_reprieve(int player, int card, event_t event){
/* CARD_ID_GRACEFUL_REPRIEVE	9363
Graceful Reprieve	|1|W
Instant
When target creature dies this turn, return that card to the battlefield under its owner's control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, NULL, 1, NULL);
		}
		else{
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled != 1 ){
				if( is_token(instance->targets[0].player, instance->targets[0].card) ){
					ai_modifier -= 48;
				}
				else{
					ai_modifier += (get_owner(instance->targets[0].player, instance->targets[0].card) == player ? 48 : -48);
				}
			}
		}
		else{
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(player, card, &graceful_reprieve_legacy, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_greatbow_doyen(int player, int card, event_t event){
/*
Greatbow Doyen |4|G
Creature — Elf Archer 2/4
Other Archer creatures you control get +1/+1.
Whenever an Archer you control deals damage to a creature, that Archer deals that much damage to that creature's controller.
*/

	boost_creature_type(player, card, event, SUBTYPE_ARCHER, 1, 1, 0, BCT_CONTROLLER_ONLY);

	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card != -1 && damage->damage_source_player == player ){
				if( has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_ARCHER) ){
					if( is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) ){
						int pos = instance->info_slot;
						if( pos < 10 ){
							instance->targets[pos].player = instance->targets[pos].card = 0;
							SET_BYTE0(instance->targets[pos].player) = damage->damage_source_player;
							SET_BYTE1(instance->targets[pos].player) = damage->damage_target_player;
							SET_BYTE0(instance->targets[pos].card) = damage->damage_source_card;
							SET_BYTE1(instance->targets[pos].card) = damage->info_slot;
							instance->info_slot++;
						}
					}
				}
			}
		}
	}

	if( get_card_instance( player, card )->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				card_instance_t *instance = get_card_instance( player, card );
				int i;
				for(i=0; i<instance->info_slot; i++){
					int archer_player = BYTE0(instance->targets[i].player);
					int archer_card = BYTE0(instance->targets[i].card);
					int damaged_player = BYTE1(instance->targets[i].player);
					int damages = BYTE1(instance->targets[i].card);
					instance->targets[i].player = instance->targets[i].card = 0;
					damage_player(damaged_player, damages, archer_player, archer_card);
				}
				instance->info_slot = 0;
		}
	}

	return 0;
}

int card_grimoire_thief(int player, int card, event_t event){
	/*
	  Grimoire Thief |U|U
	  Creature - Merfolk Rogue 2/2
	  Whenever Grimoire Thief becomes tapped, exile the top three cards of target opponent's library face down.
	  You may look at cards exiled with Grimoire Thief.
	  {U}, Sacrifice Grimoire Thief: Turn all cards exiled with Grimoire Thief face up. Counter all spells with those names.
	*/
	if (event == EVENT_TAP_CARD && affect_me(player, card) ){
		target_definition_t td_player;
		default_target_definition(player, card, &td_player, 0);
		td_player.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td_player, 1-player, -1) ){
			int i, leg = 0, idx = 0;
			for (i = 0; i < 3; ++i){
				if (deck_ptr[1-player][0] != -1){
					exiledby_remember(player, card, 1-player, deck_ptr[1-player][0], &leg, &idx);
					rfg_top_card_of_deck(1-player);
				}
			}
		}
	}

#define ABILITY()	generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK|GAA_SACRIFICE_ME, MANACOST_U(1), 0, NULL, NULL)
#define FIND_IN_RFG(instance, spell)	exiledby_choose(player, card, cards_data[spell->internal_card_id].id, EXBY_FIRST_FOUND|EXBY_TEST_IID, instance->internal_card_id, NULL, 1)

	if (event == EVENT_CAN_ACTIVATE ){
		if( ABILITY()){
			card_instance_t* instance = get_card_instance(player, card);
			card_instance_t* spell = get_card_instance(card_on_stack_controller, card_on_stack);
			if (FIND_IN_RFG(instance, spell)){
				return 99;
			}
		}
		else if( player == HUMAN && exiledby_count(player, card, 1-player) ){
				return 1;
		}
	}

	if (event == EVENT_ACTIVATE){
		enum{
			SHOW_CARDS = 1,
			REAL_ABILITY = 2,
		};
		card_instance_t* instance = get_card_instance(player, card);
		int choice = SHOW_CARDS;
		int can_counter = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK|GAA_SACRIFICE_ME, MANACOST_U(1), 0, NULL, NULL) ){
			card_instance_t* spell = get_card_instance(card_on_stack_controller, card_on_stack);
			if (FIND_IN_RFG(instance, spell)){
				choice = REAL_ABILITY;
				can_counter = 1;
			}
		}
		if( player != AI && ai_is_speculating != 1 ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Show exiled", 1, 1,
							"Sacrifice and counter.", can_counter, 2);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == SHOW_CARDS ){
			exiledby_choose(player, card, CARD_ID_GRIMOIRE_THIEF, EXBY_CHOOSE, 0, "any", 1);
			cancel = 1;
		}
		if( choice == REAL_ABILITY ){
			ABILITY();
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		if (counterspell_validate(player, card, NULL, 0)){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;

#undef FIND_IN_RFG
#undef ABILITY
}

int card_heritage_druid(int player, int card, event_t event){
/* CARD_ID_HERITAGE_DRUID	9366
Heritage Druid	|G
Creature - Elf Druid 1/1
Tap three untapped Elves you control: Add |G|G|G to your mana pool. */

	// See also card_birchlore_rangers() in onslaught.c.

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.required_subtype = SUBTYPE_ELF;
		td.illegal_state = TARGET_STATE_TAPPED;
		int count = target_available(player, card, &td);
		if (count > 2){
			declare_mana_available(player, COLOR_GREEN, 3 * (count / 3));
		}
	}

	if (event == EVENT_ATTACK_RATING && affect_me(player, card)){
		ai_defensive_modifier += 48;
	}
	if (event == EVENT_BLOCK_RATING && affect_me(player, card)){
		ai_defensive_modifier -= 48;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.required_subtype = SUBTYPE_ELF;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && target_available(player, card, &td) > 2 ){
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		if( tapsubtype_ability(player, card, 3, &td) ){
			instance->number_of_targets = 3;
			// See comments in tap_a_permanent_you_control_for_mana(), which ideally should be generalized to handle this card.
			produce_mana(player, COLOR_GREEN, 3);
			tapped_for_mana_color = -2;
			/* EVENT_TAP_CARD already sent for all three elves.  It'll get sent twice for this if this happens to be one of the ones chosen; but it's not worth
			 * the effort to fix. */
		}
		else{
			spell_fizzled = 1;
		}
	}


	return 0;
}

int card_hunting_triad(int player, int card, event_t event){
	/* Hunting Triad	|3|G
	 * Tribal Sorcery - Elf
	 * Put three 1/1 |Sgreen Elf Warrior creature tokens onto the battlefield.
	 * Reinforce 3-|3|G */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_ELF_WARRIOR, 3);
		kill_card(player, card, KILL_DESTROY);
	}

	return reinforce(player, card, event, 3, MANACOST_XG(3, 1));
}

int card_idyllic_tutor(int player, int card, event_t event){
/* CARD_ID_IDYLLIC_TUTOR	1517
Idyllic Tutor	|2|W
Sorcery
Search your library for an enchantment card, reveal it, and put it into your hand. Then shuffle your library. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ink_dissolver(int player, int card, event_t event){
/* CARD_ID_INK_DISSOLVER	9369
Ink Dissolver	|1|U
Creature - Merfolk Wizard 2/1
Kinship - At the beginning of your upkeep, you may look at the top card of your library.
If it shares a creature type with ~, you may reveal it. If you do, each opponent puts the top three cards of his or her library into his or her graveyard. */

	if( kinship(player, card, event) ){
		mill(1-player, 3);
	}
	return 0;
}

int card_inspired_sprite(int player, int card, event_t event){
/* CARD_ID_INSPIRED_SPRITE	9370
Inspired Sprite	|3|U
Creature - Faerie Wizard 2/2
Flash
Flying
Whenever you cast a Wizard spell, you may untap ~.
|T: Draw a card, then discard a card. */

	if( tribal_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), SUBTYPE_WIZARD) ){
		untap_card(player, card);
	}

	if( IS_GAA_EVENT(event) ){
		if( event == EVENT_RESOLVE_ACTIVATION ){
			draw_cards(player, 1);
			discard(player, 0, player);
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	return flash(player, card, event);
}

int card_kinsbaile_borderguard(int player, int card, event_t event){
	/* Kinsbaile Borderguard	|1|W|W
	 * Creature - Kithkin Soldier 1/1
	 * ~ enters the battlefield with a +1/+1 counter on it for each other Kithkin you control.
	 * When ~ dies, put a 1/1 |Swhite Kithkin Soldier creature token onto the battlefield for each counter on it. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, count_subtype(player, TYPE_PERMANENT, SUBTYPE_KITHKIN));

	if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) ){
		if( is_this_dying(player, card) ){
			get_card_instance(player, card)->targets[1].player = count_1_1_counters(player, card);
		}
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int amount = MAX(0, get_card_instance(player, card)->targets[1].player);
		generate_tokens_by_id(player, card, CARD_ID_KITHKIN_SOLDIER, amount);
	}

	return 0;
}

int card_kinsbaile_cavalier(int player, int card, event_t event){

	/* Kinsbaile Cavalier	|3|W
	 * Creature - Kithkin Knight 2/2
	 * Knight creatures you control have double strike. */

	boost_subtype(player, card, event, SUBTYPE_KNIGHT, 0,0, KEYWORD_DOUBLE_STRIKE,0, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	return 0;
}

int card_kithkin_zephyrnaut(int player, int card, event_t event){
/* CARD_ID_KITHKIN_ZEPHYRNAUT	9374
Kithkin Zephyrnaut	|2|W
Creature - Kithkin Soldier 2/2
Kinship - At the beginning of your upkeep, you may look at the top card of your library.
If it shares a creature type with ~, you may reveal it. If you do, ~ gets +2/+2 and gains flying and vigilance until end of turn. */

	if (kinship(player, card, event)){
		pump_ability_until_eot(player, card, player, card, 2, 2, KEYWORD_FLYING, SP_KEYWORD_VIGILANCE);
	}
	return 0;
}

int card_knowledge_exploitation(int player, int card, event_t event){
/* Knowledge Exploitation	|5|U|U
 * Tribal Sorcery - Rogue
 * Prowl |3|U
 * Search target opponent's library for an instant or sorcery card. You may cast that card without paying its mana cost.
 * Then that player shuffles his or her library. */

	cmc_mod_for_prowl(player, card, event, SUBTYPE_ROGUE, -1, MANACOST_XU(3, 1));

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->number_of_targets = 0;
		pay_prowl(player, card, event, MANACOST_XU(3, 1));
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_SPELL, "Select an instant or sorcery card to play for free.");
			int selected = new_select_a_card(player, 1-player, TUTOR_FROM_DECK, 0, AI_MAX_CMC, -1, &this_test);
			if( selected != -1 && can_legally_play_iid(player, deck_ptr[1-player][selected]) ){
				cant_be_responded_to = 1;	// before playing the card - once this effect starts resolving, it's too late to respod to whatever card gets chosen
				play_card_in_deck_for_free(player, 1-player, selected);
			}
			shuffle(1-player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_latchkey_faerie(int player, int card, event_t event){
/* CARD_ID_LATCHKEY_FAERIE	9376
Latchkey Faerie	|3|U
Creature - Faerie Rogue 3/1
Flying
Prowl |2|U (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Faerie or Rogue.)
When ~ enters the battlefield, if its prowl cost was paid, draw a card. */

	if( prowl_paid(player, card) ){
		etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	}

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return prowl(player, card, event, SUBTYPE_FAERIE, SUBTYPE_ROGUE, MANACOST_XU(2, 1));
}


int card_leafcrown_elder(int player, int card, event_t event){
/* CARD_ID_LEAF_CROWNED_ELDER	9377
Leaf-Crowned Elder	|2|G|G
Creature - Treefolk Shaman 3/5
Kinship - At the beginning of your upkeep, you may look at the top card of your library.
If it shares a creature type with ~, you may reveal it. If you do, you may play that card without paying its mana cost. */

	if (kinship(player, card, event)){
		int *deck = deck_ptr[player];
		card_ptr_t* c = cards_ptr[ cards_data[deck[0]].id ];
		char buffer[100];
		snprintf(buffer, 100, " Play %s\n Pass", c->name );
		int choice = do_dialog(player, player, card, -1, -1, buffer, 0);
		if( choice == 0 ){
			play_card_in_deck_for_free(player, player, 0);
		}
	}

	return 0;
}

int card_lightning_crafter(int player, int card, event_t event){
	/* Lightning Crafter	|3|R	0x2007b4a
	 * Creature - Goblin Shaman 3/3
	 * Champion a Goblin or Shaman
	 * |T: ~ deals 3 damage to any target. */
	champion(player, card, event, SUBTYPE_GOBLIN, SUBTYPE_SHAMAN);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_lys_alana_bowmaster(int player, int card, event_t event){
/* CARD_ID_LYS_ALANA_BOWMASTER	9381
Lys Alana Bowmaster	|2|G
Creature - Elf Archer 2/2
Reach (This creature can block creatures with flying.)
Whenever you cast an Elf spell, you may have ~ deal 2 damage to target creature with flying. */

	if( tribal_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), SUBTYPE_ELF) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "Select target creature with flying.", 0, GS_LITERAL_PROMPT) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_maralen_of_the_mornsong(int player, int card, event_t event){
/* Maralen of the Mornsong	|1|B|B
 * Legendary Creature - Elf Wizard 2/3
 * Players can't draw cards.
 * At the beginning of each player's draw step, that player loses 3 life, searches his or her library for a card, puts it into his or her
 * 		hand, then shuffles his or her library. */

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && event == EVENT_TRIGGER ){
			suppress_draw = 1;	// Deliberately do this during EVENT_TRIGGER, not EVENT_RESOLVE_TRIGGER, so nothing else can replace the (nonexistent) draw
		}

		if( event == EVENT_DRAW_PHASE ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			new_global_tutor(current_turn, current_turn, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
			lose_life(current_turn, 3);
		}
	}

	return 0;
}

static int meadowboon_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_MEADOWBOON ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			instance->damage_target_player = instance->damage_target_card = -1;
			target_definition_t td;
			default_target_definition(player, card, &td, 0);
			td.zone = TARGET_ZONE_PLAYERS;
			td.preferred_controller = player;
			td.allow_cancel = 0;

			if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
				manipulate_type(player, card, instance->targets[1].player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_meadowboon(int player, int card, event_t event)
{
  /* Meadowboon	|2|W|W
   * Creature - Elemental 3/3
   * When ~ leaves the battlefield, put a +1/+1 counter on each creature target player controls.
   * Evoke |3|W */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)meadowboon_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &meadowboon_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( comes_into_play(player, card, event) && evoked(player, card) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XW(3,1));
}

int card_merrow_witsniper(int player, int card, event_t event){
/* CARD_ID_MERROW_WITSNIPER	9384
Merrow Witsniper	|U
Creature - Merfolk Rogue 1/1
When ~ enters the battlefield, target player puts the top card of his or her library into his or her graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			mill(instance->targets[0].player, 1);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// mind shatter --> mind twist

int card_moonglove_changeling(int player, int card, event_t event){
/* CARD_ID_MOONGLOVE_CHANGELING	9387
Moonglove Changeling	|2|B
Creature - Shapeshifter 2/2
Changeling (This card is every creature type.)
|B: ~ gains deathtouch until end of turn. (Any amount of damage it deals to a creature is enough to destroy that creature.) */

	return generic_shade(player, card, event, 0, MANACOST_B(1), 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_morsel_theft(int player, int card, event_t event){
/* CARD_ID_MORSEL_THEFT	9388
Morsel Theft	|2|B|B
Tribal Sorcery - Rogue
Prowl |1|B (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Rogue.)
Target player loses 3 life and you gain 3 life. If ~'s prowl cost was paid, draw a card. */

	cmc_mod_for_prowl(player, card, event, SUBTYPE_ROGUE, -1, MANACOST_XB(1, 1));

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		pay_prowl(player, card, event, MANACOST_XB(1, 1));
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 3);
			gain_life(player, 3);
			if( prowl_paid(player, card) ){
				draw_cards(player, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mosquito_guard(int player, int card, event_t event){
/* CARD_ID_MOSQUITO_GUARD	9389
Mosquito Guard	|W
Creature - Kithkin Soldier 1/1
First strike
Reinforce 1-|1|W (|1|W, Discard this card: Put a +1/+1 counter on target creature.) */

	return reinforce(player, card, event, 1, MANACOST_XW(1, 1));
}

int card_mothdust_changeling(int player, int card, event_t event){
/* CARD_ID_MOTHDUST_CHANGELING	9390
Mothdust Changeling	|U
Creature - Shapeshifter 1/1
Changeling (This card is every creature type.)
Tap an untapped creature you control: ~ gains flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);


	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card,
								instance->parent_controller, instance->parent_card, 0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td, 1);
}

int card_murmuring_bosk(int player, int card, event_t event){
/* CARD_ID_MURMURING_BOSK	9392
Murmuring Bosk	""
Land - |H2Forest
(|T: Add |H|G to your mana pool.)
As ~ enters the battlefield, you may reveal a Treefolk card from your hand. If you don't, ~ enters the battlefield tapped.
|T: Add |W or |B to your mana pool. ~ deals 1 damage to you. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && !is_tapped(player, card) ){	// e.g. from Kismet
		int tapme = 1;
		if( is_subtype_in_hand(player, SUBTYPE_TREEFOLK) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select Treefolk card to reveal.");
			this_test.subtype = SUBTYPE_TREEFOLK;
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if (selected != -1){
				reveal_card(player, card, player, selected);
				tapme = 0;
			}
		}
		if( tapme ){
			add_state(player, card, STATE_TAPPED);
		}
	}

	if( event != EVENT_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		int black_mana_before = mana_pool[player][COLOR_BLACK];
		int white_mana_before = mana_pool[player][COLOR_WHITE];
		mana_producer(player, card, event);
		if( black_mana_before < mana_pool[player][COLOR_BLACK] || white_mana_before < mana_pool[player][COLOR_WHITE]){
			damage_player(player, 1, player, card);
		}
	}

	return 0;
}

int card_mutavault(int player, int card, event_t event){
/* CARD_ID_MUTAVAULT	9393
Mutavault	""
Land
|T: Add |C to your mana pool.
|1: ~ becomes a 2/2 creature with all creature types until end of turn. It's still a land. */

	return manland_normal(player, card, event, MANACOST_X(1));
}

int card_mutavault_animated(int player, int card, event_t event){
/* CARD_ID_MUTAVAULT_ANIMATED	9394
Mutavault Animated	""
Land Creature 2/2
~ has all creature types. It's still a land.
|T: Add |C to your mana pool. */

	changeling_switcher(player, card, event);
	return manland_animated(player, card, event, MANACOST_X(1));
}

int card_negate(int player, int card, event_t event){
/* CARD_ID_NEGATE	9395
Negate	|1|U
Instant
Counter target noncreature spell. */

	// Used externally

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_CREATURE;

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, &td, 0);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}


static int nevermaker_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_NEVERMAKER ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);
			td.illegal_type = TYPE_LAND;
			td.illegal_abilities = get_protections_from(instance->targets[0].player, instance->targets[0].card);
			td.allow_cancel = 0;

			if( can_target(&td) && new_pick_target(&td, "Select target nonland permanent.", 0, GS_LITERAL_PROMPT) ){
				put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_nevermaker(int player, int card, event_t event){
/* CARD_ID_NEVERMAKER	9396
Nevermaker	|3|U
Creature - Elemental 2/3
Flying
When ~ leaves the battlefield, put target nonland permanent on top of its owner's library.
Evoke |3|U (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)nevermaker_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &nevermaker_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XU(3, 1));
}

int card_noggin_whack(int player, int card, event_t event){

	/* Noggin Whack	|2|B|B
	 * Tribal Sorcery - Rogue
	 * Prowl |1|B
	 * Target player reveals three cards from his or her hand. You choose two of them. That player discards those cards. */

	cmc_mod_for_prowl(player, card, event, SUBTYPE_ROGUE, -1, MANACOST_XB(1, 1));

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		pay_prowl(player, card, event, MANACOST_XB(1, 1));
		if( spell_fizzled != 1 ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, 0);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			this_definition.cards_to_reveal = 3;
			this_definition.qty = 2;
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}


int card_notorious_throng(int player, int card, event_t event){
	/* Notorious Throng	|3|U
	 * Tribal Sorcery - Rogue
	 * Prowl |5|U
	 * Put X 1/1 |Sblack Faerie Rogue creature tokens with flying onto the battlefield, where X is the damage dealt to your opponents this turn. If ~'s prowl cost was paid, take an extra turn after this one. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_FAERIE_ROGUE, get_trap_condition(1-player, TRAP_DAMAGE_TAKEN));
		if( prowl_paid(player, card) ){
			return card_time_walk(player, card, event);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return prowl(player, card, event, SUBTYPE_ROGUE, -1, MANACOST_XU(5, 1));
}

int card_obsidian_battle_axe(int player, int card, event_t event){
/* CARD_ID_OBSIDIAN_BATTLE_AXE	9400
Obsidian Battle-Axe	|3
Tribal Artifact - Warrior Equipment
Equipped creature gets +2/+1 and has haste.
Whenever a Warrior creature enters the battlefield, you may attach ~ to it.
Equip |3 */

	auto_attaching_equipment(player, card, event, SUBTYPE_WARRIOR);

	return vanilla_equipment(player, card, event, 3, 2, 1, 0, SP_KEYWORD_HASTE);
}

static int offalsnout_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_OFFALSNOUT ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			int t_player = -1;
			if( count_graveyard(1-player) && ! graveyard_has_shroud(1-player) ){
				t_player = 1-player;
				if( count_graveyard(player) && ! graveyard_has_shroud(player) ){
					target_definition_t td;
					default_target_definition(player, card, &td, 0);
					td.zone = TARGET_ZONE_PLAYERS;
					td.illegal_abilities = 0;
					td.allow_cancel = 0;
					if( new_pick_target(&td, "TARGET_PLAYER", 1, 0) ){
						t_player = instance->targets[1].player;
					}
				}
			}
			else{
				if( count_graveyard(player) && ! graveyard_has_shroud(player) ){
					t_player = player;
				}
			}
			if( t_player > -1 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
				new_global_tutor(player, t_player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MAX_VALUE, &this_test);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_offalsnout(int player, int card, event_t event){
/* CARD_ID_OFFALSNOUT	9401
Offalsnout	|2|B
Creature - Elemental 2/2
Flash
When ~ leaves the battlefield, exile target card from a graveyard.
Evoke |B (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)offalsnout_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &offalsnout_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if (comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	evoke(player, card, event, MANACOST_B(1));

	return flash(player, card, event);
}

int card_oonas_blackguard(int player, int card, event_t event){
/*
Oona's Blackguard |1|B
Creature — Faerie Rogue 1/1
Flying
Each other Rogue creature you control enters the battlefield with an additional +1/+1 counter on it.
Whenever a creature you control with a +1/+1 counter on it deals combat damage to a player, that player discards a card.
*/
	if( ! is_humiliated(player, card) ){
		card_instance_t* damage = damage_being_dealt(event);
		if(damage && damage->damage_source_player == player && damage->damage_target_card == -1 &&
			in_play(damage->damage_source_player, damage->damage_source_card) && damage->info_slot > 0 &&
			count_1_1_counters(damage->damage_source_player, damage->damage_source_card) > 0 &&
			(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)) &&
		  ! damage_is_to_planeswalker(damage))
		{
			card_instance_t* this = get_card_instance(player, card);
			if( this->targets[8].player < 0 ){
				this->targets[8].player = 0;
			}
			if( damage->damage_target_player == 0){
				SET_BYTE0(this->targets[8].player)++;
			}
			if( damage->damage_target_player == 1){
				SET_BYTE1(this->targets[8].player)++;
			}
		}

		int result = resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, card);

		if( result ){
			if( BYTE0(result) ){
				new_multidiscard(0, BYTE0(result), 0, player);
			}
			if( BYTE1(result) ){
				new_multidiscard(1, BYTE0(result), 0, player);
			}
			get_card_instance(player, card)->targets[8].player = 0;
		}

		if( event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE ){
			test_definition_t test;
			default_test_definition(&test, TYPE_CREATURE);
			test.subtype = SUBTYPE_ROGUE;
			test.not_me = 1;
			test_enters_the_battlefield_with_counters(player, card, event, player, &test, COUNTER_P1_P1, 1);
		}
	}


	return 0;
}

int card_orchard_warden(int player, int card, event_t event){

	/* Orchard Warden	|4|G|G
	 * Creature - Treefolk Shaman 4/6
	 * Whenever another Treefolk creature enters the battlefield under your control, you may gain life equal to that creature's toughness. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_TREEFOLK;
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, get_toughness(trigger_cause_controller, trigger_cause));
	}

	return 0;
}

int card_preeminent_captain(int player, int card, event_t event)
{
/* CARD_ID_PREEMINENT_CAPTAIN	9406
Preeminent Captain	|2|W
Creature - Kithkin Soldier 2/2
First strike (This creature deals combat damage before creatures without first strike.)
Whenever ~ attacks, you may put a Soldier creature card from your hand onto the battlefield tapped and attacking. */

  if (declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), player, card))
	{
	  test_definition_t this_test;
	  new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Soldier card to put into play attacking.");
	  this_test.subtype = SUBTYPE_SOLDIER;
	  new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY_ATTACKING, 0, AI_MAX_CMC, &this_test);
	}

  return 0;
}

int card_prickly_boggart( int player, int card, event_t event){
/* CARD_ID_PRICKLY_BOGGART	9407
Prickly Boggart	|B
Creature - Goblin Rogue 1/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.) */

	fear(player, card, event);
	return 0;
}

int card_pyroclast_consul( int player, int card, event_t event){
/* CARD_ID_PYROCLAST_CONSUL	9410
Pyroclast Consul	|3|R|R
Creature - Elemental Shaman 3/3
Kinship - At the beginning of your upkeep, you may look at the top card of your library. If it shares a creature type with ~, you may reveal it.
If you do, ~ deals 2 damage to each creature. */

	if (kinship(player, card, event)){
		new_damage_all(player, card, ANYBODY, 2, 0, NULL);
	}
	return 0;
}

int card_rage_forger(int player, int card, event_t event)
{
	/* Rage Forger	|2|R	0x2007b9a
	 * Creature - Elemental Shaman 2/2
	 * When ~ enters the battlefield, put a +1/+1 counter on each other Shaman creature you control.
	 * Whenever a creature you control with a +1/+1 counter on it attacks, you may have that creature deal 1 damage to target player or planeswalker. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.subtype = SUBTYPE_SHAMAN;
	  test.not_me = 1;
	  new_manipulate_all(player, card, player, &test, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
	}

  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.power = 0;
	  test.power_flag = F5_POWER_1_1_COUNTERS_GREATER_THAN_VALUE;

	  int amt;
	  if ((amt = declare_attackers_trigger_test(player, card, event, RESOLVE_TRIGGER_AI(player) | DAT_TRACK, player, -1, &test)))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		  for (--amt; amt >= 0; --amt)
			if (in_play(current_turn, attackers[amt]))
			  {
				target_definition_t td;
				default_target_definition(current_turn, attackers[amt], &td, TARGET_TYPE_PLANESWALKER);
				td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

				instance->number_of_targets = 0;
				if (can_target(&td) && pick_next_target_arbitrary(&td, "TARGET_PLAYER_OR_PLANESWALKER", player, card))
				  damage_creature(instance->targets[0].player, instance->targets[0].card, 1, current_turn, attackers[amt]);
			  }
		}
	}

  return 0;
}

int card_reach_of_branches(int player, int card, event_t event){

	/* Reach of Branches	|4|G
	 * Tribal Instant - Treefolk
	 * Put a 2/5 |Sgreen Treefolk Shaman creature token onto the battlefield.
	 * Whenever |Ha Forest enters the battlefield under your control, you may return ~ from your graveyard to your hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_TREEFOLK_SHAMAN);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_redeem_the_lost(int player, int card, event_t event){
/* CARD_ID_REDEEM_THE_LOST	9414
Redeem the Lost	|1|W
Instant
Target creature you control gains protection from the color of your choice until end of turn. Clash with an opponent.
If you win, return ~ to its owner's hand. (Each clashing player reveals the top card of his or her library, then puts that card on the top or bottom.
A player wins if his or her card had a higher converted mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, keyword, 0);
		}
		if( clash(player, card) && ! is_token(player, card) ){
			bounce_permanent(player, card);
			return 0;
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_release_the_ants(int player, int card, event_t event){
	/* Release the Ants	|1|R	0x2007ba9
	 * Instant
	 * ~ deals 1 damage to any target. Clash with an opponent. If you win, return ~ to its owner's hand. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
		if( clash(player, card) && ! is_token(player, card) ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_research_the_deep(int player, int card, event_t event){
/* CARD_ID_RESEARCH_THE_DEEP	9417
Research the Deep	|1|U
Sorcery
Draw a card. Clash with an opponent. If you win, return ~ to its owner's hand. (Each clashing player reveals the top card of his or her library,
then puts that card on the top or bottom. A player wins if his or her card had a higher converted mana cost.) */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
		if( clash(player, card) && ! is_token(player, card) ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return basic_spell(player, card, event);
}

static int reveillark_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_REVEILLARK ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			if( ! graveyard_has_shroud(player) ){
				int i;
				for(i=0; i<2; i++){
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_CREATURE);
					this_test.power = 3;
					this_test.power_flag = F5_POWER_LESSER_THAN_VALUE;
					new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
				}
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}


int card_reveillark(int player, int card, event_t event){
/* CARD_ID_REVEILLARK	9418
Reveillark	|4|W
Creature - Elemental 4/3
Flying
When ~ leaves the battlefield, return up to two target creature cards with power 2 or less from your graveyard to the battlefield.
Evoke |5|W (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)reveillark_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &reveillark_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XW(5, 1));
}

int card_revive_the_fallen(int player, int card, event_t event){
/* CARD_ID_REVIVE_THE_FALLEN	9419
Revive the Fallen	|1|B
Sorcery
Return target creature card from a graveyard to its owner's hand. Clash with an opponent. If you win, return ~ to its owner's hand.
(Each clashing player reveals the top card of his or her library, then puts that card on the top or bottom.
A player wins if his or her card had a higher converted mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1){
			from_grave_to_hand(instance->targets[0].player, selected, TUTOR_HAND);
		}
		if( clash(player, card) && ! is_token(player, card) ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_rhys_the_exiled(int player, int card, event_t event){
/*
Rhys the Exiled |2|G
Legendary Creature — Elf Warrior 3/2
Whenever Rhys the Exiled attacks, you gain 1 life for each Elf you control.
{B}, Sacrifice an Elf: Regenerate Rhys the Exiled.
*/
	check_legend_rule(player, card, event);

	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		gain_life(player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ELF));
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select an Elf to sacrifice.");
	if( IS_AI(player) ){
		test.not_me = 1;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_B(1), 0, NULL, NULL) ){
			if( new_can_sacrifice_as_cost(player, card, &test) ){
				return 99;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST_B(1));
	}

	return 0;
}

int card_rustic_clachan(int player, int card, event_t event){
	/* Rustic Clachan	""
	 * Land
	 * As ~ enters the battlefield, you may reveal a Kithkin card from your hand. If you don't, ~ enters the battlefield tapped.
	 * |T: Add |W to your mana pool.
	 * Reinforce 1-|1|W */

	lorwyn_need_subtype_land(player, card, event, SUBTYPE_KITHKIN);

	if (IS_ACTIVATING_FROM_HAND(event)){
		return reinforce(player, card, event, 1, MANACOST_XW(1, 1));
	}

	return mana_producer(player, card, event);
}

int card_sage_of_fables(int player, int card, event_t event){
	/* Sage of Fables	|2|U
	 * Creature - Merfolk Wizard 2/2
	 * Each other Wizard creature you control enters the battlefield with an additional +1/+1 counter on it.
	 * |2, Remove a +1/+1 counter from a creature you control: Draw a card. */
	if( (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && in_play(player, card) &&
		! is_humiliated(player, card) )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_WIZARD;
		test.not_me = 1;
		test_enters_the_battlefield_with_counters(player, card, event, player, &test, COUNTER_P1_P1, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_P1_P1;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_X(2), 0, &td,
		                                                    "Select a creature you control to remove a counter.");
		if( spell_fizzled != 1 ){
			if( count_1_1_counters(instance->targets[0].player, instance->targets[0].card) > 0 ){
				remove_1_1_counter(instance->targets[0].player, instance->targets[0].card);
			}
			else{
				spell_fizzled = 1;
			}
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_sages_dousing(int player, int card, event_t event){
	/* Sage's Dousing	|2|U
	 * Tribal Instant - Wizard
	 * Counter target spell unless its controller pays |3. If you control a Wizard, draw a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3) &&
			check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_WIZARD) )
		{
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

static int count_deck_by_csvid(int player, int csvid){
	int amount = 0;
	int count = 0;
	while( deck_ptr[player][count] != -1 ){
			if( cards_data[deck_ptr[player][count]].id == csvid ){
				amount++;
			}
			count++;
	}
	return amount;
}

static int get_max_sac_for_scapeshift(int player, int card){
	int max_sac = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	// First case: a Valakut deck
	int valakuts = count_cards_by_id(player, CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE);
	int po_flag = check_battlefield_for_id(player, CARD_ID_PRISMATIC_OMEN);
	int v_in_deck = count_deck_by_csvid(player, CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE);
	if( valakuts || po_flag || v_in_deck ){
		if( valakuts ){
			if( ! po_flag ){
				test.id = CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE;
				test.id_flag = DOESNT_MATCH;
				while( (life[1-player] - (valakuts * max_sac * 3) > 0) || max_sac < 6 ){
						max_sac++;
				}
			}
			else{
				if( v_in_deck ){
					while( (life[1-player] - (v_in_deck * 3 * (max_sac - v_in_deck)) > 0) || max_sac < 6 ){
							max_sac++;
					}
				}
			}
		}
		else if( po_flag ){
				if( v_in_deck ){
					while( (life[1-player] - (v_in_deck * 3 * (max_sac - v_in_deck)) > 0) || max_sac < 6 ){
							max_sac++;
					}
				}
		}
		else if( v_in_deck ){
				while( (life[1-player] - (v_in_deck * 3 * (max_sac - v_in_deck)) > 0) || max_sac < 6 ){
						max_sac++;
				}
		}
		return max_sac;
	}

	// Second case: a Cloudpost-based deck
	int cp_count = count_cards_by_id(player, CARD_ID_CLOUDPOST);
	int cp_in_deck = count_deck_by_csvid(player, CARD_ID_CLOUDPOST);
	int gp_count = count_cards_by_id(player, CARD_ID_GLIMMERPOST);
	if( cp_count || cp_in_deck ){
		if( cp_in_deck ){
			return cp_in_deck;
		}
		else{
			return count_deck_by_csvid(player, CARD_ID_VESUVA);
		}
	}
	if( gp_count ){
		return count_deck_by_csvid(player, CARD_ID_CLOUDPOST);
	}

	//Third case: an Urzatron deck
	if( count_deck_by_csvid(player, CARD_ID_URZAS_TOWER) || count_deck_by_csvid(player, CARD_ID_URZAS_MINE) ||
		count_deck_by_csvid(player, CARD_ID_URZAS_POWER_PLANT) )
	{
		int pieces[3] = {0};
		CYCLE_ALL_CARDS(p, c,	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_PERMANENT) ){
										if( get_card_name(p, c) == CARD_ID_URZAS_TOWER ){
											pieces[0]++;
										}
										if( get_card_name(p, c) == CARD_ID_URZAS_MINE ){
											pieces[0]++;
										}
										if( get_card_name(p, c) == CARD_ID_URZAS_POWER_PLANT ){
											pieces[0]++;
										}
									}
								}
		);
		int i;
		for(i=0; i<3; i++){
			max_sac += (! pieces[i] ? 1 : 0);
		}
		return max_sac;
	}

	return 0;
}

static void scapeshift_resolution_for_ai(int player, int amount){
	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);

	// First case: a Valakut deck
	int valakuts = count_cards_by_id(player, CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE);
	int po_flag = check_battlefield_for_id(player, CARD_ID_PRISMATIC_OMEN);
	int v_in_deck = count_deck_by_csvid(player, CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE);
	if( valakuts || po_flag || v_in_deck ){
		if( valakuts ){
			test.qty = amount;
			test.subtype = SUBTYPE_MOUNTAIN;
			test.no_shuffle = 1;
			int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
			int diff = amount - result;
			if( diff ){
				default_test_definition(&test, TYPE_LAND);
				test.qty = diff;
				test.no_shuffle = 1;
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
			}
			shuffle(player);
			return;
		}
		else if( po_flag || v_in_deck ){
				int req = 1;
				while( life[1-player] - (req * 3 * amount) > 0 ){
						req++;
				}
				test.id = CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE;
				test.qty = req;
				test.no_shuffle = 1;
				int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
				int diff = amount - result;
				if( diff ){
					default_test_definition(&test, TYPE_LAND);
					test.subtype = SUBTYPE_MOUNTAIN;
					test.qty = diff;
					test.no_shuffle = 1;
					result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
					diff -= result;
					if( diff ){
						default_test_definition(&test, TYPE_LAND);
						test.qty = diff;
						test.no_shuffle = 1;
						new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
					}
				}
				shuffle(player);
				return;
		}
	}

	// Second case: a Cloudpost-based deck
	int cp_count = count_cards_by_id(player, CARD_ID_CLOUDPOST);
	int cp_in_deck = count_deck_by_csvid(player, CARD_ID_CLOUDPOST);
	int gp_count = count_cards_by_id(player, CARD_ID_GLIMMERPOST);
	if( cp_count || cp_in_deck ){
		if( cp_in_deck ){
			test.id = CARD_ID_CLOUDPOST;
			test.qty = amount;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
			return;
		}
		else{
			test.id = CARD_ID_VESUVA;
			test.qty = amount;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
			return;
		}
	}
	if( gp_count ){
		test.id = CARD_ID_CLOUDPOST;
		test.qty = amount;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		return;
	}

	//Third case: an Urzatron deck
	if( count_deck_by_csvid(player, CARD_ID_URZAS_TOWER) || count_deck_by_csvid(player, CARD_ID_URZAS_MINE) ||
		count_deck_by_csvid(player, CARD_ID_URZAS_POWER_PLANT) )
	{
		int pieces[3] = {0};
		CYCLE_ALL_CARDS(p, c,	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_PERMANENT) ){
										if( get_card_name(p, c) == CARD_ID_URZAS_TOWER ){
											pieces[0]++;
										}
										if( get_card_name(p, c) == CARD_ID_URZAS_MINE ){
											pieces[0]++;
										}
										if( get_card_name(p, c) == CARD_ID_URZAS_POWER_PLANT ){
											pieces[0]++;
										}
									}
								}
		);
		if( ! pieces[0] ){
			test.id = CARD_ID_URZAS_TOWER;
			test.qty = 1;
			test.no_shuffle = 1;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
		if( ! pieces[1] ){
			default_test_definition(&test, TYPE_LAND);
			test.id = CARD_ID_URZAS_MINE;
			test.qty = 1;
			test.no_shuffle = 1;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
		if( ! pieces[2] ){
			default_test_definition(&test, TYPE_LAND);
			test.id = CARD_ID_URZAS_POWER_PLANT;
			test.qty = 1;
			test.no_shuffle = 1;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
		shuffle(player);
		return;
	}
}

static int forbidden_land_sacrifices_for_ai(int unused1, int unused2, int player, int card){
	int csvid = get_card_name(player, card);
	switch( csvid ){
			case CARD_ID_CLOUDPOST:
			case CARD_ID_GLIMMERPOST:
			case CARD_ID_VALAKUT_THE_MOLTEN_PINNACLE:
			case CARD_ID_URZAS_TOWER:
			case CARD_ID_URZAS_MINE:
			case CARD_ID_URZAS_POWER_PLANT:
				return 1;
	}
	return 0;
}


int card_scapeshift(int player, int card, event_t event){
/* CARD_ID_SCAPESHIFT	9426
Scapeshift	|2|G|G
Sorcery
Sacrifice any number of lands. Search your library for up to that many land cards, put them onto the battlefield tapped, then shuffle your library.*/

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		int max_to_sac = get_max_sac_for_scapeshift(player, card);
		if( max_to_sac && max_can_sacrifice_as_cost(player, card, &test) >= max_to_sac ){
			ai_modifier += 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		if( IS_AI(player) ){
			test.special_selection_function = &forbidden_land_sacrifices_for_ai;
		}

		int max_to_sac = get_max_sac_for_scapeshift(player, card);

		int marked[150];
		int sacced = 0;
		while( new_can_sacrifice(player, card, player, &test) ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( sac ){
					marked[sacced] = BYTE3(sac);
					sacced++;
					if( IS_AI(player) ){
						if( sacced == max_to_sac ){
							break;
						}
					}
				}
				else{
					break;
				}
		}

		int i;
		for(i=0; i<sacced; i++){
			kill_card(player, marked[i], KILL_SACRIFICE);
		}
		if( ! IS_AI(player) ){
			new_default_test_definition(&test, TYPE_LAND, "Select a land card.");
			test.qty = sacced;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
		}
		else{
			scapeshift_resolution_for_ai(player, sacced);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_scarblade_elite(int player, int card, event_t event){
/*
Scarblade Elite |B|B
Creature — Elf Assassin 2/2
{T}, Exile an Assassin card from your graveyard: Destroy target creature.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, 0, "Select an Assassin card to exile.");
	this_test.subtype = SUBTYPE_ASSASSIN;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return new_special_count_grave(player, &this_test) ? 1 : 0;
		}
	}

	if(event == EVENT_ACTIVATE ){
		int to_exile = new_select_a_card(player, player, TUTOR_FROM_GRAVE, 0, AI_MIN_VALUE, -1, &this_test);
		if( to_exile > -1 ){
			generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
			if( spell_fizzled != 1 ){
				rfg_card_from_grave(player, to_exile);
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_sensation_gorger(int player, int card, event_t event){
/* CARD_ID_SENSATION_GORGER	9429
Sensation Gorger	|1|R|R
Creature - Goblin Shaman 2/2
Kinship - At the beginning of your upkeep, you may look at the top card of your library. If it shares a creature type with ~, you may reveal it.
If you do, each player discards his or her hand, then draws four cards. */

	if (kinship(player, card, event)){
		APNAP(p,{
				new_discard_all(p, player);
				draw_cards(p, 4);
				};
		);
	}
	return 0;
}

int card_shard_volley(int player, int card, event_t event){
	/* Shard Volley	|R	0x2007bd1
	 * Instant
	 * As an additional cost to cast this spell, sacrifice a land.
	 * ~ deals 3 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	if( event == EVENT_CAN_CAST && can_target(&td) ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		state_untargettable(BYTE2(sac), BYTE3(sac), 0);
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_shared_animosity(int player, int card, event_t event){
/* CARD_ID_SHARED_ANIMOSITY	9430
Shared Animosity	|2|R
Enchantment
Whenever a creature you control attacks, it gets +1/+0 until end of turn for each other attacking creature that shares a creature type with it.*/

	if( ! is_humiliated(player, card) && (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS) ){
		if(	declare_attackers_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, -1) ){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) && is_attacking(player, count) ){
					int k;
					int result = 0;
					for(k=0; k<active_cards_count[player]; k++){
						if( count != k && in_play(player, k) && is_what(player, k, TYPE_CREATURE) && is_attacking(player, k) &&
							shares_creature_subtype(player, count, player, k)
						  ){
							result++;
						}
					}
					if( result > 0 ){
						pump_until_eot(player, card, player, count, result, 0);
					}
				}
				count--;
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_shinewed(int player, int card, event_t event){
/* CARD_ID_SHINEWEND	9431
Shinewend	|1|W
Creature - Elemental 0/0
Flying
~ enters the battlefield with a +1/+1 counter on it.
|1|W, Remove a +1/+1 counter from ~: Destroy target enchantment. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), GVC_COUNTER(COUNTER_P1_P1),
									&td, "TARGET_ENCHANTMENT");
}

int card_sigil_tracer(int player, int card, event_t event){
/* CARD_ID_SIGIL_TRACER	9432
Sigil Tracer	|1|U|U
Creature - Merfolk Wizard 2/2
|1|U, Tap two untapped Wizards you control: Copy target instant or sorcery spell. You may choose new targets for the copy. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);
	td1.illegal_state = TARGET_STATE_TAPPED;
	td1.required_subtype = SUBTYPE_WIZARD;
	td1.preferred_controller = player;
	td1.allowed_controller = player;
	td1.illegal_abilities = 0;

	target_definition_t td2;
	counterspell_target_definition(player, card, &td2, TYPE_SPELL);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XU(1, 1), 0, &td2, NULL);
		if( result && target_available(player, card, &td1) > 1 ){
			return result;
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_XU(1, 1)) ){
			if( tapsubtype_ability(player, card, 2, &td1) ){
				activate_twincast(player, card, event, &td2, NULL);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		activate_twincast(player, card, event, &td2, NULL);
	}

	return 0;
}

static int slithermuse_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SLITHERMUSE ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			int amount = hand_count[1-player]-hand_count[player];
			if( amount > 0 ){
				draw_cards(player, amount);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_slithermuse(int player, int card, event_t event){
/* CARD_ID_SLITHERMUSE	9433
Slithermuse	|2|U|U
Creature - Elemental 3/3
When ~ leaves the battlefield, choose an opponent. If that player has more cards in hand than you, draw cards equal to the difference.
Evoke |3|U (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)slithermuse_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &slithermuse_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if (comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XU(3, 1));
}

static int spitebellows_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_SPITEBELLOWS ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			instance->damage_target_player = instance->damage_target_card = -1;
			remove_status(player, card, STATUS_INVISIBLE_FX);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);

			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_CREATURE);
			td1.illegal_abilities = get_protections_from(instance->targets[0].player, instance->targets[0].card);
			td1.allow_cancel = 0;

			if( can_target(&td1) && new_pick_target(&td1, "Select another target creature.", 1, GS_LITERAL_PROMPT) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, 6,
								instance->targets[0].player, instance->targets[0].card);
			}

			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_spitebellows(int player, int card, event_t event){
/* CARD_ID_SPITEBELLOWS	9434
Spitebellows	|5|R
Creature - Elemental 6/1
When ~ leaves the battlefield, it deals 6 damage to target creature.
Evoke |1|R|R (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card)  ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)spitebellows_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &spitebellows_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if (comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XR(1, 2));
}

int card_squeaking_pie_grubfellows(int player, int card, event_t event){
/* CARD_ID_SQUEAKING_PIE_GRUBFELLOWS	9435
Squeaking Pie Grubfellows	|3|B
Creature - Goblin Shaman 3/2
Kinship - At the beginning of your upkeep, you may look at the top card of your library. If it shares a creature type with ~, you may reveal it.
If you do, each opponent discards a card. */

	if (kinship(player, card, event)){
		discard(1-player, 0, player);
	}
	return 0;
}

int card_stenchskipper(int player, int card, event_t event){
/* CARD_ID_STENCHSKIPPER	9436
Stenchskipper	|3|B
Creature - Elemental 6/5
Flying
At the beginning of the end step, if you control no Goblins, sacrifice ~. */

	if( eot_trigger(player, card, event) && ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_GOBLIN) ){
		kill_card(player, card, KILL_SACRIFICE);
	}
	return 0;
}

int card_stinkdrinker_bandit(int player, int card, event_t event){
/* CARD_ID_STINKDRINKER_BANDIT	9438
Stinkdrinker Bandit	|3|B
Creature - Goblin Rogue 2/1
Prowl |1|B (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Goblin or Rogue.)
Whenever a Rogue you control attacks and isn't blocked, it gets +2/+1 until end of turn. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( current_turn == player && current_phase == PHASE_AFTER_BLOCKING && instance->targets[1].player != 66  ){
			int count = active_cards_count[player];
			while( count > -1 ){
					if( in_play(player, count) && is_attacking(player, count) && is_unblocked(player, count) &&
						has_subtype(player, count, SUBTYPE_ROGUE) )
					{
						pump_until_eot(player, card, player, count, 2, 1);
					}
					count--;
			}
			instance->targets[1].player = 66;
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
	}

	return prowl(player, card, event, SUBTYPE_GOBLIN, SUBTYPE_ROGUE, MANACOST_XB(1, 1));
}

int card_stonehewer_giant(int player, int card, event_t event){
/* CARD_ID_STONEHEWER_GIANT	9440
Stonehewer Giant	|3|W|W
Creature - Giant Warrior 4/4
Vigilance
|1|W, |T: Search your library for an Equipment card and put it onto the battlefield. Attach it to a creature you control. Then shuffle your library.*/

	vigilance(player, card, event);

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_phase == PHASE_AFTER_BLOCKING ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an Equipment card.");
		this_test.subtype = SUBTYPE_EQUIPMENT;
		int equip = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		if( equip != -1 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE );
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "Select target creature you control.", 0, 0) ){
				equip_target_creature(player, equip, player, instance->targets[0].card);
			}
			instance->number_of_targets = 0;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_stonybrook_banneret(int player, int card, event_t event){
/* CARD_ID_STONYBROOK_BANNERET	9441
Stonybrook Banneret	|1|U
Creature - Merfolk Wizard 1/1
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
Merfolk spells and Wizard spells you cast cost |1 less to cast. */

	return banneret(player, card, event, SUBTYPE_MERFOLK, SUBTYPE_WIZARD);
}

// merfolk wizard --> rhino token.

int card_stonybrook_schoolmaster(int player, int card, event_t event){
	/* Stonybrook Schoolmaster	|2|W
	 * Creature - Merfolk Wizard 1/2
	 * Whenever ~ becomes tapped, you may put a 1/1 |Sblue Merfolk Wizard creature token onto the battlefield. */

	if( event == EVENT_TAP_CARD && affect_me(player, card) && ! is_humiliated(player, card) ){
		int choice = player == HUMAN ? do_dialog(player, player, card, -1, -1, " Generate a Merfolk Wizard\n Pass", 0) : 0;
		if( choice == 0 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_MERFOLK_WIZARD, &token);
			token.pow = token.tou = 1;
			token.color_forced = COLOR_TEST_BLUE;
			generate_token(&token);
		}
	}
	return 0;
}

// supreme exemplar --> nova chaser

int card_tauren_mauler(int player, int card, event_t event){
/* CARD_ID_TAUREAN_MAULER	9447
Taurean Mauler	|2|R
Creature - Shapeshifter 2/2
Changeling (This card is every creature type.)
Whenever an opponent casts a spell, you may put a +1/+1 counter on ~. */

	if( new_specific_spell_played(player, card, event, 1-player, RESOLVE_TRIGGER_AI(player), NULL) ){
		add_1_1_counter(player, card);
	}

	return 0;
}

int card_thieves_fortune(int player, int card, event_t event){
/* CARD_ID_THIEVES_FORTUNE	9448
Thieves' Fortune	|2|U
Tribal Instant - Rogue
Prowl |U (You may cast this for its prowl cost if you dealt combat damage to a player this turn with a Rogue.)
Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in any order. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		impulse_effect(player, 4, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return prowl(player, card, event, SUBTYPE_ROGUE, -1, MANACOST_U(1));
}

int card_thornbite_staff(int player, int card, event_t event){
	/* Thornbite Staff	|2	0x2007c12
	 * Tribal Artifact - Shaman Equipment
	 * Equipped creature has "|2, |T: This creature deals 1 damage to any target" and "Whenever a creature dies, untap this creature."
	 * Whenever a Shaman creature enters the battlefield, you may attach ~ to it.
	 * Equip |4 */
	auto_attaching_equipment(player, card, event, SUBTYPE_SHAMAN);

	if( is_equipping(player, card) ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( p > -1 ){
			untap_card(p, c);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_TAP_AND_DRAW
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 4);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, can_activate_basic_equipment(player, card, event, 4), 0};
		if( is_equipping(player, card) ){

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0,
												&td, NULL)  )
			{
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 5,
							"Tap & damage", abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 4);
		}
		if( choice == CHOICE_TAP_AND_DRAW ){

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			target_definition_t td;
			default_target_definition(p, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0,
													&td, "TARGET_ANY");
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 4);
		}
		if( instance->info_slot == CHOICE_TAP_AND_DRAW ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			if( p > -1 ){
				target_definition_t td;
				default_target_definition(p, c, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
				td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

				card_instance_t *inst = get_card_instance(p, c);

				if( validate_target(p, c, &td, 0) ){
					 damage_creature(inst->targets[0].player, inst->targets[0].card, 1, p, c);
				}
			}
		}
	}

	return 0;
}

int card_titans_revenge(int player, int card, event_t event){
	/* Titan's Revenge	|X|R|R	0x2007c17
	 * Sorcery
	 * ~ deals X damage to any target. Clash with an opponent. If you win, return ~ to its owner's hand. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int valid = 0;
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
			valid = 1;
		}
		if( valid && clash(player, card) && ! is_token(player, card) ){
			bounce_permanent(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_unstoppable_ash(int player, int card, event_t event){
/* CARD_ID_UNSTOPPABLE_ASH	9451
Unstoppable Ash	|3|G
Creature - Treefolk Warrior 5/5
Trample
Champion a Treefolk or Warrior (When this enters the battlefield, sacrifice it unless you exile another Treefolk or Warrior you control.
When this leaves the battlefield, that card returns to the battlefield.) Whenever a creature you control becomes blocked, it gets +0/+5 until end of turn. */

	if( in_play(player, card) && ! is_humiliated(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( current_turn == player && current_phase == PHASE_AFTER_BLOCKING && instance->targets[1].player != 66 ){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(player, count) && is_attacking(player, count) && ! is_unblocked(player, count) ){
						pump_until_eot(player, card, player, count, 0, 5);
					}
					count--;
			}
			instance->targets[1].player = 66;
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return champion(player, card, event, SUBTYPE_TREEFOLK, SUBTYPE_WARRIOR);
}

int card_vendilion_clique(int player, int card, event_t event){
/* CARD_ID_VENDILION_CLIQUE	9452
Vendilion Clique	|1|U|U
Legendary Creature - Faerie Wizard 3/1
Flash
Flying
When ~ enters the battlefield, look at target player's hand. You may choose a nonland card from it.
If you do, that player reveals the chosen card, puts it on the bottom of his or her library, then draws a card. */

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			char msg[100] = "You may select a nonland card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, msg);
			this_test.type_flag = 1;

			int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_HAND, 0, AI_MAX_VALUE, -1, &this_test);
			if( selected != -1 ){
				put_on_top_of_deck(instance->targets[0].player, selected);
				put_top_card_of_deck_to_bottom(instance->targets[0].player);
				draw_cards(instance->targets[0].player, 1);
			}
		}
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

int card_vengeful_firebrand(int player, int card, event_t event){
  /* Vengeful Firebrand	|3|R
   * Creature - Elemental Warrior 5/2
   * ~ has haste as long as a Warrior card is in your graveyard.
   * |R: ~ gets +1/+0 until end of turn. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card) && is_sick(player, card) &&
		any_in_graveyard_by_subtype(player, SUBTYPE_WARRIOR) )
		give_haste(player, card);

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,0);
}

int card_veterans_armaments(int player, int card, event_t event){
/* CARD_ID_VETERANS_ARMAMENTS	9454
Veteran's Armaments	|2
Tribal Artifact - Soldier Equipment
Equipped creature has "Whenever this creature attacks or blocks, it gets +1/+1 until end of turn for each attacking creature."
Whenever a Soldier creature enters the battlefield, you may attach ~ to it.
Equip |2 */

	auto_attaching_equipment(player, card, event, SUBTYPE_SOLDIER);

	if (event == EVENT_ACTIVATE && !is_equipping(player, card))
		ai_modifier += 30;

	card_instance_t* instance;

  // Equipped creature has "Whenever this creature attacks or blocks, it gets +1/+1 until end of turn for each attacking creature."
  if ((xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS || event == EVENT_DECLARE_BLOCKERS)
	  && (instance = in_play(player, card))
	  && (xtrigger_condition() == XTRIGGER_ATTACKING	// resolving the trigger
		  || (is_equipping(player, card)
			  && !is_humiliated(player, card)
			  && !is_humiliated(instance->targets[8].player, instance->targets[8].card)))
	  && (declare_attackers_trigger(player, card, event, DAT_TRACK, instance->targets[8].player, instance->targets[8].card)
		  || blocking(instance->targets[8].player, instance->targets[8].card, event)))
	{
	  int p, c;
	  if (xtrigger_condition() == XTRIGGER_ATTACKING)	// apply to the creature it was equipping when attack was declared
		{
		  p = current_turn;
		  c = BYTE0(instance->targets[2].player);
		  if (!in_play(p, c))
			return 0;
		}
	  else	// Blocking, so it's the same creature currently equipped
		{
		  p = instance->targets[8].player;
		  c = instance->targets[8].card;
		}

	  int amount = count_attackers(current_turn);
	  pump_until_eot(player, card, p, c, amount, amount);
	}


	// Equip |2
	return basic_equipment(player, card, event, 2);
}

static int walker_of_the_grove_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_WALKER_OF_THE_GROVE ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( other_leaves_play(player, card, p, c, event) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
			token.color_forced = get_sleighted_color_test(instance->targets[0].player, instance->targets[0].card, COLOR_TEST_GREEN);
			token.pow = token.tou = 4;
			generate_token(&token);
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int card_walker_of_the_grove(int player, int card, event_t event){
	/* Walker of the Grove	|6|G|G
	 * Creature - Elemental 7/7
	 * When ~ leaves the battlefield, put a 4/4 |Sgreen Elemental creature token onto the battlefield.
	 * Evoke |4|G */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code > 0 ){
			return 0;
		}
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)walker_of_the_grove_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &walker_of_the_grove_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if (comes_into_play(player, card, event) && evoked(player, card)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return evoke(player, card, event, MANACOST_XG(4, 1));
}

int card_wandering_graybeard(int player, int card, event_t event){
/* CARD_ID_WANDERING_GRAYBEARD	9457
Wandering Graybeard	|3|W|W
Creature - Giant Wizard 4/4
Kinship - At the beginning of your upkeep, you may look at the top card of your library.
If it shares a creature type with ~, you may reveal it. If you do, you gain 4 life. */

	if (kinship(player, card, event)){
		gain_life(player, 4);
	}
	return 0;
}

// warren's weirdings --> diabolic edict

int card_weirding_shaman(int player, int card, event_t event){
/* CARD_ID_WEIRDING_SHAMAN	9463
Weirding Shaman	|1|B
Creature - Goblin Shaman 2/1
|3|B, Sacrifice a Goblin: Create two 1/1 |Sblack Goblin Rogue creature tokens. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_GOBLIN_ROGUE, &token);
		token.qty = 2;
		generate_token(&token);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST_XB(3, 1), 0, NULL, NULL, 0, &test, 0);
}

int card_wolf_skull_shaman(int player, int card, event_t event){
	/* Wolf-Skull Shaman	|1|G
	 * Creature - Elf Shaman 2/2
	 * Kinship - At the beginning of your upkeep, you may look at the top card of your library. If it shares a creature type with ~, you may reveal it. If you do, put a 2/2 |Sgreen Wolf creature token onto the battlefield. */

	if (kinship(player, card, event)){
		generate_token_by_id(player, card, CARD_ID_WOLF);
	}
	return 0;
}
