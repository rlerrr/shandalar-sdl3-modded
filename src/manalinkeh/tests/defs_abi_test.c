#include "../../defs.h"

enum {
  root_counter_limit = COUNTER_end,
  root_planeswalker_bit = TARGET_TYPE_PLANESWALKER,
  root_milled_trigger = XTRIGGER_MILLED,
  root_untap_flag = PB_SEND_EVENT_UNTAP_CARD_TO_ALL
};
#include "../defs.h"

STATIC_ASSERT(COUNTER_end == root_counter_limit, shared_counter_limit);
STATIC_ASSERT(TARGET_TYPE_PLANESWALKER == root_planeswalker_bit, shared_planeswalker_bit);
STATIC_ASSERT(XTRIGGER_MILLED == root_milled_trigger, shared_milled_trigger);
STATIC_ASSERT(PB_SEND_EVENT_UNTAP_CARD_TO_ALL == root_untap_flag, shared_untap_flag);

#include <stddef.h>
#include <stdio.h>
#include <string.h>

STATIC_ASSERT(sizeof(card_instance_t) == 300, host_instance_size);
STATIC_ASSERT(sizeof(card_data_t) == 72, host_engine_record_size);
STATIC_ASSERT(offsetof(card_data_t, id) == 0x24, engine_id_offset);
STATIC_ASSERT(sizeof(((card_data_t *)0)->id) == 4, engine_id_width);
STATIC_ASSERT(offsetof(card_data_t, type) == 0x28, engine_type_offset);
STATIC_ASSERT(offsetof(card_data_t, cc) == 0x2b, engine_cost_offset);
STATIC_ASSERT(offsetof(card_data_t, unused0x43) == 0x43, engine_reserved_byte_offset);
STATIC_ASSERT(offsetof(card_data_t, ai_base_value) == 0x44, engine_rating_offset);
STATIC_ASSERT(sizeof(((card_data_t *)0)->ai_base_value) == 4, engine_rating_width);
STATIC_ASSERT(sizeof(card_ptr_t) == 152, host_cards_dat_record_size);
STATIC_ASSERT(sizeof(stack_data_t) == 4, host_stack_record_size);
STATIC_ASSERT(offsetof(stack_data_t, internal_card_id) == 0, stack_card_id_offset);
STATIC_ASSERT(offsetof(stack_data_t, generating_event) == 2, stack_event_offset);
STATIC_ASSERT(offsetof(stack_data_t, unknown_sdt) == 3, stack_trigger_argument_offset);
STATIC_ASSERT(_Alignof(card_instance_t) == 4, host_instance_alignment);
STATIC_ASSERT(_Alignof(card_data_t) == 4, host_engine_record_alignment);
STATIC_ASSERT(_Alignof(card_ptr_t) == 4, host_cards_dat_record_alignment);
STATIC_ASSERT(_Alignof(stack_data_t) == 2, host_stack_record_alignment);
STATIC_ASSERT(offsetof(card_instance_t, targets) == 0x74, host_target_offset);
STATIC_ASSERT(offsetof(card_data_t, code_pointer) == 0x34, host_handler_offset);
STATIC_ASSERT(offsetof(card_ptr_t, types) == 0x84, host_subtype_offset);
STATIC_ASSERT(offsetof(card_ptr_t, subtype1) == 0x18, cards_dat_first_subtype);
STATIC_ASSERT(offsetof(card_ptr_t, subtype2) == 0x1a, cards_dat_second_subtype);
STATIC_ASSERT(offsetof(card_ptr_t, req) == 0x28, cards_dat_casting_cost);
STATIC_ASSERT(offsetof(card_ptr_t, ai_inc_power) == 0x54, cards_dat_ai_power_increment);
STATIC_ASSERT(offsetof(card_ptr_t, ai_inc_toughness) == 0x55, cards_dat_ai_toughness_increment);
STATIC_ASSERT(offsetof(card_ptr_t, ai_power) == 0x56, cards_dat_ai_power);
STATIC_ASSERT(offsetof(card_ptr_t, ai_toughness) == 0x57, cards_dat_ai_toughness);
STATIC_ASSERT(sizeof(((card_ptr_t *)0)->types) == 14, cards_dat_expanded_subtypes);
STATIC_ASSERT(offsetof(card_ptr_t, mana_source_colors) == 0x92, cards_dat_after_subtypes);
STATIC_ASSERT(sizeof(((card_instance_t *)0)->special_counters) == 4, host_counter_word);
STATIC_ASSERT(sizeof(((card_instance_t *)0)->special_counters_byte) == 1, imported_counter_byte);
STATIC_ASSERT(offsetof(card_instance_t, special_counters_byte) == 0, special_counter_byte_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters2) == 1, counter2_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters3) == 2, counter3_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters4) == 3, counter4_offset);
STATIC_ASSERT(offsetof(card_instance_t, damage_on_card) == 0x10, unsigned_damage_offset);
STATIC_ASSERT(offsetof(card_instance_t, blocking) == 0x24, unsigned_blocking_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters) == 0x120, counter_word_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters_byte) == 0x120, counter_byte_offset);
STATIC_ASSERT(sizeof(((card_instance_t *)0)->counters_byte) == 1, imported_plus_one_counter_byte);
STATIC_ASSERT(offsetof(card_instance_t, counters5) == 0x121, counter5_offset);
STATIC_ASSERT(offsetof(card_instance_t, unknown0x122) == 0x122, counter_padding_offset);
STATIC_ASSERT(offsetof(card_instance_t, upkeep_cost) == 0x124, upkeep_array_offset);
STATIC_ASSERT(offsetof(card_instance_t, counters_m1m1) == 0x12b, minus_one_counter_offset);
STATIC_ASSERT(TARGET_TYPE_PLANESWALKER == 0x100, host_planeswalker_bit);
STATIC_ASSERT(COUNTER_end == COUNTER_STASH + 1, complete_counter_range);
STATIC_ASSERT(DFC_SYMBOL_MAX == DFC_SYMBOL_FAN_OPEN, complete_dfc_range);
STATIC_ASSERT(ICON_MAX == ICON_DECAYED, complete_icon_range);
STATIC_ASSERT(ICON_CANT_ACTIVATE != ICON_CANT_ACTIVATE_TAP, distinct_activation_icons);
STATIC_ASSERT((PAUE_END_IF_TARGET_CHANGES_CONTROL &
              (PAUE_ADD_SUBTYPE | PAUE_FORCE_PT | PAUE_FORCE_SUBTYPE | PAUE_FORCE_COLOR)) == 0,
              distinct_control_duration);
STATIC_ASSERT(ICON_LOST_ABILITIES != ICON_CANT_ACTIVATE, distinct_lost_ability_icon);
STATIC_ASSERT(WAV_HIGHEST == WAV_DICE, complete_sound_range);
STATIC_ASSERT((GA_GRAVE_ABILITY_MASK & GA_EMBALM) != 0, embalm_in_graveyard_mask);
STATIC_ASSERT((GA_GRAVE_ABILITY_MASK & GA_INTERRUPT_ABILITY) == 0, interrupt_outside_graveyard_mask);
STATIC_ASSERT(STATUS_DURATION_IN_EOT1 != STATUS_BASICLAND_DEPENDANT, distinct_effect_status_bits);

/* Extended trigger identities and enable bits must never overlap. */
static int check_trigger_constants(void)
{
  const xtrigger_t triggers[] = {
    XTRIGGER_ATTACKING,
    XTRIGGER_REPLACE_MILL,
    XTRIGGER_MILLED,
    XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY,
    XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY,
    XTRIGGER_SCRY,
    XTRIGGER_BECAME_RENOWNED,
    XTRIGGER_DISCARDED,
    XTRIGGER_LAND_PLAYED,
    XTRIGGER_REPLACE_GAIN_LIFE,
    XTRIGGER_CARD_SPECIFIC,
    XTRIGGER_REPLACE_KILL,
    XTRIGGER_CARD_DRAWN,
    XTRIGGER_THIS_DIES,
    XTRIGGER_LOSE_LIFE,
    XTRIGGER_MAIN_PHASE,
    XTRIGGER_COMBAT,
    XTRIGGER_SHUFFLE,
    XTRIGGER_TRANSFORM,
    XTRIGGER_REPLACE_TOKENS,
    XTRIGGER_REPLACE_COUNTERS,
    XTRIGGER_COUNTERS,
    XTRIGGER_COUNTER_ENERGY,
    XTRIGGER_1_1_COUNTERS
  };
  const unsigned int flags[] = {
    ENABLE_XTRIGGER_REPLACE_MILL,
    ENABLE_XTRIGGER_MILLED,
    ENABLE_XTRIGGER_REPLACE_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY,
    ENABLE_XTRIGGER_CARD_TO_GY_FROM_ANYWHERE_BUT_LIBRARY,
    ENABLE_XTRIGGER_REPLACE_KILL,
    ENABLE_TRIGGER_REPLACE_CARD_DRAW,
    ENABLE_XTRIGGER_CARD_DRAWN,
    ENABLE_TRIGGER_BOUNCE_PERMANENT,
    ENABLE_XTRIGGER_MAIN_PHASE,
    ENABLE_XTRIGGER_COMBAT,
    ENABLE_XTRIGGER_SHUFFLE,
    ENABLE_XTRIGGER_COUNTERS,
    ENABLE_XTRIGGER_1_1_COUNTERS,
    ENABLE_XTRIGGER_COUNTER_ENERGY
  };
  unsigned int i, j, seen = 0;
  for (i = 0; i < sizeof(triggers) / sizeof(triggers[0]); ++i)
    for (j = 0; j < i; ++j)
      if (triggers[i] == triggers[j])
      {
        fprintf(stderr, "Extended trigger IDs overlap\n");
        return 1;
      }
  for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
  {
    if (flags[i] == 0 || (flags[i] & (flags[i] - 1)) || (seen & flags[i]))
    {
      fprintf(stderr, "Trigger enable flags are not independent bits\n");
      return 1;
    }
    seen |= flags[i];
  }
  return 0;
}

int main(void)
{
  card_instance_t host = {0}, restored;
  card_instance_t imported;
  card_data_t host_data = {0};
  unsigned char engine_bytes[sizeof(card_data_t)] = {0};
  card_ptr_t metadata = {0};
  const unsigned char ai_bytes[] = {0x80, 0x7f, 0x01, 0xff};
  const uint32_t saved_stack[] = {0xffb3ffffu, 0x017e1234u};
  stack_data_t stack[2];
  uint32_t restored_stack[2];
  int lower = 2, upper = 3;

  if (check_trigger_constants()) return 1;

  /* The host saves stack entries as dwords; imported code accesses their fields. */
  memcpy(stack, saved_stack, sizeof(stack));
  if (stack[0].internal_card_id != -1 ||
      stack[0].generating_event != EVENT_RESOLVE_ACTIVATION_FROM_HAND ||
      stack[0].unknown_sdt != -1 || stack[1].internal_card_id != 0x1234 ||
      stack[1].generating_event != EVENT_RESOLVE_TRIGGER || stack[1].unknown_sdt != 1)
  {
    fprintf(stderr, "Stack fields no longer match the saved dwords\n");
    return 1;
  }
  stack[0].generating_event = EVENT_RESOLVE_SPLICE;
  stack[1].unknown_sdt = -2;
  memcpy(restored_stack, stack, sizeof(restored_stack));
  if (restored_stack[0] != 0xffc2ffffu || restored_stack[1] != 0xfe7e1234u)
  {
    fprintf(stderr, "Stack field writes changed neighboring saved bytes\n");
    return 1;
  }

  /* Alternate field views must describe the same little-endian asset bytes. */
  metadata.subtype = 0x33441122u;
  memcpy((unsigned char *)&metadata + 0x54, ai_bytes, sizeof(ai_bytes));
  if (metadata.subtype1 != 0x1122 || metadata.subtype2 != 0x3344 ||
      metadata.ai_inc_power != 0x80 || metadata.ai_inc_toughness != 0x7f ||
      metadata.ai_power != 1 || metadata.ai_toughness != 0xff)
  {
    fprintf(stderr, "Cards.dat field views no longer share the original bytes\n");
    return 1;
  }

  if (MIN(lower++, upper++) != 2 || lower != 3 || upper != 4)
  {
    fprintf(stderr, "Imported comparison helpers must evaluate arguments once\n");
    return 1;
  }

  /* A saved counter word is four independent bytes in the imported API. */
  host.special_counters = 0x04030201u;
  host.counters = 0x88776655u;
  host.display_pic_info = 0x44332211u;
  host.damage_on_card = 65535;
  host.blocking = 255;
  memcpy(&imported, &host, sizeof(imported));
  if (imported.special_counters_byte != 1 || imported.counters2 != 2 ||
      imported.counters3 != 3 || imported.counters4 != 4 ||
      imported.counters_byte != 0x55 || imported.counters5 != 0x66 ||
      imported.unknown0x122 != 0x8877 || imported.damage_on_card != 65535 ||
      imported.blocking != 255 ||
      imported.display_pic_csv_id != 0x2211 || imported.display_pic_num != 0x4433)
  {
    fprintf(stderr, "Imported instance view changed the saved record\n");
    return 1;
  }
  imported.counters3 = 0x5a;
  imported.blocking = 149;
  imported.display_pic_num = 0x6677;
  memcpy(&restored, &imported, sizeof(restored));
  if (restored.special_counters != 0x045a0201u ||
      restored.display_pic_info != 0x66772211u || restored.damage_on_card != 65535 ||
      restored.blocking != 149)
  {
    fprintf(stderr, "Imported field writes changed neighboring host fields\n");
    return 1;
  }

  /* Imported byte counters wrap independently, preserving adjacent slots. */
  imported.special_counters_byte = imported.counters_byte = 255;
  ++imported.special_counters_byte;
  ++imported.counters_byte;
  if (imported.special_counters != 0x045a0200u || imported.counters != 0x88776600u)
  {
    fprintf(stderr, "Counter byte updates carried into neighboring slots\n");
    return 1;
  }

  /* Keep the reserved byte, signed X cost, and signed engine rating distinct. */
  engine_bytes[0x24] = 0x34;
  engine_bytes[0x25] = 0x12;
  engine_bytes[0x28] = TYPE_CREATURE;
  engine_bytes[0x2c] = 0xff;
  engine_bytes[0x43] = 4;
  engine_bytes[0x44] = 0xfe;
  engine_bytes[0x45] = engine_bytes[0x46] = engine_bytes[0x47] = 0xff;
  memcpy(&host_data, engine_bytes, sizeof(host_data));
  if (host_data.id != 0x1234 || host_data.type != TYPE_CREATURE ||
      host_data.cc[1] != -1 || (uint8_t)host_data.cc[1] != 255 ||
      host_data.unused0x43 != 4 || host_data.ai_base_value != -2)
  {
    fprintf(stderr, "Engine record fields no longer match the original bytes\n");
    return 1;
  }
  /* A complete ID assignment replaces both halves of the old imported view. */
  engine_bytes[0x26] = 0xab;
  engine_bytes[0x27] = 0xcd;
  memcpy(&host_data, engine_bytes, sizeof(host_data));
  host_data.id = (card_id_t)907;
  host_data.ai_base_value = -1;
  memcpy(engine_bytes, &host_data, sizeof(engine_bytes));
  if (engine_bytes[0x24] != 0x8b || engine_bytes[0x25] != 3 ||
      engine_bytes[0x26] != 0 || engine_bytes[0x27] != 0 ||
      engine_bytes[0x28] != TYPE_CREATURE || engine_bytes[0x2c] != 0xff ||
      engine_bytes[0x43] != 4 || engine_bytes[0x44] != 0xff ||
      engine_bytes[0x45] != 0xff || engine_bytes[0x46] != 0xff || engine_bytes[0x47] != 0xff)
  {
    fprintf(stderr, "Engine field writes changed neighboring record bytes\n");
    return 1;
  }
  return 0;
}
