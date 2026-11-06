// If any of these are implemented, they need to be put in
// functions/rules_engine.c:check_deck_legality()::banned_cards_vintage[] and banned_cards_legacy[], and in
// cards/vanguard.c:is_banned_from_edh().

#include "manalink.h"

/*********
 * White *
 *********/

/* Adorable // Kitten	|W
 * Host Creature - Cat 1/1
 * When this creature enters the battlefield,
 * //
 * roll a six-sided die. You gain life equal to the result. */

/* Aerial Toastmaster	|3|W
 * Artifact Creature - Cyborg Rigger 3/2
 * Flying
 * |3|W, Sacrifice another artifact: ~ assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Amateur Auteur	|1|W
 * Creature - Human 2/2
 * Sacrifice ~: Destroy target enchantment. */

/* By Gnome Means	|2|W
 * Enchantment
 * |1|W, Remove a counter from a permanent you control: Create a 1/1 colorless Gnome artifact creature token.
 * |1|W, Sacrifice an artifact: Choose any kind of counter a printed card refers to, then put one of that counter on target permanent. */

/* Chivalrous Chevalier	|4|W
 * Artifact Creature - Cyborg Knight 3/3
 * Flying
 * When ~ enters the battlefield, return a creature you control to its owner's hand unless you compliment an opponent. */

/* Do-It-Yourself Seraph	|4|W|W
 * Artifact Creature - Cyborg Angel 4/4
 * Flying
 * Whenever ~ attacks, you may search your library for an artifact card, exile it, then shuffle your library.
 * ~ has the text box of each card exiled with ~ in addition to its own. */

/* Gimme Five	|W
 * Instant
 * You gain 1 life for each person who high-fives you in the next thirty seconds. Each player in a silver-bordered game who high-fives you gains 1 life. (Offer high fives. Don't hit people.) */

/* Half-Kitten, Half-	""
 * Creature - Cat 1/2
 * Whenever you're dealt damage,
 * Augment |2|W (|2|W, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Humming-	""
 * Creature - Bird 2/3
 * Flying
 * Whenever you attack with two or more creatures,
 * Augment |3|W (|3|W, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Jackknight	|1|W
 * Artifact Creature - Cyborg Knight 1/1
 * Whenever another artifact enters the battlefield under your control, put a +1/+1 counter on ~. If that artifact is a Contraption, ~ gains lifelink until end of turn. */

/* Knight of the Kitchen Sink A	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from black borders (Nothing with a black border can block, target, deal damage to, or attach to this creature.) */

/* Knight of the Kitchen Sink B	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from even collector numbers (Nothing with an even collector number can block, target, deal damage to, or attach to this creature.) */

/* Knight of the Kitchen Sink C	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from loose lips (Nothing with an open mouth in its artwork can block, target, deal damage to, or attach to this creature.) */

/* Knight of the Kitchen Sink D	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from odd collector numbers (Nothing with an odd collector number can block, target, deal damage to, or attach to this creature.) */

/* Knight of the Kitchen Sink E	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from two-word names (Nothing with exactly two words in its name can block, target, deal damage to, or attach to this creature. Hyphenated words are one word.) */

/* Knight of the Kitchen Sink F	|W|W
 * Artifact Creature - Cyborg Knight 2/2
 * First strike, protection from watermarks (Nothing with a watermark can block, target, deal damage to, or attach to this creature.) */

/* Midlife Upgrade	|3|W
 * Sorcery
 * As an additional cost to cast this spell, sacrifice X Contraptions.
 * Assemble X plus one Contraptions. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Ordinary // Pony	|2|W
 * Host Creature - Horse 2/3
 * When this creature enters the battlefield,
 * //
 * you may exile target non-Horse creature you control that wasn't put onto the battlefield with this ability this turn, then return it to the battlefield under its owner's control. */

/* Rhino-	""
 * Creature - Rhino 1/4
 * Whenever this creature blocks,
 * Augment |3|W (|3|W, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Riveting Rigger	|2|W
 * Artifact Creature - Cyborg Rigger 2/2
 * When ~ enters the battlefield, you may sacrifice another artifact. If you do, put two +1/+1 counters on ~ and it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Rules Lawyer	|3|W|W
 * Artifact Creature - Cyborg Advisor 1/1
 * State-based actions don't apply to you or other permanents you control. (You don't lose the game due to having 0 or less life or drawing from an empty library. Your creatures aren't destroyed due to damage or deathtouch and aren't put into a graveyard due to having 0 or less toughness. Your planeswalkers aren't put into a graveyard if they have 0 loyalty. You don't put a legendary permanent into a graveyard if you control two with the same name. Counters aren't removed from your permanents due to game rules. Permanents you control attached or combined illegally remain on the battlefield. For complete rules and regulations, see rule 704.) */

/* Sacrifice Play	|2|W
 * Instant
 * A person outside the game chooses an attacking or blocking creature target opponent controls. That player sacrifices that creature. */

/* Shaggy // Camel	|3|W
 * Host Creature - Camel 3/3
 * When this creature enters the battlefield,
 * //
 * creatures you control get +1/+1 until end of turn. */

/* Side Quest	|W
 * Sorcery
 * Target player in a silver-bordered game you can see from your seat gains control of target creature you control until your next turn. At the beginning of your next upkeep, put two +1/+1 counters on that creature. (A creature is on the battlefield of only the game its controller is playing.) */

/* Success!	|W
 * Instant
 * Target creature gets +2/+2 until end of turn. If it's a host or has augment, it gains lifelink until end of turn. */

/* Teacher's Pet	|1|W
 * Creature - Cat Bird Scientist 2/1
 * |2|W, Sacrifice ~: Search your library for a card with augment, combine it with target host you control, then shuffle your library. */

/********
 * Blue *
 ********/

/* Animate Library	|4|U|U
 * Enchantment - Aura
 * Enchant your library
 * Enchanted library is an artifact creature on the battlefield with power and toughness each equal to the number of cards in it. It's still a library.
 * If enchanted library would leave the battlefield, exile ~ instead. */

/* Blurry Beeble	|U
 * Artifact Creature - Cyborg Beeble 1/1
 * Blurry (This creature can be blocked only if defending player was wearing glasses as it was cast.)
 * Whenever ~ deals combat damage to a player, you may draw a card. If you do, discard a card. */

/* Chipper Chopper	|3|U
 * Artifact Creature - Cyborg Rigger 1/1
 * Flying
 * When ~ enters the battlefield, you may sacrifice another artifact. If you do, put two +1/+1 counters on ~ and it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Clocknapper	|3|U|U
 * Creature - Human Spy 2/2
 * When ~ enters the battlefield, choose beginning phase, precombat main phase, combat phase, postcombat main phase, or ending phase. Steal that phase from target player during their next turn. (That phase occurs as though it's your turn instead.) */

/* Crafty // Octopus	|2|U
 * Host Creature - Octopus Rigger 1/3
 * When this creature enters the battlefield,
 * //
 * this creature assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Defective Detective	|2|U
 * Creature - Human Spy 2/1
 * ~ can't be blocked.
 * When ~ enters the battlefield, a person outside the game looks at target opponent's hand and chooses a card from it. That player reveals that card. */

/* Five-Finger Discount	|4|U|U
 * Sorcery
 * Put target nonland permanent into your hand. You may spend mana as though it were mana of any color the next time you cast that card. */

/* Graveyard Busybody	|4|U|U
 * Creature - Human Spy 100/100
 * All graveyards are also your graveyards.
 * ~'s power and toughness are each equal to the number of cards with flavor text in your graveyards. */

/* Half-Shark, Half-	""
 * Creature - Fish 3/3
 * At the beginning of your upkeep,
 * Augment |5|U (|5|U, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Incite Insight	|X|U|U
 * Sorcery
 * Assemble X Contraptions. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Kindly Cognician	|1|U
 * Artifact Creature - Cyborg Artificer 1/3
 * Spells you cast that refer to artifacts or Contraptions in their rules text cost |1 less to cast. */

/* Magic Word	|2|U
 * Enchantment - Aura
 * Enchant creature
 * As ~ enters the battlefield, choose a word.
 * Whisper the chosen word: Tap enchanted creature. */

/* Mer // Man	|4|U
 * Host Creature - Human Fish 3/3
 * When this creature enters the battlefield,
 * //
 * you may draw a card. */

/* More or Less	|U
 * Instant
 * Add or subtract 1 or one from a number or number word on target spell or permanent until end of turn. */

/* Novellamental	|1|U
 * Creature - Elemental 2/1
 * Flying
 * ~ can block only creatures with flying. */

/* Numbing // Jellyfish	|3|U
 * Host Creature - Jellyfish 2/3
 * When this creature enters the battlefield,
 * //
 * roll a six-sided die. Target player puts the top X cards of their library into their graveyard, where X is the result. */

/* S.N.E.A.K. Dispatcher	|1|U
 * Creature - Human Spy 2/1
 * |2|U, |T: Look at the top card of target player's library. If it has an Agents of S.N.E.A.K. watermark, you may reveal it and put it into your hand. Otherwise, put it on the top or bottom of its owner's library. */

/* Socketed Sprocketer	|U
 * Artifact Creature - Cyborg Knight 1/1
 * |T: Uninstall all results from ~, then roll a six-sided die. Install the result on ~. (Put the die on this card.)
 * You may uninstall a result from ~ to use it for a die you rolled.
 * Uninstall a 6 from ~: Draw a card. */

/* Spell Suck	|2|U|U
 * Instant
 * Counter target spell, then assemble a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Spy Eye	|2|U|U
 * Creature - Eye Spy 1/3
 * Flying
 * Whenever ~ deals combat damage to a player, you may draw a card from that player's library. */

/* Suspicious Nanny	|4|U
 * Creature - Human Spy Rigger 3/4
 * Whenever ~ deals combat damage to a player, it reassembles target Contraption that player controls. (Gain control of it and move it onto one of your sprockets.) */

/* Very Cryptic Command A	|1|U|U|U
 * Instant
 * Choose two -
 * * Switch target creature's power and toughness until end of turn.
 * * Target creature can't be blocked this turn.
 * * Draw a card. If that card's art is by Wayne England, you may reveal it and draw another card.
 * * Assemble a Contraption. */

/* Very Cryptic Command C	|1|U|U|U
 * Instant
 * Choose two -
 * * Draw a card from an opponent's library.
 * * Copy target instant or sorcery spell. You may choose new targets for the copy.
 * * Until end of turn, target creature loses all abilities and becomes a |Sblue Frog with base power and toughness 1/1.
 * * Create a 1/1 colorless Gnome artifact creature token. */

/* Very Cryptic Command D	|1|U|U|U
 * Instant
 * Choose two -
 * * Return target permanent to its controller's hand.
 * * Draw two cards, then discard a card.
 * * Change the target of target spell with a single target.
 * * Turn over target nontoken creature. */

/* Very Cryptic Command F	|1|U|U|U
 * Instant
 * Choose two -
 * * Scry 3.
 * * Create a 2/2 |Sblack Rogue creature token with menace.
 * * Add or subtract 1 or one from a number or number word on target spell or permanent until end of turn.
 * * Return all artifacts target player controls to their owner's hand. */

/* Wall of Fortune	|1|U
 * Artifact Creature - Wall 0/4
 * Defender
 * You may tap an untapped Wall you control to have any player reroll a die that player rolled. */

/*********
 * Black *
 ********/

/* Big Boa // Constrictor	|3|B
 * Host Creature - Snake 1/2
 * When this creature enters the battlefield,
 * //
 * roll a six-sided die. Target opponent loses life equal to the result. */

/* capital offense	|2|B|B
 * instant
 * target creature gets -x/-x until end of turn, where x is the number of times a capital letter appears in its rules text. (ignore reminder text and flavor text.) */

/* Dirty // Rat	|1|B
 * Host Creature - Rat 1/1
 * When this creature enters the battlefield,
 * //
 * target opponent discards a card. */

/* Extremely Slow Zombie	|1|B
 * Creature - Zombie 3/3
 * Last strike (This creature deals combat damage after creatures without last strike.) */

/* Finders, Keepers	|5|B
 * Sorcery
 * Destroy target creature, then assemble a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Hangman	|B
 * Creature - Human Villain 1/1
 * As ~ enters the battlefield, secretly note a word with six to eight letters.
 * |1: Target player who doesn't control ~ guesses the noted word or an unguessed letter in that word. If they guess wrong, put a +1/+1 counter on ~. Any player may activate this ability.
 * When a player guesses the noted word or all of its letters, sacrifice ~. */

/* Hazmat Suit (Used)	|3|B
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has menace.
 * Whenever a player's skin or fingernail touches enchanted creature, that player loses 2 life. */

/* Hoisted Hireling	|2|B
 * Creature - Zombie 2/1
 * ~ has flying as long as it's being held above the battlefield. */

/* Masterful Ninja	|2|B
 * Creature - Troll Ninja 1/1
 * Haste
 * Reveal ~ from your hand: ~ is on the battlefield and in your hand until end of turn.
 * |1|B: ~ gets +1/+1 until end of turn. */

/* Ninja	""
 * Creature - Ninja 1/0
 * You may activate ~'s augment ability any time you could cast an instant.
 * Whenever this creature deals combat damage to a player,
 * Augment |2|B (|2|B, Reveal this card from your hand: Combine it with target host. Augment only as-oh, nevermind.) */

/* Over My Dead Bodies	|4|B|B
 * Enchantment
 * Creature cards in graveyards can attack and block as though they were on the battlefield, can block or be blocked only by creature cards in graveyards, are Zombies in addition to their other types, and have undeathtouch. (If they would deal damage to a creature card, exile that creature card instead.)
 * Creature cards in your graveyard have haste. */

/* Overt Operative	|3|B
 * Creature - Human Ninja Rigger 2/3
 * Menace
 * Whenever ~ deals combat damage to a player, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Skull Saucer	|4|B|B
 * Creature - Zombie Head 4/1
 * Flying
 * When ~ enters the battlefield, destroy target creature and put your head on the table. Sacrifice ~ when your head stops touching the table. */

/* Sly Spy B	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, you may destroy target creature facing left in its art. (Creatures without faces don't face anywhere.) */

/* Sly Spy C	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, that player loses a finger until Sly Spy leaves the battlefield. (The finger is chosen by its owner and can't roll dice or touch cards.) */

/* Sly Spy D	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, you may destroy target creature facing right in its art. (Creatures without faces don't face anywhere.) */

/* Sly Spy E	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, that player reveals the top card of their library. Put that card into your hand and you lose life equal to its converted mana cost. */

/* Snickering Squirrel	|B
 * Creature - Squirrel Advisor 1/1
 * You may tap ~ to increase the result of a die any player rolled by 1. */

/* Steady-Handed Mook	|2|B
 * Creature - Human Rigger 1/1
 * Deathtouch
 * When ~ enters the battlefield, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Stinging // Scorpion	|4|B
 * Host Creature - Scorpion 3/2
 * When this creature enters the battlefield,
 * //
 * target creature an opponent controls gets -1/-1 until end of turn. */

/* Subcontract	|B
 * Sorcery
 * A person outside the game looks at target opponent's hand and chooses a nonland card from it. That player discards that card. */

/* Summon the Pack	|7|B
 * Sorcery
 * Open a sealed Magic booster pack, reveal the cards, and put all creature cards revealed this way onto the battlefield under your control. They're Zombies in addition to their other types. (Remove those cards from your deck before beginning a new game.) */

/* Zombified	""
 * Creature - Zombie 2/2
 * |4|B: Combine ~ from your graveyard with target host.
 * |2|B, Exile a creature card from your graveyard:
 * Augment |4|B (|4|B, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/*******
 * Red *
 *******/

/* The Big Idea	|4|R|R
 * Legendary Creature - Brainiac Villain 4/4
 * |2|BR|BR, |T: Roll a six-sided die. Create a number of 1/1 |Sred Brainiac creature tokens equal to the result.
 * Tap three untapped Brainiacs you control: The next time you would roll a six-sided die, instead roll two six-sided dice and use the total of those results. */

/* Bumbling // Pangolin	|3|R
 * Host Creature - Pangolin Beast 2/2
 * When this creature enters the battlefield,
 * //
 * you may destroy target artifact. */

/* Common // Iguana	|1|R
 * Host Creature - Lizard 1/3
 * When this creature enters the battlefield,
 * //
 * you may discard a card. If you do, draw a card. */

/* The Countdown Is at One	|3|R|R
 * Sorcery
 * Players play a Magic subgame, starting at 1 life and using their libraries as their decks. For the rest of the main game, if a source would deal damage to a player who didn't win the subgame, it deals double that damage to that player instead. */

/* Feisty // Stegosaurus	|4|R
 * Host Creature - Dinosaur 2/1
 * When this creature enters the battlefield,
 * //
 * roll a six-sided die. This creature deals damage equal to the result to target creature an opponent controls. */

/* Garbage Elemental A	|4|R
 * Creature - Elemental 2/4
 * Frenzy 2 (Whenever this creature attacks and isn't blocked, it gets +2/+0 until end of turn.)
 * Garbage Elemental can't be blocked by wordy creatures. (A creature is wordy if it has four or more lines of rules text.) */

/* Garbage Elemental B	|4|R
 * Creature - Elemental Rigger 3/1
 * When Garbage Elemental enters the battlefield, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.)
 * Undying (When this creature dies, if it had no +1/+1 counters on it, return it to the battlefield under its owner's control with a +1/+1 counter on it.) */

/* Garbage Elemental E	|4|R
 * Creature - Elemental 4/3
 * Unleash (You may have this creature enter the battlefield with a +1/+1 counter on it. It can't block as long as it has a +1/+1 counter on it.)
 * Each creature you control with any kind of counter on it has art menace. (They can't be blocked except by creatures with two or more visible figures in their art.) */

/* Garbage Elemental F	|4|R
 * Creature - Elemental 6/5
 * Last strike (This creature deals combat damage after creatures without last strike.)
 * Battalion - Whenever Garbage Elemental and at least two other creatures attack, target creature can't block this turn. */

/* Goblin Haberdasher	|2|R
 * Creature - Goblin Hatificer 2/2
 * Menace (This creature can't be blocked except by two or more creatures.)
 * Other creatures you control wearing hats in their art have menace. */

/* Half-Orc, Half-	""
 * Creature - Orc Warrior 3/1
 * Trample
 * At the beginning of each end step, if an opponent was dealt damage this turn,
 * Augment |1|R|R (|1|R|R, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Hammer Jammer	|3|R
 * Creature - Goblin Warrior 0/0
 * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of +1/+1 counters on it equal to the result.
 * Whenever you roll a die, remove all +1/+1 counters from ~, then put a number of +1/+1 counters on it equal to the result. */

/* Infinity Elemental	|4|R|R|R
 * Creature - Elemental 100/5
 * (This creature has INFINITE POWER.) */

/* It That Gets Left Hanging	|5|R
 * Creature - Elemental Horror 5/4
 * When ~ enters the battlefield, ask a person outside the game to high-five you. If they won't, ~ gains haste until end of turn. */

/* Just Desserts	|1|R
 * Instant
 * ~ deals pi damage to target creature. (Pi is the ratio of a circle's circumference to its diameter. (It's a smidgen more than 3.)) */

/* Party Crasher	|4|R
 * Creature - Goblin Berserker 3/3
 * Haste
 * You can attack with ~ once each combat during each opponent's turn. */

/* Steamflogger of the Month	|3|R|R
 * Creature - Goblin Rigger 3/3
 * When ~ enters the battlefield, it assembles a Contraption for each Contraption you control. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Steamflogger Temp	|1|R
 * Creature - Goblin Rigger 2/1
 * |6, |T: ~ assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Steamfloggery	|4|R|R
 * Instant
 * Roll a six-sided die. Assemble a number of Contraptions equal to the result. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Super-Duper Death Ray	|2|R
 * Instant
 * Trample (This spell can deal excess damage to its target's controller.)
 * ~ deals 4 damage to target creature. */

/* Target Minotaur	|1|R
 * Creature - Minotaur Warrior 2/1
 * Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.) */

/* Three-Headed Goblin	|3|R|R
 * Creature - Goblin Mutant 3/3
 * Triple strike (This creature deals first-strike, regular, and last-strike combat damage.) */

/* Work a Double	|2|R|R
 * Sorcery
 * Assemble two Contraptions. (Put the top card of your Contraption deck face up onto one of your sprockets. Then repeat this process.) */

/* Wrench-Rigger	|R
 * Creature - Goblin Rigger 1/1
 * When ~ enters the battlefield, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/*********
 * Green *
 *********/

/* As Luck Would Have It	|G
 * Enchantment
 * Hexproof
 * Whenever you roll a die, put a number of luck counters on ~ equal to the result. Then if there are 100 or more luck counters on ~, you win the game. (Count both rolls if you reroll a die.) */

/* Beast in Show	|4|G|G
 * Creature - Beast 6/4
 * Trample */

/* Chittering Doom	|3|G
 * Enchantment
 * Whenever you roll a 4 or higher on a die, create a 1/1 |Sgreen Squirrel creature token. */

/* Clever Combo	|1|G
 * Sorcery
 * Search your library for a host card or a card with augment, reveal it, put it into your hand, then shuffle your library. */

/* Eager // Beaver	|2|G
 * Host Creature - Beaver 3/2
 * When this creature enters the battlefield,
 * //
 * you may untap target permanent. */

/* First Pick	|3|G
 * Instant
 * Destroy target artifact or enchantment. Assemble a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Ground Pounder	|1|G
 * Creature - Goblin Warrior 2/2
 * |3|G: Roll a six-sided die. ~ gets +X/+X until end of turn, where X is the result.
 * Whenever you roll a 5 or higher on a die, ~ gains trample until end of turn. */

/* Half-Squirrel, Half-	""
 * Creature - Squirrel -1/0
 * Whenever a nontoken creature enters the battlefield,
 * Augment |G (|G, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Ineffable Blessing B	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose an artist.
 * Whenever a creature with art by the chosen artist enters the battlefield under your control, draw a card. */

/* Ineffable Blessing E	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose odd or even.
 * Whenever a creature with a collector number of the chosen value enters the battlefield under your control, draw a card. */

/* Joyride Rigger	|3|G
 * Creature - Goblin Rigger 3/3
 * When ~ enters the battlefield, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Monkey-	""
 * Creature - Monkey 2/2
 * Whenever a nontoken creature you control dies,
 * Augment |2|G (|2|G, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Mother // Kangaroo	|4|G
 * Host Creature - Kangaroo 1/1
 * When this creature enters the battlefield,
 * //
 * roll a six-sided die. Put a number of +1/+1 counters on this creature equal to the result. */

/* Multi-Headed	""
 * Creature - Hydra 4/4
 * At the beginning of each end step, if you rolled a die this turn,
 * Augment |4|G (|4|G, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Really Epic Punch	|1|G
 * Sorcery
 * Target creature you control gets +2/+2 if it's a host or has augment. Then it fights target creature you don't control. */

/* Selfie Preservation	|1|G
 * Sorcery
 * Search your library for a basic land card and reveal it. If there's a tree in its art, put it onto the battlefield tapped. Otherwise, put it into your hand. Then shuffle your library. */

/* Serpentine	""
 * Creature - Wurm 3/3
 * Whenever a land enters the battlefield under your control,
 * Augment |2|G (|2|G, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Shellephant	|1|G|G
 * Creature - Turtle and/or Elephant 100/100
 * |0: Choose one. You may activate this ability while ~ is in any zone.
 * * ~ has base power and toughness 1/4.
 * * ~ has base power and toughness 3/3. */

/* Slaying Mantis	|5|G|G
 * Creature - Insect Wrestler 6/6
 * Just a second (As long as this spell is on the stack, players can't move cards on the battlefield.)
 * ~ enters the battlefield by being thrown from a distance of at least three feet.
 * When ~ enters the battlefield, it fights each creature an opponent controls that it touched as it entered. */

/* Squirrel Dealer	|G
 * Creature - Raccoon Lizard Bird 1/1
 * When ~ enters the battlefield, ask a person outside the game "Do you like Squirrels?" If they do, create a 1/1 |Sgreen Squirrel creature token. */

/* Steamflogger Service Rep	|2|G
 * Creature - Goblin Rigger 1/1
 * Whenever another Goblin enters the battlefield under your control, you may pay |1. If you do, ~ assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Wild // Crocodile	|1|G
 * Host Creature - Crocodile 1/1
 * When this creature enters the battlefield,
 * //
 * search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. */

/* Willing Test Subject	|2|G
 * Creature - Spider Monkey Scientist 2/2
 * Reach
 * Whenever you roll a 4 or higher on a die, put a +1/+1 counter on ~.
 * |6: Roll a six-sided die. */

/********
 * Gold *
 ********/

/* Baron Von Count	|1|B|R
 * Legendary Creature - Human Villain 3/3
 * ~ enters the battlefield with a doom counter on "5."
 * Whenever you cast a spell with the indicated numeral in its mana cost, text box, power, or toughness, move the doom counter one numeral to the left.
 * When the doom counter moves from "1," destroy target player and put that doom counter on "5." */

/* Better Than One	|G|W
 * Sorcery
 * A person outside the game becomes your teammate. (Choose any number of cards in your hand, on top of your library, or on the battlefield under your control. Those cards become your teammate's hand, library, and permanents, respectively.) */

/* Cramped Bunker	|4|R|G
 * Enchantment
 * At the beginning of each opponent's upkeep, that player moves a permanent they control to touch ~ and no other permanents. If the player can't, destroy each permanent they control that isn't touching ~, then sacrifice it. */

/* Dr. Julius Jumblemorph	|2|G|W
 * Legendary Creature 4/4
 * ~ is every creature type (even if this card isn't on the battlefield).
 * Whenever a host enters the battlefield under your control, you may search your library and/or graveyard for a card with augment and combine it with that host. If you search your library this way, shuffle it. */

/* The Grand Calcutron	|W|U
 * Legendary Artifact
 * When ~ enters the battlefield, each player's hand becomes a program (an ordered row of revealed cards).
 * Players can only play the first card of their program.
 * If a card would be put into a player's hand from anywhere, that player reveals it and places it anywhere within their program instead.
 * At the beginning of each player's end step, if that player's program has fewer than five cards, they draw cards equal to the difference. */

/* Grusilda, Monster Masher	|3|B|R
 * Legendary Creature - Zombie Villain 4/4
 * Combined, enchanted, and equipped creatures you control have menace.
 * |3|B|R, |T: Put two target creature cards from graveyards onto the battlefield combined into one creature under your control. (Its power is equal to their total power, its toughness is equal to their total toughness, and it has their names, mana costs, types, text boxes, etc.) */

/* Ol' Buzzbark	|X|R|G
 * Legendary Creature - Goblin Warrior 3/3
 * When ~ enters the battlefield, roll X six-sided dice onto the battlefield from a height of at least X inches. For each die, put a number of +1/+1 counters equal to the result on each creature you control that die is touching. For each die, ~ deals damage equal to the result to each creature an opponent controls that die is touching. */

/* Phoebe, Head of S.N.E.A.K.	|1|U|B
 * Legendary Creature - Human Spy 2/3
 * ~, Head of S.N.E.A.K. can't be blocked by creatures with flavor text.
 * |2|U|B: ~ permanently steals target creature's text box. (That creature loses all rules text, flavor text, and watermarks. This creature gains them.) */

/* Urza, Academy Headmaster	|W|U|B|R|G
 * Legendary Planeswalker - Urza (4)
 * +1: Head to AskUrza.com and click +1.
 * -1: Head to AskUrza.com and click -1.
 * -6: Head to AskUrza.com and click -6. */

/* X	|U|B
 * Legendary Creature - Human Spy 2/2
 * As long as ~ is in ~'s owner's opponent's hand, ~'s owner may cast ~ and activate ~'s abilities. That opponent can't cast ~ and plays with their hand revealed.
 * |U|B, |T: Put ~ into target opponent's hand.
 * |3|U|B: You may play a card in the same hand as ~ without paying its mana cost. */

/**********
 * Hybrid *
 **********/

/* Mary O'Kill	|5|BR
 * Legendary Creature - Human Villain 5/5
 * |1|BR: Switch a Killbot or ~ in your hand with one on the battlefield. (If a creature is tapped, the switched creature is tapped. The same is true for untapped, attacking, blocking, enchanted, equipped, and targeted. Any counters on a creature are on the switched creature instead.) */

/************
 * Artifact *
 ************/

/* Angelic // Rocket	|8
 * Artifact Host Creature - Angel 4/4
 * Flying
 * //
 * When this creature enters the battlefield,
 * //
 * you may destroy target nonland permanent. */

/* Buzzing Whack-a-Doodle	|4
 * Artifact
 * As ~ enters the battlefield, you and an opponent each secretly choose Whack or Doodle. Then those choices are revealed. If the choices match, ~ has that ability. Otherwise, it has Buzz.
 * * Whack - |T: Target player loses 2 life.
 * * Doodle - |T: You gain 3 life.
 * * Buzz - |2, |T: Draw a card. */

/* Clock of DOOOOOOOOOOOOM!	|4
 * Artifact
 * |4, |T: Move the CRANK! counter to your Contraption deck's next sprocket and crank any number of that sprocket's Contraptions. */

/* Cogmentor	|1
 * Artifact Creature - Gnome Rigger 1/1
 * Flying
 * |4: Reassemble target Contraption you control. (Move it onto another one of your sprockets.) */

/* Contraption Cannon	|4
 * Artifact
 * |2, Sacrifice ~: It deals damage to any target equal to the number of Contraptions you control. */

/* Curious Killbot	|2
 * Artifact Creature - Killbot 2/1 */

/* Delighted Killbot	|2
 * Artifact Creature - Killbot 2/1 */

/* Despondent Killbot	|2
 * Artifact Creature - Killbot 2/1 */

/* Enraged Killbot	|2
 * Artifact Creature - Killbot 2/1 */

/* Entirely Normal Armchair	""
 * Artifact
 * During your turn, if ~ is in your hand, you may hide it on the battlefield.
 * |0: Return ~ to its owner's hand. Only any opponent may activate this ability and only if they see ~.
 * |2, Sacrifice ~: Destroy target attacking creature. */

/* Everythingamajig A	|5
 * Artifact
 * |2, |T: Move a counter from one permanent onto another. If the second permanent refers to any kind of counter, the moved counter becomes one of those counters. Otherwise, it becomes a +1/+1 counter.
 * |3, |T: Put a +1/+1 counter on target creature.
 * |4, |T: Proliferate. (You choose any number of permanents and/or players with counters on them, then give each another counter of a kind already there.) */

/* Everythingamajig D	|6
 * Artifact
 * |T: Add one mana of any color.
 * |1, |T: Say the flavor text on a card in your hand. Target opponent guesses that card's name. You may reveal that card. If you do and your opponent guessed wrong, draw a card.
 * |8, |T: Everythingamajig deals 12 damage to target creature. */

/* Everythingamajig E	|5
 * Artifact
 * Sacrifice a land: You gain 2 life.
 * Sacrifice a creature: Add |C|C.
 * |2, Discard a card: Search your library for a card that shares a complete word in its name with the name of the discarded card, reveal it, put it into your hand, then shuffle your library. */

/* Everythingamajig F	|8
 * Artifact
 * |1, |T: Scry 2.
 * |1, |T, Pay 1 life: Create a 0/1 |Swhite Goat creature token.
 * |7, |T, Sacrifice Everythingamajig: Choose target player. At the beginning of the next end step, exchange life totals with that player, exchange control of all permanents you and that player control, and exchange cards in your hands, cards in your libraries, and cards in your graveyards. */

/* Gnome-Made // Engine	|4
 * Artifact Host Creature - Construct 2/2
 * When this creature enters the battlefield,
 * //
 * create a 1/1 colorless Gnome artifact creature token. */

/* Handy Dandy Clone Machine	|3
 * Artifact
 * |2, |T: Create a 2/2 colorless Homunculus creature token. It must be represented by a unique hand and two fingers at all times, or it ceases to exist. */

/* Kindslaver	|5
 * Legendary Artifact
 * |5, |T, Sacrifice ~: A person outside the game controls target player during that player's next turn. Neither player may advise that person until the end of that turn. */

/* Krark's Other Thumb	|2
 * Legendary Artifact
 * If you would roll a die, instead roll two of those dice and ignore one of those results. */

/* Labro // Bot	|5
 * Artifact Host Creature - Hound 3/3
 * When this creature enters the battlefield,
 * //
 * return target host card or card with augment from your graveyard to your hand. */

/* Lobe Lobber	|2
 * Artifact - Equipment
 * Equipped creature has "|T: This creature deals 1 damage to target player or planeswalker. Roll a six-sided die. On a 5 or higher, untap it."
 * Equip |2 */

/* Mad Science Fair Project	|3
 * Artifact
 * |T: Roll a six-sided die. On a 3 or lower, target player adds |C. Otherwise, that player adds one mana of any color they choose. */

/* Modular Monstrosity	|7
 * Artifact Creature - Construct 3/3
 * Whenever an opponent casts a spell, you have five seconds to choose a keyword you haven't chosen for a card named ~ today that's been printed on a creature card. If you do, ~ gains that ability. Otherwise, ~ loses all keyword abilities. */

/* Proper Laboratory Attire	|1
 * Artifact - Equipment
 * Equipped creature gets +2/+1 and has protection from die rolls. (Nothing that lets a player roll a die can block, target, deal damage to, or attach to equipped creature.)
 * Equip |2 */

/* Robo-	""
 * Artifact Creature - Construct 1/1
 * At the beginning of each end step, if an artifact entered the battlefield under your control this turn,
 * Augment |2 (|2, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Split Screen	|4
 * Artifact
 * When ~ enters the battlefield, shuffle your library and deal it into four libraries. If anything refers to your library, choose one of your libraries for it.
 * Play with your libraries' top cards revealed.
 * When ~ leaves the battlefield, shuffle your libraries together. */

/* Steam-Powered	""
 * Artifact Creature - Construct 0/4
 * |5:
 * Augment |4 (|4, Reveal this card from your hand: Combine it with target host. Augment only as a sorcery.) */

/* Steel Squirrel	|2
 * Artifact Creature - Squirrel 1/1
 * Whenever you roll a 5 or higher on a die, ~ gets +X/+X until end of turn, where X is the result.
 * |6: Roll a six-sided die. */

/* Sword of Dungeons & Dragons	|3
 * Artifact - Equipment
 * Equipped creature gets +2/+2 and has protection from Rogues and from Clerics.
 * Whenever equipped creature deals combat damage to a player, create a 4/4 gold Dragon creature token with flying and roll a d20 (a twenty-sided die). If you roll a 20, repeat this process.
 * Equip |2 */

/* Voracious // Vacuum	|3
 * Artifact Host Creature - Construct 1/1
 * When this creature enters the battlefield,
 * //
 * put a +1/+1 counter on target creature. */

/********
 * Land *
 ********/

/* Secret Base A	""(Order of the Widget watermark)
 * Land
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a spell that shares a watermark with Secret Base. */

/* Secret Base B	""(Agents of S.N.E.A.K watermark)
 * Land
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a spell that shares a watermark with Secret Base. */

/* Secret Base C	""(League of Dastardly Doom watermark)
 * Land
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a spell that shares a watermark with Secret Base. */

/* Secret Base D	""(Goblin Explosioneers watermark)
 * Land
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a spell that shares a watermark with Secret Base. */

/* Secret Base E	""(Crossbreed Labs watermark)
 * Land
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a spell that shares a watermark with Secret Base. */

/* Watermarket	""
 * Land
 * |T: Add |C|C. Spend this mana only to cast spells with watermarks. */

/***************
 * Contraption *
 ***************/

/* Accessories to Murder	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets +X/+0 until end of turn, where X is the number of creatures you control. */

/* Applied Aeronautics	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature gets +1/+0, gains flying, and becomes an artifact in addition to its other types. */

/* Arms Depot	""
 * Artifact - Contraption
 * Whenever you crank ~, put two +1/+1 counters on target creature. */

/* Auto-Key	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature becomes an artifact in addition to its other types and gains "|T: You gain 3 life." */

/* Bee-Bee Gun	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature gains "|2: This creature fights another target creature." */

/* Boomflinger	""
 * Artifact - Contraption
 * Whenever you crank ~, roll two six-sided dice. ~ deals damage to target player or planeswalker equal to the difference between those results. */

/* Buzz Buggy	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets +2/+0 and gains trample until end of turn. */

/* Deadly Poison Sampler	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature gains "Whenever this creature deals combat damage to a player, destroy target creature that player controls." */

/* Dictation Quillograph	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature gains "Whenever this creature deals combat damage to a player, you may draw a card. If you do, discard a card." */

/* Dispatch Dispensary	""
 * Artifact - Contraption
 * Whenever you crank ~, create a 2/2 |Sblack Rogue creature token with menace. */

/* Division Table	""
 * Artifact - Contraption
 * Whenever you crank ~, target player loses 2 life. */

/* Dogsnail Engine	""
 * Artifact - Contraption
 * Whenever you crank ~, target player gains life equal to the greatest power among creatures you control. */

/* Dual Doomsuits	""
 * Artifact - Contraption
 * Whenever you crank ~, each time a source you control would deal damage this turn, it deals double that damage instead. */

/* Duplication Device	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature becomes a copy of any creature on the battlefield, except it's an artifact in addition to its other types. */

/* Faerie Aerie	""
 * Artifact - Contraption
 * Whenever you crank ~, create two 1/1 |Sblue Faerie Spy creature tokens with flying, haste, and "Whenever this creature deals combat damage to a player, draw a card." Exile them at the beginning of the next end step. */

/* Genetic Recombinator	""
 * Artifact - Contraption
 * Whenever you crank ~, up to two target creatures each get +2/+2 until end of turn. */

/* Gift Horse	""
 * Artifact - Contraption
 * Whenever you crank ~, roll two six-sided dice. Create a number of 1/1 |Sred Goblin creature tokens equal to the difference between those results. */

/* Gnomeball Machine	""
 * Artifact - Contraption
 * Whenever you crank ~, create two 1/1 colorless Gnome artifact creature tokens. */

/* Goblin Slingshot	""
 * Artifact - Contraption
 * Whenever you crank ~, creatures you control get +2/+0 and gain trample until end of turn. */

/* Guest List	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets -X/-X until end of turn, where X is the number of creature cards in its controller's graveyard. */

/* Hard Hat Area	""
 * Artifact - Contraption
 * Whenever you crank ~, roll two six-sided dice. ~ assembles a number of Contraptions equal to the difference between those results. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Head Banger	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature must be blocked this turn if able. */

/* Hypnotic Swirly Disc	""
 * Artifact - Contraption
 * Whenever you crank ~, target player puts the top X cards of their library into their graveyard, where X is the number of creatures you control. */

/* Inflation Station	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets +3/+3 until end of turn. */

/* Insufferable Syphon	""
 * Artifact - Contraption
 * Whenever you crank ~, target player discards a card. */

/* Jamming Device	""
 * Artifact - Contraption
 * Whenever you crank ~, creatures target player controls get -1/-1 until end of turn. */

/* Lackey Recycler	""
 * Artifact - Contraption
 * Whenever you crank ~, put target creature card from your graveyard on top of your library. */

/* Mandatory Friendship Shackles	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets -1/-1 until end of turn. */

/* Neural Network	""
 * Artifact - Contraption
 * Whenever you crank ~, gain control of target creature an opponent controls with power less than or equal to the number of creature cards in its controller's graveyard until end of turn. Untap that creature. It gains haste until end of turn. */

/* Oaken Power Suit	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets +X/+X until end of turn, where X is the greatest power among creatures you control. */

/* Optical Optimizer	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature becomes an artifact in addition to its other types and gains "|T: Draw a card." */

/* Pet Project	""
 * Artifact - Contraption
 * Whenever you crank ~, put target creature card from an opponent's graveyard onto the battlefield under your control. */

/* Quick-Stick Lick Trick	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gets +1/+1 and gains lifelink until end of turn. */

/* Rapid Prototyper	""
 * Artifact - Contraption
 * Whenever you crank ~, create an X/X colorless Construct artifact creature token, where X is the number of artifacts you control. */

/* Record Store	""
 * Artifact - Contraption
 * Whenever you crank ~, look at the top X cards of your library, where X is the number of artifacts you control. Put one of those cards into your hand and the rest on the bottom of your library in a random order. */

/* Refibrillator	""
 * Artifact - Contraption
 * Whenever you crank ~, return target creature card from your graveyard to your hand. */

/* Sap Sucker	""
 * Artifact - Contraption
 * Whenever you crank ~, add |G. Until end of turn, you don't lose this mana as steps and phases end. */

/* Sundering Fork	""
 * Artifact - Contraption
 * Whenever you crank ~, destroy target artifact. */

/* Targeting Rocket	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature blocks this turn if able. */

/* Thud-for-Duds	""
 * Artifact - Contraption
 * Whenever you crank ~, roll two six-sided dice. ~ deals damage to target creature equal to the difference between those results. */

/* Top-Secret Tunnel	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature can't be blocked this turn. */

/* Tread Mill	""
 * Artifact - Contraption
 * Whenever you crank ~, until end of turn, target creature gets +1/+2, gains vigilance, and becomes an artifact in addition to its other types. */

/* Turbo-Thwacking Auto-Hammer	""
 * Artifact - Contraption
 * Whenever you crank ~, target creature gains double strike until end of turn. */

/* Twiddlestick Charger	""
 * Artifact - Contraption
 * Whenever you crank ~, tap or untap target creature. */

/* Widget Contraption	""
 * Artifact - Contraption
 * Whenever you crank ~, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

// Plains, Island, Swamp, Mountain, Forest.

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c4930; original cards/unstable.c:107. */
static int fx_0(int player, int card, event_t event) { return 0; }

/* ManalinkEh 2022: 0x25c4b50; original cards/unstable.c:155. */
static int test_words_in_name_val(int iid, intptr_t val, int player, int card)
{
  int result;

  result = words_in_name(player, card);
  return (unsigned int)(result == val);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int test_ineffable_blessing_a(int iid, intptr_t val, int player, int card);
static int test_words_in_name_mod_val(int iid, intptr_t val, int player, int card);

/* ManalinkEh 2022: 0x25c4b10; original cards/unstable.c:305. */
static int test_words_in_name_mod_val(int iid, intptr_t val, int player, int card)
{
  int result;

  result = words_in_name(player, card);
  return (unsigned int)(result % 2 == val);
}

/* ManalinkEh 2022: 0x25c53e0; original cards/unstable.c:309. */
int card_oddly_uneven(int player, int card, event_t event)
{
  /* Oddly Uneven	|3|W|W
   * Sorcery
   * Choose one -
   * * Destroy each creature with an odd number of words in its name. (Hyphenated words are one word.)
   * * Destroy each creature with an even number of words in its name. */

  enum
  {
    CHOICE_ODD = 1,
    CHOICE_EVEN = 2
  };

  int result;
  test_definition_t test;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
    }
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        result =
            dialog_impl(player, card, event, DLG_RANDOM, "Odd", 1, 1, "Even", 1, 1, 0, 0, 0, 0);
        if (event == EVENT_RESOLVE_SPELL)
        {
          default_test_definition(&test, 0x47);
          test.value_for_special_selection_function = (intptr_t)(result == CHOICE_ODD);
          test.special_selection_function = test_words_in_name_mod_val;
          new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
          kill_card(player, card, KILL_DESTROY);
        }
        return basic_spell(player, card, event);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  dialog_impl(player, card, event, DLG_RANDOM, "Odd", 1, 1, "Even", 1, 1, 0, 0, 0, 0);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25c4b80; original cards/unstable.c:342. */
static char *target_hasnt_reminder_text(int who_chooses, int player, int card, int targeting_player,
                                        int targeting_card)
{
  int result;
  char *requirement_error;

  result = has_reminder_text(player, card);
  requirement_error = NULL;
  if (result != 0)
  {
    requirement_error = "reminder text";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x25c55c0; original cards/unstable.c:347. */
int card_old_guard(int player, int card, event_t event)
{
  /* Old Guard	|1|W
   * Artifact Creature - Cyborg Soldier 2/1
   * |W, |T: Tap target creature without reminder text. (Reminder text is still any italicized text in parentheses that explains rules you already know.) */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_hasnt_reminder_text;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_hasnt_reminder_text;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {

      instance = get_card_instance(player, card);
      tap_card(instance->targets[0].player, instance->targets[0].card);
    }
  }
  result = generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 1, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x25c5700; original cards/unstable.c:442. */
int card_crow_storm(int player, int card, event_t event)
{
  /* Crow Storm	|2|U
   * Sorcery
   * Create a 1/2 |Sblue Bird creature token with flying named Storm Crow.
   * Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

  int result;

  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      result = check_special_flags(player, card, 0x40000);
      if (result == 0)
      {
        result = get_storm_count();
        if (0 < result)
        {
          generate_tokens_by_id(player, card, CARD_ID_STORM_CROW, result);
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    generate_token_by_id(player, card, CARD_ID_STORM_CROW);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static char *target_hasnt_reminder_text(int who_chooses, int player, int card, int targeting_player,
                                        int targeting_card);

/* ManalinkEh 2022: 0x25c4970; original cards/unstable.c:1661. */
static int test_ineffable_blessing_a(int iid, intptr_t val, int player, int card)
{
  unsigned int matches;

  matches = (unsigned int)(val != 1);
  if ((cards_ptr[cards_data[iid].id]->flavor_text != NULL) &&
      (*cards_ptr[cards_data[iid].id]->flavor_text != '\0'))
  {
    matches = (unsigned int)(val == 1);
  }
  return matches;
}

/* ManalinkEh 2022: 0x25c7c10; original cards/unstable.c:1666. */
int card_ineffable_blessing_a(int player, int card, event_t event)
{
  /* Ineffable Blessing A	|1|G
   * Enchantment
   * As Ineffable Blessing enters the battlefield, choose Flavorful or Bland.
   * * Flavorful - Whenever a creature with flavor text enters the battlefield under your control, draw a card.
   * * Bland - Whenever a creature without flavor text enters the battlefield under your control, draw a card. */

  int result;
  card_instance_t *instance;
  unsigned int selected_condition;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    selected_condition = dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                                     DLG_NO_CANCEL, "Flavorful", 1, 1, "Bland", 1, 1, 0, 0, 0, 0);
    instance = get_card_instance(player, card);
    instance->targets[5].player = selected_condition | 0x46550000;
    result = create_targetted_legacy_effect(player, card, fx_0, player, card);
    alternate_legacy_text(selected_condition, player, result);
  }
  else if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {

    instance = get_card_instance(player, card);
    selected_condition = instance->targets[5].player;
    if (((short)(selected_condition >> 0x10) != 0x4655) ||
        (selected_condition = selected_condition & 0xffff, 1 < selected_condition - 1))
    {
      return 0;
    }
    default_test_definition(&test, 2);
    test.special_selection_function = test_ineffable_blessing_a;
    test.value_for_special_selection_function = selected_condition;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  else if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c88d0; original cards/unstable.c:2183. */
int card_everythingamajig_c(int player, int card, event_t event)
{
  /* Everythingamajig C	|5
   * Artifact
   * |1: Flip a coin. If you win the flip, add |C|C. Activate this ability only any time you could cast an instant.
   * |3, |T: Target player discards a card. Activate this ability only during your turn.
   * |X: Everythingamajig becomes an X/X Construct artifact creature until end of turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_DISCARD = 2,
    CHOICE_ANIMATE = 3
  };

  card_instance_t *instance;
  int selected_mode;
  int ability_available;
  bool own_turn;
  target_definition_t td;

  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    own_turn = current_turn == player;
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    instance = get_card_instance(player, card);
    selected_mode =
        dialog_impl(player, card, event, DLG_RANDOM, DLG_STORE_IN(&instance->targets[5].card), "Mana",
                    1, 1, DLG_MANA6(1, 0, 0, 0, 0, 0), "Discard", (unsigned int)own_turn, 1,
                    DLG_MANA6(3, 0, 0, 0, 0, 0), DLG_TAP, DLG_TARGET(&td, "TARGET_PLAYER"),
                    "Becomes Construct", 1, 1, DLG_MANA6(0xffffffff, 0, 0, 0, 0, 0), 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if ((((unsigned int)land_can_be_played >> 16 & 0xff) & 4) == 0)
      {
        ability_available = can_use_activated_abilities(player, card);
        return (unsigned int)(ability_available != 0 && selected_mode != 0);
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      if (selected_mode == CHOICE_MANA)
      {
        if (flip_a_coin(player, card) != 0)
        {
          produce_mana(player, 0, 2);
        }
        tapped_for_mana_color = -2;
      }
      else if (selected_mode == CHOICE_ANIMATE)
      {
        instance->info_slot = x_value;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (selected_mode == CHOICE_DISCARD)
      {
        discard(instance->targets[0].player, 0, player);
        return 0;
      }
      if (selected_mode == CHOICE_ANIMATE)
      {
        artifact_animation(player, card, instance->parent_controller, instance->parent_card, 1,
                           instance->info_slot, instance->info_slot, 0, 0);
        return 0;
      }
    }
  }
  return 0;
}

/* October 2022 private .bss cache @ 0x2774140, original line 17. */
static const Watermark* watermarks = NULL;

/* ManalinkEh 2022: 0x25c4d00; original cards/unstable.c:20. */
static int do_dialog_lots_of_choices(int player, int src_player, int src_card, char **choices,
                                     int num_choices)
{
  int i;
  int choice;
  int first_length;
  int next_length;
  char *buffer;
  int page_start;
  char buf[3000];
  do
  {
    page_start = 0;
    do
    {
      strncpy(buf, " (More)", 8);
      if (page_start < num_choices)
      {
        first_length = sprintf(buf + 7, "\n %s", choices[page_start]);
        buffer = buf + first_length + 7;
        i = 1;
        do
        {
          if (num_choices - page_start == i)
            break;
          next_length = sprintf(buffer, "\n %s", (choices + page_start)[i]);
          i = i + 1;
          buffer = buffer + next_length;
        } while (i != 10);
      }
      choice = do_dialog(player, src_player, src_card, -1, -1, buf, 0);
      if (choice != 0)
      {
        return page_start + -1 + choice;
      }
      page_start = page_start + 10;
    } while (page_start < num_choices);
  } while (true);
}

/* ManalinkEh 2022: 0x25c4fd0; original cards/unstable.c:72. */
static watermark_t choose_a_watermark(int player, int src_player, int src_card)
{
  if (watermarks == NULL)
    watermarks = get_watermarks();
  int idxs[256];
  int num_idxs = 0;
  for (int i = 0; i < 255; ++i)
    if (watermarks[i].name != NULL && watermarks[i].saturation != '\0')
      idxs[num_idxs++] = i;
  if (num_idxs > 0)
  {
    int choice;
    if (ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0))
    {
      choice = recorded_rand(player, num_idxs);
      if (ai_is_speculating != 1)
      {
        char buf[300];
        sprintf(buf, " %s\n", watermarks[idxs[choice]].disp);
        do_dialog(player, src_player, src_card, -1, -1, buf, 0);
      }
    }
    else
    {
      char *choices[num_idxs];
      for (int i = 0; i < num_idxs; ++i)
        choices[i] = watermarks[idxs[i]].disp;
      choice = do_dialog_lots_of_choices(player, src_player, src_card, choices, num_idxs);
    }
    return (watermark_t)idxs[choice];
  }
  return WMARK_NONE;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static char *target_has_league_watermark(int who_chooses, int player, int card,
                                         int targeting_player, int targeting_card);
static int test_has_watermark_val(int iid, intptr_t val, int player, int card);

/* ManalinkEh 2022: 0x25c49f0; original cards/unstable.c:113. */
static watermark_t get_watermark_by_iid(int iid)
{
  const Rarities *rarity;
  watermark_t card_watermark;

  rarity = get_rarity((unsigned int)cards_data[iid].id);
  card_watermark = (watermark_t)rarity->watermark;
  if (card_watermark != WMARK_NONE)
  {
    if (watermarks == NULL)
    {
      watermarks = get_watermarks();
    }
    if (watermarks[card_watermark].saturation != '\0')
    {
      return card_watermark;
    }
  }
  return WMARK_NONE;
}

/* ManalinkEh 2022: 0x25c4af0; original cards/unstable.c:150. */
static int test_has_watermark_val(int iid, intptr_t val, int player, int card)
{
  watermark_t watermark;

  watermark = get_watermark_by_iid(iid);
  return (unsigned int)(val == watermark);
}

/* ManalinkEh 2022: 0x25c5150; original cards/unstable.c:199. */
int card_go_to_jail(int player, int card, event_t event)
{
  /* GO TO JAIL	|W
   * Enchantment
   * When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield.
   * At the beginning of the upkeep of the exiled card's owner, that player rolls two six-sided dice. If they roll doubles, sacrifice ~. */

  int result;
  card_instance_t *instance;

  return_from_oblivion(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 2);
    td.allowed_controller = 1 - player;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
      }
    }
    instance->number_of_targets = '\0';
  }
  else if ((trigger_condition == TRIGGER_UPKEEP) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {
    result = owner_of_obliviated_card(player, card);
    upkeep_trigger_ability(player, card, event, result);
  }
  else if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    int r[2];
    roll_2d6t(current_turn, player, card, -1, -1, &r[0], &r[1]);
    if (r[0] == r[1])
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c5300; original cards/unstable.c:274. */
int card_knight_of_the_widget(int player, int card, event_t event)
{
  /* Knight of the Widget	|2|W
   * Artifact Creature - Cyborg Knight 100/100
   * Vigilance
   * ~'s power and toughness are each equal to the number of Order of the Widget watermarks among permanents you control. */

  vigilance(player, card, event);
  if ((uint32_t)(event - EVENT_POWER) < 2 && affect_me(player, card) && player != -1 &&
      !is_humiliated(player, card, AL_STATIC))
  {
    int widgets = 0;
    for (int c = 0; c < active_cards_count[player]; ++c)
    {
      card_instance_t *instance = in_play(player, c);
      if (instance)
      {
        int iid = instance->internal_card_id;
        if (iid < 0)
          iid = instance->backup_internal_card_id;
        if (get_watermark_by_iid(iid) == WMARK_ORDER_OF_THE_WIDGET &&
            is_what(player, c, TYPE_LAND | TYPE_CREATURE | TYPE_ENCHANTMENT | TYPE_ARTIFACT))
          ++widgets;
      }
    }
    set_characteristic_pt(widgets);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c57c0; original cards/unstable.c:541. */
int card_time_out(int player, int card, event_t event)
{
  /* Time Out	|4|U
   * Instant
   * Roll a six-sided die. Put target nonland permanent into its owner's library just beneath the top X cards of that library, where X is the result. */

  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {

      card_instance_t *inst = get_card_instance(player, card);
      int x = roll_d6t(player, player, card, inst->targets[0].player,
                        inst->targets[0].card);
      put_permanent_under_the_first_x_card_of_its_owners_library(
          inst->targets[0].player, inst->targets[0].card, x);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x25c4bb0; original cards/unstable.c:752. */
char *target_has_black_border(int who_chooses, int player, int card, int targeting_player,
                              int targeting_card)
{
  uint32_t csvid;
  border_t border;
  char *requirement_error;

  csvid = get_id(player, card);
  border = get_border_color(csvid);
  requirement_error = NULL;
  if (border != BORDER_BLACK)
  {
    requirement_error = "black-bordered";
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x25c4a60; original cards/unstable.c:756. */
static int test_has_any_watermark(int iid, intptr_t val, int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int current_iid = instance->internal_card_id;
  if (current_iid < 0)
    current_iid = instance->backup_internal_card_id;
  return get_watermark_by_iid(current_iid) != WMARK_NONE;
}

/* ManalinkEh 2022: 0x25c69d0; original cards/unstable.c:1005. */
int card_inhumaniac(int player, int card, event_t event)
{
  /* Inhumaniac	|1|B
   * Creature - Brainiac 1/1
   * At the beginning of your upkeep, roll a six-sided die. On a 3 or 4, put a +1/+1 counter on ~. On a 5 or higher, put two +1/+1 counters on it. On a 1, remove all +1/+1 counters from ~. */

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    int r = roll_d6t(player, player, card, -1, -1);
    if (r - 3U < 2)
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    else if (r < 5)
    {
      if (r == 1)
        remove_all_counters(player, card, player, card, COUNTER_P1_P1);
    }
    else
      add_counters(player, card, player, card, COUNTER_P1_P1, 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c4aa0; original cards/unstable.c:1067. */
static char *target_has_league_watermark(int who_chooses, int player, int card,
                                         int targeting_player, int targeting_card)
{
  card_instance_t *instance;
  int iid;
  watermark_t watermark;

  instance = get_card_instance(player, card);
  iid = instance->internal_card_id;

  if (iid < 0)
  {
    iid = instance->backup_internal_card_id;
  }
  watermark = get_watermark_by_iid(iid);
  if (watermark != WMARK_LEAGUE_OF_DASTARDLY_DOOM)
  {
    return "watermark";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x25c6b60; original cards/unstable.c:1072. */
int card_rumors_of_my_death(int player, int card, event_t event)
{
  /* "Rumors of My Death . . ."	|2|B
   * Enchantment
   * |3|B, Exile a permanent you control with a League of Dastardly Doom watermark: Return a permanent card with a League of Dastardly Doom watermark from your graveyard to the battlefield. */

  int result;
  target_t sel;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    base_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_has_league_watermark;
    new_default_test_definition(
        &test, 0x47, "Select target permanent card with a League of Dastardly Doom watermark.");
    test.special_selection_function = test_has_watermark_val;
    test.value_for_special_selection_function = 2;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 3, 1, 0, 0, 0, 0, 0,
                                         &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = new_special_count_grave(player, &test);
      if (0 < result)
      {
        result = graveyard_has_shroud(player);
        return (unsigned int)(result == 0);
      }
      return 0;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 3, 1, 0, 0, 0, 0, 0, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = select_target(
          player, card + -1000, &td,
          "Select a permanent you control with a League of Dastardly Doom watermark.", &sel);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      select_target_from_grave_source(player, card, player, SFG_0, 2, &test, 0);
      if (spell_fizzled != 1)
      {
        kill_card(sel.player, sel.card, KILL_EXILE);
        return 0;
      }
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    base_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_has_league_watermark;
    new_default_test_definition(
        &test, 0x47, "Select target permanent card with a League of Dastardly Doom watermark.");
  }
  test.value_for_special_selection_function = 2;
  test.special_selection_function = test_has_watermark_val;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  result = validate_target_from_grave_source(player, card, player, 0);
  if (result < 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
  return 0;
}

/* ManalinkEh 2022: 0x25c7090; original cards/unstable.c:1204. */
int card_sly_spy_f(int player, int card, event_t event)
{
  /* Sly Spy F	|2|B
   * Creature - Human Spy 2/2
   * Whenever Sly Spy deals combat damage to a player, roll a six-sided die. That player loses life equal to the result. */

  if (!damage_dealt_by_me(player, card, event,
                           DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                               DDBM_MUST_BE_COMBAT_DAMAGE))
    return 0;
  card_instance_t *inst = get_card_instance(player, card);
  unsigned int packed_damage = inst->targets[1].player;
  inst->targets[1].player = 0;
  int times_damaged[2] = { packed_damage & 0xff, (packed_damage >> 8) & 0xff };
  APNAP(p, {
    int hits = times_damaged[p];
    for (int i = 0; i < hits; ++i)
    {
      int amount = roll_d6t(player, player, card, p, -1);
      lose_life(p, amount);
    }
  });
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int fx_ineffable_blessing_f(int player, int card, event_t event);

/* ManalinkEh 2022: 0x25c4940; original cards/unstable.c:1229. */
static int test_has_been_banned_or_restricted(int iid, intptr_t val, int player, int card)
{
  enum
  {
    CPF_EVER_BANNED_OR_RESTRICTED = 1
  };

  return cards_ptr[cards_data[iid].id]->flags & CPF_EVER_BANNED_OR_RESTRICTED;
}

/* ManalinkEh 2022: 0x25c73a0; original cards/unstable.c:1263. */
int card_squirrel_powered_scheme(int player, int card, event_t event)
{
  /* Squirrel-Powered Scheme	|2|B
   * Enchantment
   * Increase the result of each die you roll by 2. */

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      add_to_die_rolls[player] = add_to_die_rolls[player] + 2;
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c73f0; original cards/unstable.c:1308. */
int card_box_of_free_range_goblins(int player, int card, event_t event)
{
  /* Box of Free-Range Goblins	|4|R|R
   * Sorcery
   * Roll a six-sided die. Create a number of 1/1 |Sred Goblin creature tokens equal to the result. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    result = roll_d6t(player, player, card, -1, -1);
    generate_tokens_by_id(player, card, CARD_ID_GOBLIN, result);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25c7510; original cards/unstable.c:1370. */
int card_garbage_elemental_d(int player, int card, event_t event)
{
  /* Garbage Elemental D	|4|R
   * Creature - Elemental 3/3
   * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.)
   * When Garbage Elemental enters the battlefield, roll a six-sided die. Garbage Elemental deals damage equal to the result to target opponent or planeswalker. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  cascade(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }

  opponent_or_pw_target_definition(player, card, &td);
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.allow_cancel = 0;
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
    if (result != 0)
    {

      result =
          roll_d6t(player, player, card, instance->targets[0].player, instance->targets[0].card);
      damage_creature(instance->targets[0].player, instance->targets[0].card, result, player, card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x25c7630; original cards/unstable.c:1422. */
int card_hammer_helper(int player, int card, event_t event)
{
  /* Hammer Helper	|3|R
   * Sorcery
   * Gain control of target creature until end of turn. Untap that creature and roll a six-sided die. Until end of turn, it gains haste and gets +X/+0, where X is the result. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *inst = get_card_instance(player, card);
      int x = roll_d6t(player, player, card, inst->targets[0].player, inst->targets[0].card);
      effect_act_of_treason_and_modify_pt_or_abilities(
          player, card, inst->targets[0].player, inst->targets[0].card, x, 0, 0, 0);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return aot_like_spell(player, card, event, &td, "TARGET_CREATURE", 0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int fx_stamp_of_approval(int player, int card, event_t event);

/* ManalinkEh 2022: 0x25c77a0; original cards/unstable.c:1456. */
int card_hammer_boomtacular(int player, int card, event_t event)
{
  /* Hammerfest Boomtacular	|3|R|R
   * Enchantment
   * Whenever you cast a spell with a Goblin Explosioneers watermark, ~ deals 2 damage to any target. */

  int result;
  card_instance_t *instance;


  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.special_selection_function = test_has_watermark_val;
    test.value_for_special_selection_function = 3;
    sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    target_definition_t td;

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_ANY", 0, 0);
      if (result != 0)
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25c7910; original cards/unstable.c:1497. */
int card_painiac(int player, int card, event_t event)
{
  /* Painiac	|2|R
   * Creature - Brainiac 0/3
   * At the beginning of your upkeep, roll a six-sided die. ~ gets +X/+0 until end of turn, where X is the result. */

  int power;

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  power = roll_d6t(player, player, card, -1, -1);
  pump_until_eot_merge_previous(player, card, player, card, power, 0);
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c79a0; original cards/unstable.c:1573. */
int card_druid_of_the_sacred_beaker(int player, int card, event_t event)
{
  /* Druid of the Sacred Beaker	|2|G
   * Creature - Deer Bird Ape Druid 2/2
   * |T: Add |G for each Crossbreed Labs watermark among permanents you control. */

  card_instance_t *instance;
  unsigned int iid;
  watermark_t watermark;
  int permanent;
  int card_00;
  int labs_permanents;

  if (event == EVENT_COUNT_MANA)
  {
    labs_permanents = 0;
    if ((card != affected_card) || (player != affected_card_controller))
      return mana_producing_creature(player, card, event, 0x40, COLOR_GREEN, labs_permanents);
  }
  else if ((event != EVENT_ACTIVATE) && ((event != EVENT_CAN_ACTIVATE || (player != 1))))
  {
    labs_permanents = 0;
    return mana_producing_creature(player, card, event, 0x40, COLOR_GREEN, labs_permanents);
  }
  labs_permanents = 0;
  for (card_00 = 0; card_00 < active_cards_count[player]; ++card_00)
  {
    instance = in_play(player, card_00);
    if (!instance)
      continue;
    iid = instance->internal_card_id;
    if ((int)iid < 0)
      iid = (unsigned int)instance->backup_internal_card_id;
    watermark = get_watermark_by_iid(iid);
    if (watermark == WMARK_CROSSBREED_LABS)
    {
      permanent = is_what(player, card_00, 0x47);
      labs_permanents = (labs_permanents + 1) - (unsigned int)(permanent == 0);
    }
  }
  return mana_producing_creature(player, card, event, 0x40, COLOR_GREEN, labs_permanents);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c7a90; original cards/unstable.c:1599. */
int card_earl_of_squirrel(int player, int card, event_t event)
{
  /* Earl of Squirrel	|4|G|G
   * Creature - Squirrel Advisor 4/4
   * Squirrellink (Damage dealt by this creature also causes you to create that many 1/1 |Sgreen Squirrel creature tokens.)
   * Creature tokens you control are Squirrels in addition to their other creature types.
   * Other Squirrels you control get +1/+1. */

  int result;

  squirrellink(player, card, event);
  if (event == EVENT_CHANGE_TYPE)
  {
    if (affected_card_controller == player)
    {
      result = is_token(player, affected_card);
      if (result != 0)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          result = is_humiliated(player, card, AL_STATIC);
          if (result == 0)
          {
            add_a_subtype(affected_card_controller, affected_card, SUBTYPE_SQUIRREL);
          }
        }
      }
    }
  }
  else if ((((uint32_t)(event - EVENT_POWER) < 2) && (affected_card_controller == player)) &&
           (card != affected_card))
  {
    result = has_subtype(player, affected_card, SUBTYPE_SQUIRREL);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result + 1;
      }
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static int roll_xd6_shim(int player, int card);

/* ManalinkEh 2022: 0x25c4be0; original cards/unstable.c:1641. */
static int roll_xd6_shim(int player, int card)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = roll_xd6t(player, instance->info_slot, player, card, -1, -1);
  return result;
}

/* ManalinkEh 2022: 0x25c7bb0; original cards/unstable.c:1645. */
int card_hydradoodle(int player, int card, event_t event)
{
  /* Hydradoodle	|X|X|G|G
   * Creature - Hydra Hound 0/0
   * As ~ enters the battlefield, roll X six-sided dice. ~ enters the battlefield with a number of +1/+1 counters on it equal to the total of those results.
   * Reach, trample */

  multi_x_spell(player, card, event, 2);
  enters_the_battlefield_with_counters_fn(player, card, event, COUNTER_P1_P1, roll_xd6_shim);
  return 0;
}

/* ManalinkEh 2022: 0x25c7e00; original cards/unstable.c:1715. */
int card_ineffable_blessing_c(int player, int card, event_t event)
{
  /* Ineffable Blessing C	|1|G
   * Enchantment
   * As Ineffable Blessing enters the battlefield, choose white-bordered or silver-bordered.
   * Whenever a creature with the chosen border enters the battlefield under your control, draw a card. */

  int result;
  card_instance_t *instance;
  unsigned int selected_condition;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    selected_condition =
        dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                    "White-bordered", 1, 1, "Silver-bordered", 1, 1, 0, 0, 0, 0);
    instance = get_card_instance(player, card);
    instance->targets[5].player = selected_condition | 0x46570000;
    result = create_targetted_legacy_effect(player, card, fx_0, player, card);
    alternate_legacy_text(selected_condition, player, result);
  }
  else if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {

    instance = get_card_instance(player, card);
    selected_condition = instance->targets[5].player;
    if (((short)(selected_condition >> 0x10) != 0x4657) ||
        (selected_condition = selected_condition & 0xffff, 1 < selected_condition - 1))
    {
      return 0;
    }
    default_test_definition(&test, 2);
    test.special_selection_function = test_has_border_val;
    test.value_for_special_selection_function = selected_condition;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  else if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c49b0; original cards/unstable.c:1761. */
static int test_has_rarity_val(int iid, intptr_t val, int player, int card)
{
  const Rarities *rarity;

  rarity = get_rarity((unsigned int)cards_data[iid].id);
  return (unsigned int)((unsigned int)rarity->default_rarity == val);
}

/* ManalinkEh 2022: 0x25c7ff0; original cards/unstable.c:1767. */
int card_ineffable_blessing_d(int player, int card, event_t event)
{
  /* Ineffable Blessing D	|1|G
   * Enchantment
   * As Ineffable Blessing enters the battlefield, choose a rarity.
   * Whenever a creature with the chosen rarity enters the battlefield under your control, draw a card. */

  int result;
  card_instance_t *instance;
  unsigned int selected_condition;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    selected_condition =
        dialog_impl(player, card, EVENT_RESOLVE_SPELL, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, "Common",
                    1, 1, "Uncommon", 1, 1, "Rare", 1, 1, "Mythic Rare", 1, 1, 0, 0, 0, 0);
    instance = get_card_instance(player, card);
    instance->targets[5].player = selected_condition | 0x46580000;
    result = create_targetted_legacy_effect(player, card, fx_0, player, card);
    alternate_legacy_text(selected_condition, player, result);
  }
  else if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {

    instance = get_card_instance(player, card);
    selected_condition = instance->targets[5].player;
    if (((short)(selected_condition >> 0x10) != 0x4658) ||
        (selected_condition = selected_condition & 0xffff, selected_condition - 1 < 4))
    {
      return 0;
    }
    default_test_definition(&test, 2);
    test.special_selection_function = test_has_rarity_val;
    test.value_for_special_selection_function = selected_condition;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  else if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c4c30; original cards/unstable.c:1825. */
static int fx_ineffable_blessing_f(int player, int card, event_t event)
{
  int result;

  result = txtfx(player, card, event, CARD_ID_INEFFABLE_BLESSING_F);
  if (result != txtfx_not_found)
  {
    sprintf(set_legacy_effect_name_addr, "%d", result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c8210; original cards/unstable.c:1832. */
int card_ineffable_blessing_f(int player, int card, event_t event)
{
  /* Ineffable Blessing F	|1|G
   * Enchantment
   * As Ineffable Blessing enters the battlefield, choose a number.
   * Whenever a creature with exactly the chosen number of words in its name enters the battlefield under your control, draw a card. (Hyphenated words are one word.) */

  unsigned int selected_condition;
  int result;
  card_instance_t *instance;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      result = recorded_rand(player, 5);
      result = result + 1;
    }
    else
    {
      result = choose_a_number(player, "Choose a number.", 0xfff);
    }
    make_txtfx(player, card, fx_ineffable_blessing_f, CARD_ID_INEFFABLE_BLESSING_F, result);
  }
  else if ((trigger_condition == TRIGGER_COMES_INTO_PLAY) && ((uint32_t)(event - EVENT_TRIGGER) < 2))
  {

    instance = get_card_instance(player, card);
    selected_condition = instance->targets[5].player;
    if ((short)(selected_condition >> 0x10) != 0x465a)
    {
      return 0;
    }
    default_test_definition(&test, 2);
    test.value_for_special_selection_function = selected_condition & 0xffff;
    test.special_selection_function = test_words_in_name_val;
    etb_ability_global(player, event, 2, TYPE_NONE, &test, player, card);
  }
  else if (event == EVENT_ETB)
  {
    draw_a_card(player);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x25c8500; original cards/unstable.c:2053. */
int card_border_guardian(int player, int card, event_t event)
{
  /* Border Guardian	|4
   * Artifact Creature - Knight 1/1
   * Whenever you cast a silver-bordered spell, put a +1/+1 counter on ~.
   * Whenever you cast a black-bordered spell, ~ can't be blocked this turn.
   * Whenever you cast a white-bordered spell, ~ gains double strike until end of turn. */

  uint32_t csvid;
  border_t border;
  int ability;
  int sp_ability;
  int alternate_legacy_text_number;

  sp_ability_trigger(player, event, 2, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
  {
    csvid = get_id(trigger_cause_controller, trigger_cause);
    border = get_border_color(csvid);
    if (border == BORDER_WHITE)
    {
      alternate_legacy_text_number = 2;
      sp_ability = 0;
      ability = 0x40000000;
      pump_ability_until_eot_no_repeat_alt(player, card, player, card, ability, sp_ability,
                                           alternate_legacy_text_number);
      return 0;
    }
    if (border == BORDER_SILVER)
    {
      add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    }
    else if (border == BORDER_BLACK)
    {
      alternate_legacy_text_number = 1;
      sp_ability = 0x10;
      ability = 0;
      pump_ability_until_eot_no_repeat_alt(player, card, player, card, ability, sp_ability,
                                           alternate_legacy_text_number);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c8610; original cards/unstable.c:2127. */
int card_everythingamajig_b(int player, int card, event_t event)
{
  /* Everythingamajig B	|5
   * Artifact
   * |2, |T: Draw a card. Activate this ability only if you have no cards in hand.
   * |8, |T: You gain 10 life.
   * |4, |T, Sacrifice Everythingamajig: You may put a silver-bordered permanent card from your hand onto the battlefield. */

  enum
  {
    CHOICE_DRAW = 1,
    CHOICE_LIFE = 2,
    CHOICE_HAND_TO_BF = 3
  };

  int ability_available;
  int hand_count_or_mode;
  test_definition_t test;

  if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_ACTIVATE))
  {
    hand_count_or_mode = hand_count[player];
    hand_count_or_mode = dialog_impl(
        player, card, event, DLG_RANDOM, "Draw", (unsigned int)(hand_count_or_mode < 1), 1,
        DLG_MANA6(2, 0, 0, 0, 0, 0), DLG_TAP, "Gain life", 1, 1, DLG_MANA6(8, 0, 0, 0, 0, 0),
        DLG_TAP, "Hand to battlefield", can_sacrifice_this_as_cost(player, card), 1, DLG_MANA6(
        4, 0, 0, 0, 0, 0), DLG_TAP, 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      ability_available = can_use_activated_abilities(player, card);
      return (unsigned int)(ability_available != 0 && hand_count_or_mode != 0);
    }
    if ((event == EVENT_ACTIVATE) && (hand_count_or_mode == CHOICE_HAND_TO_BF))
    {
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (hand_count_or_mode == CHOICE_LIFE)
      {
        gain_life(player, 10);
        return 0;
      }
      if (hand_count_or_mode != CHOICE_HAND_TO_BF)
      {
        if (hand_count_or_mode != CHOICE_DRAW)
        {
          return 0;
        }
        draw_a_card(player);
        return 0;
      }
      new_default_test_definition(&test, 0x47, "Select a silver-bordered permanent card.");
      test.special_selection_function = test_has_border_val;
      test.value_for_special_selection_function = 2;
      new_global_tutor(player, player, 4, 4, 0, 2, &test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c4c80; original cards/unstable.c:2324. */
static int fx_staff_of_the_letter_magus(int player, int card, event_t event)
{
  int argument2;
  const char *excluded_letter;

  argument2 = txtfx(player, card, event, CARD_ID_STAFF_OF_THE_LETTER_MAGUS);
  if ((argument2 != txtfx_not_found) && (argument2 - 0x42U < 0x19))
  {
    excluded_letter = strchr("AEIOUNRST", argument2);
    if (excluded_letter == NULL)
    {
      sprintf(set_legacy_effect_name_addr, "%c", (int)(char)argument2);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x25c4f50; original cards/unstable.c:2413. */
static int fx_stamp_of_approval(int player, int card, event_t event)
{
  unsigned int chosen_watermark;

  chosen_watermark = txtfx(player, card, event, CARD_ID_STAMP_OF_APPROVAL);
  if ((chosen_watermark != txtfx_not_found) && (chosen_watermark < 0xff))
  {
    if (watermarks == NULL)
    {
      backtrace_and_abort("cards/unstable.c:2418\nwatermarks");
    }
    strcpy(set_legacy_effect_name_addr, watermarks[chosen_watermark].disp);
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c8e10; original cards/unstable.c:2423. */
int card_stamp_of_approval(int player, int card, event_t event)
{
  /* Stamp of Approval	|3
   * Artifact
   * As ~ enters the battlefield, choose a watermark.
   * Creatures you control with the chosen watermark get +1/+1. */

  card_instance_t *instance;
  unsigned int iid;
  watermark_t encoded_or_actual_watermark;
  watermark_t chosen_watermark;

  if (event == EVENT_RESOLVE_SPELL)
  {

    encoded_or_actual_watermark = choose_a_watermark(player, player, card);
    if (encoded_or_actual_watermark != WMARK_NONE)
    {
      make_txtfx(player, card, fx_stamp_of_approval, CARD_ID_STAMP_OF_APPROVAL,
                 encoded_or_actual_watermark);
    }
  }
  else if (((uint32_t)(event - EVENT_POWER) < 2) && (affected_card_controller == player))
  {
    if (is_what(player, affected_card, 2))
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {

        instance = get_card_instance(player, card);
        encoded_or_actual_watermark = instance->targets[5].player;
        chosen_watermark = encoded_or_actual_watermark & 0xffff;
        if (((short)(encoded_or_actual_watermark >> 0x10) == 0x46b6) &&
            (chosen_watermark < WMARK_NONE))
        {
          instance = get_card_instance(affected_card_controller, affected_card);
          iid = instance->internal_card_id;

          if ((int)iid < 0)
          {
            iid = (unsigned int)instance->backup_internal_card_id;
          }
          encoded_or_actual_watermark = get_watermark_by_iid(iid);
          if (chosen_watermark == encoded_or_actual_watermark)
          {
            event_result = event_result + 1;
          }
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x25c4e40; original cards/unstable.c:48. */
static char choose_a_consonant_not_nrst(int player, int src_player, int src_card)
{
  char *choices[17] = {"B", "C", "D", "F", "G", "H", "J", "K", "L",
                       "M", "P", "Q", "V", "W", "X", "Y", "Z"};
  int num_choices = 17;
  if (ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2)))
  {
    int choice = do_dialog_lots_of_choices(player, src_player, src_card, choices, num_choices);
    return *choices[choice];
  }
  int choice = recorded_rand(player, num_choices);
  if (ai_is_speculating != 1)
  {
    char buf[300];
    sprintf(buf, " %s\n", choices[choice]);
    do_dialog(player, src_player, src_card, -1, -1, buf, 0);
  }
  return *choices[choice];
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static watermark_t get_watermark(card_instance_t *inst)
{
  return get_watermark_by_iid(inst->internal_card_id < 0
                                  ? inst->original_internal_card_id
                                  : inst->internal_card_id);
}

#endif

#ifndef SHANDALAR
static int is_dark_outdoors(void)
{
  unsigned int now = (unsigned int)localtime_hour_minute();
  unsigned int hour = now >> 16;
  unsigned int minute = now & 0xffffU;
  if (hour == 5)
    return minute <= 29;
  if (hour == 18)
    return minute > 29;
  return hour < 5 || hour > 18;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c5920; original cards/unstable.c:576. */
int card_very_cryptic_command_b(int player, int card, event_t event)
{
  /* Very Cryptic Command B	|1|U|U|U
   * Instant
   * Choose two -
   * * Untap two target permanents.
   * * Tap each permanent target player controls with exactly one word in its name.
   * * Discard all the cards in your hand, then draw that many cards.
   * * Return target instant or sorcery card from your graveyard to your hand. */

  typedef enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_TAP = 2,
    CHOICE_DISCARD_DRAW = 3,
    CHOICE_RAISE = 4
  } CommandChoices;
  CommandChoices chosen_mode;
  int available_or_target_index;
  int next_index_or_valid;
  int grave_available;
  card_instance_t *instance;
  unsigned int raise_available;
  CommandChoices *validation_cursor;
  CommandChoices *choice_cursor;
  int valid_targets;
  CommandChoices choices[2];
  target_definition_t td_permanent;
  target_definition_t td_player;
  test_definition_t test_instant_sorcery;
  test_definition_t test;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
    }
    else if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td_permanent, 0x47);
  td_permanent.preferred_controller = player;
  available_or_target_index = target_available(player, card, &td_permanent);
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  next_index_or_valid = can_target(&td_player);
  new_default_test_definition(&test_instant_sorcery, 0x38,
                              "Select target instant or sorcery card.");
  grave_available = new_special_count_grave(player, &test_instant_sorcery);
  raise_available = 0;
  if (0 < grave_available)
  {
    grave_available = graveyard_has_shroud(player);
    raise_available = (unsigned int)(grave_available == 0);
  }
  choices[0] =
      dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM, "Untap",
                  (unsigned int)(1 < available_or_target_index), 1, "Tap", next_index_or_valid, 1,
                  "Discard, draw", 1, 1, "Graveyard to hand", raise_available, 1, 0, 0, 0, 0);
  if (event == EVENT_CAN_CAST)
  {
    return (unsigned int)(choices[0] != 0);
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      choice_cursor = choices;
      instance->number_of_targets = '\0';
      available_or_target_index = 0;
      do
      {
        if (spell_fizzled == 1)
        {
          return 0;
        }
        chosen_mode = *choice_cursor;
        if (chosen_mode == CHOICE_TAP)
        {
          next_index_or_valid = available_or_target_index + 1;
          new_pick_target(&td_player, "TARGET_PLAYER", available_or_target_index, 1);
        }
        else if (chosen_mode == CHOICE_RAISE)
        {
          next_index_or_valid = available_or_target_index + 1;
          select_target_from_grave_source(player, card, player, SFG_0, 2, &test_instant_sorcery,
                                          available_or_target_index);
        }
        else
        {
          next_index_or_valid = available_or_target_index;
          if (chosen_mode == CHOICE_UNTAP)
          {
            pick_up_to_n_targets(&td_permanent, "TARGET_PERMANENT", 2);
            next_index_or_valid = available_or_target_index + 2;
          }
        }
        choice_cursor = choice_cursor + 1;
        available_or_target_index = next_index_or_valid;
      } while (choices + 2 != choice_cursor);
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    choice_cursor = choices;
    valid_targets = 0;
    available_or_target_index = 0;
    validation_cursor = choice_cursor;
    do
    {
      chosen_mode = *validation_cursor;
      if (chosen_mode == CHOICE_TAP || chosen_mode == CHOICE_RAISE)
      {
        bool valid;
        if (chosen_mode == CHOICE_TAP)
          valid = validate_target(player, card, &td_player, available_or_target_index) != 0;
        else
          valid = validate_target_from_grave_source(player, card, player,
                                                    available_or_target_index) >= 0;
        if (!valid)
          instance->targets[available_or_target_index].player = -1;
        ++available_or_target_index;
        if (valid)
          ++valid_targets;
      }
      else if (chosen_mode == CHOICE_UNTAP)
      {
        if (validate_target(player, card, &td_permanent, available_or_target_index) == 0)
        {
          instance->targets[available_or_target_index].player = -1;
        }
        else
        {
          valid_targets = valid_targets + 1;
        }
        next_index_or_valid =
            validate_target(player, card, &td_permanent, available_or_target_index);
        if (next_index_or_valid == 0)
        {
          instance->targets[available_or_target_index + 1].player = -1;
        }
        else
        {
          valid_targets = valid_targets + 1;
        }
        available_or_target_index = available_or_target_index + 2;
      }
      validation_cursor = validation_cursor + 1;
    } while (validation_cursor != choices + 2);
    if (valid_targets < 1)
    {
      spell_fizzled = 1;
    }
    else
    {
      available_or_target_index = 0;
      do
      {
        chosen_mode = *choice_cursor;
        if (chosen_mode == CHOICE_DISCARD_DRAW)
        {
          next_index_or_valid = hand_count[player];
          discard_all(player);
          draw_cards(player, next_index_or_valid);
        }
        else if (chosen_mode < CHOICE_RAISE)
        {
          if (chosen_mode == CHOICE_UNTAP)
          {
            next_index_or_valid = instance->targets[available_or_target_index].player;
            if (-1 < next_index_or_valid)
            {
              untap_card(next_index_or_valid, instance->targets[available_or_target_index].card);
            }
            next_index_or_valid = instance->targets[available_or_target_index + 1].player;
            if (-1 < next_index_or_valid)
            {
              untap_card(next_index_or_valid,
                         instance->targets[available_or_target_index + 1].card);
            }
            available_or_target_index = available_or_target_index + 2;
          }
          else if (chosen_mode == CHOICE_TAP)
          {
            if (-1 < instance->targets[available_or_target_index].player)
            {
              default_test_definition(&test, 0x47);
              test.special_selection_function = test_words_in_name_val;
              test.value_for_special_selection_function = 1;
              new_manipulate_all(player, card, instance->targets[available_or_target_index].player,
                                 &test, ACT_TAP);
            }
            ++available_or_target_index;
          }
        }
        else if (chosen_mode == CHOICE_RAISE)
        {
          if ((-1 < instance->targets[available_or_target_index].player) &&
              (next_index_or_valid = validate_target_from_grave_source(player, card, player,
                                                                       available_or_target_index),
               -1 < next_index_or_valid))
          {
            from_grave_to_hand(player, next_index_or_valid, 0);
          }
          available_or_target_index = available_or_target_index + 1;
        }
        choice_cursor = choice_cursor + 1;
      } while (choice_cursor != choices + 2);
    }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c5f90; original cards/unstable.c:760. */
int card_very_cryptic_command_e(int player, int card, event_t event)
{
  /* Very Cryptic Command E	|1|U|U|U
   * Instant
   * Choose two -
   * * Counter target black-bordered spell.
   * * Return target creature to its owner's hand.
   * * Untap each permanent you control with a watermark.
   * * Roll two six-sided dice. Target player puts the top X cards of their library into their graveyard, where X is the total of those results. */

  typedef enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_BOUNCE = 2,
    CHOICE_UNTAP = 3,
    CHOICE_MILL = 4
  } CommandChoices;
  CommandChoices chosen_mode;
  int counter_available_or_target_index;
  int creature_available_or_target_player;
  int player_available_or_mill_count;
  card_instance_t *instance;
  target_definition_t *td;
  CommandChoices *validation_cursor;
  CommandChoices *choice_cursor;
  int valid_targets;
  CommandChoices choices[2];
  target_definition_t td_spell;
  target_definition_t td_creature;
  target_definition_t td_player;
  test_definition_t test;

  if (event == EVENT_CAN_CAST)
  {
    counterspell_target_definition(player, card, &td_spell, 0);
    td_spell.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td_spell.extra = (intptr_t)target_has_black_border;
    counter_available_or_target_index = counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
    default_target_definition(player, card, &td_creature, 2);
    creature_available_or_target_player = can_target(&td_creature);
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    player_available_or_mill_count = can_target(&td_player);
    if (dialog_impl(player, card, EVENT_CAN_CAST, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                    "Counter spell", counter_available_or_target_index, 1, "Creature to hand",
                    creature_available_or_target_player, 1, "Untap permanents", 1, 1, "Mill",
                    player_available_or_mill_count, 1, 0, 0, 0, 0) != 0)
    {
      return ((counter_available_or_target_index == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      counterspell_target_definition(player, card, &td_spell, 0);
      td_spell.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td_spell.extra = (intptr_t)target_has_black_border;
      counter_available_or_target_index = counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
      default_target_definition(player, card, &td_creature, 2);
      creature_available_or_target_player = can_target(&td_creature);
      default_target_definition(player, card, &td_player, 0);
      td_player.zone = TARGET_ZONE_PLAYERS;
      player_available_or_mill_count = can_target(&td_player);
      choices[0] = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS,
                               DLG_RANDOM, "Counter spell", counter_available_or_target_index, 1,
                               "Creature to hand", creature_available_or_target_player, 1,
                               "Untap permanents", 1, 1, "Mill", player_available_or_mill_count, 1,
                               0, 0, 0, 0);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if (player == 1)
        {
          ai_modifier = ai_modifier + -0x54;
        }
        instance->number_of_targets = '\0';
        choice_cursor = choices;
        counter_available_or_target_index = 0;
        do
        {
          if (spell_fizzled == 1)
          {
            return 0;
          }
          chosen_mode = *choice_cursor;
          if (chosen_mode == CHOICE_BOUNCE)
          {
            creature_available_or_target_player = counter_available_or_target_index + 1;
            new_pick_target(&td_creature, "TARGET_CREATURE", counter_available_or_target_index, 1);
          }
          else if (chosen_mode == CHOICE_MILL)
          {
            creature_available_or_target_player = counter_available_or_target_index + 1;
            new_pick_target(&td_player, "TARGET_PLAYER", counter_available_or_target_index, 1);
          }
          else
          {
            creature_available_or_target_player = counter_available_or_target_index;
            if (chosen_mode == CHOICE_COUNTER)
            {
              creature_available_or_target_player = counter_available_or_target_index + 1;
              counterspell(player, card, EVENT_CAST_SPELL, &td_spell,
                           counter_available_or_target_index);
            }
          }
          choice_cursor = choice_cursor + 1;
          counter_available_or_target_index = creature_available_or_target_player;
        } while (choices + 2 != choice_cursor);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET) ||
           (event == EVENT_CHANGE_TARGET))
  {
    counterspell_target_definition(player, card, &td_spell, 0);
    td_spell.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td_spell.extra = (intptr_t)target_has_black_border;
    counter_available_or_target_index = counterspell(player, card, EVENT_CAN_CAST, &td_spell, -1);
    default_target_definition(player, card, &td_creature, 2);
    creature_available_or_target_player = can_target(&td_creature);
    default_target_definition(player, card, &td_player, 0);
    td_player.zone = TARGET_ZONE_PLAYERS;
    player_available_or_mill_count = can_target(&td_player);
    choices[0] =
        dialog_impl(player, card, event, DLG_CHOOSE_TWO(choices + 1), DLG_SORT_RESULTS, DLG_RANDOM,
                    "Counter spell", counter_available_or_target_index, 1, "Creature to hand",
                    creature_available_or_target_player, 1, "Untap permanents", 1, 1, "Mill",
                    player_available_or_mill_count, 1, 0, 0, 0, 0);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      choice_cursor = choices;
      valid_targets = 0;
      counter_available_or_target_index = 0;
      validation_cursor = choice_cursor;
      do
      {
        chosen_mode = *validation_cursor;
        td = &td_creature;
        if ((chosen_mode == CHOICE_BOUNCE) || (td = &td_player, chosen_mode == CHOICE_MILL))
        {
          bool valid = validate_target(player, card, td, counter_available_or_target_index) != 0;
          if (!valid)
            instance->targets[counter_available_or_target_index].player = -1;
          ++counter_available_or_target_index;
          if (valid)
            ++valid_targets;
        }
        else if (chosen_mode == CHOICE_COUNTER)
        {
          bool valid = counterspell_validate(player, card, &td_spell,
                                             counter_available_or_target_index) != 0;
          if (!valid)
            instance->targets[counter_available_or_target_index].player = -1;
          ++counter_available_or_target_index;
          if (valid)
            ++valid_targets;
        }
        validation_cursor = validation_cursor + 1;
      } while (choices + 2 != validation_cursor);
      counter_available_or_target_index = 0;
      if (valid_targets < 1)
      {
        spell_fizzled = 1;
      }
      else
      {
        do
        {
          chosen_mode = *choice_cursor;
          if (chosen_mode == CHOICE_UNTAP)
          {
            default_test_definition(&test, 0x47);
            test.special_selection_function = test_has_any_watermark;
            new_manipulate_all(player, card, player, &test, ACT_UNTAP);
          }
          else if (chosen_mode == CHOICE_COUNTER || chosen_mode == CHOICE_BOUNCE ||
                   chosen_mode == CHOICE_MILL)
          {
            if (chosen_mode == CHOICE_COUNTER)
            {
              creature_available_or_target_player =
                  instance->targets[counter_available_or_target_index].player;
              if (creature_available_or_target_player >= 0)
                real_counter_a_spell(player, card, creature_available_or_target_player,
                                     instance->targets[counter_available_or_target_index].card);
            }
            else if (chosen_mode == CHOICE_BOUNCE)
            {
              creature_available_or_target_player =
                  instance->targets[counter_available_or_target_index].player;
              if (creature_available_or_target_player >= 0)
                bounce_permanent(creature_available_or_target_player,
                                 instance->targets[counter_available_or_target_index].card);
            }
            else
            {
              player_available_or_mill_count =
                  roll_xd6t(player, 2, player, card,
                            instance->targets[counter_available_or_target_index].player, -1);
              creature_available_or_target_player =
                  instance->targets[counter_available_or_target_index].player;
              if (creature_available_or_target_player >= 0)
                mill(creature_available_or_target_player, player_available_or_mill_count);
            }
            ++counter_available_or_target_index;
          }
          choice_cursor = choice_cursor + 1;
        } while (choices + 2 != choice_cursor);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c6880; original cards/unstable.c:939. */
int card_capital_offense(int player, int card, event_t event)
{
  /* capital offense	|2|B|B
   * instant
   * target creature gets -x/-x until end of turn, where x is the number of times a capital letter appears in its rules text. (ignore reminder text and flavor text.) */

  if (event != EVENT_RESOLVE_SPELL)
    return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -1, -1, 0, 0);

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  if (valid_target(&td))
  {
    card_instance_t *instance = get_card_instance(player, card);
    char rules[3000];
    get_rules_text(rules, instance->targets[0].player, instance->targets[0].card, 1);
    int capitals = 0;
    for (const char *cursor = rules; *cursor; ++cursor)
      capitals += (unsigned char)(*cursor - 'A') < 26;
    if (capitals > 0)
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                     -capitals, -capitals);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c6aa0; original cards/unstable.c:1038. */
int card_old_fashioned_vampire(int player, int card, event_t event)
{
  /* Old-Fashioned Vampire	|3|B|B
   * Creature - Vampyre 3/3
   * Flying
   * ~ gets +2/+2 and has deathtouch as long as it's dark outdoors. */

  if ((uint32_t)(event - EVENT_POWER) < 3)
  {
    unsigned int hm = localtime_hour_minute();
    unsigned int hour = hm >> 16;
    unsigned int minute = hm & 0xffff;
    bool nighttime = hour == 5 ? minute < 30
        : hour == 18 ? minute > 29 : hour - 5U > 13U;
    if (nighttime && !is_humiliated(player, card, AL_STATIC))
    {
      if (event == EVENT_ABILITIES)
        deathtouch(player, card, EVENT_ABILITIES);
      else
        event_result += 2;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c6ed0; original cards/unstable.c:1141. */
int card_sly_spy_a(int player, int card, event_t event)
{
  /* Sly Spy A	|2|B
   * Creature - Human Spy 2/2
   * Whenever Sly Spy deals combat damage to a player, that player reveals their hand. You choose a card from it with the longest name. That player discards that card. */

  int hits;
  card_instance_t *instance;
  int word_count;
  const char *plural_suffix;
  unsigned int packed_damage_or_player;
  int most_words;
  int card_00;
  unsigned int player_pass;
  int i;
  int times_damaged[2];
  ec_definition_t ec;
  test_definition_t test;
  if (damage_dealt_by_me(player, card, event,
                         DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_DAMAGE_PLAYER |
                             DDBM_MUST_BE_COMBAT_DAMAGE) != 0)
  {
    instance = get_card_instance(player, card);
    player_pass = 0;
    packed_damage_or_player = instance->targets[1].player;
    instance->targets[1].player = 0;
    times_damaged[0] = packed_damage_or_player & 0xff;
    times_damaged[1] = packed_damage_or_player >> 8 & 0xff;
    do
    {
      packed_damage_or_player = player_pass ^ current_turn;
      i = 0;
      hits = times_damaged[packed_damage_or_player];
      if (0 < hits)
      {
        do
        {
          most_words = -1;
          card_00 = 0;
          if (0 < active_cards_count[packed_damage_or_player])
          {
            do
            {
              instance = in_hand(packed_damage_or_player, card_00);
              if ((instance != NULL) &&
                  (word_count = words_in_name(packed_damage_or_player, card_00),
                   most_words < word_count))
              {
                most_words = word_count;
              }
              card_00 = card_00 + 1;
            } while (card_00 < active_cards_count[packed_damage_or_player]);
            if (-1 < most_words)
            {
              default_ec_definition(packed_damage_or_player, player, &ec);
              ec.effect = 1;
              new_default_test_definition(&test, 0xff, "");
              test.special_selection_function = test_words_in_name_val;
              if (most_words == 1)
              {
                plural_suffix = "";
              }
              else
              {
                plural_suffix = "s";
              }
              test.value_for_special_selection_function = most_words;
              sprintf(test.message, "Select a card with %d word%s in its name.", most_words,
                      plural_suffix);
              new_effect_coercion(&ec, &test);
            }
          }
          i = i + 1;
        } while (i != hits);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c7170; original cards/unstable.c:1233. */
int card_spike_tournament_grinder(int player, int card, event_t event)
{
  /* Spike, Tournament Grinder	|2|PB|PB
   * Legendary Creature - Human Gamer 1/1
   * (|PB can be paid with either |B or 2 life.)
   * |PB|PB|PB|PB: Choose a card you own from outside the game that has been banned or restricted in a Constructed format, reveal that card, and put it into your hand. */

  int result;
  test_definition_t test;

  phyrexian_casting_costs(player, card, event, 2, 2, 0, 0, 0, 0, 0);
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = has_phyrexian_mana_for_activated_ability(player, card, 0, 4, 0, 0, 0, 0);
    if (result != 0)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x800, 0, 0, 0, 0,
                                              0, 0, 0, NULL, NULL);
      return result;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = charge_phyrexian_mana_for_activated_ability(player, card, 0, 4, 0, 0, 0, 0);
    if (result != 0)
    {
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x800, 0, 0, 0, 0, 0, 0,
                                              0, NULL, NULL);
      return result;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(
        &test, 0xff, "Select a card that has been banned or restricted in a Constructed format.");
    test.special_selection_function = test_has_been_banned_or_restricted;
    generic_wish_effect(player, card, &test, 0x2b);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x25c7470; original cards/unstable.c:1355. */
int card_garbage_elemental_c(int player, int card, event_t event)
{
  /* Garbage Elemental C	|4|R
   * Creature - Elemental 3/2
   * Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
   * When Garbage Elemental enters the battlefield, roll two six-sided dice. Create a number of 1/1 |Sred Goblin creature tokens equal to the difference between those results. */

  int howmany;

  battle_cry(player, card, event);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  howmany = roll_2d6t_difference(player, player, card, -1, -1);
  generate_tokens_by_id(player, card, CARD_ID_GOBLIN, howmany);
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
/* ManalinkEh 2022: 0x25c8380; original cards/unstable.c:1972. */
int card_hot_fix(int player, int card, event_t event)
{
  /* Hot Fix	|4|W|U
   * Sorcery
   * You have ten seconds to look at and rearrange the cards in your library. At the end of those ten seconds, if you're touching one or more of those cards, shuffle your library. */

  if (event != EVENT_RESOLVE_SPELL)
    return basic_spell(player, card, event);
  int32_t started = 0;
  bool offline_ai_rearranged = false;
  if (ai_is_speculating == 1)
    rearrange_top_x(player, player, 500);
  else if (player == 1 && (trace_mode & 2) == 0)
  {
    rearrange_top_x(1, 1, 500);
    offline_ai_rearranged = true;
  }
  else
  {
    started = time_in_ms();
    rearrange_top_x(player, player, 500);
  }
  int speculating_after_rearrange = ai_is_speculating;
  uint32_t needs_shuffle = 0;
  if (!offline_ai_rearranged || speculating_after_rearrange != 1)
  {
    if (speculating_after_rearrange == 1 || (player == 1 && (trace_mode & 2) == 0))
      needs_shuffle = started < -10000;
    else
    {
      int32_t ended = time_in_ms();
      needs_shuffle = (int32_t)((uint32_t)ended - (uint32_t)started) > 10000;
    }
  }
  if ((trace_mode & 2) != 0)
  {
    if (player == 1)
    {
      TENTATIVE_wait_for_network_result(1, 28);
      needs_shuffle = (*shandalar_target_pair_packet_value());
    }
    else
    {
      (*shandalar_target_pair_packet_value()) = needs_shuffle;
      (*shandalar_target_pair_packet_type()) = 28;
      TENTATIVE_send_network_result(0, 28);
    }
  }
  if (needs_shuffle)
    shuffle(player);
  kill_card(player, card, KILL_DESTROY);
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x25c8c00; original cards/unstable.c:2331. */
int card_staff_of_the_letter_magus(int player, int card, event_t event)
{
  /* Staff of the Letter Magus	|3
   * Artifact
   * As ~ enters the battlefield, choose a consonant other than N, R, S, or T.
   * Whenever a player casts a spell, you gain 1 life for each time the chosen letter appears in that spell's name. */

  unsigned char folded_letter;
  char chosen_letter;
  card_instance_t *instance;
  const char *excluded_letter;
  int result;
  unsigned char *name_cursor;
  unsigned int encoded_letter_or_current_char;
  unsigned char next_char;
  if (event == EVENT_RESOLVE_SPELL)
  {
    chosen_letter = choose_a_consonant_not_nrst(player, player, card);
    make_txtfx(player, card, fx_staff_of_the_letter_magus, CARD_ID_STAFF_OF_THE_LETTER_MAGUS,
               (int)chosen_letter);
    sp_ability_trigger(2, EVENT_RESOLVE_SPELL, 2, TYPE_ANY, NULL, player, card);
    return 0;
  }
  sp_ability_trigger(2, event, 2, TYPE_ANY, NULL, player, card);
  if (event != EVENT_SPA)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  encoded_letter_or_current_char = instance->targets[5].player;
  if ((short)(encoded_letter_or_current_char >> 0x10) != 0x46b5)
  {
    return 0;
  }
  encoded_letter_or_current_char = encoded_letter_or_current_char & 0xffff;
  if (0x18 < encoded_letter_or_current_char - 0x42)
  {
    return 0;
  }
  excluded_letter = strchr("AEIOUNRST", encoded_letter_or_current_char);
  if (excluded_letter != NULL)
  {
    return 0;
  }
  chosen_letter = (char)encoded_letter_or_current_char + ' ';
  result = get_card_name(trigger_cause_controller, trigger_cause);
  if (result == 0x45a5)
  {
    encoded_letter_or_current_char = 0x4f;
    name_cursor =
        (unsigned char *)"Our Market Research Shows That Players Like Really Long Card Names So We "
                         "Made this Card to Have the Absolute Longest Card Name Ever Elemental";
  }
  else if (result == 0x45d5)
  {
    name_cursor =
        (unsigned char *)"The Ultimate Nightmare of Wizards of the Coast Customer Service";
    encoded_letter_or_current_char = 0x54;
  }
  else
  {
    name_cursor = (unsigned char *)get_card_name_by_id(result);
    encoded_letter_or_current_char = (unsigned int)*name_cursor;
    if (*name_cursor == 0)
      return 0;
  }
  next_char = name_cursor[1];
  result = 0;
  while (true)
  {
    folded_letter = lc_unaccent_map[encoded_letter_or_current_char];
    if (folded_letter == 0xdf)
    {
      if (chosen_letter == 's')
      {
        result = result + 2;
      }
    }
    else if ((folded_letter & 0xdf) == 0xde)
    {
      if ((chosen_letter == 't') || (chosen_letter == 'h'))
      {
        result = result + 1;
      }
    }
    else if ((unsigned int)folded_letter == (int)chosen_letter)
    {
      result = result + (unsigned int)((unsigned char)(folded_letter + 0x9f) < 0x1a);
    }
    if (next_char == 0)
      break;
    encoded_letter_or_current_char = (unsigned int)next_char;
    next_char = name_cursor[2];
    name_cursor = name_cursor + 1;
  }
  if (0 < result)
  {
    gain_life(player, result);
  }
  return 0;
}
#endif
