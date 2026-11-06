#include "manalink.h"
#include "magic/src/duel_host.h"

int can_pay_life_impl(int player, int amount, int flags);

/* Canonical recovered helper declarations. */
static card_instance_t *find_obliviation_legacy(int player, int card, int *out_player,
                                                int *out_card, int *out_iid, int *out_owner);
int is_what(int player, int card, int test_type);
int is_token(int player, int card);
int get_special_flags(int player, int card, int type_of_special_flag);
int check_special_flags(int player, int card, int value);
int check_special_flags2(int player, int card, int value);
int check_special_flags3(int player, int card, int value);
void set_special_flags(int player, int card, int value);
void set_special_flags2(int player, int card, int value);
void set_special_flags3(int player, int card, int value);
void remove_special_flags(int player, int card, int value);
void remove_special_flags2(int player, int card, int value);
void remove_special_flags3(int player, int card, int value);
int get_special_flags_by_instance(card_instance_t *instance);
int get_special_flags2_by_instance(card_instance_t *instance);
int get_special_flags3_by_instance(card_instance_t *instance);
int pump_ability_until_eot(int player, int card, int t_player, int t_card, int power,
                                int toughness, int ability, int sp_ability);
int resolve_gfp_ability(int player, int card, event_t event, resolve_trigger_t trig_mode);
void gain_life(int player, int amount);
int has_mana_multi_a(int player, int colorless, int black, int blue, int green, int red,
                          int white);
int has_mana_multi(int player, int colorless, int black, int blue, int green, int red,
                        int white);
int get_global_color_hack(int player);
int get_color(int player, int card);
int get_color_by_internal_id_no_hack(int internal_id);
int get_color_by_internal_id(int player, int internal_id);
int get_color_real_or_iid(int player_or_owner, int player_or_negone, int card_or_iid);
void change_color(int player, int card, int t_player, int t_card, color_test_t new_color,
                       change_color_t mode);
int get_hacked_walk(int player, int card, int orig_walk);
int keyword_to_color(int keyword);
int get_colors_to_hack_from_csvid(int csvid);
int is_colorless(int player, int card);
int colors_shared(int clr1, int clr2);
int has_my_colors(int player, int card, int t_player, int t_card);
int real_count_colors(int clr);
int count_colors(int player, int card);
int evaluate_colors(int color1, int color2);
int is_monocolor_hybrid(card_ptr_t *cp);
int get_deck_color(int player, int whose_deck);
int choose_a_color(int player, int ai_choice);
int choose_a_color_from(int player, int ai_choice, color_test_t available_colors);
int select_a_color(int player);
int is_planeswalker(int player, int card);
type_t get_type_with_iid(int player, int card, int iid);
type_t get_type(int player, int card);
int get_base_power(int player, int card);
int get_base_power_iid(int owner, int iid);
int get_base_toughness(int player, int card);
int get_base_toughness_iid(int owner, int iid);
int get_cmc_by_internal_id(int iid);
int get_cmc(int player, int card);
int get_highest_power(int player);
int get_power_sum(int player);
int count_defenders(int player);
int count_creatures_by_toughness(int player, int pow, int mode);
int count_creatures_by_power(int player, int pow, int mode);
int get_highest_cmc_nonland(int player);
int get_highest_cmc(int player, type_t type);
int get_most_common_cmc_in_hand(int player, type_t type);
int get_most_common_cmc_nonland(int player);
int count_permanents_by_type(int player, type_t type);
int count_permanents_by_color(int player, int selected_type, int selected_color);
int count_cards_by_id(int player, int id);
int check_battlefield_for_subtype(int what_player, int type, int subtype);
int count_subtype(int what_player, int type, int subtype);
int count_shrines(int player, int card);
int is_x_spell(int player, int card);
int is_hardcoded_card(int player, int card);
int is_valid_card(int id);
int is_stolen(int player, int card);
int is_target(int player, int card, int controller_of_effect);
int is_sick(int player, int card);
int is_animated_and_sick(int player, int card);
int is_attacking(int player, int card);
card_instance_t *in_play_and_attacking(int player, int card);
int check_state(int player, int card, int state);
int check_status(int player, int card, int status);
void add_state(int player, int card, int state);
void remove_state(int player, int card, int state);
void add_status(int player, int card, int status);
void remove_status(int player, int card, int status);
void state_untargettable(int player, int card, int mode);
int tap_card(int player, int card);
void untap_card(int player, int card);
void untap_card_no_event(int player, int card);
int is_creature_dead(void);
int has_dead_creature(int player);
card_instance_t *has_activation_on_stack(int player, int card);
int is_basic_land_by_id(int id);
int is_basic_land(int player, int card);
int is_mana_producer_land(int player, int card);
int no_combat_damage_this_turn(int player, int card, event_t event);
void negate_combat_damage_this_turn(int player, int card, int t_player, int t_card,
                                         int alternate_legacy_text_number);
int my_damage_cannot_be_prevented(int player, int card, event_t event);
int prevent_damage(int damage_player, int damage_card_index, int amount);
void prevent_the_next_n_damage(int player, int card, int t_player, int t_card, int amount,
                                    prevent_t flags, int redir_to_player, int redir_to_card);
int fog_special2(int player, int card, int t_player, fog_special_flags_t flags, int param);
int fog_special(int player, int card, int t_player, int flags);
int fog_effect(int player, int card);
void negate_damage_this_turn(int player, int card, int t_player, int t_card, int unused);
int prevent_all_damage_to_target(int player, int card, int t_player, int t_card, int mode);
int prevent_all_damage(int player, int card, event_t event);
void damage_target_creature_or_player(int t_player, int t_card, int amount, int source_player,
                                           int source_card);
int damage_cannot_be_prevented_until_eot(int player, int card);
void prevent_all_my_damage(int player, int card, event_t event, int flags);
int prevent_all_damage_dealt_by_target(int player, int card, int t_player, int t_card,
                                            int flags);
int prevent_damage_until_eot(int player, int card, int t_player, int t_card, int maximum);
void raw_counter_activated_ability(int tgt_player, int tgt_card);
int charge_mana_hybrid_while_resolving(int player, int card, event_t event, int amt_colored,
                                            color_t first_color, color_t second_color,
                                            int amt_colorless);
int get_hacked_color(int player, int card, int color);
int get_sleighted_color(int player, int card, int color);
int get_colors_to_sleight_from_text(int csvid);
int get_hacked_subtype(int player, int card, int orig_subtype);
int get_sleighted_protection(int player, int card, int orig_prot);
int legacy_change_color(int player, int card, event_t event);
static int get_colors_to_hack_from_type_text(int csvid);
static int get_colors_to_hack_from_rules_text(int csvid);
const char *get_card_name_by_instance(card_instance_t *instance);
const char *get_card_or_subtype_name(int subtype_or_csvid);
static int subtype_name_legacy(int player, int card, event_t event);
static int legacy_name_with_rfg_check(int player, int card, event_t event);
int get_card_name(int player, int card);
int has_same_name(int player, int card, int player2, int card2);
int count_permanents_by_name(int player, type_t type, int csvid);
void ai_modifier_for_land_untappers(int player, int card, int target_pos);
void ai_modifier_for_haste_granting_effects(int player, int card);
void ai_modifier_twiddle(int player, int card, int tgtnum);
int choosing_to_untap(int player, int card, event_t event);
void remove_from_combat(int player, int card);
static int remove_untargettable_legacy(int player, int card, event_t event);
int is_artifact_creature_by_internal_id(int iid, int unused1, int unused2, int unused3);
void land_animation(int player, int card);
void true_transform(int player, int card);
int create_legacy_name_with_rfg_check(int player, int card, int owner, int iid);
int create_subtype_name_legacy(int player, int card, int subtype);
int create_card_name_legacy(int player, int card, int id);
int create_hardcoded_subtype_name_legacy(int player, int card, int subtype);
void default_pump_ability_definition(int player, int card, pump_ability_t *pump, int pow,
                                          int tou, int key, int skey);
int pump_ability(int player, int card, int t_player, int t_card, pump_ability_t *pump);
void pump_ability_by_test(int player, int card, int t_player, pump_ability_t *pump,
                               test_definition_t *this_test);
static int pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
    int player, int card, int t_player, int t_card, pump_ability_t *pump);
int legacy_effect_pump_ability_until_eot(int player, int card, event_t event);
static void sp_ability_trigger_impl(int controller_of_trigger, event_t event, int trigger_mode,
                                         type_t type, test_definition_t *test,
                                         int triggering_player, int triggering_card,
                                         int allow_copies);
void sp_ability_trigger(int controller_of_trigger, event_t event, int trigger_mode,
                             type_t type, test_definition_t *test, int triggering_player,
                             int triggering_card);
int can_counter_triggered_ability(int player, int card, event_t event,
                                       target_definition_t *td);
int test_leaves_play(int triggering_player, int triggering_card, event_t event, int controller,
                          test_definition_t *test, int can_be_suppressed,
                          int can_be_duplicated);
int test_leaves_play_mode(int triggering_player, int triggering_card, event_t event,
                               int controller, resolve_trigger_t mode, test_definition_t *test,
                               int can_be_suppressed, int can_be_duplicated);
void etb_ability_global(int controller_of_trigger, event_t event, int trigger_mode,
                             type_t type, test_definition_t *test, int triggering_player,
                             int triggering_card);
static int new_specific_cip_impl(int controller_of_trigger, event_t event, int trigger_mode,
                                      type_t type, test_definition_t *test,
                                      int triggering_player, int triggering_card);
int new_specific_cip_global(int player, event_t event, int trigger_mode, type_t type,
                                 test_definition_t *this_test, int triggering_player,
                                 int triggering_card);
int new_specific_cip(int player, int card, event_t event, int t_player, int trigger_mode,
                          test_definition_t *this_test);
int new_card_from_list(int player, int mode, test_definition_t *test);
void haste_and_sacrifice_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                        int number);
void haste_and_remove_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                     int number);
int reveal_cards_from_your_hand(int player, int card, test_definition_t *this_test);
void mana_into_string(int cless, int black, int blue, int green, int red, int white,
                           test_definition_t *this_test);
int ai_name_a_random_csvid(int player, int card, int t_player, type_t type,
                                test_definition_t *test);
int name_a_card(int player, int ai_player, test_definition_t *test, int mode);
int select_permanent_to_disable(int player, int card, target_definition_t *td,
                                     const char *prompt, test_definition_t *test);
int counterspell_validate(int player, int card, target_definition_t *td, int target_num);
int counterspell(int player, int card, event_t event, target_definition_t *td,
                      int target_num);
void untap_lands(int player, int card, int num_lands);
int charge_mana_multi(int player, int colorless, int black, int blue, int green, int red,
                           int white);
int charge_mana_multi_a(int player, int colorless, int black, int blue, int green, int red,
                             int white, int artifact);
int has_mana_for_activated_ability(int player, int card, int cless, int black, int blue,
                                        int green, int red, int white);
int charge_mana_for_activated_ability(int player, int card, int cless, int black, int blue,
                                           int green, int red, int white);
int charge_mana_for_activated_ability_while_resolving(int player, int card, event_t event,
                                                           int colorless, int black, int blue,
                                                           int green, int red, int white);
int get_cost_mod_for_activated_abilities(int player, int card, int orig_cost, int black,
                                              int blue, int green, int red, int white);
int charge_mana_while_resolving(int player, int card, event_t event, int charged_player,
                                     color_t color, int amount);
int charge_mana_while_resolving_csvid(int csvid, event_t event, int charged_player,
                                           color_t color, int amount);
int charge_mana_multi_while_resolving(int player, int card, event_t event, int charged_player,
                                           int colorless, int black, int blue, int green, int red,
                                           int white);
int charge_mana_multi_while_resolving_csvid(int csvid, event_t event, int charged_player,
                                                 int colorless, int black, int blue, int green,
                                                 int red, int white);
int charge_mana_for_multi_x(int player, int color, int num_x);
int charge_mana_for_double_x(int player, int color);
int get_sleighted_color_test(int player, int card, int orig_color_test);
void etb_ability(int player, int card, event_t event, resolve_trigger_t trigger_mode);
void get_sleighted_hacked_suppressed_ability(int player, int card, int t_player,
                                                  int *p_ability, int *p_sp_ability);
int pump_ability_until_eot_no_repeat(int player, int card, int t_player, int t_card,
                                          int ability, int sp_ability);
int generic_activated_ability(int player, int card, event_t event, int mode, int cless,
                                   int black, int blue, int green, int red, int white,
                                   uint32_t variable_costs, target_definition_t *td,
                                   const char *prompt);
int granted_generic_activated_ability(int granting_player, int granting_card,
                                           int granted_to_player, int granted_to_card,
                                           event_t event, int mode, int cless, int black, int blue,
                                           int green, int red, int white, uint32_t variable_costs,
                                           target_definition_t *td, const char *prompt);
int attachment_granting_activated_ability(int player, int card, event_t event, int mode,
                                               int cless, int black, int blue, int green, int red,
                                               int white, uint32_t variable_costs,
                                               target_definition_t *td, const char *prompt);
int pump_until_eot(int player, int card, int t_player, int t_card, int power, int toughness);
int upkeep_trigger_mode_impl(int player, int card, event_t event,
                                  resolve_trigger_t trigger_mode, int no_check);
void count_for_gfp_ability_and_store_values(int player, int card, int event, int t_player,
                                                 int type, test_definition_t *this_test,
                                                 int mode, int storage_location);
int reveal_card_until_you_reveal_test(int player, int t_player, test_definition_t *test,
                                           int flags);
void cloning_and_verify_legend(int player, int card, int t_player, int t_card);
void ai_mod_for_untap_effects(int player, int card, int target_pos, int total_mana_spent);
int pump_until_eot_merge_previous(int player, int card, int t_player, int t_card, int power,
                                       int toughness);
int pump_subtype_until_eot(int player, int card, int t_player, int subtype, int power,
                                int toughness, int ability, int sp_ability);
void pump_creatures_until_eot(int player, int card, int t_player,
                                   int alternate_legacy_text_number, int power, int toughness,
                                   int ability, int sp_ability, test_definition_t *this_test);
void count_for_gfp_ability(int player, int card, int event, int t_player, int type,
                                test_definition_t *this_test);
int trigger_gain_life(int player, int card, event_t event, int who_gains,
                           resolve_trigger_t trigger_mode);
int lose_life(int player, int amount);
int cast_counter_activated_ability(int player, int card, int target_pos);
int validate_counter_activated_ability(int player, int card, target_definition_t *td,
                                            int target_pos);
int resolve_counter_activated_ability(int player, int card, target_definition_t *td,
                                           int target_pos);
int can_counter_activated_ability(int player, int card, event_t event,
                                       target_definition_t *td);
int can_block_additional_until_eot(int player, int card, int t_player, int t_card,
                                        int num_additional);
void pump_color_until_eot(int player, int card, int t_player, int clr, int power,
                               int toughness, int ability, int sp_ability);
void pump_creatures_until_eot_no_repeat(int player, int card, int t_player,
                                             int alternate_legacy_text_number, int ability,
                                             int sp_ability, test_definition_t *this_test);
int vanilla_pump(int player, int card, event_t event, target_definition_t *td, int power,
                      int toughness, int keyword, int sp_keyword);
int vanilla_instant_pump(int player, int card, event_t event, int allowed_controller,
                              int preferred_controller, int power, int toughness, int keyword,
                              int sp_keyword);
int cannot_attack(int player, int card, event_t event);
void pump_creatures_until_eot_merge_pt(int player, int card, int t_player, int power,
                                            int toughness, test_definition_t *this_test);
void pump_creatures_until_eot_merge_pt_alternate_legacy_text(int player, int card,
                                                                  int t_player, int power,
                                                                  int toughness,
                                                                  test_definition_t *this_test,
                                                                  int alt_text_number);
void remove_summoning_sickness(int player, int card);
void twiddle(int player, int card, int tgtnum);
void ai_mod_for_tap_effects(int player, int card, int target_pos);
void ai_modifier_for_targeted_card_drawing(int player, int card, int target_pos,
                                                int cards_to_draw);
void ai_modifier_for_act_of_treason(int player, int card);
static int effect_pump_until_eot(int player, int card, event_t event);
static int reanimate_permanent_impl(int player, int card, int target_graveyard, int selected,
                                  reanimate_mode_t action, counter_t counter_type,
                                  CardFunction effect_fn, ReanimationBeforeEtb before_etb);
int new_reanimate_all(int player, int card, int targ_player, test_definition_t *this_test,
                           reanimate_mode_t action);
int reanimate_permanent(int player, int card, int target_graveyard, int selected,
                             reanimate_mode_t action);
int reanimate_permanent_with_effect(int player, int card, int target_graveyard, int selected,
                                         reanimate_mode_t action, CardFunction effect_fn);
int reanimate_permanent_with_counter_and_effect(int player, int card, int target_graveyard,
                                                     int selected, reanimate_mode_t action,
                                                     counter_t counter_type, CardFunction effect_fn);
int reanimate_permanent_with_function(int player, int card, int target_graveyard, int selected,
                                           reanimate_mode_t action,
                                           ReanimationBeforeEtb before_etb);
void reanimate_all(int player, int card, int targ_player, int type, int flag1, int subtype,
                        int flag2, int color, int flag3, int id, int flag4, int cc, int flag5,
                        reanimate_mode_t action);
const char *get_sleighted_color_text2(int player, int card, const char *fmt,
                                           int orig_color_t_1, int orig_color_t_2);
const char *get_sleighted_color_text(int player, int card, const char *fmt, int orig_color_t);
const char *get_card_name_by_id(int csvid);
int generic_spell(int player, int card, event_t event, int flags, target_definition_t *td,
                       const char *prompt, int max_targets, test_definition_t *this_test);
int specific_cip(int player, int card, event_t event, int t_player, int trigger_mode, int type,
                      int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4,
                      int cc, int flag5);
static int haste_and_kill_at_eot(int player, int card, event_t event, int kill_mode);
int haste_and_remove_eot(int player, int card, event_t event);
int haste_and_sacrifice_eot(int player, int card, event_t event);
static int generic_haste_and_kill_at_eot_legacy(int player, int card, event_t event);
int haste_and_remove_at_beginning_of_next_end_step(int player, int card, int t_player,
                                                        int t_card, int kill_mode);
static int haste_and_exile_your_upkeep(int player, int card, event_t event);
int effect_unearth(int player, int card, event_t event);
void ereboss_titan_effect(int player);
int aot_like_spell(int player, int card, event_t event, target_definition_t *td,
                        const char *msg, int additional_flags);
void global_type_change(int player, int card, event_t event, int t_player, int added_type,
                             test_definition_t *this_test, int pow, int tou, int key,
                             int subtype, int clr);
void type_uncounterable(int player, int card, event_t event, int t_player, int type,
                             test_definition_t *this_test);
int type_change_legacy(int player, int card, event_t event);
int turn_into_artifact(int player, int card, int t_player, int t_card, int mode);
int turn_into_creature(int player, int card, int t_player, int t_card, int mode, int pow,
                            int tou);
int artifact_animation(int player, int card, int t_player, int t_card, int mode, int pow,
                            int tou, int abilities, int sp_abilities);
static int global_type_animation_legacy(int player, int card, event_t event);
int real_land_animation2(int player, int card, int t_player, int t_card, int mode, int pow,
                              int tou, int key, int s_key, int clr);
int land_animation2(int player, int card, int t_player, int t_card, int mode, int pow, int tou,
                         int key, int s_key, int clr, test_definition_t *this_test);
int card_generic_animated_land(int player, int card, event_t event);
int manland_shared(int player, int card, event_t event, int colorless, int black, int blue,
                        int green, int red, int white);
int manland_animated(int player, int card, event_t event, int colorless, int black, int blue,
                          int green, int red, int white);
int manland_normal(int player, int card, event_t event, int colorless, int black, int blue,
                        int green, int red, int white);
void cloning(int player, int card, int t_player, int t_card);
int enters_the_battlefield_as_copy_of_any_creature(int player, int card, event_t event);
int shapeshift_target(int src_player, int src_card, int t_player, int t_card,
                           int turn_into_player, int turn_into_card, int mode);
int real_shapeshift_target(int src_player, int src_card, int t_player, int t_card,
                                int turn_into_player, int turn_into_card, int mode);
static int shapeshift_target_effect(int player, int card, event_t event);
void shapeshift_all(int src_player, int src_card, int t_player, test_definition_t *test,
                         int turn_into_player, int turn_into_card, int mode);
void untap_only_1_permanent_per_upkeep(int player, int card, event_t event, int t_player,
                                            int type);
void untap_permanents_during_opponents_untap(int player, int card, type_t types,
                                                  int *where_to_store);
void choose_to_untap(int player, int card, event_t event);
void activated_abilities_disabler_enters_bf(int player, int card, int action);
int activated_abilities_disabler(int player, int card, event_t event, int action);
static const char *has_activated_ability(int who_chooses, int player, int card, int unused_source_player, int unused_source_card);
int tutor_card_with_the_same_name(int player, int card, int who_tutors, int t_player,
                                       int tutor_from, int tutor_to, int must_tutor);
void lose_the_game(int player);
void cannot_lose_the_game(int player, int card, event_t event, int who_cant_lose);
void cannot_lose_the_game_for_having_less_than_0_life(int player, int card, event_t event,
                                                           int who_cant_lose);
void store_attackers(int player, int card, event_t event, declare_attackers_trigger_t mode,
                          int attacker_player, int attacker_card, test_definition_t *test);
int resolve_declare_attackers_trigger(int player, int card, event_t event,
                                           declare_attackers_trigger_t mode);
int declare_attackers_trigger(int player, int card, event_t event,
                                   declare_attackers_trigger_t mode, int attacker_player,
                                   int attacker_card);
int declare_attackers_trigger_test(int player, int card, event_t event,
                                        declare_attackers_trigger_t mode, int attacker_player,
                                        int attacker_card, test_definition_t *test);
const char *get_hacked_land_text(int player, int card, const char *fmt, ...);
int effect_this_dies(int player, int card, event_t event);
int vanilla_creature_pumper(int player, int card, event_t event, int req_cless, int req_black,
                                 int req_blue, int req_green, int req_red, int req_white, int flags,
                                 int pow, int tou, int key, int s_key, target_definition_t *td);
int generic_shade_amt_can_pump(int player, int card, int pump, int life_to_pay, int cless,
                                    int black, int blue, int green, int red, int white, int limit);
int when_attacks_pump_self(int player, int card, event_t event, int power, int toughness);
int can_lose_the_game(int player);
static int fx_permanents_destroyed_this_way_accumulate(int player, int card, event_t event);
card_instance_t *legacy_permanents_destroyed_this_way_accumulate(
    int src_player, int src_card, card_instance_t *previous_effect, int tgt_player, int tgt_card);
void if_a_card_would_be_put_into_graveyard_from_play_exile_it_instead(
    int player, int card, int event, int t_player, test_definition_t *test);
int if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(
    int player, int card, int event, int t_player, resolve_trigger_t trigger_mode,
    test_definition_t *test);
int if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(
    int player, int card, int event, int t_player, resolve_trigger_t trigger_mode,
    test_definition_t *test);
void if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(
    int player, int card, int event, int t_player, test_definition_t *test);
int color_to_color_test(int color);
void damage_creature_and_all_the_others_with_the_same_name(int player, int card, int t_player,
                                                                int t_card, int dmg);
int alternate_legacy_text(int textnum, int player, int legacycard);
static int get_pt_for_creature_with_variable_pt(int player, int iid, event_t event);
static int split_cards_cmc_table_by_id(int csvid, int half_played);
static int prevent_all_damage_dealt_by_target_legacy(int player, int card, event_t event);
int damage_cannot_be_prevented_until_eot_legacy(int player, int card, event_t event);
int no_damage_this_turn(int player, int card, event_t event);
static int prevent_all_damage_to_target_legacy(int player, int card, event_t event);
int fog_effect_legacy(int player, int card, event_t event);
static int effect_prevent_the_next_n_damage(int player, int card, event_t event);
int prevent_damage_until_eot_legacy(int player, int card, event_t event);
int equipment_granting_activated_ability(int player, int card, event_t event,
                                              const char *dialog_option, int equip_cost, int mode,
                                              int cless, int black, int blue, int green, int red,
                                              int white, uint32_t variable_costs,
                                              target_definition_t *td, const char *prompt);
int generic_activated_ability_with_sacrifice_as_cost(
    int player, int card, event_t event, generic_activated_ability_flags_t flags, int cless,
    int black, int blue, int green, int red, int white, int gvc_costs, target_definition_t *td,
    const char *prompt, type_t type, test_definition_t *test,
    sacrifice_and_report_value_flags_t sac_flags);
int generic_activated_ability_with_target_in_grave(
    int player, int card, event_t event, generic_activated_ability_flags_t flags, int cless,
    int black, int blue, int green, int red, int white, int gvc_costs, target_definition_t *td,
    const char *prompt, type_t type, test_definition_t *test, int gs_flags_for_grave_recycler);
int generic_activated_ability_with_tapsubtype_as_cost(
    int player, int card, event_t event, generic_activated_ability_flags_t flags, int cless,
    int black, int blue, int green, int red, int white, int gvc_costs, target_definition_t *td,
    const char *prompt, target_definition_t *td_subtype, int req_tap);
static void count_for_gfp_ability_and_store_values_impl(int player, int card, int event,
                                                             int t_player, int type,
                                                             test_definition_t *this_test,
                                                             CardPairFunction extra_check, int mode,
                                                             int storage_location);
void count_for_gfp_ability_and_store_values_extra(int player, int card, int event,
                                                       int t_player, int type,
                                                       CardPairFunction extra_check, int mode,
                                                       int storage_location);
int enters_the_battlefield_as_copy_of_any(int player, int card, event_t event,
                                               target_definition_t *td, const char *prompt);
static const char *has_mana_to_cast_target(int who_chooses, int player, int card,
                                         int targeting_player, int targeting_card);
int can_play_cards_as_though_they_had_flash(int player, int card, event_t event,
                                                 target_definition_t *td, const char *prompt,
                                                 int literal);
static int discard_card_by_int_id(int player, int int_crd_id, int effect,
                                     test_definition_t *this_test, int source_of_effect);
int new_effect_coercion(ec_definition_t *this_definition, test_definition_t *this_test);
int exile_top_x_card_you_may_play_them(int player, int card, int howmany,
                                            test_definition_t *test, int t_player,
                                            mpcfe_mode_t flags);
char *type_text(char *buf, int maxlen, type_t type, type_text_t flags);
static void cannot_lose_the_game_impl(int player, int card, event_t event, int who_cant_lose,
                                       int for_life, int for_poison, int for_draw_on_empty_deck);
int beginning_of_combat(int player, int card, event_t event, int aff_player);
static int this_dies_trigger_impl(int player, int card, event_t event,
                                       resolve_trigger_t trigger_mode, int who_gets_trigger,
                                       int attached, int required_kill_code);
int lobotomy_effect(int player, int t_player, int csvid, int max, int flags);
void replace_all_instances_of_one_color_word_with_another(int player, int card, int t_player,
                                                               int t_card);
void replace_all_instances_of_one_basic_land_type_word_with_another(int player, int card,
                                                                         int t_player, int t_card);
int other_leaves_play(int triggering_player, int triggering_card, int t_player, int t_card,
                           event_t event, int can_be_suppressed, int can_be_duplicated);
int leaves_play(int player, int card, event_t event, int can_be_suppressed,
                     int can_be_duplicated);

#include <windows.h>

/* Private callback recovered below; used by the October 2022 deflection helpers. */
static char* has_only_1_target_and_could_legally_change_target(int who_chooses, int player, int card);

int discard_controller = 0;

unsigned int num_bits_set(unsigned int v){
  // From Bit Twiddling Hacks, http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  v = v - ((v >> 1) & 0x55555555);                              // reuse input as temporary
  v = (v & 0x33333333) + ((v >> 2) & 0x33333333);               // temp
  return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;      // count
}

// Allows use of memset() into an address we know is large enough, but the compiler thinks isn't.
void memset_suppress_size_warning(void* addr, int val, size_t sz)
{
	memset(addr, val, sz);
}

// Allows use of memcpy() into an address we know is large enough, but the compiler thinks isn't.
void memcpy_suppress_size_warning(void* dest, const void* src, size_t sz)
{
	memcpy(dest, src, sz);
}

int get_attack_power(int player, int card)
{
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *instance = in_play(p, c);
      if (!instance)
        continue;
      switch (cards_data[instance->internal_card_id].id)
      {
      case CARD_ID_ARCADES_THE_STRATEGIST:
        if (!check_for_ability(player, card, KEYWORD_DEFENDER))
          break;
        /* fall through */
      case CARD_ID_ASSAULT_FORMATION:
      case CARD_ID_THE_GREAT_FOREST:
      case CARD_ID_CONSPIRACY_WEIGHT_ADVANTAGE:
      case CARD_ID_HIGH_ALERT:
      case CARD_ID_HUATLI_THE_SUNS_HEART:
        if (p != player)
          break;
        /* fall through */
      case CARD_ID_DORAN_THE_SIEGE_TOWER:
        if (!is_humiliated(p, c, AL_STATIC))
          return get_toughness(player, card);
        break;
      }
    }
  return get_power(player, card);
}

int get_discard_controller(void){
	return discard_controller;
}

void set_discard_flag(int player, int card){
	raw_set_poison(HUMAN, 3);
   // raw_set_poison(AI, 1+card);
   // life[AI] = 20+event;
   life[AI] = reason_for_trigger_controller;
   life[HUMAN] = card_on_stack_controller;
	discard_controller = player;
	return;
}

int duh_mode(int player){
	return IS_AI(player) || get_setting(SETTING_DUH_MODE);
}

int dealt_damage_to_player(int player, int card, int parent_player, int parent_card, int event){
	//card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		card_instance_t *parent_instance = get_card_instance(parent_player, parent_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_source_player == player ){
				if(  damage->damage_source_card == card && damage->info_slot > 0 ){
					parent_instance->targets[16].player = damage->info_slot;
				}
			}
		}
	}

	card_instance_t *parent_instance = get_card_instance(parent_player, parent_card);
	if( trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(parent_player, parent_card) &&
		reason_for_trigger_controller == parent_player && parent_instance->targets[16].player > 0 ){
		if(event == EVENT_TRIGGER){
			event_result |= 2;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				parent_instance->targets[16].player = 0;
				return 1;
		}
	}
	return 0;
}

int has_combat_damage_been_inflicted_to_opponent(int player, int card, event_t event){
	return damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_OPPONENT);
}

int has_combat_damage_been_inflicted_to_a_player(int player, int card, event_t event){
	return damage_dealt_by_me(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER);
}

int deathtouch_until_eot(int player, int card, event_t event){

	if (event == EVENT_ABILITIES){
		card_instance_t *instance = get_card_instance( player, card );
		deathtouch(instance->targets[0].player, instance->targets[0].card, event);
	}

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int get_sum(int player, type_t type){
	int p = player;
	int count = 0;
	int sum = 0;
	while (count < active_cards_count[p]){
		if (in_play(p, count) && is_what(p, count, type)){
			sum += count;	// This doesn't do what you probably think it does.
		}
		count++;
	}
	return sum;
}

int get_hand_sum(int player, type_t type){
	int p = player;
	int count = 0;
	int sum = 0;
	while (count < active_cards_count[p]){
		if (in_hand(p, count) && is_what(p, count, type)){
			sum += count;	// This doesn't do what you probably think it does.
		}
		count++;
	}
	return sum;
}

static int id_updater_for_reveal_top_card_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->damage_target_player > -1 ){
		card_instance_t *leg = get_card_instance(instance->damage_target_player, instance->damage_target_card);
		if( leg->damage_target_player > -1 ){
			int* deck = deck_ptr[leg->damage_target_player];
			if( deck[0] != -1 ){
				leg->info_slot = -cards_data[deck[0]].id-1;
			}
		}
	}
	return 0;
}

int reveal_top_card(int player, int card, event_t event){

	if( event == EVENT_RESOLVE_SPELL ){
		int* deck = deck_ptr[player];
		if( deck[0] != -1 ){
			card_data_t* card_d = &cards_data[ deck[0] ];
			int legacy = create_card_name_legacy(player, card, card_d->id );
			int leg2 = create_targetted_legacy_effect(player, card, &id_updater_for_reveal_top_card_legacy, player, legacy);
			add_status(player, leg2, STATUS_INVISIBLE_FX);
		}
	}

	return 0;
}

int reveal_top_card_both_player(int player, int card, event_t event){

  card_instance_t *instance = get_card_instance(player, card);
  int p;

  if (event == EVENT_RESOLVE_SPELL)
	for (p = 0; p <= 1; ++p)
	  {
		instance->targets[p].player = player;
		if (deck_ptr[p][0] == -1)
		  instance->targets[p].card = -1;
		else
		  instance->targets[p].card = create_card_name_legacy(player, card, cards_data[deck_ptr[p][0]].id);
	  }

  if (affect_me(player, card) && instance->targets[0].player >= 0)
	{
	  for (p = 0; p <= 1; ++p)
		if (instance->targets[p].card == -1)
		  {
			if (deck_ptr[p][0] != -1)	// was empty, now has a card
			instance->targets[p].card = create_card_name_legacy(instance->targets[p].player, card, cards_data[deck_ptr[p][0]].id);
		  }
		else
		  {
			if (deck_ptr[p][0] == -1)	// had a card, now empty
			  {
				kill_card(instance->targets[p].player, instance->targets[p].card, KILL_EXILE);
				instance->targets[p].card = -1;
			  }
			else	// had a card, still does
			  {
				card_instance_t* legacy = get_card_instance(instance->targets[p].player, instance->targets[p].card);
				int csvid = cards_data[deck_ptr[p][0]].id;
				legacy->info_slot = -csvid - 1;	// update csvid if it's different
			  }
		  }
	}

	return 0;
}

int get_random_card(void){
	int i;
	int crd = -1;
	for(i=0; i<100; i++){
		crd+=100*internal_rand(16);
	}
	if( crd > (available_slots-1) ){
		crd -= (available_slots-1);
	}

	if( is_valid_card(crd) ){
		return crd;
	}
	else{
		while(1){
			crd++;
			if( is_valid_card(crd) ){
				return crd;
			}
		}
	}
	return -1;
}

// Can a card with this iid be played (other than mana cost and timing restrictions)?
int can_legally_play_iid(int player, int internal_card_id)
{
  card_data_t *card_d = &cards_data[internal_card_id];

  // Lands can only be played once per turn, and only during your turn.
  if ((card_d->type & TYPE_LAND) &&
      ((land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) || (current_turn != player)))
    return 0;

  // Instants, interrupts, sorceries, and enchantments (including permanents with flash and
  // planeswalkers) must return true from EVENT_CAN_CAST.
  if (card_d->type &
      (TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY |
       TYPE_ENCHANTMENT)) // deliberately not is_what(), so flash cards returning 99 are correct
  {
    /* Calling code pointers directly is DANGEROUS; exe and especially asm cards make assumptions
     * about what's in each register.  One of the things needed is a valid card instance, so we have
     * to create a card.  Besides, it's needed so targeting works right; and some especially
     * misbehaving C cards set values on the card during EVENT_CAN_CAST. */
    int card_added = add_card_to_hand(player, internal_card_id);
    card_instance_t *instance = get_card_instance(player, card_added);
    instance->state |= STATE_INVISIBLE;
    --hand_count[player];
    set_special_flags(player, card_added, SF_PLAYED_FOR_FREE);
    int rval = call_card_fn(card_d->code_pointer, instance, player, card_added, EVENT_CAN_CAST);
    obliterate_card_and_recycle(player, card_added);
    return rval;
  }

  // A (non-flash) creature, artifact, or land (when one hasn't been played, and it's your turn).
  return 1;
}

// Can a card with this iid be played (other than mana cost)?
int can_legally_play_iid_now(int player, int internal_card_id, event_t event)
{
  if ((cards_data[internal_card_id].type & (TYPE_INSTANT | TYPE_INTERRUPT))	// deliberately not is_what(), to include permanents with flash
	  || can_sorcery_be_played(player, event))
	return can_legally_play_iid(player, internal_card_id);
  else
	return 0;
}

// Can a card with this csvid be played (other than mana cost and timing restrictions)?
int can_legally_play_csvid(int player, int csvid)
{
  return can_legally_play_iid(player, get_internal_card_id_from_csv_id(csvid));
}

// Can a card with this csvid be played (other than mana cost)?
int can_legally_play_csvid_now(int player, int csvid, event_t event)
{
  return can_legally_play_iid_now(player, get_internal_card_id_from_csv_id(csvid), event);
}

int can_legally_play_card_in_hand(int player, int card)
{
  card_data_t* card_d = &cards_data[get_card_instance(player, card)->internal_card_id];

  // Lands can only be played once per turn, and only during your turn.
  if ((card_d->type & TYPE_LAND)
	  && ((land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) || (current_turn != player)))
	return 0;

  // Instants, interrupts, sorceries, and enchantments (including permanents with flash and planeswalkers) must return true from EVENT_CAN_CAST.
  if (card_d->type & (TYPE_INSTANT | TYPE_INTERRUPT | TYPE_SORCERY | TYPE_ENCHANTMENT))	// deliberately not is_what(), so flash cards returning 99 are correct
	{
		return call_card_function(player, card, EVENT_CAN_CAST);
	}

  // A (non-flash) creature, artifact, or land (when one hasn't been played, and it's your turn).
  return 1;
}

static int is_aura(int player, int card ){
	if( has_subtype(player, card, SUBTYPE_AURA) ){
		return 1;
	}
	return 0;
}

/* Exiles {t_player,t_card} and then returns it to the battlefield under its owner's control.  If before_etb is non-null, call that function with the card just
 * before it re-enters the battlefield. */
void blink_effect(int t_player, int t_card, void (*before_etb)(int, int))
{
  if (!in_play(t_player, t_card))
    return;
  int token = is_token(t_player, t_card);
  int iid = get_original_internal_card_id(t_player, t_card);
  card_instance_t *instance = get_card_instance(t_player, t_card);
  int owner = (instance->state & STATE_OWNED_BY_OPPONENT) ? 1 : 0;
  int csvid = cards_data[iid].id;
  int csvids[2] = {-1, -1};
  int parts = 1;
  if (csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
  {
    unmeld(csvid, &csvids[0], &csvids[1]);
    iid = get_internal_card_id_from_csv_id(csvids[0]);
    parts = 2;
  }
  kill_card(t_player, t_card, KILL_EXILE);
  if (token || !is_what(-1, iid, TYPE_PERMANENT))
    return;
  for (int part = 0; part < parts; ++part)
  {
    if (part)
      iid = get_internal_card_id_from_csv_id(csvids[1]);
    int card_added = add_card_to_hand(owner, iid);
    if (has_subtype(owner, card_added, SUBTYPE_AURA))
    {
      if (!call_card_function(owner, card_added, EVENT_CAN_CAST))
      {
        obliterate_card(owner, card_added);
        continue;
      }
      set_special_flags3(owner, card_added, SF3_IN_PLAY_DIRECTLY);
    }
    remove_card_from_rfg(owner, cards_data[iid].id);
    if (before_etb)
      before_etb(owner, card_added);
    put_into_play(owner, card_added);
  }
}

/* Exiles {player, card} and then returns it to the battlefield under it's controllers control.  If before_etb is non-null, call that function with the card just
 * before it re-enters the battlefield. */
void blink_effect_controller(int player, int card, void (*before_etb)(int, int))
{
  if (!in_play(player, card))
    return;
  int token = is_token(player, card);
  int iid = get_original_internal_card_id(player, card);
  card_instance_t *instance = get_card_instance(player, card);
  int owner = (instance->state & STATE_OWNED_BY_OPPONENT) ? 1 : 0;
  int csvid = cards_data[iid].id;
  int csvids[2] = {-1, -1};
  int parts = 1;
  if (csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
  {
    unmeld(csvid, &csvids[0], &csvids[1]);
    iid = get_internal_card_id_from_csv_id(csvids[0]);
    parts = 2;
  }
  kill_card(player, card, KILL_EXILE);
  if (token || !is_what(-1, iid, TYPE_PERMANENT))
    return;
  for (int part = 0; part < parts; ++part)
  {
    if (part)
      iid = get_internal_card_id_from_csv_id(csvids[1]);
    int card_added = add_card_to_hand(player, iid);
    if (has_subtype(player, card_added, SUBTYPE_AURA))
    {
      if (!call_card_function(player, card_added, EVENT_CAN_CAST))
      {
        obliterate_card(player, card_added);
        continue;
      }
      set_special_flags3(player, card_added, SF3_IN_PLAY_DIRECTLY);
    }
    remove_card_from_rfg(owner, cards_data[iid].id);
    if (before_etb)
      before_etb(player, card_added);
    if (player != owner)
    {
      if (player == AI)
        remove_state(player, card_added, STATE_OWNED_BY_OPPONENT);
      else
        add_state(player, card_added, STATE_OWNED_BY_OPPONENT);
    }
    put_into_play(player, card_added);
  }
}

static int remove_until_eot_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[1].card && eot_trigger(player, card, event))
  {
    if (check_rfg(instance->targets[0].player,
                  (unsigned int)cards_data[instance->targets[0].card].id))
    {
      int returned_card = add_card_to_hand(instance->targets[0].player, instance->targets[0].card);
      bool can_return = true;
      if (has_subtype(instance->targets[0].player, returned_card, SUBTYPE_AURA))
      {
        if (!call_card_function(instance->targets[0].player, returned_card, EVENT_CAN_CAST))
        {
          obliterate_card(instance->targets[0].player, returned_card);
          can_return = false;
        }
        else
          set_special_flags3(instance->targets[0].player, returned_card, 0x40);
      }
      if (can_return)
      {
        remove_card_from_rfg(instance->targets[0].player,
                             (unsigned int)cards_data[instance->targets[0].card].id);
        unsigned int mode = instance->targets[1].player;
        int return_player = instance->targets[0].player;
        if (mode & 3)
        {
          if ((mode & 1) || ((mode & 2) && is_what(return_player, returned_card, 2)))
            add_counters(player, card, instance->targets[0].player, returned_card, COUNTER_P1_P1,
                         1);
          return_player = instance->targets[0].player;
          if (instance->targets[1].player & 2)
          {
            if (is_what(return_player, returned_card, 0x100))
              add_counters(player, card, instance->targets[0].player, returned_card,
                           COUNTER_LOYALTY, 1);
            return_player = instance->targets[0].player;
          }
        }
        put_into_play(return_player, returned_card);
        short source_id = instance->display_pic_csv_id;
        if (source_id == 0x2112)
        {
          new_multidiscard(
              player,
              get_abilities(instance->targets[0].player, returned_card, EVENT_TOUGHNESS, -1), 0,
              player);
          source_id = instance->display_pic_csv_id;
        }
        if (source_id == 0x474b)
          get_card_instance(instance->targets[0].player, returned_card)->state |= 0x10;
      }
    }
    kill_card(player, card, KILL_EXILE);
  }
  if (event == EVENT_CLEANUP)
    instance->targets[1].card = 1;
  return 0;
}

int remove_until_eot(int player, int card, int t_player, int t_card)
{
  if (!in_play(t_player, t_card))
    return -1;
  int token = is_token(t_player, t_card);
  int iid = get_original_internal_card_id(t_player, t_card);
  card_instance_t *instance = get_card_instance(t_player, t_card);
  int owner = (instance->state & STATE_OWNED_BY_OPPONENT) ? 1 : 0;
  int csvid = cards_data[iid].id;
  int csvids[2] = {-1, -1};
  int parts = 1;
  if (csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST || csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP)
  {
    unmeld(csvid, &csvids[0], &csvids[1]);
    iid = get_internal_card_id_from_csv_id(csvids[0]);
    parts = 2;
  }
  kill_card(t_player, t_card, KILL_EXILE);
  if (token || !is_what(-1, iid, TYPE_PERMANENT))
    return -1;
  int legacy = -1;
  for (int part = 0; part < parts; ++part)
  {
    if (part)
      iid = get_internal_card_id_from_csv_id(csvids[1]);
    legacy = create_legacy_effect(player, card, remove_until_eot_legacy);
    card_instance_t *leg = get_card_instance(player, legacy);
    leg->targets[0].player = owner;
    leg->targets[0].card = iid;
    leg->targets[1].player = 0;
    switch (get_id(player, card))
    {
    case CARD_ID_LONG_ROAD_HOME:
    case CARD_ID_OTHERWORLDLY_JOURNEY:
      leg->targets[1].player = 1;
      break;
    case CARD_ID_SEMESTERS_END:
      leg->targets[1].player = 2;
      break;
    }
    leg->targets[1].card = current_phase != PHASE_CLEANUP;
  }
  return legacy;
}

void obliviation(int player, int card, int targ_player, int targ_card)
{
  if (!is_token(targ_player, targ_card) && in_play(targ_player, targ_card))
  {
    int iid = get_original_internal_card_id(targ_player, targ_card);
    int csvid = cards_data[iid].id;
    int csvids[2] = {-1, -1};
    int meld = csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST ||
               csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP;
    if (meld)
    {
      unmeld(csvid, &csvids[0], &csvids[1]);
      iid = get_internal_card_id_from_csv_id(csvids[0]);
    }
    for (int i = 0; i < (meld ? 2 : 1); ++i)
    {
      card_instance_t *legacy =
          get_card_instance(player, create_card_name_legacy(player, card, cards_data[iid].id));
      legacy->targets[7].player = get_owner(targ_player, targ_card);
      legacy->targets[7].card = iid;
      if (meld && i == 0)
        iid = get_internal_card_id_from_csv_id(csvids[1]);
    }
  }
  kill_card(targ_player, targ_card, KILL_EXILE);
}

void true_return_from_oblivion(int player, int card)
{
  int p, c, iid, owner;
  while (find_obliviation_legacy(player, card, &p, &c, &iid, &owner))
  {
    kill_card(p, c, KILL_EXILE);
    if (check_rfg(owner, cards_data[iid].id))
    {
      int card_added = add_card_to_hand(owner, iid);
      if (has_subtype(owner, card_added, SUBTYPE_AURA))
      {
        if (!call_card_function(owner, card_added, EVENT_CAN_CAST))
        {
          obliterate_card(owner, card_added);
          continue;
        }
        set_special_flags3(owner, card_added, SF3_IN_PLAY_DIRECTLY);
      }
      remove_card_from_rfg(owner, cards_data[iid].id);
      put_into_play(owner, card_added);
    }
  }
}

void return_from_oblivion(int player, int card, event_t event)
{
  leaves_play(player, card, event, RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    true_return_from_oblivion(player, card);
}

int check_for_cip_effects_removal(int player, int card)
{
  if (is_what(player, card, TYPE_CREATURE))
  {
    int i, k;
    for (i = 0; i < 2; i++)
    {
      for (k = 0; k < active_cards_count[i]; k++)
      {
        if (in_play(i, k) && !is_humiliated(i, k, AL_STATIC))
        {
          if (get_id(i, k) == CARD_ID_TORPOR_ORB || get_id(i, k) == CARD_ID_HUSHWING_GRYFF ||
              get_id(i, k) == CARD_ID_TOCATLI_HONOR_GUARD || get_id(i, k) == CARD_ID_HUSHBRINGER)
          {
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

int comes_into_play_mode(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && player == reason_for_trigger_controller
	  && player == trigger_cause_controller && card == trigger_cause
	  && !is_humiliated(player, card, AL_TRIG) && ! check_for_cip_effects_removal(player, card))
	{
	  if (event == EVENT_TRIGGER)
		{
		  if (trigger_mode == RESOLVE_TRIGGER_DUH)
			trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

		  event_result |= trigger_mode;
		}
	  else if (event == EVENT_RESOLVE_TRIGGER)
		return 1;
	}
  return 0;
}

int comes_into_play(int player, int card, event_t event)
{
  return comes_into_play_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY);
}

void comes_into_play_tapped(int player, int card, event_t event)
{
  if ((event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) &&
      affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (!((card_flags[0] | card_flags[1]) & CFLAGS_ARCHELOS) &&
        !(instance->ext->special_flags2 & SF2_FACE_DOWN_DUE_TO_MANIFEST) &&
        !is_humiliated(player, card, AL_TRIG))
      get_card_instance(player, card)->state |= STATE_TAPPED;
  }
}

// As leaves_play(), but allows a card ({triggering_player,triggering_card}) to watch for a different card ({t_player,t_card}) leaving play, not itself.

extern int hack_allow_sorcery_if_rules_engine_is_only_effect;
int can_sorcery_be_played(int player, event_t event){
	// Must be your own main phase
	if(current_turn != player || (current_phase != PHASE_MAIN1 && current_phase != PHASE_MAIN2)){
		return 0;
	}

	// If the stack is empty, good
	if (stack_size <= 0){
		return 1;
	}

	if (stack_size == 1){
		/* During EVENT_CAST_SPELL and EVENT_ACTIVATE, the card that's being cast or effect that's being activated is already on the stack.  If it's the only
		 * one on the stack, however, a sorcery was legal to play when it was put there. */
		if (event == EVENT_CAST_SPELL || event == EVENT_ACTIVATE){
			return 1;
		}

		/* A rules engine activation is a special case, since it'll indirectly call other card functions that themselves will call this with EVENT_CAN_ACTIVATE
		 * or similar instead of the original EVENT_ACTIVATE.  So we set hack_allow_sorcery_if_rules_engine_is_only_effect at the start of card_rules_engine()'s
		 * EVENT_CAN_ACTIVATE and clear it at the end. */
		if (hack_allow_sorcery_if_rules_engine_is_only_effect){
			card_instance_t* instance = get_card_instance(stack_cards[0].player, stack_cards[0].card);
			if (cards_data[instance->original_internal_card_id].id == CARD_ID_RULES_ENGINE){
				return 1;
			}
		}
	}

	// Nope, there's actually a spell or effect on the stack.
	return 0;
}

int count_subtype_in_hand(int player, subtype_t type){

  int i;
  int count = 0;

  for(i=0; i < active_cards_count[player]; i++){
	  if( in_hand(player, i) && has_subtype(player, i, type) ){
		 count++;
	  }
  }

  return count;
}

/* Don't send EVENT_UNTAP_CARD events.  Use only if this card is untapping only as an implementation detail, e.g. it was tapped to prevent it from tapping to
 * generate mana for its own activation cost, and the player cancelled during mana charging. */

/* If there's another pump_until_eot effect card originating from player/card attached to t_player/t_card, controlled by player, then add the power/toughness
 * specified here to that effect and return it.  (If you've manually fiddled with the effect's other target settings, you're on your own; they're not checked
 * for.)  Otherwise, create a new effect and return that instead.  In either case, set the effect's counter_power and counter_toughness so they can be seen by
 * |n in the effect text - which must specifically have |n's in it or this'll be hopelessly confusing. */

/* Shared in October 2022 (COFF external); also used by recovered aura helpers. */

/* Just like pump_ability_until_eot(), but can't pump power or toughness, and if the creature already has an effect card from {player,card} and has all the
 * named keywords set, only gives the existing effect a new timestamp instead of making a new one. */

/* Just like pump_creatures_until_eot(), but can't pump power or toughness, and if the creature already has an effect card from {player,card} and has all the
 * named keywords set, only gives the existing effect a new timestamp instead of making a new one. */

// Just like pump_creatures_until_eot_merge_pt(), but allows an alternate legacy text number.

/* Just like pump_creatures_until_eot(), but can't pump abilities, and sends its effect through pump_until_eot_merge_previous() so they combine into a single
 * effect card.  Requires that the card's effect text uses |n substitutions. */

static int legacy_switch_power_and_toughness(int player, int card, event_t event)
{
  // targets[0] = permanent this is attached to (also in card_instance_t::damage_target_player/card)
  // targets[7].player = pending switch

  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->damage_target_player, c = instance->damage_target_card;
  card_instance_t *aff = get_card_instance(p, c);

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c))
  {
    // Mark for revisit
    instance->targets[7].player = 1;

    // Make sure both power and toughness have been calculated
    get_abilities(p, c, EVENT_POWER, -1);
    get_abilities(p, c, EVENT_TOUGHNESS, -1);

    return 0;
  }

  if (event == EVENT_CLEANUP && affect_me(player, card))
  {
    if (get_card_instance(p, c)->ext->special_flags3 & SF3_SWITCH_POW_TGH)
      remove_special_flags3(p, c, SF3_SWITCH_POW_TGH);
    else
      set_special_flags3(p, c, SF3_SWITCH_POW_TGH);
    aff->regen_status |= KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS;
    kill_card(player, card, KILL_EXILE);
    return 0;
  }

  // Any other event
  if (instance->targets[7].player == 1)
  {
    instance->targets[7].player = 0;

    SWAP(aff->power, aff->toughness);
  }

  return 0;
}

int switch_power_and_toughness_until_eot(int player, int card, int t_player, int t_card)
{
  if (get_card_instance(t_player, t_card)->ext->special_flags3 & SF3_SWITCH_POW_TGH)
    remove_special_flags3(t_player, t_card, SF3_SWITCH_POW_TGH);
  else
    set_special_flags3(t_player, t_card, SF3_SWITCH_POW_TGH);
  return create_targetted_legacy_effect_no_repeat(player, card, legacy_switch_power_and_toughness,
                                                  t_player, t_card);
}

int pick_card_from_deck(int player){

	int selected = -1;

	if( player != AI ){
		selected = show_deck( player, deck_ptr[player], 500, "Pick a card", 0, gs_done_008b40e0 );
	}
	else{
		 int i;
		 int *deck = deck_ptr[player];
		 int max_value = -1;
		 int count = count_deck(player);
		 int pos = internal_rand(count);
		 for(i=pos; i<count; i++){
			 card_ptr_t* c = cards_ptr[ cards_data[deck[i]].id ];
			 if( c->ai_base_value > max_value ){
				 max_value = c->ai_base_value;
				 selected = i;
			 }
		 }
	}

	return selected;
}

// Deprecated.  Used only by pattern_of_rebirth_tutor() (called from Pattern of Rebirth at 0x1203847).
int tutor(int player, int type, int (*func_ptr)(int, int),  int tutor_location  ){
	int card_added = -1;
	int i=0;
	while(i++ < 50){
		int selected = pick_card_from_deck(player);
		if( selected && selected != -1){
			int *deck = deck_ptr[player];
			if (is_what(-1, deck[selected], type)){
				card_added = add_card_to_hand(player, deck[selected] );
				remove_card_from_deck( player, selected );
				if( func_ptr(player, card_added) ){
					if( tutor_location == TUTOR_PLAY ){
						put_into_play(player, card_added);
					}
					else if( tutor_location == TUTOR_GRAVE ){
						kill_card(player, card_added, KILL_DESTROY);
					}
					break;
				}
				else{
					put_on_top_of_deck(player, card_added);
				}
			}
		}
		else{
			break;
		}
	}
	shuffle(player);
	if( tutor_location == TUTOR_DECK ){
		put_on_top_of_deck(player, card_added);
	}
	return card_added;
}

int legacy_effect_activated(int player, int card, event_t event){
	card_instance_t* instance = get_card_instance(player, card);
#ifdef MANALINKEH_STATIC
	// Wasm function pointers are table indices, including values below 4096.
	// Keep the original Nafs Asp record distinct from callback-based effects.
	if (instance->internal_card_id == LEGACY_EFFECT_ACTIVATED){
		return legacy_effect_custom(player, card, event);
#else
	if (instance->info_slot > 4096){
		return ((int (*)(int, int, event_t))instance->info_slot)(player, card, event);
#endif
	} else {
		// Original Nafs Asp legagy

		if (event == EVENT_CAN_ACTIVATE){
			return has_mana(player, COLOR_ANY, 1);
		} else if (event == EVENT_ACTIVATE){
			if (affect_me(player, card) && has_mana(player, COLOR_ANY, 1)){
				charge_mana(player, COLOR_COLORLESS, 1);
			}
		}
		else if (event == EVENT_RESOLVE_ACTIVATION){
			kill_card(player, instance->parent_card, KILL_EXILE);
		}
		else if ((trigger_condition == TRIGGER_DRAW_PHASE || event == EVENT_SHOULD_AI_PLAY)
				 && affect_me(player, card)
				 && current_turn == player
				 && reason_for_trigger_controller == player
				 && get_card_instance(player, card)->info_slot == 0){
			if (event == EVENT_TRIGGER){
				event_result |= 2;
			} else if (event == EVENT_RESOLVE_TRIGGER || event == EVENT_SHOULD_AI_PLAY){
				load_text(0, "NAFS_ASP");
				do_dialog(player, player, card, -1, -1, text_lines[0], 0);
				lose_life(player, 1);
				kill_card(player, card, KILL_BURY);
			}
		} else if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY){
			if (ai_is_speculating == 1){
				--life[player];
			}
			instance->info_slot = 0;
		}
		return 0;
	}
}

int create_legacy_effect(int player, int card, int (*func_ptr)(int, int, event_t) ){
	int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_CUSTOM, -1, -1);
	card_instance_t *instance = get_card_instance(player, result);
	instance->info_slot = (intptr_t) func_ptr;
	return result;
}

int create_legacy_activate(int player, int card, int (*func_ptr)(int, int, event_t) ){
	int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_ACTIVATED, -1, -1);
	card_instance_t *instance = get_card_instance(player, result);
	instance->info_slot = (intptr_t)func_ptr;
	return result;
}

int create_targetted_legacy_activate(int player, int card, int (*func_ptr)(int, int, event_t), int t_player, int t_card){
	int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_ACTIVATED, t_player, t_card);
	card_instance_t *instance = get_card_instance(player, result);
	instance->info_slot = (intptr_t)func_ptr;
	return result;
}

int create_targetted_legacy_effect(int player, int card, int (*func_ptr)(int, int, event_t), int target_player_id, int target_card ){
	int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_CUSTOM, target_player_id, target_card);
	card_instance_t *instance = get_card_instance(player, result);
	instance->info_slot =  (intptr_t) func_ptr;
	instance->targets[0].player = target_player_id;
	instance->targets[0].card = target_card;
	instance->number_of_targets = 1;
	return result;
}

/* If there's already an effect card attached to {target_player_id,target_card} with func_ptr created by {player,card}, give it a new timestamp and return it;
 * else return -1. */
int find_repeat_legacy_effect(int player, int card, CardFunction func_ptr, int target_player_id,
                              int target_card)
{
  target_t found = find_repeat_legacy_effect_alt(player, card, func_ptr, target_player_id,
                                                 target_card, -1, player);
  return found.card;
}

/* Just like create_targetted_legacy_effect(), but if there's an effect from the same source with the same func_ptr already affecting the card, just gives that
 * a new timestamp instead. */
int create_targetted_legacy_effect_no_repeat(int player, int card, int (*func_ptr)(int, int, event_t), int target_player_id, int target_card){
	int eff = find_repeat_legacy_effect(player, card, func_ptr, target_player_id, target_card);
	if (eff != -1){
		return eff;
	} else {
		return create_targetted_legacy_effect(player, card, func_ptr, target_player_id, target_card);
	}
}

// Creates a legacy effect from (player,card) under control of 1-player.
int create_legacy_effect_for_opponent(int player, int card, int (*func_ptr)(int, int, event_t), int t_player, int t_card)
{
  int card_added = add_card_to_hand(1-player, get_card_instance(player, card)->internal_card_id);
  ASSERT(card_added != -1);
  card_instance_t* temp = get_card_instance(1-player, card_added);

  // Make sure it's neither in hand nor on battlefield when it gets obliterated
  temp->state = (temp->state | STATE_INVISIBLE) & ~STATE_IN_PLAY;
  --hand_count[1-player];

  /* So all the values on the created legacy are correct - create_legacy_effect copies card_color, mana_color, color, STATUS_SLEIGHTED, STATUS_HACKED,
   * color_id[0..5], and hack_mode[0..5] from (player,card); and sets legacy->initial_color to (player,card)->color. */
  memcpy_inst_and_ext(temp, get_card_instance(player, card));

  // Still neither in hand nor on battlefield
  temp->state = (temp->state | STATE_INVISIBLE) & ~STATE_IN_PLAY;

  int legacy = create_legacy_effect(1-player, card_added, func_ptr);
  obliterate_card(1-player, card_added);

  // Fixup created legacy to point to original card as source
  card_instance_t* leg = get_card_instance(1-player, legacy);
  leg->damage_source_player = player;
  leg->damage_source_card = card;

  // And same art version
  leg->display_pic_num = get_card_image_number(leg->display_pic_csv_id, player, card);

  // Redundant attachment in targets[0], per create_targetted_legacy_effect()
  if (t_player >= 0)
	{
	  leg->targets[0].player = t_player;
	  leg->targets[0].card = t_card;
	  leg->number_of_targets = 1;
	}

  return legacy;
}

/* If this effect is controlled by a different player than the card it's attached to, then transfer control and ownership of the effect.  Calling effect should
 * return 0 if this returns true. */
int effect_follows_control_of_attachment(int player, int card, int event)
{
  if (event == EVENT_STATIC_EFFECTS)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->damage_target_player, c = instance->damage_target_card;

	  if (p != player)
		{
		  // Transfer control and ownership of effect
		  int leg = create_targetted_legacy_effect(p, 0, &empty, p, c);
		  card_instance_t* legacy = get_card_instance(p, leg);
		  int old_timestamp = legacy->timestamp;
		  memcpy_inst_and_ext(legacy, instance);
		  legacy->timestamp = old_timestamp;
		  legacy->state ^= STATE_OWNED_BY_OPPONENT;
		  kill_card(player, card, KILL_EXILE);
		  return 1;
		}
	}
  return 0;
}

int create_my_legacy(int player, int card, int (*func_ptr)(int, int, event_t)){
	int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_CUSTOM, -1, -1);
	card_instance_t *instance = get_card_instance(player, result);
	instance->info_slot =  (intptr_t) func_ptr;
	instance->targets[0].player = player;
	instance->targets[0].card = card;
	instance->number_of_targets = 1;
	// add_status(player, result, STATUS_INVISIBLE_FX);
	return result;
}

void add_legacy_effect_to_all(int player, int card, int (*func_ptr)(int, int, event_t), int t_player, test_definition_t *this_test){
	int i;
	for(i=0; i<2; i++){
		if( i == t_player || t_player == 2 ){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && new_make_test_in_play(i, count, -1, this_test) ){
						int result = create_legacy_effect_exe(player, card, LEGACY_EFFECT_CUSTOM, i, count);
						card_instance_t *instance = get_card_instance(player, result);
						instance->info_slot =  (intptr_t) func_ptr;
						instance->targets[0].player = i;
						instance->targets[0].card = count;
					}
					count--;
			}
		}
	}
}

int resolve_graveyard_trigger(int player, int card, event_t event){
	card_instance_t *instance= get_card_instance(player, card);
	int good = 0;
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && player == reason_for_trigger_controller){
		if( affect_me(player, card ) ){
			if( instance->targets[11].player > 0 ){
				good = 1;
			}
		}
	}

	if( good == 1 ){
		if(event == EVENT_TRIGGER){
			//Make all trigges mandatoy for now
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			return 1;
		}
	}
	return 0;
}

void does_not_untap(int player, int card, event_t event){
	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && affect_me(player, card) ){
		card_instance_t *instance= get_card_instance(player, card);
		instance->untap_status &= ~3;
	}
}

static int effect_does_not_untap(int player, int card, event_t event)
{
  /* targets[0]: Permanent this is attached to (also in card_instance_t::damage_target_player/card)
   * targets[1]: Source of effect.
   * targets[2].player: Number of untap steps to skip.
   * targets[2].card: mode, per ednt_flags_t. */

  card_instance_t *instance = get_card_instance(player, card);

  int flags = instance->targets[2].card;

  int t_player = instance->damage_target_player;
  int t_card = instance->damage_target_card;

  if (flags && (flags & EDNT_TAP_AT_END_OF_COMBAT))
  {
    if (end_of_combat_trigger(player, card, event, 2))
    {
      tap_card(t_player, t_card);
      instance->targets[2].card &= ~EDNT_TAP_AT_END_OF_COMBAT;
    }
  }

  if (!flags ||
      (flags && (flags & EDNT_CHECK_ORIGINAL_CONTROLLER) &&
       current_turn == instance->targets[3].player) ||
      (flags && (flags & (EDNT_REMAIN_TAPPED_UNTIL_SOURCE_LEAVES_PLAY | EDNT_WONT_UNTAP_ANYMORE))))
  {
    does_not_untap(t_player, t_card, event);
  }

  if (event == EVENT_END_OF_UNTAP_STEP && affected_card_controller == t_player)
  {
    if (!flags || (flags && !(flags & (EDNT_REMAIN_TAPPED_UNTIL_SOURCE_LEAVES_PLAY |
                                       EDNT_WONT_UNTAP_ANYMORE))))
    {
      if (instance->targets[2].player > 1)
      {
        --instance->targets[2].player;
      }
      else
      {
        remove_special_flags2(t_player, t_card, SF2_COULD_NOT_UNTAP);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (flags && (flags & EDNT_REMAIN_TAPPED_UNTIL_SOURCE_LEAVES_PLAY) &&
      instance->targets[2].player > -1)
  {
    leaves_play(instance->targets[1].player, instance->targets[1].card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      remove_special_flags2(t_player, t_card, SF2_COULD_NOT_UNTAP);
      kill_card(player, card, KILL_EXILE);
    }
  }
  if (event == EVENT_CARDCONTROLLED &&
      affect_me(instance->targets[1].player, instance->targets[1].card))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int does_not_untap_effect(int player, int card, int t_player, int t_card, int mode, int turns){
	if( mode & EDNT_TAP_TARGET ){
		tap_card(t_player, t_card);
	}
	int legacy = create_targetted_legacy_effect(player, card, &effect_does_not_untap, t_player, t_card);
	set_special_flags2(t_player, t_card, SF2_COULD_NOT_UNTAP);
	card_instance_t *instance = get_card_instance( player, legacy );

	instance->targets[1].player = player;
	instance->targets[1].card = card;
	instance->number_of_targets = 2;

	instance->targets[2].card = mode;
	instance->targets[2].player = turns;

	/*
	5/1/2007: "Your next untap..." refers to the player who controls [this permanent] when the triggered ability resolves.
	9/22/2011: If another player controls [this permanent] during your next untap step, the part of the effect that keeps it
				from untapping does nothing. It won't try to apply during a future untap step.
	*/
	instance->targets[3].player = t_player;

	return legacy;
}

static int dnuimt_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->damage_target_player > -1 && instance->targets[1].player > -1){
		if( in_play(instance->targets[1].player, instance->targets[1].card) && is_tapped(instance->targets[1].player, instance->targets[1].card) ){
			does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
		}
		else{
			kill_card(player, card, KILL_EXILE);
		}
	}

	return 0;
}

int does_not_untap_until_im_tapped(int player, int card, int t_player, int t_card){
	int legacy = create_targetted_legacy_effect(player, card, &dnuimt_legacy, t_player, t_card);
	card_instance_t *leg = get_card_instance(player, legacy);
	leg->targets[1].player = player;
	leg->targets[1].card = card;
	leg->number_of_targets = 2;
	return legacy;
}

static int ability_doesnt_untap_while_has_a_counter_and_remove_a_counter_at_upkeep(int player,
                                                                                   int card,
                                                                                   event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  counter_t type = instance->targets[1].card;
  if (event == EVENT_UNTAP)
  {
    if (count_counters(instance->damage_target_player, instance->damage_target_card, type))
      does_not_untap(instance->damage_target_player, instance->damage_target_card, EVENT_UNTAP);
    if (trigger_condition != TRIGGER_UPKEEP)
      return 0;
  }
  if (trigger_condition == TRIGGER_UPKEEP && card == affected_card && player == affected_card_controller &&
      count_counters(instance->damage_target_player, instance->damage_target_card, type))
    upkeep_trigger_ability_mode(player, card, event, instance->damage_target_player,
                                RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    remove_counters(player, card, instance->damage_target_player, instance->damage_target_card,
                    type, 1);
  return 0;
}
void gains_doesnt_untap_while_has_a_counter_and_remove_a_counter_at_upkeep(int src_player, int src_card, int t_player, int t_card, counter_t counter_type)
{
  int legacy = create_targetted_legacy_effect(src_player, src_card, ability_doesnt_untap_while_has_a_counter_and_remove_a_counter_at_upkeep, t_player, t_card);
  get_card_instance(src_player, legacy)->targets[1].card = counter_type;
}

int select_card_from_graveyard(int player, int card, int whose_grave, int type ){
	int card_added = -1;
	while(1){
		int selected = show_deck( player, get_grave(whose_grave), 500, "Pick a card", 0, gs_done_008b40e0 );
		if( selected != -1){
			const int *grave = get_grave(whose_grave);
			if (is_what(-1, grave[selected], type)){
				card_added =  add_card_to_hand(player, grave[selected] );

				card_instance_t *instance = get_card_instance(player, card);
				instance->targets[0].player = player;
				instance->targets[1].player = whose_grave;
				instance->targets[0].card = card_added;
				instance->number_of_targets = 1;
				instance->info_slot = selected;

				card_instance_t *target = get_card_instance(player, card_added);
				target->state |= STATE_INVISIBLE;
				--hand_count[player];

				return card_added;
			}
		}
		else{
			break;
		}
	}
	return card_added;
}

/* These variants should be called instead of charge_mana()/charge_mana_multi() during EVENT_RESOLVE_TRIGGER, EVENT_RESOLVE_SPELL, and RESOLVE_ACTIVATION, so
 * the prompt is correct. */

// An unusual case - the card is worded to be an activated, not a triggered ability, but we're charging for it during resolution.

int eot_trigger(int player, int card, event_t event){
	if(trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player &&
		(is_what(player, card, TYPE_EFFECT) || ! is_humiliated(player, card, AL_TRIG)) ){
		if(event == EVENT_TRIGGER){
			event_result |= 2;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			return 1;
		}
	}
	return 0;
}

int eot_trigger_mode(int player, int card, event_t event, int t_player, resolve_trigger_t trig_mode){
	if( (current_turn == t_player || t_player == ANYBODY) && trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player &&
		(is_what(player, card, TYPE_EFFECT) || ! is_humiliated(player, card, AL_TRIG))
	  ){
		if(event == EVENT_TRIGGER){
			event_result |= (trig_mode == RESOLVE_TRIGGER_DUH ? (duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL) : trig_mode);
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				return 1;
		}
	}
	return 0;
}

void all_must_attack_if_able(int player, event_t event, int subtype)
{
	event_flags |= EA_FORCE_ATTACK;	// just in case it's not set in ct_all.csv - it won't be for effect cards

	if( event == EVENT_MUST_ATTACK && current_turn == player && !forbid_attack ){
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_CREATURE) && (subtype < 0 || has_subtype(player, c, subtype)) ){
				attack_if_able(player, c, event);
			}
		}
	}
}

int cycling(int player, int card, event_t event, int colorless, int black, int blue, int green,
            int red, int white)
{
  if (check_battlefield_for_id(player, CARD_ID_NEW_PERSPECTIVES) && hand_count[player] > 6)
    colorless = black = blue = green = red = white = 0;

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    colorless -= (2 * count_cards_by_id(player, CARD_ID_FLUCTUATOR));
    colorless += (2 * count_cards_by_id(ANYBODY, CARD_ID_SUPPRESSION_FIELD));

    if (colorless < 0)
    {
      colorless = 0;
    }
    if (has_mana_multi(player, colorless, black, blue, green, red, white))
    {
      return 1;
    }
  }
  else if (event == EVENT_ACTIVATE_FROM_HAND)
  {
    colorless -= (2 * count_cards_by_id(player, CARD_ID_FLUCTUATOR));
    colorless += (2 * count_cards_by_id(ANYBODY, CARD_ID_SUPPRESSION_FIELD));

    if (colorless < 0)
    {
      colorless = 0;
    }

    charge_mana_multi(player, colorless, black, blue, green, red, white);
    if (spell_fizzled != 1)
    {
      card_is_being_cycled = 1;
      discard_card(player, card);
      card_is_being_cycled = 0;
      return 2;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1)
  {
    draw_a_card(player);
  }
  return 0;
}

int cycling_hybrid(int player, int card, event_t event, int amt_colored, color_t first_color,
                   color_t second_color, int amt_colorless)
{
  if (check_battlefield_for_id(player, CARD_ID_NEW_PERSPECTIVES) && hand_count[player] >= 7)
  {
    amt_colorless = amt_colored = 0;
    first_color = second_color = COLOR_COLORLESS;
  }

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND)
  {
    amt_colorless += 2 * count_cards_by_id(ANYBODY, CARD_ID_SUPPRESSION_FIELD);
    if (amt_colorless > 0)
    {
      amt_colorless -= 2 * count_cards_by_id(player, CARD_ID_FLUCTUATOR);
      amt_colorless = MAX(0, amt_colorless);
    }
  }

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
    return has_mana_hybrid(player, amt_colored, first_color, second_color, amt_colorless) ? 1 : 0;

  if (event == EVENT_ACTIVATE_FROM_HAND &&
      charge_mana_hybrid(player, card, amt_colored, first_color, second_color, amt_colorless))
  {
    card_is_being_cycled = 1;
    discard_card(player, card);
    card_is_being_cycled = 0;
    return 2;
  }

  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1)
    draw_a_card(player);

  return 0;
}

// Keeps the cycling abilities with odd activation costs in close proximity to normal cycling implementation.
int cycling_special_cost(int player, int card, event_t event, int mode)
{
  // Mode == 1: sacrifice a land
  // Mode == 2: pay 2 life

  int amt_colorless = 0;
  if (check_battlefield_for_id(player, CARD_ID_NEW_PERSPECTIVES) && hand_count[player] >= 7)
  {
    mode = 0;
  }

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND || event == EVENT_ACTIVATE_FROM_HAND)
  {
    amt_colorless += 2 * count_cards_by_id(ANYBODY, CARD_ID_SUPPRESSION_FIELD);
    if (amt_colorless > 0)
    {
      amt_colorless -= 2 * count_cards_by_id(player, CARD_ID_FLUCTUATOR);
      amt_colorless = MAX(0, amt_colorless);
    }
  }

  if (event == EVENT_CAN_ACTIVATE_FROM_HAND)
  {
    if (amt_colorless > 0 && !has_mana(player, COLOR_ANY, amt_colorless))
      return 0;

    switch (mode)
    {
    case 1:
      return can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ? 1 : 0;
    case 2:
      return can_pay_life(player, 2) ? 1 : 0;
    }

    return 1;
  }

  if (event == EVENT_ACTIVATE_FROM_HAND)
  {
    if (amt_colorless > 0)
    {
      charge_mana(player, COLOR_COLORLESS, amt_colorless);
      if (cancel == 1)
        return 0;
    }

    switch (mode)
    {
    case 1:
      if (!controller_sacrifices_a_permanent(player, card, TYPE_LAND, 0))
      {
        cancel = 1;
        return 0;
      }
      break;

    case 2:
      lose_life(player, 2);
      break;
    }

    card_is_being_cycled = 1;
    discard_card(player, card);
    card_is_being_cycled = 0;
    return 2;
  }

  if (event == EVENT_RESOLVE_ACTIVATION_FROM_HAND && spell_fizzled != 1)
    draw_a_card(player);

  return 0;
}

int choose_a_land(int player, int ai_choice){
	int result = -1;
	while(result == -1){
		result = choose_a_color_exe(player, "Choose a basic land type", 0, ai_choice, COLOR_TEST_ANY_COLORED);
	}
	return result;
}

int upkeep_trigger_mode(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
	return upkeep_trigger_mode_impl(player, card, event, trigger_mode, 0);
}

int upkeep_trigger_no_check(int player, int card, event_t event)
{
	//This won't check for cards that skips or adds Upkeep Phases, so it should be done by the card calling this functions.
	//Useful for cards that create an effect card that will trigger on next Upkeep.
	//Example of usage: "cantrip" in "ice_age.c"
	return upkeep_trigger_mode_impl(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1);
}

int upkeep_trigger(int player, int card, event_t event)
{
  return upkeep_trigger_mode_impl(player, card, event, RESOLVE_TRIGGER_MANDATORY, 0);
}

int global_enchantment(int player, int card, event_t event){
	if( event == EVENT_SHOULD_AI_PLAY ){
		ai_modifier += (player == HUMAN) ? -8 : 8;
	}
	else if( event == EVENT_CAN_CAST ){
		return 1;
	}
	return 0;
}

int enchant_world(int player, int card, event_t event)
{
  if (event == EVENT_SHOULD_AI_PLAY)
	ai_modifier += (player == HUMAN) ? -8 : 8;

  if (event == EVENT_CAN_CAST)
	return 1;

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player && trigger_cause == card)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  int p, c;
		  card_instance_t* inst;
		  for (p = 0; p <= 1; ++p)
			for (c = 0; c < active_cards_count[p]; ++c)
			  if ((inst = in_play(p, c)) && !(c == card && p == player) && cards_data[inst->internal_card_id].subtype == SUB_ENCHANT_WORLD)
				kill_card(p, c, KILL_STATE_BASED_ACTION);
		}
	}

  return 0;
}

static int split_cards_color_table(int player, int card){
	int csvid = player != -1 ? get_id(player, card) : cards_data[card].id;
	int half_played = get_card_instance(player, card)->info_slot;
	int result = 0;
	if( half_played ){
		switch( csvid ){
				// Invasion
				case CARD_ID_STAND_DELIVER: //Not yet coded
					result = (half_played & 1) ? COLOR_TEST_WHITE : COLOR_TEST_BLUE;
					break;
				case CARD_ID_SPITE_MALICE:
					result = (half_played & 1) ? COLOR_TEST_BLUE : COLOR_TEST_BLACK;
					break;
				case CARD_ID_PAIN_SUFFERING: //Not yet coded
					result = (half_played & 1) ? COLOR_TEST_BLACK : COLOR_TEST_RED;
					break;
				case CARD_ID_ASSAULT_BATTERY:
					result = (half_played & 1) ? COLOR_TEST_RED : COLOR_TEST_GREEN;
					break;
				case CARD_ID_WAX_WANE:
					result = (half_played & 1) ? COLOR_TEST_GREEN : COLOR_TEST_WHITE;
					break;
				// Apocalypse
				case CARD_ID_FIRE_ICE:
					result = (half_played & 1) ? COLOR_TEST_RED : COLOR_TEST_BLUE;
					break;
				case CARD_ID_ILLUSION_REALITY: //Not yet coded
					result = (half_played & 1) ? COLOR_TEST_BLUE : COLOR_TEST_GREEN;
					break;
				case CARD_ID_LIFE_DEATH:
					result = (half_played & 1) ? COLOR_TEST_GREEN : COLOR_TEST_BLACK;
					break;
				case CARD_ID_NIGHT_DAY: //Not yet coded
					result = (half_played & 1) ? COLOR_TEST_BLACK : COLOR_TEST_WHITE;
					break;
				case CARD_ID_ORDER_CHAOS:
					result = (half_played & 1) ? COLOR_TEST_WHITE : COLOR_TEST_RED;
					break;
				// Dissension
				case CARD_ID_BOUND_DETERMINED:
					result = (half_played & 1) ? COLOR_TEST_BLACK | COLOR_TEST_GREEN : COLOR_TEST_BLUE | COLOR_TEST_GREEN;
					break;
				case CARD_ID_CRIME_PUNISHMENT:
					result = (half_played & 1) ? COLOR_TEST_BLACK | COLOR_TEST_WHITE : COLOR_TEST_BLACK | COLOR_TEST_GREEN;
					break;
				case CARD_ID_HIDE_SEEK:
					result = (half_played & 1) ? COLOR_TEST_RED | COLOR_TEST_WHITE : COLOR_TEST_BLACK | COLOR_TEST_WHITE;
					break;
				case CARD_ID_HIT_RUN:
					result = (half_played & 1) ? COLOR_TEST_BLACK |  COLOR_TEST_RED : COLOR_TEST_GREEN | COLOR_TEST_RED;
					break;
				case CARD_ID_ODDS_ENDS:
					result = (half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_RED : COLOR_TEST_RED | COLOR_TEST_WHITE;
					break;
				case CARD_ID_PURE_SIMPLE:
					result = (half_played & 1) ? COLOR_TEST_GREEN | COLOR_TEST_RED : COLOR_TEST_GREEN | COLOR_TEST_WHITE;
					break;
				case CARD_ID_RESEARCH_DEVELOPMENT:
					result = (half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_GREEN : COLOR_TEST_BLUE | COLOR_TEST_RED;
					break;
				case CARD_ID_RISE_FALL:
					result = (half_played & 1) ? COLOR_TEST_BLACK | COLOR_TEST_BLUE : COLOR_TEST_BLACK | COLOR_TEST_RED;
					break;
				case CARD_ID_SUPPLY_DEMAND:
					result = (half_played & 1) ?COLOR_TEST_GREEN | COLOR_TEST_WHITE : COLOR_TEST_BLUE | COLOR_TEST_WHITE;
					break;
				case CARD_ID_TRIAL_ERROR:
					result = (half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_WHITE : COLOR_TEST_BLACK | COLOR_TEST_BLUE;
					break;
				// Dragon's Maze
				case CARD_ID_ALIVE_WELL:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_GREEN | COLOR_TEST_WHITE : ((half_played & 1) ? COLOR_TEST_GREEN : COLOR_TEST_WHITE);
					break;
				case CARD_ID_ARMED_DANGEROUS:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_GREEN | COLOR_TEST_RED : ((half_played & 1) ? COLOR_TEST_RED : COLOR_TEST_GREEN);
					break;
				case CARD_ID_BECK_CALL:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_GREEN | COLOR_TEST_WHITE :
												((half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_GREEN : COLOR_TEST_BLUE | COLOR_TEST_WHITE);
					break;
				case CARD_ID_BREAKING_ENTERING:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_BLACK | COLOR_TEST_RED :
												((half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_BLACK : COLOR_TEST_BLACK | COLOR_TEST_RED);
					break;
				case CARD_ID_CATCH_RELEASE:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_RED | COLOR_TEST_WHITE :
												((half_played & 1) ? COLOR_TEST_BLUE | COLOR_TEST_RED : COLOR_TEST_RED | COLOR_TEST_WHITE);
					break;
				case CARD_ID_DOWN_DIRTY:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_GREEN | COLOR_TEST_BLACK : ((half_played & 1) ? COLOR_TEST_BLACK : COLOR_TEST_GREEN);
					break;
				case CARD_ID_FAR_AWAY:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLACK | COLOR_TEST_BLUE : ((half_played & 1) ? COLOR_TEST_BLUE : COLOR_TEST_BLACK);
					break;
				case CARD_ID_FLESH_BLOOD:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLACK | COLOR_TEST_GREEN | COLOR_TEST_RED :
												((half_played & 1) ? COLOR_TEST_BLACK | COLOR_TEST_GREEN : COLOR_TEST_GREEN | COLOR_TEST_RED);
					break;
				case CARD_ID_GIVE_TAKE:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_GREEN : ((half_played & 1) ? COLOR_TEST_GREEN : COLOR_TEST_BLUE);
					break;
				case CARD_ID_PROFIT_LOSS:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLACK | COLOR_TEST_WHITE : ((half_played & 1) ? COLOR_TEST_WHITE : COLOR_TEST_BLACK);
					break;
				case CARD_ID_PROTECT_SERVE:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_WHITE : ((half_played & 1) ? COLOR_TEST_WHITE : COLOR_TEST_BLUE);
					break;
				case CARD_ID_READY_WILLING:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLACK | COLOR_TEST_GREEN | COLOR_TEST_WHITE :
												((half_played & 1) ? COLOR_TEST_GREEN | COLOR_TEST_WHITE : COLOR_TEST_BLACK | COLOR_TEST_WHITE);
					break;
				case CARD_ID_TOIL_TROUBLE:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLACK | COLOR_TEST_RED : ((half_played & 1) ? COLOR_TEST_BLACK : COLOR_TEST_RED);
					break;
				case CARD_ID_TURN_BURN:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_BLUE | COLOR_TEST_RED : ((half_played & 1) ? COLOR_TEST_BLUE : COLOR_TEST_RED);
					break;
				case CARD_ID_WEAR_TEAR:
					result = (half_played & 1) && (half_played & 2) ? COLOR_TEST_WHITE | COLOR_TEST_RED : ((half_played & 1) ? COLOR_TEST_RED : COLOR_TEST_WHITE);
					break;
				default:
					break;
		}
	}
	return result;
}

// Use this only for checking few cards in a row, otherwise store get_global_color_hack in a variable and use get_color_by_internal_id_no_hack

// A shim between get_color() and get_color_by_internal_id().

int pick_creature_to_regen(target_definition_t td){
	if( pick_target(&td, "TARGET_CREATURE_TO_REGEN") ){
		card_instance_t *instance = get_card_instance(td.player, td.card);
		while( spell_fizzled != 1 ){
			card_instance_t *creature = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( creature->kill_code == KILL_DESTROY && !(creature->token_status & STATUS_CANNOT_REGENERATE) ){
				break;
			}
			pick_target(&td, "TARGET_CREATURE_TO_REGEN");
		}
		if( spell_fizzled != 1 ){
			return 1;
		}
	}
	return 0;
}

static int split_cards_cmc_table(int player, int card){
	// For putting things simple, we'll always assume that the CMC we get from any other zone aside from STATE_IN_PLAY is the lesser.
	int csvid = player != -1 ? get_id(player, card) : cards_data[card].id;
	int half_played = player != -1 ? get_card_instance(player, card)->info_slot : 0;
	return split_cards_cmc_table_by_id(csvid, half_played);
}

int get_cmc_by_id(int id)
{

  if (has_no_mana_cost(-1, get_internal_card_id_from_csv_id(id)))
  {
    return 0;
  }

  card_ptr_t *c = cards_ptr[id];

  int result = c->req.req_black + c->req.req_blue + c->req.req_green + c->req.req_red + c->req.req_white;
  int cless = c->req.req_colorless;

  if (cless == 40)
  {
    cless = 0;
  }
  result += cless;
  int result_split_card = split_cards_cmc_table_by_id(id, 0);
  return result_split_card >= 0 ? result_split_card : result;
}

int paying_mana(void){
	return !unknown62BCEC || needed_mana_colors;
}

// A permanent legacy effect that does nothing.
int empty(int player, int card, event_t event){
	/* (While this sometimes has an address assigned in ManalinkEh.asm, it's not referenced by ct_all.csv or the exe; it's used as a placeholder when an address
	 * is freed.) */
	return 0;
}

// A legacy effect that does nothing except get removed at end of turn.
int empty_until_eot(int player, int card, event_t event)
{
  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

// Just like reanimate_permanent(), but attach an effect with function effect_fn to the animated permanent before putting it into play.

// Just like reanimate_permanent_with_effect(), but also add a counter to it before putting it into play.

// Just like reanimate_permanent(), but call an artibtrary function on {player,card} just before it's put on the battlefield.
// (source_of_effect_player, source_of_effect_card, player_who_control_reanimated_permanent, reanimated_card)

/* The unused eight-argument predecessor was retired; native mass reanimation uses the five-argument recovered worker. */

// did this card just go to a graveyard from play?
int graveyard_from_play(int player, int card, event_t event)
{
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (affect_me(player, card) && in_play(player, card))
    {
      card_instance_t *instance = get_card_instance(player, card);
      if (instance->kill_code != KILL_EXILE && instance->kill_code != 0)
      {
        instance = get_card_instance(player, card);
        return !(instance->ext->special_flags2 & SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE);
      }
    }
  }
  return 0;
}

// Should be called only before entering or as enters the battlefield

// Should be called if changing types after already being in play.

void cloning_card(int player, int card, event_t event)
{
  if (event == EVENT_CHANGE_TYPE && affect_me(player, card)
	  && !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->targets[12].card != -1)
		event_result = instance->targets[12].card;
	}
}

// t_player and t_card only need to be initialized during EVENT_RESOLVE_SPELL.
void enters_the_battlefield_as_copy_of(int player, int card, event_t event, int t_player, int t_card)
{
  cloning_card(player, card, event);

  if (event == EVENT_RESOLVE_SPELL && t_card >= 0)
	{
	  cloning(player, card, t_player, t_card);

	  card_instance_t* instance = get_card_instance(player, card);

	  instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE;
	  get_abilities(player, card, EVENT_CHANGE_TYPE, -1);

	  call_card_function_i(instance, player, card, EVENT_RESOLVE_SPELL);
	  dispatch_event(player, card, EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE);
	}
}

// As enters_the_battlefield_as_copy_of(), but prompts for a target.  Returns nonzero if copies.  td only needs to be initialized during EVENT_RESOLVE_SPELL.

// Common case of enters_the_battlefield_as_copy_of_any().

// Shapeshifts a single permanent.

// Shapeshifts all cards matching test and controlled by t_player (or ANYBODY).

void cannot_be_countered(int player, int card, event_t event)
{
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
    get_card_instance(player, card)->token_status |= STATUS_CANNOT_BE_COUNTERED;
}

int get_cost_for_propaganda_like_effect(int player, int card)
{
  int csvid = get_id(player, card);
  int tax = 0, annex_count = 0;
  switch (csvid)
  {
  case CARD_ID_SPHERE_OF_SAFETY:
    tax = count_subtype(player, TYPE_ENCHANTMENT, -1);
    break;

  case CARD_ID_PROPAGANDA:
  case CARD_ID_ELEPHANT_GRASS:
  case CARD_ID_GHOSTLY_PRISON:
  case CARD_ID_WINDBORN_MUSE:
  case CARD_ID_KOSKUN_FALLS:
    tax = 2;
    break;

  case CARD_ID_BAIRD_STEWARD_OF_ARGIVE:
    tax = 1;
    break;

  case CARD_ID_ARCHANGEL_OF_TITHES:
  {
    if (!is_tapped(player, card))
    {
      tax = 1;
    }
    break;
  }

  case CARD_ID_COLLECTIVE_RESTRAINT:
    tax = count_domain(player, card);
    break;

  case CARD_ID_NORNS_ANNEX:
    annex_count = 1;
    break;

  case CARD_ID_WAR_TAX:
  {
    card_instance_t *this = get_card_instance(player, card);
    if (this->targets[1].player > 0)
    {
      tax = this->targets[1].player;
    }
    break;
  }
  default:
    break;
  }

  int result = 0;
  SET_BYTE0(result) += tax;
  SET_BYTE1(result) += annex_count;
  return result;
}

void tax_attack(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_PAY_TO_ATTACK && current_phase == PHASE_DECLARE_ATTACKERS && current_turn == 1-player &&
		forbid_attack == 0 && ! is_humiliated(player, card, AL_STATIC)
	  ){
		if( affect_me( trigger_cause_controller, trigger_cause ) && reason_for_trigger_controller == affected_card_controller ){
			int result = get_cost_for_propaganda_like_effect(player, card);
			int tax = BYTE0(result);
			int annex_count = BYTE1(result);

			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if( event == EVENT_RESOLVE_TRIGGER){
					if( annex_count == 0 ){
						if( has_mana(1-player, COLOR_ANY, tax) ){
							int choice = do_dialog(1-player, player, card, -1, -1, " Pay to attack\n Don't Pay\n", 0);
							if( choice == 0 ){
								charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1-player, COLOR_COLORLESS, tax);
								if( spell_fizzled != 1 ){
									return;
								}
							}
						}
					}
					else{
						if( has_phyrexian_mana(1-player, MANACOST_XW(tax, annex_count), 0)){
							int choice = do_dialog(1-player, player, card, -1, -1, " Pay to attack\n Don't Pay\n", 0);
							if( choice == 0 ){
								if( charge_phyrexian_mana(1-player, card, event, MANACOST_XW(tax, annex_count), 0) ){
									return;
								}
							}
						}
					}
					forbid_attack = 1;
			}
		}
	}
}

/* This doesn't actually skip the step, it just reduces number of
   draws by 99.  Should be good enough 99& of the time. */
int skip_your_draw_step(int player, event_t event){
	if(event == EVENT_DRAW_PHASE && player==current_turn){
		event_result-=99;
	}
	return 1;
}

// fmt should have exactly one %s in it to receive the color text.  Returns a pointer to a static buffer.

// As get_sleighted_color_text(), but fmt should have exactly two %s in it.

/* fmt should have exactly one %s, %a, or %l in it to receive the land text for each subtype provided as a variable argument.  (%a will insert a "a " or "an "
 * before the land text as approriate; %l will make unhacked land text all lowercase, appropriate for e.g. a landwalk ability rather than a type name.)  Returns
 * a pointer to a static buffer.  Each variable argument can be a SUBTYPE_*, HARDCODED_SUBTYPE_*, or COLOR_*. */

// Just like get_type(), but the iid is already known (and possibly may not be easily determinable from just player/card, for instance if it's left play)

static int actually_has_a_card_in_hand(int player){
	int c;
	for (c = 0; c < active_cards_count[player]; ++c){
		if (in_hand(player, c)){
			return 1;
		}
	}
	return 0;
}

int pick_card_from_graveyard(int player, int target_graveyard, const char * message){

	return show_deck( player, get_grave(target_graveyard), 500, message, 0, gs_done_008b40e0 );
}

int changeling_switcher(int player, int card, event_t event){

   if( event == EVENT_ABILITIES && affect_me(player, card) ){
	   event_result |= KEYWORD_PROT_INTERRUPTS;
   }

   return 0;
}

int generic_creature_with_activated_tapsubtype_ability(int player, int card, event_t event, subtype_t subtype, int howmany, int illegalstate1, int illegalstate2){
  card_instance_t *instance = get_card_instance(player, card);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.required_subtype = subtype;
  td.illegal_abilities = 0;
  td.illegal_state = illegalstate1 | illegalstate2;
  td.allow_cancel = 0;

  target_definition_t td1;
  default_target_definition(player, card, &td1, TYPE_CREATURE);
  td1.allowed_controller = player;
  td1.preferred_controller = player;
  td1.illegal_abilities = 0;
  td1.allow_cancel = 0;
  td1.illegal_state = illegalstate1 | illegalstate2;
  td1.required_abilities = KEYWORD_PROT_INTERRUPTS;

  if( event == EVENT_CAN_ACTIVATE &&
	  (target_available(player, card, &td) + target_available(player, card, &td1) >= howmany)   ){
	return 1;
  }

  if( event == EVENT_ACTIVATE){
	 int stop = 0;
	 int test = 0;
	 while( stop != 1 ){
		   if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			  tap_card(instance->targets[0].player, instance->targets[0].card);
			  test++;
		   }
		   else if( can_target(&td1) && pick_target(&td1, "TARGET_CREATURE") ){
				   tap_card(instance->targets[0].player, instance->targets[0].card);
				   test++;

		   }

		   if( test == howmany ){
			  stop = 1;
		   }

	}

  }

  return 0;
 }

int specific_spell_played(int player, int card, event_t event, int t_player, int trigger_mode, int type, int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4, int cc, int flag5){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		!is_humiliated(player, card, AL_TRIG) && (t_player == ANYBODY || trigger_cause_controller == t_player) &&
		!is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) && ! is_token(trigger_cause_controller, trigger_cause) &&
		make_test_in_play(trigger_cause_controller, trigger_cause, -1, type, flag1, subtype, flag2, color, flag3, id, flag4, cc, flag5)
	  ){
		if(event == EVENT_TRIGGER){
			if (trigger_mode == RESOLVE_TRIGGER_DUH){
				trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
			}
			event_result |= trigger_mode;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			card_instance_t* instance = get_card_instance(player, card);
			instance->targets[1].player = trigger_cause_controller;
			instance->targets[1].card = trigger_cause;
			return 1;
		}
	}
	return 0;
}

int new_specific_spell_played(int player, int card, event_t event, int t_player, int trigger_mode, test_definition_t* this_test/*optional*/){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		!is_humiliated(player, card, AL_TRIG) && (t_player == ANYBODY || trigger_cause_controller == t_player) &&
		!is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) && ! is_token(trigger_cause_controller, trigger_cause) &&
		(!this_test || new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, this_test))
	  ){
		if(event == EVENT_TRIGGER){
			if (trigger_mode == RESOLVE_TRIGGER_DUH){
				trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
			}
			event_result |= trigger_mode;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			card_instance_t* instance = get_card_instance(player, card);
			instance->targets[1].player = trigger_cause_controller;
			instance->targets[1].card = trigger_cause;
			return 1;
		}
	}
	return 0;
}

int metamorphosis(int player, int card, type_t selected_type, kill_t kill_code){

	kill_card(player, card, kill_code);

	test_definition_t this_test;
	default_test_definition(&this_test, selected_type);
	return reveal_card_until_you_reveal_test(player, player, &this_test, RCUYRT_PUT_TEST_INTO_PLAY | RCUYRT_SHUFFLE_REST | RCUYRT_RETURN_ADDED_CARD);
}

int impulse_effect(int player, int amount, int whattodo)
{
  //  whattodo = 0 --> Impulse-like effect (add the selected card to hand and put the rest on
  //  bottom) whattodo = 1 --> Impulse-like effect, but put the selected card on the top of library
  //  whattodo = 3 --> add the selected card to hand and randomize the rest on bottom.
  //  whattodo = 2 --> add the selected card to hand and leave the rest on the top.

  int *deck = deck_ptr[player];
  int card_added = -1;
  if (count_deck(player) < amount)
  {
    amount = count_deck(player);
  }
  if (amount <= 0)
  {
    return 0;
  }
  if (deck[0] != -1)
  {
    char msg[100] = "Select a card.";
    test_definition_t this_test;
    new_default_test_definition(&this_test, TYPE_ANY, msg);
    this_test.create_minideck = amount;
    this_test.no_shuffle = 1;

    int selected =
        new_select_a_card(player, player, TUTOR_FROM_DECK, 1, AI_MAX_VALUE, -1, &this_test);
    card_added = add_card_to_hand(player, deck[selected]);
    remove_card_from_deck(player, selected);
    amount--;

    if (amount > 0 && whattodo != 2)
    {
      if (whattodo == 3)
        put_top_x_on_bottom_in_random_order(player, amount);
      else
        put_top_x_on_bottom(player, player, amount);
    }

    if (whattodo == 1)
    {
      put_on_top_of_deck(player, card_added);
    }
  }

  return card_added;
}

int count_free_mana(int player, int type){

	int count = active_cards_count[player]-1;
	int free_mana = 0;

	while( count > -1 ){
		  if( in_play(player, count) ){
			  if( get_id(player, count) == CARD_ID_SEMBLANCE_ANVIL ){
				  card_instance_t *leg = get_card_instance(player, count);
				  if( leg->info_slot & type ){
					  free_mana+=2;
				  }
			  }
			  if( get_id(player, count) == CARD_ID_HELM_OF_AWAKENING ){
				  free_mana+=1;
			  }
		  }
		  count--;
	}

  return free_mana;
}

int count_attackers(int player)
{
  int c, result = 0;
  for (c = 0; c < active_cards_count[player]; ++c)
	if (in_play_and_attacking(player, c))
	  ++result;

  return result;
}

int count_attackers_non_planeswalker(int player)
{
  int c, result = 0;
  for (c = 0; c < active_cards_count[player]; ++c)
	if (in_play_and_attacking(player, c) && !check_special_flags(player, c, SF_ATTACKING_PWALKER))
	  ++result;

  return result;
}

/* If player == whose_deck or player is an AI (actual AI, or AI speculating for a human player), then returns a color_t of the most common color in whose_deck's
 * library.  If player is a human and whose_deck isn't his, then always returns COLOR_BLACK.  If player is -1, then returns a color_test_t of the two most
 * common colors in whose_deck's library. */

int select_a_protection(int player){

  int ai_choice = 0;

  if( player == AI ){
	 ai_choice = get_deck_color(player, 1-player);
  }

  int color = choose_a_color(player, ai_choice);
  int keyword = KEYWORD_PROT_BLACK;

  if( color == 2 ){
	 keyword = KEYWORD_PROT_BLUE;
  }
  else if( color == 3 ){
		  keyword = KEYWORD_PROT_GREEN;
  }
  else if( color == 4 ){
		  keyword = KEYWORD_PROT_RED;
  }
  else if( color == 5 ){
		  keyword = KEYWORD_PROT_WHITE;
  }

  return keyword;
}

int sacrifice_at_end_of_combat(int player, int card, event_t event)
{
	if (end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! check_state(instance->damage_target_player, instance->damage_target_card, STATE_OUBLIETTED) ){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
		}
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

int remove_at_eot(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( eot_trigger(player, card, event) ){
		int p = instance->targets[0].player;
		int c = instance->targets[0].card;
		if( ! check_state(p, c, STATE_OUBLIETTED) ){
			kill_card(p, c, KILL_EXILE);
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	return 0;
}

int total_playable_lands(int player)
{
  int total = 1;
  int pass;
  for (pass = 0; pass < 2; ++pass)
  {
    int p = pass == 0 ? current_turn : 1 - current_turn;
    int c;
    for (c = active_cards_count[p] - 1; c >= 0; --c)
    {
      card_instance_t *instance = in_play(p, c);
      if (instance == NULL ||
          (!is_what(p, c, TYPE_EFFECT) && is_humiliated(p, c, AL_STATIC)))
        continue;

      switch (get_id(p, c))
      {
        case CARD_ID_STORM_CAULDRON:
        case CARD_ID_RITES_OF_FLOURISHING:
        case CARD_ID_GHIRAPUR_ORRERY:
          ++total;
          break;

        case CARD_ID_AZUSA_LOST_BUT_SEEKING:
          total += (p == player) * 2;
          break;

        case CARD_ID_EXPLORATION:
        case CARD_ID_ORACLE_OF_MUL_DAYA:
        case CARD_ID_MINA_AND_DENN_WILDBORN:
        case CARD_ID_THE_GITROG_MONSTER:
        case CARD_ID_WAYWARD_SWORDTOOTH:
        case CARD_ID_DRYAD_OF_THE_ILYSIAN_GROVE:
        case CARD_ID_AESI_TYRANT_OF_GYRE_STRAIT:
          total += p == player;
          break;

        case 903: // Custom legacy effects use this shared card ID.
          instance = get_card_instance(p, c);
          if (instance->internal_card_id != LEGACY_EFFECT_CUSTOM ||
              instance->info_slot_fn != check_playable_lands_legacy)
            break;

          switch (instance->display_pic_csv_id)
          {
            case CARD_ID_JOURNEY_OF_DISCOVERY:
              total += (p == player) * 2;
              break;
            case CARD_ID_SUMMER_BLOOM:
              total += (p == player) * 3;
              break;
            case CARD_ID_EXPLORE:
            case CARD_ID_URBAN_EVOLUTION:
            case CARD_ID_KIORA_THE_CRASHING_WAVE:
            case CARD_ID_THIS_WORLD_BELONGS_TO_ME:
            case CARD_ID_SCALE_THE_HEIGHTS:
              total += p == player;
              break;
          }
          break;
      }
    }
  }

  if (total == 1)
    player_bits[player] &= ~PB_COUNT_TOTAL_PLAYABLE_LANDS;
  return total;
}

void check_playable_lands(int player)
{
	player_bits[player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;
}

int check_playable_lands_legacy(int player, int card, event_t event){

	player_bits[player] |= PB_COUNT_TOTAL_PLAYABLE_LANDS;

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int generic_painland(int player, int card, event_t event)
{
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (IS_AI(player) && life[player] <= 1 &&
        !(get_card_instance(player, card)->mana_color & COLOR_TEST_COLORLESS))
      return 0;
    return mana_producer(player, card, event);
  }
  if (event == EVENT_ACTIVATE)
  {
    color_test_t colors = get_card_instance(player, card)->mana_color | mana_colors_added[player];
    if (IS_AI(player) && life[player] <= 1 && (colors & COLOR_TEST_COLORLESS))
    {
      produce_mana_tapped_all_one_color(player, card,
                                        COLOR_TEST_COLORLESS | mana_colors_added[player], 1);
    }
    else
    {
      produce_mana_tapped_all_one_color(player, card, colors, 1);
      if (!check_special_flags2(player, card, SF2_CONTAMINATION))
      {
        if (chosen_colors & ~(COLOR_TEST_COLORLESS | mana_colors_added[player]))
          damage_player(player, 1, player, card);
      }
      else
      {
        int ok_colors = 0;
        for (int c = 1; c < 7; ++c)
        {
          // Native code tests the color index here and may deal damage more than once.
          if ((COLOR_TEST_COLORLESS | mana_colors_added[player]) & c)
          {
            ok_colors |= mana_color_override[player][c] == COLOR_ANY
                             ? COLOR_TEST_ANY_COLORED
                             : 1 << mana_color_override[player][c];
            if (chosen_colors & ~ok_colors)
              damage_player(player, 1, player, card);
          }
        }
      }
    }
  }
  else if (event == EVENT_COUNT_MANA && affect_me(player, card))
  {
    if (CAN_TAP_FOR_MANA(player, card))
    {
      int contaminated = check_special_flags2(player, card, SF2_CONTAMINATION);
      card_instance_t *instance = get_card_instance(player, card);
      int colors = instance->mana_color | mana_colors_added[player];
      if (!contaminated)
      {
        if (IS_AI(player) && life[player] <= 1)
        {
          if (colors & COLOR_TEST_COLORLESS)
            declare_mana_available(player, COLOR_COLORLESS, 1);
        }
        else
          declare_mana_available_hex(player, colors, 1);
      }
      else if (IS_AI(player) && life[player] <= 1)
      {
        if (colors & COLOR_TEST_COLORLESS)
        {
          int quantity = mana_quantity_override[player][0];
          declare_mana_available(player, mana_color_override[player][0],
                                 quantity < 0 ? 1 : quantity);
        }
      }
      else
      {
        int quantity = 1;
        for (int c = 0; c < 7; ++c)
        {
          int mask = 1 << c;
          int replacement = mana_color_override[player][c];
          if ((colors & mask) && replacement)
          {
            colors = (colors & ~mask) |
                     (replacement == COLOR_ANY ? COLOR_TEST_ANY_COLORED : 1 << replacement);
            if (mana_quantity_override[player][c] >= 0)
              quantity = mana_quantity_override[player][c];
          }
        }
        declare_mana_available_hex(player, colors, quantity);
      }
    }
  }
  else
    return mana_producer(player, card, event);
  return 0;
}

int effect_defender_can_attack_until_eot(int player, int card, event_t event){
	if (event == EVENT_ABILITIES){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->damage_target_player >= 0){
			get_card_instance(instance->damage_target_player, instance->damage_target_card)->token_status |= STATUS_WALL_CAN_ATTACK;
		}
	}
	if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

// Also suitable as part of an enchantment
int effect_cannot_attack(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES && in_play(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  cannot_attack(instance->damage_target_player, instance->damage_target_card, event);
	}

	return 0;
}

int effect_cannot_attack_until_eot(int player, int card, event_t event)
{
  effect_cannot_attack(player, card, event);

  if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

void enters_the_battlefield_with_counters(int player, int card, event_t event, counter_t type,
                                          int number)
{
  card_instance_t *instance;
  if (((0 < number) &&
       (((event == EVENT_CAST_SPELL || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
         (card == affected_card)))) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    if ((instance->ext->special_flags2 & SF2_FACE_DOWN_DUE_TO_MANIFEST) == 0)
    {
      hack_silent_counters = hack_silent_counters + 1;
      add_counters(player, card, player, card, type, number);
      hack_silent_counters = hack_silent_counters + -1;
    }
  }
  return;
}

int count_nonbasic_lands(int player){

	int result = 0;
	int i;
	for(i=0; i<2; i++){
		if( player == 2 || i == player ){
			int count = 0;
			while( count < active_cards_count[i] ){
				   if( in_play(i, count) && is_what(i, count, TYPE_LAND) && ! is_basic_land(i, count) ){
					   result++;
				   }
				   count++;
			}
		}
	}

	return result;
}

int control_nonbasic_land(int player){

	int count = 0;
	while( count < active_cards_count[player] ){
		   if( in_play(player, count) && is_what(player, count, TYPE_LAND) && ! is_basic_land(player, count) ){
			   return 1;
		   }
		   count++;
	}

	return 0;
}

int control_basic_land(int player){
	if( player != 2 ){
		int count = 0;
		while( count < active_cards_count[player] ){
				if( in_play(player, count) && is_what(player, count, TYPE_LAND) && is_basic_land(player, count) ){
					return 1;
				}
				count++;
		}
		return 0;
	}
	else{
		int i;
		for(i=0; i<2; i++){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_LAND) && is_basic_land(i, count) ){
						return 1;
					}
					count++;
			}
		}
		return 0;
	}
	return 0;
}

int check_base_value_for_ai(int card, int conf_base_value, int mode){
	// Mode = 1 -> Select the greatest
	// Mode = -1 -> Select the lowest

	if( card < 0 ){
		return 0;
	}

	int base_value = get_base_value(-1, cards_data[card].id);
	if( mode == 1 && base_value > conf_base_value ){
		return 1;
	}
	if( mode == -1 && base_value < conf_base_value ){
		return 1;
	}
	return 0;
}

int player_reveals_x_and_discard(int t_player, int who_choses, int howmany, int effect, int ai_selection_mode, int type, int flag1,
					int subtype, int flag2, int color, int flag3, int id, int flag4, int cmc, int flag5
  ){
	// The function will return the CSV ID of the discarded card, unless otherwise noted.
	// effect = 1 -> Discard
	// effect = 2 -> Exile
	// effect = 4 -> Used for Karn Liberated (AI will chosse the less AI Base value among non-permanents).
	// effect = 8 -> Select a card to play from opponent's hand. Will return the CARD value, not the CSV ID.
	// effect = 16 -> Select a creature or a planeswalker (Despise)

	test_definition_t this_test;
	default_test_definition(&this_test, type);
	this_test.type_flag = flag1;
	this_test.subtype = subtype;
	this_test.subtype_flag = flag2;
	this_test.color = color;
	this_test.color_flag = flag3;
	this_test.id = id;
	this_test.id_flag = flag4;
	this_test.cmc = cmc;
	this_test.cmc_flag = flag5;

	test_definition_t this_test2;
	default_test_definition(&this_test2, TYPE_ANY);

	int count = 0;
	int initial_index = 0;
	int hand_index = 0;
	int initial_hand[50];
	int ai_hand[50];

	if( howmany > hand_count[t_player] ){
		howmany = hand_count[t_player];
	}
	while( count < active_cards_count[t_player] ){
		   if( in_hand(t_player, count) ){
			   card_instance_t *crd = get_card_instance(t_player, count);
			   initial_hand[initial_index] = crd->internal_card_id;
			   initial_index++;
		   }
		   count++;
	}

	count = 0;
	int selected = -1;
	while( count < howmany ){
			selected = -1;
			selected = select_card_from_zone(t_player, t_player, initial_hand, initial_index, 1, AI_MIN_VALUE, -1, &this_test2);
			if( selected != -1 ){
				ai_hand[hand_index] = initial_hand[selected];
				hand_index++;
				int i;
				for(i=selected; i<(initial_index-1); i++){
					initial_hand[i] = initial_hand[i+1];
				}
				initial_index--;
				count++;
			}
	}

	int result = 0;
	selected = select_card_from_zone(who_choses, t_player, ai_hand, hand_index, 1, AI_MAX_VALUE, -1, &this_test);

	if( selected == -1 ){
		return -1;
	}

	count = active_cards_count[t_player]-1;
	while( count > -1 ){
		   if( in_hand(t_player, count) ){
			   card_instance_t *crd = get_card_instance(t_player, count);
			   if( crd->internal_card_id == ai_hand[selected] ){
				   result = get_id(t_player, count);
				   if( effect & 1 ){
					   new_discard_card(t_player, count, who_choses, 0);
				   }
				   if( effect & 2 ){
						rfg_card_in_hand(t_player, count);
				   }
				   if( effect & 32 ){
						put_on_top_of_deck(t_player, count);
				   }
				   if( effect & 4 || effect & 64 ){
					   result = count;
				   }
				   break;
			   }
		   }
		   count--;
	}

	return result;
}

void default_ec_definition(int t_player, int who_choses, ec_definition_t *this_definition){
	this_definition->target_player = t_player;
	this_definition->who_choses = who_choses;
	this_definition->effect = EC_DISCARD;
	this_definition->qty = 1;
	this_definition->cards_to_reveal = 0;
	this_definition->ai_selection_mode = AI_MAX_VALUE;
	this_definition->special_selection_function = NULL;
}

int check_battlefield_for_id(int player, int id){

	if( id < 0 ){
		return 0;
	}

	int i;
	for(i=0; i<2; i++){
		if( player == i || player == 2 ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && get_id(i, count) == id && ! is_humiliated(i, count, AL_STATIC) ){
						// this should be right 99% of the cases, since an "humiliated" card lose all abilities
						return 1;
					}
					count++;
			}
		}
	}

	return 0;
}

int locate_id(int player, int id){

	int i;
	for(i=0; i<2; i++){
		if( player == 2 || i == player ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && get_id(i, count) == id ){
						return count;
					}
					count++;
			}
		}
	}
	return -1;
}

int check_battlefield_for_targettable_id(int player, int card, int t_player, int id, int mode){

	if( id == -1 ){
		return 0;
	}

	if( t_player < 2 ){
		int count = 0;
		while( count < active_cards_count[t_player] ){
			   if( in_play(t_player, count) && get_id(t_player, count) == id ){
					if( ! is_protected_from(t_player, count, player, card, IPF_TARGET) ){
						if( mode == 0 ){
							return 1;
						}
						if( mode == 1 ){
							return count;
						}
					}
			   }
			   count++;
		}
	}

	else if( player == 2 ){
			 int i;
			 for(i=0; i<2; i++){
				 int count = 0;
				 while( count < active_cards_count[i] ){
						if( in_play(i, count) && get_id(i, count) == id ){
							if( ! is_protected_from(i, count, player, card, IPF_TARGET) ){
								if( mode == 0 ){
									return 1;
								}
								if( mode == 1 ){
									return count;
								}
							}
						}
						count++;
				 }
			 }
	}

	return 0;
}

void make_all_untargettable(void){

	 int i;
	 for(i=0; i<2; i++){
		 int count = 0;
		 while( count < active_cards_count[i] ){
				if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
					card_instance_t *this = get_card_instance(i, count);
					this->state |= STATE_CANNOT_TARGET;
				}
				count++;
		 }
	 }
}

void remove_untargettable_from_all(void){

	 int i;
	 for(i=0; i<2; i++){
		 int count = 0;
		 while( count < active_cards_count[i] ){
				if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) ){
					card_instance_t *this = get_card_instance(i, count);
					if( this->state & STATE_CANNOT_TARGET ){
						this->state  &= ~STATE_CANNOT_TARGET;
					}
				}
				count++;
		 }
	 }
}

void nice_creature_to_sacrifice(int player, int card)
{
  unsigned int special_keywords;
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  special_keywords = instance->ext->sp_keywords2;
  if ((special_keywords & 8) == 0)
  {
    instance->ext->sp_keywords2 = special_keywords | 8;
  }
  return;
}

void reveal_target_player_hand(int player ){

	 if (duel_host_is_headless()) return;int cards_array[60];
	 int revealed = 0;
	 int i;
	 for(i=0;i<active_cards_count[player]; i++){
		 if( in_hand(player, i) ){
			 card_instance_t *rev_card = get_card_instance(player, i);
			 cards_array[revealed] = rev_card->internal_card_id;
			 revealed++;
		 }
	 }

	 if( revealed > 0){
		 show_deck( 1-player, cards_array, revealed, "Cards revealed", 0, gs_done_008b40e0 );
	 }
}

void symbiotic_creature(int player, int card, event_t event, int token_type, int amount){

	nice_creature_to_sacrifice(player, card);

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		token_generation_t token;
		default_token_definition(player, card, token_type, &token);
		token.qty = amount;
		generate_token(&token);
	}
}

// AI leaves tapped if in_play(targets[1])

// Call as if (choosing_to_untap(player, card, event)) { return should_ai_untap; }.  AI will leave tapped if that return value is 0, otherwise will untap.

int pick_targets_for_multidamage(int player, int card, int max_targets, int damages, target_definition_t *td, const char *prompt){

	card_instance_t *instance = get_card_instance(player, card);

	load_text(0, prompt);
	int trgts = 0;
	if( player != AI ){
		int players[2] = {0, 0};
		while( trgts < max_targets && can_target(td) ){
				if( select_target(player, card, td, text_lines[0], &(instance->targets[trgts])) ){
					if( instance->targets[trgts].card == -1 ){
						if( players[instance->targets[trgts].player] != 1 ){
							players[instance->targets[trgts].player] = 1;
							trgts++;
						}
						else{
							if( td->allow_cancel != 0 ){
								break;
							}
						}
					}
					else{
						state_untargettable(instance->targets[trgts].player, instance->targets[trgts].card, 1);
						trgts++;
					}
				}
				else{
					if( td->allow_cancel != 0 ){
						break;
					}
				}
		}
		int i;
		for(i=0; i<trgts; i++){
			if( instance->targets[i].card != -1 ){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
	}
	else{
		if( td->zone == TARGET_ZONE_CREATURE_OR_PLAYER ){
			instance->targets[0].player = 1-player;
			instance->targets[0].card = -1;
			if( would_validate_target(player, card, td, 0) ){
				trgts++;
			}
		}
		int count = 0;
		while( count < active_cards_count[1-player] && trgts < max_targets ){
				if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
					if( ! is_protected_from(1-player, count, player, card, IPF_TARGET) &&
						get_toughness(1-player, count) <= damages
					  ){
						instance->targets[trgts].player = 1-player;
						instance->targets[trgts].card = count;
						trgts++;
					}
				}
				count++;
		}

		if( trgts < max_targets && td->allow_cancel == 0 ){
			count = 0;
			while( count < active_cards_count[1-player] && trgts < max_targets ){
					if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
						if( ! is_protected_from(1-player, count, player, card, IPF_TARGET) &&
							get_toughness(1-player, count) > damages
						  ){
							instance->targets[trgts].player = 1-player;
							instance->targets[trgts].card = count;
							trgts++;
						}
					}
					count++;
			}
		}

		if( trgts < max_targets && td->allow_cancel == 0 ){
			count = 0;
			while( count < active_cards_count[player] && trgts < max_targets ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
						if( ! is_protected_from(player, count, player, card, IPF_TARGET) &&
							get_toughness(player, count) > damages
						  ){
							instance->targets[trgts+1].player = player;
							instance->targets[trgts+1].card = count;
							trgts++;
						}
					}
					count++;
			}
		}

		if( trgts < max_targets && td->allow_cancel == 0 ){
			count = 0;
			while( count < active_cards_count[player] && trgts < max_targets ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
						if( ! is_protected_from(player, count, player, card, IPF_TARGET) ){
							instance->targets[trgts+1].player = player;
							instance->targets[trgts+1].card = count;
							trgts++;
						}
					}
					count++;
			}
		}
	}
	return trgts;
}

void multidamage(int player, int card, int num_targets, int damages, target_definition_t *td){
	// requires the definition of targets via the "pick_targets_for_multidamage" function

	if( num_targets < 1 ){
		return;
	}

	card_instance_t *instance = get_card_instance(player, card);

	int i;
	for(i=0; i<num_targets; i++){
		if( validate_target(player, card, td, i) ){
			damage_creature(instance->targets[i].player, instance->targets[i].card, damages, player, card);
		}
	}
}

void nobody_can_attack(int player, int card, event_t event, int t_player)
{
  if (event == EVENT_ATTACK_LEGALITY
	  && (affected_card_controller == t_player || t_player == ANYBODY))
	event_result = 1;
}

static int effect_nobody_can_attack_until_eot(int player, int card, event_t event)
{
  /* Local storage:
   * damage_target_player/damage_target_card: this card can always still attack.
   * targets[1].player: the player who can't attack.
   * targets[1].card: &1: ends at end of turn.
   *                  &2: ends after target player's next declare attackers step or second main
   * phase. &4: ends after target player's next declare attackers step; during discard or cleanup,
   * changes to &2.  Suitable for assigning during combat. &8: ends at start of controller's next
   * turn &16: ends during cleanup after a begin-turn marker has been set targets[2].player: number
   * of combat phases to skip (default: 1 or just leave it unset)
   */

  event_flags |= EA_DECLARE_ATTACK;

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_ATTACK_LEGALITY &&
      (affected_card_controller == instance->targets[1].player ||
       instance->targets[1].player == ANYBODY) &&
      !affect_me(instance->damage_target_player, instance->damage_target_card))
    event_result = 1;

  if (trigger_condition == TRIGGER_LEAVE_PLAY &&
      trigger_cause_controller == instance->damage_target_player &&
      trigger_cause == instance->damage_target_card)
    instance->damage_target_player = instance->damage_target_card = -1; // detach (immediately)

  int end_effect = 0;
  if (event == EVENT_CLEANUP && (instance->targets[1].card & 1))
    end_effect = 1;

  if (event == EVENT_PHASE_CHANGED && (instance->targets[1].card & 2) &&
      current_turn == instance->targets[1].player &&
      (current_phase == PHASE_MAIN2 || current_phase == PHASE_BEFORE_BLOCKING))
    end_effect = 1;

  if (event == EVENT_PHASE_CHANGED && (instance->targets[1].card & 4) &&
      current_turn == instance->targets[1].player && current_phase == PHASE_BEFORE_BLOCKING)
    end_effect = 1;

  if (event == EVENT_BEGIN_TURN && current_turn == player && (instance->targets[1].card & 8))
    end_effect = 1;

  if (event == EVENT_BEGIN_TURN)
    get_card_instance(player, card)->targets[2].card = 66;
  if (event == EVENT_CLEANUP && get_card_instance(player, card)->targets[2].card == 66 && (instance->targets[1].card & 16))
    kill_card(player, card, KILL_EXILE);

  if (end_effect)
  {
    if (instance->targets[2].player > 0)
    {
      instance->targets[2].player--;
    }
    if (instance->targets[2].player <= 0)
    {
      instance->targets[1].card = 0;
      kill_card(player, card, KILL_EXILE);
    }
  }

  if ((instance->targets[1].card & 4) && current_phase > PHASE_MAIN2 &&
      current_turn == instance->targets[1].player)
  {
    instance->targets[1].card &= ~4;
    instance->targets[1].card |= 2;
  }

  return 0;
}

/* Only except_player/except_card, and creatures not controlled by t_player, can attack this turn.  t_player can be 2 for anybody, and except_player/except_card
 * can be -1 to not provide an exception. */
void nobody_can_attack_until_eot(int player, int card, int t_player, int except_player, int except_card)
{
  int leg = (except_card < 0 ? create_legacy_effect(player, card, effect_nobody_can_attack_until_eot)
			 : create_targetted_legacy_effect(player, card, effect_nobody_can_attack_until_eot, except_player, except_card));

  if (leg != -1)
	{
	  card_instance_t* legacy = get_card_instance(player, leg);
	  legacy->targets[1].player = t_player;
	  legacy->targets[1].card = 1;
	}
}

int target_player_skips_his_next_attack_step(int player, int card, int t_player, int this_turn_only)
{
	int leg = create_legacy_effect(player, card, effect_nobody_can_attack_until_eot);

	if (leg != -1)
	{
	  card_instance_t* legacy = get_card_instance(player, leg);
	  legacy->targets[1].player = t_player;
	  legacy->targets[1].card = (current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2) ? 4 : 2;
	  if (this_turn_only)
		legacy->targets[1].card |= 1;
	}
	return leg;
}

void nobody_can_attack_until_your_next_turn(int player, int card, int t_player)
{
  int leg = create_legacy_effect(player, card, effect_nobody_can_attack_until_eot);

  if (leg != -1)
	{
	  // Not sure whether it's better or worse to always set this here than to return the legacy and let the caller muck with it.
	  alternate_legacy_text(2, player, leg);

	  card_instance_t* legacy = get_card_instance(player, leg);
	  legacy->targets[1].player = t_player;
	  legacy->targets[1].card = 8;
	}
}

int count_planeswalkers(int player, int mode){

	int result = 0;
	int count = 0;
	while( count < active_cards_count[player] ){
			if( in_play(player, count) && is_planeswalker(player, count) ){
				if( mode == 1 ){
					return 1;
				}
				result++;
			}
			count++;
	}

	return result;
}

void modify_pt_and_abilities(int player, int card, event_t event, int p_mod, int t_mod, int a_mod ){

	if( event == EVENT_POWER && affect_me(player, card) ){
		if( p_mod > 100 ){
			event_result = (p_mod-100);
		}
		else{
			event_result += p_mod;
		}
	}

	if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
		if( t_mod > 100 ){
			event_result = (t_mod-100);
		}
		else{
			event_result += t_mod;
		}
	}

	if( event == EVENT_ABILITIES && affect_me(player, card) ){
		event_result |= a_mod;
	}
}

void real_set_cost_mod_for_activated_abilities(int player, int card, int mode){
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	if( instance->targets[17].card < 0 ){
		instance->targets[17].card = 0;
	}
	for(i=12+((mode-1)*4); i<12+(mode*4); i++){
		if( !( instance->targets[17].card & (1<<i)) ){
			instance->targets[17].card |= (1<<i);
			break;
		}
	}
}

void set_cost_mod_for_activated_abilities(int player, int card, int mode, test_definition_t *this_test){
	// mode = 1 --> -1 colorless mana (min 1)
	// mode = 2 --> -2 colorless mana (min 1)
	// mode = 3 --> +2 colorless mana
	// mode = 4 --> +3 colorless mana
	if( card > -1 ){
		real_set_cost_mod_for_activated_abilities(player, card, mode);
	}
	else{
		int i;
		int test = new_get_test_score(this_test);
		for(i=0; i<2; i++){
			if( i == player || player == 2 ){
				int count = 0;
				while( count < active_cards_count[i] ){
						if( in_play(i, count) && new_make_test_in_play(i, count, test, this_test) ){
							real_set_cost_mod_for_activated_abilities(i, count, mode);
						}
						count++;
				}
			}
		}
	}
}

void real_remove_cost_mod_for_activated_abilities(int player, int card, int mode){
	// mode = 1 --> -1 colorless mana (min 1)
	// mode = 2 --> -2 colorless mana (min 1)
	// mode = 3 --> +2 colorless mana
	// mode = 4 --> +3 colorless mana
	card_instance_t *instance = get_card_instance(player, card);
	int i;
	if( instance->targets[17].card < 0 ){
		instance->targets[17].card = 0;
	}
	for(i=(12+(mode*4))-1; i>(12+((mode-1)*4))-1; i--){
		if( instance->targets[17].card & (1<<i) ){
			instance->targets[17].card &= ~ (1<<i);
			break;
		}
	}
}

void remove_cost_mod_for_activated_abilities(int player, int card, int mode, test_definition_t *this_test){
	// mode = 1 --> -1 colorless mana (min 1)
	// mode = 2 --> -2 colorless mana (min 1)
	// mode = 3 --> +2 colorless mana
	// mode = 4 --> +3 colorless mana
	if( card > -1 ){
		real_remove_cost_mod_for_activated_abilities(player, card, mode);
	}
	else{
		int i;
		int test = new_get_test_score(this_test);
		for(i=0; i<2; i++){
			if( i == player || player == 2 ){
				int count = 0;
				while( count < active_cards_count[i] ){
						if( in_play(i, count) && new_make_test_in_play(i, count, test, this_test) ){
							real_remove_cost_mod_for_activated_abilities(i, count, mode);
						}
						count++;
				}
			}
		}
	}
}

void phantom_effect(int player, int card, event_t event, int requires_counter)
{
  int result;
  card_instance_t *instance;

  if (((((event == EVENT_PREVENT_DAMAGE) &&
         (result = is_humiliated(player, card, AL_TRIG), result == 0)) &&
        (instance = get_card_instance(affected_card_controller, affected_card),
         instance->internal_card_id == damage_card)) &&
       ((instance->damage_target_card == card && (instance->damage_target_player == player)))) &&
      (0 < instance->info_slot))
  {
    if ((requires_counter != 0) &&
        (result = count_counters(player, card, COUNTER_P1_P1), result < 1))
    {
      return;
    }
    instance = get_card_instance(affected_card_controller, affected_card);
    instance->info_slot = 0;
    remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return;
  }
  return;
}

int pick_target_by_type(int player, int card, int t_type, int must_select){

	target_definition_t td;
	default_target_definition(player, card, &td, t_type);
	td.allow_cancel = 1-must_select;

	int result = 0;

	if( t_type == TYPE_CREATURE ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			result = 1;
		}
	}
	else if( t_type == TYPE_LAND ){
			if( pick_target(&td, "TARGET_LAND") ){
				result = 1;
			}
	}
	else if( t_type == TYPE_ENCHANTMENT ){
			if( pick_target(&td, "TARGET_ENCHANTMENT") ){
				result = 1;
			}
	}
	else if( t_type == TYPE_ARTIFACT ){
			if( pick_target(&td, "TARGET_ARTIFACT") ){
				result = 1;
			}
	}
	else{
		if( pick_target(&td, "TARGET_PERMANENT") ){
			result = 1;
		}
	}
	return result;
}

int cip_damage_creature(int player, int card, event_t event, target_definition_t *td1, const char *prompt, int amount){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){

		if( amount == -1 ){
			if( player != AI ){
				amount = choose_a_number(player, "Deal how much damage?", life[player]);
			}
			else{
				amount = life[player]-6;
				if( amount < 0 ){
					amount = 0;
				}
			}
		}

		if( get_id(player, card) == CARD_ID_VOLCANO_HELLION && amount > 0 ){
			int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = player;
			leg->targets[0].card = card;
		}
		load_text(0, prompt);
		card_instance_t *instance = get_card_instance(player, card);
		instance->number_of_targets = 0;
		if (can_target(td1) && select_target(player, card, td1, text_lines[0], &(instance->targets[0]))){
			damage_target0(player, card, amount);

			if (get_id(player, card) == CARD_ID_VOLCANO_HELLION){
				damage_player(player, amount, player, card);
			}
		}
	}

	return 0;
}

int get_base_value( int player, int card ){
	int id = -1;
	if( player == -1 ){
		id = cards_data[card].id;
	}
	else{
		id = get_id(player, card);
	}
	card_ptr_t* c = cards_ptr[ id ];
	/*
	if( c->ai_base_value < 1 ){
		return my_base_value_by_id(id);
	}
	*/
	return c->ai_base_value;
}

// Selects total_damage targets, then deals 1 damage to each for each time it was targeted.  Returns number of different targets selected.
int target_and_divide_damage(int player, int card, target_definition_t* td, const char* prompt, int total_damage)
{
  target_definition_t td_default;
  if (!td)
	{
	  default_target_definition(player, card, &td_default, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td_default.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	  td_default.allow_cancel = 0;
	  td = &td_default;
	}

  if (!can_target(td))
	return 0;

  if (!prompt)
	prompt = "TARGET_ANY";

  int mode = 0;
  if (prompt[0] && prompt[1] >= 'a' && prompt[1] <= 'z')	// at least 2 characters, and 2nd is lowercase
	mode = GS_LITERAL_PROMPT;

  card_instance_t* inst = get_card_instance(player, card);
  inst->number_of_targets = 0;

  int i, failed = 0;;
  for (i = 0; i < total_damage; ++i)
	if (!new_pick_target(td, prompt, i, mode))
	  {
		failed = 1;
		break;
	  }
	else if (inst->targets[i].card >= 0)
	  get_card_instance(inst->targets[i].player, inst->targets[i].card)->state |= STATE_TARGETTED;

  for (i = 0; i < inst->number_of_targets; ++i)
	if (inst->targets[i].card >= 0)
	  get_card_instance(inst->targets[i].player, inst->targets[i].card)->state &= ~STATE_TARGETTED;

  if (failed)
	{
	  inst->number_of_targets = 0;
	  return 0;
	}

  inst->number_of_targets = total_damage;
  int rval = divide_damage(player, card, NULL);
  inst->number_of_targets = 0;

  return rval;
}

// Deals 1 damage to each of {player,card}'s targets for each time it was damaged.
int divide_damage(int player, int card, target_definition_t* td)
{
  card_instance_t* inst = get_card_instance(player, card);

  int i, validated = 0;
  for (i = 0; i < inst->number_of_targets; ++i)
	if (inst->targets[i].player >= 0)
	  {
		// Find any repeats of that target, so we deal the full amount of damage in one packet
		int j, dmg = 1;
		for (j = i + 1; j < inst->number_of_targets; ++j)
		  if (inst->targets[j].card == inst->targets[i].card
			  && inst->targets[j].player == inst->targets[i].player)
			{
			  inst->targets[j].player = inst->targets[j].card = -1;	// mark so it won't be processed again
			  ++dmg;
			}

		if (!td || validate_target(player, card, td, i))
		  {
			++validated;
			damage_creature(inst->targets[i].player, inst->targets[i].card, dmg, player, card);
		  }
		else
		  inst->targets[i].player = inst->targets[i].card = -1;
	  }

  return validated;
}

int check_playable_permanents(int player, int p_type, int mode){
	int count = 0;
	int max_value = -1;
	int result = -1;
	while( count < active_cards_count[player] ){
			if( in_hand(player, count) && is_what(player, count, p_type) ){
				int id = get_id(player, count);
				card_ptr_t* c = cards_ptr[ id ];
				if( has_mana_multi(player, c->req.req_colorless, c->req.req_black, c->req.req_blue, c->req.req_green,
					c->req.req_red, c->req.req_white)
				  ){
					card_instance_t *instance = get_card_instance(player, count);
					if( can_legally_play_iid(player, instance->internal_card_id) ){
						if( mode == 1 ){
							return 1;
						}
						if( c->ai_base_value > max_value ){
							max_value = c->ai_base_value;
							result = count;
						}
					}
				}
			}
			count++;
	}
	if( mode == 1 ){
		result = 0;
	}

	return result;
}

/* Returns the number of times {player,card} can activate for the given life and mana cost, multiplied by pump.  If limit is >= 0, it's an additional constraint
 * on the maximum number of times it can activate (e.g., to deal with a cost involving a sacrifice). */

// If p_pump >= 100, adds p_pump-100 +1/+1 counters, doesn't add power, and ignores t_pump.
int generic_shade(int player, int card, event_t event, int life_to_pay, int cless, int black,
                  int blue, int green, int red, int white, int p_pump, int t_pump, int k_pump,
                  int sp_key_pump)
{
  int parent_card;
  int result;
  card_instance_t *instance;
  int pump_amount;
  pump_amount = p_pump;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(player, card);
    if (((result != 0) && (result = has_mana_for_activated_ability(player, card, cless, black, blue,
                                                                   green, red, white),
                           result != 0)) &&
        (result = can_pay_life_impl(player, life_to_pay, 0), result != 0))
    {
      return 1;
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      result =
          charge_mana_for_activated_ability(player, card, cless, black, blue, green, red, white);
      if (result == 0)
      {
        return 0;
      }
      lose_life(player, life_to_pay);
      if (0 < p_pump)
      {
        return 0;
      }
      if (0 < t_pump)
      {
        return 0;
      }
      if ((k_pump == 0) ||
          ((result = check_for_ability(player, card, k_pump),
            result == 0 && (instance = has_activation_on_stack(player, card), instance == NULL))))
      {
        if (sp_key_pump == 0)
        {
          return 0;
        }
        result = check_for_special_ability(player, card, sp_key_pump);
        if ((result == 0) && (instance = has_activation_on_stack(player, card), instance == NULL))
        {
          return 0;
        }
      }
      ai_modifier = ai_modifier + -0x37;
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      result = instance->parent_controller;
      parent_card = instance->parent_card;
      instance = in_play(result, parent_card);
      if (instance == NULL)
      {
        return 0;
      }
      if (99 < p_pump)
      {
        add_counters(player, card, result, parent_card, COUNTER_P1_P1, p_pump + -100);
        if (k_pump == 0 && sp_key_pump == 0)
        {
          return 0;
        }
        t_pump = 0;
        p_pump = 0;
      }
      pump_ability_until_eot(result, parent_card, result, parent_card, p_pump, t_pump, k_pump, sp_key_pump);
      return 0;
    }
  }
  if (1 < (uint32_t)(event - EVENT_POW_BOOST))
  {
    return 0;
  }
  if (p_pump < 100)
  {
    if (event != EVENT_POW_BOOST)
    {
      pump_amount = t_pump;
    }
  }
  else
  {
    pump_amount = p_pump + -100;
  }
  return generic_shade_amt_can_pump(player, card, pump_amount, life_to_pay, cless, black, blue,
                                      green, red, white, -1);
}

/* Just like generic_shade(), but can't pump abilities or add counters, and sends its effect through pump_until_eot_merge_previous() so they combine into a
 * single effect card.  Requires that the card's effect text uses |n substitutions. */
int generic_shade_merge_pt(int player, int card, event_t event, int life_to_pay, int cless,
                           int black, int blue, int green, int red, int white, int p_pump,
                           int t_pump)
{
  if (event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) &&
      has_mana_for_activated_ability(player, card, cless, black, blue, green, red, white) &&
      can_pay_life(player, life_to_pay))
    return 1;

  if (event == EVENT_ACTIVATE &&
      charge_mana_for_activated_ability(player, card, cless, black, blue, green, red, white))
    lose_life(player, life_to_pay);

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->parent_controller, c = instance->parent_card;
    if (in_play(p, c))
      pump_until_eot_merge_previous_alt(player, card, p, c, p_pump, t_pump, 1);
  }

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
    return generic_shade_amt_can_pump(player, card, event == EVENT_POW_BOOST ? p_pump : t_pump,
                                      life_to_pay, cless, black, blue, green, red, white, -1);

  return 0;
}

// If p_pump >= 100, adds p_pump-100 +1/+1 counters, doesn't add power, and ignores t_pump.
int generic_husk(int player, int card, event_t event, int type, int p_pump, int t_pump, int k_pump,
                 int sp_key_pump)
{
  int parent_card;
  card_instance_t *instance;
  int result;
  unsigned int sacrifice_choice;
  test_definition_t test;
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    if (event == EVENT_POW_BOOST)
    {
      t_pump = p_pump;
    }
    if (99 < p_pump)
    {
      t_pump = p_pump + -100;
    }
    if (0 < t_pump)
    {
      new_default_test_definition(&test, type, "");
      test.not_me = 1;
      result = max_can_sacrifice_as_cost(player, card, &test);
      if (0 < result)
      {
        return generic_shade_amt_can_pump(player, card, t_pump, 0, 0, 0, 0, 0, 0, 0, result);
      }
    }
  }
  else if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_use_activated_abilities(player, card);
      if ((result != 0) &&
          (result = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0), result != 0))
      {
        result = can_sacrifice_type_as_cost(player, 1, type);
        return (unsigned int)(result != 0);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if (result != 0)
      {
        sacrifice_choice =
            controller_sacrifices_a_permanent(player, card, type, SAC_RETURN_CHOICE | SAC_AS_COST);
        if (sacrifice_choice == 0)
        {
          spell_fizzled = 1;
        }
        else if (((player == 1) && (card == sacrifice_choice >> 0x18)) && ((char)(sacrifice_choice >> 0x10) == '\x01'))
        {
          ai_modifier = ai_modifier + -0x60;
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      result = instance->parent_controller;
      parent_card = instance->parent_card;
      instance = in_play(result, parent_card);
      if (instance != NULL)
      {
        if (p_pump < 100)
        {
          if ((k_pump == 0 && sp_key_pump == 0) &&
              (((0 < p_pump || ((p_pump == 0 && (0 < t_pump)))) ||
                ((p_pump < 0 && (t_pump == 0))))))
          {
            pump_until_eot_merge_previous_alt(result, parent_card, result, parent_card, p_pump, t_pump, 0);
            return 0;
          }
        }
        else
        {
          add_counters(player, card, result, parent_card, COUNTER_P1_P1, p_pump + -100);
          if (k_pump == 0 && sp_key_pump == 0)
          {
            return 0;
          }
          t_pump = 0;
          p_pump = 0;
        }
        pump_ability_until_eot(result, parent_card, result, parent_card, p_pump, t_pump, k_pump,
                               sp_key_pump);
        return 0;
      }
    }
  }
  return 0;
}

// Like generic_shade, but includes T in activation cost.  Only supports modifications to power or toughness, due to the limited card pool.
int generic_shade_tap(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white, int power, int toughness)
{
  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int p = instance->parent_controller, c = instance->parent_card;
	  if (in_play(p, c))
		pump_ability_until_eot(p, c, p, c, power, toughness, 0, 0);
	}

  if (event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
	  int pump = event == EVENT_POW_BOOST ? power : toughness;
	  if (pump > 0
		  && generic_shade_tap(player, card, EVENT_CAN_ACTIVATE, cless, black, blue, green, red, white, power, toughness))
		return pump;
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, cless, black, blue, green, red, white, 0, NULL, NULL);
}

// Only needs to be called during EVENT_GRAVEYARD_FROM_PLAY and TRIGGER_GRAVEYARD_FROM_PLAY.  (More to the point, only need to initialize test then.)
int store_and_trigger_graveyard_from_play(int player, int card, int event, int t_player,
                                          int trigger_mode, test_definition_t *test,
                                          int triggering_player, int triggering_card)
{
  card_instance_t *aff;
  if (event == EVENT_GRAVEYARD_FROM_PLAY &&
      (affected_card_controller == t_player || t_player == ANYBODY) &&
      (aff = in_play(affected_card_controller, affected_card)) &&
      !(test->not_me == 1 && affect_me(player, card)) && aff->kill_code > 0 &&
      aff->kill_code < KILL_EXILE &&
      new_make_test_in_play(affected_card_controller, affected_card, -1, test) &&
      !check_for_death_trigger_removal(affected_card_controller, affected_card))
  {
    card_instance_t *instance = get_card_instance(triggering_player, triggering_card);

    if (instance->targets[11].player < 0)
      instance->targets[11].player = 0;

    if (instance->targets[11].player < 10)
    {
      instance->targets[instance->targets[11].player].player = affected_card_controller;
      instance->targets[instance->targets[11].player].card = affected_card;
      instance->targets[11].player++;
    }
  }

  if (trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY &&
      triggering_player == reason_for_trigger_controller &&
      affect_me(triggering_player, triggering_card))
  {
    card_instance_t *instance = get_card_instance(triggering_player, triggering_card);
    if (instance->targets[11].player > 0)
    {
      if (event == EVENT_TRIGGER)
      {
        if (trigger_mode == RESOLVE_TRIGGER_DUH)
          trigger_mode =
              duh_mode(triggering_player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
        event_result |= trigger_mode;
      }

      if (event == EVENT_RESOLVE_TRIGGER)
      {
        int num = instance->targets[11].player;
        instance->targets[11].player = 0;
        return num;
      }

      if (event == EVENT_END_TRIGGER)
        instance->targets[11].player = 0;
    }
  }

  return 0;
}

static int is_dying(int player, int card){
	if( ! in_play(player, card) ){
		return 0;
	}
	if( check_special_flags2(player, card, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE) ){
		return 0;
	}
	int kill_code = get_card_instance(player, card)->kill_code;
	if( kill_code < KILL_BURY || kill_code > KILL_SACRIFICE ){
		return 0;
	}
	return 1;
}

int count_graveyard_from_play(int player, int card, int event, int trg, int type, int flag1,
                              int subtype, int flag2, int color, int flag3, int id, int flag4,
                              int cc, int flag5)
{

  int all_players = 0;
  int not_me = 0;
  int opt_trigger = 0;
  if (trg & 2)
  {
    all_players = 1;
  }
  if (trg & 4)
  {
    trg -= 4;
    not_me = 1;
  }
  if (trg & 8)
  {
    trg -= 8;
    opt_trigger = 1;
  }

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_GRAVEYARD_FROM_PLAY && (affected_card_controller == trg || all_players == 1))
  {
    if (!in_play(affected_card_controller, affected_card))
    {
      return 0;
    }
    if (affect_me(player, card) && not_me == 1)
    {
      return 0;
    }
    if (check_for_death_trigger_removal(affected_card_controller, affected_card))
    {
      return 0;
    }
    if (make_test_in_play(affected_card_controller, affected_card, -1, type, flag1, subtype, flag2,
                          color, flag3, id, flag4, cc, flag5))
    {
      card_instance_t *affected = get_card_instance(affected_card_controller, affected_card);
      if (affected->kill_code > 0 && affected->kill_code < 4)
      {
        if (instance->targets[11].player < 0)
        {
          instance->targets[11].player = 0;
        }
        instance->targets[11].player++;
        if (get_id(player, card) == CARD_ID_KRESH_THE_BLOODBRAIDED ||
            get_id(player, card) == CARD_ID_GRAND_OSSUARY)
        {
          if (instance->targets[1].player < 0)
          {
            instance->targets[1].player = 0;
          }
          instance->targets[1].player += get_power(affected_card_controller, affected_card);
        }
        if (instance->targets[0].card == CARD_ID_SHOWSTOPPER)
        {
          if (instance->targets[1].player < 2)
          {
            instance->targets[1].player = 2;
          }
          int pos = instance->targets[1].player;
          if (pos < 18)
          {
            instance->targets[pos].player =
                get_protections_from(affected_card_controller, affected_card);
          }
        }
      }
    }
  }

  if (instance->targets[11].player > 0 && trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY &&
      player == reason_for_trigger_controller && affect_me(player, card))
  {
    if (event == EVENT_TRIGGER)
    {
      if (opt_trigger == 1)
      {
        event_result |= 1 + player;
      }
      else
      {
        event_result |= RESOLVE_TRIGGER_MANDATORY;
      }
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance->targets[11].card = instance->targets[11].player;
      instance->targets[11].player = 0;
      return 1;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[11].player = 0;
    }
  }
  return 0;
}

int dont_account_for_trinisphere = 0;
int havengul_lich_hack = 0;
// exactly one of card and iid must be >= 0.  Player must always be valid.
// this could return even negative values, used by Buyback and similar costs modifier.
int true_get_updated_casting_cost(int player, int card, int iid, event_t event, int orig_cless)
{
  if (1 < (unsigned int)player)
  {
    backtrace_and_abort("functions/functions.c:10186\nplayer == 0 || player == 1 || "
                        "!\"get_updated_casting_cost() needs a real player\"");
  }
  if ((card < 0) == (iid < 0))
  {
    backtrace_and_abort(
        "functions/functions.c:10187\n(card < 0) != (iid < 0) && \"get_updated_casting_cost() "
        "needs exactly one of card and iid to be valid\"");
  }
  if (iid < 0)
    iid = get_card_instance(player, card)->internal_card_id;
  if (cards_data[iid].type & TYPE_LAND)
    return 0;
  int spell_id = cards_data[iid].id;
  card_ptr_t *definition = cards_ptr[spell_id];
  uint32_t cost = orig_cless == -1 ? definition->req.req_colorless : (uint32_t)orig_cless;
  if (orig_cless == -1 ? cost >= 16 : orig_cless >= 16)
    cost = 0;
  int colors = get_color_by_internal_id(player, iid);
  int has_trinisphere = 0;
  int controller;
  int checked_player = 0;
  for (controller = 0; controller < 2; controller++)
  {
    card_instance_t *instance;
    int permanent = 0;
    while (permanent < active_cards_count[controller])
    {
      if ((instance = in_play(controller, permanent)) &&
          (is_what(controller, permanent, TYPE_EFFECT) ||
           !is_humiliated(controller, permanent, AL_STATIC)))
      {
        int source_id = get_id(controller, permanent);
        switch (source_id)
        {
        case CARD_ID_SPECIAL_EFFECT:
        {
          int flags = instance->targets[0].card;
          if ((flags & SE_CSVID_CANT_BE_PLAYED) || (flags & SE_TYPE_CANT_BE_PLAYED))
          {
            if (player == instance->targets[0].player || instance->targets[0].player == ANYBODY)
            {
              if (flags & SE_CSVID_CANT_BE_PLAYED)
              {
                if (cards_data[iid].id == instance->targets[1].player)
                {
                  return 99;
                }
              }
              if ((flags & SE_TYPE_CANT_BE_PLAYED) && instance->targets[1].player > -1)
              {
                if (is_what(-1, iid, instance->targets[1].player))
                {
                  return 99;
                }
              }
            }
          }
          break;
        }

        case CARD_ID_ANGELIC_ARBITER:
        {
          if (instance->targets[1].card > 0 && (instance->targets[1].card & 1) &&
              controller != player)
          {
            return 99;
          }
          break;
        }

        case CARD_ID_ANIMAR_SOUL_OF_ELEMENTS:
        {
          if (controller == player && is_what(-1, iid, TYPE_CREATURE))
          {
            cost -= count_1_1_counters(controller, permanent);
          }
          break;
        }

        case CARD_ID_ARCANE_LABORATORY:
        case CARD_ID_RULE_OF_LAW:
        {
          if (instance->targets[1].player > 0 && (instance->targets[1].player & (1 + player)))
          {
            return 99;
          }
          break;
        }

        case CARD_ID_ARCANE_MELEE:
        {
          if (!is_what(-1, iid, TYPE_CREATURE) && is_what(-1, iid, TYPE_SPELL))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_AURA_OF_SILENCE:
        {
          if (controller != player && is_what(-1, iid, TYPE_ARTIFACT | TYPE_ENCHANTMENT))
          {
            cost += 2;
          }
          break;
        }

        case CARD_ID_BALLYRUSH_BANNERET:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_SOLDIER) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_KITHKIN)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_BASANDRA_BATTLE_SERAPH:
        {
          if (current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2)
          {
            return 99;
          }
          break;
        }

        case CARD_ID_BOSK_BANNERET:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_TREEFOLK) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_SHAMAN)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_BRAND_OF_ILL_OMEN:
        {
          if (player == instance->damage_target_player && is_what(-1, iid, TYPE_CREATURE))
          {
            return 99;
          }
          break;
        }

        case CARD_ID_BRIGHTHEARTH_BANNERET:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_ELEMENTAL) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_WARRIOR)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_CENTAUR_OMENREADER:
        {
          if (is_tapped(controller, permanent) && controller == player &&
              is_what(-1, iid, TYPE_CREATURE))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_CLOUD_KEY:
        {
          if (instance->targets[1].card > 0 && is_what(-1, iid, instance->targets[1].card))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_COUNCIL_OF_THE_ABSOLUTE:
        {
          if (get_card_instance(controller, permanent)->targets[1].card == spell_id)
          {
            if (controller == player)
            {
              cost -= 2;
            }
            else
            {
              return 99;
            }
          }
          break;
        }

        case CARD_ID_CURSE_OF_EXHAUSTION:
        {
          if (player == instance->targets[0].player && instance->targets[1].player == 66)
          {
            return 49;
          }
          break;
        }

        case CARD_ID_DEFENSE_GRID:
        {
          if (current_turn != player)
          {
            cost += 3;
          }
          break;
        }

        case CARD_ID_DRAGONSPEAKER_SHAMAN:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_DRAGON))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_ELDER_DRAGON_HIGHLANDER:
        {
          if (instance->targets[5].card == iid)
          {
            counter_t counter_type;
            if (instance->targets[2].player == iid)
              counter_type = COUNTER_VORTEX;
            else if (instance->targets[2].card == iid)
              counter_type = COUNTER_VELOCITY;
            else
              break;
            int counters = count_counters(controller, permanent, counter_type);
            int myths = count_cards_by_id(player, CARD_ID_MYTH_UNBOUND);
            unsigned int repeated_counters = count_counters(controller, permanent, counter_type);
            cost += counters - (repeated_counters >> 1) * myths;
          }
          break;
        }

        case CARD_ID_EMERALD_MEDALLION:
        {
          if (controller == player && (colors & COLOR_TEST_GREEN))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_ETHERIUM_SCULPTOR:
        {
          if (is_what(-1, iid, TYPE_ARTIFACT) && controller == player)
          {
            cost--;
          }
          break;
        }

        case CARD_ID_ETHERSWORN_CANONIST:
        {
          if (!is_what(-1, iid, TYPE_ARTIFACT))
          {
            if (instance->targets[1].player > 0 && (instance->targets[1].player & (1 + player)))
            {
              return 99;
            }
          }
          break;
        }

        case CARD_ID_EXCLUSION_RITUAL:
        {
          if (instance->targets[1].card == spell_id)
          {
            return 99;
          }
          break;
        }

        case CARD_ID_EYE_OF_UGIN:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_ELDRAZI) &&
              !(colors & COLOR_TEST_ANY_COLORED))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_FROGTOSSER_BANNERET:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_GOBLIN) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_ROGUE)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_GADDOCK_TEEG:
        {
          if (!is_what(-1, iid, TYPE_CREATURE))
          {
            if (get_cmc_by_id(spell_id) > 3)
            {
              return 99;
            }
          }
          break;
        }

        case CARD_ID_GLOOM:
        {
          if ((colors & COLOR_TEST_WHITE))
          {
            cost += 3;
          }
          break;
        }

        case CARD_ID_GOBLIN_ELECTROMANCER:
        {
          if (controller == player && !is_what(-1, iid, TYPE_CREATURE) &&
              is_what(-1, iid, TYPE_SPELL))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_GOBLIN_WARCHIEF:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_GOBLIN))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_GRAND_ABOLISHER:
        {
          if (controller != player && current_turn == controller)
          {
            return 99;
          }
          break;
        }

        case CARD_ID_GRAND_ARBITER_AUGUSTIN_IV:
        {
          if (controller == player)
          {
            cost--;
          }
          else
          {
            cost++;
          }
          break;
        }

        case CARD_ID_GRID_MONITOR:
        case CARD_ID_STEEL_GOLEM:
        {
          if (controller == player && is_what(-1, iid, TYPE_CREATURE))
          {
            return 49;
          }
          break;
        }

        case CARD_ID_HEARTLESS_SUMMONING:
        {
          if (controller == player && is_what(-1, iid, TYPE_CREATURE))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_HELM_OF_AWAKENING:
          cost--;
          break;

        case CARD_ID_HERALD_OF_WAR:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_ANGEL) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_HUMAN)))
          {
            cost -= count_1_1_counters(controller, permanent);
          }
          break;
        }

        case CARD_ID_HERO_OF_IROAS:
        {
          if (controller == player && has_subtype(-1, iid, SUBTYPE_AURA) &&
              !is_humiliated(controller, permanent, AL_STATIC))
          {
            --cost;
          }
          break;
        }

        case CARD_ID_IN_THE_EYE_OF_CHAOS:
        {
          if (is_what(-1, iid, TYPE_INSTANT) && !is_what(-1, iid, TYPE_CREATURE))
          {
            cost += get_cmc_by_id(spell_id);
          }
          break;
        }

        case CARD_ID_IONA_SHIELD_OF_EMERIA:
        {
          if (controller == 1 - player && !is_what(-1, iid, TYPE_LAND))
          {
            if (colors & instance->targets[10].card)
            {
              return 49;
            }
          }
          break;
        }

          /* Semi-general case.  Checks against player need to be handled both here and specially in
           * the card function; and the card function shouldn't do anything with
           * affected_card_controller except check it with affect_me() or pass it on to functions
           * that explicitly understand the
           * (-1,iid) convention.  Usually this means checking EVENT_MODIFY_COST_GLOBAL first and
           * explicitly returning at the end.
           * Simply checking affected_card_controller == player in the card function WILL FAIL.
           * Calling most functions that look at affected_card_controller WILL CRASH. */

          // Spells your opponents cast cost more/cost less/can't be played
        case CARD_ID_ALHAMMARRET_HIGH_ARBITER:
        case CARD_ID_DRAGONLORD_DROMOKA:
        case CARD_ID_VOID_WINNOWER:
        case CARD_ID_BRISELA_VOICE_OF_NIGHTMARES:
          if (player == controller)
          {
            break;
          }
          checked_player = 1;
          // intentionally fall through

          // Spells you cast cost more/cost less/can't be played.  See above for usage.
        case CARD_ID_ARCHENEMY:
        case CARD_ID_CONDUIT_OF_RUIN:
        case CARD_ID_DRAGONLORDS_SERVANT:
        case CARD_ID_FOUNDRY_INSPECTOR:
        case CARD_ID_GEIST_FUELED_SCARECROW:
        case CARD_ID_HERALD_OF_KOZILEK:
        case CARD_ID_HERALD_OF_THE_PANTHEON:
        case CARD_ID_JACES_SANCTUM:
        case CARD_ID_KRALLENHORDE_HOWLER:
        case CARD_ID_MIZZIX_OF_THE_IZMAGNUS:
        case CARD_ID_PLANAR_GATE:
        case CARD_ID_SEAL_OF_THE_GUILDPACT:
        case CARD_ID_VORACIOUS_READER:
          if (player != controller && !checked_player)
          {
            break;
          }
          checked_player = 1;
          // intentionally fall through

          // Spells anybody casts cost more/cost less/can't be played.  See above for usage.
        case CARD_ID_FEROZS_BAN:
        case CARD_ID_IRINI_SENGIR:
        {
          int old_cost_colorless = COST_COLORLESS;
          int old_affected_card_controller = affected_card_controller;
          int old_affected_card = affected_card;
          int old_event_result = event_result;

          event_result = player;
          if (card >= 0)
          {
            affected_card_controller = player;
            affected_card = card;
          }
          else
          {
            affected_card_controller = -1;
            affected_card = iid;
          }

          call_card_function(controller, permanent, EVENT_MODIFY_COST_GLOBAL);

          affected_card_controller = old_affected_card_controller;
          affected_card = old_affected_card;
          event_result = old_event_result;

          cost += COST_COLORLESS - old_cost_colorless;
          COST_COLORLESS = old_cost_colorless;
          break;
        }

        case CARD_ID_JET_MEDALLION:
        {
          if (controller == player && (colors & COLOR_TEST_BLACK))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_LOCKET_OF_YESTERDAYS:
        {
          if (controller == player)
          {
            cost -= count_graveyard_by_id(player, spell_id);
          }
          break;
        }

        case CARD_ID_LODESTONE_GOLEM:
        {
          if (!is_what(-1, iid, TYPE_ARTIFACT))
          {
            cost++;
          }
          break;
        }

        case CARD_ID_MANA_MATRIX:
        {
          if (controller == player &&
              (is_what(-1, iid, TYPE_INSTANT) || is_what(-1, iid, TYPE_ENCHANTMENT)))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_MEDDLING_MAGE:
        case CARD_ID_NEVERMORE:
        {
          if (instance->targets[1].card == spell_id)
          {
            return 49;
          }
          break;
        }

        case CARD_ID_MYCOSYNTH_GOLEM:
        {
          if (is_what(-1, iid, TYPE_ARTIFACT) && is_what(-1, iid, TYPE_CREATURE))
          {
            cost -= count_subtype(player, TYPE_ARTIFACT, -1);
          }
          break;
        }

        case CARD_ID_NIGHTSCAPE_FAMILIAR:
        {
          if (controller == player &&
              (colors &
               get_sleighted_color_test(controller, permanent, COLOR_TEST_BLUE | COLOR_TEST_RED)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_OMNISCIENCE:
        {
          if (controller == player)
          {
            cost -= definition->req.req_colorless;
          }
          break;
        }

        case CARD_ID_PEARL_MEDALLION:
        {
          if (controller == player && (colors & COLOR_TEST_WHITE))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_RAKDOS_LORD_OF_RIOTS:
        {
          if (controller == player && is_what(-1, iid, TYPE_CREATURE))
          {
            cost -= get_trap_condition(1 - player, TRAP_LIFE_LOST);
          }
          break;
        }

        case CARD_ID_ROOFTOP_STORM:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_ZOMBIE))
          {
            cost -= definition->req.req_colorless;
          }
          break;
        }

        case CARD_ID_RUBY_MEDALLION:
        {
          if (controller == player && (colors & COLOR_TEST_RED))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_SAPPHIRE_MEDALLION:
        {
          if (controller == player && (colors & COLOR_TEST_BLUE))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_SPHERE_OF_RESISTANCE:
        {
          cost++;
          break;
        }

        case CARD_ID_STINKDRINKER_DAREDEVIL:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_GIANT))
          {
            cost -= 2;
          }
          break;
        }

        case CARD_ID_STONE_CALENDAR:
        {
          if (controller == player)
          {
            cost--;
          }
          break;
        }

        case CARD_ID_STONYBROOK_BANNERET:
        {
          if (controller == player && (has_subtype_by_id(player, spell_id, SUBTYPE_MERFOLK) ||
                                       has_subtype_by_id(player, spell_id, SUBTYPE_WIZARD)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_STORMSCAPE_FAMILIAR:
        {
          if (controller == player &&
              (colors & get_sleighted_color_test(controller, permanent,
                                                 COLOR_TEST_WHITE | COLOR_TEST_BLACK)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_SUNSCAPE_FAMILIAR:
        {
          if (controller == player &&
              (colors &
               get_sleighted_color_test(controller, permanent, COLOR_TEST_GREEN | COLOR_TEST_BLUE)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_TEFERI_MAGE_OF_ZHALFIR:
        {
          if (controller != player && !can_sorcery_be_played(player, event))
          {
            return 49;
          }
          break;
        }

        case CARD_ID_THALIA_GUARDIAN_OF_THRABEN:
        {
          if (controller != player && !is_what(-1, iid, TYPE_CREATURE))
          {
            cost++;
          }
          break;
        }

        case CARD_ID_THORNSCAPE_FAMILIAR:
        {
          if (controller == player &&
              (colors &
               get_sleighted_color_test(controller, permanent, COLOR_TEST_RED | COLOR_TEST_WHITE)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_THUNDERSCAPE_FAMILIAR:
        {
          if (controller == player &&
              (colors & get_sleighted_color_test(controller, permanent,
                                                 COLOR_TEST_BLACK | COLOR_TEST_GREEN)))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_TRINISPHERE:
          has_trinisphere = 1;
          break;

        case CARD_ID_THORN_OF_AMETHYST:
        case CARD_ID_VRYN_WINGMARE:
        {
          if (!is_what(-1, iid, TYPE_CREATURE))
          {
            cost++;
          }
          break;
        }

        case CARD_ID_UNDEAD_WARCHIEF:
        {
          if (controller == player && has_subtype_by_id(player, spell_id, SUBTYPE_ZOMBIE))
          {
            cost--;
          }
          break;
        }

        case CARD_ID_VOIDSTONE_GARGOYLE:
        {
          if (instance->targets[1].card == spell_id)
          {
            return 99;
          }
          break;
        }

        case CARD_ID_WARD_OF_BONES:
        {
          if (controller != player)
          {
            if (instance->targets[1].card == 66 && is_what(-1, iid, TYPE_CREATURE))
            {
              return 49;
            }
            else if (instance->targets[2].card == 66 && is_what(-1, iid, TYPE_ARTIFACT))
            {
              return 49;
            }
            else if (instance->targets[3].card == 66 && is_what(-1, iid, TYPE_ENCHANTMENT))
            {
              return 49;
            }
          }
          break;
        }

        case CARD_ID_OBSCURING_AETHER:
        case CARD_ID_DREAM_CHISEL:
        {
          if (controller == player && cards_data[iid].id == CARD_ID_FACE_DOWN_CREATURE)
          {
            --cost;
          }
          break;
        }

        case CARD_ID_CONSPIRACY_HYMN_OF_THE_WILDS:
        {
          if (controller == player)
          {
            if (is_what(-1, iid, TYPE_SPELL))
            {
              infinite_casting_cost();
            }
            if (is_what(-1, iid, TYPE_CREATURE) && get_stormcreature_count(controller) == 0)
            {
              COST_COLORLESS--;
            }
          }
          break;
        }

        case CARD_ID_SANCTUM_PRELATE:
        {
          int value = instance->info_slot;
          if (!is_what(-1, iid, TYPE_CREATURE) && get_cmc_by_internal_id(iid) == value)
          {
            infinite_casting_cost();
          }
          break;
        }

        case CARD_ID_AETHER_STORM:
          if (is_what(-1, iid, TYPE_CREATURE))
            return 99;
          break;

        case CARD_ID_BONTUS_MONUMENT:
          if (controller == player && is_what(-1, iid, TYPE_CREATURE) &&
              (get_sleighted_color_test(controller, permanent, COLOR_TEST_BLACK) & colors))
            --cost;
          break;
        case CARD_ID_KEFNETS_MONUMENT:
          if (controller == player && is_what(-1, iid, TYPE_CREATURE) &&
              (get_sleighted_color_test(controller, permanent, COLOR_TEST_BLUE) & colors))
            --cost;
          break;
        case CARD_ID_RHONASS_MONUMENT:
          if (controller == player && is_what(-1, iid, TYPE_CREATURE) &&
              (get_sleighted_color_test(controller, permanent, COLOR_TEST_GREEN) & colors))
            --cost;
          break;
        case CARD_ID_HAZORETS_MONUMENT:
          if (controller == player && is_what(-1, iid, TYPE_CREATURE) &&
              (get_sleighted_color_test(controller, permanent, COLOR_TEST_RED) & colors))
            --cost;
          break;
        case CARD_ID_OKETRAS_MONUMENT:
          if (controller == player && is_what(-1, iid, TYPE_CREATURE) &&
              (get_sleighted_color_test(controller, permanent, COLOR_TEST_WHITE) & colors))
            --cost;
          break;

        case CARD_ID_HAMZA_GUARDIAN_OF_ARASHIN:
          if (controller == player)
            cost -= count_creatures_with_p1_p1_counter(player);
          break;

        case CARD_ID_SPECTACLE_MAGE:
          if (controller == player && is_what(-1, iid, TYPE_SPELL))
            cost -= get_cmc_by_id(spell_id) > 4;
          break;

        default:
          break;
        }
      }
      permanent++;
    }
  }

  if (has_trinisphere && (int32_t)cost < 3 && dont_account_for_trinisphere <= 0)
  {
    if (orig_cless == -1)
    {
      unsigned int colored_cost = definition->req.req_black + definition->req.req_blue +
                                  definition->req.req_green + definition->req.req_red +
                                  definition->req.req_white;
      if (colored_cost < 3)
        cost = 3 - colored_cost;
    }
    else
      cost = 3U - (uint32_t)orig_cless;
  }
  return (int32_t)(cost + (uint32_t)havengul_lich_hack);
}

// This is a frontend and prevent the return of negative values when charging mana
int get_updated_casting_cost(int player, int card, int iid, event_t event, int cless){
	return MAX(0, true_get_updated_casting_cost(player, card, iid, event, cless));
}

int is_phyrexian(int id)
{
  if (id == CARD_ID_ACT_OF_AGGRESSION || id == CARD_ID_APOSTLES_BLESSING ||
      id == CARD_ID_BIRTHING_POD || id == CARD_ID_CATHEDRAL_MEMBRANE ||
      id == CARD_ID_CORROSIVE_GALE || id == CARD_ID_DISMEMBER || id == CARD_ID_GITAXIAN_PROBE ||
      id == CARD_ID_GUT_SHOT || id == CARD_ID_MARROW_SHARDS || id == CARD_ID_MENTAL_MISSTEP ||
      id == CARD_ID_MOLTENSTEEL_DRAGON || id == CARD_ID_MUTAGENIC_GROWTH ||
      id == CARD_ID_NORNS_ANNEX || id == CARD_ID_NOXIOUS_REVIVAL ||
      id == CARD_ID_PHYREXIAN_METAMORPH || id == CARD_ID_PITH_DRILLER ||
      id == CARD_ID_PORCELAIN_LEGIONNAIRE || id == CARD_ID_POSTMORTEM_LUNGE ||
      id == CARD_ID_RAGE_EXTRACTOR || id == CARD_ID_RUTHLESS_INVASION ||
      id == CARD_ID_SLASH_PANTHER || id == CARD_ID_SPINED_THOPTER ||
      id == CARD_ID_SURGICAL_EXTRACTION || id == CARD_ID_TEZZERETS_GAMBIT ||
      id == CARD_ID_THUNDERING_TANADON || id == CARD_ID_VAULT_SKIRGE ||
      id == CARD_ID_KRRIK_SON_OF_YAWGMOTH
  )
  {
    return 1;
  }
  return 0;
}

int is_id_in_grave(int player, int id){
	const int *grave = get_grave(player);
	int count = count_graveyard(player)-1;
	while( count > -1 ){
			if( cards_data[grave[count]].id == id ){
				return 1;
			}
			count--;
	}
	return 0;
}

int is_id_in_hand(int player, int id){
	int count = 0;
	while( count < active_cards_count[player] ){
			if( in_hand(player, count) && get_id(player, count) == id ){
				return 1;
			}
			count++;
	}
	return 0;
}

#define C(ch) ((ch) == 'B' || (ch) == 'U' || (ch) == 'G' || (ch) == 'R' || (ch) == 'W')
// These are fragile, but better than what we had before.

int is_normal_hybrid(card_ptr_t *cp)
{
  const char *c = cp->mana_cost_text;
  if (!c)
    return 0;

  int len = strlen(c);
  for (int i = 0; i < len - 1; ++i)
    if (C(c[i]) && C(c[i + 1]) && (c[i + 2] == '|' || c[i + 2] == 0))
      return 1;
  return 0;
}

int is_mixed_hybrid(card_ptr_t* cp)
{
  // All existing mixed hybrid/non-hybrid casting costs end with "|xx|x", where x is any character; no other cards do.
  const char* c = cp->mana_cost_text;
  int len;
  return c && (len = strlen(c)) >= 5 && c[len-2] == '|' && c[len-5] == '|';
}
#undef C

#define USE_HOPELESSLY_BROKEN 1

#ifdef USE_HOPELESSLY_BROKEN
int has_mana_to_cast_iid(int player, event_t event, int iid)
{
  /****************************************************************************************************************
   * Any further special cases for specific cards are unacceptable in this function and will be
   * reverted on sight. *
   ****************************************************************************************************************/
  int csvid = cards_data[iid].id;
  if (csvid == CARD_ID_REAPER_KING)
    return can_cast_reaper_king(player, -1, event);

  int split_half = split_cards_id_table(csvid, 1);
  if (split_half != -1)
    return has_mana_to_cast_id(player, event, split_half) ||
           has_mana_to_cast_id(player, event, split_cards_id_table(csvid, 2));

  card_ptr_t *cp = cards_ptr[csvid];
  int orig_cless = cp->req.req_colorless;
  if (orig_cless == 40)
    orig_cless = 0;

  int cless = get_updated_casting_cost(player, -1, iid, event, orig_cless);
  if (csvid == CARD_ID_KARADOR_GHOST_CHIEFTAIN)
  {
    cless -= count_graveyard_by_type(player, TYPE_CREATURE);
    if (cless < 0)
      cless = 0;
  }
  int is_artifact = cards_data[iid].type & TYPE_ARTIFACT;
  if (is_phyrexian(csvid))
    return has_phyrexian_mana(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green,
                              cp->req.req_red, cp->req.req_white, is_artifact);
  else if (is_normal_hybrid(cp))
  {
    int c1 = get_colors_for_hybrid(-1, iid, 0, 0);
    int c2 = get_colors_for_hybrid(-1, iid, c1, 0);
    return has_mana_hybrid(player, get_number_of_hybrid_mana(-1, iid, 0, 0), c1, c2, cless);
  }
  else if (is_monocolor_hybrid(cp))
  {
    int c1 = get_color_for_monocolored_hybrid(-1, iid);
    return has_mana_for_monocolor_hybrid(player, get_number_of_monocolored_hybrid_mana(-1, iid, 0),
                                         c1, 0);
  }
#if 0
  else if (is_mixed_hybrid(cp))
	{
	  // I can't bring myself to care.
	}
#endif
  else if (is_artifact)
    return has_mana_multi_a(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green, cp->req.req_red,
                            cp->req.req_white);
  else
    return has_mana_multi(player, cless, cp->req.req_black, cp->req.req_blue, cp->req.req_green, cp->req.req_red,
                          cp->req.req_white);
}

int charge_mana_from_id(int player, int card, event_t event, int csvid)
{
  return charge_mana_from_id_with_cost_mod(player, card, event, csvid, 0);
}
#elif defined(USE_EXE_VERSION)
/* These are almost right, but incompatible with the way alternative costs are defined - which is to force mana cost to either 0 or infinite during cost
 * modification, then charge for real during EVENT_CAST_SPELL.  Which is unqueryable and unsustainable.  Not to mention that they then try to call *this* if you
 * choose to decline the alternative costs.
 *
 * The right way is to use cost modification only for *cost modification*, and then have something else that's called for alternative costs.
 *
 * Besides which, at least half of the calls to this are followed by play_spell_for_free().  Such calls should just put the card on the stack normally and
 * charge normally. */
int has_mana_to_cast_iid(int player, event_t event, int iid)
{
  int added = add_card_to_hand(player, iid);
  card_instance_t* inst = get_card_instance(player, added);
  inst->state |= STATE_INVISIBLE;
  --hand_count[player];
  int rval = can_pay_card_mana_cost(player, 0, added);
  obliterate_card_and_recycle(player, added);
  return rval;
}

//global _charge_mana_to_cast
//_charge_mana_to_cast:
//	push	ebp
//	mov	ebp, esp
//	push	edi
//	push	dword [ebp+0x10]	; card - param3
//	push	dword [ebp+0x0c]	; player - param2
//	mov	edi, [ebp+0x08]		; card_data_t* - param1
//	call	0x402680
//	add	esp, 2*4		; pop 2 params
//	pop	edi
//	leave
//	ret

int charge_mana_from_id(int player, int card, event_t event, int csvid)
{
  int iid = get_internal_card_id_from_csv_id(csvid);
  int added = add_card_to_hand(player, iid);
  card_instance_t* inst = get_card_instance(player, added);
  inst->state |= STATE_INVISIBLE;
  --hand_count[player];
  can_pay_card_mana_cost(player, 0, added);
  int rval = charge_mana_multi(player, COST_COLORLESS, COST_BLACK, COST_BLUE, COST_GREEN, COST_RED, COST_WHITE);
  obliterate_card(player, added);
  return rval;
}
#endif

int has_mana_to_cast_id(int player, event_t event, int csvid){
	return has_mana_to_cast_iid(player, event, get_internal_card_id_from_csv_id(csvid));
}

void remove_id_from_grave(int player, int csvid, remove_id_from_grave_t mode){
	const int *grave = get_grave(player);
	int count = count_graveyard(player)-1;
	while( count > -1 ){
			if( cards_data[grave[count]].id == csvid ){
				if( mode & RIFG_OBLITERATE ){
					remove_card_from_grave(player, count);
				}
				else{
					rfg_card_from_grave(player, count);
				}
				if( !(mode & RIFG_REMOVE_ALL) ){
					break;
				}
			}
			count--;
	}
}

int already_dead(int player, int card){
	if( check_special_flags(player, card, SF_LETHAL_DAMAGE_DESTROY) ||
		check_special_flags(player, card, SF_LETHAL_DAMAGE_BURY) ||
		check_special_flags(player, card, SF_LETHAL_DAMAGE_EXILE) ||
		get_card_instance(player, card)->damage_on_card >= get_toughness(player, card)
	  ){
		return 1;
	}
	return 0;
}

/* Returns the amount of life gained, which will always be at least 1.  EA_LICH must be set in event_flags for the trigger to be dispatched (so a card with
 * Flags: Lich set in ct_all.csv must have been on the battlefield this turn, or set it manually each turn during EVENT_CAN_SKIP_TURN). */

void exchange_life_totals(int player, int t_player)
{
  if (player == t_player)
  {
    return;
  }
  if (life[player] == life[t_player])
  {
    return;
  }

  if (!life_total_can_change(player) || !life_total_can_change(1 - player))
  {
    return;
  }

  if (life[player] > life[1 - player] && (player_bits[1 - player] & PB_CANT_GAIN_LIFE))
  {
    return;
  }
  if (life[1 - player] > life[player] && (player_bits[player] & PB_CANT_GAIN_LIFE))
  {
    return;
  }

  int life_totals[2];
  APNAP(p, {
    life_totals[p] = life[p];
    if (check_battlefield_for_id(p, CARD_ID_LICH))
    {
      life_totals[p] = 0;
    };
  };);
  APNAP(p, {
    if (life_totals[p] > life_totals[1 - p])
    {
      lose_life(p, life_totals[p] - life_totals[1 - p]);
    }
    else if (life_totals[p] < life_totals[1 - p])
    {
      gain_life(p, life_totals[1 - p] - life_totals[p]);
    };
  };);
}

void set_life_total(int player, int amount)
{
  if (life[player] == amount)
  {
    return;
  }

  if (!life_total_can_change(player))
  {
    return;
  }

  if (life[player] < amount && (player_bits[player] & PB_CANT_GAIN_LIFE))
  {
    return;
  }

  if (life[player] < amount)
  {
    int lg = amount - life[player];
    gain_life(player, lg);
  }
  else if (life[player] > amount)
  {
    int ll = life[player] - amount;
    lose_life(player, ll);
  }
}

void kill_my_legacy(int player, int card){
	int id = get_id(player, card);
	int i;
	for(i=0; i<2; i++){
		int count = active_cards_count[i]-1;
		while( count > -1 ){
				if( is_what(i, count, TYPE_EFFECT) ){
					card_instance_t *this = get_card_instance( i, count );
					if( this->targets[2].card == id ){
						kill_card(i, count, KILL_EXILE);
					}
				}
				count--;
		}
	}
}

int until_eot_legacy(int player, int card, event_t event){

	if( eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int can_block_me(int blocked_player, int blocked_card, int blocking_player, int blocking_card){
	return is_legal_block(blocking_player, blocking_card, blocked_player, blocked_card);
}

// Makes player/card block player_to_block/card_to_block.  Doesn't check legality.
void block(int player, int card, int player_to_block, int card_to_block)
{
  // This is inlined in generate_token(), since some of it happens before the token's put into play and some of it after.
  // Also inlined in human_assign_blockers, where STATE_UNKNOWN8000 isn't set.

  if (player == player_to_block)
	return;

  card_instance_t* to_block = get_card_instance(player_to_block, card_to_block);
  card_instance_t* blocker = get_card_instance(player, card);

  blocker->blocking = to_block->blocking == 255 ? card_to_block : to_block->blocking;
  blocker->state |= STATE_UNKNOWN8000|STATE_BLOCKING;

  play_sound_effect(WAV_BLOCK2);

  if (event_flags & EA_SELECT_BLOCK)
	dispatch_trigger2(current_turn, TRIGGER_BLOCKER_CHOSEN, gs_prompt_blockerselection_0/*PROMPT_BLOCKERSELECTION[0]*/, 0, player, card);
}

int player_flips_a_coin(int player, int card, int who_flips)
{
  int result = 0;
  int choice = (get_id(player, card) == CARD_ID_MANA_CLASH ||
                get_id(player, card) == CARD_ID_RAKDOS_THE_SHOWSTOPPER)
                   ? FC_TAILS
                   : FC_HEAD;
  if (get_id(player, card) != CARD_ID_RAL_ZAREK && get_id(player, card) != CARD_ID_MANA_CLASH &&
      get_id(player, card) != CARD_ID_PANDORAS_BOX &&
      get_id(player, card) != CARD_ID_RAKDOS_THE_SHOWSTOPPER)
  {
    choice = do_dialog(who_flips, player, card, -1, -1, " Heads\n Tails",
                       internal_rand(100) > 49 ? FC_TAILS : FC_HEAD);
  }
  int rounds = 2 * count_cards_by_id(player, CARD_ID_KRARKS_THUMB);
  if (!rounds)
  {
    rounds = 1;
  }
  char buffer[500];
  int count = 0;
  int cf[rounds];
  int pos = 0;
  while (count < rounds)
  {
    cf[count] = coin_flip(who_flips, "", 1);
    if (rounds > 1)
    {
      pos += scnprintf(buffer + pos, 500 - pos, " Choose");
      if (cf[count] == FC_HEAD)
      {
        pos += scnprintf(buffer + pos, 500 - pos, " Heads\n");
      }
      else
      {
        pos += scnprintf(buffer + pos, 500 - pos, " Tails\n");
      }
    }
    count++;
  }
  if (rounds == 1)
  {
    result = choice == cf[0];
  }
  else
  {
    int ai_choice = 0;
    count = 0;
    while (count < rounds)
    {
      if (cf[count] == choice)
      {
        ai_choice = count;
        break;
      }
      count++;
    }
    int choice2 = do_dialog(who_flips, player, card, -1, -1, buffer, ai_choice);
    if (choice == cf[choice2])
    {
      result = 1;
    }
  }
  if (get_id(player, card) != CARD_ID_RAL_ZAREK && get_id(player, card) != CARD_ID_MANA_CLASH &&
      get_id(player, card) != CARD_ID_PANDORAS_BOX &&
      get_id(player, card) != CARD_ID_RAKDOS_THE_SHOWSTOPPER)
    dispatch_event(who_flips, result, EVENT_COIN_FLIP_WON_OR_LOST);
  return result;
}

int flip_a_coin(int player, int card){
	return player_flips_a_coin(player, card, player);
}

/* Can be used with sorceries and with any target restrictions, but the AI can't fully understand the power/toughness modifications.
 * td only needs to be set if IS_CASTING(player, card, event). */

/* A more restricted version of vanilla_pump(), usable only for instants and only when there are no restrictions on targeting except for whose creature it can
 * target; but this lets us teach the AI about the power/toughness modifications. */

void shuffle_for_exe(int whose_computer_shuffles, int whose_library)
{
  duel_host_record_shuffle(whose_library);
  if (ai_is_speculating != 1)
	{
	  play_sound_effect(WAV_SHUFFLE);
	  notify_library_shuffled(whose_library);	// shuffle_animation()
	}

  if ((trace_mode & 2)
	  && whose_computer_shuffles != 0)
	{
	  receive_library_contents(whose_library);	// receive_deck_order_from_network()
	  return;
	}

  // Shuffle ourselves, since the function doing the actual reorder in Magic.exe has a nonstandard calling convention

  int i, sz = 0;
  // First, ensure there are no "gaps" of an invalid iid of -1.
  int scratch[500];
  for (i = 0; i < 500; ++i)
	if (deck_ptr[whose_library][i] != -1)
	  scratch[sz++] = deck_ptr[whose_library][i];
  // Initialize the remaining elements to -1
  for (i = sz; i < 500; ++i)
	scratch[i] = -1;

  // Fisher-Yates shuffle
  for (i = sz - 1; i >= 1; --i)
	{
	  int j = internal_rand(i + 1);	// This introduces bias, since internal_rand() is a cruddy prng that restricts its range using mod, but no worse than what was there before.
	  if (i != j)
		SWAP(scratch[i], scratch[j]);
	}

  // Put it back.
  memcpy(deck_ptr[whose_library], scratch, sizeof(scratch));

  // And tell the other computer.
  if (trace_mode & 2)
	send_library_contents(whose_library);	// send_deck_order_over_network()
}

void shuffle(int player)
{
  int graveyard_creatures = count_graveyard_by_type(player, 2);
  shuffle_for_exe(player, player);
  for (int p = 0; p < 2; ++p)
    for (int card = active_cards_count[p] - 1; card >= 0; --card)
    {
      if (!in_play(p, card) || !is_what(p, card, 0x47))
        continue;
      if (get_id(p, card) == CARD_ID_PSYCHOGENIC_PROBE)
        damage_player(player, 2, p, card);
      if (get_id(p, card) == CARD_ID_WIDESPREAD_PANIC && hand_count[player] > 0)
      {
        char msg[100] = "Select a card to put on top.";
        test_definition_t test;
        new_default_test_definition(&test, 0xff, msg);
        int selected_card = new_select_a_card(player, player, 4, 1, 3, -1, &test);
        put_on_top_of_deck(player, selected_card);
      }
      if (player != p && get_id(p, card) == CARD_ID_COSIS_TRICKSTER &&
          (duh_mode(p) || do_dialog(p, p, card, -1, -1, " Add +1/+1 counter\n Pass", 0) == 0))
        add_counters(p, card, p, card, COUNTER_P1_P1, 1);
    }
  if (graveyard_creatures)
    ereboss_titan_effect(1 - player);
}

int end_of_combat_trigger(int player, int card, event_t event, int trigger_mode)
{

  if (trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) &&
      reason_for_trigger_controller == player && !is_humiliated(player, card, AL_TRIG))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= trigger_mode;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      return 1;
    }
  }
  return 0;
}

int discard_trigger(int player, int card, event_t event, int t_player, int trigger_mode,
                    int discard_trigger_mode)
{

  if (trigger_condition == TRIGGER_DISCARD && affect_me(player, card) &&
      reason_for_trigger_controller == player && !is_humiliated(player, card, AL_TRIG))
  {
    if (t_player != 2 && t_player != trigger_cause_controller)
    {
      return 0;
    }
    if ((discard_trigger_mode & DISCARD_NO_NORMAL) && current_phase > PHASE_MAIN2)
    {
      return 0;
    }
    if ((discard_trigger_mode & DISCARD_STILL_IN_HAND) &&
        !in_hand(trigger_cause_controller, trigger_cause))
    {
      return 0;
    }

    if (event == EVENT_TRIGGER)
    {
      if (trigger_mode == RESOLVE_TRIGGER_DUH)
      {
        trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
      }
      event_result |= trigger_mode;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      return 1;
    }
  }

  return 0;
}

int lifegaining_charm(int player, int card, event_t event, int affected_player, int clr, int trigger_mode, int cless){

	if( specific_spell_played(player, card, event, affected_player, trigger_mode, TYPE_ANY, 0, 0, 0, clr, 0, 0, 0, -1, 0) ){
		charge_mana(player, COLOR_COLORLESS, cless);
		if( spell_fizzled != 1 ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int cip_lifegain(int player, int card, event_t event, int amount){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, amount);
	}

	return 0;
}

int damage_redirection(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	// The damage directed to this target...
	int s_player = instance->targets[0].player;
	int s_card = instance->targets[0].card;

	// ...Will be redirected to this target
	int t_player = instance->targets[1].player;
	int t_card = instance->targets[1].card;

	// If "instance->targets[2].player" is set to "1", the effect end at EOT


	if( event == EVENT_PREVENT_DAMAGE && (current_phase == PHASE_FIRST_STRIKE_DAMAGE ||
		current_phase == PHASE_NORMAL_COMBAT_DAMAGE)
	  ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == s_player &&
			damage->damage_target_card == s_card && damage->info_slot > 0
		  ){
			int good = 1;
			if( t_card != -1 && ! in_play(t_player, t_card) ){
				good = 0;
			}
			if( good == 1 ){
				damage->damage_target_player = t_player;
				damage->damage_target_card = t_card;
			}
		}
	}

	if( eot_trigger(player, card, event) && instance->targets[2].player == 1 ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int generic_x_spell(int player, int card, event_t event, int zone, int mana_color, int effect){
	// Bitfields for effect
	// 1 --> Damage target
	// 8 --> Gain life if target for other effects is valid (Consume Spirit)
	// 8192 --> Pump target creature -X/-X
	// 16384 --> Mill target player

	// Use mana_color = COLOR_COLORLESS if X can be paid with any kind of mana

	/* This is currently used only by card_blaze() and card_swallowing_plague(), and nothing is assigned to
	 * card_swallowing_plague(). */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = zone;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td) ){
			return ! check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){

		if (!is_token(player, card)){
			if( mana_color > 0 ){
				charge_mana(player, mana_color, -1);
				if( spell_fizzled != 1 ){
					instance->info_slot = x_value;
				}
			}
			else{
				instance->info_slot = x_value;
			}
		}

		if( spell_fizzled != 1 ){
			if( zone == TARGET_ZONE_IN_PLAY ){
				pick_target(&td, "TARGET_CREATURE");	// nope; currently means "target creature or planeswalker"
			}
			if( zone == TARGET_ZONE_PLAYERS ){
				pick_target(&td, "TARGET_PLAYER");
			}
			if( zone == TARGET_ZONE_CREATURE_OR_PLAYER ){
				pick_target(&td, "TARGET_ANY");
			}
			if( get_id(player, card) == CARD_ID_FANNING_THE_FLAMES && has_mana(player, COLOR_COLORLESS, 3) ){//unused
				int choice = do_dialog(player, player, card, player, card, " Buyback\n Pass", 0);
				if( choice == 0 ){
					charge_mana(player, COLOR_COLORLESS, 3);
					if( spell_fizzled != 1 ){
						instance->targets[8].card = get_id(player, card);;
					}
				}
			}
		}

		set_special_flags2(player, card, SF2_X_SPELL);
	}

	else if(event == EVENT_RESOLVE_SPELL ){
			int amt = instance->info_slot;

			if( valid_target(&td)  ){
				if( effect & 1 ){	// used by: Blaze/Volcanic Geyser, Swallowing Plague
					damage_creature_or_player(player, card, event, amt);
				}
				if( effect & 8 ){	// used by: Swallowing Plague
					gain_life(player, amt);
				}
				if( effect & 8192 ){	// used by: nothing
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -amt, -amt);
				}
				if( effect & 16384 ){	// used by: nothing
					mill(instance->targets[0].player, amt);
				}
			}

			if( instance->targets[8].card == CARD_ID_FANNING_THE_FLAMES ){	// unused
				bounce_permanent(player, card);
			}
			else{
				kill_card(player, card, KILL_DESTROY);
			}
	}
	return 0;
}

int steal_permanent_from_target_opponent_deck(int player, int card, event_t event, test_definition_t *this_test){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

static int timewalk_legacy(int player, int card, event_t event){

	card_instance_t *inst = get_card_instance( player, card );

	if (inst->token_status & STATUS_TIMEWALK)
    {
		if (event == EVENT_CAN_SKIP_TURN && time_walk_flag == -1)
			time_walk_flag = player;

/* This gets checked in the wrong order - EVENT_CLEANUP is set in order by card index, not timestamp,
	and the timewalk effect with the *newest* timestamp needs to be the one that takes precedence
*/
		if (event == EVENT_CLEANUP && !dword_4EF1B8)
        {
			dword_4EF1B8 |= 1 << player;
			inst->token_status &= ~STATUS_PERMANENT;

        }
    }

    if( !(inst->token_status & STATUS_PERMANENT) && (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) ){
		kill_card(player, card, KILL_EXILE);
    }

	return 0;
}

static int create_time_walk_legacy(int player, int card){
	if (time_walk_flag == -1){
		int p, c;
		for (p = 0; p < 2; ++p){
			for (c = 0; c < active_cards_count[p]; ++c){
				card_instance_t* instance = get_card_instance(p, c);
				if (instance->info_slot == (intptr_t)timewalk_legacy && (instance->token_status & STATUS_TIMEWALK)){
					goto found;
				}
			}
		}
		// not found:
		time_walk_flag = player;
	}
 found:;
	int legacy_card = create_legacy_effect(player, card, &timewalk_legacy);
	card_instance_t* legacy_inst = get_card_instance(player, legacy_card);
	legacy_inst->token_status |= STATUS_TIMEWALK | STATUS_PERMANENT;
	ai_modifier += (player == AI ? 96 : -96);

	return legacy_card;
}
/* For reference:
int card_time_walk(int player, int card, event_t event){
	if (event == EVENT_CAN_CAST){
		return 1;
	} else if (event == EVENT_CAST_SPELL){
		if (affect_me(player, card)){
			ai_modifier += 96;
		}
	} else if (event == EVENT_RESOLVE_SPELL){
		create_time_walk_legacy(player, card);
		kill_card(player, card, KILL_BURY);
	}
	return 0;
}
*/

static int check_for_extra_turn_suppression(int player){
	int i;
	for(i=0; i<2; i++){
		int c;
		for(c=0; c<active_cards_count[i]; c++){
			if( in_play(i, c) && ! is_humiliated(i, c, AL_STATIC) && get_card_instance(i, c)->kill_code == 0){
				int id = get_id(i, c);
				if( id == CARD_ID_UGINS_NEXUS ){
					return 1;
				}
				if( id == CARD_ID_STRANGLEHOLD && i != player ){
					return 1;
				}
			}
		}
	}
	return 0;
}

int time_walk_effect(int player, int card)
{
  if (check_for_extra_turn_suppression(player))
    return -1;
  return create_time_walk_legacy(player, card);
}

static int skip_next_turn_legacy(int player, int card, event_t event)
{
  /* Targets:
   * [0].player: player whose turn is to be skipped */
  if (event == EVENT_CAN_SKIP_TURN)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (current_turn == instance->targets[0].player && !(land_can_be_played & LCBP_SKIP_TURN))
		{
		  land_can_be_played |= LCBP_SKIP_TURN;
		  kill_card(player, card, KILL_EXILE);
		}
	}

  return 0;
}

int skip_next_turn(int player, int card, int t_player){
	int legacy = create_legacy_effect(player, card, &skip_next_turn_legacy);
	card_instance_t *instance = get_card_instance(player, legacy);
	instance->targets[0].player = t_player;
	return legacy;
}

// Returns true if the turn about to be started will be skipped instead.  Accurate only during EVENT_CAN_SKIP_TURN (the first event of each turn).
int this_turn_is_being_skipped(void)
{
  if (land_can_be_played & LCBP_SKIP_TURN)	// A skip-turn effect has already resolved.
	return 1;

  /* Look for a skip_next_turn_legacy targeting the current player.  The point of this is to find a skip-your-next-turn effect that will resolve this turn, but
   * hasn't yet (perhaps because you have a tapped Time Vault that gets checked before the effect card).  No need to check for other legacies if this is itself
   * being called from a legacy, though; we can just make the first one seen trigger.  Hence it just checks for LCBP_SKIP_TURN directly. */
  int p, c;
  for (p = 0; p <= 1; ++p)
	for (c = 0; c < active_cards_count[p]; ++c)
	  if (in_play(p, c))
		{
		  card_instance_t* inst = get_card_instance(p, c);
		  if (inst->internal_card_id == LEGACY_EFFECT_CUSTOM && inst->info_slot == (intptr_t)skip_next_turn_legacy && inst->targets[0].player == current_turn)
			return 1;
		}

  return 0;
}

enum
	{
		CPL_PAY_LIFE_FOR_SPELLS_OR_ACTIVATED_ABILITY = 1<<0,
	};

int can_pay_life_impl(int player, int amount, int flags)
{
  int result;
  result = life_total_can_change(player);

  if ((result != 0) &&
      (((flags & 1U) == 0 ||
        ((result = check_battlefield_for_id(2, 0x3010),
          result == 0 && (result = check_battlefield_for_id(2, CARD_ID_YASHARN_IMPLACABLE_EARTH), result == 0))))))
  {
    return (unsigned int)(amount <= life[player]);
  }
  return 0;
}

int can_pay_life_as_cost_for_spells_or_activated_abilities(int player, int amount){
	return can_pay_life_impl(player, amount, CPL_PAY_LIFE_FOR_SPELLS_OR_ACTIVATED_ABILITY);
}

int can_pay_life(int player, int amount){
	return can_pay_life_impl(player, amount, 0);
}

// Stores an internal count of attackers in (player,card)->targets[1].player; returns that amount.

// As declare_attackers_trigger, but attacking cards must also pass test.

int pick_target_nonbasic_land(int player, int card, int cannot_cancel)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.required_subtype = SUBTYPE_NONBASIC;
  td.allow_cancel = cannot_cancel ? 0 : 1;

  card_instance_t *instance = get_card_instance(player, card);

  instance->number_of_targets = 0;

  if (can_target(&td) && pick_target(&td, "TARGET_NONBASIC_LAND"))
	{
	  if (instance->targets[0].player == player)
		ai_modifier -= 128;

	  return 1;
	}

  cancel = 1;
  return 0;
}

extern int hack_xx;
// A generalization of charge_mana_for_double_x().  num_x==2 => charge XX.  num_x==3 => charge XXX.  etc.  Returns total mana spent, not what X is.

// Charges XX of color.  Returns total mana spent, not what X is.

void check_damage_test(int source_of_damage_player, int source_of_damage_card, event_t event, ddbm_flags_t mode,
						int triggering_player, int triggering_card, int type /*optional*/,test_definition_t *test/*optional*/)
{
	if (event == EVENT_DEAL_DAMAGE){
		card_instance_t* instance = get_card_instance(triggering_player, triggering_card);
		card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
		if (damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( source_of_damage_player != ANYBODY && damage->damage_source_player != source_of_damage_player ){
				return;
			}
			if( source_of_damage_card != -1 && damage->damage_source_card != source_of_damage_card ){
				return;
			}
			if( test != NULL ){
				if( in_play(damage->damage_source_player, damage->damage_source_card) ){
					if( ! new_make_test_in_play(damage->damage_source_player, damage->damage_source_card, -1, test) ){
						return;
					}
				}
				else{
					int iid = get_internal_card_id_from_csv_id(damage->display_pic_csv_id);
					if( ! new_make_test(source_of_damage_player, iid, -1, test) ){
						return;
					}
				}
			}
			if( type ){
				int dmg_type = damage->targets[3].player;
				if( in_play(damage->damage_source_player, damage->damage_source_card) ){
					dmg_type = get_type(damage->damage_source_player, damage->damage_source_card);
				}
				if( ! is_what(-1, dmg_type, type) ){
					return;
				}
			}

			if( damage_is_to_planeswalker(damage) ){
				if (!(mode & DDBM_MAY_DAMAGE_PLANESWALKER)){
					return;
				}
			}

			if (mode & (DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_DAMAGE_CREATURE | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER |
						DDBM_MUST_DAMAGE_OPPONENT) )
			{
				if( damage->damage_target_card == -1 ){
					if( mode & DDBM_MUST_DAMAGE_CREATURE ){
						return;
					}
					else{
						if( !(mode & (DDBM_MUST_DAMAGE_PLAYER | DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_MUST_DAMAGE_OPPONENT)) ){
							return;
						}
						if( (mode & DDBM_MUST_DAMAGE_TRIGGERING_PLAYER) && damage->damage_target_player != triggering_player ){
							return;
						}
						if( (mode & DDBM_MUST_DAMAGE_OPPONENT) && damage->damage_target_player != 1-triggering_player ){
							return;
						}
					}
				}
				else{
					if( !(mode & DDBM_MUST_DAMAGE_CREATURE) ){
						return;
					}
				}
			}

			if ((mode & DDBM_MUST_BE_COMBAT_DAMAGE) && !(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE))){
				return;
			}

			if (is_humiliated(triggering_player, triggering_card, AL_TRIG)){
				return;
			}

			if( mode & DDBM_TRACE_DAMAGING_SOURCES ){
				if( instance->info_slot < 0 ){
					instance->info_slot = 0;
				}
				int pos = instance->info_slot;
				if( pos < 10 ){
					instance->targets[pos].player = damage->damage_source_player;
					instance->targets[pos].card = damage->damage_source_card;
					instance->info_slot++;
				}
			}
			else{
				int count_pos = mode & DDBM_STORE_IN_TARGETS_9 ? 9 : 8;
				if (instance->targets[count_pos].player < 0){
					instance->targets[count_pos].player = 0;
				}
				++instance->targets[count_pos].player;
				if (mode & DDBM_TRACE_DAMAGED_PLAYERS){
					if(instance->targets[1].player == -1){
						instance->targets[1].player = 0;
					}
					if (damage->damage_target_player == 1){
						++SET_BYTE1(instance->targets[1].player);
					} else {
						++SET_BYTE0(instance->targets[1].player);
					}
				}

				if (mode & DDBM_REPORT_DAMAGE_DEALT){
					if (instance->targets[16].player < 0){
						instance->targets[16].player = 0;
					}
					if (mode & DDBM_TRACE_DAMAGED_PLAYERS){
						if (damage->damage_target_player == 1){
							SET_BYTE1(instance->targets[16].player) += damage->info_slot;
						}
						else{
							SET_BYTE0(instance->targets[16].player) += damage->info_slot;
						}
					}
					else{
						instance->targets[16].player += damage->info_slot;
					}
				}
			}
		}
	}
}

void check_damage(int source_of_damage_player, int source_of_damage_card, event_t event, ddbm_flags_t mode, int triggering_player, int triggering_card){
	check_damage_test(source_of_damage_player, source_of_damage_card, event, mode, triggering_player, triggering_card, -1, NULL);
}

int resolve_damage_trigger(int player, int card, event_t event, ddbm_flags_t mode,
                           int triggering_player, int triggering_card)
{
  // DDBM_TRACE_DAMAGING_SOURCES must be used here too if it's used in "check_damage_test" or this
  // either won't work or will glitch
  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(triggering_player, triggering_card) &&
      reason_for_trigger_controller == triggering_player)
  {

    card_instance_t *instance = get_card_instance(triggering_player, triggering_card);
    int count_pos = mode & DDBM_STORE_IN_TARGETS_9 ? 9 : 8;
    if (mode & DDBM_TRACE_DAMAGING_SOURCES)
    {
      if (instance->info_slot <= 0)
      {
        return 0;
      }
    }
    else
    {
      if (instance->targets[count_pos].player <= 0)
      {
        return 0;
      }
    }

    if (event == EVENT_TRIGGER)
    {
      if (mode & DDBM_TRIGGER_OPTIONAL)
      {
        event_result |= RESOLVE_TRIGGER_AI(triggering_player);
      }
      else
      {
        event_result |= RESOLVE_TRIGGER_MANDATORY;
      }
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      int packets_dealt = instance->targets[count_pos].player;
      if (mode & DDBM_TRACE_DAMAGING_SOURCES)
      {
        packets_dealt = instance->info_slot;
        instance->info_slot = 0;
      }
      else
      {
        instance->targets[count_pos].player = 0;
      }
      if (!(mode & DDBM_REPORT_DAMAGE_DEALT))
      {
        instance->targets[16].player = 0;
      }
      card_instance_t *source = get_card_instance(player, card);
      if ((int32_t)source->token_status < 0)
      {
        destroy_attached_auras_and_obliterate_card(player, card);
        obliterate_card(player, card);
      }
      return packets_dealt;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[16].player = 0;
      if (mode & DDBM_TRACE_DAMAGING_SOURCES)
      {
        instance->info_slot = 0;
      }
      else
      {
        instance->targets[count_pos].player = 0;
      }
      if (mode & DDBM_TRACE_DAMAGED_PLAYERS)
      {
        instance->targets[1].player = 0;
      }
    }
  }

  return 0;
}

/* Just like damage_dealt_by_me(), except can specify a different triggering_player and triggering_card that highlights for the trigger and which gets its
 * targets[1,8,16].player overwritten instead of player/card. */
int damage_dealt_by_me_arbitrary(int player, int card, event_t event, ddbm_flags_t mode,
                                 int triggering_player, int triggering_card)
{
  if ((mode & DDBM_NOT_ME) || (unsigned int)player >= 3 || (unsigned int)card >= 151)
    return 0;
  ASSERT(!(mode & DDBM_TRACE_DAMAGED_CREATURES) &&
         "unimplemented for damage_dealt_by_me_arbitrary");
  if (event == EVENT_DEAL_DAMAGE)
    check_damage_test(player, card, event, mode, triggering_player, triggering_card, -1, NULL);
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (affect_me(player, card) && check_battlefield_for_id(ANYBODY, cards_data[damage_card].id) &&
        (int32_t)get_card_instance(player, card)->token_status >= 0)
    {
      card_instance_t *instance = get_card_instance(player, card);
      int dead = add_card_to_hand(player, instance->backup_internal_card_id);
      --hand_count[player];
      card_instance_t *dead_inst = get_card_instance(player, dead);
      dead_inst->kill_code = 0;
      dead_inst->damage_on_card = 0;
      dead_inst->token_status = instance->token_status | 0x80000000U;
      dead_inst->damage_target_player = instance->damage_target_player;
      dead_inst->damage_target_card = instance->damage_target_card;
      memcpy(dead_inst->targets, instance->targets, 152);
      dead_inst->info_slot = get_card_instance(player, card)->info_slot;
      dead_inst->state |= STATE_IN_PLAY;
      for (int p = 0; p < 2; ++p)
        for (int c = 0; c < active_cards_count[p]; ++c)
        {
          card_instance_t *current = get_card_instance(p, c);
          if (current->internal_card_id == damage_card &&
              get_card_instance(p, c)->damage_source_player == player &&
              get_card_instance(p, c)->damage_source_card == card)
            get_card_instance(p, c)->damage_source_card = dead;
          current = get_card_instance(p, c);
          if (current->internal_card_id != damage_card &&
              get_card_instance(p, c)->damage_target_player == player &&
              get_card_instance(p, c)->damage_target_card == card)
          {
            if (has_subtype(p, c, SUBTYPE_AURA))
              attach_aura_to_target(p, c, EVENT_GRAVEYARD_FROM_PLAY, player, dead);
            else if (is_equipment(p, c))
            {
              // Native inline attachment path also writes targets[8]; it does
              // not dispatch the normal equip event or play the equip sound.
              unattach(p, c);
              card_instance_t *eq = get_card_instance(p, c);
              eq->damage_target_player = player;
              eq->damage_target_card = dead;
              eq->targets[8].player = player;
              eq->targets[8].card = dead;
              int leg = create_targetted_legacy_effect(p, c, equipment_legacy, player, dead);
              get_card_instance(p, leg)->token_status |= STATUS_INVISIBLE_FX;
              if (get_id(p, c) == CARD_ID_NIM_DEATHMANTLE)
                force_a_subtype(player, dead, SUBTYPE_ZOMBIE);
              if (get_id(p, c) == CARD_ID_ASSAULT_SUIT)
                set_special_flags2(player, dead, 0x400000);
            }
            else
              get_card_instance(p, c)->damage_target_card = dead;
          }
        }
    }
  }
  else if (event == EVENT_CHANGE_TYPE &&
           !check_battlefield_for_id(ANYBODY, cards_data[damage_card].id) &&
           (int32_t)get_card_instance(player, card)->token_status < 0)
  {
    destroy_attached_auras_and_obliterate_card(player, card);
    obliterate_card(player, card);
  }
  return resolve_damage_trigger(player, card, event, mode, triggering_player, triggering_card);
}

// Overwrites instance->targets[8].player, instance->targets[16].player, and for some modes instance->targets[1].player.
int damage_dealt_by_me(int player, int card, event_t event, ddbm_flags_t mode){
	return damage_dealt_by_me_arbitrary(player, card, event, mode, player, card);
}

// A common case.  Uses targets[1], [8], [16].player, as usual.  DDBM_MUST_DAMAGE_PLAYER and DDBM_TRACE_DAMAGED_PLAYERS are added to mode.
void whenever_i_deal_damage_to_a_player_he_discards_a_card(int player, int card, int event, ddbm_flags_t mode, int discard_is_random)
{
  if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | mode))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
	  instance->targets[1].player = 0;
	  new_multidiscard(0, times_damaged[0], discard_is_random, player);
	  new_multidiscard(1, times_damaged[1], discard_is_random, player);
	}
}

// A common case of damage_dealt_by_me_arbitrary(), this triggers on damage dealt by the card (not necessarily a creature) {player,card} is attached to.
int attached_creature_deals_damage(int player, int card, event_t event, ddbm_flags_t mode)
{
  card_instance_t *instance;
  if ((event == EVENT_DEAL_DAMAGE || event == EVENT_GRAVEYARD_FROM_PLAY ||
       trigger_condition == TRIGGER_DEAL_DAMAGE || event == EVENT_CHANGE_TYPE) &&
      player >= 0 && card >= 0 && (instance = in_play(player, card)))
    return damage_dealt_by_me_arbitrary(instance->damage_target_player,
                                        instance->damage_target_card, event, mode, player, card);
  return 0;
}

// A common case of damage_dealt_by_me_arbitrary(), this triggers on damage dealt by the card {player,card} is equipping.
int equipped_creature_deals_damage(int player, int card, event_t event, ddbm_flags_t mode)
{
  return attached_creature_deals_damage(player, card, event, mode | DDBM_STORE_IN_TARGETS_9);
}

/* Sets triggering_player/card's targets[7].player to number of times damaged and targets[7].card to total amount of damage.  Uses targets[6] internally.  Most
 * DDBM_* flags unimplemented. */
int damage_dealt_to_me_arbitrary(int player, int card, event_t event, ddbm_flags_t mode,
                                 int triggering_player, int triggering_card)
{
  ASSERT(!(mode & DDBM_MUST_DAMAGE_OPPONENT) && "unimplemented for damage_dealt_to_me_arbitrary");
  ASSERT(!(mode & DDBM_REPORT_DAMAGE_DEALT) && "unimplemented for damage_dealt_to_me_arbitrary");
  ASSERT(!(mode & DDBM_MUST_DAMAGE_PLAYER) && "unimplemented for damage_dealt_to_me_arbitrary");
  ASSERT(!(mode & DDBM_MUST_DAMAGE_CREATURE) && "unimplemented for damage_dealt_to_me_arbitrary");
  ASSERT(!(mode & DDBM_MAY_DAMAGE_PLANESWALKER) &&
         "unimplemented for damage_dealt_to_me_arbitrary");
  // Explicitly ignore DDBM_NOT_ME
  ASSERT(!(mode & DDBM_TRACE_DAMAGED_CREATURES) &&
         "unimplemented for damage_dealt_to_me_arbitrary");
  ASSERT(!(mode & DDBM_STORE_IN_TARGETS_9) && "unimplemented for damage_dealt_to_me_arbitrary");

  card_instance_t *damage = damage_being_dealt(event);
  if (damage)
  {
    if (card >= 0 && damage->damage_target_card != card)
      return 0;
    if ((unsigned int)player < 2 && damage->damage_target_player != player)
      return 0;
  }
  if (damage)
  {
    if ((mode & DDBM_MUST_BE_COMBAT_DAMAGE) &&
        !(damage->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)))
      return 0;

    card_instance_t *instance = get_card_instance(triggering_player, triggering_card);
    if (instance->targets[6].player < 0)
      instance->targets[6].player = 0;
    if (mode & DDBM_TRACE_DAMAGED_PLAYERS)
    {
      if (damage->damage_target_player == 0)
        ++SET_BYTE0(instance->targets[6].player);
      else
        ++SET_BYTE1(instance->targets[6].player);
    }
    else
      ++instance->targets[6].player;

    if (instance->targets[6].card < 0)
      instance->targets[6].card = damage->info_slot;
    else
      instance->targets[6].card += damage->info_slot;
  }

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(triggering_player, triggering_card) &&
      reason_for_trigger_controller == triggering_player &&
      !is_humiliated(triggering_player, triggering_card, AL_TRIG))
  {
    card_instance_t *instance = get_card_instance(triggering_player, triggering_card);
    if (instance->targets[6].player <= 0)
      return 0;

    if (event == EVENT_TRIGGER)
    {
      if (mode & DDBM_TRIGGER_OPTIONAL)
        event_result |= RESOLVE_TRIGGER_AI(triggering_player);
      else
        event_result |= RESOLVE_TRIGGER_MANDATORY;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (mode & DDBM_TRACE_DAMAGED_PLAYERS)
      {
        int result = instance->targets[6].player;
        instance->targets[6].player = instance->targets[6].card = 0;
        return result;
      }
      instance->targets[7] = instance->targets[6]; // struct copy
      instance->targets[6].player = 0;
      instance->targets[6].card = 0;
      return 1;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[6].player = 0;
      instance->targets[6].card = 0;
    }
  }

  return 0;
}

#if 0
/* Returns total damage dealt in instance->targets[1].card, but gets very confused if targets[16].player (for replaced damage) is set.  Number of times damaged
 * in instance->info_slot - 2, but maxes at 10.  Overwrites all targets [1]-[9], but uses [1].player internally and nothing looks at what it writes to [2]-[9].
 * Caller is expected to clear info_slot if it does inspect them.  Should therefore never be used unless it's the triggering card being damaged.  Also isn't
 * usable with any of the damage_dealt_by_me() variants on the same card.  Doesn't appear fit for use except for the bare fact of damage having been dealt;
 * deprecating. */
int damage_dealt_to_me(int player, int card, event_t event, int mode){
	// Bitfields for mode
	// 1 --> Optional trigger
	// 2 --> Mandatory trigger

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player ){
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

					if( good > 0 ){
						if( instance->info_slot < 2 ){
							instance->info_slot = 2;
						}
						if( instance->info_slot < 10 ){
							instance->targets[instance->info_slot].player = damage->damage_source_player;
							instance->targets[instance->info_slot].card = damage->damage_source_card;
							instance->info_slot++;
						}
						if( instance->targets[1].player < 0 ){
							instance->targets[1].player = 0;
						}
						instance->targets[1].player+=good;
					}
			}
		}
	}

	if( instance->targets[1].player > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
		reason_for_trigger_controller == player ){
		if(event == EVENT_TRIGGER){
			if( mode & 1 ){
				event_result |= 1+player;
			}
			else{
				event_result |= 2;
			}
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				instance->targets[1].card = instance->targets[1].player;
				instance->targets[1].player = 0;
				return 1;
		}
		else if(event == EVENT_END_TRIGGER){
			instance->targets[1].player = 0;
		}
	}

	return 0;
}
#endif

int sengir_vampire_trigger(int player, int card, event_t event, int trig_mode)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (is_humiliated(player, card, AL_TRIG))
  {
    return 0;
  }

  if (event == EVENT_DEAL_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->info_slot > 0)
    {
      if (damage->damage_source_card == card && damage->damage_source_player == player &&
          damage->damage_target_card != -1)
      {
        if (instance->targets[1].player < 10)
        {
          int good = 1;
          if (instance->targets[1].player > 2)
          {
            int i;
            for (i = 2; i < instance->targets[1].player; i++)
            {
              if (damage->damage_target_player == instance->targets[i].player &&
                  damage->damage_target_card == instance->targets[i].card)
              {
                good = 0;
                break;
              }
            }
          }
          if (good == 1)
          {
            if (instance->targets[1].player < 2)
            {
              instance->targets[1].player = 2;
            }
            int pos = instance->targets[1].player;
            instance->targets[pos].player = damage->damage_target_player;
            instance->targets[pos].card = damage->damage_target_card;
            instance->targets[1].player++;
          }
        }
      }
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (!in_play(affected_card_controller, affected_card))
    {
      return 0;
    }
    if (is_what(affected_card_controller, affected_card, TYPE_CREATURE))
    {
        if (is_this_dying(affected_card_controller, affected_card))
      {
        if (instance->targets[1].player > 2)
        {
          int i;
          for (i = 2; i < instance->targets[1].player; i++)
          {
            if (instance->targets[i].player == affected_card_controller &&
                instance->targets[i].card == affected_card &&
                !check_for_death_trigger_removal(affected_card_controller, affected_card))
            {
              if (instance->targets[11].player < 0)
              {
                instance->targets[11].player = 0;
              }
              instance->targets[11].player++;
              instance->targets[i].player =
                  get_owner(instance->targets[i].player, instance->targets[i].card);
              instance->targets[i].card = get_id(affected_card_controller, affected_card);
            }
          }
        }
      }
    }
  }

  if (instance->targets[11].player > 0 && trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY &&
      player == reason_for_trigger_controller && affect_me(player, card))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= trig_mode;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance->targets[11].card = instance->targets[11].player;
      instance->targets[11].player = 0;
      return 1;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[11].player = 0;
    }
  }

  if (current_phase == PHASE_UNTAP)
  {
    instance->targets[1].player = 2;
  }

  return 0;
}

int target_player_skips_untap(int player, int card, event_t event){
	// Skip-untap effect handled in engine.c:untap_phase()
	return 0;
}

int target_player_skips_next_untap(int player, int card, int t_player ){
	int legacy = create_legacy_effect(player, card, &target_player_skips_untap);
	card_instance_t *instance = get_card_instance(player, legacy);
	instance->targets[0].player = t_player;
	instance->targets[0].card = 1;
	return legacy;
}

/* Uses targets[9] to store the player/card it was attached to at the time it was activated.  Should be called *before* the EVENT_RESOLVE_ACTIVATION handler if
 * td is provided. */

/* Uses targets[9] to store the player/card it was attached to at the time it was activated and info_slot to store the choice made.  Returns 1 during
 * EVENT_RESOLVE_ACTIVATION if the activated ability was selected, 0 if it was equipped. */

#define ALLOW_REEQUIP 1

#ifndef ALLOW_REEQUIP
static const char* already_equipping(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_equipping(targeting_player, targeting_card))
	{
	  card_instance_t* instance = get_card_instance(targeting_player, targeting_card);
	  if (instance->targets[8].card == card && instance->targets[8].player == player)
		return "already equipping";
	}
  return NULL;
}
#endif

int hack_what_was_sacced_for_gaa = 0;	// valid only immediately after calling generic_activated_ability with event == EVENT_ACTIVATE.

int this_dies_trigger(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  return this_dies_trigger_impl(player, card, event, trigger_mode, player, 0, 0);
}

int attached_creature_dies_trigger(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  return this_dies_trigger_impl(player, card, event, trigger_mode, player, 1, 0);
}

int attached_creature_dies_trigger_for_controller(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  return this_dies_trigger_impl(player, card, event, trigger_mode, 3, 1, 0);
}

int pick_target_permanent(target_definition_t *td){
	int type = td->required_type;

	if( type == TYPE_CREATURE ){
		if( pick_target(td, "TARGET_CREATURE") ){
			return 1;
		}
	}
	else if( type == TYPE_ARTIFACT ){
		if( pick_target(td, "TARGET_ARTIFACT") ){
			return 1;
		}
	}

	else if( type == TYPE_ENCHANTMENT ){
		if( pick_target(td, "TARGET_ENCHANTMENT") ){
			return 1;
		}
	}

	else if( type == TYPE_LAND ){
		if( pick_target(td, "TARGET_LAND") ){
			return 1;
		}
	}
	else{
		if( pick_target(td, "TARGET_PERMANENT") ){
			return 1;
		}
	}
	return 0;
}

int bounce_permanent_at_upkeep(int player, int card, event_t event, target_definition_t *td){

	card_instance_t *instance = get_card_instance(player, card);

	if( current_turn == player && !is_humiliated(player, card, AL_TRIG) && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card)
	     && reason_for_trigger_controller == player ){
		if(event == EVENT_TRIGGER && can_target(td) ){
			event_result |= 2;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				int count = count_upkeeps(player);
				while( count > 0 && can_target(td) ){
						if( pick_target_permanent(td) ){
							instance->number_of_targets = 1;
							bounce_permanent(instance->targets[0].player, instance->targets[0].card);
						}
						count--;
				}
		}
	}

	return 0;
}

int card_from_list(int player, int mode, int type, int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4, int cc, int flag5){
	// bitflags for mode :
	// 1 --> must return a correct value
	// 2 --> Will return a CSVID
	// 4 --> Will return an Internal Card Id
	int result = -1;
	if( ai_is_speculating != 1 ){
			while( 1 ){
				int cfl = choose_a_card("Choose a card", -1, -1);
				if( make_test(player, cfl, -1, type, flag1, subtype, flag2, color, flag3, id, flag4, cc, flag5) ){
					if( mode & 2 ){
						result = cards_data[cfl].id;
						break;
					}
					if( mode & 4 ){
						result = cfl;
						break;
					}
				}
				else{
					if( !(mode & 1)  ){
						break;
					}
				}
			}
	}
	return result;
}

int id_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[2].player > 0 ){
		if( (instance->targets[2].player & 1) && eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}

int create_id_legacy(int player, int card, int t_player, int t_card, int mode){
	int legacy = -1;
	if( t_card != -1 ){
		legacy = create_targetted_legacy_effect(player, card, &id_legacy, t_player, t_card);
	}
	else{
		legacy = create_legacy_effect(player, card, &id_legacy);
	}
	card_instance_t *leg = get_card_instance(player, legacy);
	leg->targets[1].player = player;
	leg->targets[1].card = card;
	if( t_card == -1 ){
		leg->targets[0].player = t_player;
	}
	leg->targets[2].player = mode;
	leg->targets[2].card = get_id(player, card);
	leg->targets[3].card = get_id(player, card);
	return legacy;
}

int check_for_id_legacy(int player, int id){
	int i;
	for(i=0; i<2; i++){
		if( i == player || player == 2 ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_EFFECT) ){
						card_instance_t *instance = get_card_instance(i, count);
						if( instance->targets[2].card == id || instance->targets[3].card == id ){
							return count;
						}
					}
					count++;
			}
		}
	}
	return -1;
}

int exile_if_would_be_put_into_graveyard_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance( player, card );
	if( instance->targets[0].player > -1 ){
		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(instance->targets[0].player, instance->targets[0].card) ){
			card_instance_t *this = get_card_instance( instance->targets[0].player, instance->targets[0].card );
			if( this->kill_code > 0 && this->kill_code < KILL_EXILE ){
				this->kill_code = KILL_EXILE;
			}
		}
		if( instance->targets[1].player > 0 && (instance->targets[1].player & 1) && eot_trigger(player, card, event) ){
			remove_special_flags2(instance->targets[0].player, instance->targets[0].card, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE);
			kill_card(player, card, KILL_EXILE);
		}
	}
	return 0;
}

void exile_if_would_be_put_into_graveyard(int player, int card, int t_player, int t_card, int mode){
	if( t_player > -1 && t_card > -1 ){
		set_special_flags2(t_player, t_card, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE);
		int legacy = create_targetted_legacy_effect(player, card, &exile_if_would_be_put_into_graveyard_legacy, t_player, t_card);
		card_instance_t *instance = get_card_instance( player, legacy );
		instance->targets[1].player = mode;
	}
}

void null_casting_cost(int player, int card)
{
  card_ptr_t *c = cards_ptr[get_id(player, card)];
  COST_COLORLESS -= get_updated_casting_cost(player, card, -1, EVENT_MODIFY_COST, c->req.req_colorless);
  COST_BLACK -= c->req.req_black;
  COST_BLUE -= c->req.req_blue;
  COST_GREEN -= c->req.req_green;
  COST_RED -= c->req.req_red;
  COST_WHITE -= c->req.req_white;
}

void infinite_casting_cost(void){
	COST_COLORLESS = COST_BLACK = COST_BLUE = COST_GREEN = COST_RED = COST_WHITE = 100;
}

// source_player/source_card are the source of the effect.  If they're unknown, put the revealed player/card there and use -1,-1 for revealed_player, revealed_card.
int die_at_end_of_combat(int player, int card, event_t event){

	if( end_of_combat_trigger(player, card, event, 2) ){
		card_instance_t *instance = get_card_instance(player, card);
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

    if (trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) && reason_for_trigger_controller == player
		&& event == EVENT_END_TRIGGER)	// If trigger is countered, it doesn't repeat next combat
	  kill_card(player, card, KILL_EXILE);

	return 0;
}

int reveal_any_number_of_cards_of_selected_color(int player, int card, int selected_color){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.zone = TARGET_ZONE_HAND;
	td.illegal_abilities = 0;
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_color = selected_color;

	card_instance_t *instance = get_card_instance(player, card);


	int revealed = 0;
	int cards_array[ 500 ];
	int choice = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0);

	if( choice == 1){
		int stop = 0;
		while( can_target(&td) && stop != 1){
			   if( pick_target(&td, "TARGET_CARD") ){
				   if( valid_target(&td) ){
					   card_instance_t *rev_card = get_card_instance(player, instance->targets[0].card);
					   cards_array[revealed] = rev_card->internal_card_id;
					   instance->targets[revealed+1].card = instance->targets[0].card;
					   rev_card->state |= STATE_CANNOT_TARGET;
					   revealed++;
				   }
			   }
			   else{
					stop = 1;
			   }
		}

		if( revealed > 0){
			int i;
			for(i=0;i<revealed; i++){
				card_instance_t *crd = get_card_instance(player, instance->targets[i+1].card);
				crd->state &= ~STATE_CANNOT_TARGET;
			}
		}
	}

	if( choice == 0){
		int i;
		for(i=0;i<active_cards_count[player]; i++){
			if( in_hand(player, i) && ( get_color(player, i) & selected_color) ){
				card_instance_t *rev_card = get_card_instance(player, i);
				cards_array[revealed] = rev_card->internal_card_id;
				revealed++;
			}
		}
	}

	if( revealed > 0){
		show_deck( player, cards_array, revealed, "Cards revealed", 0, gs_done_008b40e0 );
	}

	return revealed;
}

// Identical to reveal_card_iid, but arbitrary text so you can pretend it's not a reveal
int reveal_cards_from_hand(int player, int card, test_definition_t *this_test){

	this_test->id = CARD_ID_RULES_ENGINE;
	this_test->id_flag = 1;

	int ha[100];
	int hac = 0;
	int rev[100];
	int revealed = 0;

	int count = 0;
	while( count < active_cards_count[player] ){
			if( in_hand(player, count) ){
				card_instance_t *crd = get_card_instance(player, count);
				ha[hac] = crd->internal_card_id;
				hac++;
			}
			count++;
	}
	if( hac > 0 ){
		int choice = 0;
		if( player == HUMAN ){
			choice = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0);
		}
		if( choice == 1){
			while( 1 ){
					int selected = show_deck( HUMAN, ha, hac, "Select a card to reveal.", 0, gs_done_008b40e0 );
					if( selected != -1 ){
						if( new_make_test(player, ha[selected], -1, this_test) ){
							rev[revealed] = ha[selected];
							revealed++;
							ha[selected] = get_internal_card_id_from_csv_id(CARD_ID_RULES_ENGINE);
						}
						else{
							break;
						}
					}
					else{
						break;
					}
			}
		}
		else{
			count = 0;
			while( count < hac ){
					if( new_make_test(player, ha[count], -1, this_test) ){
						rev[revealed] = ha[count];
						revealed++;
					}
					count++;
			}
		}
		if( revealed > 0){
			show_deck( HUMAN, rev, revealed, "Cards revealed", 0, gs_done_008b40e0 );
		}
	}
	return revealed;
}

// Gives the human a choice whether to reveal a card or not.  The AI always does.  Returns 1 if revealed.
void reveal_card(int source_player, int source_card, int revealed_player, int revealed_card)
{
  if (ai_is_speculating != 1)
	{
	  load_text(0, "REVEALS");
	  DIALOG(source_player, source_card, EVENT_ACTIVATE,
			 DLG_FULLCARD(source_player, source_card),
			 DLG_SMALLCARD(revealed_player, revealed_card),
			 DLG_MSG(text_lines[0]));
	  //DLG_WHO_CHOOSES(HUMAN),
	  //gs_ok_00924800, 1, 1);	//localized "OK"
	}
}

void reveal_card_iid(int source_player, int source_card, int revealed_iid)
{
  if (ai_is_speculating != 1 && revealed_iid != -1)
	{
	  load_text(0, "REVEALS");
	  DIALOG(source_player, source_card, EVENT_ACTIVATE,
			 DLG_FULLCARD(source_player, source_card),
			 DLG_SMALLCARD_ID(revealed_iid),
			 DLG_MSG(text_lines[0]));
	  //DLG_WHO_CHOOSES(HUMAN),
	  //gs_ok_00924800, 1, 1);	//localized "OK"
	}
}

void look_at_iid(int source_player, int source_card, int revealed_iid, const char* prompt)
{
  if (ai_is_speculating != 1 && revealed_iid != -1)
	DIALOG(source_player, source_card, EVENT_ACTIVATE,
		   DLG_FULLCARD(source_player, source_card),
		   DLG_SMALLCARD_ID(revealed_iid),
		   DLG_MSG(prompt));
		   //DLG_WHO_CHOOSES(HUMAN),
		   //gs_ok_00924800, 1, 1);	//localized "OK"
}

int reveal_card_optional_iid(int source_player, int source_card, int revealed_iid, const char* prompt/*optional*/)
{
  if (ai_is_speculating != 1 && revealed_iid != -1)
	{
	  if (source_player == AI)
		{
		  reveal_card_iid(source_player, source_card, revealed_iid);
		  return 1;
		}

	  return 1 == DIALOG(source_player, source_card, EVENT_ACTIVATE,
						 DLG_FULLCARD(source_player, source_card),
						 DLG_SMALLCARD_ID(revealed_iid),
						 DLG_NO_CANCEL,
						 prompt ? prompt : "Reveal", 1, 1,
						 "Pass", 1, 1);
	}
  else
	return 0;
}

int evincars(int player, int card, event_t event, color_test_t color)
{
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && !affect_me(player, card)
	  && in_play(player, card) && in_play(affected_card_controller, affected_card)
	  && !is_humiliated(player, card, AL_STATIC))
	{
	  if (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, color))
		++event_result;
	  else
		--event_result;
	}

  return 0;
}

void set_pt_and_abilities(int player, int card, event_t event, int pow, int tou, int key, int s_key)
{
  if (event == EVENT_POWER)
  {
    if (affect_me(player, card))
      set_base_pt(pow);
  }
  else if (event == EVENT_TOUGHNESS)
  {
    if (affect_me(player, card))
      set_base_pt(tou);
  }
  else if (event == EVENT_ABILITIES && affect_me(player, card))
    event_result |= key;
  if (s_key > 0)
    special_abilities(player, card, event, s_key, player, card);
}

int get_random_card_in_hand(int player){
	if( hand_count[player] <= 0 ){
		return -1;
	}

	if ((trace_mode & 2) && current_turn == 1){
		TENTATIVE_wait_for_network_result(1, 24);	// receive_network_packet(1, 24)
		return *shandalar_network_result();
	}

	int hnd[151], hnd_count = 0, c;
	hnd[0] = -1;	// in case hand_count[] is wrong and none are found
	for (c = 0; c < active_cards_count[player]; ++c){
		if (in_hand(player, c)){
			hnd[hnd_count++] = c;
		}
	}

	int rnd = hnd_count > 1 ? internal_rand(hnd_count) : 0;

	if (trace_mode & 2){	// current_turn will be 0
		*shandalar_network_result() = hnd[rnd];
		*shandalar_network_packet_type() = 24;
		TENTATIVE_send_network_result(0, 24);	// send_network_packet(0, 24);
	}

	return hnd[rnd];
}

int can_cast_creature_from_grave(int player, int mode, event_t event){
	const int *grave = get_grave(player);
	if( grave[0] == -1 ){
		if( mode == 1 ){
			return 0;
		}
		if( mode == 2 ){
			return -1;
		}
	}
	int count = 0;
	int count_g = count_graveyard(player);
	int max_value = 0;
	int result = 0;
	if( mode == 2 ){
		result = -1;
	}

	while( count < count_g ){
			if( is_what(-1, grave[count], TYPE_CREATURE) ){
				if( !(mode & 4) ||
					((mode & 4) && ((cards_data[grave[count]].color & COLOR_TEST_BLACK) || (cards_data[grave[count]].color & COLOR_TEST_BLUE) ||
									(cards_data[grave[count]].color & COLOR_TEST_RED)))
				  ){
					int id = cards_data[grave[count]].id;
					card_ptr_t* c = cards_ptr[ id ];
					if( has_mana_to_cast_id(player, event, id) ){
						if( mode == 1 ){
							result = 1;
							break;
						}
						if( c->ai_base_value > max_value ){
							max_value = c->ai_base_value;
							result = count;
						}
					}
				}
			}
			count++;
	}
	return result;
}

static int creatures_cannot_block_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_BLOCK_LEGALITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		if( instance->targets[0].player > 0 ){
			this_test.subtype = instance->targets[0].player;
		}
		if( instance->targets[0].card > -1 ){
			this_test.subtype_flag = instance->targets[0].card;
		}
		if( instance->targets[1].player > -1 ){
			this_test.color = instance->targets[1].player;
		}
		if( instance->targets[1].card > -1 ){
			this_test.color_flag = instance->targets[1].card;
		}
		if( instance->targets[2].player > 0 ){
			this_test.id = instance->targets[2].player;
		}
		if( instance->targets[2].card > -1 ){
			this_test.id_flag = instance->targets[2].card;
		}
		if( instance->targets[3].player > -1 ){
			this_test.cmc = instance->targets[3].player;
		}
		if( instance->targets[3].card > -1 ){
			this_test.cmc_flag = instance->targets[3].card;
		}
		if( instance->targets[4].player > 0 ){
			this_test.keyword = instance->targets[4].player;
		}
		if( instance->targets[4].card > -1 ){
			this_test.keyword_flag = instance->targets[4].card;
		}
		if( instance->targets[5].player > -1 ){
			this_test.power = instance->targets[5].player;
		}
		if( instance->targets[5].card > -1 ){
			this_test.power_flag = instance->targets[5].card;
		}
		if( instance->targets[6].player > -1 ){
			this_test.toughness = instance->targets[6].player;
		}
		if( instance->targets[6].card > -1 ){
			this_test.toughness_flag = instance->targets[6].card;
		}
		if( instance->targets[7].player > 0 ){
			this_test.state = instance->targets[7].player;
		}
		if( instance->targets[7].card > -1 ){
			this_test.state_flag = instance->targets[7].card;
		}
		if( new_make_test_in_play(affected_card_controller, affected_card, -1, &this_test) ){
			event_result = 1;
		}
	}

	if( instance->targets[8].player > 0 && (instance->targets[8].player & 1) && event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int creatures_cannot_block(int player, int card, test_definition_t *this_test, int mode){
	int legacy = create_legacy_effect(player, card, &creatures_cannot_block_legacy);
	card_instance_t *instance = get_card_instance(player, legacy);
	if( this_test != NULL ){
		instance->targets[0].player = this_test->subtype;
		instance->targets[0].card = this_test->subtype_flag;
		instance->targets[1].player = this_test->color;
		instance->targets[1].card = this_test->color_flag;
		instance->targets[2].player = this_test->id;
		instance->targets[2].card = this_test->id_flag;
		instance->targets[3].player = this_test->cmc;
		instance->targets[3].card = this_test->cmc_flag;
		instance->targets[4].player = this_test->keyword;
		instance->targets[4].card = this_test->keyword_flag;
		instance->targets[5].player = this_test->power;
		instance->targets[5].card = this_test->power_flag;
		instance->targets[6].player = this_test->toughness;
		instance->targets[6].card = this_test->toughness_flag;
		instance->targets[7].player = this_test->state;
		instance->targets[7].card = this_test->state_flag;
	}
	instance->targets[8].player = mode;
	return legacy;
}

static int effect_creature1_cant_block_creature2_until_eot(int player, int card, event_t event)
{
  if (event == EVENT_BLOCK_LEGALITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->damage_target_player, instance->damage_target_card)
		  && attacking_card_controller == instance->targets[0].player && attacking_card == instance->targets[0].card)
		event_result = 1;
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

void creature1_cant_block_creature2_until_eot(int src_player, int src_card, int t_player, int t_card, int unblockable_player, int unblockable_card)
{
  int leg = create_targetted_legacy_effect(src_player, src_card, effect_creature1_cant_block_creature2_until_eot, t_player, t_card);
  if (leg >= 0)
	{
	  card_instance_t* legacy = get_card_instance(src_player, leg);
	  legacy->targets[0].player = unblockable_player;
	  legacy->targets[0].card = unblockable_card;
	  legacy->number_of_targets = 1;
	}
}

int can_cast_spell_from_grave(int player, int mode){
	const int *grave = get_grave(player);
	if( grave[0] == -1 ){
		if( mode == 1 ){
			return 0;
		}
		if( mode == 2 ){
			return -1;
		}
	}
	int count = 0;
	int count_g = count_graveyard(player);
	int max_value = 0;
	int result = 0;
	if( mode == 2 ){
		result = -1;
	}

	while( count < count_g ){
			if( (is_what(-1, grave[count], TYPE_SORCERY) ||
				is_what(-1, grave[count], TYPE_INSTANT) ||
				is_what(-1, grave[count], TYPE_INTERRUPT)) &&
				! is_what(-1, grave[count], TYPE_CREATURE)
			  ){
				int id = cards_data[grave[count]].id;
				card_ptr_t* c = cards_ptr[ id ];
				if( has_mana_multi(player, c->req.req_colorless, c->req.req_black, c->req.req_blue, c->req.req_green,
					c->req.req_red, c->req.req_white)
				  ){
					if( can_legally_play_iid(player, grave[count]) ){
						if( mode == 1 ){
							result = 1;
							break;
						}
						if( c->ai_base_value > max_value ){
							max_value = c->ai_base_value;
							result = count;
						}
					}
				}
			}
			count++;
	}
	return result;
}

int get_owner(int player, int card)
{
  return check_state(player, card, STATE_OWNED_BY_OPPONENT) ? 1 : 0;
}

/* Flags an effect card to use alternate text, set off with |#1, |#2, etc. tags in its text.
 * Works only on csvid 903 effects (which includes both LEGACY_EFFECT_CUSTOM and LEGACY_EFFECT_GENERIC) other than LEGACY_EFFECT_PUMP.  So it's nonfunctional
 * with create_card_name_legacy().
 * Returns legacycard again. */

// Sets {player,legacycard}'s image (and therefore title and text) to be csvid.  Works only on effect cards.  Returns legacycard again.
/* Same as this_dies_trigger(), but the card's owner, not its controller, gets the legacy effect if card is stolen as it dies.  In particular, when this returns
 * true, player will be the owner since it's the legacy effect, not the original card, at that point. */
int this_dies_trigger_for_owner(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  return this_dies_trigger_impl(player, card, event, trigger_mode, get_owner(player, card), 0, 0);
}

int twincast(int player, int card, int event, target_definition_t *td /*optional*/,
             int *ret_copy /*optional*/)
{
  target_definition_t td_default;
  if (!td)
  {
    td = &td_default;
    counterspell_target_definition(player, card, td, TYPE_INSTANT | TYPE_SORCERY);
    td->preferred_controller = 2;
  }

  if (event == EVENT_CAN_CAST || event == EVENT_CAST_SPELL || event == EVENT_CAN_CHANGE_TARGET ||
      event == EVENT_CHANGE_TARGET)
    return counterspell(player, card, event, td, 0);

  if (event == EVENT_RESOLVE_SPELL)
  {
    if (counterspell_validate(player, card, td, 0))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int copy =
          copy_spell_from_stack(player, instance->targets[0].player, instance->targets[0].card);
      if (ret_copy)
        *ret_copy = copy;
    }
  }

  return 0;
}

int activate_twincast(int player, int card, int event, target_definition_t* td /*optional*/, int* ret_copy /*optional*/)
{
  if (event == EVENT_CAN_ACTIVATE)
	return twincast(player, card, EVENT_CAN_CAST, td, ret_copy);

  if (event == EVENT_ACTIVATE)
	return twincast(player, card, EVENT_CAST_SPELL, td, ret_copy);

  if (event == EVENT_RESOLVE_ACTIVATION)
	return twincast(player, card, EVENT_RESOLVE_SPELL, td, ret_copy);

  return 0;
}

void get_an_additional_combat_phase(void){
	current_phase = PHASE_DECLARE_ATTACKERS;
	land_can_be_played &= ~TENTATIVE_LCBP_DURING_COMBAT;
}

int set_legacy_image(int player, int csvid, int legacycard)
{
  if (legacycard >= 0)
	{
	  card_instance_t* leg = get_card_instance(player, legacycard);
	  leg->display_pic_csv_id = csvid;
	  leg->display_pic_num = get_card_image_number(csvid, leg->damage_source_player, leg->damage_source_card);
	}
  return legacycard;
}

void set_x_for_x_spells(int player, int card, event_t event, int value){
	card_instance_t *instance = get_card_instance(player, card);
	int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
	instance->info_slot = value;
	if( diff < 0 ){
		instance->info_slot -= diff;
	}
}

// Untap all cards with type types controlled by player during his opponent's untap step.  Stores temporary data in where_to_store.

int basic_spell(int player, int card, event_t event){
	//A spell that has no restriction such as targets, life to pay and so on, always returning 1 in EVENT_CAN_CAST and doing absolutely nothing in EVENT_CAST_SPELL
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

void fight(int p1, int c1, int p2, int c2){
	/* 701.10. Fight
	 *
	 * 701.10a A spell or ability may instruct a creature to fight another creature or it may instruct two creatures to fight each other. Each of those
	 * creatures deals damage equal to its power to the other creature.
	 *
	 * 701.10b If a creature instructed to fight is no longer on the battlefield or is no longer a creature, no damage is dealt. If a creature is an illegal
	 * target for a resolving spell or ability that instructs it to fight, no damage is dealt.
	 *
	 * 701.10c If a creature fights itself, it deals damage equal to its power to itself twice.
	 *
	 * 701.10d The damage dealt when a creature fights isn't combat damage. */

	card_instance_t* i1 = in_play(p1, c1);
	if (!i1 || !is_what(p1, c1, TYPE_CREATURE)){
		return;
	}
	card_instance_t* i2 = in_play(p2, c2);
	if (!i2 || !is_what(p2, c2, TYPE_CREATURE)){
		return;
	}

	i1->regen_status |= KEYWORD_RECALC_POWER;
	int pow1 = get_abilities(p1, c1, EVENT_POWER, -1);

	i2->regen_status |= KEYWORD_RECALC_POWER;
	int pow2 = get_abilities(p2, c2, EVENT_POWER, -1);

	play_sound_effect(WAV_ASWANJAG);

	damage_creature(p2, c2, pow1, p1, c1);
	damage_creature(p1, c1, pow2, p2, c2);

	dispatch_event(p1, c1, EVENT_FIGHT);
	dispatch_event(p2, c2, EVENT_FIGHT);
}

void upkeep_trigger_ability_mode(int player, int card, event_t event, int whose_upkeep, resolve_trigger_t trigger_mode){
	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && reason_for_trigger_controller == player &&
		(whose_upkeep == current_turn || whose_upkeep == 2) &&
		!is_humiliated(player, card, AL_TRIG)
	  ){
		int count = count_upkeeps(current_turn);
		if(event == EVENT_TRIGGER && count > 0){
			if (trigger_mode == RESOLVE_TRIGGER_DUH){
				trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
			}
			event_result |= trigger_mode;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			while( count > 0 && call_card_function(player, card, EVENT_UPKEEP_TRIGGER_ABILITY) != -1){
				count--;
			}
		}
	}
}

void upkeep_trigger_ability(int player, int card, event_t event, int whose_upkeep){
	upkeep_trigger_ability_mode(player, card, event, whose_upkeep, RESOLVE_TRIGGER_MANDATORY);
}

// controller of attached creature, not of {player,card}, gets the trigger
void set_flags_when_spell_is_countered(int player, int card, int c_player, int c_card){
	if( is_what(c_player, c_card, TYPE_CREATURE) && player != c_player ){
		increase_trap_condition(c_player, TRAP_SUMMONING_TRAP, 1);
	}
}

// exiledby.c is more flexible
int exile_permanent_and_remember_it(int player, int card, int t_player, int t_card, int store_loc){
	card_instance_t *instance = get_card_instance( player, card );
	int result = -1;
	if( instance->targets[store_loc].card < store_loc+1 ){
		instance->targets[store_loc].card = store_loc+1;
	}
	int max_pos = is_what(player, card, TYPE_EFFECT) ? 18 : 10;
	int pos = instance->targets[store_loc].card;
	if( pos < max_pos && ! is_token(t_player, t_card) ){
		instance->targets[pos].player = is_stolen(t_player, t_card) ? 1-t_player : t_player;
		instance->targets[pos].card = get_original_internal_card_id(t_player, t_card);
		if( max_pos == 10 ){
			create_card_name_legacy(player, card, get_id(t_player, t_card));
		}
		instance->targets[store_loc].card++;
		result = instance->targets[pos].card;
	}
	kill_card(t_player, t_card, KILL_EXILE);
	return result;
}

/* Translates type into text, writing at most maxlen characters into buf (including terminating NUL).  Returns buf. */

// Resolve a Twiddle-like effect.

// Change ai_modifier for a Twiddle-like effect.  Should happen in EVENT_ACTIVATE or EVENT_CAST_SPELL after targetting.

int get_power(int player, int card)
{
	return get_abilities(player, card, EVENT_POWER, -1);
}

int get_toughness(int player, int card)
{
	return get_abilities(player, card, EVENT_TOUGHNESS, -1);
}

static event_t hack_current_event = 0;	// There's a couple exe variables that look like they always hold the current event (or maybe trigger), but I'm not 100% certain of any of them.

// See effect_werewolf_ransacker() in dark_ascension.c for intended usage.
/* ManalinkEh 2022: 0x26774f0; original functions/functions.c:629. */
static card_instance_t *find_obliviation_legacy(int player, int card, int *out_player,
                                                int *out_card, int *out_iid, int *out_owner)
{
  card_instance_t *instance;
  int card_00;
  int player_00;

  player_00 = 0;
  do
  {
    card_00 = 0;
    if (0 < active_cards_count[player_00])
    {
      do
      {
        instance = get_card_instance(player_00, card_00);
        if ((((instance != NULL) && (instance->internal_card_id == LEGACY_EFFECT_ASWAN)) &&
             (instance->damage_target_card == card)) &&
            (((instance->damage_target_player == player && (-1 < instance->targets[7].card)) &&
              (-1 < instance->targets[7].player))))
        {
          if (out_player != NULL)
          {
            *out_player = player_00;
          }
          if (out_card != NULL)
          {
            *out_card = card_00;
          }
          if (out_iid != NULL)
          {
            *out_iid = instance->targets[7].card;
          }
          *out_owner = instance->targets[7].player;
          return instance;
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
    if (player_00 == 2)
    {
      return NULL;
    }
  } while (true);
}

// If test is NULL, then it defaults to all non-token cards.  Only needs to be called (and thus test only needs to be set) during EVENT_GRAVEYARD_FROM_PLAY.

/* ManalinkEh 2022: 0x2679a60; original functions/functions.c:691. */
int owner_of_obliviated_card(int player, int card)
{
  int owner;

  owner = -1;
  find_obliviation_legacy(player, card, NULL, NULL, NULL, &owner);
  return owner;
}

/* ManalinkEh 2022: 0x2682020; original functions/functions.c:717. */
int check_for_death_trigger_removal(int player, int card)
{
  if (is_what(player, card, TYPE_CREATURE) &&
      (get_card_instance(player, card)->kill_code < 4 ||
       get_card_instance(player, card)->kill_code == '\x11'))
  {
    for (int i = 0; i < 2; ++i)
    {
      for (int k = 0; k < active_cards_count[i]; ++k)
      {
        if (in_play(i, k) && !is_humiliated(i, k, AL_STATIC) &&
            get_id(i, k) == CARD_ID_HUSHBRINGER)
          return 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2679d40; original functions/functions.c:1048. */
int get_special_flags(int player, int card, int type_of_special_flag)
{
  card_instance_t *instance = get_card_instance(player, card);
  switch (type_of_special_flag)
  {
    case 1: return instance->ext->special_flags;
    case 2: return instance->ext->special_flags2;
    case 3: return instance->ext->special_flags3;
    default: return -1;
  }
}

/* ManalinkEh 2022: 0x2679db0; original functions/functions.c:1062. */
int get_special_flags_by_instance(card_instance_t *instance)
{
  return instance->ext->special_flags;
}

/* ManalinkEh 2022: 0x2679dd0; original functions/functions.c:1066. */
int check_special_flags(int player, int card, int value)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  return value & instance->ext->special_flags;
}

/* ManalinkEh 2022: 0x2679e00; original functions/functions.c:1071. */
void set_special_flags(int player, int card, int value)
{
  card_instance_t *instance;
  int player_00;
  int card_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags = instance->ext->special_flags | value;
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags = instance->ext->special_flags | value;
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267a050; original functions/functions.c:1093. */
void remove_special_flags(int player, int card, int value)
{
  card_instance_t *instance;
  int card_00;
  int player_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags = instance->ext->special_flags & (~(uint32_t)value);
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags = instance->ext->special_flags & (~(uint32_t)value);
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267a110; original functions/functions.c:1115. */
int get_special_flags2_by_instance(card_instance_t *instance)
{
  return instance->ext->special_flags2;
}

/* ManalinkEh 2022: 0x267a130; original functions/functions.c:1119. */
int check_special_flags2(int player, int card, int value)
{
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  return value & instance->ext->special_flags2;
}

/* ManalinkEh 2022: 0x267a160; original functions/functions.c:1123. */
void set_special_flags2(int player, int card, int value)
{
  card_instance_t *instance;
  int player_00;
  int card_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags2 = instance->ext->special_flags2 | value;
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags2 = instance->ext->special_flags2 | value;
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267a210; original functions/functions.c:1145. */
void remove_special_flags2(int player, int card, int value)
{
  card_instance_t *instance;
  int card_00;
  int player_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags2 = instance->ext->special_flags2 & (~(uint32_t)value);
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags2 = instance->ext->special_flags2 & (~(uint32_t)value);
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267a2d0; original functions/functions.c:1167. */
int get_special_flags3_by_instance(card_instance_t *instance)
{
  return instance->ext->special_flags3;
}

/* ManalinkEh 2022: 0x267a2f0; original functions/functions.c:1171. */
int check_special_flags3(int player, int card, int value)
{
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  return value & instance->ext->special_flags3;
}

int tutor(int, int, int (*func_ptr)(int, int),  int);	// deprecated
/* ManalinkEh 2022: 0x267a320; original functions/functions.c:1175. */
void set_special_flags3(int player, int card, int value)
{
  card_instance_t *instance;
  int player_00;
  int card_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags3 = instance->ext->special_flags3 | value;
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags3 = instance->ext->special_flags3 | value;
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267a510; original functions/functions.c:1197. */
void remove_special_flags3(int player, int card, int value)
{
  card_instance_t *instance;
  int card_00;
  int player_00;
  if (card != -1)
  {
    instance = get_card_instance(player, card);
    instance->ext->special_flags3 = instance->ext->special_flags3 & (value ^ ~0U);
    return;
  }
  player_00 = 0;
  do
  {
    if (((player == player_00) || (player == 2)) &&
        (card_00 = active_cards_count[player_00] + -1, -1 < card_00))
    {
      do
      {
        instance = in_play(player_00, card_00);
        if (instance != NULL)
        {
          instance = get_card_instance(player_00, card_00);
          instance->ext->special_flags3 = instance->ext->special_flags3 & (value ^ ~0U);
        }
        card_00 = card_00 + -1;
      } while (card_00 != -1);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* ManalinkEh 2022: 0x267b020; original functions/functions.c:2027. */
int find_legacy_effect(int player, int card, CardFunction func_ptr)
{
  card_instance_t *instance;
  int card_00;

  if (0 < active_cards_count[player])
  {
    card_00 = 0;
    do
    {
      while ((((instance = in_play(player, card_00),
                instance == NULL || (instance->internal_card_id != LEGACY_EFFECT_CUSTOM)) ||
               (func_ptr != (CardFunction)instance->info_slot)) ||
              (instance->damage_source_player != player)))
      {
        card_00 = card_00 + 1;
        if (active_cards_count[player] <= card_00)
        {
          return -1;
        }
      }
      if (instance->damage_source_card == card)
      {
        return card_00;
      }
      card_00 = card_00 + 1;
    } while (card_00 < active_cards_count[player]);
  }
  return -1;
}

/* ManalinkEh 2022: 0x267b5a0; original functions/functions.c:2135. */
int create_legacy_activate_for_opponent(int player, int card, CardFunction func_ptr, int t_player,
                                        int t_card)
{
  card_instance_t *instance;
  int result;
  card_instance_t *argument2;
  int legacy;
  int opponent;

  instance = get_card_instance(player, card);
  opponent = 1 - player;
  result = add_card_to_hand(opponent, instance->internal_card_id);
  if (result == -1)
  {
    backtrace_and_abort("functions/functions.c:2138\ncard_added != -1");
  }
  instance = get_card_instance(opponent, result);
  instance->state = instance->state & 0xffffffdd | 0x20;
  hand_count[opponent] = hand_count[opponent] + -1;
  argument2 = get_card_instance(player, card);
  memcpy_inst_and_ext(instance, argument2);
  instance->state = instance->state & 0xffffffdd | 0x20;
  legacy = create_legacy_activate(opponent, result, func_ptr);
  obliterate_card(opponent, result);
  instance = get_card_instance(opponent, legacy);
  instance->damage_source_player = (int8_t)player;
  instance->damage_source_card = card;
  result = get_card_image_number((unsigned int)instance->display_pic_csv_id, player, card);
  instance->display_pic_num = (uint16_t)result;
  if (-1 < t_player)
  {
    instance->number_of_targets = '\x01';
    instance->targets[0].player = t_player;
    instance->targets[0].card = t_card;
  }
  return legacy;
}

/* ManalinkEh 2022: 0x267c470; original functions/functions.c:3170. */
int split_cards_id_table(int csvid, int half_played)
{
  if (half_played == 0)
    return -1;

  switch (csvid)
  {
  case CARD_ID_ASSAULT_BATTERY:
    return (half_played & 1U) ? CARD_ID_ASSAULT : CARD_ID_BATTERY;
  case CARD_ID_PAIN_SUFFERING:
    return (half_played & 1U) ? CARD_ID_PAIN : CARD_ID_SUFFERING;
  case CARD_ID_SPITE_MALICE:
    return (half_played & 1U) ? CARD_ID_SPITE : CARD_ID_MALICE;
  case CARD_ID_STAND_DELIVER:
    return (half_played & 1U) ? CARD_ID_STAND : CARD_ID_DELIVER;
  case CARD_ID_WAX_WANE:
    return (half_played & 1U) ? CARD_ID_WAX : CARD_ID_WANE;
  case CARD_ID_FIRE_ICE:
    return (half_played & 1U) ? CARD_ID_FIRE : CARD_ID_ICE;
  case CARD_ID_ILLUSION_REALITY:
    return (half_played & 1U) ? CARD_ID_ILLUSION : CARD_ID_REALITY;
  case CARD_ID_LIFE_DEATH:
    return (half_played & 1U) ? CARD_ID_LIFE : CARD_ID_DEATH;
  case CARD_ID_NIGHT_DAY:
    return (half_played & 1U) ? CARD_ID_NIGHT : CARD_ID_DAY;
  case CARD_ID_ORDER_CHAOS:
    return (half_played & 1U) ? CARD_ID_ORDER : CARD_ID_CHAOS;
  case CARD_ID_BOUND_DETERMINED:
    return (half_played & 1U) ? CARD_ID_BOUND : CARD_ID_DETERMINED;
  case CARD_ID_CRIME_PUNISHMENT:
    return (half_played & 1U) ? CARD_ID_CRIME : CARD_ID_PUNISHMENT;
  case CARD_ID_HIDE_SEEK:
    return (half_played & 1U) ? CARD_ID_HIDE : CARD_ID_SEEK;
  case CARD_ID_HIT_RUN:
    return (half_played & 1U) ? CARD_ID_HIT : CARD_ID_RUN;
  case CARD_ID_ODDS_ENDS:
    return (half_played & 1U) ? CARD_ID_ODDS : CARD_ID_ENDS;
  case CARD_ID_PURE_SIMPLE:
    return (half_played & 1U) ? CARD_ID_PURE : CARD_ID_SIMPLE;
  case CARD_ID_RESEARCH_DEVELOPMENT:
    return (half_played & 1U) ? CARD_ID_RESEARCH : CARD_ID_DEVELOPMENT;
  case CARD_ID_RISE_FALL:
    return (half_played & 1U) ? CARD_ID_RISE : CARD_ID_FALL;
  case CARD_ID_SUPPLY_DEMAND:
    return (half_played & 1U) ? CARD_ID_SUPPLY : CARD_ID_DEMAND;
  case CARD_ID_TRIAL_ERROR:
    return (half_played & 1U) ? CARD_ID_TRIAL : CARD_ID_ERROR;
  case CARD_ID_ALIVE_WELL:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_ALIVE : CARD_ID_WELL;
  case CARD_ID_ARMED_DANGEROUS:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_ARMED : CARD_ID_DANGEROUS;
  case CARD_ID_FAR_AWAY:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_FAR : CARD_ID_AWAY;
  case CARD_ID_BECK_CALL:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_BECK : CARD_ID_CALL;
  case CARD_ID_FLESH_BLOOD:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_FLESH : CARD_ID_BLOOD;
  case CARD_ID_BREAKING_ENTERING:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_BREAKING : CARD_ID_ENTERING;
  case CARD_ID_TURN_BURN:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_TURN : CARD_ID_BURN;
  case CARD_ID_CATCH_RELEASE:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_CATCH : CARD_ID_RELEASE;
  case CARD_ID_DOWN_DIRTY:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_DOWN : CARD_ID_DIRTY;
  case CARD_ID_GIVE_TAKE:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_GIVE : CARD_ID_TAKE;
  case CARD_ID_PROFIT_LOSS:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_PROFIT : CARD_ID_LOSS;
  case CARD_ID_PROTECT_SERVE:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_PROTECT : CARD_ID_SERVE;
  case CARD_ID_READY_WILLING:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_READY : CARD_ID_WILLING;
  case CARD_ID_WEAR_TEAR:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_WEAR : CARD_ID_TEAR;
  case CARD_ID_TOIL_TROUBLE:
    if ((half_played & 3U) == 3U)
      return csvid;
    return (half_played & 1U) ? CARD_ID_TOIL : CARD_ID_TROUBLE;
  case CARD_ID_CUT_RIBBONS:
    return (half_played & 1U) ? CARD_ID_CUT : CARD_ID_RIBBONS;
  case CARD_ID_DESTINED_LEAD:
    return (half_played & 1U) ? CARD_ID_DESTINED : CARD_ID_LEAD;
  case CARD_ID_FAILURE_COMPLY:
    return (half_played & 1U) ? CARD_ID_FAILURE : CARD_ID_COMPLY;
  case CARD_ID_HEAVEN_EARTH:
    return (half_played & 1U) ? CARD_ID_HEAVEN : CARD_ID_EARTH;
  case CARD_ID_ONWARD_VICTORY:
    return (half_played & 1U) ? CARD_ID_ONWARD : CARD_ID_VICTORY;
  case CARD_ID_PREPARE_FIGHT:
    return (half_played & 1U) ? CARD_ID_PREPARE : CARD_ID_FIGHT;
  case CARD_ID_RAGS_RICHES:
    return (half_played & 1U) ? CARD_ID_RAGS : CARD_ID_RICHES;
  case CARD_ID_REDUCE_RUBBLE:
    return (half_played & 1U) ? CARD_ID_REDUCE : CARD_ID_RUBBLE;
  case CARD_ID_SPRING_MIND:
    return (half_played & 1U) ? CARD_ID_SPRING : CARD_ID_MIND;
  case CARD_ID_START_FINISH:
    return (half_played & 1U) ? CARD_ID_START : CARD_ID_FINISH;
  case CARD_ID_APPEAL_AUTHORITY:
    return (half_played & 1U) ? CARD_ID_APPEAL : CARD_ID_AUTHORITY;
  case CARD_ID_CLAIM_FAME:
    return (half_played & 1U) ? CARD_ID_CLAIM : CARD_ID_FAME;
  case CARD_ID_CONSIGN_OBLIVION:
    return (half_played & 1U) ? CARD_ID_CONSIGN : CARD_ID_OBLIVION;
  case CARD_ID_DRIVEN_DESPAIR:
    return (half_played & 1U) ? CARD_ID_DRIVEN : CARD_ID_DESPAIR;
  case CARD_ID_FARM_MARKET:
    return (half_played & 1U) ? CARD_ID_FARM : CARD_ID_MARKET;
  case CARD_ID_GRIND_DUST:
    return (half_played & 1U) ? CARD_ID_GRIND : CARD_ID_DUST;
  case CARD_ID_LEAVE_CHANCE:
    return (half_played & 1U) ? CARD_ID_LEAVE : CARD_ID_CHANCE;
  case CARD_ID_REASON_BELIEVE:
    return (half_played & 1U) ? CARD_ID_REASON : CARD_ID_BELIEVE;
  case CARD_ID_REFUSE_COOPERATE:
    return (half_played & 1U) ? CARD_ID_REFUSE : CARD_ID_COOPERATE;
  case CARD_ID_STRUGGLE_SURVIVE:
    return (half_played & 1U) ? CARD_ID_STRUGGLE : CARD_ID_SURVIVE;
  case CARD_ID_ASSURE_ASSEMBLE:
    return (half_played & 1U) ? CARD_ID_ASSURE : CARD_ID_ASSEMBLE;
  case CARD_ID_CONNIVE_CONCOCT:
    return (half_played & 1U) ? CARD_ID_CONNIVE : CARD_ID_CONCOCT;
  case CARD_ID_DISCOVERY_DISPERSAL:
    return (half_played & 1U) ? CARD_ID_DISCOVERY : CARD_ID_DISPERSAL;
  case CARD_ID_EXPANSION_EXPLOSION:
    return (half_played & 1U) ? CARD_ID_EXPANSION : CARD_ID_EXPLOSION;
  case CARD_ID_FIND_FINALITY:
    return (half_played & 1U) ? CARD_ID_FIND : CARD_ID_FINALITY;
  case CARD_ID_FLOWER_FLOURISH:
    return (half_played & 1U) ? CARD_ID_FLOWER : CARD_ID_FLOURISH;
  case CARD_ID_INTEGRITY_INTERVENTION:
    return (half_played & 1U) ? CARD_ID_INTEGRITY : CARD_ID_INTERVENTION;
  case CARD_ID_INVERT_INVENT:
    return (half_played & 1U) ? CARD_ID_INVERT : CARD_ID_INVENT;
  case CARD_ID_RESPONSE_RESURGENCE:
    return (half_played & 1U) ? CARD_ID_RESPONSE : CARD_ID_RESURGENCE;
  case CARD_ID_STATUS_STATUE:
    return (half_played & 1U) ? CARD_ID_STATUS : CARD_ID_STATUE;
  case CARD_ID_BEDECK_BEDAZZLE:
    return (half_played & 1U) ? CARD_ID_BEDECK : CARD_ID_BEDAZZLE;
  case CARD_ID_CARNIVAL_CARNAGE:
    return (half_played & 1U) ? CARD_ID_CARNIVAL : CARD_ID_CARNAGE;
  case CARD_ID_COLLISION_COLOSSUS:
    return (half_played & 1U) ? CARD_ID_COLLISION : CARD_ID_COLOSSUS;
  case CARD_ID_CONSECRATE_CONSUME:
    return (half_played & 1U) ? CARD_ID_CONSECRATE : CARD_ID_CONSUME;
  case CARD_ID_DEPOSE_DEPLOY:
    return (half_played & 1U) ? CARD_ID_DEPOSE : CARD_ID_DEPLOY;
  case CARD_ID_INCUBATION_INCONGRUITY:
    return (half_played & 1U) ? CARD_ID_INCUBATION : CARD_ID_INCONGRUITY;
  case CARD_ID_REPUDIATE_REPLICATE:
    return (half_played & 1U) ? CARD_ID_REPUDIATE : CARD_ID_REPLICATE;
  case CARD_ID_REVIVAL_REVENGE:
    return (half_played & 1U) ? CARD_ID_REVIVAL : CARD_ID_REVENGE;
  case CARD_ID_THRASH_THREAT:
    return (half_played & 1U) ? CARD_ID_THRASH : CARD_ID_THREAT;
  case CARD_ID_WARRANT_WARDEN:
    return (half_played & 1U) ? CARD_ID_WARRANT : CARD_ID_WARDEN;
  default:
    return -1;
  }
}

/* ManalinkEh 2022: 0x267cf50; original functions/functions.c:3417. */
int dfc_front_to_back(int csvid)
{
  switch (csvid)
  {
  case CARD_ID_BLOODLINE_KEEPER:
    return CARD_ID_LORD_OF_LINEAGE;
  case CARD_ID_CIVILIZED_SCHOLAR:
    return CARD_ID_HOMICIDAL_BRUTE;
  case CARD_ID_CLOISTERED_YOUTH:
    return CARD_ID_UNHOLY_FIEND;
  case CARD_ID_DAYBREAK_RANGER:
    return CARD_ID_NIGHTFALL_PREDATOR;
  case CARD_ID_DELVER_OF_SECRETS:
    return CARD_ID_INSECTILE_ABERRATION;
  case CARD_ID_GARRUK_RELENTLESS:
    return CARD_ID_GARRUK_THE_VEIL_CURSED;
  case CARD_ID_GATSTAF_SHEPHERD:
    return CARD_ID_GATSTAF_HOWLER;
  case CARD_ID_GRIZZLED_OUTCASTS:
    return CARD_ID_KRALLENHORDE_WANTONS;
  case CARD_ID_HANWEIR_WATCHKEEP:
    return CARD_ID_BANE_OF_HANWEIR;
  case CARD_ID_INSTIGATOR_GANG:
    return CARD_ID_WILDBLOOD_PACK;
  case CARD_ID_KRUIN_OUTLAW:
    return CARD_ID_TERROR_OF_KRUIN_PASS;
  case CARD_ID_LUDEVICS_TEST_SUBJECT:
    return CARD_ID_LUDEVICS_ABOMINATION;
  case CARD_ID_MAYOR_OF_AVABRUCK:
    return CARD_ID_HOWLPACK_ALPHA;
  case CARD_ID_RECKLESS_WAIF:
    return CARD_ID_MERCILESS_PREDATOR;
  case CARD_ID_SCREECHING_BAT:
    return CARD_ID_STALKING_VAMPIRE;
  case CARD_ID_THRABEN_SENTRY:
    return CARD_ID_THRABEN_MILITIA;
  case CARD_ID_TORMENTED_PARIAH:
    return CARD_ID_RAMPAGING_WEREWOLF;
  case CARD_ID_ULVENWALD_MYSTICS:
    return CARD_ID_ULVENWALD_PRIMORDIALS;
  case CARD_ID_VILLAGE_IRONSMITH:
    return CARD_ID_IRONFANG;
  case CARD_ID_VILLAGERS_OF_ESTWALD:
    return CARD_ID_HOWLPACK_OF_ESTWALD;
  case CARD_ID_AFFLICTED_DESERTER:
    return CARD_ID_WEREWOLF_RANSACKER;
  case CARD_ID_CHALICE_OF_LIFE:
    return CARD_ID_CHALICE_OF_DEATH;
  case CARD_ID_CHOSEN_OF_MARKOV:
    return CARD_ID_MARKOVS_SERVANT;
  case CARD_ID_ELBRUS_THE_BINDING_BLADE:
    return CARD_ID_WITHENGAR_UNBOUND;
  case CARD_ID_HINTERLAND_HERMIT:
    return CARD_ID_HINTERLAND_SCOURGE;
  case CARD_ID_HUNTMASTER_OF_THE_FELLS:
    return CARD_ID_RAVAGER_OF_THE_FELLS;
  case CARD_ID_LAMBHOLT_ELDER:
    return CARD_ID_SILVERPELT_WEREWOLF;
  case CARD_ID_LOYAL_CATHAR:
    return CARD_ID_UNHALLOWED_CATHAR;
  case CARD_ID_MONDRONEN_SHAMAN:
    return CARD_ID_TOVOLARS_MAGEHUNTER;
  case CARD_ID_RAVENOUS_DEMON:
    return CARD_ID_ARCHDEMON_OF_GREED;
  case CARD_ID_SCORNED_VILLAGER:
    return CARD_ID_MOONSCARRED_WEREWOLF;
  case CARD_ID_SOUL_SEIZER:
    return CARD_ID_GHASTLY_HAUNTING;
  case CARD_ID_WOLFBITTEN_CAPTIVE:
    return CARD_ID_KRALLENHORDE_KILLER;
  case CARD_ID_CHANDRA_FIRE_OF_KALADESH:
    return CARD_ID_CHANDRA_ROARING_FLAME;
  case CARD_ID_JACE_VRYNS_PRODIGY:
    return CARD_ID_JACE_TELEPATH_UNBOUND;
  case CARD_ID_KYTHEON_HERO_OF_AKROS:
    return CARD_ID_GIDEON_BATTLE_FORGED;
  case CARD_ID_LILIANA_HERETICAL_HEALER:
    return CARD_ID_LILIANA_DEFIANT_NECROMANCER;
  case CARD_ID_NISSA_VASTWOOD_SEER:
    return CARD_ID_NISSA_SAGE_ANIMIST;
  case CARD_ID_ABERRANT_RESEARCHER:
    return CARD_ID_PERFECTED_FORM;
  case CARD_ID_ACCURSED_WITCH:
    return CARD_ID_INFECTIOUS_CURSE;
  case CARD_ID_ARCHANGEL_AVACYN:
    return CARD_ID_AVACYN_THE_PURIFIER;
  case CARD_ID_ARLINN_KORD:
    return CARD_ID_ARLINN_EMBRACED_BY_THE_MOON;
  case CARD_ID_AUTUMNAL_GLOOM:
    return CARD_ID_ANCIENT_OF_THE_EQUINOX;
  case CARD_ID_AVACYNIAN_MISSIONARIES:
    return CARD_ID_LUNARCH_INQUISITORS;
  case CARD_ID_BREAKNECK_RIDER:
    return CARD_ID_NECK_BREAKER;
  case CARD_ID_CONVICTED_KILLER:
    return CARD_ID_BRANDED_HOWLER;
  case CARD_ID_DARING_SLEUTH:
    return CARD_ID_BEARER_OF_OVERWHELMING_TRUTHS;
  case CARD_ID_DUSKWATCH_RECRUITER:
    return CARD_ID_KRALLENHORDE_HOWLER;
  case CARD_ID_ELUSIVE_TORMENTOR:
    return CARD_ID_INSIDIOUS_MIST;
  case CARD_ID_GATSTAF_ARSONISTS:
    return CARD_ID_GATSTAF_RAVAGERS;
  case CARD_ID_GEIER_REACH_BANDIT:
    return CARD_ID_VILDIN_PACK_ALPHA;
  case CARD_ID_HANWEIR_MILITIA_CAPTAIN:
    return CARD_ID_WESTVALE_CULT_LEADER;
  case CARD_ID_HARVEST_HAND:
    return CARD_ID_SCROUNGED_SCYTHE;
  case CARD_ID_HEIR_OF_FALKENRATH:
    return CARD_ID_HEIR_TO_THE_NIGHT;
  case CARD_ID_HERMIT_OF_THE_NATTERKNOLLS:
    return CARD_ID_LONE_WOLF_OF_THE_NATTERKNOLLS;
  case CARD_ID_HINTERLAND_LOGGER:
    return CARD_ID_TIMBER_SHREDDER;
  case CARD_ID_KESSIG_FORGEMASTER:
    return CARD_ID_FLAMEHEART_WEREWOLF;
  case CARD_ID_KINDLY_STRANGER:
    return CARD_ID_DEMON_POSSESSED_WITCH;
  case CARD_ID_LAMBHOLT_PACIFIST:
    return CARD_ID_LAMBHOLT_BUTCHER;
  case CARD_ID_NEGLECTED_HEIRLOOM:
    return CARD_ID_ASHMOUTH_BLADE;
  case CARD_ID_PIOUS_EVANGEL:
    return CARD_ID_WAYWARD_DISCIPLE;
  case CARD_ID_SAGE_OF_ANCIENT_LORE:
    return CARD_ID_WEREWOLF_OF_ANCIENT_HUNGER;
  case CARD_ID_SKIN_INVASION:
    return CARD_ID_SKIN_SHEDDER;
  case CARD_ID_SOLITARY_HUNTER:
    return CARD_ID_ONE_OF_THE_PACK;
  case CARD_ID_STARTLED_AWAKE:
    return CARD_ID_PERSISTENT_NIGHTMARE;
  case CARD_ID_THING_IN_THE_ICE:
    return CARD_ID_AWOKEN_HORROR;
  case CARD_ID_THRABEN_GARGOYLE:
    return CARD_ID_STONEWING_ANTAGONIZER;
  case CARD_ID_TOWN_GOSSIPMONGER:
    return CARD_ID_INCITED_RABBLE;
  case CARD_ID_UNINVITED_GEIST:
    return CARD_ID_UNIMPEDED_TRESPASSER;
  case CARD_ID_VILLAGE_MESSENGER:
    return CARD_ID_MOONRISE_INTRUDER;
  case CARD_ID_WESTVALE_ABBEY:
    return CARD_ID_ORMENDAHL_PROFANE_PRINCE;
  case CARD_ID_CONDUIT_OF_STORMS:
    return CARD_ID_CONDUIT_OF_EMRAKUL;
  case CARD_ID_CRYPTOLITH_FRAGMENT:
    return CARD_ID_AURORA_OF_EMRAKUL;
  case CARD_ID_CURIOUS_HOMUNCULUS:
    return CARD_ID_VORACIOUS_READER;
  case CARD_ID_DOCENT_OF_PERFECTION:
    return CARD_ID_FINAL_ITERATION;
  case CARD_ID_EXTRICATOR_OF_SIN:
    return CARD_ID_EXTRICATOR_OF_FLESH;
  case CARD_ID_GRIZZLED_ANGLER:
    return CARD_ID_GRISLY_ANGLERFISH;
  case CARD_ID_KESSIG_PROWLER:
    return CARD_ID_SINUOUS_PREDATOR;
  case CARD_ID_LONE_RIDER:
    return CARD_ID_IT_THAT_RIDES_AS_ONE;
  case CARD_ID_SHRILL_HOWLER:
    return CARD_ID_HOWLING_CHORUS;
  case CARD_ID_SMOLDERING_WEREWOLF:
    return CARD_ID_ERUPTING_DREADWOLF;
  case CARD_ID_TANGLECLAW_WEREWOLF:
    return CARD_ID_FIBROUS_ENTANGLER;
  case CARD_ID_ULRICH_OF_THE_KRALLENHORDE:
    return CARD_ID_ULRICH_UNCONTESTED_ALPHA;
  case CARD_ID_ULVENWALD_CAPTIVE:
    return CARD_ID_ULVENWALD_ABOMINATION;
  case CARD_ID_VILDIN_PACK_OUTCAST:
    return CARD_ID_DRONEPACK_KINDRED;
  case CARD_ID_VOLDAREN_PARIAH:
    return CARD_ID_ABOLISHER_OF_BLOODLINES;
  case CARD_ID_ARGUELS_BLOOD_FAST:
    return CARD_ID_TEMPLE_OF_ACLAZOTZ;
  case CARD_ID_CONQUERORS_GALLEON:
    return CARD_ID_CONQUERORS_FOOTHOLD;
  case CARD_ID_DOWSING_DAGGER:
    return CARD_ID_LOST_VALE;
  case CARD_ID_GROWING_RITES_OF_ITLIMOC:
    return CARD_ID_ITLIMOC_CRADLE_OF_THE_SUN;
  case CARD_ID_LEGIONS_LANDING:
    return CARD_ID_ADANTO_THE_FIRST_FORT;
  case CARD_ID_PRIMAL_AMULET:
    return CARD_ID_PRIMAL_WELLSPRING;
  case CARD_ID_SEARCH_FOR_AZCANTA:
    return CARD_ID_AZCANTA_THE_SUNKEN_RUIN;
  case CARD_ID_THAUMATIC_COMPASS:
    return CARD_ID_SPIRES_OF_ORAZCA;
  case CARD_ID_TREASURE_MAP:
    return CARD_ID_TREASURE_COVE;
  case CARD_ID_VANCES_BLASTING_CANNONS:
    return CARD_ID_SPITFIRE_BASTION;
  case CARD_ID_AZORS_GATEWAY:
    return CARD_ID_SANCTUM_OF_THE_SUN;
  case CARD_ID_GOLDEN_GUARDIAN:
    return CARD_ID_GOLD_FORGE_GARRISON;
  case CARD_ID_HADANAS_CLIMB:
    return CARD_ID_WINGED_TEMPLE_OF_ORAZCA;
  case CARD_ID_JOURNEY_TO_ETERNITY:
    return CARD_ID_ATZAL_CAVE_OF_ETERNITY;
  case CARD_ID_PATH_OF_METTLE:
    return CARD_ID_METZALI_TOWER_OF_TRIUMPH;
  case CARD_ID_PROFANE_PROCESSION:
    return CARD_ID_TOMB_OF_THE_DUSK_ROSE;
  case CARD_ID_STORM_THE_VAULT:
    return CARD_ID_VAULT_OF_CATLACAN;
  case CARD_ID_NICOL_BOLAS_THE_RAVAGER:
    return CARD_ID_NICOL_BOLAS_THE_ARISEN;
  case CARD_ID_GRIMLOCK_DINOBOT_LEADER:
    return CARD_ID_GRIMLOCK_FEROCIOUS_KING;
  case CARD_ID_AGADEEMS_AWAKENING:
    return CARD_ID_AGADEEM_THE_UNDERCRYPT;
  case CARD_ID_AKOUM_WARRIOR:
    return CARD_ID_AKOUM_TEETH;
  case CARD_ID_BALA_GED_RECOVERY:
    return CARD_ID_BALA_GED_SANCTUARY;
  case CARD_ID_BEYEEN_VEIL:
    return CARD_ID_BEYEEN_COAST;
  case CARD_ID_BLACKBLOOM_ROGUE:
    return CARD_ID_BLACKBLOOM_BOG;
  case CARD_ID_BRANCHLOFT_PATHWAY:
    return CARD_ID_BOULDERLOFT_PATHWAY;
  case CARD_ID_BRIGHTCLIMB_PATHWAY:
    return CARD_ID_GRIMCLIMB_PATHWAY;
  case CARD_ID_CLEARWATER_PATHWAY:
    return CARD_ID_MURKWATER_PATHWAY;
  case CARD_ID_CRAGCROWN_PATHWAY:
    return CARD_ID_TIMBERCROWN_PATHWAY;
  case CARD_ID_EMERIAS_CALL:
    return CARD_ID_EMERIA_SHATTERED_SKYCLAVE;
  case CARD_ID_GLASSPOOL_MIMIC:
    return CARD_ID_GLASSPOOL_SHORE;
  case CARD_ID_HAGRA_MAULING:
    return CARD_ID_HAGRA_BROODPIT;
  case CARD_ID_JWARI_DISRUPTION:
    return CARD_ID_JWARI_RUINS;
  case CARD_ID_KABIRA_TAKEDOWN:
    return CARD_ID_KABIRA_PLATEAU;
  case CARD_ID_KAZANDU_MAMMOTH:
    return CARD_ID_KAZANDU_VALLEY;
  case CARD_ID_KAZUULS_FURY:
    return CARD_ID_KAZUULS_CLIFFS;
  case CARD_ID_KHALNI_AMBUSH:
    return CARD_ID_KHALNI_TERRITORY;
  case CARD_ID_MAKINDI_STAMPEDE:
    return CARD_ID_MAKINDI_MESAS;
  case CARD_ID_MALAKIR_REBIRTH:
    return CARD_ID_MALAKIR_MIRE;
  case CARD_ID_NEEDLEVERGE_PATHWAY:
    return CARD_ID_PILLARVERGE_PATHWAY;
  case CARD_ID_ONDU_INVERSION:
    return CARD_ID_ONDU_SKYRUINS;
  case CARD_ID_PELAKKA_PREDATION:
    return CARD_ID_PELAKKA_CAVERNS;
  case CARD_ID_RIVERGLIDE_PATHWAY:
    return CARD_ID_LAVAGLIDE_PATHWAY;
  case CARD_ID_SEA_GATE_RESTORATION:
    return CARD_ID_SEA_GATE_REBORN;
  case CARD_ID_SEJIRI_SHELTER:
    return CARD_ID_SEJIRI_GLACIER;
  case CARD_ID_SHATTERSKULL_SMASHING:
    return CARD_ID_SHATTERSKULL_THE_HAMMER_PASS;
  case CARD_ID_SILUNDI_VISION:
    return CARD_ID_SILUNDI_ISLE;
  case CARD_ID_SKYCLAVE_CLERIC:
    return CARD_ID_SKYCLAVE_BASILICA;
  case CARD_ID_SONG_MAD_TREACHERY:
    return CARD_ID_SONG_MAD_RUINS;
  case CARD_ID_SPIKEFIELD_HAZARD:
    return CARD_ID_SPIKEFIELD_CAVE;
  case CARD_ID_TANGLED_FLORAHEDRON:
    return CARD_ID_TANGLED_VALE;
  case CARD_ID_TURNTIMBER_SYMBIOSIS:
    return CARD_ID_TURNTIMBER_SERPENTINE_WOOD;
  case CARD_ID_UMARA_WIZARD:
    return CARD_ID_UMARA_SKYFALLS;
  case CARD_ID_VALAKUT_AWAKENING:
    return CARD_ID_VALAKUT_STONEFORGE;
  case CARD_ID_VASTWOOD_FORTIFICATION:
    return CARD_ID_VASTWOOD_THICKET;
  case CARD_ID_ZOF_CONSUMPTION:
    return CARD_ID_ZOF_BLOODBOG;
  case CARD_ID_ALRUND_GOD_OF_THE_COSMOS:
    return CARD_ID_HAKKA_WHISPERING_RAVEN;
  case CARD_ID_BARKCHANNEL_PATHWAY:
    return CARD_ID_TIDECHANNEL_PATHWAY;
  case CARD_ID_BIRGI_GOD_OF_STORYTELLING:
    return CARD_ID_HARNFEL_HORN_OF_BOUNTY;
  case CARD_ID_BLIGHTSTEP_PATHWAY:
    return CARD_ID_SEARSTEP_PATHWAY;
  case CARD_ID_COSIMA_GOD_OF_THE_VOYAGE:
    return CARD_ID_THE_OMENKEEL;
  case CARD_ID_DARKBORE_PATHWAY:
    return CARD_ID_SLITHERBORE_PATHWAY;
  case CARD_ID_EGON_GOD_OF_DEATH:
    return CARD_ID_THRONE_OF_DEATH;
  case CARD_ID_ESIKA_GOD_OF_THE_TREE:
    return CARD_ID_THE_PRISMATIC_BRIDGE;
  case CARD_ID_HALVAR_GOD_OF_BATTLE:
    return CARD_ID_SWORD_OF_THE_REALMS;
  case CARD_ID_HENGEGATE_PATHWAY:
    return CARD_ID_MISTGATE_PATHWAY;
  case CARD_ID_JORN_GOD_OF_WINTER:
    return CARD_ID_KALDRING_THE_RIMESTAFF;
  case CARD_ID_KOLVORI_GOD_OF_KINSHIP:
    return CARD_ID_THE_RINGHART_CREST;
  case CARD_ID_REIDANE_GOD_OF_THE_WORTHY:
    return CARD_ID_VALKMIRA_PROTECTORS_SHIELD;
  case CARD_ID_TERGRID_GOD_OF_FRIGHT:
    return CARD_ID_TERGRIDS_LANTERN;
  case CARD_ID_TORALF_GOD_OF_FURY:
    return CARD_ID_TORALFS_HAMMER;
  case CARD_ID_VALKI_GOD_OF_LIES:
    return CARD_ID_TIBALT_COSMIC_IMPOSTOR;
  case CARD_ID_AUGMENTER_PUGILIST:
    return CARD_ID_ECHOING_EQUATION;
  case CARD_ID_BLEX_VEXING_PEST:
    return CARD_ID_SEARCH_FOR_BLEX;
  case CARD_ID_EXTUS_ORIQ_OVERLORD:
    return CARD_ID_AWAKEN_THE_BLOOD_AVATAR;
  case CARD_ID_FLAMESCROLL_CELEBRANT:
    return CARD_ID_REVEL_IN_SILENCE;
  case CARD_ID_JADZI_ORACLE_OF_ARCAVIOS:
    return CARD_ID_JOURNEY_TO_THE_ORACLE;
  case CARD_ID_KIANNE_DEAN_OF_SUBSTANCE:
    return CARD_ID_IMBRAHAM_DEAN_OF_THEORY;
  case CARD_ID_MILA_CRAFTY_COMPANION:
    return CARD_ID_LUKKA_WAYWARD_BONDER;
  case CARD_ID_PESTILENT_CAULDRON:
    return CARD_ID_RESTORATIVE_BURST;
  case CARD_ID_PLARGG_DEAN_OF_CHAOS:
    return CARD_ID_AUGUSTA_DEAN_OF_ORDER;
  case CARD_ID_ROWAN_SCHOLAR_OF_SPARKS:
    return CARD_ID_WILL_SCHOLAR_OF_FROST;
  case CARD_ID_SELFLESS_GLYPHWEAVER:
    return CARD_ID_DEADLY_VANITY;
  case CARD_ID_SHAILE_DEAN_OF_RADIANCE:
    return CARD_ID_EMBROSE_DEAN_OF_SHADOW;
  case CARD_ID_TORRENT_SCULPTOR:
    return CARD_ID_FLAMETHROWER_SONATA;
  case CARD_ID_UVILDA_DEAN_OF_PERFECTION:
    return CARD_ID_NASSARI_DEAN_OF_EXPRESSION;
  case CARD_ID_VALENTIN_DEAN_OF_THE_VEIN:
    return CARD_ID_LISETTE_DEAN_OF_THE_ROOT;
  case CARD_ID_WANDERING_ARCHAIC:
    return CARD_ID_EXPLORE_THE_VASTLANDS;
  case CARD_ID_AMBITIOUS_FARMHAND:
    return CARD_ID_SEASONED_CATHAR;
  case CARD_ID_ARLINN_THE_PACKS_HOPE:
    return CARD_ID_ARLINN_THE_MOONS_FURY;
  case CARD_ID_BAITHOOK_ANGLER:
    return CARD_ID_HOOK_HAUNT_DRIFTER;
  case CARD_ID_BANEBLADE_SCOUNDREL:
    return CARD_ID_BANECLAW_MARAUDER;
  case CARD_ID_BELOVED_BEGGAR:
    return CARD_ID_GENEROUS_SOUL;
  case CARD_ID_BEREAVED_SURVIVOR:
    return CARD_ID_DAUNTLESS_AVENGER;
  case CARD_ID_BIRD_ADMIRER:
    return CARD_ID_WING_SHREDDER;
  case CARD_ID_BRUTAL_CATHAR:
    return CARD_ID_MOONRAGE_BRUTE;
  case CARD_ID_BURLY_BREAKER:
    return CARD_ID_DIRE_STRAIN_DEMOLISHER;
  case CARD_ID_CHAPLAIN_OF_ALMS:
    return CARD_ID_CHAPEL_SHIELDGEIST;
  case CARD_ID_COVERT_CUTPURSE:
    return CARD_ID_COVETOUS_GEIST;
  case CARD_ID_COVETOUS_CASTAWAY:
    return CARD_ID_GHOSTLY_CASTIGATOR;
  case CARD_ID_CURSE_OF_LEECHES:
    return CARD_ID_LEECHING_LURKER;
  case CARD_ID_DAY_TOKEN:
    return CARD_ID_NIGHT_TOKEN;
  case CARD_ID_DEATHBONNET_SPROUT:
    return CARD_ID_DEATHBONNET_HULK;
  case CARD_ID_DENNICK_PIOUS_APPRENTICE:
    return CARD_ID_DENNICK_PIOUS_APPARITION;
  case CARD_ID_DEVOTED_GRAFKEEPER:
    return CARD_ID_DEPARTED_SOULKEEPER;
  case CARD_ID_ECSTATIC_AWAKENER:
    return CARD_ID_AWOKEN_DEMON;
  case CARD_ID_ENDURING_ANGEL:
    return CARD_ID_ANGELIC_ENFORCER;
  case CARD_ID_FANGBLADE_BRIGAND:
    return CARD_ID_FANGBLADE_EVISCERATOR;
  case CARD_ID_FLAME_CHANNELER:
    return CARD_ID_EMBODIMENT_OF_FLAME;
  case CARD_ID_GALEDRIFTER:
    return CARD_ID_WAILDRIFTER;
  case CARD_ID_GRAVEYARD_TRESPASSER:
    return CARD_ID_GRAVEYARD_GLUTTON;
  case CARD_ID_HARVESTTIDE_INFILTRATOR:
    return CARD_ID_HARVESTTIDE_ASSAILANT;
  case CARD_ID_HEIRLOOM_MIRROR:
    return CARD_ID_INHERITED_FIEND;
  case CARD_ID_HOSTILE_HOSTEL:
    return CARD_ID_CREEPING_INN;
  case CARD_ID_HOUND_TAMER:
    return CARD_ID_UNTAMED_PUP;
  case CARD_ID_JERREN_CORRUPTED_BISHOP:
    return CARD_ID_ORMENDAHL_THE_CORRUPTER;
  case CARD_ID_KESSIG_NATURALIST:
    return CARD_ID_LORD_OF_THE_ULVENWALD;
  case CARD_ID_LUDEVIC_NECROGENIUS:
    return CARD_ID_OLAG_LUDEVICS_HUBRIS;
  case CARD_ID_LUNARCH_VETERAN:
    return CARD_ID_LUMINOUS_PHANTOM;
  case CARD_ID_MALEVOLENT_HERMIT:
    return CARD_ID_BENEVOLENT_GEIST;
  case CARD_ID_MOURNING_PATROL:
    return CARD_ID_MORNING_APPARITION;
  case CARD_ID_MYSTERIOUS_TOME:
    return CARD_ID_CHILLING_CHRONICLE;
  case CARD_ID_MYSTIC_SKULL:
    return CARD_ID_MYSTIC_MONSTROSITY;
  case CARD_ID_OUTLAND_LIBERATOR:
    return CARD_ID_FRENZIED_TRAPBREAKER;
  case CARD_ID_OVERWHELMED_ARCHIVIST:
    return CARD_ID_ARCHIVE_HAUNT;
  case CARD_ID_POPPET_STITCHER:
    return CARD_ID_POPPET_FACTORY;
  case CARD_ID_RECKLESS_STORMSEEKER:
    return CARD_ID_STORM_CHARGED_SLASHER;
  case CARD_ID_SHADY_TRAVELER:
    return CARD_ID_STALKING_PREDATOR;
  case CARD_ID_SMOLDERING_EGG:
    return CARD_ID_ASHMOUTH_DRAGON;
  case CARD_ID_SPELLRUNE_PAINTER:
    return CARD_ID_SPELLRUNE_HOWLER;
  case CARD_ID_SUSPICIOUS_STOWAWAY:
    return CARD_ID_SEAFARING_WEREWOLF;
  case CARD_ID_TAVERN_RUFFIAN:
    return CARD_ID_TAVERN_SMASHER;
  case CARD_ID_TIRELESS_HAULER:
    return CARD_ID_DIRE_STRAIN_BRAWLER;
  case CARD_ID_TOVOLARS_HUNTMASTER:
    return CARD_ID_TOVOLARS_PACKLEADER;
  case CARD_ID_TOVOLAR_DIRE_OVERLORD:
    return CARD_ID_TOVOLAR_THE_MIDNIGHT_SCOURGE;
  case CARD_ID_VENGEFUL_STRANGLER:
    return CARD_ID_STRANGLING_GRASP;
  case CARD_ID_VILLAGE_WATCH:
    return CARD_ID_VILLAGE_REAVERS;
  case CARD_ID_ALLURING_SUITOR:
    return CARD_ID_DEADLY_DANCER;
  case CARD_ID_AVABRUCK_CARETAKER:
    return CARD_ID_HOLLOWHENGE_HUNTMASTER;
  case CARD_ID_BALLISTA_WATCHER:
    return CARD_ID_BALLISTA_WIELDER;
  case CARD_ID_BINDING_GEIST:
    return CARD_ID_SPECTRAL_BINDING;
  case CARD_ID_BIOLUME_EGG:
    return CARD_ID_BIOLUME_SERPENT;
  case CARD_ID_BLOODSWORN_SQUIRE:
    return CARD_ID_BLOODSWORN_KNIGHT;
  case CARD_ID_BRINE_COMBER:
    return CARD_ID_BRINEBOUND_GIFT;
  case CARD_ID_CATAPULT_FODDER:
    return CARD_ID_CATAPULT_CAPTAIN;
  case CARD_ID_CHILD_OF_THE_PACK:
    return CARD_ID_SAVAGE_PACKMATE;
  case CARD_ID_CONCEALING_CURTAINS:
    return CARD_ID_REVEALING_EYE;
  case CARD_ID_DESPERATE_FARMER:
    return CARD_ID_DEPRAVED_HARVESTER;
  case CARD_ID_DISTRACTING_GEIST:
    return CARD_ID_CLEVER_DISTRACTION;
  case CARD_ID_DORMANT_GROVE:
    return CARD_ID_GNARLED_GROVESTRIDER;
  case CARD_ID_DOROTHEA_VENGEFUL_VICTIM:
    return CARD_ID_DOROTHEAS_RETRIBUTION;
  case CARD_ID_DROGSKOL_INFANTRY:
    return CARD_ID_DROGSKOL_ARMAMENTS;
  case CARD_ID_EDGAR_CHARMED_GROOM:
    return CARD_ID_EDGAR_MARKOVS_COFFIN;
  case CARD_ID_FAITHBOUND_JUDGE:
    return CARD_ID_SINNERS_JUDGMENT;
  case CARD_ID_FEARFUL_VILLAGER:
    return CARD_ID_FEARSOME_WEREWOLF;
  case CARD_ID_FOREBODING_STATUE:
    return CARD_ID_FORSAKEN_THRESHER;
  case CARD_ID_GUTTER_SKULKER:
    return CARD_ID_GUTTER_SHORTCUT;
  case CARD_ID_HENRIKA_DOMNATHI:
    return CARD_ID_HENRIKA_INFERNAL_SEER;
  case CARD_ID_HOOKHAND_MARINER:
    return CARD_ID_RIPHOOK_RAIDER;
  case CARD_ID_HOWLPACK_PIPER:
    return CARD_ID_WILDSONG_HOWLER;
  case CARD_ID_ILL_TEMPERED_LONER:
    return CARD_ID_HOWLPACK_AVENGER;
  case CARD_ID_INFESTATION_EXPERT:
    return CARD_ID_INFESTED_WEREWOLF;
  case CARD_ID_INNOCENT_TRAVELER:
    return CARD_ID_MALICIOUS_INVADER;
  case CARD_ID_JACOB_HAUKEN_INSPECTOR:
    return CARD_ID_HAUKENS_INSIGHT;
  case CARD_ID_KATILDA_DAWNHART_MARTYR:
    return CARD_ID_KATILDAS_RISING_DAWN;
  case CARD_ID_KINDLY_ANCESTOR:
    return CARD_ID_ANCESTORS_EMBRACE;
  case CARD_ID_LAMBHOLT_RACONTEUR:
    return CARD_ID_LAMBHOLT_RAVAGER;
  case CARD_ID_LANTERN_BEARER:
    return CARD_ID_LANTERNS_LIFT;
  case CARD_ID_MIRRORHALL_MIMIC:
    return CARD_ID_GHASTLY_MIMICRY;
  case CARD_ID_MISCHIEVOUS_CATGEIST:
    return CARD_ID_CATLIKE_CURIOSITY;
  case CARD_ID_OAKSHADE_STALKER:
    return CARD_ID_MOONLIT_AMBUSHER;
  case CARD_ID_PANICKED_BYSTANDER:
    return CARD_ID_CACKLING_CULPRIT;
  case CARD_ID_RADIANT_GRACE:
    return CARD_ID_RADIANT_RESTRAINTS;
  case CARD_ID_RAGGED_RECLUSE:
    return CARD_ID_ODIOUS_WITCH;
  case CARD_ID_RESTLESS_BLOODSEEKER:
    return CARD_ID_BLOODSOAKED_REVELER;
  case CARD_ID_RUNO_STROMKIRK:
    return CARD_ID_KROTHUSS_LORD_OF_THE_DEEP;
  case CARD_ID_SOULCIPHER_BOARD:
    return CARD_ID_CIPHERBOUND_SPIRIT;
  case CARD_ID_TWINBLADE_GEIST:
    return CARD_ID_TWINBLADE_INVOCATION;
  case CARD_ID_ULVENWALD_ODDITY:
    return CARD_ID_ULVENWALD_BEHEMOTH;
  case CARD_ID_VOLATILE_ARSONIST:
    return CARD_ID_DIRE_STRAIN_ANARCHIST;
  case CARD_ID_VOLDAREN_BLOODCASTER:
    return CARD_ID_BLOODBAT_SUMMONER;
  case CARD_ID_VOLTAIC_VISIONARY:
    return CARD_ID_VOLT_CHARGED_BERSERKER;
  case CARD_ID_WEARY_PRISONER:
    return CARD_ID_WRATHFUL_JAILBREAKER;
  case CARD_ID_WEAVER_OF_BLOSSOMS:
    return CARD_ID_BLOSSOM_CLAD_WEREWOLF;
  case CARD_ID_WEDDING_ANNOUNCEMENT:
    return CARD_ID_WEDDING_FESTIVITY;
  case CARD_ID_WOLFKIN_OUTCAST:
    return CARD_ID_WEDDING_CRASHER;
  case CARD_ID_AZUSAS_MANY_JOURNEYS:
    return CARD_ID_LIKENESS_OF_THE_SEEKER;
  case CARD_ID_BEFRIENDING_THE_MOTHS:
    return CARD_ID_IMPERIAL_MOTH;
  case CARD_ID_BEHOLD_THE_UNSPEAKABLE:
    return CARD_ID_VISION_OF_THE_UNSPEAKABLE;
  case CARD_ID_BOSEIJU_REACHES_SKYWARD:
    return CARD_ID_BRANCH_OF_BOSEIJU;
  case CARD_ID_ERA_OF_ENLIGHTENMENT:
    return CARD_ID_HAND_OF_ENLIGHTENMENT;
  case CARD_ID_FABLE_OF_THE_MIRROR_BREAKER:
    return CARD_ID_REFLECTION_OF_KIKI_JIKI;
  case CARD_ID_HIDETSUGU_CONSUMES_ALL:
    return CARD_ID_VESSEL_OF_THE_ALL_CONSUMING;
  case CARD_ID_INVENTIVE_ITERATION:
    return CARD_ID_LIVING_BREAKTHROUGH;
  case CARD_ID_JUGAN_DEFENDS_THE_TEMPLE:
    return CARD_ID_REMNANT_OF_THE_RISING_STAR;
  case CARD_ID_KUMANO_FACES_KAKKAZAN:
    return CARD_ID_ETCHING_OF_KUMANO;
  case CARD_ID_LIFE_OF_TOSHIRO_UMEZAWA:
    return CARD_ID_MEMORY_OF_TOSHIRO;
  case CARD_ID_MICHIKOS_REIGN_OF_TRUTH:
    return CARD_ID_PORTRAIT_OF_MICHIKO;
  case CARD_ID_OKIBA_RECKONER_RAID:
    return CARD_ID_NEZUMI_ROAD_CAPTAIN;
  case CARD_ID_TALES_OF_MASTER_SESHIRO:
    return CARD_ID_SESHIROS_LIVING_LEGACY;
  case CARD_ID_TEACHINGS_OF_THE_KIRIN:
    return CARD_ID_KIRIN_TOUCHED_OROCHI;
  case CARD_ID_THE_DRAGON_KAMI_REBORN:
    return CARD_ID_DRAGON_KAMIS_EGG;
  case CARD_ID_THE_FALL_OF_LORD_KONDA:
    return CARD_ID_FRAGMENT_OF_KONDA;
  case CARD_ID_THE_KAMI_WAR:
    return CARD_ID_O_KAGACHI_MADE_MANIFEST;
  case CARD_ID_THE_LONG_REACH_OF_NIGHT:
    return CARD_ID_ANIMUS_OF_NIGHTS_REACH;
  case CARD_ID_THE_MODERN_AGE:
    return CARD_ID_VECTOR_GLIDER;
  case CARD_ID_THE_RESTORATION_OF_EIGANJO:
    return CARD_ID_ARCHITECT_OF_RESTORATION;
  case CARD_ID_THE_SHATTERED_STATES_ERA:
    return CARD_ID_NAMELESS_CONQUEROR;
  case CARD_ID_TRIBUTE_TO_HOROBI:
    return CARD_ID_ECHO_OF_DEATHS_WAIL;
  default:
    return 0;
  }
}

/* ManalinkEh 2022: 0x267dce0; original functions/functions.c:3759. */
int dfc_back_to_front(int csvid)
{
  switch (csvid)
  {
  case CARD_ID_LORD_OF_LINEAGE:
    return CARD_ID_BLOODLINE_KEEPER;
  case CARD_ID_HOMICIDAL_BRUTE:
    return CARD_ID_CIVILIZED_SCHOLAR;
  case CARD_ID_UNHOLY_FIEND:
    return CARD_ID_CLOISTERED_YOUTH;
  case CARD_ID_NIGHTFALL_PREDATOR:
    return CARD_ID_DAYBREAK_RANGER;
  case CARD_ID_INSECTILE_ABERRATION:
    return CARD_ID_DELVER_OF_SECRETS;
  case CARD_ID_GARRUK_THE_VEIL_CURSED:
    return CARD_ID_GARRUK_RELENTLESS;
  case CARD_ID_GATSTAF_HOWLER:
    return CARD_ID_GATSTAF_SHEPHERD;
  case CARD_ID_KRALLENHORDE_WANTONS:
    return CARD_ID_GRIZZLED_OUTCASTS;
  case CARD_ID_BANE_OF_HANWEIR:
    return CARD_ID_HANWEIR_WATCHKEEP;
  case CARD_ID_WILDBLOOD_PACK:
    return CARD_ID_INSTIGATOR_GANG;
  case CARD_ID_TERROR_OF_KRUIN_PASS:
    return CARD_ID_KRUIN_OUTLAW;
  case CARD_ID_LUDEVICS_ABOMINATION:
    return CARD_ID_LUDEVICS_TEST_SUBJECT;
  case CARD_ID_HOWLPACK_ALPHA:
    return CARD_ID_MAYOR_OF_AVABRUCK;
  case CARD_ID_MERCILESS_PREDATOR:
    return CARD_ID_RECKLESS_WAIF;
  case CARD_ID_STALKING_VAMPIRE:
    return CARD_ID_SCREECHING_BAT;
  case CARD_ID_THRABEN_MILITIA:
    return CARD_ID_THRABEN_SENTRY;
  case CARD_ID_RAMPAGING_WEREWOLF:
    return CARD_ID_TORMENTED_PARIAH;
  case CARD_ID_ULVENWALD_PRIMORDIALS:
    return CARD_ID_ULVENWALD_MYSTICS;
  case CARD_ID_IRONFANG:
    return CARD_ID_VILLAGE_IRONSMITH;
  case CARD_ID_HOWLPACK_OF_ESTWALD:
    return CARD_ID_VILLAGERS_OF_ESTWALD;
  case CARD_ID_WEREWOLF_RANSACKER:
    return CARD_ID_AFFLICTED_DESERTER;
  case CARD_ID_CHALICE_OF_DEATH:
    return CARD_ID_CHALICE_OF_LIFE;
  case CARD_ID_MARKOVS_SERVANT:
    return CARD_ID_CHOSEN_OF_MARKOV;
  case CARD_ID_WITHENGAR_UNBOUND:
    return CARD_ID_ELBRUS_THE_BINDING_BLADE;
  case CARD_ID_HINTERLAND_SCOURGE:
    return CARD_ID_HINTERLAND_HERMIT;
  case CARD_ID_RAVAGER_OF_THE_FELLS:
    return CARD_ID_HUNTMASTER_OF_THE_FELLS;
  case CARD_ID_SILVERPELT_WEREWOLF:
    return CARD_ID_LAMBHOLT_ELDER;
  case CARD_ID_UNHALLOWED_CATHAR:
    return CARD_ID_LOYAL_CATHAR;
  case CARD_ID_TOVOLARS_MAGEHUNTER:
    return CARD_ID_MONDRONEN_SHAMAN;
  case CARD_ID_ARCHDEMON_OF_GREED:
    return CARD_ID_RAVENOUS_DEMON;
  case CARD_ID_MOONSCARRED_WEREWOLF:
    return CARD_ID_SCORNED_VILLAGER;
  case CARD_ID_GHASTLY_HAUNTING:
    return CARD_ID_SOUL_SEIZER;
  case CARD_ID_KRALLENHORDE_KILLER:
    return CARD_ID_WOLFBITTEN_CAPTIVE;
  case CARD_ID_CHANDRA_ROARING_FLAME:
    return CARD_ID_CHANDRA_FIRE_OF_KALADESH;
  case CARD_ID_JACE_TELEPATH_UNBOUND:
    return CARD_ID_JACE_VRYNS_PRODIGY;
  case CARD_ID_GIDEON_BATTLE_FORGED:
    return CARD_ID_KYTHEON_HERO_OF_AKROS;
  case CARD_ID_LILIANA_DEFIANT_NECROMANCER:
    return CARD_ID_LILIANA_HERETICAL_HEALER;
  case CARD_ID_NISSA_SAGE_ANIMIST:
    return CARD_ID_NISSA_VASTWOOD_SEER;
  case CARD_ID_PERFECTED_FORM:
    return CARD_ID_ABERRANT_RESEARCHER;
  case CARD_ID_INFECTIOUS_CURSE:
    return CARD_ID_ACCURSED_WITCH;
  case CARD_ID_AVACYN_THE_PURIFIER:
    return CARD_ID_ARCHANGEL_AVACYN;
  case CARD_ID_ARLINN_EMBRACED_BY_THE_MOON:
    return CARD_ID_ARLINN_KORD;
  case CARD_ID_ANCIENT_OF_THE_EQUINOX:
    return CARD_ID_AUTUMNAL_GLOOM;
  case CARD_ID_LUNARCH_INQUISITORS:
    return CARD_ID_AVACYNIAN_MISSIONARIES;
  case CARD_ID_NECK_BREAKER:
    return CARD_ID_BREAKNECK_RIDER;
  case CARD_ID_BRANDED_HOWLER:
    return CARD_ID_CONVICTED_KILLER;
  case CARD_ID_BEARER_OF_OVERWHELMING_TRUTHS:
    return CARD_ID_DARING_SLEUTH;
  case CARD_ID_KRALLENHORDE_HOWLER:
    return CARD_ID_DUSKWATCH_RECRUITER;
  case CARD_ID_INSIDIOUS_MIST:
    return CARD_ID_ELUSIVE_TORMENTOR;
  case CARD_ID_GATSTAF_RAVAGERS:
    return CARD_ID_GATSTAF_ARSONISTS;
  case CARD_ID_VILDIN_PACK_ALPHA:
    return CARD_ID_GEIER_REACH_BANDIT;
  case CARD_ID_WESTVALE_CULT_LEADER:
    return CARD_ID_HANWEIR_MILITIA_CAPTAIN;
  case CARD_ID_SCROUNGED_SCYTHE:
    return CARD_ID_HARVEST_HAND;
  case CARD_ID_HEIR_TO_THE_NIGHT:
    return CARD_ID_HEIR_OF_FALKENRATH;
  case CARD_ID_LONE_WOLF_OF_THE_NATTERKNOLLS:
    return CARD_ID_HERMIT_OF_THE_NATTERKNOLLS;
  case CARD_ID_TIMBER_SHREDDER:
    return CARD_ID_HINTERLAND_LOGGER;
  case CARD_ID_FLAMEHEART_WEREWOLF:
    return CARD_ID_KESSIG_FORGEMASTER;
  case CARD_ID_DEMON_POSSESSED_WITCH:
    return CARD_ID_KINDLY_STRANGER;
  case CARD_ID_LAMBHOLT_BUTCHER:
    return CARD_ID_LAMBHOLT_PACIFIST;
  case CARD_ID_ASHMOUTH_BLADE:
    return CARD_ID_NEGLECTED_HEIRLOOM;
  case CARD_ID_WAYWARD_DISCIPLE:
    return CARD_ID_PIOUS_EVANGEL;
  case CARD_ID_WEREWOLF_OF_ANCIENT_HUNGER:
    return CARD_ID_SAGE_OF_ANCIENT_LORE;
  case CARD_ID_SKIN_SHEDDER:
    return CARD_ID_SKIN_INVASION;
  case CARD_ID_ONE_OF_THE_PACK:
    return CARD_ID_SOLITARY_HUNTER;
  case CARD_ID_PERSISTENT_NIGHTMARE:
    return CARD_ID_STARTLED_AWAKE;
  case CARD_ID_AWOKEN_HORROR:
    return CARD_ID_THING_IN_THE_ICE;
  case CARD_ID_STONEWING_ANTAGONIZER:
    return CARD_ID_THRABEN_GARGOYLE;
  case CARD_ID_INCITED_RABBLE:
    return CARD_ID_TOWN_GOSSIPMONGER;
  case CARD_ID_UNIMPEDED_TRESPASSER:
    return CARD_ID_UNINVITED_GEIST;
  case CARD_ID_MOONRISE_INTRUDER:
    return CARD_ID_VILLAGE_MESSENGER;
  case CARD_ID_ORMENDAHL_PROFANE_PRINCE:
    return CARD_ID_WESTVALE_ABBEY;
  case CARD_ID_CONDUIT_OF_EMRAKUL:
    return CARD_ID_CONDUIT_OF_STORMS;
  case CARD_ID_AURORA_OF_EMRAKUL:
    return CARD_ID_CRYPTOLITH_FRAGMENT;
  case CARD_ID_VORACIOUS_READER:
    return CARD_ID_CURIOUS_HOMUNCULUS;
  case CARD_ID_FINAL_ITERATION:
    return CARD_ID_DOCENT_OF_PERFECTION;
  case CARD_ID_EXTRICATOR_OF_FLESH:
    return CARD_ID_EXTRICATOR_OF_SIN;
  case CARD_ID_GRISLY_ANGLERFISH:
    return CARD_ID_GRIZZLED_ANGLER;
  case CARD_ID_SINUOUS_PREDATOR:
    return CARD_ID_KESSIG_PROWLER;
  case CARD_ID_IT_THAT_RIDES_AS_ONE:
    return CARD_ID_LONE_RIDER;
  case CARD_ID_HOWLING_CHORUS:
    return CARD_ID_SHRILL_HOWLER;
  case CARD_ID_ERUPTING_DREADWOLF:
    return CARD_ID_SMOLDERING_WEREWOLF;
  case CARD_ID_FIBROUS_ENTANGLER:
    return CARD_ID_TANGLECLAW_WEREWOLF;
  case CARD_ID_ULRICH_UNCONTESTED_ALPHA:
    return CARD_ID_ULRICH_OF_THE_KRALLENHORDE;
  case CARD_ID_ULVENWALD_ABOMINATION:
    return CARD_ID_ULVENWALD_CAPTIVE;
  case CARD_ID_DRONEPACK_KINDRED:
    return CARD_ID_VILDIN_PACK_OUTCAST;
  case CARD_ID_ABOLISHER_OF_BLOODLINES:
    return CARD_ID_VOLDAREN_PARIAH;
  case CARD_ID_TEMPLE_OF_ACLAZOTZ:
    return CARD_ID_ARGUELS_BLOOD_FAST;
  case CARD_ID_CONQUERORS_FOOTHOLD:
    return CARD_ID_CONQUERORS_GALLEON;
  case CARD_ID_LOST_VALE:
    return CARD_ID_DOWSING_DAGGER;
  case CARD_ID_ITLIMOC_CRADLE_OF_THE_SUN:
    return CARD_ID_GROWING_RITES_OF_ITLIMOC;
  case CARD_ID_ADANTO_THE_FIRST_FORT:
    return CARD_ID_LEGIONS_LANDING;
  case CARD_ID_PRIMAL_WELLSPRING:
    return CARD_ID_PRIMAL_AMULET;
  case CARD_ID_AZCANTA_THE_SUNKEN_RUIN:
    return CARD_ID_SEARCH_FOR_AZCANTA;
  case CARD_ID_SPIRES_OF_ORAZCA:
    return CARD_ID_THAUMATIC_COMPASS;
  case CARD_ID_TREASURE_COVE:
    return CARD_ID_TREASURE_MAP;
  case CARD_ID_SPITFIRE_BASTION:
    return CARD_ID_VANCES_BLASTING_CANNONS;
  case CARD_ID_SANCTUM_OF_THE_SUN:
    return CARD_ID_AZORS_GATEWAY;
  case CARD_ID_GOLD_FORGE_GARRISON:
    return CARD_ID_GOLDEN_GUARDIAN;
  case CARD_ID_WINGED_TEMPLE_OF_ORAZCA:
    return CARD_ID_HADANAS_CLIMB;
  case CARD_ID_ATZAL_CAVE_OF_ETERNITY:
    return CARD_ID_JOURNEY_TO_ETERNITY;
  case CARD_ID_METZALI_TOWER_OF_TRIUMPH:
    return CARD_ID_PATH_OF_METTLE;
  case CARD_ID_TOMB_OF_THE_DUSK_ROSE:
    return CARD_ID_PROFANE_PROCESSION;
  case CARD_ID_VAULT_OF_CATLACAN:
    return CARD_ID_STORM_THE_VAULT;
  case CARD_ID_NICOL_BOLAS_THE_ARISEN:
    return CARD_ID_NICOL_BOLAS_THE_RAVAGER;
  case CARD_ID_GRIMLOCK_FEROCIOUS_KING:
    return CARD_ID_GRIMLOCK_DINOBOT_LEADER;
  case CARD_ID_AGADEEM_THE_UNDERCRYPT:
    return CARD_ID_AGADEEMS_AWAKENING;
  case CARD_ID_AKOUM_TEETH:
    return CARD_ID_AKOUM_WARRIOR;
  case CARD_ID_BALA_GED_SANCTUARY:
    return CARD_ID_BALA_GED_RECOVERY;
  case CARD_ID_BEYEEN_COAST:
    return CARD_ID_BEYEEN_VEIL;
  case CARD_ID_BLACKBLOOM_BOG:
    return CARD_ID_BLACKBLOOM_ROGUE;
  case CARD_ID_BOULDERLOFT_PATHWAY:
    return CARD_ID_BRANCHLOFT_PATHWAY;
  case CARD_ID_GRIMCLIMB_PATHWAY:
    return CARD_ID_BRIGHTCLIMB_PATHWAY;
  case CARD_ID_MURKWATER_PATHWAY:
    return CARD_ID_CLEARWATER_PATHWAY;
  case CARD_ID_TIMBERCROWN_PATHWAY:
    return CARD_ID_CRAGCROWN_PATHWAY;
  case CARD_ID_EMERIA_SHATTERED_SKYCLAVE:
    return CARD_ID_EMERIAS_CALL;
  case CARD_ID_GLASSPOOL_SHORE:
    return CARD_ID_GLASSPOOL_MIMIC;
  case CARD_ID_HAGRA_BROODPIT:
    return CARD_ID_HAGRA_MAULING;
  case CARD_ID_JWARI_RUINS:
    return CARD_ID_JWARI_DISRUPTION;
  case CARD_ID_KABIRA_PLATEAU:
    return CARD_ID_KABIRA_TAKEDOWN;
  case CARD_ID_KAZANDU_VALLEY:
    return CARD_ID_KAZANDU_MAMMOTH;
  case CARD_ID_KAZUULS_CLIFFS:
    return CARD_ID_KAZUULS_FURY;
  case CARD_ID_KHALNI_TERRITORY:
    return CARD_ID_KHALNI_AMBUSH;
  case CARD_ID_MAKINDI_MESAS:
    return CARD_ID_MAKINDI_STAMPEDE;
  case CARD_ID_MALAKIR_MIRE:
    return CARD_ID_MALAKIR_REBIRTH;
  case CARD_ID_PILLARVERGE_PATHWAY:
    return CARD_ID_NEEDLEVERGE_PATHWAY;
  case CARD_ID_ONDU_SKYRUINS:
    return CARD_ID_ONDU_INVERSION;
  case CARD_ID_PELAKKA_CAVERNS:
    return CARD_ID_PELAKKA_PREDATION;
  case CARD_ID_LAVAGLIDE_PATHWAY:
    return CARD_ID_RIVERGLIDE_PATHWAY;
  case CARD_ID_SEA_GATE_REBORN:
    return CARD_ID_SEA_GATE_RESTORATION;
  case CARD_ID_SEJIRI_GLACIER:
    return CARD_ID_SEJIRI_SHELTER;
  case CARD_ID_SHATTERSKULL_THE_HAMMER_PASS:
    return CARD_ID_SHATTERSKULL_SMASHING;
  case CARD_ID_SILUNDI_ISLE:
    return CARD_ID_SILUNDI_VISION;
  case CARD_ID_SKYCLAVE_BASILICA:
    return CARD_ID_SKYCLAVE_CLERIC;
  case CARD_ID_SONG_MAD_RUINS:
    return CARD_ID_SONG_MAD_TREACHERY;
  case CARD_ID_SPIKEFIELD_CAVE:
    return CARD_ID_SPIKEFIELD_HAZARD;
  case CARD_ID_TANGLED_VALE:
    return CARD_ID_TANGLED_FLORAHEDRON;
  case CARD_ID_TURNTIMBER_SERPENTINE_WOOD:
    return CARD_ID_TURNTIMBER_SYMBIOSIS;
  case CARD_ID_UMARA_SKYFALLS:
    return CARD_ID_UMARA_WIZARD;
  case CARD_ID_VALAKUT_STONEFORGE:
    return CARD_ID_VALAKUT_AWAKENING;
  case CARD_ID_VASTWOOD_THICKET:
    return CARD_ID_VASTWOOD_FORTIFICATION;
  case CARD_ID_ZOF_BLOODBOG:
    return CARD_ID_ZOF_CONSUMPTION;
  case CARD_ID_HAKKA_WHISPERING_RAVEN:
    return CARD_ID_ALRUND_GOD_OF_THE_COSMOS;
  case CARD_ID_TIDECHANNEL_PATHWAY:
    return CARD_ID_BARKCHANNEL_PATHWAY;
  case CARD_ID_HARNFEL_HORN_OF_BOUNTY:
    return CARD_ID_BIRGI_GOD_OF_STORYTELLING;
  case CARD_ID_SEARSTEP_PATHWAY:
    return CARD_ID_BLIGHTSTEP_PATHWAY;
  case CARD_ID_THE_OMENKEEL:
    return CARD_ID_COSIMA_GOD_OF_THE_VOYAGE;
  case CARD_ID_SLITHERBORE_PATHWAY:
    return CARD_ID_DARKBORE_PATHWAY;
  case CARD_ID_THRONE_OF_DEATH:
    return CARD_ID_EGON_GOD_OF_DEATH;
  case CARD_ID_THE_PRISMATIC_BRIDGE:
    return CARD_ID_ESIKA_GOD_OF_THE_TREE;
  case CARD_ID_SWORD_OF_THE_REALMS:
    return CARD_ID_HALVAR_GOD_OF_BATTLE;
  case CARD_ID_MISTGATE_PATHWAY:
    return CARD_ID_HENGEGATE_PATHWAY;
  case CARD_ID_KALDRING_THE_RIMESTAFF:
    return CARD_ID_JORN_GOD_OF_WINTER;
  case CARD_ID_THE_RINGHART_CREST:
    return CARD_ID_KOLVORI_GOD_OF_KINSHIP;
  case CARD_ID_VALKMIRA_PROTECTORS_SHIELD:
    return CARD_ID_REIDANE_GOD_OF_THE_WORTHY;
  case CARD_ID_TERGRIDS_LANTERN:
    return CARD_ID_TERGRID_GOD_OF_FRIGHT;
  case CARD_ID_TORALFS_HAMMER:
    return CARD_ID_TORALF_GOD_OF_FURY;
  case CARD_ID_TIBALT_COSMIC_IMPOSTOR:
    return CARD_ID_VALKI_GOD_OF_LIES;
  case CARD_ID_ECHOING_EQUATION:
    return CARD_ID_AUGMENTER_PUGILIST;
  case CARD_ID_SEARCH_FOR_BLEX:
    return CARD_ID_BLEX_VEXING_PEST;
  case CARD_ID_AWAKEN_THE_BLOOD_AVATAR:
    return CARD_ID_EXTUS_ORIQ_OVERLORD;
  case CARD_ID_REVEL_IN_SILENCE:
    return CARD_ID_FLAMESCROLL_CELEBRANT;
  case CARD_ID_JOURNEY_TO_THE_ORACLE:
    return CARD_ID_JADZI_ORACLE_OF_ARCAVIOS;
  case CARD_ID_IMBRAHAM_DEAN_OF_THEORY:
    return CARD_ID_KIANNE_DEAN_OF_SUBSTANCE;
  case CARD_ID_LUKKA_WAYWARD_BONDER:
    return CARD_ID_MILA_CRAFTY_COMPANION;
  case CARD_ID_RESTORATIVE_BURST:
    return CARD_ID_PESTILENT_CAULDRON;
  case CARD_ID_AUGUSTA_DEAN_OF_ORDER:
    return CARD_ID_PLARGG_DEAN_OF_CHAOS;
  case CARD_ID_WILL_SCHOLAR_OF_FROST:
    return CARD_ID_ROWAN_SCHOLAR_OF_SPARKS;
  case CARD_ID_DEADLY_VANITY:
    return CARD_ID_SELFLESS_GLYPHWEAVER;
  case CARD_ID_EMBROSE_DEAN_OF_SHADOW:
    return CARD_ID_SHAILE_DEAN_OF_RADIANCE;
  case CARD_ID_FLAMETHROWER_SONATA:
    return CARD_ID_TORRENT_SCULPTOR;
  case CARD_ID_NASSARI_DEAN_OF_EXPRESSION:
    return CARD_ID_UVILDA_DEAN_OF_PERFECTION;
  case CARD_ID_LISETTE_DEAN_OF_THE_ROOT:
    return CARD_ID_VALENTIN_DEAN_OF_THE_VEIN;
  case CARD_ID_EXPLORE_THE_VASTLANDS:
    return CARD_ID_WANDERING_ARCHAIC;
  case CARD_ID_SEASONED_CATHAR:
    return CARD_ID_AMBITIOUS_FARMHAND;
  case CARD_ID_ARLINN_THE_MOONS_FURY:
    return CARD_ID_ARLINN_THE_PACKS_HOPE;
  case CARD_ID_HOOK_HAUNT_DRIFTER:
    return CARD_ID_BAITHOOK_ANGLER;
  case CARD_ID_BANECLAW_MARAUDER:
    return CARD_ID_BANEBLADE_SCOUNDREL;
  case CARD_ID_GENEROUS_SOUL:
    return CARD_ID_BELOVED_BEGGAR;
  case CARD_ID_DAUNTLESS_AVENGER:
    return CARD_ID_BEREAVED_SURVIVOR;
  case CARD_ID_WING_SHREDDER:
    return CARD_ID_BIRD_ADMIRER;
  case CARD_ID_MOONRAGE_BRUTE:
    return CARD_ID_BRUTAL_CATHAR;
  case CARD_ID_DIRE_STRAIN_DEMOLISHER:
    return CARD_ID_BURLY_BREAKER;
  case CARD_ID_CHAPEL_SHIELDGEIST:
    return CARD_ID_CHAPLAIN_OF_ALMS;
  case CARD_ID_COVETOUS_GEIST:
    return CARD_ID_COVERT_CUTPURSE;
  case CARD_ID_GHOSTLY_CASTIGATOR:
    return CARD_ID_COVETOUS_CASTAWAY;
  case CARD_ID_LEECHING_LURKER:
    return CARD_ID_CURSE_OF_LEECHES;
  case CARD_ID_NIGHT_TOKEN:
    return CARD_ID_DAY_TOKEN;
  case CARD_ID_DEATHBONNET_HULK:
    return CARD_ID_DEATHBONNET_SPROUT;
  case CARD_ID_DENNICK_PIOUS_APPARITION:
    return CARD_ID_DENNICK_PIOUS_APPRENTICE;
  case CARD_ID_DEPARTED_SOULKEEPER:
    return CARD_ID_DEVOTED_GRAFKEEPER;
  case CARD_ID_AWOKEN_DEMON:
    return CARD_ID_ECSTATIC_AWAKENER;
  case CARD_ID_ANGELIC_ENFORCER:
    return CARD_ID_ENDURING_ANGEL;
  case CARD_ID_FANGBLADE_EVISCERATOR:
    return CARD_ID_FANGBLADE_BRIGAND;
  case CARD_ID_EMBODIMENT_OF_FLAME:
    return CARD_ID_FLAME_CHANNELER;
  case CARD_ID_WAILDRIFTER:
    return CARD_ID_GALEDRIFTER;
  case CARD_ID_GRAVEYARD_GLUTTON:
    return CARD_ID_GRAVEYARD_TRESPASSER;
  case CARD_ID_HARVESTTIDE_ASSAILANT:
    return CARD_ID_HARVESTTIDE_INFILTRATOR;
  case CARD_ID_INHERITED_FIEND:
    return CARD_ID_HEIRLOOM_MIRROR;
  case CARD_ID_CREEPING_INN:
    return CARD_ID_HOSTILE_HOSTEL;
  case CARD_ID_UNTAMED_PUP:
    return CARD_ID_HOUND_TAMER;
  case CARD_ID_ORMENDAHL_THE_CORRUPTER:
    return CARD_ID_JERREN_CORRUPTED_BISHOP;
  case CARD_ID_LORD_OF_THE_ULVENWALD:
    return CARD_ID_KESSIG_NATURALIST;
  case CARD_ID_OLAG_LUDEVICS_HUBRIS:
    return CARD_ID_LUDEVIC_NECROGENIUS;
  case CARD_ID_LUMINOUS_PHANTOM:
    return CARD_ID_LUNARCH_VETERAN;
  case CARD_ID_BENEVOLENT_GEIST:
    return CARD_ID_MALEVOLENT_HERMIT;
  case CARD_ID_MORNING_APPARITION:
    return CARD_ID_MOURNING_PATROL;
  case CARD_ID_CHILLING_CHRONICLE:
    return CARD_ID_MYSTERIOUS_TOME;
  case CARD_ID_MYSTIC_MONSTROSITY:
    return CARD_ID_MYSTIC_SKULL;
  case CARD_ID_FRENZIED_TRAPBREAKER:
    return CARD_ID_OUTLAND_LIBERATOR;
  case CARD_ID_ARCHIVE_HAUNT:
    return CARD_ID_OVERWHELMED_ARCHIVIST;
  case CARD_ID_POPPET_FACTORY:
    return CARD_ID_POPPET_STITCHER;
  case CARD_ID_STORM_CHARGED_SLASHER:
    return CARD_ID_RECKLESS_STORMSEEKER;
  case CARD_ID_STALKING_PREDATOR:
    return CARD_ID_SHADY_TRAVELER;
  case CARD_ID_ASHMOUTH_DRAGON:
    return CARD_ID_SMOLDERING_EGG;
  case CARD_ID_SPELLRUNE_HOWLER:
    return CARD_ID_SPELLRUNE_PAINTER;
  case CARD_ID_SEAFARING_WEREWOLF:
    return CARD_ID_SUSPICIOUS_STOWAWAY;
  case CARD_ID_TAVERN_SMASHER:
    return CARD_ID_TAVERN_RUFFIAN;
  case CARD_ID_DIRE_STRAIN_BRAWLER:
    return CARD_ID_TIRELESS_HAULER;
  case CARD_ID_TOVOLARS_PACKLEADER:
    return CARD_ID_TOVOLARS_HUNTMASTER;
  case CARD_ID_TOVOLAR_THE_MIDNIGHT_SCOURGE:
    return CARD_ID_TOVOLAR_DIRE_OVERLORD;
  case CARD_ID_STRANGLING_GRASP:
    return CARD_ID_VENGEFUL_STRANGLER;
  case CARD_ID_VILLAGE_REAVERS:
    return CARD_ID_VILLAGE_WATCH;
  case CARD_ID_DEADLY_DANCER:
    return CARD_ID_ALLURING_SUITOR;
  case CARD_ID_HOLLOWHENGE_HUNTMASTER:
    return CARD_ID_AVABRUCK_CARETAKER;
  case CARD_ID_BALLISTA_WIELDER:
    return CARD_ID_BALLISTA_WATCHER;
  case CARD_ID_SPECTRAL_BINDING:
    return CARD_ID_BINDING_GEIST;
  case CARD_ID_BIOLUME_SERPENT:
    return CARD_ID_BIOLUME_EGG;
  case CARD_ID_BLOODSWORN_KNIGHT:
    return CARD_ID_BLOODSWORN_SQUIRE;
  case CARD_ID_BRINEBOUND_GIFT:
    return CARD_ID_BRINE_COMBER;
  case CARD_ID_CATAPULT_CAPTAIN:
    return CARD_ID_CATAPULT_FODDER;
  case CARD_ID_SAVAGE_PACKMATE:
    return CARD_ID_CHILD_OF_THE_PACK;
  case CARD_ID_REVEALING_EYE:
    return CARD_ID_CONCEALING_CURTAINS;
  case CARD_ID_DEPRAVED_HARVESTER:
    return CARD_ID_DESPERATE_FARMER;
  case CARD_ID_CLEVER_DISTRACTION:
    return CARD_ID_DISTRACTING_GEIST;
  case CARD_ID_GNARLED_GROVESTRIDER:
    return CARD_ID_DORMANT_GROVE;
  case CARD_ID_DOROTHEAS_RETRIBUTION:
    return CARD_ID_DOROTHEA_VENGEFUL_VICTIM;
  case CARD_ID_DROGSKOL_ARMAMENTS:
    return CARD_ID_DROGSKOL_INFANTRY;
  case CARD_ID_EDGAR_MARKOVS_COFFIN:
    return CARD_ID_EDGAR_CHARMED_GROOM;
  case CARD_ID_SINNERS_JUDGMENT:
    return CARD_ID_FAITHBOUND_JUDGE;
  case CARD_ID_FEARSOME_WEREWOLF:
    return CARD_ID_FEARFUL_VILLAGER;
  case CARD_ID_FORSAKEN_THRESHER:
    return CARD_ID_FOREBODING_STATUE;
  case CARD_ID_GUTTER_SHORTCUT:
    return CARD_ID_GUTTER_SKULKER;
  case CARD_ID_HENRIKA_INFERNAL_SEER:
    return CARD_ID_HENRIKA_DOMNATHI;
  case CARD_ID_RIPHOOK_RAIDER:
    return CARD_ID_HOOKHAND_MARINER;
  case CARD_ID_WILDSONG_HOWLER:
    return CARD_ID_HOWLPACK_PIPER;
  case CARD_ID_HOWLPACK_AVENGER:
    return CARD_ID_ILL_TEMPERED_LONER;
  case CARD_ID_INFESTED_WEREWOLF:
    return CARD_ID_INFESTATION_EXPERT;
  case CARD_ID_MALICIOUS_INVADER:
    return CARD_ID_INNOCENT_TRAVELER;
  case CARD_ID_HAUKENS_INSIGHT:
    return CARD_ID_JACOB_HAUKEN_INSPECTOR;
  case CARD_ID_KATILDAS_RISING_DAWN:
    return CARD_ID_KATILDA_DAWNHART_MARTYR;
  case CARD_ID_ANCESTORS_EMBRACE:
    return CARD_ID_KINDLY_ANCESTOR;
  case CARD_ID_LAMBHOLT_RAVAGER:
    return CARD_ID_LAMBHOLT_RACONTEUR;
  case CARD_ID_LANTERNS_LIFT:
    return CARD_ID_LANTERN_BEARER;
  case CARD_ID_GHASTLY_MIMICRY:
    return CARD_ID_MIRRORHALL_MIMIC;
  case CARD_ID_CATLIKE_CURIOSITY:
    return CARD_ID_MISCHIEVOUS_CATGEIST;
  case CARD_ID_MOONLIT_AMBUSHER:
    return CARD_ID_OAKSHADE_STALKER;
  case CARD_ID_CACKLING_CULPRIT:
    return CARD_ID_PANICKED_BYSTANDER;
  case CARD_ID_RADIANT_RESTRAINTS:
    return CARD_ID_RADIANT_GRACE;
  case CARD_ID_ODIOUS_WITCH:
    return CARD_ID_RAGGED_RECLUSE;
  case CARD_ID_BLOODSOAKED_REVELER:
    return CARD_ID_RESTLESS_BLOODSEEKER;
  case CARD_ID_KROTHUSS_LORD_OF_THE_DEEP:
    return CARD_ID_RUNO_STROMKIRK;
  case CARD_ID_CIPHERBOUND_SPIRIT:
    return CARD_ID_SOULCIPHER_BOARD;
  case CARD_ID_TWINBLADE_INVOCATION:
    return CARD_ID_TWINBLADE_GEIST;
  case CARD_ID_ULVENWALD_BEHEMOTH:
    return CARD_ID_ULVENWALD_ODDITY;
  case CARD_ID_DIRE_STRAIN_ANARCHIST:
    return CARD_ID_VOLATILE_ARSONIST;
  case CARD_ID_BLOODBAT_SUMMONER:
    return CARD_ID_VOLDAREN_BLOODCASTER;
  case CARD_ID_VOLT_CHARGED_BERSERKER:
    return CARD_ID_VOLTAIC_VISIONARY;
  case CARD_ID_WRATHFUL_JAILBREAKER:
    return CARD_ID_WEARY_PRISONER;
  case CARD_ID_BLOSSOM_CLAD_WEREWOLF:
    return CARD_ID_WEAVER_OF_BLOSSOMS;
  case CARD_ID_WEDDING_FESTIVITY:
    return CARD_ID_WEDDING_ANNOUNCEMENT;
  case CARD_ID_WEDDING_CRASHER:
    return CARD_ID_WOLFKIN_OUTCAST;
  case CARD_ID_LIKENESS_OF_THE_SEEKER:
    return CARD_ID_AZUSAS_MANY_JOURNEYS;
  case CARD_ID_IMPERIAL_MOTH:
    return CARD_ID_BEFRIENDING_THE_MOTHS;
  case CARD_ID_VISION_OF_THE_UNSPEAKABLE:
    return CARD_ID_BEHOLD_THE_UNSPEAKABLE;
  case CARD_ID_BRANCH_OF_BOSEIJU:
    return CARD_ID_BOSEIJU_REACHES_SKYWARD;
  case CARD_ID_HAND_OF_ENLIGHTENMENT:
    return CARD_ID_ERA_OF_ENLIGHTENMENT;
  case CARD_ID_REFLECTION_OF_KIKI_JIKI:
    return CARD_ID_FABLE_OF_THE_MIRROR_BREAKER;
  case CARD_ID_VESSEL_OF_THE_ALL_CONSUMING:
    return CARD_ID_HIDETSUGU_CONSUMES_ALL;
  case CARD_ID_LIVING_BREAKTHROUGH:
    return CARD_ID_INVENTIVE_ITERATION;
  case CARD_ID_REMNANT_OF_THE_RISING_STAR:
    return CARD_ID_JUGAN_DEFENDS_THE_TEMPLE;
  case CARD_ID_ETCHING_OF_KUMANO:
    return CARD_ID_KUMANO_FACES_KAKKAZAN;
  case CARD_ID_MEMORY_OF_TOSHIRO:
    return CARD_ID_LIFE_OF_TOSHIRO_UMEZAWA;
  case CARD_ID_PORTRAIT_OF_MICHIKO:
    return CARD_ID_MICHIKOS_REIGN_OF_TRUTH;
  case CARD_ID_NEZUMI_ROAD_CAPTAIN:
    return CARD_ID_OKIBA_RECKONER_RAID;
  case CARD_ID_SESHIROS_LIVING_LEGACY:
    return CARD_ID_TALES_OF_MASTER_SESHIRO;
  case CARD_ID_KIRIN_TOUCHED_OROCHI:
    return CARD_ID_TEACHINGS_OF_THE_KIRIN;
  case CARD_ID_DRAGON_KAMIS_EGG:
    return CARD_ID_THE_DRAGON_KAMI_REBORN;
  case CARD_ID_FRAGMENT_OF_KONDA:
    return CARD_ID_THE_FALL_OF_LORD_KONDA;
  case CARD_ID_O_KAGACHI_MADE_MANIFEST:
    return CARD_ID_THE_KAMI_WAR;
  case CARD_ID_ANIMUS_OF_NIGHTS_REACH:
    return CARD_ID_THE_LONG_REACH_OF_NIGHT;
  case CARD_ID_VECTOR_GLIDER:
    return CARD_ID_THE_MODERN_AGE;
  case CARD_ID_ARCHITECT_OF_RESTORATION:
    return CARD_ID_THE_RESTORATION_OF_EIGANJO;
  case CARD_ID_NAMELESS_CONQUEROR:
    return CARD_ID_THE_SHATTERED_STATES_ERA;
  case CARD_ID_ECHO_OF_DEATHS_WAIL:
    return CARD_ID_TRIBUTE_TO_HOROBI;
  default:
    return 0;
  }
}

/* ManalinkEh 2022: 0x267eed0; original functions/functions.c:5054. */
int grafdiggers_cages(int player, type_t type, tutor_from_t from)
{
  card_instance_t *instance;
  int card;
  int player_00;

  player_00 = 0;

  do
  {
    if (0 < active_cards_count[player_00])
    {
      card = 0;
      do
      {
        if ((((player == 2 || player == player_00) &&
              (instance = in_play(player_00, card), instance != NULL)) &&
             (is_humiliated(player_00, card, AL_STATIC) == 0)) &&
            (((type & (TYPE_SPELL | TYPE_CREATURE)) != TYPE_NONE &&
              ((((from & TUTOR_FROM_RFG) != 0 &&
                 (get_id(player_00, card) == 0x2f9e)) ||
                (((from & TUTOR_FROM_GRAVE) != 0 &&
                  (get_id(player_00, card) == 0x523d))))))))
        {
          return 1;
        }
        card = card + 1;
      } while (card < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
    if (player_00 == 2)
    {
      return 0;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x26955c0; original functions/functions.c:5075. */
void put_into_play_transformed(int player, int card)
{
  card_instance_t *instance;
  int result;
  card_instance_t *current_instance;

  instance = get_card_instance(player, card);
  if (instance != NULL)
  {
    result = get_id(player, card);
    instance->targets[0xd].card = result;
    instance->targets[0xd].player = result;
    true_transform(player, card);
    instance->internal_card_id = instance->targets[0xc].card;
    current_instance = get_card_instance(player, card);
    if ((get_special_flags3_by_instance(current_instance) & SF3_CARD_IS_FLIPPED) == 0)
    {
      set_special_flags3(player, card, 0x200);
    }
    else
    {
      remove_special_flags3(player, card, 0x200);
    }
    put_into_play(player, card);
    instance->regen_status = instance->regen_status | 0x1f000000;
    get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
    get_abilities(player, card, EVENT_SET_COLOR, -1);
    result = is_what(player, card, 2);
    if (result != 0)
    {
      get_abilities(player, card, EVENT_POWER, -1);
      get_abilities(player, card, EVENT_TOUGHNESS, -1);
    }
    verify_legend_rule(player, card, (unsigned int)cards_data[instance->targets[0xc].card].id);
  }
  return;
}

/* ManalinkEh 2022: 0x26792c0; original functions/functions.c:7324. */
static int no_combat_damage_this_turn_to(int player, int card, event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;

  damage = combat_damage_being_prevented(event);
  if (damage == NULL)
  {
    if (event == EVENT_CLEANUP)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((damage->damage_target_card == instance->targets[0].card) &&
        ((int)damage->damage_target_player == instance->targets[0].player))
    {
      damage->info_slot = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2681450; original functions/functions.c:7340. */
void negate_combat_damage_this_turn_to(int player, int card, int t_player, int t_card,
                                       int alternate_legacy_text_number)
{
  int legacycard;

  legacycard =
      create_targetted_legacy_effect(player, card, no_combat_damage_this_turn_to, t_player, t_card);
  if ((-1 < legacycard) && (0 < alternate_legacy_text_number))
  {
    alternate_legacy_text(alternate_legacy_text_number, player, legacycard);
    return;
  }
  return;
}

/* Retained legacy spell-trigger interface: sp_ability_trigger_impl @ 0x2684f40. */
/* Copied-spell flag read adapted to the existing source accessor.
 * The target stores this flag in card_ext_t at offset 184. */

/* End retained legacy spell-trigger interface: 2684f40. */

/* Focused October 2022 helpers using existing source flag storage. */

/* Legacy compatibility accessor: original target-slot storage.
 * The recovered extension-backed getter is below. */

/* ManalinkEh 2022: 0x2681710; original functions/functions.c:7559. */
void enters_the_battlefield_with_counters_fn(int player, int card, event_t event, counter_t type,
                                             CardPairFunction fn)
{
  card_instance_t *instance;
  int result;
  if ((((event == EVENT_CAST_SPELL) || (event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE)) &&
       (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    if ((get_special_flags2_by_instance(instance) & SF2_FACE_DOWN_DUE_TO_MANIFEST) == 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        hack_silent_counters = hack_silent_counters + 1;
        result = (*fn)(player, card);
        add_counters(player, card, player, card, type, result);
        hack_silent_counters = hack_silent_counters + -1;
        return;
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26817d0; original functions/functions.c:7590. */
int count_basic_lands(int player, int subtype)
{
  card_instance_t *instance;
  int card;
  int player_00;
  int count;

  count = 0;
  player_00 = 0;
  do
  {

    if (((player == player_00) || (player == 2)) && (0 < active_cards_count[player_00]))
    {
      card = 0;
      do
      {
        instance = in_play(player_00, card);
        if (((instance != NULL) && (is_basic_land(player_00, card) != 0)) &&
            ((subtype < 0 || (has_subtype(player_00, card, subtype) != 0))))
        {
          count = count + 1;
        }
        card = card + 1;
      } while (card < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return count;
}

/* ManalinkEh 2022: 0x269b2e0; original functions/functions.c:8011. */
int lobotomy_effect(int player, int t_player, int csvid, int max, int flags)
{
  int exiled = 0;
  int exiled_from_hand = 0;
  int completed_zones = 0;
  test_definition_t test;
  new_default_test_definition(&test, 0, "");
  test.id = csvid;
  test.no_shuffle = 1;
  do
  {
    if (ai_is_speculating != 1)
    {
      if (!is_id_in_hand(t_player, csvid))
        scnprintf(test.message, 100, "Here is %s\'s hand.", opponent_name);
      else
        scnprintf(test.message, 100, "Hand: Select a card named %s to exile.",
                  cards_ptr[csvid]->full_name);
    }
    if (hand_count[t_player] == 0)
      ++completed_zones;
    else if (hand_count[t_player] > 0 && (exiled < max || max < 0))
      do
      {
        int selected = new_global_tutor(player, t_player, 4, 6, 0, 5, &test);
        unsigned int remaining = flags & 1;
        if (selected >= 0)
        {
          ++exiled_from_hand;
          ++exiled;
          remaining = hand_count[t_player];
        }
        if (!remaining || !is_id_in_hand(t_player, csvid))
        {
          ++completed_zones;
          break;
        }
      } while (hand_count[t_player] > 0 && (exiled < max || max < 0));
    if (ai_is_speculating != 1)
      scnprintf(test.message, 100, "Graveyard: Select a card named %s to exile.",
                cards_ptr[csvid]->full_name);
    bool counted_graveyard_pass = true;
    if (new_special_count_grave(t_player, &test))
      do
      {
        int count = new_special_count_grave(t_player, &test);
        if (count < 1 || (max >= 0 && max <= exiled))
        {
          counted_graveyard_pass = false;
          break;
        }
        int selected = new_global_tutor(player, t_player, 2, 6, flags & 1U, 5, &test);
        unsigned int remaining = flags & 1U;
        if (selected >= 0)
        {
          remaining = new_special_count_grave(t_player, &test);
          ++exiled;
        }
        if (!remaining || !new_special_count_grave(t_player, &test))
          break;
      } while (true);
    if (counted_graveyard_pass)
      ++completed_zones;
    int *deck = deck_ptr[t_player];
    if (ai_is_speculating != 1)
    {
      if (!special_count_zone(t_player, deck, &test))
        scnprintf(test.message, 100, "Here is %s\'s library.", opponent_name);
      else
        scnprintf(test.message, 100, "Library: Select a card named %s to exile.",
                  cards_ptr[csvid]->full_name);
      deck = deck_ptr[t_player];
    }
    if (*deck == -1)
      ++completed_zones;
    else if (exiled < max || max < 0)
      do
      {
        int selected = new_global_tutor(player, t_player, 1, 6, 0, 5, &test);
        if (selected < 0 && !(flags & 1U))
        {
          ++completed_zones;
          break;
        }
        deck = deck_ptr[t_player];
        if (selected >= 0)
        {
          ++exiled;
          if (*deck == -1)
          {
            ++completed_zones;
            break;
          }
        }
        if (!special_count_zone(t_player, deck, &test))
        {
          ++completed_zones;
          break;
        }
      } while (*deck_ptr[t_player] != -1 && (exiled < max || max < 0));
    if (completed_zones > 2)
    {
      if (!(flags & 2U))
        shuffle(t_player);
      return flags & 4U ? exiled_from_hand : exiled;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x26884f0; original functions/functions.c:8825. */
void nobody_can_attack_this_turn_and_next_turn(int player, int card, int t_player)
{
  int legacycard;
  card_instance_t *instance;

  legacycard = create_legacy_effect(player, card, effect_nobody_can_attack_until_eot);
  if (legacycard != -1)
  {
    if (-1 < legacycard)
    {
      alternate_legacy_text(2, player, legacycard);
    }
    instance = get_card_instance(player, legacycard);
    instance->targets[1].player = t_player;
    instance->targets[1].card = 0x10;
  }
  return;
}

/* ManalinkEh 2022: 0x2679600; original functions/functions.c:9717. */
int effect_this_dies(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  unsigned int trigger_mode;
  if (trigger_condition != TRIGGER_GRAVEYARD_FROM_PLAY)
  {
    return 0;
  }
  if ((((reason_for_trigger_controller == player) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_TRIGGER)
    {
      trigger_mode = (unsigned int)instance->eot_toughness3;
      if (trigger_mode == 3)
      {
        result = duh_mode(player);
        trigger_mode = (result != 0) + 1;
      }
      event_result = event_result | trigger_mode;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (instance->eot_toughness3 != '\0')
      {
        instance->eot_toughness3 = '\0';
        result = get_internal_card_id_from_csv_id((unsigned int)instance->display_pic_csv_id);
        call_card_fn(cards_data[result].code_pointer, instance, player, card,
                     EVENT_RESOLVE_THIS_DIES_TRIGGER);
      }
    }
    else if (event == EVENT_END_TRIGGER)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2686130; original functions/functions.c:9752. */
static int this_dies_trigger_impl(int player, int card, event_t event,
                                       resolve_trigger_t trigger_mode, int who_gets_trigger,
                                       int attached, int required_kill_code)
{
  card_instance_t *instance;
  card_instance_t *dying_instance;
  int result;
  unsigned int legacy_state;
  unsigned int unused_flags;
  target_t *unused_target_begin;
  target_t *unused_target_end;
  target_t *unused_target;
  bool unused_condition;
  unsigned char unused_byte;
  int remaining_triggers;
  int dying_player;
  int dying_card;
  card_instance_t *trigger_instance;
  unused_byte = 0;
  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (attached == 0)
    {
      instance = in_play(player, card);
      dying_player = player;
      dying_card = card;
      trigger_instance = instance;
    }
    else
    {
      trigger_instance = in_play(player, card);
      if (trigger_instance == NULL)
      {
        return 0;
      }
      dying_player = (int)trigger_instance->damage_target_player;
      if (trigger_instance->damage_target_player < '\0')
      {
        return 0;
      }
      dying_card = trigger_instance->damage_target_card;
      if (dying_card < 0)
      {
        return 0;
      }
      instance = in_play(dying_player, dying_card);
      if (who_gets_trigger == 3)
      {
        who_gets_trigger = (int)trigger_instance->damage_target_player;
      }
    }
    if ((((instance == NULL) || (dying_card != affected_card)) ||
         (dying_player != affected_card_controller)) ||
        (((dying_instance = get_card_instance(dying_player, dying_card),
           (get_special_flags2_by_instance(dying_instance) & SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE) != 0 ||
               (result = check_for_death_trigger_removal(dying_player, dying_card), result != 0)) ||
          (result = is_humiliated(player, card, AL_TRIG), result != 0))))
    {
      return 0;
    }
    if (required_kill_code == 0)
    {
      if (instance->kill_code == 4)
      {
        return 0;
      }
    }
    else if ((unsigned int)instance->kill_code != required_kill_code)
    {
      return 0;
    }
    result = is_what(dying_player, dying_card, 2);
    remaining_triggers = 1;
    if (result != 0)
    {
      remaining_triggers = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
    }
    do
    {
      if (who_gets_trigger == player)
      {
        result = create_legacy_effect(who_gets_trigger, card, effect_this_dies);
      }
      else
      {
        if (1 - player != who_gets_trigger)
        {
          backtrace_and_abort("functions/functions.c:9804\nwho_gets_trigger == 1-player");
        }
        result = create_legacy_effect_for_opponent(player, card, effect_this_dies, -1, -1);
      }
      if (result == -1)
      {
        backtrace_and_abort("functions/functions.c:9808\nlegacy != -1");
      }
      instance = get_card_instance(who_gets_trigger, result);
      dying_instance = get_card_instance(dying_player, dying_card);
      if ((dying_instance->state & 0x1000) == 0)
      {
        legacy_state = instance->state & 0xffffefff;
      }
      else
      {
        legacy_state = instance->state | 0x1000;
      }
      instance->state = legacy_state;
      /* The last-known flags moved out of targets[] in 2022. Death-trigger
         callbacks still need them (for example SF_CORRECTLY_RESOLVED on Rancor). */
      *instance->ext = *trigger_instance->ext;
      memcpy(instance->targets, trigger_instance->targets, sizeof(instance->targets));
      instance->number_of_targets = trigger_instance->number_of_targets;
      instance->parent_controller = player;
      instance->parent_card = card;
      instance->eot_toughness0 = '\x01';
      instance->eot_toughness3 = (uint8_t)trigger_mode;
      instance->special_counters_byte = trigger_instance->special_counters_byte;
      instance->special_counter_type = trigger_instance->special_counter_type;
      instance->counters2 = trigger_instance->counters2;
      instance->counters3 = trigger_instance->counters3;
      instance->counters4 = trigger_instance->counters4;
      instance->counters_byte = trigger_instance->counters_byte;
      instance->counters5 = trigger_instance->counters5;
      instance->counters_m1m1 = trigger_instance->counters_m1m1;
      instance->damage_target_player = trigger_instance->damage_target_player;
      instance->damage_target_card = trigger_instance->damage_target_card;
      result = is_what(player, card, 2);
      if (result != 0)
      {
        result = get_abilities(player, card, EVENT_POWER, -1);
        instance->power = (int16_t)result;
        result = get_abilities(player, card, EVENT_TOUGHNESS, -1);
        instance->toughness = (int16_t)result;
      }
      remaining_triggers = remaining_triggers + -1;
    } while (remaining_triggers != 0);
  }
  return (unsigned int)(event == EVENT_RESOLVE_THIS_DIES_TRIGGER);
}

/* ManalinkEh 2022: 0x2689140; original functions/functions.c:9853. */
int this_is_sacrificed_trigger(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  int result;

  result = this_dies_trigger_impl(player, card, event, trigger_mode, player, 0, 3);
  return result;
}

/* ManalinkEh 2022: 0x2689170; original functions/functions.c:9857. */
int this_is_exiled_trigger(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  int result;

  result = this_dies_trigger_impl(player, card, event, trigger_mode, player, 0, 4);
  return result;
}

/* ManalinkEh 2022: 0x268f070; original functions/functions.c:11362. */
int life_total_can_change(int player)
{
  if (check_battlefield_for_id(player, CARD_ID_PLATINUM_EMPERION))
    return 0;
  return check_for_id_legacy(player, CARD_ID_TEFERIS_PROTECTION) < 0;
}

/* ManalinkEh 2022: 0x268fdf0; original functions/functions.c:12260. */
/* Only the flags=1 specialization survives in the release. */
static int how_much_life_can_i_pay_impl(int player, int flags)
{
  if (life_total_can_change(player) && !check_battlefield_for_id(2, CARD_ID_ANGEL_OF_JUBILATION))
    return life[player];
  return 0;
}

/* ManalinkEh 2022: 0x26910e0; original functions/functions.c:12271. */
int how_much_life_can_i_pay(int player)
{
  return how_much_life_can_i_pay_impl(player, 1);
}

/* ManalinkEh 2022: 0x268d9a0; original functions/functions.c:12547. */
void dat_ability(int player, int card, event_t event, declare_attackers_trigger_t mode,
                 int skip_store_attackers)
{
  int result;
  card_instance_t *instance;

  if (skip_store_attackers == 0)
  {
    store_attackers(player, card, event, mode, player, card, NULL);
  }
  result = resolve_declare_attackers_trigger(player, card, event, mode);
  if (result != 0)
  {

    if ((mode & DAT_DONT_REPORT_VALUE) == 0)
    {
      if (is_what(player, card, 0x80) == 0)
      {
        instance = get_card_instance(player, card);
        instance->info_slot = result;
      }
      else
      {
        instance = get_card_instance(player, card);
        instance->targets[1].player = result;
      }
    }
    result = count_cards_by_id(player, CARD_ID_WULFGAR_OF_ICEWIND_DALE);
    for (result = result + 1; result != 0; result = result + -1)
    {
      call_card_function(player, card, EVENT_DAT_ABILITY);
    }
  }
  if ((event == EVENT_CLEANUP) && ((mode & DAT_DONT_REPORT_VALUE) == 0))
  {

    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return;
  }
  return;
}

// Returns the first activation card currently on the stack controlled by player that was activated for {player,card}, or NULL if not found.

// Resets {player,card}'s timestamp to now.
/* ManalinkEh 2022: 0x2694000; original functions/functions.c:12637. */
void multi_x_spell(int player, int card, event_t event, int num_x)
{
  int result;

  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = played_for_free(player, card);
    if (result == 0)
    {
      result = is_token(player, card);
      if (result == 0)
      {
        x_value = x_value / num_x;
        set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
        set_special_flags2(player, card, 0x20000);
        return;
      }
    }
  }
  return;
}

// Uses {player, card}->targets[11].player.
/* ManalinkEh 2022: 0x2691b80; original functions/functions.c:14308. */
int get_toughness_sum(int player)
{
  card_instance_t *instance;
  int result;
  int player_00;
  int card;
  int total_toughness;

  total_toughness = 0;
  player_00 = 0;
  do
  {

    if (((player == player_00) || (player == 2)) && (0 < active_cards_count[player_00]))
    {
      card = 0;
      do
      {
        instance = in_play(player_00, card);
        if (instance != NULL)
        {
          result = is_what(player_00, card, 2);
          if (result != 0)
          {
            result = get_abilities(player_00, card, EVENT_TOUGHNESS, -1);
            total_toughness = total_toughness + result;
          }
        }
        card = card + 1;
      } while (card < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return total_toughness;
}

/* ManalinkEh 2022: 0x26930f0; original functions/functions.c:15061. */
int cast_counter_triggered_ability(int player, int card, int target_pos)
{
  if (stack_size < 1)
  {
    spell_fizzled = 1;
    return 0;
  }
  card_instance_t *instance = get_card_instance(player, card);
  int stack_index = stack_size - 2;
  int target_card = stack_cards[stack_index].card;
  instance->targets[target_pos].player = stack_cards[stack_index].player;
  instance->targets[target_pos].card = target_card;
  ++instance->number_of_targets;
  instance->targets[9].player = stack_phase[stack_index];
  card_instance_t *target = get_card_instance(instance->targets[target_pos].player,
                                             instance->targets[target_pos].card);
  int iid = target->internal_card_id;
  if (iid == -1)
    iid = get_card_instance(instance->targets[target_pos].player,
                            instance->targets[target_pos].card)->backup_internal_card_id;
  instance->targets[9].card = iid;
  ai_modifier += 100;
  return 1;
}
/* ManalinkEh 2022: 0x26931c0; original functions/functions.c:15087. */
int validate_counter_triggered_ability(int player, int card, target_definition_t *td,
                                       int target_pos)
{
  card_instance_t *instance;
  unsigned int can_counter;
  target_definition_t td_default;

  instance = get_card_instance(player, card);
  if (td == NULL)
  {
    counter_triggered_target_definition(player, card, &td_default, 0);
  }
  instance =
      get_card_instance(instance->targets[target_pos].player, instance->targets[target_pos].card);
  can_counter = 0;
  if ((instance->state & 0x10000000) != 0)
  {

    can_counter = (unsigned int)(cards_data[stack_data[stack_size - 1].internal_card_id].id != 0x388);
  }
  return can_counter;
}
/* ManalinkEh 2022: 0x2677990; original functions/functions.c:15108. */
static int counter_triggered_ability_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  card_instance_t *stack_instance;
  int i;

  instance = get_card_instance(player, card);
  if (0 < stack_size)
  {
    i = 0;
    do
    {
      if (((stack_instance = get_card_instance(stack_cards[i].player, stack_cards[i].card),
            stack_instance->original_internal_card_id != instance->targets[9].card ||
                (-1 < (int)stack_instance->token_status)) ||
           (stack_phase[i] != instance->targets[9].player)))
      {
        i = i + 1;
        continue;
      }
      i = i + 1;
      stack_instance->state = stack_instance->state | 0x40000000;
      kill_card(player, card, KILL_EXILE);
    } while (i < stack_size);
  }
  if ((event == EVENT_PHASE_CHANGED && instance->targets[2].player < current_phase) ||
      (event != EVENT_PHASE_CHANGED && event == EVENT_CAN_SKIP_TURN) || stack_size == 0)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2693260; original functions/functions.c:15134. */
int resolve_counter_triggered_ability(int player, int card, target_definition_t *td, int target_pos)
{
  int32_t target_iid;
  unsigned int legacy_state;
  unsigned int legacy_token_status;
  card_instance_t *instance;
  int saved_phase;
  card_instance_t *legacy_instance;

  instance = get_card_instance(player, card);
  if (validate_counter_triggered_ability(player, card, td, target_pos) != 0)
  {

    instance =
        get_card_instance(instance->targets[target_pos].player, instance->targets[target_pos].card);
    instance->state = instance->state | 0x40000000;
    return 1;
  }

  legacy_instance = get_card_instance(player, create_legacy_effect(player, card, counter_triggered_ability_legacy));
  saved_phase = current_phase;
  target_iid = instance->targets[9].card;
  legacy_instance->targets[9].player = instance->targets[9].player;
  legacy_instance->targets[2].player = saved_phase;
  legacy_state = legacy_instance->state;
  legacy_token_status = legacy_instance->token_status;
  legacy_instance->targets[9].card = target_iid;
  legacy_instance->token_status = legacy_token_status | 0x10000;
  legacy_instance->state = legacy_state | 0x22;
  return 1;
}

// Displays msg in the small horizontal window and sleeps for 2 seconds.
/* ManalinkEh 2022: 0x2678680; original functions/functions.c:15201. */
static char *has_only_1_target_and_could_legally_change_target(int who_chooses, int player,
                                                               int card)
{
  card_instance_t *instance;
  char *error;
  int result;

  instance = get_card_instance(player, card);
  error = "must have only 1 target";
  if (instance->number_of_targets == '\x01')
  {

    result = call_card_function(player, card, EVENT_CAN_CHANGE_TARGET);
    error = NULL;
    if (result == 0)
    {
      return "this spell cannot change its target";
    }
  }
  return error;
}

/* ManalinkEh 2022: 0x26934a0; original functions/functions.c:15212. */
int deflect_one_target(int player, int card, event_t event, target_definition_t *td)
{
  int32_t new_target_player;
  int player_00;
  card_instance_t *instance;
  int result;
  card_instance_t *spell_instance;
  target_definition_t td_default;

  if (td == NULL)
  {
    td = &td_default;
    counterspell_target_definition(player, card, td, 0xff);
  }
  if ((td->extra == 0) && (td->special == 0))
  {
    td->extra = (intptr_t)has_only_1_target_and_could_legally_change_target;
    td->special = 0x8000;
  }
  instance = get_card_instance(player, card);
  player_00 = card_on_stack_controller;
  result = card_on_stack;
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->targets[0].player = card_on_stack_controller;
        instance->targets[0].card = result;
        instance->number_of_targets = '\x01';
        if (counterspell_validate(player, card, td, 0) == 0)
        {
          instance->targets[0].player = -1;
          instance->targets[0].card = -1;
          spell_fizzled = 1;
        }
        else
        {
          dispatch_event_arbitrary_to_one_card(player_00, result, EVENT_CHANGE_TARGET, player_00,
                                               result, player, card);
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) &&
             (result = counterspell_validate(player, card, td, 0), result != 0))
    {
      spell_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      new_target_player = instance->targets[1].player;
      spell_instance->targets[0].card = instance->targets[1].card;
      spell_instance->targets[0].player = new_target_player;
      new_target_player = instance->targets[2].player;
      spell_instance->targets[1].card = instance->targets[2].card;
      spell_instance->targets[1].player = new_target_player;
    }
    return 0;
  }
  return counterspell(player, card, EVENT_CAN_CAST, td, 0);
}

/* ManalinkEh 2022: 0x2693670; original functions/functions.c:15290. */
int deflect_multi_targets(int player, int card, event_t event, target_definition_t *td)
{
  unsigned char target_count;
  int32_t new_target_player;
  int saved_attacking_player;
  card_instance_t *instance;
  int result;
  card_instance_t *spell_instance;
  int card_index;
  target_definition_t td_default;
  int target_index;

  if (td == NULL)
  {
    td = &td_default;
    counterspell_target_definition(player, card, td, 0xff);
  }
  instance = get_card_instance(player, card);
  result = card_on_stack_controller;
  if (event == EVENT_CAN_CAST)
  {
    return counterspell(player, card, EVENT_CAN_CAST, td, 0);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance->number_of_targets = '\x01';
      instance->targets[0].player = result;
      target_index = card_on_stack;
      instance->targets[0].card = card_on_stack;
      if (counterspell_validate(player, card, td, 0) != 0)
      {
        set_special_flags2(result, target_index, 0x40000000);
        spell_instance = get_card_instance(result, target_index);
        target_count = spell_instance->number_of_targets;
        if (target_count == 10 || target_count < 10)
        {
          instance->info_slot = (unsigned int)target_count;
        }
        else
        {
          instance->info_slot = 10;
        }
        saved_attacking_player = attacking_card_controller;
        card_index = attacking_card;
        attacking_card_controller = player;
        attacking_card = card;
        call_card_function(result, target_index, EVENT_CHANGE_TARGET);
        attacking_card = card_index;
        attacking_card_controller = saved_attacking_player;
        remove_special_flags2(result, target_index, 0x40000000);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) &&
           (result = counterspell_validate(player, card, td, 0), result != 0))
  {
    spell_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    result = instance->info_slot;
    if (0 < result)
    {
      target_index = 0;
      do
      {
        card_index = target_index + 1;
        new_target_player = instance->targets[target_index + 1].player;
        spell_instance->targets[target_index].card = instance->targets[target_index + 1].card;
        spell_instance->targets[target_index].player = new_target_player;
        new_target_player = instance->targets[2].player;
        spell_instance->targets[1].card = instance->targets[2].card;
        spell_instance->targets[1].player = new_target_player;
        target_index = card_index;
      } while (card_index != result);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26835b0; original functions/functions.c:15649. */
static int extra_combat_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_PHASE_CHANGED) && (0x1d < current_phase)) && (current_turn == player))
  {
    instance = get_card_instance(player, card);
    if (0x41 < instance->targets[1].player)
    {
      instance = get_card_instance(player, card);
      land_can_be_played = land_can_be_played & 0xfffffff7;
      current_phase = 0x15;
      instance->targets[1].player = instance->targets[1].player + -1;
    }
  }
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x26939a0; original functions/functions.c:15661. */
int extra_combat_effect(int player, int card, event_t event, int untap,
                        test_definition_t *this_test, int can_repeat)
{
  test_definition_t default_test;
  if (untap)
  {
    int whose_creatures = 2;
    if (!this_test)
    {
      this_test = &default_test;
      default_test_definition(this_test, 2);
      whose_creatures = player;
    }
    new_manipulate_all(player, card, whose_creatures, this_test, ACT_UNTAP);
  }
  bool found = false;
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    if (!in_play(player, c) || !is_what(player, c, 0x80) ||
        get_card_instance(player, c)->info_slot_fn != extra_combat_legacy)
      continue;
    if (can_repeat)
    {
      card_instance_t *instance = get_card_instance(player, c);
      ++instance->targets[1].player;
      if (current_phase == 0x1e && get_card_instance(player, c)->targets[1].player > 0x41)
      {
        instance = get_card_instance(player, c);
        land_can_be_played &= 0xfffffff7;
        current_phase = 0x15;
        --instance->targets[1].player;
      }
    }
    found = true;
  }
  if (found)
    return 0;
  int legacy_card = create_legacy_effect(player, card, extra_combat_legacy);
  if (legacy_card >= 0)
    set_legacy_image(player, 0x469, legacy_card);
  card_instance_t *instance = get_card_instance(player, legacy_card);
  instance->targets[1].player = 0x42;
  int source_id = get_id(player, card);
  bool at_end_of_combat = current_phase == 0x1e;
  instance->targets[1].card = source_id;
  if (at_end_of_combat)
  {
    --instance->targets[1].player;
    current_phase = 0x15;
    land_can_be_played &= 0xfffffff7;
  }
  return legacy_card;
}

/* ManalinkEh 2022: 0x2693e80; original functions/functions.c:15742. */
int create_card_name_legacy_unattached(int player, int card, int id)
{
  int legacy_card;
  card_instance_t *instance;

  legacy_card = create_legacy_effect_exe(player, card, LEGACY_EFFECT_ASWAN, -1, -1);
  instance = get_card_instance(player, legacy_card);
  instance->info_slot = id ^ 0xffffffff;
  return legacy_card;
}

int effect_put_into_a_graveyard_this_way(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (affect_me(instance->damage_target_player,
                  instance->damage_target_card)) // detach, so this isn't immediately buried
      instance->damage_target_player = instance->damage_target_card = -1;
  }

  if (event == EVENT_STATIC_EFFECTS ||
      (trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) &&
       reason_for_trigger_controller == player &&
       trigger_cause_controller == instance->targets[0].player &&
       trigger_cause == instance->targets[0].card))
  {
    if (instance->targets[0].player < 0 || instance->targets[0].card < 0 ||
        instance->targets[3].player == 42)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }

    // May need to hack put_on_top_of_deck(), oubliette_helper_5(), and bounce_permanent() to clear
    // STATUS_DYING and/or kill_code before triggering.
    card_instance_t *aff =
        get_card_instance(instance->targets[0].player, instance->targets[0].card);
    if (!(aff->token_status & STATUS_DYING) ||
        (aff->kill_code != KILL_DESTROY && aff->kill_code != KILL_BURY))
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }

    if (event == EVENT_TRIGGER)
      return RESOLVE_TRIGGER_MANDATORY;

    if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (check_for_death_trigger_removal(instance->targets[0].player, instance->targets[0].card))
        return 0;
      instance->targets[3].player = 42;
      return 1;
    }
  }

  if (event == EVENT_PHASE_CHANGED || event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);

  return 0;
}

/* Must set replace_milled as the very last thing after this returns true if anything is done with the card.  If test is NULL, then it defaults to all cards.
 * Only needs to be called (and thus test only needs to be set) if xtrigger_condition() == XTRIGGER_REPLACE_MILL. */

/* Must set replace_milled as the very last thing after this returns true if anything is done with the card.  If test is NULL, then it defaults to all cards.
 * Only needs to be called (and thus test only needs to be set) if xtrigger_condition() == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY. */

/* A front end to XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY.  trigger_cause_controller (whose graveyard), trigger_cause (iid of card), gy_from_anywhere_pos
 * (tentative position), and gy_from_anywhere_source (canonical graveyard_source[][] value) are all still valid.  Only needs to be called (and thus test only
 * needs to be set) if xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY. */
int when_a_card_is_put_into_graveyard_from_anywhere_but_library_do_something(
    int player, int card, int event, int t_player, resolve_trigger_t trigger_mode,
    test_definition_t *test /*optional*/)
{
  enable_xtrigger_flags |= ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;

  if (xtrigger_condition() == XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY &&
      affect_me(player, card) && reason_for_trigger_controller == player &&
      (trigger_cause_controller == t_player || t_player == ANYBODY) &&
      (!is_humiliated(player, card, AL_STATIC) ||
       (is_this_dying(trigger_cause_controller, trigger_cause) &&
        check_for_death_trigger_removal(trigger_cause_controller, trigger_cause))) &&
      (!test || new_make_test(trigger_cause_controller, trigger_cause, -1, test)))
  {
    if (event == EVENT_TRIGGER)
    {
      if (trigger_mode == RESOLVE_TRIGGER_DUH)
        trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

      event_result |= trigger_mode;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
      return 1;
  }

  return 0;
}

/* A fairly common case.  If test is NULL, then it defaults to all non-token cards.  Only needs to be called (and thus test only needs to be set) during
 * EVENT_GRAVEYARD_FROM_PLAY, XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY, and XTRIGGER_REPLACE_MILL. */

int can_play_iid(int player, int event, int iid){
	if( can_legally_play_iid(player, iid) && has_mana_to_cast_iid(player, event, iid) )
		return 1;
	return 0;
}

void cannot_attack_alone(int player, int card, event_t event){
  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.illegal_abilities = 0;
	  td.required_state = TARGET_STATE_ATTACKING;

	  if (!can_target(&td))
		event_result = 1;
	}
}

void cannot_block_alone(int player, int card, event_t event){
  if ( event == EVENT_BLOCK_LEGALITY && affect_me(player, card))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.illegal_abilities = 0;
	  td.required_state = TARGET_STATE_BLOCKING;

	  if (!can_target(&td))
		event_result = 1;
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int life_total_can_change(int player);

int nightveil_specter_like_ability(int player, int card, event_t event, unsigned int flags, unsigned int cards_to_exile, unsigned int csvid_to_display){

	int exby_flags = (flags & NSLA_MUST_PAY_MANACOST_OF_EXILED_CARDS) ? EXBY_TEST_HAS_MANA_TO_CAST : 0;

	int ddbm_flags = (flags & NSLA_EXILE_ONLY_WITH_COMBAT_DAMAGE) ? DDBM_MUST_BE_COMBAT_DAMAGE : 0;
	ddbm_flags |= (flags & NSLA_EXILE_ONLY_WHEN_DAMAGING_PLAYER) ? DDBM_MUST_DAMAGE_PLAYER : 0;

	if( damage_dealt_by_me(player, card, event, ddbm_flags) ){
		int t_player = 1-player;
		if( flags & NSLA_EXILE_FROM_PLAYERS_DECK ){
			t_player = player;
		}
		unsigned int count = 0;
		while( count < cards_to_exile && deck_ptr[t_player][0] != -1){
				exiledby_remember(player, card, t_player, deck_ptr[t_player][0], NULL, NULL);
				rfg_card_in_deck(t_player, 0);
				count++;
		}
	}

  if (event == EVENT_CAN_ACTIVATE)
	{
	  int can_cast = exiledby_choose(player, card, csvid_to_display, EXBY_CAN_CAST|EXBY_TEST_CAN_CAST|exby_flags, event, NULL, 1);
	  if (IS_AI(player) || can_cast)
		return can_cast;
	  else
		return 1;	// human can always list which cards have been exiled
	}

  if (event == EVENT_ACTIVATE)
	{
	  if (!IS_AI(player))
		{
		  int can_cast = exiledby_choose(player, card, csvid_to_display, EXBY_CAN_CAST|EXBY_TEST_CAN_CAST|exby_flags, event, NULL, 1);
		  if (DIALOG(player, card, event, DLG_NO_STORAGE, DLG_NO_CANCEL,
					 "Show exiled cards", 1, 1,
					 "Play an exiled card", can_cast, 1) == 1)
			{
			  exiledby_choose(player, card, csvid_to_display, EXBY_CHOOSE, 0, NULL, 1);
			  cancel = 1;
			  return 0;
			}
		}

	  int rval = exiledby_choose(player, card, csvid_to_display, EXBY_CHOOSE|EXBY_TEST_CAN_CAST|exby_flags, event, NULL, 1);
	  int* loc = (int*)rval;
	  if (!loc)
		spell_fizzled = 1;
	  else
		{
			int raw = *loc;
			int t_player = *loc & 0x80000000 ? 1 : 0;
			int iid = *loc & ~0x80000000;
			int csvid = cards_data[iid].id;
			*loc = -1;
			if( (exby_flags & EXBY_TEST_HAS_MANA_TO_CAST) ){
				charge_mana_from_id(player, -1, event, csvid);
			}
			if( cancel != 1 && play_card_in_exile_for_free(player, t_player, csvid) != -1){
				cant_be_responded_to = 1;	// response to the spell allowed by put_card_on_stack(), but none for this activation
			}
			else{
				*loc = raw;
				cancel = 1;
			}
		}
	}

	if( (flags & NSLA_REMOVE_STORED_CARDS_AT_EOT) && event == EVENT_CLEANUP ){
		int id = exiledby_detach(player, card);
		exiledby_destroy_detached(player, id);
	}

	return 0;
}

int card_drawn_trigger(int player, int card, event_t event, int who_draws, resolve_trigger_t trigger_mode)
{
  if (trigger_condition == TRIGGER_CARD_DRAWN && affect_me(player, card) && reason_for_trigger_controller == player
	  && (trigger_cause_controller == who_draws || who_draws == ANYBODY)
	  && in_play(player, card) && !is_humiliated(player, card, AL_TRIG))
	{
	  if (event == EVENT_TRIGGER)
		{
		  if (trigger_mode == RESOLVE_TRIGGER_DUH)
			trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;

		  event_result |= trigger_mode;
		}

	  if (event == EVENT_RESOLVE_TRIGGER)
		return 1;
	}

  return 0;
}

int beginning_of_phase(int player, int card, event_t event, int aff_player, int aff_card, int phase)
{
  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_PHASE_CHANGED && current_phase == phase && instance->eot_toughness == 0 &&
      in_play(player, card))
  {
    if ((aff_player == ANYBODY || current_turn == aff_player) &&
        (aff_card == -1 || aff_card == card))
    {
      instance->eot_toughness = 1; // prevent being asked multiple times
      return 1;
    }
  }

  if (event == EVENT_PHASE_CHANGED && current_phase != phase)
  {
    instance->eot_toughness = 0;
  }

  return 0;
}

/* ManalinkEh 2022: 0x2697910; original functions/functions.c:17156. */
int beginning_of_combat(int player, int card, event_t event, int aff_player)
{
  int result;
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((((aff_player == 2) || (current_turn == aff_player)) &&
         (result = player_has_legal_attacker(current_turn), result == 0)) &&
        (result = is_humiliated(player, card, AL_TRIG), result == 0))
    {
      event_flags = event_flags | 0x1000000;
    }
  }
  else if ((event == EVENT_BEGIN_COMBAT) &&
           (result = is_humiliated(player, card, AL_TRIG), result == 0))
  {
    return (unsigned int)(current_turn == aff_player || aff_player == 2);
  }
  return 0;
}

void permanents_enters_battlefield_tapped(int player, int card, event_t event, int t_player, int type, test_definition_t *test){
	/* If this changes the events it looks at, please grep for its uses - at least one card filters by the same events
	 * so it doesn't construct a test_definition_t every single time its function is called. */
	if( (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && ! affect_me(player, card) ){
		if( is_what(player, card, TYPE_EFFECT) || (in_play(player, card) && ! is_humiliated(player, card, AL_TRIG)) ){
			if( affected_card_controller == t_player || t_player == ANYBODY ){
				if( (type && is_what(affected_card_controller, affected_card, type)) ||
					(test != NULL && new_make_test_in_play(affected_card_controller, affected_card, -1, test))
				  ){
					if( ! check_state(affected_card_controller, affected_card, STATE_TAPPED) ){
						add_state(affected_card_controller, affected_card, STATE_TAPPED);
						if( spell_fizzled == 1 ){
							remove_state(affected_card_controller, affected_card, STATE_TAPPED);
						}
					}
				}
			}
		}
	}
}

void popup(const char* title, const char* fmt, ...)
{
  char buf[8000];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, 8000, fmt, args);
  va_end(args);

  if (duel_host_is_headless()) {
    fprintf(stderr, "DUEL_ENGINE_ERROR: %s: %s\n", title, buf);
    fflush(stderr);
    abort();
  }MessageBox(0, buf, title, MB_ICONERROR|MB_TASKMODAL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int deflect_one_target(int player, int card, event_t event, target_definition_t *td);
int deflect_multi_targets(int player, int card, event_t event, target_definition_t *td);

int recycling_land(int player, int card, event_t event, int recycling_type, int cless, int black, int blue, int green, int red, int white){

	char msg[100] = "Select a card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, recycling_type, msg);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		int ai_choice = 0;
		if( ! paying_mana() && has_mana_for_activated_ability(player, card, cless+1, black, blue, green, red, white) &&
			count_graveyard_by_type(player, recycling_type) > 0
		  ){
			ai_choice = 1;
		}

		if( ai_choice == 1){
			choice = do_dialog(player, player, card, -1, -1, " Add 1\n Recycle\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				tap_card(player, card);
				if( charge_mana_for_activated_ability(player, card, cless, black, blue, green, red, white) ){
					if( new_select_target_from_grave(player, card, player, 0, AI_MAX_VALUE, &this_test, 0) != -1 ){
						instance->info_slot = 1;
					}
					else{
						untap_card_no_event(player, card);
						spell_fizzled = 1;
					}
				}
				else{
					untap_card_no_event(player, card);
				}
		}

		else{
			spell_fizzled = 1;
		}
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot > 0 ){
				int selected = validate_target_from_grave(player, card, player, 0);
				if( selected != -1 ){
					from_graveyard_to_deck(player, selected, 1);
				}
				card_instance_t *parent = get_card_instance( instance->parent_controller, instance->parent_card );
				parent->info_slot = 0;
			}
	}
	else{
		return mana_producer(player, card, event);
	}

	return 0;
}

int card_fake_card(int player, int card, event_t event){


	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return 1;
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
			 life[player] = get_color(player, instance->parent_card);
	}

	return 0;
}

int insurrection_effect(int player, int card){

	manipulate_all(player, card, player, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0, ACT_UNTAP);
	pump_subtype_until_eot(player, card, player, -1, 0, 0, 0, SP_KEYWORD_HASTE);

	int count = active_cards_count[1-player]-1;
	int result = 0;
	while( count > -1 ){
			if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
				effect_act_of_treason(player, card, 1-player, count);
				result++;
			}
			count--;
	}
	return result;
}

static int default_tutor3(int player, int card){
	return 1;
}

int pattern_of_rebirth_tutor (int player){
	return tutor(player, TYPE_CREATURE, &default_tutor3, TUTOR_PLAY );
}

static int is_karoo(int csvid){

	switch( csvid ){
			case (CARD_ID_AZORIUS_CHANCERY):
			case (CARD_ID_BOROS_GARRISON):
			case (CARD_ID_DIMIR_AQUEDUCT):
			case (CARD_ID_GOLGARI_ROT_FARM):
			case (CARD_ID_GRUUL_TURF):
			case (CARD_ID_IZZET_BOILERWORKS):
			case (CARD_ID_NOGGLE_BRIDGEBREAKER):
			case (CARD_ID_ORZHOV_BASILICA):
			case (CARD_ID_RAKDOS_CARNARIUM):
			case (CARD_ID_SELESNYA_SANCTUARY):
			case (CARD_ID_SIMIC_GROWTH_CHAMBER):
				return 1;
				break;
			default:
				return 0;
	}
}

static const char* is_not_karoo(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( ! is_karoo(get_id(player, card)) ){
		return NULL;
	}
	return "AI helper function";
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void multi_x_spell(int player, int card, event_t event, int num_x);

int karoo(int player, int card, event_t event, int color1, int color2, int ai_card)
{
  /*
  if (player == AI && !(trace_mode & 2)){
          if (event == EVENT_CHANGE_TYPE && affect_me(player, card)){
                  event_result = get_internal_card_id_from_csv_id(ai_card);
          }
          return 0;
  }
  */
  if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);

  if (event == EVENT_CAST_SPELL && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
  {
    if (!IS_AI(player))
      return 0;
    int mod = -15;
    target_definition_t td;
    base_target_definition(player, card, &td, TYPE_LAND);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.allow_cancel = 0;
    td.extra = target_predicate_extra(is_not_karoo);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

    if (can_target(&td))
    {
      mod = 24;
    }
    td.required_state = TARGET_STATE_TAPPED;
    if (can_target(&td))
    {
      mod = 48;
    }
    ai_modifier += mod;
  }

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

  if (event == EVENT_ETB && !is_humiliated(player, card, AL_TRIG))
  {
    target_definition_t td;
    base_target_definition(player, card, &td, TYPE_LAND);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.allow_cancel = 0;
    td.illegal_abilities = 0;

    card_instance_t *instance = get_card_instance(player, card);

    if (can_target(&td))
    {
      /* The not-Karoo filter ranks AI choices; it is not a rules restriction.
         Scripted hosts must be able to select any controlled land, including
         this one. Normal gameplay and speculative AI retain the preference. */
      if (IS_AI(player) && !duel_host_scripted_input())
      {
        td.extra = target_predicate_extra(is_not_karoo);
        td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      }
      if (new_pick_target(&td, "Choose a land to bounce", 0, GS_LITERAL_PROMPT))
      {
        bounce_permanent(player, instance->targets[0].card);
      }
      if (instance->number_of_targets == 0)
      {
        base_target_definition(player, card, &td, TYPE_LAND);
        td.allowed_controller = player;
        td.preferred_controller = player;
        td.allow_cancel = 0;
        td.illegal_abilities = 0;
        if (new_pick_target(&td, "Choose a land to bounce", 0, GS_LITERAL_PROMPT))
        {
          bounce_permanent(player, instance->targets[0].card);
        }
      }
      instance->number_of_targets = 0;
    }
  }

  return two_mana_land(player, card, event, color1, color2);
}

void play_land_sound_effect(int player, int card){
	card_data_t* cd = get_card_data(player, card);
	play_land_sound_effect_force_color(player, card, cd->color);
}

void play_land_sound_effect_force_color(int player, int card, int colors){
	if (!is_what(player, card, TYPE_LAND)){
		// For e.g. Magus of the Coffers and other cards call lands' functions
		return;
	}

	colors &= COLOR_TEST_ANY_COLORED;	// so e.g. COLOR_TEST_WHITE below also includes COLOR_TEST_WHITE|COLOR_TEST_COLORLESS, for Karoo; and COLOR_TEST_ARTIFACT is treated like COLOR_TEST_COLORLESS.

	wav_t wav1 = -1, wav2 = -1;
	switch (colors){
		// colorless
		case 0:					wav1 = WAV_GREY;	break;

		// one color
		case COLOR_TEST_BLACK:	wav1 = WAV_BLACK;	break;
		case COLOR_TEST_BLUE:	wav1 = WAV_BLUE;	break;
		case COLOR_TEST_GREEN:	wav1 = WAV_GREEN;	break;
		case COLOR_TEST_RED:	wav1 = WAV_RED;		break;
		case COLOR_TEST_WHITE:	wav1 = WAV_WHITE;	break;

		// two colors
		case COLOR_TEST_WHITE|COLOR_TEST_BLUE:	wav1 = WAV_WHITEBLUE;	break;
		case COLOR_TEST_BLUE|COLOR_TEST_BLACK:	wav1 = WAV_BLUEBLACK;	break;
		case COLOR_TEST_BLACK|COLOR_TEST_RED:	wav1 = WAV_BLACKRED;	break;
		case COLOR_TEST_RED|COLOR_TEST_GREEN:	wav1 = WAV_REDGREEN;	break;
		case COLOR_TEST_GREEN|COLOR_TEST_WHITE:	wav1 = WAV_GREENWHITE;	break;

		case COLOR_TEST_WHITE|COLOR_TEST_BLACK:	wav1 = WAV_WHITEBLACK;	break;
		case COLOR_TEST_BLACK|COLOR_TEST_GREEN:	wav1 = WAV_BLACKGREEN;	break;
		case COLOR_TEST_GREEN|COLOR_TEST_BLUE:	wav1 = WAV_GREENBLUE;	break;
		case COLOR_TEST_BLUE|COLOR_TEST_RED:	wav1 = WAV_BLUERED;		break;
		case COLOR_TEST_RED|COLOR_TEST_WHITE:	wav1 = WAV_REDWHITE;	break;

		// three colors - bit of a hack
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_WHITE|COLOR_TEST_BLUE):	wav1 = WAV_RED;		wav2 = WAV_BLACKGREEN;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_BLUE|COLOR_TEST_BLACK):	wav1 = WAV_GREEN;	wav2 = WAV_REDWHITE;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_BLACK|COLOR_TEST_RED):	wav1 = WAV_WHITE;	wav2 = WAV_GREENBLUE;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_RED|COLOR_TEST_GREEN):	wav1 = WAV_BLUE;	wav2 = WAV_WHITEBLACK;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_GREEN|COLOR_TEST_WHITE):	wav1 = WAV_BLACK;	wav2 = WAV_BLUERED;		break;

		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_WHITE|COLOR_TEST_BLACK):	wav1 = WAV_BLUE;	wav2 = WAV_REDGREEN;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_BLACK|COLOR_TEST_GREEN):	wav1 = WAV_RED;		wav2 = WAV_WHITEBLUE;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_GREEN|COLOR_TEST_BLUE):	wav1 = WAV_WHITE;	wav2 = WAV_BLACKRED;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_BLUE|COLOR_TEST_RED):	wav1 = WAV_BLACK;	wav2 = WAV_GREENWHITE;	break;
		case COLOR_TEST_ANY_COLORED & ~(COLOR_TEST_RED|COLOR_TEST_WHITE):	wav1 = WAV_GREEN;	wav2 = WAV_BLUEBLACK;	break;


		// four colors - slightly less of a hack.
		// (While there aren't any naturally four-color lands that I know of, these can still be played by Reflecting Pool, Exotic Orchard, etc.)
		case COLOR_TEST_ANY_COLORED & ~COLOR_TEST_BLACK:	wav1 = WAV_REDGREEN;	wav2 = WAV_WHITEBLUE;	break;
		case COLOR_TEST_ANY_COLORED & ~COLOR_TEST_BLUE:		wav1 = WAV_BLACKRED;	wav2 = WAV_GREENWHITE;	break;
		case COLOR_TEST_ANY_COLORED & ~COLOR_TEST_GREEN:	wav1 = WAV_WHITEBLUE;	wav2 = WAV_BLACKRED;	break;
		case COLOR_TEST_ANY_COLORED & ~COLOR_TEST_RED:		wav1 = WAV_GREENWHITE;	wav2 = WAV_BLUEBLACK;	break;
		case COLOR_TEST_ANY_COLORED & ~COLOR_TEST_WHITE:	wav1 = WAV_BLUEBLACK;	wav2 = WAV_REDGREEN;	break;

		// five colors - going with gembazar, even though City of Brass is silent.  (It's better than polkamix?)
		case COLOR_TEST_ANY_COLORED:			wav1 = WAV_GEMBAZAR;	break;
	}

	if (wav1 != (wav_t)-1){
		play_sound_effect(wav1);
	}
	if (wav2 != (wav_t)-1){
		play_sound_effect(wav2);
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

void for_each_creature_damaged_by_me(int player, int card, event_t event, int flags,
                                     void (*fn)(int, int, int, int), int arg1, int arg2)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_DEAL_DAMAGE)
  {
    card_instance_t *dmg = get_card_instance(affected_card_controller, affected_card);
    if (dmg->internal_card_id == damage_card && dmg->damage_source_player == player &&
        (dmg->damage_source_card == card ||
         is_multiblocker_card_copying(player, dmg->damage_source_card, player, card)))
    {
      if (dmg->info_slot > 0 ||
          get_card_instance(dmg->damage_source_player, dmg->damage_source_card)
                  ->targets[16]
                  .player > 0)
      {
        if (dmg->damage_target_card > -1)
        {
          int good = 1;
          if ((flags & DDBM_MUST_BE_COMBAT_DAMAGE) &&
              !(dmg->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)))
          {
            good = 0;
          }
          if (good)
          {
            int pos = instance->info_slot;
            if (pos < 10)
            {
              instance->targets[pos].player = dmg->damage_target_player;
              instance->targets[pos].card = dmg->damage_target_card;
              instance->info_slot++;
            }
          }
        }
      }
    }
  }
  if (instance->info_slot > 0 && trigger_condition == TRIGGER_DEAL_DAMAGE &&
      affect_me(player, card) && reason_for_trigger_controller == player)
  {
    if (event == EVENT_TRIGGER)
    {
      event_result |= RESOLVE_TRIGGER_MANDATORY;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      int i;
      for (i = 0; i < instance->info_slot; i++)
      {
        if (in_play(instance->targets[i].player, instance->targets[i].card))
        {
          fn(arg1, arg2, instance->targets[i].player, instance->targets[i].card);
        }
      }
      instance->info_slot = 0;
    }
  }
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void dat_ability(int player, int card, event_t event, declare_attackers_trigger_t mode,
                 int skip_store_attackers);
int dfc_front_to_back(int csvid);
int dfc_back_to_front(int csvid);
int count_basic_lands(int player, int subtype);
int check_for_death_trigger_removal(int player, int card);
int extra_combat_effect(int player, int card, event_t event, int untap,
                        test_definition_t *this_test, int can_repeat);

unsigned int round_down_value(unsigned int orig_value){
	unsigned int amount = orig_value/2;
	if( amount*2 != orig_value ){
		amount = (orig_value-1)/2;
	}
	return amount;
}

unsigned int round_up_value(unsigned int orig_value){
	unsigned int amount = orig_value/2;
	if( amount*2 != orig_value ){
		amount = (orig_value+1)/2;
	}
	return amount;
}

void no_more_than_x_creatures_can_attack(int player, int card, event_t event, int t_player,
                                         int max_value)
{
  if (event == EVENT_ATTACK_LEGALITY &&
      (affected_card_controller == t_player || t_player == ANYBODY))
  {
    card_instance_t *aff = get_card_instance(affected_card_controller, affected_card);
    if (aff->state & STATE_ATTACKING)
    { // Already attacking
      return;
    }

    int num_attacking = 0;
    int c;
    for (c = 0; c < active_cards_count[affected_card_controller]; ++c)
    {
      if ((aff = in_play(affected_card_controller, c)) &&
          is_what(affected_card_controller, c, TYPE_CREATURE) && (aff->state & STATE_ATTACKING) &&
          ++num_attacking >= max_value)
      {
        event_result = 1;
        break;
      }
    }
  }
}

void no_more_than_x_creatures_can_block(int player, int card, event_t event, int t_player, int max_value){
	if( current_turn != t_player || t_player == ANYBODY ){
		card_instance_t* aff = get_card_instance(affected_card_controller, affected_card);
		if (aff->state & STATE_BLOCKING){	// Already blocking
			return;
		}

		int num_blockers = 0;
		int c;
		for (c = 0; c < active_cards_count[affected_card_controller]; ++c){
			if ((aff = in_play(affected_card_controller, c)) && (aff->state & STATE_BLOCKING) && ++num_blockers >= max_value){
				event_result = 1;
				break;
			}
		}
	}
}

int choose_a_color_and_show_legacy(int player, int card, int ai_t_player, int ret_location){
	//	return chosen color
	//	store the legacy in "targets[ret_location].card" if "ret_location" > -1.
	int clr = choose_a_color(player, get_deck_color(player, ai_t_player));
	int legacy = create_card_name_legacy(player, card, CARD_ID_BLACK+(clr-1));
	if( ret_location > -1 ){
		get_card_instance(player, card)->targets[ret_location].card = legacy;
	}
	return clr;
}

static int discard_trigger_delayed_ability(int player, int card, event_t event)
{
  if (eot_trigger(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->targets[0].player;
    int csvid = instance->targets[0].card;
    int mode = instance->targets[1].player;
    if (p > -1 && csvid > -1)
    {
      int count = count_graveyard(p) - 1;
      while (count > -1)
      {
        if (cards_data[get_grave(p)[count]].id == csvid)
        {
          if (mode & 1)
          {
            add_card_to_hand(p, get_grave(p)[count]);
            remove_card_from_grave(p, count);
          }
          if (mode & 2)
          {
            reanimate_permanent(p, -1, p, count, REANIMATE_PLUS1_PLUS1_COUNTER);
          }
          break;
        }
        count--;
      }
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

int card_madness_effect(int player, int card, event_t event){
	//Madness ruling changed, so this is no more used.

	int iid = get_card_instance(player, card)->targets[1].card;

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[1].player = current_phase;
	}

	if( get_card_instance(player, card)->targets[1].player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		if( current_phase != instance->targets[1].player ){
			if( iid > -1 ){
				int pos = find_iid_in_rfg(player, iid);
				if( pos > -1 ){
					from_exile_to_graveyard(player, pos);
				}
			}
			kill_card(player, card, KILL_SACRIFICE);
		}

		if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player &&
		    instance->targets[2].card != 66 )
		{

			int trig = 0;

			if( ! is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) ){
				trig = 1;
			}


			if( trig == 1 ){
				if( event == EVENT_TRIGGER){
					event_result |= RESOLVE_TRIGGER_MANDATORY;
				}
				else if( event == EVENT_RESOLVE_TRIGGER ){
						if( iid > -1 ){
							int pos = find_iid_in_rfg(player, iid);
							if( pos > -1 ){
								from_exile_to_graveyard(player, pos);
							}
						}
						kill_card(player, card, KILL_SACRIFICE);
				}
			}
		}

		if( event == EVENT_CAN_ACTIVATE ){
			int result = 0;
			if( iid > -1 && instance->targets[2].card != 66 ){
				int fake = add_card_to_hand(player, iid);
				add_state(player, fake, STATE_INVISIBLE);
				hand_count[player]--;
				if( call_card_function(player, fake, EVENT_CAN_PAY_MADNESS_COST) ){
					if( is_what(player, fake, TYPE_PERMANENT) ){
						result = 1;
					}
					else{
						result = call_card_function(player, fake, EVENT_CAN_CAST);
					}
				}
				obliterate_card(player, fake);
			}
			return result;
		}

		if( event == EVENT_ACTIVATE ){
			if( iid > -1 ){
				int fake = add_card_to_hand(player, iid);
				add_state(player, fake, STATE_INVISIBLE);
				hand_count[player]--;
				if( call_card_function(player, fake, EVENT_PAY_MADNESS_COST) ){
					instance->targets[2].card = 66;
					remove_state(player, fake, STATE_INVISIBLE);
					play_card_in_hand_for_free(player, fake);
					cant_be_responded_to = 1;
				}
				else{
					obliterate_card(player, fake);
				}
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			kill_card(instance->parent_controller, instance->parent_card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int madness(int player, int card, event_t event, int colorless, int black, int blue, int green,
            int red, int white)
{
  if (event == EVENT_CAN_PAY_MADNESS_COST)
  {
    int adjustment = MAX(0, true_get_updated_casting_cost(player, card, -1, EVENT_CAN_CAST, 0));
    // Preserve the native query: the adjusted sum is tested, but only the original cost is used.
    int cless = adjustment + colorless >= 0 ? MAX(0, colorless) : 0;
    if (!has_mana_multi(player, cless, black, blue, green, red, white))
      return 0;
    if (is_what(player, card, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_LAND))
      return 1;
    if (is_what(player, card, TYPE_ENCHANTMENT) && !has_subtype(player, card, SUBTYPE_AURA))
      return 1;
    return call_card_function(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_PAY_MADNESS_COST)
  {
    int penalty =
        MAX(0, true_get_updated_casting_cost(player, card, -1, EVENT_CAN_CAST, colorless)) -
        MAX(0, colorless);
    if (penalty > 0 && colorless < 0)
    {
      charge_mana_multi(player, penalty, black, blue, green, red, white);
      if (spell_fizzled == 1)
        return 0;
      charge_mana(player, COLOR_ANY, colorless);
    }
    else
    {
      charge_mana_multi(player, colorless + (penalty > 0 ? penalty : 0), black, blue, green, red,
                        white);
    }
    if (spell_fizzled != 1)
    {
      if (colorless == -1)
        set_x_for_x_spells(player, card, event, x_value);
      set_special_flags(player, card, SF3_MADNESS_COST_PAID);
      return 1;
    }
  }
  return 0;
}

void discard_card_impl(int player, int card, int player_who_controls_effect, int flags)
{
  if (player != player_who_controls_effect)
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (in_play(player, c) && get_id(player, c) == CARD_ID_TAMIYO_COLLECTOR_OF_TALES && !is_humiliated(player, c, AL_STATIC))
        return;

  int v2; // ST14_4
  int v3; // ST10_4

  v2 = trigger_cause_controller;
  v3 = trigger_cause;
  trigger_cause_controller = player;
  trigger_cause = card;
  byte_786DD4 = 0;
  dispatch_trigger_twice_once_with_each_player_as_reason(player, TRIGGER_DISCARD, str_Draw_a_card,
                                                         0);
  trigger_cause = v3;
  trigger_cause_controller = v2;
  if (byte_786DD4 != 1)
  {
    // Begin addition
    int csvid = get_id(player, card);
    int quagnoth_flag = 0;
    int madness_flag = 0;
    int falkenrath_gorger_flag = check_battlefield_for_id(player, CARD_ID_FALKENRATH_GORGER);
    if (cards_data[get_card_instance(player, card)->internal_card_id].cc[2] == 11 ||
        csvid == CARD_ID_ICHOR_SLICK ||
        (falkenrath_gorger_flag && has_subtype(player, card, SUBTYPE_VAMPIRE)))
    {
      /* "Madness - If you discard this card, discard it into exile.
       * When you do, cast it for its madness cost or put it into your graveyard."
       * We're just faking it, until some cards with "when THIS is put into exile" appears.
       */
      enum
      {
        MADNESS_MODE_STANDARD = 1,
        MADNESS_MODE_FALKENRATH_GORGER
      };

      int mode = MADNESS_MODE_STANDARD;
      int options[2] = {call_card_function(player, card, EVENT_CAN_PAY_MADNESS_COST),
                        (falkenrath_gorger_flag && has_subtype(player, card, SUBTYPE_VAMPIRE) &&
                         has_mana_to_cast_iid(player, EVENT_CAN_CAST,
                                              get_original_internal_card_id(player, card)))};
      mode = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
                    "Pay Madness cost", options[0], 10, "Use Falkenrath Goger's Madness",
                    options[1], 10, "Decline", 1, 1);

      if (mode == MADNESS_MODE_STANDARD)
      {
        if (call_card_function(player, card, EVENT_PAY_MADNESS_COST))
        {
          madness_flag = 1;
          play_card_in_hand_madness(player, card);
        }
      }
      if (mode == MADNESS_MODE_FALKENRATH_GORGER)
      {
        if (charge_mana_from_id(player, -1, EVENT_CAST_SPELL, get_id(player, card)))
        {
          madness_flag = 1;
          play_card_in_hand_madness(player, card);
        }
      }
    }
    if (player != player_who_controls_effect)
    {
      if (check_battlefield_for_id(player, CARD_ID_NEPHALIA_ACADEMY) &&
          !is_humiliated(player, card, AL_STATIC))
      {
        /* Nephalia Academy
         * If a spell or ability an opponent controls causes you to discard a card,
         * you may reveal that card and put it on top of your library instead of putting it anywhere
         * else.*/
        int na_effect = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL,
                               "Use Nephalia Academy ability", 1, 1, "Decline", 1, 1);
        if (na_effect == 1)
        {
          reveal_card(player, card, player, card);
          put_on_top_of_deck(player, card);
          return;
        }
      }

      switch (csvid)
      {
      case CARD_ID_DODECAPOD: // Replacement: it goes directly from hand to play
      {
        add_counters(player, card, player, card, COUNTER_P1_P1, 2);
        put_into_play(player, card);
      }
        return;

      case CARD_ID_LOXODON_SMITER:   // Replacement: it goes directly from hand to play
      case CARD_ID_OBSTINATE_BALOTH: // Replacement: it goes directly from hand to play
      case CARD_ID_NULLHIDE_FEROX:
      case CARD_ID_WILT_LEAF_LIEGE: // Replacement: it goes directly from hand to play
      {
        put_into_play(player, card);
        return;
      }

      case CARD_ID_GUERRILLA_TACTICS:
      {
        target_definition_t td;
        default_target_definition(player, card, &td, TYPE_CREATURE);
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        td.allow_cancel = 0;

        card_instance_t *instance = get_card_instance(player, card);

        if (can_target(&td) && pick_target(&td, "TARGET_CREATURE_OR_PLAYER"))
        {
          damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
        }
        break;
      }

      case CARD_ID_MANGARAS_BLESSING:
      {
        gain_life(player, 2);
        int legacy = create_legacy_effect(player, card, &discard_trigger_delayed_ability);
        card_instance_t *instance = get_card_instance(player, legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = csvid;
        instance->targets[1].player = 1; // Return to hand at eot.
      }
      break;

      case CARD_ID_METROGNOME:
      {
        token_generation_t token;
        default_token_definition(player, card, CARD_ID_GNOME, &token);
        token.qty = 4;
        token.pow = token.tou = 1;
        generate_token(&token);
      }
      break;

      case CARD_ID_PSYCHIC_PURGE:
        lose_life(player_who_controls_effect, 5);
        break;

      case CARD_ID_PURE_INTENTIONS:
      {
        int legacy = create_legacy_effect(player, card, &discard_trigger_delayed_ability);
        card_instance_t *instance = get_card_instance(player, legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = csvid;
        instance->targets[1].player = 1; // Return to hand at eot.
      }
      break;

      case CARD_ID_SAND_GOLEM:
      {
        int legacy = create_legacy_effect(player, card, &discard_trigger_delayed_ability);
        card_instance_t *instance = get_card_instance(player, legacy);
        instance->targets[0].player = player;
        instance->targets[0].card = csvid;
        instance->targets[1].player = 2; // Return to play with a +1/+1 counter at eot.
      }
      break;

      case CARD_ID_QUAGNOTH:
        quagnoth_flag = 1;
        break;

      default:
        break;
      }
    }
    // End addition

    dispatch_event_to_single_card(player, card, EVENT_DISCARD, 1 - player, -1);
    if (!madness_flag)
    { // If Madness was paid, the card is no more in hand, so callig this block will surely screw
      // things up.
      put_card_in_graveyard(player, card);
      get_card_instance(player, card)->internal_card_id = -1;
      call_sub_437E20_unless_ai_is_speculating();
      play_sound_effect(WAV_DISCARD);
      --hand_count[player];
    }

    // Begin addition
    if (quagnoth_flag)
    {
      seek_grave_for_id_to_reanimate(player, -1, player, csvid, REANIMATEXTRA_RETURN_TO_HAND2);
    }
    // End addition
  }
}

void discard_card(int player, int card){
	discard_card_impl(player, card, player, 0);
}

void new_discard_card(int player, int card, int player_who_controls_effect, int flags){
	discard_card_impl(player, card, player_who_controls_effect, flags);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int pump_until_eot_merge_previous_alt(int player, int card, int t_player, int t_card, int power,
                                      int toughness, int alternate_txt);

#ifndef SHANDALAR
static int effect_pump_until_eot(int player, int card, event_t event);
static int pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
    int player, int card, int t_player, int t_card, pump_ability_t *pump);

/* ManalinkEh 2022: 0x26775c0; original functions/functions.c:1219. */
static int effect_pump_until_eot(int player, int card, event_t event)
{
  short pump_amount;
  card_instance_t *instance;
  if ((uint32_t)event - (uint32_t)EVENT_POWER < 2U)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      if (event == EVENT_POWER)
      {
        pump_amount = instance->counter_power;
      }
      else
      {
        pump_amount = instance->counter_toughness;
      }
      event_result = event_result + pump_amount;
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

#endif

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int create_legacy_activate_for_opponent(int player, int card, CardFunction func_ptr, int t_player,
                                        int t_card);

void discard(int player, int flags, int player_who_controls_effect)
{
  if (player != player_who_controls_effect)
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (in_play(player, c) && get_id(player, c) == CARD_ID_TAMIYO_COLLECTOR_OF_TALES && !is_humiliated(player, c, AL_STATIC))
        return;

  if (hand_count[player] < 1 ||
      (player == AI && !(trace_mode & 2) && dword_60A4B0 + hand_count[AI] < 1))
    return;

  int random = flags & DISC_RANDOM;
  int card = -1;
  if ((player && !(trace_mode & 2)) || ai_is_speculating == 1 || random)
  {
    if ((trace_mode & 2) && player == AI)
    {
      receive_network_packet(AI, 20);
      card = dblword_628C10;
    }
    else
    {
      int found = 0;
      for (int tries = 0; tries < 999; ++tries)
      {
        card = internal_rand(active_cards_count[player]);
        card_instance_t *instance = get_card_instance(player, card);
        if (instance->internal_card_id != -1 &&
            !(instance->state & (STATE_CANNOT_TARGET | STATE_INVISIBLE | STATE_IN_PLAY)))
        {
          found = 1;
          break;
        }
      }
      if (!found)
        for (int c = 0; c < active_cards_count[player]; ++c)
        {
          card_instance_t *instance = get_card_instance(player, c);
          if (instance->internal_card_id != -1 &&
              !(instance->state & (STATE_CANNOT_TARGET | STATE_INVISIBLE | STATE_IN_PLAY)))
          {
            found = 1;
            card = c;
            break;
          }
        }
      if ((trace_mode & 2) && player == HUMAN)
      {
        if (!found)
          card = -1;
        dblword_628C10 = card;
        byte_628C0C = 20;
        send_network_packet(HUMAN, 20);
      }
    }
  }
  else
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");
    test.state = STATE_CANNOT_TARGET;
    test.state_flag = DOESNT_MATCH;
    card = new_select_a_card(player, player, TUTOR_FROM_HAND, !(flags & DISC_OPTIONAL),
                             AI_MIN_VALUE, -1, &test);
    if ((flags & DISC_OPTIONAL) && card < 0)
    {
      spell_fizzled = 1;
      return;
    }
  }

  if (!(trace_mode & 2) && player == AI && ai_is_speculating != 1)
  {
    load_text(0, "PROMPT_DISCARDACARD");
    do_dialog(AI, AI, card, -1, -1, text_lines[random ? 1 : 2], 0);
  }
  if (flags & DISC_JUST_MARK)
    get_card_instance(player, card)->state |= STATE_CANNOT_TARGET;
  else
    discard_card_impl(player, card, player_who_controls_effect, flags);
}

void new_multidiscard(int player, int quantity, int flags, int player_who_controls_effect)
{
  if ((flags & DISC_OPTIONAL) && quantity > 1)
    flags |= DISC_JUST_MARK;
  int marked = flags & DISC_JUST_MARK;
  while (quantity)
  {
    if (spell_fizzled == 1)
      break;
    discard(player, flags, player_who_controls_effect);
    --quantity;
  }

  if (marked)
  {
    if (flags & DISC_OPTIONAL)
      flags &= ~DISC_JUST_MARK;
    for (int c = 0; c < active_cards_count[player]; ++c)
    {
      card_instance_t *instance = in_hand(player, c);
      if (instance && (instance->state & STATE_CANNOT_TARGET))
      {
        if (spell_fizzled == 1)
          instance->state &= ~STATE_CANNOT_TARGET;
        else
          discard_card_impl(player, c, player_who_controls_effect, flags);
      }
    }
  }
}

/* ManalinkEh 2022: 0x269f000; original functions/functions.c:531. */
static int remove_until_upkeep_legacy(int player, int card, event_t event)
{
  upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }

  card_instance_t *instance = in_play(player, card);
  if (instance == NULL)
  {
    return 0;
  }
  int return_turn = instance->targets[1].player;
  if (return_turn != 2 && return_turn != current_turn)
  {
    return 0;
  }
  if (check_rfg(instance->targets[0].player,
                (unsigned int)cards_data[instance->targets[0].card].id) != 0)
  {

    int new_card = add_card_to_hand(instance->targets[0].player, instance->targets[0].card);
    if (has_subtype(instance->targets[0].player, new_card, SUBTYPE_AURA) != 0)
    {
      if (call_card_function(instance->targets[0].player, new_card, EVENT_CAN_CAST) == 0)
      {
        obliterate_card(instance->targets[0].player, new_card);
        kill_card(player, card, KILL_EXILE);
        return 0;
      }
      set_special_flags3(instance->targets[0].player, new_card, 0x40);
    }
    remove_card_from_rfg(instance->targets[0].player,
                         (unsigned int)cards_data[instance->targets[0].card].id);
    put_into_play(instance->targets[0].player, new_card);
    if (instance->targets[1].card == 0x42)
    {
      new_multidiscard(player,
                       get_abilities(instance->targets[0].player, new_card, EVENT_TOUGHNESS, -1), 0,
                       player);
    }
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

void multidiscard(int player, int quantity, int random){
	new_multidiscard(player, quantity, random ? DISC_RANDOM : 0, player);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void sp_ability_trigger(int controller_of_trigger, event_t event, int trigger_mode, type_t type,
                        test_definition_t *test, int triggering_player, int triggering_card);
void magecraft_trigger(int controller_of_trigger, event_t event, int trigger_mode, type_t type,
                       test_definition_t *test, int triggering_player, int triggering_card);

void new_discard_all(int player, int player_who_controls_effect){
	int p;
	for (p = 0; p < 2; p++){
		if (p == player || player == 2){
			while (hand_count[p] > 0){
				if (actually_has_a_card_in_hand(p)){
					discard(p, 0, player_who_controls_effect);
				} else {
					if (ai_is_speculating != 1){
						popup("discard_all()", "Couldn't find card in hand for %s player\nhand_count[%d] still %d", p == 0 ? "human" : "ai", p, hand_count[p]);
					}
					hand_count[p] = 0;
				}
			}
		}
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int can_counter_triggered_ability(int player, int card, event_t event, target_definition_t *td);

void discard_all(int player){
	new_discard_all(player, player);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267ab60; original functions/functions.c:1234. */
int pump_until_eot(int player, int card, int t_player, int t_card, int power, int toughness)
{
  int result;
  card_instance_t *instance;

  result =
      create_targetted_legacy_effect(player, card, effect_pump_until_eot, t_player, t_card);
  instance = get_card_instance(player, result);
  instance->counter_power = (int16_t)power;
  instance->counter_toughness = (int16_t)toughness;
  instance = in_play(t_player, t_card);
  if (instance != NULL)
  {
    instance->regen_status = instance->regen_status | 0x8000000;
    get_abilities(t_player, t_card, EVENT_ABILITIES, -1);
  }
  return result;
}

#endif

void new_timestamp(int player, int card)
{
  TENTATIVE_set_timestamps(player, card);	// set_timestamp()
  compact_timestamp_slots();	// compact_timestamps()
}

/* Retained legacy target-layout compatibility body for 0x2693060. */

/* ManalinkEh 2022: 0x267b0a0; original functions/functions.c:2043. */
target_t find_repeat_legacy_effect_alt(int player, int card, CardFunction func_ptr,
                                       int target_player_id, int target_card,
                                       int alternate_legacy_text, int whose_control)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
  {
    player = instance->parent_controller;
    card = instance->parent_card;
  }
  for (int p = 0; p < 2; ++p)
    if (whose_control == p || whose_control == 2)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        instance = in_play(p, c);
        if (instance && instance->internal_card_id == LEGACY_EFFECT_CUSTOM &&
            func_ptr == (CardFunction)instance->info_slot &&
            instance->damage_source_player == player && instance->damage_source_card == card &&
            instance->damage_target_player == target_player_id &&
            instance->damage_target_card == target_card &&
            (alternate_legacy_text < 0 || instance->eot_toughness == alternate_legacy_text))
        {
          new_timestamp(p, c);
          target_t found = {p, c};
          return found;
        }
      }
  target_t missing = {-1, -1};
  return missing;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int attached_leaves_play(int triggering_player, int triggering_card, event_t event,
                         int can_be_suppressed, int can_be_duplicated);
int test_leaves_play(int triggering_player, int triggering_card, event_t event, int controller,
                     test_definition_t *test, int can_be_suppressed, int can_be_duplicated);
int other_leaves_play_mode(int triggering_player, int triggering_card, int t_player, int t_card,
                           event_t event, resolve_trigger_t mode, int can_be_suppressed,
                           int can_be_duplicated);
int attached_leaves_play_mode(int triggering_player, int triggering_card, event_t event,
                              resolve_trigger_t mode, int can_be_suppressed, int can_be_duplicated);
int test_leaves_play_mode(int triggering_player, int triggering_card, event_t event, int controller,
                          resolve_trigger_t mode, test_definition_t *test, int can_be_suppressed,
                          int can_be_duplicated);
int end_the_turn(int player, int card, event_t event);

int
whenever_a_player_sacrifices_a_permanent(int player, int card, event_t event, int who_sacs, type_t typ, resolve_trigger_t mode)
{
  card_instance_t* inst, *aff;
  if (event == EVENT_GRAVEYARD_FROM_PLAY
	  && (affected_card_controller == who_sacs || who_sacs == ANYBODY)
	  && ((aff = in_play(affected_card_controller, affected_card)))
	  && is_what(affected_card_controller, affected_card, typ)
	  && aff->kill_code == KILL_SACRIFICE && !check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION)
	  && (inst = in_play(player, card)) && !is_humiliated(player, card, AL_TRIG))
	{
	  if (inst->targets[11].player < 0)
		inst->targets[11].player = 0;
	  ++inst->targets[11].player;
	}

  if (trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && affect_me(player, card) && player == reason_for_trigger_controller
	  && (inst = in_play(player, card)) && inst->targets[11].player > 0)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= mode;
	  else if (event == EVENT_RESOLVE_TRIGGER)
		{
		  if (--inst->targets[11].player > 0)
			inst->state &= ~STATE_PROCESSING;	// trigger again
		  return 1;
		}
	  else if (event == EVENT_END_TRIGGER)
		inst->targets[11].player = 0;
	}

  return 0;
}

static int find_target(card_instance_t* inst, int player, int card)
{
  int i;
  for (i = inst->number_of_targets - 1; i >= 0; --i)
    if (inst->targets[i].card == card && inst->targets[i].player == player)
      break;
  return i;
}

static int fx_permanents_destroyed_this_way_accumulate_condition_lambda(int player, int card)
{
  if (BYTE1(get_card_instance(player, card)->eot_toughness) & 2)	// Already mid-resolution
	return 0;

  if (card == trigger_cause && player == trigger_cause_controller)	// This effect!
	return 0;

  card_instance_t* ins = get_card_instance(player, card);
  if (ins->number_of_targets == 0)	// no targets left in play
	return 1;

  card_instance_t* tgt;
  int i;
  for (i = 0; i < ins->number_of_targets; ++i)
	if ((tgt = in_play(ins->targets[i].player, ins->targets[i].card))	// a target that's still in play
		&& (tgt->token_status & STATUS_DYING)	// and dying
		&& tgt->kill_code == KILL_DESTROY)	// due to destruction
	  return 0;

  return 1;
}

static int fx_permanents_destroyed_this_way_accumulate_triggering(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && affect_me(player, card) && player == reason_for_trigger_controller
	  && fx_permanents_destroyed_this_way_accumulate_condition_lambda(player, card))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;
	  if (event == EVENT_RESOLVE_TRIGGER)
		return 1;
	}
  return 0;
}

/* Call once for each target.  previous_effect should be NULL on the first call, else the return value of the previous call to this function.  When the last
 * target is destroyed (not counting ones that stop being destroyed for whatever reason), then the original card's function will be sent
 * EVENT_RESOLVE_GENERAL_EFFECT, with player/card set to one of the effect cards created.  The number of targets that were destroyed will be in
 * HIWORD(effect->eot_toughness).  The number of those that were controlled by player 0 will be in LOWORD(effect->targets[10].player), and the number that were
 * controlled by player 1 will be in HIWORD(effect->targets[10].player). */

void life_sucking(int player, int card, int amount){
	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allow_cancel = 0;

	int t_player = 1-player;

	if( ! would_validate_arbitrary_target(&td, t_player, -1) ){
		t_player = player;
		if( ! would_validate_arbitrary_target(&td, t_player, -1) ){
			return;
		}
	}

	lose_life(t_player, amount);
	gain_life(player, amount);
}

void display_error_message(const char* msg){
  set_centerwindow_txt(msg);
  Sleep(2000);
  set_centerwindow_txt("");
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int query(int player, int card, iid_t iid, event_query_t quer);

int you_may_spend_mana_as_though_it_were_mana_of_any_color_to_play_spell(int player, int card, event_t event, int type,
	test_definition_t *test)
{
// Cards that use this function should have the "flags:activate" set to "1" in "ct_all.csv"

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) &&
				((type && is_what(player, i, type)) || (test != NULL && new_make_test_in_play(player, i, -1, test))) )
			{
				int new_cmc = get_updated_casting_cost(player, card, -1, event, get_cmc(player, i));
				if( has_mana(player, COLOR_ANY, new_cmc) ){
					int tr = can_legally_play_card_in_hand(player, i);
					if( tr ){
						if( tr == 99 ){
							return 99;
						}
						if( can_sorcery_be_played(player, event) ||
							(cards_data[get_original_internal_card_id(player, i)].type & (TYPE_INSTANT | TYPE_INTERRUPT)) )
						{
							return 1;
						}
					}
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int playable[2][hand_count[player]];
		int pc = 0;

		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) &&
				((type && is_what(player, i, type)) || (test != NULL && new_make_test_in_play(player, i, -1, test))) )
			{
				int new_cmc = get_updated_casting_cost(player, card, -1, event, get_cmc(player, i));
				if( has_mana(player, COLOR_ANY, new_cmc) ){
					if( can_legally_play_card_in_hand(player, i) ){
						playable[0][pc] = get_card_instance(player, i)->internal_card_id;
						playable[1][pc] = i;
						pc++;
					}
				}
			}
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to play.");

		int selected = select_card_from_zone(player, player, playable[0], pc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected != -1 ){
			int new_cmc = get_updated_casting_cost(player, card, -1, event, get_cmc(player, playable[1][selected]));
			if( charge_mana(player, COLOR_ANY, new_cmc) ){
				play_card_in_hand_for_free(player, playable[1][selected]);
				cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			spell_fizzled = 1;
			return 0;
		}
	}

	return vanguard_card(player, card, event, 7, 20, 11);
}

int effect_this_dies(int player, int card, event_t event);
static int this_dies_trigger_impl(int player, int card, event_t event,
                                       resolve_trigger_t trigger_mode, int who_gets_trigger,
                                       int attached, int required_kill_code);

int is_this_dying(int player, int card){
//AKA will go into a graveyard from play, this should be checked only during EVENT_GRAVEYARD_FROM_PLAY to be realiable
	if( in_play(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->kill_code >= KILL_BURY && instance->kill_code < KILL_EXILE ){
			if( ! check_special_flags2(player, card, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE) ){
				return 1;
			}
		}
	}
	return 0;
}

/* ManalinkEh 2022: 0x26a2080; original functions/functions.c:743. */
int leaves_play(int player, int card, event_t event, int can_be_suppressed,
                     int can_be_duplicated)
{
  int result;
  int remaining_triggers;
  if (trigger_condition == TRIGGER_LEAVE_PLAY)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((((result == 0) || (can_be_suppressed == 0)) && (card == affected_card)) &&
        (((player == affected_card_controller && (trigger_cause_controller == player)) &&
          ((trigger_cause == card && (reason_for_trigger_controller == player))))))
    {
      result = is_this_dying(player, card);
      if ((result == 0) ||
          ((result = check_for_death_trigger_removal(player, card),
            result == 0 && ((((unsigned int)player_bits[player] >> 24 & 0xff) & 2) == 0))))
      {
        if (event != EVENT_TRIGGER)
        {
          if (event != EVENT_RESOLVE_TRIGGER)
          {
            return 0;
          }
          if (can_be_suppressed == 0)
          {
            return 0;
          }
          if (((can_be_duplicated == 0) ||
               (result = is_what(trigger_cause_controller, trigger_cause, 2), result == 0)) ||
              (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))
          {
            remaining_triggers = 1;
            result = remaining_triggers;
          }
          else
          {
            remaining_triggers = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
            result = remaining_triggers;
          }
          do
          {
            if (is_what(player, card, 0x80) == 0)
            {
              call_card_function(player, card, EVENT_LEAVES_PLAY_ABILITY);
            }
            else
            {
              dispatch_event_with_attacker_to_one_card(player, card, EVENT_LEAVES_PLAY_ABILITY, -1,
                                                       -1);
            }
            remaining_triggers = remaining_triggers + -1;
          } while (remaining_triggers != 0);
          return result;
        }
        if (can_be_suppressed != 0)
        {
          event_result = event_result | 2;
          return 0;
        }
      }
      else
      {
        if (can_be_suppressed != 0)
        {
          return 0;
        }
        if (event != EVENT_TRIGGER)
        {
          return 0;
        }
      }
      if (((can_be_duplicated == 0) ||
           (result = is_what(trigger_cause_controller, trigger_cause, 2), result == 0)) ||
          (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))
      {
        result = 1;
      }
      else
      {
        result = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
      }
      do
      {
        if (is_what(player, card, 0x80) == 0)
        {
          call_card_function(player, card, EVENT_LEAVES_PLAY_ABILITY);
        }
        else
        {
          dispatch_event_with_attacker_to_one_card(player, card, EVENT_LEAVES_PLAY_ABILITY, -1, -1);
        }
        result = result + -1;
      } while (result != 0);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int this_is_sacrificed_trigger(int player, int card, event_t event, resolve_trigger_t trigger_mode);

/* ManalinkEh 2022: 0x26a33f0; original functions/functions.c:799. */
int other_leaves_play_mode(int triggering_player, int triggering_card, int t_player, int t_card,
                           event_t event, resolve_trigger_t mode, int can_be_suppressed,
                           int can_be_duplicated)
{
  int result;
  int remaining_triggers;

  if (((((trigger_condition == TRIGGER_LEAVE_PLAY) && (triggering_card == affected_card)) &&
        (triggering_player == affected_card_controller)) &&
       ((trigger_cause == t_card && (trigger_cause_controller == t_player)))) &&
      (reason_for_trigger_controller == triggering_player))
  {
    result = is_humiliated(triggering_player, triggering_card, AL_TRIG);
    if (((result == 0) || (can_be_suppressed == 0)) &&
        ((result = is_this_dying(t_player, t_card),
          result == 0 || ((result = check_for_death_trigger_removal(t_player, t_card),
                          can_be_suppressed == 0 || (result == 0))))))
    {
      if (event == EVENT_TRIGGER)
      {
        if (can_be_suppressed == 0)
        {

          if (((can_be_duplicated == 0) ||
               (result = is_what(trigger_cause_controller, trigger_cause, 2), result == 0)) ||
              (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))
          {
            remaining_triggers = 1;
            result = remaining_triggers;
          }
          else
          {
            remaining_triggers = card_counts[triggering_player][CCOUNTS_TEYSA_KARLOV] + 1;
            result = remaining_triggers;
          }
          do
          {
            if (is_what(triggering_player, triggering_card, 0x80) == 0)
            {
              call_card_function(triggering_player, triggering_card, EVENT_LEAVES_PLAY_ABILITY);
            }
            remaining_triggers = remaining_triggers + -1;
          } while (remaining_triggers != 0);
          return result;
        }
        event_result = event_result | mode;
        return 0;
      }

      if ((event == EVENT_RESOLVE_TRIGGER) && (can_be_suppressed != 0))
      {
        if ((can_be_duplicated == 0) ||
            ((result = is_what(trigger_cause_controller, trigger_cause, 2),
              result == 0 ||
                  (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))))
        {
          remaining_triggers = 1;
          result = remaining_triggers;
        }
        else
        {
          remaining_triggers = card_counts[triggering_player][CCOUNTS_TEYSA_KARLOV] + 1;
          result = remaining_triggers;
        }
        do
        {
          if (is_what(triggering_player, triggering_card, 0x80) == 0)
          {
            call_card_function(triggering_player, triggering_card, EVENT_LEAVES_PLAY_ABILITY);
          }
          remaining_triggers = remaining_triggers + -1;
        } while (remaining_triggers != 0);
        return result;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26a3630; original functions/functions.c:856. */
int other_leaves_play(int triggering_player, int triggering_card, int t_player, int t_card,
                           event_t event, int can_be_suppressed, int can_be_duplicated)
{
  int result;
  result = other_leaves_play_mode(triggering_player, triggering_card, t_player, t_card, event,
                                  RESOLVE_TRIGGER_MANDATORY, can_be_suppressed, can_be_duplicated);
  return result;
}

/* ManalinkEh 2022: 0x26a3680; original functions/functions.c:861. */
int attached_leaves_play_mode(int triggering_player, int triggering_card, event_t event,
                              resolve_trigger_t mode, int can_be_suppressed, int can_be_duplicated)
{
  int result;
  card_instance_t *instance;

  if (trigger_condition != TRIGGER_LEAVE_PLAY)
  {
    return 0;
  }
  if (((triggering_card == affected_card) && (triggering_player == affected_card_controller)) &&
      (reason_for_trigger_controller == triggering_player))
  {
    result = is_humiliated(triggering_player, triggering_card, AL_TRIG);
    if ((result == 0) || (can_be_suppressed == 0))
    {

      instance = get_card_instance(triggering_player, triggering_card);
      if ((instance->damage_target_card == trigger_cause) &&
          (instance->damage_target_player == trigger_cause_controller))
      {
        result = other_leaves_play_mode(
            triggering_player, triggering_card, (int)instance->damage_target_player,
            instance->damage_target_card, event, mode, can_be_suppressed, can_be_duplicated);
        return result;
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int validate_counter_triggered_ability(int player, int card, target_definition_t *td,
                                       int target_pos);
int resolve_counter_triggered_ability(int player, int card, target_definition_t *td,
                                      int target_pos);

/* ManalinkEh 2022: 0x26a3750; original functions/functions.c:874. */
int attached_leaves_play(int triggering_player, int triggering_card, event_t event,
                         int can_be_suppressed, int can_be_duplicated)
{
  int result;

  result =
      attached_leaves_play_mode(triggering_player, triggering_card, event,
                                RESOLVE_TRIGGER_MANDATORY, can_be_suppressed, can_be_duplicated);
  return result;
}

int reanimate_csvids_at_eot_legacy(int player, int card, event_t event)
{
  if (eot_trigger(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    for (int i = 0; i < 11; ++i)
    {
      if (instance->targets[i].player >= 0 &&
          !grafdiggers_cages(
              player,
              get_type_with_iid(player, card,
                                get_internal_card_id_from_csv_id(instance->targets[i].player)),
              TUTOR_FROM_GRAVE))
        seek_grave_for_id_to_reanimate(player, card, player, instance->targets[i].player,
                                       REANIMATE_DEFAULT);
      if (instance->targets[i].card >= 0 &&
          !grafdiggers_cages(
              player,
              get_type_with_iid(player, card,
                                get_internal_card_id_from_csv_id(instance->targets[i].card)),
              TUTOR_FROM_GRAVE))
        seek_grave_for_id_to_reanimate(player, card, player, instance->targets[i].card,
                                       REANIMATE_DEFAULT);
    }
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* October 2022 extension-backed flag APIs. */
int get_special_flags_by_instance(card_instance_t *instance);
int get_special_flags(int player, int card, int type_of_special_flag);
int check_special_flags(int player, int card, int value);
void set_special_flags(int player, int card, int value);
void remove_special_flags(int player, int card, int value);
int get_special_flags2_by_instance(card_instance_t *instance);
int check_special_flags2(int player, int card, int value);
void set_special_flags2(int player, int card, int value);
void remove_special_flags2(int player, int card, int value);
int get_special_flags3_by_instance(card_instance_t *instance);
int check_special_flags3(int player, int card, int value);
void set_special_flags3(int player, int card, int value);
void remove_special_flags3(int player, int card, int value);

int whenever_i_deal_damage_to_a_player(int player, int card, int event, ddbm_flags_t mode)
{
	int result = 0;
	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | mode))
	{
		card_instance_t* instance = get_card_instance(player, card);
		SET_BYTE0(result) = BYTE0(instance->targets[1].player);
		SET_BYTE1(result) = BYTE1(instance->targets[1].player);
		instance->targets[1].player = 0;
		return result;
	}
	return 0;
}

/* ManalinkEh 2022: 0x26780e0; original functions/functions.c:19052. */
static int count_harmonic_prodigy(int player, int card)
{
  card_instance_t *instance;
  int result;
  int card_00;
  int count;

  if (active_cards_count[player] < 1)
  {
    return 0;
  }
  count = 0;
  card_00 = 0;
  do
  {
    while (((instance = in_play(player, card_00), card != card_00 && (instance != NULL)) &&
            (result = is_humiliated(player, card_00, AL_TRIG), result == 0)))
    {
      result = get_id(player, card_00);
      card_00 = card_00 + 1;
      count = count + (unsigned int)(result == 0x5a16);
      if (active_cards_count[player] <= card_00)
      {
        return count;
      }
    }
    card_00 = card_00 + 1;
  } while (card_00 < active_cards_count[player]);
  return count;
}

int whenever_a_player_plays_a_land(int player, int card, event_t event, int t_player, int trig_mode){

	if( trigger_condition == TRIGGER_SPELL_CAST || trigger_condition == TRIGGER_COMES_INTO_PLAY ){
		int trig = 0;
		// The normal case, a land played from hand
		if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
			if( is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) ){
				if( t_player == ANYBODY || t_player == trigger_cause_controller ){
					trig = 1;
				}
			}
		}
		// We're emulating a land played from hand (see Crucible of Worlds)
		if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && player == reason_for_trigger_controller ){
			if( is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) &&
				check_special_flags3(trigger_cause_controller, trigger_cause, SF3_EMULATING_LANDS_PLAYED) )
			{
				if( t_player == ANYBODY || t_player == trigger_cause_controller ){
					trig = 1;
				}
			}
		}
		if( trig ){
			if( trig_mode == RESOLVE_TRIGGER_DUH ){
				trig_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
			}
			if(event == EVENT_TRIGGER){
				event_result |= trig_mode;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					return 1;
			}
		}
	}
	return 0;
}

int spell_puts_up_to_x_target_cards_from_graveyard_on_top_of_deck(int player, int card,
                                                                  event_t event, int t_player,
                                                                  int max_targets,
                                                                  test_definition_t *test,
                                                                  int draw_else_slowtrip)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET)
  {
    if (t_player == player)
    {
      return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, test) >= 0;
    }
    if (t_player == 1 - player)
    {
      return generic_spell(player, card, event, GS_GRAVE_RECYCLER_OPP_GRAVE, NULL, NULL, 1, test) >=
             0;
    }
    if (t_player == ANYBODY)
    {
      return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1,
                           test) >= 0;
    }
  }

  if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET)
  {
    if (t_player == ANYBODY)
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      if (!new_pick_target(&td, "Select a graveyard owner", 0, 1 | GS_LITERAL_PROMPT))
      {
        return 0;
      }
      t_player = instance->targets[0].player;
      instance->number_of_targets = 0;
    }
    int pos = t_player == ANYBODY ? 1 : 0;
    int max_targ = MIN(count_graveyard_by_type(t_player, TYPE_ANY), max_targets);
    max_targ = MIN(max_targ, 10);
    int result = select_multiple_cards_from_graveyard(
        player, t_player, 0, t_player == IS_AI(player) ? AI_MAX_VALUE : AI_MIN_VALUE, test,
        max_targ, &instance->targets[pos]);
    instance->info_slot = result + pos;
  }

  if (event == EVENT_RESOLVE_SPELL)
  {
    int pos = t_player == ANYBODY ? 1 : 0;
    int validating_player = t_player == ANYBODY ? instance->targets[0].player : t_player;
    int i, num_validated = 0, selected, num_targets = 0;
    for (i = pos; i < instance->info_slot; ++i)
    {
      ++num_targets;
      if ((selected = validate_target_from_grave(player, card, validating_player, i)) != -1)
      {
        int card_added =
            add_card_to_hand(validating_player, get_grave(validating_player)[selected]);
        remove_card_from_grave(validating_player, selected);
        put_on_top_of_deck(validating_player, card_added);
        ++num_validated;
      }
    }

    if (num_validated > 1)
    {
      rearrange_top_x(t_player, player, num_validated);
    }
    if (num_validated == 0 && num_targets > 0)
    {
      spell_fizzled = 1;
    }
    else
    {
      if (draw_else_slowtrip == 1)
      {
        draw_a_card(player);
      }
      else if (draw_else_slowtrip == 2)
      {
        cantrip(player, card, 1);
      }
    }

    kill_card(player, card, KILL_DESTROY);
  }

  return 0;
}

int reanimate_test_from_any_graveyard(int player, int card, test_definition_t *test, int targeted, reanimate_mode_t mode){
	if( (new_special_count_grave(player, test) && (! targeted || ! graveyard_has_shroud(player))) ||
		(new_special_count_grave(1-player, test) && (! targeted || ! graveyard_has_shroud(1-player))) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[0].player = 1-player;
		if( new_special_count_grave(1-player, test) && (! targeted || ! graveyard_has_shroud(1-player)) ){
			if( new_special_count_grave(player, test) && (! targeted || ! graveyard_has_shroud(player)) ){
				target_definition_t td1;
				default_target_definition(player, card, &td1, TYPE_CREATURE);
				td1.zone = TARGET_ZONE_PLAYERS;
				td1.illegal_abilities = 0;
				td1.allow_cancel = 0;
				new_pick_target(&td1, "Select a graveyard owner.", 0, GS_LITERAL_PROMPT);
				instance->number_of_targets = 0;
			}
		}
		else{
			instance->targets[0].player = player;
		}
		int selected = new_select_a_card(player, instance->targets[0].player, TUTOR_FROM_GRAVE, 1, AI_MAX_CMC, -1,test);
		return reanimate_permanent(player, card, instance->targets[0].player, selected, mode);
	}
	return -1;
}

int x_spell_with_x_targets(int player, int card, event_t event, target_definition_t *td,
                           const char *msg, int additional_flags)
{

  if (event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET)
  {
    return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL | additional_flags, td,
                         NULL, 1, NULL);
  }

  if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
    int max = MIN(10, has_mana(player, COLOR_ANY, 1) - diff);
    if (check_special_flags2(player, card, SF2_COPIED_FROM_STACK))
    {
      max = instance->info_slot;
    }
    if (event == EVENT_CHANGE_TARGET)
      td->who_chooses = attacking_card_controller;
    int i = 0;
    while (i < max && can_target(td))
    {
      if (new_pick_target(td, "TARGET_CREATURE", i, (additional_flags & GS_LITERAL_PROMPT)))
      {
        state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
        i++;
      }
    }
    if (!check_special_flags2(player, card, SF2_COPIED_FROM_STACK))
    {
      instance->info_slot = instance->number_of_targets;
    }
    for (i = 0; i < instance->number_of_targets; i++)
    {
      state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
    }
    if (!check_special_flags2(player, card, SF2_COPIED_FROM_STACK))
    {
      charge_mana(player, COLOR_ANY, instance->info_slot + diff);
    }
  }

  return 0;
}

void return_cards_exiled_by_me_to_hand(int player, int card){
	int leg = 0;
	int idx = 0;
	int* loc;
	while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
			int owner = (*loc & 0x80000000) ? 1 : 0;
			int iid = *loc & ~0x80000000;
			*loc = -1;
			if( check_rfg(owner, cards_data[iid].id) ){
				add_card_to_hand(owner, iid);
				remove_card_from_rfg(owner, cards_data[iid].id);
			}
	}
}

void return_cards_exiled_by_me_to_play(int player, int card){
	int leg = 0;
	int idx = 0;
	int* loc;
	while ((loc = exiledby_find_any(player, card, &leg, &idx)) != NULL){
			int owner = (*loc & 0x80000000) ? 1 : 0;
			int iid = *loc & ~0x80000000;
			*loc = -1;
			if( check_rfg(owner, cards_data[iid].id) ){
				int card_added = add_card_to_hand(owner, iid);
				remove_card_from_rfg(owner, cards_data[iid].id);
				put_into_play(owner, card_added);
			}
	}
}

/* ManalinkEh 2022: 0x26a7dd0; original functions/functions.c:19793. */
int query(int player, int card, iid_t iid, event_query_t quer)
{
  int saved_attacking_card;
  int result;

  saved_attacking_card = attacking_card;
  attacking_card = quer;
  if (iid == -1)
  {
    result = call_card_function(player, card, EVENT_QUERY);
    attacking_card = saved_attacking_card;
    return result;
  }
  if ((player != -1) && (card == -1))
  {
    result = (*cards_data[iid].code_pointer)(player, 0, EVENT_QUERY);
    attacking_card = saved_attacking_card;
    return result;
  }
  result = (*cards_data[iid].code_pointer)(0, 0, EVENT_QUERY);
  attacking_card = saved_attacking_card;
  return result;
}

/* ManalinkEh 2022: 0x26a2e90; original functions/functions.c:19809. */
static int end_the_turn_legacy(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_PHASE_CHANGED)
  {
    if ((current_phase == 0x14) || (current_phase == 0x1e))
    {
      cant_be_responded_to = 1;
      instance = get_card_instance(player, card);
      if (instance->targets[6].card != 1)
      {
        current_phase = 0x1f;
        instance = get_card_instance(player, card);
        instance->targets[6].card = 1;
      }
    }
    suppress_draw = 1;
    land_can_be_played = land_can_be_played | 1;
    lands_played = 99;
  }
  else
  {
    suppress_draw = 1;
    land_can_be_played = land_can_be_played | 1;
    lands_played = 99;
    if (event == EVENT_STATIC_EFFECTS)
    {
      player_bits[0] = player_bits[0] | 0x3000000;
      player_bits[1] = player_bits[1] | 0x3000000;
    }
    else if (event == EVENT_ABILITIES)
    {
      result = is_what(affected_card_controller, affected_card, 0x47);
      if (result != 0)
      {
        instance = get_card_instance(affected_card_controller, affected_card);
        instance->targets[0x11].card = instance->targets[0x11].card | 0x7e;
      }
    }
  }
  leaves_play(player, card, event, 0, 0);
  result = eot_trigger(player, card, event);
  if ((event == EVENT_LEAVES_PLAY_ABILITY) || (result != 0))
  {
    cant_be_responded_to = 1;
    instance = in_play(player, card);
    if (instance != NULL)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  return 0;
}

/* Retained legacy leaves-play test interface: test_leaves_play_mode @ 0x26a3790. */

/* End retained legacy leaves-play test interface: 26a3790. */

/* Retained legacy leaves-play test interface: test_leaves_play @ 0x26a3a80. */

/* End retained legacy leaves-play test interface: 26a3a80. */

/* ManalinkEh 2022: 0x26a7e80; original functions/functions.c:19846. */
int end_the_turn(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;
  unsigned int player_00;
  unsigned int player_offset;

  result = target_player_cant_cast_type(player, card, 2, 0x1fe);
  instance = get_card_instance(player, result);
  instance->targets[2].player = 0;

  instance = get_card_instance(player, result);
  instance->token_status = instance->token_status | 0x10000;
  result = target_player_cant_cast_type(player, card, 1 - player, 0x1fe);
  instance = get_card_instance(player, result);
  instance->targets[2].player = 0;

  instance = get_card_instance(player, result);
  instance->token_status = instance->token_status | 0x10000;
  if (current_phase - 0x15U < 7)
  {

    player_offset = 0;
    do
    {
      player_00 = player_offset ^ current_turn;

      if (0 < active_cards_count[player_00])
      {
        result = 0;
        do
        {

          instance = get_card_instance(player_00, result);
          if ((instance->state & 0xc) != 0)
          {
            remove_state(player_00, result, 0xc);
          }
          result = result + 1;
        } while (result < active_cards_count[player_00]);
      }
      player_offset = player_offset + 1;
    } while (player_offset != 2);
  }
  stack_size = 0;
  result = 0;
  do
  {
    if ((stack_cards[result].player != -1) && (stack_cards[result].card != -1))
    {

      instance = get_card_instance(stack_cards[result].player, stack_cards[result].card);
      if ((instance->state & 0x10000000) == 0)
      {
        kill_card(stack_cards[result].player, stack_cards[result].card, KILL_EXILE);
      }
      else
      {
        add_state(stack_cards[result].player, stack_cards[result].card, 0x40000000);
      }
    }
    stack_phase[result] = -1;
    result = result + 1;
  } while (result != 0x20);
  result = create_legacy_effect(player, card, end_the_turn_legacy);

  instance = get_card_instance(player, result);
  current_phase = 0x1f;
  instance->token_status = instance->token_status | 0x10000;
  return 0;
}

/* Retained legacy spell-trigger interface: sp_ability_trigger @ 0x26a8120. */

/* End retained legacy spell-trigger interface: 26a8120. */

/* ManalinkEh 2022: 0x26a8160; original functions/functions.c:19968. */
void magecraft_trigger(int controller_of_trigger, event_t event, int trigger_mode, type_t type,
                       test_definition_t *test, int triggering_player, int triggering_card)
{
  sp_ability_trigger_impl(controller_of_trigger, event, trigger_mode, type, test, triggering_player,
                          triggering_card, 1);
  return;
}

/* ManalinkEh 2022: 0x26a81a0; original functions/functions.c:19973. */
char *get_color_text(int player, char *buffer, int clr)
{
  int count;
  int written;
  char *buf;
  size_t size;
  int length;
  unsigned int colors_written;
  bool has_black;

  if (ai_is_speculating != 1)
  {
    count = count_colors(player, clr);
    has_black = (clr & 2U) != 0;
    if (has_black)
    {
      length = scnprintf(buffer, 100, "black");
    }
    else
    {
      length = 0;
    }
    colors_written = (unsigned int)has_black;
    if ((clr & 4U) != 0)
    {
      if ((int)(count - colors_written) < 2)
      {
        written = scnprintf(buffer + length, 100U - length, " or ");
      }
      else
      {
        written = scnprintf(buffer + length, 100U - length, ", ");
      }
      length = length + written;
      colors_written = colors_written + 1;
      written = scnprintf(buffer + length, 100 - length, "blue");
      length = length + written;
    }
    if ((clr & 8U) != 0)
    {
      if ((int)(count - colors_written) < 2)
      {
        written = scnprintf(buffer + length, 100U - length, " or ");
      }
      else
      {
        written = scnprintf(buffer + length, 100U - length, ", ");
      }
      length = length + written;
      colors_written = colors_written + 1;
      written = scnprintf(buffer + length, 100 - length, "green");
      length = length + written;
    }
    if ((clr & 0x10U) != 0)
    {
      if ((int)(count - colors_written) < 2)
      {
        written = scnprintf(buffer + length, 100U - length, " or ");
      }
      else
      {
        written = scnprintf(buffer + length, 100U - length, ", ");
      }
      length = length + written;
      written = scnprintf(buffer + length, 100 - length, "red");
      length = length + written;
    }
    if ((clr & 0x20U) != 0)
    {
      buf = buffer + length;
      size = 100 - length;
      if (count != 1)
      {
        count = scnprintf(buf, size, "or ");
        size = 100 - (count + length);
        buf = buffer + count + length;
      }
      scnprintf(buf, size, "white");
    }
    return buffer;
  }
  return "";
}

#ifndef SHANDALAR
/* Recovered October 2022 typed consumers and compiler parts. */

/* ManalinkEh 2022: 0x267ac00; original functions/functions.c:1247. */
int pump_until_eot_merge_previous_alt(int player, int card, int t_player, int t_card, int power,
                                      int toughness, int alternate_txt)
{
  int c;
  uint16_t previous_toughness;
  card_instance_t *instance;
  int32_t source_player;
  int legacy_card;
  int source_card;

  instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
  {
    source_player = instance->parent_controller;
    source_card = instance->parent_card;
  }
  else
  {
    source_card = card;
    source_player = player;
  }
  if (0 < active_cards_count[player])
  {
    c = 0;
    do
    {
      instance = in_play(player, c);
      if (((((instance != NULL) && (instance->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
            (instance->info_slot_fn == effect_pump_until_eot)) &&
           ((instance->damage_source_player == source_player &&
             (instance->damage_source_card == source_card)))) &&
          ((instance->damage_target_player == t_player &&
            ((instance->damage_target_card == t_card &&
              (instance->eot_toughness == alternate_txt))))))
      {
        previous_toughness = instance->counter_toughness;
        instance->counter_power = instance->counter_power + (short)power;
        instance->counter_toughness = previous_toughness + (short)toughness;
        return c;
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
  }
  legacy_card = pump_until_eot(player, card, t_player, t_card, power, toughness);
  instance = get_card_instance(player, legacy_card);
  instance->eot_toughness = alternate_txt;
  return legacy_card;
}

/* ManalinkEh 2022: 0x267ad30; original functions/functions.c:1288. */
int pump_until_eot_merge_previous(int player, int card, int t_player, int t_card, int power,
                                       int toughness)
{
  int result;
  result = pump_until_eot_merge_previous_alt(player, card, t_player, t_card, power, toughness, 0);
  return result;
}

#endif

#ifndef SHANDALAR

/* ManalinkEh 2022: 0x267a730; original functions/functions.c:1293. */
void default_pump_ability_definition(int player, int card, pump_ability_t *pump, int pow,
                                          int tou, int key, int skey)
{
  pump->source_p = player;
  pump->source_c = card;
  pump->pow_mod = pow;
  pump->tou_mod = tou;
  pump->key_mod = key;
  pump->skey_mod = skey;
  pump->paue_flags = 0;
  pump->can_block_additional_creatures = 0;
  pump->eot_removal_method = 0;
  pump->subtype = SUBTYPE_INVALID;
  pump->color = COLOR_TEST_0;
  pump->disabling_mode = 0;
  return;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 ability writers and Aura pump consumers. */

/* ManalinkEh 2022: 0x26a2390; original functions/functions.c:1308. */
int legacy_effect_pump_ability_until_eot(int player, int card, event_t event)
{
  subtype_t subtype;
  color_test_t color;
  int target_player;
  bool remove_effect;
  bool timed_out;
  card_instance_t *instance;
  unsigned int flags;
  card_instance_t *target_instance;
  int result;
  kill_t kill_mode;

  instance = get_card_instance(player, card);
  if (instance->targets[3].player & PAUE_END_IF_TARGET_CHANGES_CONTROL)
  {
    event_flags |= EA_CONTROLLED;
    if (instance->damage_target_player != instance->targets[9].player ||
        instance->targets[9].card == -2)
    {
      /* Latch expiry during queries; remove outside the characteristic cache. */
      instance->targets[9].card = -2;
      if (event == EVENT_STATIC_EFFECTS || event == EVENT_CARDCONTROLLED || event == EVENT_CLEANUP)
        kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  flags = instance->targets[1].player;
  if ((((uint32_t)event - (uint32_t)EVENT_POWER < 3U) &&
       (instance->targets[0].card == affected_card)) &&
      (instance->targets[0].player == affected_card_controller))
  {
    if (event == EVENT_POWER)
    {
      event_result = instance->counter_power + event_result;
    }
    else if (event == EVENT_TOUGHNESS)
    {
      event_result = instance->counter_toughness + event_result;
    }
    else if (instance->targets[4].player == 1)
    {
      event_result = event_result | flags;
    }
    else
    {
      event_result = (flags ^ 0xffffffff) & event_result;
    }
  }
  if (flags == 0xffffffff)
  {
    instance->targets[1].player = 0;
  }
  result = instance->targets[1].card;
  if (result == -1)
  {
    instance->targets[1].card = 0;
  }
  else if (result != 0)
  {
    special_abilities(instance->targets[0].player, instance->targets[0].card, event, result, player,
                      card);
  }
  flags = instance->targets[4].card;
  if (0 < (int)flags)
  {
    if ((flags & 3) == 3)
    {
      result = instance->targets[0].player;
      spirit_link_effect(result, instance->targets[0].card, event, result);
    }
    else
    {
      if ((flags & 1) != 0)
      {
        spirit_link_effect(instance->targets[0].player, instance->targets[0].card, event, 0);
        flags = instance->targets[4].card;
      }
      if ((flags & 2) != 0)
      {
        spirit_link_effect(instance->targets[0].player, instance->targets[0].card, event, 1);
      }
    }
  }
  flags = instance->targets[3].player;
  if ((int)flags > 0)
  {
    if (flags & 0x10)
    {
      cannot_attack(instance->targets[0].player, instance->targets[0].card, event);
      flags = instance->targets[3].player;
    }
    bool follow_source_leaving = (flags & 8) != 0;
    if (flags & 0x20)
    {
      leaves_play(instance->targets[0].player, instance->targets[0].card, event, 0, 0);
      if (event == EVENT_LEAVES_PLAY_ABILITY)
      {
        if (instance->targets[0].player == trigger_cause_controller &&
            instance->targets[0].card == trigger_cause &&
            in_play(instance->damage_source_player, instance->damage_source_card))
        {
          kill_mode = instance->targets[3].card;
          if (3U < (uint32_t)kill_mode - (uint32_t)KILL_BURY)
            kill_mode = KILL_SACRIFICE;
          kill_card(instance->damage_source_player, instance->damage_source_card, kill_mode);
        }
        follow_source_leaving = (instance->targets[3].player & 8) != 0;
      }
      else
      {
        flags = instance->targets[3].player;
        follow_source_leaving = (flags & 8) != 0;
      }
    }
    if (follow_source_leaving)
    {
      leaves_play(instance->damage_source_player, instance->damage_source_card, event, 0, 0);
      if (event == EVENT_LEAVES_PLAY_ABILITY &&
          instance->damage_source_player == trigger_cause_controller &&
          instance->damage_source_card == trigger_cause &&
          in_play(instance->targets[0].player, instance->targets[0].card))
      {
        kill_mode = instance->targets[3].card;
        if (3U < (uint32_t)kill_mode - (uint32_t)KILL_BURY)
          kill_mode = KILL_SACRIFICE;
        kill_card(instance->targets[0].player, instance->targets[0].card, kill_mode);
      }
    }
  }
  result = instance->targets[5].player;
  if (0 < result)
  {
    event_flags = event_flags | 0x100000;
    attached_creature_can_block_additional(player, card, event, result);
  }
  subtype = instance->targets[8].player;
  flags = instance->targets[3].player;
  if (0 < (int)subtype)
  {
    if ((flags & 0x40000) == 0)
    {
      attached_is_additional_subtype(player, card, event, subtype);
      flags = instance->targets[3].player;
    }
    else
    {
      attached_is_forced_subtype(player, card, event, subtype);
      flags = instance->targets[3].player;
    }
  }
  if (0 < instance->targets[8].card)
  {
    /* Native 2022 code tests the color slot, then reads this subtype slot. */
    color = instance->targets[8].player;
    if ((flags & 0x20000) == 0)
    {
      attached_is_additional_color(player, card, event, color);
      flags = instance->targets[3].player;
    }
    else
    {
      attached_is_forced_color(player, card, event, color);
      flags = instance->targets[3].player;
    }
  }
  timed_out = false;
  if (flags & 0x200)
  {
    bool expiration_event;
    if (!(instance->targets[1].card & 0x10000000U) && !(flags & 0x4000))
      expiration_event = event == EVENT_CLEANUP;
    else
    {
      result = eot_trigger(player, card, event);
      flags = instance->targets[3].player;
      expiration_event = result != 0;
    }
    if (expiration_event)
    {
      result = instance->targets[7].player;
      timed_out = result <= 0;
      if (result > 0)
        instance->targets[7].player = result - 1;
    }
  }
  remove_effect = timed_out;
  if ((flags & 0x40) != 0)
  {
    result = end_of_combat_trigger(player, card, event, 2);
    flags = instance->targets[3].player;
    remove_effect = true;
    if (result == 0)
    {
      remove_effect = timed_out;
    }
  }
  if ((flags & 0x180) != 0)
  {
    leaves_play(instance->targets[6].player, instance->targets[6].card, event, 0, 0);
    flags = instance->targets[3].player;
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      remove_effect = true;
    }
  }
  bool source_untapping = false;
  if (flags & 0x80)
  {
    result = instance->targets[6].player;
    player_bits[result] |= 4;
    source_untapping = event == EVENT_UNTAP_CARD && instance->targets[6].card == affected_card &&
                       result == affected_card_controller;
  }
  if (source_untapping || ((flags & 0x8000) && current_turn == player && current_phase == 1) ||
      remove_effect)
  {
    target_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
    if ((target_instance->state & 0x800000) == 0)
    {
      flags = instance->targets[3].player;
      if ((flags & 4) != 0)
      {
        humiliate(player, card, instance->targets[0].player, instance->targets[0].card, 0);
        flags = instance->targets[3].player;
      }
      if ((flags & 2) != 0)
      {
        disable_all_activated_abilities(instance->targets[0].player, instance->targets[0].card, 0);
        flags = instance->targets[3].player;
      }
      if ((flags & 1) != 0)
      {
        disable_nonmana_activated_abilities(instance->targets[0].player, instance->targets[0].card,
                                            0);
      }
      if (((instance->targets[1].card & 0x10000000) != 0) ||
          ((instance->targets[3].player & 0x4000) != 0))
      {
        kill_mode = instance->targets[3].card;
        result = instance->targets[0].card;
        target_player = instance->targets[0].player;
        if (kill_mode == (KILL_EXILE | KILL_BURY))
        {
          bounce_permanent(target_player, result);
        }
        else
        {
          kill_card(target_player, result, kill_mode);
        }
      }
    }
    kill_card(player, card, KILL_EXILE);
  }
  flags = instance->targets[5].card;
  return flags & (int)(flags ^ 0xffffffff) >> 0x1f;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 pump effects and cleanup helpers. */
static int effect_pump_until_eot(int player, int card, event_t event);
static int pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
    int player, int card, int t_player, int t_card, pump_ability_t *pump);

/* ManalinkEh 2022: 0x267a790; original functions/functions.c:1480. */
void get_sleighted_hacked_suppressed_ability(int player, int card, int t_player, int *p_ability,
                                             int *p_sp_ability)
{
  unsigned int ability = *p_ability;
  unsigned int special_ability = *p_sp_ability;
  if (ability & 0x1000f81f)
  {
    if (ability & 0x10000000)
      ability &= 0xefffffff;
    else
    {
      unsigned int protection = ability & 0xf800;
      if (protection)
      {
        ability &= 0xffff07ff;
        for (unsigned int color = 0; color < 5; ++color)
          if (protection & (0x800U << color))
          {
            card_instance_t *instance = get_card_instance(player, card);
            unsigned int replacement = color + 1;
            if (instance->color_id[color + 1])
              replacement = instance->color_id[color + 1];
            ability |= 0x800U << (((char)replacement - 1U) & 0x1f);
          }
      }
      unsigned int landwalk = ability & 0x1f;
      if (landwalk)
      {
        ability &= 0xffffffe0;
        for (unsigned int land = 0; land < 5; ++land)
          if (landwalk & (1U << land))
          {
            card_instance_t *instance = get_card_instance(player, card);
            unsigned int replacement = land + 1;
            if (instance->hack_mode[land + 1])
              replacement = instance->hack_mode[land + 1];
            ability |= 1U << (((char)replacement - 1U) & 0x1f);
          }
      }
    }
  }
  unsigned int restrictions = player_bits[t_player];
  if (restrictions & 0x1001f0)
  {
    if (restrictions & 0x10)
      ability &= 0xfffffeff;
    if (restrictions & 0x20)
      ability &= 0xffffffdf;
    if (restrictions & 0x40)
      special_ability &= 0xfffffdff;
    if (restrictions & 0x80)
      ability &= 0xffffff7f;
    if (restrictions & 0x100000)
      ability &= 0xfffdffff;
    if (restrictions & 0x100)
      special_ability &= 0xff7fffff;
  }
  *p_ability = ability;
  *p_sp_ability = special_ability;
}

/* ManalinkEh 2022: 0x267ad70; original functions/functions.c:1533. */
static int pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
    int player, int card, int t_player, int t_card, pump_ability_t *pump)
{
  keyword_t keywords;
  subtype_t added_subtype;
  color_test_t added_color;
  int legacy_card;
  card_instance_t *instance;

  if ((pump->paue_flags & 0x10000) != 0)
  {
    untap_card(t_player, t_card);
  }
  legacy_card = create_targetted_legacy_effect(player, card, legacy_effect_pump_ability_until_eot,
                                           t_player, t_card);
  instance = get_card_instance(player, legacy_card);
  instance->number_of_targets = '\x01';
  instance->targets[0].player = t_player;
  keywords = pump->key_mod;
  instance->targets[0].card = t_card;
  instance->targets[1].player = keywords;
  instance->targets[1].card = pump->skey_mod;
  instance->counter_power = (int16_t)pump->pow_mod;
  instance->counter_toughness = (int16_t)pump->tou_mod;
  instance->targets[3].player = pump->paue_flags;
  instance->targets[3].card = pump->eot_removal_method;
  instance->targets[5].player = pump->can_block_additional_creatures;
  instance->targets[6].player = pump->source_p;
  instance->targets[6].card = pump->source_c;
  added_subtype = pump->subtype;
  added_color = pump->color;
  instance->targets[4].player = 1;
  instance->targets[8].player = added_subtype;
  instance->targets[8].card = added_color;
  if (pump->paue_flags & PAUE_END_IF_TARGET_CHANGES_CONTROL)
  {
    instance->targets[9].player = t_player;
    instance->targets[9].card = -1;
    event_flags |= EA_CONTROLLED;
  }
  instance = in_play(t_player, t_card);
  if (instance != NULL)
  {
    instance->regen_status = instance->regen_status | 0x8000000;
    get_abilities(t_player, t_card, EVENT_ABILITIES, -1);
  }
  return legacy_card;
}

/* ManalinkEh 2022: 0x267ae90; original functions/functions.c:1563. */
int pump_ability(int player, int card, int t_player, int t_card, pump_ability_t *pump)
{
  int result;
  if ((pump->key_mod != KEYWORD_0) || (pump->skey_mod != SP_KEYWORD_0))
  {
    get_sleighted_hacked_suppressed_ability(player, card, t_player, &pump->key_mod,
                                                 &pump->skey_mod);
  }
  result = pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
      player, card, t_player, t_card, pump);
  return result;
}

/* ManalinkEh 2022: 0x267af00; original functions/functions.c:1570. */
int pump_ability_until_eot(int player, int card, int t_player, int t_card, int power,
                                int toughness, int ability, int sp_ability)
{
  pump_ability_t pump;
  pump.skey_mod = sp_ability;
  pump.paue_flags = sp_ability >= 0 ? PAUE_END_AT_EOT : 0;
  pump.pow_mod = power;
  pump.tou_mod = toughness;
  pump.key_mod = ability;
  pump.source_p = player;
  pump.source_c = card;
  pump.can_block_additional_creatures = 0;
  pump.eot_removal_method = 0;
  pump.subtype = SUBTYPE_INVALID;
  pump.color = COLOR_TEST_0;
  pump.disabling_mode = 0;
  return pump_ability(player, card, t_player, t_card, &pump);
}

/* ManalinkEh 2022: 0x267b1b0; original functions/functions.c:1580. */
int pump_ability_until_eot_no_repeat_alt(int player, int card, int t_player, int t_card,
                                         int ability, int sp_ability,
                                         int alternate_legacy_text_number)
{
  card_instance_t *instance;
  int card_00;
  unsigned int special_abilities;
  target_t previous_effect;
  pump_ability_t pump;

  get_sleighted_hacked_suppressed_ability(player, card, t_player, &ability, &sp_ability);
  instance = get_card_instance(t_player, t_card);
  if ((ability == (instance->regen_status & ability)) &&
      (special_abilities = get_special_abilities_by_instance(instance),
       sp_ability == (special_abilities & sp_ability)))
  {
    previous_effect = find_repeat_legacy_effect_alt(player, card, legacy_effect_pump_ability_until_eot,
                                          t_player, t_card, alternate_legacy_text_number, player);
    if (previous_effect.card != -1)
    {
      return previous_effect.card;
    }
  }
  pump.paue_flags = ((sp_ability ^ 0xffffffffU) >> 0x1f) << 9;
  pump.source_c = card;
  pump.key_mod = ability;
  pump.source_p = player;
  pump.pow_mod = 0;
  pump.tou_mod = 0;
  pump.can_block_additional_creatures = 0;
  pump.eot_removal_method = 0;
  pump.subtype = SUBTYPE_INVALID;
  pump.color = COLOR_TEST_0;
  pump.disabling_mode = 0;
  pump.skey_mod = sp_ability;
  card_00 = pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
      player, card, t_player, t_card, &pump);
  if ((0 < alternate_legacy_text_number) && (-1 < card_00))
  {
    instance = get_card_instance(player, card_00);
    instance->eot_toughness = alternate_legacy_text_number;
    return card_00;
  }
  return card_00;
}

/* ManalinkEh 2022: 0x267b300; original functions/functions.c:1607. */
int pump_ability_until_eot_no_repeat(int player, int card, int t_player, int t_card,
                                          int ability, int sp_ability)
{
  int result;
  result =
      pump_ability_until_eot_no_repeat_alt(player, card, t_player, t_card, ability, sp_ability, -1);
  return result;
}

/* ManalinkEh 2022: 0x26825d0; original functions/functions.c:1612. */
void pump_ability_by_test(int player, int card, int t_player, pump_ability_t *pump,
                          test_definition_t *this_test)
{
  if (pump->key_mod != KEYWORD_0 || pump->skey_mod != SP_KEYWORD_0)
    get_sleighted_hacked_suppressed_ability(player, card, t_player, &pump->key_mod,
                                            &pump->skey_mod);
  for (int pass = 0; pass < 2; ++pass)
  {
    int p = pass == 0 ? current_turn : 1 - current_turn;
    if (t_player != p && t_player != 2)
      continue;
    for (int c = active_cards_count[p] - 1; c >= 0; --c)
    {
      if (!in_play(p, c) || !is_what(p, c, 2))
        continue;
      if (this_test &&
          (!new_make_test_in_play(p, c, -1, this_test) ||
           (this_test->not_me && (this_test->not_me != 1 || (player == p && card == c)))))
        continue;
      pump_ability_until_eot_with_precomputed_sleight_hack_suppress(player, card, p, c, pump);
    }
  }
}

/* ManalinkEh 2022: 0x267af90; original functions/functions.c:1633. */
int can_block_additional_until_eot(int player, int card, int t_player, int t_card,
                                        int num_additional)
{
  int legacy_card;
  card_instance_t *instance;

  legacy_card = create_targetted_legacy_effect(player, card, legacy_effect_pump_ability_until_eot,
                                           t_player, t_card);
  instance = get_card_instance(player, legacy_card);
  instance->targets[0].player = t_player;
  instance->targets[0].card = t_card;
  instance->targets[1].player = 0;
  instance->targets[1].card = 0;
  instance->counter_power = 0;
  instance->counter_toughness = 0;
  instance->targets[3].player = 0x200;
  instance->targets[4].player = 1;
  instance->targets[5].player = num_additional;
  return legacy_card;
}

/* ManalinkEh 2022: 0x2685dd0; original functions/functions.c:1648. */
void pump_creatures_until_eot(int player, int card, int t_player, int alternate_legacy_text_number,
                              int power, int toughness, int ability, int sp_ability,
                              test_definition_t *this_test)
{
  unsigned char state_flags;
  card_instance_t *instance;
  int result;
  int card_00;
  int t_player_00;
  int test_score;
  int shs_ability;
  int shs_sp_ability;
  pump_ability_t pump;

  test_score = 0;
  if (this_test != NULL)
  {
    test_score = new_get_test_score(this_test);
  }
  instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
  {
    player = instance->parent_controller;
    card = instance->parent_card;
  }
  t_player_00 = 0;
  do
  {
    if ((t_player == t_player_00) || (t_player == 2))
    {
      shs_ability = ability;
      shs_sp_ability = sp_ability;
      if (ability == 0 && sp_ability == 0)
      {
        pump.key_mod = KEYWORD_0;
        pump.skey_mod = SP_KEYWORD_0;
        pump.paue_flags = 0x200;
      }
      else
      {
        get_sleighted_hacked_suppressed_ability(player, card, t_player_00, &shs_ability,
                                                &shs_sp_ability);
        pump.key_mod = shs_ability;
        pump.skey_mod = shs_sp_ability;
        pump.paue_flags = sp_ability >= 0 ? 0x200 : 0;
      }
      pump.tou_mod = toughness;
      pump.pow_mod = power;
      pump.source_c = card;
      pump.source_p = player;
      pump.disabling_mode = 0;
      pump.color = COLOR_TEST_0;
      pump.subtype = SUBTYPE_INVALID;
      pump.eot_removal_method = 0;
      pump.can_block_additional_creatures = 0;
      card_00 = active_cards_count[t_player_00] + -1;
      if (-1 < card_00)
      {
        do
        {
          instance = in_play(t_player_00, card_00);
          if ((instance != NULL) && (result = is_what(t_player_00, card_00, 2), result != 0))
          {
            if (this_test == NULL)
            {
              state_flags = ((uint32_t)instance->state >> 16 & 0xffU);
            }
            else
            {
              if (((this_test->not_me != 0) &&
                   ((this_test->not_me != 1 || ((player == t_player_00 && (card == card_00)))))) ||
                  (result = new_make_test_in_play(t_player_00, card_00, test_score, this_test),
                   result == 0))
              {
                --card_00;
                continue;
              }
              state_flags = ((uint32_t)instance->state >> 16 & 0xffU);
            }
            if ((((state_flags & 0x10) == 0) &&
                 (result = pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
                      player, card, t_player_00, card_00, &pump),
                  -1 < result)) &&
                (0 < alternate_legacy_text_number))
            {
              alternate_legacy_text(alternate_legacy_text_number, player, result);
            }
          }
          card_00 = card_00 + -1;
        } while (card_00 != -1);
      }
    }
    t_player_00 = t_player_00 + 1;
    if (t_player_00 == 2)
    {
      return;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x2686520; original functions/functions.c:1689. */
void pump_creatures_until_eot_no_repeat(int player, int card, int t_player,
                                        int alternate_legacy_text_number, int ability,
                                        int sp_ability, test_definition_t *this_test)
{
  card_instance_t *instance;
  int result;
  unsigned int special_abilities;
  int card_00;
  int t_player_00;
  target_t previous_effect;
  int test_score;
  int shs_ability;
  int shs_sp_ability;
  pump_ability_t pump;

  test_score = 0;
  if (this_test != NULL)
  {
    test_score = new_get_test_score(this_test);
  }
  instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
  {
    player = instance->parent_controller;
    card = instance->parent_card;
  }
  t_player_00 = 0;
  do
  {
    if ((t_player == t_player_00) || (t_player == 2))
    {
      shs_ability = ability;
      shs_sp_ability = sp_ability;
      if (ability == 0 && sp_ability == 0)
      {
        pump.key_mod = KEYWORD_0;
        pump.skey_mod = SP_KEYWORD_0;
        pump.paue_flags = 0x200;
      }
      else
      {
        get_sleighted_hacked_suppressed_ability(player, card, t_player_00, &shs_ability,
                                                &shs_sp_ability);
        pump.key_mod = shs_ability;
        pump.skey_mod = shs_sp_ability;
        pump.paue_flags = sp_ability >= 0 ? 0x200 : 0;
      }
      pump.source_c = card;
      pump.source_p = player;
      pump.disabling_mode = 0;
      pump.color = COLOR_TEST_0;
      pump.subtype = SUBTYPE_INVALID;
      pump.eot_removal_method = 0;
      pump.can_block_additional_creatures = 0;
      pump.tou_mod = 0;
      pump.pow_mod = 0;
      card_00 = active_cards_count[t_player_00] + -1;
      if (-1 < card_00)
      {
        do
        {
          instance = in_play(t_player_00, card_00);
          if ((((instance != NULL) && (result = is_what(t_player_00, card_00, 2), result != 0)) &&
               ((this_test == NULL ||
                 (((this_test->not_me == 0 ||
                    ((this_test->not_me == 1 && ((player != t_player_00 || (card != card_00)))))) &&
                   (result = new_make_test_in_play(t_player_00, card_00, test_score, this_test),
                    result != 0)))))) &&
              ((((instance->state & 0x100000) == 0 &&
                 (((shs_ability != (instance->regen_status & shs_ability) ||
                    (special_abilities = get_special_abilities_by_instance(instance),
                     shs_sp_ability != (special_abilities & shs_sp_ability))) ||
                   (previous_effect = find_repeat_legacy_effect_alt(
                        player, card, legacy_effect_pump_ability_until_eot, t_player_00, card_00,
                        -1, player),
                    previous_effect.card == -1)))) &&
                ((result = pump_ability_until_eot_with_precomputed_sleight_hack_suppress(
                      player, card, t_player_00, card_00, &pump),
                  -1 < result && (0 < alternate_legacy_text_number))))))
          {
            alternate_legacy_text(alternate_legacy_text_number, player, result);
          }
          card_00 = card_00 + -1;
        } while (card_00 != -1);
      }
    }
    t_player_00 = t_player_00 + 1;
    if (t_player_00 == 2)
    {
      return;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x2682750; original functions/functions.c:1737. */
void pump_creatures_until_eot_merge_pt_alternate_legacy_text(int player, int card, int t_player,
                                                             int power, int toughness,
                                                             test_definition_t *this_test,
                                                             int alt_text_number)
{
  int score = this_test ? new_get_test_score(this_test) : 0;
  for (int p = 0; p < 2; ++p)
    if (t_player == p || t_player == 2)
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
      {
        card_instance_t *instance = in_play(p, c);
        if (!instance || !is_what(p, c, 2))
          continue;
        if (this_test &&
            ((this_test->not_me && (this_test->not_me != 1 || (player == p && card == c))) ||
             !new_make_test_in_play(p, c, score, this_test)))
          continue;
        if (instance->state & 0x100000)
          continue;
        int alternate_text = alt_text_number < 1 ? 0 : alt_text_number;
        pump_until_eot_merge_previous_alt(player, card, p, c, power, toughness, alternate_text);
      }
}

/* ManalinkEh 2022: 0x2682890; original functions/functions.c:1761. */
void pump_creatures_until_eot_merge_pt(int player, int card, int t_player, int power,
                                            int toughness, test_definition_t *this_test)
{
  pump_creatures_until_eot_merge_pt_alternate_legacy_text(player, card, t_player, power,
                                                               toughness, this_test, 0);
  return;
}

/* ManalinkEh 2022: 0x2686030; original functions/functions.c:1766. */
int pump_subtype_until_eot(int player, int card, int t_player, int subtype, int power,
                                int toughness, int ability, int sp_ability)
{
  test_definition_t this_test;
  new_default_test_definition(&this_test, 2, "");
  if (0 < subtype)
  {
    this_test.subtype = subtype;
  }
  pump_creatures_until_eot(player, card, t_player, 0, power, toughness, ability, sp_ability,
                                &this_test);
  return 0;
}

/* ManalinkEh 2022: 0x26860b0; original functions/functions.c:1778. */
void pump_color_until_eot(int player, int card, int t_player, int clr, int power,
                               int toughness, int ability, int sp_ability)
{
  test_definition_t this_test;
  default_test_definition(&this_test, 2);
  if (0 < clr)
  {
    this_test.color = clr;
  }
  pump_creatures_until_eot(player, card, t_player, 0, power, toughness, ability, sp_ability,
                                &this_test);
  return;
}

/* ManalinkEh 2022: 0x267f4a0; original functions/functions.c:5667. */
int enters_the_battlefield_as_copy_of_any(int player, int card, event_t event,
                                          target_definition_t *td, const char *prompt)
{
  unsigned int copied = 0;
  unsigned int target_card = 0xffffffff;
  int target_player = -1;
  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= 0x800000;
    int selected = pick_target(td, prompt);
    instance->state &= 0xff7fffff;
    if (!selected)
      spell_fizzled = 0;
    else
    {
      target_card = instance->targets[0].card;
      instance->number_of_targets = 0;
      target_player = instance->targets[0].player;
      if (target_card != card || player != target_player)
        copied = (target_card ^ 0xffffffff) >> 0x1f;
      else
      {
        target_card = 0xffffffff;
        target_player = -1;
      }
    }
  }
  enters_the_battlefield_as_copy_of(player, card, event, target_player, target_card);
  return copied;
}

/* ManalinkEh 2022: 0x2686bb0; original functions/functions.c:5847. */
void shapeshift_all(int src_player, int src_card, int t_player, test_definition_t *test,
                    int turn_into_player, int turn_into_card, int mode)
{
  char marked[2][151];
  int score = new_get_test_score(test);
  memset(marked, 0, sizeof(marked));
  int legendary = is_legendary(turn_into_player, turn_into_card);
  int marked_count = 0;
  bool both_players = t_player == 2;
  for (int p = 0; p < 2; ++p)
    if (t_player == p || both_players)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && (test->not_me != 1 || src_card != c || src_player != p) &&
            new_make_test_in_play(p, c, score, test))
        {
          marked[p][c] = 1;
          ++marked_count;
        }
  for (int pass = 0; pass < 2; ++pass)
  {
    int p = pass ? 1 - current_turn : current_turn;
    if (t_player == p || both_players)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (marked[p][c] && in_play(p, c))
          real_shapeshift_target(src_player, src_card, p, c, turn_into_player, turn_into_card,
                                 mode);
  }
  if (marked_count > 0 && legendary)
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (marked[p][c] && in_play(p, c) && is_legendary(p, c))
        {
          verify_legend_rule(p, c, get_id(p, c));
          return;
        }
}

/* ManalinkEh 2022: 0x2684210; original functions/functions.c:6906. */
static int new_specific_cip_impl(int controller_of_trigger, event_t event, int trigger_mode,
                                      type_t type, test_definition_t *test,
                                      int triggering_player, int triggering_card)
{
  int result;
  card_instance_t *instance;
  int entering_card;
  if (reason_for_trigger_controller == trigger_cause_controller)
  {
    result = trigger_cause_controller;
    entering_card = trigger_cause;
    if (triggering_player != -1 && triggering_card != -1)
    {
      result = triggering_player;
      entering_card = triggering_card;
    }
    if (((((entering_card == affected_card) && (result == affected_card_controller)) &&
          ((trigger_cause_controller == controller_of_trigger || (controller_of_trigger == 2)))) &&
         (result = check_for_cip_effects_removal(trigger_cause_controller, trigger_cause),
          result == 0)) &&
        (((type == TYPE_NONE ||
           (result = is_what(trigger_cause_controller, trigger_cause, type), result != 0)) &&
          ((test == NULL ||
            ((result = new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, test),
              result != 0 &&
                  (((test->not_me != 1 || (triggering_player != trigger_cause_controller)) ||
                    (triggering_card != trigger_cause))))))))))
    {
      if (event == EVENT_TRIGGER)
      {
        if (trigger_mode == 3)
        {
          result = duh_mode(triggering_player);
          trigger_mode = (result != 0) + 1;
        }
        event_result = event_result | trigger_mode;
        return 0;
      }
      if (event == EVENT_RESOLVE_TRIGGER)
      {
        entering_card = trigger_cause;
        result = trigger_cause_controller;
        if (triggering_player != -1 && triggering_card != -1)
        {
          instance = get_card_instance(triggering_player, triggering_card);
          entering_card = trigger_cause;
          result = trigger_cause_controller;
          instance->targets[1].player = trigger_cause_controller;
          instance->targets[1].card = entering_card;
        }
        instance = get_card_instance(result, entering_card);
        instance->regen_status = instance->regen_status | 0x6000000;
        return 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2684410; original functions/functions.c:6885. */
int specific_cip(int player, int card, event_t event, int t_player, int trigger_mode, int type,
                      int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4,
                      int cc, int flag5)
{
  int result;
  test_definition_t test;
  result = 0;
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (reason_for_trigger_controller == player))
    {
      result = 0;
      new_default_test_definition(&test, type, "");
      test.type_flag = (unsigned short)flag1;
      test.subtype = subtype;
      test.subtype_flag = (unsigned short)flag2;
      test.color = color;
      test.color_flag = (unsigned short)flag3;
      test.id = id;
      test.id_flag = (unsigned short)flag4;
      test.cmc = cc;
      test.cmc_flag = (unsigned short)flag5;
      if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
      {
        result =
            new_specific_cip_impl(player, event, trigger_mode, TYPE_NONE, &test, player, card);
      }
    }
    return result;
  }
  return result;
}

/* ManalinkEh 2022: 0x2684560; original functions/functions.c:6962. */
int new_specific_cip(int player, int card, event_t event, int t_player, int trigger_mode,
                          test_definition_t *this_test)
{
  int result;
  if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
  {
    return 0;
  }
  result =
      new_specific_cip_impl(player, event, trigger_mode, TYPE_NONE, this_test, player, card);
  return result;
}

/* ManalinkEh 2022: 0x2684500; original functions/functions.c:6966. */
int new_specific_cip_global(int player, event_t event, int trigger_mode, type_t type,
                                 test_definition_t *this_test, int triggering_player,
                                 int triggering_card)
{
  int result;
  if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
  {
    return 0;
  }
  result = new_specific_cip_impl(player, event, trigger_mode, type, this_test,
                                      triggering_player, triggering_card);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 activated-ability core. */

/* ManalinkEh 2022: 0x2681550; original functions/functions.c:7536. */
int cannot_attack(int player, int card, event_t event)
{
  card_instance_t *instance;
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((-1 < player && (-1 < card))))
  {
    instance = get_card_instance(player, card);
    instance->token_status = instance->token_status | 0x8000;
  }
  return 0;
}

/* ManalinkEh 2022: 0x26888a0; original functions/functions.c:9038. */
int get_cost_mod_for_activated_abilities(int player, int card, int orig_cost, int black,
                                              int blue, int green, int red, int white)
{
  int cost_modifiers;
  card_instance_t *instance;
  unsigned int cost;
  int i;

  instance = get_card_instance(player, card);
  cost_modifiers = instance->targets[0x11].card;
  cost = (int)(orig_cost ^ 0xffffffffU) >> 0x1f & orig_cost;
  if (0x800 < cost_modifiers)
  {
    i = 0x14;
    do
    {
      if ((cost_modifiers >> ((unsigned char)i & 0x1f) & 1U) != 0)
      {
        cost = cost + 2;
      }
      i = i + 1;
    } while (i != 0x18);
    do
    {
      if ((cost_modifiers >> ((unsigned char)i & 0x1f) & 1U) != 0)
      {
        cost = cost + 3;
      }
      i = i + 1;
    } while (i != 0x1c);
    do
    {
      cost = (cost + 1) - (unsigned int)((cost_modifiers >> ((unsigned char)i & 0x1f) & 1U) == 0);
      i = i + 1;
    } while (i != 0x20);
    if (0 < (int)cost)
    {
      i = 0xc;
      do
      {
        cost = (cost - 1) + (unsigned int)((cost_modifiers >> ((unsigned char)i & 0x1f) & 1U) == 0);
        i = i + 1;
      } while (i != 0x10);
      do
      {
        if ((cost_modifiers >> ((unsigned char)i & 0x1f) & 1U) != 0)
        {
          cost = cost - 2;
        }
        i = i + 1;
      } while (i != 0x14);
      cost = cost & (int)(cost ^ 0xffffffff) >> 0x1f;
      if ((((((white == 0 && red == 0) && green == 0) && blue == 0) && black == 0) && cost == 0) &&
          (0 < orig_cost))
      {
        return 1;
      }
    }
  }
  if ((cost == 0) && (orig_cost < 0))
  {
    return orig_cost;
  }
  return cost;
}

/* ManalinkEh 2022: 0x2690310; original functions/functions.c:9438. */
int generic_shade_amt_can_pump(int player, int card, int pump, int life_to_pay, int cless,
                                    int black, int blue, int green, int red, int white, int limit)
{
  char cost_component_count;
  int cost_or_life_limit;
  int upper_bound;
  int amount;
  int lower_bound;

  if (pump < 1)
  {
    return 0;
  }
  if (can_use_activated_abilities(player, card) != 0)
  {
    cost_or_life_limit = get_cost_mod_for_activated_abilities(player, card, cless, black, blue, green, red,
                                                      white);
    cost_component_count = (cost_or_life_limit != 0) + (black != 0) + (blue != 0) + (green != 0) + (red != 0) + (white != 0);
    if (cost_component_count == '\0')
    {
      amount = 99;
    }
    else if (cost_component_count == '\x01')
    {
      if (cost_or_life_limit == 0)
      {
        if (black == 0)
        {
          if (blue == 0)
          {
            if (green == 0)
            {
              if (red == 0)
              {
                amount = has_mana(player, 5, 1);
                amount = amount / white;
              }
              else
              {
                amount = has_mana(player, 4, 1);
                amount = amount / red;
              }
            }
            else
            {
              amount = has_mana(player, 3, 1);
              amount = amount / green;
            }
          }
          else
          {
            amount = has_mana(player, 2, 1);
            amount = amount / blue;
          }
        }
        else
        {
          amount = has_mana(player, 1, 1);
          amount = amount / black;
        }
      }
      else
      {
        amount = has_mana(player, 7, 1);
        amount = amount / cost_or_life_limit;
      }
    }
    else
    {
      upper_bound = has_mana(player, 7, 1);
      upper_bound = upper_bound / (black + cost_or_life_limit + blue + green + red + white);
      if (black != 0)
      {
        amount = has_mana(player, 1, 1);
        if (amount / black < upper_bound)
        {
          upper_bound = amount / black;
        }
      }
      if (blue != 0)
      {
        amount = has_mana(player, 2, 1);
        if (amount / blue < upper_bound)
        {
          upper_bound = amount / blue;
        }
      }
      if (green != 0)
      {
        amount = has_mana(player, 3, 1);
        if (amount / green < upper_bound)
        {
          upper_bound = amount / green;
        }
      }
      if (red != 0)
      {
        amount = has_mana(player, 4, 1);
        if (amount / red < upper_bound)
        {
          upper_bound = amount / red;
        }
      }
      if (white != 0)
      {
        amount = has_mana(player, 5, 1);
        if (amount / white < upper_bound)
        {
          upper_bound = amount / white;
        }
      }
      if (upper_bound < 1)
      {
        return 0;
      }
      lower_bound = 0;
      do
      {
        amount = (lower_bound + 1 + upper_bound) / 2;
        if (has_mana_multi(player, cost_or_life_limit * amount, black * amount, blue * amount, green * amount,
                               red * amount, white * amount) == 0)
        {
          upper_bound = amount + -1;
          amount = lower_bound;
        }
        lower_bound = amount;
      } while (amount != upper_bound);
      if (amount < 1)
      {
        return 0;
      }
      amount = pump * amount;
    }
    if (0 < life_to_pay)
    {
      cost_or_life_limit = can_pay_life_impl(player, 1, 0);
      if ((cost_or_life_limit == 0) || (cost_or_life_limit = (life[player] + -1) / life_to_pay, cost_or_life_limit < 1))
      {
        return 0;
      }
      if (cost_or_life_limit < amount)
      {
        amount = cost_or_life_limit;
      }
    }
    if ((-1 < limit) && (limit < amount))
    {
      amount = limit;
    }
    return pump * amount;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2694580; original functions/functions.c:9944. */
static void count_for_gfp_ability_and_store_values_impl(int player, int card, int event,
                                                        int t_player, int type,
                                                        test_definition_t *this_test,
                                                        CardPairFunction extra_check, int mode,
                                                        int storage_location)
{
  char *counter_byte;
  card_instance_t *instance;
  int result;
  int value;
  card_instance_t *affected_instance;
  unsigned int player_00;
  int count;
  int multiplier;
  unsigned int bits;
  int csvids[3];
  value = affected_card;
  result = affected_card_controller;
  if (event != 0x77)
  {
    return;
  }

  instance = in_play(affected_card_controller, affected_card);
  if (instance == NULL)
  {
    return;
  }
  instance = get_card_instance(result, value);
  if ((instance->ext->special_flags2 & SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE) != 0)
  {
    return;
  }
  instance = get_card_instance(result, value);
  if (instance->kill_code < KILL_BURY || instance->kill_code > KILL_SACRIFICE)
  {
    return;
  }
  if (mode & 0x20)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (t_player == 2 || t_player != (instance->state >> 0xc & 1))
      return;
  }
  else if (t_player != 2 && t_player != affected_card_controller)
    return;
  if (this_test)
  {
    if (this_test->not_me == 1 && card == affected_card && player == affected_card_controller)
      return;
    if (type && !is_what(affected_card_controller, affected_card, type))
      return;
    if (!new_make_test_in_play(affected_card_controller, affected_card, -1, this_test))
      return;
  }
  else if (type && !is_what(affected_card_controller, affected_card, type))
    return;
  if ((extra_check != (CardPairFunction)0x0) &&
      (result = (*extra_check)(affected_card_controller, affected_card), result == 0))
  {
    return;
  }
  result = check_for_death_trigger_removal(affected_card_controller, affected_card);
  if (result != 0)
  {
    return;
  }
  instance = get_card_instance(player, card);
  if (instance->targets[0xb].player < 0)
  {
    instance->targets[0xb].player = 0;
  }
  value = is_token(affected_card_controller, affected_card);
  result = affected_card;
  player_00 = affected_card_controller;
  if ((!value && (mode & 0x400)) || (value && !(mode & 0x400) && (mode & 0x40)))
    return;
  if ((mode & 0x189fU) != 0)
  {
    if ((mode & 0x10U) == 0)
    {
      if (instance->targets[storage_location].player < 0)
      {
        instance->targets[storage_location].player = 0;
      }
      if ((mode & 1U) != 0)
      {
        value = get_abilities(player_00, result, EVENT_POWER, -1);
        result = affected_card;
        instance->targets[storage_location].player =
            instance->targets[storage_location].player + (card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1) * value;
        player_00 = affected_card_controller;
      }
      if ((mode & 2U) != 0)
      {
        result = get_abilities(player_00, result, EVENT_TOUGHNESS, -1);
        player_00 = affected_card_controller;
        instance->targets[storage_location].player =
            instance->targets[storage_location].player + (card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1) * result;
        result = affected_card;
      }
      if ((mode & 4U) != 0)
      {
        value = get_cmc(player_00, result);
        count = is_what(affected_card_controller, affected_card, 2);
        result = affected_card;
        multiplier = 1;
        if (count != 0)
        {
          multiplier = card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1;
        }
        instance->targets[storage_location].player =
            instance->targets[storage_location].player + multiplier * value;
        player_00 = affected_card_controller;
      }
      if ((mode & 8U) != 0)
      {
        bits = get_color(player_00, result);
        result = affected_card;
        instance->targets[storage_location].player =
            instance->targets[storage_location].player | bits;
        player_00 = affected_card_controller;
      }
      if ((mode & 0x800U) != 0)
      {
        result = count_counters(player_00, result, COUNTER_FUNGUS);
        player_00 = affected_card_controller;
        instance->targets[storage_location].player =
            instance->targets[storage_location].player + (card_counts[player][CCOUNTS_TEYSA_KARLOV] + 1) * result;
        result = affected_card;
      }
    }
    else
    {
      value = (unsigned int)(unsigned char)instance->targets[0xb].player + storage_location;
      result = get_id(affected_card_controller, affected_card);
      bool melded = result == 0x3eae ||
                    (result = get_id(affected_card_controller, affected_card), result == 0x3ef4);
      if (!melded)
      {
        result = get_id(affected_card_controller, affected_card);
        melded = result == 0x3efb;
      }
      count = melded ? 2 : 1;
      if (melded)
        *(char *)&instance->targets[11].player = (char)instance->targets[11].player + 1;
      unmeld(result, csvids + 1, csvids + 2);
      if ((value < 0xb) && ((t_player == 2 || ((mode & 0x80U) != 0))))
      {
        do
        {
          affected_instance = get_card_instance(affected_card_controller, affected_card);
          instance->targets[value].player = affected_instance->state >> 0xc & 1;
          result = get_internal_card_id_from_csv_id(csvids[count]);
          instance->targets[value].card = result;
          if ((mode & 0x1000U) != 0)
          {
            result = get_original_internal_card_id(affected_card_controller, affected_card);
            instance->targets[value].player = instance->targets[value].player | result << 4;
          }
          value = value + 1;
          count = count + -1;
          player_00 = affected_card_controller;
          result = affected_card;
        } while ((count != 0) && (value < 0xb));
      }
      else
      {
        player_00 = affected_card_controller;
        result = affected_card;
        if (storage_location < 0xb)
        {
          do
          {
            if (instance->targets[storage_location].player == -1)
            {
              result = get_internal_card_id_from_csv_id(csvids[count]);
              instance->targets[storage_location].player = result;
            }
            else if (instance->targets[storage_location].card == -1)
            {
              result = get_internal_card_id_from_csv_id(csvids[count]);
              instance->targets[storage_location].card = result;
            }
            else
            {
              count = count + -1;
              storage_location = storage_location + 1;
            }
            player_00 = affected_card_controller;
            result = affected_card;
          } while ((storage_location < 0xb) && (count != 0));
        }
      }
    }
  }
  if ((mode & 0x300U) == 0)
  {
    *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
    result = is_what(player_00, result, 2);
  }
  else
  {
    bits = player_00;
    if ((mode & 0x100U) != 0)
    {
      affected_instance = get_card_instance(player_00, result);
      player_00 = affected_instance->state >> 0xc & 1;
      result = affected_card;
      bits = affected_card_controller;
    }
    if (player_00 == 1)
    {
      SET_BYTE1(instance->targets[0xb].player) =
          (char)SET_BYTE1(instance->targets[0xb].player) + '\x01';
      result = is_what(bits, result, 2);
      if ((result != 0) && ((mode & 0x10U) == 0))
      {
        counter_byte = (char *)&SET_BYTE1(instance->targets[0xb].player);
        *counter_byte = *counter_byte + card_counts[player][CCOUNTS_TEYSA_KARLOV];
      }
      instance->targets[11].card = 0;
      return;
    }
    *(char *)&instance->targets[0xb].player = (char)instance->targets[0xb].player + '\x01';
    result = is_what(bits, result, 2);
  }
  if ((result != 0) && ((mode & 0x10U) == 0))
  {
    *(uint8_t *)&instance->targets[0xb].player =
        (char)instance->targets[0xb].player + card_counts[player][CCOUNTS_TEYSA_KARLOV];
  }
  instance->targets[0xb].card = 0;
  return;
}

/* ManalinkEh 2022: 0x2694c90; original functions/functions.c:10062. */
void count_for_gfp_ability_and_store_values_extra(int player, int card, int event,
                                                       int t_player, int type,
                                                       CardPairFunction extra_check, int mode,
                                                       int storage_location)
{
  count_for_gfp_ability_and_store_values_impl(player, card, event, t_player, type, NULL,
                                                   extra_check, mode, storage_location);
  return;
}

/* ManalinkEh 2022: 0x2694ce0; original functions/functions.c:10066. */
void count_for_gfp_ability_and_store_values(int player, int card, int event, int t_player,
                                                 int type, test_definition_t *this_test,
                                                 int mode, int storage_location)
{
  count_for_gfp_ability_and_store_values_impl(player, card, event, t_player, type, this_test,
                                                   (CardPairFunction)0x0, mode, storage_location);
  return;
}

/* ManalinkEh 2022: 0x2694d30; original functions/functions.c:10070. */
void count_for_gfp_ability(int player, int card, int event, int t_player, int type,
                                test_definition_t *this_test)
{
  count_for_gfp_ability_and_store_values_impl(player, card, event, t_player, type, this_test,
                                                   (CardPairFunction)0x0, 0, 0);
  return;
}

/* ManalinkEh 2022: 0x2689440; original functions/functions.c:10074. */
int resolve_gfp_ability(int player, int card, event_t event, resolve_trigger_t trig_mode)
{
  if (trigger_condition != TRIGGER_GRAVEYARD_FROM_PLAY || reason_for_trigger_controller != player)
    return 0;
  card_instance_t *instance = in_play(player, card);
  if (!instance)
    return 0;
  int num = instance->targets[11].player;
  if (num > 0 && affect_me(player, card))
  {
    if (event == EVENT_TRIGGER)
    {
      if (trig_mode == RESOLVE_TRIGGER_DUH)
        trig_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
      event_result |= trig_mode;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance->targets[11].card = num;
      instance->targets[11].player = 0;
      return num;
    }
    else if (event == EVENT_END_TRIGGER)
      instance->targets[11].player = 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x268b720; original functions/functions.c:11106. */
int charge_mana_from_id_with_cost_mod(int player, int card, event_t event, int csvid, int cost_mod)
{
  if (csvid < 0)
    csvid = get_id(player, card);
  int iid;
  if (card == -1)
    iid = get_internal_card_id_from_csv_id(csvid);
  else
  {
    iid = get_card_instance(player, card)->internal_card_id;
    if (cards_data[iid].id != csvid)
      backtrace_and_abort("functions/functions.c:11121\ncards_data[iid].id == csvid");
  }
  if (csvid == CARD_ID_REAPER_KING)
  {
    if (card >= 0)
      return casting_reaper_king(player, card, event);
    int temporary_card = add_card_to_hand(player, iid);
    int result = casting_reaper_king(player, temporary_card, event);
    obliterate_card(player, temporary_card);
    return result;
  }
  card_ptr_t *cp = cards_ptr[csvid];
  unsigned int base_cost = cp->req.req_colorless;
  if (base_cost == 40)
    base_cost = 0;
  int artifact = cards_data[iid].type & TYPE_ARTIFACT;
  int temporary_card = -1;
  int result;
  if (is_phyrexian(csvid))
  {
    if (card < 0)
    {
      card = add_card_to_hand(player, iid);
      card_instance_t *instance = get_card_instance(player, card);
      uint32_t state = instance->state;
      --hand_count[player];
      instance->state = state | STATE_INVISIBLE;
      temporary_card = card;
    }
    result = charge_phyrexian_mana(player, card, event, base_cost, cp->req.req_black,
        cp->req.req_blue, cp->req.req_green, cp->req.req_red, cp->req.req_white, artifact);
  }
  else
  {
    int cost = true_get_updated_casting_cost(player, card < 0 ? -1 : card,
        card < 0 ? iid : -1, event, (int32_t)(base_cost - (uint32_t)cost_mod));
    if (cost < 0)
      cost = 0;
    int normal_hybrid = is_normal_hybrid(cp);
    int monocolor_hybrid = normal_hybrid ? 0 : is_monocolor_hybrid(cp);
    if (!normal_hybrid && !monocolor_hybrid)
    {
      if (csvid == CARD_ID_KARADOR_GHOST_CHIEFTAIN)
      {
        cost = (int32_t)((uint32_t)cost - count_graveyard_by_type(player, TYPE_CREATURE));
        if (cost < 0)
          cost = 0;
      }
      int pay_x_after_fixed_cost = 0;
      if (cards_data[iid].cc[1] == 0xff)
      {
        if (cost == 0)
          cost = -1;
        else
          pay_x_after_fixed_cost = 1;
      }
      pay_mana_xbugrwaU[2] = cp->req.req_blue;
      pay_mana_xbugrwaU[5] = cp->req.req_white;
      pay_mana_xbugrwaU[1] = cp->req.req_black;
      pay_mana_xbugrwaU[4] = cp->req.req_red;
      pay_mana_xbugrwaU[3] = cp->req.req_green;
      pay_mana_xbugrwaU[0] = artifact ? 0 : cost;
      pay_mana_xbugrwaU[6] = artifact ? cost : 0;
      charge_mana(player, COLOR_COLORLESS, 0);
      if (!pay_x_after_fixed_cost || spell_fizzled == 1)
        return spell_fizzled != 1;
      pay_mana_xbugrwaU[0] = artifact ? 0 : -1;
      pay_mana_xbugrwaU[6] = artifact ? -1 : 0;
      for (int color = 1; color <= 5; ++color)
        pay_mana_xbugrwaU[color] = 0;
      charge_mana(player, COLOR_COLORLESS, 0);
      return spell_fizzled != 1;
    }
    if (card < 0)
    {
      card = add_card_to_hand(player, iid);
      card_instance_t *instance = get_card_instance(player, card);
      uint32_t state = instance->state;
      --hand_count[player];
      instance->state = state | STATE_INVISIBLE;
      temporary_card = card;
    }
    if (normal_hybrid)
    {
      int first_color = get_colors_for_hybrid(player, card, 0, 0);
      int second_color = get_colors_for_hybrid(player, card, first_color, 0);
      int number = get_number_of_hybrid_mana(player, card, 0, 0);
      result = charge_mana_hybrid(player, card, number, first_color, second_color, cost);
    }
    else
    {
      int color = get_color_for_monocolored_hybrid(player, card);
      int number = get_number_of_monocolored_hybrid_mana(player, card, 0);
      result = charge_mana_for_monocolor_hybrid(player, card, number, color, cost);
    }
  }
  if (temporary_card >= 0)
    obliterate_card(player, temporary_card);
  return result;
}

/* Attack counts may use a 32-bit target field or the pointer-sized info slot. */
static int32_t read_attack_count(card_instance_t *instance, int32_t *storage)
{
  return storage ? *storage : (int32_t)instance->info_slot;
}

static void write_attack_count(card_instance_t *instance, int32_t *storage, int32_t count)
{
  if (storage)
    *storage = count;
  else
    instance->info_slot = count;
}

/* ManalinkEh 2022: 0x268d340; original functions/functions.c:12407. */
void store_attackers(int player, int card, event_t event, declare_attackers_trigger_t mode,
                          int attacker_player, int attacker_card, test_definition_t *test)
{
  int result;
  int32_t stored_count;
  card_instance_t *attacker;
  card_instance_t *instance;
  int32_t count;
  int32_t *storage;
  if (event == EVENT_DECLARE_ATTACKERS)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((result == 0) && (((current_turn == attacker_player || (attacker_player == 2)) &&
                          (number_of_attackers_declared != 0))))
    {
      instance = get_card_instance(player, card);
      if ((mode & DAT_STORE_IN_INFO_SLOT) == 0)
      {
        if ((mode & DAT_STORE_IN_TARGETS_3) == 0)
        {
          storage = &instance->targets[1].player;
          stored_count = instance->targets[1].player;
        }
        else
        {
          storage = &instance->targets[3].player;
          stored_count = instance->targets[3].player;
        }
      }
      else
      {
        storage = NULL;
        stored_count = instance->info_slot;
      }
      if (stored_count < 0)
      {
        write_attack_count(instance, storage, 0);
      }
      if ((mode & DAT_ATTACKS_ALONE) == 0)
      {
        if (((mode & DAT_ATTACKS_WITH_3_OR_MORE) != 0) && (number_of_attackers_declared < 3))
        {
          return;
        }
      }
      else
      {
        if (number_of_attackers_declared != 1)
        {
          return;
        }
        if ((mode & DAT_ATTACKS_WITH_3_OR_MORE) != 0)
        {
          return;
        }
      }
      if (test != NULL)
      {
        test->state = test->state | 4;
        result = check_battlefield_for_special_card(player, card, current_turn, 4, test);
        if (result < (int)(unsigned int)test->qty)
        {
          return;
        }
      }
      if ((((mode & DAT_ATTACKS_WITH_2_OR_MORE) == 0) || (1 < number_of_attackers_declared)) &&
          (((mode & DAT_BATTALION) == 0 ||
            ((2 < number_of_attackers_declared &&
              (attacker = get_card_instance(attacker_player, attacker_card),
               (attacker->state & 4) != 0))))))
      {
        result = attacker_card;
        if (attacker_card < 0)
        {
          result = active_cards_count[current_turn] + -1;
          if (result < 0)
          {
            return;
          }
          attacker_card = 0;
        }
        do
        {
          attacker = in_play(current_turn, attacker_card);

          if ((((attacker != NULL) && ((attacker->state & 4) != 0)) &&
               ((((mode & DAT_ATTACKS_PLAYER) == 0 ||
                  (attacker = get_card_instance(current_turn, attacker_card),
                   (attacker->ext->special_flags & SF_ATTACKING_PWALKER) == 0)) &&
                 (((mode & DAT_ATTACKS_PLANESWALKER) == 0 ||
                   (attacker = get_card_instance(current_turn, attacker_card),
                    (attacker->ext->special_flags & SF_ATTACKING_PWALKER) != 0)))))) &&
              ((test == NULL ||
                (((test->not_me == 0 || ((test->not_me == 1 && ((card != attacker_card ||
                                                                 (current_turn != player)))))) &&
                  (new_make_test_in_play(current_turn, attacker_card, -1, test) != 0))))))
          {
            count = read_attack_count(instance, storage);
            if (((mode & DAT_TRACK) != 0) && ((int)count < 0x40))
            {
              ((unsigned char *)&instance->targets[2].player)[count] = (unsigned char)attacker_card;
              count = read_attack_count(instance, storage);
            }
            write_attack_count(instance, storage, count + 1);
          }
          attacker_card = attacker_card + 1;
        } while (attacker_card <= result);
      }
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    storage = NULL;
    if (((mode & DAT_STORE_IN_INFO_SLOT) == 0) &&
        (storage = &instance->targets[1].player, (mode & DAT_STORE_IN_TARGETS_3) != 0))
    {
      storage = &instance->targets[3].player;
    }
    write_attack_count(instance, storage, 0);
    return;
  }
  return;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int charge_mana_from_id_with_cost_mod(int player, int card, event_t event, int csvid, int cost_mod);

/* ManalinkEh 2022: 0x268d6a0; original functions/functions.c:12483. */
int resolve_declare_attackers_trigger(int player, int card, event_t event,
                                      declare_attackers_trigger_t mode)
{
  unsigned char attacker;
  xtrigger_t condition;
  int result;
  card_instance_t *instance;
  declare_attackers_trigger_t trigger_mode;
  int32_t stored_count;
  int32_t *storage;
  condition = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    condition = xtrigger_impl_value_dont_use_directly;
  }
  if ((((condition != XTRIGGER_ATTACKING) || (card != affected_card)) ||
       (player != affected_card_controller)) ||
      ((reason_for_trigger_controller != player ||
        (result = is_humiliated(player, card, AL_TRIG), result != 0))))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if ((mode & DAT_STORE_IN_INFO_SLOT) == 0)
  {
    if ((mode & DAT_STORE_IN_TARGETS_3) == 0)
    {
      storage = &instance->targets[1].player;
      stored_count = instance->targets[1].player;
    }
    else
    {
      storage = &instance->targets[3].player;
      stored_count = instance->targets[3].player;
    }
  }
  else
  {
    storage = NULL;
    stored_count = instance->info_slot;
  }
  if (stored_count < 1)
  {
    return 0;
  }
  result = 0;
  if (event != EVENT_TRIGGER)
  {
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      result = read_attack_count(instance, storage);
      if ((mode & DAT_SEPARATE_TRIGGERS) != 0)
      {
        attacker = ((unsigned char *)&instance->targets[2].player)[result];
        if (1 < result)
        {
          instance->state = instance->state & 0xfffffeff;
        }
        write_attack_count(instance, storage, read_attack_count(instance, storage) - 1);
        return attacker + 1;
      }
    }
    else if (event != EVENT_END_TRIGGER)
    {
      return 0;
    }
    write_attack_count(instance, storage, 0);
    return result;
  }
  trigger_mode = mode & 0xf;
  if (trigger_mode == DAT_RESOLVE_TRIGGER_DUH)
    trigger_mode = duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL;
  else if (trigger_mode == 0)
    trigger_mode = DAT_RESOLVE_TRIGGER_MANDATORY;
  event_result = event_result | trigger_mode;
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 specialized activation costs. */
/* ManalinkEh 2022: 0x268d7e0; original functions/functions.c:12534. */
int declare_attackers_trigger(int player, int card, event_t event,
                                   declare_attackers_trigger_t mode, int attacker_player,
                                   int attacker_card)
{
  int result;
  store_attackers(player, card, event, mode, attacker_player, attacker_card, NULL);
  result = resolve_declare_attackers_trigger(player, card, event, mode);
  return result;
}

/* ManalinkEh 2022: 0x268d840; original functions/functions.c:9701. */
int when_attacks_pump_self(int player, int card, event_t event, int power, int toughness)
{
  int result;
  result = declare_attackers_trigger(player, card, event, 0, player, card);
  if (result != 0)
  {
    pump_until_eot_merge_previous_alt(player, card, player, card, power, toughness, 0);
  }
  if (event == EVENT_POW_BOOST)
  {
    if (current_turn != player)
    {
      return 0;
    }
    if (0x15 < current_phase)
    {
      return 0;
    }
    if (power < 1)
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_TRIG);
  }
  else
  {
    if (event != EVENT_TOU_BOOST)
    {
      return 0;
    }
    if (current_turn != player)
    {
      return 0;
    }
    if (0x15 < current_phase)
    {
      return 0;
    }
    if (toughness < 1)
    {
      return 0;
    }
    result = is_humiliated(player, card, AL_TRIG);
    power = toughness;
  }
  if (result != 0)
  {
    return 0;
  }
  return power;
}

/* ManalinkEh 2022: 0x268d940; original functions/functions.c:12541. */
int declare_attackers_trigger_test(int player, int card, event_t event,
                                        declare_attackers_trigger_t mode, int attacker_player,
                                        int attacker_card, test_definition_t *test)
{
  int result;
  store_attackers(player, card, event, mode, attacker_player, attacker_card, test);
  result = resolve_declare_attackers_trigger(player, card, event, mode);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 attacker/keyword helpers. */

/* ManalinkEh 2022: 0x268e850; original functions/functions.c:13220. */
int charge_mana_for_activated_ability(int player, int card, int cless, int black, int blue,
    int green, int red, int white)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->ext->special_flags2 & SF2_MYCOSYNTH_LATTICE)
  {
    cless += black + blue + green + red + white;
    black = blue = green = red = white = 0;
  }
  int cost = get_cost_mod_for_activated_abilities(player, card, cless, black, blue, green, red, white);
  instance = get_card_instance(player, card);
  if (instance->ext->special_flags2 & SF2_CELESTIAL_DAWN)
  {
    white += black + blue + green + red;
    black = blue = green = red = 0;
  }
  if (cless == -1 && cost != -1)
  {
    charge_mana(player, COLOR_COLORLESS, cost);
    if (spell_fizzled == 1)
      return 0;
    cost = -1;
  }
  else if (!(cost || black || blue || green || red || white))
    return 1;
  pay_mana_xbugrwaU[0] = cost;
  pay_mana_xbugrwaU[2] = blue;
  pay_mana_xbugrwaU[3] = green;
  pay_mana_xbugrwaU[5] = white;
  pay_mana_xbugrwaU[1] = black;
  pay_mana_xbugrwaU[4] = red;
  charge_mana(player, COLOR_COLORLESS, 0);
  return spell_fizzled != 1;
}

/* ManalinkEh 2022: 0x268e9e0; original functions/functions.c:2604. */
int charge_mana_for_activated_ability_while_resolving(int player, int card, event_t event,
                                                           int colorless, int black, int blue,
                                                           int green, int red, int white)
{
  int result;

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    event = EVENT_RESOLVE_TRIGGER;
  }
  put_card_or_activation_onto_stack(player, card, event, 0, 0);
  spell_fizzled = 0;
  ldoubleclicked = 0;
  result = charge_mana_for_activated_ability(player, card, colorless, black, blue, green, red,
                                                  white);
  obliterate_top_card_of_stack();
  return result;
}

/* ManalinkEh 2022: 0x26901c0; original functions/functions.c:13251. */
int has_mana_for_activated_ability(int player, int card, int cless, int black, int blue,
    int green, int red, int white)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->ext->special_flags2 & SF2_MYCOSYNTH_LATTICE)
  {
    cless += blue + black + green + red + white;
    black = blue = green = red = white = 0;
  }
  int cost = get_cost_mod_for_activated_abilities(player, card, cless, black, blue, green, red, white);
  instance = get_card_instance(player, card);
  if (instance->ext->special_flags2 & SF2_CELESTIAL_DAWN)
  {
    white += black + blue + green + red;
    black = blue = green = red = 0;
  }
  if (!(cost || black || blue || green || red || white))
    return 1;
  return has_mana_multi(player, cost, black, blue, green, red, white) != 0;
}

/* ManalinkEh 2022: 0x2692a40; original functions/functions.c:14935. */
int counterspell_validate(int player, int card, target_definition_t *td, int target_num)
{
  int32_t saved_preferred_controller;
  int valid;
  card_instance_t *instance;
  target_definition_t td_default;

  if (td == NULL)
  {
    counterspell_target_definition(player, card, &td_default, 0);
    td_default.preferred_controller = 2;
    valid = validate_target(player, card, &td_default, target_num);
  }
  else
  {
    saved_preferred_controller = td->preferred_controller;
    td->preferred_controller = td->allowed_controller;
    valid = validate_target(player, card, td, target_num);
    td->preferred_controller = saved_preferred_controller;
  }
  instance = get_card_instance(player, card);
  if (valid != 0)
  {
    instance =
        get_card_instance(instance->targets[target_num].player, instance->targets[target_num].card);
    if ((instance->state & 0x20) != 0)
    {
      return 1;
    }
  }
  spell_fizzled = 1;
  return 0;
}

/* ManalinkEh 2022: 0x2692b30; original functions/functions.c:14863. */
int counterspell(int player, int card, event_t event, target_definition_t *td,
                      int target_num)
{
  int32_t saved_target_player;
  int32_t saved_target_card;
  card_instance_t *instance;
  int result;
  target_definition_t td_default;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    if (card_on_stack_controller == -1)
    {
      return 0;
    }
    if (td == NULL)
    {
      td = &td_default;
      counterspell_target_definition(player, card, td, 0);
    }
    result = card_on_stack;
    if (target_num < 0)
    {
      saved_target_player = instance->targets[0].player;
      instance->targets[0].player = card_on_stack_controller;
      saved_target_card = instance->targets[0].card;
      instance->targets[0].card = result;
      result = would_validate_target(player, card, td, 0);
      instance->targets[0].player = saved_target_player;
      instance->targets[0].card = saved_target_card;
      if (result == 0)
      {
        return 0;
      }
    }
    else
    {
      instance->targets[target_num].player = card_on_stack_controller;
      instance->targets[target_num].card = result;
      if ((int)(unsigned int)instance->number_of_targets <= target_num)
      {
        instance->number_of_targets = (char)target_num + '\x01';
      }
      if (would_validate_target(player, card, td, target_num) == 0)
      {
        return 0;
      }
    }
    return 99;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    if (card_on_stack_controller == -1)
    {
      spell_fizzled = 1;
    }
    else
    {
      instance->targets[target_num].player = card_on_stack_controller;
      instance->targets[target_num].card = card_on_stack;
      if ((int)(unsigned int)instance->number_of_targets <= target_num)
      {
        instance->number_of_targets = (char)target_num + '\x01';
      }
    }
    ai_modifier = ai_modifier + -0x24;
    return 0;
  }
  if (event == EVENT_CAN_CHANGE_TARGET)
  {
    instance = get_card_instance(player, card);
    return (instance->state >> 0x14 ^ 1) & 1;
  }
  if (event == EVENT_CHANGE_TARGET)
  {
    instance->targets[9].player = player;
    instance->targets[9].card = card;
    instance->targets[target_num].player = player;
    instance->targets[target_num].card = card;
    return 0;
  }
  if (event != EVENT_CAN_COUNTER)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (counterspell_validate(player, card, td, target_num) != 0)
      {
        real_counter_a_spell(player, card, instance->targets[target_num].player,
                             instance->targets[target_num].card);
      }
      instance->number_of_targets = '\0';
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
    return 0;
  }
  if (raw_mana_available[player][2] < 2)
  {
    return 0;
  }
  ai_modifier = ai_modifier + 0x18;
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 entry-trigger helpers. */

/* ManalinkEh 2022: 0x269c8c0; original functions/functions.c:13396. */
int granted_generic_activated_ability(int granting_player, int granting_card, int granted_to_player,
                                      int granted_to_card, event_t event, int mode, int cless,
                                      int black, int blue, int green, int red, int white,
                                      uint32_t variable_costs, target_definition_t *td,
                                      const char *prompt)
{
  int old_number_of_targets;
  char counter_cost_mode;
  card_instance_t *instance;
  int result;
  unsigned int untap_cost;
  unsigned int sacrifice_cost;
  card_instance_t *other_instance;
  unsigned int used_abilities;
  target_definition_t *td_00;
  card_instance_t *current_instance;
  unsigned int sacrifice_choice;
  unsigned int state_or_usage;
  unsigned int exclude_source;
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(granting_player, granting_card);
    if (granting_card != affected_card)
    {
      return 0;
    }
    if (granting_player != affected_card_controller)
    {
      return 0;
    }
    if ((mode & 0x2000U) == 0)
    {
      return 0;
    }
    instance->targets[2].player = 0;
    return 0;
  }
  counter_cost_mode = (char)(variable_costs >> 0x18);
  if (event == EVENT_CAN_ACTIVATE)
  {
    instance = get_card_instance(granting_player, granting_card);
    other_instance = get_card_instance(granted_to_player, granted_to_card);
    if ((mode & 1U) != 0)
    {
      result = is_tapped(granted_to_player, granted_to_card);
      if (result != 0)
      {
        return 0;
      }
      current_instance = get_card_instance(granted_to_player, granted_to_card);
      if ((current_instance->ext->special_flags2 & SF2_KATABATIC_WINDS) != 0)
      {
        return 0;
      }
    }
    if ((((mode & 0x80001U) != 0) &&
         (result = is_what(granted_to_player, granted_to_card, 2), result != 0)) &&
        (result = is_sick(granted_to_player, granted_to_card), result != 0))
    {
      return 0;
    }
    if (mode & 0x100U)
    {
      if (can_sorcery_be_played(granted_to_player, EVENT_CAN_ACTIVATE) == 0)
      {
        return 0;
      }
      if (is_what(granted_to_player, granted_to_card, 2) != 0)
      {
        return 0;
      }
      mode = mode | 4;
      td->flags = td->flags | 5;
      cless = get_updated_equip_cost(granted_to_player, granted_to_card, cless);
    }
    if (mode & 4U)
    {
      if (((mode & 0x40000U) == 0) || ((other_instance->state & 0x100000) != 0))
      {
        result = can_target(td);
      }
      else
      {
        current_instance = get_card_instance(granted_to_player, granted_to_card);
        current_instance->state = current_instance->state | 0x100000;
        result = can_target(td);
        current_instance = get_card_instance(granted_to_player, granted_to_card);
        if ((current_instance->state & 0x100000) != 0)
        {
          current_instance->state = current_instance->state & 0xffefffff;
        }
      }
      if (result == 0)
      {
        return 0;
      }
    }
    if (((mode & 0x10U) != 0) &&
        (result = can_sorcery_be_played(granted_to_player, EVENT_CAN_ACTIVATE), result == 0))
    {
      return 0;
    }
    if (((mode & 0x20000020U) != 0) && (hand_count[granted_to_player] < 1))
    {
      return 0;
    }
    if (((mode & 0x40U) != 0) &&
        (result = count_counters(granted_to_player, granted_to_card, COUNTER_P1_P1), result < 1))
    {
      return 0;
    }
    if (((mode & 0x80U) != 0) &&
        (result = count_counters(granted_to_player, granted_to_card, COUNTER_M1_M1), result < 1))
    {
      return 0;
    }
    if (counter_cost_mode == '\x01')
    {
      result = count_counters(granted_to_player, granted_to_card,
                              variable_costs >> 0x10 & COUNTER_invalid);
      if (result < (int)(variable_costs >> 8 & 0xff))
      {
        return 0;
      }
    }
    else if ((counter_cost_mode == '\x02') &&
             (result = count_player_counters(granted_to_player, variable_costs >> 0x10 & 0xff),
              result < (int)(variable_costs >> 8 & 0xff)))
    {
      return 0;
    }
    if (((mode & 0x400U) != 0) &&
        (result = counterspell(granted_to_player, granted_to_card, EVENT_CAN_CAST, td, -1),
         result == 0))
    {
      return 0;
    }
    if ((((mode & 0x2000U) != 0) && (result = instance->targets[2].player, 0 < result)) &&
        ((result >> ((unsigned char)granted_to_player & 0x1f) & 1U) != 0))
    {
      return 0;
    }
    if ((mode & 0x8000U) != 0)
    {
      if (current_phase != 4)
      {
        return 0;
      }
      result = count_upkeeps(granted_to_player);
      if (result < 1)
      {
        return 0;
      }
    }
    if ((mode & 0x10000U) == 0)
    {
      if (((mode & 0x40000000U) != 0) && (current_turn == granted_to_player))
      {
        return 0;
      }
    }
    else
    {
      if (current_turn != granted_to_player)
      {
        return 0;
      }
      if ((mode & 0x40000000U) != 0)
      {
        return 0;
      }
    }
    if (((mode & 0x80000U) != 0) &&
        (result = is_tapped(granted_to_player, granted_to_card), result == 0))
    {
      return 0;
    }
    if (((mode & 0x200U) != 0) &&
        (result = can_sacrifice_as_cost(granted_to_player, 1, 2, 0, 0, 0, 0, 0, 0, 0, -1, 0),
         result == 0))
    {
      return 0;
    }
    if (((mode & 8U) != 0) &&
        (result = can_sacrifice_this_as_cost(granted_to_player, granted_to_card), result == 0))
    {
      return 0;
    }
    if ((((mode & 0x208U) != 0) && ((mode & 0x40000U) != 0)) &&
        (result = count_subtype(granted_to_player, 2, -1), result < 2))
    {
      return 0;
    }
    if (((mode & 0x100000U) != 0) &&
        (((ai_is_speculating == 1 ||
           ((granted_to_player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
          (instance->targets[2].card == 1))))
    {
      return 0;
    }
    if (('\0' < (char)variable_costs) &&
        (result = can_pay_life_impl(granted_to_player, (int)(char)variable_costs, 1), result == 0))
    {
      return 0;
    }
    if ((mode & 0x400000U) != 0)
    {
      target_definition_t td_default;
      td_00 = td;
      if (td == NULL)
      {
        default_target_definition(granted_to_player, granted_to_card, &td_default, 0);
        td_default.zone = TARGET_ZONE_PLAYERS;
        td_00 = &td_default;
        td_default.allowed_controller = 1 - granted_to_player;
      }
      if (would_validate_arbitrary_target(td_00, 1 - granted_to_player, -1) == 0)
      {
        return 0;
      }
    }
    if (((mode & 0x10000000U) != 0) && (0x14 < current_phase))
    {
      return 0;
    }
    if (((mode & 0x1000U) != 0) && (8 < current_phase - 0x15U))
    {
      return 0;
    }
    if ((mode & 0x800000U) != 0)
    {
      if ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) == 0)
      {
        return 0;
      }
      if (td == NULL)
      {
        if (other_instance->kill_code != '\x02')
        {
          return 0;
        }
        if (granted_to_player == 1 && can_regenerate(1, granted_to_card) == 0)
        {
          return 0;
        }
      }
    }
    if ((mode < 0) && (granted_to_player == 1))
    {
      instance = get_card_instance(1, granted_to_card);
      if ((instance->ext->special_flags & SF_TYPE_ALREADY_CHANGED) != 0)
      {
        return 0;
      }
      if (is_what(1, granted_to_card, 2) != 0)
      {
        return 0;
      }
    }
    if ((mode & 0x800U) == 0)
    {
      if (cless == -2)
      {
        cless = 0;
      }
      result = has_mana_for_activated_ability(granted_to_player, granted_to_card, cless, black,
                                              blue, green, red, white);
    }
    else
    {
      if (cless == -2)
      {
        cless = 0;
      }
      result = has_mana_multi(granted_to_player, cless, black, blue, green, red, white);
    }
    if (result != 0)
    {
      if (can_use_activated_abilities(granted_to_player, granted_to_card) == 0)
      {
        if ((mode & 0x200000U) == 0)
        {
          return 0;
        }
        result = can_produce_mana(granted_to_player, granted_to_card);
        if (result == 0)
        {
          return 0;
        }
      }
      else
      {
        result = can_produce_mana(granted_to_player, granted_to_card);
        if ((result == 0) && ((mode & 0x200000U) != 0))
        {
          return 0;
        }
      }
      if ((((granting_player == granted_to_player) && (granting_card == granted_to_card)) ||
           (result = is_humiliated(granting_player, granting_card, AL_STATIC), result == 0)) &&
          (((unknown62BCEC != 0 && (needed_mana_colors == 0)) || ((mode & 0x200000U) != 0))))
      {
        return (((mode & 0xf800400U) == 0) ? 1U : 99U);
      }
    }
    return 0;
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }
  target_definition_t td_default;
  hack_what_was_sacced_for_gaa = 0;
  instance = get_card_instance(granting_player, granting_card);
  if ((mode & 0x100U) != 0)
  {
    mode = mode | 4;
    cless = get_updated_equip_cost(granted_to_player, granted_to_card, cless);
  }
  if ((mode & 0x800U) == 0)
  {
    result = 0;
    if (cless != -2)
    {
      result = cless;
    }
    result = charge_mana_for_activated_ability(granted_to_player, granted_to_card, result, black,
                                               blue, green, red, white);
    if ((mode & 0x200000U) == 0)
    {
      tapped_for_mana_color = -1;
    }
    if (result == 0)
    {
      return 0;
    }
  }
  else
  {
    pay_mana_xbugrwaU[0] = cless;
    if (cless == -2)
    {
      pay_mana_xbugrwaU[0] = 0;
    }
    pay_mana_xbugrwaU[1] = black;
    pay_mana_xbugrwaU[2] = blue;
    pay_mana_xbugrwaU[3] = green;
    pay_mana_xbugrwaU[4] = red;
    pay_mana_xbugrwaU[5] = white;
    charge_mana(granted_to_player, COLOR_COLORLESS, 0);
    if (spell_fizzled == 1)
    {
      if ((mode & 0x200000U) != 0)
      {
        return 0;
      }
      tapped_for_mana_color = -1;
      return 0;
    }
    if ((mode & 0x200000U) == 0)
    {
      tapped_for_mana_color = -1;
    }
  }
  if (cless == -2)
  {
    result = charge_mana_for_multi_x(granted_to_player, 0, 2);
    instance->info_slot = result / 2;
  }
  else if (((cless + 2U < 2) || (black == -1)) ||
           ((blue == -1 || ((green == -1 || red == -1 || (white == -1))))))
  {
    instance->info_slot = x_value;
  }
  exclude_source = mode & 0x40000;
  if (exclude_source != 0)
  {
    other_instance = get_card_instance(granted_to_player, granted_to_card);
    other_instance->state = other_instance->state | 0x100000;
  }
  sacrifice_choice = 0;
  untap_cost = mode & 0x80000;
  sacrifice_cost = mode & 0x200;
  if (sacrifice_cost)
  {
    test_definition_t test;
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    sacrifice_choice = new_sacrifice(granted_to_player, granted_to_card, granted_to_player,
                                     SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
    if (!sacrifice_choice)
      spell_fizzled = 1;
  }
  if (!sacrifice_cost || sacrifice_choice)
    do
    {
      if (mode & 4U)
      {
        if ((mode & 2U) == 0)
        {
          load_text(NULL, prompt);
          prompt = text_lines[0];
        }
        target_t tgt;
        other_instance = get_card_instance(td->player, td->card);
        old_number_of_targets = other_instance->number_of_targets;
        result = select_target(td->player, td->card, td, prompt, &tgt);
        other_instance->number_of_targets = old_number_of_targets;

        if ((((result != 0) && (((((mode & 0x800000U) == 0 || (granted_to_player != 1)) ||
                                  (result = can_regenerate(tgt.player, tgt.card), result != 0)) &&
                                 (((mode & 0x2000000U) == 0 ||
                                   (current_instance = get_card_instance(tgt.player, tgt.card),
                                    current_instance->damage_target_card == -1)))))) &&
             (((mode & 0x4000000U) == 0 ||
               (current_instance = get_card_instance(tgt.player, tgt.card),
                current_instance->damage_target_card != -1)))) &&
            (((mode & 0x8000000U) == 0 ||
              ((current_instance = get_card_instance(tgt.player, tgt.card),
                current_instance->damage_target_card == -1 &&
                    (current_instance->damage_target_player == granted_to_player))))))
        {
          if (((mode & 0x100U) != 0) &&
              (((current_instance = is_equipping(granted_to_player, granted_to_card),
                 current_instance != NULL &&
                     (other_instance->targets[0].player == (int)instance->damage_target_player)) &&
                (other_instance->targets[0].card == instance->damage_target_card))))
          {
            ai_modifier = ai_modifier + -0x60;
          }
          instance->targets[0].card = tgt.card;
          instance->targets[0].player = tgt.player;
          instance->number_of_targets = '\x01';
        }
        else
          break;
      }
      if ((mode & 0x400000U) != 0)
      {
        result = 1 - granted_to_player;
        if (td == NULL)
        {
          default_target_definition(granted_to_player, granted_to_card, &td_default, 0);
          td = &td_default;
          td_default.zone = TARGET_ZONE_PLAYERS;
          td_default.allowed_controller = result;
        }
        if (validate_arbitrary_target(td, result, -1) == 0)
          break;
        instance->targets[0].card = -1;
        instance->targets[0].player = result;
        instance->number_of_targets = '\x01';
      }
      if ((mode & 0x400U) != 0)
      {
        if (card_on_stack_controller == -1)
          break;
        instance->targets[0].player = card_on_stack_controller;
        instance->targets[0].card = card_on_stack;
        instance->number_of_targets = '\x01';
      }
      if ((mode & 1U) != 0)
      {
        add_state(granted_to_player, granted_to_card, 0x10);
      }
      if (untap_cost != 0)
      {
        remove_state(granted_to_player, granted_to_card, 0x10);
      }
      if ('\0' < (char)variable_costs)
      {
        lose_life(granted_to_player, (int)(char)variable_costs);
      }
      if ((mode & 8U) != 0)
      {
        kill_card(granted_to_player, granted_to_card, KILL_SACRIFICE);
      }
      if (((mode & 0x20U) == 0) ||
          (discard(granted_to_player, 4, granting_player), spell_fizzled != 1))
      {
        if ((mode & 0x20000000U) != 0)
        {
          discard(granted_to_player, 1, granting_player);
        }
        if ((mode & 0x40U) != 0)
        {
          remove_counters(granted_to_player, granted_to_card, granted_to_player, granted_to_card,
                          COUNTER_P1_P1, 1);
        }
        if ((mode & 0x80U) != 0)
        {
          remove_counters(granted_to_player, granted_to_card, granted_to_player, granted_to_card,
                          COUNTER_M1_M1, 1);
        }
        if (counter_cost_mode == '\x01')
        {
          remove_counters(granted_to_player, granted_to_card, granted_to_player, granted_to_card,
                          variable_costs >> 0x10 & COUNTER_invalid, variable_costs >> 8 & 0xff);
        }
        else if (counter_cost_mode == '\x02')
        {
          remove_player_counters(granted_to_player, variable_costs >> 0x10 & 0xff,
                                 variable_costs >> 8 & 0xff);
        }
        if ((mode & 0x2000U) != 0)
        {
          state_or_usage = instance->targets[2].player;
          used_abilities = 0;
          if (0 < (int)state_or_usage)
          {
            used_abilities = state_or_usage;
          }
          instance->targets[2].player =
              used_abilities | 1 << ((unsigned char)granted_to_player & 0x1f);
        }
        if ((mode & 0x4000U) != 0)
        {
          kill_card(granted_to_player, granted_to_card, KILL_EXILE);
        }
        if ((mode & 0x20000U) != 0)
        {
          bounce_permanent(granted_to_player, granted_to_card);
        }
        if ((mode < 0) && (granted_to_player == 1))
        {
          set_special_flags(1, granted_to_card, 4);
        }
        if (sacrifice_cost != 0)
        {
          kill_card((int)sacrifice_choice >> 0x10 & 0xff, sacrifice_choice >> 0x18, KILL_SACRIFICE);
        }
        if (exclude_source != 0)
        {
          instance = get_card_instance(granted_to_player, granted_to_card);
          if ((instance->state & 0x100000) != 0)
          {
            instance->state = instance->state & 0xffefffff;
          }
        }
        if (untap_cost != 0)
        {
          untap_card(granted_to_player, granted_to_card);
        }
        hack_what_was_sacced_for_gaa = sacrifice_choice;
        return 1;
      }
    } while (false);

  if (exclude_source)
  {
    instance = get_card_instance(granted_to_player, granted_to_card);
    state_or_usage = instance->state;
    if (state_or_usage & 0x100000)
      instance->state = state_or_usage & 0xffefffff;
  }
  if (sacrifice_cost)
  {
    // Preserve the zero-choice cancellation cleanup as well as marked choices.
    state_or_usage = sacrifice_choice >> 0x18;
    exclude_source = (int)sacrifice_choice >> 0x10 & 0xff;
    instance = get_card_instance(exclude_source, state_or_usage);
    if (instance->state & 0x100000)
      instance->state &= 0xffefffff;
  }
  if (mode & 1U)
    remove_state(granted_to_player, granted_to_card, 0x10);
  if (untap_cost)
    add_state(granted_to_player, granted_to_card, 0x10);
  spell_fizzled = 1;
  hack_what_was_sacced_for_gaa = 0;
  return 0;
}

/* ManalinkEh 2022: 0x269e2b0; original functions/functions.c:11835. */
int vanilla_creature_pumper(int player, int card, event_t event, int req_cless, int req_black,
                            int req_blue, int req_green, int req_red, int req_white, int flags,
                            int pow, int tou, int key, int s_key, target_definition_t *td)
{
  target_definition_t td_default;
  if (event != EVENT_ACTIVATE && (uint32_t)event - (uint32_t)EVENT_RESOLVE_ACTIVATION > 1U &&
      event != EVENT_CLEANUP)
    return 0;
  if (!td)
  {
    td = &td_default;
    default_target_definition(player, card, td, 2);
    td_default.preferred_controller = player;
  }
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(td))
  {
    card_instance_t *instance = get_card_instance(player, card);
    pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                           pow, tou, key, s_key);
  }
  return granted_generic_activated_ability(player, card, player, card, event, flags | 4, req_cless,
                                           req_black, req_blue, req_green, req_red, req_white, 0,
                                           td, "TARGET_CREATURE");
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int deflect_away_from_me(int player, int card, event_t event, int mode, int cless, int black,
                         int blue, int green, int red, int white, uint32_t variable_costs,
                         target_definition_t *td, int must_be_creature);
static char *has_only_1_target_and_could_legally_change_target_and_targets_me(int who_chooses,
                                                                              int player, int card,
                                                                              int targeting_player,
                                                                              int targeting_card);

/* ManalinkEh 2022: 0x269db00; original functions/functions.c:13271. */
int generic_activated_ability(int player, int card, event_t event, int mode, int cless,
                                   int black, int blue, int green, int red, int white,
                                   uint32_t variable_costs, target_definition_t *td,
                                   const char *prompt)
{
  int result;
  result =
      granted_generic_activated_ability(player, card, player, card, event, mode, cless, black,
                                             blue, green, red, white, variable_costs, td, prompt);
  return result;
}

/* ManalinkEh 2022: 0x269e690; original functions/functions.c:13279. */
int attachment_granting_activated_ability(int player, int card, event_t event, int mode,
    int cless, int black, int blue, int green, int red, int white, uint32_t variable_costs,
    target_definition_t *td, const char *prompt)
{
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE &&
      event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  card_instance_t *instance = in_play(player, card);
  if (!instance)
    return 0;
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    if (event == EVENT_CAN_ACTIVATE &&
        (instance->damage_target_card < 0 || instance->damage_target_player < 0 ||
         !in_play(instance->damage_target_player, instance->damage_target_card)))
      return 0;
    instance->targets[9].player = instance->damage_target_player;
    instance->targets[9].card = instance->damage_target_card;
  }
  int recipient_player = instance->targets[9].player;
  if (recipient_player < 0)
    return 0;
  int recipient_card = instance->targets[9].card;
  if (recipient_card < 0)
    return 0;
  if (td)
  {
    td->player = recipient_player;
    td->card = recipient_card;
    if (td->illegal_abilities & KEYWORD_SHROUD)
    {
      td->illegal_abilities = get_protections_from(recipient_player, recipient_card);
      recipient_card = instance->targets[9].card;
      recipient_player = instance->targets[9].player;
    }
  }
  return granted_generic_activated_ability(player, card, recipient_player, recipient_card,
      event, mode, cless, black, blue, green, red, white, variable_costs, td, prompt);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269e810; original functions/functions.c:13321. */
int equipment_granting_activated_ability(int player, int card, event_t event,
    const char *dialog_option, int equip_cost, int mode, int cless, int black, int blue,
    int green, int red, int white, uint32_t variable_costs, target_definition_t *td,
    const char *prompt)
{
  enum
  {
    CHOICE_ABILITY = 1,
    CHOICE_EQUIP = 2
  };

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE &&
      event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  card_instance_t *instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_ACTIVATION && !is_equipping(player, card))
  {
    if (event == EVENT_ACTIVATE)
      instance->info_slot = CHOICE_EQUIP;
    return basic_equipment(player, card, event, equip_cost);
  }
  int equip_available = can_activate_basic_equipment(player, card, event, equip_cost);
  int granted_available = attachment_granting_activated_ability(player, card,
      EVENT_CAN_ACTIVATE, mode, cless, black, blue, green, red, white, variable_costs, td, prompt);
  int choice = DIALOG(player, card, event,
      dialog_option, granted_available, 2, "Equip", equip_available, 1);
  if (event == EVENT_CAN_ACTIVATE)
    return choice;
  if (event == EVENT_ACTIVATE)
  {
    if (choice == CHOICE_ABILITY)
      attachment_granting_activated_ability(player, card, event, mode, cless,
          black, blue, green, red, white, variable_costs, td, prompt);
    else if (choice == CHOICE_EQUIP)
      activate_basic_equipment(player, card, equip_cost);
    return 0;
  }
  if (choice == CHOICE_ABILITY)
  {
    attachment_granting_activated_ability(player, card, event, mode, cless,
        black, blue, green, red, white, variable_costs, td, prompt);
    return 1;
  }
  if (choice == CHOICE_EQUIP)
    resolve_activation_basic_equipment(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2678000; original functions/functions.c:15356. */
static char *has_only_1_target_and_could_legally_change_target_and_targets_me(int who_chooses,
                                                                              int player, int card,
                                                                              int targeting_player,
                                                                              int targeting_card)
{
  card_instance_t *instance;
  char *error;
  int result;

  instance = get_card_instance(player, card);
  error = "must have only 1 target";
  if (instance->number_of_targets == '\x01')
  {
    result = call_card_function(player, card, EVENT_CAN_CHANGE_TARGET);
    if (result == 0)
    {
      return "cannot change target";
    }
    error = "must target this card";
    if ((instance->targets[0].player == targeting_player) &&
        (instance->targets[0].card == targeting_card))
    {
      error = NULL;
    }
  }
  return error;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269e400; original functions/functions.c:15373. */
int deflect_away_from_me(int player, int card, event_t event, int mode, int cless, int black,
                         int blue, int green, int red, int white, uint32_t variable_costs,
                         target_definition_t *td, int must_be_creature)
{
  int32_t new_target_player;
  int saved_attacking_player;
  card_instance_t *instance;
  int result;
  int saved_attacking_card;
  int stack_player;
  card_instance_t *spell_instance;
  target_definition_t td_default;

  if (td == NULL)
  {
    td = &td_default;
    counterspell_target_definition(player, card, td, 0xff);
    td_default.special = td_default.special | 0x8000;
    td_default.extra = (intptr_t)has_only_1_target_and_could_legally_change_target_and_targets_me;
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      if ((1 < stack_size) && (result = granted_generic_activated_ability(
                                   player, card, player, card, EVENT_ACTIVATE, mode | 0x400, cless,
                                   black, blue, green, red, white, variable_costs, td, NULL),
                               stack_player = card_on_stack_controller, result != 0))
      {

        instance->number_of_targets = '\x01';
        instance->targets[0].player = stack_player;
        result = card_on_stack;
        instance->targets[0].card = card_on_stack;
        if (counterspell_validate(player, card, td, 0) != 0)
        {

          if (must_be_creature != 0)
          {
            set_special_flags2(stack_player, result, 0x4000);
          }
          saved_attacking_player = attacking_card_controller;
          saved_attacking_card = attacking_card;
          attacking_card_controller = player;
          attacking_card = card;
          call_card_function(stack_player, result, EVENT_CHANGE_TARGET);
          attacking_card = saved_attacking_card;
          attacking_card_controller = saved_attacking_player;
          remove_special_flags2(stack_player, result, 0x4000);
        }
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) &&
             (stack_player = counterspell_validate(player, card, td, 0), stack_player != 0))
    {

      spell_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      new_target_player = instance->targets[1].player;
      spell_instance->targets[0].card = instance->targets[1].card;
      spell_instance->targets[0].player = new_target_player;
    }
    return 0;
  }
  return granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE,
                                                 mode | 0x400, cless, black, blue, green, red,
                                                 white, variable_costs, td, NULL);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 death-trigger counting and resolution. */

/* ManalinkEh 2022: 0x26a0250; original functions/functions.c:15783. */
int generic_spell(int player, int card, event_t event, int flags, target_definition_t *td,
                  const char *prompt, int max_targets, test_definition_t *this_test)
{
  int i;
  enum
  {
    SF2_REDIRECT = 0x40000000
  };

  target_t *selected_target;
  int old_number_of_targets;
  int target_player;
  target_t *target_begin;
  card_instance_t *instance;
  int target_value;
  int32_t t_player;
  int result;
  unsigned int count_or_mode;
  card_instance_t *current_instance;
  const char *prompt_00;
  card_instance_t *target_instance;
  const char **prompt_lines;
  int target_card;
  unsigned int card_00;
  target_t *cleanup_target;
  int rv;
  int min_targets;
  target_t *saved_target;
  int32_t base_target;
  int32_t selected_targets;
  int32_t cleanup_end;
  unsigned int sacrifice_choice;
  unsigned int targeting_flags;
  int aura_spell;
  int graveyard_flags;
  int32_t target_limit;
  int normal_targeting;
  card_instance_t *storage_inst;
  int p;
  int c;
  int targeted_player[2];

  instance = get_card_instance(player, card);
  aura_spell = (flags & 0x200);
  if (event == EVENT_GET_SELECTED_CARD && ((uint32_t)aura_spell) != 0)
  {
    engine_selected_card = (int)instance->damage_target_player << 8 | instance->damage_target_card;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    if (flags == 0)
    {
      return 1;
    }
    if ((flags & 1U) != 0)
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_SPELLSKITE) != 0)
      {
        old_number_of_targets = instance->number_of_targets;
        instance->number_of_targets = '\n';
        result = would_validate_target(player, card, td, 9);
        instance->number_of_targets = old_number_of_targets;
        return result;
      }
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_QUICKSILVER_DRAGON) != 0)
      {
        td->zone = 0x200;
        td->required_type = 2;
      }
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_REBOUND) != 0)
      {
        td->zone = 0x1000;
      }
      if ((flags & 0x20U) == 0)
      {
        if ((flags & 0x4000U) == 0)
        {
          if ((((uint32_t)aura_spell) != 0) && (0 < td->illegal_abilities))
          {
            result = target_available(player, card, td);
            if (result == 1)
            {
              for (int p = 0; p < 2; ++p)
              {
                for (int c = 0; c < active_cards_count[p]; ++c)
                {
                  if (in_play(p, c) != NULL && is_what(p, c, 0x47) != 0 &&
                      would_validate_arbitrary_target(td, p, c) != 0 &&
                      is_humiliated(p, c, AL_STATIC) == 0)
                  {
                    int csvid = get_id(p, c);
                    if (csvid == CARD_ID_TETSUO_UMEZAWA || csvid == CARD_ID_BARTEL_RUNEAXE)
                    {
                      return 0;
                    }
                  }
                }
              }
            }
          }
          if (max_targets != 1)
          {
            result = target_available(td->player, td->card, td);
            if (result < max_targets)
            {
              return 0;
            }
          }
        }
        if (((flags & 0x4000U) != 0 || max_targets == 1) && can_target(td) == 0)
        {
          return 0;
        }
      }
    }
    if ((flags & 4U) != 0)
    {
      if (check_battlefield_for_id(2, 0x23ba) != 0)
      {
        return 0;
      }
    }
    if ((flags & 0x1808U) != 0)
    {
      int grave_player = player;
      if ((flags & 0x800U) != 0)
      {
        grave_player = 1 - player;
      }
      if ((flags & 0x1000U) != 0)
      {
        grave_player = 2;
      }
      int found = 0;
      for (int p = 0; p < 2; ++p)
      {
        if ((p == grave_player || grave_player == 2) && new_special_count_grave(p, this_test) > 0 &&
            graveyard_has_shroud(p) == 0)
        {
          found = 1;
          break;
        }
      }
      if (!found)
      {
        return 0;
      }
    }
    if (((flags & 0x10U) != 0) && ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) == 0))
    {
      return 0;
    }
    if (((flags & 0x100U) != 0) && (((unsigned char)land_can_be_played & 4) == 0))
    {
      return 0;
    }
    if ((flags & 0x40U) != 0)
    {
      if (td == NULL)
      {
        result = opponent_is_valid_target(player, card);
      }
      else
      {
        instance->targets[0].card = -1;
        instance->targets[0].player = 1 - player;
        instance->number_of_targets = '\x01';
        result = would_validate_target(player, card, td, 0);
        instance->number_of_targets = '\0';
      }
      if (result == 0)
      {
        return 0;
      }
    }
    if ((flags & 2U) != 0)
    {
      if (counterspell(player, card, EVENT_CAN_CAST, td, -1) == 0)
      {
        return 0;
      }
    }
    if ((flags & 0x400U) != 0)
    {
      if (can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) == 0)
      {
        return 0;
      }
    }
    if ((flags & 0x8000U) != 0)
    {
      if (new_can_sacrifice_as_cost(player, 1, this_test) == 0)
      {
        return 0;
      }
    }
    if ((flags & 0x112U) == 0)
    {
      return 1;
    }
    return 99;
  }
  if (event == EVENT_CAN_CHANGE_TARGET)
  {
    if (((instance->targets[0].card != -1) && ((flags & 1U) != 0)) && (max_targets == 1))
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_REDIRECT) == 0)
      {
        current_instance =
            get_card_instance(instance->targets[0].player, instance->targets[0].card);
        current_instance->state = current_instance->state | 0x100000;
      }
    }
    if ((flags & 2U) == 0)
    {
      result =
          generic_spell(player, card, EVENT_CAN_CAST, flags, td, prompt, max_targets, this_test);
    }
    else
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->state & 0x100000) == 0)
      {
        return 1;
      }
      result = 0;
    }
    if (instance->targets[0].card == -1)
    {
      return result;
    }
    if ((flags & 1U) == 0)
    {
      return result;
    }
    if (max_targets == 1)
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_REDIRECT) != 0)
      {
        return result;
      }
      target_instance = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      if ((target_instance->state & 0x100000) != 0)
      {
        target_instance->state = target_instance->state & 0xffefffff;
        return result;
      }
      return result;
    }
    return result;
  }
  if (event != EVENT_CAST_SPELL && event != EVENT_CHANGE_TARGET)
    return 0;
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
  }
  normal_targeting = 0;
  if (flags == 0)
  {
    return 0;
  }
  current_instance = get_card_instance(player, card);
  if ((current_instance->ext->special_flags2 & SF2_FACE_DOWN_DUE_TO_MANIFEST) != 0)
  {
    return 0;
  }
  if (((player == 1) && (aura_spell != 0)) && (((unsigned char)trace_mode & 2) == 0))
  {
    ai_modifier = ai_modifier +
                  (unsigned int)((instance->targets[0].player == td->preferred_controller) ? 0 : -48) +
                  0x18;
  }
  result = 0;
  if (event == EVENT_CHANGE_TARGET)
  {
    current_instance = get_card_instance(player, card);
    if ((current_instance->ext->special_flags2 & SF2_SPELLSKITE) != 0)
    {
      count_or_mode = (unsigned int)instance->number_of_targets;
      int target_cards[count_or_mode];
      i = 0;
      while (i < (int)(unsigned int)instance->number_of_targets)
      {
        target_card = instance->targets[i].card;
        target_player = instance->targets[i].player;
        i = i + 1;
        if (target_card == -1)
        {
          target_card = get_internal_card_id_from_csv_id(target_player == player ? 0x615a : 0x6161);
          target_cards[i - 1] = target_card;
        }
        else
        {
          current_instance = get_card_instance(target_player, target_card);
          target_cards[i - 1] = current_instance->internal_card_id;
        }
      }
      return show_deck(player, target_cards, i,
                       " Select the target to change (Rules Engine = you, Deadbox = opponent)", 0,
                       gs_done_008b40e0);
    }
    current_instance = get_card_instance(player, card);
    if ((current_instance->ext->special_flags2 & SF2_QUICKSILVER_DRAGON) == 0)
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_REBOUND) != 0)
      {
        td->zone = 0x1000;
      }
    }
    else
    {
      td->zone = 0x200;
      td->required_type = 2;
    }
    if ((instance->targets[0].card != -1) && ((flags & 10U) == 0))
    {
      current_instance = get_card_instance(player, card);
      if ((current_instance->ext->special_flags2 & SF2_REDIRECT) == 0)
      {
        current_instance =
            get_card_instance(instance->targets[0].player, instance->targets[0].card);
        current_instance->state = current_instance->state | 0x100000;
      }
    }
  }
  if (flags & 4U)
  {
    target_value = is_token(player, card);
    if (target_value == 0)
    {
      set_x_for_x_spells(player, card, event, x_value);
    }
    set_special_flags2(player, card, 0x20000);
  }
  sacrifice_choice = 0;
  if (event != EVENT_CHANGE_TARGET)
  {
    if (flags & 0x400U)
    {
      normal_targeting = 1;
      test_definition_t test;
      new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
      sacrifice_choice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (!sacrifice_choice)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
    if (flags & 0x8000U)
    {
      sacrifice_choice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, this_test);
      if (!sacrifice_choice)
      {
        spell_fizzled = 1;
        return 0;
      }
    }
  }
  targeting_flags = (flags & 0x21U) != 0;
  graveyard_flags = flags & 0x1808;
  bool select_normal_targets;
  if (event == EVENT_CHANGE_TARGET)
  {
    storage_inst = get_card_instance(attacking_card_controller, attacking_card);
    max_targets = (int)instance->number_of_targets;
    get_card_instance(player, card);
    base_target = 1;
    select_normal_targets = graveyard_flags == 0 && (char)targeting_flags != 0;
    if (select_normal_targets)
      min_targets = max_targets;
  }
  else
  {
    normal_targeting = 1;
    current_instance = get_card_instance(player, card);
    select_normal_targets = ((current_instance->ext->special_flags & SF_TARGETS_ALREADY_SET) == 0 ||
                             (char)normal_targeting == 0) &&
                            graveyard_flags == 0 && (char)targeting_flags != 0;
    base_target = 0;
    storage_inst = instance;
    if (select_normal_targets)
      min_targets = 0;
  }
  if (select_normal_targets)
  {
    if (event == EVENT_CHANGE_TARGET)
    {
      td->allow_cancel = 0;
      td->who_chooses = attacking_card_controller;
    }
    target_limit = (base_target + max_targets);
    targeting_flags = flags & 0x80;
    instance->number_of_targets = '\0';
    targeted_player[0] = 0;
    targeted_player[1] = 0;
    selected_targets = base_target;
    rv = result;
    while (true)
    {
      result = can_target(td);
      if ((result == 0) || (target_limit <= selected_targets))
        break;
      prompt_00 = prompt;
      if (targeting_flags == 0)
      {
        prompt_lines = load_text_nocopy(prompt, NULL);
        prompt_00 = *prompt_lines;
      }
      if (select_target(player, card, td, prompt_00, storage_inst->targets + selected_targets) == 0)
        break;
      selected_target = &storage_inst->targets[selected_targets];
      result = selected_target->card;
      target_value = selected_target->player;
      if (result == -1)
      {
        if (targeted_player[target_value] == 0)
        {
          targeted_player[target_value] = 1;
          ++selected_targets;
        }
        else if (!(flags & 0x4000U))
        {
          storage_inst->number_of_targets += 0xff;
          char illegal_tgt[200];
          sprintf(illegal_tgt, gs_illegal_target_format, gs_target_error_cannot_target_this);
          display_error_message(illegal_tgt);
        }
        else
          ++selected_targets;
      }
      else
      {
        if (aura_spell)
        {
          saved_target = selected_target;
          target_value = is_humiliated(target_value, result, AL_STATIC);
          result = saved_target->card;
          bool rejects_aura = false;
          if (!target_value)
          {
            result = get_id(saved_target->player, result);
            rejects_aura = result == 0x32b || result == 0x242;
            if (!rejects_aura)
              result = saved_target->card;
          }
          if (rejects_aura)
          {
            char illegal_tgt[200];
            sprintf(illegal_tgt, gs_illegal_target_format, gs_target_error_cannot_target_this);
            display_error_message(illegal_tgt);
            instance->number_of_targets += 0xff;
            continue;
          }
          target_value = saved_target->player;
        }
        if (!(flags & 0x4000U))
        {
          target_instance = get_card_instance(target_value, result);
          target_instance->state |= 0x100000;
          ++selected_targets;
        }
        else
        {
          ++selected_targets;
          add_state(target_value, result, 0x200000);
        }
      }
    }
    result = rv;
    cleanup_end = base_target + selected_targets;
    if (base_target < cleanup_end)
    {
      cleanup_target = storage_inst->targets + base_target;
      target_begin = storage_inst->targets;
      do
      {
        if (cleanup_target->card != -1)
        {
          card_instance_t *cleanup_instance =
              get_card_instance(cleanup_target->player, cleanup_target->card);
          if ((cleanup_instance->state & 0x100000) != 0)
          {
            cleanup_instance->state = cleanup_instance->state & 0xffefffff;
          }
          remove_state(cleanup_target->player, cleanup_target->card, 0x200000);
        }
        cleanup_target = cleanup_target + 1;
      } while (cleanup_target != target_begin + cleanup_end);
    }
    if (event == EVENT_CHANGE_TARGET)
    {
      current_instance = get_card_instance(player, card);
      if (((current_instance->ext->special_flags2 & SF2_REDIRECT) == 0) &&
          (target_value = instance->targets[0].card, target_value != -1))
      {
        current_instance = get_card_instance(instance->targets[0].player, target_value);
        if ((current_instance->state & 0x100000) != 0)
        {
          current_instance->state = current_instance->state & 0xffefffff;
        }
      }
    }
    if ((flags & 0x20U) == 0)
    {
      if ((flags & 0x4000U) == 0)
      {
        if (selected_targets < (int)(base_target + max_targets))
        {
          spell_fizzled = 1;
        }
      }
      else if (base_target == selected_targets)
        spell_fizzled = 1;
    }
    if (selected_targets < (int)(base_target + min_targets))
    {
      spell_fizzled = 1;
    }
    graveyard_flags = 0x0;
  }
  if ((flags & 0x40U) != 0)
  {
    instance->targets[0].card = -1;
    instance->targets[0].player = 1 - player;
    instance->number_of_targets = '\x01';
  }
  bool select_graveyard_targets = true;
  if (flags & 2U)
  {
    if (event == EVENT_CHANGE_TARGET)
    {
      if (validate_arbitrary_target(td, player, card))
      {
        instance->targets[1].player = player;
        instance->targets[1].card = card;
      }
    }
    else
    {
      counterspell(player, card, event, td, 0);
      select_graveyard_targets = spell_fizzled != 1;
    }
  }
  if (select_graveyard_targets)
  {
    if (((uint32_t)graveyard_flags) != 0)
    {
      t_player = player;
      if ((flags & 0x800U) != 0)
      {
        t_player = 1 - player;
      }
      if ((flags & 0x1000U) != 0)
      {
        t_player = 2;
      }
      if (event == EVENT_CHANGE_TARGET)
      {
        p = attacking_card_controller;
        c = attacking_card;
      }
      else
      {
        c = card;
        p = player;
      }
      if (max_targets < 2)
      {
        count_or_mode = (unsigned int)this_test->ai_selection_mode;
        if (t_player == 2)
        {
          target_value = select_target_from_either_grave(p, c, SFG_0, count_or_mode, count_or_mode,
                                                         this_test, base_target, (base_target + 1));
        }
        else
        {
          target_value = select_target_from_grave_source(p, c, t_player, SFG_0, count_or_mode,
                                                         this_test, base_target);
        }
        if (target_value == -1)
        {
          spell_fizzled = 1;
          return result;
        }
        storage_inst->number_of_targets = storage_inst->number_of_targets + '\x01';
      }
      else
      {
        if (t_player == 2)
        {
          target_definition_t td9;
          default_target_definition(player, card, &td9, 0);
          td9.zone = TARGET_ZONE_PLAYERS;
          td9.illegal_abilities = 0;
          td9.who_chooses = p;
          target_t old_t0 = instance->targets[0];
          int old_nt = instance->number_of_targets;
          if (pick_target(&td9, "TARGET_PLAYER") == 0)
          {
            instance->targets[0].player = old_t0.player;
            instance->targets[0].card = old_t0.card;
            instance->number_of_targets = old_nt;
            return 0;
          }
          t_player = instance->targets[0].player;
          instance->targets[0].card = old_t0.card;
          instance->targets[0].player = old_t0.player;
          instance->number_of_targets = old_nt;
        }
        result =
            select_multiple_cards_from_graveyard(p, t_player, (unsigned int)((flags & 0x20U) == 0),
                                                 (unsigned int)this_test->ai_selection_mode,
                                                 this_test, max_targets, storage_inst->targets);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        if ((flags & 0x24U) == 0x20)
        {
          instance->info_slot = result;
        }
        instance->number_of_targets = instance->number_of_targets + (char)result;
      }
    }
  }
  if (sacrifice_choice)
  {
    card_00 = sacrifice_choice >> 0x18;
    count_or_mode = (int)sacrifice_choice >> 0x10 & 0xff;
    if (select_graveyard_targets && spell_fizzled != 1)
      kill_card(count_or_mode, card_00, KILL_SACRIFICE);
    else
    {
      current_instance = get_card_instance(count_or_mode, card_00);
      if (current_instance->state & 0x100000)
        current_instance->state &= 0xffefffff;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26a1550; original functions/functions.c:11588. */
int vanilla_pump(int player, int card, event_t event, target_definition_t *td, int power,
                 int toughness, int keyword, int sp_keyword)
{
  uint32_t hack_side_effect = (uint32_t)keyword & 0x1f000000U;
  target_definition_t td_prompt = *td;
  const char *prompt = "TARGET_CREATURE";
  if (td_prompt.allowed_controller == player)
    prompt = "TARGET_CREATURE_YOU_CONTROL";
  else if (td_prompt.allowed_controller == 1 - player)
    prompt = "TARGET_CREATURE_OPPONENTS_CONTROL";
  bool regenerates = hack_side_effect == 0x0a000000U || hack_side_effect == 0x18000000U;
  /* These spells create a regeneration shield; Oracle does not require the
     target to be dying. Resolution below uses regenerate_or_shield(). */
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int leg;
      if ((keyword & 0xe0ffffffU) == 0 && sp_keyword == 0)
        leg = pump_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, power, toughness);
      else
        leg = pump_ability_until_eot(player, card, instance->targets[0].player,
                                     instance->targets[0].card, power, toughness,
                                     keyword & 0xe0ffffffU, sp_keyword);
      if (hack_side_effect == 0x03000000U)
      {
        if (leg >= 0)
          alternate_legacy_text(1, player, leg);
        int cantrip_leg = cantrip(player, card, 1);
        if (cantrip_leg >= 0)
          alternate_legacy_text(2, player, cantrip_leg);
      }
      else if (hack_side_effect <= 0x03000000U)
      {
        if (hack_side_effect == 0x01000000U)
          draw_a_card(player);
        else if (hack_side_effect == 0x02000000U)
          scry(player, 1);
      }
      else if (regenerates)
      {
        if (leg >= 0)
          alternate_legacy_text(1, player, leg);
        int regen_leg = regenerate_or_shield(player, card, instance->targets[0].player,
                                             instance->targets[0].card);
        if (regen_leg >= 0)
          alternate_legacy_text(2, player, regen_leg);
        if (hack_side_effect == 0x18000000U)
          return 1;
      }
      else if (hack_side_effect == 0x04000000U)
      {
        if (leg >= 0)
          alternate_legacy_text(1, player, leg);
        return 1;
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, td, prompt, 1, NULL);
}

/* ManalinkEh 2022: 0x26a1900; original functions/functions.c:11658. */
int vanilla_instant_pump(int player, int card, event_t event, int allowed_controller,
                              int preferred_controller, int power, int toughness, int keyword,
                              int sp_keyword)
{
  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event != EVENT_CHECK_PUMP)
  {
    if (event != EVENT_CAN_CAST)
    {
      if (event == EVENT_CAST_SPELL)
      {
        if ((card != affected_card) || (player != affected_card_controller))
        {
          return 0;
        }
      }
      else if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
               (event != EVENT_CHANGE_TARGET))
      {
        return 0;
      }
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = preferred_controller;
    td.allowed_controller = allowed_controller;
    result = vanilla_pump(player, card, event, &td, power, toughness, keyword, sp_keyword);
    return result;
  }
  if ((power == 0 && (sp_keyword & 0x800U) == 0) && toughness == 0)
  {
    return 0;
  }
  instance = in_hand(player, card);
  if (instance != NULL)
  {
    result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
    if (result == 0)
    {
      return 0;
    }
    if ((sp_keyword & 0x800U) != 0)
    {
      toughness = 99;
    }
    result = 1 - player;
    if ((result == preferred_controller) || (preferred_controller == 2))
    {
      if (power < 0)
      {
        pumpable_power[result] = pumpable_power[result] + power;
      }
      if (toughness < 0)
      {
        pumpable_toughness[result] = pumpable_toughness[result] + toughness;
      }
    }
    if ((player != preferred_controller) && (preferred_controller != 2))
    {
      return 0;
    }
    if (0 < power)
    {
      pumpable_power[player] = pumpable_power[player] + power;
    }
    if (0 < toughness)
    {
      pumpable_toughness[player] = pumpable_toughness[player] + toughness;
      return 0;
    }
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x26a4540; original functions/functions.c:18743. */
int generic_activated_ability_with_sacrifice_as_cost(int player, int card, event_t event,
                                                     generic_activated_ability_flags_t flags,
                                                     int cless, int black, int blue, int green,
                                                     int red, int white, int gvc_costs,
                                                     target_definition_t *td, const char *prompt,
                                                     type_t type, test_definition_t *test,
                                                     sacrifice_and_report_value_flags_t sac_flags)
{
  int c;
  int sac;
  unsigned short original_quantity;
  int can_sacrifice;
  card_instance_t *instance;
  uint32_t reported_value;
  int amount;
  unsigned int value;
  int has_type;
  unsigned int card_00;
  uint32_t accumulated_value;
  char marked_for_sacrifice[151];
  test_definition_t test2;
  test_definition_t test2_1;
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    if ((flags & GAA_ONCE_PER_TURN) == GAA_NONE)
    {
      return 0;
    }
    instance->targets[2].player = 0;
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (test == NULL)
    {
      if (type == TYPE_NONE)
      {
        return 0;
      }
      can_sacrifice = can_sacrifice_type_as_cost(player, 1, type);
    }
    else
    {
      can_sacrifice = new_can_sacrifice_as_cost(player, card, test);
      if ((type != TYPE_NONE) &&
          (has_type = can_sacrifice_type_as_cost(player, 1, type), has_type != 0))
        return granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE,
                                                 flags, cless, black, blue, green, red, white,
                                                 gvc_costs, td, prompt);
    }
    if (can_sacrifice == 0)
    {
      return 0;
    }
    return granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE, flags,
                                             cless, black, blue, green, red, white, gvc_costs, td,
                                             prompt);
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }
  granted_generic_activated_ability(player, card, player, card, EVENT_ACTIVATE, flags, cless, black,
                                    blue, green, red, white, gvc_costs, td, prompt);
  if (spell_fizzled == 1)
  {
    return 0;
  }
  if (type != TYPE_NONE)
  {
    if (type == TYPE_ANY)
    {
      test = &test2;
      new_default_test_definition(test, 0xff, "Select a card to sacrifice.");
    }
    else
    {
      char buffer[100];
      char typetext[100];
      if (ai_is_speculating == 1)
      {
        typetext[0] = '\0';
      }
      else
      {
        type_text(typetext, 100, type, TYPETEXT_0);
      }
      scnprintf(buffer, 100, "Select a %s card to sacrifice.", typetext);
      test = &test2_1;
      new_default_test_definition(test, type, buffer);
    }
  }
  original_quantity = test->qty;
  if ((sac_flags & SARV_REPORT_COUNT) == SARV_0)
  {
    if (original_quantity < 2)
    {
      value =
          new_sacrifice(player, card, player,
                        SAC_RETURN_CHOICE | SAC_AS_COST | SAC_ALL_OR_NONE | SAC_JUST_MARK, test);
      if (value != 0)
      {
        card_00 = value >> 0x18;
        value = (int)value >> 0x10 & 0xff;
        instance = get_card_instance(value, card_00);
        if ((instance->state & 0x100000) != 0)
        {
          instance->state = instance->state & 0xffefffff;
        }
        if (spell_fizzled == 1)
        {
          return 0;
        }
        if (sac_flags != SARV_0)
        {
          instance = get_card_instance(value, card_00);
          if ((sac_flags & SARV_REPORT_POWER) == SARV_0)
          {
            reported_value = (uint32_t)instance->internal_card_id;
          }
          else
          {
            reported_value = get_abilities(value, card_00, EVENT_POWER, -1);
          }
          if ((sac_flags & SARV_REPORT_TOUGHNESS) != SARV_0)
          {
            reported_value = get_abilities(value, card_00, EVENT_TOUGHNESS, -1);
          }
          if ((sac_flags & SARV_REPORT_COLORS) != SARV_0)
          {
            reported_value = get_color(value, card_00);
          }
          if ((sac_flags & SARV_REPORT_CMC) != SARV_0)
          {
            reported_value = get_cmc(value, card_00);
          }
          instance = get_card_instance(player, card);
          instance->info_slot = reported_value;
        }
        kill_card(value, card_00, KILL_SACRIFICE);
        return 0;
      }
      spell_fizzled = 1;
      if ((flags & GAA_UNTAPPED) != GAA_NONE)
        remove_state(player, card, 0x10);
      return 0;
    }
    sac = mark_sacrifice(player, card, player,
                         SAC_RETURN_CHOICE | SAC_AS_COST | SAC_ALL_OR_NONE | SAC_JUST_MARK, test,
                         marked_for_sacrifice);
  }
  else
  {
    test->qty = 99;
    sac = mark_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_DONE, test,
                         marked_for_sacrifice);
    test->qty = original_quantity;
  }
  if (sac != 0)
  {
    if (spell_fizzled == 1)
    {
      return 0;
    }
    c = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        if (marked_for_sacrifice[c] != '\0')
        {
          instance = get_card_instance(player, c);
          instance->state = instance->state & 0xffefffff;
          if ((spell_fizzled != 1) && (instance = in_play(player, c), instance != NULL))
          {
            if (sac_flags != SARV_0)
            {
              accumulated_value = (uint32_t)0x0;
              if ((sac_flags & (SARV_REPORT_CMC | SARV_REPORT_COLORS | SARV_REPORT_TOUGHNESS |
                                SARV_REPORT_POWER)) == SARV_0)
              {
                instance = get_card_instance(player, c);
                accumulated_value = (uint32_t)instance->internal_card_id;
              }
              if ((sac_flags & SARV_REPORT_POWER) != SARV_0)
              {
                amount = get_abilities(player, c, EVENT_POWER, -1);
                accumulated_value = accumulated_value + amount;
              }
              if ((sac_flags & SARV_REPORT_TOUGHNESS) != SARV_0)
              {
                amount = get_abilities(player, c, EVENT_TOUGHNESS, -1);
                accumulated_value = accumulated_value + amount;
              }
              if ((sac_flags & SARV_REPORT_COLORS) != SARV_0)
              {
                value = get_color(player, c);
                accumulated_value = (uint32_t)((unsigned int)accumulated_value | value);
              }
              if ((sac_flags & SARV_REPORT_CMC) != SARV_0)
              {
                amount = get_cmc(player, c);
                accumulated_value = accumulated_value + amount;
              }
              instance = get_card_instance(player, card);
              instance->info_slot = accumulated_value;
            }
            kill_card(player, c, KILL_SACRIFICE);
          }
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
    if ((sac_flags & SARV_REPORT_COUNT) == SARV_0)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = sac;
    return 0;
  }
  spell_fizzled = 1;
  if ((flags & GAA_UNTAPPED) != GAA_NONE)
  {
    remove_state(player, card, 0x10);
  }
  return 0;
}

/* ManalinkEh 2022: 0x26a4c00; original functions/functions.c:18868. */
int generic_activated_ability_with_target_in_grave(
    int player, int card, event_t event, generic_activated_ability_flags_t flags, int cless,
    int black, int blue, int green, int red, int white, int gvc_costs, target_definition_t *td,
    const char *prompt, type_t type, test_definition_t *test, int gs_flags_for_grave_recycler)
{
  int count;
  unsigned int single_selection_data;
  unsigned int either_selection_data;
  unsigned int selection_data;
  card_instance_t *instance;
  int matching_count;
  int32_t selected_graveyard;
  int result;
  bool both_players;
  int count_or_player;
  test_definition_t test2;
  test_definition_t test2_1;

  count_or_player = player;
  if ((gs_flags_for_grave_recycler & 0x3808U) == 0)
  {
    backtrace_and_abort(
        "functions/functions.c:18876\n0 && \"unsupported \'gs_flags_for_grave_recycler\'\"");
  }
  if ((flags & (GAA_CAN_ONLY_TARGET_OPPONENT | GAA_CAN_TARGET)) != GAA_NONE)
  {
    backtrace_and_abort(
        "functions/functions.c:18879\n0 && \"unsupported \'generic_activated_ability_flags_t\'\"");
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((gs_flags_for_grave_recycler & 0x2000U) == 0)
    {
      result = 0;
      if (gs_flags_for_grave_recycler == 0x800)
      {
        count_or_player = 1 - player;
        both_players = count_or_player == 2;
      }
      else if (gs_flags_for_grave_recycler == 0x1000)
      {
        count_or_player = 2;
        both_players = true;
      }
      else
      {
        both_players = player == 2;
      }
      do
      {
        if (((count_or_player == result) || (both_players)) &&
            (matching_count = graveyard_has_shroud(result), matching_count == 0))
        {
          if (type == TYPE_NONE)
          {
            if (test == NULL)
            {
              ++result;
              if (result == 2)
                return 0;
              continue;
            }
          }
          else
          {
            count = count_graveyard_by_type(player, type);
            if ((int)(unsigned int)test->qty <= count)
              return granted_generic_activated_ability(
                  player, card, player, card, EVENT_CAN_ACTIVATE, flags, cless, black, blue, green,
                  red, white, gvc_costs, td, prompt);
          }
          matching_count = new_special_count_grave(result, test);
          if ((int)(unsigned int)test->qty <= matching_count)
            return granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE,
                                                     flags, cless, black, blue, green, red, white,
                                                     gvc_costs, td, prompt);
        }
        result = result + 1;
        if (result == 2)
        {
          return 0;
        }
      } while (true);
    }
    count_or_player = 0;
    result = 0;
    do
    {
      if (graveyard_has_shroud(result) == 0)
      {
        if (type != TYPE_NONE)
        {
          count = count_graveyard_by_type(player, type);
          count_or_player = count_or_player + count;
        }
        if (test != NULL)
        {
          matching_count = new_special_count_grave(result, test);
          count_or_player = count_or_player + matching_count;
        }
        if ((int)(unsigned int)test->qty <= count_or_player)
        {
          return granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE,
                                                   flags, cless, black, blue, green, red, white,
                                                   gvc_costs, td, prompt);
        }
      }
      result = result + 1;
      if (result == 2)
      {
        return 0;
      }
    } while (true);
  }
  if (event != EVENT_ACTIVATE)
  {
    return 0;
  }
  if (type != TYPE_NONE)
  {
    if (type == TYPE_ANY)
    {
      test = &test2;
      new_default_test_definition(test, 0xff, "Select target card.");
    }
    else
    {
      char buffer[100];
      char typetext[100];
      if (ai_is_speculating == 1)
      {
        typetext[0] = '\0';
      }
      else
      {
        type_text(typetext, 100, type, TYPETEXT_0);
      }
      scnprintf(buffer, 100, "Select target %s card.", typetext);
      test = &test2_1;
      new_default_test_definition(test, type, buffer);
    }
  }
  if (test->qty == 1)
  {
    result = player;
    if (gs_flags_for_grave_recycler == 0x800)
    {
      result = 1 - player;
    }
    single_selection_data =
        (uint32_t)(uint16_t)test->ai_selection_mode | ((uint32_t)(uint16_t)test->qty << 16);
    if ((gs_flags_for_grave_recycler == 0x1000) || (result == 2))
    {
      result = select_target_from_either_grave(player, card, SFG_0, single_selection_data & 0xff,
                                               single_selection_data >> 8 & 0xff, test, 0, 1);
    }
    else
    {
      result = select_target_from_grave_source(player, card, result, SFG_0,
                                               single_selection_data & 0xffff, test, 0);
    }
    if (result == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else if ((gs_flags_for_grave_recycler & 0x2000U) != 0)
  {
    instance = get_card_instance(player, card);
    either_selection_data =
        (uint32_t)(uint16_t)test->ai_selection_mode | ((uint32_t)(uint16_t)test->qty << 16);
    select_multiple_cards_from_either_graveyard(player, card, SFG_0, either_selection_data & 0xff,
                                                either_selection_data >> 8 & 0xff, test,
                                                (unsigned int)test->qty, instance->targets);
  }
  else
  {
    instance = get_card_instance(player, card);
    result = player;
    bool choose_graveyard = false;
    bool adjust_selection_mode = false;
    if (gs_flags_for_grave_recycler == 0x800)
    {
      selected_graveyard = 1 - player;
      choose_graveyard = selected_graveyard == 2;
      adjust_selection_mode = true;
    }
    else if (player == 2 || gs_flags_for_grave_recycler == 0x1000)
    {
      choose_graveyard = true;
      adjust_selection_mode = true;
    }
    if (choose_graveyard)
    {
      target_definition_t td2;
      default_target_definition(player, card, &td2, 0);
      td2.zone = TARGET_ZONE_PLAYERS;
      new_pick_target(&td2, "Select whose graveyard.", 0, 0x81);
      if (spell_fizzled == 1)
        return 0;
      selected_graveyard = instance->targets[0].player;
    }
    selection_data =
        (uint32_t)(uint16_t)test->ai_selection_mode | ((uint32_t)(uint16_t)test->qty << 16);
    if (adjust_selection_mode && selected_graveyard != player)
    {
      selection_data >>= 8;
      result = selected_graveyard;
    }
    select_multiple_cards_from_graveyard(player, result, -1, selection_data & 0xff, test,
                                         (unsigned int)test->qty, instance->targets + 1);
  }
  granted_generic_activated_ability(player, card, player, card, EVENT_ACTIVATE, flags, cless, black,
                                    blue, green, red, white, gvc_costs, td, prompt);
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 action and copy helpers. */

/* ManalinkEh 2022: 0x26a5370; original functions/functions.c:19061. */
void etb_ability(int player, int card, event_t event, resolve_trigger_t trigger_mode)
{
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
  {
    if (((((card == affected_card) && (player == affected_card_controller)) &&
          (reason_for_trigger_controller == player)) &&
         ((trigger_cause_controller == player && (trigger_cause == card)))) &&
        (!is_humiliated(player, card, AL_TRIG) && !check_for_cip_effects_removal(player, card)))
    {
      if (event == EVENT_TRIGGER)
      {
        if (trigger_mode == RESOLVE_TRIGGER_DUH)
        {
          trigger_mode = (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL;
        }
        event_result = event_result | trigger_mode;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        int amount = 1;
        if (is_what(player, card, 0x47) != 0)
        {
          amount = card_counts[player][CCOUNTS_YAROK_THE_DESECRATED] + 1;
        }
        if (is_what(player, card, 0x42) != 0)
        {
          amount = amount + (unsigned int)card_counts[player][CCOUNTS_PANHARMONICON];
        }
        if (is_what(player, card, 1) != 0)
        {
          amount = amount + (unsigned int)card_counts[player][CCOUNTS_ANCIENT_GREENWARDEN];
        }
        if (has_subtype(player, card, SUBTYPE_WIZARD) != 0)
        {
          unsigned char wizard_trigger_bonus = card_counts[player][CCOUNTS_NABAN_ITERATION];
          int prodigies = count_harmonic_prodigy(player, card);
          amount += (unsigned int)wizard_trigger_bonus + prodigies;
        }
        if (has_subtype(player, card, SUBTYPE_SHAMAN) &&
            !has_subtype(player, card, SUBTYPE_WIZARD))
        {
          amount += count_harmonic_prodigy(player, card);
        }
        for (; amount != 0; amount = amount + -1)
        {
          call_card_function(player, card, EVENT_ETB);
        }
      }
    }
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26a5a60; original functions/functions.c:19100. */
void etb_ability_global(int controller_of_trigger, event_t event, int trigger_mode, type_t type,
                        test_definition_t *test, int triggering_player, int triggering_card)
{
  if (trigger_condition != TRIGGER_COMES_INTO_PLAY)
  {
    return;
  }
  if (reason_for_trigger_controller != triggering_player)
  {
    return;
  }
  if (triggering_card != affected_card)
  {
    return;
  }
  if (triggering_player != affected_card_controller)
  {
    return;
  }
  if (is_humiliated(triggering_player, triggering_card, AL_TRIG) != 0)
  {
    return;
  }
  if ((controller_of_trigger != 2) && (trigger_cause_controller != controller_of_trigger))
  {
    return;
  }
  if (check_for_cip_effects_removal(trigger_cause_controller, trigger_cause))
  {
    return;
  }
  if ((type != TYPE_NONE) &&
      !is_what(trigger_cause_controller, trigger_cause, type))
  {
    return;
  }
  if (test != NULL)
  {
    if (new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, test) == 0)
    {
      return;
    }
    if (((test->not_me == 1) && (trigger_cause_controller == triggering_player)) &&
        (trigger_cause == triggering_card))
    {
      return;
    }
  }
  if (event == EVENT_TRIGGER)
  {
    if (trigger_mode == 3)
    {
      trigger_mode = (duh_mode(triggering_player) != 0) + 1;
    }
    event_result = event_result | trigger_mode;
    return;
  }
  if (event != EVENT_RESOLVE_TRIGGER)
  {
    return;
  }
  int amount = 1;
  card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
  instance->regen_status = instance->regen_status | 0x6000000;
  if (is_what(trigger_cause_controller, trigger_cause, 0x47) != 0)
  {
    amount = card_counts[triggering_player][CCOUNTS_YAROK_THE_DESECRATED] + 1;
  }
  if (is_what(trigger_cause_controller, trigger_cause, 0x42) != 0)
  {
    amount = amount + (unsigned int)card_counts[triggering_player][CCOUNTS_PANHARMONICON];
  }
  if (is_what(trigger_cause_controller, trigger_cause, 1) != 0)
  {
    amount = amount + (unsigned int)card_counts[triggering_player][CCOUNTS_ANCIENT_GREENWARDEN];
  }
  bool dispatch_even_if_zero = false;
  if (has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_WIZARD) == 0)
  {
    if (has_subtype(triggering_player, triggering_card, SUBTYPE_SHAMAN) &&
        !has_subtype(triggering_player, triggering_card, SUBTYPE_WIZARD))
      amount += count_harmonic_prodigy(triggering_player, triggering_card);
    else
      dispatch_even_if_zero = true;
  }
  else
  {
    unsigned char wizard_bonus = card_counts[triggering_player][CCOUNTS_NABAN_ITERATION];
    int prodigies = count_harmonic_prodigy(triggering_player, triggering_card);
    amount += (unsigned int)wizard_bonus + prodigies;
    if (has_subtype(triggering_player, triggering_card, SUBTYPE_SHAMAN) &&
        !has_subtype(triggering_player, triggering_card, SUBTYPE_WIZARD))
      amount += count_harmonic_prodigy(triggering_player, triggering_card);
  }
  if (!dispatch_even_if_zero && amount == 0)
    return;
  do
  {
    call_card_function(triggering_player, triggering_card, EVENT_ETB);
    --amount;
  } while (amount != 0);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 specific enters-play helpers. */

/* ManalinkEh 2022: 0x26a66a0; original functions/functions.c:19344. */
int generic_activated_ability_with_tapsubtype_as_cost(
    int player, int card, event_t event, generic_activated_ability_flags_t flags, int cless,
    int black, int blue, int green, int red, int white, int gvc_costs, target_definition_t *td,
    const char *prompt, target_definition_t *td_subtype, int req_tap)
{
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (target_available(player, card, td_subtype) < req_tap)
      return 0;
    return granted_generic_activated_ability(player, card, player, card, event, flags, cless,
                                                  black, blue, green, red, white, gvc_costs, td,
                                                  prompt);
  }
  if (event == EVENT_ACTIVATE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int to_tap[req_tap];
    for (int i = 0; i < req_tap; ++i)
    {
      char typetext[100];
      char clr[100];
      char buffer[500];
      const char *subtype_text = "";
      const char *subtype_space = "";
      const char *color_text = "";
      const char *color_space = "";
      if (ai_is_speculating == 1)
        typetext[0] = '\0';
      else
        type_text(typetext, 100, td_subtype->required_type, 0);
      if (td_subtype->required_subtype >= 0)
      {
        subtype_text = raw_get_subtype_text(td_subtype->required_subtype);
        subtype_space = " ";
      }
      if (td_subtype->required_color != 0)
      {
        color_text = get_sleighted_color_text(player, card, clr, td_subtype->required_color);
        color_space = " ";
      }
      scnprintf(buffer, 500, "Select an untapped %s%s%s%s%s to tap (%d of %d)", color_text,
                color_space, subtype_text, subtype_space, typetext, i + 1, req_tap);
      if (!new_pick_target(td_subtype, buffer, 9, 0x81))
        break;
      to_tap[i] = instance->targets[9].card;
      get_card_instance(player, to_tap[i])->state |= 0x100000;
    }
    for (int i = 0; i < instance->number_of_targets; ++i)
    {
      card_instance_t *target = get_card_instance(player, to_tap[i]);
      if (target->state & 0x100000)
        target->state &= ~0x100000U;
    }
    int good = spell_fizzled != 1;
    instance->number_of_targets = 0;
    if (good)
    {
      granted_generic_activated_ability(player, card, player, card, EVENT_ACTIVATE, flags,
                                             cless, black, blue, green, red, white, gvc_costs, td,
                                             prompt);
      if (spell_fizzled != 1)
        for (int i = 0; i < req_tap; ++i)
          tap_card(player, to_tap[i]);
      instance->number_of_targets = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26a8080; original functions/functions.c:19896. */
void disable_legend_rule(int player, int card, event_t event, int t_player)
{
  card_instance_t *instance;
  if (((event == EVENT_STATIC_EFFECTS) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        if ((t_player & 0xfffffffdU) == 0)
        {
          player_bits[0] = (player_bits_t)((uint32_t)player_bits[0] | PB_LEGEND_RULE_DISABLED);
        }
        if (t_player - 1U < 2)
        {
          player_bits[1] = (player_bits_t)((uint32_t)player_bits[1] | PB_LEGEND_RULE_DISABLED);
          return;
        }
      }
    }
  }
  return;
}

/* Recovered native October 2022 spell-cast trigger interfaces. */

/* ManalinkEh 2022: 0x2684f40; original functions/functions.c:19902. */
static void sp_ability_trigger_impl(int controller_of_trigger, event_t event, int trigger_mode,
                                    type_t type, test_definition_t *test, int triggering_player,
                                    int triggering_card, int allow_copies)
{
  if (trigger_condition != TRIGGER_SPELL_CAST)
  {
    return;
  }
  if (reason_for_trigger_controller != triggering_player)
  {
    return;
  }
  if (is_what(trigger_cause_controller, trigger_cause, 1) != 0)
  {
    return;
  }
  if (triggering_card != affected_card)
  {
    return;
  }
  if (triggering_player != affected_card_controller)
  {
    return;
  }
  if (is_humiliated(triggering_player, triggering_card, AL_TRIG))
  {
    return;
  }
  if ((controller_of_trigger != 2) && (controller_of_trigger != trigger_cause_controller))
  {
    return;
  }
  card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
  if (((((uint32_t)instance->ext->special_flags2) & 0x80000000U) != 0) && (allow_copies == 0))
  {
    return;
  }
  if ((type != TYPE_NONE) &&
      !is_what(trigger_cause_controller, trigger_cause, type))
  {
    return;
  }
  if (test != NULL)
  {
    if (new_make_test_in_play(trigger_cause_controller, trigger_cause, -1, test) == 0)
    {
      return;
    }
    if (((test->not_me == 1) && (triggering_player == trigger_cause_controller)) &&
        (triggering_card == trigger_cause))
    {
      return;
    }
  }
  if (event == EVENT_TRIGGER)
  {
    if (trigger_mode == 3)
    {
      trigger_mode = (duh_mode(triggering_player) != 0) + 1;
    }
    event_result = event_result | trigger_mode;
    return;
  }
  if (event != EVENT_RESOLVE_TRIGGER)
  {
    return;
  }
  instance = get_card_instance(trigger_cause_controller, trigger_cause);
  instance->regen_status = instance->regen_status | 0x6000000;
  int amount;
  if (has_subtype(triggering_player, triggering_card, SUBTYPE_WIZARD) == 0)
  {
    if (!has_subtype(triggering_player, triggering_card, SUBTYPE_SHAMAN))
    {
      call_card_function(triggering_player, triggering_card, EVENT_SPA);
      return;
    }
    amount = 1;
    if (!has_subtype(triggering_player, triggering_card, SUBTYPE_WIZARD))
    {
      int additional_triggers = count_harmonic_prodigy(triggering_player, triggering_card);
      amount += additional_triggers;
    }
  }
  else
  {
    amount = count_harmonic_prodigy(triggering_player, triggering_card);
    ++amount;
    if (has_subtype(triggering_player, triggering_card, SUBTYPE_SHAMAN) &&
        !has_subtype(triggering_player, triggering_card, SUBTYPE_WIZARD))
    {
      int additional_triggers = count_harmonic_prodigy(triggering_player, triggering_card);
      amount += additional_triggers;
    }
  }
  if (!amount)
    return;
  do
  {
    call_card_function(triggering_player, triggering_card, EVENT_SPA);
    --amount;
  } while (amount != 0);
}

/* ManalinkEh 2022: 0x26a8120; original functions/functions.c:19963. */
void sp_ability_trigger(int controller_of_trigger, event_t event, int trigger_mode,
                             type_t type, test_definition_t *test, int triggering_player,
                             int triggering_card)
{
  sp_ability_trigger_impl(controller_of_trigger, event, trigger_mode, type, test,
                               triggering_player, triggering_card, 0);
  return;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 life-gain and loss-prevention helpers. */

#define life_gain_context_push push_affected_card_stack
#define life_gain_context_pop pop_affected_card_stack
int life_to_lose_on_gain_life;
int suspend_cannot_lose_the_game;

/* ManalinkEh 2022: 0x268f730; original functions/functions.c:11288. */
static void gain_life_impl(int player, int amount, int from_lifelink)
{
  if (player == ANYBODY)
  {
    gain_life(current_turn, amount);
    gain_life(1 - current_turn, amount);
    return;
  }
  if (amount <= 0 || (player_bits[player] & PB_CANT_GAIN_LIFE) || !life_total_can_change(player))
    return;
  if (event_flags & EA_LICH)
  {
    life_gain_context_push();
    int saved_loss = life_to_lose_on_gain_life;
    int saved_gain = life_gained;
    life_to_lose_on_gain_life = 0;
    life_gained = amount;
    dispatch_xtrigger2(current_turn, XTRIGGER_REPLACE_GAIN_LIFE, "gaining life", 0, player,
        from_lifelink);
    amount = life_gained;
    life_gained = saved_gain;
    life_to_lose_on_gain_life = saved_loss;
    life_gain_context_pop();
    if (amount <= 0)
    {
      if (amount < 0)
        lose_life(player, -amount);
      if (life_to_lose_on_gain_life > 0)
        lose_life(player, life_to_lose_on_gain_life);
      return;
    }
  }
  life[player] += amount;
  increase_trap_condition(player, TRAP_LIFE_GAINED, amount);
  int saved_gain = life_gained;
  if (event_flags & EA_LICH)
  {
    life_gained = amount;
    dispatch_trigger2(current_turn, TRIGGER_GAIN_LIFE, "gained life", 0, player, -1);
  }
  life_gained = saved_gain;
  if (life_gain_draw_count[player])
    draw_cards(player, amount);
  if (life_to_lose_on_gain_life > 0)
    lose_life(player, life_to_lose_on_gain_life);
}

/* ManalinkEh 2022: 0x268f900; original functions/functions.c:11331. */
void gain_life(int player, int amount)
{
  gain_life_impl(player, amount, 0);
  return;
}

/* ManalinkEh 2022: 0x268f9f0; original functions/functions.c:11335. */
void gain_life_from_lifelink(int player, int amount)
{
  gain_life_impl(player, amount, 1);
  return;
}

/* ManalinkEh 2022: 0x268be60; original functions/functions.c:11341. */
int trigger_gain_life(int player, int card, event_t event, int who_gains,
    resolve_trigger_t trigger_mode)
{
  int gained = life_gained;
  if (trigger_condition != TRIGGER_GAIN_LIFE || reason_for_trigger_controller != player ||
      gained <= 0 || (who_gains != ANYBODY && trigger_cause_controller != who_gains) ||
      card != affected_card || player != affected_card_controller ||
      is_humiliated(player, card, AL_TRIG))
    return 0;
  if (event == EVENT_TRIGGER)
  {
    if (trigger_mode == RESOLVE_TRIGGER_DUH)
      trigger_mode = duh_mode(player) ? RESOLVE_TRIGGER_MANDATORY : RESOLVE_TRIGGER_OPTIONAL;
    event_result |= trigger_mode;
  }
  else if (event == EVENT_RESOLVE_TRIGGER)
    return gained;
  return 0;
}

/* ManalinkEh 2022: 0x268f0b0; original functions/functions.c:11369. */
int player_can_gain_life_as_cost(int player)
{
  int result;
  if (((uint32_t)player_bits[player] & PB_CANT_GAIN_LIFE) != 0)
  {
    return 0;
  }
  result = life_total_can_change(player);
  return (unsigned int)(result != 0);
}

/* ManalinkEh 2022: 0x2685c90; original functions/functions.c:14700. */
static int can_lose_the_game_impl(int player)
{
  for (int controller = 0; controller < 2; ++controller)
    for (int card = 0; card < active_cards_count[controller]; ++card)
    {
      card_instance_t *instance = in_play(controller, card);
      if (!instance)
        continue;
      if (is_what(controller, card, TYPE_PERMANENT))
      {
        int id = get_id(controller, card);
        int prevents_loss = 0;
        if (id == CARD_ID_PLATINUM_ANGEL || id == CARD_ID_LICHS_MASTERY)
          prevents_loss = player == controller;
        else if (id == CARD_ID_GIDEON_OF_THE_TRIALS_EMBLEM)
        {
          int has_gideon = check_battlefield_for_subtype(player, TARGET_TYPE_PLANESWALKER, SUBTYPE_GIDEON);
          prevents_loss = has_gideon && player == controller;
        }
        else if (id == CARD_ID_ABYSSAL_PERSECUTOR)
          prevents_loss = player != controller;
        if (prevents_loss && !is_humiliated(controller, card, AL_STATIC))
          return 0;
      }
      else if (is_what(controller, card, TYPE_EFFECT))
      {
        instance = get_card_instance(controller, card);
        if (instance->display_pic_csv_id == CARD_ID_ANGELS_GRACE && player == controller)
          return 0;
      }
    }
  return 1;
}

/* ManalinkEh 2022: 0x26927f0; original functions/functions.c:14700. */
int can_lose_the_game(int player)
{
  int result;

  if (suspend_cannot_lose_the_game == 0)
  {
    result = can_lose_the_game_impl(player);
    return result;
  }
  return 1;
}

/* ManalinkEh 2022: 0x2692810; original functions/functions.c:14734. */
void lose_the_game(int player)
{
  int result;

  result = check_battlefield_for_id(player, CARD_ID_EXQUISITE_ARCHANGEL);
  if (result == 0)
  {
    if (player != 2)
    {
      if ((suspend_cannot_lose_the_game == 0) &&
          (result = can_lose_the_game_impl(player), result == 0))
      {
        return;
      }
      if (ai_is_speculating != 1)
      {
        real_lose_the_game(player);
        return;
      }
      life[player] = -99;
      life[1 - player] = 0x14;
      return;
    }
    if (ai_is_speculating != 1)
    {
      real_lose_the_game(2);
      return;
    }
    life[1] = -99;
    life[0] = -99;
  }
  else
  {
    life[player] = -1;
  }
  return;
}

#undef life_gain_context_push
#undef life_gain_context_pop
#endif

#ifndef SHANDALAR
/* Recovered October 2022 cannot-lose protection helpers. */

/* ManalinkEh 2022: 0x269a810; original functions/functions.c:75. */
void ereboss_titan_effect(int player)
{
  for (int i = count_graveyard(player) - 1; i >= 0; --i)
    if (cards_data[graveyard_ptr_mutable[player][i]].id == CARD_ID_EREBOSS_TITAN &&
        hand_count[player] != 0)
    {
      int preview = add_card_to_hand(player, graveyard_ptr_mutable[player][i]);
      int choice = dialog_impl(player, preview, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_RANDOM,
                               "Return Erebos's Titan to your hand", 1, 10, "Decline", 1, 1,
                               0, 0, 0, 0);
      if (choice == 1)
      {
        discard(player, 0, player);
        from_grave_to_hand(player, i, 1);
      }
      obliterate_card(player, preview);
    }
}

/* ManalinkEh 2022: 0x2685a40; original functions/functions.c:562. */
int remove_until_upkeep_impl(int player, int card, int t_player, int t_card, int whose_upkeep,
                             int discard_cards_equal_to_toughness)
{
  unsigned int original_state;
  unsigned int original_card_data;
  bool has_second_half;
  card_instance_t *instance;
  int result;
  int card_00;
  int image_number;
  unsigned int csvid;
  uint16_t saved_image_number;
  int csvids[2];

  instance = in_play(t_player, t_card);
  if (instance != NULL)
  {
    result = is_token(t_player, t_card);
    card_00 = get_original_internal_card_id(t_player, t_card);
    instance = get_card_instance(t_player, t_card);
    csvids[0] = -1;
    original_state = instance->state;
    csvids[1] = -1;
    original_card_data =
        (unsigned int)cards_data[card_00].id | 0;
    csvid = original_card_data & 0xffff;
    image_number = get_card_image_number(csvid, t_player, t_card);
    saved_image_number = (uint16_t)image_number;
    if ((csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST) || (has_second_half = false, csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP))
    {
      unmeld(csvid, csvids, csvids + 1);
      card_00 = get_internal_card_id_from_csv_id(csvids[0]);
      has_second_half = true;
      saved_image_number = 0;
    }
    kill_card(t_player, t_card, KILL_EXILE);
    if (result == 0)
    {
      result = is_what(-1, card_00, 0x47);
      if (result != 0)
      {
        while (true)
        {
          result = create_legacy_effect(player, card, remove_until_upkeep_legacy);
          instance = get_card_instance(player, result);
          instance->targets[0].card = card_00;
          instance->targets[0].player = original_state >> 0xc & 1;
          instance->targets[1].player = whose_upkeep;
          instance->targets[1].card = -(unsigned int)(discard_cards_equal_to_toughness == 0) | 0x42;
          instance->display_pic_csv_id = (uint16_t)original_card_data;
          instance->display_pic_num = saved_image_number;
          if (!has_second_half)
            break;
          card_00 = get_internal_card_id_from_csv_id(csvids[1]);
          has_second_half = false;
        }
        return result;
      }
    }
  }
  return -1;
}

/* ManalinkEh 2022: 0x2685c10; original functions/functions.c:596. */
int remove_until_upkeep(int player, int card, int t_player, int t_card, int whose_upkeep)
{
  int result;

  result = remove_until_upkeep_impl(player, card, t_player, t_card, whose_upkeep, 0);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 native life-loss engine. */

/* ManalinkEh 2022: 0x2685c50; original functions/functions.c:600. */
int remove_until_upkeep_discard_cards_equal_to_toughness(int player, int card, int t_player,
                                                         int t_card, int whose_upkeep)
{
  int result;

  result = remove_until_upkeep_impl(player, card, t_player, t_card, whose_upkeep, 1);
  return result;
}

#endif

#ifndef SHANDALAR
/* Recovered native October 2022 leaves-play test interfaces. */

/* ManalinkEh 2022: 0x26a3790; original functions/functions.c:878. */
int test_leaves_play_mode(int triggering_player, int triggering_card, event_t event,
                               int controller, resolve_trigger_t mode, test_definition_t *test,
                               int can_be_suppressed, int can_be_duplicated)
{
  int result;
  int remaining_triggers;
  if ((((trigger_condition == TRIGGER_LEAVE_PLAY) && (triggering_card == affected_card)) &&
       (triggering_player == affected_card_controller)) &&
      (((result = new_make_test_in_play(trigger_cause_controller, trigger_cause, 0, test),
         result != 0 && (reason_for_trigger_controller == triggering_player)) &&
        ((trigger_cause_controller == controller || (controller == 2))))))
  {
    result = is_humiliated(triggering_player, triggering_card, AL_TRIG);
    if (((result == 0) || (can_be_suppressed == 0)) &&
        ((result = is_this_dying(trigger_cause_controller, trigger_cause),
          result == 0 ||
              ((result = check_for_death_trigger_removal(trigger_cause_controller, trigger_cause),
                can_be_suppressed == 0 || (result == 0))))))
    {
      if (event == EVENT_TRIGGER)
      {
        if (can_be_suppressed != 0)
        {
          event_result = event_result | mode;
          return 0;
        }
        if (((can_be_duplicated == 0) ||
             (result = is_what(trigger_cause_controller, trigger_cause, 2), result == 0)) ||
            (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))
        {
          result = 1;
        }
        else
        {
          result = card_counts[triggering_player][CCOUNTS_TEYSA_KARLOV] + 1;
        }
        do
        {
          if (is_what(triggering_player, triggering_card, 0x80) == 0)
          {
            call_card_function(triggering_player, triggering_card, EVENT_LEAVES_PLAY_ABILITY);
          }
          else
          {
            dispatch_event_arbitrary_to_one_card(triggering_player, triggering_card,
                                                 EVENT_LEAVES_PLAY_ABILITY,
                                                 trigger_cause_controller, trigger_cause, -1, -1);
          }
          result = result + -1;
        } while (result != 0);
      }
      else
      {
        if ((event == EVENT_RESOLVE_TRIGGER) && (can_be_suppressed != 0))
        {
          if ((can_be_duplicated == 0) ||
              ((result = is_what(trigger_cause_controller, trigger_cause, 2),
                result == 0 ||
                    (result = is_this_dying(trigger_cause_controller, trigger_cause), result == 0))))
          {
            remaining_triggers = 1;
            result = remaining_triggers;
          }
          else
          {
            remaining_triggers = card_counts[triggering_player][CCOUNTS_TEYSA_KARLOV] + 1;
            result = remaining_triggers;
          }
          do
          {
            if (is_what(triggering_player, triggering_card, 0x80) == 0)
            {
              call_card_function(triggering_player, triggering_card, EVENT_LEAVES_PLAY_ABILITY);
            }
            else
            {
              dispatch_event_arbitrary_to_one_card(triggering_player, triggering_card,
                                                   EVENT_LEAVES_PLAY_ABILITY,
                                                   trigger_cause_controller, trigger_cause, -1, -1);
            }
            remaining_triggers = remaining_triggers + -1;
          } while (remaining_triggers != 0);
          return result;
        }
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x26a3a80; original functions/functions.c:939. */
int test_leaves_play(int triggering_player, int triggering_card, event_t event, int controller,
                          test_definition_t *test, int can_be_suppressed,
                          int can_be_duplicated)
{
  int result;
  result = test_leaves_play_mode(triggering_player, triggering_card, event, controller,
                                      RESOLVE_TRIGGER_MANDATORY, test, can_be_suppressed,
                                      can_be_duplicated);
  return result;
}

/* ManalinkEh 2022: 0x2683c00; original functions/functions.c:2855. */
int upkeep_trigger_mode_impl(int player, int card, event_t event,
                                  resolve_trigger_t trigger_mode, int no_check)
{
  int result;

  if (trigger_condition != TRIGGER_UPKEEP)
  {
    return 0;
  }
  if (((card == affected_card) && (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    result = is_what(player, card, 0x80);
    if ((result == 0) && (result = is_humiliated(player, card, AL_TRIG), result != 0))
    {
      return 0;
    }
    result = 0;
    if (no_check == 0)
    {
      result = count_upkeeps(current_turn);
      if (result < 1)
      {
        return 0;
      }
      result = 1;
    }
    if (event == EVENT_TRIGGER)
    {
      if (trigger_mode == RESOLVE_TRIGGER_DUH)
      {
        result = duh_mode(player);
        trigger_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
      }
      event_result = event_result | trigger_mode;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (no_check != 0)
      {
        return 1;
      }
      return result;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a2d40; original functions/functions.c:5013. */
int effect_unearth(int player, int card, event_t event)
{
  int p;
  int c;
  card_instance_t *instance;
  int iid;
  card_instance_t *reanimated_instance;

  instance = get_card_instance(player, card);
  p = instance->targets[0].player;
  c = instance->targets[0].card;
  haste(p, c, event);
  if (instance->targets[1].player < 0)
  {
    reanimated_instance = in_play(p, c);
    if (reanimated_instance != NULL)
    {
      instance->targets[1].player = 0;
      set_special_flags(p, c, 0x4000);
    }
  }
  leaves_play(p, c, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    iid = get_original_internal_card_id(p, c);
    reanimated_instance = get_card_instance(p, c);
    add_card_to_rfg(reanimated_instance->state >> 0xc & 1, iid);
  }
  if (instance->targets[1].card != 0x42)
  {
    if (eot_trigger(player, card, event))
    {
      instance->targets[1].card = 0x42;
      reanimated_instance = get_card_instance(p, c);
      if ((reanimated_instance->state & 0x800000) == 0)
      {
        kill_card(p, c, KILL_EXILE);
        return 0;
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2683db0; original functions/functions.c:5039. */
static int haste_and_exile_your_upkeep(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
    haste(instance->damage_target_player, instance->damage_target_card, event);
  if (instance->damage_target_player == current_turn &&
      upkeep_trigger_mode_impl(player, card, event, RESOLVE_TRIGGER_MANDATORY, 0))
  {
    kill_card(instance->damage_target_player, instance->damage_target_card, KILL_EXILE);
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269ba70; original functions/functions.c:5286. */
/* Release callers use COUNTER_P1_P1 and null hooks; other hook paths are not present in this worker. */
static int new_reanimate_all_impl(int player, int card, int targ_player,
                                  test_definition_t *this_test, reanimate_mode_t action,
                                  counter_t counter_type, int (*effect_fn)(int, int, event_t),
                                  void (*before_etb)(int, int))
{
  int value;
  reanimate_mode_t living_death;
  int selected;
  card_instance_t *instance;
  int result;
  int *shift;
  int *next_shift;
  int can_cast;
  unsigned int turn;
  int *entry;
  int index;
  unsigned int player_00;
  int titan_player;
  int reanimated_count;
  int *aura_count;
  int source_or_limit;
  int aura_offset;
  int zone_offset;
  int caster_aura_count;
  unsigned int aura_pass;
  unsigned int count_or_limit;
  int trc[2];
  int ac[2];
  test_definition_t this_test2;
  int to_reanimate[2][500];
  int auras[2][500];

  source_or_limit = -1;
  if (card != -1)
  {
    source_or_limit = get_id(player, card);
  }
  value = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_GRAVE);
  living_death = action & REANIMATE_SPECIAL_LIVING_DEAD;
  trc[0] = 0;
  trc[1] = 0;
  ac[0] = 0;
  ac[1] = 0;
  reanimated_count = 0;
  titan_player = -1;
  index = 0;
  do
  {
    if ((targ_player == index) || (targ_player == 2))
    {
      if ((source_or_limit == 0x16a1) && (targ_player == index))
      {
        instance = get_card_instance(player, card);
        count_or_limit = instance->info_slot;
      }
      else
      {
        count_or_limit = -1;
      }
      entry = graveyard_ptr_mutable[index];
      selected = count_graveyard(index);
      selected = selected + -1;
      if ((int)count_or_limit < selected)
      {
        do
        {
          result = is_what(-1, entry[selected], 0x47);
          if ((result != 0) &&
              (result = new_make_test(index, entry[selected], -1, this_test), result != 0))
          {
            result = entry[selected];
            if (targ_player != 2)
            {
              result = is_what(-1, result, 2);
              if (result != 0)
              {
                titan_player = 1 - index;
              }
              result = entry[selected];
            }
            result = is_what(-1, result, 2);
            if ((result == 0) ||
                ((result = is_what(-1, entry[selected], 2), value == 0 && (result != 0))))
            {
              result = has_subtype_by_id(player, (unsigned int)cards_data[entry[selected]].id,
                                         SUBTYPE_AURA);
              if (result == 0)
              {
                result = trc[index];
                if (result < 500)
                {
                  to_reanimate[index][result] = entry[selected];
                  trc[index] = result + 1;
                  if (living_death == REANIMATE_DEFAULT)
                  {
                    remove_card_from_grave(index, selected);
                  }
                  else
                  {
                    rfg_card_from_grave(index, selected);
                  }
                  reanimated_count = reanimated_count + 1;
                }
              }
              else
              {
                result = add_card_to_hand(player, entry[selected]);
                instance = get_card_instance(player, result);
                instance->state = instance->state | 0x20;
                hand_count[player] = hand_count[player] + -1;
                can_cast = call_card_function_i(instance, player, result, EVENT_CAN_CAST);
                obliterate_card_and_recycle(player, result);
                if (can_cast != 0)
                {
                  result = ac[index];
                  if (result < 500)
                  {
                    auras[index][result] = entry[selected];
                    ac[index] = result + 1;
                    if (living_death == REANIMATE_DEFAULT)
                    {
                      remove_card_from_grave(index, selected);
                    }
                    else
                    {
                      rfg_card_from_grave(index, selected);
                    }
                    reanimated_count = reanimated_count + 1;
                  }
                }
              }
            }
          }
          selected = selected + -1;
        } while (selected != count_or_limit);
      }
    }
    index = index + 1;
  } while (index != 2);
  if (living_death != REANIMATE_DEFAULT)
  {
    default_test_definition(&this_test2, 2);
    new_manipulate_all(player, card, 2, &this_test2, ACT_KILL_SACRIFICE);
    aura_count = ac;
    aura_offset = 0;
    do
    {
      value = *aura_count;
      if (0 < value)
      {
        entry = auras[0] + aura_offset;
        source_or_limit = value;
        if (500 < value)
        {
          source_or_limit = 500;
        }
        index = 0;
        do
        {
          selected = add_card_to_hand(player, *entry);
          if (-1 < selected)
          {
            instance = get_card_instance(player, selected);
            turn = instance->state;
            hand_count[player] = hand_count[player] + -1;
            instance->state = turn | 0x20;
            result = call_card_function_i(instance, player, selected, EVENT_CAN_CAST);
            obliterate_card_and_recycle(player, selected);
            if ((result == 0) && (shift = entry, index < source_or_limit))
            {
              do
              {
                next_shift = shift + 1;
                *shift = shift[1];
                shift = next_shift;
              } while (next_shift != auras[0] + source_or_limit + aura_offset);
            }
          }
          index = index + 1;
          entry = entry + 1;
        } while (index != value);
      }
      aura_count = aura_count + 1;
      aura_offset = aura_offset + 500;
    } while (aura_offset != 1000);
  }
  if ((action & REANIMATE_ALL_UNDER_CASTERS_CONTROL) == REANIMATE_DEFAULT)
  {
    value = 0;
    zone_offset = 0;
    do
    {
      index = ac[value];
      if (0 < index)
      {
        entry = auras[0] + zone_offset;
        do
        {
          selected = *entry;
          bool return_card = living_death == REANIMATE_DEFAULT;
          if (!return_card && check_rfg(value, (unsigned int)cards_data[selected].id))
          {
            remove_card_from_rfg(value, (unsigned int)cards_data[selected].id);
            return_card = true;
          }
          if (return_card)
          {
            selected = add_card_to_hand_invisible(value, selected);
            *entry = selected;
          }
          entry = entry + 1;
        } while (entry != auras[0] + index + zone_offset);
      }
      index = trc[value];
      if (0 < index)
      {
        entry = to_reanimate[0] + zone_offset;
        do
        {
          selected = *entry;
          bool return_card = living_death == REANIMATE_DEFAULT;
          if (!return_card && check_rfg(value, (unsigned int)cards_data[selected].id))
          {
            remove_card_from_rfg(value, (unsigned int)cards_data[selected].id);
            return_card = true;
          }
          if (return_card)
          {
            selected = add_card_to_hand_invisible(value, selected);
            *entry = selected;
          }
          entry = entry + 1;
        } while (entry != to_reanimate[0] + index + zone_offset);
      }
      value = value + 1;
      zone_offset = zone_offset + 500;
    } while (value != 2);
    aura_pass = 0;
    turn = current_turn;
    do
    {
      player_00 = aura_pass ^ turn;
      value = ac[player_00];
      if (0 < value)
      {
        entry = auras[player_00];
        do
        {
          index = *entry;
          entry = entry + 1;
          reanimate_permanent_impl(player_00, -1, player_00, index,
                                   action | REANIMATE_SPECIAL_R_ALL, COUNTER_invalid,
                                   (CardFunction)0x0, (ReanimationBeforeEtb)0x0);
          turn = current_turn;
        } while (entry != auras[player_00] + value);
      }
      aura_pass = aura_pass + 1;
    } while (aura_pass != 2);
    count_or_limit = 0;
    do
    {
      turn = turn ^ count_or_limit;
      value = trc[turn];
      if (0 < value)
      {
        entry = to_reanimate[turn];
        do
        {
          index = *entry;
          entry = entry + 1;
          reanimate_permanent_impl(turn, -1, turn, index, action | REANIMATE_SPECIAL_R_ALL,
                                   COUNTER_invalid, (CardFunction)0x0, (ReanimationBeforeEtb)0x0);
        } while (to_reanimate[turn] + value != entry);
      }
      count_or_limit = count_or_limit + 1;
      turn = current_turn;
    } while (count_or_limit != 2);
  }
  else
  {
    caster_aura_count = ac[player];
    if (0 < caster_aura_count)
    {
      entry = auras[player];
      do
      {
        value = *entry;
        bool return_card = living_death == REANIMATE_DEFAULT;
        if (!return_card && check_rfg(player, (unsigned int)cards_data[value].id))
        {
          remove_card_from_rfg(player, (unsigned int)cards_data[value].id);
          return_card = true;
        }
        if (return_card)
        {
          value = add_card_to_hand(player, value);
          *entry = value;
        }
        entry = entry + 1;
      } while (auras[player] + caster_aura_count != entry);
    }
    count_or_limit = trc[player];
    if (0 < (int)count_or_limit)
    {
      entry = to_reanimate[player];
      do
      {
        value = *entry;
        bool return_card = living_death == REANIMATE_DEFAULT;
        if (!return_card && check_rfg(player, (unsigned int)cards_data[value].id))
        {
          remove_card_from_rfg(player, (unsigned int)cards_data[value].id);
          return_card = true;
        }
        if (return_card)
        {
          value = add_card_to_hand(player, value);
          *entry = value;
        }
        entry = entry + 1;
      } while (to_reanimate[player] + count_or_limit != entry);
    }
    index = 1 - player;
    value = ac[index];
    if (0 < value)
    {
      entry = auras[index];
      do
      {
        selected = *entry;
        bool return_card = living_death == REANIMATE_DEFAULT;
        if (!return_card && check_rfg(index, (unsigned int)cards_data[selected].id))
        {
          remove_card_from_rfg(player, (unsigned int)cards_data[selected].id);
          return_card = true;
        }
        if (return_card)
        {
          selected = add_card_to_hand_invisible(player, selected);
          instance = get_card_instance(player, selected);
          caster_aura_count = caster_aura_count + 1;
          instance->state = instance->state ^ 0x1000;
          auras[player][count_or_limit] = selected;
        }
        entry = entry + 1;
      } while (auras[index] + value != entry);
    }
    value = trc[index];
    if (0 < value)
    {
      entry = to_reanimate[index];
      do
      {
        selected = *entry;
        bool return_card = living_death == REANIMATE_DEFAULT;
        if (!return_card && check_rfg(index, (unsigned int)cards_data[selected].id))
        {
          remove_card_from_rfg(player, (unsigned int)cards_data[selected].id);
          return_card = true;
        }
        if (return_card)
        {
          selected = add_card_to_hand_invisible(player, selected);
          instance = get_card_instance(player, selected);
          instance->state = instance->state ^ 0x1000;
          to_reanimate[player][count_or_limit] = selected;
          count_or_limit = count_or_limit + 1;
        }
        entry = entry + 1;
      } while (to_reanimate[index] + value != entry);
    }
    ac[player] = caster_aura_count;
    trc[player] = count_or_limit;
    ac[index] = 0;
    trc[index] = 0;
    value = ac[player];
    if (0 < value)
    {
      entry = auras[player];
      do
      {
        index = *entry;
        entry = entry + 1;
        instance = get_card_instance(player, index);
        instance->state = instance->state & 0xffffffdf;
        reanimate_permanent_impl(player, card, player, index, action | REANIMATE_SPECIAL_R_ALL,
                                 COUNTER_invalid, (CardFunction)0x0, (ReanimationBeforeEtb)0x0);
      } while (entry != auras[player] + value);
    }
    value = trc[player];
    if (0 < value)
    {
      index = player * 2000;
      do
      {
        instance = get_card_instance(player, *(int *)((int)auras[0] + index));
        instance->state = instance->state & 0xffffffdf;
        entry = (int *)((int)to_reanimate[0] + index);
        index = index + 4;
        reanimate_permanent_impl(player, card, player, *entry, action | REANIMATE_SPECIAL_R_ALL,
                                 COUNTER_invalid, (CardFunction)0x0, (ReanimationBeforeEtb)0x0);
      } while ((value + player * 500) * 4 != index);
    }
  }
  if (-1 < titan_player)
  {
    ereboss_titan_effect(titan_player);
  }
  return reanimated_count;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 haste/delayed-removal family. */
static int haste_and_kill_at_eot(int player, int card, event_t event, int kill_mode);
int haste_and_remove_eot(int player, int card, event_t event);
int haste_and_sacrifice_eot(int player, int card, event_t event);
static int generic_haste_and_kill_at_eot_legacy(int player, int card, event_t event);
int haste_and_remove_at_beginning_of_next_end_step(int player, int card, int t_player,
                                                        int t_card, int kill_mode);
void haste_and_sacrifice_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                        int number);
void haste_and_remove_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                     int number);

/* ManalinkEh 2022: 0x269c810; original functions/functions.c:5532. */
int new_reanimate_all(int player, int card, int targ_player, test_definition_t *this_test,
                           reanimate_mode_t action)
{
  int result;
  result = new_reanimate_all_impl(player, card, targ_player, this_test, action, COUNTER_P1_P1, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x269c830; original functions/functions.c:5536. */
void reanimate_all(int player, int card, int targ_player, int type, int flag1, int subtype,
                        int flag2, int color, int flag3, int id, int flag4, int cc, int flag5,
                        reanimate_mode_t action)
{
  test_definition_t test;
  new_default_test_definition(&test, type, "");
  test.type_flag = (unsigned short)flag1;
  test.subtype = subtype;
  test.subtype_flag = (unsigned short)flag2;
  test.color = color;
  test.color_flag = (unsigned short)flag3;
  test.id = id;
  test.id_flag = (unsigned short)flag4;
  test.cmc = cc;
  test.cmc_flag = (unsigned short)flag5;
  new_reanimate_all_impl(player, card, targ_player, &test, action, COUNTER_P1_P1, NULL, NULL);
  return;
}

/* ManalinkEh 2022: 0x2683090; original functions/functions.c:5703. */
static int shapeshift_target_effect(int player, int card, event_t event)
{
  unsigned int mode;
  int card_00;
  CardFunction func_ptr;
  card_instance_t *instance;
  int result;
  card_instance_t *current_instance;
  unsigned int copies_token_characteristics;
  int target_player;

  instance = get_card_instance(player, card);
  mode = instance->targets[1].player;
  card_00 = instance->damage_target_card;
  target_player = (int)instance->damage_target_player;
  func_ptr = (CardFunction)instance->targets[2].player;
  copies_token_characteristics = mode & 0x400000;
  if (event == EVENT_CHANGE_TYPE && card_00 == affected_card &&
      (result = instance->targets[1].card, affected_card_controller == target_player) && result != -1 &&
      ((((unsigned int)land_can_be_played >> 16) & 2) == 0))
  {
    event_result = result;
    if (copies_token_characteristics)
      set_special_flags2(target_player, card_00, 0x400);
    else if (!(mode & 1))
      return 0;
  }
  if (copies_token_characteristics)
    token_characteristic_setting_effects(player, card, event, target_player, card_00);
  bool expired = (mode & 1) && eot_trigger(player, card, event);
  if (!expired)
  {
    if (event != EVENT_STATIC_EFFECTS || !(mode & 0x800000) ||
        count_counters(target_player, card_00, mode >> 0x18))
      return 0;
  }
  if (((mode & 2) != 0) && (func_ptr != (CardFunction)0xffffffff))
  {
    create_targetted_legacy_activate((int)instance->damage_source_player,
                                     instance->damage_source_card, func_ptr, target_player, card_00);
  }
  if ((copies_token_characteristics != 0) &&
      (current_instance = get_card_instance(player, card),
       (current_instance->ext->special_flags2 & SF2_TEMPORARY_COPY_OF_TOKEN) == 0))
  {
    set_special_infos(target_player, card_00, instance->targets[10].player);
  }
  if (((mode & 8) != 0) && (instance = get_card_instance(player, card),
                            (instance->ext->special_flags3 & 0x1000000U) != SF3_0))
  {
    remove_special_flags(target_player, card_00, 0x1000000);
  }
  kill_card(player, card, KILL_EXILE);
  instance = get_card_instance(target_player, card_00);
  instance->regen_status = instance->regen_status | 0x1000000;
  get_abilities(target_player, card_00, EVENT_CHANGE_TYPE, -1);
  instance = in_play(target_player, card_00);
  if (instance == NULL)
  {
    return 0;
  }
  result = get_id(target_player, card_00);
  verify_legend_rule(target_player, card_00, result);
  return 0;
}

/* ManalinkEh 2022: 0x26867e0; original functions/functions.c:5769. */
int real_shapeshift_target(int src_player, int src_card, int t_player, int t_card,
                                int turn_into_player, int turn_into_card, int mode)
{
  int p;
  int card;
  card_instance_t *instance;
  int result;
  card_instance_t *candidate_instance;
  int special_infos;
  unsigned int argument1;
  int card_00;
  target_t previous_effect;

  previous_effect = find_repeat_legacy_effect_alt(src_player, src_card, shapeshift_target_effect,
                                        t_player, t_card, -1, src_player);
  if (previous_effect.card != -1)
  {
    kill_card(src_player, previous_effect.card, KILL_EXILE);
  }
  card = create_targetted_legacy_effect(src_player, src_card, shapeshift_target_effect,
                                        t_player, t_card);
  instance = get_card_instance(src_player, card);
  instance->targets[0].player = t_player;
  instance->targets[0].card = t_card;
  instance->targets[10].player = 0;
  instance->targets[1].player = mode & 0xffbfffff;
  if (turn_into_player == -1)
  {
    instance->targets[1].card = turn_into_card;
  }
  else
  {
    candidate_instance = get_card_instance(turn_into_player, turn_into_card);
    argument1 = candidate_instance->internal_card_id;
    if (argument1 == 0xffffffff)
    {
      argument1 = (unsigned int)candidate_instance->backup_internal_card_id;
    }
    if (((mode & 4U) != 0) && ((cards_data[argument1].type & 0x40) == 0))
    {
      argument1 = create_a_card_type(argument1);
      cards_data[argument1].type = cards_data[argument1].type | 0x40;
    }
    instance->targets[1].card = argument1;
    if (copy_token_characteristics_for_clone(src_player, card, turn_into_player,
                                                      turn_into_card) != 0)
    {
      instance->targets[1].player = instance->targets[1].player | 0x400000;
      result = get_special_infos(t_player, t_card);
      instance->targets[10].player = result;
      candidate_instance = get_card_instance(t_player, t_card);
      if ((candidate_instance->ext->special_flags2 & SF2_TEMPORARY_COPY_OF_TOKEN) != 0)
      {
        p = 0;
        do
        {
          card_00 = 0;
          if (0 < active_cards_count[p])
          {
            do
            {
              candidate_instance = in_play(p, card_00);
              if (((((candidate_instance != NULL) && (candidate_instance->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
                    (candidate_instance->info_slot_fn == shapeshift_target_effect)) &&
                   ((candidate_instance->damage_target_player == p &&
                     (candidate_instance->damage_target_card == card_00)))) &&
                  (((candidate_instance->targets[1].player & 0x400000) != 0 &&
                    (candidate_instance = get_card_instance(p, card_00),
                     (candidate_instance->ext->special_flags2 & SF2_TEMPORARY_COPY_OF_TOKEN) == 0))))
              {
                set_special_flags2(p, card_00, 0x400);
                special_infos = get_special_infos(p, card_00);
                instance->targets[10].player = special_infos;
              }
              card_00 = card_00 + 1;
            } while (card_00 < active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
      }
      set_special_infos(t_player, t_card, get_special_infos(src_player, card));
    }
  }
  instance = get_card_instance(t_player, t_card);
  instance->regen_status = instance->regen_status | 0x1f000000;
  get_abilities(t_player, t_card, EVENT_CHANGE_TYPE, -1);
  get_abilities(t_player, t_card, EVENT_SET_COLOR, -1);
  get_abilities(t_player, t_card, EVENT_ABILITIES, -1);
  result = is_what(t_player, t_card, 2);
  if ((result != 0) &&
      (get_abilities(t_player, t_card, EVENT_TOUGHNESS, -1), instance->internal_card_id != -1))
  {
    get_abilities(t_player, t_card, EVENT_POWER, -1);
    return card;
  }
  return card;
}

/* ManalinkEh 2022: 0x267f930; original functions/functions.c:6234. */
void replace_all_instances_of_one_color_word_with_another(int player, int card, int t_player,
                                                          int t_card)
{
  int i;
  unsigned int color_value;
  card_instance_t *instance;
  int32_t colors_in_text;
  int blue_option;
  int result;
  int white_option;
  uint32_t green_priority;
  int red_option;
  uint32_t red_priority;
  uint32_t white_priority;
  unsigned int choice;
  int green_option;
  int options[6];

  if (player == 1)
  {
    options[1] = 0;
    options[2] = 0;
    options[3] = 0;
    options[4] = 0;
    options[5] = 0;
    result = get_id(t_player, t_card);
    colors_in_text = cards_ptr[result]->sleight_color;
    instance = get_card_instance(t_player, t_card);
    if ((instance->ext->special_flags3 & SF3_HARDCODED_SLEIGHT_WORDS_REPLACED) == SF3_0)
    {
      set_special_flags3(t_player, t_card, 0x2000);
      colors_in_text = get_colors_to_sleight_from_text(get_id(t_player, t_card));
      result = get_id(t_player, t_card);
      cards_ptr[result]->sleight_color = colors_in_text;
    }
    for (i = 1; i < 6; ++i)
      if (((colors_in_text >> i) & 1U) && get_card_instance(t_player, t_card)->color_id[i] == 0)
        options[i] = 1;
    for (int index = 1; index < 6; ++index)
    {
      instance = get_card_instance(t_player, t_card);
      if (instance->color_id[index] != 0)
      {
        instance = get_card_instance(t_player, t_card);
        options[instance->color_id[index]] = 1;
      }
    }
    green_option = options[3];
    result = options[1];
    red_option = options[4];
    blue_option = options[2];
    white_option = options[5];
  }
  else
  {
    green_option = 1;
    result = 1;
    red_option = 1;
    blue_option = 1;
    white_option = 1;
  }
  color_value = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
                            "From Black...", result, 1, "From Blue...", blue_option, 1,
                            "From Green...", green_option, 1, "From Red...", red_option, 1,
                            "From White...", white_option, 1, 0, 0, 0, 0);
  options[1] = 0;
  options[2] = 0;
  options[3] = 0;
  options[4] = 0;
  options[5] = 0;
  options[color_value] = 1;
  if ((color_value == 4) || (color_value == 1))
  {
    white_priority = 10;
  }
  else
  {
    white_priority = 1;
  }
  if ((color_value == 2) || (color_value == 5))
  {
    red_priority = 10;
  }
  else
  {
    red_priority = 1;
  }
  if (color_value - 1 < 2)
  {
    green_priority = 10;
    result = 1;
  }
  else
  {
    green_priority = 1;
    result = ((color_value - 3 < 2) ? 10U : 1U);
  }
  choice = (unsigned int)(options[3] == 0);
  result = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                       DLG_OMIT_ILLEGAL, "...to Black", (unsigned int)(options[1] == 0),
                       (((color_value - 3 & 0xfffffffd) == 0) ? 10U : 1U), "...to Blue",
                       (unsigned int)(options[2] == 0), result, "...to Green", choice,
                       green_priority, "...to Red", (unsigned int)(options[4] == 0), red_priority,
                       "...to White", (unsigned int)(options[5] == 0), white_priority, 0, 0, 0, 0);
  instance = get_card_instance(t_player, t_card);
  choice = (unsigned int)result;
  if (instance->color_id[color_value] == '\0')
  {
    instance = get_card_instance(t_player, t_card);
    instance->color_id[color_value] = (uint8_t)choice;
  }
  for (i = 1; i < 6; ++i)
  {
    instance = get_card_instance(t_player, t_card);
    if (instance->color_id[i] == color_value)
    {
      instance = get_card_instance(t_player, t_card);
      instance->color_id[i] = (uint8_t)choice;
    }
  }
}

/* ManalinkEh 2022: 0x267fdc0; original functions/functions.c:6289. */
void replace_all_instances_of_one_basic_land_type_word_with_another(int player, int card,
                                                                    int t_player, int t_card)
{
  int i;
  unsigned int color_value;
  card_instance_t *instance;
  unsigned int colors_in_rules;
  int blue_option;
  int result;
  int white_option;
  uint32_t green_priority;
  int red_option;
  uint32_t red_priority;
  uint32_t white_priority;
  unsigned int choice;
  int green_option;
  int options[6];

  if (player == 1)
  {
    options[1] = 0;
    options[2] = 0;
    options[3] = 0;
    options[4] = 0;
    options[5] = 0;
    result = get_id(t_player, t_card);
    color_value = (unsigned int)cards_ptr[result]->hack_colors;
    instance = get_card_instance(t_player, t_card);
    if ((instance->ext->special_flags3 & SF3_HARDCODED_HACK_WORDS_REPLACED) == SF3_0)
    {
      set_special_flags3(t_player, t_card, 0x4000);
      result = get_id(t_player, t_card);
      color_value = get_colors_to_hack_from_type_text(result);
      colors_in_rules = get_colors_to_hack_from_rules_text(result);
      color_value = color_value | colors_in_rules;
      result = get_id(t_player, t_card);
      cards_ptr[result]->hack_colors = (uint8_t)color_value;
    }
    for (i = 1; i < 6; ++i)
      if (((color_value >> i) & 1U) && get_card_instance(t_player, t_card)->hack_mode[i] == 0)
        options[i] = 1;
    for (int index = 1; index < 6; ++index)
    {
      instance = get_card_instance(t_player, t_card);
      if (instance->hack_mode[index] != 0)
      {
        instance = get_card_instance(t_player, t_card);
        options[instance->hack_mode[index]] = 1;
      }
    }
    green_option = options[3];
    result = options[1];
    red_option = options[4];
    blue_option = options[2];
    white_option = options[5];
  }
  else
  {
    green_option = 1;
    result = 1;
    red_option = 1;
    blue_option = 1;
    white_option = 1;
  }
  color_value = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
                            "From Swamp...", result, 1, "From Island...", blue_option, 1,
                            "From Forest...", green_option, 1, "From Mountain...", red_option, 1,
                            "From Plain...", white_option, 1, 0, 0, 0, 0);
  options[1] = 0;
  options[2] = 0;
  options[3] = 0;
  options[4] = 0;
  options[5] = 0;
  options[color_value] = 1;
  if ((color_value == 4) || (color_value == 1))
  {
    white_priority = 10;
  }
  else
  {
    white_priority = 1;
  }
  if ((color_value == 2) || (color_value == 5))
  {
    red_priority = 10;
  }
  else
  {
    red_priority = 1;
  }
  if (color_value - 1 < 2)
  {
    green_priority = 10;
    result = 1;
  }
  else
  {
    green_priority = 1;
    result = ((color_value - 3 < 2) ? 10U : 1U);
  }
  choice = (unsigned int)(options[3] == 0);
  result =
      dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, DLG_OMIT_ILLEGAL,
                  "...to Swamp", (unsigned int)(options[1] == 0),
                  (((color_value - 3 & 0xfffffffd) == 0) ? 10U : 1U), "...to Island",
                  (unsigned int)(options[2] == 0), result, "...to Forest", choice, green_priority,
                  "...to Mountain", (unsigned int)(options[4] == 0), red_priority, "...to Plain",
                  (unsigned int)(options[5] == 0), white_priority, 0, 0, 0, 0);
  instance = get_card_instance(t_player, t_card);
  choice = (unsigned int)result;
  if (instance->hack_mode[color_value] == '\0')
  {
    instance = get_card_instance(t_player, t_card);
    instance->hack_mode[color_value] = (uint8_t)choice;
  }
  for (i = 1; i < 6; ++i)
  {
    instance = get_card_instance(t_player, t_card);
    if (instance->hack_mode[i] == color_value)
    {
      instance = get_card_instance(t_player, t_card);
      instance->hack_mode[i] = (uint8_t)choice;
    }
  }
}

/* ManalinkEh 2022: 0x269ad40; original functions/functions.c:6972. */
int reveal_card_until_you_reveal_test(int player, int t_player, test_definition_t *test,
                                           int flags)
{
  int *deck = deck_ptr[player];
  if (flags & 0x4000)
    deck = deck_ptr[t_player];
  unsigned int qty = test->qty;
  int wanted, limit;
  if (flags & 0x1000)
  {
    wanted = count_deck(t_player);
    if ((int)(qty & 255) < wanted)
      wanted = qty & 255;
    limit = (test->qty >> 8) & 255;
  }
  else
  {
    wanted = count_deck(t_player);
    if ((int)qty < wanted)
      wanted = qty;
    limit = count_deck(t_player);
  }
  int target_deck_size = count_deck(t_player);
  int remaining = 0;
  uint32_t packed_result = (uint32_t)deck[0];
  if (deck[0] != -1)
  {
    if (target_deck_size < limit)
      limit = target_deck_size;
    int deck_copy[count_deck(player)];
    int recipient = (flags & 0x100) ? t_player : player;
    int removed = 0;
    int found = 0;
    int last_added = -1;
    packed_result = UINT32_MAX;
    int scanned = 0;
    while (deck[scanned] != -1)
    {
      deck_copy[scanned] = deck[scanned];
      if (new_make_test(t_player, deck[scanned], -1, test))
      {
        int iid = deck[scanned];
        if (flags & 0xe0)
        {
          if ((flags & 0xc0) && is_what(-1, iid, 2) && grafdiggers_cages(player, TYPE_CREATURE, 1))
            goto after_move;
          ++removed;
          last_added = add_card_to_hand(recipient, iid);
          remove_card_from_deck(t_player, scanned);
          if (!(flags & 0x100) && player != t_player)
          {
            if (player == 0 && t_player == 1)
              add_state(player, last_added, 0x1000);
            else if (player == 1 && t_player == 0)
              remove_state(player, last_added, 0x1000);
          }
          if (flags & 0x80)
            add_state(recipient, last_added, 0x10);
          if (flags & 0xc0)
          {
            set_special_flags(recipient, last_added, 0x10000);
            put_into_play(recipient, last_added);
          }
        }
      after_move:
        if (flags & 0x2000)
        {
          play_card_in_deck_for_free(player, recipient, scanned);
          ++removed;
        }
        if (flags & 0x800)
        {
          add_card_to_rfg(player, iid);
          rfg_card_in_deck(player, scanned);
          ++removed;
        }
        if (flags & 0x200)
        {
          int report = last_added;
          if (last_added < 0)
          {
            report = iid;
            if (((uint32_t)last_added & packed_result) == UINT32_MAX)
              packed_result = 0;
          }
          if (found == 0)
            packed_result |= (uint32_t)(int32_t)(int16_t)report;
          else if (found == 1)
            packed_result |= (uint32_t)report << 16;
        }
        ++found;
      }
      ++scanned;
      if (found == wanted || scanned == limit)
        break;
    }
    show_deck(current_turn, deck_copy, scanned, "Cards revealed.", 0, gs_done_008b40e0);
    show_deck(current_turn ^ 1, deck_copy, scanned, "Cards revealed.", 0, gs_done_008b40e0);
    remaining = scanned - removed;
    if (remaining > 0)
    {
      if (flags & 1)
        mill(t_player, remaining);
      if (flags & 2)
        put_top_x_on_bottom(player, t_player, remaining);
      if (flags & 4)
        put_top_x_on_bottom_in_random_order(t_player, remaining);
      if (flags & 8)
        for (int i = 0; i < remaining; ++i)
          rfg_top_card_of_deck(t_player);
      if (flags & 0x10)
        shuffle(t_player);
    }
  }
  if (flags & 0x400)
    packed_result = (uint32_t)remaining;
  return (int32_t)packed_result;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 reanimation family. */
static int new_reanimate_all_impl(int player, int card, int targ_player,
                                  test_definition_t *this_test, reanimate_mode_t action,
                                  counter_t counter_type, int (*effect_fn)(int, int, event_t),
                                  void (*before_etb)(int, int));
int new_reanimate_all(int player, int card, int targ_player, test_definition_t *this_test,
                           reanimate_mode_t action);
static int reanimate_permanent_impl(int player, int card, int target_graveyard, int selected,
                                  reanimate_mode_t action, counter_t counter_type,
                                  CardFunction effect_fn, ReanimationBeforeEtb before_etb);
int reanimate_permanent(int player, int card, int target_graveyard, int selected,
                             reanimate_mode_t action);
int reanimate_permanent_with_effect(int player, int card, int target_graveyard, int selected,
                                         reanimate_mode_t action, CardFunction effect_fn);
int reanimate_permanent_with_counter_and_effect(int player, int card, int target_graveyard,
                                                     int selected, reanimate_mode_t action,
                                                     counter_t counter_type, CardFunction effect_fn);
int reanimate_permanent_with_function(int player, int card, int target_graveyard, int selected,
                                           reanimate_mode_t action,
                                           ReanimationBeforeEtb before_etb);
void reanimate_all(int player, int card, int targ_player, int type, int flag1, int subtype,
                        int flag2, int color, int flag3, int id, int flag4, int cc, int flag5,
                        reanimate_mode_t action);
static int haste_and_exile_your_upkeep(int player, int card, event_t event);
int effect_unearth(int player, int card, event_t event);
void ereboss_titan_effect(int player);
int upkeep_trigger_mode_impl(int player, int card, event_t event,
                                  resolve_trigger_t trigger_mode, int no_check);

/* ManalinkEh 2022: 0x2699e10; original functions/functions.c:7761. */
static int discard_card_by_int_id(int player, int int_crd_id, int effect,
                                  test_definition_t *this_test, int source_of_effect)
{
  int result = -1;
  for (int card = active_cards_count[player] - 1; card >= 0; --card)
  {
    if (!in_hand(player, card))
      continue;
    card_instance_t *instance = get_card_instance(player, card);
    bool matches;
    if (int_crd_id >= 0)
      matches = instance->internal_card_id == int_crd_id;
    else if (int_crd_id == -1)
      matches = new_make_test_in_play(player, card, -1, this_test) != 0;
    else
      matches = false;
    if (!matches)
      continue;
    result = get_id(player, card);
    if (effect & 0x4c)
      result = card;
    if (effect & 1)
      discard_card_impl(player, card, source_of_effect, 0);
    if (effect & 2)
      rfg_card_in_hand(player, card);
    if (effect & 0x20)
      put_on_top_of_deck(player, card);
    if (effect & 0x100)
      put_on_bottom_of_deck(player, card);
    if (int_crd_id >= 0)
      return result;
  }
  return result;
}

/* ManalinkEh 2022: 0x2699f60; original functions/functions.c:7811. */
int new_effect_coercion(ec_definition_t *this_definition, test_definition_t *this_test)
{
  int initial_hand[50];
  int revealed_hand[50];
  int revealed_count = 0;
  test_definition_t test_default;
  if (!this_test)
  {
    this_test = &test_default;
    default_test_definition(this_test, 0xff);
  }
  if (this_definition->cards_to_reveal > 0)
  {
    int target_player = this_definition->target_player;
    if (hand_count[target_player] < this_definition->cards_to_reveal)
      this_definition->cards_to_reveal = hand_count[target_player];
    int initial_count = 0;
    for (int c = 0; c < active_cards_count[target_player]; ++c)
    {
      if (in_hand(target_player, c))
      {
        int iid = get_original_internal_card_id(this_definition->target_player, c);
        initial_hand[initial_count++] = iid;
      }
      target_player = this_definition->target_player;
    }
    test_definition_t reveal_test;
    new_default_test_definition(&reveal_test, 0xff, "Select a card to reveal.");
    int *end = initial_hand + initial_count - 1;
    while (revealed_count < this_definition->cards_to_reveal)
    {
      int selected = select_card_from_zone(this_definition->target_player,
                                           this_definition->target_player, initial_hand,
                                           initial_count - revealed_count, 1, 3, -1, &reveal_test);
      if (selected == -1)
        continue;
      revealed_hand[revealed_count++] = initial_hand[selected];
      if (selected < initial_count - revealed_count)
        for (int *entry = initial_hand + selected; entry != end; ++entry)
          *entry = entry[1];
      --end;
    }
    if (this_definition->effect & 0x80)
    {
      if (this_definition->cards_to_reveal > 0)
      {
        show_deck(this_definition->who_choses, revealed_hand, revealed_count,
                  "Target player revealed these.", 0, gs_done_008b40e0);
        if (revealed_count < 1)
          return -1;
        for (int *entry = revealed_hand; entry != revealed_hand + revealed_count; ++entry)
          if (new_make_test(this_definition->target_player, *entry, -1, this_test))
            discard_card_by_int_id(this_definition->target_player, *entry, 1, NULL,
                                   this_definition->who_choses);
        return -1;
      }
      reveal_target_player_hand(this_definition->target_player);
      discard_card_by_int_id(this_definition->target_player, -1, this_definition->effect, this_test,
                             this_definition->who_choses);
      return -1;
    }
  }
  else if (this_definition->effect & 0x80)
  {
    reveal_target_player_hand(this_definition->target_player);
    discard_card_by_int_id(this_definition->target_player, -1, this_definition->effect, this_test,
                           this_definition->who_choses);
    return -1;
  }
  int result = -1;
  for (int processed = 0; processed < this_definition->qty; ++processed)
  {
    int iid;
    if (this_definition->cards_to_reveal > 0)
    {
      if (revealed_count < 1)
        return result;
      int must_match = 0;
      for (int index = 0; index < revealed_count; ++index)
        if (new_make_test(this_definition->target_player, revealed_hand[index], -1, this_test))
        {
          must_match = 1;
          break;
        }
      int selected = select_card_from_zone(
          this_definition->who_choses, this_definition->target_player, revealed_hand,
          revealed_count, must_match, this_definition->ai_selection_mode, -1, this_test);
      if (selected == -1)
        return result;
      iid = revealed_hand[selected];
      if (selected < revealed_count - 1)
        for (int *entry = revealed_hand + selected; entry != revealed_hand + revealed_count - 1;
             ++entry)
          *entry = entry[1];
      --revealed_count;
    }
    else
    {
      int target_player = this_definition->target_player;
      if (hand_count[target_player] < 1)
        return result;
      this_test->zone = 0x100;
      int matches = check_battlefield_for_special_card(-1, -1, target_player, 0, this_test);
      int selected = new_select_a_card(this_definition->who_choses, this_definition->target_player,
                                       4, (unsigned int)(matches != 0),
                                       this_definition->ai_selection_mode, -1, this_test);
      if (selected == -1)
        return result;
      iid = get_original_internal_card_id(this_definition->target_player, selected);
    }
    if (iid != -1)
      result = discard_card_by_int_id(this_definition->target_player, iid, this_definition->effect,
                                      NULL, this_definition->who_choses);
  }
  return result;
}

/* ManalinkEh 2022: 0x26a3060; original functions/functions.c:11961. */
/* The surviving release worker covers for_life == 1. */
static void cannot_lose_the_game_impl(int player, int card, event_t event, int who_cant_lose,
                                       int for_life, int for_poison, int for_draw_on_empty_deck)
{
  card_instance_t *instance = in_play(player, card);
  if (!instance)
    return;
  int restore;
  if (instance->targets[5].player < 0 || instance->targets[5].card < 0)
  {
    instance->targets[5].player = 0;
    instance->targets[5].card = 0;
    instance->targets[6].player = who_cant_lose;
    leaves_play(player, card, event, 0, 0);
    restore = event == EVENT_LEAVES_PLAY_ABILITY;
  }
  else
  {
    int previous_player = instance->targets[6].player;
    leaves_play(player, card, event, 0, 0);
    restore = event == EVENT_LEAVES_PLAY_ABILITY || who_cant_lose != previous_player;
  }
  // Leaving play permanently disables further suppression by this instance.
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    instance->targets[6].card = 17;
  if (restore)
  {
    int previous_player = instance->targets[6].player;
    if (for_poison)
    {
      raw_set_poison(previous_player, (int8_t)instance->targets[5].card +
          POISON_COUNTERS(previous_player));
      previous_player = instance->targets[6].player;
    }
    int previous_life = life[previous_player];
    instance->targets[5].card = 0;
    instance->targets[6].player = who_cant_lose;
    life[previous_player] = previous_life - instance->targets[5].player;
    instance->targets[5].player = 0;
  }
  if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW && *deck_ptr[who_cant_lose] == -1 &&
      who_cant_lose == reason_for_trigger_controller && card == affected_card &&
      affected_card_controller == player && for_draw_on_empty_deck && !suppress_draw)
  {
    if (event == EVENT_TRIGGER)
      event_result = RESOLVE_TRIGGER_MANDATORY;
    else if (event == EVENT_RESOLVE_TRIGGER)
      suppress_draw = 1;
  }
  int current_life = life[who_cant_lose];
  if (current_life < 1)
  {
    if (instance->targets[6].card != 17)
    {
      int hidden_loss = instance->targets[5].player;
      life[who_cant_lose] = 1;
      instance->targets[5].player = hidden_loss + (1 - current_life);
    }
  }
  else if (current_life > 1 && instance->targets[5].player > 0)
  {
    int restored = current_life - 1;
    if (instance->targets[5].player < restored)
      restored = instance->targets[5].player;
    life[who_cant_lose] = current_life - restored;
    instance->targets[5].player -= restored;
  }
  if (for_poison)
  {
    uint8_t poison = POISON_COUNTERS(who_cant_lose);
    if (poison < 9 && instance->targets[5].card > 0)
    {
      int restored = 9 - poison;
      if (instance->targets[5].card < restored)
        restored = instance->targets[5].card;
      raw_set_poison(who_cant_lose, poison + (int8_t)restored);
      instance->targets[5].card -= restored;
    }
    else if (poison >= 10 && instance->targets[6].card != 17)
    {
      raw_set_poison(who_cant_lose, 9);
      instance->targets[5].card += poison - 9;
    }
  }
}

/* ManalinkEh 2022: 0x26a33c0; original functions/functions.c:12052. */
void cannot_lose_the_game(int player, int card, event_t event, int who_cant_lose)
{
  cannot_lose_the_game_impl(player, card, event, who_cant_lose, 1, 1, 1);
  return;
}

/* ManalinkEh 2022: 0x26a3390; original functions/functions.c:12056. */
void cannot_lose_the_game_for_having_less_than_0_life(int player, int card, event_t event,
                                                           int who_cant_lose)
{
  cannot_lose_the_game_impl(player, card, event, who_cant_lose, 1, 0, 0);
  return;
}

/* ManalinkEh 2022: 0x268f1b0; original functions/functions.c:12300. */
int lose_life(int player, int amount)
{
  if (amount <= 0)
    return 0;
  int lost = 0;
  if (player == ANYBODY)
  {
    lost = lose_life(current_turn, amount);
    player = 1 - current_turn;
  }
  if ((unsigned int)player > 1 || !life_total_can_change(player))
    return lost;
  // Snapshot simultaneous triggers before any of them can remove a source.
  int marked[2][151] = {{0}};
  int own_triggers = 0;
  for (int card = 0; card < active_cards_count[player]; ++card)
  {
    card_instance_t *instance = in_play(player, card);
    if (!instance)
      continue;
    int id = cards_data[instance->internal_card_id].id;
    if ((id == CARD_ID_TRANSCENDENCE || id == CARD_ID_OATH_OF_LIM_DUL ||
         id == CARD_ID_LICHS_TOMB || id == CARD_ID_GONTIS_MACHINATIONS ||
         id == CARD_ID_LICHS_MASTERY || id == CARD_ID_VILIS_BROKER_OF_BLOOD) &&
        !is_humiliated(player, card, AL_TRIG))
    {
      own_triggers = 1;
      marked[player][card] = id;
    }
  }
  int opponent = 1 - player;
  int opponent_triggers = 0;
  for (int card = 0; card < active_cards_count[opponent]; ++card)
  {
    card_instance_t *instance = in_play(opponent, card);
    if (!instance)
      continue;
    int id = cards_data[instance->internal_card_id].id;
    if ((id == CARD_ID_MINDCRANK || id == CARD_ID_EXQUISITE_BLOOD) &&
        !is_humiliated(opponent, card, AL_TRIG))
    {
      opponent_triggers = 1;
      marked[opponent][card] = id;
    }
  }
  increase_trap_condition(player, TRAP_LIFE_LOST, amount);
  life[player] -= amount;
  play_sound_effect(WAV_LIFELOSS);
  if (own_triggers)
    for (int card = 0; card < active_cards_count[player]; ++card)
      switch (marked[player][card])
      {
      case CARD_ID_TRANSCENDENCE:
        gain_life_impl(player, amount * 2, 0);
        break;
      case CARD_ID_OATH_OF_LIM_DUL:
        oath_of_lim_dul_lifeloss_trigger(player, card, amount);
        break;
      case CARD_ID_LICHS_TOMB:
        impose_sacrifice(player, card, player, amount, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
        break;
      case CARD_ID_GONTIS_MACHINATIONS:
      {
        card_instance_t *instance = get_card_instance(player, card);
        if (instance->targets[1].player != 0x42)
        {
          add_energy(player, 1);
          instance = get_card_instance(player, card);
          instance->targets[1].player = 0x42;
        }
        break;
      }
      case CARD_ID_LICHS_MASTERY:
        lichs_mastery_lifeloss_trigger(player, card, amount);
        break;
      case CARD_ID_VILIS_BROKER_OF_BLOOD:
        draw_cards(player, amount);
        break;
      }
  lost += amount;
  if (opponent_triggers)
    for (int card = 0; card < active_cards_count[opponent]; ++card)
      switch (marked[opponent][card])
      {
      case CARD_ID_MINDCRANK:
        mill(player, amount);
        break;
      case CARD_ID_EXQUISITE_BLOOD:
        gain_life_impl(opponent, amount, 0);
        break;
      }
  return lost;
}

/* ManalinkEh 2022: 0x268ecf0; original functions/functions.c:13906.
 * Built-in chooser UI is stubbed; see functions/choose_a_card.cpp. */
int new_card_from_list(int player, int mode, test_definition_t *test)
{
  if (ai_is_speculating == 1)
    return -1;

  type_t types = test->type;
  if (types == TYPE_ANY)
    types |= TARGET_TYPE_PLANESWALKER;

  switch (test->type_flag)
  {
  case DOESNT_MATCH:
    types = ~types;
    break;
  case F1_NO_CREATURE:
    types &= ~TYPE_CREATURE;
    break;
  case F1_NO_PWALKER:
    types &= ~TARGET_TYPE_PLANESWALKER;
    break;
  case F1_NONARTIFACT_CREATURE:
    types = (types & ~TYPE_ARTIFACT) | TYPE_CREATURE;
    break;
  }

  color_test_t colors = test->color_flag == 2 ? -2 : -1;
  int selected;
  if (mode & 1)
    selected = choose_a_card_test_ct(test->message, test, colors, types);
  else
    selected = choose_a_card_test_ct_allow_cancel(test->message, test, colors, types);

  if (selected < 0)
    return -1;
  if ((mode & 6) == 4)
    return get_internal_card_id_from_csv_id(selected);
  return selected;
}

/* ManalinkEh 2022: 0x2691e10; original functions/functions.c:14372. */
int reveal_cards_from_your_hand(int player, int card, test_definition_t *this_test)
{
  int i;
  card_instance_t *instance;
  int selected;
  int *shift;
  int *next_shift;
  int index;
  int active_count;
  int revealed[100];
  int your_hand[100];

  active_count = active_cards_count[1];
  if (player != 1)
  {
    if (do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0) != 0)
    {
      i = 0;
      if (active_cards_count[player] < 1)
      {
        return 0;
      }
      index = 0;
      do
      {
        instance = in_hand(player, index);
        if (instance != NULL)
        {
          instance = get_card_instance(player, index);
          your_hand[i] = instance->internal_card_id;
          i = i + 1;
        }
        index = index + 1;
      } while (index < active_cards_count[player]);
      if (i < 1)
      {
        return 0;
      }
      index = 0;
      do
      {
        selected =
            show_deck(0, your_hand, i - index, "Choose a card to reveal", 0, gs_done_008b40e0);
        if ((selected == -1) || (new_make_test(player, your_hand[selected], -1, this_test) == 0))
          break;
        revealed[index] = your_hand[selected];
        if (selected < i)
        {
          shift = your_hand + selected;
          do
          {
            next_shift = shift + 1;
            *shift = shift[1];
            shift = next_shift;
          } while (your_hand + i != next_shift);
        }
        index = index + 1;
      } while (i != index);
      if (index > 0)
        show_deck(0, revealed, index, "Player revealed these cards", 0, gs_done_008b40e0);
      return index;
    }
    active_count = active_cards_count[player];
  }
  if (active_count < 1)
  {
    return 0;
  }
  i = 0;
  index = 0;
  do
  {
    if ((instance = in_hand(player, i),
         instance == NULL ||
             (selected = new_make_test_in_play(player, i, -1, this_test), selected == 0)))
    {
      i = i + 1;
      continue;
    }
    instance = get_card_instance(player, i);
    i = i + 1;
    revealed[index] = instance->internal_card_id;
    index = index + 1;
  } while (i < active_cards_count[player]);
  if (0 < index)
  {
    show_deck(0, revealed, index, "Player revealed these cards", 0, gs_done_008b40e0);
    return index;
  }
  return index;
}

/* ManalinkEh 2022: 0x2692630; original functions/functions.c:14669. */
void mana_into_string(int cless, int black, int blue, int green, int red, int white,
                           test_definition_t *this_test)
{
  int length;
  int written;
  int symbol_index;
  char buffer[100];

  length = 0;
  if (cless != 0)
  {
    if ((cless == -1) || (cless == 0x28))
    {
      length = scnprintf(buffer, 100, "X");
    }
    else
    {
      length = scnprintf(buffer, 100, "%d", cless);
    }
  }
  if (0 < black)
  {
    symbol_index = 0;
    do
    {
      symbol_index = symbol_index + 1;
      written = scnprintf(buffer + length, 100 - length, "B");
      length = length + written;
    } while (black != symbol_index);
  }
  if (0 < blue)
  {
    symbol_index = 0;
    do
    {
      symbol_index = symbol_index + 1;
      written = scnprintf(buffer + length, 100 - length, "U");
      length = length + written;
    } while (blue != symbol_index);
  }
  if (0 < green)
  {
    symbol_index = 0;
    do
    {
      symbol_index = symbol_index + 1;
      written = scnprintf(buffer + length, 100 - length, "G");
      length = length + written;
    } while (green != symbol_index);
  }
  if (0 < red)
  {
    symbol_index = 0;
    do
    {
      symbol_index = symbol_index + 1;
      written = scnprintf(buffer + length, 100 - length, "R");
      length = length + written;
    } while (red != symbol_index);
  }
  if (0 < white)
  {
    symbol_index = 0;
    do
    {
      symbol_index = symbol_index + 1;
      written = scnprintf(buffer + length, 100 - length, "W");
      length = length + written;
    } while (white != symbol_index);
  }
  strcpy(this_test->message, buffer);
  return;
}

/* ManalinkEh 2022: 0x2691090; original functions/functions.c:16759. */
static const char *has_mana_to_cast_target(int who_chooses, int player, int card,
                                         int targeting_player, int targeting_card)
{
  event_t event;
  int result;
  char *error;
  result = get_id(player, card);
  event = hack_current_event;
  result = get_internal_card_id_from_csv_id(result);
  result = has_mana_to_cast_iid(who_chooses, event, result);
  error = NULL;
  if (result == 0)
  {
    error = "not enough mana";
  }
  return error;
}

/* ManalinkEh 2022: 0x2696b60; original functions/functions.c:16763. */
int can_play_cards_as_though_they_had_flash(int player, int card, event_t event,
                                            target_definition_t *td, const char *prompt,
                                            int literal)
{
  card_instance_t *instance;
  int result;
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    return 0;
  }
  bool automatic =
      ai_is_speculating == 1 || (player == 1 && (((unsigned char)trace_mode & 2) == 0));
  if ((automatic || event == EVENT_CAN_ACTIVATE) && td->extra == -1)
  {
    td->special |= 0x8000;
    td->extra = (intptr_t)has_mana_to_cast_target;
  }
  hack_current_event = event;
  if (event == EVENT_CAN_ACTIVATE)
    return can_target(td);
  if (event == EVENT_ACTIVATE)
  {
    if (literal == 0)
    {
      load_text(NULL, prompt);
      prompt = text_lines[0];
    }
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    result = pick_next_target_noload(td, prompt);
    if ((result != 0) && (result = charge_mana_from_id_with_cost_mod(
                              player, instance->targets[0].card, EVENT_ACTIVATE, -1, 0),
                          result != 0))
    {
      result = instance->targets[0].card;
      instance->number_of_targets = '\0';
      play_card_in_hand_for_free(instance->targets[0].player, result);
      cant_be_responded_to = 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2677c50; original functions/functions.c:18225. */
static int fx_permanents_destroyed_this_way_accumulate(int player, int card, event_t event)
{
  int iid;
  unsigned char count_or_flags;
  int32_t last_target_card;
  int card_00;
  card_instance_t *instance;
  card_instance_t *pending_instance;
  card_instance_t *current_instance;
  unsigned int target_index;
  int slot;
  int source_player_or_index;

  instance = get_card_instance(player, card);
  bool process_dies = trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY;
  bool resolve_now = false;
  if (trigger_condition == TRIGGER_LEAVE_PLAY)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (reason_for_trigger_controller == player)) &&
        (event == EVENT_TRIGGER))
    {
      current_instance = get_card_instance(player, card);
      card_00 = trigger_cause;
      source_player_or_index = trigger_cause_controller;
      if ((current_instance->eot_toughness1 & 2) == 0)
      {
        count_or_flags = instance->number_of_targets;
        target_index = (unsigned int)count_or_flags;
        slot = target_index - 1;
        if (slot < 0)
        {
          return 0;
        }
        while ((trigger_cause != instance->targets[slot].card ||
                (trigger_cause_controller != instance->targets[slot].player)))
        {
          slot = slot + -1;
          if (slot == -1)
          {
            return 0;
          }
        }
        if (slot < 0)
        {
          return 0;
        }
        last_target_card = instance->targets[target_index - 1].card;
        instance->targets[slot].player = instance->targets[target_index - 1].player;
        instance->targets[slot].card = last_target_card;
        instance->number_of_targets = count_or_flags - 1;
        current_instance = get_card_instance(source_player_or_index, card_00);
        if (current_instance->kill_code == '\x02')
        {
          instance->eot_toughness = (int32_t)((uint32_t)instance->eot_toughness + 0x10000U);
          if (instance->targets[10].card != 0xdeadcad)
          {
            instance->targets[10].card = 0xdeadcad;
            instance->targets[10].player = 0;
          }
          if (trigger_cause_controller == 0)
          {
            instance->targets[10].player =
                (int32_t)(((uint32_t)instance->targets[10].player & 0xffff0000U) |
                          (((uint32_t)instance->targets[10].player + 1U) & 0xffffU));
          }
          else if (trigger_cause_controller == 1)
          {
            instance->targets[10].player =
                (int32_t)((uint32_t)instance->targets[10].player + 0x10000U);
          }
        }
      }
      if (trigger_condition != TRIGGER_GRAVEYARD_FROM_PLAY)
      {
        return 0;
      }
      process_dies = true;
    }
  }
  if (process_dies)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      current_instance = get_card_instance(player, card);
      count_or_flags = current_instance->eot_toughness1;
      target_index = count_or_flags & 2;
      if (((count_or_flags & 2) == 0) &&
          ((card != trigger_cause || (player != trigger_cause_controller))))
      {
        current_instance = get_card_instance(player, card);
        bool pending_destroy = false;
        if (current_instance->number_of_targets != '\0')
        {
          do
          {
            pending_instance = in_play(current_instance->targets[target_index].player,
                                       current_instance->targets[target_index].card);
            if (((pending_instance != NULL) && ((pending_instance->token_status & 0x80) != 0)) &&
                (pending_instance->kill_code == '\x02'))
            {
              pending_destroy = true;
              break;
            }
            target_index = target_index + 1;
          } while ((int)target_index < (int)(unsigned int)current_instance->number_of_targets);
        }
        if (!pending_destroy)
        {
          if (event == EVENT_TRIGGER)
          {
            event_result |= 2;
            return 0;
          }
          resolve_now = event == EVENT_RESOLVE_TRIGGER;
        }
      }
    }
  }
  if (!resolve_now && event != EVENT_PHASE_CHANGED && event != EVENT_CLEANUP)
  {
    return 0;
  }
  instance->eot_toughness1 = instance->eot_toughness1 | 2;
  if (((instance->eot_toughness1 & 1) != 0) && (0 < active_cards_count[player]))
  {
    source_player_or_index = 0;
    do
    {
      current_instance = in_play(player, source_player_or_index);
      if (((current_instance != NULL) &&
           (current_instance->damage_source_card == instance->damage_source_card)) &&
          ((current_instance->damage_source_player == instance->damage_source_player &&
            (((current_instance->internal_card_id == instance->internal_card_id &&
               (current_instance->info_slot == instance->info_slot)) &&
              (card != source_player_or_index))))))
      {
        current_instance->eot_toughness2 =
            current_instance->eot_toughness2 + (char)((uint32_t)instance->eot_toughness >> 16);
        current_instance->eot_toughness3 =
            current_instance->eot_toughness3 + instance->eot_toughness3;
        instance->eot_toughness = (int32_t)((uint32_t)instance->eot_toughness & 0xffffU);
        if (instance->targets[10].card == 0xdeadcad)
        {
          if (current_instance->targets[10].card != 0xdeadcad)
          {
            current_instance->targets[10].card = 0xdeadcad;
            current_instance->targets[10].player = 0;
          }
          if (10 < current_instance->number_of_targets)
          {
            current_instance->number_of_targets = current_instance->number_of_targets - 1;
          }
          current_instance->targets[10].player =
              (int32_t)((((uint32_t)current_instance->targets[10].player +
                          (uint32_t)instance->targets[10].player) &
                         0xffffU) |
                        ((((uint32_t)current_instance->targets[10].player & 0xffff0000U) +
                          ((uint32_t)instance->targets[10].player & 0xffff0000U)) &
                         0xffff0000U));
          instance->targets[10].player = 0;
        }
        kill_card(player, card, KILL_EXILE);
        return 0;
      }
      source_player_or_index = source_player_or_index + 1;
    } while (source_player_or_index < active_cards_count[player]);
  }
  if (instance->targets[10].card != 0xdeadcad)
  {
    instance->targets[10].card = 0xdeadcad;
    instance->targets[10].player = 0;
  }
  iid = get_internal_card_id_from_csv_id((unsigned int)instance->display_pic_csv_id);
  call_card_fn(cards_data[iid].code_pointer, instance, player, card, EVENT_RESOLVE_GENERAL_EFFECT);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269f880; original functions/functions.c:18336. */
card_instance_t *legacy_permanents_destroyed_this_way_accumulate(
    int src_player, int src_card, card_instance_t *previous_effect, int tgt_player, int tgt_card)
{
  unsigned char target_index;
  int result;
  if (previous_effect == NULL)
  {
    result = create_legacy_effect(src_player, src_card,
                                  fx_permanents_destroyed_this_way_accumulate);
    previous_effect = get_card_instance(src_player, result);
  }
  else if (previous_effect->number_of_targets == '\x12')
  {
    previous_effect->eot_toughness1 = previous_effect->eot_toughness1 | 1;
    result = create_legacy_effect(src_player, src_card,
                                  fx_permanents_destroyed_this_way_accumulate);
    previous_effect = get_card_instance(src_player, result);
    previous_effect->eot_toughness1 = previous_effect->eot_toughness1 | 1;
  }
  if (-1 < tgt_player)
  {
    target_index = previous_effect->number_of_targets;
    previous_effect->targets[target_index].player = tgt_player;
    previous_effect->number_of_targets = target_index + 1;
    previous_effect->targets[target_index].card = tgt_card;
  }
  return previous_effect;
}

/* ManalinkEh 2022: 0x269fe40; original functions/functions.c:18459. */
int exile_top_x_card_you_may_play_them(int player, int card, int howmany, test_definition_t *test,
                                       int t_player, mpcfe_mode_t flags)
{
  int amount = count_deck(t_player);
  if (howmany < amount)
    amount = howmany;
  if (amount != 0)
  {
    char buffer[100];
    int pos = scnprintf(buffer, sizeof(buffer), "Cards exiled");
    if (card != -1)
    {
      int id = get_id(player, card);
      scnprintf(buffer + pos, sizeof(buffer) - pos, " by %s", cards_ptr[id]->name);
    }
    int *deck = deck_ptr[t_player];
    show_deck(player, deck, amount, buffer, 0, gs_done_008b40e0);
    int legacy = -1;
    for (int count = 0; *deck != -1 && count < amount; ++count)
    {
      int iid = *deck_ptr[t_player];
      if (iid != -1)
      {
        rfg_top_card_of_deck(t_player);
        if (!test || new_make_test(t_player, iid, -1, test))
          legacy = create_may_play_card_from_exile_effect(
              player, card, t_player, (unsigned int)cards_data[iid].id, flags);
      }
    }
    return legacy;
  }
  return -1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678910; original functions/functions.c:18504. */
static int haste_and_kill_at_eot(int player, int card, event_t event, int kill_mode)
{
  int target_card;
  int result;
  card_instance_t *instance;
  card_instance_t *attached_instance;
  int target_player;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    target_card = instance->damage_target_card;
    haste(target_player, target_card, event);
    if ((((trigger_condition == TRIGGER_EOT) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (player == reason_for_trigger_controller))
    {
      result = instance->targets[1].player;
      if (result < 1)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          attached_instance = get_card_instance(target_player, target_card);
          if (((attached_instance->state & STATE_OUBLIETTED) == 0) && (kill_mode != 0))
          {
            if (kill_mode != 4)
            {
              if (4 < kill_mode)
              {
                if (kill_mode != 5)
                {
                  return 0;
                }
                bounce_permanent(target_player, target_card);
                return 0;
              }
              if (2 < kill_mode)
              {
                if (kill_mode != 3)
                {
                  return 0;
                }
                if (target_player != instance->targets[0].player)
                {
                  return 0;
                }
                kill_card(target_player, target_card, KILL_SACRIFICE);
                return 0;
              }
              if (kill_mode < 1)
              {
                return 0;
              }
            }
            kill_card(target_player, target_card, kill_mode);
          }
          else
          {
            kill_card(player, card, KILL_EXILE);
          }
        }
      }
      else
      {
        instance->targets[1].player = result + -1;
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678ac0; original functions/functions.c:18552. */
static int generic_haste_and_kill_at_eot_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;
  instance = get_card_instance(player, card);
  result = haste_and_kill_at_eot(player, card, event, instance->targets[1].card);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678a80; original functions/functions.c:18556. */
int haste_and_remove_eot(int player, int card, event_t event)
{
  int result;
  result = haste_and_kill_at_eot(player, card, event, 4);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695730; original functions/functions.c:5108. */
static int reanimate_permanent_impl(int player, int card, int target_graveyard, int selected,
                                    reanimate_mode_t action, counter_t counter_type,
                                    CardFunction effect_fn, ReanimationBeforeEtb before_etb)
{
  int *graveyard;
  int player_00;
  reanimate_mode_t mode;
  int t_card;
  int result;
  card_instance_t *instance;
  reanimate_mode_t return_to_hand;
  int effect_source_card;

  if (selected == -1)
  {
    return -1;
  }
  mode = (action ^ REANIMATEXTRA_RETURN_TO_HAND2) >> 0x1f & action;
  graveyard = graveyard_ptr_mutable[target_graveyard];
  player_00 = player;
  if ((mode & REANIMATE_UNDER_OWNER_CONTROL) != REANIMATE_DEFAULT)
  {
    player_00 = target_graveyard;
  }
  t_card = selected;
  if ((mode & REANIMATE_SPECIAL_R_ALL) == REANIMATE_DEFAULT)
  {
    t_card = add_card_to_hand(player_00, graveyard[selected]);
    remove_card_from_grave(target_graveyard, selected);
  }
  if ((((mode & REANIMATE_RETURN_TO_HAND) == REANIMATE_DEFAULT) &&
       (result = is_what(-1, graveyard[selected], 2), result != 0)) &&
      (result = grafdiggers_cages(player, TYPE_CREATURE, TUTOR_FROM_GRAVE), result != 0))
  {
    return -1;
  }
  return_to_hand = mode & REANIMATE_RETURN_TO_HAND;
  effect_source_card = card;
  if (player >= 0 && card >= 0)
  {
    if ((mode & REANIMATE_ATTACH_AS_AURA) != REANIMATE_DEFAULT)
    {
      set_special_flags(player, card, 0x20000000);
      attach_aura_to_target(player, card, EVENT_RESOLVE_SPELL, player_00, t_card);
      instance = get_card_instance(player, card);
      instance->targets[0].player = player_00;
      instance->damage_target_player = (int8_t)player_00;
      instance->targets[0].card = t_card;
      instance->damage_target_card = t_card;
    }
    if ((mode & REANIMATE_ATTACH_AURA_TO_REANIMATOR) != REANIMATE_DEFAULT)
    {
      set_special_flags(player_00, t_card, 0x20000000);
      attach_aura_to_target(player_00, t_card, EVENT_RESOLVE_SPELL, player, card);
      instance = get_card_instance(player_00, t_card);
      instance->damage_target_player = (int8_t)player;
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->damage_target_card = card;
    }
  }
  else if (card < 0)
    effect_source_card = t_card;
  if (counter_type != COUNTER_invalid)
  {
    hack_silent_counters = hack_silent_counters + 1;
    add_counters(player, card, player_00, t_card, counter_type, 1);
    hack_silent_counters = hack_silent_counters + -1;
  }
  if ((mode & REANIMATE_TAP) != REANIMATE_DEFAULT)
  {
    instance = get_card_instance(player_00, t_card);
    instance->state = instance->state | 0x10;
  }
  if ((mode & REANIMATE_PLUS1_PLUS1_COUNTER) != REANIMATE_DEFAULT)
  {
    add_counters(player, card, player_00, t_card, COUNTER_P1_P1, 1);
  }
  if ((mode & REANIMATE_MINUS1_MINUS1_COUNTER) != REANIMATE_DEFAULT)
  {
    add_counters(player, card, player_00, t_card, COUNTER_M1_M1, 1);
  }
  if (effect_fn != (CardFunction)0x0)
  {
    create_targetted_legacy_effect(player, effect_source_card, effect_fn, player_00, t_card);
  }
  if (action == REANIMATE_UNEARTH)
  {
    convert_to_token(player_00, t_card);
    set_special_flags(player_00, t_card, 0x4000);
    set_special_flags2(player_00, t_card, 0x10000000);
  }
  else
  {
    if ((mode & REANIMATE_ADD_BLACK_ZOMBIE) != REANIMATE_DEFAULT)
    {
      change_color(player, effect_source_card, player_00, t_card, COLOR_TEST_BLACK,
                   CHANGE_COLOR_NO_SOUND | CHANGE_COLOR_ADD);
      add_a_subtype_permanently(player, effect_source_card, player_00, t_card, SUBTYPE_ZOMBIE);
    }
    if ((mode & REANIMATE_HASTE_AND_EXILE_AT_EOT) != REANIMATE_DEFAULT)
    {
      create_targetted_legacy_effect(player, effect_source_card, haste_and_remove_eot, player_00,
                                     t_card);
    }
  }
  if ((mode & REANIMATE_HASTE_AND_EXILE_AT_YOUR_UPKEEP) != REANIMATE_DEFAULT)
  {
    create_targetted_legacy_effect(player, effect_source_card, haste_and_exile_your_upkeep,
                                   player_00, t_card);
  }
  if ((mode & REANIMATE_HASTE_UNTIL_EOT) != REANIMATE_DEFAULT)
  {
    pump_ability_until_eot(player, effect_source_card, player_00, t_card, 0, 0, 0, 4);
  }
  if (player_00 != target_graveyard)
  {
    if (player == 0)
    {
      add_state(player_00, t_card, 0x1000);
    }
    else
    {
      remove_state(player_00, t_card, 0x1000);
    }
  }
  if (before_etb != (ReanimationBeforeEtb)0x0)
  {
    (*before_etb)(player, card, player_00, t_card);
  }
  if ((((mode & (REANIMATE_RETURN_TO_HAND | REANIMATE_ATTACH_AS_AURA |
                 REANIMATE_HASTE_AND_EXILE_AT_EOT | REANIMATE_ADD_BLACK_ZOMBIE | REANIMATE_UNEARTH |
                 REANIMATE_NO_CONTROL_LEGACY)) == REANIMATE_DEFAULT &&
        effect_fn == (CardFunction)0x0) &&
       (effect_source_card != -1)) &&
      ((target_graveyard != player ||
        (instance = get_card_instance(player_00, t_card), player != (instance->state >> 0xc & 1)))))
  {
    create_targetted_legacy_effect(player, effect_source_card, empty, player_00, t_card);
  }
  if (return_to_hand == REANIMATE_DEFAULT)
  {
    set_special_flags3(player_00, t_card, 1);
    if ((mode & REANIMATE_RETURN_TO_PLAY_TRANSFORMED) == REANIMATE_DEFAULT)
    {
      put_into_play(player_00, t_card);
    }
    else
    {
      put_into_play_transformed(player_00, t_card);
    }
    if (action == REANIMATE_UNEARTH)
    {
      instance = in_play(player_00, t_card);
      if (instance != NULL)
      {
        create_targetted_legacy_effect(player, effect_source_card, effect_unearth, player_00,
                                       t_card);
        return t_card;
      }
      return t_card;
    }
    return t_card;
  }
  return t_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269b760; original functions/functions.c:5249. */
int reanimate_permanent(int player, int card, int target_graveyard, int selected,
                             reanimate_mode_t action)
{
  unsigned int card_00;
  int result;

  card_00 = reanimate_permanent_impl(player, card, target_graveyard, selected, action,
                                          COUNTER_invalid, (CardFunction)0x0,
                                          (ReanimationBeforeEtb)0x0);
  if ((card_00 != 0xffffffff) && ((card_00 & 0x800) == 0))
  {
    result = is_what(player, card_00, 2);
    if (result != 0)
    {
      ereboss_titan_effect(1 - player);
    }
  }
  return card_00;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269b8f0; original functions/functions.c:5258. */
int reanimate_permanent_with_effect(int player, int card, int target_graveyard, int selected,
                                         reanimate_mode_t action, CardFunction effect_fn)
{
  unsigned int card_00;
  int result;

  card_00 =
      reanimate_permanent_impl(player, card, target_graveyard, selected, action,
                                    COUNTER_invalid, effect_fn, (ReanimationBeforeEtb)0x0);
  if ((card_00 != 0xffffffff) && ((card_00 & 0x800) == 0))
  {
    result = is_what(player, card_00, 2);
    if (result != 0)
    {
      ereboss_titan_effect(1 - player);
    }
  }
  return card_00;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269b970; original functions/functions.c:5267. */
int reanimate_permanent_with_counter_and_effect(int player, int card, int target_graveyard,
                                                     int selected, reanimate_mode_t action,
                                                     counter_t counter_type, CardFunction effect_fn)
{
  unsigned int card_00;
  int result;

  card_00 = reanimate_permanent_impl(player, card, target_graveyard, selected, action,
                                          counter_type, effect_fn, (ReanimationBeforeEtb)0x0);
  if ((card_00 != 0xffffffff) && ((card_00 & 0x800) == 0))
  {
    result = is_what(player, card_00, 2);
    if (result != 0)
    {
      ereboss_titan_effect(1 - player);
    }
  }
  return card_00;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269b9f0; original functions/functions.c:5278. */
int reanimate_permanent_with_function(int player, int card, int target_graveyard, int selected,
                                           reanimate_mode_t action,
                                           ReanimationBeforeEtb before_etb)
{
  unsigned int card_00;
  int result;

  card_00 = reanimate_permanent_impl(player, card, target_graveyard, selected, action,
                                          COUNTER_invalid, (CardFunction)0x0, before_etb);
  if ((card_00 != 0xffffffff) && ((card_00 & 0x800) == 0))
  {
    result = is_what(player, card_00, 2);
    if (result != 0)
    {
      ereboss_titan_effect(1 - player);
    }
  }
  return card_00;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678aa0; original functions/functions.c:18560. */
int haste_and_sacrifice_eot(int player, int card, event_t event)
{
  int result;
  result = haste_and_kill_at_eot(player, card, event, 3);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a0080; original functions/functions.c:18564. */
int haste_and_remove_at_beginning_of_next_end_step(int player, int card, int t_player, int t_card,
                                                   int kill_mode)
{
  int result;
  int legacy_card;
  card_instance_t *instance;

  if (kill_mode == 4 || kill_mode == 3)
  {
    if (kill_mode == 4)
    {
      legacy_card =
          create_targetted_legacy_effect(player, card, haste_and_remove_eot, t_player, t_card);
      result = current_phase;
    }
    else
    {
      legacy_card =
          create_targetted_legacy_effect(player, card, haste_and_sacrifice_eot, t_player, t_card);
      instance = get_card_instance(player, legacy_card);
      result = current_phase;
      instance->number_of_targets = 0;
    }
    if (result == 0x1f && legacy_card >= 0)
    {
      instance = get_card_instance(player, legacy_card);
      instance->targets[1].player = 1;
    }
    return legacy_card;
  }
  if (kill_mode < 1 || kill_mode > 5)
    return -1;
  create_targetted_legacy_effect(player, card, generic_haste_and_kill_at_eot_legacy, t_player,
                                 t_card);
  instance = get_card_instance(player, -1);
  instance->targets[1].card = kill_mode;
  return -1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a01a0; original functions/functions.c:18598. */
void haste_and_sacrifice_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                        int number)
{
  haste_and_remove_at_beginning_of_next_end_step(token->s_player, token->s_card,
                                                      token->t_player, beast, 3);
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a01e0; original functions/functions.c:18603. */
void haste_and_remove_token_at_end_of_next_turn(token_generation_t *token, int beast,
                                                     int number)
{
  haste_and_remove_at_beginning_of_next_end_step(token->s_player, token->s_card,
                                                      token->t_player, beast, 4);
  return;
}

#endif

#ifndef SHANDALAR
/* October 2022 shared cycling state at 0x2775470. */
int card_is_being_cycled = 0;
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679820; original functions/functions.c:125. */
int is_valid_card(int id)
{
  if (id < 0 || id >= available_slots || !(card_coded[id / 8] & (1 << (id % 8))))
    return 0;
  int iid = get_internal_card_id_from_csv_id(id);
  if (cards_data[iid].cc[2] == 3 || cards_ptr[id]->expansion == 0x1000 ||
      cards_data[iid].type == TYPE_EFFECT)
    return 0;
  return (id & ~4) != 0x38 && id != 0x2b && id != 0x1df && id != 0x1af && id != 0x2fb &&
         id != 0x32a && id != 0x394 && id != 0x615a && id != 0x6161;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26798f0; original functions/functions.c:197. */
const char *get_card_name_by_id(int csvid) { return cards_ptr[csvid]->name; }

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679910; original functions/functions.c:202. */
const char *get_card_name_by_instance(card_instance_t *instance)
{
  return cards_ptr[cards_data[instance->internal_card_id].id]->name;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679940; original functions/functions.c:207. */
const char *get_card_or_subtype_name(int subtype_or_csvid)
{
  if (subtype_or_csvid < 0)
    return cards_ptr[-(subtype_or_csvid + 1)]->name;
  else if (subtype_or_csvid <= 232)
    return shandalar_hunting_subtype_name(subtype_or_csvid);
  else
    return "";
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679c30; original functions/functions.c:1013. */
int tap_card(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!(instance->state & STATE_TAPPED))
  {
    tapped_for_mana_color = -1;
    instance->state |= STATE_TAPPED;
    dispatch_event(player, card, EVENT_TAP_CARD);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679c90; original functions/functions.c:1027. */
void untap_card(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->state & STATE_TAPPED)
  {
    unsigned char bits = player_bits[player];
    instance->state &= ~STATE_TAPPED;
    if (bits & PB_SEND_EVENT_UNTAP_CARD_TO_ALL)
      dispatch_event(player, card, EVENT_UNTAP_CARD);
    else
      dispatch_event_with_attacker_to_one_card(player, card, EVENT_UNTAP_CARD, 1 - player, -1);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679d10; original functions/functions.c:1043. */
void untap_card_no_event(int player, int card)
{
  get_card_instance(player, card)->state &= ~STATE_TAPPED;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267a900; original functions/functions.c:1847. */
int count_shrines(int player, int card)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
  td.required_subtype = SUBTYPE_SHRINE;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  return target_available(player, card, &td);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bbb0; original functions/functions.c:2402. */
int count_cards_by_id(int player, int id)
{
  int total = 0;
  int i;
  for (i = 0; i < 2; i++)
  {
    if (player == 2 || i == player)
    {
      int count = 0;
      while (count < active_cards_count[i])
      {
        if (in_play(i, count) && !is_humiliated(i, count, AL_STATIC) &&
            get_id(i, count) == id)
        {
          total++;
        }
        count++;
      }
    }
  }
  return total;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268ff90; original functions/functions.c:2449. */
int has_mana_multi(int player, int colorless, int black, int blue, int green, int red,
                        int white)
{
  if (card_flags[player] & CFLAGS_KRRIK)
  {
    int krrik_offset = how_much_life_can_i_pay_impl(player, 1) / 2;
    black = black < krrik_offset ? 0 : black - krrik_offset;
  }
  MANA_ARTIFACT = 0;
  MANA_UNKNOWN = 0;
  MANA_COLORLESS = MAX(0, colorless);
  MANA_BLACK = MAX(0, black);
  MANA_BLUE = MAX(0, blue);
  MANA_GREEN = MAX(0, green);
  MANA_RED = MAX(0, red);
  MANA_WHITE = MAX(0, white);
  return check_mana_multi(player);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268fe30; original functions/functions.c:2466. */
int has_mana_multi_a(int player, int colorless, int black, int blue, int green, int red,
                          int white)
{
  if (card_flags[player] & CFLAGS_KRRIK)
  {
    int krrik_offset = how_much_life_can_i_pay_impl(player, 1) / 2;
    black = black < krrik_offset ? 0 : black - krrik_offset;
  }
  colorless = MAX(0, colorless);
  black = MAX(0, black);
  blue = MAX(0, blue);
  green = MAX(0, green);
  red = MAX(0, red);
  white = MAX(0, white);
  if (!(black || blue || green || red || white))
    return has_mana(player, COLOR_ARTIFACT, colorless) != 0;
  for (int generic = colorless; generic >= 0; --generic)
  {
    MANA_BLACK = black;
    MANA_BLUE = blue;
    MANA_GREEN = green;
    MANA_RED = red;
    MANA_WHITE = white;
    MANA_COLORLESS = generic;
    MANA_ARTIFACT = colorless - generic;
    MANA_UNKNOWN = 0;
    if (check_mana_multi(player))
      return 1;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bc50; original functions/functions.c:2503. */
int charge_mana_multi(int player, int colorless, int black, int blue, int green, int red,
                           int white)
{
  pay_mana_xbugrwaU[0] = colorless;
  pay_mana_xbugrwaU[1] = black;
  pay_mana_xbugrwaU[2] = blue;
  pay_mana_xbugrwaU[3] = green;
  pay_mana_xbugrwaU[4] = red;
  pay_mana_xbugrwaU[5] = white;
  charge_mana(player, COLOR_COLORLESS, 0);
  return spell_fizzled != 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bcb0; original functions/functions.c:2517. */
int charge_mana_multi_a(int player, int colorless, int black, int blue, int green, int red,
                             int white, int artifact)
{
  pay_mana_xbugrwaU[0] = colorless;
  pay_mana_xbugrwaU[1] = black;
  pay_mana_xbugrwaU[2] = blue;
  pay_mana_xbugrwaU[3] = green;
  pay_mana_xbugrwaU[4] = red;
  pay_mana_xbugrwaU[5] = white;
  pay_mana_xbugrwaU[6] = artifact;
  charge_mana(player, COLOR_COLORLESS, 0);
  return spell_fizzled != 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bd20; original functions/functions.c:2534. */
int charge_mana_while_resolving(int player, int card, event_t event, int charged_player,
                                     color_t color, int amount)
{
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    event = EVENT_RESOLVE_TRIGGER;
  put_card_or_activation_onto_stack(player, card, event, 0, 0);
  spell_fizzled = 0;
  ldoubleclicked = 0;
  charge_mana(charged_player, color, amount);
  obliterate_top_card_of_stack();
  return spell_fizzled != 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bda0; original functions/functions.c:2545. */
int charge_mana_while_resolving_csvid(int csvid, event_t event, int charged_player,
                                           color_t color, int amount)
{
  int card_added = add_card_to_hand(charged_player, get_internal_card_id_from_csv_id(csvid));
  card_instance_t *instance = get_card_instance(charged_player, card_added);
  unsigned state = instance->state;
  --hand_count[charged_player];
  instance->state = state | STATE_INVISIBLE;
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    event = EVENT_RESOLVE_TRIGGER;
  put_card_or_activation_onto_stack(charged_player, card_added, event, 0, 0);
  spell_fizzled = 0;
  ldoubleclicked = 0;
  charge_mana(charged_player, color, amount);
  obliterate_top_card_of_stack();
  obliterate_card(charged_player, card_added);
  return spell_fizzled != 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267be70; original functions/functions.c:2563. */
int charge_mana_multi_while_resolving(int player, int card, event_t event, int charged_player,
                                           int colorless, int black, int blue, int green, int red,
                                           int white)
{
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    event = EVENT_RESOLVE_TRIGGER;
  put_card_or_activation_onto_stack(player, card, event, 0, 0);
  pay_mana_xbugrwaU[0] = colorless;
  pay_mana_xbugrwaU[1] = black;
  pay_mana_xbugrwaU[2] = blue;
  pay_mana_xbugrwaU[3] = green;
  pay_mana_xbugrwaU[4] = red;
  pay_mana_xbugrwaU[5] = white;
  spell_fizzled = 0;
  ldoubleclicked = 0;
  charge_mana(charged_player, COLOR_COLORLESS, 0);
  int result = spell_fizzled != 1;
  obliterate_top_card_of_stack();
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267bf30; original functions/functions.c:2574. */
int charge_mana_multi_while_resolving_csvid(int csvid, event_t event, int charged_player,
                                                 int colorless, int black, int blue, int green,
                                                 int red, int white)
{
  int card_added = add_card_to_hand(charged_player, get_internal_card_id_from_csv_id(csvid));
  card_instance_t *instance = get_card_instance(charged_player, card_added);
  unsigned state = instance->state;
  --hand_count[charged_player];
  instance->state = state | STATE_INVISIBLE;
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    event = EVENT_RESOLVE_TRIGGER;
  put_card_or_activation_onto_stack(charged_player, card_added, event, 0, 0);
  pay_mana_xbugrwaU[0] = colorless;
  pay_mana_xbugrwaU[1] = black;
  pay_mana_xbugrwaU[2] = blue;
  pay_mana_xbugrwaU[3] = green;
  pay_mana_xbugrwaU[4] = red;
  spell_fizzled = 0;
  ldoubleclicked = 0;
  pay_mana_xbugrwaU[5] = white;
  charge_mana(charged_player, COLOR_COLORLESS, 0);
  int result = spell_fizzled != 1;
  obliterate_top_card_of_stack();
  obliterate_card(charged_player, card_added);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c030; original functions/functions.c:2592. */
int charge_mana_hybrid_while_resolving(int player, int card, event_t event, int amt_colored,
                                            color_t first_color, color_t second_color,
                                            int amt_colorless)
{
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    event = EVENT_RESOLVE_TRIGGER;
  put_card_or_activation_onto_stack(player, card, event, 0, 0);
  spell_fizzled = 0;
  ldoubleclicked = 0;
  int result =
      charge_mana_hybrid(player, card, amt_colored, first_color, second_color, amt_colorless);
  obliterate_top_card_of_stack();
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c0c0; original functions/functions.c:2626. */
int is_basic_land_by_id(int id)
{
  return id == CARD_ID_FOREST || id == CARD_ID_ISLAND || id == CARD_ID_SWAMP ||
         id == CARD_ID_PLAINS || id == CARD_ID_MOUNTAIN ||
         (unsigned int)id - CARD_ID_SNOW_COVERED_FOREST <= 4U || id == CARD_ID_WASTES;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c110; original functions/functions.c:2615. */
int is_basic_land(int player, int card)
{
  return is_basic_land_by_id(player == -1 ? cards_data[card].id : get_id(player, card));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c190; original functions/functions.c:2828. */
int choose_a_color(int player, int ai_choice)
{
  int result = -1;
  while (result == -1)
  {
    result = choose_a_color_exe(player, "Choose a color", 1, ai_choice, COLOR_TEST_ANY_COLORED);
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c1d0; original functions/functions.c:2836. */
int choose_a_color_from(int player, int ai_choice, color_test_t available_colors)
{
  int result = -1;
  while (result == -1)
  {
    result = choose_a_color_exe(player, "Choose a color", 1, ai_choice, available_colors);
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267c3a0; original functions/functions.c:2935. */
int get_global_color_hack(int player)
{
  unsigned int timestamp = 0;
  int result = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c))
      {
        if (get_id(p, c) == CARD_ID_PAINTERS_SERVANT)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->targets[1].card >= 0 && instance->timestamp > timestamp)
          {
            result |= instance->targets[1].card;
            timestamp = instance->timestamp;
          }
        }
        if (get_id(p, c) == CARD_ID_MYCOSYNTH_LATTICE)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->timestamp > timestamp)
          {
            result = COLOR_TEST_COLORLESS;
            timestamp = instance->timestamp;
          }
        }
      }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267ea70; original functions/functions.c:4094. */
int get_color(int player, int card)
{
  int result = get_abilities(player, card, EVENT_SET_COLOR, -1);
  int half_played = get_card_instance(player, card)->info_slot;
  int split_csvid = split_cards_id_table(get_id(player, card), half_played);
  if (split_csvid >= 0) {
    int iid = get_internal_card_id_from_csv_id(split_csvid);
    if (iid >= 0 && cards_data[iid].color)
      result = cards_data[iid].color | get_global_color_hack(player);
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267eaf0; original functions/functions.c:4103. */
int get_color_by_internal_id_no_hack(int internal_id)
{
  card_data_t *data = &cards_data[internal_id];
  int color = data->color;
  if (data->type == TYPE_LAND)
  {
    if (data->id == CARD_ID_DRYAD_ARBOR)
      color = COLOR_TEST_GREEN;
    else if (data->id == CARD_ID_JASCONIAN_ISLE)
      color = COLOR_TEST_BLUE;
    else
      color = 0;
  }
  else if ((data->type & TYPE_ARTIFACT) && data->cc[0] == 0)
    color = 0;
  if ((data->extra_ability & EA_MANA_SOURCE) && data->color != COLOR_TEST_COLORLESS)
    color &= COLOR_TEST_ANY_COLORED;
  return color;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267eb80; original functions/functions.c:4193. */
int is_creature_dead(void)
{
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_data_t *data = get_card_data(p, c);
      if ((data->type & TYPE_CREATURE) && in_play(p, c) && can_regenerate(p, c))
        return 99;
    }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267ed20; original functions/functions.c:4847. */
static int split_cards_cmc_table_by_id(int csvid, int half_played)
{
  int first = split_cards_id_table(csvid, 1);
  if (first < 0)
    return -1;
  if (num_bits_set((unsigned int)half_played) - 1U <= 1U)
  {
    int selected = split_cards_id_table(csvid, half_played);
    return get_cmc_by_internal_id(get_internal_card_id_from_csv_id(selected));
  }
  int result = get_cmc_by_internal_id(get_internal_card_id_from_csv_id(first));
  int second = split_cards_id_table(csvid, 2);
  return result + get_cmc_by_internal_id(get_internal_card_id_from_csv_id(second));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267ecb0; original functions/functions.c:4938. */
int get_cmc_by_internal_id(int iid)
{
  int result = cards_data[iid].cc[0];
  if (cards_data[iid].cc[1] <= 16)
    result += cards_data[iid].cc[1];
  if (has_no_mana_cost(-1, iid))
    return 0;
  int split = split_cards_cmc_table_by_id(cards_data[iid].id, 0);
  return split < 0 ? result : split;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694100; original functions/functions.c:4885. */
int get_cmc(int player, int card)
{
  if (player == -1)
    return get_cmc_by_internal_id(card);
  card_instance_t *instance = get_card_instance(player, card);
  if (is_token(player, card))
    return 0;
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  if (in_play(player, card) && check_special_flags3(player, card, SF3_CARD_IS_FLIPPED))
    iid = get_original_internal_card_id(player, card);
  int result = get_cmc_by_internal_id(iid);
  if (is_x_spell(player, card) && (instance->state & STATE_INVISIBLE))
    result += instance->info_slot;
  int csvid = get_id(player, card);
  int half = get_card_instance(player, card)->info_slot;
  int split = split_cards_cmc_table_by_id(csvid, half);
  if (split >= 0)
  {
    is_x_spell(player, card);
    csvid = get_id(player, card);
    if ((csvid == CARD_ID_CRIME_PUNISHMENT && (instance->info_slot & 2)) ||
        (csvid == CARD_ID_SUPPLY_DEMAND && (instance->info_slot & 1)))
      split += instance->targets[1].card > 0 ? instance->targets[1].card : 0;
    return split;
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2682fe0; original functions/functions.c:4973. */
int legacy_change_color(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[0].player;
  int c = instance->targets[0].card;

  if (instance->targets[1].player > 0)
  {
    if (event == EVENT_SET_COLOR && affect_me(p, c))
    {
      if (instance->targets[1].player & CHANGE_COLOR_ADD)
      {
        event_result |= instance->targets[1].card;
      }
      if (instance->targets[1].player & CHANGE_COLOR_SET)
      {
        event_result = instance->targets[1].card;
      }
    }
    if ((instance->targets[1].player & CHANGE_COLOR_END_AT_EOT) && eot_trigger(player, card, event))
    {
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f030; original functions/functions.c:5568. */
void cloning(int player, int card, int t_player, int t_card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (t_player == -1)
  {
    instance->targets[12].card = t_card;
  }
  else
  {
    card_instance_t *target = get_card_instance(t_player, t_card);
    int iid = target->internal_card_id;
    if (iid == -1)
      iid = target->backup_internal_card_id;

    switch (get_id(player, card))
    {
    case CARD_ID_PHYREXIAN_METAMORPH:
      if (!(cards_data[iid].type & TYPE_ARTIFACT))
      {
        iid = create_a_card_type(iid);
        cards_data[iid].type |= TYPE_ARTIFACT;
      }
      break;

    case CARD_ID_COPY_ARTIFACT:
      if (!(cards_data[iid].type & TYPE_ENCHANTMENT) || cards_data[iid].cc[2] == 9)
      {
        iid = create_a_card_type(iid);
        cards_data[iid].type |= TYPE_ENCHANTMENT;
      }
      break;
    }
    if (target->targets[13].player > -1 && target->targets[13].card > -1)
    {
      if (t_player != -1 && check_status(t_player, t_card, STATUS_ANIMATED))
      {
        instance->targets[13].player =
            cards_data[get_original_internal_card_id(t_player, t_card)].id;
        instance->targets[13].card =
            cards_data[get_original_internal_card_id(t_player, t_card)].id + 1;
      }
      else
      {
        instance->targets[13].player = target->targets[13].player;
        instance->targets[13].card = target->targets[13].card;
      }
    }
    if (target->targets[12].player > -1)
    {
      if (t_player != -1 && check_status(t_player, t_card, STATUS_ANIMATED))
      {
        instance->targets[12].player =
            cards_data[get_original_internal_card_id(t_player, t_card)].id;
      }
      else
      {
        instance->targets[12].player = target->targets[12].player;
      }
    }
    if (t_player != -1 && check_status(t_player, t_card, STATUS_ANIMATED))
    {
      instance->targets[12].card = get_original_internal_card_id(t_player, t_card);
    }
    else
    {
      instance->targets[12].card = iid;
    }
    instance->initial_color = 0;
    copy_token_characteristics_for_clone(player, card, t_player, t_card);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f2d0; original functions/functions.c:5624. */
void cloning_and_verify_legend(int player, int card, int t_player, int t_card)
{
  cloning(player, card, t_player, t_card);

  card_instance_t *instance = get_card_instance(player, card);
  instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE;
  get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
  if (in_play(player, card))
    verify_legend_rule(player, card, get_id(player, card));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f570; original functions/functions.c:5692. */
int enters_the_battlefield_as_copy_of_any_creature(int player, int card, event_t event)
{
  target_definition_t td;
  if (event == EVENT_RESOLVE_SPELL)
  {
    base_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = ANYBODY;
  }
  return enters_the_battlefield_as_copy_of_any(player, card, event, &td, "SELECT_A_CREATURE");
}

#endif

#ifndef SHANDALAR
static int create_targetted_legacy_effect_replace(int player, int card, CardFunction func_ptr,
                                                  int target_player_id, int target_card)
{
  target_t previous = find_repeat_legacy_effect_alt(player, card, func_ptr,
                                                   target_player_id, target_card, -1, player);
  if (previous.card != -1)
    kill_card(player, previous.card, KILL_EXILE);
  return create_targetted_legacy_effect(player, card, func_ptr, target_player_id, target_card);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2686b30; original functions/functions.c:5836. */
int shapeshift_target(int src_player, int src_card, int t_player, int t_card,
                           int turn_into_player, int turn_into_card, int mode)
{
  int legacy = real_shapeshift_target(src_player, src_card, t_player, t_card, turn_into_player,
                                           turn_into_card, mode);

  if (in_play(t_player, t_card))
    verify_legend_rule(t_player, t_card, get_id(t_player, t_card));

  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f5e0; original functions/functions.c:5887. */
void land_animation(int player, int card)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (instance->targets[13].player == instance->targets[13].card)
  {
    int id = instance->targets[13].player;
    id++;
    instance->targets[12].card = get_internal_card_id_from_csv_id(id);
    instance->targets[13].card = id;
  }
  else
  {
    int id = instance->targets[13].player;
    instance->targets[12].card = get_internal_card_id_from_csv_id(id);
    instance->targets[13].card = id;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f6a0; original functions/functions.c:5991. */
int is_colorless(int player, int card)
{
  if (get_color(player, card) < 2)
  {
    return 1;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f6d0; original functions/functions.c:5998. */
int is_target(int player, int card, int controller_of_effect)
{
  for (int i = 0; i < stack_size; ++i)
  {
    if (controller_of_effect == ANYBODY || stack_cards[i].player == controller_of_effect)
    {
      card_instance_t *instance = get_card_instance(stack_cards[i].player, stack_cards[i].card);
      for (int j = 0; j < instance->number_of_targets; ++j)
        if (instance->targets[j].player == player && instance->targets[j].card == card)
          return 1;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f7a0; original functions/functions.c:6116. */
int get_colors_to_sleight_from_text(int csvid)
{
  char *txt = cards_ptr[csvid]->rules_text;
  int colors_to_sleight = 0;
  if (txt)
  {
    while ((txt = strstr(txt, "|S")))
    {
      txt += 2;
      int c;
      for (c = COLOR_BLACK; c <= COLOR_WHITE; ++c)
      {
        int i;
        for (i = 0; i < num_words_to_sleight; ++i)
        {
          int l = strlen(strs_words_to_sleight[c][i]);
          if (!strncmp(txt, strs_words_to_sleight[c][i], l))
          {
            txt += l;
            colors_to_sleight |= 1 << c;
            goto outer_continue;
          }
        }
      }
    outer_continue:;
    }
  }
  return colors_to_sleight;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678430; original functions/functions.c:6145. */
static int get_colors_to_hack_from_rules_text(int csvid)
{
  char *txt = cards_ptr[csvid]->rules_text;
  int colors_to_sleight = 0;
  if (txt)
  {
    while ((txt = strstr(txt, "|H")))
    {
      txt += 2;
      int c;
      for (c = COLOR_BLACK; c <= COLOR_WHITE; ++c)
      {
        int i;
        for (i = 0; i < num_words_to_hack; ++i)
        {
          int l = strlen(strs_words_to_hack[c][i]);
          if (!strncmp(txt, strs_words_to_hack[c][i], l))
          {
            txt += l;
            colors_to_sleight |= 1 << c;
            goto outer_continue;
          }
        }
      }
    outer_continue:;
    }
  }
  return colors_to_sleight;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678330; original functions/functions.c:6173. */
static int get_colors_to_hack_from_type_text(int csvid)
{
  const char *txt = cards_ptr[csvid]->type_text;
  int colors_to_sleight = 0;
  if (txt)
  {
    while ((txt = strstr(txt, "|H")))
    {
      txt += 2;
      int c;
      for (c = COLOR_BLACK; c <= COLOR_WHITE; ++c)
      {
        int i;
        for (i = 0; i < num_words_to_hack; ++i)
        {
          int l = strlen(strs_words_to_hack[c][i]);
          if (!strncmp(txt, strs_words_to_hack[c][i], l))
          {
            txt += l;
            colors_to_sleight |= 1 << c;
            goto outer_continue;
          }
        }
      }
    outer_continue:;
    }
  }
  return colors_to_sleight;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f8a0; original functions/functions.c:6201. */
int get_colors_to_hack_from_csvid(int csvid)
{
  int result = get_colors_to_hack_from_type_text(csvid);
  result |= get_colors_to_hack_from_rules_text(csvid);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f8c0; original functions/functions.c:6208. */
int get_hacked_color(int player, int card, int color)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = instance->hack_mode[color];
  if (!instance->hack_mode[color])
    result = color;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x267f900; original functions/functions.c:6221. */
int get_sleighted_color(int player, int card, int color)
{
  card_instance_t *instance;
  color_t result;

  instance = get_card_instance(player, card);
  result = (uint8_t)instance->color_id[color];
  if (!instance->color_id[color])
    result = color;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680270; original functions/functions.c:6346. */
const char *get_sleighted_color_text(int player, int card, const char *fmt, int orig_color_t)
{
  if (ai_is_speculating == 1)
  {
    return "";
  }

  static char buf[500];

  static const char *words[][2] = {
      {"colorless", "COLORLESS"}, {"black", "BLACK"}, {"blue", "BLUE"},
      {"green", "GREEN"},         {"red", "RED"},     {"white", "WHITE"},
      {"artifact", "ARTIFACT"},   {"any", "ANY"}};

  int sleighted = get_sleighted_color(player, card, orig_color_t);

  sprintf(buf, fmt, words[sleighted][sleighted == orig_color_t ? 0 : 1]);

  return buf;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26802f0; original functions/functions.c:6372. */
const char *get_sleighted_color_text2(int player, int card, const char *fmt,
                                           int orig_color_t_1, int orig_color_t_2)
{
  if (ai_is_speculating == 1)
  {
    return "";
  }

  static char buf[500];

  static const char *words[][2] = {
      {"colorless", "COLORLESS"}, {"black", "BLACK"}, {"blue", "BLUE"},
      {"green", "GREEN"},         {"red", "RED"},     {"white", "WHITE"},
      {"artifact", "ARTIFACT"},   {"any", "ANY"}};

  int sleighted1 = get_sleighted_color(player, card, orig_color_t_1);
  int sleighted2 = get_sleighted_color(player, card, orig_color_t_2);

  sprintf(buf, fmt, words[sleighted1][sleighted1 == orig_color_t_1 ? 0 : 1],
          words[sleighted2][sleighted2 == orig_color_t_2 ? 0 : 1]);

  return buf;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26803c0; original functions/functions.c:6398. */
int get_sleighted_color_test(int player, int card, int orig_color_test)
{
  if (card < 0 || player < 0)
  {
    return orig_color_test;
  }
  switch (orig_color_test)
  {
  case COLOR_TEST_BLACK:
    orig_color_test = COLOR_BLACK;
    break;
  case COLOR_TEST_BLUE:
    orig_color_test = COLOR_BLUE;
    break;
  case COLOR_TEST_GREEN:
    orig_color_test = COLOR_GREEN;
    break;
  case COLOR_TEST_RED:
    orig_color_test = COLOR_RED;
    break;
  case COLOR_TEST_WHITE:
    orig_color_test = COLOR_WHITE;
    break;
  default:
  {
    if (orig_color_test & COLOR_TEST_ANY_COLORED)
    {
      uint32_t col, new_color_test = 0;
      for (col = COLOR_BLACK; col <= COLOR_WHITE; ++col)
      {
        if (orig_color_test & (COLOR_TEST_BLACK << (col - 1)))
        {
          new_color_test |= COLOR_TEST_BLACK << (get_sleighted_color(player, card, col) - 1);
        }
      }
      return (orig_color_test & ~COLOR_TEST_ANY_COLORED) | new_color_test;
    }
    return orig_color_test;
  }
  }
  return COLOR_TEST_BLACK << (get_sleighted_color(player, card, orig_color_test) - 1);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26804d0; original functions/functions.c:4124. */
int get_color_by_internal_id(int player, int internal_id)
{
  unsigned int timestamp = 0;
  int changer = -1;
  int hack = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c) && !is_humiliated(p, c, AL_STATIC))
      {
        if (get_id(p, c) == CARD_ID_PAINTERS_SERVANT)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->targets[1].card >= 0 && instance->timestamp > timestamp)
          {
            hack |= instance->targets[1].card;
            changer = CARD_ID_PAINTERS_SERVANT;
            timestamp = instance->timestamp;
          }
        }
        if (p == player && get_id(player, c) == CARD_ID_CELESTIAL_DAWN)
        {
          card_instance_t *instance = get_card_instance(player, c);
          unsigned int next_timestamp = instance->timestamp;
          if (next_timestamp > timestamp)
          {
            hack = get_sleighted_color_test(player, c, COLOR_TEST_WHITE);
            changer = CARD_ID_CELESTIAL_DAWN;
            timestamp = next_timestamp;
          }
        }
        if (get_id(p, c) == CARD_ID_MYCOSYNTH_LATTICE)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->timestamp > timestamp)
          {
            hack = COLOR_TEST_COLORLESS;
            changer = CARD_ID_MYCOSYNTH_LATTICE;
            timestamp = instance->timestamp;
          }
        }
      }
  int color = get_color_by_internal_id_no_hack(internal_id);
  if (changer == CARD_ID_MYCOSYNTH_LATTICE)
    return hack;
  if (changer == CARD_ID_PAINTERS_SERVANT)
    return color | hack;
  if (changer == CARD_ID_CELESTIAL_DAWN && !(cards_data[internal_id].type & TYPE_LAND))
    return hack;
  return color;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26806c0; original functions/functions.c:4185. */
int get_color_real_or_iid(int player_or_owner, int player_or_negone, int card_or_iid)
{
  return (player_or_negone >= 0 ? get_color(player_or_negone, card_or_iid)
                                : get_color_by_internal_id(player_or_owner, card_or_iid));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26806f0; original functions/functions.c:4995. */
void change_color(int player, int card, int t_player, int t_card, color_test_t new_color,
                       change_color_t mode)
{

  int legacy = create_targetted_legacy_effect_no_repeat(player, card, &legacy_change_color,
                                                        t_player, t_card);
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[1].player = mode;
  if (!(mode & CHANGE_COLOR_NO_SLEIGHT))
  {
    new_color = get_sleighted_color_test(player, card, new_color);
  }
  instance->targets[1].card = new_color;
  if (!(mode & CHANGE_COLOR_NO_SOUND))
  {
    play_sound_effect(WAV_CHANGEC);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26807a0; original functions/functions.c:6426. */
int get_sleighted_protection(int player, int card, int orig_prot)
{
  switch (orig_prot)
  {
  case KEYWORD_PROT_BLACK:
    orig_prot = COLOR_BLACK;
    break;
  case KEYWORD_PROT_BLUE:
    orig_prot = COLOR_BLUE;
    break;
  case KEYWORD_PROT_GREEN:
    orig_prot = COLOR_GREEN;
    break;
  case KEYWORD_PROT_RED:
    orig_prot = COLOR_RED;
    break;
  case KEYWORD_PROT_WHITE:
    orig_prot = COLOR_WHITE;
    break;
  default:
  {
    if (orig_prot & KEYWORD_PROT_COLORED)
    {
      uint32_t col, new_prot = 0;
      for (col = COLOR_BLACK; col <= COLOR_WHITE; ++col)
      {
        if (orig_prot & (KEYWORD_PROT_BLACK << (col - 1)))
        {
          new_prot |= KEYWORD_PROT_BLACK << (get_sleighted_color(player, card, col) - 1);
        }
      }
      return (orig_prot & ~KEYWORD_PROT_COLORED) | new_prot;
    }
    return orig_prot;
  }
  }
  return KEYWORD_PROT_BLACK << (get_sleighted_color(player, card, orig_prot) - 1);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26808c0; original functions/functions.c:6454. */
const char *get_hacked_land_text(int player, int card, const char *fmt, ...)
{
  static char buf[500];
  if (ai_is_speculating == 1)
  {
    *buf = 0;
    return buf;
  }

  static const char *words[][3] = {
      {"land", "Land", "LAND"},
      {"swamp", "Swamp", "SWAMP"},
      {"island", "Island", "ISLAND"},
      {"forest", "Forest", "FOREST"},
      {"mountain", "Mountain", "MOUNTAIN"},
      {"plains", "Plains", "PLAINS"},
      {"land", "Land", "LAND"},
      {"land", "Land", "LAND"},
  };

  va_list args;
  va_start(args, fmt);

  const char *p = fmt;
  char *q = buf;
  for (; (*q = *p); ++p, ++q)
  {
    if (*p == '%' && (*(p + 1) == 's' || *(p + 1) == 'a' || *(p + 1) == 'l'))
    {

      int hacked, orig_color_t, orig_subtype = va_arg(args, int);

      switch (orig_subtype)
      {
      case HARDCODED_SUBTYPE_SWAMP:
      case SUBTYPE_SWAMP:
      case COLOR_BLACK:
        orig_color_t = COLOR_BLACK;
        break;
      case HARDCODED_SUBTYPE_ISLAND:
      case SUBTYPE_ISLAND:
      case COLOR_BLUE:
        orig_color_t = COLOR_BLUE;
        break;
      case HARDCODED_SUBTYPE_FOREST:
      case SUBTYPE_FOREST:
      case COLOR_GREEN:
        orig_color_t = COLOR_GREEN;
        break;
      case HARDCODED_SUBTYPE_MOUNTAIN:
      case SUBTYPE_MOUNTAIN:
      case COLOR_RED:
        orig_color_t = COLOR_RED;
        break;
      case HARDCODED_SUBTYPE_PLAINS:
      case SUBTYPE_PLAINS:
      case COLOR_WHITE:
        orig_color_t = COLOR_WHITE;
        break;
      default:
        hacked = orig_color_t = 0;
        goto skip_hack;
      }
      hacked = (player >= 0 && card >= 0) ? get_hacked_color(player, card, orig_color_t)
                                          : orig_color_t;

    skip_hack:
      ++p;
      int article = *p == 'a';
      int lowercase = *p == 'l';

      if (article)
      {
        *q++ = 'a';
        if (hacked == COLOR_BLUE)
        {
          *q++ = 'n';
        }
        *q++ = ' ';
      }
      if (hacked != orig_color_t)
      {
        q += sprintf(q, "%s", words[hacked][2]) - 1;
      }
      else if (lowercase)
      {
        q += sprintf(q, "%s", words[hacked][0]) - 1;
      }
      else
      {
        q += sprintf(q, "%s", words[hacked][1]) - 1;
      }
    }
  }

  va_end(args);

  return buf;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680af0; original functions/functions.c:6521. */
int get_hacked_walk(int player, int card, int orig_walk)
{
  switch (orig_walk)
  {
  case KEYWORD_SWAMPWALK:
    orig_walk = COLOR_BLACK;
    break;
  case KEYWORD_ISLANDWALK:
    orig_walk = COLOR_BLUE;
    break;
  case KEYWORD_FORESTWALK:
    orig_walk = COLOR_GREEN;
    break;
  case KEYWORD_MOUNTAINWALK:
    orig_walk = COLOR_RED;
    break;
  case KEYWORD_PLAINSWALK:
    orig_walk = COLOR_WHITE;
    break;
  default:
  {
    if (orig_walk & KEYWORD_BASIC_LANDWALK)
    {
      uint32_t col, new_walk = 0;
      for (col = COLOR_BLACK; col <= COLOR_WHITE; ++col)
      {
        if (orig_walk & (KEYWORD_SWAMPWALK << (col - 1)))
        {
          new_walk |= KEYWORD_SWAMPWALK << (get_hacked_color(player, card, col) - 1);
        }
      }
      return (orig_walk & ~KEYWORD_BASIC_LANDWALK) | new_walk;
    }
    return orig_walk;
  }
  }
  return KEYWORD_SWAMPWALK << (get_hacked_color(player, card, orig_walk) - 1);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680be0; original functions/functions.c:6546. */
int get_hacked_subtype(int player, int card, int orig_subtype)
{
  if (card < 0 || player < 0)
  {
    return orig_subtype;
  }

  static int subtypes[][4] = {
      {0, 0, 0, 0},
      {SUBTYPE_SWAMP, HARDCODED_SUBTYPE_SWAMP, COLOR_BLACK, CARD_ID_SWAMP},
      {SUBTYPE_ISLAND, HARDCODED_SUBTYPE_ISLAND, COLOR_BLUE, CARD_ID_ISLAND},
      {SUBTYPE_FOREST, HARDCODED_SUBTYPE_FOREST, COLOR_GREEN, CARD_ID_FOREST},
      {SUBTYPE_MOUNTAIN, HARDCODED_SUBTYPE_MOUNTAIN, COLOR_RED, CARD_ID_MOUNTAIN},
      {SUBTYPE_PLAINS, HARDCODED_SUBTYPE_PLAINS, COLOR_WHITE, CARD_ID_PLAINS}};

  int column, orig_color_t;

  switch (orig_subtype)
  {
  case SUBTYPE_SWAMP:
    column = 0;
    orig_color_t = COLOR_BLACK;
    break;
  case HARDCODED_SUBTYPE_SWAMP:
    column = 1;
    orig_color_t = COLOR_BLACK;
    break;
  case COLOR_BLACK:
    column = 2;
    orig_color_t = COLOR_BLACK;
    break;
  case CARD_ID_SWAMP:
    column = 3;
    orig_color_t = COLOR_BLACK;
    break;

  case SUBTYPE_ISLAND:
    column = 0;
    orig_color_t = COLOR_BLUE;
    break;
  case HARDCODED_SUBTYPE_ISLAND:
    column = 1;
    orig_color_t = COLOR_BLUE;
    break;
  case COLOR_BLUE:
    column = 2;
    orig_color_t = COLOR_BLUE;
    break;
  case CARD_ID_ISLAND:
    column = 3;
    orig_color_t = COLOR_BLUE;
    break;

  case SUBTYPE_FOREST:
    column = 0;
    orig_color_t = COLOR_GREEN;
    break;
  case HARDCODED_SUBTYPE_FOREST:
    column = 1;
    orig_color_t = COLOR_GREEN;
    break;
  case COLOR_GREEN:
    column = 2;
    orig_color_t = COLOR_GREEN;
    break;
  case CARD_ID_FOREST:
    column = 3;
    orig_color_t = COLOR_GREEN;
    break;

  case SUBTYPE_MOUNTAIN:
    column = 0;
    orig_color_t = COLOR_RED;
    break;
  case HARDCODED_SUBTYPE_MOUNTAIN:
    column = 1;
    orig_color_t = COLOR_RED;
    break;
  case COLOR_RED:
    column = 2;
    orig_color_t = COLOR_RED;
    break;
  case CARD_ID_MOUNTAIN:
    column = 3;
    orig_color_t = COLOR_RED;
    break;

  case SUBTYPE_PLAINS:
    column = 0;
    orig_color_t = COLOR_WHITE;
    break;
  case HARDCODED_SUBTYPE_PLAINS:
    column = 1;
    orig_color_t = COLOR_WHITE;
    break;
  case COLOR_WHITE:
    column = 2;
    orig_color_t = COLOR_WHITE;
    break;
  case CARD_ID_PLAINS:
    column = 3;
    orig_color_t = COLOR_WHITE;
    break;

  default:
    return orig_subtype;
  }

  return subtypes[get_hacked_color(player, card, orig_color_t)][column];
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680da0; original functions/functions.c:6703. */
int color_to_color_test(int color)
{
  if (color == COLOR_ANY)
    return COLOR_TEST_ANY_COLORED;
  return (int)(1U << ((unsigned int)color & 31));
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680dc0; original functions/functions.c:6709. */
int keyword_to_color(int keyword)
{

  int i;
  for (i = 0; i < 5; i++)
  {
    if ((1 << (11 + i)) == keyword)
    {
      return i + 1;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2680e20; original functions/functions.c:6763. */
int is_token(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!(instance->token_status & STATUS_TOKEN))
    return 0;
  return !check_special_flags(player, card, SF_UNEARTH);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26812d0; original functions/functions.c:7156. */
int is_attacking(int player, int card)
{
  return (get_card_instance(player, card)->state & STATE_ATTACKING) != 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2681300; original functions/functions.c:7165. */
card_instance_t *in_play_and_attacking(int player, int card)
{
  card_instance_t *instance = in_play(player, card);
  if (instance && !(instance->state & STATE_ATTACKING))
    return NULL;
  return instance;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26845a0; original functions/functions.c:7194. */
int get_deck_color(int player, int whose_deck)
{
  if (player >= 0 && !IS_AI(player) && player != whose_deck)
  {
    return COLOR_BLACK;
  }
  int *deck = deck_ptr[whose_deck];
  int count = count_deck(whose_deck) - 1;
  int clrs[5] = {0, 0, 0, 0, 0};
  while (count > -1)
  {
    int color = cards_data[deck[count]].color;
    if ((color & COLOR_TEST_ANY_COLORED) && !is_what(-1, deck[count], TYPE_LAND))
    {
      int i;
      for (i = COLOR_BLACK; i <= COLOR_WHITE; i++)
      {
        if (color & (1 << i))
        {
          clrs[i - 1]++;
        }
      }
    }
    count--;
  }
  if (player != -1)
  {
    int ai_choice = 1;
    int i;
    int par = -1;
    for (i = COLOR_BLACK; i <= COLOR_WHITE; i++)
    {
      if (clrs[i - 1] > par)
      {
        par = clrs[i - 1];
        ai_choice = i;
      }
    }
    return ai_choice;
  }
  else
  {
    int clrs_res[2] = {0, 0};
    int par = -1;
    int i;
    for (i = COLOR_BLACK; i <= COLOR_WHITE; i++)
    {
      if (clrs[i - 1] > par)
      {
        par = clrs[i - 1];
        clrs_res[0] = i;
      }
    }
    par = -1;
    for (i = COLOR_BLACK; i <= COLOR_WHITE; i++)
    {
      if (clrs[i - 1] > par && i != clrs_res[0])
      {
        par = clrs[i - 1];
        clrs_res[1] = i;
      }
    }
    return (1 << clrs_res[0]) | (1 << clrs_res[1]);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2684730; original functions/functions.c:6720. */
int select_a_color(int player)
{

  int ai_choice = 0;

  if (player == AI)
  {
    ai_choice = get_deck_color(player, 1 - player);
  }

  int color = choose_a_color(player, ai_choice);
  int keyword = color_to_color_test(color);

  return keyword;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679240; original functions/functions.c:7301. */
int no_combat_damage_this_turn(int player, int card, event_t event)
{
  card_instance_t *damage = combat_damage_being_prevented(event);
  if (damage)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (damage->damage_source_card == instance->targets[0].card &&
        damage->damage_source_player == instance->targets[0].player)
      damage->info_slot = 0;
  }
  else if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2681af0; original functions/functions.c:8124. */
int is_planeswalker(int player, int card)
{

  if (player == -1)
  {
    card_data_t *card_d = &cards_data[card];
    if ((card_d->type & TYPE_ENCHANTMENT) && card_d->cc[2] == 9)
    {
      return 1;
    }
  }
  else
  {
    card_data_t *card_d = get_card_data(player, card);
    if ((card_d->type & TYPE_ENCHANTMENT) && card_d->cc[2] == 9)
    {
      return 1;
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2681b50; original functions/functions.c:6596. */
type_t get_type_with_iid(int player, int card, int iid)
{
  type_t typ = cards_data[iid].type;
  if ((typ & (TYPE_INSTANT | TYPE_INTERRUPT)) && (typ & (TYPE_PERMANENT | TYPE_SORCERY)))
    typ &= ~(TYPE_INSTANT | TYPE_INTERRUPT);
  if (typ & TYPE_INTERRUPT)
    typ |= TYPE_INSTANT;
  if (player == -1 || card == -1)
  {
    if ((cards_data[iid].type & TYPE_ENCHANTMENT) && cards_data[iid].cc[2] == 9)
      typ = (typ & ~TYPE_ENCHANTMENT) | TARGET_TYPE_PLANESWALKER;
  }
  else
  {
    if (is_planeswalker(player, card) ||
        ((cards_data[iid].type & TYPE_ENCHANTMENT) && cards_data[iid].cc[2] == 9))
      typ = (typ & ~TYPE_ENCHANTMENT) | TARGET_TYPE_PLANESWALKER;
    if (typ & (TYPE_PERMANENT | TARGET_TYPE_PLANESWALKER))
    {
      int flags =
          check_special_flags2(player, card, SF2_ENCHANTED_EVENING | SF2_MYCOSYNTH_LATTICE);
      if (flags & SF2_ENCHANTED_EVENING)
        typ |= TYPE_ENCHANTMENT;
      if (flags & SF2_MYCOSYNTH_LATTICE)
        typ |= TYPE_ARTIFACT;
    }
  }
  return typ;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2681c50; original functions/functions.c:6630. */
type_t get_type(int player, int card)
{
  int iid;
  if (player < 0)
    iid = card;
  else
  {
    card_instance_t *instance = get_card_instance(player, card);
    iid = instance->internal_card_id;
    if (iid < 0)
      iid = instance->backup_internal_card_id;
  }
  return get_type_with_iid(player, card, iid);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2681ca0; original functions/functions.c:6645. */
int is_what(int player, int card, int test_type)
{

  int typ;
  if (player == -1)
  {
    typ = cards_data[card].type;
    if ((test_type & TYPE_CREATURE) && cards_data[card].id == CARD_ID_GRIST_THE_HUNGER_TIDE)
      return 1;
  }
  else
  {
    typ = get_card_data(player, card)->type;
  }

  if ((test_type & TARGET_TYPE_PLANESWALKER) && is_planeswalker(player, card))
  {
    return 1;
  }

  if (test_type & TYPE_ENCHANTMENT)
  {
    if (player != -1 && (typ & TYPE_PERMANENT) &&
        check_special_flags2(player, card, SF2_ENCHANTED_EVENING))
    {
      typ |= TYPE_ENCHANTMENT;
    }
    else if ((test_type & TYPE_PERMANENT) != TYPE_PERMANENT && is_planeswalker(player, card))
    {
      return 0;
    }
  }

  if (test_type & TYPE_ARTIFACT)
  {
    if (player != -1 && (typ & TYPE_PERMANENT) &&
        check_special_flags2(player, card, SF2_MYCOSYNTH_LATTICE))
    {
      typ |= TYPE_ARTIFACT;
    }
  }

  if (typ & TYPE_INTERRUPT)
  {
    typ |= TYPE_INSTANT;
  }

  if ((typ & (TYPE_INSTANT | TYPE_INTERRUPT)) && (typ & (TYPE_PERMANENT | TYPE_SORCERY)))
  {
    typ &= ~(TYPE_INSTANT | TYPE_INTERRUPT);
  }

  return typ & test_type ? 1 : 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2682100; original functions/functions.c:945. */
int is_sick(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->state & STATE_SUMMON_SICK)
    if (is_what(player, card, TYPE_CREATURE))
      return !check_special_flags2(player, card, SF2_THOUSAND_YEAR_ELIXIR);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2682170; original functions/functions.c:955. */
int is_animated_and_sick(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->state & STATE_SUMMON_SICK)
    if (is_what(player, card, TYPE_CREATURE))
      return !check_special_flags2(player, card, SF2_THOUSAND_YEAR_ELIXIR);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26828d0; original functions/functions.c:1857. */
int count_subtype(int what_player, int type, int subtype)
{
  int p, c, count = 0;
  for (p = 0; p <= 1; ++p)
    if (what_player == ANYBODY || what_player == p)
      for (c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, type) && !is_what(p, c, TYPE_EFFECT) &&
            (subtype == -1 || has_subtype(p, c, subtype)))
          ++count;
  return count;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2682d30; original functions/functions.c:1868. */
int check_battlefield_for_subtype(int what_player, int type, int subtype)
{
  int p, c;
  for (p = 0; p <= 1; ++p)
    if (what_player == ANYBODY || what_player == p)
      for (c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, type) &&
            (subtype == -1 || has_subtype(p, c, subtype)))
          return 1;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2683b50; original functions/functions.c:2844. */
int count_permanents_by_type(int player, type_t type)
{
  int p, c, count = 0;
  for (p = 0; p <= 1; ++p)
    if (player == ANYBODY || player == p)
      for (c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, type) && !is_what(p, c, TYPE_EFFECT))
          ++count;
  return count;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2683e60; original functions/functions.c:6744. */
int count_permanents_by_color(int player, int selected_type, int selected_color)
{
  int result = 0;
  if (player == ANYBODY)
  {
    result = count_permanents_by_color(0, selected_type, selected_color);
    player = 1;
  }
  for (int c = 0; c < active_cards_count[player]; ++c)
    if (is_what(player, c, selected_type) && in_play(player, c) &&
        (get_color(player, c) & selected_color))
      ++result;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2684ee0; original functions/functions.c:8112. */
int is_mana_producer_land(int player, int card)
{
  int iid = player == -1 ? card : get_card_instance(player, card)->internal_card_id;
  return is_what(-1, iid, TYPE_LAND) && (cards_data[iid].extra_ability & EA_MANA_SOURCE);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26942c0; original functions/functions.c:8357. */
int get_most_common_cmc_nonland(int player)
{
  int c, cmc[17] = {0};

  for (c = 0; c < active_cards_count[player]; ++c)
    if (in_play(player, c) && is_what(player, c, TYPE_PERMANENT) &&
        !is_what(player, c, TYPE_LAND))
      cmc[get_cmc(player, c)]++;

  int i, result = -1, par = 0;
  for (i = 0; i < 17; ++i)
    if (cmc[i] > par)
    {
      par = cmc[i];
      result = i;
    }

  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694390; original functions/functions.c:8376. */
int get_most_common_cmc_in_hand(int player, type_t type)
{
  int c, cmc[17] = {0};

  for (c = 0; c < active_cards_count[player]; ++c)
    if (in_hand(player, c) && is_what(player, c, type))
      cmc[get_cmc(player, c)]++;

  int i, result = -1, par = 0;
  for (i = 0; i < 17; ++i)
    if (cmc[i] > par)
    {
      par = cmc[i];
      result = i;
    }

  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694440; original functions/functions.c:8395. */
int get_highest_cmc(int player, type_t type)
{
  int p, c, result = 0;
  for (p = 0; p < 2; p++)
  {
    if (player == p || player == ANYBODY)
    {
      for (c = 0; c < active_cards_count[p]; ++c)
      {
        if (in_play(p, c) && is_what(p, c, type))
        {
          int cmc = get_cmc(p, c);
          if (cmc > result)
          {
            result = cmc;
          }
        }
      }
    }
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26944f0; original functions/functions.c:8412. */
int get_highest_cmc_nonland(int player)
{
  int c, result = 0;
  for (c = 0; c < active_cards_count[player]; ++c)
    if (in_play(player, c) && is_what(player, c, TYPE_PERMANENT) &&
        !is_what(player, c, TYPE_LAND))
    {
      int cmc = get_cmc(player, c);
      if (cmc > result)
        result = cmc;
    }

  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687680; original functions/functions.c:8439. */
void choose_to_untap(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    set_special_flags2(player, card, SF2_COULD_NOT_UNTAP);
  }

  if (instance->targets[1].player > -1 && instance->targets[1].card > -1 &&
      !in_play(instance->targets[1].player, instance->targets[1].card))
  {
    instance->targets[1].player = -1;
  }

  if (event == EVENT_UNTAP && affect_me(player, card) && is_tapped(player, card))
  {
    instance->untap_status &= ~2;
  }

  if (current_phase == PHASE_UNTAP && affect_me(player, card))
  {
    if (event == EVENT_TRIGGER)
    {
      if (instance->untap_status & 1)
      {
        if (!(instance->untap_status & 2) &&
            !(cards_data[instance->internal_card_id].type & types_that_dont_untap))
        {
          if ((player != 1 || (trace_mode & 2)) && ai_is_speculating != 1)
          {
            event_result |= 1;
          }
          else if (instance->targets[1].player > -1 && instance->targets[1].card > -1 &&
                   in_play(instance->targets[1].player, instance->targets[1].card))
          {
            ;
          }
          else
          {
            event_result |= 2;
          }
        }
      }
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance->untap_status |= 2;
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687840; original functions/functions.c:8476. */
int choosing_to_untap(int player, int card, event_t event)
{
  if (event == EVENT_UNTAP && affect_me(player, card))
    get_card_instance(player, card)->untap_status &= ~2;

  if (current_phase == PHASE_UNTAP && affect_me(player, card))
  {
    if (event == EVENT_TRIGGER)
    {
      card_instance_t *instance = get_card_instance(player, card);
      if ((instance->untap_status & 3) == 1 &&
          !(cards_data[instance->internal_card_id].type & types_that_dont_untap))
      {
        if (!IS_AI(player))
          event_result |= 1;
        else if (call_card_function_i(instance, player, card, EVENT_CHOOSING_TO_UNTAP))
          event_result |= 2;
      }
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
      get_card_instance(player, card)->untap_status |= 2;
  }

  return event == EVENT_CHOOSING_TO_UNTAP;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687990; original functions/functions.c:8502. */
void add_state(int player, int card, int state)
{
  if (card != -1)
  {
    get_card_instance(player, card)->state |= state;
    return;
  }
  for (int p = 0; p < 2; ++p)
    if (player == p || player == ANYBODY)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
          get_card_instance(p, c)->state |= state;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687a50; original functions/functions.c:8524. */
void remove_state(int player, int card, int state)
{
  if (card != -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->state & state)
      instance->state &= ~state;
    return;
  }
  for (int p = 0; p < 2; ++p)
    if (player == p || player == ANYBODY)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, TYPE_PERMANENT))
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->state & state)
            instance->state &= ~state;
        }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687d40; original functions/functions.c:8550. */
void add_status(int player, int card, int status)
{
  get_card_instance(player, card)->token_status |= status;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687d70; original functions/functions.c:8555. */
void remove_status(int player, int card, int status)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->token_status & status)
    instance->token_status &= ~status;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687da0; original functions/functions.c:8562. */
void state_untargettable(int player, int card, int mode)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (mode == 0)
  {
    if (instance->state & STATE_CANNOT_TARGET)
      instance->state &= ~STATE_CANNOT_TARGET;
  }
  else if (mode == 1)
    instance->state |= STATE_CANNOT_TARGET;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2686f00; original functions/functions.c:8856. */
int fog_effect_legacy(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->info_slot > 0 &&
        !(get_card_instance(affected_card_controller, affected_card)->state & STATE_CANNOT_TARGET))
    {
      if (instance->targets[2].player >= 0 && instance->targets[2].card >= 0 &&
          instance->targets[2].player == damage->damage_source_player &&
          instance->targets[2].card == damage->damage_source_card)
        return 0;
      fog_special_flags_t flags = instance->targets[0].card;
      if ((flags & FOG_BY_ANY_TYPE) ||
          is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE))
      {
        if (instance->targets[0].player != ANYBODY &&
            instance->targets[0].player != ((flags & FOG_BY_CONTROLLED_BY_PLAYER)
                                                ? damage->damage_source_player
                                                : damage->damage_target_player))
          return 0;
        if ((flags & FOG_COMBAT_DAMAGE_ONLY) &&
            !(damage->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)))
          return 0;
        if ((flags & FOG_ONLY_DAMAGE_TO_T_PLAYER) &&
            (damage->damage_target_card != -1 || damage_is_to_planeswalker(damage)))
          return 0;
        if ((flags & FOG_UNBLOCKED_CREATURES_ONLY) &&
            !is_unblocked(damage->damage_source_player, damage->damage_source_card))
          return 0;
        if ((flags & FOG_CREATURES_WITHOUT_TRAMPLE) &&
            check_for_ability(damage->damage_source_player, damage->damage_source_card,
                              KEYWORD_TRAMPLE))
          return 0;
        if (flags & FOG_COLOR_MASK)
        {
          int color = damage->initial_color;
          if (in_play(damage->damage_source_player, damage->damage_source_card))
            color = get_color(damage->damage_source_player, damage->damage_source_card);
          if (flags & FOG_INVERSE_COLOR_CHECK)
          {
            if (color & flags)
              return 0;
          }
          else if (!(color & flags))
            return 0;
        }
        if ((flags & FOG_BY_NON_SUBTYPE) && instance->targets[1].player > 0 &&
            has_subtype(damage->damage_source_player, damage->damage_source_card,
                        instance->targets[1].player))
          return 0;
        if (instance->display_pic_csv_id == 0x2efd &&
            (has_subtype(damage->damage_source_player, damage->damage_source_card, SUBTYPE_WOLF) ||
             has_subtype(damage->damage_source_player, damage->damage_source_card,
                         SUBTYPE_WEREWOLF)))
          return 0;
        get_card_instance(affected_card_controller, affected_card)->info_slot = 0;
      }
    }
  }
  else if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26885d0; original functions/functions.c:8952. */
int fog_special2(int player, int card, int t_player, fog_special_flags_t flags, int param)
{
  if (!(flags & FOG_ONLY_DAMAGE_TO_T_PLAYER))
  {
    int protect = ANYBODY;
    if (t_player != ANYBODY)
      protect = (flags & FOG_BY_CONTROLLED_BY_PLAYER) ? 1 - t_player : t_player;
    for (int p = 0; p < 2; ++p)
      if (protect == ANYBODY || protect == p)
        for (int c = 0; c < active_cards_count[p]; ++c)
          if (in_play(p, c) && is_planeswalker(p, c))
            set_special_flags(p, c, SF_WONT_RECEIVE_DAMAGE);
  }
  int legacy = create_legacy_effect(player, card, fog_effect_legacy);
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[0].player = t_player;
  instance->targets[0].card = flags;
  instance->targets[1].player = param;
  instance->targets[1].card = get_id(player, card);
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2688700; original functions/functions.c:8982. */
int fog_special(int player, int card, int t_player, int flags)
{
  return fog_special2(player, card, t_player, flags, 0);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2688730; original functions/functions.c:8986. */
int fog_effect(int player, int card)
{
  return fog_special2(player, card, ANYBODY, FOG_COMBAT_DAMAGE_ONLY, 0);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2688770; original functions/functions.c:8990. */
int colors_shared(int clr1, int clr2) { return clr1 & clr2 & COLOR_TEST_ANY_COLORED; }

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2688780; original functions/functions.c:8994. */
int has_my_colors(int player, int card, int t_player, int t_card)
{

  int clrs[2];
  if (player != -1)
  {
    clrs[0] = get_color(player, card);
  }
  else
  {
    clrs[0] = cards_data[card].color;
  }
  if (t_player != -1)
  {
    clrs[1] = get_color(t_player, t_card);
  }
  else
  {
    clrs[1] = cards_data[t_card].color;
  }

  return colors_shared(clrs[0], clrs[1]);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2683660; original functions/functions.c:9225. */
int no_damage_this_turn(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->info_slot > 0 &&
        damage->damage_source_player == instance->targets[0].player &&
        damage->damage_source_card == instance->targets[0].card)
      get_card_instance(affected_card_controller, affected_card)->info_slot = 0;
  }
  else if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (card == affected_card && player == affected_card_controller &&
        instance->damage_target_player >= 0 && (land_can_be_played & LCBP_PENDING_DAMAGE_CARDS))
    {
      instance->damage_target_card = -1;
      instance->damage_target_player = -1;
      event_result = 1;
    }
  }
  else if (event == EVENT_AFTER_DAMAGE && instance->damage_target_player == -1 &&
           !(land_can_be_played & LCBP_PENDING_DAMAGE_CARDS))
    kill_card(player, card, KILL_EXILE);
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2688d80; original functions/functions.c:9261. */
void negate_damage_this_turn(int player, int card, int t_player, int t_card, int unused)
{
  create_targetted_legacy_effect(player, card, no_damage_this_turn, t_player, t_card);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2689800; original functions/functions.c:10171. */
void remove_summoning_sickness(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->state & STATE_SUMMON_SICK)
    instance->state &= ~(STATE_SUMMON_SICK | STATE_SUMMON_SICK_BACKUP);
}

#endif

#ifndef SHANDALAR
static int get_updated_casting_cost_with_cost_mod(int player, int card, int iid,
                                                   event_t event, int cless, int cost_mod)
{
  int cost = true_get_updated_casting_cost(player, card, iid, event, cless - cost_mod);
  return cost < 0 ? 0 : cost;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268b5f0; original functions/functions.c:11021. */
int is_monocolor_hybrid(card_ptr_t *cp)
{
  const char *c = cp->mana_cost_text;
  return c && c[0] == '|' && c[1] == '2' &&
         (c[2] == 'B' || c[2] == 'U' || c[2] == 'G' || c[2] == 'R' || c[2] == 'W');
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268bf20; original functions/functions.c:11439. */
int check_state(int player, int card, int state)
{
  return (get_card_instance(player, card)->state & state) != 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268bf50; original functions/functions.c:11447. */
int check_status(int player, int card, int status)
{
  return (get_card_instance(player, card)->token_status & status) != 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c010; original functions/functions.c:11472. */
void untap_lands(int player, int card, int num_lands)
{
  int i = 0;
  card_instance_t *instance = get_card_instance(player, card);
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  while (i++ < num_lands)
  {
    if (can_target(&td))
    {
      if (select_target(player, card, &td, "Choose a land to untap", NULL))
      {
        instance->number_of_targets = 1;
        untap_card(instance->targets[0].player, instance->targets[0].card);
      }
      else
      {
        i = num_lands;
      }
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c6e0; original functions/functions.c:11785. */
int count_creatures_by_power(int player, int pow, int mode)
{

  int count = 0;
  int result = 0;
  while (count < active_cards_count[player])
  {
    if (in_play(player, count) && is_what(player, count, TYPE_CREATURE))
    {
      int his_pow = get_power(player, count);
      if (mode == 0 && his_pow == pow)
      {
        result++;
      }
      if (mode == 1 && his_pow > pow)
      {
        result++;
      }
      if (mode == 2 && his_pow < pow)
      {
        result++;
      }
    }
    count++;
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c7a0; original functions/functions.c:11810. */
int count_creatures_by_toughness(int player, int pow, int mode)
{

  int count = 0;
  int result = 0;
  while (count < active_cards_count[player])
  {
    if (in_play(player, count) && is_what(player, count, TYPE_CREATURE))
    {
      int his_pow = get_toughness(player, count);
      if (mode == 0 && his_pow == pow)
      {
        result++;
      }
      if (mode == 1 && his_pow > pow)
      {
        result++;
      }
      if (mode == 2 && his_pow < pow)
      {
        result++;
      }
    }
    count++;
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c860; original functions/functions.c:11862. */
int real_count_colors(int clr)
{
  return num_bits_set((unsigned int)clr & COLOR_TEST_ANY_COLORED);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c8b0; original functions/functions.c:11867. */
int count_colors(int player, int card)
{

  int clr;
  if (player != -1)
  {
    clr = get_color(player, card);
  }
  else
  {
    clr = cards_data[card].color;
    if (cards_data[card].type == TYPE_LAND)
    {
      return cards_data[card].id == CARD_ID_DRYAD_ARBOR ? 1 : 0;
    }
  }
  return real_count_colors(clr);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268c950; original functions/functions.c:11882. */
int evaluate_colors(int color1, int color2)
{
  if (color2 != COLOR_TEST_COLORLESS)
  {
    int i;
    for (i = 1; i < 6; i++)
    {
      if (!(color1 & (1 << i)) && (color2 & (1 << i)))
      {
        return 0;
      }
    }
  }
  return 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268db60; original functions/functions.c:12601. */
int charge_mana_for_multi_x(int player, int color, int num_x)
{
#ifdef MANALINKEH_STATIC
  if (duel_host_choice_ready(DUEL_CHOICE_X,player)) {
    int maximum=has_mana(player,color,1)/num_x;
    if (max_x_value>=0 && max_x_value/num_x<maximum) maximum=max_x_value/num_x;
    int chosen=duel_host_choose_x(player,maximum);
    charge_mana(player,color,chosen*num_x);
    return spell_fizzled == 1 ? 0 : chosen*num_x;
  }
#endif
  int old_hack_xx = hack_xx;
  int amount;
  if (player == HUMAN && ai_is_speculating != 1)
  {
    hack_xx = num_x;
    charge_mana(player, color, -1);
    amount = x_value;
  }
  else
  {
    int mana_avail = has_mana(player, color, 1);
    if (mana_avail / num_x < 1)
    {
      spell_fizzled = 1;
      return 0;
    }
    amount = (mana_avail / num_x) * num_x;
    charge_mana(player, color, amount);
    old_hack_xx = hack_xx;
  }
  int fizzled = spell_fizzled == 1;
  hack_xx = old_hack_xx;
  return fizzled ? 0 : amount;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268dc30; original functions/functions.c:12632. */
int charge_mana_for_double_x(int player, int color)
{
  int result;
  result = charge_mana_for_multi_x(player, color, 2);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268e7a0; original functions/functions.c:13190. */
int has_dead_creature(int player)
{
  for (int p = 0; p < 2; ++p)
    if (player == p || player == ANYBODY)
      for (int *grave = graveyard_ptr_mutable[p]; *grave != -1; ++grave)
        if (cards_data[*grave].type & TYPE_CREATURE)
          return 1;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268ea80; original functions/functions.c:13776. */
int prevent_all_damage(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->damage_target_card == card &&
        damage->damage_target_player == player && damage->info_slot > 0)
      get_card_instance(affected_card_controller, affected_card)->info_slot = 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26837c0; original functions/functions.c:13791. */
static int prevent_all_damage_to_target_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->info_slot > 0 &&
        damage->damage_target_card == instance->targets[0].card &&
        damage->damage_target_player == instance->targets[0].player)
    {
      int good = instance->targets[1].player < 0 ||
                 (damage->damage_source_card == instance->targets[1].card &&
                  damage->damage_source_player == instance->targets[1].player);
      if (instance->targets[2].player > 1)
      {
        int color = in_play(damage->damage_source_player, damage->damage_source_card)
                        ? get_color(damage->damage_source_player, damage->damage_source_card)
                        : damage->initial_color;
        if (!(color & instance->targets[2].player))
          good = 0;
      }
      if (good)
        get_card_instance(affected_card_controller, affected_card)->info_slot = 0;
    }
  }
  if (instance->targets[2].player > 0 && (instance->targets[2].player & 1) &&
      eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268eaf0; original functions/functions.c:13832. */
int prevent_all_damage_to_target(int player, int card, int t_player, int t_card, int mode)
{
  int legacy;
  card_instance_t *instance;
  if (t_card == -1)
  {
    legacy = create_legacy_effect(player, card, prevent_all_damage_to_target_legacy);
    instance = get_card_instance(player, legacy);
    instance->targets[0].player = t_player;
    instance->targets[0].card = -1;
  }
  else
  {
    legacy = create_targetted_legacy_effect(player, card, prevent_all_damage_to_target_legacy,
                                            t_player, t_card);
    instance = get_card_instance(player, legacy);
  }
  instance->targets[2].player = mode;
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268edc0; original functions/functions.c:13955. */
int name_a_card(int player, int ai_player, test_definition_t *test, int mode)
{
  if (ai_is_speculating != 1 && (player != AI || (trace_mode & 2)))
    return new_card_from_list(player, mode, test);
  int selected = new_select_a_card(player, ai_player, 1, 7, 0, -1, test);
  int iid;
  if (selected != -1)
    iid = deck_ptr[ai_player][selected];
  else
  {
    selected = new_select_a_card(player, ai_player, 2, 7, 0, -1, test);
    if (selected != -1)
      iid = graveyard_ptr_mutable[ai_player][selected];
    else
    {
      selected = new_select_a_card(player, ai_player, 3, 7, 0, -1, test);
      if (selected == -1)
        return -1;
      iid = rfg_ptr[ai_player][selected];
    }
  }
  return mode & 2 ? cards_data[iid].id : iid;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2691a40; original functions/functions.c:14277. */
int count_defenders(int player)
{
  int p, c, rval = 0;
  for (p = 0; p < 2; ++p)
    if (p == player || player == ANYBODY)
      for (c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE) &&
            check_for_ability(p, c, KEYWORD_DEFENDER))
          ++rval;

  return rval;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2691ae0; original functions/functions.c:14289. */
int get_power_sum(int player)
{

  int result = 0;
  int i;
  for (i = 0; i < 2; i++)
  {
    if (i == player || player == 2)
    {
      int count = 0;
      while (count < active_cards_count[i])
      {
        if (in_play(i, count) && is_what(i, count, TYPE_CREATURE))
        {
          result += get_power(i, count);
        }
        count++;
      }
    }
  }

  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692070; original functions/functions.c:14424. */
int get_highest_power(int player)
{
  int result = 0;
  for (int p = 0; p < 2; ++p)
    if (p == player || player == ANYBODY)
      for (int c = 0; c < active_cards_count[p]; ++c)
        if (in_play(p, c) && is_what(p, c, TYPE_CREATURE))
        {
          int power = get_power(p, c);
          if (power > result)
            result = power;
        }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26925a0; original functions/functions.c:14654. */
int is_stolen(int player, int card)
{
  if (player == 0)
    return (get_card_instance(0, card)->state & STATE_OWNED_BY_OPPONENT) != 0;
  if (player == 1)
    return (get_card_instance(1, card)->state & STATE_OWNED_BY_OPPONENT) == 0;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26928c0; original functions/functions.c:14756. */
int card_generic_animated_land(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[5].player > 0)
  {
    instance->display_pic_csv_id = instance->targets[5].player;
  }
  if (instance->targets[5].card > 0)
  {
    instance->display_pic_num = instance->targets[5].card;
  }
  if (event == EVENT_POWER && affect_me(player, card) && instance->targets[7].player > 0)
  {
    set_base_pt(instance->targets[7].player - 1);
  }
  if (event == EVENT_TOUGHNESS && affect_me(player, card) && instance->targets[7].card > 0)
  {
    set_base_pt(instance->targets[7].card - 1);
  }
  if (event == EVENT_ABILITIES && affect_me(player, card) && instance->targets[8].player > 0)
  {
    event_result |= instance->targets[8].player;
  }
  if (event == EVENT_SET_COLOR && affect_me(player, card) && instance->targets[6].player > 0)
  {
    event_result = instance->targets[6].player;
  }
  if (instance->targets[8].card > 0)
  {
    special_abilities(player, card, event, instance->targets[8].card, player, card);
  }
  int result = 0;
  if (instance->targets[6].card > -1)
  {
    int (*ptFunction)(int, int, event_t) = (void *)instance->targets[6].card;
    result = ptFunction(player, card, event);
  }
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692dd0; original functions/functions.c:14958. */
int can_counter_activated_ability(int player, int card, event_t event,
                                       target_definition_t *td)
{
  int valid;
  int stack_offset;
  target_definition_t td_default;

  if (td == NULL)
  {
    td = &td_default;
    counter_activated_target_definition(player, card, td, 0);
  }
  stack_offset = ((unsigned int)event - (unsigned int)EVENT_CAST_SPELL < 2U) + 1;
  if (stack_offset <= stack_size)
  {
    valid = would_validate_arbitrary_target(td, stack_cards[stack_size - stack_offset].player,
                                                 stack_cards[stack_size - stack_offset].card);
    if (((valid != 0) && ((char)stack_data[stack_size - stack_offset].generating_event != '~')) &&
        (cards_data[stack_data[stack_size - stack_offset].internal_card_id].id != 0x388))
    {
      return 99;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692e80; original functions/functions.c:14977. */
int cast_counter_activated_ability(int player, int card, int target_pos)
{
  int32_t result;
  card_instance_t *instance;
  if (stack_size < 2)
  {
    spell_fizzled = 1;
    return 0;
  }
  instance = get_card_instance(player, card);
  result = stack_cards[stack_size - 2].card;
  instance->targets[target_pos].player = stack_cards[stack_size - 2].player;
  instance->targets[target_pos].card = result;
  instance->number_of_targets = instance->number_of_targets + '\x01';
  ai_modifier = ai_modifier + 100;
  return 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692ef0; original functions/functions.c:14996. */
int validate_counter_activated_ability(int player, int card, target_definition_t *td,
                                            int target_pos)
{
  uint16_t stack_csvid;
  int32_t saved_preferred_controller;
  card_instance_t *instance;
  target_definition_t td_default;

  instance = get_card_instance(player, card);
  if (td == NULL)
  {
    td = &td_default;
    counter_activated_target_definition(player, card, td, 0);
  }
  saved_preferred_controller = td->preferred_controller;
  td->preferred_controller = td->allowed_controller;
  if (validate_arbitrary_target(td, instance->targets[target_pos].player,
                                         instance->targets[target_pos].card) != 0)
  {
    instance =
        get_card_instance(instance->targets[target_pos].player, instance->targets[target_pos].card);
    if ((instance->state & 0x80) != 0)
    {
      stack_csvid = cards_data[stack_data[stack_size - 1].internal_card_id].id;
      td->preferred_controller = saved_preferred_controller;
      return (unsigned int)(stack_csvid != 0x388);
    }
  }
  td->preferred_controller = saved_preferred_controller;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692fc0; original functions/functions.c:15025. */
void raw_counter_activated_ability(int tgt_player, int tgt_card)
{
  card_instance_t *instance;
  instance = get_card_instance(tgt_player, tgt_card);
  instance->internal_card_id = -1;
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2692ff0; original functions/functions.c:15030. */
int resolve_counter_activated_ability(int player, int card, target_definition_t *td,
                                           int target_pos)
{
  int result;
  card_instance_t *instance;
  result = validate_counter_activated_ability(player, card, td, target_pos);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  instance =
      get_card_instance(instance->targets[target_pos].player, instance->targets[target_pos].card);
  instance->internal_card_id = -1;
  return 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693060; original functions/functions.c:15042. */
int can_counter_triggered_ability(int player, int card, event_t event,
                                       target_definition_t *td)
{
  int result;
  target_definition_t td_default;
  if (td == NULL)
  {
    td = &td_default;
    counter_triggered_target_definition(player, card, td, 0xff);
  }
  if ((((0 < stack_size) &&
        (result = would_validate_arbitrary_target(td, player, card), result != 0)) &&
       ((char)stack_data[stack_size - 1].generating_event == '~')) &&
      ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) == 0))
  {
    return 99;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x268cab0; original functions/functions.c:15433. */
int type_change_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[0].player < 0)
    return 0;
  int p = instance->targets[0].player;
  int c = instance->targets[0].card;
  int destroyed = 0;
  if (event == EVENT_CHANGE_TYPE && affect_me(p, c))
    event_result = instance->dummy3;
  if (instance->targets[9].player > 0)
  {
    if (instance->targets[9].player & 1)
      attached_is_forced_subtype(player, card, event, instance->targets[9].card);
    if (instance->targets[9].player & 2)
      attached_is_additional_subtype(player, card, event, instance->targets[9].card);
  }
  if (event == EVENT_STATIC_EFFECTS && instance->targets[1].player > 0)
  {
    if ((instance->targets[1].player & 2) && instance->targets[4].player >= 0 &&
        !in_play(instance->targets[4].player, instance->targets[4].card))
      destroyed = 1;
    if ((instance->targets[1].player & 4) &&
        !count_counters(p, c, (unsigned char)instance->targets[7].player))
      destroyed = 1;
  }
  if (event == EVENT_ABILITIES && affect_me(p, c) && instance->targets[8].player > 0)
    event_result |= instance->targets[8].player;
  if (event == EVENT_SET_COLOR && affect_me(p, c))
  {
    if (instance->targets[6].player > 0)
      event_result = instance->targets[6].player;
    if (instance->targets[6].card > 0)
      event_result |= instance->targets[6].card;
  }
  if (instance->targets[2].card == CARD_ID_GENJU_OF_THE_FIELDS)
    spirit_link_effect(p, c, event, p);
  if (instance->targets[8].card > 0)
    special_abilities(p, c, event, instance->targets[8].card, player, card);
  if (instance->targets[1].player > 0 && (instance->targets[1].player & 1) &&
      eot_trigger(player, card, event))
  {
    if (instance->targets[2].card == CARD_ID_CHIMERIC_COILS)
    {
      destroyed = 0;
      kill_card(p, c, KILL_SACRIFICE);
    }
    else
      destroyed = 1;
  }
  if (instance->targets[1].player > 0 && (instance->targets[1].player & 8) &&
      current_turn == instance->targets[4].player &&
      upkeep_trigger_mode_impl(player, card, event, RESOLVE_TRIGGER_MANDATORY, 0))
    destroyed = 1;
  if (instance->targets[1].player > 0 && (instance->targets[1].player & 0x20) &&
      event == EVENT_BEGIN_TURN && current_turn == player)
    destroyed = 1;
  if (instance->targets[1].player > 0 && (instance->targets[1].player & 0x10) &&
      end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
    destroyed = 1;
  if (destroyed)
  {
    remove_special_flags(p, c, SF_TYPE_ALREADY_CHANGED);
    kill_card(player, card, KILL_EXILE);
    recalculate_all_cards_in_play();
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693880; original functions/functions.c:15528. */
int turn_into_artifact(int player, int card, int t_player, int t_card, int mode)
{
  int legacy = -1;
  if (t_player != -1 && t_card != -1)
  {
    card_instance_t *instance = get_card_instance(t_player, t_card);
    int newtype = create_a_card_type(instance->internal_card_id);
    cards_at_7c7000[newtype]->type |= (cards_data[instance->internal_card_id].type | TYPE_ARTIFACT);
    legacy =
        create_targetted_legacy_effect(player, card, &type_change_legacy, t_player, t_card);
    card_instance_t *leg = get_card_instance(player, legacy);
    leg->dummy3 = newtype;
    leg->token_status |= STATUS_PERMANENT;
    leg->targets[1].player = mode;
  }
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694d70; original functions/functions.c:15543. */
int turn_into_creature(int player, int card, int t_player, int t_card, int mode, int pow,
                            int tou)
{

  int legacy = -1;
  if (t_player != -1 && t_card != -1)
  {
    card_instance_t *instance = get_card_instance(t_player, t_card);
    int newtype = create_a_card_type(instance->internal_card_id);
    cards_at_7c7000[newtype]->type |= (cards_data[instance->internal_card_id].type | TYPE_CREATURE);
    if (pow != 0)
    {
      cards_at_7c7000[newtype]->power = pow != -1 ? pow : get_cmc(t_player, t_card);
    }
    if (tou != 0)
    {
      cards_at_7c7000[newtype]->toughness = tou != -1 ? tou : get_cmc(t_player, t_card);
    }
    legacy = create_targetted_legacy_effect_no_repeat(player, card, &type_change_legacy,
                                                      t_player, t_card);
    card_instance_t *leg = get_card_instance(player, legacy);
    leg->dummy3 = newtype;
    leg->token_status |= STATUS_PERMANENT;
    leg->targets[1].player = mode;
  }
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694ea0; original functions/functions.c:14787. */
int real_land_animation2(int player, int card, int t_player, int t_card, int mode, int pow,
                              int tou, int key, int s_key, int clr)
{

  int legacy = turn_into_creature(player, card, t_player, t_card, mode, pow, tou);
  card_instance_t *leg = get_card_instance(player, legacy);
  leg->targets[1].player = mode;

  leg->targets[2].card = get_id(player, card);
  leg->targets[4].player = player;
  leg->targets[4].card = card;
  leg->targets[6].player = clr;
  leg->targets[8].player = key;
  leg->targets[8].card = s_key;
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695040; original functions/functions.c:15569. */
int artifact_animation(int player, int card, int t_player, int t_card, int mode, int pow,
                            int tou, int abilities, int sp_abilities)
{

  int legacy_card = turn_into_creature(player, card, t_player, t_card, mode, pow, tou);
  card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
  legacy_instance->targets[2].card = get_id(player, card);
  legacy_instance->targets[8].player = abilities;
  legacy_instance->targets[8].card = sp_abilities;
  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693940; original functions/functions.c:15586. */
int is_hardcoded_card(int player, int card)
{
  (void)player;
  (void)card;
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678c50; original functions/functions.c:15710. */
static void alternate_legacy_text_impl(int textnum, int player, int legacycard)
{
  card_instance_t *instance = get_card_instance(player, legacycard);
  if (cards_data[instance->internal_card_id].id != 902 &&
      cards_data[instance->internal_card_id].id != 903)
    backtrace_and_abort("functions/functions.c:15713\n(cards_data[instance->internal_card_id].id "
                        "== 902 || cards_data[instance->internal_card_id].id == 903) && "
                        "\"alternate_legacy_text only works on csvid 902 or 903 effects\"");
  if (instance->internal_card_id == LEGACY_EFFECT_PUMP)
    backtrace_and_abort(
        "functions/functions.c:15714\ninstance->internal_card_id != LEGACY_EFFECT_PUMP && "
        "\"alternate_legacy_text doesn't work on LEGACY_EFFECT_PUMP\"");
  instance->eot_toughness = (instance->eot_toughness & ~0x300) | textnum;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26813f0; original functions/functions.c:7317. */
void negate_combat_damage_this_turn(int player, int card, int t_player, int t_card,
                                         int alternate_legacy_text_number)
{
  int legacy = create_targetted_legacy_effect(player, card, no_combat_damage_this_turn,
                                              t_player, t_card);
  if (legacy >= 0 && alternate_legacy_text_number > 0)
    alternate_legacy_text_impl(alternate_legacy_text_number, player, legacy);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693bc0; original functions/functions.c:15710. */
int alternate_legacy_text(int textnum, int player, int legacycard)
{
  if (legacycard >= 0)
    alternate_legacy_text_impl(textnum, player, legacycard);
  return legacycard;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693c00; original functions/functions.c:15734. */
int create_card_name_legacy(int player, int card, int id)
{
  int legacy_card = create_legacy_effect_exe(player, card, LEGACY_EFFECT_ASWAN, player, card);
  card_instance_t *legacy = get_card_instance(player, legacy_card);
  legacy->info_slot = -id - 1;
  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26786e0; original functions/functions.c:15749. */
static int subtype_name_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_SET_LEGACY_EFFECT_NAME && instance->targets[0].player > -1)
  {
    scnprintf(set_legacy_effect_name_addr, 51, "%s",
              raw_get_subtype_text(instance->targets[0].player));
  }
  if (instance->targets[0].card == 2 && event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693ee0; original functions/functions.c:15760. */
int create_subtype_name_legacy(int player, int card, int subtype)
{
  int legacy_card =
      create_targetted_legacy_effect(player, card, &subtype_name_legacy, player, card);
  get_card_instance(player, legacy_card)->targets[0].player = subtype;
  get_card_instance(player, legacy_card)->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2693f40; original functions/functions.c:15767. */
int create_hardcoded_subtype_name_legacy(int player, int card, int subtype)
{
  int legacy_card = create_legacy_effect_exe(player, card, LEGACY_EFFECT_ASWAN, player, card);
  card_instance_t *legacy = get_card_instance(player, legacy_card);
  legacy->info_slot = subtype;
  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26940a0; original functions/functions.c:16233. */
int is_x_spell(int player, int card)
{
  card_data_t *data = player == -1 ? &cards_data[card] : get_card_data(player, card);
  if (data->cc[1] == 255)
    return 1;
  return check_special_flags2(player, card, SF2_X_SPELL);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695480; original functions/functions.c:16311. */
void untap_permanents_during_opponents_untap(int player, int card, type_t types,
                                                  int *where_to_store)
{
  if (current_phase == PHASE_UNTAP && current_turn == 1 - player)
  {
    if (*where_to_store != 77)
    {
      manipulate_all(player, card, player, types, MATCH, 0, 0, 0, 0, 0, 0, -1, 0, ACT_UNTAP);
      *where_to_store = 77;
    }
  }
  else if (*where_to_store == 77)
  {
    *where_to_store = 0;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695540; original functions/functions.c:16322. */
void damage_target_creature_or_player(int t_player, int t_card, int amount, int source_player,
                                           int source_card)
{
  if (t_card != -1)
    damage_creature(t_player, t_card, amount, source_player, source_card);
  else
    damage_player(t_player, amount, source_player, source_card);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695570; original functions/functions.c:16331. */
void true_transform(int player, int card)
{

  card_instance_t *instance = get_card_instance(player, card);

  int id = instance->targets[13].player;
  if (instance->targets[13].player == instance->targets[13].card)
  {
    id++;
  }
  instance->targets[12].card = get_internal_card_id_from_csv_id(id);
  instance->targets[13].card = id;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269db70; original functions/functions.c:5904. */
int manland_shared(int player, int card, event_t event, int colorless, int black, int blue,
                        int green, int red, int white)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_ACTIVATE)
  {
    set_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
    int result = granted_generic_activated_ability(player, card, player, card, event,
                                                        GAA_TYPE_CHANGE, colorless, black, blue,
                                                        green, red, white, 0, NULL, NULL);
    if (result && (get_id(player, card) != CARD_ID_NANTUKO_MONASTERY || has_threshold(player)))
    {
      remove_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
      return 1;
    }
    remove_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
  }
  else if (event == EVENT_ACTIVATE)
  {
    instance->info_slot = 0;
    int choice = 1;
    if (mana_producer(player, card, EVENT_CAN_ACTIVATE))
    {
      set_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
      choice = 0;
      if (unknown62BCEC != 0 && needed_mana_colors == 0 &&
          granted_generic_activated_ability(player, card, player, card, EVENT_CAN_ACTIVATE,
                                                 GAA_TYPE_CHANGE, colorless, black, blue, green,
                                                 red, white, 0, NULL, NULL) &&
          (get_id(player, card) != CARD_ID_NANTUKO_MONASTERY || has_threshold(player)))
        choice = do_dialog(player, player, card, -1, -1, " Generate Mana\n Animate\n Cancel", 1);
      remove_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
    }
    if (choice == 1)
    {
      set_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
      if (granted_generic_activated_ability(player, card, player, card, event, GAA_TYPE_CHANGE,
                                                 colorless, black, blue, green, red, white, 0, NULL,
                                                 NULL))
        instance->info_slot = 66;
      remove_special_flags(player, card, SF_MANA_PRODUCER_DISABLED_FOR_AI);
      return 0;
    }
    if (choice != 0)
    {
      spell_fizzled = 1;
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION && instance->info_slot == 66 &&
           instance->targets[13].player == instance->targets[13].card)
  {
    get_card_instance(player, instance->parent_card)->token_status |= STATUS_ANIMATED;
    true_transform(player, instance->parent_card);
    return 0;
  }
  return mana_producer(player, card, event);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269df80; original functions/functions.c:5966. */
int manland_animated(int player, int card, event_t event, int colorless, int black, int blue,
                          int green, int red, int white)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (get_id(player, card) != CARD_ID_STALKING_STONES_ANIMATED && eot_trigger(player, card, event))
  {
    remove_status(player, card, STATUS_ANIMATED);
    remove_special_flags(player, card, SF_TYPE_ALREADY_CHANGED);
    true_transform(player, card);
  }
  else if (event == EVENT_SET_COLOR && affect_me(player, card))
  {
    event_result = cards_data[instance->internal_card_id].color;
  }
  return manland_shared(player, card, event, colorless, black, blue, green, red, white);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269e0b0; original functions/functions.c:5979. */
int manland_normal(int player, int card, event_t event, int colorless, int black, int blue,
                        int green, int red, int white)
{
  double_faced_card(player, card, event);
  return manland_shared(player, card, event, colorless, black, blue, green, red, white);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2677a90; original functions/functions.c:16365. */
static int remove_untargettable_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) &&
      reason_for_trigger_controller == player)
  {
    if (trigger_cause_controller == instance->targets[0].player &&
        check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result |= 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        state_untargettable(trigger_cause_controller, trigger_cause, 0);
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2695e20; original functions/functions.c:16384. */
void type_uncounterable(int player, int card, event_t event, int t_player, int type,
                             test_definition_t *this_test)
{
  if (is_what(player, card, TYPE_PERMANENT) && is_humiliated(player, card, AL_STATIC))
  {
    return;
  }
  if (event == EVENT_CAST_SPELL && !affect_me(player, card) &&
      !is_what(affected_card_controller, affected_card, TYPE_LAND) && in_play(player, card))
  {
    if (t_player == affected_card_controller || t_player == ANYBODY)
    {
      if (this_test != NULL)
      {
        if (new_make_test(
                player,
                get_card_instance(affected_card_controller, affected_card)->internal_card_id, -1,
                this_test))
        {
          state_untargettable(affected_card_controller, affected_card, 1);
        }
      }
      if (type)
      {
        if (is_what(affected_card_controller, affected_card, type))
        {
          state_untargettable(affected_card_controller, affected_card, 1);
        }
      }
      if (this_test == NULL && !type)
      {
        state_untargettable(affected_card_controller, affected_card, 1);
      }
    }
  }

  if (this_dies_trigger_impl(player, card, event, RESOLVE_TRIGGER_MANDATORY, player, 0, 0))
  {
    int legacy = create_legacy_effect(player, card, &remove_untargettable_legacy);
    get_card_instance(player, legacy)->targets[0].player = player;
    get_card_instance(player, legacy)->token_status |= STATUS_INVISIBLE_FX;
  }

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) &&
      reason_for_trigger_controller == player)
  {
    if ((trigger_cause_controller == t_player || t_player == ANYBODY) &&
        check_state(trigger_cause_controller, trigger_cause, STATE_CANNOT_TARGET))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result |= 2;
      }
      else if (event == EVENT_RESOLVE_TRIGGER)
      {
        state_untargettable(trigger_cause_controller, trigger_cause, 0);
      }
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2685200; original functions/functions.c:16424. */
int my_damage_cannot_be_prevented(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int source_player =
        is_what(player, card, TYPE_EFFECT) ? instance->targets[0].player : player;
    int source_card = is_what(player, card, TYPE_EFFECT) ? instance->targets[0].card : card;
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->damage_source_player == source_player &&
        damage->damage_source_card == source_card)
      get_card_instance(affected_card_controller, affected_card)->state |= STATE_CANNOT_TARGET;
  }
  else if (event == EVENT_CLEANUP && is_what(player, card, TYPE_EFFECT))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2683470; original functions/functions.c:16446. */
int damage_cannot_be_prevented_until_eot_legacy(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card)
      get_card_instance(affected_card_controller, affected_card)->state |= STATE_CANNOT_TARGET;
  }
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696100; original functions/functions.c:16461. */
int damage_cannot_be_prevented_until_eot(int player, int card)
{
  return create_legacy_effect(player, card, damage_cannot_be_prevented_until_eot_legacy);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696130; original functions/functions.c:16465. */
int is_artifact_creature_by_internal_id(int iid, int unused1, int unused2, int unused3)
{
  return is_what(-1, iid, TYPE_CREATURE) && is_what(-1, iid, TYPE_ARTIFACT) ? 1 : 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696190; original functions/functions.c:16469. */
void global_type_change(int player, int card, event_t event, int t_player, int added_type,
                             test_definition_t *this_test, int pow, int tou, int key,
                             int subtype, int clr)
{
  if (event == EVENT_CHANGE_TYPE && in_play(player, card))
  {
    if (affected_card_controller == t_player || t_player == 2)
    {
      if (!is_what(affected_card_controller, affected_card, TYPE_EFFECT) &&
          new_make_test_in_play(affected_card_controller, affected_card, -1, this_test))
      {
        if (!(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
        {

          land_can_be_played |= LCBP_NEED_EVENT_CHANGE_TYPE_SECOND_PASS;
        }
        else
        {
          int newtype = g_card_count;
          int end = newtype + 16;

          for (; newtype < end; ++newtype)
          {
            if (cards_data[newtype].id == cards_data[event_result].id)
            {
              if (new_make_test(player, newtype, -1, this_test))
              {
                event_result = newtype;
                goto found;
              }
            }
          }

          newtype = create_a_card_type(event_result);
          if (newtype != -1)
          {
            cards_data[newtype].type |= added_type;
            if (pow == 16384)
              pow = get_cmc(affected_card_controller, affected_card);
            cards_data[newtype].power = pow;
            if (tou == 16384)
              tou = get_cmc(affected_card_controller, affected_card);
            cards_data[newtype].toughness = tou;
            cards_data[newtype].static_ability = key;
            event_result = newtype;
          }

        found:
          get_card_instance(affected_card_controller, affected_card)->token_status |=
              STATUS_ANIMATED;
          if (subtype > 0 && !has_subtype(affected_card_controller, affected_card, subtype))
          {
            add_a_subtype(affected_card_controller, affected_card, subtype);
          }
        }
      }
    }
  }

  if (event == EVENT_SET_COLOR && in_play(player, card) && clr > 0)
  {
    if (affected_card_controller == t_player || t_player == 2)
    {
      if (new_make_test_in_play(affected_card_controller, affected_card, -1, this_test))
      {
        event_result = get_sleighted_color_test(player, card, clr);
      }
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696490; original functions/functions.c:14805. */
static int global_type_animation_legacy(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (instance->targets[0].player > -1)
  {

    test_definition_t this_test;
    default_test_definition(&this_test, instance->targets[4].player);
    this_test.type_flag = instance->targets[4].card;
    this_test.subtype = instance->targets[5].player;
    this_test.subtype_flag = instance->targets[5].card;
    this_test.color = instance->targets[6].player;
    this_test.color_flag = instance->targets[6].card;
    this_test.cmc = instance->targets[7].player;
    this_test.cmc_flag = instance->targets[7].card;

    global_type_change(player, card, event, instance->targets[0].player, TYPE_CREATURE,
                            &this_test, instance->targets[1].player, instance->targets[1].card,
                            instance->targets[2].player, instance->targets[3].card,
                            instance->targets[3].player);

    if (instance->targets[0].card > -1 && (instance->targets[0].card & 1))
    {
      if (eot_trigger(player, card, event))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2694f40; original functions/functions.c:14835. */
int land_animation2(int player, int card, int t_player, int t_card, int mode, int pow, int tou,
                         int key, int s_key, int clr, test_definition_t *this_test)
{
  if (t_card != -1)
    return real_land_animation2(player, card, t_player, t_card, mode, pow, tou, key, s_key,
                                     clr);
  int legacy = create_legacy_effect(player, card, &global_type_animation_legacy);
  card_instance_t *leg = get_card_instance(player, legacy);
  leg->targets[0].player = t_player;
  leg->targets[0].card = mode;
  leg->targets[1].player = pow;
  leg->targets[1].card = tou;
  leg->targets[2].player = key;
  leg->targets[2].card = s_key;
  leg->targets[3].player = clr;
  leg->targets[3].card = -1;
  leg->targets[4].player = this_test->type;
  leg->targets[4].card = this_test->type_flag;
  leg->targets[5].player = this_test->subtype;
  leg->targets[5].card = this_test->subtype_flag;
  leg->targets[6].player = this_test->color_flag;
  leg->targets[7].card = this_test->cmc_flag;
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678d10; original functions/functions.c:16525. */
static void type_text_impl(char *buf, int maxlen, type_t type, type_text_t flags)
{

  const char *types[17] = {0};
  int num_types = 0;

  load_text(0, "TYPE");

  if (!(flags & TYPETEXT_NO_ABBREV))
  {

    if ((type & (TYPE_INSTANT | TYPE_INTERRUPT)) == (TYPE_INSTANT | TYPE_INTERRUPT))
      type &= ~TYPE_INTERRUPT;

    if ((type & (TYPE_PERMANENT | TYPE_INSTANT | TYPE_SORCERY)) ==
        (TYPE_PERMANENT | TYPE_INSTANT | TYPE_SORCERY))
    {
      type &= ~(TYPE_NONEFFECT | TARGET_TYPE_PLANESWALKER);
      types[num_types++] = text_lines[2];
    }

    if ((type & TYPE_PERMANENT) == TYPE_PERMANENT)
    {
      type &= ~(TYPE_PERMANENT | TARGET_TYPE_PLANESWALKER);
      types[num_types++] = text_lines[9];
    }
  }

#define TYPE(typ, idx)                                                                             \
  if (type & typ)                                                                                  \
  types[num_types++] = text_lines[idx]
  TYPE(TYPE_ARTIFACT, 1);
  TYPE(TYPE_CREATURE, 3);
  TYPE(TYPE_ENCHANTMENT, 5);
  TYPE(TYPE_INSTANT, 6);
  TYPE(TYPE_INTERRUPT, 7);
  TYPE(TYPE_LAND, 8);
  TYPE(TARGET_TYPE_PLANESWALKER, 12);
  TYPE(TYPE_SORCERY, 14);

  sort_ArrayConstCharPtr(types, num_types);

  TYPE(TYPE_EFFECT, 4);

  if (num_types == 0)
    types[num_types++] = text_lines[0];

  const char *non_txt = (flags & TYPETEXT_INVERT) ? "non" : "";

  if (num_types == 1)
    scnprintf(buf, maxlen, "%s%s", non_txt, types[0]);
  else
  {
    int i;
    char *p = buf;
    int end = num_types - ((flags & TYPETEXT_INVERT) ? 1 : 2);
    for (i = 0; i < end; ++i)
      p += scnprintf(p, maxlen - (p - buf), "%s%s, ", non_txt, types[i]);

    if (flags & TYPETEXT_INVERT)
      scnprintf(p, maxlen - (p - buf), "%s%s", non_txt, types[num_types - 1]);
    else
      scnprintf(p, maxlen - (p - buf), "%s or %s", types[num_types - 2], types[num_types - 1]);
  }

  return;
#undef TYPE
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26965d0; original functions/functions.c:16525. */
char *type_text(char *buf, int maxlen, type_t type, type_text_t flags)
{
  if (ai_is_speculating == 1)
    *buf = 0;
  else
    type_text_impl(buf, maxlen, type, flags);
  return buf;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696610; original functions/functions.c:16601. */
void untap_only_1_permanent_per_upkeep(int player, int card, event_t event, int t_player,
                                            int type)
{
  target_definition_t td;
  default_target_definition(player, card, &td, type);
  td.required_state = TARGET_STATE_TAPPED;
  td.illegal_abilities = 0;
  td.allowed_controller = t_player != 2 ? t_player : current_turn;
  td.preferred_controller = t_player != 2 ? t_player : current_turn;
  td.who_chooses = t_player != 2 ? t_player : current_turn;
  td.allow_cancel = 0;

  card_instance_t *instance = get_card_instance(player, card);

  upkeep_trigger_ability(player, card, event, t_player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    char prompt[200];
    if (ai_is_speculating == 1)
    {
      prompt[0] = 0;
    }
    else
    {
      char typetext[100];
      type_text_impl(typetext, 100, type, TYPETEXT_0);
      scnprintf(prompt, 200, "PROCESSING %s: Select %s to untap.",
                cards_ptr[get_id(player, card)]->full_name, typetext);
    }
    instance->number_of_targets = 0;
    if (can_target(&td) && pick_next_target_noload(&td, prompt))
    {
      instance->number_of_targets = 0;
      untap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }

  if (current_phase == PHASE_UNTAP && event == EVENT_UNTAP &&
      (current_turn == t_player || t_player == 2) &&
      is_what(affected_card_controller, affected_card, type))
  {
    card_instance_t *instance2 = get_card_instance(affected_card_controller, affected_card);
    instance2->untap_status &= ~3;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696810; original functions/functions.c:16637. */
void twiddle(int player, int card, int tgtnum)
{
  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[tgtnum].player;
  int c = instance->targets[tgtnum].card;

  load_text(0, "TWIDDLE");
  switch (DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_SMALLCARD(p, c),
                 text_lines[1], 1, p == AI ? 1 : 3, text_lines[2], 1, p == AI ? 3 : 1))
  {
  case 1:
    tap_card(p, c);
    break;

  case 2:
    untap_card(p, c);
    break;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696930; original functions/functions.c:16662. */
void ai_modifier_twiddle(int player, int card, int tgtnum)
{
  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[tgtnum].player;
  int c = instance->targets[tgtnum].card;

  if (!is_what(p, c, TYPE_CREATURE) &&
      (!(get_card_data(p, c)->extra_ability & (EA_ACT_ABILITY | EA_ACT_INTERRUPT)) ||
       !is_what(p, c, TYPE_CREATURE | TYPE_ARTIFACT | TYPE_LAND)))
    ai_modifier -= 24;

  if (!(p == AI) != !(is_tapped(p, c)) || (player == p && card == c))
    ai_modifier -= 96;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696a00; original functions/functions.c:16678. */
void remove_from_combat(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);

  instance->blocking = -1;

  if (instance->state & STATE_ATTACKING)
    instance->state |= STATE_ATTACKED;

  if (instance->state & STATE_BLOCKING)
    instance->state |= STATE_BLOCKED;

  instance->state &= ~(STATE_ATTACKING | STATE_BLOCKING);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2677b70; original functions/functions.c:16703. */
static int get_pt_for_creature_with_variable_pt(int player, int iid, event_t event)
{
  if (cards_data[iid].id == CARD_ID_GRIST_THE_HUNGER_TIDE)
    return 1;
  int curr = event == EVENT_POWER ? cards_data[iid].power : cards_data[iid].toughness;
  if ((curr > 0 && (curr & 0x4000)))
  {
    int characteristic = curr;
    curr &= ~0x4000;
    /* Null bindings are retained placeholder records in the imported EXE
       table. They have no characteristic-defining callback to invoke. */
    if (!cards_data[iid].code_pointer)
      return curr;

    int old_affected_card_controller = affected_card_controller;
    int old_affected_card = affected_card;
    int old_event_result = event_result;
    int old_raw_base_pt = raw_base_pt;
    int old_raw_characteristic_pt = raw_characteristic_pt;

    affected_card_controller = player;
    affected_card = -1;
    event_result = curr;
    raw_base_pt = curr;
    raw_characteristic_pt = characteristic;
    call_card_fn(cards_data[iid].code_pointer, NULL, player, -1, event);
    curr = event_result;

    affected_card_controller = old_affected_card_controller;
    affected_card = old_affected_card;
    event_result = old_event_result;
    raw_base_pt = old_raw_base_pt;
    raw_characteristic_pt = old_raw_characteristic_pt;
  }
  return curr;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696aa0; original functions/functions.c:16731. */
int get_base_power(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  return get_pt_for_creature_with_variable_pt(player, iid, EVENT_POWER);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696ae0; original functions/functions.c:16740. */
int get_base_power_iid(int owner, int iid)
{
  return get_pt_for_creature_with_variable_pt(owner, iid, EVENT_POWER);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696b00; original functions/functions.c:16744. */
int get_base_toughness(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid = instance->internal_card_id;
  if (iid == -1)
    iid = instance->backup_internal_card_id;
  return get_pt_for_creature_with_variable_pt(player, iid, EVENT_TOUGHNESS);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696b40; original functions/functions.c:16753. */
int get_base_toughness_iid(int owner, int iid)
{
  return get_pt_for_creature_with_variable_pt(owner, iid, EVENT_TOUGHNESS);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696e30; original functions/functions.c:16852. */
void if_a_card_would_be_put_into_graveyard_from_play_exile_it_instead(
    int player, int card, int event, int t_player, test_definition_t *test)
{
  card_instance_t *instance;
  int result;
  bool should_replace;

  if (event == 0x77)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if ((result == 0) && ((t_player == affected_card_controller || (t_player == 2))))
      {
        instance = in_play(affected_card_controller, affected_card);
        if ((instance != NULL) && ((unsigned char)(instance->kill_code - 1) < 3))
        {
          if (test == NULL)
          {
            result = is_token(affected_card_controller, affected_card);
            should_replace = result == 0;
          }
          else
          {
            result = new_make_test_in_play(affected_card_controller, affected_card, -1, test);
            should_replace = result != 0;
          }
          if (should_replace)
          {
            set_special_flags2(affected_card_controller, affected_card, 0x10000000);
            instance->kill_code = '\x04';
          }
        }
      }
    }
  }
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2696f20; original functions/functions.c:16868. */
int if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(
    int player, int card, int event, int t_player, resolve_trigger_t trigger_mode,
    test_definition_t *test)
{
  xtrigger_t condition;
  int result;
  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_REPLACE_MILL;
  condition = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    condition = xtrigger_impl_value_dont_use_directly;
  }
  if (condition == XTRIGGER_REPLACE_MILL)
  {
    if (((((card == affected_card) && (player == affected_card_controller)) &&
          (reason_for_trigger_controller == player)) &&
         (replace_milled == 0)) &&
        (((trigger_cause_controller == t_player || (t_player == 2)) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0))))
    {
      if ((test != NULL) &&
          (result = new_make_test(trigger_cause_controller,
                                       deck_ptr[trigger_cause_controller][trigger_cause], -1, test),
           result == 0))
      {
        return 0;
      }
      if (event == 0x7d)
      {
        if (trigger_mode == RESOLVE_TRIGGER_DUH)
        {
          result = duh_mode(player);
          trigger_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
        }
        event_result = event_result | trigger_mode;
      }
      return (unsigned int)(event == 0x7e);
    }
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2697040; original functions/functions.c:16892. */
int if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(
    int player, int card, int event, int t_player, resolve_trigger_t trigger_mode,
    test_definition_t *test)
{
  xtrigger_t condition;
  int result;
  enable_xtrigger_flags =
      enable_xtrigger_flags | ENABLE_XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY;
  condition = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    condition = xtrigger_impl_value_dont_use_directly;
  }
  if (condition == XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY)
  {
    if (((((card == affected_card) && (player == affected_card_controller)) &&
          (reason_for_trigger_controller == player)) &&
         (replace_milled == 0)) &&
        (((trigger_cause_controller == t_player || (t_player == 2)) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0))))
    {
      if ((test != NULL) &&
          (result = new_make_test(trigger_cause_controller, trigger_cause, -1, test),
           result == 0))
      {
        return 0;
      }
      if (event == 0x7d)
      {
        if (trigger_mode == RESOLVE_TRIGGER_DUH)
        {
          result = duh_mode(player);
          trigger_mode = (result != 0) + RESOLVE_TRIGGER_OPTIONAL;
        }
        event_result = event_result | trigger_mode;
      }
      return (unsigned int)(event == 0x7e);
    }
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2697150; original functions/functions.c:16943. */
void if_a_card_would_be_put_into_graveyard_from_anywhere_exile_it_instead(
    int player, int card, int event, int t_player, test_definition_t *test)
{
  int result;
  result = if_a_card_would_be_put_into_graveyard_from_library_do_something_instead(
      player, card, event, t_player, RESOLVE_TRIGGER_MANDATORY, test);
  if (result != 0)
  {
    rfg_top_card_of_deck(trigger_cause_controller);
    replace_milled = 1;
  }
  result =
      if_a_card_would_be_put_into_graveyard_from_anywhere_but_library_do_something_instead(
          player, card, event, t_player, RESOLVE_TRIGGER_MANDATORY, test);
  if (result != 0)
  {
    add_card_to_rfg(trigger_cause_controller, trigger_cause);
    play_sound_effect(WAV_DESTROY);
    replace_milled = 1;
  }
  if_a_card_would_be_put_into_graveyard_from_play_exile_it_instead(player, card, event,
                                                                        t_player, test);
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679080; original functions/functions.c:17071. */
static void prevent_all_my_damage_impl(int player, int card, event_t event, int flags)
{
  card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
  if (damage->internal_card_id == damage_card && damage->info_slot > 0 &&
      damage->damage_source_card == card && damage->damage_source_player == player)
  {
    int combat = 1;
    if (flags & DDBM_MUST_BE_COMBAT_DAMAGE)
      combat = (damage->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)) != 0;
    if ((!(flags & DDBM_MUST_DAMAGE_PLAYER) ||
         (damage->damage_target_card == -1 &&
          !check_special_flags(damage->damage_source_player, damage->damage_source_card,
                                    SF_ATTACKING_PWALKER))) &&
        (!(flags & DDBM_MUST_DAMAGE_OPPONENT) ||
         (damage->damage_target_card == -1 && damage->damage_target_player == 1 - player)) &&
        combat)
      get_card_instance(affected_card_controller, affected_card)->info_slot = 0;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2697740; original functions/functions.c:17071. */
void prevent_all_my_damage(int player, int card, event_t event, int flags)
{
  if (event == EVENT_PREVENT_DAMAGE)
    prevent_all_my_damage_impl(player, card, EVENT_PREVENT_DAMAGE, flags);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2679180; original functions/functions.c:17096. */
static int prevent_all_damage_dealt_by_target_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0 && event == EVENT_PREVENT_DAMAGE)
  {
    prevent_all_my_damage_impl(instance->damage_target_player, instance->damage_target_card,
        EVENT_PREVENT_DAMAGE, instance->targets[1].player);
    return 0;
  }
  if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2697760; original functions/functions.c:17110. */
int prevent_all_damage_dealt_by_target(int player, int card, int t_player, int t_card,
                                            int flags)
{
  int legacy = create_targetted_legacy_effect(
      player, card, prevent_all_damage_dealt_by_target_legacy, t_player, t_card);
  get_card_instance(player, legacy)->targets[1].player = flags;
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269f2e0; original functions/functions.c:18129. */
card_instance_t *has_activation_on_stack(int player, int card)
{
  for (int c = 0; c < active_cards_count[player]; ++c)
  {
    card_instance_t *instance = get_card_instance(player, c);
    if (instance && instance->internal_card_id == activation_card &&
        instance->parent_card == card && instance->parent_controller == player)
      return instance;
  }
  return NULL;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269f930; original functions/functions.c:18357. */
int get_card_name(int player, int card)
{
  int csvid = player == -1 ? cards_data[card].id : get_id(player, card);
  if (player != -1)
  {

    if (cards_data[get_card_instance(player, card)->original_internal_card_id].id ==
        CARD_ID_SAKASHIMA_THE_IMPOSTOR)
    {
      return CARD_ID_SAKASHIMA_THE_IMPOSTOR;
    }
    if (cards_data[get_card_instance(player, card)->original_internal_card_id].id ==
        CARD_ID_LAZAV_DIMIR_MASTERMIND)
    {
      return CARD_ID_LAZAV_DIMIR_MASTERMIND;
    }
    if (get_id(player, card) == CARD_ID_DARKSTEEL_MUTATION_INSECT)
    {
      return cards_data[get_card_instance(player, card)->targets[1].card].id;
    }

    if (get_id(player, card) == CARD_ID_WASTES)
    {
      int iid = get_card_instance(player, card)->targets[1].card;
      return iid == -1 ? CARD_ID_WASTES : cards_data[iid].id;
    }
  }
  if (csvid == CARD_ID_FACE_DOWN_CREATURE)
  {
    return -1;
  }
  return csvid;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269fa60; original functions/functions.c:18382. */
int has_same_name(int player, int card, int player2, int card2)
{
  int id1 = get_card_name(player, card);
  int id2 = get_card_name(player2, card2);
  if (id1 == -1 || id2 == -1)
  {
    return 0;
  }

  if (id1 == CARD_ID_RIPTIDE_REPLICATOR_TOKEN && id2 == CARD_ID_RIPTIDE_REPLICATOR_TOKEN)
  {
    if (get_card_instance(player, card)->targets[1].card != -1 &&
        get_card_instance(player2, card2)->targets[1].card != -1 &&
        get_card_instance(player, card)->targets[1].card ==
            get_card_instance(player2, card2)->targets[1].card)
    {
      return 1;
    }
    return 0;
  }
  if (id1 == id2)
  {
    return 1;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269fb40; original functions/functions.c:18408. */
void damage_creature_and_all_the_others_with_the_same_name(int player, int card, int t_player,
                                                                int t_card, int dmg)
{
  int id = get_card_name(t_player, t_card);
  if (id != -1)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_CREATURE);
    this_test.id = id;
    new_damage_all(player, card, ANYBODY, dmg, 0, &this_test);
  }
  else
    damage_creature(t_player, t_card, dmg, player, card);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269fbf0; original functions/functions.c:18421. */
int tutor_card_with_the_same_name(int player, int card, int who_tutors, int t_player,
                                       int tutor_from, int tutor_to, int must_tutor)
{
  int id = get_card_name(player, card);

  if (id == CARD_ID_SPLINTER_TOKEN)
  {
    id = CARD_ID_SPLINTER;
  }
  char msg[100];
  if (id == -1)
  {
    scnprintf(msg, 100,
              "You won't be able to tutor any card as you're searching for a card with no name.");
  }
  else
  {
    scnprintf(msg, 100, "Select a card named %s", cards_ptr[id]->name);
  }
  test_definition_t this_test;
  new_default_test_definition(&this_test, TYPE_ANY, msg);
  this_test.id = id;
  int result = new_global_tutor(who_tutors, t_player, tutor_from, tutor_to, must_tutor,
                                     AI_FIRST_FOUND, &this_test);
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x269fcf0; original functions/functions.c:18441. */
int count_permanents_by_name(int player, type_t type, int csvid)
{
  if (csvid == -1)
    return 0;
  int result = 0;
  int p = current_turn;
  for (int c = active_cards_count[p] - 1; c >= 0; --c)
    if ((p == player || player == ANYBODY) && in_play(p, c) && is_what(p, c, type))
      result += get_card_name(p, c) == csvid;
  p = 1 - current_turn;
  for (int c = active_cards_count[p] - 1; c >= 0; --c)
    if ((player == ANYBODY || p == player) && in_play(p, c) && is_what(p, c, type))
      result += get_card_name(p, c) == csvid;
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a3f20; original functions/functions.c:18701. */
int prevent_damage(int damage_player, int damage_card_index, int amount)
{
  card_instance_t *damage = get_card_instance(damage_player, damage_card_index);
  int original = damage->info_slot;
  if (amount != -1 && amount < original)
  {
    damage->info_slot = original - amount;
    return amount;
  }
  damage->info_slot = 0;
  return original;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a3f70; original functions/functions.c:8183. */
static int effect_prevent_the_next_n_damage(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card &&
        damage->damage_target_player == instance->targets[0].player &&
        damage->damage_target_card == instance->targets[0].card && damage->info_slot > 0 &&
        !(get_card_instance(affected_card_controller, affected_card)->state & STATE_CANNOT_TARGET))
    {
      int amount = damage->info_slot;
      if (!(instance->targets[2].player & PREVENT_INFINITE) && instance->counter_toughness < amount)
        amount = instance->counter_toughness;
      if (amount > 0)
      {
        prevent_damage(affected_card_controller, affected_card, amount);
        if (!(instance->targets[2].player & PREVENT_INFINITE))
          instance->counter_toughness -= amount;
        if ((instance->targets[2].player & PREVENT_ADD_1_1_COUNTER) &&
            damage->damage_target_card >= 0)
          add_counters(player, card, damage->damage_target_player,
                                   damage->damage_target_card, COUNTER_P1_P1, amount);
        if (instance->targets[2].player & PREVENT_GAIN_LIFE)
          gain_life_impl(player, amount, 0);
        if ((instance->targets[2].player & PREVENT_REDIRECT) && instance->targets[1].player >= 0 &&
            (instance->targets[1].card == -1 ||
             in_play(instance->targets[1].player, instance->targets[1].card)))
          damage_creature(instance->targets[1].player, instance->targets[1].card, amount,
                          damage->damage_source_player, damage->damage_source_card);
      }
      if (instance->counter_toughness <= 0 && !(instance->targets[2].player & PREVENT_INFINITE))
        kill_card(player, card, KILL_EXILE);
      else
      {
        unsigned char *low = (unsigned char *)&instance->untap_status;
        unsigned char value = *low & 0xfc;
        value = value < 0xfc ? value + 4 : 0;
        *low = (*low & 3) | value;
      }
    }
  }
  else if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    card_instance_t *instance = get_card_instance(player, card);
    snprintf(set_legacy_effect_name_addr, 51, "%s (%d)",
             cards_ptr[instance->display_pic_csv_id]->name, (int)instance->counter_toughness);
  }
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2687320; original functions/functions.c:8269. */
void prevent_the_next_n_damage(int player, int card, int t_player, int t_card, int amount,
                                    prevent_t flags, int redir_to_player, int redir_to_card)
{
  if (amount <= 0 && !(flags & PREVENT_INFINITE))
    return;
  int legacy;
  int opponent = 0;
  if (t_card != -1)
    legacy = create_targetted_legacy_effect(player, card, effect_prevent_the_next_n_damage,
                                            t_player, t_card);
  else if (player == t_player)
    legacy = create_legacy_effect(player, card, effect_prevent_the_next_n_damage);
  else
  {
    legacy = create_legacy_effect_for_opponent(player, card, effect_prevent_the_next_n_damage,
                                               -1, -1);
    player = 1 - player;
    opponent = 1;
  }
  if (legacy == -1)
    return;
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[0].player = t_player;
  instance->targets[0].card = t_card;
  instance->number_of_targets = 1;
  if (opponent)
    instance->state ^= STATE_OWNED_BY_OPPONENT;
  if (flags & PREVENT_REDIRECT)
  {
    if ((unsigned int)redir_to_player < 2U && (unsigned int)redir_to_card + 1U < 151U)
    {
      instance->number_of_targets = 2;
      instance->targets[1].player = redir_to_player;
      instance->targets[1].card = redir_to_card;
    }
    else
      flags &= ~PREVENT_REDIRECT;
  }
  instance->targets[2].player = flags;
  if (!(flags & PREVENT_INFINITE))
  {
    instance->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
    instance->counter_toughness = (int16_t)amount;
  }
  if (land_can_be_played & LCBP_DAMAGE_PREVENTION)
    for (int p = 0; p < 2; ++p)
      for (int c = 0; c < active_cards_count[p]; ++c)
      {
        card_instance_t *damage = in_play(p, c);
        if (damage && damage->internal_card_id == damage_card && !(damage->state & STATE_TAPPED))
        {
          dispatch_event_arbitrary_to_one_card(player, legacy, EVENT_PREVENT_DAMAGE, p, c, 1 - p,
                                               -1);
          if (instance->internal_card_id == -1)
            return;
        }
      }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a4230; original functions/functions.c:15594. */
int prevent_damage_until_eot_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_PREVENT_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    int damage_player = affected_card_controller;
    int damage_card_index = affected_card;
    if (damage->internal_card_id == damage_card && damage->info_slot > 0 &&
        damage->damage_target_player == instance->targets[0].player &&
        damage->damage_target_card == instance->targets[0].card)
    {
      int amount = damage->info_slot;
      int remaining = instance->targets[1].player;
      if (remaining != -1 && remaining < amount)
      {
        prevent_damage(affected_card_controller, affected_card, remaining);
        instance->targets[1].player = 0;
      }
      else
      {
        if (remaining != -1)
        {
          instance->targets[1].player = remaining - amount;
          damage = get_card_instance(damage_player, damage_card_index);
        }
        damage->info_slot = 0;
      }
    }
  }
  if (eot_trigger(player, card, event))
    kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a4350; original functions/functions.c:15628. */
int prevent_damage_until_eot(int player, int card, int t_player, int t_card, int maximum)
{
  prevent_damage(t_player, t_card, maximum);
  if (maximum != -1 && maximum <= 0)
    return -1;
  card_instance_t *damage = get_card_instance(t_player, t_card);
  int legacy = create_legacy_effect(player, card, prevent_damage_until_eot_legacy);
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[0].player = damage->damage_source_player;
  instance->targets[0].card = damage->damage_source_card;
  instance->targets[1].player = maximum;
  instance->targets[2].card = get_id(player, card);
  return legacy;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a5250; original functions/functions.c:19023. */
int ai_name_a_random_csvid(int player, int card, int t_player, type_t type,
                                test_definition_t *test)
{
  int csvid = CARD_ID_LIGHTNING_BOLT;
  for (int rounds = 0; rounds < 10; ++rounds)
  {
    int count = internal_rand(count_deck(t_player));
    int iid = deck_ptr[t_player][count];
    if (iid != -1 &&
        (type ? is_what(-1, iid, type) : test && new_make_test(t_player, iid, -1, test)))
    {
      csvid = cards_data[deck_ptr[t_player][count]].id;
      break;
    }
  }
  char buffer[100];
  scnprintf(buffer, 100, "Opponent named %s", cards_ptr[csvid]->name);
  do_dialog(1 - player, player, card, -1, -1, buffer, 0);
  return csvid;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a6340; original functions/functions.c:19302. */
void ai_modifier_for_act_of_treason(int player, int card)
{

  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[0].player > -1)
  {
    if (is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE))
    {
      int pow = get_power(instance->targets[0].player, instance->targets[0].card);
      if (instance->targets[0].player == player)
      {
        int mod = -(pow * 8);
        if (check_state(instance->targets[0].player, instance->targets[0].card, STATE_SUMMONSICK_NOATTACK) &&
            current_phase < PHASE_DECLARE_ATTACKERS && current_turn == player)
        {
          mod = pow * 8;
        }
        if (is_tapped(instance->targets[0].player, instance->targets[0].card) &&
            current_phase < PHASE_DECLARE_BLOCKERS && current_turn == 1 - player)
        {
          mod = pow * 8;
        }
        ai_modifier += mod;
      }
      else
      {
        int mod = pow * 8;
        if ((get_card_instance(instance->targets[0].player, instance->targets[0].card)
                 ->regen_status &
             KEYWORD_DEFENDER) &&
            check_for_special_ability(instance->targets[0].player, instance->targets[0].card,
                                           SP_KEYWORD_CANNOT_BLOCK))
        {
          mod = -(pow * 8);
        }
        ai_modifier += mod;
      }
    }
    else
    {
      int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
      if (instance->targets[0].player == player)
      {
        ai_modifier +=
            (is_tapped(instance->targets[0].player, instance->targets[0].card) ? (cmc * 8)
                                                                               : -(cmc * 8));
      }
      else
      {
        ai_modifier += (cmc * 8);
      }
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a6500; original functions/functions.c:19277. */
int aot_like_spell(int player, int card, event_t event, target_definition_t *td,
                        const char *msg, int additional_flags)
{
  card_instance_t *instance = get_card_instance(player, card);
  int flags = GS_CAN_TARGET | additional_flags;
  if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CHANGE_TARGET)
  {
    generic_spell(player, card, event, flags, td, msg, 1, NULL);
    if (event == EVENT_CHANGE_TARGET)
      td->who_chooses = attacking_card_controller;
    if (spell_fizzled != 1)
      ai_modifier_for_act_of_treason(player, card);
    return 0;
  }
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(td))
      effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, flags, td, msg, 1, NULL);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a6a90; original functions/functions.c:19400. */
void ai_modifier_for_targeted_card_drawing(int player, int card, int target_pos,
                                                int cards_to_draw)
{
  card_instance_t *instance = get_card_instance(player, card);
  int targeted_player = instance->targets[target_pos].player;
  int damage_from_triggers = 0;
  CYCLE_ALL_CARDS(p, c, {
    if (in_play(p, c) && !is_humiliated(p, c, AL_TRIG))
    {
      int csvid = get_id(p, c);
      switch (csvid)
      {
      case CARD_ID_OB_NIXILIS_REIGNITED_EMBLEM:
      {
        if (p == targeted_player)
        {
          damage_from_triggers += 2;
        }
        break;
      }

      case CARD_ID_PHYREXIAN_TYRANNY:
        damage_from_triggers += 2;
        break;

      case CARD_ID_SPITEFUL_VISIONS:
        damage_from_triggers++;
        break;

      case CARD_ID_UNDERWORLD_DREAMS:
      case CARD_ID_FATE_UNRAVELER:
      case CARD_ID_NEKUSAR_THE_MINDRAZER:
      {
        damage_from_triggers += (p != targeted_player ? 1 : 0);
      }
      }
    }
  });
  if (targeted_player == player)
  {
    if (life[player] - (damage_from_triggers * cards_to_draw) < 6)
    {
      ai_modifier -= 96;
    }
    else
    {
      int amount = get_total_available_lands(player);
      ai_modifier += (amount < 3 || amount >= 6 ? 48 : -48);
    }
  }
  if (targeted_player == 1 - player)
  {
    if (life[player] - (damage_from_triggers * cards_to_draw) <= 0)
    {
      ai_modifier += 96;
    }
    else
    {
      ai_modifier -= 96;
    }
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a6f40; original functions/functions.c:19479. */
void ai_modifier_for_haste_granting_effects(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int mod = -48;
  if (check_state(instance->targets[0].player, instance->targets[0].card, STATE_SUMMONSICK_BOTH))
  {
    mod = 0;
    if (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS)
    {
      mod += 48;
    }
    int iid =
        get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id;
    if (cards_data[iid].extra_ability & EA_ACT_ABILITY)
    {
      mod += 24;
    }
  }
  ai_modifier += mod;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678770; original functions/functions.c:19495. */
static int legacy_name_with_rfg_check(int player, int card, event_t event)
{

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->targets[1].card > -1)
    {
      scnprintf(set_legacy_effect_name_addr, 51, "%s",
                cards_ptr[cards_data[instance->targets[1].card].id]->name);
    }
  }

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->targets[1].card > -1 && instance->targets[1].player > -1)
    {
      if (!check_rfg(instance->targets[1].player, cards_data[instance->targets[1].card].id))
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a6fe0; original functions/functions.c:19518. */
int create_legacy_name_with_rfg_check(int player, int card, int owner, int iid)
{

  int legacy_card =
      create_targetted_legacy_effect(player, card, &legacy_name_with_rfg_check, player, card);
  get_card_instance(player, legacy_card)->targets[1].player = owner;
  get_card_instance(player, legacy_card)->targets[1].card = iid;
  get_card_instance(player, legacy_card)->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
  return legacy_card;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a7050; original functions/functions.c:19528. */
void ai_modifier_for_land_untappers(int player, int card, int target_pos)
{
  card_instance_t *instance = get_card_instance(player, card);
  int iid =
      get_card_instance(instance->targets[target_pos].player, instance->targets[target_pos].card)
          ->internal_card_id;
  ai_modifier +=
      (is_tapped(instance->targets[target_pos].player, instance->targets[target_pos].card)
           ? cards_ptr[cards_data[iid].id]->ai_base_value
           : -48);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2678530; original functions/functions.c:19535. */
static const char *has_activated_ability(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  int iid = get_card_instance(player, card)->internal_card_id;
  return has_an_activated_ability(iid) ? NULL : "AI helper function";
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a70d0; original functions/functions.c:19544. */
int select_permanent_to_disable(int player, int card, target_definition_t *td,
                                     const char *prompt, test_definition_t *test)
{
  td->allow_cancel = 0;
  td->illegal_abilities = 0;
  if (IS_AI(player))
  {
    td->special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td->extra = target_predicate_extra(has_activated_ability);
  }

  card_instance_t *instance = get_card_instance(player, card);

  int choice = 1;
  if (IS_AI(player))
  {
    state_untargettable(player, card, 1);
  }
  if (can_target(td))
  {
    choice = do_dialog(player, player, card, -1, -1,
                       " Select a card in play\n Choose a card from a list", 0);
  }
  int csvid = 0;
  if (choice == 0)
  {
    if (new_pick_target(td, prompt, 0, GS_LITERAL_PROMPT))
    {
      csvid = get_card_name(instance->targets[0].player, instance->targets[0].card);
    }
  }
  else
  {
    if (IS_AI(player))
    {
      csvid = ai_name_a_random_csvid(player, card, 1 - player, 0, test);
    }
    else
    {
      csvid = new_card_from_list(player, 3, test);
    }
  }
  if (IS_AI(player))
  {
    state_untargettable(player, card, 0);
  }
  return csvid;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a7330; original functions/functions.c:19582. */
void activated_abilities_disabler_enters_bf(int player, int card, int action)
{
  int csvid = get_card_instance(player, card)->info_slot;
  if (csvid > 0)
    create_card_name_legacy(player, card, csvid);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26a7370; original functions/functions.c:19590. */
int activated_abilities_disabler(int player, int card, event_t event, int action)
{
  if (event == EVENT_ABILITIES && !affect_me(player, card) &&
      !is_humiliated(player, card, AL_STATIC))
  {
    int csvid = get_card_instance(player, card)->info_slot;
    if (csvid > 0 && get_id(affected_card_controller, affected_card) ==
                         get_card_instance(player, card)->info_slot)
      action_on_card(player, card, affected_card_controller, affected_card, action);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
/* ManalinkEh 2022: 0x26a7450; original functions/functions.c:19604. */
void ai_mod_for_tap_effects(int player, int card, int target_pos)
{

  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[target_pos].player;
  int c = instance->targets[target_pos].card;
  int mod = 0;
  if (is_tapped(p, c))
  {
    ai_modifier -= 96;
    return;
  }
  if (current_turn == player)
  {
    if (is_what(p, c, TYPE_CREATURE))
    {
      if (current_phase < PHASE_DECLARE_BLOCKERS)
      {
        int pow = get_power(p, c);
        int mul = 1;
        if (check_for_ability(p, c, KEYWORD_INFECT | KEYWORD_FIRST_STRIKE | KEYWORD_DOUBLE_STRIKE))
        {
          mul++;
        }
        if (check_for_special_ability(p, c, SP_KEYWORD_DEATHTOUCH))
        {
          mul++;
        }
        mod += (!check_for_special_ability(p, c, SP_KEYWORD_CANNOT_BLOCK) ? (pow * mul) : -24);
      }
      else
      {
        mod -= 96;
      }
    }
    int iid = get_card_instance(p, c)->internal_card_id;
    if (cards_data[iid].extra_ability & EA_ACT_ABILITY)
    {
      mod += (get_cmc(p, c) * 4);
    }
  }
  if (current_turn == 1 - player)
  {
    int mul = 2;
    if (is_what(p, c, TYPE_CREATURE))
    {
      int pow = get_power(p, c);
      if (current_phase < PHASE_DECLARE_ATTACKERS)
      {
        mul = 3;
        if (check_for_ability(p, c, KEYWORD_FLYING))
        {
          mul++;
        }
        if (check_for_special_ability(p, c, SP_KEYWORD_SHADOW | SP_KEYWORD_UNBLOCKABLE))
        {
          mul++;
        }
        mod += (!check_for_ability(p, c, KEYWORD_DEFENDER) ? (pow * mul) : -24);
      }
      else
      {
        if (check_for_ability(p, c, KEYWORD_FLYING))
        {
          mul++;
        }
        mod += (!check_for_special_ability(p, c, SP_KEYWORD_CANNOT_BLOCK) ? (pow * mul) : -24);
      }
    }
    int iid = get_card_instance(p, c)->internal_card_id;
    if (cards_data[iid].extra_ability & EA_ACT_ABILITY)
    {
      mod += (get_cmc(p, c) * 4);
    }
  }
  ai_modifier += mod;
}

/* ManalinkEh 2022: 0x26a7700; original functions/functions.c:19666. */
void ai_mod_for_untap_effects(int player, int card, int target_pos, int total_mana_spent)
{

  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->targets[target_pos].player;
  int c = instance->targets[target_pos].card;
  int pow_mod = current_turn == player ? 8 : 12;
  int ab_mod = current_turn == player ? 4 : 8;
  int csvid = get_id(p, c);
  int mod = 0;
  if (!is_tapped(p, c))
  {
    mod -= 48;
  }
  if (is_what(p, c, TYPE_CREATURE))
  {
    mod += (get_power(p, c) * pow_mod);
    int iid = get_card_instance(p, c)->internal_card_id;
    if ((cards_data[iid].extra_ability & EA_ACT_ABILITY) &&
        can_use_activated_abilities(p, c) && !is_sick(p, c))
    {
      mod += ((get_cmc(p, c) - total_mana_spent) * ab_mod);
    }
  }
  else
  {
    if (can_use_activated_abilities(p, c) && !is_animated_and_sick(p, c))
    {
      switch (csvid)
      {
      case CARD_ID_TOLARIAN_ACADEMY:
      {
        mod += ((count_subtype(p, TYPE_ARTIFACT, -1) - total_mana_spent) * (ab_mod * 2));
        break;
      }
      case CARD_ID_SERRAS_SANCTUM:
      {
        mod += ((count_subtype(p, TYPE_ENCHANTMENT, -1) - total_mana_spent) * (ab_mod * 2));
        break;
      }
      case CARD_ID_GAEAS_CRADLE:
      {
        mod += ((count_subtype(p, TYPE_CREATURE, -1) - total_mana_spent) * (ab_mod * 2));
        break;
      }
      case CARD_ID_GRIM_MONOLITH:
      case CARD_ID_MANA_VAULT:
      case CARD_ID_MISHRAS_WORKSHOP:
      {
        mod += ((3 - total_mana_spent) * (ab_mod * 2));
        break;
      }
      case CARD_ID_SOL_RING:
      {
        mod += ((2 - total_mana_spent) * (ab_mod * 2));
        break;
      }
      default:
      {
        mod += ((get_cmc(p, c) - total_mana_spent) * ab_mod);
        break;
      }
      }
    }
  }
  ai_modifier += (p == player ? mod : -mod);
}

static void disable_legend_rule_impl(int t_player)
{
  if (((unsigned int)t_player & 0xfffffffdU) == 0)
    player_bits[0] = (player_bits_t)((uint32_t)player_bits[0] | PB_LEGEND_RULE_DISABLED);
  if ((unsigned int)t_player - 1U < 2)
    player_bits[1] = (player_bits_t)((uint32_t)player_bits[1] | PB_LEGEND_RULE_DISABLED);
}
#endif

/* SDL host helper retained during the 2022 migration. */
int legacy_effect_custom(int player, int card, event_t event){
	card_instance_t* instance = get_card_instance(player, card);
	CardFunction handler = (CardFunction)(uintptr_t)(uint32_t)instance->info_slot;
	return handler ? call_card_fn(handler, instance, player, card, event) : 0;
}

/* Check for another card of the requested type in this player's hand. */
int check_hand_for_type(int player, int card, type_t type)
{
  for (int c = 0; c < active_cards_count[player]; ++c)
    if (c != card && in_hand(player, c) && is_what(player, c, type))
      return 1;
  return 0;
}
