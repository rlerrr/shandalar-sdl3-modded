#pragma once
#include "duel_case_types.h"
#include "miniz.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

// Version 1 uses explicit little-endian integers, a shared UTF-8 string table,
// and independently compressed case records. No native pointers are stored.
class CaseArchive {
 static constexpr uint64_t header_size=48, index_size=24, max_record=64*1024*1024;
 std::ifstream file;
 std::vector<std::string> strings;
 uint64_t data_end=0;
 [[noreturn]] static void invalid(const std::string &message) {
  // Emscripten disables C++ exception catching in this build. Format failures
  // must still produce the same explicit harness diagnostic on both targets.
  std::fprintf(stderr,"DUEL_HARNESS_ERROR: invalid duel case archive: %s\n",message.c_str());
  std::fflush(stderr);std::abort();
 }
 static uint32_t u32(const unsigned char *p) {
  return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
 }
 static uint64_t u64(const unsigned char *p) {return u32(p)|(uint64_t(u32(p+4))<<32);}
 static int number(const unsigned char *p) {
  uint32_t bits=u32(p);int32_t value;std::memcpy(&value,&bits,4);return value;
 }
 std::vector<unsigned char> read(uint64_t offset,uint64_t size) {
  if(size>256*1024*1024)invalid("oversized read");
  std::vector<unsigned char> data(static_cast<size_t>(size));
  file.clear();file.seekg(static_cast<std::streamoff>(offset));
  if(size)file.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));
  if(!file || (size && uint64_t(file.gcount())!=size))invalid("truncated data");
  return data;
 }
 const char *string(uint32_t id) const {
  if(id>=strings.size())invalid("string ID out of bounds");
  return strings[id].c_str();
 }
 static bool checksum(const std::vector<unsigned char> &data,uint32_t expected) {
  return mz_crc32(0,data.data(),data.size())==expected;
 }
 public:
 struct Entry {const char *name;uint64_t offset;uint32_t packed,raw,crc;};
 struct Loaded {
  Case spec{};
  std::vector<Setup> cards;
  std::vector<Step> steps;
 };
 std::vector<Entry> entries;

 explicit CaseArchive(const std::string &path):file(path,std::ios::binary) {
  if(!file)invalid("cannot read "+path);
  file.seekg(0,std::ios::end);auto end=file.tellg();
  if(end<static_cast<std::streamoff>(header_size))invalid("truncated header");
  const uint64_t size=static_cast<uint64_t>(end);auto header=read(0,header_size);const auto *h=header.data();
  const uint32_t count=u32(h+12);data_end=u64(h+16);const uint64_t index_offset=u64(h+24);
  if(std::memcmp(h,"DUELCASE",8) || u32(h+8)!=1 || u64(h+32)!=size ||
     count>2000000 || data_end<header_size || data_end>index_offset || index_offset>size ||
     size-index_offset!=uint64_t(count)*index_size)invalid("header or version mismatch");
  auto table=read(data_end,index_offset-data_end);auto index=read(index_offset,size-index_offset);
  if(!checksum(table,u32(h+40)) || !checksum(index,u32(h+44)))invalid("metadata checksum mismatch");
  if(table.size()<4)invalid("truncated string table");
  const uint32_t string_count=u32(table.data());
  if(string_count>2000000)invalid("oversized string table");
  strings.reserve(string_count);size_t cursor=4;
  for(uint32_t i=0;i<string_count;++i) {
   if(cursor+4>table.size())invalid("truncated string length");
   uint32_t length=u32(table.data()+cursor);cursor+=4;
   if(length>1024*1024 || length>table.size()-cursor)invalid("truncated or oversized string");
   strings.emplace_back(reinterpret_cast<const char*>(table.data()+cursor),length);cursor+=length;
   if(strings.back().find('\0')!=std::string::npos)invalid("NUL in string");
  }
  if(cursor!=table.size())invalid("trailing string data");
  entries.reserve(count);std::unordered_set<std::string> names;
  for(uint32_t i=0;i<count;++i) {
   const auto *p=index.data()+i*index_size;const char *name=string(u32(p));
   Entry entry{name,u64(p+4),u32(p+12),u32(p+16),u32(p+20)};
   if(!names.insert(name).second)invalid("duplicate case name");
   if(entry.offset<header_size || entry.offset>data_end || entry.packed>data_end-entry.offset ||
      entry.raw<32 || entry.raw>max_record || entry.packed>max_record)invalid("case bounds");
   entries.push_back(entry);
  }
 }

 Loaded load(const Entry &entry) {
  auto packed=read(entry.offset,entry.packed);std::vector<unsigned char> raw(entry.raw);
  mz_ulong raw_size=entry.raw,packed_size=entry.packed;
  if(mz_uncompress2(raw.data(),&raw_size,packed.data(),&packed_size)!=MZ_OK ||
     raw_size!=entry.raw || packed_size!=entry.packed || !checksum(raw,entry.crc))invalid("case compression or checksum");
  const auto *p=raw.data();const uint32_t cards=u32(p+24),steps=u32(p+28);
  if(32+uint64_t(cards)*20+uint64_t(steps)*36!=raw.size() || std::strcmp(string(u32(p)),entry.name))invalid("case shape or name");
  Loaded result;
  result.spec={string(u32(p)),string(u32(p+4)),string(u32(p+8)),string(u32(p+12)),string(u32(p+16)),u32(p+20),nullptr,static_cast<int>(cards),nullptr,static_cast<int>(steps)};
  result.cards.reserve(cards);result.steps.reserve(steps);p+=32;
  for(uint32_t i=0;i<cards;++i,p+=20)result.cards.push_back({string(u32(p)),number(p+4),number(p+8),number(p+12),number(p+16)});
  for(uint32_t i=0;i<steps;++i,p+=36)result.steps.push_back({string(u32(p)),string(u32(p+4)),string(u32(p+8)),string(u32(p+12)),number(p+16),number(p+20),number(p+24),number(p+28),number(p+32)});
  result.spec.cards=result.cards.data();result.spec.steps=result.steps.data();return result;
 }
};
