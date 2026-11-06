// If any of these are implemented, they need to be put in
// functions/rules_engine.c:check_deck_legality()::banned_cards_vintage[] and banned_cards_legacy[], and in
// cards/vanguard.c:is_banned_from_edh().

#include "manalink.h"

/* Native shared card-name data and text-effect sentinel. */
const unsigned char lc_unaccent_map[256] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
  0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
  0x80, 0x81, 0x2c, 0x66, 0x84, 0x85, 0x86, 0x87, 0x5e, 0x25, 0x73, 0x3c, 0x8c, 0x8d, 0x7a, 0x8f,
  0x90, 0x27, 0x27, 0x22, 0x22, 0x95, 0x2d, 0x2d, 0x7e, 0x99, 0x73, 0x3e, 0x9c, 0x9d, 0x7a, 0x79,
  0x5f, 0x21, 0x24, 0x24, 0x24, 0x24, 0x7c, 0x25, 0x22, 0x40, 0x5e, 0x3c, 0x7e, 0x2d, 0x40, 0x7e,
  0x5e, 0x2b, 0x32, 0x33, 0x27, 0x75, 0x26, 0x2e, 0x2c, 0x31, 0x5e, 0x3e, 0x31, 0x32, 0x33, 0x3f,
  0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0xc6, 0x63, 0x65, 0x65, 0x65, 0x65, 0x69, 0x69, 0x69, 0x69,
  0x64, 0x6e, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x2a, 0x6f, 0x75, 0x75, 0x75, 0x75, 0x79, 0xde, 0xdf,
  0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0xe6, 0x63, 0x65, 0x65, 0x65, 0x65, 0x69, 0x69, 0x69, 0x69,
  0x64, 0x6e, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x2f, 0x6f, 0x75, 0x75, 0x75, 0x75, 0x79, 0xfe, 0x79,
};

const int txtfx_not_found = 0x7fffffff;

/***********
 * Unglued *
 ***********/

/* Charm School	|2|W
 * Enchantment
 * As ~ enters the battlefield, choose a color and balance ~ on your head.
 * Prevent all damage that would be dealt to you by sources of the chosen color.
 * When ~ falls off your head, sacrifice ~. */

/* Double Dip	|4|W
 * Instant
 * Choose another player. You gain 5 life. At the beginning of the first upkeep in your next game with that player, you gain 5 life. */

/* Get a Life	|W
 * Instant
 * Target player and each of that player's teammates exchange life totals. */

/* I'm Rubber, You're Glue	|W|W
 * Enchantment
 * When you speak in non-rhyming sentences, sacrifice ~.
 * Say "I'm rubber, you're glue. Everything bounces off me and sticks to you": Change the target of target spell or ability that targets only you. */

/* Knight of the Hokey Pokey	|W|W
 * Creature - Knight 2/2
 * First strike
 * |1|W, Do the Hokey Pokey: Prevent all damage a source of your choice would deal to ~ this turn. (To do the Hokey Pokey, put your right foot in. Put your right foot out. Put your right foot in. Shake it all about. Repeat this process for your left foot, right arm, left arm, and whole self.) */

/* Lexivore	|3|W
 * Creature - Beast 2/3
 * Whenever ~ deals damage to a player, destroy target permanent other than ~ with the most lines of text in its text box. (If two or more cards are tied, target any one of them.) */

/* Look at Me, I'm the DCI	|5|W|W
 * Sorcery
 * Ban a card other than a basic land card for the rest of the match. (All cards with that name in any zone or sideboard are removed from the match.) */

/* Mesa Chicken	|W|W
 * Creature - Chicken 2/2
 * Stand up, Flap your arms, Cluck like a chicken: ~ gains flying until end of turn. */

/* Miss Demeanor	|3|W
 * Creature - Lady of Proper Etiquette 3/1
 * Flying, first strike
 * At the beginning of each other player's upkeep, you may compliment that player on their game play. If you don't, sacrifice ~. */

/* Prismatic Wardrobe	|W
 * Sorcery
 * Destroy target nonartifact, nonland permanent that doesn't share a color with clothing worn by its controller. */

/* Sex Appeal	|W
 * Instant
 * Prevent the next 3 damage that would be dealt this turn to any number of targets, divided as you choose. If there are more people in the room of a different gender than you are as you cast this spell, prevent the next 6 damage this way instead. */

/* Bureaucracy	|3|U|U
 * Enchantment
 * Pursuant to subsection 3.1(4) of Richard's Rules of Order, at the beginning of the upkeep of each participant in this game of the Magic: The Gathering(R) trading card game (hereafter known as "PLAYER"), that PLAYER performs all actions in the sequence of previously added actions (hereafter known as "ACTION QUEUE"), in the order those actions were added, then adds another action to the end of the ACTION QUEUE. All actions must be simple physical or verbal actions that a player can perform while sitting in a chair, without jeopardizing the health and security of said PLAYER.
 * When any PLAYER does not perform all the prescribed actions in the correct order, sacrifice ~ and said PLAYER discards their complement of cards in hand (hereafter known as "HAND"). */

/* Censorship	|U
 * Enchantment
 * As ~ enters the battlefield, choose a word.
 * Whenever a player says the chosen word, ~ deals 2 damage to that player. */

/* Checks and Balances	|2|U
 * Enchantment
 * Cast this spell only if there are three or more players in the game.
 * Whenever a player casts a spell, each of that player's opponents may discard a card. If they do, counter that spell. */

/* Chicken a la King	|1|U|U
 * Creature - Chicken 2/2
 * Whenever a 6 is rolled on a six-sided die, put a +1/+1 counter on each Chicken. (You may roll dice only when instructed to.)
 * Tap an untapped Chicken you control: Roll a six-sided die. */

/* Clambassadors	|3|U
 * Creature - Clamfolk 4/4
 * Whenever ~ deals damage to a player, choose an artifact, creature, or land you control. That player gains control of that permanent. */

/* Clam-I-Am	|2|U
 * Creature - Clamfolk 2/2
 * If you roll a 3 on a six-sided die, you may reroll that die. */

/* Clam Session	|1|U|U
 * Creature - Clamfolk 2/5
 * As ~ enters the battlefield, choose a word.
 * At the beginning of your upkeep, you may sing at least six words of a song, at least one of which is the chosen word. If you don't, sacrifice ~. You can't repeat a song. */

/* Common Courtesy	|2|U|U
 * Enchantment
 * Whenever a player casts a spell without asking your permission while casting it, counter that spell.
 * When a player asks you permission to cast a spell and you refuse, counter that spell and sacrifice ~. */

/* Denied!	|U
 * Instant
 * Choose a card name, then target spell's controller reveals their hand. If a card with the chosen name is revealed this way, counter that spell. */

/* Double Take	|3|U|U
 * Instant
 * Choose another player. You draw two cards. At the beginning of the first upkeep in your next game with that player, you draw two cards. */

/* Free-for-All	|3|U
 * Enchantment
 * When ~ enters the battlefield, exile all creatures face down.
 * At the beginning of each player's upkeep, that player chooses a card exiled with ~ at random and puts it onto the battlefield.
 * When ~ leaves the battlefield, put all cards exiled with it into their owners' graveyards. */

/* Psychic Network	|U
 * Enchantment
 * Each player plays with the top card of their library held against their forehead, revealed to each other player. */

/* Sorry	|U|U
 * Enchantment
 * Before any player casts a spell with the same name as a card in any graveyard, that player may say "sorry." If they don't, any other player may say "sorry" as the spell is being cast. When another player says "sorry" this way, counter the spell.
 * Whenever a player says "sorry" at any other time, ~ deals 2 damage to that player. */

/* B.F.M. (Big Furry Monster)	|B|B|B|B|B|B|B|B|B|B|B|B|B|B|B
 * Creature - The Biggest, Baddest, Nastiest,
 * You must cast both B.F.M. cards to put
 * leaves the battlefield, sacrifice the other.
 * B.F.M. can't be blocked except by three or
 * //
 * Scariest Creature You'll Ever See 99/99
 * B.F.M. onto the battlefield. If one B.F.M. card
 * more creatures. */

/* Deadhead	|3|B
 * Creature - Zombie 3/3
 * |0: Return ~ from your graveyard to the battlefield. Activate this ability only if an opponent isn't touching their hand (of cards). */

/* Double Cross	|3|B|B
 * Sorcery
 * Choose another player. Look at that player's hand and choose a card other than a basic land card from it. They discard that card. At the beginning of the first upkeep in your next game with that player, look at that player's hand and choose a card other than a basic land card from it. They discard that card. */

/* Handcuffs	|3|B|B
 * Enchantment
 * As ~ enters the battlefield, choose a player.
 * When the chosen player's hands aren't touching each other, sacrifice ~ and that player sacrifices three permanents. */

/* Infernal Spawn of Evil	|6|B|B|B
 * Creature - Beast 7/7
 * Flying, first strike
 * |1|B, Reveal ~ from your hand, Say "It's coming!": ~ deals 1 damage to target opponent or planeswalker. Activate this ability only during your upkeep and only once each turn. */

/* Ow	|B
 * Enchantment
 * Whenever a creature deals damage to a player, that player may say "~" once for each permanent named ~. If the player doesn't, ~ deals 1 damage to them. */

/* Volrath's Motion Sensor	|B
 * Enchantment - Aura
 * Enchant player
 * As ~ enters the battlefield, choose one of enchanted player's hands. That player balances ~ on the back of that hand.
 * When ~ falls off the chosen hand, sacrifice ~ and enchanted player loses 3 life. */

/* Burning Cinder Fury of Crimson Chaos Fire	|3|R
 * Enchantment
 * Whenever any player taps a permanent, that player choose one of their opponents. The chosen player gains control of that permanent at the beginning of the next end step.
 * At the beginning of each player's end step, if that player didn't tap any nonland permanents that turn, ~ deals 3 damage to that player. */

/* Giant Chicken	""(token)
 * Creature - Giant Chicken 4/4 */

/* Double Deal	|4|R
 * Sorcery
 * Choose another player. ~ deals 3 damage to that player. At the beginning of the first upkeep in your next game with that player, ~ deals 3 damage to the player. */

/* Goblin Bookie	|R
 * Creature - Goblin 1/1
 * |R, |T: Reflip any coin or reroll any die. (Activate this ability only any time it makes sense.) */

/* Goblin Bowling Team	|3|R
 * Creature - Goblin 1/1
 * If ~ would deal damage to a permanent or player, it deals that much damage plus the result of a six-sided die roll to that permanent or player instead. */

/* ManalinkEh 2022: 0x23030f0; original cards/unglued_unhinged.c:191. */
int has_reminder_text(int player, int card)
{
  int result;
  const char *reminder_start;

  result = get_id(player, card);
  if (cards_ptr[result]->rules_text != NULL)
  {
    reminder_start = strchr(cards_ptr[result]->rules_text, 0x28);
    return (unsigned int)(reminder_start != NULL);
  }
  return 0;
}

/* Hurloon Wrangler	|2|R
 * Creature - Minotaur 2/2
 * Denimwalk (This creature can't be blocked as long as defending player is wearing denim.) */

/* Jalum Grifter	|3|R|R
 * Legendary Creature - Devil 3/5
 * |1|R, |T: Shuffle ~ and two lands you control face down. Target opponent chooses one of those cards. Turn the cards face up. If they chose ~, sacrifice it. Otherwise, destroy target permanent. */

/* Landfill	|4|R
 * Sorcery
 * Choose a land type. Exile all lands you control of that type. Drop those cards, one at a time, onto the playing area from a height of at least one foot. Destroy each permanent completely covered by those cards. Then return the cards you dropped to the battlefield tapped. */

/* Ricochet	|R
 * Enchantment
 * Whenever a player casts a spell that targets a single player, each player rolls a six-sided die. Change the target of that spell to the player with the lowest result. Reroll to break ties, if necessary. */

/* The Ultimate Nightmare of WOTC	|X|R|R
 * Sorcery
 * The Ultimate Nightmare of Wizards of the Coast(R) Customer Service deals X damage to each of Y target creatures and Z target players or planeswalkers. */

/* Cardboard Carapace	|5|G
 * Enchantment - Aura
 * Enchant creature
 * For each other card named ~ you have with you outside the game, enchanted creature gets +1/+1. */

/* Double Play	|3|G|G
 * Sorcery
 * Choose another player. Search your library for a basic land card, put it onto the battlefield, then shuffle your library. At the beginning of the first upkeep in your next game with that player, search your library for a basic land card, put it onto the battlefield, then shuffle your library. */

// No "Rabid Sheep" token; I just used CARD_ID_SHEEP.  We can make a named token if someone complains.

/* Free-Range Chicken	|3|G
 * Creature - Chicken 3/3
 * |1|G: Roll two six-sided dice. If both results are the same, ~ gets +X/+X until end of turn, where X is that result. If the total of those results is equal to any other total you have rolled this turn for ~, sacrifice it. (For example, if you roll two 3s, ~ gets +3/+3. If you roll a total of 6 for ~ later that turn, sacrifice it.) */

/* Gerrymandering	|2|G
 * Sorcery
 * Exile all lands. Give each player a number of those cards chosen at random equal to the number of those cards the player controlled. Each player returns those cards to the battlefield under their control. */

/* Ghazban Ogress	|G
 * Creature - Ogre 2/2
 * When ~ enters the battlefield, if one player has won more Magic games that day than any other player, that player gains control of ~. */

/* Gus	|2|G
 * Creature - Gus 2/2
 * ~ enters the battlefield with a +1/+1 counter on it for each Magic game you have lost to one of your opponents since you last won a game against them. */

/* Incoming!	|4|G|G|G|G
 * Sorcery
 * Each player searches their library for any number of artifact, creature, enchantment, and/or land cards, puts them onto the battlefield, then shuffles their library. */

/* Mine, Mine, Mine!	|4|G|G
 * Enchantment
 * When Mine, Mine, Mine! enters the battlefield, each player puts their library into their hand.
 * Players have no maximum hand size and don't lose the game for drawing from an empty library.
 * Each player can't cast more than one spell each turn.
 * When Mine, Mine, Mine! leaves the battlefield, each player shuffles their hand and graveyard into their library. */

/* Squirrel Farm	|2|G
 * Enchantment
 * |1|G: Reveal a card in your hand, covering the artist credit. Target opponent guesses the artist. If they guess wrong, create a 1/1 |Sgreen Squirrel creature token. */

/* Ashnod's Coupon	|0
 * Artifact
 * |T, Sacrifice ~: Target player gets you target drink. You pay any costs for the drink. */

/* Blacker Lotus	|0
 * Artifact
 * |T: Tear ~ into pieces. Add four mana of any one color. Remove the pieces from the game. */

/* Bronze Calendar	|4
 * Artifact
 * Spells you cast cost |1 less to cast.
 * You must speak in a voice other than your normal voice.
 * When you speak in your normal voice, sacrifice ~. */

/* Chaos Confetti	|4
 * Artifact
 * |4, |T: Tear ~ into pieces. Throw the pieces onto the battlefield from a distance of at least five feet. Destroy each permanent that a piece touches. Remove the pieces from the game. */

/* Clay Pigeon	|3
 * Artifact Creature - Bird 1/1
 * Flying
 * |1, Throw ~ into the air at least two feet above your head while seated: If you catch ~ with one hand, prevent all damage a source of your choice would deal to you this turn and tap ~. Otherwise, sacrifice it. */

/* Giant Fan	|4
 * Artifact
 * |2, |T: Move a counter from one permanent onto another. If the second permanent refers to any kind of counter, the moved counter becomes one of those counters. Otherwise, it becomes a +1/+1 counter. */

/* ManalinkEh 2022: 0x2302b40; original cards/unglued_unhinged.c:197. */
static int roll_d6_shim(int player, int card)
{
  int result;

  result = roll_d6t(player, player, card, -1, -1);
  return result;
}

/* Jester's Sombrero	|2
 * Artifact
 * |2, |T, Sacrifice ~: Look at target player's sideboard, choose three cards from it, and remove those cards from the match. */

/* Mirror Mirror	|7
 * Artifact
 * ~ enters the battlefield tapped.
 * |7, |T, Sacrifice ~: Choose target player. At the beginning of the next end step, exchange life totals with that player, exchange control of all permanents you and that player control, and exchange cards in your hands, cards in your libraries, and cards in your graveyards. */

/* Urza's Contact Lenses	|0
 * Artifact
 * ~ enters the battlefield tapped.
 * ~ doesn't untap during your untap step.
 * As long as ~ is untapped, all players play with their hands revealed.
 * Clap your hands twice: You may tap or untap ~. */

// Plains, Island, Swamp, Mountain, Forest.

/************
 * Unhinged *
 ************/

/* Atinlay Igpay	|5|W
 * Eaturecray - Igpay
 * Oubleday ikestray
 * Enwhay ouyay eakspay ay onnay-igpay-atinlay ordway, acrificesay ~. */

/* Bosom Buddy	|3|W
 * Creature - Elephant Townsfolk 1/4
 * Whenever you cast a spell, you may gain 1/2 life for each word in that spell's name. */

/* Cardpecker	|1|W
 * Creature - Bird 301/1
 * Flying
 * Gotcha - If an opponent touches the table with their hand, you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Cheap Ass	|1|W
 * Creature - Donkey Townsfolk 1/303
 * Spells you cast cost {1/2} less to cast. */

/* Circle of Protection: Art	|1|W
 * Enchantment
 * As ~ enters the battlefield, choose an artist.
 * |1|W: The next time a source of your choice with art by the chosen artist would deal damage to you this turn, prevent that damage.
 * |1|W: Return ~ to its owner's hand. */

/* Collector Protector	|3|W|W
 * Creature - Human Gamer 2/5
 * |W, Give an opponent a nonland card you own from outside the game: Prevent the next 1 damage that would be dealt to you or ~ this turn. */

/* Drawn Together	|2|W|W
 * Enchantment
 * As ~ enters the battlefield, choose an artist.
 * Creatures with art by the chosen artist get +2/+2. */

/* Emcee	|2|W
 * Creature - Human Rogue 0/1
 * Whenever another creature enters the battlefield, you may stand up and say in a deep, booming voice, "Presenting . . . " and that creature's name. If you do, put a +1/+1 counter on that creature. */

/* Erase (Not the Urza's Legacy One)	|2|W
 * Instant
 * If you control two or more |Swhite permanents that share an artist, you may cast Erase (Not the Urza's Legacy One) without paying its mana cost.
 * Exile target enchantment. */

/* Fascist Art Director	|1|W|W
 * Creature - Human Horror 2/2
 * |W|W: ~ gains protection from the artist of your choice until end of turn. */

/* First Come, First Served	|1|W
 * Enchantment
 * Each attacking or blocking creature with the lowest collector number among attacking or blocking creatures has first strike. */

/* Frankie Peanuts	|2|W|W
 * Legendary Creature - Elephant Rogue 2/3
 * At the beginning of your upkeep, you may ask target player a yes-or-no question. If you do, that player answers the question truthfully and abides by that answer if able until end of turn. */

/* Head to Head	|W
 * Instant
 * You and target opponent play Seven Questions with the top card of that player's library. If you win, prevent all damage that would be dealt this turn by a source of your choice. (To play Seven Questions, that player looks at the card, then you ask up to six yes-or-no questions about the card that they answer truthfully. You guess the card's name-that's question seven-then the player reveals the card.) */

/* Ladies' Knight	|3|W
 * Creature - Human Knight 2/2
 * Flying
 * Spells cast by players wearing at least one item of women's clothing cost |1 less to cast. (You can define women's clothing as you see fit. Be reasonable.) */

/* Little Girl	{HW}
 * Creature - Human Child 300/300 */

/* Look at Me, I'm R&D	|2|W
 * Enchantment
 * As ~ enters the battlefield, choose a number and a second number one higher or one lower than the first number.
 * All instances of the first chosen number in the full text of each permanent, spell, and card in any zone are changed to the second chosen number. */

/* Man of Measure	|1|W|W
 * Creature - Human Knight 2/2
 * As long as you're shorter than an opponent, ~ gets +0/+1 and has first strike.
 * As long as you're taller than an opponent, ~ gets +1/+0. */

/* Save Life	|W
 * Instant
 * Choose one -
 * * Target player gains 21/2 life.
 * * Prevent the next 21/2 damage that would be dealt to target creature this turn.
 * Gotcha - If an opponent says "save" or "life," you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Standing Army	|2|W|W
 * Creature - Human Soldier 2/4
 * As long as you're standing, ~ has vigilance. */

/* Staying Power	|2|W
 * Enchantment
 * "Until end of turn" and "this turn" effects don't end. */

/* _____	|1|U
 * Creature - Shapeshifter 1/1
 * |1: This card's name becomes the card name of your choice. Activate this ability anywhere, anytime. */

/* Ambiguity	|2|U|U
 * Enchantment
 * Whenever a player plays a spell that counters a spell that has been played or a player plays a spell that comes into play with counters, that player may counter the next spell played or put an additional counter on a permanent that has already been played, but not countered. */

/* Artful Looter	|2|U
 * Creature - Human Wizard 1/2
 * |T: Draw a card, then discard a card.
 * Whenever a permanent that shares an artist with another permanent you control enters the battlefield, untap ~. */

/* Avatar of Me	|2|U|U
 * Creature - Avatar 100/100
 * This spell costs |1 more to cast for each ten years you've been alive.
 * ~'s power is equal to your height in feet and its toughness is equal to your American shoe size. Round to the nearest 1/2.
 * ~ is the color of your eyes. */

/* Brushstroke Paintermage	|3|U
 * Creature - Human Wizard 2/3
 * |T: Target permanent's artist becomes the artist of your choice until end of turn. */

/* Bursting Beebles	|2|U
 * Creature - Beeble 2/2
 * ~ can't be blocked as long as defending player controls two or more nonland permanents that share an artist. */

/* Carnivorous Death-Parrot	|1|U
 * Creature - Bird 2/2
 * Flying
 * At the beginning of your upkeep, sacrifice ~ unless you say its flavor text. */

/* Cheatyface	|U|U|U
 * Creature - Efreet 2/2
 * If ~ is in your hand, you may sneak ~ onto the battlefield. If an opponent catches you right away, that player may exile ~.
 * Flying */

/* Flaccify	|2|U
 * Instant
 * Counter target spell unless its controller pays |3{1/2}. */

/* Framed!	|1|U
 * Instant
 * Choose an artist. Tap all untapped permanents with art by that artist or untap all tapped permanents with art by that artist. */

/* Greater Morphling	|6|U|U
 * Creature - Shapeshifter 5/5
 * |2: ~ gains your choice of banding, bushido 1, double strike, fear, flying, first strike, haste, landwalk of your choice, protection from a color of your choice, provoke, rampage 1, shadow, or trample until end of turn.
 * |2: ~ becomes the colors of your choice until end of turn.
 * |2: ~ becomes the creature type of your choice until end of turn.
 * |2: ~'s expansion symbol becomes the symbol of your choice until end of turn.
 * |2: ~'s art becomes by the artist of your choice until end of turn.
 * |2: ~ gets +2/-2 or -2/+2 until end of turn.
 * |2: Untap ~. */

/* Loose Lips	|U
 * Enchantment - Aura
 * Enchant creature
 * As ~ enters the battlefield, choose a sentence with eight or fewer words.
 * Enchanted creature has flying.
 * Whenever enchanted creature deals damage to an opponent, that player may say the chosen sentence. If they don't, you draw two cards. */

/* Magical Hacker	|1|U
 * Creature - Human Gamer 1/2
 * |U: Change the text of target spell or permanent by replacing all instances of + with -, and vice versa, until end of turn. */

/* Moniker Mage	|2|U
 * Creature - Human Wizard 2/2
 * |U, Say your middle name: ~ gains shroud until end of turn.
 * |U, Say an opponent's middle name: ~ gains flying until end of turn. */

/* Mouth to Mouth	|3|U
 * Sorcery
 * Choose target opponent and target creature they control. You and that player have a breath-holding contest. If you win, you gain control of that creature. */

/* Number Crunch	|2|U
 * Instant
 * Return target permanent to its owner's hand.
 * Gotcha - If an opponent says a number, you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Question Elemental?	|2|U|U
 * Creature - Elemental? 3/4
 * Did you know this creature has flying?
 * Are you aware that any time you say something that isn't a question, when a player points out this fact first, they gain control of Question Elemental? */

/* Richard Garfield, Ph.D.	|3|U|U
 * Legendary Creature - Human Designer 2/2
 * You may play cards as though they were other cards of your choice with the same mana cost. You can't choose the same card twice. (Mana cost includes color.) */

/* Smart Ass	|2|U
 * Creature - Donkey Wizard 302/1
 * Whenever ~ attacks, choose a card name. If defending player has no cards with the chosen name in their hand, they may reveal their hand. If they don't reveal their hand, ~ can't be blocked this turn. */

/* Spell Counter	|2|U|U
 * Instant
 * Counter target spell.
 * Gotcha - If an opponent says "spell" or "counter," you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Topsy Turvy	|2|U
 * Enchantment
 * The phases of each player's turn are reversed. (The phases are, in reverse order, ending, postcombat main, combat, precombat main, and beginning.)
 * As long as there are more than two players in the game, the turn order is reversed. */

/* Aesthetic Consultation	|B
 * Instant
 * Choose an artist. Exile the top six cards of your library, then reveal cards from the top of your library until you reveal a card with art by the chosen artist. Put that card in your hand and exile all other cards revealed this way. */

/* Bad Ass	|2|B|B
 * Creature - Donkey Zombie 303/1
 * |1|B, Growl: Regenerate ~. */

/* ManalinkEh 2022: 0x2302910; original cards/unglued_unhinged.c:199. */
int test_has_border_val(int iid, int val, int player, int card)
{
  border_t border;

  border = get_border_color((unsigned int)cards_data[iid].id);
  return (unsigned int)(val == border);
}

/* ManalinkEh 2022: 0x2303170; original cards/unglued_unhinged.c:206. */
void make_txtfx(int player, int card, CardFunction fxfn, card_id_t marker, int val)
{
  card_instance_t *instance;
  int legacy_card;
  target_t existing_legacy;

  instance = get_card_instance(player, card);
  instance->targets[5].player = marker << 0x10 | val & 0xffffU;
  existing_legacy = find_repeat_legacy_effect_alt(player, card, fxfn, player, card, -1, 2);
  if (existing_legacy.player < 0)
  {

    legacy_card = create_targetted_legacy_effect(player, card, fxfn, player, card);
    if (-1 < legacy_card)
    {
      instance = get_card_instance(player, legacy_card);
      instance->eot_toughness = 0x100;
      return;
    }
  }
  else
  {
    instance = get_card_instance(existing_legacy.player, existing_legacy.card);
    instance->targets[0x12].card = instance->targets[0x12].card + 1;
  }
  return;
}

/* Enter the Dungeon	|B|B
 * Sorcery
 * Players play a Magic subgame under the table, starting at 5 life and using their libraries as their decks. The winner searches their library for two cards, puts those cards into their hand, then shuffles their library. */

/* Eye to Eye	|2|B
 * Instant
 * You and target creature's controller have a staring contest. If you win, destroy that creature. */

/* Farewell to Arms	|1|B|B
 * Enchantment
 * As ~ enters the battlefield, choose a hand attached to an opponent's arm.
 * When the chosen hand isn't behind its owner's back, sacrifice ~. If you do, that player discards their hand . . . of cards. (The lawyers wouldn't let us do it the other way.) */

/* Infernal Spawn of Infernal Spawn of Evil	|8|B|B
 * Creature - Demon Child 8/8
 * Flying, first strike, trample
 * Once each turn, while you're searching your library, you may pay |1|B, reveal ~ from your library and say "I'm coming, too!" If you do, ~ deals 2 damage to a player of your choice. */

/* Kill! Destroy!	|1|B|B
 * Instant
 * Destroy target non|Sblack creature.
 * Gotcha - If an opponent says "kill" or "destroy," you may say "Gotcha!" When you do, return Kill! Destroy! from your graveyard to your hand. */

/* Mother of Goons	|2|B
 * Creature - Human Cleric 3/2
 * Whenever a creature an opponent controls dies, sacrifice ~ unless you insult that creature. */

/* Necro-Impotence	|B|B|B
 * Enchantment
 * Skip your untap step.
 * At the beginning of your upkeep, you may pay X life. If you do, untap X permanents.
 * Pay 1/2 life: Exile the top card of your library face down. Put that card into your hand at the beginning of the next end step. */

/* Persecute Artist	|1|B|B
 * Sorcery
 * Choose an artist other than Rebecca Guay. Target player reveals their hand and discards all nonland cards with art by the chosen artist. */

/* Phyrexian Librarian	|3|B
 * Creature - Horror 3/3
 * Flying, trample
 * At the beginning of your upkeep, exile the top card of your library and balance it on your body.
 * When a balanced card falls or touches another balanced card, sacrifice ~. */

/* Stop That	|B
 * Instant
 * Target player discards a card.
 * Gotcha - If an opponent audibly flicks the cards in their hand, you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Tainted Monkey	|1|B
 * Creature - Monkey 1/1
 * |T: Choose a word. Target player puts the top card of their library into their graveyard. If that card has the chosen word in its text box, that player loses 3 life. */

/* Vile Bile	|1|B
 * Creature - Ooze 302/302
 * Whenever a player's skin or fingernail touches ~, that player loses 2 life. */

/* Wet Willie of the Damned	|2|B|B
 * Sorcery
 * ~ deals 21/2 damage to any target and you gain 21/2 life. */

/* When Fluffy Bunnies Attack	|3|B
 * Instant
 * Target creature gets -X/-X until end of turn, where X is the number of times the letter of your choice appears in that creature's name. */

/* Working Stiff	|1|B
 * Creature - Mummy 2/2
 * As ~ enters the battlefield, straighten your arms.
 * When you bend an elbow, sacrifice ~. */

/* Zombie Fanboy	|2|B
 * Creature - Zombie Gamer 1/1
 * As ~ enters the battlefield, choose an artist.
 * Whenever a permanent with art by the chosen artist is put into a graveyard, put two +1/+1 counters on ~. */

/* Zzzyxas's Abyss	|1|B|B
 * Enchantment
 * At the beginning of your upkeep, destroy all nonland permanents with the first name alphabetically among nonland permanents. */

/* Assquatch	|4|R
 * Creature - Donkey 303/303
 * Each other Donkey gets +11/2/+11/2.
 * Whenever another Donkey enters the battlefield, untap target creature and gain control of it until end of turn. That creature gains haste until end of turn. */

/* Blast from the Past	|2|R
 * Instant
 * Madness |R, cycling |1|R, kicker |2|R, flashback |3|R, buyback |4|R
 * ~ deals 2 damage to any target. If this spell was kicked, create a 1/1 |Sred Goblin creature token. */

/* Curse of the Fire Penguin	|4|R|R
 * Enchantment - Aura
 * Enchant creature
 * ~ consumes and confuses enchanted creature.
 * -----
 * Creature - Penguin
 * Trample
 * When this creature dies, return ~ to the battlefield.
 * 6/5 */

/* Deal Damage	|2|R|R
 * Instant
 * ~ deals 4 damage to any target.
 * Gotcha - If an opponent says "deal" or "damage," you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Dumb Ass	|2|R
 * Creature - Donkey Barbarian 303/2
 * At the beginning of your upkeep, flip a coin. If you lose the flip, target opponent chooses whether ~ attacks this turn. */

/* Face to Face	|1|R
 * Sorcery
 * You and target opponent play a best two-out-of-three Rock, Paper, Scissors match. If you win, ~ deals 5 damage to that player. */

/* Frazzled Editor	|1|R
 * Creature - Human Bureaucrat 2/2
 * Protection from wordy (Something is wordy if it has four or more lines of rules text.) */

/* Goblin Mime	|1|R
 * Creature - Goblin Mime 2/2
 * When you speak, sacrifice ~. */

/* Goblin S.W.A.T. Team	|3|R
 * Creature - Goblin Warrior 2/2
 * Say "~": Put a +1/+1 counter on ~ unless an opponent swats the table within five seconds. Activate this ability only once each turn. */

/* Mana Flair	|1|R
 * Instant
 * Add |R for each nonland permanent with art by the artist of your choice. */

/* Mons's Goblin Waiters	|R
 * Creature - Goblin Waiter 1/1
 * Sacrifice a creature or land: Add |HR. */

/* Orcish Paratroopers	|2|R
 * Creature - Orc Paratrooper 4/4
 * When ~ enters the battlefield, flip it from a height of at least one foot. Sacrifice ~ unless it turns over completely at least once during the flip and lands face up. */

/* Pygmy Giant	|1|R|R
 * Creature - Giant 0/2
 * |R, |T, Sacrifice a creature: ~ deals X damage to target creature, where X is a number in the sacrificed creature's text box. */

/* Red-Hot Hottie	|2|R|R
 * Creature - Elemental 2/5
 * Whenever ~ deals damage to a creature, put a third-degree-burn counter on that creature. It gains "At the beginning of each end step, sacrifice this creature unless you scream 'Aaah!' at the top of your lungs." */

/* Rocket-Powered Turbo Slug	|3|R
 * Creature - Slug 3/1
 * Super haste (This may attack the turn before you cast it. (You may put this card from your hand onto the battlefield tapped and attacking during your declare attackers step. If you do, you lose the game at the beginning of your next turn's end step unless you pay this card's mana cost during that turn.)) */

/* Saute	|1|R|R
 * Instant
 * ~ deals 31/2 damage to any target. */

/* Six-y Beast	|3|R
 * Creature - Beast 0/0
 * As ~ enters the battlefield, you secretly put six or fewer +1/+1 counters on it, then an opponent guesses the number of counters. If that player guesses right, sacrifice ~ after it enters the battlefield. */

/* Touch and Go	|3|R
 * Sorcery
 * Destroy target land.
 * Gotcha - If an opponent touches their face, you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Yet Another AEther Vortex	|3|R|R
 * Enchantment
 * All creatures have haste.
 * Players play with the top card of their libraries revealed.
 * Noninstant, nonsorcery cards on top of a library are on the battlefield under their owner's control in addition to being in that library. */

/* B-I-N-G-O	|1|G
 * Creature - Hound 1/1
 * Trample
 * Whenever a player casts a spell, put a chip counter on its converted mana cost.
 * ~ gets +9/+9 for each set of three numbers in a row with chip counters on them. */

/* Creature Guy	|3|G
 * Creature - Beast 3/3
 * Gotcha - If an opponent says "creature" or "guy," you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Fat Ass	|4|G
 * Creature - Donkey Shaman 2/303
 * ~ gets +2/+2 and has trample as long as you're eating. (Gum isn't food. We checked.) */

/* Form of the Squirrel	|G
 * Enchantment
 * As ~ enters the battlefield, create a 1/1 |Sgreen Squirrel creature token. You lose the game when that creature leaves the battlefield.
 * Creatures can't attack you.
 * You have shroud.
 * You can't cast spells. */

/* Fraction Jackson	|2|G
 * Creature - Human Hero 1/301
 * |G, |T: Return target card with a 1/2 on it from your graveyard to your hand. */

/* Gluetius Maximus	|3|G|G
 * Creature - Beast 5/5
 * As ~ enters the battlefield, an opponent chooses one of your fingers. (Thumbs are fingers, too.)
 * When the chosen finger isn't touching ~, sacrifice ~. */

/* Granny's Payback	|7|G
 * Sorcery
 * You gain life equal to your age. */

/* Graphic Violence	|2|G
 * Instant
 * All creatures with art by the artist of your choice get +2/+2 and gain trample until end of turn. */

/* Keeper of the Sacred Word	|2|G
 * Creature - Human Druid 2/3
 * As ~ enters the battlefield, choose a word.
 * Whenever an opponent says the chosen word, ~ gets +3/+3 until end of turn. */

/* Land Aid '04	|G|G
 * Sorcery
 * Search your library for a basic land card, put that card onto the battlefield tapped, then shuffle your library. If you sang a song the whole time you were searching and shuffling, you may untap that land. */

/* Laughing Hyena	|1|G
 * Creature - Hyena 2/2
 * Gotcha - If an opponent laughs, you may say "Gotcha!" When you do, return ~ from your graveyard to your hand. */

/* Monkey Monkey Monkey	|3|G
 * Creature - Monkey 1/1
 * As ~ enters the battlefield, choose a letter.
 * ~ gets +1/+1 for each nonland permanent whose name begins with the chosen letter. */

/* Name Dropping	|1|G
 * Enchantment
 * Gotcha - If an opponent says a word that's in the name of a card in your graveyard, you may say "Gotcha!" When you do, return that card to your hand. */

/* Old Fogey	|G|G
 * Summon Dinosaur
 * Phasing, cumulative upkeep |1, echo |G|G, fading 3, bands with other Dinosaurs, protection from Homarids, snow-covered |H2plainswalk, flanking, rampage 2 */

/* Our Market Research Shows That Players	|1|G|G
 * Creature - Elemental 2/2
 * Art rampage 2 (Whenever this creature becomes blocked by a creature, it gets +2/+2 for each creature in the blocker's art beyond the first.) */

/* Remodel	|2|G
 * Instant
 * If you control two or more |Sgreen permanents that share an artist, you may cast this spell without paying its mana cost.
 * Exile target artifact. */

/* Side to Side	|2|G
 * Instant
 * You and target opponent arm-wrestle. If you win, create a 3/3 |Sgreen Ape creature token. */

/* S.N.O.T.	|G
 * Creature - Ooze 400/400
 * You may have S.N.O.T. enter the battlefield stuck to another creature named S.N.O.T. If you do, all those creatures form a single creature.
 * S.N.O.T.'s power and toughness are each equal to the square of the number of cards named S.N.O.T. stuck together to form it. (One is a 1/1, two are a 4/4, three are a 9/9, and four are a 16/16. If you manage to get -1 ~s stuck together, it's a 1/1.) */

/* Stone-Cold Basilisk	|4|G
 * Creature - Basilisk 302/5
 * Whenever ~ blocks or becomes blocked by a creature with fewer letters in its name, destroy that creature at end of combat. (Punctuation and spaces aren't letters.)
 * Whenever an opponent reads ~, that player is turned to stone until end of turn. Stoned players can't attack, block, cast spells, or activate abilities. */

/* Supersize	|1|G
 * Instant
 * Target creature gets +31/2/+31/2 until end of turn. */

// No Expansion-Symbol token.  Sorry.

/* "Ach! Hans, Run!"	|2|R|R|G|G
 * Enchantment
 * At the beginning of your upkeep, you may say "Ach! Hans, run! It's the . . ." and the name of a creature card. If you do, search your library for a card with that name, put it onto the battlefield, then shuffle your library. That creature gains haste. Exile it at the beginning of the next end step. */

/* Meddling Kids	|2|W|U
 * Creature - Human Child 2/3
 * As ~ enters the battlefield, choose a word with four or more letters.
 * Spells with the chosen word in their text box can't be cast. */

/* Who // What // When // Where // Why	|X|W|/|2|R|/|2|U|/|3|B|/|1|G
 * Instant
 * Target player gains X life.
 * //
 * Destroy target artifact.
 * //
 * Counter target creature spell.
 * //
 * Destroy target land.
 * //
 * Destroy target enchantment. */

/* Gleemax	|1|0|0|0|0|0|0
 * Legendary Artifact
 * You choose all targets for all spells and abilities. */

/* Letter Bomb	|6
 * Artifact
 * When ~ enters the battlefield, sign it and shuffle it into target player's library. That player reveals each card they draw until they draw ~. When that player draws ~, it deals 191/2 damage to them. */

/* Mox Lotus	|15
 * Artifact
 * |T: Add |+oo.
 * |1|0|0: Add one mana of any color. */

/* My First Tome	|3
 * Artifact
 * |1, |T: Say the flavor text on a card in your hand. Target opponent guesses that card's name. You may reveal that card. If you do and they guessed wrong, you draw a card. */

/* Pointy Finger of Doom	|4
 * Artifact
 * |3, |T: Spin ~ in the middle of the table so that it rotates completely at least once, then destroy the closest permanent the finger points to. */

/* Rod of Spanking	|1
 * Artifact
 * |2, |T: ~ deals 1 damage to target player. Then untap ~ unless that player says "Thank you, sir. May I have another?" */

/* Time Machine	|5
 * Artifact
 * |T: Exile ~ and target nontoken creature you own. Return them to the battlefield at the beginning of your upkeep on your turn X in the next game you play with one of your opponents in this game, where X is the converted mana cost of the exiled creature. */

/* Toy Boat	|3
 * Artifact Creature - Ship 3/3
 * Cumulative upkeep-Say "~" quickly. (At the beginning of your upkeep, put an age counter on ~, then sacrifice it unless you say "~" once for each age counter on it-without pausing between or fumbling it.) */

/* Urza's Hot Tub	|2
 * Artifact
 * |2, Discard a card: Search your library for a card that shares a complete word in its name with the discarded card, reveal it, put it into your hand, then shuffle your library. */

/* Water Gun Balloon Game	|2
 * Artifact
 * As ~ enters the battlefield, each player puts a pop counter on a 0.
 * Whenever a player casts a spell, move that player's pop counter up 1.
 * Whenever a player's pop counter hits 5, that player creates a 5/5 pink Giant Teddy Bear creature token and resets all pop counters to 0. */

// No Giant Teddy Bear token.  Sorry.

/* World-Bottling Kit	|5
 * Artifact
 * |5, Sacrifice ~: Choose a Magic set. Exile all permanents with that set's expansion symbol except for basic lands. */

/* City of Ass	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add one and one-half mana of any one color. */

/* R&D's Secret Lair	""
 * Legendary Land
 * Play cards as written. Ignore all errata.
 * |T: Add |C. (This mana is still added to your mana pool.) */

// Plains, Island, Swamp, Mountain, Forest.

/* Super Secret Tech	|3
 * Artifact
 * Premium spells cost |1 less to cast.
 * Premium creatures get +1/+1. */

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2302cb0; original cards/unglued_unhinged.c:227. */
static int txtfx_name(int player, int card, event_t event, card_id_t marker)
{
  unsigned int packed_text_effect;
  card_instance_t *instance;
  unsigned int stored_value;

  instance = get_displayed_card_instance(player, card);
  instance = get_displayed_card_instance((int)instance->damage_target_player,
                                         instance->damage_target_card);
  packed_text_effect = instance->targets[5].player;
  stored_value = packed_text_effect & 0xffff;
  if (packed_text_effect >> 0x10 != marker)
  {
    stored_value = txtfx_not_found;
  }
  return stored_value;
}

/* ManalinkEh 2022: 0x2303230; original cards/unglued_unhinged.c:227. */
int txtfx(int player, int card, event_t event, card_id_t marker)
{
  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
    return txtfx_name(player, card, event, marker);
  return txtfx_not_found;
}

/* ManalinkEh 2022: 0x2302980; original cards/unglued_unhinged.c:240. */
static int words_in_name_by_csvid(int csvid)
{
  if (csvid < 0 || csvid == 0x45ec)
    return 0;
  if (csvid == CARD_ID_OUR_MARKET_RESEARCH_SHOWS_THAT_PLAYERS)
    return 25;
  if (csvid == CARD_ID_THE_ULTIMATE_NIGHTMARE_OF_WOTC)
    return 10;
  if (csvid == CARD_ID_RUMORS_OF_MY_DEATH)
    return 4;
  const char *name = get_card_name_by_id(csvid);
  int result = 1;
  if (name)
    for (; *name; ++name)
      if (*name == ' ')
        result += name[1] != '/';
  return result;
}

/* ManalinkEh 2022: 0x2303260; original cards/unglued_unhinged.c:272. */
int words_in_name(int player, int card)
{
  return words_in_name_by_csvid(get_card_name(player, card));
}

/* ManalinkEh 2022: 0x2303280; original cards/unglued_unhinged.c:287. */
int card_the_cheese_stands_alone(int player, int card, event_t event)
{
  /* The Cheese Stands Alone	|4|W|W
   * Enchantment
   * When you control no permanents other than ~ and have no cards in hand, you win the game. */

  int c;
  int humiliated;
  card_instance_t *instance;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      humiliated = is_humiliated(player, card, AL_TRIG);
      if ((humiliated == 0) && (hand_count[player] < 1))
      {

        if (0 < active_cards_count[player])
        {
          c = 0;
          do
          {
            if (card != c)
            {
              instance = in_play(player, c);
              if (instance != NULL)
              {
                if (is_what(player, c, 0x47) != 0)
                {
                  return 0;
                }
              }
            }
            c = c + 1;
          } while (c < active_cards_count[player]);
        }
        lose_the_game(1 - player);
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2303350; original cards/unglued_unhinged.c:341. */
int card_once_more_with_feeling(int player, int card, event_t event)
{
  /* Once More with Feeling	|W|W|W|W
   * Sorcery
   * Exile all permanents and all cards from all graveyards. Each player shuffles their hand into their library, then draws seven cards. Each player's life total becomes 10. Exile ~.
   * DCI ruling - A deck can have only one card named ~. */

  test_definition_t test;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + (2 - hand_count[1]) * 0x18;
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 0x47);
    new_manipulate_all(player, card, 2, &test, ACT_KILL_REMOVE);

    APNAP(p, {
      rfg_whole_graveyard(p);
    });

    APNAP(p, {
      reshuffle_hand_into_deck(p, 1);
    });

    APNAP(p, {
      shuffle(p);
    });

    APNAP(p, {
      draw_cards(p, 7);
    });

    APNAP(p, {
      set_life_total(p, 10);
    });
    kill_card(player, card, KILL_EXILE);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2303590; original cards/unglued_unhinged.c:486. */
int card_jumbo_imp(int player, int card, event_t event)
{
  /* Jumbo Imp	|2|B
   * Creature - Imp 0/0
   * Flying
   * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of +1/+1 counters on it equal to the result.
   * At the beginning of your upkeep, roll a six-sided die and put a number of +1/+1 counters on ~ equal to the result.
   * At the beginning of your end step, roll a six-sided die and remove a number of +1/+1 counters from ~ equal to the result. */

  int result;

  enters_the_battlefield_with_counters_fn(player, card, event, COUNTER_P1_P1, roll_d6_shim);
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = roll_d6t(player, player, card, -1, -1);
    add_counters(player, card, player, card, COUNTER_P1_P1, result);
  }
  if ((current_turn == player) && (result = eot_trigger(player, card, event), result != 0))
  {
    result = roll_d6t(player, player, card, -1, -1);
    remove_counters(player, card, player, card, COUNTER_P1_P1, result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23036a0; original cards/unglued_unhinged.c:507. */
int card_organ_harvest(int player, int card, event_t event)
{
  /* Organ Harvest	|B
   * Sorcery
   * Your team may sacrifice any number of creatures. For each creature sacrificed this way, you add |B|B. */

  int result;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    new_default_test_definition(&test, 2, "");
    test.qty = (unsigned short)active_cards_count[player];
    result = new_sacrifice(player, card, player, SAC_DONE | SAC_NO_CANCEL, &test);
    produce_mana(player, 1, result * 2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2303760; original cards/unglued_unhinged.c:531. */
int card_poultrygeist(int player, int card, event_t event)
{
  /* Poultrygeist	|2|B
   * Creature - Chicken 1/1
   * Flying
   * Whenever a creature dies, you may roll a six-sided die. If you roll a 1, sacrifice ~. Otherwise, put a +1/+1 counter on ~. */

  int pending_deaths_or_mode;
  int die_roll;
  card_instance_t *instance;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
  }
  pending_deaths_or_mode = duh_mode(player);
  pending_deaths_or_mode = resolve_gfp_ability(
      player, card, event, (pending_deaths_or_mode != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (0 < pending_deaths_or_mode)
  {
    do
    {
      die_roll = roll_d6t(player, player, card, -1, -1);
      instance = in_play(player, card);
      if (instance)
      {
        if (die_roll == 1)
          kill_card(player, card, KILL_SACRIFICE);
        else
          add_counters(player, card, player, card, COUNTER_P1_P1, 1);
      }
      --pending_deaths_or_mode;
    } while (pending_deaths_or_mode != 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2303870; original cards/unglued_unhinged.c:559. */
int card_temp_of_the_damned(int player, int card, event_t event)
{
  /* Temp of the Damned	|2|B
   * Creature - Zombie 3/3
   * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of funk counters on it equal to the result.
   * At the beginning of your upkeep, remove a funk counter from ~. If you can't, sacrifice it. */

  int result;

  enters_the_battlefield_with_counters_fn(player, card, event, COUNTER_FUNK, roll_d6_shim);
  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  result = count_counters(player, card, COUNTER_FUNK);
  if (result != 0)
  {
    remove_counters(player, card, player, card, COUNTER_FUNK, 1);
    return 0;
  }
  kill_card(player, card, KILL_SACRIFICE);
  return 0;
}

/* ManalinkEh 2022: 0x2303930; original cards/unglued_unhinged.c:591. */
int card_chicken_egg(int player, int card, event_t event)
{
  /* Chicken Egg	|1|R
   * Creature - Egg 0/1
   * At the beginning of your upkeep, roll a six-sided die. If you roll a 6, sacrifice ~ and create a 4/4 |Sred Giant Chicken creature token. */

  int result;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = roll_d6t(player, player, card, -1, -1);
    if (result == 6)
    {
      kill_card(player, card, KILL_SACRIFICE);
      generate_token_by_id(player, card, CARD_ID_GIANT_BIRD);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
char *target_has_2_word_name(int who_chooses, int player, int card, int targeting_player,
                             int targeting_card);

int card_goblin_tutor(int player, int card, event_t event){
	/* Goblin Tutor	|R	0x20013fd
	 * Instant
	 * Roll a six-sided die. If you roll a 1, ~ has no effect. Otherwise, search your library for the indicated card, reveal it, put it into your hand, then shuffle your library.
	 * 2 - A card named ~
	 * 3 - An enchantment card
	 * 4 - An artifact card
	 * 5 - A creature card
	 * 6 - An instant or sorcery card */

	if (event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		int roll = roll_d6t(player, player, card, -1, -1);
		switch (roll){
			case 2:
				new_default_test_definition(&test, TYPE_ANY, "Select a Goblin Tutor card.");
				test.id = CARD_ID_GOBLIN_TUTOR;
				break;
			case 3: new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment card."); break;
			case 4: new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card."); break;
			case 5: new_default_test_definition(&test, TYPE_CREATURE, "Select an creature card."); break;
			case 6: new_default_test_definition(&test, TYPE_SPELL, "Select an instant or sorcery card."); break;
		}
		if (roll != 1){ new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, roll == 2 ? AI_FIRST_FOUND : AI_MAX_VALUE, &test); }
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2303b60; original cards/unglued_unhinged.c:666. */
int card_krazy_kow(int player, int card, event_t event)
{
  /* Krazy Kow	|3|R
   * Creature - Cow 3/3
   * At the beginning of your upkeep, roll a six-sided die. If you a roll a 1, sacrifice ~ and it deals 3 damage to each creature and each player. */

  int result;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    result = roll_d6t(player, player, card, -1, -1);
    if (result == 1)
    {
      kill_card(player, card, KILL_SACRIFICE);
      new_damage_all(player, card, 2, 3, 3, NULL);
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int rock_paper_scissors(int player, int card, event_t event, int beaten);

/* ManalinkEh 2022: 0x2303df0; original cards/unglued_unhinged.c:755. */
int card_strategy_schmategy(int player, int card, event_t event)
{
  /* Strategy, Schmategy	|1|R
   * Sorcery
   * Roll a six-sided die. ~ has the indicated effect.
   * 1 - Do nothing.
   * 2 - Destroy all artifacts.
   * 3 - Destroy all lands.
   * 4 - ~ deals 3 damage to each creature and each player.
   * 5 - Each player discards their hand and draws seven cards.
   * 6 - Repeat this process two more times. */

  int result;
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2303e00; original cards/unglued_unhinged.c:818. */
int card_elvish_impersonators(int player, int card, event_t event)
{
  /* Elvish Impersonators	|3|G
   * Creature - Elves 100/100
   * As ~ enters the battlefield, roll a six-sided die twice. Its base power becomes the first result and its base toughness becomes the second result. */

  int rolled_power;
  int rolled_toughness;
  card_instance_t *instance;
  unsigned int newval;
  unsigned int packed_stat;

  if (event != EVENT_RESOLVE_SPELL)
  {

    if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
         (affected_card_controller == player)) &&
        (card != -1))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_POWER)
      {
        packed_stat = instance->targets[5].player;
      }
      else
      {
        packed_stat = instance->targets[5].card;
      }
      newval = 0;
      if ((short)(packed_stat >> 0x10) == 0x454e)
      {
        newval = packed_stat & 0xffff;
      }
      set_characteristic_pt(newval);
    }
    return 0;
  }

  rolled_power = roll_d6t(player, player, card, -1, -1);
  rolled_toughness = roll_d6t(player, player, card, -1, -1);
  instance = get_card_instance(player, card);
  instance->targets[5].player = (int)(short)rolled_power | 0x454e0000;
  instance->targets[5].card = (int)(short)rolled_toughness | 0x454e0000;
  return 0;
}

/* ManalinkEh 2022: 0x2303f20; original cards/unglued_unhinged.c:849. */
int card_flock_of_rabid_sheep(int player, int card, event_t event)
{
  /* Flock of Rabid Sheep	|X|G|G
   * Sorcery
   * Flip X coins. For each flip you win, create a 2/2 |Sgreen Sheep creature token named Rabid Sheep. */

  int remaining_flips;
  card_instance_t *instance;
  int flip_result;
  int sheep_count;
  token_generation_t token;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    remaining_flips = instance->info_slot;
    if (0 < remaining_flips)
    {
      sheep_count = 0;
      do
      {
        flip_result = flip_a_coin(player, card);
        sheep_count = (sheep_count + 1) - (unsigned int)(flip_result == 0);
        remaining_flips = remaining_flips + -1;
      } while (remaining_flips != 0);
      if (sheep_count != 0)
      {
        default_token_definition(player, card, CARD_ID_SHEEP, &token);
        token.qty = (unsigned char)sheep_count;
        token.tou = 2;
        token.pow = 2;
        generate_token(&token);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 4, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x2304010; original cards/unglued_unhinged.c:892. */
int card_growth_spurt(int player, int card, event_t event)
{
  /* Growth Spurt	|1|G
   * Instant
   * Roll a six-sided die. Target creature gets +X/+X until end of turn, where X is the result. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_RESOLVE_SPELL)
  {
    result = vanilla_instant_pump(player, card, event, 2, player, 4, 4, 0, 0);
    return result;
  }
  default_target_definition(player, card, &td, 2);
  result = valid_target(&td);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    result = roll_d6t(player, player, card, instance->targets[0].player, instance->targets[0].card);
    pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, result,
                   result);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2304240; original cards/unglued_unhinged.c:976. */
int card_team_spirit(int player, int card, event_t event)
{
  /* Team Spirit	|2|G
   * Instant
   * Creatures target player's team controls get +1/+1 until end of turn. */

  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      instance = get_card_instance(player, card);
      pump_subtype_until_eot(player, card, instance->targets[0].player, -1, 1, 1, 0, 0);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x23043b0; original cards/unglued_unhinged.c:1001. */
int card_timmy_power_gamer(int player, int card, event_t event)
{
  /* Timmy, Power Gamer	|2|G|G
   * Legendary Creature - Human Gamer 1/1
   * |4: You may put a creature card from your hand onto the battlefield. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 2, "Select a creature card.");
    new_global_tutor(player, player, 4, 4, 0, 2, &test);
  }
  result = generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

int card_jack_in_the_mox(int player, int card, event_t event){
	/* Jack-in-the-Mox	|0	0x2005499
	 * Artifact
	 * |T: Roll a six-sided die. This ability has the indicated effect.
	 * 1 - Sacrifice ~ and you lose 5 life.
	 * 2 - Add |W.
	 * 3 - Add |U.
	 * 4 - Add |B.
	 * 5 - Add |R.
	 * 6 - Add |G. */

	if (event != EVENT_ACTIVATE){
		return mana_producer(player, card, event);
	}
	get_card_instance(player, card)->state |= STATE_TAPPED;
	int roll = roll_d6t(player, player, card, -1, -1);
	int color = -1;
	switch (roll){
		case 1: lose_life(player, 5); kill_card(player, card, KILL_SACRIFICE); break;
		case 2: color = COLOR_WHITE; break;
		case 3: color = COLOR_BLUE; break;
		case 4: color = COLOR_BLACK; break;
		case 5: color = COLOR_RED; break;
		case 6: color = COLOR_GREEN; break;
	}
	if (color != -1 && can_produce_mana(player, card)){
		produce_mana_tapped(player, card, color, 1);
	}
	else{
		tapped_for_mana_color = -2;
	}
	return 0;
}

/* ManalinkEh 2022: 0x2302be0; original cards/unglued_unhinged.c:1096. */
static int rock_paper_scissors(int player, int card, event_t event, int beaten)
{
  int result;
  card_instance_t *instance;

  if (event != EVENT_ABILITIES)
  {
    return 0;
  }
  result = get_card_name(affected_card_controller, affected_card);
  if (result == beaten)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 2);
      if (result != 0)
      {
        result = is_humiliated(player, card, AL_STATIC);
        if (result == 0)
        {
          cannot_attack(affected_card_controller, affected_card, EVENT_ABILITIES);
          cannot_block(affected_card_controller, affected_card, EVENT_ABILITIES);
        }
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int has_reminder_text(int player, int card);

/* ManalinkEh 2022: 0x2304590; original cards/unglued_unhinged.c:1110. */
int card_paper_tiger(int player, int card, event_t event)
{
  /* Paper Tiger	|4
   * Artifact Creature - Cat 4/3
   * Creatures named Rock Lobster can't attack or block. */

  int result;

  result = rock_paper_scissors(player, card, event, 0x45b7);
  return result;
}

/* ManalinkEh 2022: 0x23045b0; original cards/unglued_unhinged.c:1119. */
int card_rock_lobster(int player, int card, event_t event)
{
  /* Rock Lobster	|4
   * Artifact Creature - Lobster 4/3
   * Creatures named Scissors Lizard can't attack or block. */

  int result;

  result = rock_paper_scissors(player, card, event, 0x45bd);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23045d0; original cards/unglued_unhinged.c:1128. */
int card_scissors_lizard(int player, int card, event_t event)
{
  /* Scissors Lizard	|4
   * Artifact Creature - Lizard 4/3
   * Creatures named Paper Tiger can't attack or block. */

  int result;

  result = rock_paper_scissors(player, card, event, 0x45a7);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23045f0; original cards/unglued_unhinged.c:1137. */
int card_spatula_of_the_ages(int player, int card, event_t event)
{
  /* Spatula of the Ages	|4
   * Artifact
   * |4, |T, Sacrifice ~: You may put a silver-bordered permanent card from your hand onto the battlefield. */

  int result;
  test_definition_t test;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 0x47, "Select a silver-bordered permanent card.");
    test.special_selection_function = test_has_border_val;
    test.value_for_special_selection_function = 2;
    new_global_tutor(player, player, 4, 4, 0, 2, &test);
  }
  result = generic_activated_ability(player, card, event, 9, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23046e0; original cards/unglued_unhinged.c:1163. */
int card_urzas_science_fair_project(int player, int card, event_t event)
{
  /* Urza's Science Fair Project	|6
   * Artifact Creature - Construct 4/4
   * |2: Roll a six-sided die. ~ gets the indicated result.
   * 1 - It gets -2/-2 until end of turn.
   * 2 - Prevent all combat damage it would deal this turn.
   * 3 - It gains vigilance until end of turn.
   * 4 - It gains first strike until end of turn.
   * 5 - It gains flying until end of turn.
   * 6 - It gets +2/+2 until end of turn. */

  int parent_card;
  int parent_player;
  int roll_power_or_keywords;
  card_instance_t *instance;
  int toughness_bonus;
  int alternate_legacy_text_number;

  if (event != EVENT_RESOLVE_ACTIVATION)
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);

  roll_power_or_keywords = roll_d6t(player, player, card, -1, -1);
  instance = get_card_instance(player, card);
  parent_player = instance->parent_controller;
  parent_card = instance->parent_card;
  instance = in_play(parent_player, parent_card);
  if (instance == NULL)
  {
    return 0;
  }
  switch (roll_power_or_keywords)
  {
  default:
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  case 1:
    toughness_bonus = -2;
    roll_power_or_keywords = -2;
    pump_until_eot_merge_previous_alt(player, card, parent_player, parent_card, roll_power_or_keywords,
                                      toughness_bonus, 1);
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  case 2:
    negate_combat_damage_this_turn(player, card, parent_player, parent_card, 2);
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  case 3:
    alternate_legacy_text_number = 3;
    toughness_bonus = 8;
    roll_power_or_keywords = 0;
    break;
  case 4:
    alternate_legacy_text_number = 4;
    toughness_bonus = 0;
    roll_power_or_keywords = 0x100;
    break;
  case 5:
    alternate_legacy_text_number = 5;
    toughness_bonus = 0;
    roll_power_or_keywords = 0x20;
    break;
  case 6:
    toughness_bonus = 2;
    roll_power_or_keywords = 2;
    pump_until_eot_merge_previous_alt(player, card, parent_player, parent_card, roll_power_or_keywords,
                                      toughness_bonus, 1);
    return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  pump_ability_until_eot_no_repeat_alt(player, card, parent_player, parent_card, roll_power_or_keywords,
                                       toughness_bonus, alternate_legacy_text_number);
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23048a0; original cards/unglued_unhinged.c:1229. */
int card_awol(int player, int card, event_t event)
{
  /* AWOL	|2|W
   * Instant
   * Exile target attacking creature. Then remove it from the game. Then put it into the absolutely-removed-from-the-freaking-game-forever zone. */

  int owner;
  card_instance_t *instance;
  int iid;
  unsigned int csvid;
  int csvids[2];
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_ATTACKING;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      instance = get_card_instance(player, card);
      if (is_token(instance->targets[0].player, instance->targets[0].card) == 0)
      {
        owner = get_owner(instance->targets[0].player, instance->targets[0].card);
        iid = get_original_internal_card_id(instance->targets[0].player,
                                              instance->targets[0].card);
        csvid = (unsigned int)cards_data[iid].id;
        if ((csvid == CARD_ID_BRISELA_VOICE_OF_NIGHTMARES || csvid == CARD_ID_CHITTERING_HOST) || (csvid == CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP))
        {
          unmeld(csvid, csvids, csvids + 1);
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
          remove_card_from_rfg(owner, csvids[0]);
          remove_card_from_rfg(owner, csvids[1]);
        }
        else
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
          remove_card_from_rfg(owner, csvid);
        }
      }
      else
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ATTACKING_CREATURE", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int test_has_border_val(int iid, int val, int player, int card);
char *target_has_silver_border(int who_chooses, int player, int card, int targeting_player,
                               int targeting_card);

/* ManalinkEh 2022: 0x2304b20; original cards/unglued_unhinged.c:1364. */
int card_wordmail(int player, int card, event_t event)
{
  /* Wordmail	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 for each word in its name. */

  int result;
  card_instance_t *instance;

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) && (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
         (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      result = get_card_name((int)instance->damage_target_player, instance->damage_target_card);
      result = words_in_name_by_csvid(result);
      event_result = event_result + result;
    }
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, player);
}

/* ManalinkEh 2022: 0x2302a00; original cards/unglued_unhinged.c:1421. */
char *target_has_2_word_name(int who_chooses, int player, int card, int targeting_player,
                             int targeting_card)
{
  int result;
  char *requirement_error;

  result = get_card_name(player, card);
  result = words_in_name_by_csvid(result);
  requirement_error = NULL;
  if (result != 2)
  {
    requirement_error = "two-word name";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x2304c70; original cards/unglued_unhinged.c:1425. */
int card_double_header(int player, int card, event_t event)
{
  /* Double Header	|3|U|U
   * Creature - Drake 2/3
   * Flying
   * When ~ enters the battlefield, you may return target permanent with a two-word name to its owner's hand. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  default_target_definition(player, card, &td, 0x47);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)target_has_2_word_name;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target permanent with a two-word name.", 0, 0x80);
    if (result != 0)
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2304d50; original cards/unglued_unhinged.c:1470. */
int card_johnny_combo_player(int player, int card, event_t event)
{
  /* Johnny, Combo Player	|2|U|U
   * Legendary Creature - Human Gamer 1/1
   * |4: Search your library for a card, put that card into your hand, then shuffle your library. */

  int result;
  test_definition_t test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 0xff, "Select a card.");
    new_global_tutor(player, player, 1, 1, 0, 1, &test);
  }
  result = generic_activated_ability(player, card, event, 0, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2305090; original cards/unglued_unhinged.c:1662. */
int card_bloodletter(int player, int card, event_t event)
{
  /* Bloodletter	|2|B
   * Creature - Zombie 2/3
   * When the names of three or more nonland permanents begin with the same letter, sacrifice ~. If you do, it deals 2 damage to each creature and each player. */

  card_instance_t *instance;
  int scan_player_or_result;
  int id_count_or_result;
  unsigned char *name_start;
  int card_00;
  unsigned char ltr;
  int letter[26];
  if (((event == EVENT_STATIC_EFFECTS) && (instance = in_play(player, card), instance != NULL)) &&
      (scan_player_or_result = is_humiliated(player, card, AL_TRIG), scan_player_or_result == 0))
  {
    memset(letter, 0, sizeof(letter));
    scan_player_or_result = 0;
    do
    {
      card_00 = 0;
      if (0 < active_cards_count[scan_player_or_result])
      {
        do
        {
          instance = in_play(scan_player_or_result, card_00);
          if (((instance != NULL) &&
               (id_count_or_result = is_what(scan_player_or_result, card_00, 1),
                id_count_or_result == 0)) &&
              (id_count_or_result = is_what(scan_player_or_result, card_00, 0x47),
               id_count_or_result != 0))
          {
            id_count_or_result = get_card_name(scan_player_or_result, card_00);
            name_start = (unsigned char *)get_card_name_by_id(id_count_or_result);
            ltr = lc_unaccent_map[*name_start];
            if ((ltr & 0xdf) == 0xc6)
            {
              letter[0] = letter[0] + 1;
              if (2 < letter[0])
              {
                if (can_sacrifice_this_as_cost(player, card) == 0)
                  return 0;
                kill_card(player, card, KILL_SACRIFICE);
                new_damage_all(player, card, 2, 2, 3, NULL);
                return 0;
              }
              letter[4] = letter[4] + 1;
              id_count_or_result = letter[4];
            }
            else
            {
              if (0x19 < (unsigned char)(ltr + 0x9f))
              {
                ++card_00;
                continue;
              }
              id_count_or_result = letter[ltr - 0x61];
              letter[ltr - 0x61] = id_count_or_result + 1;
              id_count_or_result = id_count_or_result + 1;
            }
            if (2 < id_count_or_result)
            {
              if (can_sacrifice_this_as_cost(player, card) == 0)
              {
                return 0;
              }
              kill_card(player, card, KILL_SACRIFICE);
              new_damage_all(player, card, 2, 2, 3, NULL);
              return 0;
            }
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[scan_player_or_result]);
      }
      scan_player_or_result = scan_player_or_result + 1;
    } while (scan_player_or_result != 2);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int roll_d6_shim(int player, int card);

static const char *target_has_reminder_text(int who_chooses, int player, int card,
                                            int targeting_player, int targeting_card)
{
  return has_reminder_text(player, card) ? NULL : "reminder text";
}

int card_duh(int player, int card, event_t event)
{
  /* Duh	|B
   * Instant
   * Destroy target creature with reminder text. (Reminder text is any italicized text in parentheses that explains rules you already know.) */

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(target_has_reminder_text);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET|GS_LITERAL_PROMPT, &td, "Select target creature with reminder text.", 1, NULL);
}

/* ManalinkEh 2022: 0x23059c0; original cards/unglued_unhinged.c:1946. */
int card_goblin_secret_agent(int player, int card, event_t event)
{
  /* Goblin Secret Agent	|2|R
   * Creature - Goblin Rogue 2/2
   * First strike
   * At the beginning of your upkeep, reveal a card from your hand at random. */

  card_instance_t *instance;
  int result;
  int upperbound;
  int indices[151];

  upkeep_trigger_ability(player, card, event, player);

  if ((event == EVENT_UPKEEP_TRIGGER_ABILITY) && (0 < active_cards_count[player]))
  {
    result = 0;
    upperbound = 0;
    do
    {
      instance = in_hand(player, result);
      if (instance != NULL)
      {
        indices[upperbound] = result;
        upperbound = upperbound + 1;
      }
      result = result + 1;
    } while (result < active_cards_count[player]);
    if (0 < upperbound)
    {
      result = network_rand(player, upperbound);
      reveal_card(player, card, player, indices[result]);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2305ca0; original cards/unglued_unhinged.c:2154. */
int card_shoe_tree(int player, int card, event_t event)
{
  /* Shoe Tree	|4|G
   * Creature - Treefolk 3/3
   * ~ enters the battlefield with up to two shoe counters on it. Use your shoes as counters.
   * ~ gets +1/+1 for each shoe counter on it. */

  int result;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_SHOE, 2);
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = count_counters(player, card, COUNTER_SHOE);
      event_result = event_result + result;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2305f10; original cards/unglued_unhinged.c:2238. */
int card_uktabi_kong(int player, int card, event_t event)
{
  /* Uktabi Kong	|5|G|G|G
   * Creature - Ape 8/8
   * Trample
   * When ~ enters the battlefield, destroy all artifacts.
   * Tap two untapped Apes you control: Create a 1/1 |Sgreen Ape creature token. */

  test_definition_t test;
  int result;
  token_generation_t token;


  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
         (event == EVENT_RESOLVE_ACTIVATION)) ||
        (event == EVENT_CLEANUP))
    {
      target_definition_t td;
      base_target_definition(player, card, &td, 0x47);
      td.illegal_state = TARGET_STATE_TAPPED;
      td.required_subtype = 5;
      td.allowed_controller = player;
      td.preferred_controller = player;
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        default_token_definition(player, card, CARD_ID_APE, &token);
        token.tou = 1;
        token.pow = 1;
        generate_token(&token);
      }
      result = generic_activated_ability_with_tapsubtype_as_cost(player, card, event, GAA_NONE, 0,
                                                                 0, 0, 0, 0, 0, 0, NULL, NULL,
                                                                 &td, 2);
    }
    else
    {
      result = 0;
    }
    return result;
  }
  default_test_definition(&test, 0x40);
  new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2302940; original cards/unglued_unhinged.c:2279. */
char *target_has_silver_border(int who_chooses, int player, int card, int targeting_player,
                               int targeting_card)
{
  uint32_t csvid;
  border_t border;
  char *requirement_error;

  csvid = get_id(player, card);
  border = get_border_color(csvid);
  requirement_error = NULL;
  if (border != BORDER_SILVER)
  {
    requirement_error = "silver-bordered";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x23060d0; original cards/unglued_unhinged.c:2283. */
int card_ass_whuppin(int player, int card, event_t event)
{
  /* Ass Whuppin'	|1|W|B
   * Sorcery
   * Destroy target silver-bordered permanent in any game you can see from your seat. */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0x47);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)target_has_silver_border;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {

      card_instance_t *inst = get_card_instance(player, card);
      kill_card(inst->targets[0].player, inst->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target silver-bordered permanent.",
                         1, NULL);
}

/* ManalinkEh 2022: 0x2302ba0; original cards/unglued_unhinged.c:2316. */
static int test_rare_or_mythic(int iid, int val, int player, int card)
{
  const Rarities *rarity;

  rarity = get_rarity((unsigned int)cards_data[iid].id);
  return (unsigned int)((uint32_t)rarity->default_rarity - 3u < 2u);
}

/* ManalinkEh 2022: 0x2306230; original cards/unglued_unhinged.c:2321. */
int card_rare_b_gone(int player, int card, event_t event)
{
  /* Rare-B-Gone	|2|B|R
   * Sorcery
   * Each player sacrifices all permanents that are rare or mythic rare, then each player reveals their hand and discards all cards that are rare or mythic rare. */

  int result;
  ec_definition_t hoop_to_jump_through;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&test, 0x47);
    test.special_selection_function = test_rare_or_mythic;
    new_manipulate_all(player, card, 2, &test, ACT_KILL_SACRIFICE);
    default_ec_definition(0, player, &hoop_to_jump_through);

    hoop_to_jump_through.effect = 0x81;
    test.type = 0xff;
    hoop_to_jump_through.target_player = current_turn;
    new_effect_coercion(&hoop_to_jump_through, &test);
    hoop_to_jump_through.target_player = current_turn ^ 1;
    new_effect_coercion(&hoop_to_jump_through, &test);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2306330; original cards/unglued_unhinged.c:2372. */
int card_mana_screw(int player, int card, event_t event)
{
  /* Mana Screw	|1
   * Artifact
   * |1: Flip a coin. If you win the flip, add |C|C. Activate this ability only any time you could cast an instant. */

  int result;

  if (event == EVENT_CAN_ACTIVATE)
  {
    if ((((unsigned int)land_can_be_played >> 16 & 0xff) & 4) == 0)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x200000, 1, 0, 0, 0, 0,
                                         0, 0, NULL, NULL);
      return result;
    }
  }
  else if ((event == EVENT_ACTIVATE) &&
           (generic_activated_ability(player, card, EVENT_ACTIVATE, 0x200000, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL),
            spell_fizzled != 1))
  {
    result = flip_a_coin(player, card);
    if (result != 0)
    {
      produce_mana(player, 0, 2);
    }
    tapped_for_mana_color = -2;
  }
  return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2302de0; original cards/unglued_unhinged.c:39. */
void get_rules_text(char *out_buf, int player, int card, int remove_reminder_text)
{
  char next_symbol;
  int result;
  char *marker;
  char character;
  const char *source;
  char dfcbackpat1[300];
  char dfcbackpat2[300];

  result = get_id(player, card);
  if (result == 0x33c4)
  {
    *out_buf = '\0';
    return;
  }
  const char *orig_rules = cards_ptr[result]->rules_text;
  unsigned int rules_size = strlen(orig_rules) + 1;
  char rules[rules_size];
  memcpy(rules, orig_rules, rules_size);
  source = rules;
  marker = strstr(rules, "\n--FLIP--\n");
  if (marker != NULL)
  {
    *marker = '\0';
  }
  dfcbackpat1[0] = '\n';
  dfcbackpat1[1] = '(';
  dfcbackpat1[2] = '\0';
  strcpy(dfcbackpat1 + 2, cards_ptr[result + 1]->name);
  strcpy(dfcbackpat2, dfcbackpat1);
  strcat(dfcbackpat1, " - ");
  strcat(dfcbackpat2, ")");
  marker = strstr(rules, dfcbackpat1);
  if (marker != NULL)
  {
    *marker = '\0';
  }
  marker = strstr(rules, dfcbackpat2);
  if (marker != NULL)
  {
    *marker = '\0';
  }
  while (true)
  {
    character = *source;
    *out_buf = character;
    if (character == '\0')
      return;
    if (character == '(' && remove_reminder_text != 0)
    {
      *out_buf = '\0';
      marker = strchr(source + 1, ')');
      if (marker == NULL)
        return;
      source = marker + 1;
      continue;
    }
    if (character != '|')
    {
      ++source;
      ++out_buf;
      continue;
    }
    character = source[1];
    switch (character)
    {
    case '/':
      out_buf[0] = 'o';
      /* Preserve the native one-character advance: the next character replaces this 'r'. */
      out_buf[1] = 'r';
      source += 2;
      ++out_buf;
      break;
    case '0':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 'A':
    case 'C':
    case 'E':
    case 'I':
    case 'L':
    case 'Q':
    case 'S':
    case 'T':
    case 'X':
      *out_buf = '\0';
      source += 2;
      break;
    case '1':
      character = source[2];
      *out_buf = '\0';
      source += (unsigned char)(character - 0x30U) < 10 ? 3 : 2;
      break;
    case '2':
      character = source[2];
      *out_buf = '\0';
      source += strchr("0BUGRW", (int)character) != NULL ? 3 : 2;
      break;
    case 'B':
    case 'G':
    case 'R':
    case 'U':
    case 'W':
      next_symbol = source[2];
      *out_buf = '\0';
      if (character == next_symbol)
        result = 1;
      else
        result = (strchr("BUGRW", (int)next_symbol) != NULL) + 1;
      source += 1 + result;
      break;
    case 'H':
      character = source[2];
      *out_buf = '\0';
      source += (unsigned char)(character - 0x31U) < 2 ? 3 : 2;
      break;
    case 'P':
      next_symbol = source[2];
      if (strchr("BUGRW", (int)next_symbol) != NULL)
      {
        *out_buf = '\0';
        source += 3;
      }
      else
      {
        ++source;
        ++out_buf;
      }
      break;
    case '|':
      source += 2;
      ++out_buf;
      break;
    default:
      ++source;
      ++out_buf;
      break;
    }
  }
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static char *target_must_have_counters(int who_chooses, int player, int card, int targeting_player,
                                       int targeting_card);

/* ManalinkEh 2022: 0x23034f0; original cards/unglued_unhinged.c:427. */
int card_fowl_play(int player, int card, event_t event)
{
  /* Fowl Play	|2|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature is a Chicken with base power and toughness 1/1 and loses all abilities. */

  int result;

  attached_set_pt(player, card, event, 1, 1);
  attached_is_forced_subtype(player, card, event, SUBTYPE_BIRD);
  disable_attached(player, card, event, 0x7e);
  result = aura(player, card, event, NULL, NULL);
  disabling_aura_ai(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2302d00; original cards/unglued_unhinged.c:690. */
static int fx_spark_fiend(int player, int card, event_t event)
{
  int result;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    result = txtfx_name(player, card, event, 0x45c4);
    if (result != txtfx_not_found)
    {
      if (result != 0x2f02)
      {
        sprintf(set_legacy_effect_name_addr, "%d", result);
        return 0;
      }
      strncpy(set_legacy_effect_name_addr, "Don\'t roll", 0xb);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2303c00; original cards/unglued_unhinged.c:702. */
int card_spark_fiend(int player, int card, event_t event)
{
  /* Spark Fiend	|4|R
   * Creature - Beast 5/6
   * When ~ enters the battlefield, roll two six-sided dice. If you rolled 2, 3, or 12, sacrifice ~. If you rolled 7 or 11, don't roll dice for ~ during any of your following upkeeps. If you rolled any other total, note that total.
   * At the beginning of your upkeep, roll two six-sided dice. If you rolled 7, sacrifice ~. If you roll the noted total, don't roll dice for ~ during any of your following upkeeps. Otherwise, do nothing. */

  unsigned int packed_point;
  int val;
  card_instance_t *instance;
  unsigned int rolled_total;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    upkeep_trigger_ability(player, card, event, player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {
      instance = get_card_instance(player, card);
      packed_point = instance->targets[5].player;
      if ((short)(packed_point >> 0x10) == 0x45c4)
      {
        if ((short)packed_point != 0x2f02)
        {
          rolled_total = roll_xd6t(player, 2, player, card, -1, -1);
          if (rolled_total == 7)
          {
            kill_card(player, card, KILL_SACRIFICE);
          }
          else if (rolled_total == (packed_point & 0xffff))
          {
            make_txtfx(player, card, fx_spark_fiend, CARD_ID_SPARK_FIEND, 0x2f02);
          }
        }
      }
      else
      {
        roll_xd6t(player, 2, player, card, -1, -1);
      }
    }
    return 0;
  }
  val = roll_xd6t(player, 2, player, card, -1, -1);
  if ((val - 2U < 2) || (val == 0xc))
  {
    kill_card(player, card, KILL_SACRIFICE);
  }
  else
  {
    if ((val - 7U & 0xfffffffb) == 0)
    {
      val = 0x2f02;
    }
    make_txtfx(player, card, fx_spark_fiend, CARD_ID_SPARK_FIEND, val);
  }
  upkeep_trigger_ability(player, card, EVENT_ETB, player);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2302b70; original cards/unglued_unhinged.c:919. */
static char *target_must_have_counters(int who_chooses, int player, int card, int targeting_player,
                                       int targeting_card)
{
  int result;
  char *requirement_error;

  result = count_counters(player, card, ~COUNTER_P1_P1);
  requirement_error = NULL;
  if (result == 0)
  {
    requirement_error = "must have counters";
  }
  return requirement_error;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2304110; original cards/unglued_unhinged.c:924. */
int card_hungry_hungry_heifer(int player, int card, event_t event)
{
  /* Hungry Hungry Heifer	|2|G
   * Creature - Cow 3/3
   * At the beginning of your upkeep, you may remove a counter from a permanent you control. If you don't, sacrifice ~. */

  card_instance_t *instance;
  int result;
  counter_t type;
  target_definition_t td;

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }

  base_target_definition(player, card, &td, 0x47);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)target_must_have_counters;
  instance = get_card_instance(player, card);
  instance->number_of_targets = '\0';
  result = can_target(&td);
  if ((result != 0) && (result = pick_target(&td, "TARGET_PERMANENT"), result != 0))
  {

    instance->number_of_targets = '\0';
    type = choose_existing_counter_type(
        player, player, card, instance->targets[0].player, instance->targets[0].card,
        CECT_REMOVE | CECT_CONSIDER_ALL | CECT_HUMAN_CAN_CANCEL, -1, -1);
    if (type != COUNTER_invalid)
    {
      remove_counters(player, card, instance->targets[0].player,
                                  instance->targets[0].card, type, 1);
      return 0;
    }
  }
  kill_card(player, card, KILL_SACRIFICE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2304e30; original cards/unglued_unhinged.c:1497. */
int card_mise(int player, int card, event_t event)
{
  /* Mise	|U
   * Instant
   * Choose a nonland card name, then reveal the top card of your library. If that card has the chosen name, you draw three cards. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    int top = deck_ptr[player][0];
    uint32_t csvid;
    if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
    {
      csvid = 0x2ee2;
      if (top != -1 && !is_what(-1, top, 1))
        csvid = (uint16_t)cards_data[top].id;
    }
    else
    {
      test_definition_t test;
      new_default_test_definition(&test, 1, "");
      test.type_flag = 1;
      csvid = choose_a_card_test_ct("Choose a nonland card name.", &test, (color_test_t)-1,
                                    (type_t)~TYPE_LAND);
    }
    if (player == 1 || (trace_mode & 2) != 0)
      look_at_iid(player, card, get_internal_card_id_from_csv_id(csvid), "chooses:");
    if (top != -1)
    {
      reveal_card_iid(player, card, top);
      if ((uint16_t)cards_data[top].id == csvid)
        draw_cards(player, 3);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2302a30; original cards/unglued_unhinged.c:1547. */
static int num_letters_permanents_names_include(int player)
{
  int i;
  int csvid;
  unsigned char name_char;
  card_instance_t *instance;
  int letter_index;
  unsigned char *name_cursor;
  int distinct_letters;
  char letter[26];
  memset(letter, 0, sizeof(letter));
  if (active_cards_count[player] < 1)
  {
    letter[0] = '\0';
  }
  else
  {
    i = 0;
    do
    {
      instance = in_play(player, i);
      if (instance != NULL)
      {
        if (is_what(player, i, 0x47) != 0)
        {
          csvid = get_card_name(player, i);
          name_cursor = (unsigned char *)get_card_name_by_id(csvid);
          name_char = *name_cursor;
          while (name_char != 0)
          {
            name_cursor = name_cursor + 1;
            name_char = lc_unaccent_map[name_char];
            if ((name_char & 0xdf) == 0xc6)
            {
              letter[0] = '\x01';
              letter[4] = '\x01';
            }
            else if ((unsigned char)(name_char + 0x9f) < 0x1a)
            {
              letter[name_char - 0x61] = 1;
            }
            name_char = *name_cursor;
          }
        }
      }
      i = i + 1;
    } while (i < active_cards_count[player]);
  }
  distinct_letters = 0;
  for (letter_index = 0; letter_index < 26; ++letter_index)
  {
    distinct_letters += letter[letter_index] != 0;
  }
  return distinct_letters;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2304fc0; original cards/unglued_unhinged.c:1579. */
int card_now_i_know_my_abcs(int player, int card, event_t event)
{
  /* Now I Know My ABC's	|1|U|U
   * Enchantment
   * At the beginning of your upkeep, if you control permanents with names that include all twenty-six letters of the English alphabet, you win the game. */

  int letter_count_or_result;
  int distinct_letters;
  int ai_weight;
  if ((trigger_condition == TRIGGER_UPKEEP) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {
    if (((current_turn == player) &&
         (letter_count_or_result = num_letters_permanents_names_include(player),
          0x19 < letter_count_or_result)) &&
        (letter_count_or_result = upkeep_trigger(player, card, event), letter_count_or_result != 0))
    {
      lose_the_game(1 - player);
    }
  }
  else if ((event == EVENT_SHOULD_AI_PLAY) && (current_phase == 0x1f))
  {
    distinct_letters = num_letters_permanents_names_include(player);
    letter_count_or_result = distinct_letters + -10;
    if (0 < letter_count_or_result)
    {
      ai_weight = letter_count_or_result;
      if (player != 1)
      {
        ai_weight = 10 - distinct_letters;
      }
      ai_modifier = ai_modifier + letter_count_or_result * ai_weight;
    }
  }
  return global_enchantment(player, card, event);
}

#endif

#ifndef SHANDALAR
static int three_nonland_permanents_start_with_same_letter(void)
{
  int letter[26] = {0};
  for (int player = 0; player < 2; ++player)
    for (int card = 0; card < active_cards_count[player]; ++card)
      if (in_play(player, card) && !is_what(player, card, TYPE_LAND) && is_what(player, card, 0x47))
      {
        const unsigned char *name = (const unsigned char *)get_card_name_by_id(get_card_name(player, card));
        unsigned char c = lc_unaccent_map[*name];
        if ((c & 0xdf) == 0xc6)
        {
          if (++letter[0] > 2 || ++letter[4] > 2)
            return 1;
        }
        else if ((unsigned char)(c - 'a') < 26 && ++letter[c - 'a'] > 2)
          return 1;
      }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2305250; original cards/unglued_unhinged.c:1679. */
int card_booster_tutor(int player, int card, event_t event)
{
  /* Booster Tutor	|B
   * Instant
   * Open a sealed Magic booster pack, reveal the cards, and put one of those cards into your hand. (Remove that card from your deck before beginning a new game.) */

  if (event == EVENT_RESOLVE_SPELL)
  {
    EXPANSION_t expansion = (EXPANSION_t)-1;
    if (ai_is_speculating == 1 || (player == 1 && (trace_mode & 2) == 0))
    {
      expansion = EXPANSION_ARABIAN_NIGHTS;
      announce_expansion_choice(player, card, expansion);
    }
    else if ((trace_mode & 2) != 0 && player != 0)
    {
      TENTATIVE_wait_for_network_result(1, 26);
      expansion = dblword_628C10;
      announce_expansion_choice(player, card, expansion);
    }
    else
    {
      choose_packs(1, PACK_ALLOW_BOOSTER, &expansion, NULL);
      if ((trace_mode & 2) != 0)
      {
        dblword_628C10 = expansion;
        byte_628C0C = 26;
        TENTATIVE_send_network_result(0, 26);
      }
    }
    if (is_valid_pack(expansion, 1))
    {
      int capacity = pack_size(expansion, 1);
      int csvids[capacity];
      int iids[capacity];
      int num_csvids = make_pack(expansion, 1, csvids);
      for (int i = 0; i < num_csvids; ++i)
        iids[i] = get_internal_card_id_from_csv_id(csvids[i]);
      if (num_csvids == 0)
      {
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      if (ai_is_speculating != 1 && player == 1)
        show_deck(0, iids, num_csvids, "Booster contents", 0, gs_done_008b40e0);
      test_definition_t test;
      _Static_assert(sizeof(test) == (sizeof(void *) == 8 ? 224 : 216), "native Booster Tutor filter");
      default_test_definition(&test, 0xff);
      int selected;
      do
      {
        selected = select_card_from_zone(player, player, iids, num_csvids, 1, 1, -1, &test);
      } while (selected < 0 || selected >= num_csvids);
      int added = add_card_to_hand(player, iids[selected]);
      if (player == 1)
        reveal_card(1, card, 1, added);
    }
    kill_card(player, card, 2);
  }
  return basic_spell(player, card, event);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2305650; original cards/unglued_unhinged.c:1769. */
int card_the_fallen_apart(int player, int card, event_t event)
{
  /* The Fallen Apart	|2|B|B
   * Creature - Zombie 4/4
   * ~ enters the battlefield with two arms and two legs.
   * Whenever damage is dealt to ~, remove an arm or a leg from it.
   * ~ can't attack if it has no legs and can't block if it has no arms. */

  int32_t packed_limbs;
  bool has_arms;
  bool has_legs;
  int choice_or_result;
  card_instance_t *instance;
  const char *arm_plural;
  unsigned int leg_count;
  const char *leg_plural;
  const char *limb_description;
  int arm_count;
  char limbs[600];

  bool process_damage = true;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    instance->targets[5].card = 0x45d40202;
    if (is_humiliated(player, card, AL_TRIG) != 0)
    {
      return 0;
    }
  }
  else
  {
    choice_or_result = is_humiliated(player, card, AL_TRIG);
    if (choice_or_result != 0)
      process_damage = false;
  }
  if (process_damage)
  {
    choice_or_result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (choice_or_result != 0)
    {
      instance = get_card_instance(player, card);
      packed_limbs = instance->targets[5].card;
      if ((short)((unsigned int)packed_limbs >> 0x10) != 0x45d4)
      {
        return 0;
      }
      arm_count = (int)(char)((unsigned int)packed_limbs >> 8);
      has_arms = 0 < arm_count;
      leg_count = (unsigned int)(char)packed_limbs;
      has_legs = 0 < (int)leg_count;
      if ((has_arms) || (has_legs))
      {
        if (ai_is_speculating == 1)
        {
          limbs[0] = '\0';
        }
        else
        {
          if (leg_count == 1)
          {
            leg_plural = "";
          }
          else
          {
            leg_plural = "s";
          }
          if (arm_count == 1)
          {
            arm_plural = "";
          }
          else
          {
            arm_plural = "s";
          }
          sprintf(limbs, "%d arm%s, %d leg%s", arm_count, arm_plural, leg_count, leg_plural);
        }
        limb_description = limbs;
        choice_or_result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                       DLG_NO_CANCEL, DLG_HEADER(limb_description), "Remove arm",
                                       has_arms, ((1 < arm_count) ? 1000U : 1U),
                                       "Remove leg", (unsigned int)has_legs,
                                       ((1 < (int)leg_count) ? 1000U : 1U), 0, 0, 0, 0);
        if (choice_or_result == 0)
        {
          if (has_arms)
          {
            arm_count = arm_count + -1;
          }
        }
        else
        {
          leg_count = (leg_count - 1) + (unsigned int)!has_legs;
        }
        instance->targets[5].card = arm_count << 8 | leg_count | 0x45d40000;
      }
    }
  }
  if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((instance = in_play(player, card),
        instance != NULL &&
            (choice_or_result = is_humiliated(player, card, AL_STATIC), choice_or_result == 0))))
  {
    packed_limbs = instance->targets[5].card;
    if ((short)((unsigned int)packed_limbs >> 0x10) == 0x45d4)
    {
      if ((char)packed_limbs < '\x01')
      {
        cannot_attack(player, card, EVENT_ABILITIES);
      }
      if ('\0' < (char)((unsigned int)packed_limbs >> 8))
      {
        return 0;
      }
    }
    else
    {
      cannot_attack(player, card, EVENT_ABILITIES);
    }
    cannot_block(player, card, EVENT_ABILITIES);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2305a70; original cards/unglued_unhinged.c:1989. */
int card_punctuate(int player, int card, event_t event)
{
  /* Punctuate	|3|R
   * Instant
   * ~ deals damage to target creature equal to half the number of punctuation marks in that creature's text box. (The punctuation marks are ! ? , ; : - - ( ) / " " ' ' & .) */

  int result;
  card_instance_t *instance;
  const char *rules_cursor;
  const char *punct;
  target_definition_t td;
  char rules[3000];
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      get_rules_text(rules, instance->targets[0].player, instance->targets[0].card, 0);
      rules_cursor = rules;
      result = 0;
      while ((punct = strpbrk(rules_cursor, "!?,;:\226\227()/\"\224\223'\221\222&.")) != NULL)
      {
        rules_cursor = punct + 1;
        result = result + 1;
      }
      damage_creature(instance->targets[0].player, instance->targets[0].card, result / 2, player,
                      card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2305c30; original cards/unglued_unhinged.c:2072. */
int card_elvish_house_party(int player, int card, event_t event)
{
  /* Elvish House Party	|4|G|G
   * Creature - Elf Rogue 100/100
   * ~'s power and toughness are each equal to the current hour, using the twelve-hour system. */

  int result;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      result = localtime_hour();
      result = result % 0xc;
      if (result == 0)
      {
        result = 0xc;
      }
      set_characteristic_pt(result);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2305d40; original cards/unglued_unhinged.c:2189. */
int card_symbol_status(int player, int card, event_t event)
{
  /* Symbol Status	|2|G|G
   * Sorcery
   * Create a 1/1 colorless Expansion-Symbol creature token for each different expansion symbol among permanents you control. */

  int player_count_or_result;
  card_instance_t *instance;
  int id_or_result;
  const Rarities *rarity;
  char *expansion_cursor;
  int card_00;
  int any_expansion_symbols;
  int no_early_core_sets;
  if (event == EVENT_RESOLVE_SPELL)
  {
    char expansions[198] = {0};
    get_config_data(0xffffffff, 1, &any_expansion_symbols);
    if (any_expansion_symbols != 0)
    {
      get_config_data(3, 2, &no_early_core_sets);
      player_count_or_result = 0;
      do
      {
        card_00 = 0;
        if (0 < active_cards_count[player_count_or_result])
        {
          do
          {
            instance = in_play(player_count_or_result, card_00);
            if ((instance != NULL) &&
                (id_or_result = is_what(player_count_or_result, card_00, 0x47), id_or_result != 0))
            {
              id_or_result = get_id(player_count_or_result, card_00);
              rarity = get_rarity(id_or_result);
              if (rarity->default_expansion < 0xc6)
              {
                expansions[rarity->default_expansion] = '\x01';
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 < active_cards_count[player_count_or_result]);
        }
        player_count_or_result = player_count_or_result + 1;
      } while (player_count_or_result != 2);
      if (no_early_core_sets != 0)
      {
        expansions[0x12] = '\0';
        expansions[0xb] = '\0';
        expansions[7] = '\0';
        expansions[2] = 0;
        expansions[3] = 0;
        expansions[4] = 0;
      }
      expansion_cursor = expansions + 1;
      player_count_or_result = 0;
      do
      {
        player_count_or_result =
            (player_count_or_result + 1) - (unsigned int)(*expansion_cursor == '\0');
        expansion_cursor = expansion_cursor + 1;
      } while (expansion_cursor != expansions + 198);
      if (0 < player_count_or_result)
      {
        generate_tokens_by_id(player, card, CARD_ID_EXPANSION_SYMBOL, player_count_or_result);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  player_count_or_result = basic_spell(player, card, event);
  return player_count_or_result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2302d80; original cards/unglued_unhinged.c:2416. */
static int fx_togglodyte(int player, int card, event_t event)
{
  int result;
  const char *argument2;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
  {
    result = txtfx_name(player, card, event, 0x45d8);
    if (result != txtfx_not_found)
    {
      argument2 = "On";
      if (result == 0)
      {
        argument2 = "Off";
      }
      strcpy(set_legacy_effect_name_addr, argument2);
      return 0;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline card mechanics; original native callers contain the expansions. */
/* ManalinkEh 2022: 0x2306480; original cards/unglued_unhinged.c:2423. */
int card_togglodyte(int player, int card, event_t event)
{
  /* Togglodyte	|3
   * Artifact Creature - Golem 4/4
   * ~ enters the battlefield turned on.
   * Whenever a player casts a spell, toggle ~'s ON/OFF switch.
   * As long as ~ is turned off, it can't attack or block, and prevent all damage it would deal. */

  int32_t packed_toggle;
  card_instance_t *instance;
  unsigned int val;

  if (event == EVENT_RESOLVE_SPELL)
    make_txtfx(player, card, fx_togglodyte, CARD_ID_TOGGLODYTE, 1);
  if (new_specific_spell_played(player, card, event, 2, 2, NULL))
  {
    instance = get_card_instance(player, card);
    packed_toggle = instance->targets[5].player;
    if ((int16_t)(packed_toggle >> 16) == CARD_ID_TOGGLODYTE)
    {
      val = (unsigned int)((short)packed_toggle == 0);
    }
    else
    {
      val = 1;
    }
    make_txtfx(player, card, fx_togglodyte, CARD_ID_TOGGLODYTE, val);
  }
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      instance = get_card_instance(player, card);
      packed_toggle = instance->targets[5].player;
      if (((int16_t)(packed_toggle >> 16) == CARD_ID_TOGGLODYTE) && ((short)packed_toggle == 0))
      {
        cannot_attack(player, card, EVENT_ABILITIES);
        cannot_block(player, card, EVENT_ABILITIES);
      }
    }
  }
  instance = combat_damage_being_prevented(event);
  if (((instance != NULL) && (instance->damage_source_player == player)) &&
      (instance->damage_source_card == card))
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {
      instance = get_card_instance(player, card);
      packed_toggle = instance->targets[5].player;
      if (((int16_t)(packed_toggle >> 16) == CARD_ID_TOGGLODYTE) && ((short)packed_toggle == 0))
      {
        prevent_damage(affected_card_controller, affected_card, -1);
      }
    }
  }
  return 0;
}
#endif
