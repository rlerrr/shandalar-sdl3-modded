#include "platform/file_io.h"
#include "card_pool.h"
#include "platform/iniparser.h"
#include "platform/legacy_io.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *section, *key, *value; } Entry;
typedef struct {
  Entry *entries;
  size_t count, capacity;
  char *visited[64];
  int visited_count, failed;
} Config;
typedef struct { int first, last, disposition, all; } Rule;

static const char *default_treasures[] = {
  "Ancestral Recall", "Balance", "Black Lotus", "Channel", "Contract from Below",
  "Demonic Tutor", "Fastbond", "Imperial Seal", "Library of Alexandria", "Lotus Petal",
  "Mana Crypt", "Mishra's Workshop", "Mox Emerald", "Mox Jet", "Mox Pearl", "Mox Ruby",
  "Mox Sapphire", "Mystical Tutor", "Necropotence", "Sol Ring", "Strip Mine", "Time Vault",
  "Time Walk", "Timetwister", "Tinker", "Tolarian Academy", "Vampiric Tutor",
  "Wheel of Fortune", "Windfall", "Yawgmoth's Bargain"
};

static int equal(const char *a, const char *b)
{
  while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
  return *a == *b;
}

static char *copy(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (p) memcpy(p, s, n);
  return p;
}

static void collect(void *context, const char *section, const char *key, const char *value)
{
  Config *cfg = context;
  Entry *entry;
  if (cfg->failed) return;
  if (cfg->count == cfg->capacity) {
    size_t capacity = cfg->capacity ? cfg->capacity * 2 : 64;
    Entry *p = realloc(cfg->entries, capacity * sizeof(*p));
    if (!p) { cfg->failed = 1; return; }
    cfg->entries = p;
    cfg->capacity = capacity;
  }
  entry = &cfg->entries[cfg->count++];
  entry->section = copy(section); entry->key = copy(key); entry->value = copy(value);
  if (!entry->section || !entry->key || !entry->value) cfg->failed = 1;
}

static SDL_IOStream *open_asset(const char *directory, const char *name, const char *mode)
{
  char path[2048];
  if (snprintf(path, sizeof(path), "%s/%s", directory, name) >= (int)sizeof(path)) return NULL;
  return shandalar_open_file_io(path, mode);
}

static int read_config(Config *cfg, const char *directory, const char *name, int optional)
{
  size_t begin = cfg->count, end, i;
  SDL_IOStream *file;
  dictionary *dict;
  char normalized[1024];
  size_t n = strlen(name);
  if (n >= sizeof(normalized)) return 0;
  /* Normalize separators and case for include-cycle detection. */
  for (i = 0; i <= n; ++i) normalized[i] = name[i] == '\\' ? '/' : (char)tolower((unsigned char)name[i]);
  for (i = 0; i < (size_t)cfg->visited_count; ++i)
    if (equal(cfg->visited[i], normalized)) return 1;
  if (cfg->visited_count == 64) { fprintf(stderr, "Shandalar.ini: too many included configurations\n"); return 0; }
  cfg->visited[cfg->visited_count] = copy(normalized);
  if (!cfg->visited[cfg->visited_count++]) return 0;
  file = open_asset(directory, name, "r");
  if (!file) {
    if (!optional) fprintf(stderr, "Shandalar.ini: cannot read included file %s\n", name);
    return optional;
  }
  dict = iniparser_load_io_entries(file, name, collect, cfg);
  shandalar_io_close(file);
  if (!dict || cfg->failed) { iniparser_freedict(dict); return 0; }
  iniparser_freedict(dict);
  end = cfg->count;
  /* Parent entries precede all its includes, in their listed order. */
  for (i = begin; i < end; ++i) {
    if (equal(cfg->entries[i].section, "configs") &&
        !read_config(cfg, directory, cfg->entries[i].key, 0)) return 0;
  }
  return 1;
}

static void free_config(Config *cfg)
{
  size_t i;
  for (i = 0; i < cfg->count; ++i) {
    free(cfg->entries[i].section); free(cfg->entries[i].key); free(cfg->entries[i].value);
  }
  for (i = 0; i < (size_t)cfg->visited_count; ++i) free(cfg->visited[i]);
  free(cfg->entries);
}

static int read_set_names(const char *directory, char names[256][256])
{
  SDL_IOStream *file = open_asset(directory, "Menus.txt", "r");
  char line[1024];
  int count = 0, i;
  if (!file) return 0;
  while (shandalar_io_gets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = 0;
    if (equal(line, "@EXPANSIONNAMES")) {
      if (!shandalar_io_gets(line, sizeof(line), file)) break;
      count = atoi(line);
      if (count < 9 || count > 256) { count = 0; break; }
      for (i = 0; i < count; ++i) {
        if (!shandalar_io_gets(line, sizeof(line), file)) { count = 0; break; }
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) >= 256) { count = 0; break; }
        strcpy(names[i], line);
      }
      break;
    }
  }
  shandalar_io_close(file);
  return count;
}

static uint32_t read_u32(const unsigned char *p)
{
  return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static unsigned char *read_printings(const char *directory, int sets, size_t *records, size_t *stride)
{
  SDL_IOStream *file = open_asset(directory, "Rarity.dat", "rb");
  unsigned char header[8], *data = NULL;
  if (!file) return NULL;
  if (shandalar_io_read(header, 1, 8, file) != 8 || read_u32(header + 4) < (unsigned)sets || read_u32(header + 4) > 256) goto done;
  *records = read_u32(header);
  /* Some distributions have two trailing unnamed sets in Rarity.dat. The
   * binary header determines the stride; Menus.txt determines named ranges. */
  *stride = ((size_t)read_u32(header + 4) * 3 + 7) / 8;
  if (!*records || *records > 100000) goto done;
  data = malloc(*records * *stride);
  if (data && shandalar_io_read(data, *stride, *records, file) != *records) { free(data); data = NULL; }
done:
  shandalar_io_close(file);
  return data;
}

static int set_index(char names[256][256], int count, const char *name)
{
  int i;
  for (i = 8; i < count; ++i) if (equal(names[i], name)) return i;
  return -1;
}

static int parse_rule(const Entry *entry, char names[256][256], int sets, Rule *rule)
{
  static const char *values[] = {"enable", "nosale", "nochoose", "disable", "ignore"};
  char range[1024], *last;
  int d;
  for (d = 0; d < 5 && !equal(values[d], entry->value); ++d) {}
  if (d == 5 || strlen(entry->key) >= sizeof(range)) return 0;
  strcpy(range, entry->key);
  last = strstr(range, "--");
  rule->all = equal(range, "--");
  if (last) {
    *last = 0; last += 2;
    rule->first = *range ? set_index(names, sets, range) : 8;
    rule->last = *last ? set_index(names, sets, last) : sets - 1;
  } else rule->first = rule->last = set_index(names, sets, range);
  rule->disposition = d;
  return rule->all || (rule->first >= 8 && rule->last >= rule->first);
}

static int find_card(const CardPoolCard *cards, size_t count, const char *key)
{
  char *end;
  long id = strtol(key, &end, 10);
  size_t i;
  int numeric = *key && !*end;
  for (i = 0; i < count; ++i)
    if ((numeric && id == cards[i].csvid) || (!numeric && cards[i].name && equal(cards[i].name, key))) return (int)i;
  return -1;
}

void card_pool_free(CardPool *pool)
{
  free(pool->dispositions);
  memset(pool, 0, sizeof(*pool));
}

int card_pool_allows(const CardPool *pool, int iid, CardPoolDisposition maximum)
{
  return iid >= 0 && (size_t)iid < pool->count && pool->dispositions[iid] <= maximum;
}

int card_pool_load(CardPool *pool, const char *directory, const CardPoolCard *cards, size_t count)
{
  static const char *sections[] = {"enable", "nosale", "nochoose", "disable", "dungeontreasure"};
  Config cfg = {0};
  CardPool result = {0};
  char (*names)[256] = calloc(256, 256);
  unsigned char ignored[256] = {0}, *printings = NULL;
  Rule *rules = NULL;
  size_t i, j, rule_count = 0, records = 0, stride = 0;
  int sets = 0, section, saw_treasures = 0, success = 0;
  if (!names || !read_config(&cfg, directory, "Shandalar.ini", 1)) goto done;
  result.count = count;
  result.dispositions = malloc(count ? count : 1);
  rules = calloc(cfg.count ? cfg.count : 1, sizeof(*rules));
  if (!result.dispositions || !rules) goto done;
  sets = read_set_names(directory, names);
  if (sets) printings = read_printings(directory, sets, &records, &stride);
  for (i = 0; i < cfg.count; ++i) {
    Rule rule;
    if (!equal(cfg.entries[i].section, "sets")) continue;
    if (!equal(cfg.entries[i].key, "--") && (!sets || !printings)) {
      fprintf(stderr, "Shandalar.ini: set rules require matching Menus.txt and Rarity.dat\n"); goto done;
    }
    if (!parse_rule(&cfg.entries[i], names, sets, &rule)) {
      fprintf(stderr, "Shandalar.ini: invalid set rule %s=%s\n", cfg.entries[i].key, cfg.entries[i].value); goto done;
    }
    if (rule.disposition == 4) {
      int s;
      for (s = rule.all ? 8 : rule.first; s <= (rule.all ? sets - 1 : rule.last); ++s) ignored[s] = 1;
    } else rules[rule_count++] = rule;
  }
  for (i = 0; i < count; ++i) {
    result.dispositions[i] = cards[i].eligible ? CARD_POOL_ENABLE : CARD_POOL_DISABLE;
    if (!cards[i].eligible) continue;
    for (j = 0; j < rule_count; ++j) {
      Rule *rule = &rules[j];
      int match = rule->all, s;
      if (printings && cards[i].csvid >= 0 && (size_t)cards[i].csvid < records) {
        const unsigned char *record = printings + cards[i].csvid * stride;
        for (s = rule->first; !match && s <= rule->last; ++s) {
          int bit = s * 3;
          unsigned bits = record[bit / 8];
          if (bit / 8 + 1 < (int)stride) bits |= (unsigned)record[bit / 8 + 1] << 8;
          if (!ignored[s] && ((bits >> (bit % 8)) & 7)) match = 1;
        }
      }
      if (match) { result.dispositions[i] = (unsigned char)rule->disposition; break; }
    }
  }
  for (section = 0; section < 5; ++section) {
    for (i = 0; i < cfg.count; ++i) {
      int iid;
      if (!equal(cfg.entries[i].section, sections[section])) continue;
      if (section == 4) saw_treasures = 1;
      iid = find_card(cards, count, cfg.entries[i].key);
      if (iid < 0 || !cards[iid].eligible) {
        fprintf(stderr, "Shandalar.ini: unavailable card in [%s]: %s\n", sections[section], cfg.entries[i].key);
        continue;
      }
      result.dispositions[iid] = (unsigned char)section;
      if (section == 4 && !cards[iid].basic_land) {
        for (j = 0; j < (size_t)result.treasure_count && result.treasures[j] != iid; ++j) {}
        if (j == (size_t)result.treasure_count && result.treasure_count < 30)
          result.treasures[result.treasure_count++] = iid;
      }
    }
  }
  if (!saw_treasures) {
    for (i = 0; i < 30; ++i) {
      int iid = find_card(cards, count, default_treasures[i]);
      if (iid >= 0 && cards[iid].eligible) {
        result.dispositions[iid] = CARD_POOL_DUNGEON_TREASURE;
        result.treasures[result.treasure_count++] = iid;
      }
    }
  }
  for (i = 0; i < count; ++i) if (cards[i].eligible && cards[i].basic_land) result.dispositions[i] = CARD_POOL_ENABLE;
  card_pool_free(pool);
  *pool = result;
  memset(&result, 0, sizeof(result));
  success = 1;
done:
  free(names); free(printings); free(rules); free_config(&cfg); card_pool_free(&result);
  return success;
}
