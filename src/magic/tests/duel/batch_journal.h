#ifndef DUEL_BATCH_JOURNAL_H
#define DUEL_BATCH_JOURNAL_H

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>

static std::string journal_string(const std::string &value) {
  static const char hex[]="0123456789abcdef";
  std::string out="\"";
  for(unsigned char c:value) {
    if(c=='"' || c=='\\') {out+='\\';out+=c;}
    else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
    else out+=c;
  }
  return out+'"';
}

// Flush every test independently: a later trap must not erase earlier results.
class BatchJournal:public testing::EmptyTestEventListener {
  std::ofstream stream;
  void write(const std::string &record) {
    stream<<record<<'\n'; stream.flush();
    if(!stream) {std::cerr<<"DUEL_HARNESS_ERROR: cannot write batch journal\n";std::abort();}
  }
 public:
  explicit BatchJournal(const std::string &path):stream(path,std::ios::trunc) {
    write("{\"event\":\"protocol\",\"version\":1}");
  }
  void OnTestStart(const testing::TestInfo &info) override {
    std::cout.flush();std::cerr.flush();std::fflush(nullptr);
    write("{\"event\":\"start\",\"filter\":"+journal_string(std::string(info.test_suite_name())+'.'+info.name())+'}');
  }
  void OnTestEnd(const testing::TestInfo &info) override {
    const auto &result=*info.result();
    std::ostringstream out;
    out<<"{\"event\":\"end\",\"filter\":"<<journal_string(std::string(info.test_suite_name())+'.'+info.name())
       <<",\"passed\":"<<(result.Passed()?"true":"false")<<",\"skipped\":"<<(result.Skipped()?"true":"false")
       <<",\"elapsed_ms\":"<<result.elapsed_time()<<",\"failures\":[";
    bool comma=false;
    for(int i=0;i<result.total_part_count();++i) {
      const auto &part=result.GetTestPartResult(i);
      if(!part.failed())continue;
      if(comma)out<<',';comma=true;
      out<<journal_string(part.message());
    }
    out<<"]}";
    std::cout.flush();std::cerr.flush();std::fflush(nullptr);
    write(out.str());
  }
};
#endif
