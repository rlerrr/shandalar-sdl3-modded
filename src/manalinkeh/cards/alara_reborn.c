#include "manalink.h"

/* Canonical recovered helper declarations. */
void cascade(int player, int card, event_t event);

// Functions

static int sen_triplets_effect(int player, int card, event_t event);

void do_multicascade(int player, int card, int override, int amount);
void multicascade(int player, int card, event_t event, int amount);

/* ManalinkEh 2022: 0x22fa680; original cards/alara_reborn.c:4. */
void do_multicascade(int player, int card, int override, int amount)
{
  int exiled[500];
  int my_cmc = get_cmc(player, card);
  if (override >= 0)
    my_cmc = override;
  int *deck = deck_ptr[player];
  for (int i = 0; i < amount; ++i)
  {
    int averna_check = 0;
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (in_play(player, c) != NULL && get_id(player, c) == CARD_ID_AVERNA_THE_CHAOS_BLOOM &&
          is_humiliated(player, c, AL_STATIC) == 0)
        ++averna_check;

    int lands_exiled = 0;
    int exiled_index = 0;
    int iid;
    bool stopped = false;
    do
    {
      iid = deck[exiled_index];
      if (iid == -1)
      {
        stopped = true;
        break;
      }
      iid = deck[0];
      exiled[exiled_index] = iid;
      rfg_top_card_of_deck(player);
      int is_land = is_what(-1, iid, TYPE_LAND);
      ++exiled_index;
      lands_exiled = (lands_exiled + 1) - (unsigned int)(is_land == 0);
      if (is_what(-1, iid, TYPE_LAND) == 0 && get_cmc_by_internal_id(iid) < my_cmc)
      {
        stopped = true;
        break;
      }
    } while (exiled_index != 500);
    if (exiled_index == 0)
      continue;

    if (!stopped && averna_check != 0)
      iid = -1;
    if (averna_check != 0 && lands_exiled != 0)
    {
      while (true)
      {
        test_definition_t test;
        new_default_test_definition(&test, TYPE_LAND, "Select a Land card to put into play tapped.");
        int selected = select_card_from_zone(player, player, exiled, exiled_index,
                                             0, 1, -1, &test);
        if (selected < 0)
          break;
        int added = add_card_to_hand(player, exiled[selected]);
        add_state(player, added, STATE_TAPPED);
        put_into_play(player, added);
        for (int z = selected; z < exiled_index; ++z)
          exiled[z] = exiled[z + 1];
        --exiled_index;
        --averna_check;
        --lands_exiled;
        if (averna_check == 0 || lands_exiled == 0)
          break;
      }
    }
    if (stopped && iid >= 0 && can_legally_play_iid(player, iid) != 0)
    {
      char buffer[1000];
      unsigned short csvid = cards_data[iid].id;
      scnprintf(buffer, 1000, " Play %s\n Do not play", cards_ptr[csvid]->name);
      if (do_dialog(player, player, card, -1, -1, buffer, 0) == 0)
      {
        --exiled_index;
        play_card_in_exile_for_free(player, player, (unsigned int)csvid);
        exiled[exiled_index] = -1;
      }
    }
    int total = count_deck(player) + exiled_index;
    while (exiled_index > 0)
    {
      int destination = total - exiled_index;
      if (exiled_index < 2)
        deck_ptr[player][destination] = exiled[0];
      else
      {
        int rnd = internal_rand(exiled_index);
        deck_ptr[player][destination] = exiled[rnd];
        for (int z = rnd; z < exiled_index; ++z)
          exiled[z] = exiled[z + 1];
      }
      --exiled_index;
    }
  }
  int *grave = graveyard_ptr_mutable[player];
  int count = count_graveyard(player);
  for (int k = count - 1; k >= 0; --k)
    if (cards_data[grave[k]].id == CARD_ID_AURORA_PHOENIX)
    {
      add_card_to_hand(player, grave[k]);
      remove_card_from_grave(player, k);
    }
}

void do_cascade(int player, int card, int override) { do_multicascade(player, card, override, 1); }

/* ManalinkEh 2022: 0x22fab70; original cards/alara_reborn.c:111. */
void multicascade(int player, int card, event_t event, int amount)
{
  int result;

  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_token(player, card);
    if (result == 0)
    {
      result = check_special_flags(player, card, 0x40000);
      if (result == 0)
      {
        result = get_cmc(player, card);
        do_multicascade(player, card, result, amount);
        return;
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x22fabf0; original cards/alara_reborn.c:117. */
void cascade(int player, int card, event_t event)
{
  multicascade(player, card, event, 1);
  return;
}

static int has_mana_hybrid_arb(int player, int number, int first_color, int second_color, int colorless, int exc_clr){

	int i,x;
	int mana[6];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}
	mana[exc_clr] = 1;

	for ( i = number ;i>=0;i--){
		 mana[first_color] = i;
		 mana[second_color] = number - i;
		if( has_mana_multi( player,  colorless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			return 1;
		}
	}
	return 0;
}

static int charge_mana_hybrid_arb(int player, int card, int number, int first_color, int second_color, int colorless, int exc_clr){
	int i,x;
	int mana[6];
	int mana2[10];

	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}
	mana[exc_clr] = 1;

	char buffer[500];
	int pos = scnprintf(buffer, 500, " Cancel\n");
	int m = 1;
	test_definition_t this_test;
	for( i = number ;i>=0;i--){
		mana[first_color] = i;
		mana[second_color] = number - i;
		if( has_mana_multi( player, colorless, mana[1], mana[2], mana[3], mana[4], mana[5]) ){
			mana2[m] = 0;
			mana_into_string(colorless, mana[1], mana[2], mana[3], mana[4], mana[5], &this_test);
			pos +=scnprintf(buffer+pos, 500-pos, " Pay %s\n", this_test.message);
			int q;
			for(q=1; q<6; q++){
				int k;
				for(k=0; k<mana[q]; k++){
					mana2[m] |= (1<<(((q-1)*6)+k));
				}
			}
			m++;
		}
	}
	int choice = 1;
	if( m > 2 && player == HUMAN){
		choice = do_dialog(player, player, card, -1, -1, buffer, 1);
	}
	if( choice == 0 ){
		spell_fizzled = 1;
		return 0;
	}
	for( x = 0 ;x < 6;x++){
		mana[x] = 0;
	}
	int q;
	for(q=0; q<5; q++){
		int k;
		for(k=0; k<6; k++){
			if( mana2[choice] & (1<<(((q*6)+k))) ){
				mana[q+1]++;
			}
		}
	}
	charge_mana_multi(  player, colorless, mana[1], mana[2], mana[3], mana[4], mana[5] );
	if( spell_fizzled != 1 ){
		return 1;
	}
	return 0;
}

static int arb_hybrid_casting(int player, int card, event_t event, int exc_clr){

	card_instance_t *instance = get_card_instance(player, card);

	if( played_for_free(player, card) || is_token(player, card) || instance->info_slot == 0){
		return 1;
	}
	else{
		int c1 = get_colors_for_hybrid(player, card, exc_clr, 0);
		int c2 = get_colors_for_hybrid(player, card, c1, exc_clr);
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_hybrid_arb(player, get_number_of_hybrid_mana(player, card, exc_clr, 0), c1, c2, cless, exc_clr) ){
			charge_mana_hybrid_arb(player, card, get_number_of_hybrid_mana(player, card, exc_clr, 0), c1, c2, cless, exc_clr);
			if( spell_fizzled != 1 ){
				return 1;
			}
		}
	}
	return 0;
}

static int modify_cost_for_arb_hybrid_spells(int player, int card, event_t event, int exc_clr ){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int c1 = get_colors_for_hybrid(player, card, exc_clr, 0);
		int c2 = get_colors_for_hybrid(player, card, c1, exc_clr);
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_hybrid_arb(player, get_number_of_hybrid_mana(player, card, exc_clr, 0), c1, c2, cless, exc_clr) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	return 0;
}

static int arb_new_hybrid(int player, int card, event_t event, int exc_clr ){

	card_instance_t *instance = get_card_instance(player, card);

	modify_cost_for_arb_hybrid_spells(player, card, event, exc_clr);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( instance->info_slot == 1 && ! played_for_free(player, card) && ! is_token(player, card) ){
			if( ! arb_hybrid_casting(player, card, event, exc_clr) ){
				spell_fizzled = 1;
			}
		}
	}
	return 0;
}

static int alara_reborn_blades(int player, int card, event_t event, int color, int keyword, int sp_keyword)
{
  int result;
  test_definition_t this_test;
  if ((((uint32_t)(event - EVENT_POWER) < 3) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      default_test_definition(&this_test, 0x47);
      this_test.not_me = 1;
      this_test.color_flag = 2;
      result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
      if (result != 0)
      {
        if (event == EVENT_ABILITIES)
        {
          event_result = event_result | keyword;
          if (sp_keyword != 0)
          {
            special_abilities(player, card, EVENT_ABILITIES, sp_keyword, player, card);
          }
        }
        else
        {
          event_result = event_result + 1;
        }
      }
    }
  }
  result = 0;
  if (0 < color)
  {
    result = arb_new_hybrid(player, card, event, color);
  }
  return result;
}

// Cards

int card_anathemancer(int player, int card, event_t event){
/* CARD_ID_ANATHEMANCER	10236
Anathemancer	|1|B|R
Creature - Zombie Wizard 2/2
When ~ enters the battlefield, it deals damage to target player equal to the number of nonbasic lands that player controls.
Unearth |5|B|R (|5|B|R: Return this card from your graveyard to the battlefield. It gains haste.
Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		etb_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( pick_player_duh(player, card, 1-player, 0) ){
			int count = 0;
			int damage = 0;
			int p = instance->targets[0].player;
			while(count < active_cards_count[p]){
					card_data_t* card_d = get_card_data(p, count);
					if( (card_d->type & TYPE_LAND)  && in_play(p, count) && ! is_basic_land(p, count) ){
						damage++;
					}
					count++;
			}
			damage_player(p, damage, player, card);
		}
	}

	return unearth(player, event, MANACOST_XBR(5, 1, 1));
}

int card_architects_of_will(int player, int card, event_t event){

	/* Architects of Will	|2|U|B
	 * Artifact Creature - Human Wizard 3/3
	 * When ~ enters the battlefield, look at the top three cards of target player's library, then put them back in any order.
	 * Cycling |UB */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		card_instance_t *instance = get_card_instance( player, card);

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE );
		td1.zone = TARGET_ZONE_PLAYERS;
		td1.allow_cancel = 0;

		select_target(player, card, &td1, "Select target player", NULL);
		rearrange_top_x(instance->targets[0].player, player, 3);
	}

	return cycling_hybrid(player, card, event, 1, COLOR_BLUE, COLOR_BLACK, 0);
}

int card_ardent_plea(int player, int card, event_t event){
/* CARD_ID_ARDENT_PLEA	10238
Ardent Plea	|1|W|U
Enchantment
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less.
You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.) */

	exalted(player, card, event, 0, 0);
	cascade(player, card, event);
	return global_enchantment(player, card, event);
}

int card_arsenal_thresher(int player, int card, event_t event){
/*
Arsenal Thresher	|2|WB|U
Artifact Creature - Construct 2/2
As ~ enters the battlefield, you may reveal any number of other artifact cards from your hand.
~ enters the battlefield with a +1/+1 counter on it for each card revealed this way.
*/

	if( (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && affect_me(player, card) &&
		! check_special_flags2(player, card, SF2_FACE_DOWN_DUE_TO_MANIFEST))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card to reveal.");
		test.zone = TARGET_ZONE_HAND;
		test.id = cards_data[iid_draw_a_card].id;
		test.id_flag = DOESNT_MATCH;

		int revealed[hand_count[player]];
		int rc = 0;

		int hand_copy[hand_count[player]];
		int hc = 0;

		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) ){
										hand_copy[hc] = get_original_internal_card_id(p, c);
										hc++;
									}
								}
		);

		if( ! hc ){
			return 0;
		}

		while( rc < hc ){
				int selected = select_card_from_zone(player, player, hand_copy, hc, 0, AI_FIRST_FOUND, -1, &test);
				if( selected != -1 ){
					revealed[rc] = hand_copy[selected];
					rc++;
					hand_copy[selected] = iid_draw_a_card;
				}
				else{
					break;
				}
		}

		if( rc ){
			show_deck(1-player, revealed, rc, "Arsenal Thresher revealed these cards", 0, gs_done_008b40e0 );
			enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, rc);
		}
	}

	return arb_new_hybrid(player, card, event, COLOR_BLUE);
}

/*
Aven Mimeomancer	|1|W|U	0x0000000
Creature - Bird Wizard 3/1
Flying
At the beginning of your upkeep, you may put a feather counter on target creature. If you do, that creature has base power and toughness 3/1 and has flying for as long as it has a feather counter on it.
*/

/*
Bant Sojourners	|1|G|W|U	0x0000000
Creature - Human Soldier 2/4
When you cycle ~ or it dies, you may create a 1/1 |Swhite Soldier creature token.
Cycling |2|W (|2|W, Discard this card: Draw a card.)
*/

int card_bant_sureblade(int player, int card, event_t event)
{
/* CARD_ID_BANT_SUREBLADE	10242
Bant Sureblade	|GU|W
Creature - Human Soldier 2/1
As long as you control another multicolored permanent, ~ gets +1/+1 and has first strike. */

  return alara_reborn_blades(player, card, event, COLOR_WHITE, KEYWORD_FIRST_STRIKE, 0);
}

int card_behemoth_sledge(int player, int card, event_t event){
/* CARD_ID_BEHEMOTH_SLEDGE	10243
Behemoth Sledge	|1|G|W
Artifact - Equipment
Equipped creature gets +2/+2 and has lifelink and trample.
Equip |3 */

	return vanilla_equipment(player, card, event, 3, 2, 2, KEYWORD_TRAMPLE, SP_KEYWORD_LIFELINK);
}

int card_bituminous_blast(int player, int card, event_t event){
/* CARD_ID_BITUMINOUS_BLAST	10244
Bituminous Blast	|3|B|R
Instant
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.)
~ deals 4 damage to target creature. */

	if( ! is_unlocked(player, card, event, 33) ){ return 0; }

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
		if (event == EVENT_CHANGE_TARGET){
			if (check_special_flags2(player, card, SF2_SPELLSKITE)){
				int display[instance->number_of_targets];
				int count = 0;
				for (; count < instance->number_of_targets; ++count){
					target_t target = instance->targets[count];
					display[count] = target.card == -1 ? get_internal_card_id_from_csv_id(target.player == player ? 0x615a : 0x6161) : get_card_instance(target.player, target.card)->internal_card_id;
				}
				return show_deck(player, display, count, " Select the target to change (Rules Engine = you, Deadbox = opponent)", 0, gs_done_008b40e0);
			}
			if (check_special_flags2(player, card, SF2_QUICKSILVER_DRAGON)){
				td.zone = TARGET_ZONE_IN_PLAY;
				td.required_type = TYPE_CREATURE;
			}
			if (check_special_flags2(player, card, SF2_MISDIRECTION) || check_special_flags2(player, card, SF2_REBOUND)){
				td.allow_cancel = 0;
				td.who_chooses = attacking_card_controller;
			}
		}else{
			if (!check_special_flags2(player, card, SF2_MISDIRECTION)){ check_special_flags2(player, card, SF2_REBOUND); }
		}

		if (pick_target(&td, "TARGET_CREATURE")){ do_cascade(player, card, -1); }

		if (event == EVENT_CHANGE_TARGET && instance->targets[0].card != -1){ state_untargettable(instance->targets[0].player, instance->targets[0].card, 0); }
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_blitz_hellion(int player, int card, event_t event)
{
  /* CARD_ID_BLITZ_HELLION	10245
   * Blitz Hellion	|3|R|G
   * Creature - Hellion 7/7
   * Trample, haste
   * At the beginning of the end step, ~'s owner shuffles it into his or her library. */

  haste(player, card, event);
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  shuffle_into_library(player, card);
  return 0;
}

int card_bloodbraid_elf(int player, int card, event_t event)
{
  /* CARD_ID_BLOODBRAID_ELF	10246
Bloodbraid Elf	|2|R|G
Creature - Elf Berserker 3/2
Haste
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less.
You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.) */

  if (event == EVENT_RESOLVE_SPELL)
  {
    if (!is_unlocked(player, card, event, 27) && get_id(player, card) == CARD_ID_BLOODBRAID_ELF)
      kill_card(player, card, KILL_SACRIFICE);
  }
  else
  {
    haste(player, card, event);
    multicascade(player, card, event, 1);
  }
  return 0;
}

/*
Brainbite	|2|U|B	0x0000000
Sorcery
Target opponent reveals his or her hand. You choose a card from it. That player discards that card.
Draw a card.
*/

/*
Breath of Malfegor	|3|B|R	0x0000000
Instant
~ deals 5 damage to each opponent.
*/

int card_captured_sunlight(int player, int card, event_t event){
/* CARD_ID_CAPTURED_SUNLIGHT	10249
Captured Sunlight	|2|G|W
Sorcery
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less.
You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.)
You gain 4 life. */

	cascade(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 4);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_cerodon_yearling(int player, int card, event_t event){
/* CARD_ID_CERODON_YEARLING	10250
Cerodon Yearling	|R|W
Creature - Beast 2/2
Vigilance, haste */

	haste(player, card, event);
	vigilance(player, card, event);
	return 0;
}

int card_cloven_casting(int player, int card, event_t event){
/*
Cloven Casting	|5|U|R
Enchantment
Whenever you cast a multicolored instant or sorcery spell, you may pay |1.
If you do, copy that spell. You may choose new targets for the copy.
*/

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) &&
		! check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST) && has_mana(player, COLOR_ANY, 1) )
	{
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_SPELL);
		this_test.type_flag = F1_NO_CREATURE;
		this_test.color_flag = F3_MULTICOLORED;

		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &this_test) ){
			charge_mana(player, COLOR_COLORLESS, 1);
			if( spell_fizzled != 1 ){
				copy_spell_from_stack(player, trigger_cause_controller, trigger_cause);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_colossal_might(int player, int card, event_t event){
	/* Colossal Might	|R|G
	 * Instant
	 * Target creature gets +4/+2 and gains trample until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 4,2, KEYWORD_TRAMPLE,0);
}

/*
Crystallization	|GU|W	0x0000000
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block.
When enchanted creature becomes the target of a spell or ability, exile that creature.
*/

int card_dauntless_escort(int player, int card, event_t event)
{
  /* Dauntless Escort	|1|G|W
   * Creature - Rhino Soldier 3/3
   * Sacrifice ~: Creatures you control gain indestructible until end of turn. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	pump_creatures_until_eot(player, card, player, 0, 0,0, 0,SP_KEYWORD_INDESTRUCTIBLE, NULL);

  return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_deadshot_minotaur(int player, int card, event_t event)
{
  /* Deadshot Minotaur	|3|R|G
   * Creature - Minotaur 3/4
   * When ~ enters the battlefield, it deals 3 damage to target creature with flying.
   * Cycling |RG */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;
		td.allow_cancel = 0;

		etb_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_abilities = KEYWORD_FLYING;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_FLYING"))
			damage_target0(player, card, 3);
	}

	return cycling_hybrid(player, card, event, 1, COLOR_RED, COLOR_GREEN, 0);
}

int card_deathbringer_thoctar(int player, int card, event_t event){
	/* Deathbringer Thoctar	|4|B|R	0x2002145
	 * Creature - Zombie Beast 3/3
	 * Whenever another creature dies, you may put a +1/+1 counter on ~.
	 * Remove a +1/+1 counter from ~: ~ deals 1 damage to any target. */
	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		card_instance_t *instance = get_card_instance( player, card);
		int amount = instance->targets[11].card;
		while (amount > 0){ add_counters(player, card, player, card, COUNTER_P1_P1, 1); --amount; }
		instance->targets[11].card = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		damage_target0(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_1_1_COUNTER, MANACOST0, 0, &td, "TARGET_ANY");
}

int card_defiler_of_souls(int player, int card, event_t event){

	/* Defiler of Souls	|3|B|B|R
	 * Creature - Demon 5/5
	 * Flying
	 * At the beginning of each player's upkeep, that player sacrifices a monocolored creature. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a monocolored creature to sacrifice.");
		test.color_flag = F3_MONOCOLORED;

		new_sacrifice(player, card, current_turn, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_demonic_dread(int player, int card, event_t event){
	/* Demonic Dread	|1|B|R
	 * Sorcery
	 * Cascade
	 * Target creature can't block this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled !=1 ){
			do_multicascade(player, card, -1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_demonspine_whip(int player, int card, event_t event){

	/* Demonspine Whip	|B|R
	 * Artifact - Equipment
	 * |X: Equipped creature gets +X/+0 until end of turn.
	 * Equip |1 */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_PUMP
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) && generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
		return basic_equipment(player, card, event, 1);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = instance->targets[2].player = 0;
		int can_equip = can_activate_basic_equipment(player, card, event, 1);
		int can_pump = (is_equipping(player, card) && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL) ? 1 : 0);

		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						  "Equip", can_equip, 5,
						  "Pump equipped creature", can_pump, (current_phase == PHASE_AFTER_BLOCKING ? 10 : 0));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EQUIP ){
			basic_equipment(player, card, event, 1);
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}

		if( choice == CHOICE_PUMP ){
			charge_mana_for_activated_ability(player, card, MANACOST0);
			if( spell_fizzled != 1 ){
				charge_mana(player, COLOR_COLORLESS, -1);
				if( spell_fizzled != 1 ){
					instance->targets[2].player = x_value;
					instance->info_slot = choice;
				}
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( instance->info_slot == CHOICE_PUMP ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->damage_target_player, instance->damage_target_card,
							instance->targets[2].player, 0);
		}
	}
	return 0;
}

int card_deny_reality(int player, int card, event_t event){
	/* Deny Reality	|3|U|B
	 * Sorcery
	 * Cascade
	 * Return target permanent to its owner's hand. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		if( spell_fizzled != 1 ){
			do_multicascade(player, card, -1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/*
Double Negative	|U|U|R	0x0000000
Instant
Counter up to two target spells.
*/

/*
Dragon Appeasement	|3|B|R|G	0x0000000
Enchantment
Skip your draw step.
Whenever you sacrifice a creature, you may draw a card.
*/

int card_dragon_broodmother(int player, int card, event_t event){
	/* Dragon Broodmother	|2|R|R|R|G
	 * Creature - Dragon 4/4
	 * Flying
	 * At the beginning of each upkeep, put a 1/1 |Sred and |Sgreen Dragon creature token with flying and devour 2 onto the battlefield. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_DRAGON, &token);
		token.color_forced = COLOR_TEST_RED|COLOR_TEST_GREEN;
		token.pow = 1;
		token.tou = 1;
		token.special_infos = 66;
		generate_token(&token);
	}

	return 0;
}

int card_drastic_revelation(int player, int card, event_t event){
  /* Drastic Revelation	|2|U|B|R
   * Sorcery
   * Discard your hand. Draw seven cards, then discard three cards at random. */

	if( event == EVENT_RESOLVE_SPELL ){
		discard_all(player);
		draw_cards(player, 7);
		multidiscard(player, 3, DISC_RANDOM);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_enigma_sphinx(int player, int card, event_t event){
  /* Enigma Sphinx	|4|W|U|B
   * Artifact Creature - Sphinx 5/4
   * Flying
   * When this creature is put into your graveyard from the battlefield, put it into your library third from the top.
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

	cascade(player, card, event);
	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		int owner, position;
		if (find_in_owners_graveyard(player, card, &owner, &position)){
			int iid = get_grave(owner)[position];
			remove_card_from_grave(owner, position);
			put_iid_under_the_first_x_cards_of_library(owner, iid, 2);
		}
	}
	return 0;
}

int card_enlisted_wurm(int player, int card, event_t event){
  /* Enlisted Wurm	|4|G|W
   * Creature - Wurm 5/5
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

	cascade(player, card, event);

	return 0;
}

/*
Esper Sojourners	|W|U|B	0x0000000
Artifact Creature - Vedalken Wizard 2/3
When you cycle ~ or it dies, you may tap or untap target permanent.
Cycling |2|U (|2|U, Discard this card: Draw a card.)
*/

int card_esper_stormblade(int player, int card, event_t event)
{
  /* Esper Stormblade	|WB|U
   * Artifact Creature - Vedalken Wizard 2/1
   * As long as you control another multicolored permanent, this creature gets +1/+1 and has flying. */

	// As long as you control another multicolored permanent, ~ gets +1/+1 and has flying.
	alara_reborn_blades(player, card, event, 0, KEYWORD_FLYING, 0);

	// Casting cost.  Doesn't use arb_new_hybrid() because any additional color cost added by something else needs to be payable with artifact mana.
	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_multi_a(player, cless, 1, 1, 0, 0, 0) || has_mana_multi_a(player, cless, 0, 1, 0, 0, 1) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int cless = get_updated_casting_cost(player, card, -1, event, -1);
			if( has_mana_multi_a(player, cless, 1, 1, 0, 0, 0) ){
				charge_mana_multi_a(player, 0, 1, 1, 0, 0, 0, cless);
			}
			else{
				charge_mana_multi_a(player, 0, 0, 1, 0, 0, 1, cless);
			}
		}
	}

	return 0;
}

/*
Ethercaste Knight |W|U 	0x20013d5 (Akrosan Squire)
Artifact Creature � Human Knight 1/3
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
*/

/* ManalinkEh 2022: 0x22fcc40; original cards/alara_reborn.c:1026. */
int card_etherium_abomination(int player, int card, event_t event)
{
  /* Etherium Abomination	|3|U|B
   * Artifact Creature - Horror 4/3
   * Unearth |1|U|B (|1|U|B: Return this card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step or if it would leave the battlefield. Unearth only as a sorcery.) */

  int result;
  result = unearth(player, event, 1, 1, 1, 0, 0, 0);
  return result;
}

int card_ethersworn_shieldmage(int player, int card, event_t event){
  /* Ethersworn Shieldmage	|1|W|U
   * Artifact Creature - Vedalken Wizard 2/2
   * Flash
   * When this creature enters, prevent all damage that would be dealt to artifact creatures this turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) && is_what(i, count, TYPE_ARTIFACT) ){
						prevent_all_damage_to_target(player, card, i, count, 1);
					}
					count--;
			}
		}
	}

	return flash(player, card, event);
}

int card_etherwrought_page(int player, int card, event_t event){
	/* Etherwrought Page	|1|W|U|B
	 * Artifact
	 * At the beginning of your upkeep, choose one -
	 * * You gain 2 life.
	 * * Look at the top card of your library. You may put that card into your graveyard.
	 * * Each opponent loses 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int choice = 0;
		int ai_choice = 2;
		if( life[player] < 6 ){
			ai_choice = 0;
		}
		choice = do_dialog(player, player, card, -1, -1, " Gain life\n Look at library\n Opponent loses life", ai_choice);
		if( choice == 0 ){
			gain_life(player, 2);
		}
		if( choice == 1 ){
			if( deck_ptr[player][0] != -1 ){
				if( show_deck(player, deck_ptr[player], 1, "Top card of library", 0, "Into grave?") == -1 )
					mill(player, 1);
			}
		}
		if( choice == 2 ){
			lose_life(1-player, 1);
		}
	}

	return 0;
}

int card_fieldmist_borderpost(int player, int card, event_t event){
  /* Fieldmist Borderpost	|1|W|U
   * Artifact
   * You may pay |1 and return a basic land you control to its owner's hand rather than pay this spell's mana cost.
   * This artifact enters tapped.
   * |T: Add |W or |U. */

	// also code for all the others borderposts

	comes_into_play_tapped(player, card, event);

	if( event != EVENT_MODIFY_COST && !(event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return mana_producer(player, card, event);
	}

	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_LAND);
	this_test.subtype = SUBTYPE_BASIC;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		if( has_mana(player, COLOR_ARTIFACT, get_updated_casting_cost(player, card, -1, event, 1)) && check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
			card_ptr_t* c = cards_ptr[ get_id(player, card) ];
			int cless = get_updated_casting_cost(player, card, -1, event, c->req.req_colorless);
			if( ! has_mana_multi_a(player, cless, c->req.req_black, c->req.req_blue, c->req.req_green, c->req.req_red, c->req.req_white) ){
				infinite_casting_cost();
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1){
			int cless1 = get_updated_casting_cost(player, card, -1, event, 1);
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				if( has_mana(player, COLOR_ARTIFACT, cless1) && check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
					choice = do_dialog(player, player, card, -1, -1, " Play normally\n Pay 1 and bounce a basic land\n Cancel", 1);
				}
			}
			else{
				choice = 1;
			}

			if( choice == 0 ){
				if( instance->info_slot == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
				}
			}
			else if( choice == 1 ){
					if (cless1 > 0){
						charge_mana(player, COLOR_ARTIFACT, cless1);
					}
					if( spell_fizzled != 1 ){
						if( player != AI ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_LAND);
							td.allowed_controller = player;
							td.preferred_controller = player;
							td.illegal_abilities = 0;
							td.required_subtype = SUBTYPE_BASIC;
							if( pick_target(&td, "TARGET_BASICLAND") ){
								bounce_permanent(instance->targets[0].player, instance->targets[0].card);
							}
						}
						else{
							int result = check_battlefield_for_special_card(player, card, player, 2, &this_test);
							if( result != -1 ){
								bounce_permanent(player, result);
							}
							else{
								spell_fizzled = 1;
							}
						}
					}
			}
			else{
				spell_fizzled = 1;
			}
			if (spell_fizzled == 1){
				untap_card_no_event(player, card);
			}
		}
	}

	return 0;
}

/*
Fight to the Death	|R|W	0x0000000
Instant
Destroy all blocking creatures and all blocked creatures.
*/

int card_filigree_angel(int player, int card, event_t event){
  /* Filigree Angel	|5|W|W|U
   * Artifact Creature - Angel 4/4
   * Flying
   * When this creature enters, you gain 3 life for each artifact you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, count_subtype(player, TYPE_ARTIFACT, -1) * 3);
	}

	return 0;
}

int finest_hour_legacy(int player, int card, event_t event){

	if( end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		get_an_additional_combat_phase();
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_finest_hour(int player, int card, event_t event)
{
/*
Finest Hour	|2|G|W|U
Enchantment
Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
Whenever a creature you control attacks alone, if it's the first combat phase of the turn, untap that creature.
		After this phase, there is an additional combat phase.
*/

	if (beginning_of_phase(player, card, event, player, card, PHASE_AFTER_BLOCKING)){
		get_card_instance(player, card)->targets[9].player = 66;
	}
	int attacker = exalted(player, card, event, 0, 0);
	if (attacker >= 0 && get_card_instance(player, card)->targets[9].player < 66){
		untap_card(player, attacker);
		extra_combat_effect(player, card, event, 0, NULL, 0);
	}
	if (eot_trigger(player, card, event)){
		get_card_instance(player, card)->targets[9].player = 0;
	}
	return global_enchantment(player, card, event);
}

int card_flurry_of_wings(int player, int card, event_t event){
	/* Flurry of Wings	|G|W|U
	 * Instant
	 * Put X 1/1 |Swhite Bird Soldier creature tokens with flying onto the battlefield, where X is the number of attacking creatures. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_BIRD_SOLDIER, count_attackers(current_turn));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/*
Giant Ambush Beetle	|3|BG|R	0x0000000
Creature - Insect 4/3
Haste
When ~ enters the battlefield, you may have target creature block it this turn if able.
*/

/*
Glassdust Hulk	|3|W|U	0x0000000
Artifact Creature - Golem 3/4
Whenever another artifact enters the battlefield under your control, ~ gets +1/+1 until end of turn and can't be blocked this turn.
Cycling |WU (|WU, Discard this card: Draw a card.)
*/

int card_glory_of_warfare(int player, int card, event_t event)
{
  /* Glory of Warfare	|2|R|W
   * Enchantment
   * During your turn, creatures you control get +2/+0.
   * During turns other than yours, creatures you control get +0/+2. */

  card_instance_t *instance;
  int result;
  int toughness;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (current_turn == player)
    {
      toughness = 0;
      result = 2;
    }
    else
    {
      toughness = 2;
      result = 0;
    }
    boost_creature_type(player, card, event, ~SUBTYPE_INVALID, result, toughness, 0,
                        BCT_INCLUDE_SELF | BCT_CONTROLLER_ONLY);
  }
  return global_enchantment(player, card, event);
}

/*
Gloryscale Viashino	|1|R|G|W	0x0000000
Creature - Viashino Soldier 3/3
Whenever you cast a multicolored spell, ~ gets +3/+3 until end of turn.
*/

/*
Godtracker of Jund	|1|R|G	0x0000000
Creature - Elf Shaman 2/2
Whenever a creature with power 5 or greater enters the battlefield under your control, you may put a +1/+1 counter on ~.
*/

/*
Gorger Wurm	|3|R|G	0x2008301 (Gluttonous Slime)
Creature - Wurm 5/5
Devour 1 (As this enters the battlefield, you may sacrifice any number of creatures. This creature enters the battlefield with that many +1/+1 counters on it.)
*/

int card_grixis_grimblade(int player, int card, event_t event)
{
  /* Grixis Grimblade	|UR|B
   * Creature - Zombie Warrior 2/1
   * As long as you control another multicolored permanent, this creature gets +1/+1 and has deathtouch. (Any amount of damage it deals to a creature is enough to destroy that creature.) */

  // As long as you control another multicolored permanent, ~ gets +1/+1 and has deathtouch.
  return alara_reborn_blades(player, card, event, COLOR_BLACK, 0, SP_KEYWORD_DEATHTOUCH);
}

/*
Grixis Sojourners	|1|U|B|R	0x0000000
Creature - Zombie Ogre 4/3
When you cycle ~ or it dies, you may exile target card from a graveyard.
Cycling |2|B (|2|B, Discard this card: Draw a card.)
*/

int card_identity_crisis(int player, int card, event_t event){
	/* Identity Crisis	|2|W|W|B|B
	 * Sorcery
	 * Exile all cards from target player's hand and graveyard. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);
			this_test.zone = TARGET_ZONE_HAND;
			new_manipulate_all(player, card, instance->targets[0].player, &this_test, KILL_EXILE);

			int count = count_graveyard(instance->targets[0].player)-1;
			while( count > -1 ){
					rfg_card_from_grave(instance->targets[0].player, count);
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_igneous_pouncer(int player, int card, event_t event)
{
  /* Igneous Pouncer	|4|B|R
   * Creature - Elemental 5/1
   * Haste
   * |H2Swampcycling |2, |H2mountaincycling |2 */
  haste(player, card, event);
  return landcycling(player, card, event, 2, SUBTYPE_SWAMP, SUBTYPE_MOUNTAIN);
}

int card_illusory_demon(int player, int card, event_t event){
  /* Illusory Demon |1|U|B
   * Creature � Demon Illusion 4/3
   * Flying
   * When you cast a spell, sacrifice Illusory Demon. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_intimidation_bolt(int player, int card, event_t event){
	/* Intimidation Bolt	|1|R|W
	 * Instant
	 * ~ deals 3 damage to target creature. Other creatures can't attack this turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
			nobody_can_attack_until_eot(player, card, ANYBODY, instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_jenara_asura_of_war(int player, int card, event_t event){
  /* Jenara, Asura of War |W|U|G
   * Legendary Creature � Angel 3/3
   * Flying
   * {1}{W}: Put a +1/+1 counter on Jenara, Asura of War. */

	check_legend_rule(player, card, event);

	if( event != EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_XW(1, 1), 1, 1, 0, 0);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);
		add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_jhessian_zombies(int player, int card, event_t event)
{
  /* Jhessian Zombies	|4|U|B
   * Creature - Zombie 2/4
   * Fear
   * |H2Islandcycling |2, |H2swampcycling |2 */
	fear(player, card, event);
	return landcycling(player, card, event, 2, SUBTYPE_ISLAND, SUBTYPE_SWAMP);
}

int card_jund_hackblade(int player, int card, event_t event)
{
  /* Jund Hackblade	|BG|R
   * Creature - Goblin Berserker 2/1
   * As long as you control another multicolored permanent, this creature gets +1/+1 and has haste. */

  // As long as you control another multicolored permanent, ~ gets +1/+1 and has haste.
  return alara_reborn_blades(player, card, event, COLOR_RED, 0, SP_KEYWORD_HASTE);
}

/*
Jund Sojourners	|B|R|G	0x0000000
Creature - Viashino Shaman 3/2
When you cycle ~ or it dies, you may have it deal 1 damage to target creature or player.
Cycling |2|R (|2|R, Discard this card: Draw a card.)
*/

int card_karrthus_tyrant_of_jund(int player, int card, event_t event){
  /* Karrthus, Tyrant of Jund |4|B|R|G
   * Legendary Creature � Dragon 7/7
   * Flying, haste
   * When Karrthus, Tyrant of Jund enters the battlefield, gain control of all Dragons, then untap all Dragons.
   * Other Dragon creatures you control have haste. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	boost_subtype(player, card, event, SUBTYPE_DRAGON, 0,0, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_DRAGON;
		int result = new_get_test_score(&test);

		APNAP(p,{
					int c = active_cards_count[p]-1;
					while( c > -1 ){
							if( in_play(p, c) && new_make_test_in_play(p, c, result, &test) ){
								gain_control_permanently(player, p, c);
							}
							c--;
					}
				}
		);

		new_manipulate_all(player, card, player, &test, ACT_UNTAP);
	}

	return 0;
}

int card_kathari_bomber(int player, int card, event_t event){
	/* Kathari Bomber	|1|B|R
	 * Creature - Bird Shaman 2/2
	 * Flying
	 * When ~ deals combat damage to a player, put two 1/1 |Sred Goblin creature tokens onto the battlefield and sacrifice ~.
	 * Unearth |3|B|R */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT+DDBM_MUST_BE_COMBAT_DAMAGE) ){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN, 2);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return unearth(player, event, 3, 1, 0, 0, 1, 0);
}

int card_kathari_remnant(int player, int card, event_t event){
  /* Kathari Remnant	|2|U|B
   * Creature - Bird Skeleton 0/1
   * Flying
   * |B: Regenerate this creature.
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

	cascade(player, card, event);
	return regeneration(player, card, event, MANACOST_B(1));
}

int card_knight_of_new_alara(int player, int card, event_t event){
  /* Knight of New Alara	|2|G|W
   * Creature - Human Knight 2/2
   * Each other multicolored creature you control gets +1/+1 for each of its colors. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player && affected_card != card  ){
			int my_c = count_colors(affected_card_controller, affected_card);
			if( my_c > 1 ){
				event_result+=my_c;
			}
		}
	}

	return 0;
}

int card_knotvine_paladin(int player, int card, event_t event)
{
  /* Knotvine Paladin	|G|W
   * Creature - Human Knight 2/2
   * Whenever this creature attacks, it gets +1/+1 until end of turn for each untapped creature you control. */

	// Whenever ~ attacks, it gets +1/+1 until end of turn for each untapped creature you control.
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY)
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_TAPPED;
		test.state_flag = DOESNT_MATCH;

		int count = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);

		if (count > 0)
			pump_until_eot(player, card, player, card, count, count);
	}

	return 0;
}

int card_lavalanche(int player, int card, event_t event){
	/* Lavalanche	|X|B|R|G	0x2002000
	 * Sorcery
	 * ~ deals X damage to target player or planeswalker and each creature that player or that planeswalker's controller controls. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( instance->info_slot > 0 ){
			damage_target0(player, card, instance->info_slot);
			new_damage_all(player, card, instance->targets[0].player, instance->info_slot, NDA_ALL_CREATURES, NULL);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

// zombie wizard --> rhino token

int card_lich_lord_of_unx(int player, int card, event_t event){
  /* Lich Lord of Unx |1|U|B
   * Creature - Zombie Wizard 2/2
   * {U}{B}, {T}: Put a 1/1 blue and black Zombie Wizard creature token onto the battlefield.
   * {U}{U}{B}{B}: Target player loses X life and puts the top X cards of his or her library into his or her graveyard,
   * 				where X is the number of Zombies you control. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_BU(1, 1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BU(2, 2), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = 0;
		int ai_choice = count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE) >= 4 ? 1 : 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_BU(1, 1), 0, NULL, NULL) ){
			if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BU(2, 2), 0, &td, NULL) ){
				choice = do_dialog(player, player, card, -1, -1, " Make a Zombie Wizard\n Unleash zombie power\n Cancel", ai_choice);
			}
		}
		else{
			choice = 1;
		}
		if( choice == 2 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_BU(choice+1, choice+1)) ){
			if( choice == 0 ){
				tap_card(player, card);
			}
			if( choice == 1 ){
				pick_target(&td, "TARGET_PLAYER");
			}
			instance->info_slot = 66+choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE_WIZARD, &token);
			token.pow = token.tou = 1;
			token.color_forced = COLOR_TEST_BLUE | COLOR_TEST_BLACK;
			generate_token(&token);
		}

		if( instance->info_slot == 67 && valid_target(&td) ){
			mill(instance->targets[0].player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE));
			lose_life(instance->targets[0].player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_ZOMBIE));
		}
	}

	return 0;
}

int card_lightning_reaver(int player, int card, event_t event)
{
  /* Lightning Reaver	|3|B|R
   * Creature - Zombie Beast 3/3
   * Haste; fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
   * Whenever this creature deals combat damage to a player, put a charge counter on it.
   * At the beginning of your end step, this creature deals damage equal to the number of charge counters on it to each opponent. */

  int result;
  haste(player, card, event);
  fear(player, card, event);
  result =
      damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_BE_COMBAT_DAMAGE);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_CHARGE, 1);
  }
  if (((current_turn == player) &&
       (result = count_counters(player, card, COUNTER_CHARGE), 0 < result)) &&
      (result = eot_trigger(player, card, event), result != 0))
  {
    result = count_counters(player, card, COUNTER_CHARGE);
    damage_player(1 - player, result, player, card);
  }
  return 0;
}

int card_lord_of_extinction(int player, int card, event_t event)
{
  /* Lord of Extinction	|3|B|G
   * Creature - Elemental 100/100
   * ~'s power and toughness are each equal to the number of cards in all graveyards. */

  int newval;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    newval = count_graveyard(2);
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_lorescale_coatl(int player, int card, event_t event)
{
  /* Lorescale Coatl	|1|G|U
   * Creature - Snake 2/2
   * Whenever you draw a card, you may put a +1/+1 counter on ~. */

  if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY))
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

/* ManalinkEh 2022: 0x22fe8f0; original cards/alara_reborn.c:1708. */
int card_magefire_wings(int player, int card, event_t event)
{
  /* Magefire Wings	|U|R	0x0000000
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+0 and has flying. */

  int result;
  result = generic_aura(player, card, event, player, 2, 0, 0x20, 0, 0, 0, 0);
  return result;
}

// madrush cyclops --> fervor

int card_maelstrom_nexus(int player, int card, event_t event){
  /* Maelstrom Nexus |W|U|B|R|G
   * Enchantment
   * The first spell you cast each turn has cascade.
   * (When you cast your first spell, exile cards from the top of your library until you exile a nonland card that costs less.
   * You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.) */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( get_specific_storm_count(player) < 2 ){
			if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
				do_cascade(player, card, get_cmc(trigger_cause_controller, trigger_cause));
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_maelstrom_pulse(int player, int card, event_t event){
  /* Maelstrom Pulse	|1|B|G
   * Sorcery
   * Destroy target nonland permanent and all other permanents with the same name as that permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TYPE_LAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0)){
			echoing_effect(player, card, instance->targets[0].player, instance->targets[0].card, &td, KILL_DESTROY );
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nonland permanent", 1, NULL);
}

int card_mage_slayer(int player, int card, event_t event)
{
	/* Mage Slayer	|1|R|G	0x2008450
	 * Artifact - Equipment
	 * Whenever equipped creature attacks, it deals damage equal to its power to the player or planeswalker it's attacking.
	 * Equip |3 */
	if( is_equipping(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		int ep = instance->damage_target_player;
		int ec = instance->damage_target_card;
		store_attackers(player, card, event, DAT_TRACK, ep, ec, NULL);
		dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
		if (event == EVENT_DAT_ABILITY){
			int attacking = planeswalker_its_attacking(ep, ec);
			if (attacking != -2){
				damage_creature(1-current_turn, attacking, get_power(ep, ec), ep, ec);
			}
		}
	}

	if( event == EVENT_ACTIVATE && ! is_equipping(player, card) ){
		ai_modifier+=25;
	}

	return basic_equipment(player, card, event, 3);
}

/* ManalinkEh 2022: 0x22fecc0; original cards/alara_reborn.c:1794. */
int card_marisis_twinclaws(int player, int card, event_t event)
{
  /* Marisi's Twinclaws	|2|RW|G	0x0000000
   * Creature - Cat Warrior 2/4
   * Double strike */

  return arb_new_hybrid(player, card, event, COLOR_GREEN);
}

/* ManalinkEh 2022: 0x22fece0; original cards/alara_reborn.c:1803. */
int card_marrow_chomper(int player, int card, event_t event)
{
  /* Marrow Chomper	|3|B|G	0x0000000
   * Creature - Zombie Lizard 3/3
   * Devour 2 (As this enters the battlefield, you may sacrifice any number of creatures. This creature enters the battlefield with twice that many +1/+1 counters on it.)
   * When ~ enters the battlefield, you gain 2 life for each creature it devoured. */

  card_instance_t *instance;
  devour(player, card, event, 2);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  gain_life(player, instance->targets[1].card * 2);
  return 0;
}

int card_mask_of_riddles(int player, int card, event_t event)
{
  /* Mask of Riddles	|U|B
   * Artifact - Equipment
   * Equipped creature has fear.
   * Whenever equipped creature deals combat damage to a player, you may draw a card.
   * Equip |2 */

	if( is_equipping(player, card) ){
		int packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|
													DDBM_TRIGGER_OPTIONAL);
		if( packets ){
			draw_cards(player, packets);
		}
	}

	return vanilla_equipment(player, card, event, 2, 0, 0, 0, SP_KEYWORD_FEAR);
}

int card_mayaels_aria(int player, int card, event_t event){
/*
Mayael's Aria	|R|G|W
Enchantment
At the beginning of your upkeep, put a +1/+1 counter on each creature you control if you control a creature with power 5 or greater.
Then you gain 10 life if you control a creature with power 10 or greater.
Then you win the game if you control a creature with power 20 or greater. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY || event == EVENT_SHOULD_AI_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 4;
		this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));
		}
		this_test.power = 9;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			gain_life(player, 10);
		}
		this_test.power = 19;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			lose_the_game(1-player);
		}
	}

	return global_enchantment(player, card, event);
}

/*
Messenger Falcons	|2|GU|W	0x0000000
Creature - Bird 2/2
Flying
When ~ enters the battlefield, draw a card.
*/

int mind_funeral_effect(int player, int card, int t_player ){
	int *deck = deck_ptr[t_player];
	int cards_to_mill = 0;
	int lands = 0;
	int x = 0;
	while(deck[x] != -1 && lands < 4){
			if( cards_data[deck[x]].type & TYPE_LAND ){
				lands++;
			}
			cards_to_mill++;
			x++;
	}
	card_ptr_t* c = cards_ptr[ get_id(player, card)  ];
	char buffer[100];
	scnprintf(buffer, 100, " Cards revealed by %s.", c->name);
	show_deck( HUMAN, deck, x, buffer, 0, gs_done_008b40e0 );
	mill(t_player, cards_to_mill);
	return cards_to_mill;
}

int card_mind_funeral(int player, int card, event_t event){
  /* Mind Funeral |1|U|B
   * Sorcery,
   * Target opponent reveals cards from the top of his or her library until four land cards are revealed.
   * That player puts all cards revealed this way into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			mind_funeral_effect(player, card, instance->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_monstrous_carabid(int player, int card, event_t event){

	/* Monstrous Carabid	|3|B|R
	 * Creature - Insect 4/4
	 * ~ attacks each turn if able.
	 * Cycling |BR */

	attack_if_able(player, card, event);

	return cycling_hybrid(player, card, event, 1, COLOR_BLACK, COLOR_RED, 0);
}

static int mycoid_shepherd_effect(int player, int card, event_t event)
{

  // In case something is copying Mycoid Shepherd and then reverts to its original form.
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_MYCOID_SHEPHERD)
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
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    this_test.power = 4;
    this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
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
            // Will trigger also if this is not a creature when it goes into the graveyard;
            if (!is_what(p, c, TYPE_CREATURE) ||
                !new_make_test_in_play(player, card, 0, &this_test))
            {
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            }
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
              instance->targets[11].player = 0;
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, &this_test);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    for (int amount = BYTE0(result); amount > 0; --amount)
      gain_life(player, 5);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

/*
Morbid Bloom	|4|B|G	0x0000000
Sorcery
Exile target creature card from a graveyard, then create X 1/1 |Sgreen Saproling creature tokens, where X is the exiled card's toughness.
*/

int card_mycoid_shepherd(int player, int card, event_t event)
{
  /* Mycoid Shepherd |1|W|G|G
   *   Creature � Fungus 5/4,
   *   Whenever Mycoid Shepherd or another creature you control with power 5 or greater dies, you may
   *   gain 5 life. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (int)mycoid_shepherd_effect)
        {
          if ((inst->damage_target_player == player && inst->damage_target_card == card) ||
              (inst->targets[0].player == player && inst->targets[0].card == card))
          {
            found = 1;
            break;
          }
        }
      }
    }
    if (!found)
    {
      int legacy =
          create_targetted_legacy_effect(player, card, &mycoid_shepherd_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    this_test.power = 4;
    this_test.power_flag = F5_POWER_GREATER_THAN_VALUE;
    count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, &this_test);
  }

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
    if (result)
    {
      int amount = BYTE0(result);
      gain_life(player, amount * 5);
    }
  }

  return 0;
}

int card_naya_hushblade(int player, int card, event_t event)
{
  /* Naya Hushblade	|RW|G
   * Creature - Elf Rogue 2/1
   * As long as you control another multicolored permanent, this creature gets +1/+1 and has shroud. (It can't be the target of spells or abilities.) */

  // As long as you control another multicolored permanent, ~ gets +1/+1 and has shroud.
  return alara_reborn_blades(player, card, event, COLOR_GREEN, KEYWORD_SHROUD, 0);
}

/*
Naya Sojourners	|2|R|G|W	0x0000000
Creature - Elf Shaman 5/3
When you cycle ~ or it dies, you may put a +1/+1 counter on target creature.
Cycling |2|G (|2|G, Discard this card: Draw a card.)
*/

int card_necromancers_covenant(int player, int card, event_t event){
	/* Necromancer's Covenant	|3|W|B|B
	 * Enchantment
	 * When ~ enters the battlefield, exile all creature cards from target player's graveyard,
	 * then put a 2/2 |Sblack Zombie creature token onto the battlefield for each card exiled this way.
	 * Zombies you control have lifelink. */


	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		etb_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			const int *grave = get_grave(instance->targets[0].player);
			int count = count_graveyard(instance->targets[0].player)-1;
			int amount = 0;
			while( count > -1 ){
					if( is_what(-1, grave[count], TYPE_CREATURE) ){
						amount++;
						rfg_card_from_grave(instance->targets[0].player, count);
					}
					count--;
			}

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.qty = amount;
			generate_token(&token);

			instance->number_of_targets = 0;
		}
	}

	if (event == EVENT_ABILITIES && affected_card_controller == player && in_play(player, card) && !is_humiliated(player, card, AL_STATIC) &&
		has_subtype(affected_card_controller, affected_card, SUBTYPE_ZOMBIE) )
	{
		lifelink(affected_card_controller, affected_card, event);
	}

	return global_enchantment(player, card, event);
}

int card_nemesis_of_reason(int player, int card, event_t event)
{
  /* Nemesis of Reason	|3|U|B
   * Creature - Leviathan Horror 3/7
   * Whenever this creature attacks, defending player mills ten cards. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    mill(1-current_turn, 10);
  return 0;
}

int card_nulltread_gargantuan(int player, int card, event_t event){
  /* Nulltread Gargantuan |1|U|G
   * Creature � Beast 5/6
   * When Nulltread Gargantuan enters the battlefield, put a creature you control on top of its owner's library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		if( ! can_target(&td) ){
			ai_modifier-=1000;
		}
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		etb_ability(player, card, event, can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( new_pick_target(&td, "Select a creature you control.", 0, GS_LITERAL_PROMPT) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			instance->number_of_targets = 0;
		}
	}

	return 0;
}

int card_offering_to_asha(int player, int card, event_t event){
	/* Offering to Asha	|2|W|U
	 * Instant
	 * Counter target spell unless its controller pays |4. You gain 4 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		if (counterspell_resolve_unless_pay_x(player, card, NULL, 0, 4)){
			gain_life(player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_pale_recluse(int player, int card, event_t event)
{
  /* Pale Recluse	|4|G|W
   * Creature - Spider 4/5
   * Reach
   * |H2Forestcycling |2, |H2plainscycling |2 */
  return landcycling(player, card, event, 2, SUBTYPE_FOREST, SUBTYPE_PLAINS);
}

/*
Predatory Advantage	|3|R|G	0x0000000
Enchantment
At the beginning of each opponent's end step, if that player didn't cast a creature spell this turn, create a 2/2 |Sgreen Lizard creature token.
*/

int card_putrid_leech(int player, int card, event_t event){

	/* Putrid Leech	|B|G
	 * Creature - Zombie Leech 2/2
	 * Pay 2 life: ~ gets +2/+2 until end of turn. Activate this ability only once each turn. */

	if( (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
		&& generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST0, 2, NULL, NULL) )
	{
		return generic_shade(player, card, event, 2, MANACOST0, 2, 2, 0, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		int pp = instance->parent_controller, pc = instance->parent_card;
		if (in_play(pp, pc)){
			pump_until_eot(pp, pc, pp, pc, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 2, NULL, NULL);
}

int card_qasali_pridemage(int player, int card, event_t event){
  /* Qasali Pridemage	|G|W
   * Creature - Cat Wizard 2/2
   * Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)
   * |1, Sacrifice this creature: Destroy target artifact or enchantment. */

	exalted(player, card, event, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT );

	card_instance_t *instance = get_card_instance( player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td, "DISENCHANT");
}

int card_reborn_hope(int player, int card, event_t event){

	/* Reborn Hope	|G|W
	 * Sorcery
	 * Return target multicolored card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, 0, "Select target multicolored card.");
	this_test.color_flag = F3_MULTICOLORED;

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if (selected != -1){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
}

int card_retaliator_griffin(int player, int card, event_t event){
  /* Retaliator Griffin |1|W|R|G
   * 	  Creature � Griffin 2/2
   * 	  Flying
   * 	  Whenever a source an opponent controls deals damage to you, you may put that many +1/+1 counters on Retaliator Griffin. */

	check_damage(1-player, -1, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_REPORT_DAMAGE_DEALT, player, card);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_REPORT_DAMAGE_DEALT | DDBM_TRIGGER_OPTIONAL,
								player, card) )
	{
		add_counters(player, card, player, card, COUNTER_P1_P1, get_card_instance(player, card)->targets[16].player);
		get_card_instance(player, card)->targets[16].player = 0;
	}

	return 0;
}

int card_sages_of_the_anima(int player, int card, event_t event){

	/* Sages of the Anima	|3|G|U
	 * Creature - Elf Wizard 3/4
	 * If you would draw a card, instead reveal the top three cards of your library. Put all creature cards revealed this way into your hand and the rest on the
	 * bottom of your library in any order. */

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player &&
		!suppress_draw && ! is_humiliated(player, card, AL_TRIG) )
	{
		if( event == EVENT_TRIGGER){
			event_result |= 2u;
		}
		else if( event == EVENT_RESOLVE_TRIGGER ){
				int amount = 3;
				if( count_deck(player) < amount ){
					amount = count_deck(player);
				}
				if( amount > 0 ){
					int *deck = deck_ptr[player];
					show_deck( player, deck, amount, "Sages of the Anima effect", 0, gs_done_008b40e0 );
					int i;
					for(i=(amount-1); i>-1; i--){
						if( is_what(-1, deck[i], TYPE_CREATURE) ){
							add_card_to_hand(player, deck[i]);
							remove_card_from_deck(player, i);
							amount--;
						}
					}
					if( amount > 0 ){
						put_top_x_on_bottom(player, player, amount);
					}
				}
				suppress_draw = 1;
		}
	}

	return 0;
}

int card_sanctum_plowbeast(int player, int card, event_t event)
{
  /* Sanctum Plowbeast	|4|W|U
   * Artifact Creature - Beast 3/6
   * Defender
   * |H2Plainscycling |2, |H2islandcycling |2 */
  return landcycling(player, card, event, 2, SUBTYPE_PLAINS, SUBTYPE_ISLAND);
}

/* ManalinkEh 2022: 0x22fa060; original cards/alara_reborn.c:2383. */
static int sen_triplets_effect(int player, int card, event_t event)
{
  int opponent_card;
  int play_check_or_csv_id;
  card_instance_t *instance;
  int mana_available;
  int opponent;
  int best_play_available;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    player_bits[(1 - player)] = player_bits[(1 - player)] | 0x200;
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_DESTROY);
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {

    opponent = 1 - player;
    if (0 < active_cards_count[opponent])
    {
      best_play_available = 0;
      opponent_card = 0;
      do
      {
        instance = in_hand(opponent, opponent_card);
        if (instance != NULL)
        {

          play_check_or_csv_id = can_legally_play_csvid_now(player, get_id(opponent, opponent_card),
                                                            EVENT_CAN_ACTIVATE);
          if (play_check_or_csv_id != 0)
          {
            mana_available = has_mana_to_cast_iid(
                player, EVENT_CAN_ACTIVATE, get_original_internal_card_id(opponent, opponent_card));
            if (((mana_available != 0) &&
                 (best_play_available = play_check_or_csv_id, play_check_or_csv_id == 99)) &&
                ((play_check_or_csv_id = is_what(opponent, opponent_card, 0x20),
                  play_check_or_csv_id != 0 ||
                      (play_check_or_csv_id = is_what(opponent, opponent_card, 0x22),
                       play_check_or_csv_id != 0))))
            {
              return 99;
            }
          }
        }
        opponent_card = opponent_card + 1;
        if (active_cards_count[opponent] <= opponent_card)
        {
          return best_play_available;
        }
      } while (true);
    }
  }
  else if ((event == EVENT_ACTIVATE) && (opponent = 1 - player, hand_count[opponent] != 0))
  {

    new_default_test_definition(&this_test, 0xff, "Select a card to play.");
    opponent_card = new_select_a_card(player, opponent, 4, 0, 1, -1, &this_test);
    if (((-1 < opponent_card) &&
         ((play_check_or_csv_id = is_what(opponent, opponent_card, 1),
           play_check_or_csv_id == 0 || (((unsigned char)land_can_be_played & 1) == 0)))) &&
        ((play_check_or_csv_id = is_what(opponent, opponent_card, 0x30),
          play_check_or_csv_id != 0 ||
              (play_check_or_csv_id = can_sorcery_be_played(player, EVENT_ACTIVATE),
               play_check_or_csv_id != 0))))
    {
      play_check_or_csv_id = get_id(opponent, opponent_card);
      if (has_mana_to_cast_id(player, EVENT_ACTIVATE, play_check_or_csv_id) != 0)
      {
        play_check_or_csv_id = get_id(opponent, opponent_card);
        charge_mana_from_id(player, -1, EVENT_ACTIVATE, play_check_or_csv_id);
        if ((spell_fizzled == 1) &&
            (play_check_or_csv_id = is_what(opponent, opponent_card, 1), play_check_or_csv_id == 0))
        {
          return 0;
        }
        opponent_plays_card_in_your_hand_for_free(opponent, opponent_card);
        cant_be_responded_to = 1;
        return 0;
      }
    }
    spell_fizzled = 1;
    return 0;
  }
  return 0;
}

/*
Sangrite Backlash	|BG|R	0x0000000
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/-3.
*/

/*
Sanity Gnawers	|1|B|R	0x0000000
Creature - Rat 1/1
When ~ enters the battlefield, target player discards a card at random.
*/

/* ManalinkEh 2022: 0x2300100; original cards/alara_reborn.c:2435. */
int card_sen_triplets(int player, int card, event_t event)
{
  /* Sen Triplets	|2|W|U|B	0x0000000
   * Legendary Artifact Creature - Human Wizard 3/3
   * At the beginning of your upkeep, choose target opponent. This turn, that player can't cast spells or activate abilities and
   * plays with his or her hand revealed. You may play cards from that player's hand this turn. */

  int result;
  test_definition_t test;

  check_legend_rule(player, card, event);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = opponent_is_valid_target(player, card);
    if (result != 0)
    {
      default_test_definition(&test, 0x47);
      new_manipulate_all(player, card, 1 - player, &test, ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
      target_player_cant_cast_type(player, card, 1 - player, 0x7f);
      create_legacy_activate(player, card, sen_triplets_effect);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23001d0; original cards/alara_reborn.c:2456. */
int card_sewn_eye_drake(int player, int card, event_t event)
{
  /* Sewn-Eye Drake	|2|UR|B	0x0000000
   * Creature - Zombie Drake 3/1
   * Flying, haste */

  haste(player, card, event);
  return arb_new_hybrid(player, card, event, COLOR_BLACK);
}

/*
Shield of the Righteous	|W|U	0x0000000
Artifact - Equipment
Equipped creature gets +0/+2 and has vigilance.
Whenever equipped creature blocks a creature, that creature doesn't untap during its controller's next untap step.
Equip |2
*/

int card_sigil_captain(int player, int card, event_t event)
{
  /* Sigil Captain	|1|G|W|W
   * Creature - Rhino Soldier 3/3
   * Whenever a creature you control enters, if that creature is 1/1, put two +1/+1 counters on it. */

  test_definition_t this_test;
  if (((trigger_condition == TRIGGER_COMES_INTO_PLAY) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&this_test, 2);
    this_test.power = 1;
    this_test.toughness = 1;
    etb_ability_global(player, event, 2, TYPE_NONE, &this_test, player, card);
  }
  if (event != EVENT_ETB)
  {
    return 0;
  }
  add_counters(player, card, trigger_cause_controller, trigger_cause, COUNTER_P1_P1, 2);
  return 0;
}

/*
Sigil of the Nayan Gods	|1|G|W	0x0000000
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 for each creature you control.
Cycling |GW (|GW, Discard this card: Draw a card.)
*/

/*
Singe-Mind Ogre	|2|B|R	0x0000000
Creature - Ogre Mutant 3/2
When ~ enters the battlefield, target player reveals a card at random from his or her hand, then loses life equal to that card's converted mana cost.
*/

/*
Skyclaw Thrash	|3|U|R	0x0000000
Artifact Creature - Viashino Warrior 4/4
Whenever ~ attacks, flip a coin. If you win the flip, ~ gets +1/+1 and gains flying until end of turn.
*/

int card_slave_of_bolas(int player, int card, event_t event){
/*
Slave of Bolas	|3|UR|B
Sorcery
Gain control of target creature. Untap that creature. It gains haste until end of turn.
Sacrifice it at the beginning of the next end step.
*/
	modify_cost_for_arb_hybrid_spells(player, card, event, COLOR_BLACK);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
        if (event == EVENT_CHANGE_TARGET){
            if (check_special_flags2(player, card, SF2_SPELLSKITE)){
                int display[instance->number_of_targets];
                int count = 0;
                for (; count < instance->number_of_targets; ++count){
                    target_t target = instance->targets[count];
                    display[count] = target.card == -1 ? get_internal_card_id_from_csv_id(target.player == player ? 0x615a : 0x6161) : get_card_instance(target.player, target.card)->internal_card_id;
                }
                return show_deck(player, display, count, " Select the target to change (Rules Engine = you, Deadbox = opponent)", 0, gs_done_008b40e0);
            }
            if (check_special_flags2(player, card, SF2_QUICKSILVER_DRAGON)){
                td.zone = TARGET_ZONE_IN_PLAY;
                td.required_type = TYPE_CREATURE;
            }
            if (check_special_flags2(player, card, SF2_MISDIRECTION) || check_special_flags2(player, card, SF2_REBOUND)){
                td.who_chooses = attacking_card_controller;
                td.allow_cancel = 0;
            }
        }else{
            if (!check_special_flags2(player, card, SF2_MISDIRECTION)){ check_special_flags2(player, card, SF2_REBOUND); }
        }

		instance->number_of_targets = 0;
		if( arb_hybrid_casting(player, card, event, COLOR_BLACK) ){
			pick_target(&td, "TARGET_CREATURE");
		}
	        if (event == EVENT_CHANGE_TARGET && instance->targets[0].card != -1){ state_untargettable(instance->targets[0].player, instance->targets[0].card, 0); }
    }

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
			haste_and_remove_at_beginning_of_next_end_step(player, card, instance->targets[0].player, instance->targets[0].card, 3);
		}
		kill_card(player, card, KILL_DESTROY );
	}

	return 0;
}

int card_soul_manipulation(int player, int card, event_t event)
{
  /* Soul Manipulation	|1|U|B
   * Instant
   * Choose one or both - Counter target creature spell; and/or return target creature card from your graveyard to your hand. */

  if (!IS_CASTING(player, card, event))
	return 0;

  target_definition_t td;
  counterspell_target_definition(player, card, &td, TYPE_CREATURE);

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
	{
	  if (counterspell(player, card, event, &td, 0))
		return (instance->info_slot = 99);

	  return (instance->info_slot = any_in_graveyard_by_type(player, TYPE_CREATURE) && !graveyard_has_shroud(player));
	}

  if (event == EVENT_CAST_SPELL)
	{
	  instance->targets[2].card = 0;

	  if (instance->info_slot == 99)
		{
		  counterspell(player, card, event, &td, 0);
		  instance->targets[2].card |= 1;
		}

	  if (any_in_graveyard_by_type(player, TYPE_CREATURE) && !graveyard_has_shroud(player))
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
		  if (select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 1) != -1)
			instance->targets[2].card |= 2;
		}

	  cancel = instance->targets[2].card == 0;
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int selected;
	  if ((instance->targets[2].card & 2) && (selected = validate_target_from_grave_source(player, card, player, 1)) != -1)
		from_grave_to_hand(player, selected, TUTOR_HAND);

	  if (instance->targets[2].card & 1)
		counterspell(player, card, event, &td, 0);
	  else
		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_soulquake(int player, int card, event_t event){
  /* Soulquake	|3|U|U|B|B
   * Sorcery
   * Return all creatures on the battlefield and all creature cards in graveyards to their owners' hands. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		APNAP(p, {
					new_manipulate_all(player, card, p, &this_test, ACT_BOUNCE);
					int count = count_graveyard(p)-1;
					const int *grave = get_grave(p);
					while( count > -1 ){
							if( is_what(-1, grave[count], TYPE_CREATURE) ){
								from_grave_to_hand(p, count, TUTOR_HAND);
							}
							count--;
					}
				};
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sovereigns_of_lost_alara(int player, int card, event_t event)
{
  /* Sovereigns of Lost Alara	|4|W|U
   * Creature - Spirit 4/5
   * Exalted
   * Whenever a creature you control attacks alone, you may search your library for an Aura card that could enchant that creature, put it onto the battlefield
   * attached to that creature, then shuffle your library. */

  /* This trigger is the same as exalted()'s; I'm leaving it as-is because the second part should be optional, and I don't yet know how to put two triggers, one
   * optional and one mandatory, on the same card. */
  store_attackers(player, card, event, DAT_ATTACKS_ALONE | DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int attacker = BYTE0(instance->targets[2].player);
	  if (in_play(player, attacker))
		{
			pump_until_eot(player, card, player, attacker, 1, 1);

			if( player == AI || do_dialog(player, player, card, -1, -1, " Tutor an Aura\n Pass", 0) == 0){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an Aura card with enchant creature.");
				this_test.subtype = SUBTYPE_AURA_CREATURE;

				int result = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_CMC, &this_test);
				if (result > -1){
					put_into_play_aura_attached_to_target(player, result, player, attacker);
				}
			}
		}
	}

  return 0;
}

int card_spellbound_dragon(int player, int card, event_t event)
{
  /* Spellbound Dragon	|3|U|R
   * Creature - Dragon 3/5
   * Flying
   * Whenever this creature attacks, draw a card, then discard a card. This creature gets +X/+0 until end of turn, where X is the discarded card's mana value. */

  /* Whenever ~ attacks, draw a card, then discard a card. ~ gets +X/+0 until end of turn, where X is the discarded card's converted mana cost. */
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY)
	{
		test_definition_t this_test;
		default_test_definition(&this_test, 0);

		draw_a_card(player);

		if (hand_count[player] > 0)
		{
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MAX_CMC, -1, &this_test);
			int cmc = get_cmc(player, selected);
			discard_card(player, selected);
			pump_until_eot(player, card, player, card, cmc, 0);
		}
	}

	return 0;
}

int card_spellbreaker_behemoth(int player, int card, event_t event){
  /* Spellbreaker Behemoth |1|R|G|G
   * Creature � Beast 5/5
   * Spellbreaker Behemoth can't be countered.
   * Creature spells you control with power 5 or greater can't be countered. */

	cannot_be_countered(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 4;
		this_test.power_flag = 2;

		type_uncounterable(player, card, event, player, 0, &this_test);
	}

	return 0;
}

int card_sphinx_steel_wind(int player, int card, event_t event){
  /* Sphinx of the Steel Wind	|5|W|U|B
   * Artifact Creature - Sphinx 6/6
   * Flying, first strike, vigilance, lifelink, protection from red and from green */

	lifelink(player, card, event);

	vigilance(player, card, event);

	return 0;
}

/*
Stormcaller's Boon	|2|W|U	0x0000000
Enchantment
Sacrifice ~: Creatures you control gain flying until end of turn.
Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
*/

int card_stun_sniper(int player, int card, event_t event){
	/* Stun Sniper	|R|W
	 * Creature - Human Archer 1/1
	 * |1, |T: ~ deals 1 damage to target creature. Tap that creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, instance->parent_card);
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_tainted_sigil(int player, int card, event_t event){
  /* Tainted Sigil	|1|W|B
   * Artifact
   * |T, Sacrifice this artifact: You gain life equal to the total life lost by all players this turn. (Damage causes loss of life.) */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, get_trap_condition(player, TRAP_LIFE_LOST)+get_trap_condition(1-player, TRAP_LIFE_LOST));
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME|GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_thopter_foundry(int player, int card, event_t event){
/* Thopter Foundry	|WB|U
 * Artifact
 * |1, Sacrifice a nontoken artifact: Put a 1/1 |Sblue Thopter artifact creature token with flying onto the battlefield. You gain 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return arb_new_hybrid(player, card, event, COLOR_BLUE);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select a nontoken artifact to sacrifice.");
	test.type_flag = F1_NO_TOKEN;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			if( new_can_sacrifice_as_cost(player, card, &test) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);

		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_THOPTER, &token);
		token.color_forced = COLOR_TEST_BLUE;
		generate_token(&token);
	}

	return 0;
}

int card_thought_hemorrhage(int player, int card, event_t event){
/* Thought Hemorrhage	|2|B|R
 * Sorcery
 * Name a nonland card. Target player reveals his or her hand. ~ deals 3 damage to that player for each card with that name revealed
 * this way. Search that player's graveyard, hand, and library for all cards with that name and exile them.
 * Then that player shuffles his or her library. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

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
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Choose a nonland card name.");
				test.type_flag = DOESNT_MATCH;
				csvid = new_card_from_list(player, 3, &test);
			}
			if( csvid > -1 ){
				amount = lobotomy_effect(player, instance->targets[0].player, csvid, -1, LOBOTOMY_MUST_FIND_ALL_COPIES | LOBOTOMY_REPORT_ONLY_EXILED_FROM_HAND);
			}
			damage_player(instance->targets[0].player, amount*3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_thraximundar(int player, int card, event_t event){
	/* Thraximundar	|4|U|B|R
	 * Legendary Creature - Zombie Assassin 6/6
	 * Haste
	 * Whenever ~ attacks, defending player sacrifices a creature.
	 * Whenever a player sacrifices a creature, you may put a +1/+1 counter on ~. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		player_sacrifices_a_creature(player, card, 1-current_turn);
	}

	if (whenever_a_player_sacrifices_a_permanent(player, card, event, ANYBODY, TYPE_CREATURE, RESOLVE_TRIGGER_AI(player))){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_time_sieve(int player, int card, event_t event){
  /* Time Sieve |U|B
   * Artifact
   * {T}, Sacrifice five artifacts: Take an extra turn after this one. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
	test.qty = 5;

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			if( new_can_sacrifice_as_cost(player, card, &test) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			int sac[5] = {0};
			int good = 1;
			test.qty = 1;
			int i;
			for(i=0; i<5; i++){
				sac[i] = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
				if( ! sac[i] ){
					good = 0;
					remove_state(player, card, STATE_TAPPED);
					cancel = 1;
					break;
				}
			}
			for(i=0; i<5; i++){
				if( sac[i] ){
					spell_fizzled = 1;
					remove_state(player, card, STATE_TAPPED);
					state_untargettable(BYTE2(sac[i]), BYTE3(sac[i]), 0);
				}
			}
			if( good ){
				for(i=0; i<5; i++){
					if( sac[i] ){
						kill_card(BYTE2(sac[i]), BYTE3(sac[i]), KILL_SACRIFICE);
					}
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		time_walk_effect(instance->parent_controller, instance->parent_card);
	}

	return 0;
}

/*
Trace of Abundance	|RW|G	0x0000000
Enchantment - Aura
Enchant land
Enchanted land has shroud. (It can't be the target of spells or abilities.)
Whenever enchanted land is tapped for mana, its controller adds one mana of any color to his or her mana pool (in addition to the mana the land produces).
*/

int card_unbender_tine(int player, int card, event_t event){
  /* Unbender Tine |2|W|U
   * Artifact
   * {T}: Untap another target permanent. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.special = TARGET_SPECIAL_NOT_ME;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select another target permanent.");
}

int card_unscythe_killer_of_kings(int player, int card, event_t event){
/* Unscythe, Killer of Kings	|U|B|B|R
 * Legendary Artifact - Equipment
 * Equipped creature gets +3/+3 and has first strike.
 * Whenever a creature dealt damage by equipped creature this turn dies, you may exile that card.
 *		If you do, put a 2/2 |Sblack Zombie creature token onto the battlefield.
 * Equip |2 */

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( is_equipping(player, card) ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		card_instance_t *damage = damage_being_dealt(event);
		if( damage ){
			if( damage->damage_source_card == p && damage->damage_source_player == c ){
				int i;
				for(i=1; i<10; i++){
					if( instance->targets[i].player == -1 ){
						SET_BYTE0(instance->targets[i].player) = damage->damage_target_player;
						instance->targets[i].card = damage->damage_target_card;
						break;
					}
				}
			}
		}

		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			if( !check_for_death_trigger_removal(affected_card_controller, affected_card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
				is_this_dying(affected_card_controller, affected_card) )
			{
				int i;
				for(i=1; i<10; i++){
					if( instance->targets[i].player != -1 ){
						if( BYTE0(instance->targets[i].player) == affected_card_controller && instance->targets[i].card == affected_card ){
							SET_BYTE0(instance->targets[i].player) = get_owner(affected_card_controller, affected_card);
							SET_BYTE1(instance->targets[i].player) = 1;
							instance->targets[i].card = get_original_internal_card_id(affected_card_controller, affected_card);
							if( instance->targets[11].player < 0 ){
								instance->targets[11].player = 0;
							}
							instance->targets[11].player++;
							break;
						}
					}
				}
			}
		}

		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			int to_be_exiled[2][9] = {{-1}};
			int tbec = 0;
			int i;
			for(i=1; i<10; i++){
				if( instance->targets[i].player > -1 && BYTE1(instance->targets[i].player) == 1 ){
					to_be_exiled[0][i] = BYTE0(instance->targets[i].player);
					to_be_exiled[1][i] = instance->targets[i].card;
					tbec++;
				}
			}

			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a card to exile.");

			int amount = 0;
			while( tbec ){
					int selected = select_card_from_zone(player, player, to_be_exiled[1], tbec, 0, AI_MAX_VALUE, -1, &test);
					if( selected != -1 ){
						int x;
						int owner = to_be_exiled[0][selected];
						int iid = to_be_exiled[1][selected];
						for( x = count_graveyard(owner)-1; x > -1; x-- ){
							if( get_grave(owner)[x] == iid ){
								rfg_card_from_grave(owner, x);
								amount++;
								int y;
								for(y=selected; y<tbec-1; y++){
									to_be_exiled[0][y] = to_be_exiled[0][y+1];
									to_be_exiled[1][y] = to_be_exiled[1][y+1];
								}
								tbec--;
								break;
							}
						}
					}
					else{
						break;
					}
			}
			generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, amount * (card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1));
			instance->targets[11].player = 0;
		}
	}

	if( event == EVENT_CLEANUP ){
		instance->targets[2].player = 0;
	}

	return vanilla_equipment(player, card, event, 2, 3, 3, KEYWORD_FIRST_STRIKE, 0);
}

int card_uril_the_miststalker(int player, int card, event_t event ){
  /* Uril, the Miststalker	|2|R|G|W
   * Legendary Creature - Beast 5/5
   * Hexproof (This creature can't be the target of spells or abilities your opponents control.)
   * Uril gets +2/+2 for each Aura attached to it. */

	hexproof(player, card, event);

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && in_play(player, card) &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		event_result += count_auras_enchanting_me(player, card) * 2;
	}

	return 0;
}

int card_valley_rannet(int player, int card, event_t event)
{
  /* Valley Rannet	|4|R|G
   * Creature - Beast 6/3
   * |H2Mountaincycling |2, |H2forestcycling |2 */
  return landcycling(player, card, event, 2, SUBTYPE_MOUNTAIN, SUBTYPE_FOREST);
}

/*
Vectis Dominator	|1|W|B	0x0000000
Artifact Creature - Human Wizard 0/2
|T: Tap target creature unless its controller pays 2 life.
*/

/* ManalinkEh 2022: 0x2302190; original cards/alara_reborn.c:3187. */
int card_vedalken_ghoul(int player, int card, event_t event)
{
  /* Vedalken Ghoul	|U|B	0x0000000
   * Creature - Vedalken Zombie 1/1
   * Whenever ~ becomes blocked, defending player loses 4 life. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          lose_life(1 - current_turn, 4);
        }
      }
    }
  }
  return 0;
}

int card_vedalken_heretic(int player, int card, event_t event){
  /* Vedalken Heretic |U|G
   * Creature � Vedalken Rogue 1/1
   * Whenever Vedalken Heretic deals damage to an opponent, you may draw a card. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_TRIGGER_OPTIONAL) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_vengeful_rebirth(int player, int card, event_t event){
	/* Vengeful Rebirth	|4|R|G	0x20084c3
	 * Sorcery
	 * Return target card from your graveyard to your hand. If you return a nonland card to your hand this way, ~ deals damage equal to that card's converted mana cost to any target.
	 * Exile ~. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select target card.");

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td) ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
		}
	}

	if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET){
		get_card_instance( player, card )->number_of_targets = 0;
		int selected = select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 1);
		if( selected != -1 ){
			pick_target(&td, "TARGET_ANY");
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			int iid = get_grave(player)[selected];
			int cmc = is_what(-1, iid, TYPE_LAND) ? -1 : get_cmc_by_id(cards_data[iid].id);
			from_grave_to_hand(player, selected, TUTOR_HAND);
			if (cmc >= 0 && valid_target(&td)){
				damage_target0(player, card, cmc);
			}
			kill_card(player, card, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_violent_outburst(int player, int card, event_t event){
  /* Violent Outburst	|1|R|G
   * Instant
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)
   * Creatures you control get +1/+0 until end of turn. */

	cascade(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// vithian renegades --> uktabi orangutan

int card_wargate(int player, int card, event_t event){
  /* Wargate	|X|G|W|U
   * Sorcery
   * Search your library for a permanent card with mana value X or less, put it onto the battlefield, then shuffle. */

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );
		x_value = 0; //For avoiding problems with permanents with X in the CMC cost.
		char msg[100];
		scnprintf(msg, 100, "Select a permanent card with CMC %d or less", instance->info_slot);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, msg);
		this_test.cmc = instance->info_slot+1;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_winged_coatl(int player, int card, event_t event){
  /* Winged Coatl	|1|G|U
   * Creature - Snake 1/1
   * Flash (You may cast this spell any time you could cast an instant.)
   * Flying
   * Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.) */

	deathtouch(player, card, event);
	return flash(player, card, event);
}

int card_zealous_persecution(int player, int card, event_t event)
{
  /* Zealous Persecution	|W|B
   * Instant
   * Until end of turn, creatures you control get +1/+1 and creatures your opponents control get -1/-1. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    for (int pass = 0; pass < 2; ++pass)
    {
      int p = current_turn ^ pass;
      if (p == player)
        pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
      if (p == 1-player)
        pump_subtype_until_eot(player, card, 1-player, -1, -1, -1, 0, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}
