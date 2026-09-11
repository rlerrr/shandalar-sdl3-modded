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

/* GO TO JAIL	|W
 * Enchantment
 * When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield.
 * At the beginning of the upkeep of the exiled card's owner, that player rolls two six-sided dice. If they roll doubles, sacrifice ~. */

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

/* Knight of the Widget	|2|W
 * Artifact Creature - Cyborg Knight 100/100
 * Vigilance
 * ~'s power and toughness are each equal to the number of Order of the Widget watermarks among permanents you control. */

/* Midlife Upgrade	|3|W
 * Sorcery
 * As an additional cost to cast this spell, sacrifice X Contraptions.
 * Assemble X plus one Contraptions. (To assemble a Contraption, put the top card of your Contraption deck face up onto one of your sprockets.) */

/* Oddly Uneven	|3|W|W
 * Sorcery
 * Choose one -
 * * Destroy each creature with an odd number of words in its name. (Hyphenated words are one word.)
 * * Destroy each creature with an even number of words in its name. */

/* Old Guard	|1|W
 * Artifact Creature - Cyborg Soldier 2/1
 * |W, |T: Tap target creature without reminder text. (Reminder text is still any italicized text in parentheses that explains rules you already know.) */

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

/* Crow Storm	|2|U
 * Sorcery
 * Create a 1/2 |Sblue Bird creature token with flying named Storm Crow.
 * Storm (When you cast this spell, copy it for each spell cast before it this turn.) */

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

/* Time Out	|4|U
 * Instant
 * Roll a six-sided die. Put target nonland permanent into its owner's library just beneath the top X cards of that library, where X is the result. */

/* Very Cryptic Command A	|1|U|U|U
 * Instant
 * Choose two -
 * * Switch target creature's power and toughness until end of turn.
 * * Target creature can't be blocked this turn.
 * * Draw a card. If that card's art is by Wayne England, you may reveal it and draw another card.
 * * Assemble a Contraption. */

/* Very Cryptic Command B	|1|U|U|U
 * Instant
 * Choose two -
 * * Untap two target permanents.
 * * Tap each permanent target player controls with exactly one word in its name.
 * * Discard all the cards in your hand, then draw that many cards.
 * * Return target instant or sorcery card from your graveyard to your hand. */

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

/* Very Cryptic Command E	|1|U|U|U
 * Instant
 * Choose two -
 * * Counter target black-bordered spell.
 * * Return target creature to its owner's hand.
 * * Untap each permanent you control with a watermark.
 * * Roll two six-sided dice. Target player puts the top X cards of their library into their graveyard, where X is the total of those results. */

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

/* Inhumaniac	|1|B
 * Creature - Brainiac 1/1
 * At the beginning of your upkeep, roll a six-sided die. On a 3 or 4, put a +1/+1 counter on ~. On a 5 or higher, put two +1/+1 counters on it. On a 1, remove all +1/+1 counters from ~. */

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

/* Old-Fashioned Vampire	|3|B|B
 * Creature - Vampyre 3/3
 * Flying
 * ~ gets +2/+2 and has deathtouch as long as it's dark outdoors. */

/* Over My Dead Bodies	|4|B|B
 * Enchantment
 * Creature cards in graveyards can attack and block as though they were on the battlefield, can block or be blocked only by creature cards in graveyards, are Zombies in addition to their other types, and have undeathtouch. (If they would deal damage to a creature card, exile that creature card instead.)
 * Creature cards in your graveyard have haste. */

/* Overt Operative	|3|B
 * Creature - Human Ninja Rigger 2/3
 * Menace
 * Whenever ~ deals combat damage to a player, it assembles a Contraption. (Put the top card of your Contraption deck face up onto one of your sprockets.) */

/* "Rumors of My Death . . ."	|2|B
 * Enchantment
 * |3|B, Exile a permanent you control with a League of Dastardly Doom watermark: Return a permanent card with a League of Dastardly Doom watermark from your graveyard to the battlefield. */

/* Skull Saucer	|4|B|B
 * Creature - Zombie Head 4/1
 * Flying
 * When ~ enters the battlefield, destroy target creature and put your head on the table. Sacrifice ~ when your head stops touching the table. */

/* Sly Spy A	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, that player reveals their hand. You choose a card from it with the longest name. That player discards that card. */

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

/* Sly Spy F	|2|B
 * Creature - Human Spy 2/2
 * Whenever Sly Spy deals combat damage to a player, roll a six-sided die. That player loses life equal to the result. */

/* Snickering Squirrel	|B
 * Creature - Squirrel Advisor 1/1
 * You may tap ~ to increase the result of a die any player rolled by 1. */

/* Spike, Tournament Grinder	|2|PB|PB
 * Legendary Creature - Human Gamer 1/1
 * (|PB can be paid with either |B or 2 life.)
 * |PB|PB|PB|PB: Choose a card you own from outside the game that has been banned or restricted in a Constructed format, reveal that card, and put it into your hand. */

/* Squirrel-Powered Scheme	|2|B
 * Enchantment
 * Increase the result of each die you roll by 2. */

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

/* Box of Free-Range Goblins	|4|R|R
 * Sorcery
 * Roll a six-sided die. Create a number of 1/1 |Sred Goblin creature tokens equal to the result. */

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

/* Garbage Elemental C	|4|R
 * Creature - Elemental 3/2
 * Battle cry (Whenever this creature attacks, each other attacking creature gets +1/+0 until end of turn.)
 * When Garbage Elemental enters the battlefield, roll two six-sided dice. Create a number of 1/1 |Sred Goblin creature tokens equal to the difference between those results. */

/* Garbage Elemental D	|4|R
 * Creature - Elemental 3/3
 * Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom of your library in a random order.)
 * When Garbage Elemental enters the battlefield, roll a six-sided die. Garbage Elemental deals damage equal to the result to target opponent or planeswalker. */

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

/* Hammer Helper	|3|R
 * Sorcery
 * Gain control of target creature until end of turn. Untap that creature and roll a six-sided die. Until end of turn, it gains haste and gets +X/+0, where X is the result. */

/* Hammer Jammer	|3|R
 * Creature - Goblin Warrior 0/0
 * As ~ enters the battlefield, roll a six-sided die. ~ enters the battlefield with a number of +1/+1 counters on it equal to the result.
 * Whenever you roll a die, remove all +1/+1 counters from ~, then put a number of +1/+1 counters on it equal to the result. */

/* Hammerfest Boomtacular	|3|R|R
 * Enchantment
 * Whenever you cast a spell with a Goblin Explosioneers watermark, ~ deals 2 damage to any target. */

/* Infinity Elemental	|4|R|R|R
 * Creature - Elemental 100/5
 * (This creature has INFINITE POWER.) */

/* It That Gets Left Hanging	|5|R
 * Creature - Elemental Horror 5/4
 * When ~ enters the battlefield, ask a person outside the game to high-five you. If they won't, ~ gains haste until end of turn. */

/* Just Desserts	|1|R
 * Instant
 * ~ deals pi damage to target creature. (Pi is the ratio of a circle's circumference to its diameter. (It's a smidgen more than 3.)) */

/* Painiac	|2|R
 * Creature - Brainiac 0/3
 * At the beginning of your upkeep, roll a six-sided die. ~ gets +X/+0 until end of turn, where X is the result. */

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

/* Druid of the Sacred Beaker	|2|G
 * Creature - Deer Bird Ape Druid 2/2
 * |T: Add |G for each Crossbreed Labs watermark among permanents you control. */

/* Eager // Beaver	|2|G
 * Host Creature - Beaver 3/2
 * When this creature enters the battlefield,
 * //
 * you may untap target permanent. */

/* Earl of Squirrel	|4|G|G
 * Creature - Squirrel Advisor 4/4
 * Squirrellink (Damage dealt by this creature also causes you to create that many 1/1 |Sgreen Squirrel creature tokens.)
 * Creature tokens you control are Squirrels in addition to their other creature types.
 * Other Squirrels you control get +1/+1. */

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

/* Hydradoodle	|X|X|G|G
 * Creature - Hydra Hound 0/0
 * As ~ enters the battlefield, roll X six-sided dice. ~ enters the battlefield with a number of +1/+1 counters on it equal to the total of those results.
 * Reach, trample */

/* Ineffable Blessing A	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose Flavorful or Bland.
 * * Flavorful - Whenever a creature with flavor text enters the battlefield under your control, draw a card.
 * * Bland - Whenever a creature without flavor text enters the battlefield under your control, draw a card. */

/* Ineffable Blessing B	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose an artist.
 * Whenever a creature with art by the chosen artist enters the battlefield under your control, draw a card. */

/* Ineffable Blessing C	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose white-bordered or silver-bordered.
 * Whenever a creature with the chosen border enters the battlefield under your control, draw a card. */

/* Ineffable Blessing D	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose a rarity.
 * Whenever a creature with the chosen rarity enters the battlefield under your control, draw a card. */

/* Ineffable Blessing E	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose odd or even.
 * Whenever a creature with a collector number of the chosen value enters the battlefield under your control, draw a card. */

/* Ineffable Blessing F	|1|G
 * Enchantment
 * As Ineffable Blessing enters the battlefield, choose a number.
 * Whenever a creature with exactly the chosen number of words in its name enters the battlefield under your control, draw a card. (Hyphenated words are one word.) */

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

/* Hot Fix	|4|W|U
 * Sorcery
 * You have ten seconds to look at and rearrange the cards in your library. At the end of those ten seconds, if you're touching one or more of those cards, shuffle your library. */

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

/* Border Guardian	|4
 * Artifact Creature - Knight 1/1
 * Whenever you cast a silver-bordered spell, put a +1/+1 counter on ~.
 * Whenever you cast a black-bordered spell, ~ can't be blocked this turn.
 * Whenever you cast a white-bordered spell, ~ gains double strike until end of turn. */

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

/* Everythingamajig B	|5
 * Artifact
 * |2, |T: Draw a card. Activate this ability only if you have no cards in hand.
 * |8, |T: You gain 10 life.
 * |4, |T, Sacrifice Everythingamajig: You may put a silver-bordered permanent card from your hand onto the battlefield. */

/* Everythingamajig C	|5
 * Artifact
 * |1: Flip a coin. If you win the flip, add |C|C. Activate this ability only any time you could cast an instant.
 * |3, |T: Target player discards a card. Activate this ability only during your turn.
 * |X: Everythingamajig becomes an X/X Construct artifact creature until end of turn. */

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

/* Staff of the Letter Magus	|3
 * Artifact
 * As ~ enters the battlefield, choose a consonant other than N, R, S, or T.
 * Whenever a player casts a spell, you gain 1 life for each time the chosen letter appears in that spell's name. */

/* Stamp of Approval	|3
 * Artifact
 * As ~ enters the battlefield, choose a watermark.
 * Creatures you control with the chosen watermark get +1/+1. */

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
