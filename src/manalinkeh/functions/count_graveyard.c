#include "manalink.h"

/* Canonical recovered helper declarations. */
int new_special_count_grave(int player, test_definition_t *this_test);

/* Original source signature; GCC removes the unused player argument. */
static int count_zone_by_type(int player, int type, const int *zone);

int count_rfg_by_type(int player, int type);

int count_graveyard_by_color(int player, int selected_color)
{
  unsigned int timestamp = 0;
  int changer = -1, hack = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c))
      {
        if (get_id(p, c) == CARD_ID_PAINTERS_SERVANT)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->targets[1].card >= 0 && instance->timestamp > timestamp)
          {
            hack |= instance->targets[1].card;
            changer = CARD_ID_PAINTERS_SERVANT;
            timestamp = instance->timestamp;
          }
        }
        if (p == player && get_id(player, c) == CARD_ID_CELESTIAL_DAWN)
        {
          card_instance_t *instance = get_card_instance(player, c);
          unsigned int next_timestamp = instance->timestamp;
          if (next_timestamp > timestamp)
          {
            hack = get_sleighted_color_test(player, c, COLOR_TEST_WHITE);
            changer = CARD_ID_CELESTIAL_DAWN;
            timestamp = next_timestamp;
          }
        }
        if (get_id(p, c) == CARD_ID_MYCOSYNTH_LATTICE)
        {
          card_instance_t *instance = get_card_instance(p, c);
          if (instance->timestamp > timestamp)
          {
            hack = COLOR_TEST_COLORLESS;
            changer = CARD_ID_MYCOSYNTH_LATTICE;
            timestamp = instance->timestamp;
          }
        }
      }
  const int *grave = get_grave(player);
  int result = 0;
  for (int i = 0; grave[i] != -1; ++i)
  {
    if (changer == CARD_ID_CELESTIAL_DAWN)
    {
      if (!is_what(-1, grave[i], TYPE_LAND) && (selected_color & hack))
        ++result;
    }
    else if ((changer == CARD_ID_PAINTERS_SERVANT || changer == CARD_ID_MYCOSYNTH_LATTICE) &&
             (selected_color & hack))
      return count_graveyard(player);
    // Native has no fallback that counts printed colors.
  }
  return result;
}

int count_graveyard_by_id(int player, int id){
	int i;
	int result = 0;
	for(i=0; i<2; i++){
		if( i == player || player == 2 ){
			const int *grave = get_grave(i);
			int count = 0;
			while( grave[count] != -1 ){
					if( cards_data[ grave[count] ].id == id ){
						result++;
					}
					count++;
			}
		}
	}
	return result;
}

// Count the number of cards in a zone

int count_zone(int player, const int* zone){
  int i;

  for (i = 0; i < 500; i++)
	if (zone[i] == -1)
	  return i;

  return 500;
}

int count_deck(int player){
	return count_zone(player, deck_ptr[player]);
}

int count_graveyard(int player){
	int result = 0;
	int i;
	for(i=0; i<2; i++){
		if( i == player || player == 2 ){
			result += count_zone(i, get_grave(i));
		}
	}
	return result;
}

int count_rfg(int player){
	return count_zone(player, rfg_ptr[player]);
}

void purge_rfg(int player){
	int *rfg = rfg_ptr[player];
	rfg[count_rfg(player) - 1] = -1;
}

/* ManalinkEh 2022: 0x26e5c70; original functions/count_graveyard.c:119. */
/* Native clone removes the original source player parameter. */
static int count_zone_by_type(int player, int type, const int *zone)
{
  const int *zone_end;
  int type_match;
  int matches;

  zone_end = zone + 500;
  matches = 0;
  do
  {
    if (*zone == -1)
    {
      return matches;
    }
    type_match = is_what(-1, *zone, type);
    matches = (matches + 1) - (unsigned int)(type_match == 0);
    zone = zone + 1;
  } while (zone != zone_end);
  return matches;
}

int count_graveyard_by_type(int player, int type){
	int i;
	int result = 0;
	for(i=0; i<2; i++){
		if( i == player || player == 2 ){
			result += count_zone_by_type(i, type, get_grave(i));
		}
	}
	return result;
}

/* ManalinkEh 2022: 0x26e60e0; original functions/count_graveyard.c:143. */
int count_rfg_by_type(int player, int type)
{
  int second_count;
  int first_count;

  first_count = 0;
  if ((player & 0xfffffffdU) == 0)
  {
    first_count = count_zone_by_type(0, type, rfg_ptr[player]);
  }
  if (player - 1U < 2)
  {
    second_count = count_zone_by_type(1, type, rfg_ptr[player]);
    return first_count + second_count;
  }
  return first_count;
}

// Equivalent to count_graveyard_by_type(player, type) > 0, but stops when it finds the first qualifying card.

int any_in_graveyard_by_type(int player, int type)
{
  int p, i;
  for (p = 0; p <= 1; ++p)
	if (p == player || player == ANYBODY)
	  for (i = 0; i < 500 && get_grave(p)[i] != -1; ++i)
		if (is_what(-1, get_grave(p)[i], type))
		  return 1;
  return 0;
}

int count_graveyard_by_subtype(int player, subtype_t subtype)
{
  int count = 0;
  for (int p = 0; p < 2; ++p)
    if (player == p || player == 2)
      for (int i = 0; i < 500; ++i)
      {
        int iid = graveyard_ptr_mutable[p][i];
        if (iid == -1)
          break;
        count += has_subtype_by_id(player, (unsigned int)cards_data[iid].id, subtype) != 0;
      }
  return count;
}

int any_in_graveyard_by_subtype(int player, subtype_t subtype)
{
  int grave_owner;
  int grave_byte_offset;

  grave_owner = 0;
  do
  {
    if ((player == grave_owner) || (player == 2))
    {
      grave_byte_offset = 0;
      do
      {
        if (*(int *)((int)graveyard_ptr_mutable[grave_owner] + grave_byte_offset) == -1)
          break;
        if (has_subtype_by_id(
                player,
                (unsigned int)cards_data[*(int *)((int)graveyard_ptr_mutable[grave_owner] +
                                                  grave_byte_offset)]
                    .id,
                subtype) != 0)
        {
          return 1;
        }
        grave_byte_offset = grave_byte_offset + 4;
      } while (grave_byte_offset != 2000);
    }
    grave_owner = grave_owner + 1;
    if (grave_owner == 2)
    {
      return 0;
    }
  } while (true);
}

int count_deck_by_type(int player, int type){
	return count_zone_by_type(player, type, deck_ptr[player]);
}

int new_special_count_grave(int player, test_definition_t *this_test)
{
  int test_score = new_get_test_score(this_test);
  int matches = 0;
  for (int p = 0; p < 2; ++p)
    if (player == p || player == ANYBODY)
    {
      int *grave = graveyard_ptr_mutable[p];
      int iid = *grave;
      while (iid != -1)
      {
        ++grave;
        if (new_make_test(player, iid, test_score, this_test))
          ++matches;
        iid = *grave;
      }
    }
  return matches;
}

/* ManalinkEh 2022: 0x26e6370; original functions/count_graveyard.c:211. */
int special_count_zone(int player, int *zone, test_definition_t *this_test)
{
  int *zone_end;
  int test_type;
  int test_match;
  int matches;

  matches = 0;
  test_type = new_get_test_score(this_test);
  zone_end = zone + 500;
  do
  {
    if (*zone == -1)
    {
      return matches;
    }
    test_match = new_make_test(player, *zone, test_type, this_test);
    matches = (matches + 1) - (unsigned int)(test_match == 0);
    zone = zone + 1;
  } while (zone != zone_end);
  return matches;
}

int special_count_grave(int player, int type, int flag1, int subtype, int flag2, int color, int flag3, int id, int flag4, int cc, int flag5){

	int count = count_graveyard(player)-1;
	int result = 0;
	const int *grave = get_grave(player);
	int test_type = get_test_score(type, flag1, subtype, flag2, color, flag3, id, flag4, cc, flag5);
	while( count > - 1 ){
			if( make_test(player, grave[count], test_type, type, flag1, subtype, flag2, color, flag3, id,
						  flag4, cc, flag5)
			  ){
				result++;
			}
			count--;
	}
	return result;
}
