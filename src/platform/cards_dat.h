#ifndef SHANDALAR_CARDS_DAT_H
#define SHANDALAR_CARDS_DAT_H

#include "defs.h"

/* CARDS.DAT always contains the original 32-bit, 0x98-byte record layout.
 * Pointer-like fields in this structure are offsets into the text block that
 * follows the records (artist is an index into const_db_artist_names). */
typedef struct card_disk_record_t
{
  int32_t id;
  uint32_t full_name_offset;
  uint32_t name_offset;
  uint32_t expansion;
  uint32_t color;
  uint32_t card_type;
  uint32_t subtype;
  uint32_t type_text_offset;
  uint32_t db_card_type_2;
  uint32_t rarity;
  casting_cost_t req;
  uint8_t unused0x31;
  uint16_t unused0x32;
  uint32_t flags;
  uint32_t unused0x38;
  uint32_t unused0x3c;
  uint32_t artist_index;
  int32_t num_pics;
  uint32_t mana_cost_text_offset;
  uint32_t ai_modifiers1;
  uint32_t ai_modifiers2;
  uint8_t ai_inc_power;
  uint8_t ai_inc_toughness;
  uint8_t ai_power;
  uint8_t ai_toughness;
  int16_t ai_base_value;
  int16_t ai_dependencies;
  int32_t sleight_color;
  int16_t ai_against_color;
  int16_t ai_for_color;
  int16_t ai_counts_as_color;
  int16_t ai_against_land;
  int16_t ai_for_land;
  int16_t ai_counts_as_land;
  int32_t ai_abilities;
  int32_t expansion_rarity;
  uint32_t rules_text_offset;
  uint32_t flavor_text_offset;
  int32_t power;
  int32_t toughness;
  int16_t types[7];
  int8_t mana_source_colors;
  int8_t inflatable;
  uint8_t hack_colors;
  uint8_t currently_zero_for_all_cards;
  uint8_t enchant_type;
  uint8_t hack_mode;
} card_disk_record_t;

STATIC_ASSERT(sizeof(card_disk_record_t) == 0x98, card_disk_record_must_match_cards_dat);

#endif
