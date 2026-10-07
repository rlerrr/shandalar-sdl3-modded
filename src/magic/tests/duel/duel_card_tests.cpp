#include <gtest/gtest.h>
#include "duel_test_api.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <memory>
#include <string>
#ifndef DUEL_TEST_ASSETS
#define DUEL_TEST_ASSETS "/assets_modded"
#endif
struct Setup { const char *ref; int player, zone, csvid, state; };
struct Fixture { const char *name; const Setup *cards; int count, turn, combat; int life[2]; int mana[2][6]; };
struct Step { const char *op, *ref, *other, *property; int player, value, zone, index, color; };
struct Case { const char *name, *card, *oracle_id, *oracle_text, *fixture; unsigned seed; const Setup *cards; int count; const Step *steps; int step_count; };
struct Template { const char *name; int csvid, power, toughness; const char *oracle_id; };
struct Smoke { const char *name; int csvid; };
#include "duel_cases.inc"
using Refs = std::map<std::string,DuelTestRef>;
static std::string observable_state() {
  std::ostringstream out;
  out<<"{\"stack\":"<<duel_test_stack_size()<<",\"players\":[";
  for(int p=0;p<2;++p) {
    if(p)out<<',';
    out<<"{\"life\":"<<duel_test_life(p)<<",\"mana\":[";
    for(int color=0;color<6;++color){if(color)out<<',';out<<duel_test_mana(p,color);}
    out<<"],\"zones\":[";
    for(int zone=0;zone<5;++zone){if(zone)out<<',';out<<'[';for(int n=0;n<duel_test_zone_count(p,zone);++n){if(n)out<<',';out<<duel_test_zone_csvid(p,zone,n);}out<<']';}
    out<<"],\"permanents\":[";
    bool comma=false;
    for(int n=0;n<150;++n){auto c=duel_test_inspect({p,n});if(!c.in_play)continue;if(comma)out<<',';comma=true;out<<'['<<n<<','<<c.csvid<<','<<c.power<<','<<c.toughness<<','<<c.tapped<<','<<c.sick<<','<<c.attacking<<','<<c.damage<<']';}
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
  std::unique_ptr<DuelTestSnapshot,decltype(&duel_test_free_snapshot)> saved(nullptr,duel_test_free_snapshot);
  for(int i=0;i<spec->step_count;++i) {
    const auto &s=spec->steps[i];
    SCOPED_TRACE(std::string("step ")+std::to_string(i)+" "+s.op+" "+s.ref);
    auto ref = refs.count(s.ref) ? refs.at(s.ref) : DuelTestRef{-1,-1};
    const std::string op=s.op;
    if(op=="choose_target")duel_test_decision(DUEL_CHOICE_TARGET,s.player,ref.player,ref.card);
    else if(op=="choose_zone")duel_test_decision(DUEL_CHOICE_ZONE,s.player,s.value,-1);
    else if(op=="choose_dialog")duel_test_decision(DUEL_CHOICE_DIALOG,s.player,s.value,-1);
    else if(op=="choose_number")duel_test_decision(DUEL_CHOICE_NUMBER,s.player,s.value,-1);
    else if(op=="choose_color")duel_test_decision(DUEL_CHOICE_COLOR,s.player,s.value,-1);
    else if(op=="choose_priority")duel_test_decision(DUEL_CHOICE_PRIORITY,ref.player>=0?ref.player:s.player,ref.card>=0?ref.card:s.value,-1);
    else if(op=="choose_trigger")duel_test_decision(DUEL_CHOICE_PRIORITY,s.player,-3,duel_test_inspect(ref).csvid);
    else if(op=="can_activate")EXPECT_EQ(!!duel_test_can_activate(ref),s.value);
    else if(op=="activate")EXPECT_EQ(!!duel_test_activate(ref),s.value);
    else if(op=="can_cast")EXPECT_EQ(!!duel_test_can_cast(ref),s.value);
    else if(op=="cast")EXPECT_EQ(!!duel_test_cast(ref),s.value);
    else if(op=="resolve")duel_test_resolve();
    else if(op=="cleanup")duel_test_cleanup();
    else if(op=="remove_from_combat")duel_test_remove_from_combat(ref);
    else if(op=="destroy")duel_test_destroy(ref);
    else if(op=="attack")EXPECT_EQ(!!duel_test_attack(ref),s.value);
    else if(op=="block")EXPECT_EQ(!!duel_test_block(ref,refs.at(s.other)),s.value);
    else if(op=="combat_damage")duel_test_combat_damage();
    else if(op=="turn")duel_test_set_turn(s.player,s.value);
    else if(op=="assert_card")EXPECT_EQ(property(duel_test_inspect(ref),s.property),s.value);
    else if(op=="assert_zone")EXPECT_EQ(duel_test_zone_count(s.player,s.zone),s.value);
    else if(op=="assert_zone_card")EXPECT_EQ(duel_test_zone_csvid(s.player,s.zone,s.index),s.value);
    else if(op=="assert_zone_contains") {
      bool found=false; for(int n=0;n<duel_test_zone_count(s.player,s.zone);++n) if(duel_test_zone_csvid(s.player,s.zone,n)==s.value)found=true;
      EXPECT_TRUE(found)<<"Missing CSVID "<<s.value;
    }
    else if(op=="assert_zone_state") {
      int found=0; DuelTestRef r={s.player,0};
      for(int n=0;n<151;++n) { r.card=n; if(duel_test_inspect(r).in_play && found++==s.index)break; }
      ASSERT_LT(r.card,151); EXPECT_EQ(property(duel_test_inspect(r),s.property),s.value);
    }
    else if(op=="assert_mana")EXPECT_EQ(duel_test_mana(s.player,s.color),s.value);
    else if(op=="assert_life")EXPECT_EQ(duel_test_life(s.player),s.value);
    else if(op=="assert_stack")EXPECT_EQ(duel_test_stack_size(),s.value);
    else if(op=="capture")saved.reset(duel_test_capture());
    else if(op=="restore") { ASSERT_NE(saved.get(),nullptr); duel_test_restore(saved.get()); }
    else if(op=="ai_roundtrip")duel_test_ai_roundtrip();
    else if(op=="ai_turns")duel_test_ai_turns(s.value);
    else FAIL()<<"Unimplemented action "<<op;
    duel_test_check_invariants();
    if(HasFailure())duel_test_dump();
  }
  duel_test_finish_decisions();
  std::cout<<"OBSERVATION: "<<observable_state()<<'\n';
 }
};
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
 for(unsigned seed:{1u,123u,999u})testing::RegisterTest("AI",("Seed_"+std::to_string(seed)).c_str(),nullptr,nullptr,__FILE__,__LINE__,[seed](){return new AITest(seed);});
 for(const auto *c:cases) testing::RegisterTest("Behavior",c->name,nullptr,nullptr,__FILE__,__LINE__,[c](){return new BehaviorTest(c);});
 for(const auto &t:templates) { auto *s=&t; auto name="Card_"+std::to_string(t.csvid); testing::RegisterTest("Templates",name.c_str(),nullptr,nullptr,__FILE__,__LINE__,[s](){return new TemplateTest(s);}); }
 for(const auto &t:smoke_cards) { auto *s=&t; auto name="Card_"+std::to_string(t.csvid); testing::RegisterTest("Smoke",name.c_str(),nullptr,nullptr,__FILE__,__LINE__,[s](){return new SmokeTest(s);}); }
 testing::InitGoogleTest(&argc,argv);
 for(int i=1;i<argc;++i)if(!std::strcmp(argv[i],"--gtest_list_tests"))return RUN_ALL_TESTS();
 if (!duel_test_initialize(DUEL_TEST_ASSETS)) { std::cerr<<"Missing real card database at "<<DUEL_TEST_ASSETS<<"\n"; return 2; }
 for(int i=1;i<argc;++i)if(!std::strcmp(argv[i],"--duel-presentation-probe"))duel_test_probe_presentation();
 return RUN_ALL_TESTS();
}
