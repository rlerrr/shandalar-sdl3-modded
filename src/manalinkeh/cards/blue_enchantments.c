#include "manalink.h"

int card_illusions_of_grandeur(int player, int card, event_t event){
  /* Illusions of Grandeur	|3|U	0x2001CE0
   * Enchantment
   * Cumulative upkeep |2
   * When ~ enters the battlefield, you gain 20 life.
   * When ~ leaves the battlefield, you lose 20 life. */

    if( ! is_unlocked(player, card, event, 21) ){ return 0; }

      if( current_turn == player && upkeep_trigger(player, card, event) ){
          int kill = 1;
          add_ccounter(player, card);
          int amount = count_ccounters(player, card);
          if( has_mana( player, COLOR_COLORLESS, amount *2) ){
              int choice = do_dialog(player, player, card, -1, -1, " Pay Cumulative Upkeep\n Pass", 0);
              if( choice == 0 ){
                  charge_mana( player, COLOR_COLORLESS, amount *2);
                  if( spell_fizzled != 1 ){
                      kill = 0;
                  }
              }
          }
 
          if( kill != 0 ){
              kill_card(player, card, KILL_SACRIFICE);
              lose_life(player, 20);
          }
      }

    if( comes_into_play(player, card, event) ){
        gain_life(player, 20);
    }

    if( leaves_play(player, card, event, RESOLVE_TRIGGER_MANDATORY, 1) ){
        lose_life(player, 20);
    }

    return global_enchantment(player, card, event);
}

int card_mystic_remora(int player, int card, event_t event){
  /* Mystic Remora	|U	0x20017EA
   * Enchantment
   * Cumulative upkeep |1
   * Whenever an opponent casts a noncreature spell, you may draw a card unless that player pays |4. */

    if(event == EVENT_CAN_CAST){
        return 1;
    }
    if( ! is_unlocked(player, card, event, 3) ){ return 0; }

    cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
    if(trigger_condition == TRIGGER_SPELL_CAST && player == affected_card_controller
       && player == reason_for_trigger_controller && player != trigger_cause_controller && card == affected_card )
    {
        card_instance_t *instance = get_card_instance(trigger_cause_controller, trigger_cause);
        if( ! ( cards_data[ instance->internal_card_id].type & TYPE_CREATURE ) && ! ( cards_data[ instance->internal_card_id].type & TYPE_LAND ) ){
            if(event == EVENT_TRIGGER){
                event_result |= RESOLVE_TRIGGER_MANDATORY;
            }
            else if(event == EVENT_RESOLVE_TRIGGER){
                int choice = 0;
                if( has_mana(1-player, COLOR_COLORLESS, 4) && trigger_cause_controller == HUMAN ){
                    choice = do_dialog(1-player, player, card, -1, -1, " Let opponent draw\n Pay 4", 0);
                }
                if( choice == 1 ){
                    charge_mana_multi(1-player, 4, 0, 0, 0, 0, 0);
                    if( spell_fizzled != 1 ){
                        return 0;
                    }
                }
                draw_a_card(player);
            }
        }
    }
    return global_enchantment(player, card, event);
}

int card_battle_of_wits(int player, int card, event_t event){
  /* Battle of Wits	|3|U|U
   * Enchantment
   * At the beginning of your upkeep, if you have 200 or more cards in your library, you win the game. */

    if( event == EVENT_CAN_CAST ){
        return 1;
    }
    else if( player == current_turn && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && count_deck(player) >= 200 ){
        if(event == EVENT_TRIGGER){
            event_result |= RESOLVE_TRIGGER_MANDATORY;
        }
        else if(event == EVENT_RESOLVE_TRIGGER){
            lose_the_game(1-player);
        }
    }
    return global_enchantment(player, card, event);
}

