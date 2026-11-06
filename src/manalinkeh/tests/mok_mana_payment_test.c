/* Run the real payment loop with scripted mouse actions and mana sources. */
#include "manalink.h"
#include "magic/src/duel_host.h"

int g_affected_card_controller, g_affected_card, g_duel_ai_mode_state;
int g_recorded_action_player, g_land_can_be_played, g_max_x_value, g_x_value;
int g_mana_charge[8], g_raw_mana_available[2][8], g_special_mana_pool[2][8];
int g_spell_fizzled, g_target_player_choice, g_target_selection_status_code;
int g_stop_phase, g_stop_phase_player, g_recorded_action_controller, g_recorded_action_phase;
int g_stack_size, g_stack_proxy_internal_card_id = 4, g_required_mana_color_mask;
int g_mana_color_conversions[2][11];
target_t global_stack_cards[32];
card_instance_t global_card_instances[2][151];
card_data_t global_cards_data[16];
uint32_t card_flags[2];
int unknown62BCEC, unknown_charge_mana_004d7cb0, trace_mode, mana_paid[7];
int tapped_for_mana_color;
int (*charge_mana_addr_of_pre_mana_pool)[8], (*charge_mana_addr_of_pay_mana_xbugrw)[8];
int *charge_mana_addr_of_x_value, *charge_mana_addr_of_max_x_value;
char global_all_purpose_buffer[1024];
char g_text_lines[300][300];
char gs_prompt_specialfephase_7[1];
static int failures, selections, autotaps, activations, pushes;
static int late_double_click, manual_source, cancel_payment;

static void check(int value, const char *message)
{
  if (!value) { fprintf(stderr, "%s\n", message); ++failures; }
}
card_instance_t *get_card_instance(int player, int card)
{ return &global_card_instances[player][card]; }
int has_mana(int player, color_t color, int amount) { return 0; }
int pay_mana_maximally_satisfied(int *cost, int x, int max_x)
{ for (int c=0;c<7;++c) if(cost[c]>0) return 0; return 1; }
static void spend_pool(int player, int *paid, int *total, int generic)
{
  for (int c=0;c<7;++c)
    while (g_raw_mana_available[player][c]>0 && g_mana_charge[generic ? 0 : c]>0) {
      --g_raw_mana_available[player][c]; --g_mana_charge[generic ? 0 : c];
      ++paid[c]; ++*total;
    }
}
void auto_pay_colored_mana(int p,int *a,int *n,int limit) { spend_pool(p,a,n,0); }
void auto_pay_generic_mana(int p,int *a,int *n,int limit,int *x,int max_x) { spend_pool(p,a,n,1); }
void auto_pay_variable_mana(int p,int *a,int *n,int limit,int *x,int max_x) {}
int activate_mana_sources_for_payment(int p,int *a,int *n,int flags,int extra)
{
  if (!pay_mana_maximally_satisfied(g_mana_charge,0,0)) {
    ++autotaps;
    for (int c=0;c<7;++c)
      if (g_mana_charge[c]>0) g_raw_mana_available[p][c]+=g_mana_charge[c];
    spend_pool(p,a,n,0);
  }
  return 1;
}
intptr_t get_setting(int setting) { return 0; }
int copy_mana_pool_to_display(void) { return 0; }
void format_manacost_into_global_allpurpose_buffer(int ignored,int *cost,int x,int max_x) {}
int select_card_for_action(int p,int owner,int chooser,int type,int color,char *prompt,int mode)
{
  ++selections;
  if (selections>1 || cancel_payment) {
    g_target_player_choice=-1; g_target_selection_status_code=-2; return -1;
  }
  g_target_player_choice=p;
  g_recorded_action_player=late_double_click;
  return manual_source ? 6 : 5;
}
int duel_host_choice_ready(DuelChoiceKind kind,int player) { return 0; }
int duel_host_choose_x(int player,int max_x) { return 0; }
int can_pay_life(int p,int n) { return 0; }
int how_much_life_can_i_pay(int p) { return 0; }
int choose_a_number(int p,const char *prompt,int initial) { return 0; }
int internal_rand(int n) { return 0; }
int shandalar_record_ai_action_selection(void) { return 0; }
int shandalar_replay_ai_action_selection(void) { return 0; }
int g_ai_recorded_choice, g_ai_modifier, charge_mana_called_from_pay_phyrexian_mana;
void record_ai_action_selection(void) {}
void replay_ai_action_selection(void) {}
int get_required_mana_color_mask(int *cost) { return 0; }
void manalinkeh_hostabi_TENTATIVE_reassess_all_cards(void) {}
int put_card_on_stack2(int p,int c) { ++pushes; return 0; }
int dispatch_event_to_single_card(int p,int c,event_t e,int ap,int ac) { return 1; }
void put_card_or_activation_onto_stack(int p,int c,event_t e,int a,int b) { ++pushes; }
void event_activate_then_duplicate_into_stack(int p,int c,int e,int ap,int ac)
{ ++activations; g_raw_mana_available[p][0]+=2; }
int obliterate_top_card_of_stack(void) { return 0; }
int dispatch_event(int p,int c,event_t e) { return 0; }
int dispatch_trigger2(int p,trigger_t t,const char *s,int a,int b,int c) { return 0; }
int play_sound_effect(wav_t sound) { return 0; }
int resolve_top_card_on_stack(void) { return 0; }
int get_mana_payment_amount(int *cost,int color,int *pool,int source,int dbl,int max_x,int x)
{ return 0; }
void spend_mana_for_cost(int *cost,int c,int n,int *x,int max,int p,color_t src,int *a,int *total) {}
unsigned int num_bits_set(unsigned int v) { unsigned int n=0;while(v){n+=v&1;v>>=1;}return n; }
color_t single_color_test_bit_to_color_t(color_test_t c) { return COLOR_COLORLESS; }
int load_text(const char *file,const char *section) { return 0; }
int shandalar_choose_a_color(int p,const char *s,int names,int initial,int colors) { return 0; }
int refund_paid_mana(int *paid) { return 0; }
int produce_mana(int p,int c,int n) { g_raw_mana_available[p][c]+=n;return 0; }
int lose_life(int p,int n) { return 0; }

static void reset(void)
{
  memset(g_mana_charge,0,sizeof g_mana_charge);
  memset(g_raw_mana_available,0,sizeof g_raw_mana_available);
  memset(global_card_instances,0,sizeof global_card_instances);
  memset(global_cards_data,0,sizeof global_cards_data);
  selections=autotaps=activations=pushes=0;
  late_double_click=manual_source=cancel_payment=0;
  g_recorded_action_player=g_spell_fizzled=g_land_can_be_played=0;
  g_duel_ai_mode_state=0;g_max_x_value=-1;g_x_value=0;
  g_stop_phase=g_stop_phase_player=-1;
  g_affected_card_controller=1;g_affected_card=12;
  g_mana_color_conversions[0][0]=-1;
  g_stack_size=1;global_stack_cards[0]=(target_t){0,5};
  global_card_instances[0][5].internal_card_id=5;
  global_cards_data[5].type=TYPE_SORCERY;
  global_card_instances[0][6].internal_card_id=6;
  global_card_instances[0][6].state=STATE_IN_PLAY;
  global_cards_data[6].type=TYPE_LAND;
  global_cards_data[6].extra_ability=EA_MANA_SOURCE;
}
static void check_paid(const char *message)
{
  check(charge_mana(0,COLOR_COLORLESS,2)==2 && !g_spell_fizzled,message);
  check(g_land_can_be_played==0 && g_affected_card_controller==1 && g_affected_card==12,
        "Payment must restore its charging flag and affected-card context");
}
int main(void)
{
  reset();g_recorded_action_player=1;
  check_paid("A double-click already recorded by the UI must request initial auto-payment");
  check(autotaps==1 && selections==0,"Initial auto-payment must not enter manual selection");

  reset();late_double_click=1;
  check_paid("A second click received inside manual payment must request auto-payment");
  check(autotaps==1 && selections==1 && !activations && !pushes,
        "A pending ordinary spell must not be cast again");

  reset();late_double_click=1;global_cards_data[5].type=TYPE_INTERRUPT;
  global_cards_data[5].extra_ability=EA_MANA_SOURCE;
  check(charge_mana(0,COLOR_BLACK,1)==1 && !g_spell_fizzled && mana_paid[COLOR_BLACK]==1,
        "Double-clicking a pending Dark Ritual must pay its one black mana casting cost");
  check(!activations && !pushes && g_stack_size==1 && global_stack_cards[0].card==5,
        "A pending Dark Ritual must remain on the stack, not be used as a mana source");

  reset();late_double_click=1;
  global_card_instances[0][5].state=STATE_IN_PLAY;
  global_cards_data[5].type=TYPE_ENCHANTMENT;global_cards_data[5].extra_ability=EA_MANA_SOURCE;
  global_stack_cards[0].card=7;
  global_card_instances[0][7].internal_card_id=g_stack_proxy_internal_card_id;
  global_card_instances[0][7].parent_controller=0;global_card_instances[0][7].parent_card=5;
  check_paid("A second click on an activation's parent must request auto-payment");
  check(!activations && !pushes,"Auto-payment must not activate a pending mana ability twice");

  reset();late_double_click=1;g_raw_mana_available[0][COLOR_GREEN]=2;
  check_paid("A late double-click must spend colored pool mana toward a generic cost");
  check(!autotaps && !g_raw_mana_available[0][COLOR_GREEN],
        "Existing pool mana must be used before tapping sources");

  reset();manual_source=1;
  check_paid("A single click must still allow manual activation of a mana source");
  check(!autotaps && activations==1 && pushes==1,"Manual payment must remain manual");

  reset();cancel_payment=1;
  check(charge_mana(0,COLOR_COLORLESS,2)==0 && g_spell_fizzled && !autotaps,
        "Canceling manual payment must not trigger auto-payment");
  return failures ? 1 : 0;
}
