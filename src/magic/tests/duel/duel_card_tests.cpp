#include <gtest/gtest.h>
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <crtdbg.h>
#include <windows.h>
#endif
#include "duel_test_api.h"
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
#include "duel_cases.inc"
extern "C" int ai_opinion_of_gamestate_continued(int player, int score);
extern "C" int manalinkeh_charge_mana_for_multi_x(int player, int color, int count);
extern "C" int g_max_x_value;
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
 const Case *spec;
 public:
 explicit BehaviorTest(const Case *c):spec(c){}
 void TestBody() override {
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
    else if(op=="can_activate")EXPECT_EQ(!!duel_test_can_activate(ref),s.value);
    else if(op=="require_can_activate")ASSERT_EQ(!!duel_test_can_activate(ref),s.value);
    else if(op=="activate")EXPECT_EQ(!!duel_test_activate(ref),s.value);
    else if(op=="can_activate_hand")EXPECT_EQ(!!duel_test_can_activate_hand(ref),s.value);
    else if(op=="require_can_activate_hand")ASSERT_EQ(!!duel_test_can_activate_hand(ref),s.value);
    else if(op=="activate_hand")ASSERT_EQ(!!duel_test_activate_hand(ref),s.value);
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
    else if(op=="discard_policy")duel_test_set_discard_policy(s.value);
    else if(op=="assert_discard_choices")EXPECT_EQ(duel_test_discard_choice_count(),s.value);
    else if(op=="refresh")duel_test_refresh();
    else if(op=="resolve")duel_test_resolve();
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
    else if(op=="set_mana")duel_test_set_mana(s.player,s.color,s.value);
    else if(op=="set_life")duel_test_set_life(s.player,s.value);
    else if(op=="set_budget")duel_test_set_budget(s.value);
    else if(op=="assert_card_name")EXPECT_STREQ(duel_test_card_name(ref),spec->card);
    else if(op=="assert_payment_source")EXPECT_NE(std::string(duel_test_payment_prompt()).find(spec->card),std::string::npos);
    else if(op=="assert_card")EXPECT_EQ(property(duel_test_inspect(ref),s.property),s.value);
    else if(op=="require_card")ASSERT_EQ(property(duel_test_inspect(ref),s.property),s.value);
    else if(op=="assert_zone")EXPECT_EQ(duel_test_zone_count(s.player,s.zone),s.value);
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
  for(int owner:{0,1})for(int count:{2,3})for(int x:{0,2,4}) {
    Refs refs;load_fixture(refs,"empty_main",123);duel_test_set_turn(owner,0);
    for(int c=0;c<6;++c)duel_test_set_mana(owner,c,c==0?20:0);duel_test_refresh();g_max_x_value=-1;
    duel_test_decision(DUEL_CHOICE_X,owner,x,-1);
    EXPECT_EQ(manalinkeh_charge_mana_for_multi_x(owner,0,count),count*x);
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
int main(int argc,char**argv) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
 _set_error_mode(_OUT_TO_STDERR);
 _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
 _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
 std::string case_list,journal;
 for(int i=1;i<argc;++i) {
  std::string arg=argv[i];
  if(arg.rfind("--duel-case-list=",0)==0)case_list=arg.substr(17);
  if(arg.rfind("--duel-batch-journal=",0)==0)journal=arg.substr(21);
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
 if(!exact || suite=="Behavior")for(const auto *c:cases)if(selected("Behavior",c->name))testing::RegisterTest("Behavior",c->name,nullptr,nullptr,__FILE__,__LINE__,[c](){return new BehaviorTest(c);});
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
