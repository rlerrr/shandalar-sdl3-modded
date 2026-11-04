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
#ifndef DUEL_TEST_ASSETS
#define DUEL_TEST_ASSETS "/assets_modded"
#endif
#include "duel_case_types.h"
#include "duel_cases.inc"
using Refs = std::map<std::string,DuelTestRef>;
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
  for(int i=0;i<spec->step_count;++i) {
    const auto &s=spec->steps[i];
    if(std::getenv("DUEL_TEST_TRACE"))std::cerr<<"STEP "<<i<<' '<<s.op<<' '<<s.ref<<'\n';
    SCOPED_TRACE(std::string("step ")+std::to_string(i)+" "+s.op+" "+s.ref);
    auto ref = refs.count(s.ref) ? refs.at(s.ref) : DuelTestRef{-1,-1};
    const std::string op=s.op;
    if(op=="choose_target")duel_test_decision(DUEL_CHOICE_TARGET,s.player,ref.player,ref.card);
    else if(op=="choose_optional_zone")duel_test_decision(DUEL_CHOICE_ZONE,s.player,-5,s.value);
    else if(op=="assert_forced_zone_choices")EXPECT_EQ(duel_test_forced_zone_choices(),s.value);
    else if(op=="choose_damage_target")duel_test_damage_decision(s.player,ref);
    else if(op=="choose_damage_player")duel_test_damage_decision(s.player,DuelTestRef{s.value,-1});
    else if(op=="choose_cancel_target")duel_test_decision(DUEL_CHOICE_TARGET,s.player,-1,-1);
    else if(op=="assert_offered_count")EXPECT_EQ(duel_test_offered_count(),s.value);
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
    else if(op=="can_cast")EXPECT_EQ(!!duel_test_can_cast(ref),s.value);
    else if(op=="can_target_creature")EXPECT_EQ(!!duel_test_can_target_creature(ref,refs.at(s.other)),s.value);
    else if(op=="cast")EXPECT_EQ(!!duel_test_cast(ref),s.value);
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
    else if(op=="assert_zone_multiplicity") {
      int count=0;
      for(int n=0;n<duel_test_zone_count(s.player,s.zone);++n)
        count+=duel_test_zone_csvid(s.player,s.zone,n)==s.index;
      EXPECT_EQ(count,s.value)<<"Zone "<<s.zone<<" CSVID "<<s.index;
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
    else if(op=="bind_permanent_copy") {
      DuelTestRef selected={s.player,-1};int ordinal=0;
      for(int index=0;index<duel_test_zone_count(s.player,DUEL_ZONE_BATTLEFIELD);++index) {
        auto candidate=duel_test_permanent_at(s.player,index);
        if(duel_test_inspect(candidate).csvid==s.value && ordinal++==s.index)selected=candidate;
      }
      ASSERT_GE(selected.card,0)<<"Missing permanent copy "<<s.index<<" of CSVID "<<s.value;
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
