#include "manalink.h"

void default_token_definition(int player, int card, int id, token_generation_t *token)
{
  token->id = id;
  token->eff_player = player;
  token->eff_card = card;
  token->t_player = player;

  if (player >= 0 && card >= 0)	// Find the real source if this was given either an activation card or an effect card
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->internal_card_id == activation_card)
		{
		  player = instance->parent_controller;
		  card = instance->parent_card;
		}
	  else if (instance->internal_card_id >= 0)	// No need to check backup_internal_card_id; if it's tokens from an effect card that's already left play, it's always a bug
		{
		  int csvid = cards_data[instance->internal_card_id].id;
		  if (csvid >= 901 && csvid <= 908)
			{
			  player = instance->damage_source_player;
			  card = instance->damage_source_card;
			}
		}
	}
  if (player >= 0 && card >= 0)
	{
	  token->s_player = player;
	  token->s_card = card;
	  token->no_sleight = 0;
	}
  else
	{
	  token->s_player = token->s_card = -1;
	  token->no_sleight = 1;
	}

  token->qty = 1;
  token->pow = -1;
  token->tou = -1;
  token->key_plus = 0;
  token->s_key_plus = 0;
  token->color_forced = 0;
  token->special_infos = 0;
  token->action = 0;
  token->action_argument = 0;
  token->legacy = 0;
  token->special_code_for_legacy = &empty;
  token->special_code_on_generation = NULL;
  token->keep_track_of_tokens_generated = 0;
  token->special_flags2 = 0;
}

static int get_copyable_special_infos(int t_player, int t_card);

// Create a token definition that's a copy of (t_player,t_card).
void copy_token_definition(int player, int card, token_generation_t* token, int t_player, int t_card)
{
  int csvid = get_id(t_player, t_card);
  default_token_definition(player, card, csvid, token);
  token->no_sleight = 1;

  if (cards_ptr[csvid]->card_type == 8)	// A token defined in manalink.csv
	{
	  card_instance_t* instance = get_card_instance(t_player, t_card);
	  if (check_special_flags(t_player, t_card, SF_DONT_COPY_TOKEN_SOURCE))
		token->s_player = token->s_card = -1;
	  else
		{
		  token->s_player = instance->damage_source_player;
		  token->s_card = instance->damage_source_card;
		}
	  token->pow = instance->targets[5].player;
	  token->tou = instance->targets[5].card;
	  token->key_plus = instance->targets[6].player;
	  token->s_key_plus = instance->targets[7].player;
	  token->color_forced = instance->targets[7].card;
	  token->special_flags2 = instance->targets[8].card;
	  token->special_infos = get_copyable_special_infos(t_player, t_card);
		if( check_special_flags2(t_player, t_card, SF2_ENCHANTED_EVENING) ){
			token->special_flags2 = SF2_ENCHANTED_EVENING;
		}
	}
}

int copy_token_characteristics_for_clone(int player, int card, int t_player, int t_card)
{
  int csvid = get_id(t_player, t_card);
  if (cards_ptr[csvid]->card_type == 8)	// A token defined in manalink.csv
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  card_instance_t* token = get_card_instance(t_player, t_card);

	  int i;
	  for (i = 4; i <= 8; ++i)
		instance->targets[i] = token->targets[i];	// struct copy

	  if (instance->targets[7].card > 0)
		instance->initial_color = instance->targets[7].card & COLOR_TEST_ANY_COLORED;

	  instance->color = instance->initial_color & COLOR_TEST_ANY_COLORED;
	  instance->regen_status |= KEYWORD_RECALC_SET_COLOR;

	  set_special_infos(player, card, get_copyable_special_infos(t_player, t_card));

		if( check_special_flags2(t_player, t_card, SF2_ENCHANTED_EVENING) ){
			set_special_flags2(player, card, SF2_ENCHANTED_EVENING);
		}

	  return 1;
	}
  else
	return 0;
}

/* Just like generic_token(), but supports storing the values on an effect card to set them on a different permanent.  The idea is that changing a permanent to
 * temporarily be a copy of a token shouldn't have to overwrite all of its targets. */
int token_characteristic_setting_effects(int player, int card, event_t event, int t_player, int t_card){
	if (card == -1){
		return 0;
	}
	if (check_special_flags2(player, card, SF2_TEMPORARY_COPY_OF_TOKEN)){
		return 0;
	}
	card_instance_t* instance = get_card_instance(player, card);
	if( event == EVENT_POWER && affect_me(t_player, t_card) && instance->targets[5].player >= 0 ){
		event_result += instance->targets[5].player - get_base_power(player, card);
	}
	if( event == EVENT_TOUGHNESS && affect_me(t_player, t_card) && instance->targets[5].card >= 0 ){
		event_result += instance->targets[5].card - get_base_toughness(player, card);
	}
	if( event == EVENT_ABILITIES && affect_me(t_player, t_card) && instance->targets[6].player > 0 ){
		event_result |= instance->targets[6].player;
	}
	if( event == EVENT_SET_COLOR && affect_me(t_player, t_card) ){
		if (!(card == t_card && player == t_player)){
			if (instance->targets[7].card > 0){
				event_result = instance->targets[7].card & COLOR_TEST_ANY_COLORED;
			}
		}	// otherwise handled by setting initial_color in set_token()
	}
	if( instance->targets[7].player > 0 && instance->targets[7].player > 0 ){
		// Deliberately use the attached-to card as the effect source, not the attachment
		special_abilities(t_player, t_card, event, instance->targets[7].player, t_player, t_card);
	}
	if (event == EVENT_CHANGE_TYPE && affect_me(t_player, t_card) && instance->targets[8].card > 0){
		if (!(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS)){
			// So there's a chance to set SF2_TEMPORARY_COPY_OF_TOKEN.
			land_can_be_played |= LCBP_NEED_EVENT_CHANGE_TYPE_SECOND_PASS;
		} else {
			set_special_flags2(t_player, t_card, instance->targets[8].card);
		}
	}
	return 0;
}

int generic_token(int player, int card, event_t event){
	return token_characteristic_setting_effects(player, card, event, player, card);
}

static void set_token(int player, int card, token_generation_t *token ){
	card_instance_t *instance = get_card_instance(player, card);
	instance->damage_source_player = token->s_player;
	instance->damage_source_card = token->s_card;
	instance->targets[5].player = token->pow;
	instance->targets[5].card = token->tou;
	instance->targets[6].player = token->key_plus;
	// instance->targets[6].card unused
	instance->targets[7].player = token->s_key_plus;
	// instance->targets[7].card set below
	instance->targets[8].card = token->special_flags2;

	color_test_t forced = token->color_forced;
	int sleight = token->s_player >= 0 && token->s_card >= 0 && !token->no_sleight && !(forced > 0 && (forced & 0x80));
	if (forced <= 0){
		forced = get_card_instance(player, card)->color;
	}
	if (sleight){
		forced = get_sleighted_color_test(token->s_player, token->s_card, forced & COLOR_TEST_ANY_COLORED);
	} else {
		forced &= COLOR_TEST_ANY_COLORED;
	}

	instance->initial_color = forced;
	instance->color = forced;
	instance->regen_status |= KEYWORD_RECALC_SET_COLOR;

	instance->targets[7].card = forced | 0x80;

	set_special_flags2(player, card, token->special_flags2);

	convert_to_token(player, card);
	set_special_infos(player, card, token->special_infos);
}

void convert_to_token(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	instance->token_status |= STATUS_TOKEN;
}

static int get_updated_tokens_number(int player, int number)
{
  card_instance_t* instance;
  int p, c;
  for (p = 0; p <= 1; ++p)
	for (c = 0; c < active_cards_count[p]; ++c)
	  if ((instance = in_play(p, c)))
		switch (cards_data[instance->internal_card_id].id)
		  {
			case CARD_ID_DOUBLING_SEASON:		// self only: doubles
			case CARD_ID_PARALLEL_LIVES:
			case CARD_ID_ANOINTED_PROCESSION:
			  if (p != player)
				continue;
			  // else fall through
			case CARD_ID_SELESNYA_LOFT_GARDENS:	// everyone: doubles
			case CARD_ID_PRIMAL_VIGOR:
			  number *= 2;
			  break;
		  }
  return number;
}

static int is_reserved_id(int csvid){
	int iid = get_internal_card_id_from_csv_id(csvid);
	return (cards_data[iid].cc[2] == 3	// vanguard card, edh, etc.
			|| cards_data[iid].type == TYPE_EFFECT);
}

void set_special_infos(int player, int card, int infos){
	card_instance_t *instance = get_card_instance(player, card);
	instance->targets[14].player = infos;
}

int get_special_infos(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[14].player > -1 ){
		return instance->targets[14].player;
	}
	return 0;
}

static int get_copyable_special_infos(int t_player, int t_card)
{
  /* Augh.  Most special_infos are characteristic of the token, and so should be copied; some are added by the effect putting it into play, and so shouldn't.
   * But many of the latter are on token function that are shared among multiple ids and so are prohibitively difficult to test for; so only allow special_infos
   * that are whitelisted here. */
  int special_infos = get_special_infos(t_player, t_card);
  if (special_infos)
	switch (get_backup_id(t_player, t_card))
	  {
		case CARD_ID_ASSASSIN:
		  if (special_infos != 66)	// Vraska the Unseen assassin: has "Whenever this creature deals combat damage to a player, that player loses the game."
			special_infos = 0;
		  break;
		case CARD_ID_ASSEMBLY_WORKER:
		  if (special_infos != 66)	// Urza's Factory assembly-worker: does *not* have Mishra's Factory assembly-worker abilities
			special_infos = 0;
		  break;
		case CARD_ID_BAT:
		  if (special_infos != 66)	// Sengir Nosferatu bat: has "1B, Sacrifice this creature: Return an exiled card named Sengir Nosferatu to the battlefield under its owner's control."
			special_infos = 0;
		  break;
		case CARD_ID_SPIRIT:
		  if (special_infos != 66)	// Daxos the Returned bird : has "This creature's power and toughness are each equal to the number of experience counters you have."
			special_infos = 0;
		  break;
		case CARD_ID_CLERIC:
		  if (special_infos != 66)	// Deathpact Angel cleric: has "3WBB, T, Sacrifice this creature: Return a card named Deathpact Angel from your graveyard to the battlefield."
			special_infos = 0;
		  break;
		case CARD_ID_DRAGON:
		  if (special_infos != 66 // Dragon Broodmother dragon: has Devour 2
			  && special_infos != 67)	// Dragon Egg dragon: has "R: This creature gets +1/+0 until end of turn."
			special_infos = 0;
		  break;
		case CARD_ID_ELDRAZI_SPAWN:
		  if (special_infos != 66)	// Hedron Fields of Agadeem eldrazi: has annihilator 1; does *not* have eldrazi spawn mana generation.
			special_infos = 0;
		  break;
		case CARD_ID_ELEMENTAL:
		  if (special_infos != 67)	// Voice of Resurgence elemental: has "This creature's power and toughness are each equal to the number of creatures you control."
			special_infos = 0;
		  break;
		case CARD_ID_GOBLIN:
		  if (special_infos != 67)	// Goblin Assault goblin: has haste
			special_infos = 0;
		  break;
		case CARD_ID_ILLUSION_TOKEN:
		  if (special_infos != 66)	// Summoner's Bane illusion: does *not* have flying.
			special_infos = 0;
		  break;
		case CARD_ID_INSECT:
		  if (special_infos != 67)	// Carrion Call/Phyrexian Swarmlord/Trigon of Infestation insect: has infect
			special_infos = 0;
		  break;
		case CARD_ID_OOZE:
		  if (special_infos != 66	// Gutter Grime ooze: has "This creature's power and toughness are each equal to the number of slime counters on Gutter Grime."
			  && special_infos != 67)	// Mitotic Slime ooze: has "When this creature dies, put two 1/1 green Ooze creature tokens onto the battlefield."
			special_infos = 0;
		  break;
		case CARD_ID_SAPROLING:
		  if (special_infos != 66)	// Saproling Burst saproling: has "This creature's power and toughness are each equal to the number of fade counters on Saproling Burst."
			special_infos = 0;
		  break;
		case CARD_ID_WOLF:
		  if (special_infos != 66)	// Sound the Call wolf: has "This creature gets +1/+1 for each card named Sound the Call in each graveyard."
			special_infos = 0;
		  break;
		default:
		  special_infos = 0;
	  }
  return special_infos;
}

static int real_generate_token(int player, int int_id, token_generation_t *token, int delay_put_into_play){
	int card_added = add_card_to_hand( player, int_id );
	set_token(player, card_added, token);
	if (delay_put_into_play){
		get_card_instance(player, card_added)->state |= STATE_INVISIBLE;
		--hand_count[player];
	} else {
		put_into_play(player, card_added);
	}
	return card_added;
}

void generate_token(token_generation_t *token){
	int int_id = get_internal_card_id_from_csv_id(token->id);
	if(!(cards_data[int_id].type & TYPE_ARTIFACT) && (token->action & TOKEN_ACTION_CONVERT_INTO_ARTIFACT)){// not is_what() - deliberately ignore SF2_MYCOSYNTH_LATTICE
		int_id = create_a_card_type(int_id);
		cards_data[int_id].type |= TYPE_ARTIFACT;
	}
	int number = token->qty;
	if( ! is_what(-1, int_id, TYPE_EFFECT) ){
		number = get_updated_tokens_number(token->t_player, number);
	}

	int i;
	int card_added = -1;

	ASSERT(token->eff_card != -1 || !(token->keep_track_of_tokens_generated > 0 || token->legacy == 1 ||
									  (token->action & (TOKEN_ACTION_EQUIP | TOKEN_ACTION_HASTE | TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_PUMP_TOUGHNESS))));

	card_instance_t *source = token->keep_track_of_tokens_generated ? get_card_instance(token->eff_player, token->eff_card) : NULL;
	for( i = 0; i < number; i++){
		card_added = real_generate_token(token->t_player, int_id, token, 1);

		card_instance_t* instance = get_card_instance(token->t_player, card_added);

		if (token->legacy == 1){
			create_targetted_legacy_effect(token->eff_player, token->eff_card, token->special_code_for_legacy, token->t_player, card_added);
		}

		if (token->action & TOKEN_ACTION_TAPPED){
			instance->state |= STATE_TAPPED;	// Not tap_card() to avoid an EVENT_TAP_CARD
		}
		if (token->action & TOKEN_ACTION_ATTACKING_UNTAPPED){
			instance->state |= STATE_ATTACKING;
			choose_who_attack(token->t_player, card_added);
		}
		if (token->action & TOKEN_ACTION_BLOCKING){
			card_instance_t* to_block = get_card_instance(1 - token->t_player, token->action_argument);
			instance->blocking = to_block->blocking == 255 ? token->action_argument : to_block->blocking;
			instance->state |= STATE_UNKNOWN8000|STATE_BLOCKING;
		}
		if ((token->action & TOKEN_ACTION_EQUIP) && i == 0){
			equip_target_creature(token->eff_player, token->eff_card, token->t_player, card_added);
		}
		if (token->action & TOKEN_ACTION_DONT_COPY_TOKEN_SOURCE){
			set_special_flags(token->t_player, card_added, SF_DONT_COPY_TOKEN_SOURCE);
		}
		if (token->action & (TOKEN_ACTION_HASTE | TOKEN_ACTION_PUMP_POWER | TOKEN_ACTION_PUMP_TOUGHNESS)){
			pump_ability_until_eot(token->eff_player, token->eff_card, token->t_player, card_added,
								   (token->action & TOKEN_ACTION_PUMP_POWER) ? token->action_argument : 0,
								   (token->action & TOKEN_ACTION_PUMP_TOUGHNESS) ? token->action_argument : 0,
								   0,
								   (token->action & TOKEN_ACTION_HASTE) ? SP_KEYWORD_HASTE : 0);
		}
		if (token->action & TOKEN_ACTION_ADD_SUBTYPE){
			add_a_subtype(token->t_player, card_added, token->action_argument);
		}

		if (token->special_code_on_generation){
			(*token->special_code_on_generation)(token, card_added, i);
		}

		if( (int)token->keep_track_of_tokens_generated > i ){
			source->targets[i].player = token->t_player;
			source->targets[i].card = card_added;
		}

		put_into_play(token->t_player, card_added);

		if ((token->action & TOKEN_ACTION_BLOCKING) && in_play(token->t_player, card_added)){
			play_sound_effect(WAV_BLOCK2);
			if (event_flags & EA_SELECT_BLOCK){
				dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0/*PROMPT_BLOCKERSELECTION[0]*/, 0, token->t_player, card_added);
			}
		}

		// legendary tokens
		if( token->special_infos == 99 ){
			// I'm almost certain this is no longer used.
			int p;
			int kill_me = 0;
			int leg_id = get_id(token->t_player, card_added);
			for(p=0;p<2;p++){
				int c = active_cards_count[p]-1;
				while( c > -1 ){
						if( in_play(p, c) && ! (p==token->t_player && c==card_added) ){
							if( get_id(p, c) == leg_id && get_special_infos(p, c) == 99 ){
								kill_card(p, c, KILL_SACRIFICE);
								kill_me = 1;
							}
						}
						c--;
				}
			}
			if( kill_me == 1 ){
				kill_card(token->t_player, card_added, KILL_SACRIFICE);
			}
		}
	}
	if( token->keep_track_of_tokens_generated > 0  ){
		token->keep_track_of_tokens_generated = MIN(token->keep_track_of_tokens_generated, (unsigned int)number);
	}
}

void copy_token(int player, int card, int tok_player, int tok_card ){
	if( tok_player < 0 || tok_card < 0 ){
		return;
	}

	token_generation_t token;
	copy_token_definition(player, card, &token, tok_player, tok_card);
	generate_token(&token);
}

static int generate_token_by_id_impl(int player, int card, int csvid, int number, int reserved){
	if (number <= 0){
		return 0;
	}

	token_generation_t token;
	default_token_definition(player, card, csvid, &token);
	int int_id = get_internal_card_id_from_csv_id(csvid);
	if (reserved){
		token.no_sleight = 1;
		number = 1;
	} else {
		number = get_updated_tokens_number(player, number);
		if (number < 1){
			number = 1;
		}
	}

	int i;
	int card_added = -1;
	for( i = 0; i < number; i++){
		card_added = real_generate_token(player, int_id, &token, 0);
	}

	return card_added;
}

void generate_token_by_id(int player, int card, int csvid){
	generate_token_by_id_impl(player, card, csvid, 1, is_reserved_id(csvid));
}

void generate_tokens_by_id(int player, int card, int csvid, int howmany){
	generate_token_by_id_impl(player, card, csvid, howmany, is_reserved_id(csvid));
}

int generate_reserved_token_by_id(int player, int csvid){
	ASSERT(is_reserved_id(csvid));
	// It's only safe to use -1 for card because we're sending 1 for reserved, and therefore will be setting no_sleight=1
	return generate_token_by_id_impl(player, -1, csvid, 1, 1);
}

void copy_all_tokens(int player, int card, int type, test_definition_t *test){
	int c, marked[151] = {0};
	for (c = 0; c < active_cards_count[player]; ++c){
		if (in_play(player, c) && is_token(player, c)){
			if( (type && is_what(player, c, type)) || (test != NULL && new_make_test_in_play(player, c, -1, test)) ){
				marked[c] = 1;
			}
		}
	}

	for (c = active_cards_count[player]-1; c >-1 ; c--){
		if (marked[c]){
			copy_token(player, card, player, c);
		}
	}
}

static int from_subtype_to_token_id(int subtype){
	switch( subtype ){
			case	SUBTYPE_ADVISOR: return 0;
			case	SUBTYPE_ALLY: return 0;
			case	SUBTYPE_ANGEL: return CARD_ID_ANGEL;
			case	SUBTYPE_ANTEATER: return 0;
			case	SUBTYPE_ANTELOPE: return 0;
			case	SUBTYPE_APE: return CARD_ID_APE;
			case	SUBTYPE_ARCHER: return 0;
			case	SUBTYPE_ARCHON: return 0;
			case	SUBTYPE_ARTIFICER: return 0;
			case	SUBTYPE_ASSASSIN: return CARD_ID_ASSASSIN;
			case	SUBTYPE_ASSEMBLY_WORKER: return 0;
			case	SUBTYPE_ATOG: return 0;
			case	SUBTYPE_AUROCHS: return 0;
			case	SUBTYPE_AVATAR: return CARD_ID_AVATAR;
			case	SUBTYPE_BADGER: return 0;
			case	SUBTYPE_BARBARIAN: return 0;
			case	SUBTYPE_BASILISK: return 0;
			case	SUBTYPE_BAT: return CARD_ID_BAT;
			case	SUBTYPE_BEAR: return CARD_ID_BEAR;
			case	SUBTYPE_BEAST: return CARD_ID_BEAST;
			case	SUBTYPE_BEEBLE: return 0;
			case	SUBTYPE_BERSERKER: return 0;
			case	SUBTYPE_BIRD: return CARD_ID_BIRD;
			case	SUBTYPE_BLINKMOTH: return 0;
			case	SUBTYPE_BOAR: return CARD_ID_BOAR;
			case	SUBTYPE_BRINGER: return 0;
			case	SUBTYPE_BRUSHWAGG: return 0;
			case	SUBTYPE_CAMARID: return CARD_ID_CAMARID;
			case	SUBTYPE_CAMEL: return 0;
			case	SUBTYPE_CARIBOU: return CARD_ID_CARIBOU;
			case	SUBTYPE_CARRIER: return 0;
			case	SUBTYPE_CAT: return CARD_ID_CAT;
			case	SUBTYPE_CENTAUR: return CARD_ID_CENTAUR;
			case	SUBTYPE_CEPHALID: return 0;
			case	SUBTYPE_CHIMERA: return 0;
			case	SUBTYPE_CITIZEN: return CARD_ID_CITIZEN;
			case	SUBTYPE_CLERIC: return CARD_ID_CLERIC;
			case	SUBTYPE_COCKATRICE: return 0;
			case	SUBTYPE_CONSTRUCT: return CARD_ID_CONSTRUCT;
			case	SUBTYPE_COWARD: return 0;
			case	SUBTYPE_CRAB: return 0;
			case	SUBTYPE_CROCODILE: return 0;
			case	SUBTYPE_CYCLOPS: return 0;
			case	SUBTYPE_DAUTHI: return 0;
			case	SUBTYPE_DEMON: return CARD_ID_DEMON;
			case	SUBTYPE_DESERTER: return CARD_ID_DESERTER;
			case	SUBTYPE_DEVIL: return CARD_ID_DEVIL;
			case	SUBTYPE_DJINN: return 0;
			case	SUBTYPE_DRAGON: return CARD_ID_DRAGON;
			case	SUBTYPE_DRAKE: return CARD_ID_DRAKE;
			case	SUBTYPE_DREADNOUGHT: return 0;
			case	SUBTYPE_DRONE: return 0;
			case	SUBTYPE_DRUID: return 0;
			case	SUBTYPE_DRYAD: return 0;
			case	SUBTYPE_DWARF: return 0;
			case	SUBTYPE_EFREET: return 0;
			case	SUBTYPE_ELDER: return 0;
			case	SUBTYPE_ELDRAZI: return CARD_ID_ELDRAZI;
			case	SUBTYPE_ELEMENTAL: return CARD_ID_ELEMENTAL;
			case	SUBTYPE_ELEPHANT: return CARD_ID_ELEPHANT;
			case	SUBTYPE_ELF: return 0;
			case	SUBTYPE_ELK: return 0;
			case	SUBTYPE_EYE: return 0;
			case	SUBTYPE_FAERIE: return CARD_ID_FAERIE;
			case	SUBTYPE_FERRET: return 0;
			case	SUBTYPE_FISH: return CARD_ID_FISH;
			case	SUBTYPE_FLAGBEARER: return 0;
			case	SUBTYPE_FOX: return 0;
			case	SUBTYPE_FROG: return 0;
			case	SUBTYPE_FUNGUS: return 0;
			case	SUBTYPE_GARGOYLE: return CARD_ID_GARGOYLE;
			case	SUBTYPE_GERM: return CARD_ID_GERM;
			case	SUBTYPE_GIANT: return CARD_ID_GIANT;
			case	SUBTYPE_GNOME: return CARD_ID_GNOME;
			case	SUBTYPE_GOAT: return CARD_ID_GOAT;
			case	SUBTYPE_GOBLIN: return CARD_ID_GOBLIN;
			case	SUBTYPE_GOD: return 0;
			case	SUBTYPE_GOLEM: return CARD_ID_GOLEM;
			case	SUBTYPE_GORGON: return 0;
			case	SUBTYPE_GRAVEBORN: return CARD_ID_GRAVEBORN;
			case	SUBTYPE_GREMLIN: return 0;
			case	SUBTYPE_GRIFFIN: return CARD_ID_GRIFFIN;
			case	SUBTYPE_HAG: return 0;
			case	SUBTYPE_HARPY: return CARD_ID_HARPY;
			case	SUBTYPE_HEAD: return 0;
			case	SUBTYPE_HELLION: return CARD_ID_HELLION;
			case	SUBTYPE_HIPPO: return CARD_ID_HIPPO;
			case	SUBTYPE_HIPPOGRIFF: return 0;
			case	SUBTYPE_HOMARID: return 0;
			case	SUBTYPE_HOMUNCULUS: return CARD_ID_HOMUNCULUS;
			case	SUBTYPE_HORROR: return CARD_ID_HORROR;
			case	SUBTYPE_HORSE: return 0;
			case	SUBTYPE_HOUND: return CARD_ID_HOUND;
			case	SUBTYPE_HUMAN: return CARD_ID_HUMAN;
			case	SUBTYPE_HYDRA: return CARD_ID_HYDRA;
			case	SUBTYPE_HYENA: return 0;
			case	SUBTYPE_ILLUSION: return CARD_ID_ILLUSION_TOKEN;
			case	SUBTYPE_IMP: return 0;
			case	SUBTYPE_INCARNATION: return 0;
			case	SUBTYPE_INSECT: return CARD_ID_INSECT;
			case	SUBTYPE_JELLYFISH: return 0;
			case	SUBTYPE_JUGGERNAUT: return 0;
			case	SUBTYPE_KAVU: return CARD_ID_KAVU;
			case	SUBTYPE_KIRIN: return 0;
			case	SUBTYPE_KITHKIN: return 0;
			case	SUBTYPE_KNIGHT: return CARD_ID_KNIGHT;
			case	SUBTYPE_KOBOLD: return 0;
			case	SUBTYPE_KOR: return 0;
			case	SUBTYPE_KRAKEN: return CARD_ID_KRAKEN;
			case	SUBTYPE_LAMIA: return 0;
			case	SUBTYPE_LAMMASU: return 0;
			case	SUBTYPE_LEECH: return 0;
			case	SUBTYPE_LEVIATHAN: return 0;
			case	SUBTYPE_LHURGOYF: return 0;
			case	SUBTYPE_LICID: return 0;
			case	SUBTYPE_LIZARD: return CARD_ID_LIZARD;
			case	SUBTYPE_MANTICORE: return 0;
			case	SUBTYPE_MASTICORE: return 0;
			case	SUBTYPE_MERCENARY: return 0;
			case	SUBTYPE_MERFOLK: return CARD_ID_MERFOLK;
			case	SUBTYPE_METATHRAN: return 0;
			case	SUBTYPE_MINION: return CARD_ID_MINION;
			case	SUBTYPE_MINOTAUR: return CARD_ID_MINOTAUR;
			case	SUBTYPE_MOLE: return 0;
			case	SUBTYPE_MONGER: return 0;
			case	SUBTYPE_MONGOOSE: return 0;
			case	SUBTYPE_MONK: return CARD_ID_MONK;
			case	SUBTYPE_MOONFOLK: return 0;
			case	SUBTYPE_MUTANT: return 0;
			case	SUBTYPE_MYR: return CARD_ID_MYR;
			case	SUBTYPE_MYSTIC: return 0;
			case	SUBTYPE_NAGA: return 0;
			case	SUBTYPE_NAUTILUS: return 0;
			case	SUBTYPE_NEPHILIM: return 0;
			case	SUBTYPE_NIGHTMARE: return 0;
			case	SUBTYPE_NIGHTSTALKER: return 0;
			case	SUBTYPE_NINJA: return 0;
			case	SUBTYPE_NOGGLE: return 0;
			case	SUBTYPE_NOMAD: return 0;
			case	SUBTYPE_NYMPH: return 0;
			case	SUBTYPE_OCTOPUS: return CARD_ID_OCTOPUS;
			case	SUBTYPE_OGRE: return CARD_ID_OGRE;
			case	SUBTYPE_OOZE: return CARD_ID_OOZE;
			case	SUBTYPE_ORB: return CARD_ID_ORB;
			case	SUBTYPE_ORC: return 0;
			case	SUBTYPE_ORGG: return 0;
			case	SUBTYPE_OUPHE: return 0;
			case	SUBTYPE_OX: return 0;
			case	SUBTYPE_OYSTER: return 0;
			case	SUBTYPE_PEGASUS: return CARD_ID_PEGASUS;
			case	SUBTYPE_PENTAVITE: return CARD_ID_PENTAVITE;
			case	SUBTYPE_PEST: return CARD_ID_PEST;
			case	SUBTYPE_PHELDDAGRIF: return 0;
			case	SUBTYPE_PHOENIX: return 0;
			case	SUBTYPE_PINCHER: return CARD_ID_PINCHER;
			case	SUBTYPE_PIRATE: return 0;
			case	SUBTYPE_PLANT: return CARD_ID_PLANT;
			case	SUBTYPE_PRAETOR: return 0;
			case	SUBTYPE_PRISM: return CARD_ID_PRISM;
			case	SUBTYPE_PROCESSOR: return 0;
			case	SUBTYPE_RABBIT: return 0;
			case	SUBTYPE_RAT: return CARD_ID_RAT;
			case	SUBTYPE_REBEL: return 0;
			case	SUBTYPE_REFLECTION: return CARD_ID_REFLECTION;
			case	SUBTYPE_REVELER: return 0;
			case	SUBTYPE_RHINO: return CARD_ID_RHINO;
			case	SUBTYPE_RIGGER: return 0;
			case	SUBTYPE_ROGUE: return 0;
			case	SUBTYPE_SABLE: return 0;
			case	SUBTYPE_SALAMANDER: return 0;
			case	SUBTYPE_SAMURAI: return 0;
			case	SUBTYPE_SAND: return 0;
			case	SUBTYPE_SAPROLING: return CARD_ID_SAPROLING;
			case	SUBTYPE_SATYR: return CARD_ID_SATYR;
			case	SUBTYPE_SCARECROW: return 0;
			case	SUBTYPE_SCION: return 0;
			case	SUBTYPE_SCORPION: return 0;
			case	SUBTYPE_SCOUT: return 0;
			case	SUBTYPE_SERF: return CARD_ID_SERF;
			case	SUBTYPE_SERPENT: return 0;
			case	SUBTYPE_SHADE: return 0;
			case	SUBTYPE_SHAMAN: return 0;
			case	SUBTYPE_SHAPESHIFTER: return CARD_ID_SHAPESHIFTER_TOKEN;
			case	SUBTYPE_SHEEP: return CARD_ID_SHEEP;
			case	SUBTYPE_SIREN: return 0;
			case	SUBTYPE_SKELETON: return CARD_ID_SKELETON;
			case	SUBTYPE_SLITH: return 0;
			case	SUBTYPE_SLIVER: return CARD_ID_SLIVER;
			case	SUBTYPE_SLUG: return 0;
			case	SUBTYPE_SNAKE: return CARD_ID_SNAKE;
			case	SUBTYPE_SOLDIER: return CARD_ID_SOLDIER;
			case	SUBTYPE_SOLTARI: return 0;
			case	SUBTYPE_SPAWN: return CARD_ID_SPAWN;
			case	SUBTYPE_SPECTER: return 0;
			case	SUBTYPE_SPELLSHAPER: return 0;
			case	SUBTYPE_SPHINX: return CARD_ID_SPHINX;
			case	SUBTYPE_SPIDER: return CARD_ID_SPIDER;
			case	SUBTYPE_SPIKE: return CARD_ID_SPIKE;
			case	SUBTYPE_SPIRIT: return CARD_ID_SPIRIT;
			case	SUBTYPE_SPLINTER: return CARD_ID_SPLINTER_TOKEN;
			case	SUBTYPE_SPONGE: return 0;
			case	SUBTYPE_SQUID: return CARD_ID_SQUID;
			case	SUBTYPE_SQUIRREL: return CARD_ID_SQUIRREL;
			case	SUBTYPE_STARFISH: return CARD_ID_STARFISH;
			case	SUBTYPE_SURRAKAR: return CARD_ID_SURVIVOR;
			case	SUBTYPE_SURVIVOR: return 0;
			case	SUBTYPE_TETRAVITE: return 0;
			case	SUBTYPE_THALAKOS: return 0;
			case	SUBTYPE_THOPTER: return CARD_ID_THOPTER;
			case	SUBTYPE_THRULL: return CARD_ID_THRULL;
			case	SUBTYPE_TREEFOLK: return CARD_ID_TREEFOLK;
			case	SUBTYPE_TRISKELAVITE: return CARD_ID_TRISKELAVITE;
			case	SUBTYPE_TROLL: return 0;
			case	SUBTYPE_TURTLE: return 0;
			case	SUBTYPE_UNICORN: return 0;
			case	SUBTYPE_VAMPIRE: return CARD_ID_VAMPIRE;
			case	SUBTYPE_VEDALKEN: return 0;
			case	SUBTYPE_VIASHINO: return 0;
			case	SUBTYPE_VOLVER: return 0;
			case	SUBTYPE_WALL: return 0;
			case	SUBTYPE_WARRIOR: return CARD_ID_WARRIOR;
			case	SUBTYPE_WEIRD: return CARD_ID_WEIRD;
			case	SUBTYPE_WEREWOLF: return 0;
			case	SUBTYPE_WHALE: return CARD_ID_WHALE;
			case	SUBTYPE_WIZARD: return 0;
			case	SUBTYPE_WOLF: return CARD_ID_WOLF;
			case	SUBTYPE_WOLVERINE: return 0;
			case	SUBTYPE_WOMBAT: return 0;
			case	SUBTYPE_WORM: return CARD_ID_WORM;
			case	SUBTYPE_WRAITH: return 0;
			case	SUBTYPE_WURM: return 0;
			case	SUBTYPE_YETI: return 0;
			case	SUBTYPE_ZOMBIE: return CARD_ID_ZOMBIE;
			case	SUBTYPE_ZUBERA: return 0;
			default: return 0;
	}
}

static void store_selected_subtype(token_generation_t* token, int beast, int unused)
{
	get_card_instance(token->t_player, beast)->targets[1].card = token->action_argument;
}

void generate_custom_token_from_subtype(int player, int card, int subtype, int color, int pow, int tou, int amount){
	// See Riptide Replicator and Volrath's Laboratory

	int csvid = from_subtype_to_token_id(subtype);

	token_generation_t token;
	if( csvid ){
		default_token_definition(player, card, csvid, &token);
	}
	else{
		default_token_definition(player, card, CARD_ID_RIPTIDE_REPLICATOR_TOKEN, &token);
	}
	token.qty = amount;
	token.pow = pow;
	token.tou = tou;
	token.color_forced = color;
	token.no_sleight = 1;
	if( ! csvid ){
		token.special_code_on_generation = &store_selected_subtype;
		token.action_argument = subtype;
	}
	generate_token(&token);
}
