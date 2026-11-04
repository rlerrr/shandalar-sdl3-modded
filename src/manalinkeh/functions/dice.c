#include "manalink.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
  RXDY_NONE = 0,
  RXDY_SHOW_TOTAL = 1,
  RXDY_SHOW_DIFFERENCE = 2
} RollXDY;
_Static_assert(sizeof(RollXDY) == 4, "dice selector width");

/* ManalinkEh 2022: 0x2650020; original functions/dice.c:13. */
static int roll_xdy(int player, int src_player, int src_card, int tgt_player, int tgt_card, int x, int y,
             int *natural_result, int plus, int *first2, RollXDY show_math)
{
  char buf[1500];
  char *prompt = buf;
  int pos = 0;
  if (x < 1)
    backtrace_and_abort("functions/dice.c:19\nx >= 1 && y >= 1");
  if (ai_is_speculating != 0)
    prompt = "r";
  int adj_num_to_roll = (int)((unsigned int)x + (unsigned int)roll_extra_dice[player]);
  if (adj_num_to_roll < 1)
    backtrace_and_abort("functions/dice.c:26\nadj_num_to_roll >= 1");
  int rolls[adj_num_to_roll];
  if (adj_num_to_roll == 1)
  {
    int result = network_rand(player, y) + 1;
    rolls[0] = result;
    if (ai_is_speculating != 1)
    {
      const char *suffix;
      const char *subject;
      if (player == 0)
      {
        suffix = "";
        subject = "You";
      }
      else
      {
        suffix = "s";
        subject = opponent_name;
      }
      pos = sprintf(buf, "%s roll%s a d%d...\n%d", subject, suffix, y, result);
    }
  }
  else
  {
    if (ai_is_speculating != 1)
    {
      const char *suffix;
      const char *subject;
      if (player == 0)
      {
        suffix = "";
        subject = "You";
      }
      else
      {
        suffix = "s";
        subject = opponent_name;
      }
      pos = sprintf(buf, "%s roll%s %dd%d...\n", subject, suffix, adj_num_to_roll, y);
    }
    for (int i = 0; i < adj_num_to_roll; ++i)
    {
      rolls[i] = network_rand(player, y) + 1;
      if (ai_is_speculating != 1)
      {
        const char *separator;
        if (i == 0)
          separator = "";
        else
          separator = " ";
        pos += sprintf(buf + pos, "%s%d", separator, rolls[i]);
      }
    }
    if (roll_extra_dice[player] > 0)
    {
      descending_qsort(adj_num_to_roll, rolls);
      /* These two headings are formatted even during AI speculation. */
      if (x == 1)
        pos += sprintf(buf + pos, "\nignoring all but the highest...\n%d", rolls[0]);
      else
      {
        pos += sprintf(buf + pos, "\nignoring all but the highest %d...\n", x);
        if (ai_is_speculating != 1 && x > 0)
        {
          const char *separator = "";
          pos += sprintf(buf + pos, "%s%d", separator, rolls[0]);
          separator = " ";
          for (int i = 1; i < x; ++i)
            pos += sprintf(buf + pos, "%s%d", separator, rolls[i]);
        }
      }
    }
  }
  if (natural_result != NULL)
    *natural_result = rolls[0];
  int adjustment = (int)((unsigned int)plus + (unsigned int)add_to_die_rolls[player]);
  if (adjustment != 0)
  {
    if (ai_is_speculating != 1)
    {
      const char *each = " to each";
      if (x < 2)
        each = "";
      const char *verb;
      const char *suffix;
      const char *subject;
      if (player == 0)
      {
        if (adjustment < 1)
        {
          verb = "subtract";
          suffix = "";
          subject = "You";
        }
        else
        {
          verb = "add";
          suffix = "";
          subject = "You";
        }
      }
      else if (adjustment < 1)
      {
        verb = "subtract";
        suffix = "s";
        subject = opponent_name;
      }
      else
      {
        verb = "add";
        suffix = "s";
        subject = opponent_name;
      }
      unsigned int magnitude =
          adjustment < 0 ? 0U - (unsigned int)adjustment : (unsigned int)adjustment;
      pos += sprintf(buf + pos, "\n%s %s%s %d%s...\n", subject, verb, suffix, (int)magnitude, each);
    }
    for (int i = 0; i < x; ++i)
    {
      rolls[i] = (int)((unsigned int)rolls[i] + (unsigned int)adjustment);
      if (ai_is_speculating != 1)
      {
        const char *separator;
        if (i == 0)
          separator = "";
        else
          separator = " ";
        pos += sprintf(buf + pos, "%s%d", separator, rolls[i]);
      }
    }
  }
  unsigned int total = 0;
  for (int i = 0; i < x; ++i)
    total += (unsigned int)rolls[i];
  if (x >= 2 && ai_is_speculating != 1)
  {
    if (show_math == RXDY_SHOW_TOTAL)
      pos += sprintf(buf + pos, "\ntotalling %d.", (int)total);
    else if (show_math == RXDY_SHOW_DIFFERENCE)
    {
      int difference = (int)((unsigned int)rolls[0] - (unsigned int)rolls[1]);
      unsigned int magnitude =
          difference < 0 ? 0U - (unsigned int)difference : (unsigned int)difference;
      pos += sprintf(buf + pos, "\nThe difference is %d.", (int)magnitude);
    }
  }
  if (ai_is_speculating != 1)
  {
    play_sound_effect(0x58);
    buf[pos] = '\n';
    buf[pos + 1] = '\0';
    raw_do_dialog(src_player, src_card, tgt_player,
                                                                 tgt_card, prompt, player == 0);
  }
  if (first2 != NULL)
  {
    first2[0] = rolls[0];
    first2[1] = x > 1 ? rolls[1] : 0;
  }
  return (int)total;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int roll_d6t(int player, int src_player, int src_card, int tgt_player, int tgt_card);
void roll_2d6t(int player, int src_player, int src_card, int tgt_player, int tgt_card, int *r1,
               int *r2);
int roll_xd6t(int player, int x, int src_player, int src_card, int tgt_player, int tgt_card);

/* ManalinkEh 2022: 0x2650690; original functions/dice.c:154. */
int roll_d4t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result =
      roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 4, NULL, 0, NULL, RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x26506f0; original functions/dice.c:160. */
int roll_d6t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result =
      roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 6, NULL, 0, NULL, RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x2650750; original functions/dice.c:166. */
int roll_d8t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result =
      roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 8, NULL, 0, NULL, RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x26507b0; original functions/dice.c:172. */
int roll_d10t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result =
      roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 10, NULL, 0, NULL, RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x2650810; original functions/dice.c:178. */
int roll_d12t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result = roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 0xc, NULL, 0, NULL,
                    RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x2650870; original functions/dice.c:184. */
int roll_d20t(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result = roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 0x14, NULL, 0, NULL,
                    RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x26508d0; original functions/dice.c:190. */
int roll_d20t_plus(int player, int plus, int src_player, int src_card, int tgt_player, int tgt_card)
{
  int result;

  result = roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 0x14, NULL, plus, NULL,
                    RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x2650930; original functions/dice.c:196. */
int roll_d20t_nat(int player, int src_player, int src_card, int tgt_player, int tgt_card,
                  int *natural_result)
{
  int result;

  result = roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 1, 0x14, natural_result, 0,
                    NULL, RXDY_NONE);
  return result;
}

/* ManalinkEh 2022: 0x2650990; original functions/dice.c:202. */
void roll_2d6t(int player, int src_player, int src_card, int tgt_player, int tgt_card, int *r1,
               int *r2)
{
  int first2[2];

  roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 2, 6, NULL, 0, first2, RXDY_NONE);
  *r1 = first2[0];
  *r2 = first2[1];
  return;
}

/* ManalinkEh 2022: 0x2650a00; original functions/dice.c:211. */
int roll_2d6t_difference(int player, int src_player, int src_card, int tgt_player, int tgt_card)
{
  unsigned int difference_sign_bit;
  int first2[2];

  roll_xdy(player, src_player, src_card, tgt_player, tgt_card, 2, 6, NULL, 0, first2,
           RXDY_SHOW_DIFFERENCE);
  difference_sign_bit = first2[0] - first2[1] >> 0x1f;
  return (first2[0] - first2[1] ^ difference_sign_bit) - difference_sign_bit;
}

/* ManalinkEh 2022: 0x2650a70; original functions/dice.c:219. */
int roll_xd6t(int player, int x, int src_player, int src_card, int tgt_player, int tgt_card)
{
  bool single_die;

  if (x < 1)
  {
    return 0;
  }
  single_die = x == 1;
  if (single_die)
  {
    x = 1;
  }
  return roll_xdy(player, src_player, src_card, tgt_player, tgt_card, x, 6, NULL, 0, NULL,
                  (unsigned int)!single_die);
}
