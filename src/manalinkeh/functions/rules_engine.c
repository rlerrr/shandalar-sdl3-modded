#include <ctype.h>
#include <limits.h>

#include "manalink.h"
#include "platform/file_io.h"

static void increase_trap_condition_impl(int player, int trap, int val, int add);

/* Canonical recovered helper declarations. */
int get_stormcolor_count(int player, color_t clr);
void increment_storm_count(int player, int mode);
int get_specific_storm_count(int player);
int get_storm_count(void);
int get_stormcreature_count(int k);

/* October 2022 independent game-mode flags. */
other_game_mode_t other_game_mode;

extern int mulligans_complete[2];

int hack_allow_sorcery_if_rules_engine_is_only_effect = 0;

static int vanguard_avatar[2][10];
static int vcount[2] = {0, 0};
static int companion[2][10];
static int ccount[2];
int companion_effect(int player, int card, event_t event);
#ifdef CARDS_IN_HAND_THIS_TURN	// unused
static int cards_in_hand_this_turn[2][100];
#endif
static int settings[SETTING_MAX_PLUS_1] = { 0 };
static event_t actual_event = -1;
static int challenge = 0;
static int rules_engine_card[2] = {-1,-1};
static int deadbox_card[2] = {-1,-1};
static int starting_life_total[2] = {20,20};
static int force_init_upkeep = 0;

/* Current targets usage
 * [0], [1], [2], [4]:	activate_cards_in_hand().  No need to exhaustively sort them out just yet.
 * [3]:					Unused, but currently set to -1 in activate_cards_in_hand()'s EVENT_ACTIVATE handler.
 * [5]-[17]:			Assigned in rules_engine_globals_t.
 * [18]:				Counter storage. */

// Storage is contiguous with Rules Engine's full targets array.
typedef struct
{
  target_t targets[5];	// For use in activate_cards_in_hand().
  uint16_t shorts_per_turn[TRAP_NUMSHORTS_SHORTS_PER_TURN];
  uint8_t unassigned1[20];	// Unassigned storage is spaced out so that values of any given size can be added without invalidating savegames.
  uint16_t shorts_persistent[TRAP_NUMSHORTS_SHORTS_PERSISTENT];
  uint8_t unassigned2[16];
  uint8_t bytes_per_turn[TRAP_NUMBYTES_BYTES_PER_TURN];
  uint8_t unassigned3[10];
  uint8_t bytes_persistent[TRAP_NUMBYTES_BYTES_PERSISTENT];
  uint8_t unassigned4[6];
  uint8_t nybbles_per_turn[TRAP_NUMBYTES_NYBBLES_PER_TURN];
  uint8_t unassigned5[5];
  uint8_t bits_per_turn[TRAP_NUMBYTES_BITS_PER_TURN];
  uint8_t unassigned6[3];
  uint8_t bits_persistent[TRAP_NUMBYTES_BITS_PERSISTENT];
  uint8_t unassigned7[3];
  target_t counters;
} PACKED rules_engine_globals_t;

STATIC_ASSERT(sizeof(rules_engine_globals_t) == sizeof(((card_instance_t*)(NULL))->targets), sizeof_rules_engine_globals_t_must_match_target_array);

int get_setting(int thing){
	return settings[thing];
}

// exe interface - no longer used
void set_draft(void){
}
void set_momir(void){
}
void set_testing(void){
}
void set_none(void){
}

int get_momir(void){
	return singleplayer_mode == SPM_MOMIR;
}

static int get_challenge(void){
	return challenge;
}

void set_challenge(int m){
	challenge = m;
	set_challenge_round_to_0();
	ante = 0;
}

static int check_deck_legality(void)
{
  int vintage_legal = 1, legacy_legal = 1, illegal_card = -1;
  int new_deck[500];
  for (int i = 0; i < 500; ++i)
    new_deck[i] = -1;
  for (int i = 0; i < 500; ++i)
  {
    if (deck_ptr[HUMAN][i] < 0)
    {
      new_deck[i] = -1;
      break;
    }
    new_deck[i] = cards_data[deck_ptr[HUMAN][i]].id;
  }
  if (count_deck(HUMAN) < 52 && gauntlet_round == 0)
    return 0;
  uint8_t counts[available_slots + 1];
  memset(counts, 0, sizeof(counts));
  for (int i = 0; i <= available_slots; ++i)
    counts[i] = 0;
  for (int i = 0; i < 500; ++i)
  {
    int id = new_deck[i];
    if (id >= 0 && id <= available_slots && counts[id] != 255)
      ++counts[id];
  }
  for (int i = 0; i < 500; ++i)
  {
    int id = new_deck[i];
    if (id < 0 || id > available_slots || counts[id] == 0)
      continue;
    if (counts[id] > 4 && counts[id] > limit_allowed_in_deck(id))
    {
      illegal_card = id;
      goto invalid_deck;
    }
    CardPtrFlags flags = cards_ptr[id]->flags;
    if (vintage_legal &&
        (((flags & CPF_RESTRICTED_VINTAGE) && counts[id] > 1) || (flags & CPF_BANNED_VINTAGE)))
    {
      vintage_legal = 0;
      illegal_card = id;
    }
    if (legacy_legal && (flags & CPF_BANNED_LEGACY))
    {
      legacy_legal = 0;
      illegal_card = id;
    }
  }
  if (vintage_legal || legacy_legal)
    return -1;
invalid_deck:;
  int fake = add_card_to_hand(HUMAN, get_internal_card_id_from_csv_id(illegal_card));
  card_instance_t *instance = get_card_instance(HUMAN, fake);
  instance->state |= STATE_INVISIBLE;
  --hand_count[HUMAN];
  return fake;
}

void read_settings(void)
{
  // Only read config once
  if (settings[SETTING_SETTING_FILE_READ] == 0)
  {
    settings[SETTING_SETTING_FILE_READ] = 1;

    // Default values
    settings[SETTING_ABILITY_ICON_FILE] = (int)("Abilities.pic");
    settings[SETTING_ABILITY_ICON_MAX] = 26;
    settings[SETTING_HAND_SIZE] = 8;
    settings[SETTING_MIN_PICS_TO_RANDOMIZE] = 15;
    settings[SETTING_RULES_ENGINE] = 1;
    settings[SETTING_OPPONENT_SKILL] = 3;

    // if the config file exists, then read in the config settings
    char buffer[200];
    SDL_IOStream *file = shandalar_open_file_io("config.txt", "r");
    if (file != NULL)
    {
      settings[SETTING_SETTING_FILE_READ] = 2;
      while (shandalar_io_gets(buffer, 200, file))
      {
        const char *p;
        // skip whitespace
        for (p = buffer; *p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'); ++p)
        {
        }

        switch (*p)
        {
#define CFG(str, settingnum)                                                                       \
  if (_strnicmp(p, str ":", strlen(str ":")) == 0)                                               \
  {                                                                                                \
    settings[settingnum] = atoi(p + strlen(str ":"));                                              \
    continue;                                                                                      \
  }
#define CFGSTR(str, settingnum)                                                                    \
  if (_strnicmp(p, str ":", strlen(str ":")) == 0)                                               \
  {                                                                                                \
    const char *q = p + strlen(str ":");                                                           \
    while (isspace(*q))                                                                            \
      ++q;                                                                                         \
    char *dup = strdup(q);                                                                         \
    char *r = dup + strlen(dup) - 1;                                                               \
    while (r > dup && isspace(*r))                                                                 \
    {                                                                                              \
      *r = 0;                                                                                      \
      --r;                                                                                         \
    }                                                                                              \
    if (*dup)                                                                                      \
      settings[settingnum] = (int)dup;                                                             \
    continue;                                                                                      \
  }
        case '/':
        case 0:
          // Ignore blank lines and lines starting with /
          continue;
        case 'a':
        case 'A':
          CFGSTR("AbilityIconFile", SETTING_ABILITY_ICON_FILE);
          CFG("AbilityIconMax", SETTING_ABILITY_ICON_MAX);
          CFG("AbilityIconSize", SETTING_ABILITY_ICON_SIZE);
          CFG("AiDecisionTime", SETTING_AI_DECISION_TIME);
          CFG("AiUsesRulesEngine", SETTING_AI_USES_RULES_ENGINE);
          CFG("AlphabetizeDeck", SETTING_ALPHABETIZE_DECK);
          CFG("Archenemy", SETTING_ARCHENEMY);
          CFG("AttachEquipment", SETTING_ATTACH_EQUIPMENT);
          CFG("AutoHaste", SETTING_AUTO_HASTE);
          CFG("AutoMill", SETTING_AUTO_MILL);
          CFG("AutotapArtifacts", SETTING_AUTOTAP_ARTIFACTS);
          CFG("AutotapCreatures", SETTING_AUTOTAP_CREATURES);
          break;
        case 'c':
        case 'C':
          CFG("ChallengeMode", SETTING_CHALLENGE_MODE);
          CFG("CheckDeckLegality", SETTING_CHECK_DECK_LEGALITY);
          CFG("CrippleAI", SETTING_CRIPPLE_AI);
          break;
        case 'd':
        case 'D':
          CFG("DuhMode", SETTING_DUH_MODE);
          CFG("Debug", SETTING_DEBUG);
          break;
        case 'h':
        case 'H':
          CFG("HandSize", SETTING_HAND_SIZE);
          break;
        case 'm':
        case 'M':
          CFG("ManaBurn", SETTING_MANA_BURN);
          CFG("MinPicsToRandomize", SETTING_MIN_PICS_TO_RANDOMIZE);
          break;
        case 'o':
        case 'O':
          CFG("OldPWRules", SETTING_OLD_PW_RULES);
          CFG("OpponentSkill", SETTING_OPPONENT_SKILL);
          break;
        case 'q':
        case 'Q':
          CFG("QuickStart", SETTING_QUICK_START);
          break;
        case 'r':
        case 'R':
          CFG("RulesEngine", SETTING_RULES_ENGINE);
          break;
        case 's':
        case 'S':
          CFG("SmallCardSize", SETTING_SMALLCARD_SIZE);
          CFG("SmartTarget", SETTING_SMART_TARGET);
          break;
        case 't':
        case 'T':
          CFG("TournamentMode", SETTING_TOURNAMENT_MODE);
          break;
        case 'u':
        case 'U':
          CFG("UnlimitedMana", SETTING_UNLIMITED_MANA);
          break;
        case 'v':
        case 'V':
          CFG("VerifyHandCount", SETTING_VERIFY_HAND_COUNT);
          break;
#undef CFG
        }
        popup("config.txt", "Unknown line in config.txt: \"%s\"", buffer);
      }
      shandalar_io_close(file);
    }
    opponent_skill = settings[SETTING_OPPONENT_SKILL];
  }
}

static void read_and_adjust_settings(void)
{

  read_settings();
  opponent_skill = settings[SETTING_OPPONENT_SKILL];

  // if the challenge file exists, then add the challenge flag and remove debug modes
  if (settings[SETTING_CHALLENGE_MODE] || get_challenge())
  {
    settings[SETTING_CHALLENGE_MODE] = 1;
    debug_mode = 0;
    settings[SETTING_AUTO_MILL] = 0;
    settings[SETTING_UNLIMITED_MANA] = 0;
    settings[SETTING_AUTO_HASTE] = 0;
    settings[SETTING_QUICK_START] = 0;
    settings[SETTING_CRIPPLE_AI] = 0;
    settings[SETTING_DEBUG] = 0;
  }
  else
  {
    // Set exe debug mode
    if (settings[SETTING_DEBUG])
    {
      debug_mode = 1;
      trace_mode = 0;
    }
    // if we are testing, turn on crippleAI, autohaste, unlimited mana, and
    // quickstart and automill
    if (singleplayer_mode == SPM_TESTING)
    {
      force_init_upkeep = 1;
      settings[SETTING_AUTO_MILL] = 1;
      settings[SETTING_UNLIMITED_MANA] = 1;
      settings[SETTING_AUTO_HASTE] = 1;
      settings[SETTING_QUICK_START] = 1;
      settings[SETTING_CRIPPLE_AI] = 1;
    }
  }

  /* Make sure rules_engine_card and deadbox_card globals are correct.  They need to be reset at
   * each new game and each loaded game, since their placement will vary on how many times you
   * mulligan and whether they were added midgame by a Wish card or such. */
  int p, c;
  for (p = 0; p <= 1; ++p)
  {
    rules_engine_card[p] = -1;
    deadbox_card[p] = -1;
    for (c = 0; c < 150; ++c)
    {
      if (in_play(p, c))
      {
        int csvid = get_id(p, c);
        if (csvid == CARD_ID_RULES_ENGINE)
        {
          rules_engine_card[p] = c;
        }
        if (csvid == CARD_ID_DEADBOX)
        {
          deadbox_card[p] = c;
        }
      }
    }
  }
}

static UnorderedMapIntInt card_image_numbers = NULL;

int get_card_image_number(int csvid, int player, int card)
{
  // 0x468160

  // This surely belongs somewhere else.  But so do settings.

  if (__builtin_expect(csvid < 0 || player < 0 || card < 0, 0))
	return 0;

  int num_pics = cards_ptr[csvid]->num_pics;
  if (__builtin_expect(num_pics <= 1, 1))
	return 0;

  if (__builtin_expect(num_pics < settings[SETTING_MIN_PICS_TO_RANDOMIZE], 1))
	return (card + player) % num_pics;

  int key = (LOWORD(csvid) << 16) | (BYTE0(player) << 8) | BYTE0(card);
  int* prev_value = UnorderedMapIntInt_fetch(&card_image_numbers, key);
  if (__builtin_expect(prev_value != NULL, 1))
	return *prev_value;

  return *(UnorderedMapIntInt_set(&card_image_numbers, key)) = internal_rand(num_pics);

  /* card_image_numbers should likely be cleared when loading a save game or when starting a new one, but the earliest existing injection at game start happens
   * after cards are drawn. */
}

void pregame(void)
{

  /* This called after *every card drawn* during the initial draw.
   * There isn't really much of a better place to put it; any earlier and libraries won't have been
   * initialized. */

  hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
  debug_addcard_last_color = COLOR_TEST_ANY;
  debug_addcard_last_type = TYPE_ANY;

  if (game_type != GAMETYPE_SOLO_DUEL)
  {
    singleplayer_mode = 0; // Seems to be handled by the dialog code already, but I can't find it,
                           // and no reason to risk not clearing it.
    /* Recovered 2022 game-mode fragment: reset outside solo duel @ 0x26c7a80. */
    other_game_mode = OGM_NONE;
    /* End recovered 2022 game-mode fragment: reset outside solo duel. */
  }

  int i;
  for (i = 0; i < 50; ++i)
  {
    is_unlocked(0, 0, 0, i);
  }
  // Check for vanguard avatars in the deck or already drawn
  int p, c;
  for (p = 0; p < 2; ++p)
  {
    if (vcount[p] == 0)
    {
      mulligans_complete[p] = 0;
      int *deck = deck_ptr[p];
      for (i = 0; i < 500 && deck[i] != -1; i++)
      {
        if (cards_data[deck[i]].cc[2] == 3)
        {
          int csvid = cards_data[deck[i]].id;
          if (csvid == CARD_ID_AVATAR_MOMIR_VIG)
          {
            singleplayer_mode = SPM_MOMIR;
          }
          /* Recovered 2022 game-mode fragment: EDH avatar in deck @ 0x26c7a80. */
          if (csvid == CARD_ID_ELDER_DRAGON_HIGHLANDER)
          {
            other_game_mode |= OGM_EDH;
          }
          /* End recovered 2022 game-mode fragment: EDH avatar in deck. */
          if (vcount[p] < 10)
          {
            vanguard_avatar[p][vcount[p]] = csvid;
            vcount[p]++;
          }
        }
      }
      for (c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *instance = get_card_instance(p, c);
        if (instance->internal_card_id >= 0 && cards_data[instance->internal_card_id].cc[2] == 3)
        {
          int csvid = cards_data[instance->internal_card_id].id;
          if (csvid == CARD_ID_AVATAR_MOMIR_VIG)
          {
            singleplayer_mode = SPM_MOMIR;
          }
          /* Recovered 2022 game-mode fragment: EDH avatar already drawn @ 0x26c7a80. */
          if (csvid == CARD_ID_ELDER_DRAGON_HIGHLANDER)
          {
            other_game_mode |= OGM_EDH;
          }
          /* End recovered 2022 game-mode fragment: EDH avatar already drawn. */
          if (vcount[p] < 10)
          {
            vanguard_avatar[p][vcount[p]] = csvid;
            vcount[p]++;
          }
        }
      }
    }
    if (ccount[p] == 0)
    {
      int *deck = deck_ptr[p];
      for (int k = 0; k < 500 && deck[k] != -1; ++k)
        if (query(-1, -1, deck[k], QUERY_COMPANION) && ccount[p] < 10)
          companion[p][ccount[p]++] = deck[k];
      for (int k = 0; k < active_cards_count[p]; ++k)
      {
        if (p == AI && ccount[AI])
          break;
        card_instance_t *instance = get_card_instance(p, k);
        if (query(p, k, instance->internal_card_id, QUERY_COMPANION) && ccount[p] < 10)
          companion[p][ccount[p]++] = instance->internal_card_id;
      }
    }
  }

  // if the draft file exists, then add the draft avatar
  if (singleplayer_mode == SPM_DRAFT)
  {
    vanguard_avatar[HUMAN][0] = CARD_ID_DRAFT;
    vcount[HUMAN] = 1;
    vcount[AI] = 0;
  }
  else if (singleplayer_mode == SPM_MOMIR)
  {
    vanguard_avatar[HUMAN][0] = CARD_ID_AVATAR_MOMIR_VIG;
    vcount[HUMAN] = 1;
    vanguard_avatar[AI][0] = CARD_ID_AVATAR_MOMIR_VIG;
    vcount[AI] = 1;
  }

  settings[SETTING_GAMESTART_SETTINGS_APPLIED] = 0;
  read_and_adjust_settings();

  if (vcount[AI] == 0 && get_setting(SETTING_ARCHENEMY) && !get_setting(SETTING_CHALLENGE_MODE))
  {
    int draft_found = 0;
    int k;
    for (k = 0; k < 10; k++)
    {
      if (vanguard_avatar[HUMAN][k] == CARD_ID_DRAFT)
      {
        draft_found++;
        break;
      }
    }
    if (!draft_found)
    {
      vanguard_avatar[AI][0] = CARD_ID_ARCHENEMY;
      vcount[AI] = 1;
    }
  }
}

void after_load_game(void)
{
  hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
  debug_addcard_last_color = COLOR_TEST_ANY;
  debug_addcard_last_type = TYPE_ANY;

  // Not strictly correct if the game was saved in turn one, but wouldn't have worked anyway since
  // the other settings were off, too.
  // ...I think.  Challenge setting might get forced on, in which case, challenge_mode_upkeep()
  // should still be called (once). On yet a third hand, if that's true, it was *always* getting
  // called the first turn after a load, which was even worse.
  settings[SETTING_GAMESTART_SETTINGS_APPLIED] = 1;

  read_and_adjust_settings();
}

int quick_start(void){

	if( vcount[HUMAN] ){
		return 1;
	}

	return get_setting(SETTING_QUICK_START);
}

// Has a code pointer assigned, but if the exe calls it, I can't find it.
int fake_random(int num){
	return 0;
}

int is_hidden_agenda(int csvid){
	switch( csvid ){
			case CARD_ID_CONSPIRACY_BRAGOS_FAVOR:
			case CARD_ID_CONSPIRACY_DOUBLE_STROKE:
			case CARD_ID_CONSPIRACY_IMMEDIATE_ACTION:
			case CARD_ID_CONSPIRACY_ITERATIVE_ANALYSIS:
			case CARD_ID_CONSPIRACY_MUZZIOS_PREPARATIONS:
			case CARD_ID_CONSPIRACY_SECRET_SUMMONING:
			case CARD_ID_CONSPIRACY_SECRETS_OF_PARADISE:
			case CARD_ID_CONSPIRACY_UNEXPECTED_POTENTIAL:
			case CARD_ID_CONSPIRACY_ADRIANAS_VALOR:
			case CARD_ID_CONSPIRACY_ASSEMBLE_THE_RANK_AND_VILE:
			case CARD_ID_CONSPIRACY_ECHOING_BOON:
			case CARD_ID_CONSPIRACY_HIRED_HEIST:
			case CARD_ID_CONSPIRACY_INCENDIARY_DISSENT:
			case CARD_ID_CONSPIRACY_NATURAL_UNITY:
			case CARD_ID_CONSPIRACY_SUMMONERS_BOND:
				return 1;
			default:
				return 0;
	}
	return 0;
}

static void vanguard_check(void)
{
  // put the vanguard avatars into play
  int p;
  for (p = 0; p < 2; p++)
  {
    if (!get_setting(SETTING_CHALLENGE_MODE))
    {
      int k;
      for (k = 0; k < vcount[p]; k++)
      {
        if (vanguard_avatar[p][k] != -1)
        {
          if (!is_hidden_agenda(vanguard_avatar[p][k]))
          {
            int card_added =
                add_card_to_hand(p, get_internal_card_id_from_csv_id(vanguard_avatar[p][k]));
            put_into_play(p, card_added);
            vanguard_avatar[p][k] = -1;
          }
          else
          {
            int card_added = add_card_to_hand(
                p, get_internal_card_id_from_csv_id(CARD_ID_CONSPIRACY_HIDDEN_AGENDA));
            card_instance_t *instance = get_card_instance(p, card_added);
            instance->targets[13].player = CARD_ID_CONSPIRACY_HIDDEN_AGENDA;
            instance->targets[13].card = vanguard_avatar[p][k];
            put_into_play(p, card_added);
            vanguard_avatar[p][k] = -1;
          }
        }
      }
      vcount[p] = 0;
    }
    else
    {
      if (p == 0)
      {
        apply_challenge_vanguard();
      }
    }
    if (ccount[p])
    {
      test_definition_t test;
      new_default_test_definition(&test, TYPE_ANY, "Select your companion.");
      int selected = select_card_from_zone(p, p, companion[p], ccount[p], 0, AI_MAX_CMC, -1, &test);
      if (selected != -1)
      {
        int iid = companion[p][selected];
        create_legacy_activate_from_iid(p, iid, companion_effect, -1, -1);
        int found = 0;
        for (int k = 0; k < 500 && deck_ptr[p][k] != -1; ++k)
          if (deck_ptr[p][k] == iid)
          {
            remove_card_from_deck(p, k);
            found = 1;
            break;
          }
        if (!found)
          for (int k = 0; k < active_cards_count[p]; ++k)
            if (get_original_internal_card_id(p, k) == iid)
            {
              int was_in_hand = in_hand(p, k) != NULL;
              obliterate_card(p, k);
              if (was_in_hand)
              {
                add_card_to_hand(p, deck_ptr[p][0]);
                remove_card_from_deck(p, 0);
              }
              break;
            }
      }
      ccount[p] = 0;
    }
  }
}

// Perform any upkeep triggers for cards in the graveyard
static void resolve_graveyard_upkeep_triggers(int player, int card, event_t event ){
	card_instance_t *instance= get_card_instance(player, card);
	if( current_turn == player ){

		// if any abilities were found, do the upkeep trigger
		if(trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player){
			int grave_count = count_graveyard(player);
			const int *graveyard = get_grave(player);
			int i = 0;
			int ability_found = 0;
			int c1 = count_upkeeps(player);
			while(i<grave_count && ability_found != 1){
					card_data_t* card_d = &cards_data[ graveyard[i] ];
					if( card_d->id == CARD_ID_DEATH_SPARK ){
						if( graveyard[i+1] != -1 && (cards_data[ graveyard[i+1] ].type & TYPE_CREATURE) ){
							ability_found = 1;
						}
					}
					else if( card_d->cc[2] == 5 || card_d->id == CARD_ID_ETERNAL_DRAGON || ( player == AI && card_d->id == CARD_ID_NIM_DEVOURER) ||
							( player == AI && card_d->id == CARD_ID_NECROSAVANT) || ( player == AI && card_d->id == CARD_ID_UNDEAD_GLADIATOR)
						 ){
							ability_found = 1;
					}
					i++;
			}
			if( ability_found && c1 > 0){
				if(event == EVENT_TRIGGER){
					event_result |= 2;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
						while( c1 > 0 ){
							i = grave_count-1;
							while( i > -1 ){
								card_data_t* card_d = &cards_data[ graveyard[i] ];
								if( card_d->id != CARD_ID_DEATH_SPARK ){
									if( card_d->cc[2] == 5 || card_d->id == CARD_ID_ETERNAL_DRAGON ||
										( player == AI && card_d->id == CARD_ID_NIM_DEVOURER) ||
										( player == AI && card_d->id == CARD_ID_NECROSAVANT)
									  ){
										// put a copy of the card into your hand
										int card_added =  add_card_to_hand(player, graveyard[i] );

										instance->number_of_targets = 1;
										card_instance_t *target = get_card_instance(player, card_added);
										target->state |= STATE_INVISIBLE;
										--hand_count[player];

										CardFunction ptFunction = card_d->code_pointer;
										int result = ptFunction(player, card_added, EVENT_GRAVEYARD_ABILITY_UPKEEP);
										if( result == -1 ){ // this card is removed
											remove_card_from_grave(player, i);
										}
										else if( result == -2 ){ // -2 means no card is removed
											obliterate_card_and_recycle(player, card_added);
										}
										else{  // another card was removed
											obliterate_card_and_recycle(player, card_added);
											remove_card_from_grave(player, result);
										}
									}
								}

								else{
										if( graveyard[i+1] != -1 && (cards_data[ graveyard[i+1] ].type & TYPE_CREATURE) ){
											int card_added =  add_card_to_hand(player, graveyard[i] );
											card_instance_t *target = get_card_instance(player, card_added);
											target->state |= STATE_INVISIBLE;
											--hand_count[player];

											CardFunction ptFunction = card_d->code_pointer;
											int result = ptFunction(player, card_added, EVENT_GRAVEYARD_ABILITY_UPKEEP);
											if( result == -1 ){ // this card is removed
												remove_card_from_grave(player, i);
											}
											else if( result == -2 ){ // -2 means no card is removed
													obliterate_card_and_recycle(player, card_added);
											}
											else{  // another card was removed
												obliterate_card_and_recycle(player, card_added);
												remove_card_from_grave(player, result);
											}
										}
								}
								i--;
							}
							c1--;
						}
				}
			}
		}
	}
}

static void resolve_dredge_triggers(int player, int card, event_t event ){
	int i;

	// Check for dredge
	if(trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && reason_for_trigger_controller == player && !suppress_draw){
		// see if there are any cards to dredge
		int grave_count = count_graveyard(player);
		const int *graveyard = get_grave(player);
		int ability_found = 0;
		char buffer[500];
		int pos = scnprintf(buffer, 500, "Dredge:\n Pass\n" );
		for(i=0;i<grave_count;i++){
			card_data_t* card_d = &cards_data[ graveyard[i] ];
			if( card_d->cc[2] == 6 ){
				CardFunction ptFunction = card_d->code_pointer;
				int result = ptFunction(player, 0, EVENT_DREDGE);
				if( count_deck(player) >= result ){
					ability_found = 1;
					card_ptr_t* c = cards_ptr[ card_d->id ];
					pos += scnprintf(buffer + pos, 500-pos, " %s (%d)\n", c->name, result );
				}
			}
		}

		if( ability_found ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
				int choice = do_dialog(player, player, card, -1, -1, buffer, 1);
				if( choice != 0 ){
				   int valid = 0;
				   for(i=0;i<grave_count;i++){
						card_data_t* card_d = &cards_data[ graveyard[i] ];
						if( card_d->cc[2] == 6 ){
							CardFunction ptFunction = card_d->code_pointer;
							int result = ptFunction(player, 0, EVENT_DREDGE);
							if( count_deck(player) >= result && ++valid == choice ){
								from_grave_to_hand(player, i, TUTOR_HAND);
								mill( player, result );
								suppress_draw = 1;
							}
						}
					}
				}
			}
		}
	}
}
/*
static int is_unearth(const card_data_t* cd)
{
	* Unearth.  (cc[2] == 9 or Viscera Dragger), is type creature, and is not any of:
   * Carrionette
   * Champion of Stray Souls
   * Chronosavant
   * Firewing Phoenix
   * Gangrenous Goliath
   * Glory
   * Gravecrawler
   * Jarad, Golgari Lich Lord
   * Masked Admirers
   * Necrosavant
   * Rot Farm Skeleton
   * Salvage Titan
   * Sekki, Seasons' Guide
   * Shard Phoenix
   * Skaab Ruinator
   * Tymaret, the Murder King
   * Worldheart Phoenix
   *
   * Why in the bloody blue blazes are all of those *and* unearth *and* flashback *and* planeswalkers all sharing cc[2]==9?
   * There's 256 possible values and we've used only 20 of them.
   */
/*
  if (cd->cc[2] != 9 && cd->id != CARD_ID_VISCERA_DRAGGER)
	return 0;

  if (!(cd->type & TYPE_CREATURE))
	return 0;

  switch (cd->id)
	{
	  case CARD_ID_CHRONOSAVANT:
	  case CARD_ID_FIREWING_PHOENIX:
	  case CARD_ID_GANGRENOUS_GOLIATH:
	  case CARD_ID_GLORY:
	  case CARD_ID_GRAVECRAWLER:
	  case CARD_ID_JARAD_GOLGARI_LICH_LORD:
	  case CARD_ID_MASKED_ADMIRERS:
	  case CARD_ID_NECROSAVANT:
	  case CARD_ID_ROT_FARM_SKELETON:
	  case CARD_ID_SALVAGE_TITAN:
	  case CARD_ID_SEKKI_SEASONS_GUIDE:
	  case CARD_ID_SHARD_PHOENIX:
	  case CARD_ID_SKAAB_RUINATOR:
	  case CARD_ID_TYMARET_THE_MURDER_KING:
	  case CARD_ID_WORLDHEART_PHOENIX:
		return 0;
	}

  return 1;
}
*/

static int activate_cards_in_hand(int player, int card, event_t event)
{
  if (IS_AI(player) && !get_setting(SETTING_AI_USES_RULES_ENGINE))
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  int homing_sliver_flag = get_trap_condition(player, TRAP_HOMING_SLIVER);
  int i;
  if (event == EVENT_CAN_ACTIVATE)
  {
    land_can_be_played |= LCBP_ACTIVATING_FROM_HAND;
    int available = 0;
    if (hand_count[player] > 0 && !(player_bits[player] & PB_HAND_ABILITIES_DISABLED))
    {
      for (i = 0; i < active_cards_count[player]; ++i)
      {
        card_instance_t *hand = get_card_instance(player, i);
        if ((hand->state & (STATE_INVISIBLE | STATE_IN_PLAY)) || hand->internal_card_id < 0)
          continue;
        int iid = hand->internal_card_id;
        if (cards_data[iid].cc[2] == 4 || cards_data[iid].cc[2] == 16 ||
            cards_data[iid].cc[2] == 23 || get_id(player, i) == CARD_ID_DRAGON_WINGS ||
            (homing_sliver_flag && has_subtype(player, i, SUBTYPE_SLIVER)))
        {
          int result = cards_data[iid].code_pointer(player, i, EVENT_CAN_ACTIVATE_FROM_HAND);
          if (result)
          {
            available = result;
            if (result == 99)
              goto can_return;
          }
        }
      }
      if (available)
        goto can_return;
    }
    const int *graveyard = get_grave(player);
    if (graveyard[0] != -1 && !(player_bits[player] & PB_GY_ABILITIES_DISABLED))
    {
      int count = count_graveyard(player);
      for (i = 0; i < count; ++i)
      {
        card_data_t *card_d = &cards_data[graveyard[i]];
        if ((card_d->cc[2] == 9 || card_d->id == CARD_ID_HOGAAK_ARISEN_NECROPOLIS ||
             card_d->id == CARD_ID_GRYFFS_BOON || card_d->id == CARD_ID_VINEWEFT ||
             card_d->id == CARD_ID_MARSHALING_CRY || card_d->id == CARD_ID_SKARRGAN_FIREBIRD ||
             card_d->cc[2] == 10 || card_d->id == CARD_ID_VISCERA_DRAGGER ||
             card_d->id == CARD_ID_HAAKON_STROMGALD_SCOURGE ||
             card_d->id == CARD_ID_UNDEAD_GLADIATOR || card_d->cc[2] == 27 ||
             card_d->id == CARD_ID_DIHADAS_PLOY || card_d->cc[2] == 17 || card_d->cc[2] == 21 ||
             (card_d->cc[2] >= 23 && card_d->cc[2] <= 25)))
        {
          int fake = add_card_to_hand(player, graveyard[i]);
          if (fake >= 0)
          {
            add_state(player, fake, STATE_INVISIBLE);
            --hand_count[player];
            int result = card_d->code_pointer(player, fake, EVENT_GRAVEYARD_ABILITY);
            type_t type = get_type_with_iid(player, fake, graveyard[i]);
            obliterate_card_and_recycle(player, fake);
            if ((!(result & (GA_GRAVE_ABILITY_MASK & ~GA_RETURN_TO_HAND)) && result) ||
                ((result & (GA_GRAVE_ABILITY_MASK & ~GA_RETURN_TO_HAND)) &&
                 !grafdiggers_cages(player, type, TUTOR_FROM_GRAVE)))
            {
              available = result;
              if (result & GA_INTERRUPT_ABILITY)
              {
                available = 99;
                goto can_return;
              }
            }
          }
        }
      }
      if (available)
        goto can_return;
    }
    if (check_rfg(player, CARD_ID_TORRENT_ELEMENTAL) && can_sorcery_be_played(player, event) &&
        has_mana_hybrid(player, 2, COLOR_BLACK, COLOR_GREEN, 3))
    {
      available = 1;
      goto can_return;
    }
    if (check_rfg(player, CARD_ID_ETERNAL_SCOURGE) &&
        can_legally_play_csvid_now(player, CARD_ID_ETERNAL_SCOURGE, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_ETERNAL_SCOURGE))
    {
      available = 1;
      goto can_return;
    }
    if (check_rfg(player, CARD_ID_SQUEE_THE_IMMORTAL) &&
        can_legally_play_csvid_now(player, CARD_ID_SQUEE_THE_IMMORTAL, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_SQUEE_THE_IMMORTAL))
    {
      available = 1;
      goto can_return;
    }
    if (check_rfg(player, CARD_ID_MISTHOLLOW_GRIFFIN) &&
        can_legally_play_csvid_now(player, CARD_ID_MISTHOLLOW_GRIFFIN, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_MISTHOLLOW_GRIFFIN))
      available = 1;
  can_return:
    land_can_be_played &= ~LCBP_ACTIVATING_FROM_HAND;
    return available;
  }
  if (event == EVENT_ACTIVATE)
  {
    land_can_be_played |= LCBP_ACTIVATING_FROM_HAND;
    hack_allow_sorcery_if_rules_engine_is_only_effect = 1;
    unsigned int cb = 3000;
    char buffer[cb];
    int opts[100][2];
    int opts_count = 1;
    int pos = scnprintf(buffer, cb, " Cancel\n");
    if (!(player_bits[player] & PB_HAND_ABILITIES_DISABLED))
    {
      for (i = 0; i < active_cards_count[player]; i++)
      {
        card_instance_t *hand_instance = get_card_instance(player, i);
        if (!(hand_instance->state & STATE_INVISIBLE) && !(hand_instance->state & STATE_IN_PLAY))
        {
          int id = hand_instance->internal_card_id;
          if (id > -1)
          {
            int can_be_activated = 0;
            int csvid = cards_data[id].id;
            switch (csvid)
            {
            case CARD_ID_MARSHALING_CRY:
            case CARD_ID_SILENT_BLADE_ONI:
            case CARD_ID_VISCERA_DRAGGER:
            case CARD_ID_DRAGON_WINGS:
              can_be_activated = 1;
              break;
            default:
              break;
            }
            // Cycling / Basic landcycling / other activation from hand
            if (cards_data[id].cc[2] == 4 || cards_data[id].cc[2] == 23)
            {
              can_be_activated = 1;
            }
            // Forecast: could be used only during your upkeeps, only on your turn and only once per
            // turn
            if (cards_data[id].cc[2] == 16 && current_turn == player &&
                current_phase == PHASE_UPKEEP && count_upkeeps(player) &&
                hand_instance->info_slot == 0)
            {
              can_be_activated = 1;
            }
            // Homing Sliver gives "Slivercycling" to all Slivers in your hand
            if (homing_sliver_flag && has_subtype(player, i, SUBTYPE_SLIVER))
            {
              can_be_activated = 1;
            }
            if (can_be_activated)
            {
              CardFunction ptFunction = cards_data[id].code_pointer;
              if (ptFunction(player, i, EVENT_CAN_ACTIVATE_FROM_HAND))
              {
                card_ptr_t *c = cards_ptr[get_id(player, i)];
                pos += scnprintf(buffer + pos, cb - pos, " Activate %s\n", c->name);
                opts[opts_count][0] = i;
                opts[opts_count][1] = GA_EXTRA_ACTIVATE_FROM_HAND;
                opts_count++;
              }
            }
          }
        }
      }

      // also check for cards in the graveyard
    }
    const int *graveyard = get_grave(player);
    int creature_cage = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_GRAVE);
    int spell_cage = grafdiggers_cages(player, TYPE_SPELL, TUTOR_FROM_GRAVE);
    if (!(player_bits[player] & PB_GY_ABILITIES_DISABLED))
    {
      int count = count_graveyard(player);
      for (i = 0; i < count; ++i)
      {
        card_data_t *card_d = &cards_data[graveyard[i]];
        if (!(card_d->cc[2] == 9 || card_d->id == CARD_ID_HOGAAK_ARISEN_NECROPOLIS ||
              card_d->id == CARD_ID_GRYFFS_BOON || card_d->id == CARD_ID_VINEWEFT ||
              card_d->id == CARD_ID_MARSHALING_CRY || card_d->id == CARD_ID_SKARRGAN_FIREBIRD ||
              card_d->cc[2] == 10 || card_d->id == CARD_ID_VISCERA_DRAGGER ||
              card_d->id == CARD_ID_HAAKON_STROMGALD_SCOURGE ||
              card_d->id == CARD_ID_UNDEAD_GLADIATOR || card_d->cc[2] == 27 ||
              card_d->id == CARD_ID_DIHADAS_PLOY || card_d->cc[2] == 17 || card_d->cc[2] == 21 ||
              (card_d->cc[2] >= 23 && card_d->cc[2] <= 25)))
          continue;
        int fake = add_card_to_hand(player, graveyard[i]);
        if (fake < 0)
          continue;
        add_state(player, fake, STATE_INVISIBLE);
        --hand_count[player];
        int result = call_card_function(player, fake, EVENT_GRAVEYARD_ABILITY);
        if (result)
        {
          const char *name = cards_ptr[card_d->id]->name;
          if (card_d->type & (TYPE_CREATURE | TYPE_LAND))
          {
            if (!creature_cage || !(card_d->type & TYPE_CREATURE))
            {
              if (result & GA_PLAYABLE_FROM_GRAVE)
              {
                pos += scnprintf(buffer + pos, cb - pos, " Play %s\n", name);
                opts[opts_count][0] = i;
                opts[opts_count++][1] = result;
              }
              if (result & GA_UNEARTH)
              {
                pos += scnprintf(buffer + pos, cb - pos, " Unearth %s\n", name);
                opts[opts_count][0] = i;
                opts[opts_count++][1] = result;
              }
              if (result & GA_ESCAPE)
              {
                pos += scnprintf(buffer + pos, cb - pos, " Escape %s\n", name);
                opts[opts_count][0] = i;
                opts[opts_count++][1] = result;
              }
              if (result & GA_RETURN_TO_PLAY)
              {
                pos += scnprintf(buffer + pos, cb - pos, " Return %s to play\n", name);
                opts[opts_count][0] = i;
                opts[opts_count++][1] = result;
              }
            }
            if (result & GA_BASIC)
            {
              const char *fmt = result & GA_EMBALM                      ? " Embalm %s\n"
                                : result & GA_ETERNALIZE                ? " Eternalize %s\n"
                                : (result & GA_SCAVENGE) == GA_SCAVENGE ? " Scavenge %s\n"
                                                                        : " Activate %s\n";
              pos += scnprintf(buffer + pos, cb - pos, fmt, name);
              opts[opts_count][0] = i;
              opts[opts_count++][1] = result;
            }
            if (result & GA_RETURN_TO_HAND)
            {
              pos += scnprintf(buffer + pos, cb - pos, " Return %s to hand\n", name);
              opts[opts_count][0] = i;
              opts[opts_count++][1] = result;
            }
            if (result & GA_PUT_ON_TOP_OF_DECK)
            {
              pos += scnprintf(buffer + pos, cb - pos, " Put %s on top of deck\n", name);
              opts[opts_count][0] = i;
              opts[opts_count++][1] = result;
            }
          }
          else if (!spell_cage || !(card_d->type & TYPE_SPELL))
          {
            const char *fmt = NULL;
            if (result & GA_RETURN_TO_HAND)
              fmt = " Return %s to hand\n";
            else if (card_d->cc[2] == 24)
              fmt = " Pay %s's Aftermath\n";
            else if (result & GA_ESCAPE)
              fmt = " Escape %s\n";
            else if (can_legally_play_iid(player, graveyard[i]))
            {
              if (card_d->cc[2] == 10)
                fmt = " Retrace %s\n";
              else if (card_d->cc[2] == 25 || card_d->id == CARD_ID_DIHADAS_PLOY)
                fmt = " Jump-start %s\n";
              else
                fmt = " Flashback %s\n";
            }
            if (fmt)
            {
              pos += scnprintf(buffer + pos, cb - pos, fmt, name);
              opts[opts_count][0] = i;
              opts[opts_count++][1] = result;
            }
          }
        }
        obliterate_card_and_recycle(player, fake);
      }
    }
    if (check_rfg(player, CARD_ID_TORRENT_ELEMENTAL) && can_sorcery_be_played(player, event) &&
        has_mana_hybrid(player, 2, COLOR_BLACK, COLOR_GREEN, 3))
    {
      pos += scnprintf(buffer + pos, cb - pos, " Return Torrent Elemental to play\n");
      opts[opts_count][0] = CARD_ID_TORRENT_ELEMENTAL;
      opts[opts_count++][1] = 0;
    }
    if (check_rfg(player, CARD_ID_ETERNAL_SCOURGE) &&
        can_legally_play_csvid_now(player, CARD_ID_ETERNAL_SCOURGE, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_ETERNAL_SCOURGE))
    {
      pos += scnprintf(buffer + pos, cb - pos, " Cast Eternal Scourge\n");
      opts[opts_count][0] = CARD_ID_ETERNAL_SCOURGE;
      opts[opts_count][1] = 0;
      ++opts_count;
    }
    if (check_rfg(player, CARD_ID_SQUEE_THE_IMMORTAL) &&
        can_legally_play_csvid_now(player, CARD_ID_SQUEE_THE_IMMORTAL, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_SQUEE_THE_IMMORTAL))
    {
      pos += scnprintf(buffer + pos, cb - pos, " Cast Squee, the Immortal\n");
      opts[opts_count][0] = CARD_ID_SQUEE_THE_IMMORTAL;
      opts[opts_count][1] = 0;
      ++opts_count;
    }
    if (check_rfg(player, CARD_ID_MISTHOLLOW_GRIFFIN) &&
        can_legally_play_csvid_now(player, CARD_ID_MISTHOLLOW_GRIFFIN, event) &&
        has_mana_to_cast_id(player, event, CARD_ID_MISTHOLLOW_GRIFFIN))
    {
      pos += scnprintf(buffer + pos, cb - pos, " Cast Misthollow Griffin\n");
      opts[opts_count][0] = CARD_ID_MISTHOLLOW_GRIFFIN;
      opts[opts_count][1] = 0;
    }

    for (i = 0; i < 5; ++i)
      instance->targets[i].player = instance->targets[i].card = -1;
    int choice = do_dialog(player, player, card, -1, -1, buffer, 1);
    if (choice == 0)
      goto activate_failed;
    int mode = opts[choice][1];
    int selected = opts[choice][0];
    if (mode == 0)
    {
      if (selected == CARD_ID_TORRENT_ELEMENTAL)
      {
        if (charge_mana_hybrid(player, card, 2, COLOR_BLACK, COLOR_GREEN, 3))
        {
          remove_card_from_rfg(player, selected);
          instance->targets[2].player = 0;
          instance->targets[2].card = selected;
        }
      }
      else if (charge_mana_from_id(player, -1, event, selected))
      {
        remove_card_from_rfg(player, selected);
        instance->targets[2].player = 0;
        instance->targets[2].card = selected;
      }
    }
    else if (mode == GA_EXTRA_ACTIVATE_FROM_HAND)
    {
      card_instance_t *hand = get_card_instance(player, selected);
      int iid = hand->internal_card_id;
      CardFunction fn = cards_data[iid].code_pointer;
      if (!fn(player, selected, EVENT_CAN_ACTIVATE_FROM_HAND))
        goto activate_failed;
      int result = fn(player, selected, EVENT_ACTIVATE_FROM_HAND);
      if (!result)
        goto activate_failed;
      if (result == 2)
        activate_astral_slide();
      if (cards_data[iid].cc[2] == 16)
        get_card_instance(player, selected)->info_slot = 1;
      hand->number_of_targets = 0;
      instance->targets[0] = hand->targets[0];
      instance->targets[2].player = GA_EXTRA_ACTIVATE_FROM_HAND;
      instance->targets[2].card = selected;
    }
    else if (mode & GA_GRAVE_ABILITY_MASK)
    {
      int fake = add_card_to_hand(player, graveyard[selected]);
      if (fake < 0)
        goto activate_failed;
      add_state(player, fake, STATE_INVISIBLE);
      --hand_count[player];
      int source = graveyard_source[player][selected];
      get_card_instance(player, fake)->targets[3].card = source;
      get_card_instance(player, fake)->targets[3].player = selected;
      int paid = call_card_function(player, fake, EVENT_PAY_FLASHBACK_COSTS);
      if (!paid || spell_fizzled == 1)
      {
        obliterate_card(player, fake);
        goto activate_failed;
      }
      selected = find_in_graveyard_by_source(player, source, selected);
      if (mode & GA_PLAYABLE_FROM_GRAVE)
      {
        if (paid == GAPAID_REMOVE)
          play_card_in_grave_for_free(player, player, selected);
        else if (paid == GAPAID_EXILE)
          flashback_spell(player, selected);
        cant_be_responded_to = 1;
      }
      else if (mode & GA_ESCAPE)
      {
        if (paid == GAPAID_REMOVE)
          escape_spell(player, selected);
      }
      else
      {
        instance->targets[2].player = mode;
        instance->targets[2].card = graveyard[selected];
        if (paid == GAPAID_REMOVE)
          remove_card_from_grave(player, selected);
        else if (paid == GAPAID_EXILE)
          rfg_card_from_grave(player, selected);
      }
      obliterate_card(player, fake);
    }
    land_can_be_played &= ~LCBP_ACTIVATING_FROM_HAND;
    hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
    return 0;
  activate_failed:
    land_can_be_played &= ~LCBP_ACTIVATING_FROM_HAND;
    spell_fizzled = 1;
    hack_allow_sorcery_if_rules_engine_is_only_effect = 0;
    return 0;
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int iid = instance->targets[2].card;
    if (iid < 0)
      return 0;
    if (iid == CARD_ID_TORRENT_ELEMENTAL)
    {
      int added = add_card_to_hand(player, get_internal_card_id_from_csv_id(iid));
      add_state(player, added, STATE_TAPPED);
      put_into_play(player, added);
      return 0;
    }
    if (iid == CARD_ID_ETERNAL_SCOURGE || iid == CARD_ID_MISTHOLLOW_GRIFFIN)
    {
      play_card_in_exile_for_free(player, player, iid);
      return 0;
    }
    if (iid == CARD_ID_SQUEE_THE_IMMORTAL)
    {
      int added = play_card_in_exile_for_free(player, player, iid);
      if (cards_data[get_commander_id(player, 1)].id == CARD_ID_SQUEE_THE_IMMORTAL)
        commander_enters_the_battlefield(player, locate_edh_card(player), player, added, 1);
      return 0;
    }
    int mode = instance->targets[2].player;
    if (mode & GA_EXTRA_ACTIVATE_FROM_HAND)
    {
      card_instance_t *hand = get_card_instance(player, iid);
      card_data_t *data = get_card_data(player, iid);
      call_card_fn(data->code_pointer, hand, player, iid, EVENT_RESOLVE_ACTIVATION_FROM_HAND);
      if (data->cc[2] == 16)
        hand->info_slot = 1;
      return 0;
    }
    int added = add_card_to_hand(player, iid);
    add_state(player, added, STATE_INVISIBLE);
    --hand_count[player];
    if (mode & GA_BASIC)
    {
      call_card_function(player, added, EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY);
      obliterate_card(player, added);
    }
    if (mode & GA_RETURN_TO_HAND)
    {
      remove_state(player, added, STATE_INVISIBLE);
      ++hand_count[player];
    }
    if (mode & GA_PUT_ON_TOP_OF_DECK)
    {
      remove_state(player, added, STATE_INVISIBLE);
      ++hand_count[player];
      put_on_top_of_deck(player, added);
    }
    if ((mode & GA_RETURN_TO_PLAY_MODIFIED) == GA_RETURN_TO_PLAY_MODIFIED)
      call_card_function(player, added, EVENT_RETURN_TO_PLAY_FROM_GRAVE_MODIFIED);
    if (mode & GA_UNEARTH)
    {
      convert_to_token(player, added);
      set_special_flags(player, added, SF_UNEARTH | SF_NOT_CAST);
      set_special_flags3(player, added, SF3_REANIMATED);
    }
    if (mode & (GA_RETURN_TO_PLAY | GA_UNEARTH | GA_ESCAPE))
    {
      remove_state(player, added, STATE_INVISIBLE);
      ++hand_count[player];
      put_into_play(player, added);
    }
    if (mode & GA_UNEARTH)
      create_targetted_legacy_effect(player, added, effect_unearth, player, added);
    if ((mode & GA_RETURN_TO_PLAY_WITH_EFFECT) == GA_RETURN_TO_PLAY_WITH_EFFECT)
      call_card_function(player, added, EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY);
    if (mode & (GA_BASIC | GA_RETURN_TO_HAND | GA_PUT_ON_TOP_OF_DECK | GA_RETURN_TO_PLAY |
                GA_UNEARTH | GA_ESCAPE))
      for (i = 0; i < active_cards_count[player]; ++i)
        if (in_play(player, i) && get_id(player, i) == CARD_ID_LABORATORY_DRUDGE)
          get_card_instance(player, i)->info_slot = 66;
    return 0;
  }
  if (event == EVENT_CLEANUP && (get_rules_engine_infos(player) & REF_FORECAST))
    for (i = 0; i < active_cards_count[player]; ++i)
      if (in_hand(player, i) && get_card_data(player, i)->cc[2] == 16)
        get_card_instance(player, i)->info_slot = 0;
  return 0;
}

int get_flashback(int player, int card){
	if( check_special_flags(player, card, SF_FLASHBACK) ){
		return 1;
	}
	return 0;
}

#ifdef CARDS_IN_HAND_THIS_TURN	// unused
static int was_card_in_hand(int player, int card){
	return 1; // comment out this code since track_hand is so slow
	int i=0;
	while( cards_in_hand_this_turn[player][i] != -1){
		if( cards_in_hand_this_turn[player][i++] == card ){
			return 1;
		}
	}
	return 0;
}
#endif

#ifdef CARDS_IN_HAND_THIS_TURN	// unused
static void track_hand(int player, int card, event_t event ){
	int i;
	if( comes_into_play(player, card, event) == 1 && player == HUMAN){
		for(i=0;i<100;i++){
			cards_in_hand_this_turn[HUMAN][i] = -1;
			cards_in_hand_this_turn[AI][i] = -1;
		}
	}

	int p;
	for (p=0;p<2;p++){
		for(i=0;i<active_cards_count[p]; i++){
			card_instance_t *card_instance = get_card_instance(p, i);
			if( ! ( card_instance->state & STATE_INVISIBLE ) && ! ( card_instance->state & STATE_IN_PLAY )  ){
				int j=0;
				while( cards_in_hand_this_turn[p][j] != -1 && j < 100 ){
					if( cards_in_hand_this_turn[p][j] == i ){
						break;
					}
					j++;
				}
				cards_in_hand_this_turn[p][j] = i;
			}
		}
	}

	if( event == EVENT_CLEANUP ){
		for(i=0;i<100;i++){
			cards_in_hand_this_turn[HUMAN][i] = -1;
			cards_in_hand_this_turn[AI][i] = -1;
		}
	}
}
#endif

static int get_rules_engine_card(int player){
	return rules_engine_card[player];
}

static void reset_storm_count(int player, int card)
{
  target_definition_t td;

  if (player != HUMAN)
    return;

  base_target_definition(HUMAN, card, &td, TYPE_CREATURE);
  td.required_subtype = SUBTYPE_WEREWOLF;
  if (!can_target(&td))
  {
    for (int p = 0; p < 2; ++p)
    {
      int rec = rules_engine_card[p];
      if (rec >= 0)
      {
        increase_trap_condition_impl(p, 0x403, 0, 0);
        increase_trap_condition_impl(p, 0x404, 0, 0);
        increase_trap_condition_impl(p, 0x405, 0, 0);
        increase_trap_condition_impl(p, 0x406, 0, 0);
        increase_trap_condition_impl(p, 0x407, 0, 0);
        remove_all_counters(HUMAN, card, p, rec, COUNTER_VORTEX);
        remove_all_counters(HUMAN, card, p, rec, COUNTER_BLOOD);
        remove_all_counters(HUMAN, card, p, rec, COUNTER_CHARGE);
        remove_all_counters(HUMAN, card, p, rec, COUNTER_DEVOTION);
      }
    }
  }
  else
  {
    int human_count = rules_engine_card[HUMAN] == -1
                          ? 0
                          : count_counters(HUMAN, rules_engine_card[HUMAN], COUNTER_VORTEX);
    int ai_count =
        rules_engine_card[AI] == -1 ? 0 : count_counters(AI, rules_engine_card[AI], COUNTER_VORTEX);
    for (int p = 0; p < 2; ++p)
    {
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        if (in_play(p, c))
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (has_subtype(p, c, SUBTYPE_WEREWOLF) && instance->targets[9].card != 0x42)
          {
            instance->targets[9].card = 0x42;
            instance->targets[8].player = human_count;
            instance->targets[8].card = ai_count;
          }
          if (get_id(p, c) == CARD_ID_RULES_ENGINE)
          {
            increase_trap_condition_impl(p, 0x403, 0, 0);
            increase_trap_condition_impl(p, 0x404, 0, 0);
            increase_trap_condition_impl(p, 0x405, 0, 0);
            increase_trap_condition_impl(p, 0x406, 0, 0);
            increase_trap_condition_impl(p, 0x407, 0, 0);
            remove_all_counters(HUMAN, card, p, c, COUNTER_VORTEX);
            remove_all_counters(HUMAN, card, p, c, COUNTER_BLOOD);
            remove_all_counters(HUMAN, card, p, c, COUNTER_CHARGE);
            remove_all_counters(HUMAN, card, p, c, COUNTER_DEVOTION);
          }
        }
      }
    }
  }
}

static void keep_storm_count(int player, int card, event_t event)
{
  enum
  {
    TRAP_NONCREATURES_PLAYED = 0x408
  };
  int not_cast;
  if (player != HUMAN)
    return;

  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card &&
      affected_card_controller == HUMAN && reason_for_trigger_controller == HUMAN &&
      (not_cast = check_special_flags(trigger_cause_controller, trigger_cause, SF_NOT_CAST),
       event == EVENT_END_TRIGGER && !not_cast))
  {
    card_instance_t *played = get_card_instance(trigger_cause_controller, trigger_cause);
    uint8_t types = cards_data[played->internal_card_id].type;
    if (!(types & TYPE_LAND))
    {
      int mode;
      if (types & TYPE_CREATURE)
        mode = STORM_CREATURE;
      else
      {
        increase_trap_condition_impl(trigger_cause_controller, TRAP_NONCREATURES_PLAYED, 1, 1);
        mode = 0;
      }
      if (types & TYPE_ENCHANTMENT)
        mode |= STORM_ENCHANTMENT;
      if ((types & TYPE_SPELL) && !(types & TYPE_CREATURE))
        mode |= STORM_INSTANT_SORCERY;
      if (get_id(trigger_cause_controller, trigger_cause) == CARD_ID_PEER_THROUGH_DEPTHS)
        increase_trap_condition_impl(trigger_cause_controller, TRAP_PEER_THROUGH_DEPTHS, 1, 1);
      if (get_id(trigger_cause_controller, trigger_cause) == CARD_ID_REACH_THROUGH_MISTS)
        increase_trap_condition_impl(trigger_cause_controller, TRAP_REACH_THROUGH_MISTS, 1, 1);
      unsigned int colors = get_color(trigger_cause_controller, trigger_cause);
      increment_storm_count(trigger_cause_controller, mode | (colors & COLOR_TEST_ANY_COLORED));
    }
  }
  else if (event == EVENT_CAN_SKIP_TURN)
    reset_storm_count(player, card);
}

#if 0	// unused
int get_turn_count(void)
{
  return get_trap_condition(HUMAN, TRAP_TURN_COUNT) + get_trap_condition(AI, TRAP_TURN_COUNT);
}
#endif

int get_specific_turn_count(int player){
  return get_trap_condition(player, TRAP_TURN_COUNT);
}

event_t get_actual_event(void){
	return actual_event;
}

static void parse_grave_for_centaur_vinecrasher(int player, int card){
	const int *grave = get_grave(player);
	int count = count_graveyard(player)-1;
	while( count > -1 ){
			if( has_mana(player, COLOR_GREEN, 2) && cards_data[grave[count]].id == CARD_ID_CENTAUR_VINECRASHER  ){
				int choice = do_dialog(player, player, card, -1, -1, " Return Centaur Vinecrasher to your hand\n Do not return", 0);
				if( choice == 0 ){
					charge_mana(player, COLOR_GREEN, 2);
					if( spell_fizzled != 1 ){
						add_card_to_hand(player, grave[count]);
						remove_card_from_grave(player, count);
					}
				}
			}
			count--;
	}
}

static int prized_amalgam_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		seek_grave_for_id_to_reanimate(player, card, player, CARD_ID_PRIZED_AMALGAM, REANIMATE_TAP);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

	enum{
		GC_BLOODGHAST = 		1<<0,
		GC_VENGEVINE =	 		1<<1,
		GC_WONDER =		 		1<<2,
		GC_SWORD_OF_THE_MEEK =	1<<3,
		GC_DEARLY_DEPARTED =	1<<4,
		GC_CHANDRAS_PHOENIX =	1<<5,
		GC_BLOOD_SPEAKER =	 	1<<6,
		GC_SOSUKES_SUMMONS =	1<<7,
		GC_VEILBORN_GHOUL =	 	1<<8,
		GC_NETHER_TRAITOR =	 	1<<9,
		GC_MASKED_ADMIRERS =	1<<10,
		GC_AUNTIES_SNITCH =		1<<11,
		GC_REACH_OF_BRANCHES =	1<<12,
		GC_BRAWN =				1<<13,
		GC_FILTH =				1<<14,
		GC_VALOR =				1<<15,
		GC_BLADEWINGS_THRALL =	1<<16,
		GC_DRAGON_ENCHANTMENTS =1<<17,
		GC_RECOVER =			1<<18,
		GC_PYREWILD_SHAMAN =	1<<19,
		GC_ANGER =				1<<20,
		GC_AKOUM_FIREBIRD =		1<<21,
		GC_CENTAUR_VINECRASHER =1<<22,
		GC_KOZILEKS_RETURN =	1<<23,
		GC_PRIZED_AMALGAM =		1<<24,
		GC_OVALCHASE_DAREDEVIL= 1<<25,
 GC_DARK_INTIMATIONS = 1<<26,
 GC_BONEYARD_SCOURGE = 1<<27,
 GC_GATE_COLOSSUS = 1<<28,
 GC_ARCLIGHT_PHOENIX = 1<<29,
 GC_DEATHLESS_KNIGHT = 1<<30,
 GC_PUNISHING_FIRE = (int)(1U<<31),
	};

/* ManalinkEh 2022: 0x26c5ff0; original functions/rules_engine.c:439. */
int companion_effect(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
    if (result != 0)
    {
      result = has_mana(player, 0, 3);
      return (unsigned int)(result != 0);
    }
  }
  else if (((event == EVENT_ACTIVATE) &&
            (result = charge_mana(player, COLOR_COLORLESS, 3), result != 0)) &&
           (spell_fizzled != 1))
  {
    instance = get_card_instance(player, card);
    result = get_internal_card_id_from_csv_id((unsigned int)instance->display_pic_csv_id);
    add_card_to_hand(player, result);
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

static void resolve_graveyard_eot_triggers(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (player == current_turn && eot_trigger(player, card, event))
  {
    int count = count_graveyard(player);
    const int* grave = get_grave(player);
    for (int i = 0; i < count; ++i)
      if (cards_data[grave[i]].cc[2] == 0x1c)
      {
        if (event == EVENT_TRIGGER)
          event_result |= RESOLVE_TRIGGER_MANDATORY;
        else if (event == EVENT_RESOLVE_TRIGGER)
          for (int pos = count-1; pos >= 0; --pos)
            if (cards_data[grave[pos]].cc[2] == 0x1c)
            {
              int iid = grave[pos];
              int temporary = add_card_to_hand(player, iid);
              instance->number_of_targets = 1;
              card_instance_t* temporary_instance = get_card_instance(player, temporary);
              temporary_instance->state |= STATE_INVISIBLE;
              --hand_count[player];
              int result = cards_data[iid].code_pointer(player, temporary, EVENT_GRAVEYARD_ABILITY_EOT);
              if (result == -1)
                remove_card_from_grave(player, pos);
              else if (result == -2)
                obliterate_card_and_recycle(player, temporary);
              else
              {
                obliterate_card_and_recycle(player, temporary);
                remove_card_from_grave(player, result);
              }
            }
        break;
      }
  }
}

/* ManalinkEh 2022: 0x26c7f10; original functions/rules_engine.c:1534. */
int get_aftermath(int player, int card)
{
  int result;
  result = check_special_flags(player, card, 0x200000);
  return (unsigned int)(result != 0);
}

int card_coa_special_effect(int player, int card, event_t event);

static int get_graveyard_counts(int player)
{

  int ids[] = {
      CARD_ID_BLOODGHAST,          // 0
      CARD_ID_VENGEVINE,           // 1
      CARD_ID_WONDER,              // 2
      CARD_ID_SWORD_OF_THE_MEEK,   // 3
      CARD_ID_DEARLY_DEPARTED,     // 4
      CARD_ID_CHANDRAS_PHOENIX,    // 5
      CARD_ID_BLOOD_SPEAKER,       // 6
      CARD_ID_SOSUKES_SUMMONS,     // 7
      CARD_ID_VEILBORN_GHOUL,      // 8
      CARD_ID_NETHER_TRAITOR,      // 9
      CARD_ID_MASKED_ADMIRERS,     // 10
      CARD_ID_AUNTIES_SNITCH,      // 11
      CARD_ID_REACH_OF_BRANCHES,   // 12
      CARD_ID_BRAWN,               // 13
      CARD_ID_FILTH,               // 14
      CARD_ID_VALOR,               // 15
      CARD_ID_BLADEWINGS_THRALL,   // 16
      -1,                          // 17
      -1,                          // 18
      CARD_ID_PYREWILD_SHAMAN,     // 19
      CARD_ID_ANGER,               // 20
      CARD_ID_AKOUM_FIREBIRD,      // 21
      CARD_ID_CENTAUR_VINECRASHER, // 22
      CARD_ID_KOZILEKS_RETURN,     // 23
      CARD_ID_PRIZED_AMALGAM,      // 24
      CARD_ID_OVALCHASE_DAREDEVIL, // 25
      CARD_ID_DARK_INTIMATIONS,    // 26
      CARD_ID_BONEYARD_SCOURGE,    // 27
      CARD_ID_GATE_COLOSSUS,       // 28
      CARD_ID_ARCLIGHT_PHOENIX,    // 29
      CARD_ID_DEATHLESS_KNIGHT,    // 30
      CARD_ID_PUNISHING_FIRE       // 31
  };

  int result = 0;
  const int *grave = get_grave(player);
  int count = 0;
  while (grave[count] != -1)
  {
    card_data_t *card_d = &cards_data[grave[count]];
    if (card_d->cc[2] == 18)
    {
      result |= GC_DRAGON_ENCHANTMENTS;
    }
    if (card_d->cc[2] == 20)
    {
      result |= GC_RECOVER;
    }
    unsigned int k;
    for (k = 0; k < (sizeof(ids) / sizeof(ids[0])); k++)
    {
      if (card_d->id == ids[k])
      {
        result |= 1 << k;
        break;
      }
    }
    count++;
  }
  return result;
}

static void static_graveyard_abilities(int player, int card, event_t event)
{
  enum
  {
    GT_CIP_BLOODGHAST = 1,
    GT_CIP_SWORD_OF_THE_MEEK = 2,
    GT_CIP_DEARLY_DEPARTED = 4,
    GT_CIP_BLOOD_SPEAKER = 8,
    GT_CIP_SOSUKES_SUMMONS = 16,
    GT_CIP_VEILBORN_GHOUL = 32,
    GT_CIP_REACH_OF_BRANCHES = 64,
    GT_CIP_BLADEWINGS_THRALL = 128,
    GT_CIP_DRAGON_ENCHANTMENTS = 256,
    GT_CIP_AKOUM_FIREBIRD = 512,
    GT_CIP_PRIZED_AMALGAM = 1024,
    GT_CIP_OVALCHASE_DAREDEVIL = 2048,
    GT_CIP_GATE_COLOSSUS = 4096
  };
  enum
  {
    GT_SC_VENGEVINE = 1,
    GT_SC_MASKED_ADMIRERS = 2,
    GT_SC_KOZILEKS_RETURN = 4,
    GT_SC_PRIZED_AMALGAM = 8,
    GT_SC_DARK_INTIMATIONS = 16
  };
  enum
  {
    GT_DT_CHANDRAS_PHOENIX = 1,
    GT_DT_AUNTIES_SNITCH = 2,
    GT_DT_PYREWILD_SHAMAN = 4
  };
  enum
  {
    GTA_CREATURE_YOU_CONTROL_DIED = 1
  };
  enum
  {
    GT_LG_DEATHLESS_KNIGHT = 1,
    GT_LG_PUNISHING_FIRE = 2
  };

  if (((unsigned int)player_bits[player] >> 24) & 2)
    return;

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player && trigger_cause_controller == player)
  {
    unsigned int graveyard_mask, triggers;
    int result;
    graveyard_mask = get_graveyard_counts(player);
    triggers = graveyard_mask & 1;
    if (triggers != 0)
    {
      result = is_what(player, trigger_cause, 1);
      triggers = (unsigned int)(result != 0);
    }
    if ((((graveyard_mask & 8) != 0) &&
         (result = get_power(trigger_cause_controller, trigger_cause), result == 1)) &&
        (result = get_toughness(trigger_cause_controller, trigger_cause), result == 1))
    {
      triggers = triggers | GT_CIP_SWORD_OF_THE_MEEK;
    }
    if ((((graveyard_mask & 0x10) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 2), result != 0)) &&
        (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_HUMAN), result != 0))
    {
      triggers = triggers | GT_CIP_DEARLY_DEPARTED;
    }
    if ((((graveyard_mask & 0x40) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 0x47), result != 0)) &&
        (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_DEMON), result != 0))
    {
      triggers = triggers | GT_CIP_BLOOD_SPEAKER;
    }
    if (((((graveyard_mask & 0x80) != 0) &&
          (result = is_what(trigger_cause_controller, trigger_cause, 2), result != 0)) &&
         (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_SNAKE),
          result != 0)) &&
        (result = is_token(trigger_cause_controller, trigger_cause), result == 0))
    {
      triggers = triggers | GT_CIP_SOSUKES_SUMMONS;
    }
    if ((((graveyard_mask & 0x100) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 1), result != 0)) &&
        (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_SWAMP), result != 0))
    {
      triggers = triggers | GT_CIP_VEILBORN_GHOUL;
    }
    if ((((graveyard_mask & 0x1000) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 1), result != 0)) &&
        (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_FOREST),
         result != 0))
    {
      triggers = triggers | GT_CIP_REACH_OF_BRANCHES;
    }
    if ((((graveyard_mask & 0x10000) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 0x47), result != 0)) &&
        (result = has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_DRAGON),
         result != 0))
    {
      triggers = triggers | GT_CIP_BLADEWINGS_THRALL;
    }
    if ((((graveyard_mask & 0x20000) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 2), result != 0)) &&
        (result = get_cmc(trigger_cause_controller, trigger_cause), 5 < result))
    {
      triggers = triggers | GT_CIP_DRAGON_ENCHANTMENTS;
    }
    if ((((graveyard_mask & 0x200000) != 0) &&
         (result = is_what(trigger_cause_controller, trigger_cause, 1), result != 0)) &&
        (result = has_mana_multi(player, 4, 0, 0, 0, 2, 0), result != 0))
    {
      triggers = triggers | GT_CIP_AKOUM_FIREBIRD;
    }
    if (((((graveyard_mask & 0x1000000) != 0) &&
          (result = is_what(trigger_cause_controller, trigger_cause, 2), result != 0)) &&
         (result = check_special_flags3(trigger_cause_controller, trigger_cause, 1),
          result != 0)) &&
        (result = get_owner(trigger_cause_controller, trigger_cause), result == player))
    {
      triggers = triggers | GT_CIP_PRIZED_AMALGAM;
    }

    if ((graveyard_mask & 0x2000000) && is_what(trigger_cause_controller, trigger_cause, 0x40))
      triggers |= GT_CIP_OVALCHASE_DAREDEVIL;
    if ((graveyard_mask & 0x10000000) &&
        has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_GATE))
      triggers |= GT_CIP_GATE_COLOSSUS;
    if (triggers)
    {
      int cause_card = trigger_cause;
      int cause_player = trigger_cause_controller;
      if (event == EVENT_TRIGGER)
        event_result |= 2;
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        int *graveyard = graveyard_ptr_mutable[player];
        int count = count_graveyard(player);
        for (int index = count - 1; index >= 0; --index)
        {
          int iid = graveyard[index];
          uint16_t id = cards_data[iid].id;
          switch (id)
          {
          case CARD_ID_DEARLY_DEPARTED:
            if (triggers & GT_CIP_DEARLY_DEPARTED)
              add_counters(cause_player, cause_card, cause_player, cause_card, COUNTER_P1_P1, 1);
            break;
          case CARD_ID_SWORD_OF_THE_MEEK:
            if ((triggers & GT_CIP_SWORD_OF_THE_MEEK) && (duh_mode(player) ||
                                   do_dialog(player, player, card, -1, -1,
                                             " Return Sword of the Meek\tto play\n Pass", 0) == 0))
              equip_target_creature(
                  player, reanimate_permanent(player, card, player, index, REANIMATE_DEFAULT),
                  cause_player, cause_card);
            break;
          case CARD_ID_BLOOD_SPEAKER:
            if (triggers & GT_CIP_BLOOD_SPEAKER)
            {
              add_card_to_hand(player, iid);
              remove_card_from_grave(player, index);
            }
            break;
          case CARD_ID_SOSUKES_SUMMONS:
          case CARD_ID_REACH_OF_BRANCHES:
          case CARD_ID_VEILBORN_GHOUL:
          {
            unsigned int required = id == CARD_ID_SOSUKES_SUMMONS
                                        ? GT_CIP_SOSUKES_SUMMONS
                                        : (id == CARD_ID_REACH_OF_BRANCHES ? GT_CIP_REACH_OF_BRANCHES : GT_CIP_VEILBORN_GHOUL);
            if (triggers & required)
            {
              int automatic = duh_mode(player);
              if (!automatic)
              {
                const char *prompt =
                    id == CARD_ID_SOSUKES_SUMMONS
                        ? " Return Sosuke\'s Summons to hand\n Do not return"
                        : (id == CARD_ID_REACH_OF_BRANCHES
                               ? " Return Reach of Branches to hand\n Do not return"
                               : " Return Veilborn Ghoul to hand\n Do not return");
                automatic = do_dialog(player, player, card, -1, -1, prompt, 0) == 0;
              }
              if (automatic)
              {
                add_card_to_hand(player, graveyard[index]);
                remove_card_from_grave(player, index);
              }
            }
            break;
          }
          case CARD_ID_BLADEWINGS_THRALL:
          case CARD_ID_OVALCHASE_DAREDEVIL:
          {
            unsigned int required = id == CARD_ID_BLADEWINGS_THRALL ? GT_CIP_BLADEWINGS_THRALL : GT_CIP_OVALCHASE_DAREDEVIL;
            if (triggers & required)
            {
              int automatic = duh_mode(player);
              if (!automatic)
              {
                const char *prompt = id == CARD_ID_BLADEWINGS_THRALL
                                         ? " Return Bladewing\'s Thrall to play\n Do not return"
                                         : " Return Ovalchase Daredevil to play\n Do not return";
                automatic = do_dialog(player, player, card, -1, -1, prompt, 0) == 0;
              }
              if (automatic)
              {
                int returned = add_card_to_hand(player, graveyard[index]);
                remove_card_from_grave(player, index);
                put_into_play(player, returned);
              }
            }
            break;
          }
          case CARD_ID_BLOODGHAST:
            if ((triggers & GT_CIP_BLOODGHAST) && (duh_mode(player) ||
                                   do_dialog(player, player, card, -1, -1,
                                             " Return Bloodghast to play\n Do not return", 0) == 0))
              reanimate_permanent(player, card, player, index, REANIMATE_DEFAULT);
            break;
          case CARD_ID_PRIZED_AMALGAM:
            if (triggers & GT_CIP_PRIZED_AMALGAM)
              create_legacy_effect(player, card, prized_amalgam_legacy);
            break;
          case CARD_ID_AKOUM_FIREBIRD:
            if ((triggers & GT_CIP_AKOUM_FIREBIRD) &&
                do_dialog(player, player, card, -1, -1, " Return Akoum Firebird to bf\n Decline",
                          0) == 0 &&
                charge_mana_multi_while_resolving_csvid(0x3b9b, EVENT_RESOLVE_TRIGGER, player, 4, 0,
                                                        0, 0, 2, 0))
              reanimate_permanent(player, card, player, index, REANIMATE_DEFAULT);
            break;
          case CARD_ID_GATE_COLOSSUS:
            if ((triggers & GT_CIP_GATE_COLOSSUS) &&
                (duh_mode(player) ||
                 do_dialog(player, player, card, -1, -1,
                           " Put Gate Colossus on top of your library\n Leave in grave", 0) == 0))
              from_graveyard_to_deck(player, index, 1);
            break;
          }
          // The native code re-reads this slot after the preceding action.
          if (cards_data[graveyard[index]].cc[2] == 0x12 && (triggers & GT_CIP_DRAGON_ENCHANTMENTS))
          {
            char buffer[100];
            scnprintf(buffer, 100, " Return %s to bf\n Decline",
                      cards_ptr[cards_data[graveyard[index]].id]->name);
            if (duh_mode(player) || do_dialog(player, player, card, -1, -1, buffer, 0) == 0)
            {
              int returned = add_card_to_hand(player, graveyard[index]);
              remove_card_from_grave(player, index);
              put_into_play_aura_attached_to_target(player, returned, trigger_cause_controller,
                                                    trigger_cause);
            }
          }
        }
      }
    }
  }

  if (trigger_condition == TRIGGER_SPELL_CAST && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    unsigned int graveyard_mask = get_graveyard_counts(player);
    int result = is_what(trigger_cause_controller, trigger_cause, 1);
    if (!result)
    {
      unsigned int triggers = 0;
      unsigned int bolas = graveyard_mask & 0x4000000;
      if (trigger_cause_controller == player && is_what(player, trigger_cause, 2))
      {
        triggers = graveyard_mask & 2;
        if (triggers != 0)
        {
          if (rules_engine_card[player] == -1)
          {
            triggers = 0;
          }
          else
          {
            result = count_counters(player, rules_engine_card[player], COUNTER_BLOOD);
            triggers = (unsigned int)(result == 1);
          }
        }
        if (((graveyard_mask & 0x400) != 0) && (result = has_mana(player, 3, 2), result != 0))
        {
          triggers = triggers | GT_SC_MASKED_ADMIRERS;
        }
        if (((graveyard_mask & 0x1000000) != 0) &&
            (result = check_special_flags(trigger_cause_controller, trigger_cause, 0x10000000),
             result != 0))
        {
          triggers = triggers | GT_SC_PRIZED_AMALGAM;
        }

        if ((graveyard_mask & 0x800000) &&
            has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_ELDRAZI) &&
            get_cmc(trigger_cause_controller, trigger_cause) >= 7)
          triggers |= GT_SC_KOZILEKS_RETURN;
      }
      if (bolas && has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_BOLAS) &&
          is_what(trigger_cause_controller, trigger_cause, 0x100))
        triggers |= GT_SC_DARK_INTIMATIONS;
      if (triggers)
      {
        if (event == EVENT_TRIGGER)
          event_result |= 2;
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          int *graveyard;
          int grave_index, selected, own_value, opponent_value, choice;
          uint16_t id;
          graveyard = graveyard_ptr_mutable[player];
          grave_index = count_graveyard(player);
          grave_index = grave_index + -1;
          if (-1 < grave_index)
          {
            do
            {
              selected = graveyard[grave_index];
              id = cards_data[selected].id;
              if (id == CARD_ID_KOZILEKS_RETURN)
              {
                if ((triggers & GT_SC_KOZILEKS_RETURN) != 0)
                {
                  selected = add_card_to_hand(player, selected);
                  add_state(player, selected, 0x20);
                  test_definition_t this_test;
                  default_test_definition(&this_test, TYPE_CREATURE);
                  this_test.toughness = 6;
                  this_test.toughness_flag = F5_TOUGHNESS_LESSER_THAN_VALUE;
                  own_value =
                      check_battlefield_for_special_card(player, card, player, 4, &this_test);
                  opponent_value =
                      check_battlefield_for_special_card(player, card, 1 - player, 4, &this_test);
                  choice =
                      do_dialog(player, player, card, -1, -1, " Exile Kozilek\'s Return\n Decline",
                                (unsigned int)(opponent_value < own_value));
                  if (choice == 0)
                  {
                    remove_card_from_grave(player, grave_index);
                    new_damage_all(player, selected, 2, 5, 1, NULL);
                    rfg_card_in_hand(player, selected);
                  }
                  else
                  {
                    obliterate_card(player, selected);
                    hand_count[player] = hand_count[player] + -1;
                  }
                }
              }
              else if ((unsigned short)id < 0x3d0d)
              {
                if (id == CARD_ID_MASKED_ADMIRERS)
                {
                  if (((((triggers & GT_SC_MASKED_ADMIRERS) != 0) &&
                        (selected = has_mana(player, 3, 2), selected != 0)) &&
                       ((selected = duh_mode(player),
                         selected != 0 ||
                             (selected =
                                  do_dialog(player, player, card, -1, -1,
                                            " Return Masked Admirers to hand\n Do not return", 0),
                              selected == 0)))) &&
                      (charge_mana(player, COLOR_GREEN, 2), spell_fizzled != 1))
                  {
                    add_card_to_hand(player, graveyard[grave_index]);
                    remove_card_from_grave(player, grave_index);
                  }
                }
                else
                {
                  if (((id == CARD_ID_VENGEVINE) && ((triggers & GT_SC_VENGEVINE) != 0)) &&
                      ((selected = duh_mode(player),
                        selected != 0 ||
                            (selected = do_dialog(player, player, card, -1, -1,
                                                  " Return Vengevine to play\n Do not return", 0),
                             selected == 0))))
                  {
                    reanimate_permanent(player, card, player, grave_index, REANIMATE_DEFAULT);
                  }
                }
              }
              else if (id == CARD_ID_PRIZED_AMALGAM)
              {
                if ((triggers & GT_SC_PRIZED_AMALGAM) != 0)
                {
                  create_legacy_effect(player, card, prized_amalgam_legacy);
                }
              }
              else if ((id == CARD_ID_DARK_INTIMATIONS) && ((triggers & GT_SC_DARK_INTIMATIONS) != 0))
              {
                add_counters(player, card, trigger_cause_controller, trigger_cause, COUNTER_LOYALTY,
                             1);
                rfg_card_from_grave(player, grave_index);
              }
              grave_index = grave_index + -1;
            } while (grave_index != -1);
          }
        }
      }
    }
  }

  if (event == EVENT_ABILITIES && affected_card_controller == player)
  {
    card_instance_t *instance;
    unsigned int graveyard_mask;
    int result;
    instance = in_play(player, affected_card);
    if ((instance != NULL) &&
        (result = is_what(affected_card_controller, affected_card, 2), result != 0))
    {
      graveyard_mask = get_graveyard_counts(player);
      if (((graveyard_mask & 4) != 0) &&
          (result = check_battlefield_for_subtype(player, 1, 0x5002), result != 0))
      {
        event_result = event_result | 0x20;
      }
      if (((graveyard_mask & 0x2000) != 0) &&
          (result = check_battlefield_for_subtype(player, 1, 0x5001), result != 0))
      {
        event_result = event_result | 0x80;
      }
      if (((graveyard_mask & 0x4000) != 0) &&
          (result = check_battlefield_for_subtype(player, 1, 0x5009), result != 0))
      {
        event_result = event_result | 1;
      }
      if (((graveyard_mask & 0x8000) != 0) &&
          (result = check_battlefield_for_subtype(player, 1, 0x5007), result != 0))
      {
        event_result = event_result | 0x100;
      }
      if (((graveyard_mask & 0x100000) != 0) &&
          (result = check_battlefield_for_subtype(player, 1, 0x5006), result != 0))
      {
        haste(affected_card_controller, affected_card, EVENT_ABILITIES);
      }
    }
  }

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && card == affected_card && player == affected_card_controller &&
      reason_for_trigger_controller == player)
  {
    unsigned int graveyard_mask = get_graveyard_counts(player);
    unsigned int triggers = 0;
    card_instance_t *instance;
    if (rules_engine_card[player] >= 0)
    {
      instance = get_card_instance(player, rules_engine_card[player]);
      triggers = ((uint32_t)instance->targets[16].card >> 11) & 1;
      if (triggers)
        triggers = (graveyard_mask >> 5) & 1;
      if (rules_engine_card[player] >= 0)
      {
        instance = get_card_instance(player, rules_engine_card[player]);
        if ((instance->targets[16].card & 0x200) ||
            (rules_engine_card[player] >= 0 &&
             (get_card_instance(player, rules_engine_card[player])->targets[16].card & 0x400)))
          if (graveyard_mask & 0x800)
            triggers |= GT_DT_AUNTIES_SNITCH;
      }
    }
    int opponent = 1 - player;
    int opponent_rules = rules_engine_card[opponent];
    if (opponent_rules >= 0 &&
        (get_card_instance(opponent, opponent_rules)->targets[16].card & 0x2000))
    {
      int can_pay = has_mana(player, 0, 3);
      if (can_pay && (graveyard_mask & 0x80000))
        triggers |= GT_DT_PYREWILD_SHAMAN;
    }
    if (triggers)
    {
      if (event == EVENT_TRIGGER)
        event_result |= 2;
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        int *graveyard = graveyard_ptr_mutable[player];
        int count = count_graveyard(player);
        for (int index = count - 1; index >= 0; --index)
        {
          int iid = graveyard[index];
          uint16_t id = cards_data[iid].id;
          if (id == CARD_ID_CHANDRAS_PHOENIX)
          {
            if (triggers & GT_DT_CHANDRAS_PHOENIX)
            {
              add_card_to_hand(player, iid);
              remove_card_from_grave(player, index);
            }
          }
          else if (id == CARD_ID_PYREWILD_SHAMAN)
          {
            if ((triggers & GT_DT_PYREWILD_SHAMAN) && has_mana(player, 0, 3) &&
                (duh_mode(player) || do_dialog(player, player, card, -1, -1,
                                               " Return Pyrewild Shaman\n Pass", 0) == 0) &&
                charge_mana_while_resolving_csvid(0x338e, EVENT_RESOLVE_TRIGGER, player,
                                                  COLOR_COLORLESS, 3))
            {
              add_card_to_hand(player, graveyard[index]);
              remove_card_from_grave(player, index);
            }
          }
          else if (id == CARD_ID_AUNTIES_SNITCH && (triggers & GT_DT_AUNTIES_SNITCH) &&
                   (duh_mode(player) || do_dialog(player, player, card, -1, -1,
                                                  " Return Auntie\'s Snitch\n Pass", 0) == 0))
          {
            add_card_to_hand(player, graveyard[index]);
            remove_card_from_grave(player, index);
          }
        }
        increase_trap_condition_impl(player, 0x603, 0, 0);
      }
    }
  }

  {
    card_instance_t *instance;
    unsigned int graveyard_mask;
    int result;
    if ((((event == EVENT_GRAVEYARD_FROM_PLAY) &&
          (result = check_for_death_trigger_removal(affected_card_controller, affected_card),
           result == 0)) &&
         (instance = get_card_instance(affected_card_controller, affected_card),
          (unsigned char)(instance->kill_code - 1) < 3)) &&
        (result = check_special_flags2(affected_card_controller, affected_card, 0x10000000),
         result == 0))
    {
      graveyard_mask = get_graveyard_counts(player);
      if ((graveyard_mask & 0x40200) != 0 && affected_card_controller == player)
      {
        instance = in_play(player, affected_card);
        if ((instance != NULL) &&
            (result = is_what(affected_card_controller, affected_card, 2), result != 0))
        {
          instance = get_card_instance(player, card);
          if (instance->targets[1].player < 0)
          {
            instance = get_card_instance(player, card);
            instance->targets[1].player = 0;
          }
          instance = get_card_instance(player, card);
          instance->targets[1].player = instance->targets[1].player | GTA_CREATURE_YOU_CONTROL_DIED;
        }
      }
      if ((((graveyard_mask & 0x8000000) != 0) && (affected_card_controller == player)) &&
          ((instance = in_play(player, affected_card),
            instance != NULL &&
                ((result = is_what(affected_card_controller, affected_card, 2),
                  result != 0 && (result = has_subtype(affected_card_controller, affected_card,
                                                       SUBTYPE_DRAGON),
                                  result != 0))))))
      {
        instance = get_card_instance(player, card);
        if (instance->targets[1].player < 0)
        {
          instance = get_card_instance(player, card);
          instance->targets[1].player = 0;
        }
        instance = get_card_instance(player, card);
        instance->targets[1].player = instance->targets[1].player | GTA_CREATURE_YOU_CONTROL_DIED;
      }
    }
  }
  card_instance_t *engine = get_card_instance(player, card);
  if (engine->targets[1].player > 0 && trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && card == affected_card &&
      player == affected_card_controller && reason_for_trigger_controller == player)
  {
    engine = get_card_instance(player, card);
    if (event == EVENT_TRIGGER)
      event_result |= 2;
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      unsigned int recover_trigger = engine->targets[1].player;
      int *graveyard = graveyard_ptr_mutable[player];
      int count = count_graveyard(player);
      recover_trigger &= GTA_CREATURE_YOU_CONTROL_DIED;
      for (int index = count - 1; index >= 0; --index)
      {
        uint16_t id = cards_data[graveyard[index]].id;
        const char *prompt = NULL;
        int cost[6] = {0};
        switch (id)
        {
        case CARD_ID_KROVIKAN_ROT:
          cost[0] = 1;
          cost[1] = 2;
          prompt = " Return Krovikan Rot to your hand\n Do not return";
          break;
        case CARD_ID_GRIM_HARVEST:
          cost[0] = 2;
          cost[1] = 1;
          prompt = " Return Grim Harvest to your hand\n Do not return";
          break;
        case CARD_ID_CONTROVERT:
          cost[0] = 2;
          cost[2] = 2;
          prompt = " Return Controvert to your hand\n Do not return";
          break;
        case CARD_ID_ICEFALL:
          cost[4] = 2;
          prompt = " Return Icefall to your hand\n Do not return";
          break;
        case CARD_ID_RESIZE:
          cost[0] = 1;
          cost[3] = 1;
          prompt = " Return Resize to your hand\n Do not return";
          break;
        case CARD_ID_SUNS_BOUNTY:
          cost[0] = 1;
          cost[5] = 1;
          prompt = " Return Sun\'s Bounty to your hand\n Do not return";
          break;
        }
        if (prompt)
        {
          // Affordability is queried even when this pending-trigger bit is clear.
          int can_pay =
              has_mana_multi(player, cost[0], cost[1], cost[2], cost[3], cost[4], cost[5]);
          if (can_pay && recover_trigger)
          {
            bool returned = false;
            if (do_dialog(player, player, card, -1, -1, prompt, 0) == 0)
            {
              charge_mana_multi(player, cost[0], cost[1], cost[2], cost[3], cost[4], cost[5]);
              if (spell_fizzled != 1)
              {
                add_card_to_hand(player, graveyard[index]);
                remove_card_from_grave(player, index);
                returned = true;
              }
            }
            if (!returned)
              rfg_card_from_grave(player, index);
          }
        }
        else if (id == CARD_ID_GARZAS_ASSASSIN)
        {
          if (can_pay_life(player, (life[player] + 1) / 2) && recover_trigger)
          {
            if (do_dialog(player, player, card, -1, -1,
                          " Return Garza\'s Assassin to your hand\n Do not return", 0) == 0)
            {
              lose_life(player, (life[player] + 1) / 2);
              add_card_to_hand(player, graveyard[index]);
              remove_card_from_grave(player, index);
            }
            else
              rfg_card_from_grave(player, index);
          }
        }
        else if (id == CARD_ID_NETHER_TRAITOR)
        {
          if (recover_trigger && has_mana(player, 1, 1) &&
              do_dialog(player, player, card, -1, -1,
                        " Return Nether Traitor to play\n Do not return", 0) == 0)
          {
            charge_mana(player, COLOR_BLACK, 1);
            if (spell_fizzled != 1)
              reanimate_permanent(player, -1, player, index, REANIMATE_DEFAULT);
          }
        }
        else if (id == CARD_ID_BONEYARD_SCOURGE && recover_trigger &&
                 has_mana_multi(player, 1, 1, 0, 0, 0, 0) &&
                 do_dialog(player, player, card, -1, -1,
                           " Return Boneyard Scourge to play\n Do not return", 0) == 0)
        {
          charge_mana_multi(player, 1, 1, 0, 0, 0, 0);
          if (spell_fizzled != 1)
            reanimate_permanent(player, -1, player, index, REANIMATE_DEFAULT);
        }
      }
    }
    if (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER || event == EVENT_END_TRIGGER)
      get_card_instance(player, card)->targets[1].player = 0;
  }

  trigger_t saved_trigger = trigger_condition;
  enable_xtrigger_flags_t enabled = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
  xtrigger_t condition;
  bool milled = saved_trigger == 0xe5 ? xtrigger_impl_value_dont_use_directly == XTRIGGER_MILLED
                                      : saved_trigger == XTRIGGER_MILLED;
  if (milled)
  {
    if (card == affected_card && player == affected_card_controller &&
        reason_for_trigger_controller == player)
    {
      enable_xtrigger_flags = enabled;
      unsigned int graveyard_mask = get_graveyard_counts(player);
      if (graveyard_mask & 0x400000)
      {
        if ((uint32_t)(event - EVENT_TRIGGER) > 1)
          return;
        if (num_cards_milled > 0)
        {
          unsigned int lands = 0;
          for (int index = 0; index < num_cards_milled; ++index)
            if (is_what(-1, cards_milled[index].internal_card_id, 1))
            {
              if (event == EVENT_TRIGGER)
              {
                event_result |= 2;
                return;
              }
              ++lands;
            }
          enabled = enable_xtrigger_flags;
          saved_trigger = trigger_condition;
          if (lands)
          {
            parse_grave_for_centaur_vinecrasher(player, card);
            enabled = enable_xtrigger_flags;
            saved_trigger = trigger_condition;
          }
        }
      }
    }
    enable_xtrigger_flags = enabled | ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
    condition = xtrigger_impl_value_dont_use_directly;
    if (saved_trigger != 0xe5)
      condition = (xtrigger_t)saved_trigger;
  }
  else
  {
    enable_xtrigger_flags = enabled | ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
    condition = saved_trigger == 0xe5 ? xtrigger_impl_value_dont_use_directly
                                      : (xtrigger_t)trigger_condition;
  }
  if (condition == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY &&
      (get_graveyard_counts(player) & 0x400000))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_LAND);
    if (when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(
            player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test))
      parse_grave_for_centaur_vinecrasher(player, card);
  }
  if (beginning_of_combat(player, card, event, player) && rules_engine_card[player] != -1 &&
      count_counters(player, rules_engine_card[player], COUNTER_CHARGE) > 2 &&
      (get_graveyard_counts(player) & 0x20000000))
  {
    int *graveyard = graveyard_ptr_mutable[player];
    int count = count_graveyard(player);
    for (int index = count - 1; index >= 0; --index)
      if (cards_data[graveyard[index]].id == CARD_ID_ARCLIGHT_PHOENIX)
        reanimate_permanent(player, card, player, index, REANIMATE_DEFAULT);
  }
  if (trigger_condition != TRIGGER_GAIN_LIFE || card != affected_card || player != affected_card_controller ||
      reason_for_trigger_controller != player)
    return;
  unsigned int graveyard_mask = get_graveyard_counts(player);
  bool return_grove = false;
  bool return_punishing_fire = false;
  bool check_punishing_fire = true;
  if ((graveyard_mask & 0x40000000) && trigger_cause_controller == player)
  {
    if (rules_engine_card[player] < 0)
    {
      return_grove = true;
      check_punishing_fire = false;
    }
    else
    {
      engine = get_card_instance(player, rules_engine_card[player]);
      if (!((uint32_t)engine->targets[5].player & 0xffff0000U))
      {
        return_grove = true;
        return_punishing_fire =
            (int)graveyard_mask < 0 && trigger_cause_controller != player && has_mana(player, 4, 1);
        check_punishing_fire = false;
      }
    }
  }
  if (check_punishing_fire)
  {
    if ((int)graveyard_mask >= 0 || trigger_cause_controller == player || !has_mana(player, 4, 1))
      return;
    return_punishing_fire = true;
  }
  if (event == EVENT_TRIGGER)
    event_result |= 2;
  else if (event == EVENT_RESOLVE_TRIGGER)
  {
    int *graveyard = graveyard_ptr_mutable[player];
    int count = count_graveyard(player);
    for (int index = count - 1; index >= 0; --index)
    {
      uint16_t id = cards_data[graveyard[index]].id;
      if (id == CARD_ID_PUNISHING_FIRE)
      {
        if (return_punishing_fire && has_mana(player, 4, 1))
        {
          int temporary = add_card_to_hand(player, graveyard[index]);
          add_state(player, temporary, 0x20);
          --hand_count[player];
          int choice =
              do_dialog(player, player, temporary, -1, -1, " Get back Punishing Fire\n Pass", 0);
          if (!choice)
          {
            charge_mana(player, COLOR_RED, 1);
            if (spell_fizzled != 1)
              from_grave_to_hand(player, index, -1);
          }
          obliterate_card(player, temporary);
        }
      }
      else if (id == CARD_ID_DEATHLESS_KNIGHT && return_grove)
        from_grave_to_hand(player, index, -1);
    }
  }
}

static void keep_turn_count(int player, int card, event_t event)
{
  if (event == EVENT_BEGIN_TURN && current_turn == player)
	increase_trap_condition(player, TRAP_TURN_COUNT, 1);
}

static int initial_upkeep_triggers(int player, int card, event_t event)
{

  if (player == HUMAN && !get_trap_condition(HUMAN, TRAP_FIRST_TURN_UPKEEP) &&
      upkeep_trigger(player, card, event))
  {
    int i;

    // deal with serum powder
    if (player == HUMAN)
    {

      int p = HUMAN;
      int choice = 1;
      int serum_found = 1;
      if (!is_unlocked(player, card, event, 7))
      {
        serum_found = 0;
      }
      while (serum_found == 1)
      {
        serum_found = 0;
        for (i = 0; i < active_cards_count[p]; i++)
        {
          if (in_hand(p, i) && get_id(p, i) == CARD_ID_SERUM_POWDER)
          {
            choice = do_dialog(p, p, i, -1, -1, " Take Serum Powder Mulligan\n Keep Hand", 0);
            serum_found = 1;
            break;
          }
        }
        if (serum_found == 0 || choice == 1)
        {
          break;
        }

        int hand_size = 0;
        for (i = 0; i < active_cards_count[player]; i++)
        {
          if (in_hand(player, i))
          {
            hand_size++;
            kill_card(player, i, KILL_EXILE);
          }
        }

        // proceed with normal mulliganning
        int cards = hand_size;
        while (cards > 0)
        {
          // draw the appropriate number of cards
          shuffle_vanguard(player, 1);
          draw_cards(player, cards);

          // allow the choice to mulligan
          choice = do_dialog(player, player, card, -1, -1, " Keep\n Mulligan", 0);
          if (choice == 1)
          {
            cards--;
          }
          else
          {
            break;
          }

          // put your hand back into the deck
          for (i = 0; i < active_cards_count[p]; ++i)
          {
            if (in_hand(player, i))
            {
              put_on_top_of_deck(player, i);
            }
          }
        }
      }
    }
    for (i = 0; i < 2; i++)
    {
      // leylines and chancellors
      int count = active_cards_count[i] - 1;
      while (count > -1)
      {
        if (in_hand(i, count))
        {
          int id = get_id(i, count);
          if (id == CARD_ID_LEYLINE_OF_THE_VOID || id == CARD_ID_LEYLINE_OF_SANCTITY ||
              id == CARD_ID_LEYLINE_OF_PUNISHMENT || id == CARD_ID_LEYLINE_OF_VITALITY ||
              id == CARD_ID_LEYLINE_OF_SINGULARITY || id == CARD_ID_LEYLINE_OF_LIGHTNING ||
              id == CARD_ID_LEYLINE_OF_THE_MEEK || id == CARD_ID_LEYLINE_OF_LIFEFORCE ||
              id == CARD_ID_LEYLINE_OF_ANTICIPATION || id == CARD_ID_LEYLINE_OF_ABUNDANCE ||
              id == CARD_ID_LEYLINE_OF_COMBUSTION)
          {
            card_ptr_t *c_me = cards_ptr[id];
            char buffer[100];
            snprintf(buffer, 100, " Put %s into play\n Leave it in hand", c_me->name);
            int choice2 = do_dialog(i, i, count, -1, -1, buffer, 0);
            if (choice2 == 0)
            {
              put_into_play(i, count);
            }
          }
          if (id == CARD_ID_CHANCELLOR_OF_THE_ANNEX)
          {
            int choice2 =
                do_dialog(i, i, count, -1, -1, " Reveal Chancellor of the Annex\n Pass", 0);
            if (choice2 == 0)
            {
              int leg = create_legacy_effect(i, count, card_coa_special_effect);
              set_special_infos(i, leg, 66);
            }
          }
          if (id == CARD_ID_CHANCELLOR_OF_THE_DROSS)
          {
            int choice2 =
                do_dialog(i, i, count, -1, -1, " Reveal Chancellor of the Dross\n Pass", 0);
            if (choice2 == 0)
            {
              lose_life(1 - i, 3);
              gain_life(i, 3);
            }
          }
          if (id == CARD_ID_CHANCELLOR_OF_THE_FORGE)
          {
            int choice2 =
                do_dialog(i, i, count, -1, -1, " Reveal Chancellor of the Forge\n Pass", 0);
            if (choice2 == 0)
            {
              token_generation_t token;
              default_token_definition(player, card, CARD_ID_GOBLIN, &token);
              token.t_player = i;
              token.s_key_plus = SP_KEYWORD_HASTE;
              generate_token(&token);
            }
          }
          if (id == CARD_ID_CHANCELLOR_OF_THE_SPIRES)
          {
            int choice2 =
                do_dialog(i, i, count, -1, -1, " Reveal Chancellor of the Spires\n Pass", 0);
            if (choice2 == 0)
            {
              mill(1 - i, 7);
            }
          }
          if (id == CARD_ID_CHANCELLOR_OF_THE_TANGLE)
          {
            int choice2 =
                do_dialog(i, i, count, -1, -1, " Reveal Chancellor of the Tangle\n Pass", 0);
            if (choice2 == 0)
            {
              increase_trap_condition(i, TRAP_COT_FLAG, 1);
              /*
              int legacy_card = create_legacy_effect(i, count, &effect_mana_drain );
              card_instance_t *legacy = get_card_instance(i, legacy_card);
              legacy->targets[0].player = PHASE_MAIN1;
              legacy->targets[1].card = CARD_ID_CHANCELLOR_OF_THE_TANGLE;
              */
            }
          }
          if (id == CARD_ID_GEMSTONE_CAVERNS && hand_count[player] > 1)
          {
            int choice2 = do_dialog(i, i, count, -1, -1, " Reveal Gemstone Caverns\n Pass", 0);
            if (choice2 == 0)
            {
              target_definition_t td;
              default_target_definition(i, count, &td, TYPE_ANY);
              td.zone = TARGET_ZONE_HAND;
              td.allowed_controller = i;
              td.preferred_controller = i;
              td.who_chooses = i;
              td.illegal_abilities = 0;

              card_instance_t *instance2 = get_card_instance(i, count);

              state_untargettable(i, count, 1);

              if (pick_target(&td, "TARGET_CARD"))
              {
                instance2->number_of_targets = 1;
                rfg_card_in_hand(instance2->targets[0].player, instance2->targets[0].card);
                put_into_play(i, count);
                add_counters(i, count, i, count, COUNTER_LUCK, 1);
              }

              state_untargettable(i, count, 0);
            }
          }
          if (id == CARD_ID_PROVIDENCE)
          {
            int choice2 = do_dialog(i, i, count, -1, -1, " Reveal Providence\n Pass", 0);
            if (choice2 == 0)
            {
              set_life_total(i, 26);
            }
          }
          if (id == CARD_ID_SPHINX_OF_FORESIGHT)
          {
            int choice2 = do_dialog(i, i, count, -1, -1, " Reveal Sphinx of Foresight\n Pass", 0);
            if (choice2 == 0)
              scry(HUMAN, 3);
          }
        }
        count--;
      }
    }
    increase_trap_condition(HUMAN, TRAP_FIRST_TURN_UPKEEP, 1);
  }

  // deal with challenges
  if (get_setting(SETTING_CHALLENGE_MODE))
  {
    if ((get_challenge3() == 5 || get_challenge4() == 0 || get_challenge4() == 1 ||
         (gauntlet_round >= 29 && gauntlet_round < 40) || (gauntlet_round >= 59)) &&
        current_turn == HUMAN)
    {
      if (player == HUMAN && upkeep_trigger(player, card, event))
      {
        if (get_challenge4() == 0 || get_challenge4() == 1 ||
            (gauntlet_round >= 29 && gauntlet_round < 40) ||
            (gauntlet_round >= 59 && gauntlet_round < 70) || get_challenge3() == 5)
        {
          apply_challenge_onslaught();
        }
        else if (gauntlet_round >= 70)
        {
          apply_challenge_puzzle_upkeep();
        }
      }
    }
  }

  if (get_setting(SETTING_GAMESTART_SETTINGS_APPLIED) == 0 &&
      (get_setting(SETTING_AUTO_MILL) || get_setting(SETTING_CRIPPLE_AI) ||
       get_setting(SETTING_CHECK_DECK_LEGALITY) || get_setting(SETTING_CHALLENGE_MODE)))
  {
    if (player == HUMAN && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result |= RESOLVE_TRIGGER_MANDATORY;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        if (ai_is_speculating != 1)
        {
          settings[SETTING_GAMESTART_SETTINGS_APPLIED] = 1;
          force_init_upkeep = 0;
        }
        if (get_setting(SETTING_AUTO_MILL))
        {
          if (count_deck(HUMAN) > 30)
          {
            mill(HUMAN, 30);
            mill(AI, 30);
          }
        }
        if (get_setting(SETTING_CRIPPLE_AI))
        {
          // put every card back into the deck
          int count = 0;
          while (count < active_cards_count[AI])
          {
            if (!in_play(AI, count))
            {
              put_on_top_of_deck(AI, count);
            }
            count++;
          }
        }
        if ((get_setting(SETTING_CHECK_DECK_LEGALITY) || get_setting(SETTING_CHALLENGE_MODE)) &&
            get_challenge_round() != 19)
        {
          int illegal_card = check_deck_legality();
          if (illegal_card > -1 && POISON_COUNTERS(HUMAN) == 0)
          {
            // gain_life(HUMAN, get_challenge_round());
            if (get_setting(SETTING_CHALLENGE_MODE))
            {
              lose_the_game(HUMAN);
            }
            else
            {
              do_dialog(HUMAN, HUMAN, illegal_card, -1, -1, "Your deck is not legal.", 0);
              lose_the_game(HUMAN);
            }
          }
        }
        if (get_setting(SETTING_CHALLENGE_MODE))
        {
          debug_mode = 0;
          challenge_mode_upkeep(0);
        }
      }
    }
  }

  if (event == EVENT_PHASE_CHANGED && current_turn == player && current_phase == PHASE_MAIN1 &&
      get_trap_condition(player, TRAP_COT_FLAG) != 0)
  {
    produce_mana(player, COLOR_GREEN, get_trap_condition(player, TRAP_COT_FLAG));
    set_trap_condition(player, TRAP_COT_FLAG, 0);
  }

  return 0;
}

static int count_cards_actually_in_hand(int player){
	int c, total = 0;
	for (c = 0; c < active_cards_count[player]; ++c){
		if (in_hand(player, c)
			&& !(cards_data[get_card_instance(player, c)->internal_card_id].type & TYPE_EFFECT)){	// e.g. "draw a card"
			++total;
		}
	}
	return total;
}

static void debug_settings(int player, int card, event_t event){
	// if we are in challenge mode, make sure all the debug settings are off
	if( get_setting(SETTING_CHALLENGE_MODE) ){
		if( debug_mode == 1 || get_setting(SETTING_AUTO_MILL) || get_setting(SETTING_UNLIMITED_MANA) || get_setting(SETTING_AUTO_HASTE) || get_setting(SETTING_CRIPPLE_AI) || game_number > 0 ){
			do_dialog(HUMAN, player, card, -1, -1, "Invalid Settings", 0);
			lose_the_game(HUMAN);
		}

		if( gauntlet_round < 30 && (( gauntlet_round - get_challenge_round() > 1)  || ( get_challenge_round() - gauntlet_round > 1))  ){
			char buffer[100];
			snprintf(buffer, 100, " gauntlet: %d, challenge: %d", gauntlet_round , get_challenge_round());
			do_dialog(HUMAN, player, card, -1, -1, buffer, 0);
			lose_the_game(HUMAN);
		}

		/*
		if(  count_cards_by_id(HUMAN, CARD_ID_RULES_ENGINE) == 0 && current_phase > PHASE_UPKEEP){
			do_dialog(HUMAN, player, card, -1, -1, "No rules engine", 0);
			lose_the_game(HUMAN);
		}
		*/
	}

	// give each play unlimited mana
	if( get_setting(SETTING_UNLIMITED_MANA) == 1 ){
		int p, c;
		for(p=0;p<2;p++){
			for(c=0;c<6;c++){
				if( ! has_mana(p, c, 10) ) {
					produce_mana(p, c, 10);
				}
			}
		}
	}

	// give all creatures haste
	if( get_setting(SETTING_AUTO_HASTE) ){
		haste(affected_card_controller, affected_card, event);
	}

	if( get_setting(SETTING_VERIFY_HAND_COUNT)
		&& event == EVENT_STATIC_EFFECTS
		&& current_phase != PHASE_DRAW){	// the draw-a-card legacy is sometimes considered to be in hand by the exe, sometimes not - just avoid the issue
		get_card_instance(player, card)->info_slot |= REF_LEYLINES;	// so TRAP_FIRST_TURN_UPKEEP is updated
		if (get_trap_condition(HUMAN, TRAP_FIRST_TURN_UPKEEP)){	// to avoid the false positive at start of game
			int actually_in_hand = count_cards_actually_in_hand(player);
			if (actually_in_hand != hand_count[player]){
				if (ai_is_speculating != 1){
					popup("Rules Engine", "Rules Engine detected hand count inconsistency for %s\nhand_count[%d]=%d\ncount_cards_actually_in_hand(%d)=%d",
						  player == 0 ? "human" : "ai", player, hand_count[player], player, actually_in_hand);
				}
				hand_count[player] = actually_in_hand;
			}
		}
	}
}

static int effect_challenge3(int player, int card, event_t event ){
	if( eot_trigger(player, card, event) ){
		life[HUMAN]=0;
	}
	return 0;
}

static int gauntlet_settings(int player, int card, event_t event ){
	if( ! get_setting(SETTING_CHALLENGE_MODE) ){
		return 0;
	}

	// during the pit challenge, legends have shroud
	if( get_challenge4() == 1 && event == EVENT_ABILITIES ){
		if( has_creature_type(affected_card_controller, affected_card, SUBTYPE_LEGEND ) ){
			event_result |= KEYWORD_SHROUD;
		}
	}

	// during the enchanted challenge, enchantments are indestructible
	if( get_challenge4() == 6 ){
		int c=0;
		for(c=0;c< active_cards_count[AI];c++){
			card_data_t* card_d = get_card_data(AI, c);
			if((card_d->type & TYPE_ENCHANTMENT) && in_play(AI, c)){
				indestructible(AI, c, event);
			}
		}
	}

	// if we won the enchanted challenge, your enchantments are indestructible
	if( event == EVENT_GRAVEYARD_FROM_PLAY && is_unlocked(HUMAN, card, event, 36) ){
		int c=0;
		for(c=0;c< active_cards_count[HUMAN];c++){
			card_data_t* card_d = get_card_data(HUMAN, c);
			if((card_d->type & TYPE_ENCHANTMENT) && in_play(HUMAN, c)){
				indestructible(HUMAN, c, event);
			}
		}
	}

	if( (( gauntlet_round >= 71 && gauntlet_round <= 80 ) || get_challenge2() == 1 ) && player == HUMAN){
		skip_your_draw_step(player, event);
	}

	if( get_challenge2() == 3 ){
		if(event == EVENT_MODIFY_COST_GLOBAL && player == HUMAN && affected_card_controller == player
		   && get_trap_condition(player, TRAP_CHALLENGE_EPIC) && !is_what(affected_card_controller, affected_card, TYPE_LAND)){
			infinite_casting_cost();
		}

		// keep track of spells played
		if(trigger_condition == TRIGGER_SPELL_CAST && event == EVENT_END_TRIGGER && affect_me(player, card)
		   && player == reason_for_trigger_controller && player == HUMAN && player == trigger_cause_controller
		   && !is_what(trigger_cause_controller, trigger_cause, TYPE_LAND)){
			set_trap_condition(player, TRAP_CHALLENGE_EPIC, 1);
		}
	}

	if( get_challenge3() == 8 ){
		if(trigger_condition == TRIGGER_SPELL_CAST && event == EVENT_END_TRIGGER && affect_me(player, card)
		   && player == reason_for_trigger_controller && trigger_cause_controller == HUMAN && player == HUMAN)
		{
			card_instance_t *played= get_card_instance(trigger_cause_controller, trigger_cause);
			if( ! ( cards_data[ played->internal_card_id].type & TYPE_LAND ) ){
				create_legacy_effect(player, card, &effect_challenge3 );
			}
		}
	}
	else if( get_challenge4() == 3 ){
		if(trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)
		   && player == reason_for_trigger_controller && trigger_cause_controller == AI && player == AI)
		{
			card_instance_t *played= get_card_instance(trigger_cause_controller, trigger_cause);
			if( ! ( cards_data[ played->internal_card_id].type & TYPE_LAND ) ){
				if(event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if(event == EVENT_RESOLVE_TRIGGER){
					do_cascade(trigger_cause_controller, trigger_cause, -1);
				}
			}
		}
	}

	int round = get_challenge_round() - 1;

	// end the game if the time limit for puzzle challenge is over
	if( ( gauntlet_round == 72 || gauntlet_round == 73 ) && current_turn == HUMAN && current_phase == PHASE_MAIN1 ){
		life[HUMAN]=0;
		raw_set_poison(HUMAN, 10);
	}
	else if( gauntlet_round > 70 && gauntlet_round < 81 && current_turn == HUMAN && eot_trigger(player, card, event) ){
		life[HUMAN]=0;
		raw_set_poison(HUMAN, 10);
	}

	if( round < 49 || gauntlet_round > 59 ){ return 0; }
	/*
	if(event == EVENT_MODIFY_COST_GLOBAL ){
		card_instance_t *instance = get_card_instance( affected_card_controller, affected_card);
		int type = cards_data[ instance->internal_card_id ].type;

		if( round == 49 && ( instance->card_color & COLOR_TEST_BLACK ) && ! ( type & TYPE_LAND ) && ! ( type & TYPE_ARTIFACT ) ){
			infinite_casting_cost();
		}
		else if( round == 50 && ! ( type & TYPE_LAND ) && ! ( type & TYPE_ARTIFACT )&&instance->card_color & COLOR_TEST_BLUE ){
			infinite_casting_cost();
		}
		else if( round == 51 && ! ( type & TYPE_LAND ) && ! ( type & TYPE_ARTIFACT )&&instance->card_color & COLOR_TEST_GREEN ){
			infinite_casting_cost();
		}
		else if( round == 52 && ! ( type & TYPE_LAND ) && ! ( type & TYPE_ARTIFACT )&&instance->card_color & COLOR_TEST_RED ){
			infinite_casting_cost();
		}
		else if( round == 53 && ! ( type & TYPE_LAND ) && ! ( type & TYPE_ARTIFACT )&& instance->card_color & COLOR_TEST_WHITE ){
			infinite_casting_cost();
		}
		else if( round == 54 && type & TYPE_ARTIFACT ){
			infinite_casting_cost();
		}
		else if( round == 55 && is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT | TARGET_TYPE_PLANESWALKER)){
			infinite_casting_cost();
		}
		else if( round == 56 && type & TYPE_CREATURE ){
			infinite_casting_cost();
		}
		else if( round == 57 && is_what(affected_card_controller, affected_card, TYPE_SPELL)){
			infinite_casting_cost();
		}
		else if( round == 58 && type & TYPE_LAND ){
			infinite_casting_cost();
		}
	}
	*/
	return 0;
}

static void vengeful_pharaoh_effect(int player, int card, event_t event ){

	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player && current_turn == 1-player
	  ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_state = TARGET_STATE_ATTACKING;
		td.allowed_controller = 1-player;
		td.preferred_controller = 1-player;
		td.illegal_abilities |= KEYWORD_PROT_BLACK | KEYWORD_PROT_CREATURES;
		td.allow_cancel = 0;

		if( can_target(&td) && (current_phase == PHASE_FIRST_STRIKE_DAMAGE || current_phase == PHASE_NORMAL_COMBAT_DAMAGE)
		  ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if( event == EVENT_RESOLVE_TRIGGER ){
				const int *grave = get_grave(player);
				int count = count_graveyard(player)-1;
				while( count > -1 ){
					if( cards_data[grave[count]].id == CARD_ID_VENGEFUL_PHARAOH && can_target(&td) ){
						pick_target(&td, "TARGET_CREATURE");
						card_instance_t *instance = get_card_instance(player, card);
						kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
						int card_added = add_card_to_hand(player, grave[count]);
						remove_card_from_grave(player, count);
						put_on_top_of_deck(player, card_added);
					}
					count--;
				}
			}
		}
	}

	return;
}

int check_card_for_rules_engine(int card)
{
  if (card < 0)
    return 0;
  uint16_t id = cards_data[card].id;
  unsigned int flags = id == CARD_ID_SERUM_POWDER || id == CARD_ID_CHANCELLOR_OF_THE_ANNEX ||
                       id == CARD_ID_GEMSTONE_CAVERNS || id == CARD_ID_SERRA_AVENGER ||
                       id == CARD_ID_PROVIDENCE || id == CARD_ID_SPHINX_OF_FORESIGHT ||
                       (uint16_t)(id + 0xe06fU) < 5 || (uint16_t)(id + 0xb074U) < 2 ||
                       (uint16_t)(id + 0xd47dU) < 4 || (uint16_t)(id + 0xd2c5U) < 4;
  unsigned char ability_class = cards_data[card].cc[2];
  if (ability_class == 0xd)
    flags |= 2;
  else
  {
    bool trap = false;
    if (id != CARD_ID_MINDBREAK_TRAP && id != CARD_ID_RUNEFLARE_TRAP)
    {
      trap = has_subtype_by_id(-1, id, SUBTYPE_TRAP) != 0;
      id = cards_data[card].id;
      ability_class = cards_data[card].cc[2];
    }
    if (trap || id == CARD_ID_LIGHTNING_PHOENIX || id == CARD_ID_FAIRGROUNDS_TRUMPETER ||
        id == CARD_ID_HIXUS_PRISON_WARDEN || id == CARD_ID_NOVIJEN_HEART_OF_PROGRESS ||
        id == CARD_ID_HOWL_OF_THE_HORDE || id == CARD_ID_BLOODSOAKED_CHAMPION ||
        id == CARD_ID_CHANDRAS_PHOENIX || id == CARD_ID_PYREWILD_SHAMAN ||
        id == CARD_ID_BLOODCHIEF_ASCENSION || id == CARD_ID_SEARING_BLAZE ||
        id == CARD_ID_ARCHENEMY || id == CARD_ID_ORAN_RIEF_THE_VASTWOOD ||
        id == CARD_ID_TYMNA_THE_WEAVER || id == CARD_ID_HOLLOW_ONE ||
        id == CARD_ID_DEATHLESS_KNIGHT || id == CARD_ID_ROWAN_SCHOLAR_OF_SPARKS ||
        id == CARD_ID_SWEET_GUM_RECLUSE)
      flags |= 2;
  }
  if (ability_class == 0xc || id == CARD_ID_SIFT_THROUGH_SANDS || id == CARD_ID_MINDBREAK_TRAP ||
      id == CARD_ID_MINDS_DESIRE || id == CARD_ID_MOGG_CONSCRIPTS || id == CARD_ID_LURE_OF_PREY ||
      id == CARD_ID_MAELSTROM_NEXUS || id == CARD_ID_WING_SHARDS ||
      id == CARD_ID_RASHMI_ETERNITIES_CRAFTER || id == CARD_ID_VIAL_SMASHER_THE_FIERCE ||
      id == CARD_ID_VOLCANIC_TORRENT)
    flags |= 4;
  if (ability_class == 6)
    flags |= 0x10;
  if (ability_class == 5 || id == CARD_ID_DEATH_SPARK || id == CARD_ID_ETERNAL_DRAGON ||
      id == CARD_ID_NIM_DEVOURER || id == CARD_ID_NECROSAVANT || id == CARD_ID_UNDEAD_GLADIATOR)
    flags |= 0x20;
  if ((ability_class & 0xef) == 2 || ability_class == 0x14 || id == CARD_ID_PYREWILD_SHAMAN ||
      id == CARD_ID_AUNTIES_SNITCH || id == CARD_ID_BLIGHTSTEEL_COLOSSUS ||
      id == CARD_ID_DARKSTEEL_COLOSSUS || id == CARD_ID_KOZILEK_BUTCHER_OF_TRUTH ||
      id == CARD_ID_VENGEVINE || id == CARD_ID_ULAMOG_THE_INFINITE_GYRE ||
      id == CARD_ID_EMRAKUL_THE_AEONS_TORN || id == CARD_ID_PROGENITUS ||
      id == CARD_ID_NEXUS_OF_FATE || id == CARD_ID_ARCLIGHT_PHOENIX ||
      id == CARD_ID_CURIOUS_OBSESSION)
    flags |= 0x40;
  if (id == CARD_ID_PUNISHING_FIRE || id == CARD_ID_DEATHLESS_KNIGHT)
    flags |= 0x140;
  if (id == CARD_ID_BRIDGE_FROM_BELOW)
    flags |= 0x80;
  if (id == CARD_ID_KROVIKAN_HORROR)
    flags |= 0x200;
  if (ability_class == 0xb || id == CARD_ID_OBSTINATE_BALOTH || id == CARD_ID_WILT_LEAF_LIEGE ||
      id == CARD_ID_METROGNOME || id == CARD_ID_LOXODON_SMITER || id == CARD_ID_QUAGNOTH ||
      id == CARD_ID_DODECAPOD || id == CARD_ID_ICHOR_SLICK || id == CARD_ID_NULLHIDE_FEROX)
    flags |= 0x400;
  if ((ability_class == 9 && !(cards_data[card].type & 4)) || ability_class == 4 ||
      ability_class == 10 || ability_class == 0x1b || (ability_class & 0xfb) == 0x11 ||
      (unsigned char)(ability_class - 0x17) < 3 || id == CARD_ID_VISCERA_DRAGGER ||
      id == CARD_ID_HAAKON_STROMGALD_SCOURGE || id == CARD_ID_SILENT_BLADE_ONI ||
      id == CARD_ID_MARSHALING_CRY || id == CARD_ID_UNDEAD_GLADIATOR ||
      id == CARD_ID_DRAGON_WINGS || id == CARD_ID_VINEWEFT || id == CARD_ID_GRYFFS_BOON ||
      id == CARD_ID_MISTHOLLOW_GRIFFIN || id == CARD_ID_HOGAAK_ARISEN_NECROPOLIS ||
      id == CARD_ID_ELSPETH_SUNS_NEMESIS || id == CARD_ID_DIHADAS_PLOY)
    flags |= 0x1000;
  if (ability_class == 0xe)
    flags |= 0x2000;
  if (id == CARD_ID_VENGEFUL_PHARAOH)
    flags |= 0x4000;
  switch (ability_class)
  {
  case 0xf:
    flags |= 0x8000;
    break;
  case 0x10:
    flags |= 0x11000;
    break;
  case 0x13:
    flags |= 0x80000;
    break;
  case 0x16:
    flags |= 0x400000;
    break;
  case 0x1c:
    flags |= 0x200000;
    break;
  }
  return flags;
}

static void shape_deadbox(void){
	deadbox_card[AI] = generate_reserved_token_by_id(AI, CARD_ID_DEADBOX);
	deadbox_card[HUMAN] = generate_reserved_token_by_id(HUMAN, CARD_ID_DEADBOX);
}

static void shape_rules_engine(int infos){
		int z;
		for (z = 0; z < 2; z++){
			rules_engine_card[z] = generate_reserved_token_by_id(z, CARD_ID_RULES_ENGINE); // rules engine
			card_instance_t *instance = get_card_instance(z, rules_engine_card[z]);
			instance->info_slot = infos;
			// Initialize everything from targets[5] up to end of targets array to 0
			memset(&instance->targets[5], 0, sizeof(target_t) * 14);
		}
}

static void clear_temporary_traps(card_instance_t* instance);
static void check_trap_conditions(int player, int card, event_t event);

static int check_decks_for_rules_engine(void){

	int i;
	int z;
	int result = 0;
	for(z=0; z < 2; z++){
		int *deck = deck_ptr[z];
		for(i=0; i < count_deck(z); i++ ){
			result |= check_card_for_rules_engine(deck[i]);
		}
		for (i = 0; i < active_cards_count[z]; ++i){
			result |= check_card_for_rules_engine(get_card_instance(z, i)->internal_card_id);
		}
	}
	return result;
}

void recalculate_rules_engine_and_deadbox(void)
{
  int flags = check_decks_for_rules_engine();

  if (flags & REF_DEADBOX)
	{
	  if (get_deadbox_card(HUMAN) == -1)
		shape_deadbox();
	}
  else
	{
	  int db = get_deadbox_card(HUMAN);
	  if (db != -1)
		kill_card(HUMAN, db, KILL_SACRIFICE);
	  db = get_deadbox_card(AI);
	  if (db != -1)
		kill_card(AI, db, KILL_SACRIFICE);
	  deadbox_card[HUMAN] = deadbox_card[AI] = -1;
	}

  if (flags & ~REF_DEADBOX)
	{
	  int re = get_rules_engine_card(HUMAN);
	  if (re == -1)
		shape_rules_engine(flags);
	  else
		{
		  get_card_instance(HUMAN, re)->info_slot = flags;
		  re = get_rules_engine_card(AI);
		  if (re != -1)
			get_card_instance(AI, re)->info_slot = flags;
		}
	}
  else
	{
	  int re = get_rules_engine_card(HUMAN);
	  if (re != -1)
		kill_card(HUMAN, re, KILL_SACRIFICE);
	  re = get_rules_engine_card(AI);
	  if (re != -1)
		kill_card(AI, re, KILL_SACRIFICE);
	  rules_engine_card[HUMAN] = rules_engine_card[AI] = -1;
	}
}

void update_rules_engine(int infos){
	if( infos < 1 ){
		return;
	}
	int re = get_rules_engine_card(HUMAN);
	if( re < 0 ){
		if( infos != 8192 ){
			shape_rules_engine(infos);
		}
	}
	else{
		int i;
		for(i=0; i<2; i++){
			int re_card = get_rules_engine_card(i);
			card_instance_t *instance = get_card_instance( i, re_card );
			instance->info_slot |= infos;
		}
	}
	if( infos & 8192 ){
		if( get_deadbox_card(HUMAN) == -1 ){
			shape_deadbox();
		}
	}
}

int get_rules_engine_infos(int player){
	int re = get_rules_engine_card(player);
	if( re > -1 ){
		card_instance_t *instance = get_card_instance(player,re);
		return instance->info_slot;
	}
	return 0;
}

int get_deadbox_card(int player){
	return deadbox_card[player];
}

void game_startup(void){

	memset(graveyard_source, -1, sizeof(graveyard_source));
	next_graveyard_source_id = 1;

	int re_infos = 0;

	re_infos = check_decks_for_rules_engine();

	hack_allow_sorcery_if_rules_engine_is_only_effect = 0;

	if( get_setting(SETTING_CHALLENGE_MODE) ){
		re_infos |= REF_LEYLINES; // upkeep triggers
		re_infos |= REF_GAUNTLET; // gauntlet settings
		set_challenge_round();
	}
	if( get_setting(SETTING_ARCHENEMY) ){	// Also set by the Archenemy card, if it's in a deck instead of playing via setting.
		re_infos |= REF_TRAP_CONDITIONS;
	}
	vanguard_check();

	if (get_setting(SETTING_RULES_ENGINE) == 0){
		return;
	}

	if( re_infos & REF_DEADBOX ){
		shape_deadbox();
	}

	if( re_infos > 0 && re_infos != REF_DEADBOX ){
		shape_rules_engine(re_infos);
	}

	return;
}

void set_starting_life_total(int player, int amount)
{
  life[player] = amount;
  starting_life_total[player] = amount;
}

// Clear the per-turn trap conditions out at start of turn
int get_starting_life_total(int player){
	return starting_life_total[player];
}

static void increase_trap_condition_impl(int player, int trap, int val, int add /*otherwise set*/)
{
  int rec = get_rules_engine_card(player);
  if (rec < 0)
    return;

  card_instance_t *instance = get_card_instance(player, rec);

  // Strictly speaking, this is undefined behavior, but should work on everything Manalink can.
  rules_engine_globals_t *globals = (rules_engine_globals_t *)(&instance->targets[0].player);

  int bit, idx = trap & 0xFF;

  // Uses gcc case ranges extension.  Could be done (more verbosely) with a series of if/elses, of
  // course.
  switch (trap)
  {
  case TRAP_MIN_SHORTS_PER_TURN ... TRAP_MAX_SHORTS_PER_TURN:
    if (add)
      val += globals->shorts_per_turn[idx];

    if (val > USHRT_MAX)
      val = USHRT_MAX;
    else if (val < 0)
      val = 0;

    globals->shorts_per_turn[idx] = val;
    break;

  case TRAP_MIN_SHORTS_PERSISTENT ... TRAP_MAX_SHORTS_PERSISTENT:
    if (add)
      val += globals->shorts_persistent[idx];

    if (val > USHRT_MAX)
      val = USHRT_MAX;
    else if (val < 0)
      val = 0;

    globals->shorts_persistent[idx] = val;
    break;

  case TRAP_MIN_BYTES_PER_TURN ... TRAP_MAX_BYTES_PER_TURN:
    if (add)
      val += globals->bytes_per_turn[idx];

    if (val > UCHAR_MAX)
      val = UCHAR_MAX;
    else if (val < 0)
      val = 0;

    globals->bytes_per_turn[idx] = val;
    break;

  case TRAP_MIN_BYTES_PERSISTENT ... TRAP_MAX_BYTES_PERSISTENT:
    if (add)
      val += globals->bytes_persistent[idx];

    if (val > UCHAR_MAX)
      val = UCHAR_MAX;
    else if (val < 0)
      val = 0;

    globals->bytes_persistent[idx] = val;
    break;

  case TRAP_MIN_NYBBLES_PER_TURN ... TRAP_MAX_NYBBLES_PER_TURN:
    if (idx % 2 == 0)
    {
      // low nybble
      idx /= 2;

      if (add)
        val += globals->nybbles_per_turn[idx] & 0x0F;

      if (val > 0x0F)
        val = 0x0F;
      else if (val < 0)
        val = 0;

      globals->nybbles_per_turn[idx] = (globals->nybbles_per_turn[idx] & ~0x0F) | val;
    }
    else
    {
      // high nybble
      idx /= 2;

      if (add)
        val += (globals->nybbles_per_turn[idx] & 0xF0) >> 4;

      if (val > 0x0F)
        val = 0x0F;
      else if (val < 0)
        val = 0;

      globals->nybbles_per_turn[idx] = (globals->nybbles_per_turn[idx] & ~0xF0) | (val << 4);
    }
    break;

  case TRAP_MIN_BITS_PER_TURN ... TRAP_MAX_BITS_PER_TURN:
    bit = idx % 8;
    idx /= 8;

    if (val > 0)
      globals->bits_per_turn[idx] |= 1 << bit;
    else
      globals->bits_per_turn[idx] &= ~(1 << bit);
    break;

  case TRAP_MIN_BITS_PERSISTENT ... TRAP_MAX_BITS_PERSISTENT:
    bit = idx % 8;
    idx /= 8;

    if (val > 0)
      globals->bits_persistent[idx] |= 1 << bit;
    else
      globals->bits_persistent[idx] &= ~(1 << bit);
    break;
  }
}

int get_trap_condition(int player, int trap)
{
  int rec = get_rules_engine_card(player);
  if (rec < 0)
	return 0;

  card_instance_t* instance = get_card_instance(player, rec);

  // Strictly speaking, this is undefined behavior, but should work on everything Manalink can.
  rules_engine_globals_t* globals = (rules_engine_globals_t*)(&instance->targets[0].player);

  int bit, idx = trap & 0xFF;

  // Uses gcc case ranges extension.  Could be done (more verbosely) with a series of if/elses, of course.
  switch (trap)
	{
	  case TRAP_MIN_SHORTS_PER_TURN ... TRAP_MAX_SHORTS_PER_TURN:
		return globals->shorts_per_turn[idx];

	  case TRAP_MIN_SHORTS_PERSISTENT ... TRAP_MAX_SHORTS_PERSISTENT:
		return globals->shorts_persistent[idx];

	  case TRAP_MIN_BYTES_PER_TURN ... TRAP_MAX_BYTES_PER_TURN:
		return globals->bytes_per_turn[idx];

	  case TRAP_MIN_BYTES_PERSISTENT ... TRAP_MAX_BYTES_PERSISTENT:
		return globals->bytes_persistent[idx];

	  case TRAP_MIN_NYBBLES_PER_TURN ... TRAP_MAX_NYBBLES_PER_TURN:
		if (idx % 2 == 0)
		  {
			// low nybble
			idx /= 2;

			return globals->nybbles_per_turn[idx] & 0x0F;
		  }
		else
		  {
			// high nybble
			idx /= 2;

			return (globals->nybbles_per_turn[idx] & 0xF0) >> 4;
		  }

	  case TRAP_MIN_BITS_PER_TURN ... TRAP_MAX_BITS_PER_TURN:
		bit = idx % 8;
		idx /= 8;

		return (globals->bits_per_turn[idx] & (1 << bit)) ? 1 : 0;

	  case TRAP_MIN_BITS_PERSISTENT ... TRAP_MAX_BITS_PERSISTENT:
		bit = idx % 8;
		idx /= 8;

		return (globals->bits_persistent[idx] & (1 << bit)) ? 1 : 0;
	}

  return 0;
}

static void clear_temporary_traps(card_instance_t* instance)
{
  // Strictly speaking, this is undefined behavior, but should work on everything Manalink can.
  rules_engine_globals_t* globals = (rules_engine_globals_t*)(&instance->targets[0].player);

  memset(globals->shorts_per_turn, 0, sizeof(globals->shorts_per_turn));
  memset(globals->bytes_per_turn, 0, sizeof(globals->bytes_per_turn));
  memset(globals->nybbles_per_turn, 0, sizeof(globals->nybbles_per_turn));
  memset(globals->bits_per_turn, 0, sizeof(globals->bits_per_turn));
}

static void check_trap_conditions(int player, int card, event_t event){

	if( player != HUMAN ){ return; }

	// Oran Rief the Vastwood + Novijen, Heart of Progress + Hixus, Prison Warden
	if( event == EVENT_CAST_SPELL && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
		set_special_flags(affected_card_controller, affected_card, SF_JUST_CAME_INTO_PLAY);
	}

	// how many unblocked creatures
	int i, p = current_turn;
	if( event == EVENT_DECLARE_BLOCKERS ){
		for(i=0;i<active_cards_count[p];i++){
			if( is_unblocked(p, i) ){
				increase_trap_condition(current_turn, TRAP_UNBLOCKED_CREATURES, 1);
			}
		}
	}

	// was life gained?
	// handled in "gain_life" in "functions.c"

	// how many Zubera died this turn?
	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) ){
		int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
		if( kill_code >= KILL_DESTROY && kill_code < KILL_EXILE ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_ZUBERA) ){
				increase_trap_condition(affected_card_controller, TRAP_DEAD_ZUBERA_COUNT, 1);
			}
			if( get_id(affected_card_controller, affected_card) == CARD_ID_CLUE && kill_code == KILL_SACRIFICE ){
				increase_trap_condition(affected_card_controller, TRAP_CLUES_SACRIFICED, 1);
			}
		}
	}

	if(event == EVENT_DECLARE_BLOCKERS){
		int count = 0;
		while(count < active_cards_count[current_turn]){
			if( is_what(current_turn, count, TYPE_CREATURE) && is_attacking(current_turn, count)  ){
				set_trap_condition(current_turn, TRAP_RAID, 1);
				break;
			}
			count++;
		}
	}

#if 0
	/* Nothing currently checks TRAP_NUMBER_OF_ATTACKING_CREATURES (byte), and TRAP_NEMESIS_TRAP (bit) is inaccurate - the card's mana cost decreases if a white
	 * creature is currently attacking, not if one attacked this turn; and something may well have already destroyed it. */

	// how many creatures are attacking?
	if(event == EVENT_DECLARE_BLOCKERS){
		int count = 0;
		while(count < active_cards_count[current_turn]){
			if( is_what(current_turn, count, TYPE_CREATURE) && is_attacking(current_turn, count)  ){
				increase_trap_condition(current_turn, TRAP_NUMBER_OF_ATTACKING_CREATURES, 1);
				if( get_color(current_turn, count) & COLOR_TEST_WHITE ){
					increase_trap_condition(player, TRAP_NEMESIS_TRAP, 1);
					increase_trap_condition(1-player, TRAP_NEMESIS_TRAP, 1);
				}
			}
			count++;
		}
	}
	else{
		set_trap_condition(current_turn, TRAP_NUMBER_OF_ATTACKING_CREATURES, 0);
		set_trap_condition(1-current_turn, TRAP_NUMBER_OF_ATTACKING_CREATURES, 0);
	}
#endif

	// did an artifact / creature / land come into play?
	if(event == EVENT_END_TRIGGER && trigger_condition == TRIGGER_COMES_INTO_PLAY && reason_for_trigger_controller == affected_card_controller){
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_ARTIFACT) ){
			increase_trap_condition(trigger_cause_controller, TRAP_ARTIFACTS_PLAYED, 1);
		}
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) ){
			increase_trap_condition(trigger_cause_controller, TRAP_LANDS_PLAYED, 1);
		}
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_CREATURE) ){
			increase_trap_condition(trigger_cause_controller, TRAP_CREATURES_PLAYED, 1);
			if( get_color(trigger_cause_controller, trigger_cause) & COLOR_TEST_GREEN ){
				increase_trap_condition(trigger_cause_controller, TRAP_PERMAFROST_TRAP, 1);
			}
		}
		if( is_planeswalker(trigger_cause_controller, trigger_cause) ){
			increase_trap_condition(trigger_cause_controller, TRAP_PLANESWALKERS_PLAYED, 1);
		}
	}

	if (event == EVENT_END_TRIGGER && trigger_condition == TRIGGER_DISCARD && reason_for_trigger_controller == affected_card_controller){
		increase_trap_condition(trigger_cause_controller, TRAP_DISCARDED_CARDS, 1);
	}

	if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) &&
		reason_for_trigger_controller == affected_card_controller )
	{
		if( is_what(trigger_cause_controller, trigger_cause, TYPE_PERMANENT) ){
			set_trap_condition(trigger_cause_controller, TRAP_REVOLT, 1);
		}
	}

	if( event == EVENT_CLEANUP ){
		remove_special_flags(2, -1, SF_JUST_CAME_INTO_PLAY);
	}
}

void increase_trap_condition(int player, int trap, int amount)
{
  if (amount != 0)
    increase_trap_condition_impl(player, trap, amount, 1);
}

void set_trap_condition(int player, int trap, int value)
{
  increase_trap_condition_impl(player, trap, value, 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static void beginning_of_combat_effects(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_DECLARE_ATTACKERS && instance->targets[1].player != 1 && in_play(player, card)){
		instance->targets[1].player = 1;
		if( current_turn == player && ferocious(player, card) ){
			const int *grave = get_grave(player);
			int count = count_graveyard(player)-1;
			while( count > -1 ){
					if( cards_data[grave[count]].id == CARD_ID_FLAMEWAKE_PHOENIX && has_mana(player, COLOR_RED, 1) ){
						if( do_dialog(player, player, card, -1, -1, " Return Flamewake Phoenix to play\n Pass", 0) == 0 ){
							charge_mana(player, COLOR_RED, 1);
							if( spell_fizzled != 1 ){
								reanimate_permanent(player, card, player, count, REANIMATE_DEFAULT);
							}
						}
					}
					count--;
			}
		}
	}

	if(event == EVENT_PHASE_CHANGED && current_phase != PHASE_DECLARE_ATTACKERS){
		instance->targets[1].player = 0;
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_rules_engine(int player, int card, event_t event ){
	card_instance_t *instance = get_card_instance(player,card);

	if( event == EVENT_CLEANUP ){
		int i;
		for(i=0; i<2; i++){
			remove_special_flags2(i, -1, SF2_HAS_DAMAGED_PLAYER0);
			remove_special_flags2(i, -1, SF2_HAS_DAMAGED_PLAYER1);
		}
    set_trap_condition(current_turn, TRAP_TURN_IS_EXTRA, 0);
    int extra_turn = 0;
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c))
        {
          card_instance_t* legacy = get_card_instance(p, c);
          if (legacy->internal_card_id == LEGACY_EFFECT_CUSTOM &&
              (legacy->token_status & STATUS_TIMEWALK) && legacy->targets[0].player == current_turn)
            extra_turn = 1;
        }
    if (time_walk_flag == current_turn || extra_turn)
      set_trap_condition(current_turn, TRAP_TURN_IS_EXTRA, 1);

	}

	// (I'd think this should be during EVENT_CLEANUP, but it's harmless to preserve the original behavior and delay until EVENT_CAN_SKIP_TURN of next turn)
	if (event == EVENT_CAN_SKIP_TURN){
		clear_temporary_traps(instance);
	}

	debug_settings(player, card, event);
	if ((instance->info_slot & REF_LEYLINES) || force_init_upkeep){
		initial_upkeep_triggers(player, card, event);            // 1
		keep_turn_count(player, card, event);                    // same as above
	}
	if( instance->info_slot & REF_TRAP_CONDITIONS ){
		check_trap_conditions(player, card, event);                   // 2
	}
	if( instance->info_slot & REF_STORM_COUNT ){
		keep_storm_count(player, card, event);                   // 4
	}
	if( instance->info_slot & REF_GAUNTLET ){
		gauntlet_settings(player, card, event);                  // 8
		//apply_challenge_10_to_1(player, card, event);
	}
	if( instance->info_slot & REF_DREDGE ){
		resolve_dredge_triggers(player, card, event);            // 16
	}
	if( instance->info_slot & REF_UPKEEP_ABILITY_IN_GRAVE ){
		resolve_graveyard_upkeep_triggers(player, card, event);  // 32
	}
	if( instance->info_slot & REF_CONT_AND_TRIG_ABILITY_IN_GRAVE ){
		static_graveyard_abilities(player, card, event);         // 64
	}
	if( instance->info_slot & REF_BRIDGE_FROM_BELOW ){
		bridge_from_below(player, card, event);                  // 128
	}
	if (instance->info_slot & REF_GAIN_LIFE_TRIGGER)
    event_flags |= EA_LICH;
	if( instance->info_slot & REF_KROVIKAN_HORROR ){
		krovikan_horror(player, card, event);                    // 512
	}

	#if 0
	if( instance->info_slot & REF_DISCARD_TRIGGER_MADNESS ){
		discard_triggers(player, card, event);                   // 1024
	}
	#endif

	// (1<<13) --> Deabox
	if( instance->info_slot & REF_VENGEFUL_PHARAOH ){
		vengeful_pharaoh_effect(player, card, event);                 // 16384
	}
	if( instance->info_slot & REF_MIRACLES ){
		miracle(player, card, event);
	}
	// (1<<16) --> Forecast
	// (1<<17) --> Scavenge
	// (1<<18) --> Onslaught's auras auto-returning from grave
	if( instance->info_slot & REF_CONSPIRE ){
		conspire(player, card, event);
	}
	// (1<<20) --> Recover
  if (instance->info_slot & REF_EOT_ABILITY_IN_GRAVE)
    resolve_graveyard_eot_triggers(player, card, event);
	if( instance->info_slot & REF_BEGINNING_OF_COMBAT_EFFECTS ){
		beginning_of_combat_effects(player, card, event);
	}
	if( instance->info_slot & REF_ACTIVATED_ABILITIES_HAND_GRAVE ){
		return activate_cards_in_hand(player, card, event);           // 4096
	}

	return 0;
}

/* October 2022 storm backing-state layout checks. */
#ifndef SHANDALAR
STATIC_ASSERT(sizeof(rules_engine_card) == 8, storm_native_card_slots);
STATIC_ASSERT(sizeof(rules_engine_globals_t) == 152, storm_native_globals_bytes);
STATIC_ASSERT(offsetof(rules_engine_globals_t, targets) == 0, storm_native_offset_targets);
STATIC_ASSERT(offsetof(rules_engine_globals_t, shorts_per_turn) == 40, storm_native_offset_shorts_per_turn);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned1) == 48, storm_native_offset_unassigned1);
STATIC_ASSERT(offsetof(rules_engine_globals_t, shorts_persistent) == 68, storm_native_offset_shorts_persistent);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned2) == 70, storm_native_offset_unassigned2);
STATIC_ASSERT(offsetof(rules_engine_globals_t, bytes_per_turn) == 86, storm_native_offset_bytes_per_turn);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned3) == 102, storm_native_offset_unassigned3);
STATIC_ASSERT(offsetof(rules_engine_globals_t, bytes_persistent) == 112, storm_native_offset_bytes_persistent);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned4) == 114, storm_native_offset_unassigned4);
STATIC_ASSERT(offsetof(rules_engine_globals_t, nybbles_per_turn) == 120, storm_native_offset_nybbles_per_turn);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned5) == 128, storm_native_offset_unassigned5);
STATIC_ASSERT(offsetof(rules_engine_globals_t, bits_per_turn) == 133, storm_native_offset_bits_per_turn);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned6) == 137, storm_native_offset_unassigned6);
STATIC_ASSERT(offsetof(rules_engine_globals_t, bits_persistent) == 140, storm_native_offset_bits_persistent);
STATIC_ASSERT(offsetof(rules_engine_globals_t, unassigned7) == 141, storm_native_offset_unassigned7);
STATIC_ASSERT(offsetof(rules_engine_globals_t, counters) == 144, storm_native_offset_counters);
STATIC_ASSERT(COUNTER_BLOOD == 19, storm_native_COUNTER_BLOOD);
STATIC_ASSERT(COUNTER_DEVOTION == 32, storm_native_COUNTER_DEVOTION);
STATIC_ASSERT(COUNTER_VORTEX == 38, storm_native_COUNTER_VORTEX);
STATIC_ASSERT(COUNTER_CHARGE == 123, storm_native_COUNTER_CHARGE);
STATIC_ASSERT(sizeof(storm_mode_t) == 4, storm_native_mode_bytes);
STATIC_ASSERT(STORM_CREATURE == 64, storm_canonical_creature_mode);
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c7890; original functions/rules_engine.c:84. */
int limit_allowed_in_deck(int csvid)
{
  static int unlimited_list[17] = {91,   126,   164,   188,   239,   2100,  2101, 2102, 2103,
                                         2104, 20819, 15719, 22636, 19685, 18489, 7107, 13335};
  for (unsigned int i = 0; i < 17; ++i)
    if (csvid == unlimited_list[i])
      return 5000;
  return csvid == CARD_ID_SEVEN_DWARVES ? 7 : 4;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c78f0; original functions/rules_engine.c:365. */
int random_rogue(void)
{
  extern int shandalar_select_random_rogue(int difficulty);
  read_and_adjust_settings();
  return shandalar_select_random_rogue(opponent_skill);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c7f70; original functions/rules_engine.c:1599. */
void increment_storm_count(int player, int mode)
{
  int card;

  card = rules_engine_card[player];
  if (card != -1)
  {
    hack_silent_counters = hack_silent_counters + 1;
    add_counters(player, card, player, card, COUNTER_VORTEX, 1);
    increase_trap_condition_impl(player, 0x301, 1, 1);
    if ((mode & STORM_CREATURE) != 0)
    {
      add_counters(player, card, player, card, COUNTER_BLOOD, 1);
    }
    if ((mode & STORM_ENCHANTMENT) != 0)
    {
      add_counters(player, card, player, card, COUNTER_DEVOTION, 1);
    }
    if ((mode & STORM_INSTANT_SORCERY) != 0)
    {
      add_counters(player, card, player, card, COUNTER_CHARGE, 1);
    }
    hack_silent_counters = hack_silent_counters + -1;
    if ((mode & 2U) != 0)
    {
      increase_trap_condition_impl(player, 0x403, 1, 1);
    }
    if ((mode & 4U) != 0)
    {
      increase_trap_condition_impl(player, 0x404, 1, 1);
    }
    if ((mode & 8U) != 0)
    {
      increase_trap_condition_impl(player, 0x405, 1, 1);
    }
    if ((mode & 0x10U) != 0)
    {
      increase_trap_condition_impl(player, 0x406, 1, 1);
    }
    if ((mode & 0x20U) != 0)
    {
      increase_trap_condition_impl(player, 0x407, 1, 1);
      return;
    }
  }
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c81a0; original functions/rules_engine.c:1735. */
int get_specific_storm_count(int player)
{
  int result;

  if (rules_engine_card[player] != -1)
  {
    result = count_counters(player, rules_engine_card[player], COUNTER_VORTEX);
    return result;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c81e0; original functions/rules_engine.c:1744. */
int get_storm_count(void)
{
  int total = 0;
  if (rules_engine_card[1] != -1)
    total = count_counters(1, rules_engine_card[1], COUNTER_VORTEX);
  if (rules_engine_card[0] != -1)
    total += count_counters(0, rules_engine_card[0], COUNTER_VORTEX);
  return total;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c8240; original functions/rules_engine.c:1749. */
int get_stormcreature_count(int k)
{
  int result;

  if (rules_engine_card[k] != -1)
  {
    result = count_counters(k, rules_engine_card[k], COUNTER_BLOOD);
    return result;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c8280; original functions/rules_engine.c:1757. */
int get_storminstantsorcery_count(int k)
{
  int result;

  if (rules_engine_card[k] != -1)
  {
    result = count_counters(k, rules_engine_card[k], COUNTER_CHARGE);
    return result;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022 inline reset_storm_count: owner 0x26c9140; DIE 0x68be08e. */

/* End ManalinkEh 2022 inline reset_storm_count. */
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022 inline keep_storm_count: owner 0x26c9140; DIE 0x68be050. */

/* End ManalinkEh 2022 inline keep_storm_count. */
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c82c0; original functions/rules_engine.c:1765. */
int get_stormenchantment_count(int k)
{
  int result;

  if (rules_engine_card[k] != -1)
  {
    result = count_counters(k, rules_engine_card[k], COUNTER_DEVOTION);
    return result;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26c90c0; original functions/rules_engine.c:1773. */
int get_stormcolor_count(int player, color_t clr)
{
  int result;
  if (4U < (unsigned int)(clr - COLOR_BLACK))
  {
    return 0;
  }
  result = get_trap_condition(player, clr + 0x402);
  return result;
}

#endif

#define DUEL_RULE_STATE(X) X(vanguard_avatar) X(vcount) X(companion) X(ccount) X(settings) X(actual_event) X(challenge) X(rules_engine_card) X(deadbox_card) X(starting_life_total) X(force_init_upkeep) X(hack_allow_sorcery_if_rules_engine_is_only_effect) X(other_game_mode)

/* SDL host helper retained during the 2022 migration. */
void duel_rules_snapshot_capture(void *destination) {
  unsigned char *p=destination;
#define FIELD(v) memcpy(p,&v,sizeof(v)); p+=sizeof(v);
  DUEL_RULE_STATE(FIELD)
#undef FIELD
}

/* SDL host helper retained during the 2022 migration. */
void duel_rules_initialize_headless(void) {
  memset(settings,0,sizeof(settings));
  settings[SETTING_SETTING_FILE_READ]=1;
  settings[SETTING_RULES_ENGINE]=1;
  settings[SETTING_AI_USES_RULES_ENGINE]=1;
  settings[SETTING_HAND_SIZE]=7;
  settings[SETTING_MIN_PICS_TO_RANDOMIZE]=15;
}

/* SDL host helper retained during the 2022 migration. */
void duel_rules_snapshot_restore(const void *source) {
  const unsigned char *p=source;
#define FIELD(v) memcpy(&v,p,sizeof(v)); p+=sizeof(v);
  DUEL_RULE_STATE(FIELD)
#undef FIELD
}

/* SDL host helper retained during the 2022 migration. */
size_t duel_rules_snapshot_size(void) {
  size_t n=0;
#define FIELD(v) n+=sizeof(v);
  DUEL_RULE_STATE(FIELD)
#undef FIELD
  return n;
}
