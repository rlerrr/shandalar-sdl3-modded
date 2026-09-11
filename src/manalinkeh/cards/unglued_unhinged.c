// If any of these are implemented, they need to be put in
// functions/rules_engine.c:check_deck_legality()::banned_cards_vintage[] and banned_cards_legacy[], and in
// cards/vanguard.c:is_banned_from_edh().

#include "manalink.h"

/***********
 * Unglued *
 ***********/

/* Charm School	|2|W
 * Enchantment
 * As ~ enters the battlefield, choose a color and balance ~ on your head.
 * Prevent all damage that would be dealt to you by sources of the chosen color.
 * When ~ falls off your head, sacrifice ~. */

/* The Cheese Stands Alone	|4|W|W
 * Enchantment
 * When you control no permanents other than ~ and have no cards in hand, you win the game. */

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

/* Once More with Feeling	|W|W|W|W
 * Sorcery
 * Exile all permanents and all cards from all graveyards. Each player shuffles their hand into their library, then draws seven cards. Each player's life total becomes 10. Exile ~.
 * DCI ruling - A deck can have only one card named ~. */

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

/* Fowl Play	|2|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature is a Chicken with base power and toughness 1/1 and loses all abilities. */

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

/* Jumbo Imp	|2|B
 * Creature - Imp 0/0
 * Flying
 * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of +1/+1 counters on it equal to the result.
 * At the beginning of your upkeep, roll a six-sided die and put a number of +1/+1 counters on ~ equal to the result.
 * At the beginning of your end step, roll a six-sided die and remove a number of +1/+1 counters from ~ equal to the result. */

/* Organ Harvest	|B
 * Sorcery
 * Your team may sacrifice any number of creatures. For each creature sacrificed this way, you add |B|B. */

/* Ow	|B
 * Enchantment
 * Whenever a creature deals damage to a player, that player may say "~" once for each permanent named ~. If the player doesn't, ~ deals 1 damage to them. */

/* Poultrygeist	|2|B
 * Creature - Chicken 1/1
 * Flying
 * Whenever a creature dies, you may roll a six-sided die. If you roll a 1, sacrifice ~. Otherwise, put a +1/+1 counter on ~. */

/* Temp of the Damned	|2|B
 * Creature - Zombie 3/3
 * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of funk counters on it equal to the result.
 * At the beginning of your upkeep, remove a funk counter from ~. If you can't, sacrifice it. */

/* Volrath's Motion Sensor	|B
 * Enchantment - Aura
 * Enchant player
 * As ~ enters the battlefield, choose one of enchanted player's hands. That player balances ~ on the back of that hand.
 * When ~ falls off the chosen hand, sacrifice ~ and enchanted player loses 3 life. */

/* Burning Cinder Fury of Crimson Chaos Fire	|3|R
 * Enchantment
 * Whenever any player taps a permanent, that player choose one of their opponents. The chosen player gains control of that permanent at the beginning of the next end step.
 * At the beginning of each player's end step, if that player didn't tap any nonland permanents that turn, ~ deals 3 damage to that player. */

/* Chicken Egg	|1|R
 * Creature - Egg 0/1
 * At the beginning of your upkeep, roll a six-sided die. If you roll a 6, sacrifice ~ and create a 4/4 |Sred Giant Chicken creature token. */

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

int card_goblin_tutor(int player, int card, event_t event){
	/* Goblin Tutor	|R	0x20013fd
	 * Instant
	 * Roll a six-sided die. If you roll a 1, ~ has no effect. Otherwise, search your library for the indicated card, reveal it, put it into your hand, then shuffle your library.
	 * 2 - A card named ~
	 * 3 - An enchantment card
	 * 4 - An artifact card
	 * 5 - A creature card
	 * 6 - An instant or sorcery card */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		int roll = internal_rand(6) + 1;
		if( roll == 1 ){
			do_dialog(player, player, card, -1, -1, "Rolled 1: no effect", 1);
		}
		else if( roll == 2 ){
			do_dialog(player, player, card, -1, -1, "Rolled 2: search for Goblin Tutor", 1);
			new_default_test_definition(&test, TYPE_ANY, "Select a Goblin Tutor card.");
			test.id = CARD_ID_GOBLIN_TUTOR;

		}
		else if( roll == 3 ){
			do_dialog(player, player, card, -1, -1, "Rolled 3: find an enchantment", 1);
			new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		}
		else if( roll == 4 ){
			do_dialog(player, player, card, -1, -1, "Rolled 4: find an artifact", 1);
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact card.");
		}
		else if( roll == 5 ){
			do_dialog(player, player, card, -1, -1, "Rolled 5: find a creature", 1);
			new_default_test_definition(&test, TYPE_CREATURE, "Select an creature card.");
		}
		else if( roll == 6 ){
			do_dialog(player, player, card, -1, -1, "Rolled 6: find an instant or sorcery", 1);
			new_default_test_definition(&test, TYPE_SPELL, "Select an creature card.");
		}

		if( roll != 1 ){
			int ai_mode = roll == 2 ? AI_FIRST_FOUND : AI_MAX_VALUE;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, ai_mode, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

/* Hurloon Wrangler	|2|R
 * Creature - Minotaur 2/2
 * Denimwalk (This creature can't be blocked as long as defending player is wearing denim.) */

/* Jalum Grifter	|3|R|R
 * Legendary Creature - Devil 3/5
 * |1|R, |T: Shuffle ~ and two lands you control face down. Target opponent chooses one of those cards. Turn the cards face up. If they chose ~, sacrifice it. Otherwise, destroy target permanent. */

/* Krazy Kow	|3|R
 * Creature - Cow 3/3
 * At the beginning of your upkeep, roll a six-sided die. If you a roll a 1, sacrifice ~ and it deals 3 damage to each creature and each player. */

/* Landfill	|4|R
 * Sorcery
 * Choose a land type. Exile all lands you control of that type. Drop those cards, one at a time, onto the playing area from a height of at least one foot. Destroy each permanent completely covered by those cards. Then return the cards you dropped to the battlefield tapped. */

/* Ricochet	|R
 * Enchantment
 * Whenever a player casts a spell that targets a single player, each player rolls a six-sided die. Change the target of that spell to the player with the lowest result. Reroll to break ties, if necessary. */

/* Spark Fiend	|4|R
 * Creature - Beast 5/6
 * When ~ enters the battlefield, roll two six-sided dice. If you rolled 2, 3, or 12, sacrifice ~. If you rolled 7 or 11, don't roll dice for ~ during any of your following upkeeps. If you rolled any other total, note that total.
 * At the beginning of your upkeep, roll two six-sided dice. If you rolled 7, sacrifice ~. If you roll the noted total, don't roll dice for ~ during any of your following upkeeps. Otherwise, do nothing. */

/* Strategy, Schmategy	|1|R
 * Sorcery
 * Roll a six-sided die. ~ has the indicated effect.
 * 1 - Do nothing.
 * 2 - Destroy all artifacts.
 * 3 - Destroy all lands.
 * 4 - ~ deals 3 damage to each creature and each player.
 * 5 - Each player discards their hand and draws seven cards.
 * 6 - Repeat this process two more times. */

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

/* Elvish Impersonators	|3|G
 * Creature - Elves 100/100
 * As ~ enters the battlefield, roll a six-sided die twice. Its base power becomes the first result and its base toughness becomes the second result. */

/* Flock of Rabid Sheep	|X|G|G
 * Sorcery
 * Flip X coins. For each flip you win, create a 2/2 |Sgreen Sheep creature token named Rabid Sheep. */

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

/* Growth Spurt	|1|G
 * Instant
 * Roll a six-sided die. Target creature gets +X/+X until end of turn, where X is the result. */

/* Gus	|2|G
 * Creature - Gus 2/2
 * ~ enters the battlefield with a +1/+1 counter on it for each Magic game you have lost to one of your opponents since you last won a game against them. */

/* Hungry Hungry Heifer	|2|G
 * Creature - Cow 3/3
 * At the beginning of your upkeep, you may remove a counter from a permanent you control. If you don't, sacrifice ~. */

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

/* Team Spirit	|2|G
 * Instant
 * Creatures target player's team controls get +1/+1 until end of turn. */

/* Timmy, Power Gamer	|2|G|G
 * Legendary Creature - Human Gamer 1/1
 * |4: You may put a creature card from your hand onto the battlefield. */

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

	if( event == EVENT_ACTIVATE ){
		tap_card(player, card);
		int roll = internal_rand(6);
		if( roll == 0 ){
			lose_life(player, 5);
			kill_card(player, card, KILL_SACRIFICE);
			spell_fizzled = 1;
			return 0;
		}
		else if( roll == 1 ){
			produce_mana(player, COLOR_WHITE, 1);
		}
		else if( roll == 2 ){
			produce_mana(player, COLOR_BLUE, 1);
		}
		else if( roll == 3 ){
			produce_mana(player, COLOR_BLACK, 1);
		}
		else if( roll == 4 ){
			produce_mana(player, COLOR_RED, 1);
		}
		else if( roll == 5 ){
			produce_mana(player, COLOR_GREEN, 1);
		}
		spell_fizzled = 1;
		return 0;
	}
	int result = mana_producer(player, card, event);
	return result;
}

/* Jester's Sombrero	|2
 * Artifact
 * |2, |T, Sacrifice ~: Look at target player's sideboard, choose three cards from it, and remove those cards from the match. */

/* Mirror Mirror	|7
 * Artifact
 * ~ enters the battlefield tapped.
 * |7, |T, Sacrifice ~: Choose target player. At the beginning of the next end step, exchange life totals with that player, exchange control of all permanents you and that player control, and exchange cards in your hands, cards in your libraries, and cards in your graveyards. */

/* Paper Tiger	|4
 * Artifact Creature - Cat 4/3
 * Creatures named Rock Lobster can't attack or block. */

/* Rock Lobster	|4
 * Artifact Creature - Lobster 4/3
 * Creatures named Scissors Lizard can't attack or block. */

/* Scissors Lizard	|4
 * Artifact Creature - Lizard 4/3
 * Creatures named Paper Tiger can't attack or block. */

/* Spatula of the Ages	|4
 * Artifact
 * |4, |T, Sacrifice ~: You may put a silver-bordered permanent card from your hand onto the battlefield. */

/* Urza's Contact Lenses	|0
 * Artifact
 * ~ enters the battlefield tapped.
 * ~ doesn't untap during your untap step.
 * As long as ~ is untapped, all players play with their hands revealed.
 * Clap your hands twice: You may tap or untap ~. */

/* Urza's Science Fair Project	|6
 * Artifact Creature - Construct 4/4
 * |2: Roll a six-sided die. ~ gets the indicated result.
 * 1 - It gets -2/-2 until end of turn.
 * 2 - Prevent all combat damage it would deal this turn.
 * 3 - It gains vigilance until end of turn.
 * 4 - It gains first strike until end of turn.
 * 5 - It gains flying until end of turn.
 * 6 - It gets +2/+2 until end of turn. */

// Plains, Island, Swamp, Mountain, Forest.

/************
 * Unhinged *
 ************/


/* Atinlay Igpay	|5|W
 * Eaturecray - Igpay
 * Oubleday ikestray
 * Enwhay ouyay eakspay ay onnay-igpay-atinlay ordway, acrificesay ~. */

/* AWOL	|2|W
 * Instant
 * Exile target attacking creature. Then remove it from the game. Then put it into the absolutely-removed-from-the-freaking-game-forever zone. */

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

/* Wordmail	|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 for each word in its name. */

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

/* Double Header	|3|U|U
 * Creature - Drake 2/3
 * Flying
 * When ~ enters the battlefield, you may return target permanent with a two-word name to its owner's hand. */

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

/* Johnny, Combo Player	|2|U|U
 * Legendary Creature - Human Gamer 1/1
 * |4: Search your library for a card, put that card into your hand, then shuffle your library. */

/* Loose Lips	|U
 * Enchantment - Aura
 * Enchant creature
 * As ~ enters the battlefield, choose a sentence with eight or fewer words.
 * Enchanted creature has flying.
 * Whenever enchanted creature deals damage to an opponent, that player may say the chosen sentence. If they don't, you draw two cards. */

/* Magical Hacker	|1|U
 * Creature - Human Gamer 1/2
 * |U: Change the text of target spell or permanent by replacing all instances of + with -, and vice versa, until end of turn. */

/* Mise	|U
 * Instant
 * Choose a nonland card name, then reveal the top card of your library. If that card has the chosen name, you draw three cards. */

/* Moniker Mage	|2|U
 * Creature - Human Wizard 2/2
 * |U, Say your middle name: ~ gains shroud until end of turn.
 * |U, Say an opponent's middle name: ~ gains flying until end of turn. */

/* Mouth to Mouth	|3|U
 * Sorcery
 * Choose target opponent and target creature they control. You and that player have a breath-holding contest. If you win, you gain control of that creature. */

/* Now I Know My ABC's	|1|U|U
 * Enchantment
 * At the beginning of your upkeep, if you control permanents with names that include all twenty-six letters of the English alphabet, you win the game. */

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

/* Bloodletter	|2|B
 * Creature - Zombie 2/3
 * When the names of three or more nonland permanents begin with the same letter, sacrifice ~. If you do, it deals 2 damage to each creature and each player. */

/* Booster Tutor	|B
 * Instant
 * Open a sealed Magic booster pack, reveal the cards, and put one of those cards into your hand. (Remove that card from your deck before beginning a new game.) */

static const char* target_has_reminder_text(int who_chooses, int player, int card)
{
  card_ptr_t* cp = cards_ptr[get_id(player, card)];
  return cp->rules_text && strchr(cp->rules_text, '(') ? NULL : "no reminder text";
}

int card_duh(int player, int card, event_t event)
{
  /* Duh	|B
   * Instant
   * Destroy target creature with reminder text. (Reminder text is any italicized text in parentheses that explains rules you already know.) */

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (int32_t)target_has_reminder_text;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET|GS_LITERAL_PROMPT, &td, "Select target creature with reminder text.", 1, NULL);
}

/* Enter the Dungeon	|B|B
 * Sorcery
 * Players play a Magic subgame under the table, starting at 5 life and using their libraries as their decks. The winner searches their library for two cards, puts those cards into their hand, then shuffles their library. */

/* Eye to Eye	|2|B
 * Instant
 * You and target creature's controller have a staring contest. If you win, destroy that creature. */

/* The Fallen Apart	|2|B|B
 * Creature - Zombie 4/4
 * ~ enters the battlefield with two arms and two legs.
 * Whenever damage is dealt to ~, remove an arm or a leg from it.
 * ~ can't attack if it has no legs and can't block if it has no arms. */

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

/* Goblin Secret Agent	|2|R
 * Creature - Goblin Rogue 2/2
 * First strike
 * At the beginning of your upkeep, reveal a card from your hand at random. */

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

/* Punctuate	|3|R
 * Instant
 * ~ deals damage to target creature equal to half the number of punctuation marks in that creature's text box. (The punctuation marks are ! ? , ; : - - ( ) / " " ' ' & .) */

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

/* Elvish House Party	|4|G|G
 * Creature - Elf Rogue 100/100
 * ~'s power and toughness are each equal to the current hour, using the twelve-hour system. */

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

/* Shoe Tree	|4|G
 * Creature - Treefolk 3/3
 * ~ enters the battlefield with up to two shoe counters on it. Use your shoes as counters.
 * ~ gets +1/+1 for each shoe counter on it. */

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

/* Symbol Status	|2|G|G
 * Sorcery
 * Create a 1/1 colorless Expansion-Symbol creature token for each different expansion symbol among permanents you control. */

// No Expansion-Symbol token.  Sorry.

/* Uktabi Kong	|5|G|G|G
 * Creature - Ape 8/8
 * Trample
 * When ~ enters the battlefield, destroy all artifacts.
 * Tap two untapped Apes you control: Create a 1/1 |Sgreen Ape creature token. */

/* "Ach! Hans, Run!"	|2|R|R|G|G
 * Enchantment
 * At the beginning of your upkeep, you may say "Ach! Hans, run! It's the . . ." and the name of a creature card. If you do, search your library for a card with that name, put it onto the battlefield, then shuffle your library. That creature gains haste. Exile it at the beginning of the next end step. */

/* Ass Whuppin'	|1|W|B
 * Sorcery
 * Destroy target silver-bordered permanent in any game you can see from your seat. */

/* Meddling Kids	|2|W|U
 * Creature - Human Child 2/3
 * As ~ enters the battlefield, choose a word with four or more letters.
 * Spells with the chosen word in their text box can't be cast. */

/* Rare-B-Gone	|2|B|R
 * Sorcery
 * Each player sacrifices all permanents that are rare or mythic rare, then each player reveals their hand and discards all cards that are rare or mythic rare. */

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

/* Mana Screw	|1
 * Artifact
 * |1: Flip a coin. If you win the flip, add |C|C. Activate this ability only any time you could cast an instant. */

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

/* Togglodyte	|3
 * Artifact Creature - Golem 4/4
 * ~ enters the battlefield turned on.
 * Whenever a player casts a spell, toggle ~'s ON/OFF switch.
 * As long as ~ is turned off, it can't attack or block, and prevent all damage it would deal. */

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
