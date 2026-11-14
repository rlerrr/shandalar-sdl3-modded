#include <gtest/gtest.h>
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <crtdbg.h>
#include <windows.h>
#endif
#include "duel_test_api.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <memory>
#include <string>
#include "batch_journal.h"
#include "scry_observation.h"
#ifndef DUEL_TEST_ASSETS
#define DUEL_TEST_ASSETS "/assets_modded"
#endif
#include "duel_case_types.h"
#include "case_archive.h"
#include "duel_cases.inc"
extern "C" int ai_opinion_of_gamestate_continued(int player, int score);
extern "C" int manalinkeh_charge_mana_for_multi_x(int player, int color, int count);
extern "C" int g_max_x_value, g_spell_fizzled;
extern "C" int manalinkeh_get_storm_count(void);
extern "C" int manalinkeh_put_into_play(int, int);
extern "C" int manalinkeh_reveal_top_cards_of_library(int player, int count);
extern "C" int manalinkeh_select_one_and_put_the_rest_on_bottom(int chooser, int owner, int count);
extern "C" void manalinkeh_shuffle_for_exe(int, int), manalinkeh_shuffle(int);
extern "C" void manalinkeh_backup_data_for_ai_0(void), manalinkeh_restore_data_for_ai_0(void);
extern "C" void manalinkeh_backup_data_for_ai(void), manalinkeh_restore_data_for_ai(void);
extern "C" int g_duel_ai_mode_state, g_ai_speculation_depth;
extern "C" int g_current_phase;
extern "C" int tapped_for_mana_color;
extern "C" int graveyard_source[2][500];
extern "C" int manalinkeh_find_in_owners_graveyard(int, int, int*, int*);
extern "C" int manalinkeh_turn_card_in_grave_face_down(int, int);
extern "C" void manalinkeh_turn_card_in_grave_face_up(int, int, int);
using Refs = std::map<std::string,DuelTestRef>;
static std::vector<int> library_state(int player) {
  std::vector<int> result;
  for(int i=0;i<duel_test_zone_count(player,DUEL_ZONE_LIBRARY);++i)
    result.push_back(duel_test_zone_csvid(player,DUEL_ZONE_LIBRARY,i));
  return result;
}
static std::string observable_state() {
  std::ostringstream out;
  out<<"{\"stack\":"<<duel_test_stack_size()<<",\"players\":[";
  for(int p=0;p<2;++p) {
    if(p)out<<',';
    out<<"{\"life\":"<<duel_test_life(p)<<",\"energy\":"<<duel_test_energy(p)<<",\"poison\":"<<duel_test_poison(p)<<",\"mana\":[";
    for(int color=0;color<6;++color){if(color)out<<',';out<<duel_test_mana(p,color);}
    out<<"],\"zones\":[";
    for(int zone=0;zone<5;++zone){if(zone)out<<',';out<<'[';for(int n=0;n<duel_test_zone_count(p,zone);++n){if(n)out<<',';out<<duel_test_zone_csvid(p,zone,n);}out<<']';}
    out<<"],\"permanents\":[";
    bool comma=false;
    for(int n=0;n<150;++n){auto c=duel_test_inspect({p,n});if(!c.in_play)continue;if(comma)out<<',';comma=true;out<<'['<<n<<','<<c.csvid<<','<<c.power<<','<<c.toughness<<','<<c.tapped<<','<<c.sick<<','<<c.attacking<<','<<c.damage<<','<<c.plus1_counters<<','<<c.minus1_counters<<']';}
    out<<"]}";
  }
  out<<"]}";
  return out.str();
}
static void add_cards(Refs &refs,const Setup *cards,int count) {
  for(int i=0;i<count;++i) refs[cards[i].ref]=duel_test_add(cards[i].player,cards[i].zone,cards[i].csvid,cards[i].state);
}
static void load_fixture(Refs &refs,const char *name,unsigned seed) {
  duel_test_reset(seed);
  for(const auto &f:fixtures) if(!std::strcmp(f.name,name)) {
    duel_test_set_turn(f.turn,f.combat);
    for(int p=0;p<2;++p) { duel_test_set_life(p,f.life[p]); for(int c=0;c<6;++c)duel_test_set_mana(p,c,f.mana[p][c]); }
    add_cards(refs,f.cards,f.count); return;
  }
  FAIL()<<"Unknown fixture "<<name;
}
static int property(const DuelTestCard &c,const char *name) {
#define PROPERTY(n) if(!std::strcmp(name,#n)) return c.n
  PROPERTY(csvid); PROPERTY(power); PROPERTY(toughness); PROPERTY(tapped); PROPERTY(sick);
  PROPERTY(attacking); PROPERTY(in_play); PROPERTY(in_hand); PROPERTY(damage);
  PROPERTY(blocking_count); PROPERTY(blocked);
  PROPERTY(plus1_counters); PROPERTY(minus1_counters);
#undef PROPERTY
  ADD_FAILURE()<<"Unknown observation "<<name; return -99999;
}
class BehaviorTest:public testing::Test {
 CaseArchive *archive;
 const CaseArchive::Entry *entry; const Case *direct=nullptr;
 public:
 BehaviorTest(CaseArchive *a,const CaseArchive::Entry *e):archive(a),entry(e){} explicit BehaviorTest(const Case *s):archive(nullptr),entry(nullptr),direct(s){}
 void TestBody() override {
  auto loaded=archive ? archive->load(*entry) : CaseArchive::Loaded{};
  const Case *spec=direct ? direct : &loaded.spec;
  std::cerr<<"CASE: "<<spec->name<<" CARD: "<<spec->card<<" ORACLE: "<<spec->oracle_id<<" FIXTURE: "<<spec->fixture<<" SEED: "<<spec->seed<<"\n";
  SCOPED_TRACE(std::string(spec->card)+" Oracle "+spec->oracle_id+" "+spec->oracle_text);
  Refs refs; load_fixture(refs,spec->fixture,spec->seed); add_cards(refs,spec->cards,spec->count); duel_test_prepare_fixture();
  // Keep the resulting board visible even if a fatal prerequisite assertion
  // stops a case before its dependent actions can queue invalid choices.
  struct ObservationOnExit {
    ~ObservationOnExit() { std::cout<<"OBSERVATION: "<<observable_state()<<'\n'; }
  } observation_on_exit;
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(nullptr,duel_test_free_snapshot);
  Refs saved_refs;
  std::string recorded_state;
  std::map<int,std::vector<int>> recorded_libraries;
  for(int i=0;i<spec->step_count;++i) {
    const auto &s=spec->steps[i];
    if(std::getenv("DUEL_TEST_TRACE"))std::cerr<<"STEP "<<i<<' '<<s.op<<' '<<s.ref<<'\n';
    SCOPED_TRACE(std::string("step ")+std::to_string(i)+" "+s.op+" "+s.ref);
    auto ref = refs.count(s.ref) ? refs.at(s.ref) : DuelTestRef{-1,-1};
    const std::string op=s.op;
    if(op=="choose_target")duel_test_decision(DUEL_CHOICE_TARGET,s.player,ref.player,ref.card);
    else if(op=="record_library")recorded_libraries[s.player]=library_state(s.player);
    else if(op=="assert_scry_result") {
      ASSERT_TRUE(recorded_libraries.count(s.player));
      EXPECT_TRUE(valid_scry_result(recorded_libraries.at(s.player),library_state(s.player),s.value));
    }
    else if(op=="choose_optional_zone")duel_test_decision(DUEL_CHOICE_ZONE,s.player,-5,s.value);
    else if(op=="assert_forced_zone_choices")EXPECT_EQ(duel_test_forced_zone_choices(),s.value);
    else if(op=="choose_damage_target")duel_test_damage_decision(s.player,ref);
    else if(op=="choose_damage_player")duel_test_damage_decision(s.player,DuelTestRef{s.value,-1});
    else if(op=="choose_cancel_target")duel_test_decision(DUEL_CHOICE_TARGET,s.player,-1,-1);
    else if(op=="assert_offered_count")EXPECT_EQ(duel_test_offered_count(),s.value);
    else if(op=="assert_offered_selection_required")EXPECT_EQ(duel_test_offered_selection_required(),s.value);
    else if(op=="observe_search_filters")duel_host_observe_zone_legality(s.value);
    else if(op=="present_ai_dialogs")duel_host_present_ai_dialogs(s.value);
    else if(op=="assert_offered_legal_card") {
      bool found=false;
      for(int c=0;c<duel_test_offered_count();++c) if(duel_test_offered_csvid(c)==s.index) {
        found=true;
        EXPECT_EQ(duel_test_offered_legal(c),s.value);
      }
      EXPECT_TRUE(found)<<"Card "<<s.index<<" missing from offered search zone";
    }
    else if(op=="assert_offered_card") {
      ASSERT_LT(s.index,duel_test_offered_count());
      EXPECT_EQ(duel_test_offered_csvid(s.index),s.value);
    }
    else if(op=="choose_player")duel_test_decision(DUEL_CHOICE_TARGET,s.player,s.value,-1);
    else if(op=="choose_zone")duel_test_decision(DUEL_CHOICE_ZONE,s.player,s.value,-1);
    else if(op=="choose_zone_card")duel_test_decision(DUEL_CHOICE_ZONE,s.player,-3,s.value);
    else if(op=="choose_dialog")duel_test_decision(DUEL_CHOICE_DIALOG,s.player,s.value,-1);
    else if(op=="choose_payment")duel_test_decision(DUEL_CHOICE_DIALOG,s.player,-4,s.value);
    else if(op=="choose_number")duel_test_decision(DUEL_CHOICE_NUMBER,s.player,s.value,-1);
    else if(op=="choose_x")duel_test_decision(DUEL_CHOICE_X,s.player,s.value,-1);
    else if(op=="choose_color")duel_test_decision(DUEL_CHOICE_COLOR,s.player,s.value,-1);
    else if(op=="choose_mana")duel_test_decision(DUEL_CHOICE_MANA,s.player,-3,s.color);
    else if(op=="choose_cancel_mana")duel_test_decision(DUEL_CHOICE_MANA,s.player,-2,-1);
    else if(op=="choose_priority")duel_test_decision(DUEL_CHOICE_PRIORITY,ref.player>=0?ref.player:s.player,ref.card>=0?ref.card:s.value,-1);
    else if(op=="choose_regeneration")duel_test_regeneration_decision(ref);
    else if(op=="choose_trigger")duel_test_decision(DUEL_CHOICE_PRIORITY,s.player,-3,duel_test_inspect(ref).csvid);
    else if(op=="choose_trigger_source")duel_test_source_trigger_decision(s.player,ref);
    else if(op=="choose_optional_target")duel_test_optional_target_decision(s.player,ref);
    else if(op=="assert_forced_target_choices")EXPECT_EQ(duel_test_forced_target_choices(),s.value);
    else if(op=="choose_activation_mode")duel_test_activation_mode_decision(s.player,ref,s.property,s.color,s.value);
    else if(op=="require_activation_mode")ASSERT_EQ(duel_test_finish_activation_mode(),s.value);
    else if(op=="can_activate")EXPECT_EQ(!!duel_test_can_activate(ref),s.value);
    else if(op=="require_can_activate")ASSERT_EQ(!!duel_test_can_activate(ref),s.value);
    else if(op=="activate")EXPECT_EQ(!!duel_test_activate(ref),s.value);
    else if(op=="can_activate_hand")EXPECT_EQ(!!duel_test_can_activate_hand(ref),s.value);
    else if(op=="require_can_activate_hand")ASSERT_EQ(!!duel_test_can_activate_hand(ref),s.value);
    else if(op=="activate_hand")ASSERT_EQ(!!duel_test_activate_hand(ref),s.value);
    else if(op=="can_activate_graveyard")EXPECT_EQ(!!duel_test_can_activate_graveyard(ref),s.value);
    else if(op=="require_can_activate_graveyard")ASSERT_EQ(!!duel_test_can_activate_graveyard(ref),s.value);
    else if(op=="activate_graveyard")EXPECT_EQ(!!duel_test_activate_graveyard(ref),s.value);
    else if(op=="prepare_graveyard_abilities")duel_test_prepare_graveyard_abilities();
    else if(op=="can_cast_graveyard")EXPECT_EQ(!!duel_test_can_cast_graveyard(ref),s.value);
    else if(op=="require_can_cast_graveyard")ASSERT_EQ(!!duel_test_can_cast_graveyard(ref),s.value);
    else if(op=="cast_graveyard")EXPECT_EQ(!!duel_test_cast_graveyard(ref),s.value);
    else if(op=="assert_unused_choices")EXPECT_EQ(duel_test_finish_choice_plan(),s.value);
    else if(op=="can_cast")EXPECT_EQ(!!duel_test_can_cast(ref),s.value);
    else if(op=="require_can_cast")ASSERT_EQ(!!duel_test_can_cast(ref),s.value);
    else if(op=="can_target_creature")EXPECT_EQ(!!duel_test_can_target_creature(ref,refs.at(s.other)),s.value);
    else if(op=="probe_activation_target") {
      int legal=duel_test_probe_activation_target(ref,refs.at(s.other),s.index);
      ASSERT_GE(legal,0)<<"Activation target definition absent or ambiguous";
      ASSERT_EQ(legal,s.value);
    }
    else if(op=="cast")EXPECT_EQ(!!duel_test_cast(ref),s.value);
    else if(op=="cast_response")EXPECT_EQ(!!duel_test_cast_response(ref),s.value);
    else if(op=="announce_spell")EXPECT_TRUE(duel_test_announce_spell(ref));
    else if(op=="reset_death_history")duel_test_reset_death_history();
    else if(op=="discard_policy")duel_test_set_discard_policy(s.value);
    else if(op=="assert_discard_choices")EXPECT_EQ(duel_test_discard_choice_count(),s.value);
    else if(op=="refresh")duel_test_refresh();
    else if(op=="resolve")duel_test_resolve();
    else if(op=="resolve_response")duel_test_resolve_response();
    else if(op=="cleanup")duel_test_cleanup();
    else if(op=="remove_from_combat")duel_test_remove_from_combat(ref);
    else if(op=="destroy")duel_test_destroy(ref);
    else if(op=="attack")EXPECT_EQ(!!duel_test_attack(ref),s.value);
    else if(op=="begin_attackers")duel_test_begin_attackers();
    else if(op=="finish_attackers")duel_test_finish_attackers();
    else if(op=="begin_blockers")duel_test_begin_blockers();
    else if(op=="block")EXPECT_EQ(!!duel_test_block(ref,refs.at(s.other)),s.value);
    else if(op=="combat_damage")duel_test_combat_damage();
    else if(op=="end_combat")duel_test_end_combat();
    else if(op=="advance_combat")duel_test_advance_combat();
    else if(op=="turn")duel_test_set_turn(s.player,s.value);
    else if(op=="advance_turn")duel_test_advance_turn();
    else if(op=="untap_step")ASSERT_TRUE(duel_test_untap_step(s.player));
    else if(op=="upkeep_step")ASSERT_TRUE(duel_test_upkeep_step(s.player));
    else if(op=="set_mana")duel_test_set_mana(s.player,s.color,s.value);
    else if(op=="set_life")duel_test_set_life(s.player,s.value);
    else if(op=="set_budget")duel_test_set_budget(s.value);
    else if(op=="assert_card_name")EXPECT_STREQ(duel_test_card_name(ref),spec->card);
    else if(op=="assert_payment_source")EXPECT_NE(std::string(duel_test_payment_prompt()).find(spec->card),std::string::npos);
    else if(op=="assert_card")EXPECT_EQ(property(duel_test_inspect(ref),s.property),s.value);
    else if(op=="assert_counter")EXPECT_EQ(duel_test_counter(ref,s.property),s.value);
    else if(op=="require_card")ASSERT_EQ(property(duel_test_inspect(ref),s.property),s.value);
    else if(op=="assert_zone")EXPECT_EQ(duel_test_zone_count(s.player,s.zone),s.value);
    else if(op=="assert_shuffle_count")EXPECT_EQ(duel_test_shuffle_count(s.player),s.value);
    else if(op=="assert_zone_card")EXPECT_EQ(duel_test_zone_csvid(s.player,s.zone,s.index),s.value);
    else if(op=="assert_zone_contains") {
      bool found=false; for(int n=0;n<duel_test_zone_count(s.player,s.zone);++n) if(duel_test_zone_csvid(s.player,s.zone,n)==s.value)found=true;
      EXPECT_TRUE(found)<<"Missing CSVID "<<s.value;
    }
    else if(op=="assert_zone_multiplicity" || op=="assert_zone_multiplicity_max") {
      int count=0;
      for(int n=0;n<duel_test_zone_count(s.player,s.zone);++n)
        count+=duel_test_zone_csvid(s.player,s.zone,n)==s.index;
      if(op=="assert_zone_multiplicity_max") { EXPECT_LE(count,s.value)<<"Zone "<<s.zone<<" CSVID "<<s.index; }
      else { EXPECT_EQ(count,s.value)<<"Zone "<<s.zone<<" CSVID "<<s.index; }
    }
    else if(op=="assert_zone_state") {
      DuelTestRef r=duel_test_permanent_at(s.player,s.index);
      ASSERT_GE(r.card,0); EXPECT_EQ(property(duel_test_inspect(r),s.property),s.value);
    }
    else if(op=="assert_matching_permanents") {
      int matches=0;
      for(int index=0;index<duel_test_zone_count(s.player,DUEL_ZONE_BATTLEFIELD);++index) {
        auto candidate=duel_test_permanent_at(s.player,index);
        auto card=duel_test_inspect(candidate);
        if(card.csvid!=s.index)continue;
        ++matches;
        const bool token_property=std::string(s.property)=="color" || std::string(s.property)=="artifact" ||
          std::string(s.property)=="token" || std::string(s.property).rfind("subtype_",0)==0;
        EXPECT_EQ(token_property?duel_test_token_characteristic(candidate,s.property):property(card,s.property),s.value);
      }
      ASSERT_GT(matches,0)<<"No matching permanent for CSVID "<<s.index;
    }
    else if(op=="bind_permanent_copy" || op=="bind_hand_copy") {
      const int zone=op=="bind_hand_copy"?DUEL_ZONE_HAND:DUEL_ZONE_BATTLEFIELD;
      auto at=op=="bind_hand_copy"?duel_test_hand_at:duel_test_permanent_at;
      DuelTestRef selected={s.player,-1};int ordinal=0;
      for(int index=0;index<duel_test_zone_count(s.player,zone);++index) {
        auto candidate=at(s.player,index);
        if(duel_test_inspect(candidate).csvid==s.value && ordinal++==s.index)selected=candidate;
      }
      ASSERT_GE(selected.card,0)<<"Missing copy "<<s.index<<" of CSVID "<<s.value<<" in zone "<<zone;
      refs[s.ref]=selected;
    }
    else if(op=="bind_permanent" || op=="bind_hand") {
      int zone=op=="bind_hand"?DUEL_ZONE_HAND:DUEL_ZONE_BATTLEFIELD;
      auto at=op=="bind_hand"?duel_test_hand_at:duel_test_permanent_at;
      DuelTestRef r={s.player,-1}; int matches=0;
      if(s.index>=0) {
        r=at(s.player,s.index);
        matches=r.card>=0 && duel_test_inspect(r).csvid==s.value;
      } else {
        for(int index=0;index<duel_test_zone_count(s.player,zone);++index) {
          auto candidate=at(s.player,index);
          if(duel_test_inspect(candidate).csvid==s.value) { r=candidate; ++matches; }
        }
      }
      EXPECT_EQ(matches,1)<<"Expected one matching "<<(op=="bind_hand"?"hand card":"permanent")<<" for CSVID "<<s.value;
      if(matches!=1) { duel_test_dump(); break; }
      refs[s.ref]=r;
    }
    else if(op=="assert_hand_graveyard_total") {
      int count=0;
      for(int zone:{DUEL_ZONE_HAND,DUEL_ZONE_GRAVEYARD})
        for(int n=0;n<duel_test_zone_count(s.player,zone);++n)
          count+=duel_test_zone_csvid(s.player,zone,n)==s.index;
      EXPECT_EQ(count,s.value)<<"CSVID "<<s.index;
    }
    else if(op=="record_state")recorded_state=observable_state();
    else if(op=="assert_recorded_state") { ASSERT_FALSE(recorded_state.empty()); EXPECT_EQ(observable_state(),recorded_state); }
    else if(op=="set_card_available")duel_test_set_card_available(s.index,s.value);
    else if(op=="assert_card_available")EXPECT_EQ(duel_test_card_available(s.index),s.value);
    else if(op=="assert_mana")EXPECT_EQ(duel_test_mana(s.player,s.color),s.value);
    else if(op=="assert_mana_at_most")EXPECT_LE(duel_test_mana(s.player,s.color),s.value);
    else if(op=="assert_mana_total") {
      int total=0;for(int color=0;color<6;++color)total+=duel_test_mana(s.player,color);
      EXPECT_EQ(total,s.value);
    }
    else if(op=="assert_life")EXPECT_EQ(duel_test_life(s.player),s.value);
    else if(op=="assert_poison")EXPECT_EQ(duel_test_poison(s.player),s.value);
    else if(op=="assert_energy")EXPECT_EQ(duel_test_energy(s.player),s.value);
    else if(op=="assert_stack")EXPECT_EQ(duel_test_stack_size(),s.value);
    else if(op=="assert_targets_consumed")EXPECT_EQ(duel_test_finish_target_plan(),0)<<"Unconsumed planned targets";
    else if(op=="assert_choices_consumed")EXPECT_EQ(duel_test_finish_choice_plan(),0)<<"Unconsumed planned choices";
    else if(op=="capture") { saved.reset(duel_test_capture()); saved_refs=refs; }
    else if(op=="restore") { ASSERT_NE(saved.get(),nullptr); duel_test_restore(saved.get()); refs=saved_refs; }
    else if(op=="ai_roundtrip")duel_test_ai_roundtrip();
    else if(op=="ai_turns")duel_test_ai_turns(s.value);
    else if(op=="choose_mana_source")duel_test_decision(DUEL_CHOICE_MANA,ref.player,ref.card,-1);
    else if(op=="can_activate_granted_mana")EXPECT_EQ(!!duel_test_can_activate_granted_mana(ref,refs.at(s.other)),s.value);
    else if(op=="activate_granted_mana")EXPECT_EQ(!!duel_test_activate_granted_mana(ref,refs.at(s.other)),s.value);
    else FAIL()<<"Unimplemented action "<<op;
    duel_test_check_invariants();
    if(HasFailure())duel_test_dump();
  }
  duel_test_finish_decisions();
 }
};
TEST(ScryObservation, AcceptsTopBottomAndReorderingOfInspectedCards) {
  const std::vector<int> before={1,2,3,4,5};
  EXPECT_TRUE(valid_scry_result(before,{1,2,3,4,5},2));
  EXPECT_TRUE(valid_scry_result(before,{2,3,4,5,1},2));
  EXPECT_TRUE(valid_scry_result(before,{2,1,3,4,5},2));
  EXPECT_TRUE(valid_scry_result(before,{3,4,5,2,1},2));
}
TEST(ScryObservation, RejectsChangesToUninspectedCardsOrTheirOrder) {
  const std::vector<int> before={1,2,3,4,5};
  EXPECT_FALSE(valid_scry_result(before,{1,2,4,3,5},2));
  EXPECT_FALSE(valid_scry_result(before,{1,2,3,4,6},2));
  EXPECT_FALSE(valid_scry_result(before,{1,2,3,4},2));
}
TEST(ScryObservation, PreservesMultiplicityWithDuplicateInspectedCards) {
  EXPECT_TRUE(valid_scry_result({1,1,2,3},{2,3,1,1},2));
  EXPECT_FALSE(valid_scry_result({1,1,2,3},{1,2,3,3},2));
}
TEST(ScryObservation, HandlesShortEmptyAndZeroInspectionLibraries) {
  EXPECT_TRUE(valid_scry_result({}, {},3));
  EXPECT_TRUE(valid_scry_result({1},{1},3));
  EXPECT_TRUE(valid_scry_result({1,2},{1,2},0));
  EXPECT_FALSE(valid_scry_result({1,2},{2,1},0));
  EXPECT_FALSE(valid_scry_result({1},{1},-1));
}
TEST(HandExchangeInput, OfferedIdentityPoliciesAndCountsSurvivePendingAndResolvedSnapshots) {
  for(int policy:{1,2}) {
    Refs refs; load_fixture(refs,"stocked_zones",123);
    auto source=duel_test_add(0,DUEL_ZONE_HAND,1439,0); // Careful Study.
    duel_test_prepare_fixture(); duel_test_set_discard_policy(policy);
    ASSERT_TRUE(duel_test_cast(source));
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> pending(duel_test_capture(),duel_test_free_snapshot);
    duel_test_resolve(); ASSERT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_discard_choice_count(),2);
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_HAND),4);
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_GRAVEYARD),7);
    int forest=0,growth=0,plains=0;
    for(int c=0;c<duel_test_zone_count(0,DUEL_ZONE_HAND);++c) {
      int id=duel_test_zone_csvid(0,DUEL_ZONE_HAND,c);forest+=id==91;growth+=id==97;plains+=id==188;
    }
    EXPECT_EQ(forest,policy==1?0:1); EXPECT_EQ(growth,policy==1?0:1); EXPECT_EQ(plains,policy==1?2:0);
    auto result=observable_state();
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> resolved(duel_test_capture(),duel_test_free_snapshot);
    duel_test_set_discard_policy(0); EXPECT_EQ(duel_test_discard_choice_count(),0);
    duel_test_restore(resolved.get()); EXPECT_EQ(duel_test_discard_choice_count(),2);
    duel_test_set_discard_policy(3-policy);
    duel_test_ai_roundtrip(); duel_test_restore(pending.get());
    EXPECT_EQ(duel_test_discard_choice_count(),0);
    duel_test_resolve(); ASSERT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_discard_choice_count(),2); EXPECT_EQ(observable_state(),result);
  }
}
TEST(HandExchangeInput, DiscardPolicyLeavesOtherZoneSelectorsAndTheirExplicitChoicesIntact) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(0,DUEL_ZONE_HAND,1124,0); // Rampant Growth.
  duel_test_prepare_fixture(); duel_test_set_discard_policy(2);
  ASSERT_TRUE(duel_test_cast(source));
  duel_test_decision(DUEL_CHOICE_ZONE,0,-3,91);
  duel_test_resolve();
  EXPECT_EQ(duel_test_finish_choice_plan(),0); EXPECT_EQ(duel_test_discard_choice_count(),0);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),6);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_BATTLEFIELD),1);
}
TEST(HandExchangeInput, ExplicitAiModeInputIsOptInAndSurvivesSnapshots) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(1,DUEL_ZONE_BATTLEFIELD,16106,DUEL_STATE_UNTAPPED); // Geier Reach Sanitarium.
  duel_test_prepare_fixture();
  EXPECT_FALSE(duel_host_presenting_ai_dialogs());
  duel_host_present_ai_dialogs(1);
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> ready(duel_test_capture(),duel_test_free_snapshot);
  duel_host_present_ai_dialogs(0); duel_test_ai_roundtrip();
  duel_test_restore(ready.get()); EXPECT_TRUE(duel_host_presenting_ai_dialogs());
  duel_host_set_ai_choices(1); EXPECT_FALSE(duel_host_presenting_ai_dialogs());
  duel_host_set_ai_choices(0);
  auto activate_mode=[&](int mode) {
    duel_test_decision(DUEL_CHOICE_DIALOG,1,mode,-1);
    EXPECT_TRUE(duel_test_activate(source));
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
  };
  activate_mode(0); EXPECT_EQ(duel_test_stack_size(),0);
  EXPECT_EQ(duel_test_mana(1,0),11);
  duel_test_restore(ready.get()); activate_mode(1);
  EXPECT_EQ(duel_test_stack_size(),1); EXPECT_EQ(duel_test_mana(1,0),8);
  duel_test_set_discard_policy(1); duel_test_resolve();
  EXPECT_EQ(duel_test_finish_choice_plan(),0);
  for(int p:{0,1}) {
    EXPECT_EQ(duel_test_zone_count(p,DUEL_ZONE_HAND),4);
    EXPECT_EQ(duel_test_zone_count(p,DUEL_ZONE_LIBRARY),6);
    EXPECT_EQ(duel_test_zone_count(p,DUEL_ZONE_GRAVEYARD),5);
  }
  duel_test_restore(ready.get());
  for(int color=0;color<6;++color)duel_test_set_mana(1,color,color==0?1:0);
  duel_test_decision(DUEL_CHOICE_DIALOG,1,1,-1);
  EXPECT_FALSE(duel_test_activate(source));
  EXPECT_EQ(duel_test_finish_choice_plan(),0);
  EXPECT_EQ(duel_test_stack_size(),0); EXPECT_EQ(duel_test_mana(1,0),1);
  EXPECT_FALSE(duel_test_inspect(source).tapped);
}
TEST(GraveyardProvenance, LookupIgnoresUnusedSlotsIncludingSourceZero) {
  for(int owner:{0,1})for(int size:{0,1,3}) {
    SCOPED_TRACE(::testing::Message()<<"owner="<<owner<<" size="<<size);
    Refs refs;load_fixture(refs,"empty_main",123);
    for(int i=0;i<size;++i)duel_test_add(owner,DUEL_ZONE_GRAVEYARD,514,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,6736,0);
    ASSERT_EQ(source.card,0);duel_test_prepare_fixture();
    const int encoded=owner<<8;
    // Model stale provenance outside the live zone, including its first unused
    // entry and slot499.  Both owners can collide; owner0 naturally encodes0.
    for(int i=0;i<size;++i)graveyard_source[owner][i]=encoded^1;
    graveyard_source[owner][size]=encoded;graveyard_source[owner][499]=encoded;
    auto before=observable_state();
    std::vector<int> provenance(graveyard_source[owner],graveyard_source[owner]+500);
    int found_owner=-2,position=-2;
    EXPECT_EQ(manalinkeh_find_in_owners_graveyard(source.player,source.card,&found_owner,&position),0);
    EXPECT_EQ(found_owner,owner);EXPECT_EQ(position,-1);
    EXPECT_EQ(observable_state(),before);
    EXPECT_TRUE(std::equal(provenance.begin(),provenance.end(),graveyard_source[owner]));
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),size);
    for(int i=0;i<size;++i)EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,i),514);
  }
}
TEST(GraveyardProvenance, LookupKeepsNewestLiveMatchAndIgnoresGenerationBits) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);
    for(int id:{514,230,514})duel_test_add(owner,DUEL_ZONE_GRAVEYARD,id,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,6736,0);
    ASSERT_EQ(source.card,0);duel_test_prepare_fixture();
    const int encoded=owner<<8;
    graveyard_source[owner][0]=encoded;graveyard_source[owner][1]=encoded^1;
    graveyard_source[owner][2]=encoded|0x200;graveyard_source[owner][499]=encoded;
    int found_owner=-2,position=-2;
    ASSERT_NE(manalinkeh_find_in_owners_graveyard(source.player,source.card,&found_owner,&position),0);
    EXPECT_EQ(found_owner,owner);EXPECT_EQ(position,2);
    // A temporarily hidden live card still occupies its graveyard position.
    const int hidden=manalinkeh_turn_card_in_grave_face_down(owner,2);
    ASSERT_NE(manalinkeh_find_in_owners_graveyard(source.player,source.card,&found_owner,&position),0);
    EXPECT_EQ(position,2);manalinkeh_turn_card_in_grave_face_up(owner,2,hidden);
    graveyard_source[owner][2]=encoded^1;
    ASSERT_NE(manalinkeh_find_in_owners_graveyard(source.player,source.card,&found_owner,&position),0);
    EXPECT_EQ(found_owner,owner);EXPECT_EQ(position,0);
    auto token=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,6736,DUEL_STATE_TOKEN);
    graveyard_source[owner][2]=(owner<<8)|(token.card&0xff);
    EXPECT_EQ(manalinkeh_find_in_owners_graveyard(token.player,token.card,&found_owner,&position),0);
    EXPECT_EQ(found_owner,owner);EXPECT_EQ(position,-1);
  }
}
TEST(ShuffleObservation, ActualEntriesCountTheLibraryOwnerForBothPathsAndSmallDecks) {
  for(int actor:{0,1})for(int owner:{0,1})for(int size:{0,1,3}) {
    SCOPED_TRACE(::testing::Message()<<"actor="<<actor<<" owner="<<owner<<" size="<<size);
    Refs refs; load_fixture(refs,"empty_main",123);
    for(int i=0;i<size;++i)duel_test_add(owner,DUEL_ZONE_LIBRARY,i==0?91:i==1?106:126,0);
    duel_test_prepare_fixture();
    EXPECT_EQ(duel_test_shuffle_count(0),0); EXPECT_EQ(duel_test_shuffle_count(1),0);
    auto original=library_state(owner);std::sort(original.begin(),original.end());
    manalinkeh_shuffle_for_exe(actor,owner); // Native shuffle_duel_library redirects here.
    EXPECT_EQ(duel_test_shuffle_count(owner),1); EXPECT_EQ(duel_test_shuffle_count(1-owner),0);
    manalinkeh_shuffle(owner); // The Manalink wrapper must not count a second time.
    EXPECT_EQ(duel_test_shuffle_count(owner),2); EXPECT_EQ(duel_test_shuffle_count(1-owner),0);
    auto actual=library_state(owner);std::sort(actual.begin(),actual.end());EXPECT_EQ(actual,original);
    auto host=duel_host_capture();auto state=observable_state();
    EXPECT_EQ(duel_test_shuffle_count(-1),-1);EXPECT_EQ(duel_test_shuffle_count(2),-1);
    EXPECT_EQ(duel_test_shuffle_count(owner),2);EXPECT_EQ(observable_state(),state);
    auto after=duel_host_capture();EXPECT_EQ(after.random_state,host.random_state);
    EXPECT_EQ(after.ticks,host.ticks);EXPECT_EQ(after.remaining_steps,host.remaining_steps);
  }
}
TEST(ShuffleObservation, TestAndBothAiSnapshotsRestoreCountsWithoutRefundingWork) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);
    for(int id:{91,106,126})duel_test_add(owner,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();manalinkeh_shuffle(owner);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
    auto first=library_state(owner);manalinkeh_shuffle(owner);auto second=library_state(owner);
    EXPECT_EQ(duel_test_shuffle_count(owner),2);duel_test_restore(saved.get());
    EXPECT_EQ(duel_test_shuffle_count(owner),1);EXPECT_EQ(library_state(owner),first);
    manalinkeh_shuffle(owner);EXPECT_EQ(duel_test_shuffle_count(owner),2);EXPECT_EQ(library_state(owner),second);
    for(int kind:{0,1}) {
      if(kind==0)manalinkeh_backup_data_for_ai_0();else manalinkeh_backup_data_for_ai();
      manalinkeh_shuffle(owner);EXPECT_EQ(duel_test_shuffle_count(owner),3);
      duel_host_clock();auto work=duel_host_capture();
      if(kind==0)manalinkeh_restore_data_for_ai_0();else manalinkeh_restore_data_for_ai();
      EXPECT_EQ(duel_test_shuffle_count(owner),2);EXPECT_EQ(library_state(owner),second);
      auto after=duel_host_capture();EXPECT_LE(after.remaining_steps,work.remaining_steps);EXPECT_GE(after.ticks,work.ticks);
    }
    auto host=duel_host_capture();
    struct RestorePolicy {
      int mode,depth,choices;
      ~RestorePolicy(){g_duel_ai_mode_state=mode;g_ai_speculation_depth=depth;duel_host_set_ai_choices(choices);}
    } restore{g_duel_ai_mode_state,g_ai_speculation_depth,host.ai_choices};
    g_duel_ai_mode_state=1;g_ai_speculation_depth=1;duel_host_set_ai_choices(1);
    manalinkeh_shuffle_for_exe(1-owner,owner);
    EXPECT_EQ(duel_test_shuffle_count(owner),3); // Retained calls are real even under these policies.
  }
}
TEST(ShuffleObservation, RealTutorShufflesAfterResolutionAndReplaysUnderSnapshot) {
  Refs refs;load_fixture(refs,"empty_main",123);
  auto tutor=duel_test_add(0,DUEL_ZONE_HAND,1124,0); // Rampant Growth.
  for(int id:{91,106,126})duel_test_add(0,DUEL_ZONE_LIBRARY,id,0);
  duel_test_prepare_fixture();
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
  for(int replay=0;replay<2;++replay) {
    if(replay)duel_test_restore(saved.get());
    EXPECT_EQ(duel_test_shuffle_count(0),0);EXPECT_EQ(duel_test_shuffle_count(1),0);
    ASSERT_TRUE(duel_test_cast(tutor));EXPECT_EQ(duel_test_stack_size(),1);EXPECT_EQ(duel_test_shuffle_count(0),0);
    duel_test_decision(DUEL_CHOICE_ZONE,0,-3,91);duel_test_resolve();
    EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
    EXPECT_EQ(duel_test_shuffle_count(0),1);EXPECT_EQ(duel_test_shuffle_count(1),0);
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),2);
    EXPECT_EQ(duel_test_zone_csvid(0,DUEL_ZONE_BATTLEFIELD,0),91);
  }
}
TEST(TokenCharacteristics, GeneratedWolvesExposeWolfWithoutChangingOtherSubtypes) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,91,0);duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,91,0);
    duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,91,0); // Foreign Forest does not increase the token count.
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,9565,0);duel_test_prepare_fixture();
    ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();EXPECT_EQ(duel_test_stack_size(),0);
    int wolves=0;
    for(int i=0;i<duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD);++i) {
      auto r=duel_test_permanent_at(owner,i);if(duel_test_inspect(r).csvid!=18856)continue;
      ++wolves;EXPECT_EQ(duel_test_inspect(r).power,2);EXPECT_EQ(duel_test_inspect(r).toughness,2);
      EXPECT_EQ(duel_test_token_characteristic(r,"token"),1);
      EXPECT_EQ(duel_test_token_characteristic(r,"subtype_Wolf"),1);
      EXPECT_EQ(duel_test_token_characteristic(r,"subtype_Bear"),0);
    }
    EXPECT_EQ(wolves,2);EXPECT_EQ(duel_test_token_characteristic(bear,"subtype_Wolf"),0);
    EXPECT_EQ(duel_test_token_characteristic(bear,"subtype_Bear"),1);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
  }
}
TEST(FiresOfYavimaya, ExileNotificationUsesIidWhileSurvivorStillGrantsHaste) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);
    auto selected=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,5000,0);
    auto survivor=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,5000,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_HAND,106,0);
    auto purge=duel_test_add(1-owner,DUEL_ZONE_HAND,14986,0);
    duel_test_prepare_fixture();duel_test_set_turn(1-owner,0);
    duel_test_decision(DUEL_CHOICE_TARGET,1-owner,owner,selected.card);
    ASSERT_TRUE(duel_test_cast(purge));duel_test_resolve();
    EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
    EXPECT_FALSE(duel_test_inspect(selected).in_play);EXPECT_TRUE(duel_test_inspect(survivor).in_play);
    ASSERT_EQ(duel_test_zone_count(owner,DUEL_ZONE_EXILE),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_EXILE,0),5000);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
    for(int remove_survivor:{0,1}) {
      if(remove_survivor){duel_test_restore(saved.get());duel_test_destroy(survivor);}
      duel_test_set_turn(owner,0);ASSERT_TRUE(duel_test_cast(bear));duel_test_resolve();
      duel_test_begin_attackers();EXPECT_EQ(duel_test_attack(bear),!remove_survivor);
      duel_test_finish_attackers();EXPECT_EQ(duel_test_finish_choice_plan(),0);
    }
  }
}
TEST(FiresOfYavimaya, SacrificePumpKeepsCostsAndTemporaryEffect) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto fires=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,5000,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_HAND,106,0);duel_test_prepare_fixture();
    ASSERT_TRUE(duel_test_cast(bear));duel_test_resolve();
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,bear.card);
    ASSERT_TRUE(duel_test_activate(fires));EXPECT_EQ(duel_test_stack_size(),1);
    EXPECT_FALSE(duel_test_inspect(fires).in_play);EXPECT_EQ(duel_test_inspect(bear).power,2);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,0),5000);
    duel_test_resolve();EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_inspect(bear).power,4);EXPECT_EQ(duel_test_inspect(bear).toughness,4);
    duel_test_begin_attackers();EXPECT_FALSE(duel_test_attack(bear));duel_test_finish_attackers();
    duel_test_cleanup();EXPECT_EQ(duel_test_inspect(bear).power,2);EXPECT_EQ(duel_test_inspect(bear).toughness,2);
  }
}
TEST(MysticDecree, CanonicalCallbackCastRemovesFlyingForBothControllers) {
  for(int owner:{0,1})for(int present:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto flyer=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,0,0); // Air Elemental, printed flying.
    auto blocker=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto decree=duel_test_add(owner,DUEL_ZONE_HAND,2226,0);duel_test_prepare_fixture();
    if(present){ASSERT_TRUE(duel_test_cast(decree));duel_test_resolve();EXPECT_TRUE(duel_test_inspect(decree).in_play);}
    duel_test_set_turn(1-owner,0);duel_test_begin_attackers();ASSERT_TRUE(duel_test_attack(flyer));
    duel_test_finish_attackers();duel_test_begin_blockers();
    EXPECT_EQ(duel_test_block(blocker,flyer),present);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
  }
}
TEST(TokenCharacteristics, RealReplacementTokensExposeApeAndFrogLizard) {
  for(int spell:{8822,13034})for(int caster:{0,1}) {
    const int owner=1-caster, token_id=spell==8822?18689:18739;
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(caster,0);
    auto target=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto source=duel_test_add(caster,DUEL_ZONE_HAND,spell,0);duel_test_prepare_fixture();
    duel_test_decision(DUEL_CHOICE_TARGET,caster,owner,target.card);
    ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,0),106);
    ASSERT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),1);
    EXPECT_EQ(duel_test_zone_count(caster,DUEL_ZONE_BATTLEFIELD),0);
    auto token=duel_test_permanent_at(owner,0);EXPECT_EQ(duel_test_inspect(token).csvid,token_id);
    EXPECT_EQ(duel_test_inspect(token).power,3);EXPECT_EQ(duel_test_inspect(token).toughness,3);
    EXPECT_EQ(duel_test_token_characteristic(token,"token"),1);EXPECT_EQ(duel_test_token_characteristic(token,"color"),0x08);
    EXPECT_EQ(duel_test_token_characteristic(token,"subtype_Ape"),spell==8822);
    EXPECT_EQ(duel_test_token_characteristic(token,"subtype_Frog"),spell==13034);
    EXPECT_EQ(duel_test_token_characteristic(token,"subtype_Lizard"),spell==13034);
    EXPECT_EQ(duel_test_token_characteristic(token,"subtype_Bear"),0);
    EXPECT_EQ(duel_test_token_characteristic(token,"subtype_Wolf"),0);
  }
}
TEST(CardCallbackAbi, RegisteredHandlersUseThreeArgumentsForBothControllers) {
  // Exercise the actual registry's indirect calls, without claiming Oracle
  // coverage for these cards or interpreting their raw permission results.
  for(int csvid:{12406,10152,14046,4932,6399,6400,13356,13358,13298,5643,13601,8719})for(int owner:{0,1}) {
    SCOPED_TRACE(::testing::Message()<<"csvid="<<csvid<<" owner="<<owner);
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,csvid,0);duel_test_prepare_fixture();
    duel_test_can_cast(source); // Some creature/land handlers legitimately return0.
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_TRUE(duel_test_inspect(source).in_hand);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_HAND,0),csvid);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
  }
}
TEST(CardCallbackAbi, RegisteredDeathtouchHandlerReturnsIntThroughEntryCastAndReplay) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto existing=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,1602,DUEL_STATE_UNTAPPED);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,1602,0); // Registry points at deathtouch itself.
    duel_test_prepare_fixture();
    duel_test_set_mana(owner,0,2);duel_test_set_mana(owner,3,1);duel_test_set_mana(owner,4,1);
    duel_test_refresh();
    EXPECT_EQ(duel_test_token_characteristic(existing,"subtype_Snake"),1);
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(ready.get());
      ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();duel_test_refresh();
      EXPECT_TRUE(duel_test_inspect(source).in_play);
      EXPECT_EQ(duel_test_inspect(source).power,2);EXPECT_EQ(duel_test_inspect(source).toughness,2);
      EXPECT_EQ(duel_test_token_characteristic(source,"subtype_Snake"),1);
      EXPECT_EQ(duel_test_mana(owner,0),0);EXPECT_EQ(duel_test_mana(owner,3),0);EXPECT_EQ(duel_test_mana(owner,4),0);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_TRUE(duel_test_inspect(existing).in_play);
      if(!replay)completed=observable_state();else EXPECT_EQ(observable_state(),completed);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),completed);
    }
  }
}
TEST(CardCallbackAbi, ModularTargetPredicateUsesFiveArgumentsThroughRealDeathAndReplay) {
  for(int owner:{0,1})for(bool decline:{false,true}) {
    SCOPED_TRACE(::testing::Message()<<"owner="<<owner<<" decline="<<decline);
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,6850,0); // Arcbound Ravager.
    auto target=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,6867,0); // Coretapper.
    duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,106,0); // Nonartifact filter witness.
    duel_test_prepare_fixture();ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();
    ASSERT_EQ(duel_test_counter(source,"plus1"),1);
    EXPECT_EQ(duel_test_counter(target,"plus1"),0);
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(ready.get());
      duel_test_decision(DUEL_CHOICE_TARGET,owner,decline?-1:owner,decline?-1:target.card);
      duel_test_destroy(source);duel_test_refresh();duel_test_resolve();
      EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,0),6850);
      EXPECT_EQ(duel_test_counter(target,"plus1"),decline?0:1);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),1);
      if(replay)EXPECT_EQ(observable_state(),completed);
      else completed=observable_state();
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),completed);
      duel_test_check_invariants();
    }
  }
}
TEST(Flashback, QueryPreservesCallerChoicesAndRealCastCompletesUnderSnapshotReplay) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123); duel_test_set_turn(owner,0);
    for(int p:{0,1})for(int c=0;c<6;++c)duel_test_set_mana(p,c,p==owner && (c==0 || c==2)?1:0);
    auto source=duel_test_add(owner,DUEL_ZONE_GRAVEYARD,5800,0); // Deep Analysis.
    for(int card:{91,188,106})duel_test_add(owner,DUEL_ZONE_LIBRARY,card,0);
    duel_test_prepare_fixture(); duel_test_prepare_graveyard_abilities();
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,-1);
    auto state=observable_state(); auto host=duel_host_capture();
    ASSERT_TRUE(duel_test_can_cast_graveyard(source)); ASSERT_TRUE(duel_test_can_cast_graveyard(source));
    EXPECT_FALSE(duel_test_can_activate_graveyard(source));
    EXPECT_EQ(observable_state(),state);
    auto after=duel_host_capture();
    EXPECT_EQ(after.random_state,host.random_state); EXPECT_EQ(after.ticks,host.ticks);
    EXPECT_EQ(after.remaining_steps,host.remaining_steps);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> ready(duel_test_capture(),duel_test_free_snapshot);
    ASSERT_TRUE(duel_test_cast_graveyard(source)); EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_stack_size(),0); EXPECT_EQ(duel_test_life(owner),17);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),0);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_EXILE),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_EXILE,0),5800);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),1);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_LIBRARY,0),106);
    for(int c=0;c<6;++c)EXPECT_EQ(duel_test_mana(owner,c),0);
    auto completed=observable_state();
    duel_test_restore(ready.get()); ASSERT_TRUE(duel_test_cast_graveyard(source));
    EXPECT_EQ(duel_test_finish_choice_plan(),0); EXPECT_EQ(observable_state(),completed);
  }
}
TEST(Flashback, CostTimingAndPendingStackDenialsLeaveCallerStateUntouched) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123); duel_test_set_turn(owner,0);
    for(int p:{0,1})for(int c=0;c<6;++c)duel_test_set_mana(p,c,p==owner && (c==0 || c==2)?1:0);
    auto source=duel_test_add(owner,DUEL_ZONE_GRAVEYARD,5800,0);
    auto creature=duel_test_add(owner,DUEL_ZONE_GRAVEYARD,106,0);
    auto bolt=duel_test_add(owner,DUEL_ZONE_HAND,145,0);
    duel_test_prepare_fixture(); duel_test_prepare_graveyard_abilities();
    auto denied=[&](DuelTestRef ref) {
      auto before=observable_state(); EXPECT_FALSE(duel_test_can_cast_graveyard(ref));
      EXPECT_FALSE(duel_test_cast_graveyard(ref)); EXPECT_EQ(observable_state(),before);
    };
    denied({-1,0}); denied({owner,-1}); denied(creature);
    duel_test_set_life(owner,2); denied(source); duel_test_set_life(owner,20);
    duel_test_set_mana(owner,2,0); denied(source); duel_test_set_mana(owner,2,1);
    duel_test_set_turn(1-owner,0); denied(source);
    duel_test_set_turn(owner,1); denied(source);
    duel_test_set_turn(owner,0); ASSERT_TRUE(duel_test_can_cast_graveyard(source));
    duel_test_set_mana(owner,4,1); duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
    ASSERT_TRUE(duel_test_cast(bolt)); ASSERT_EQ(duel_test_stack_size(),1);
    denied(source); duel_test_resolve();
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
  }
}
TEST(Flashback, ActualMenuSelectsTheRequestedDuplicateAmongOtherLegalSpells) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123); duel_test_set_turn(owner,0);
    for(int p:{0,1})for(int c=0;c<6;++c)duel_test_set_mana(p,c,p==owner?(c==0?2:c==2?1:0):0);
    duel_test_add(owner,DUEL_ZONE_GRAVEYARD,8702,0); // Think Twice, another Flashback spell.
    duel_test_add(owner,DUEL_ZONE_GRAVEYARD,5800,0);
    auto source=duel_test_add(owner,DUEL_ZONE_GRAVEYARD,5800,0);
    for(int card:{91,188,106})duel_test_add(owner,DUEL_ZONE_LIBRARY,card,0);
    duel_test_prepare_fixture(); duel_test_prepare_graveyard_abilities();
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,-1);
    ASSERT_TRUE(duel_test_can_cast_graveyard(source)); ASSERT_TRUE(duel_test_cast_graveyard(source));
    EXPECT_EQ(duel_test_finish_choice_plan(),0); EXPECT_EQ(duel_test_stack_size(),0);
    EXPECT_EQ(duel_test_life(owner),17); EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),2);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,0),8702);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,1),5800);
    EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_EXILE,0),5800);
    EXPECT_EQ(duel_test_mana(owner,0),1); EXPECT_EQ(duel_test_mana(owner,2),0);
  }
}
TEST(UntapStep, RealPreparationAndSnapshotsPreserveTheOneStepBoundary) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123);
    for(int p:{0,1}) for(int c=0;c<6;++c)duel_test_set_mana(p,c,0);
    duel_test_set_turn(1-owner,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_TAPPED);
    auto ring=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,230,DUEL_STATE_TAPPED);
    auto land=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,91,DUEL_STATE_TAPPED);
    auto foreign=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_TAPPED);
    duel_test_prepare_fixture(); auto original=observable_state();
    EXPECT_FALSE(duel_test_untap_step(owner));
    EXPECT_EQ(observable_state(),original);
    duel_test_advance_turn();
    auto prepared=observable_state();
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> ready(duel_test_capture(),duel_test_free_snapshot);
    for(int bad:{-1,2,1-owner}) EXPECT_FALSE(duel_test_untap_step(bad));
    EXPECT_EQ(observable_state(),prepared);
    ASSERT_TRUE(duel_test_untap_step(owner));
    EXPECT_FALSE(duel_test_inspect(bear).tapped); EXPECT_FALSE(duel_test_inspect(ring).tapped);
    EXPECT_FALSE(duel_test_inspect(land).tapped); EXPECT_TRUE(duel_test_inspect(foreign).tapped);
    auto completed=observable_state();
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> consumed(duel_test_capture(),duel_test_free_snapshot);
    EXPECT_FALSE(duel_test_untap_step(owner)); EXPECT_EQ(observable_state(),completed);
    duel_test_restore(ready.get()); ASSERT_TRUE(duel_test_untap_step(owner));
    EXPECT_EQ(observable_state(),completed);
    duel_test_restore(consumed.get()); EXPECT_FALSE(duel_test_untap_step(owner));
    duel_test_set_turn(owner,0); EXPECT_FALSE(duel_test_untap_step(owner));
    EXPECT_EQ(observable_state(),completed);
    duel_test_restore(ready.get()); duel_test_set_turn(owner,0);
    EXPECT_FALSE(duel_test_untap_step(owner)); EXPECT_EQ(observable_state(),prepared);
  }
}
TEST(UntapStep, FloatingPoolsAndPendingSpellsRejectWithoutChangingTheBoard) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123);
    for(int p:{0,1}) for(int c=0;c<6;++c)duel_test_set_mana(p,c,0);
    duel_test_set_turn(1-owner,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_TAPPED);
    auto bolt=duel_test_add(owner,DUEL_ZONE_HAND,145,0);
    duel_test_prepare_fixture(); duel_test_advance_turn();
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> ready(duel_test_capture(),duel_test_free_snapshot);
    for(int player:{owner,1-owner}) {
      duel_test_set_mana(player,2,1); auto before=observable_state();
      EXPECT_FALSE(duel_test_untap_step(owner)); EXPECT_EQ(observable_state(),before);
      duel_test_set_mana(player,2,0);
    }
    ASSERT_TRUE(duel_test_untap_step(owner)); EXPECT_FALSE(duel_test_inspect(bear).tapped);
    duel_test_restore(ready.get()); duel_test_set_mana(owner,4,1);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
    ASSERT_TRUE(duel_test_cast(bolt)); ASSERT_EQ(duel_test_stack_size(),1);
    auto pending=observable_state(); EXPECT_FALSE(duel_test_untap_step(owner));
    EXPECT_EQ(observable_state(),pending);
    duel_test_restore(ready.get()); ASSERT_TRUE(duel_test_untap_step(owner));
  }
}
TEST(UntapStep, TheFullWrapperHonorsARealSkippedUntap) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"empty_main",123);
    for(int p:{0,1}) for(int c=0;c<6;++c)duel_test_set_mana(p,c,0);
    duel_test_set_turn(1-owner,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_TAPPED);
    auto ring=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,230,DUEL_STATE_TAPPED);
    duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,233,0); // Stasis.
    duel_test_prepare_fixture(); duel_test_advance_turn();
    ASSERT_TRUE(duel_test_untap_step(owner));
    EXPECT_TRUE(duel_test_inspect(bear).tapped); EXPECT_TRUE(duel_test_inspect(ring).tapped);
    EXPECT_FALSE(duel_test_untap_step(owner));
    EXPECT_EQ(duel_test_life(0),20); EXPECT_EQ(duel_test_life(1),20);
  }
}
static bool prepare_upkeep_control_fixture(int owner, int csvid, bool skipped=false) {
  Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
  for(int p:{0,1})for(int i=0;i<6;++i)duel_test_add(p,DUEL_ZONE_LIBRARY,91,0);
  if(skipped)duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,233,0); // Opponent's Stasis.
  auto source=duel_test_add(owner,DUEL_ZONE_HAND,csvid,0);
  duel_test_prepare_fixture();
  if(!duel_test_cast(source))return false;
  duel_test_resolve();
  for(int p:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(p,color,0);
  return duel_test_finish_choice_plan()==0;
}
TEST(UpkeepStep, RealUntapProvenanceAndSnapshotsResolveExactlyOnceWithoutADrawStep) {
  for(int owner:{0,1}) {
    ASSERT_TRUE(prepare_upkeep_control_fixture(owner,1110)); // Actual Phyrexian Arena cast.
    EXPECT_FALSE(duel_test_upkeep_step(owner));
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));
    ASSERT_TRUE(duel_test_upkeep_step(1-owner));EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),0);
    duel_test_advance_turn();auto before=observable_state();
    EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_EQ(observable_state(),before);
    ASSERT_TRUE(duel_test_untap_step(owner));EXPECT_EQ(g_current_phase,1); // PHASE_UNTAP.
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    before=observable_state();
    for(int bad:{-1,2,1-owner})EXPECT_FALSE(duel_test_upkeep_step(bad));
    EXPECT_EQ(observable_state(),before);duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),before);
    ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(g_current_phase,4); // Genuine PHASE_UPKEEP.
    EXPECT_EQ(duel_test_life(owner),19);EXPECT_EQ(duel_test_life(1-owner),20);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),1);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),5);
    EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_HAND),0);
    auto completed=observable_state();
    auto consumed=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_EQ(observable_state(),completed);
    duel_test_ai_roundtrip();EXPECT_FALSE(duel_test_upkeep_step(owner));
    duel_test_restore(ready.get());duel_test_ai_roundtrip();ASSERT_TRUE(duel_test_upkeep_step(owner));
    EXPECT_EQ(observable_state(),completed);EXPECT_EQ(g_current_phase,4);
    duel_test_restore(consumed.get());EXPECT_FALSE(duel_test_upkeep_step(owner));
    duel_test_restore(ready.get());duel_test_set_turn(owner,0);
    EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_FALSE(duel_test_untap_step(owner));
    duel_test_restore(consumed.get());
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));ASSERT_TRUE(duel_test_upkeep_step(1-owner));
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));ASSERT_TRUE(duel_test_upkeep_step(owner));
    EXPECT_EQ(duel_test_life(owner),18);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),4);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
  }
}
TEST(UpkeepStep, FloatingPoolsAndPendingSpellsRejectWithoutConsumingPreparation) {
  for(int owner:{0,1}) {
    ASSERT_TRUE(prepare_upkeep_control_fixture(owner,1110));
    duel_test_set_turn(1-owner,0);duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));
    for(int player:{0,1})for(int color=0;color<6;++color) {
      duel_test_set_mana(player,color,1);auto before=observable_state();auto host=duel_host_capture();
      EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_EQ(observable_state(),before);
      auto after=duel_host_capture();EXPECT_EQ(after.random_state,host.random_state);
      EXPECT_EQ(after.ticks,host.ticks);EXPECT_EQ(after.remaining_steps,host.remaining_steps);
      duel_test_set_mana(player,color,0);
    }
    ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(duel_test_life(owner),19);
    ASSERT_TRUE(prepare_upkeep_control_fixture(owner,1110));
    auto bolt=duel_test_add(owner,DUEL_ZONE_HAND,145,0);
    duel_test_set_mana(owner,4,1);duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
    ASSERT_TRUE(duel_test_cast(bolt));ASSERT_EQ(duel_test_stack_size(),1);
    auto pending=observable_state();EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_EQ(observable_state(),pending);
    duel_test_resolve();EXPECT_EQ(duel_test_finish_choice_plan(),0);
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));ASSERT_TRUE(duel_test_upkeep_step(1-owner));
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));ASSERT_TRUE(duel_test_upkeep_step(owner));
    EXPECT_EQ(duel_test_life(owner),19);EXPECT_EQ(duel_test_life(1-owner),17);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),1);duel_test_check_invariants();
  }
}
TEST(UpkeepStep, ControllerAndActivePlayerRecipientsRemainDistinctAcrossActualTurns) {
  for(int csvid:{1110,13606,7368})for(int owner:{0,1}) {
    SCOPED_TRACE(::testing::Message()<<"csvid="<<csvid<<" owner="<<owner);
    ASSERT_TRUE(prepare_upkeep_control_fixture(owner,csvid));
    int drawn[2]={0,0};
    for(int active:{1-owner,owner}) {
      duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(active));ASSERT_TRUE(duel_test_upkeep_step(active));
      if(csvid==7368)drawn[active]+=2; // Seizan affects the active player.
      else if(csvid==13606 || active==owner)++drawn[owner]; // Force each upkeep; Arena own only.
      for(int player:{0,1}) {
        EXPECT_EQ(duel_test_life(player),20-drawn[player]);
        EXPECT_EQ(duel_test_zone_count(player,DUEL_ZONE_HAND),drawn[player]);
        EXPECT_EQ(duel_test_zone_count(player,DUEL_ZONE_LIBRARY),6-drawn[player]);
      }
      EXPECT_EQ(g_current_phase,4);EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    }
    duel_test_check_invariants();
  }
}
TEST(UpkeepStep, RealSkippedUntapStillPreparesUpkeepAndMainPhaseActionsCannotRewindIt) {
  for(int owner:{0,1}) {
    ASSERT_TRUE(prepare_upkeep_control_fixture(owner,1110,true));
    auto bolt=duel_test_add(owner,DUEL_ZONE_HAND,145,0);
    duel_test_set_turn(1-owner,0);duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));
    EXPECT_EQ(g_current_phase,20); // Stasis returned before the real wrapper entered UNTAP.
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(g_current_phase,4);
    EXPECT_EQ(duel_test_life(owner),19);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);
    auto completed=observable_state();EXPECT_FALSE(duel_test_upkeep_step(owner));
    duel_test_restore(ready.get());duel_test_ai_roundtrip();ASSERT_TRUE(duel_test_upkeep_step(owner));
    EXPECT_EQ(observable_state(),completed);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    duel_test_restore(ready.get());
    duel_test_set_mana(owner,4,1);duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
    ASSERT_TRUE(duel_test_cast(bolt));duel_test_resolve();EXPECT_EQ(g_current_phase,20);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,4),0);
    auto acted=observable_state();EXPECT_FALSE(duel_test_upkeep_step(owner));EXPECT_EQ(observable_state(),acted);
    EXPECT_EQ(duel_test_life(owner),20);EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
  }
}
static bool prepare_declaration_control_fixture(Refs &refs, int defender, int source_id, int victim_id, int damage_spell=0) {
  load_fixture(refs,"empty_main",123);duel_test_set_turn(defender,0);
  refs["source"]=duel_test_add(defender,DUEL_ZONE_HAND,source_id,0);
  refs["victim"]=duel_test_add(1-defender,DUEL_ZONE_BATTLEFIELD,victim_id,DUEL_STATE_UNTAPPED);
  if(damage_spell) {
    refs["growth"]=duel_test_add(1-defender,DUEL_ZONE_HAND,97,0);
    refs["damage"]=duel_test_add(1-defender,DUEL_ZONE_HAND,damage_spell,0);
  }
  duel_test_prepare_fixture();
  if(!duel_test_cast(refs.at("source")))return false;
  duel_test_resolve();
  for(int p:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(p,color,0);
  duel_test_advance_turn();
  return duel_test_finish_choice_plan()==0;
}
TEST(BlockDeclaration, RealValorMultiblockKeepsOnlyPrintedPermanentsVisibleAcrossReplay) {
  for(int defender:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);int attacker=1-defender;
    duel_test_set_turn(attacker,0);
    auto source=duel_test_add(defender,DUEL_ZONE_BATTLEFIELD,106,0);
    auto first=duel_test_add(attacker,DUEL_ZONE_BATTLEFIELD,106,0);
    auto second=duel_test_add(attacker,DUEL_ZONE_BATTLEFIELD,106,0);
    auto valor=duel_test_add(defender,DUEL_ZONE_HAND,8312,0); // Valor Made Real, W instant.
    duel_test_prepare_fixture();
    for(int player:{0,1})for(int color=0;color<6;++color)
      duel_test_set_mana(player,color,player==defender && color==5?1:0);
    duel_test_begin_attackers();ASSERT_TRUE(duel_test_attack(first));ASSERT_TRUE(duel_test_attack(second));
    duel_test_finish_attackers();
    duel_test_decision(DUEL_CHOICE_TARGET,defender,defender,source.card);
    ASSERT_TRUE(duel_test_cast(valor));duel_test_resolve();
    EXPECT_EQ(duel_test_mana(defender,5),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(ready.get());
      duel_test_begin_blockers();ASSERT_TRUE(duel_test_block(source,first));
      ASSERT_TRUE(duel_test_block(source,second));duel_test_advance_combat();
      EXPECT_EQ(duel_test_inspect(source).blocking_count,2);
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_BATTLEFIELD),1);
      EXPECT_EQ(duel_test_zone_csvid(defender,DUEL_ZONE_BATTLEFIELD,0),106);
      EXPECT_EQ(duel_test_zone_csvid(defender,DUEL_ZONE_BATTLEFIELD,1),-1);
      EXPECT_EQ(duel_test_permanent_at(defender,0).card,source.card);
      EXPECT_EQ(duel_test_permanent_at(defender,1).card,-1);
      EXPECT_EQ(duel_test_zone_count(attacker,DUEL_ZONE_BATTLEFIELD),2);
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_inspect(source).csvid,106);EXPECT_TRUE(duel_test_inspect(source).in_play);
      EXPECT_EQ(duel_test_life(defender),20);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(replay)EXPECT_EQ(observable_state(),result);else result=observable_state();
      duel_test_check_invariants();
    }
  }
}
TEST(BlockDeclaration, QueuedDestructionCompletesBeforeDamageAndReplaysWithoutChangingOrdinaryBlocks) {
  for(int defender:{0,1})for(int source_id:{3666,106}) { // Alaborn Zealot / ordinary Bears.
    SCOPED_TRACE(::testing::Message()<<"defender="<<defender<<" source="<<source_id);
    Refs refs;ASSERT_TRUE(prepare_declaration_control_fixture(refs,defender,source_id,13603)); // Yoked Ox0/4.
    auto source=refs.at("source"),victim=refs.at("victim");
    duel_test_begin_attackers();ASSERT_TRUE(duel_test_attack(victim));duel_test_finish_attackers();
    duel_test_begin_blockers();ASSERT_TRUE(duel_test_block(source,victim));
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<2;++replay) {
      if(replay){duel_test_restore(ready.get());duel_test_ai_roundtrip();}
      duel_test_advance_combat();EXPECT_EQ(g_current_phase,24); // AFTER_BLOCKING, before damage.
      bool destroyed=source_id==3666;
      EXPECT_EQ(!!duel_test_inspect(source).in_play,!destroyed);
      EXPECT_EQ(!!duel_test_inspect(victim).in_play,!destroyed);
      for(int player:{0,1}) {
        EXPECT_EQ(duel_test_zone_count(player,DUEL_ZONE_GRAVEYARD),destroyed?1:0);
        EXPECT_EQ(duel_test_life(player),20);
      }
      if(destroyed) {
        EXPECT_EQ(duel_test_zone_csvid(defender,DUEL_ZONE_GRAVEYARD,0),3666);
        EXPECT_EQ(duel_test_zone_csvid(1-defender,DUEL_ZONE_GRAVEYARD,0),13603);
      } else {
        EXPECT_EQ(duel_test_inspect(source).damage,0);EXPECT_EQ(duel_test_inspect(victim).damage,0);
        EXPECT_EQ(duel_test_inspect(victim).blocked,1);
      }
      EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(replay)EXPECT_EQ(observable_state(),completed);else completed=observable_state();
      duel_test_check_invariants();
    }
  }
}
TEST(BlockDeclaration, RealRegenerationWindowHonorsAnActualIncinerateRestrictionAndSnapshots) {
  for(int defender:{0,1})for(bool cannot_regenerate:{false,true}) {
    SCOPED_TRACE(::testing::Message()<<"defender="<<defender<<" cannot_regenerate="<<cannot_regenerate);
    Refs refs;ASSERT_TRUE(prepare_declaration_control_fixture(refs,defender,3666,1056,cannot_regenerate?1254:145));
    auto source=refs.at("source"),victim=refs.at("victim");int attacker=1-defender;
    duel_test_set_mana(attacker,3,2);duel_test_set_mana(attacker,4,1);
    duel_test_set_mana(attacker,0,cannot_regenerate?1:0);
    duel_test_decision(DUEL_CHOICE_TARGET,attacker,victim.player,victim.card);
    ASSERT_TRUE(duel_test_cast(refs.at("growth")));duel_test_resolve();
    duel_test_decision(DUEL_CHOICE_TARGET,attacker,victim.player,victim.card);
    ASSERT_TRUE(duel_test_cast(refs.at("damage")));duel_test_resolve();
    ASSERT_TRUE(duel_test_inspect(victim).in_play);
    EXPECT_EQ(duel_test_inspect(victim).toughness,5);EXPECT_EQ(duel_test_inspect(victim).damage,3);
    EXPECT_EQ(duel_test_mana(attacker,3),1);
    duel_test_begin_attackers();ASSERT_TRUE(duel_test_attack(victim));duel_test_finish_attackers();
    duel_test_begin_blockers();ASSERT_TRUE(duel_test_block(source,victim));
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<2;++replay) {
      if(replay){duel_test_restore(ready.get());duel_test_ai_roundtrip();}
      duel_test_regeneration_decision(victim);duel_test_advance_combat();
      EXPECT_EQ(g_current_phase,24);EXPECT_FALSE(duel_test_inspect(source).in_play);
      EXPECT_EQ(!!duel_test_inspect(victim).in_play,!cannot_regenerate);
      // An ineffective regeneration activation may still be offered; the AI
      // can omit it. Payment follows the actual consumed action. Incinerate's
      // required destruction outcome above is independent of that UI policy.
      int unused=duel_test_finish_choice_plan();
      EXPECT_TRUE(unused==0 || (cannot_regenerate && unused==1));
      EXPECT_EQ(duel_test_mana(attacker,3),unused);
      if(!cannot_regenerate) {
        EXPECT_TRUE(duel_test_inspect(victim).tapped);EXPECT_FALSE(duel_test_inspect(victim).attacking);
        EXPECT_EQ(duel_test_inspect(victim).damage,0);
      }
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_zone_count(attacker,DUEL_ZONE_GRAVEYARD),cannot_regenerate?3:2);
      EXPECT_EQ(duel_test_life(0),20);EXPECT_EQ(duel_test_life(1),20);EXPECT_EQ(duel_test_stack_size(),0);
      if(replay)EXPECT_EQ(observable_state(),completed);else completed=observable_state();
      duel_test_check_invariants();
    }
  }
}
TEST(TargetEligibility, CandidatePredicateIsIndependentOfOtherLegalTargetsAndPreservesCallerInput) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"stocked_zones",123);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,1226,DUEL_STATE_UNTAPPED); // Visara.
    auto bear=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_UNTAPPED);
    auto land=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,91,DUEL_STATE_TAPPED);
    auto indestructible=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,11238,DUEL_STATE_UNTAPPED);
    duel_test_prepare_fixture(); ASSERT_TRUE(duel_test_can_activate(source));
    duel_test_decision(DUEL_CHOICE_TARGET,owner,bear.player,bear.card);
    auto before=observable_state(); auto host=duel_host_capture();
    EXPECT_EQ(duel_test_probe_activation_target(source,land,-1),0);
    EXPECT_EQ(duel_test_probe_activation_target(source,bear,-1),1);
    EXPECT_EQ(duel_test_probe_activation_target(source,source,-1),1);
    EXPECT_EQ(duel_test_probe_activation_target(source,indestructible,-1),1);
    EXPECT_EQ(observable_state(),before);
    auto after=duel_host_capture();
    EXPECT_EQ(after.random_state,host.random_state); EXPECT_EQ(after.ticks,host.ticks);
    EXPECT_EQ(after.remaining_steps,host.remaining_steps);
    int player,card; EXPECT_FALSE(duel_host_target_probe_candidate(&player,&card));
    ASSERT_TRUE(duel_test_activate(source)); EXPECT_EQ(duel_test_finish_choice_plan(),0);
    duel_test_resolve(); EXPECT_FALSE(duel_test_inspect(bear).in_play);
    EXPECT_TRUE(duel_test_inspect(indestructible).in_play);
  }
}
TEST(TargetEligibility, ManaSourceFallbackInspectsActualNonbasicLandAbilityWithoutKeepingCosts) {
  for(int owner:{0,1}) {
    Refs refs; load_fixture(refs,"stocked_zones",123);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,3329,DUEL_STATE_UNTAPPED); // Wasteland.
    auto basic=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,91,DUEL_STATE_TAPPED);
    auto nonbasic=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,241,DUEL_STATE_TAPPED);
    duel_test_prepare_fixture(); auto before=observable_state();
    duel_test_decision(DUEL_CHOICE_NUMBER,owner,5,-1);
    EXPECT_EQ(duel_test_probe_activation_target(source,basic,1),0);
    EXPECT_EQ(duel_test_probe_activation_target(source,nonbasic,1),1);
    EXPECT_EQ(observable_state(),before); EXPECT_TRUE(duel_test_inspect(source).in_play);
    EXPECT_FALSE(duel_test_inspect(source).tapped); EXPECT_EQ(duel_test_finish_choice_plan(),1);
  }
}
TEST(TargetEligibility, TargetlessAbilityRemainsUnknownAndSnapshotsRestoreProbePolicy) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(0,DUEL_ZONE_BATTLEFIELD,230,DUEL_STATE_UNTAPPED); // Sol Ring.
  auto candidate=duel_test_add(1,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_UNTAPPED);
  duel_test_prepare_fixture(); auto before=observable_state();
  EXPECT_EQ(duel_test_probe_activation_target(source,candidate,-1),-1);
  EXPECT_EQ(observable_state(),before);
  duel_host_probe_target(candidate.player,candidate.card);
  duel_host_observe_target_legal(1);
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
  duel_host_observe_target_legal(0); EXPECT_EQ(duel_host_target_probe_result(),-2);
  duel_test_restore(saved.get()); EXPECT_EQ(duel_host_target_probe_result(),1);
  duel_host_set_ai_choices(1); int player,card;
  EXPECT_FALSE(duel_host_target_probe_candidate(&player,&card));
  duel_host_set_ai_choices(0); EXPECT_TRUE(duel_host_target_probe_candidate(&player,&card));
  EXPECT_EQ(player,candidate.player); EXPECT_EQ(card,candidate.card);
  duel_host_probe_target(-1,-1);
}
TEST(RemovalRegeneration, BlockedAttemptsRemainUnusedAndLegalRegenerationIsConsumed) {
  for(int csvid:{16909,1471,12594}) { // Final Reward, Terminate, Murder.
    Refs refs; load_fixture(refs,"stocked_zones",123);
    auto victim=duel_test_add(1,DUEL_ZONE_BATTLEFIELD,1056,DUEL_STATE_UNTAPPED); // Horned Troll.
    auto source=duel_test_add(0,DUEL_ZONE_HAND,csvid,0);
    duel_test_prepare_fixture();
    duel_test_decision(DUEL_CHOICE_TARGET,0,victim.player,victim.card);
    ASSERT_TRUE(duel_test_cast(source));
    duel_test_regeneration_decision(victim);
    duel_test_resolve();
    const bool regenerates=csvid==12594;
    EXPECT_EQ(duel_test_finish_choice_plan(),regenerates?0:1)<<csvid;
    EXPECT_EQ(!!duel_test_inspect(victim).in_play,regenerates)<<csvid;
    EXPECT_EQ(duel_test_mana(1,3),regenerates?9:10)<<csvid;
    duel_test_check_invariants();
  }
}
TEST(SearchObservation, RejectedNamedChoiceDeclinesWithoutChangingThePredicate) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(0,DUEL_ZONE_HAND,1124,0); // Rampant Growth.
  duel_test_prepare_fixture(); duel_host_observe_zone_legality(1);
  ASSERT_TRUE(duel_test_cast(source));
  duel_test_decision(DUEL_CHOICE_ZONE,0,-3,106); // A creature is not a basic land.
  duel_test_resolve();
  EXPECT_EQ(duel_test_finish_choice_plan(),0);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),7);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_HAND),4);
  EXPECT_EQ(duel_test_offered_selection_required(),0);
  bool found=false;
  for(int c=0;c<duel_test_offered_count();++c) if(duel_test_offered_csvid(c)==106) {
    found=true; EXPECT_EQ(duel_test_offered_legal(c),0);
  }
  EXPECT_TRUE(found);
}
TEST(SearchObservation, ImpossibleRequiredSelectionPreservesTheIncorrectPolicyAndPredicate) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(0,DUEL_ZONE_HAND,10692,0); // Trapmaker's Snare.
  duel_test_prepare_fixture(); duel_host_observe_zone_legality(1);
  ASSERT_TRUE(duel_test_cast(source));
  duel_test_decision(DUEL_CHOICE_ZONE,0,-5,106); // No Trap in this library.
  duel_test_resolve();
  EXPECT_EQ(duel_test_finish_choice_plan(),0);
  EXPECT_EQ(duel_test_offered_selection_required(),1);
  EXPECT_EQ(duel_test_forced_zone_choices(),1);
  EXPECT_EQ(duel_test_offered_count(),7);
  for(int c=0;c<duel_test_offered_count();++c) EXPECT_EQ(duel_test_offered_legal(c),0);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),7);
  EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_HAND),4);
}
TEST(SearchObservation, SnapshotRestoresSelectionPolicyEligibilityAndObservationFlag) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto source=duel_test_add(0,DUEL_ZONE_HAND,1124,0);
  duel_test_prepare_fixture(); duel_host_observe_zone_legality(1);
  ASSERT_TRUE(duel_test_cast(source));
  duel_test_decision(DUEL_CHOICE_ZONE,0,-3,91);
  duel_test_resolve(); ASSERT_EQ(duel_test_finish_choice_plan(),0);
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
  duel_host_observe_zone_legality(0);
  const int cards[]={0},legal[]={1};
  duel_test_decision(DUEL_CHOICE_ZONE,0,0,-1);
  ASSERT_EQ(duel_host_choose_zone(0,"selection policy snapshot probe",1,cards,1,legal),0);
  ASSERT_EQ(duel_test_finish_choice_plan(),0);
  ASSERT_EQ(duel_test_offered_selection_required(),1);
  duel_test_restore(saved.get());
  EXPECT_TRUE(duel_host_observing_zone_legality());
  EXPECT_EQ(duel_test_offered_selection_required(),0);
  EXPECT_EQ(duel_test_offered_count(),7);
  bool found=false;
  for(int c=0;c<duel_test_offered_count();++c) if(duel_test_offered_csvid(c)==106) {
    found=true; EXPECT_EQ(duel_test_offered_legal(c),0);
  }
  EXPECT_TRUE(found);
}
TEST(CharacteristicValues, CrowdedBattlefieldsUseAllCardSlotsWithoutSignedIndexOverflow) {
  for(int player:{0,1})for(int count:{17,128,140}) {
    Refs refs; load_fixture(refs,"empty_main",123);
    for(int c=0;c<count;++c)duel_test_add(player,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_UNTAPPED);
    duel_test_add(1-player,DUEL_ZONE_BATTLEFIELD,2085,DUEL_STATE_UNTAPPED);
    duel_test_prepare_fixture();auto initial=observable_state();
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
    int score=ai_opinion_of_gamestate_continued(player,0);
    duel_test_restore(saved.get());EXPECT_EQ(observable_state(),initial);
    EXPECT_EQ(ai_opinion_of_gamestate_continued(player,0),score);
    duel_test_restore(saved.get());EXPECT_EQ(observable_state(),initial);
  }
}
TEST(LibraryReveal, InformationalDisplayPreservesCountOrderAndPendingSelectionAcrossSnapshots) {
  for(int owner:{0,1})for(int size:{0,1,4})for(int requested:{0,1,3,8}) {
    Refs refs;load_fixture(refs,"empty_main",123);
    const int cards[]={91,106,126,188};
    for(int n=0;n<size;++n)duel_test_add(owner,DUEL_ZONE_LIBRARY,cards[n],0);
    duel_test_prepare_fixture();auto initial=observable_state();
    // An informational reveal must leave the following selector's input alone.
    duel_test_decision(DUEL_CHOICE_ZONE,owner,-1,-1);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      auto host=duel_host_capture();
      EXPECT_EQ(manalinkeh_reveal_top_cards_of_library(owner,requested),(std::min)(size,requested));
      EXPECT_EQ(observable_state(),initial);
      auto after=duel_host_capture();
      EXPECT_EQ(after.random_state,host.random_state);EXPECT_EQ(after.ticks,host.ticks);
      EXPECT_EQ(after.remaining_steps,host.remaining_steps);
      EXPECT_EQ(duel_host_choose_zone(owner,"selector after informational reveal",0,nullptr,0,nullptr),-1);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),initial);duel_test_check_invariants();
    }
  }
}
TEST(LibraryReveal, EmptyAndOneCardSelectionStayBoundedAcrossOwnersAndSnapshots) {
  for(int chooser:{0,1})for(int owner:{0,1})for(int count:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);
    if(count)duel_test_add(owner,DUEL_ZONE_LIBRARY,91,0);
    for(int id:{106,126})duel_test_add(1-owner,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();auto initial=observable_state();
    if(count)duel_test_decision(DUEL_CHOICE_ZONE,chooser,0,-1);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      int selected=manalinkeh_select_one_and_put_the_rest_on_bottom(chooser,owner,3);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),0);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),count);
      EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_LIBRARY),2);
      EXPECT_EQ(duel_test_zone_csvid(1-owner,DUEL_ZONE_LIBRARY,0),106);
      EXPECT_EQ(duel_test_zone_csvid(1-owner,DUEL_ZONE_LIBRARY,1),126);
      if(count) {
        ASSERT_GE(selected,0);auto card=duel_test_inspect({owner,selected});
        EXPECT_TRUE(card.in_hand);EXPECT_FALSE(card.in_play);EXPECT_EQ(card.csvid,91);
      } else {
        EXPECT_EQ(selected,-1);EXPECT_EQ(observable_state(),initial);
      }
      if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),result);duel_test_check_invariants();
    }
  }
}
TEST(LibraryReveal, AiTrialNoticesPreserveRealAcknowledgementsAndFollowingSelector) {
  for(int owner:{0,1})for(int csvid:{12897,13181,5997}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,csvid,0);
    if(csvid==12897) {
      for(int id:{91,106,188})duel_test_add(owner,DUEL_ZONE_LIBRARY,id,0);
    } else {
      for(int id:{91,188,126,239})duel_test_add(1-owner,DUEL_ZONE_LIBRARY,id,0);
    }
    duel_test_prepare_fixture();duel_test_begin_attackers();ASSERT_TRUE(duel_test_attack(source));
    duel_test_finish_attackers();duel_test_begin_blockers();duel_test_advance_combat();
    const int notices=csvid==13181?1:2;
    if(csvid==13181)duel_test_decision(DUEL_CHOICE_ZONE,0,-1,-1);
    else for(int player:{owner,1-owner})duel_test_decision(DUEL_CHOICE_ZONE,player,-1,-1);
    duel_test_decision(DUEL_CHOICE_ZONE,owner,-1,-1); // A later real selector.
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    const auto before=observable_state();
    {
      struct RestorePolicy {
        int mode,depth,choices;
        ~RestorePolicy(){g_duel_ai_mode_state=mode;g_ai_speculation_depth=depth;duel_host_set_ai_choices(choices);}
      } policy{g_duel_ai_mode_state,g_ai_speculation_depth,duel_host_capture().ai_choices};
      g_duel_ai_mode_state=1;g_ai_speculation_depth=0;duel_host_set_ai_choices(0);
      duel_test_combat_damage();duel_test_resolve();duel_test_end_combat();
    }
    auto trial=observable_state();EXPECT_NE(trial,before);
    EXPECT_EQ(duel_test_finish_choice_plan(),notices+1);
    duel_test_restore(ready.get());
    duel_test_combat_damage();duel_test_resolve();duel_test_end_combat();
    EXPECT_EQ(observable_state(),trial);
    EXPECT_EQ(duel_host_choose_zone(owner,"selector after real informational display",0,nullptr,0,nullptr),-1);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
  }
}
TEST(CounterObservation, RealChargeCostsAndSnapshotsPreserveCountsWithoutChangingQueryState) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,11523,0); // Shriekhorn: three charge, tap/remove one, mill two.
    for(int id:{91,106,126})duel_test_add(1-owner,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();
    EXPECT_EQ(duel_test_counter(source,"charge"),-1); // A hand slot is not a permanent.
    ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();
    EXPECT_EQ(duel_test_counter(source,"charge"),3);EXPECT_EQ(duel_test_counter(source,"loyalty"),0);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      EXPECT_EQ(duel_test_counter(source,"charge"),3);
      auto state=observable_state();auto host=duel_host_capture();
      EXPECT_EQ(duel_test_counter(source,"charge"),3);EXPECT_EQ(duel_test_counter(source,"bogus"),-1);
      EXPECT_EQ(duel_test_counter({-1,0},"charge"),-1);EXPECT_EQ(duel_test_counter({2,0},"charge"),-1);
      EXPECT_EQ(duel_test_counter({owner,-1},"charge"),-1);EXPECT_EQ(duel_test_counter({owner,150},"charge"),-1);
      EXPECT_EQ(duel_test_counter(source,nullptr),-1);EXPECT_EQ(observable_state(),state);
      auto after=duel_host_capture();EXPECT_EQ(after.random_state,host.random_state);
      EXPECT_EQ(after.ticks,host.ticks);EXPECT_EQ(after.remaining_steps,host.remaining_steps);
      duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
      ASSERT_TRUE(duel_test_activate(source));
      EXPECT_EQ(duel_test_counter(source,"charge"),2);EXPECT_TRUE(duel_test_inspect(source).tapped);
      EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_LIBRARY),3); // Cost paid before the effect.
      duel_test_resolve();EXPECT_EQ(duel_test_counter(source,"charge"),2);
      EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_LIBRARY),1);
      EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_GRAVEYARD),2);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
    }
  }
}
TEST(CounterObservation, GrowthCallbackReadsStaySeparateAndReplayWithoutMutation) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,19739,0); // Simic Ascendancy.
    auto target=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    duel_test_prepare_fixture();
    EXPECT_EQ(duel_test_counter(source,"growth"),-1);
    ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();
    EXPECT_EQ(duel_test_counter(source,"growth"),0);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,target.card);
    ASSERT_TRUE(duel_test_activate(source));EXPECT_EQ(duel_test_stack_size(),1);
    EXPECT_EQ(duel_test_counter(target,"plus1"),0);
    EXPECT_EQ(duel_test_counter(source,"growth"),0);
    auto pending=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(pending.get());
      duel_test_resolve();EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_counter(target,"plus1"),1);
      EXPECT_EQ(duel_test_counter(source,"growth"),1);
      EXPECT_EQ(duel_test_counter(source,"charge"),0);
      EXPECT_EQ(duel_test_counter(source,"plus1"),0);
      auto state=observable_state();auto host=duel_host_capture();
      EXPECT_EQ(duel_test_counter(source,"growth"),1);
      auto after=duel_host_capture();EXPECT_EQ(after.random_state,host.random_state);
      EXPECT_EQ(after.ticks,host.ticks);EXPECT_EQ(after.remaining_steps,host.remaining_steps);
      EXPECT_EQ(observable_state(),state);duel_test_ai_roundtrip();
      EXPECT_EQ(duel_test_counter(source,"growth"),1);EXPECT_EQ(observable_state(),state);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
    }
  }
}
TEST(CounterObservation, RealArcaneKiLoreAndPaidCounterCostsStayDistinctAcrossSnapshots) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto altar=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,7438,0);
    auto scroll=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,14620,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto arcane=duel_test_add(owner,DUEL_ZONE_HAND,7353,0); // Reach Through Mists, real U/Arcane/draw.
    for(int id:{91,106})duel_test_add(owner,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();
    for(int color=0;color<6;++color)duel_test_set_mana(owner,color,color==2?1:0);
    EXPECT_EQ(duel_test_counter(altar,"ki"),0);EXPECT_EQ(duel_test_counter(scroll,"lore"),0);
    EXPECT_EQ(duel_test_counter(arcane,"ki"),-1);
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(ready.get());
      if(owner==0)duel_test_source_trigger_decision(owner,altar);
      ASSERT_TRUE(duel_test_cast(arcane));duel_test_resolve();
      EXPECT_EQ(duel_test_mana(owner,2),0);
      EXPECT_EQ(duel_test_counter(altar,"ki"),1);EXPECT_EQ(duel_test_counter(scroll,"lore"),1);
      EXPECT_EQ(duel_test_counter(altar,"lore"),0);EXPECT_EQ(duel_test_counter(scroll,"ki"),0);
      EXPECT_EQ(duel_test_counter(altar,"charge"),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      for(int color=0;color<6;++color)duel_test_set_mana(owner,color,color==0?2:0);
      ASSERT_TRUE(duel_test_activate(altar));EXPECT_EQ(duel_test_stack_size(),1);
      EXPECT_EQ(duel_test_counter(altar,"ki"),0);EXPECT_EQ(duel_test_counter(scroll,"lore"),1);
      EXPECT_EQ(duel_test_mana(owner,0),0);EXPECT_TRUE(duel_test_inspect(altar).tapped);
      auto pending=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
      duel_test_resolve();EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),4);
      auto token_state=observable_state();duel_test_restore(pending.get());duel_test_resolve();
      EXPECT_EQ(observable_state(),token_state);EXPECT_EQ(duel_test_counter(altar,"ki"),0);
      duel_test_set_mana(owner,0,3);duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,bear.card);
      ASSERT_TRUE(duel_test_activate(scroll));EXPECT_EQ(duel_test_inspect(bear).power,2);
      EXPECT_EQ(duel_test_mana(owner,0),0);duel_test_resolve();
      EXPECT_EQ(duel_test_inspect(bear).power,3);EXPECT_EQ(duel_test_inspect(bear).toughness,3);
      auto state=observable_state();auto host=duel_host_capture();
      for(int n=0;n<3;++n) {EXPECT_EQ(duel_test_counter(altar,"ki"),0);EXPECT_EQ(duel_test_counter(scroll,"lore"),1);}
      auto after=duel_host_capture();EXPECT_EQ(after.random_state,host.random_state);
      EXPECT_EQ(after.ticks,host.ticks);EXPECT_EQ(after.remaining_steps,host.remaining_steps);
      EXPECT_EQ(observable_state(),state);duel_test_ai_roundtrip();
      EXPECT_EQ(duel_test_counter(scroll,"lore"),1);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(replay)EXPECT_EQ(observable_state(),result);else result=observable_state();
      duel_test_check_invariants();
    }
  }
}
TEST(CounterObservation, RealLoyaltyEntryAndActivationCostsReplayForBothControllers) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,9185,0); // Jace Beleren: starts three, +2 each player draws.
    for(int p:{0,1})for(int id:{91,106})duel_test_add(p,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();
    EXPECT_EQ(duel_test_counter(source,"loyalty"),3);EXPECT_EQ(duel_test_counter(source,"charge"),0);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      EXPECT_EQ(duel_test_counter(source,"loyalty"),3);
      duel_test_decision(DUEL_CHOICE_DIALOG,owner,0,-1);ASSERT_TRUE(duel_test_activate(source));
      EXPECT_EQ(duel_test_counter(source,"loyalty"),5);
      for(int p:{0,1})EXPECT_EQ(duel_test_zone_count(p,DUEL_ZONE_LIBRARY),2);
      duel_test_resolve();EXPECT_EQ(duel_test_counter(source,"loyalty"),5);
      for(int p:{0,1})EXPECT_EQ(duel_test_zone_count(p,DUEL_ZONE_LIBRARY),1);
      EXPECT_FALSE(duel_test_can_activate(source)); // Actual once-per-turn rule is retained.
      EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
    }
  }
}
TEST(TriggerSourceSelection, LiveSourceDefersAcrossEarlierWindowsAndReplaysWithoutLegacyAmbiguity) {
  Refs refs;load_fixture(refs,"empty_main",123);
  auto victim=duel_test_add(0,DUEL_ZONE_BATTLEFIELD,106,0);
  auto first=duel_test_add(0,DUEL_ZONE_HAND,12391,0); // Harvester of Souls, plus its same-display legacy.
  auto unrelated=duel_test_add(0,DUEL_ZONE_HAND,230,0); // Sol Ring.
  for(int id:{91,106,126,188,239,91})duel_test_add(0,DUEL_ZONE_LIBRARY,id,0);
  duel_test_prepare_fixture();
  ASSERT_TRUE(duel_test_cast(first));duel_test_resolve();
  duel_test_reset_death_history();
  duel_test_source_trigger_decision(0,first);duel_test_decision(DUEL_CHOICE_DIALOG,0,0,-1);
  auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
  std::string result;
  for(int replay=0;replay<2;++replay) {
    if(replay)duel_test_restore(saved.get());
    ASSERT_TRUE(duel_test_cast(unrelated));duel_test_resolve();
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),6);
    duel_test_destroy(victim);
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_LIBRARY),5);
    EXPECT_EQ(duel_test_zone_count(0,DUEL_ZONE_HAND),1);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
    EXPECT_TRUE(duel_test_inspect(first).in_play);
    if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
    duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),result);duel_test_check_invariants();
  }
}
TEST(OptionalTargetReceipt, RealCancellationAndMandatoryFallbackRetainPolicyAndSnapshotCounts) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto bear=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto growth=duel_test_add(owner,DUEL_ZONE_HAND,97,0); // Giant Growth permits cancelling its cast target.
    auto stalker=duel_test_add(owner,DUEL_ZONE_HAND,8526,0); // Dream Stalker's own-permanent return is mandatory.
    duel_test_prepare_fixture();
    duel_test_optional_target_decision(owner,bear);
    EXPECT_FALSE(duel_test_cast(growth));EXPECT_EQ(duel_test_forced_target_choices(),0);
    EXPECT_TRUE(duel_test_inspect(growth).in_hand);EXPECT_EQ(duel_test_inspect(bear).power,2);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      EXPECT_EQ(duel_test_forced_target_choices(),0);
      duel_test_optional_target_decision(owner,bear);
      ASSERT_TRUE(duel_test_cast(stalker));duel_test_resolve();
      EXPECT_EQ(duel_test_forced_target_choices(),1);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);EXPECT_TRUE(duel_test_inspect(stalker).in_play);
      int bears=0,growths=0;
      for(int n=0;n<2;++n) { int id=duel_test_zone_csvid(owner,DUEL_ZONE_HAND,n);bears+=id==106;growths+=id==97; }
      EXPECT_EQ(bears,1);EXPECT_EQ(growths,1);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
      if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
      duel_test_ai_roundtrip();EXPECT_EQ(duel_test_forced_target_choices(),1);
      EXPECT_EQ(observable_state(),result);duel_test_check_invariants();
    }
  }
}
TEST(AuraEntry, RealDirectPositiveAuraEntryAttachesWithoutManaAndReplaysForBothControllers) {
  for(int owner:{0,1})for(int receiver:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto bear=duel_test_add(receiver,DUEL_ZONE_BATTLEFIELD,106,0);
    auto aura=duel_test_add(owner,DUEL_ZONE_HAND,112,0); // Holy Strength, +1/+2.
    auto removal=duel_test_add(1-owner,DUEL_ZONE_HAND,1096,0); // Naturalize, real 1G.
    duel_test_prepare_fixture();
    for(int player:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(player,color,0);
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(ready.get());
      duel_test_decision(DUEL_CHOICE_TARGET,owner,receiver,bear.card);
      manalinkeh_put_into_play(owner,aura.card); // Exact production entry used by library tutors.
      duel_test_refresh();
      EXPECT_TRUE(duel_test_inspect(aura).in_play);EXPECT_FALSE(duel_test_inspect(aura).in_hand);
      EXPECT_EQ(duel_test_inspect(bear).power,3);EXPECT_EQ(duel_test_inspect(bear).toughness,4);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),1+(receiver==owner));
      EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      for(int player:{0,1})for(int color=0;color<6;++color)EXPECT_EQ(duel_test_mana(player,color),0);
      duel_test_ai_roundtrip();EXPECT_EQ(duel_test_inspect(bear).power,3);
      duel_test_set_mana(1-owner,0,1);duel_test_set_mana(1-owner,3,1);
      duel_test_decision(DUEL_CHOICE_TARGET,1-owner,owner,aura.card);
      ASSERT_TRUE(duel_test_cast(removal));duel_test_resolve();duel_test_refresh();
      EXPECT_FALSE(duel_test_inspect(aura).in_play);
      EXPECT_EQ(duel_test_inspect(bear).power,2);EXPECT_EQ(duel_test_inspect(bear).toughness,2);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_zone_csvid(owner,DUEL_ZONE_GRAVEYARD,0),112);
      EXPECT_EQ(duel_test_mana(1-owner,0),0);EXPECT_EQ(duel_test_mana(1-owner,3),0);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(replay)EXPECT_EQ(observable_state(),result);else result=observable_state();
      duel_test_check_invariants();
    }
  }
}

TEST(ActivationModeIntent, MenusAutomaticValuesAndManaFallbacksStayDistinctAcrossSnapshots) {
  for(int owner:{0,1})for(int budget:{0,3}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,4138,0); // Heart Warden: mana or {2}/sacrifice/draw.
    for(int id:{91,106})duel_test_add(owner,DUEL_ZONE_LIBRARY,id,0);
    duel_test_prepare_fixture();
    for(int color=0;color<6;++color)duel_test_set_mana(owner,color,color==0?budget:0);
    duel_test_refresh();duel_test_activation_mode_decision(owner,source,"Sac & draw a card",3,1);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      ASSERT_TRUE(duel_test_activate(source));
      EXPECT_EQ(duel_test_finish_activation_mode(),budget?1:0);
      if(budget) {
        EXPECT_EQ(duel_test_stack_size(),1);EXPECT_EQ(duel_test_mana(owner,0),1);
        EXPECT_FALSE(duel_test_inspect(source).in_play);duel_test_resolve();
        /* Heart Warden's hardcoded AI path has a separate ordinary draw
           defect. This guard verifies mode intent/costs, not that defect. */
        if(owner==0) {
          EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),1);
          EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),1);
        }
      } else {
        EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,3),1);
        EXPECT_TRUE(duel_test_inspect(source).in_play);EXPECT_TRUE(duel_test_inspect(source).tapped);
        EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),2);
      }
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),result);duel_test_check_invariants();
    }
  }
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto canopy=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,8945,0);
    duel_test_prepare_fixture();for(int c=0;c<6;++c)duel_test_set_mana(owner,c,0);duel_test_refresh();
    duel_test_activation_mode_decision(owner,canopy,"Sac and draw a card",3,1);
    ASSERT_TRUE(duel_test_activate(canopy));EXPECT_LT(duel_test_finish_activation_mode(),1);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,3),1);EXPECT_EQ(duel_test_life(owner),19);
    duel_test_check_invariants();
    load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto backwoods=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,12194,0);
    auto victim=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    duel_test_prepare_fixture();for(int c=0;c<6;++c)duel_test_set_mana(owner,c,0);duel_test_refresh();
    duel_test_activation_mode_decision(owner,backwoods,"Sac crit & draw",0,1);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,victim.card); // Conditional on selecting the value mode.
    ASSERT_TRUE(duel_test_activate(backwoods));EXPECT_EQ(duel_test_finish_activation_mode(),0);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,0),1);
    EXPECT_TRUE(duel_test_inspect(victim).in_play);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    duel_test_check_invariants();
  }
}
TEST(ActivationHeuristics, ManaMarkerDoesNotResolveNonmanaModesAcrossResetAndSnapshots) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    EXPECT_EQ(tapped_for_mana_color,-1);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,8130,0); // Skarrg.
    auto ally=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0);
    auto twiddle=duel_test_add(owner,DUEL_ZONE_HAND,256,0);
    duel_test_prepare_fixture();
    for(int color=0;color<6;++color)duel_test_set_mana(owner,color,0);
    duel_test_refresh();
    auto clean=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    ASSERT_TRUE(duel_test_activate(source)); // Only the actual Add C mode is affordable.
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,0),1);
    EXPECT_TRUE(duel_test_inspect(source).tapped);EXPECT_EQ(tapped_for_mana_color,0);
    auto mana=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    duel_test_restore(clean.get());EXPECT_EQ(tapped_for_mana_color,-1);
    duel_test_restore(mana.get());EXPECT_EQ(tapped_for_mana_color,0);
    // Real untap, then a real paid nonmana activation in the same game.
    duel_test_set_mana(owner,2,1);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,source.card);
    ASSERT_TRUE(duel_test_cast(twiddle));
    duel_test_decision(DUEL_CHOICE_DIALOG,owner,1,-1);duel_test_resolve();
    EXPECT_FALSE(duel_test_inspect(source).tapped);
    duel_test_set_mana(owner,3,1);duel_test_set_mana(owner,4,1);
    duel_test_activation_mode_decision(owner,source,"Pump a creature",0,0);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,ally.card);
    auto before=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(before.get());
      ASSERT_TRUE(duel_test_activate(source));EXPECT_EQ(duel_test_finish_activation_mode(),1);
      EXPECT_EQ(duel_test_stack_size(),1);EXPECT_EQ(duel_test_inspect(ally).power,2);
      EXPECT_EQ(duel_test_mana(owner,3),0);EXPECT_EQ(duel_test_mana(owner,4),0);
      EXPECT_TRUE(duel_test_inspect(source).tapped);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      auto pending=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
      duel_test_resolve();EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_inspect(ally).power,3);EXPECT_EQ(duel_test_inspect(ally).toughness,3);
      if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
      duel_test_restore(pending.get());duel_test_resolve();EXPECT_EQ(observable_state(),result);
      duel_test_check_invariants();
    }
    // Seed the next fixture with the real mana receipt, proving reset isolation.
    duel_test_restore(mana.get());EXPECT_EQ(tapped_for_mana_color,0);
    load_fixture(refs,"empty_main",123);EXPECT_EQ(tapped_for_mana_color,-1);
  }
}
TEST(ActivationHeuristics, ManaBatteryHostOptionsSurviveQueriesAndPendingSnapshots) {
  for(int owner:{0,1})for(int id:{580,584,662,765,852,4516}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,id,0);
    duel_test_prepare_fixture();
    for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?10:0);
    duel_test_refresh();
    // The query parses both menu strings before it displays any menu.
    ASSERT_TRUE(duel_test_can_activate(source));
    EXPECT_EQ(duel_test_counter(source,"charge"),0);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_mana(owner,0),10);
    duel_test_activation_mode_decision(owner,source,"Charge battery (add counter).",0,1);
    ASSERT_TRUE(duel_test_activate(source));ASSERT_EQ(duel_test_finish_activation_mode(),1);
    EXPECT_EQ(duel_test_stack_size(),1);EXPECT_TRUE(duel_test_inspect(source).tapped);
    EXPECT_EQ(duel_test_counter(source,"charge"),0);
    auto pending=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(pending.get());
      duel_test_resolve();EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_counter(source,"charge"),1);
      EXPECT_EQ(duel_test_mana(owner,0),id==4516?9:8);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);duel_test_check_invariants();
    }
  }
}
TEST(ActivationHeuristics, DisabledGraveyardModeDoesNotPromptBeforeTradingPostChoice) {
  for(int owner:{0,1})for(int nonartifact_grave:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,12632,0);
    duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,0); // Grizzly Bears enables the creature cost; Post is the only artifact.
    duel_test_add(owner,DUEL_ZONE_HAND,145,0); // Lightning Bolt; discard mode is also legal.
    duel_test_add(owner,DUEL_ZONE_LIBRARY,91,0);
    if(nonartifact_grave)duel_test_add(owner,DUEL_ZONE_GRAVEYARD,145,0);
    duel_test_prepare_fixture();
    for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?1:0);
    duel_test_refresh();
    duel_test_activation_mode_decision(owner,source,"Draw a card.",0,0);
    if(owner==0)duel_test_decision(DUEL_CHOICE_TARGET,owner,owner,source.card);
    auto saved=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string result;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      ASSERT_TRUE(duel_test_can_activate(source));ASSERT_TRUE(duel_test_activate(source));
      ASSERT_EQ(duel_test_finish_activation_mode(),1);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),1);
      EXPECT_EQ(duel_test_mana(owner,0),0);EXPECT_FALSE(duel_test_inspect(source).in_play);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1+nonartifact_grave);
      duel_test_resolve();EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_HAND),2);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),0);
      EXPECT_EQ(duel_test_life(owner),20);EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(!replay)result=observable_state();else EXPECT_EQ(observable_state(),result);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),result);duel_test_check_invariants();
    }
  }
}
TEST(CastAnnouncement, RealHistoryAndCounterResolutionSurviveBothSnapshotBoundaries) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(1-owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,12461,0); // Second Guess.
    auto precursor=duel_test_add(1-owner,DUEL_ZONE_HAND,514,0); // Ornithopter.
    auto parent=duel_test_add(1-owner,DUEL_ZONE_HAND,106,0); // Grizzly Bears.
    duel_test_prepare_fixture();
    for(int player:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(player,color,10);
    duel_test_refresh();
    EXPECT_EQ(manalinkeh_get_storm_count(),0);
    ASSERT_TRUE(duel_test_cast(precursor));duel_test_resolve();
    EXPECT_EQ(manalinkeh_get_storm_count(),1);
    ASSERT_TRUE(duel_test_cast(parent));ASSERT_EQ(duel_test_stack_size(),1);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> before(duel_test_capture(),duel_test_free_snapshot);
    ASSERT_TRUE(duel_test_announce_spell(parent));
    EXPECT_EQ(manalinkeh_get_storm_count(),2);EXPECT_EQ(duel_test_stack_size(),1);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> after(duel_test_capture(),duel_test_free_snapshot);
    std::string resolved;
    for(int replay=0;replay<3;++replay) {
      if(replay==1)duel_test_restore(after.get());
      if(replay==2) {duel_test_restore(before.get());ASSERT_TRUE(duel_test_announce_spell(parent));}
      ASSERT_TRUE(duel_test_cast_response(source));EXPECT_EQ(duel_test_stack_size(),2);
      duel_test_resolve_response();
      EXPECT_EQ(manalinkeh_get_storm_count(),3); // No second announcement of the parent.
      EXPECT_EQ(duel_test_stack_size(),0);EXPECT_FALSE(duel_test_inspect(parent).in_play);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_zone_count(1-owner,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      if(!replay)resolved=observable_state();else EXPECT_EQ(observable_state(),resolved);
    }
  }
}
TEST(CastAnnouncement, RealResponseParentContextSupportsRecursiveCounterCopiesAndReplay) {
  for(int owner:{0,1})for(bool paid:{false,true}) {
    SCOPED_TRACE(::testing::Message()<<"owner="<<owner<<" paid="<<paid);
    int defender=1-owner;
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(defender,0);
    auto precursor=duel_test_add(defender,DUEL_ZONE_HAND,514,0);
    auto parent=duel_test_add(defender,DUEL_ZONE_HAND,999,0); // Concentrate.
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,6540,0); // Hindering Touch, tax2.
    for(int csvid:{106,91,188})duel_test_add(defender,DUEL_ZONE_LIBRARY,csvid,0);
    duel_test_prepare_fixture();ASSERT_TRUE(duel_test_cast(precursor));duel_test_resolve();
    duel_test_set_mana(defender,0,2);duel_test_set_mana(defender,2,2);
    ASSERT_TRUE(duel_test_cast(parent));ASSERT_TRUE(duel_test_announce_spell(parent));
    ASSERT_EQ(manalinkeh_get_storm_count(),2);
    for(int player:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(player,color,0);
    duel_test_set_mana(defender,0,paid?6:2);
    duel_test_set_mana(owner,0,3);duel_test_set_mana(owner,2,1);
    ASSERT_TRUE(duel_test_cast_response(source));EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_mana(owner,0),0);EXPECT_EQ(duel_test_mana(owner,2),0);
    for(int copy=0;copy<(paid?2:1);++copy)duel_test_decision(DUEL_CHOICE_DIALOG,defender,0,-1);
    auto before=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> announced(nullptr,duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<3;++replay) {
      if(replay==1)duel_test_restore(before.get());
      if(replay==2)duel_test_restore(announced.get());
      if(replay!=2)ASSERT_TRUE(duel_test_announce_spell(source));
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      EXPECT_EQ(manalinkeh_get_storm_count(),3);
      EXPECT_EQ(duel_test_mana(defender,0),paid?2:0);
      EXPECT_EQ(duel_test_stack_size(),paid?2:1);
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_GRAVEYARD),paid?0:1);
      if(!paid)EXPECT_EQ(duel_test_zone_csvid(defender,DUEL_ZONE_GRAVEYARD,0),999);
      if(!replay)announced.reset(duel_test_capture());
      if(paid) {
        duel_test_decision(DUEL_CHOICE_DIALOG,defender,0,-1);
        duel_test_resolve_response();
      } else duel_test_resolve();
      EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_stack_size(),0);
      EXPECT_EQ(duel_test_mana(defender,0),0);EXPECT_EQ(manalinkeh_get_storm_count(),3);
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_LIBRARY),paid?0:3);
      EXPECT_EQ(duel_test_zone_count(defender,DUEL_ZONE_GRAVEYARD),1);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
      if(!replay)completed=observable_state();else EXPECT_EQ(observable_state(),completed);
      duel_test_ai_roundtrip();EXPECT_EQ(observable_state(),completed);
    }
  }
}
TEST(CastAnnouncement, StackObserverKeepsAbilityAfterSourceSacrifice) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,10880,DUEL_STATE_UNTAPPED); // Bloodthrone Vampire.
    auto other=duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_UNTAPPED);
    duel_test_prepare_fixture();
    if(owner==0)duel_test_decision(DUEL_CHOICE_TARGET,owner,source.player,source.card);
    ASSERT_TRUE(duel_test_activate(source));EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_FALSE(duel_test_inspect(source).in_play);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
    EXPECT_EQ(duel_test_stack_size(),1); // The ability survives its sacrificed source.
    auto pending=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(pending.get());
      EXPECT_EQ(duel_test_stack_size(),1);duel_test_resolve();
      EXPECT_EQ(duel_test_stack_size(),0);EXPECT_TRUE(duel_test_inspect(other).in_play);
      EXPECT_EQ(duel_test_inspect(other).power,2);EXPECT_EQ(duel_test_inspect(other).toughness,2);
      if(!replay)completed=observable_state();else EXPECT_EQ(observable_state(),completed);
    }
  }
}
TEST(CastAnnouncement, NullPromptSpellUsesRealTargetSelectionAndSnapshotReplay) {
  for(int owner:{0,1}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,8571,0); // Ground Rift omits its prompt key.
    auto flyer=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,514,DUEL_STATE_UNTAPPED);
    duel_test_prepare_fixture();duel_test_set_mana(owner,4,1);duel_test_refresh();
    EXPECT_FALSE(duel_test_can_cast(source)); // The null prompt does not relax targeting.
    auto target=duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,106,DUEL_STATE_UNTAPPED);
    duel_test_prepare_fixture();duel_test_refresh();
    ASSERT_TRUE(duel_test_can_cast(source));
    duel_test_decision(DUEL_CHOICE_TARGET,owner,target.player,target.card);
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
    std::string resolved;
    for(int replay=0;replay<2;++replay) {
      if(replay)duel_test_restore(saved.get());
      ASSERT_TRUE(duel_test_cast(source));
      EXPECT_EQ(duel_test_mana(owner,4),0);EXPECT_FALSE(duel_test_inspect(source).in_hand);
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      duel_test_resolve();
      EXPECT_TRUE(duel_test_inspect(target).in_play);EXPECT_TRUE(duel_test_inspect(flyer).in_play);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
      if(!replay)resolved=observable_state();else EXPECT_EQ(observable_state(),resolved);
    }
  }
}
TEST(CharacteristicValues, ScriptedXUsesActualPaymentAndSpellResolutionForBothControllers) {
  for(int owner:{0,1})for(int x:{0,3,8}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,980,0); // Blaze: XR.
    duel_test_prepare_fixture();for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?12:c==4?1:0);duel_test_refresh();
    duel_test_decision(DUEL_CHOICE_X,owner,x,-1);
    duel_test_decision(DUEL_CHOICE_TARGET,owner,1-owner,-1);
    EXPECT_TRUE(duel_host_choice_ready(DUEL_CHOICE_X,owner));
    EXPECT_FALSE(duel_host_choice_ready(DUEL_CHOICE_X,1-owner));
    std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
    ASSERT_TRUE(duel_test_cast(source));EXPECT_EQ(duel_test_mana(owner,0),12-x);EXPECT_EQ(duel_test_mana(owner,4),0);
    EXPECT_FALSE(duel_host_choice_ready(DUEL_CHOICE_X,owner));duel_test_resolve();
    EXPECT_EQ(duel_test_life(1-owner),20-x);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    auto after=observable_state();duel_test_restore(saved.get());
    ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();EXPECT_EQ(observable_state(),after);EXPECT_EQ(duel_test_finish_choice_plan(),0);
  }
}
TEST(CharacteristicValues, ScriptedXCreatesRealEntryCountersIncludingZero) {
  for(int owner:{0,1})for(int x:{0,1,5}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,5585,0); // Ivy Elemental: XG.
    duel_test_prepare_fixture();for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?10:c==3?1:0);duel_test_refresh();
    duel_test_decision(DUEL_CHOICE_X,owner,x,-1);ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();
    EXPECT_EQ(duel_test_mana(owner,0),10-x);EXPECT_EQ(duel_test_mana(owner,3),0);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    auto state=duel_test_inspect(source);EXPECT_EQ(state.in_play,x!=0);
    if(x) {EXPECT_EQ(state.power,x);EXPECT_EQ(state.toughness,x);EXPECT_EQ(state.plus1_counters,x);}
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),x==0?1:0);
  }
}
TEST(CharacteristicValues, ScriptedMultiXUsesNormalManaPaymentIncludingZero) {
  for(int owner:{0,1})for(int count:{2,3})for(int x:{0,2,4})for(int fizzled:{-1,0}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?20:0);duel_test_refresh();g_max_x_value=-1;g_spell_fizzled=fizzled;
    duel_test_decision(DUEL_CHOICE_X,owner,x,-1);
    EXPECT_EQ(manalinkeh_charge_mana_for_multi_x(owner,0,count),count*x);
    EXPECT_EQ(g_spell_fizzled,fizzled);
    EXPECT_EQ(duel_test_mana(owner,0),20-count*x);EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_stack_size(),0);EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),0);
  }
}
TEST(CharacteristicValues, PlannedXDoesNotInterceptFixedCostsOrAiPolicyAndSurvivesSnapshots) {
  Refs refs;load_fixture(refs,"empty_main",123);
  auto source=duel_test_add(0,DUEL_ZONE_HAND,106,0);duel_test_prepare_fixture();
  duel_test_decision(DUEL_CHOICE_X,0,3,-1);
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
  EXPECT_TRUE(duel_host_choice_ready(DUEL_CHOICE_X,0));duel_host_set_ai_choices(1);
  EXPECT_FALSE(duel_host_choice_ready(DUEL_CHOICE_X,0));duel_host_set_ai_choices(0);
  ASSERT_TRUE(duel_test_cast(source));duel_test_resolve();EXPECT_TRUE(duel_host_choice_ready(DUEL_CHOICE_X,0));
  EXPECT_EQ(duel_test_finish_choice_plan(),1);duel_test_restore(saved.get());
  EXPECT_TRUE(duel_host_choice_ready(DUEL_CHOICE_X,0));EXPECT_EQ(duel_host_choose_x(0,3),3);EXPECT_EQ(duel_test_finish_choice_plan(),0);
}
TEST(CharacteristicValues, OutsideBattlefieldUsesCurrentCountsWithoutCreatingCards) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto initial=observable_state();
  EXPECT_EQ(duel_test_base_power(0,7682),4); // Kiyomaro: cards in its owner's hand.
  EXPECT_EQ(duel_test_base_toughness(0,7682),4);
  EXPECT_EQ(duel_test_base_power(0,15938),4); // Sage of Ancient Lore.
  EXPECT_EQ(duel_test_base_power(0,15939),8); // Werewolf: all players' hands.
  EXPECT_EQ(duel_test_base_power(0,14169),0); // Seraph: creatures you control.
  EXPECT_EQ(duel_test_base_power(0,15842),0); // Westvale Cult Leader.
  EXPECT_EQ(duel_test_base_power(0,20952),0); // Anax: devotion to red.
  EXPECT_EQ(observable_state(),initial);
  duel_test_add(0,DUEL_ZONE_BATTLEFIELD,9640,0); // Pyre Charger contributes RR.
  duel_test_prepare_fixture();
  EXPECT_EQ(duel_test_base_power(0,14169),1);
  EXPECT_EQ(duel_test_base_toughness(0,15842),1);
  EXPECT_EQ(duel_test_base_power(0,20952),2);
  EXPECT_EQ(duel_test_base_power(0,7682),4);
  EXPECT_EQ(duel_test_base_toughness(0,15939),8);
}
TEST(CharacteristicValues, NullBindingDoesNotCallAnAbsentCharacteristicCallback) {
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto initial=observable_state();
  // Imported placeholders retain null bindings; this verifies query safety,
  // not implementation of their missing printed abilities.
  EXPECT_EQ(duel_test_base_power(0,8740),0);
  EXPECT_EQ(duel_test_base_toughness(0,8740),0);
  EXPECT_EQ(observable_state(),initial);
}
TEST(SummoningSickness, SuspendHasteEndsOnControlLoss) {
  Refs refs; load_fixture(refs,"empty_main",123);
  auto creature=duel_test_add(0,DUEL_ZONE_BATTLEFIELD,149,DUEL_STATE_SICK); // Llanowar Elves
  auto control=duel_test_add(1,DUEL_ZONE_HAND,44,0); // Control Magic
  auto effect=duel_test_suspend_haste(creature);
  duel_test_prepare_fixture();
  EXPECT_TRUE(duel_test_can_activate(creature));
  duel_test_cleanup(); duel_test_refresh();
  EXPECT_TRUE(duel_test_can_activate(creature))<<"Suspend haste survives cleanup";
  duel_test_set_turn(1,0);
  duel_test_decision(DUEL_CHOICE_TARGET,1,creature.player,creature.card);
  ASSERT_TRUE(duel_test_cast(control)); duel_test_resolve(); duel_test_refresh();
  DuelTestRef stolen={1,-1};
  for(int i=0;i<150;++i) {
    auto c=duel_test_inspect({1,i});
    if(c.in_play && c.csvid==149)stolen.card=i;
  }
  ASSERT_GE(stolen.card,0);
  EXPECT_FALSE(duel_test_inspect(effect).in_play);
  EXPECT_FALSE(duel_test_can_activate(stolen));
  EXPECT_FALSE(duel_test_attack(stolen));
  duel_test_destroy(control); duel_test_refresh();
  DuelTestRef returned={0,-1};
  for(int i=0;i<150;++i) {
    auto c=duel_test_inspect({0,i});
    if(c.in_play && c.csvid==149)returned.card=i;
  }
  ASSERT_GE(returned.card,0);
  EXPECT_FALSE(duel_test_can_activate(returned))<<"Regaining control must not revive the expired grant";
  EXPECT_FALSE(duel_test_attack(returned));
  duel_test_finish_decisions(); duel_test_check_invariants();
}
class TemplateTest:public testing::Test {
 const Template *spec;
 public: explicit TemplateTest(const Template *s):spec(s){}
 void TestBody() override {
  SCOPED_TRACE(std::string(spec->name)+" CSVID "+std::to_string(spec->csvid)+" Oracle "+spec->oracle_id);
  Refs refs; load_fixture(refs,"empty_main",123);
  auto card=duel_test_add(0,DUEL_ZONE_BATTLEFIELD,spec->csvid,0); auto c=duel_test_inspect(card);
  EXPECT_EQ(c.power,spec->power); EXPECT_EQ(c.toughness,spec->toughness); EXPECT_TRUE(c.in_play);
  auto saved=duel_test_capture(); duel_test_cleanup(); duel_test_ai_roundtrip(); duel_test_restore(saved);
  EXPECT_EQ(duel_test_inspect(card).power,spec->power); duel_test_free_snapshot(saved);
  duel_test_check_invariants(); duel_test_finish_decisions();
 }
};
class SmokeTest:public testing::Test {
 const Smoke *spec;
 public: explicit SmokeTest(const Smoke *s):spec(s){}
 void TestBody() override {
  SCOPED_TRACE(std::string(spec->name)+" CSVID "+std::to_string(spec->csvid));
  Refs refs; load_fixture(refs,"stocked_zones",123);
  auto card=duel_test_add(0,DUEL_ZONE_HAND,spec->csvid,0);
  EXPECT_EQ(duel_test_inspect(card).csvid,spec->csvid);
  auto saved=duel_test_capture(); duel_test_ai_roundtrip(); duel_test_cleanup(); duel_test_restore(saved);
  EXPECT_EQ(duel_test_inspect(card).csvid,spec->csvid);
  duel_test_free_snapshot(saved); duel_test_check_invariants(); duel_test_finish_decisions();
 }
};
class BindingTest:public testing::Test {
 const Smoke *spec;
 public: explicit BindingTest(const Smoke *s):spec(s){}
 void TestBody() override {
  SCOPED_TRACE(std::string(spec->name)+" CSVID "+std::to_string(spec->csvid));
  EXPECT_TRUE(duel_test_handler_matches_override(spec->csvid));
 }
};
class AITest:public testing::Test {
 unsigned seed;
 public:explicit AITest(unsigned s):seed(s){}
 void TestBody() override {
  SCOPED_TRACE("AI seed "+std::to_string(seed));
  Refs refs;load_fixture(refs,"stocked_zones",seed);duel_test_prepare_fixture();
  for(int p=0;p<2;++p)for(int n=0;n<80;++n)duel_test_add(p,DUEL_ZONE_LIBRARY,n%3?91:106,0);
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(duel_test_capture(),duel_test_free_snapshot);
  auto initial=observable_state();
  duel_test_ai_turns(4);auto result=observable_state();
  EXPECT_NE(result,initial)<<"The AI made no observable progress";
  duel_test_restore(saved.get());EXPECT_EQ(observable_state(),initial);
  duel_test_ai_turns(4);EXPECT_EQ(observable_state(),result)<<"AI search changed a caller-owned fixture snapshot";
  duel_test_finish_decisions();std::cout<<"OBSERVATION: "<<result<<'\n';
 }
};
// OUT-ONLY: root integrates/red-greens after the engine19 valid7 pair gate.
// One new TEST/suite; no author compilation or execution. No selector bypass
// by card name, fabricated target, changed cancellation flag, or fake phase.
static bool prepare_paid_clock_selector_control(int owner, bool nonempty,
                                                DuelTestRef* clock_out) {
  Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
  for(int p:{0,1})for(int i=0;i<6;++i)duel_test_add(p,DUEL_ZONE_LIBRARY,91,0);
  *clock_out=duel_test_add(owner,DUEL_ZONE_HAND,7607,0); // Printed {4}.
  if(nonempty)duel_test_add(1-owner,DUEL_ZONE_BATTLEFIELD,106,0);
  // A subsequent legal action is already present in the original fixture.
  // This HAND creature must not count as an eligible in-play permanent.
  duel_test_add(1-owner,DUEL_ZONE_HAND,106,0);
  duel_test_add(1-owner,DUEL_ZONE_HAND,88,0); // Printed {G} instant Fog.
  duel_test_prepare_fixture();
  for(int p:{0,1})for(int c=0;c<6;++c)duel_test_set_mana(p,c,0);
  duel_test_set_mana(owner,0,4);
  if(!duel_test_cast(*clock_out) || duel_test_stack_size()!=1)return false;
  for(int c=0;c<6;++c)if(duel_test_mana(owner,c)!=0)return false;
  duel_test_resolve();
  return duel_test_inspect(*clock_out).in_play && duel_test_finish_choice_plan()==0;
}

TEST(EmptyTargetSelection, PaidClockDeclineKeepsEmptyNonemptyReplayAndSubsequentActions) {
  for(int owner:{0,1})for(bool nonempty:{false,true}) {
    SCOPED_TRACE(owner);
    SCOPED_TRACE(nonempty);
    const int active=1-owner;
    DuelTestRef clock{};
    ASSERT_TRUE(prepare_paid_clock_selector_control(owner,nonempty,&clock));
    ASSERT_EQ(duel_test_zone_count(owner,DUEL_ZONE_BATTLEFIELD),1);
    ASSERT_EQ(duel_test_zone_count(active,DUEL_ZONE_BATTLEFIELD),nonempty?1:0);
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(active));
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(
        duel_test_capture(),duel_test_free_snapshot);
    std::string completed;
    for(int replay:{0,1}) {
      if(replay){duel_test_restore(ready.get());duel_test_ai_roundtrip();}
      // Actual active-player choice: decline paying2 life.
      duel_test_decision(DUEL_CHOICE_DIALOG,active,1,-1);
      if(nonempty) {
        auto chosen=duel_test_permanent_at(active,0);
        ASSERT_EQ(duel_test_inspect(chosen).csvid,106);
        // Nonempty must still consult the real mandatory selector: refusal
        // validates the fallback and increments the actual forced-choice count.
        duel_test_optional_target_decision(active,chosen);
      }
      // EMPTY: deliberately no target/cancel/optional-target plan. There is
      // still an opposing Clock in play and a Bear in hand, so the bridge must
      // use the complete real zone/controller/type predicate, not a shortcut.
      ASSERT_TRUE(duel_test_upkeep_step(active));
      EXPECT_EQ(duel_test_finish_choice_plan(),0);
      EXPECT_EQ(duel_test_forced_target_choices(),nonempty?1:0);
      EXPECT_EQ(duel_test_life(owner),20);EXPECT_EQ(duel_test_life(active),20);
      EXPECT_EQ(duel_test_zone_count(active,DUEL_ZONE_BATTLEFIELD),0);
      ASSERT_EQ(duel_test_zone_count(active,DUEL_ZONE_HAND),nonempty?3:2);
      int bears=0,fogs=0;
      for(int i=0;i<duel_test_zone_count(active,DUEL_ZONE_HAND);++i) {
        int csvid=duel_test_zone_csvid(active,DUEL_ZONE_HAND,i);
        if(csvid==106)++bears;else {EXPECT_EQ(csvid,88);++fogs;}
      }
      EXPECT_EQ(bears,nonempty?2:1);EXPECT_EQ(fogs,1);
      EXPECT_TRUE(duel_test_inspect(clock).in_play);
      EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_LIBRARY),6);
      EXPECT_EQ(duel_test_zone_count(active,DUEL_ZONE_LIBRARY),6);
      if(!replay)completed=observable_state();
      else EXPECT_EQ(observable_state(),completed);
      EXPECT_FALSE(duel_test_upkeep_step(active));
      EXPECT_EQ(observable_state(),completed);
    }
    // A real exactly-paid instant in the actual completed upkeep. The hand
    // Bear stays out of play throughout; no manufactured main phase/turn.
    DuelTestRef followup{};bool found_fog=false;
    for(int i=0;i<duel_test_zone_count(active,DUEL_ZONE_HAND);++i) {
      auto candidate=duel_test_hand_at(active,i);
      if(duel_test_inspect(candidate).csvid==88){followup=candidate;found_fog=true;break;}
    }
    ASSERT_TRUE(found_fog);ASSERT_EQ(duel_test_inspect(followup).csvid,88);
    duel_test_set_mana(active,3,1);
    ASSERT_TRUE(duel_test_cast(followup));EXPECT_EQ(duel_test_stack_size(),1);
    EXPECT_EQ(duel_test_mana(active,3),0);
    duel_test_resolve();EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_zone_count(active,DUEL_ZONE_BATTLEFIELD),0);
    EXPECT_EQ(duel_test_zone_count(active,DUEL_ZONE_HAND),nonempty?2:1);
    for(int i=0;i<duel_test_zone_count(active,DUEL_ZONE_HAND);++i)
      EXPECT_EQ(duel_test_zone_csvid(active,DUEL_ZONE_HAND,i),106);
    EXPECT_EQ(duel_test_zone_count(active,DUEL_ZONE_GRAVEYARD),1);
    EXPECT_EQ(duel_test_zone_csvid(active,DUEL_ZONE_GRAVEYARD,0),88);
    EXPECT_TRUE(duel_test_inspect(clock).in_play);
    EXPECT_EQ(duel_test_life(owner),20);EXPECT_EQ(duel_test_life(active),20);
    duel_test_finish_decisions();duel_test_check_invariants();
  }
}

static const Setup input_control_0_cards[] = {{"source",0,0,13740,0},{"host",1,0,106,0},{"library_0_0",0,2,91,0},{"library_0_1",0,2,91,0},{"library_0_2",0,2,91,0},{"library_1_0",1,2,91,0},{"library_1_1",1,2,91,0},{"library_1_2",1,2,91,0},{"wrong_host",1,1,106,0}};
static const Step input_control_0_steps[] = {{"turn","","","",1,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",1,1,0,0,0},{"set_mana","","","",1,0,0,0,1},{"set_mana","","","",1,0,0,0,2},{"set_mana","","","",1,1,0,0,3},{"set_mana","","","",1,0,0,0,4},{"set_mana","","","",1,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"cast","host","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","sick",0,1,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",0,1,0,0,0},{"set_mana","","","",0,0,0,0,1},{"set_mana","","","",0,0,0,0,2},{"set_mana","","","",0,1,0,0,3},{"set_mana","","","",0,0,0,0,4},{"set_mana","","","",0,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"can_cast","source","","",0,1,0,0,0},{"choose_target","host","","",0,0,0,0,0},{"cast","source","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,0,0,0,0},{"assert_zone","","","",0,1,0,0,0},{"refresh","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"assert_card","host","","sick",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"can_activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"can_activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"can_activate_granted_mana","host","host","",0,0,0,0,0},{"activate_granted_mana","host","host","",0,0,0,0,0},{"can_activate_granted_mana","source","source","",0,0,0,0,0},{"activate_granted_mana","source","source","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,1,0,0,0},{"capture","","","",0,0,0,0,0},{"choose_color","","","",1,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,1,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"restore","","","",0,0,0,0,0},{"ai_roundtrip","","","",0,0,0,0,0},{"choose_color","","","",1,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,1,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"destroy","source","","",0,0,0,0,0},{"resolve","","","",0,0,0,0,0},{"refresh","","","",0,0,0,0,0},{"assert_card","source","","in_play",0,0,0,0,0},{"assert_zone_multiplicity","","","",0,1,3,13740,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_mana","","","",1,1,0,0,3},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0}};
static const Case input_control_0 = {"GrantedManaControl_0_fresh","Karametra's Favor","fc3e48c9-e303-435c-bdb1-7dc7881a9858","Enchant creature\nWhen this Aura enters, draw a card.\nEnchanted creature has \"{T}: Add one mana of any color.\"","empty_main",123,input_control_0_cards,9,input_control_0_steps,108};
static const Setup input_control_1_cards[] = {{"source",0,0,13740,0},{"host",1,1,106,8},{"library_0_0",0,2,91,0},{"library_0_1",0,2,91,0},{"library_0_2",0,2,91,0},{"library_1_0",1,2,91,0},{"library_1_1",1,2,91,0},{"library_1_2",1,2,91,0},{"wrong_host",1,1,106,0}};
static const Step input_control_1_steps[] = {{"set_mana","","","",1,0,0,0,0},{"set_mana","","","",1,0,0,0,1},{"set_mana","","","",1,0,0,0,2},{"set_mana","","","",1,0,0,0,3},{"set_mana","","","",1,0,0,0,4},{"set_mana","","","",1,0,0,0,5},{"turn","","","",0,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",0,1,0,0,0},{"set_mana","","","",0,0,0,0,1},{"set_mana","","","",0,0,0,0,2},{"set_mana","","","",0,1,0,0,3},{"set_mana","","","",0,0,0,0,4},{"set_mana","","","",0,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"can_cast","source","","",0,1,0,0,0},{"choose_target","host","","",0,0,0,0,0},{"cast","source","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,0,0,0,0},{"assert_zone","","","",0,1,0,0,0},{"refresh","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"can_activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"can_activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"can_activate_granted_mana","host","host","",0,0,0,0,0},{"activate_granted_mana","host","host","",0,0,0,0,0},{"can_activate_granted_mana","source","source","",0,0,0,0,0},{"activate_granted_mana","source","source","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,1,0,0,0},{"capture","","","",0,0,0,0,0},{"choose_color","","","",1,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,1,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"restore","","","",0,0,0,0,0},{"ai_roundtrip","","","",0,0,0,0,0},{"choose_color","","","",1,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,1,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"destroy","source","","",0,0,0,0,0},{"resolve","","","",0,0,0,0,0},{"refresh","","","",0,0,0,0,0},{"assert_card","source","","in_play",0,0,0,0,0},{"assert_zone_multiplicity","","","",0,1,3,13740,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_mana","","","",1,1,0,0,3},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0}};
static const Case input_control_1 = {"GrantedManaControl_0_ready","Karametra's Favor","fc3e48c9-e303-435c-bdb1-7dc7881a9858","Enchant creature\nWhen this Aura enters, draw a card.\nEnchanted creature has \"{T}: Add one mana of any color.\"","empty_main",123,input_control_1_cards,9,input_control_1_steps,95};
static const Setup input_control_2_cards[] = {{"source",1,0,13740,0},{"host",0,0,106,0},{"library_0_0",0,2,91,0},{"library_0_1",0,2,91,0},{"library_0_2",0,2,91,0},{"library_1_0",1,2,91,0},{"library_1_1",1,2,91,0},{"library_1_2",1,2,91,0},{"wrong_host",0,1,106,0}};
static const Step input_control_2_steps[] = {{"turn","","","",0,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",0,1,0,0,0},{"set_mana","","","",0,0,0,0,1},{"set_mana","","","",0,0,0,0,2},{"set_mana","","","",0,1,0,0,3},{"set_mana","","","",0,0,0,0,4},{"set_mana","","","",0,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"cast","host","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","sick",0,1,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",1,1,0,0,0},{"set_mana","","","",1,0,0,0,1},{"set_mana","","","",1,0,0,0,2},{"set_mana","","","",1,1,0,0,3},{"set_mana","","","",1,0,0,0,4},{"set_mana","","","",1,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"can_cast","source","","",0,1,0,0,0},{"choose_target","host","","",1,0,0,0,0},{"cast","source","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,0,0,0,0},{"assert_zone","","","",1,1,0,0,0},{"refresh","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"assert_card","host","","sick",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"can_activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"can_activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"can_activate_granted_mana","host","host","",0,0,0,0,0},{"activate_granted_mana","host","host","",0,0,0,0,0},{"can_activate_granted_mana","source","source","",0,0,0,0,0},{"activate_granted_mana","source","source","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,1,0,0,0},{"capture","","","",0,0,0,0,0},{"choose_color","","","",0,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,1,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"restore","","","",0,0,0,0,0},{"ai_roundtrip","","","",0,0,0,0,0},{"choose_color","","","",0,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,1,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"destroy","source","","",0,0,0,0,0},{"resolve","","","",0,0,0,0,0},{"refresh","","","",0,0,0,0,0},{"assert_card","source","","in_play",0,0,0,0,0},{"assert_zone_multiplicity","","","",1,1,3,13740,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_mana","","","",0,1,0,0,3},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0}};
static const Case input_control_2 = {"GrantedManaControl_1_fresh","Karametra's Favor","fc3e48c9-e303-435c-bdb1-7dc7881a9858","Enchant creature\nWhen this Aura enters, draw a card.\nEnchanted creature has \"{T}: Add one mana of any color.\"","empty_main",123,input_control_2_cards,9,input_control_2_steps,108};
static const Setup input_control_3_cards[] = {{"source",1,0,13740,0},{"host",0,1,106,8},{"library_0_0",0,2,91,0},{"library_0_1",0,2,91,0},{"library_0_2",0,2,91,0},{"library_1_0",1,2,91,0},{"library_1_1",1,2,91,0},{"library_1_2",1,2,91,0},{"wrong_host",0,1,106,0}};
static const Step input_control_3_steps[] = {{"set_mana","","","",0,0,0,0,0},{"set_mana","","","",0,0,0,0,1},{"set_mana","","","",0,0,0,0,2},{"set_mana","","","",0,0,0,0,3},{"set_mana","","","",0,0,0,0,4},{"set_mana","","","",0,0,0,0,5},{"turn","","","",1,0,0,0,0},{"present_ai_dialogs","","","",0,1,0,0,0},{"set_mana","","","",1,1,0,0,0},{"set_mana","","","",1,0,0,0,1},{"set_mana","","","",1,0,0,0,2},{"set_mana","","","",1,1,0,0,3},{"set_mana","","","",1,0,0,0,4},{"set_mana","","","",1,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"can_cast","source","","",0,1,0,0,0},{"choose_target","host","","",1,0,0,0,0},{"cast","source","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,0,0,0,0},{"assert_zone","","","",1,1,0,0,0},{"refresh","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"can_activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"activate_granted_mana","wrong_host","source","",0,0,0,0,0},{"can_activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"activate_granted_mana","host","wrong_host","",0,0,0,0,0},{"can_activate_granted_mana","host","host","",0,0,0,0,0},{"activate_granted_mana","host","host","",0,0,0,0,0},{"can_activate_granted_mana","source","source","",0,0,0,0,0},{"activate_granted_mana","source","source","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"can_activate_granted_mana","host","source","",0,1,0,0,0},{"capture","","","",0,0,0,0,0},{"choose_color","","","",0,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,1,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"record_state","","","",0,0,0,0,0},{"restore","","","",0,0,0,0,0},{"ai_roundtrip","","","",0,0,0,0,0},{"choose_color","","","",0,3,0,0,0},{"activate_granted_mana","host","source","",0,1,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_card","host","","tapped",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","wrong_host","","tapped",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,1,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0},{"destroy","source","","",0,0,0,0,0},{"resolve","","","",0,0,0,0,0},{"refresh","","","",0,0,0,0,0},{"assert_card","source","","in_play",0,0,0,0,0},{"assert_zone_multiplicity","","","",1,1,3,13740,0},{"can_activate_granted_mana","host","source","",0,0,0,0,0},{"activate_granted_mana","host","source","",0,0,0,0,0},{"assert_mana","","","",0,1,0,0,3},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0}};
static const Case input_control_3 = {"GrantedManaControl_1_ready","Karametra's Favor","fc3e48c9-e303-435c-bdb1-7dc7881a9858","Enchant creature\nWhen this Aura enters, draw a card.\nEnchanted creature has \"{T}: Add one mana of any color.\"","empty_main",123,input_control_3_cards,9,input_control_3_steps,95};
static const Setup input_control_4_cards[] = {{"source",0,0,3690,0},{"generic_land",0,1,15719,0},{"green_land",0,1,91,0},{"library_0_0",0,2,91,0},{"library_0_1",0,2,91,0},{"library_0_2",0,2,91,0},{"library_0_3",0,2,91,0},{"library_1_0",1,2,91,0},{"library_1_1",1,2,91,0},{"library_1_2",1,2,91,0},{"library_1_3",1,2,91,0}};
static const Step input_control_4_steps[] = {{"turn","","","",0,0,0,0,0},{"set_mana","","","",0,1,0,0,0},{"set_mana","","","",0,0,0,0,1},{"set_mana","","","",0,0,0,0,2},{"set_mana","","","",0,1,0,0,3},{"set_mana","","","",0,0,0,0,4},{"set_mana","","","",0,0,0,0,5},{"set_mana","","","",1,0,0,0,0},{"set_mana","","","",1,0,0,0,1},{"set_mana","","","",1,0,0,0,2},{"set_mana","","","",1,0,0,0,3},{"set_mana","","","",1,0,0,0,4},{"set_mana","","","",1,0,0,0,5},{"refresh","","","",0,0,0,0,0},{"cast","source","","",0,1,0,0,0},{"assert_stack","","","",0,1,0,0,0},{"resolve","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"assert_life","","","",0,20,0,0,0},{"assert_zone","","","",0,0,3,0,0},{"assert_zone","","","",0,4,2,0,0},{"assert_life","","","",1,20,0,0,0},{"assert_zone","","","",1,0,3,0,0},{"assert_zone","","","",1,4,2,0,0},{"assert_zone","","","",0,0,0,0,0},{"assert_zone","","","",0,3,1,0,0},{"assert_card","source","","in_play",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","generic_land","","in_play",0,1,0,0,0},{"assert_card","generic_land","","tapped",0,0,0,0,0},{"assert_card","green_land","","in_play",0,1,0,0,0},{"assert_card","green_land","","tapped",0,0,0,0,0},{"assert_card","source","","power",0,2,0,0,0},{"assert_card","source","","toughness",0,4,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"untap_step","","","",1,0,0,0,0},{"upkeep_step","","","",1,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"assert_life","","","",0,20,0,0,0},{"assert_zone","","","",0,0,3,0,0},{"assert_zone","","","",0,4,2,0,0},{"assert_life","","","",1,20,0,0,0},{"assert_zone","","","",1,0,3,0,0},{"assert_zone","","","",1,4,2,0,0},{"assert_zone","","","",0,0,0,0,0},{"assert_zone","","","",0,3,1,0,0},{"assert_card","source","","in_play",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","generic_land","","in_play",0,1,0,0,0},{"assert_card","generic_land","","tapped",0,0,0,0,0},{"assert_card","green_land","","in_play",0,1,0,0,0},{"assert_card","green_land","","tapped",0,0,0,0,0},{"assert_card","source","","power",0,2,0,0,0},{"assert_card","source","","toughness",0,4,0,0,0},{"turn","","","",1,0,0,0,0},{"advance_turn","","","",0,0,0,0,0},{"untap_step","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"assert_life","","","",0,20,0,0,0},{"assert_zone","","","",0,0,3,0,0},{"assert_zone","","","",0,4,2,0,0},{"assert_life","","","",1,20,0,0,0},{"assert_zone","","","",1,0,3,0,0},{"assert_zone","","","",1,4,2,0,0},{"assert_zone","","","",0,0,0,0,0},{"assert_zone","","","",0,3,1,0,0},{"assert_card","source","","in_play",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","generic_land","","in_play",0,1,0,0,0},{"assert_card","generic_land","","tapped",0,0,0,0,0},{"assert_card","green_land","","in_play",0,1,0,0,0},{"assert_card","green_land","","tapped",0,0,0,0,0},{"assert_card","source","","power",0,2,0,0,0},{"assert_card","source","","toughness",0,4,0,0,0},{"assert_card","source","","sick",0,0,0,0,0},{"choose_dialog","","","",0,0,0,0,0},{"choose_mana_source","generic_land","","",0,0,0,0,0},{"choose_mana_source","green_land","","",0,0,0,0,0},{"capture","","","",0,0,0,0,0},{"upkeep_step","","","",0,0,0,0,0},{"assert_unused_choices","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"assert_life","","","",0,20,0,0,0},{"assert_zone","","","",0,0,3,0,0},{"assert_zone","","","",0,4,2,0,0},{"assert_life","","","",1,20,0,0,0},{"assert_zone","","","",1,0,3,0,0},{"assert_zone","","","",1,4,2,0,0},{"assert_zone","","","",0,0,0,0,0},{"assert_zone","","","",0,3,1,0,0},{"assert_card","source","","in_play",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","generic_land","","in_play",0,1,0,0,0},{"assert_card","generic_land","","tapped",0,1,0,0,0},{"assert_card","green_land","","in_play",0,1,0,0,0},{"assert_card","green_land","","tapped",0,1,0,0,0},{"assert_card","source","","power",0,2,0,0,0},{"assert_card","source","","toughness",0,4,0,0,0},{"record_state","","","",0,0,0,0,0},{"restore","","","",0,0,0,0,0},{"ai_roundtrip","","","",0,0,0,0,0},{"upkeep_step","","","",0,0,0,0,0},{"assert_unused_choices","","","",0,0,0,0,0},{"assert_stack","","","",0,0,0,0,0},{"assert_choices_consumed","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,0},{"assert_mana","","","",0,0,0,0,1},{"assert_mana","","","",0,0,0,0,2},{"assert_mana","","","",0,0,0,0,3},{"assert_mana","","","",0,0,0,0,4},{"assert_mana","","","",0,0,0,0,5},{"assert_mana","","","",1,0,0,0,0},{"assert_mana","","","",1,0,0,0,1},{"assert_mana","","","",1,0,0,0,2},{"assert_mana","","","",1,0,0,0,3},{"assert_mana","","","",1,0,0,0,4},{"assert_mana","","","",1,0,0,0,5},{"assert_life","","","",0,20,0,0,0},{"assert_zone","","","",0,0,3,0,0},{"assert_zone","","","",0,4,2,0,0},{"assert_life","","","",1,20,0,0,0},{"assert_zone","","","",1,0,3,0,0},{"assert_zone","","","",1,4,2,0,0},{"assert_zone","","","",0,0,0,0,0},{"assert_zone","","","",0,3,1,0,0},{"assert_card","source","","in_play",0,1,0,0,0},{"assert_card","source","","tapped",0,0,0,0,0},{"assert_card","generic_land","","in_play",0,1,0,0,0},{"assert_card","generic_land","","tapped",0,1,0,0,0},{"assert_card","green_land","","in_play",0,1,0,0,0},{"assert_card","green_land","","tapped",0,1,0,0,0},{"assert_card","source","","power",0,2,0,0,0},{"assert_card","source","","toughness",0,4,0,0,0},{"assert_recorded_state","","","",0,0,0,0,0}};
static const Case input_control_4 = {"ManaSourceControl_RealEchoPaymentReplay","Acridian","c6d0f1bc-b82a-4e64-a4bc-11ad3f54c71a","Echo {1}{G} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)","empty_main",123,input_control_4_cards,11,input_control_4_steps,186};
TEST(ManaSourceInput, PaysRealEchoFromLandsWithZeroPoolAndReplays) {
  const Case* controls[] = {&input_control_4};
  for (const Case* control:controls) {
    SCOPED_TRACE(control->name);
    BehaviorTest scenario(control);
    scenario.TestBody();
    ASSERT_FALSE(HasFatalFailure());
  }
}
TEST(GrantedManaProvider, UsesActualHostControllerAndPreservesNativeRestrictions) {
  const Case* controls[] = {&input_control_0,&input_control_1,&input_control_2,&input_control_3};
  for (const Case* control:controls) {
    SCOPED_TRACE(control->name);
    BehaviorTest scenario(control);
    scenario.TestBody();
    ASSERT_FALSE(HasFatalFailure());
  }
}

// Root integration fragment only. No counter setters or synthetic events.
// Real pilot sources: Yavimaya Ants2385 (2GG cast, cumulative GG) and
// Gallowbraid2980 (3BB cast, cumulative pay1 life). Both source controllers.
TEST(AgeCounters, RealPaidCumulativeUpkeepAndSnapshotHistory) {
  for(int owner:{0,1})for(int csvid:{2385,2980}) {
    SCOPED_TRACE(::testing::Message()<<"owner="<<owner<<" source="<<csvid);
    const bool mana=csvid==2385;
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    for(int p:{0,1})for(int i=0;i<6;++i)duel_test_add(p,DUEL_ZONE_LIBRARY,91,0);
    auto source=duel_test_add(owner,DUEL_ZONE_HAND,csvid,0);
    std::vector<DuelTestRef> lands;
    if(mana)for(int i=0;i<6;++i)lands.push_back(duel_test_add(owner,DUEL_ZONE_BATTLEFIELD,91,0));
    duel_test_prepare_fixture();
    for(int p:{0,1})for(int color=0;color<6;++color)duel_test_set_mana(p,color,0);
    duel_test_set_mana(owner,0,mana?2:3);
    duel_test_set_mana(owner,mana?3:1,2);
    duel_test_refresh();ASSERT_TRUE(duel_test_cast(source));ASSERT_EQ(duel_test_stack_size(),1);
    duel_test_resolve();ASSERT_EQ(duel_test_stack_size(),0);
    EXPECT_EQ(duel_test_finish_choice_plan(),0);EXPECT_EQ(duel_test_counter(source,"age"),0);
    for(int p:{0,1})for(int color=0;color<6;++color)EXPECT_EQ(duel_test_mana(p,color),0);
    for(auto land:lands)EXPECT_FALSE(duel_test_inspect(land).tapped);

    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));
    ASSERT_TRUE(duel_test_upkeep_step(1-owner));EXPECT_EQ(duel_test_counter(source,"age"),0);
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));
    duel_test_decision(DUEL_CHOICE_DIALOG,owner,0,-1); // Actual payable offer.
    if(mana && owner==0)for(int i=0;i<2;++i)
      duel_test_decision(DUEL_CHOICE_MANA,owner,lands[i].card,-1);
    ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_counter(source,"age"),1);EXPECT_EQ(duel_test_life(owner),mana?20:19);
    EXPECT_FALSE(duel_test_upkeep_step(owner)); // Cannot age twice in one phase.

    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));
    ASSERT_TRUE(duel_test_upkeep_step(1-owner));EXPECT_EQ(duel_test_counter(source,"age"),1);
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));
    auto ready=std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)>(duel_test_capture(),duel_test_free_snapshot);
    duel_test_ai_roundtrip();EXPECT_EQ(duel_test_counter(source,"age"),1);
    auto pay_second=[&](){
      duel_test_decision(DUEL_CHOICE_DIALOG,owner,0,-1);
      if(mana && owner==0)for(int i=0;i<4;++i)
        duel_test_decision(DUEL_CHOICE_MANA,owner,lands[i].card,-1);
      ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(duel_test_finish_choice_plan(),0);
      EXPECT_EQ(duel_test_counter(source,"age"),2);EXPECT_EQ(duel_test_life(owner),mana?20:17);
      for(int p:{0,1})for(int color=0;color<6;++color)EXPECT_EQ(duel_test_mana(p,color),0);
      if(mana){int tapped=0;for(auto land:lands)tapped+=!!duel_test_inspect(land).tapped;EXPECT_EQ(tapped,4);}
    };
    pay_second();auto completed=observable_state();
    duel_test_restore(ready.get());EXPECT_EQ(duel_test_counter(source,"age"),1);
    pay_second();EXPECT_EQ(observable_state(),completed);EXPECT_EQ(duel_test_counter(source,"age"),2);

    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(1-owner));
    ASSERT_TRUE(duel_test_upkeep_step(1-owner));EXPECT_EQ(duel_test_counter(source,"age"),2);
    duel_test_advance_turn();ASSERT_TRUE(duel_test_untap_step(owner));
    duel_test_decision(DUEL_CHOICE_DIALOG,owner,1,-1); // Third cost is payable; real decline.
    ASSERT_TRUE(duel_test_upkeep_step(owner));EXPECT_EQ(duel_test_finish_choice_plan(),0);
    EXPECT_EQ(duel_test_zone_count(owner,DUEL_ZONE_GRAVEYARD),1);
    EXPECT_EQ(duel_test_life(owner),mana?20:17);EXPECT_EQ(duel_test_life(1-owner),20);
    // The retired source is observed by its exact owner zone, never its old instance ref.
    duel_test_check_invariants();
  }
}

// Generated OUT-only from four frozen countdown observer scenarios.
// Root may insert after pre-hook diagnostic review. No old test is changed.
static const Setup countdown_observer_0_cards[] = {
  {"source",0,0,4673,0},
};
static const Step countdown_observer_0_steps[] = {
  {"turn","","","",0,0,0,0,0},
  {"present_ai_dialogs","","","",0,1,0,0,0},
  {"set_mana","","","",0,0,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,0,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,0,0,0,5},
  {"set_mana","","","",1,0,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,0,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,0,0,0,5},
  {"set_mana","","","",0,2,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,2,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,0,0,0,5},
  {"refresh","","","",0,0,0,0,0},
  {"cast","source","","",0,1,0,0,0},
  {"resolve","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"capture","","","",0,0,0,0,0},
  {"ai_roundtrip","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"restore","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,2,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,2,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,1,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,1,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"assert_zone_multiplicity","","","",0,1,3,4673,0},
};
static const Case countdown_observer_0 = {"CountdownObserver_4673_0","Blastoderm","d6ee8bd6-0207-46ee-9892-c1d1b23b934e","Shroud (This creature can't be the target of spells or abilities.)\nFading 3 (This creature enters with three fade counters on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)","empty_main",123,countdown_observer_0_cards,1,countdown_observer_0_steps,90};

static const Setup countdown_observer_1_cards[] = {
  {"source",1,0,4673,0},
};
static const Step countdown_observer_1_steps[] = {
  {"turn","","","",1,0,0,0,0},
  {"present_ai_dialogs","","","",0,1,0,0,0},
  {"set_mana","","","",0,0,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,0,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,0,0,0,5},
  {"set_mana","","","",1,0,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,0,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,0,0,0,5},
  {"set_mana","","","",1,2,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,2,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,0,0,0,5},
  {"refresh","","","",0,0,0,0,0},
  {"cast","source","","",0,1,0,0,0},
  {"resolve","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"capture","","","",0,0,0,0,0},
  {"ai_roundtrip","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"restore","","","",0,0,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,2,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,2,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,1,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,1,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","fade",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"assert_zone_multiplicity","","","",1,1,3,4673,0},
};
static const Case countdown_observer_1 = {"CountdownObserver_4673_1","Blastoderm","d6ee8bd6-0207-46ee-9892-c1d1b23b934e","Shroud (This creature can't be the target of spells or abilities.)\nFading 3 (This creature enters with three fade counters on it. At the beginning of your upkeep, remove a fade counter from it. If you can't, sacrifice it.)","empty_main",123,countdown_observer_1_cards,1,countdown_observer_1_steps,90};

static const Setup countdown_observer_2_cards[] = {
  {"source",0,0,8753,0},
};
static const Step countdown_observer_2_steps[] = {
  {"turn","","","",0,0,0,0,0},
  {"present_ai_dialogs","","","",0,1,0,0,0},
  {"set_mana","","","",0,0,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,0,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,0,0,0,5},
  {"set_mana","","","",1,0,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,0,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,0,0,0,5},
  {"set_mana","","","",0,2,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,0,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,2,0,0,5},
  {"refresh","","","",0,0,0,0,0},
  {"cast","source","","",0,1,0,0,0},
  {"resolve","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"capture","","","",0,0,0,0,0},
  {"ai_roundtrip","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"restore","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,3,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,2,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,2,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,1,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,1,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"assert_zone_multiplicity","","","",0,1,3,8753,0},
};
static const Case countdown_observer_2 = {"CountdownObserver_8753_0","Calciderm","329d53eb-d443-4376-ae5a-4be7dfb1fc91","Shroud (This creature can't be the target of spells or abilities.)\nVanishing 4 (This creature enters with four time counters on it. At the beginning of your upkeep, remove a time counter from it. When the last is removed, sacrifice it.)","empty_main",123,countdown_observer_2_cards,1,countdown_observer_2_steps,90};

static const Setup countdown_observer_3_cards[] = {
  {"source",1,0,8753,0},
};
static const Step countdown_observer_3_steps[] = {
  {"turn","","","",1,0,0,0,0},
  {"present_ai_dialogs","","","",0,1,0,0,0},
  {"set_mana","","","",0,0,0,0,0},
  {"set_mana","","","",0,0,0,0,1},
  {"set_mana","","","",0,0,0,0,2},
  {"set_mana","","","",0,0,0,0,3},
  {"set_mana","","","",0,0,0,0,4},
  {"set_mana","","","",0,0,0,0,5},
  {"set_mana","","","",1,0,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,0,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,0,0,0,5},
  {"set_mana","","","",1,2,0,0,0},
  {"set_mana","","","",1,0,0,0,1},
  {"set_mana","","","",1,0,0,0,2},
  {"set_mana","","","",1,0,0,0,3},
  {"set_mana","","","",1,0,0,0,4},
  {"set_mana","","","",1,2,0,0,5},
  {"refresh","","","",0,0,0,0,0},
  {"cast","source","","",0,1,0,0,0},
  {"resolve","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"capture","","","",0,0,0,0,0},
  {"ai_roundtrip","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"restore","","","",0,0,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,4,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,3,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,3,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,2,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,2,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,1,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",0,0,0,0,0},
  {"upkeep_step","","","",0,0,0,0,0},
  {"turn","","","",0,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,1,0,0,0},
  {"assert_counter","source","","time",0,1,0,0,0},
  {"advance_turn","","","",0,0,0,0,0},
  {"untap_step","","","",1,0,0,0,0},
  {"upkeep_step","","","",1,0,0,0,0},
  {"turn","","","",1,0,0,0,0},
  {"assert_choices_consumed","","","",0,0,0,0,0},
  {"assert_card","source","","in_play",0,0,0,0,0},
  {"assert_stack","","","",0,0,0,0,0},
  {"assert_zone_multiplicity","","","",1,1,3,8753,0},
};
static const Case countdown_observer_3 = {"CountdownObserver_8753_1","Calciderm","329d53eb-d443-4376-ae5a-4be7dfb1fc91","Shroud (This creature can't be the target of spells or abilities.)\nVanishing 4 (This creature enters with four time counters on it. At the beginning of your upkeep, remove a time counter from it. When the last is removed, sacrifice it.)","empty_main",123,countdown_observer_3_cards,1,countdown_observer_3_steps,90};

TEST(CountdownCounterObservation, FadingZeroSurvivesUntilNextFailedRemoval) {
  const Case* controls[] = {&countdown_observer_0,&countdown_observer_1};
  for (const Case* control:controls) {
    SCOPED_TRACE(control->name);
    BehaviorTest scenario(control);
    scenario.TestBody();
    ASSERT_FALSE(HasFatalFailure());
  }
}

TEST(CountdownCounterObservation, VanishingLastRemovalSacrificesOnSameUpkeep) {
  const Case* controls[] = {&countdown_observer_2,&countdown_observer_3};
  for (const Case* control:controls) {
    SCOPED_TRACE(control->name);
    BehaviorTest scenario(control);
    scenario.TestBody();
    ASSERT_FALSE(HasFatalFailure());
  }
}

int main(int argc,char**argv) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
 _set_error_mode(_OUT_TO_STDERR);
 _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
 _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
 std::string case_list,journal,case_data=DUEL_TEST_CASE_DATA;
 for(int i=1;i<argc;++i) {
  std::string arg=argv[i];
  if(arg.rfind("--duel-case-list=",0)==0)case_list=arg.substr(17);
  if(arg.rfind("--duel-batch-journal=",0)==0)journal=arg.substr(21);
  if(arg.rfind("--duel-case-data=",0)==0)case_data=arg.substr(17);
 }
 testing::InitGoogleTest(&argc,argv);
 std::unordered_set<std::string> requested;
 if(!case_list.empty()) {
  std::ifstream input(case_list);std::string entry,filter;
  if(!input) {std::cerr<<"DUEL_HARNESS_ERROR: cannot read batch case list\n";return 2;}
  while(std::getline(input,entry)) {
   if(!entry.empty() && entry.back()=='\r')entry.pop_back();
   if(entry.empty() || entry.find_first_of("*?:-")!=std::string::npos || !requested.insert(entry).second) {
    std::cerr<<"DUEL_HARNESS_ERROR: invalid or duplicate batch case\n";return 2;
   }
   if(!filter.empty())filter+=':';filter+=entry;
  }
  if(requested.empty()) {std::cerr<<"DUEL_HARNESS_ERROR: empty batch case list\n";return 2;}
  GTEST_FLAG_SET(filter,filter);
 }
 // The isolated runner requests one exact test. Avoid allocating tens of
 // thousands of unused GoogleTest factories in every fresh process. Complex
 // filters retain Google's normal full-registration matching behavior.
 const std::string filter=GTEST_FLAG_GET(filter);
 const auto dot=filter.find('.');
 const bool exact=dot!=std::string::npos && filter.find_first_of("*?:-")==std::string::npos;
 const std::string suite=exact?filter.substr(0,dot):"", name=exact?filter.substr(dot+1):"";
 auto selected=[&](const char *s,const char *n){return !requested.empty()?requested.count(std::string(s)+'.'+n)!=0:!exact || (suite==s && name==n);};
 if(!exact || suite=="Bindings")for(const auto &t:handler_bindings) { auto *s=&t; auto n="Card_"+std::to_string(t.csvid); if(selected("Bindings",n.c_str()))testing::RegisterTest("Bindings",n.c_str(),nullptr,nullptr,__FILE__,__LINE__,[s](){return new BindingTest(s);}); }
 if(!exact || suite=="AI")for(unsigned seed:{1u,123u,999u}) { auto n="Seed_"+std::to_string(seed); if(selected("AI",n.c_str()))testing::RegisterTest("AI",n.c_str(),nullptr,nullptr,__FILE__,__LINE__,[seed](){return new AITest(seed);}); }
 std::unique_ptr<CaseArchive> archive;
 if(!exact || suite=="Behavior") {
  archive.reset(new CaseArchive(case_data));
  for(const auto &entry:archive->entries)if(selected("Behavior",entry.name)) {
   auto *a=archive.get();auto *e=&entry;
   testing::RegisterTest("Behavior",entry.name,nullptr,nullptr,__FILE__,__LINE__,[a,e](){return new BehaviorTest(a,e);});
  }
 }
 if(!exact || suite=="Templates")for(const auto &t:templates) { auto *s=&t; auto n="Card_"+std::to_string(t.csvid); if(selected("Templates",n.c_str()))testing::RegisterTest("Templates",n.c_str(),nullptr,nullptr,__FILE__,__LINE__,[s](){return new TemplateTest(s);}); }
 if(!exact || suite=="Smoke")for(const auto &t:smoke_cards) { auto *s=&t; auto n="Card_"+std::to_string(t.csvid); if(selected("Smoke",n.c_str()))testing::RegisterTest("Smoke",n.c_str(),nullptr,nullptr,__FILE__,__LINE__,[s](){return new SmokeTest(s);}); }
 if(!requested.empty()) {
  auto remaining=requested;
  const auto &unit=*testing::UnitTest::GetInstance();
  for(int i=0;i<unit.total_test_suite_count();++i) {
   const auto &s=*unit.GetTestSuite(i);
   for(int j=0;j<s.total_test_count();++j)remaining.erase(std::string(s.name())+'.'+s.GetTestInfo(j)->name());
  }
  if(!remaining.empty()) {std::cerr<<"DUEL_HARNESS_ERROR: unregistered batch case "<<*remaining.begin()<<'\n';return 2;}
 }
 if(!journal.empty())testing::UnitTest::GetInstance()->listeners().Append(new BatchJournal(journal));
 if(GTEST_FLAG_GET(list_tests))return RUN_ALL_TESTS();
 if (!duel_test_initialize(DUEL_TEST_ASSETS)) { std::cerr<<"Missing real card database at "<<DUEL_TEST_ASSETS<<"\n"; return 2; }
 for(int i=1;i<argc;++i)if(!std::strcmp(argv[i],"--duel-presentation-probe"))duel_test_probe_presentation();
 return RUN_ALL_TESTS();
}
