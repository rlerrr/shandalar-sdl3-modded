#include "manalink.h"

/* Aminatou's Augury	|6|U|U
 * Sorcery
 * Exile the top eight cards of your library. You may put a land card from among them onto the battlefield. Until end of turn, for each nonland card type, you may cast a card of that type from among the exiled cards without paying its mana cost. */

/* Aminatou, the Fateshifter	|W|U|B
 * Legendary Planeswalker - Aminatou (3)
 * +1: Draw a card, then put a card from your hand on top of your library.
 * -1: Exile another target permanent you own, then return it to the battlefield under your control.
 * -6: Choose left or right. Each player gains control of all nonland permanents other than ~ controlled by the next player in the chosen direction.
 * ~ can be your commander. */

/* Ancient Stone Idol	|10
 * Artifact Creature - Golem 12/12
 * Flash
 * This spell costs |1 less to cast for each attacking creature.
 * Trample
 * When ~ dies, create a 6/12 colorless Construct artifact creature token with trample. */

/* Arixmethes, Slumbering Isle	|2|G|U
 * Legendary Creature - Kraken 12/12
 * ~ enters the battlefield tapped with five slumber counters on it.
 * As long as ~ has a slumber counter on it, it's a land. (It's not a creature.)
 * Whenever you cast a spell, you may remove a slumber counter from ~.
 * |T: Add |G|U. */

/* Bloodtracker	|3|B
 * Creature - Vampire Wizard 2/2
 * Flying
 * |B, Pay 2 life: Put a +1/+1 counter on ~.
 * When ~ leaves the battlefield, draw a card for each +1/+1 counter on it. */

/* Boreas Charger	|2|W
 * Creature - Pegasus 2/1
 * Flying
 * When ~ leaves the battlefield, choose an opponent who controls more lands than you. Search your library for a number of |H1Plains cards equal to the difference and reveal them. Put one of them onto the battlefield tapped and the rest into your hand. Then shuffle your library. */

/* Brudiclad, Telchor Engineer	|4|U|R
 * Legendary Artifact Creature - Artificer 4/4
 * Creature tokens you control have haste.
 * At the beginning of combat on your turn, create a 2/1 |Sblue Myr artifact creature token. Then you may choose a token you control. If you do, each other token you control becomes a copy of that token. */

/* Coveted Jewel	|6
 * Artifact
 * When ~ enters the battlefield, draw three cards.
 * |T: Add three mana of any one color.
 * Whenever one or more creatures an opponent controls attack you and aren't blocked, that player draws three cards and gains control of ~. Untap it. */

/* Crash of Rhino Beetles	|4|G
 * Creature - Insect 5/5
 * Trample
 * ~ gets +10/+10 as long as you control ten or more lands. */

/* Echo Storm	|3|U|U
 * Sorcery
 * When you cast this spell, copy it for each time you've cast your commander from the command zone this game. You may choose new targets for the copies.
 * Create a token that's a copy of target artifact. */

/* Emissary of Grudges	|5|R
 * Creature - Efreet 6/5
 * Flying, haste
 * As ~ enters the battlefield, secretly choose an opponent.
 * Reveal the player you chose: Choose new targets for target spell or ability if it's controlled by the chosen player and if it targets you or a permanent you control. Activate this ability only once. */

/* Empyrial Storm	|4|W|W
 * Sorcery
 * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
 * Create a 4/4 |Swhite Angel creature token with flying. */

/* Enchanter's Bane	|1|R
 * Enchantment
 * At the beginning of your end step, target enchantment deals damage equal to its converted mana cost to its controller unless that player sacrifices it. */

/* Endless Atlas	|2
 * Artifact
 * |2, |T: Draw a card. Activate this ability only if you control three or more lands with the same name. */

/* Entreat the Dead	|X|X|B|B|B
 * Sorcery
 * Return X target creature cards from your graveyard to the battlefield.
 * Miracle |X|B|B (You may cast this card for its miracle cost when you draw it if it's the first card you drew this turn.) */

/* Estrid's Invocation	|2|U
 * Enchantment
 * You may have ~ enter the battlefield as a copy of any enchantment you control, except it has "At the beginning of your upkeep, you may exile this enchantment. If you do, return it to the battlefield under its owner's control." */

/* Estrid, the Masked	|1|G|W|U
 * Legendary Planeswalker - Estrid (3)
 * +2: Untap each enchanted permanent you control.
 * -1: Create a |Swhite Aura enchantment token named Mask attached to another target permanent. The token has enchant permanent and totem armor.
 * -7: Put the top seven cards of your library into your graveyard. Return all non-Aura enchantment cards from your graveyard to the battlefield, then do the same for Aura cards.
 * ~ can be your commander. */

/* Ever-Watching Threshold	|2|U
 * Enchantment
 * Whenever an opponent attacks you and/or a planeswalker you control with one or more creatures, draw a card. */

/* Forge of Heroes	""
 * Land
 * |T: Add |C.
 * |T: Choose target commander that entered the battlefield this turn. Put a +1/+1 counter on it if it's a creature and a loyalty counter on it if it's a planeswalker. */

/* Fury Storm	|2|R|R
 * Instant
 * When you cast this spell, copy it for each time you've cast your commander from the command zone this game. You may choose new targets for the copies.
 * Copy target instant or sorcery spell. You may choose new targets for the copy. */

/* Genesis Storm	|4|G|G
 * Sorcery
 * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
 * Reveal cards from the top of your library until you reveal a nonland permanent card. You may put that card onto the battlefield. Then put all cards revealed this way that weren't put onto the battlefield on the bottom of your library in a random order. */

/* Geode Golem	|5
 * Artifact Creature - Golem 5/3
 * Trample
 * Whenever ~ deals combat damage to a player, you may cast your commander from the command zone without paying its mana cost. (You still pay any additional costs.) */

/* Gyrus, Waker of Corpses	|X|B|R|G
 * Legendary Creature - Hydra 0/0
 * ~ enters the battlefield with a number of +1/+1 counters on it equal to the amount of mana spent to cast it.
 * Whenever ~ attacks, you may exile target creature card with lesser power from your graveyard. If you do, create a token that's a copy of that card and that's tapped and attacking. Exile the token at end of combat. */

/* Heavenly Blademaster	|5|W
 * Creature - Angel 3/6
 * Flying, double strike
 * When ~ enters the battlefield, you may attach any number of Auras and Equipment you control to it.
 * Other creatures you control get +1/+1 for each Aura and Equipment attached to ~. */

/* Isolated Watchtower	""
 * Land
 * |T: Add |C.
 * |2, |T: Scry 1, then you may reveal the top card of your library. If a basic land card is revealed this way, put it onto the battlefield tapped. Activate this ability only if an opponent controls at least two more lands than you. */

/* Kestia, the Cultivator	|1|G|W|U
 * Legendary Enchantment Creature - Nymph 4/4
 * Bestow |3|G|W|U (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
 * Enchanted creature gets +4/+4.
 * Whenever an enchanted creature or enchantment creature you control attacks, draw a card. */

/* Lord Windgrace	|2|B|R|G
 * Legendary Planeswalker - Windgrace (5)
 * +2: Discard a card, then draw a card. If a land card is discarded this way, draw an additional card.
 * -3: Return up to two target land cards from your graveyard to the battlefield.
 * -11: Destroy up to six target nonland permanents, then create six 2/2 |Sgreen Cat Warrior creature tokens with |H2forestwalk.
 * ~ can be your commander. */

/* Loyal Apprentice	|1|R
 * Creature - Human Artificer 2/1
 * Haste
 * Lieutenant - At the beginning of combat on your turn, if you control your commander, create a 1/1 colorless Thopter artifact creature token with flying. That token gains haste until end of turn. */

/* Loyal Drake	|2|U
 * Creature - Drake 2/2
 * Flying
 * Lieutenant - At the beginning of combat on your turn, if you control your commander, draw a card. */

/* Loyal Guardian	|4|G
 * Creature - Rhino 4/4
 * Trample
 * Lieutenant - At the beginning of combat on your turn, if you control your commander, put a +1/+1 counter on each creature you control. */

/* Loyal Subordinate	|2|B
 * Creature - Zombie 3/1
 * Menace
 * Lieutenant - At the beginning of combat on your turn, if you control your commander, each opponent loses 3 life. */

/* Loyal Unicorn	|3|W
 * Creature - Unicorn 3/4
 * Vigilance
 * Lieutenant - At the beginning of combat on your turn, if you control your commander, prevent all combat damage that would be dealt to creatures you control this turn. Other creatures you control gain vigilance until end of turn. */

/* Magus of the Balance	|1|W
 * Creature - Human Wizard 2/2
 * |4|W, |T, Sacrifice ~: Each player chooses a number of lands they control equal to the number of lands controlled by the player who controls the fewest, then sacrifices the rest. Players discard cards and sacrifice creatures the same way. */

/* Myth Unbound	|2|G
 * Enchantment
 * Your commander costs |1 less to cast for each time it's been cast from the command zone this game.
 * Whenever your commander is put into the command zone from anywhere, draw a card. */

/* Nesting Dragon	|3|R|R
 * Creature - Dragon 5/4
 * Flying
 * Landfall - Whenever a land enters the battlefield under your control, create a 0/2 |Sred Dragon Egg creature token with defender and "When this creature dies, create a 2/2 |Sred Dragon creature token with flying and '|R: This creature gets +1/+0 until end of turn.'" */

/* Night Incarnate	|4|B
 * Creature - Elemental 3/4
 * Deathtouch
 * When ~ leaves the battlefield, all creatures get -3/-3 until end of turn.
 * Evoke |3|B (You may cast this spell for its evoke cost. If you do, it's sacrificed when it enters the battlefield.) */

/* Nylea's Colossus	|6|G
 * Enchantment Creature - Giant 6/6
 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, double target creature's power and toughness until end of turn. */

/* Octopus Umbra	|3|U|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has base power and toughness 8/8 and has "Whenever this creature attacks, you may tap target creature with power 8 or less."
 * Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

/* Primordial Mist	|4|U
 * Enchantment
 * At the beginning of your end step, you may manifest the top card of your library. (Put it onto the battlefield face down as a 2/2 creature. Turn it face up any time for its mana cost if it's a creature card.)
 * Exile a face-down permanent you control face up: You may play that card this turn. (You still pay its costs. Timing rules still apply.) */

/* Ravenous Slime	|2|G
 * Creature - Ooze 1/1
 * ~ can't be blocked by creatures with power 2 or less.
 * If a creature an opponent controls would die, instead exile it and put a number of +1/+1 counters equal to that creature's power on ~. */

/* Reality Scramble	|2|R|R
 * Sorcery
 * Put target permanent you own on the bottom of your library. Reveal cards from the top of your library until you reveal a card that shares a card type with that permanent. Put that card onto the battlefield and the rest on the bottom of your library in a random order.
 * Retrace (You may cast this card from your graveyard by discarding a land card in addition to paying its other costs.) */

/* Retrofitter Foundry	|1
 * Artifact
 * |3: Untap ~.
 * |2, |T: Create a 1/1 colorless Servo artifact creature token.
 * |1, |T, Sacrifice a Servo: Create a 1/1 colorless Thopter artifact creature token with flying.
 * |T, Sacrifice a Thopter: Create a 4/4 colorless Construct artifact creature token. */

/* Saheeli's Directive	|X|R|R|R
 * Sorcery
 * Improvise (Each artifact you tap after you're done activating mana abilities pays for |1.)
 * Reveal the top X cards of your library. You may put any number of artifact cards with converted mana cost X or less from among them onto the battlefield. Then put all cards revealed this way that weren't put onto the battlefield into your graveyard. */

/* Saheeli, the Gifted	|2|U|R
 * Legendary Planeswalker - Saheeli (4)
 * +1: Create a 1/1 colorless Servo artifact creature token.
 * +1: The next spell you cast this turn costs |1 less to cast for each artifact you control as you cast it.
 * -7: For each artifact you control, create a token that's a copy of it. Those tokens gain haste. Exile those tokens at the beginning of the next end step.
 * ~ can be your commander. */

/* Skull Storm	|7|B|B
 * Sorcery
 * When you cast this spell, copy it for each time you've cast your commander from the command zone this game.
 * Each opponent sacrifices a creature. Each opponent who can't loses half their life, rounded up. */

/* Sower of Discord	|4|B|B
 * Creature - Demon 6/6
 * Flying
 * As ~ enters the battlefield, choose two players.
 * Whenever damage is dealt to one of the chosen players, the other chosen player also loses that much life. */

/* Tawnos, Urza's Apprentice	|U|R
 * Legendary Creature - Human Artificer 1/3
 * Haste
 * |U|R, |T: Copy target activated or triggered ability you control from an artifact source. You may choose new targets for the copy. (Mana abilities can't be targeted.) */

/* Thantis, the Warweaver	|3|B|R|G
 * Legendary Creature - Spider 5/5
 * Vigilance, reach
 * All creatures attack each combat if able.
 * Whenever a creature attacks you or a planeswalker you control, put a +1/+1 counter on ~. */

/* Treasure Nabber	|2|R
 * Creature - Goblin Rogue 3/2
 * Whenever an opponent taps an artifact for mana, gain control of that artifact until the end of your next turn. */

/* Turntimber Sower	|2|G
 * Creature - Elf Druid 3/3
 * Whenever one or more land cards are put into your graveyard from anywhere, create a 0/1 |Sgreen Plant creature token.
 * |G, Sacrifice three creatures: Return target land card from your graveyard to your hand. */

/* Tuvasa the Sunlit	|G|W|U
 * Legendary Creature - Merfolk Shaman 1/1
 * ~ gets +1/+1 for each enchantment you control.
 * Whenever you cast your first enchantment spell each turn, draw a card. */

/* Varchild, Betrayer of Kjeldor	|2|R
 * Legendary Creature - Human Knight 3/3
 * Whenever ~ deals combat damage to a player, that player creates that many 1/1 |Sred Survivor creature tokens.
 * Survivors your opponents control can't block, and they can't attack you or a planeswalker you control.
 * When ~ leaves the battlefield, gain control of all Survivors. */

/* Varina, Lich Queen	|1|W|U|B
 * Legendary Creature - Zombie Wizard 4/4
 * Whenever you attack with one or more Zombies, draw that many cards, then discard that many cards. You gain that much life.
 * |2, Exile two cards from your graveyard: Create a tapped 2/2 |Sblack Zombie creature token. */

/* Vedalken Humiliator	|3|U
 * Creature - Vedalken Wizard 3/4
 * Metalcraft - Whenever ~ attacks, if you control three or more artifacts, creatures your opponents control lose all abilities and have base power and toughness 1/1 until end of turn. */

/* Whiptongue Hydra	|5|G
 * Creature - Lizard Hydra 4/4
 * Reach
 * When ~ enters the battlefield, destroy all creatures with flying. Put a +1/+1 counter on ~ for each creature destroyed this way. */

/* Windgrace's Judgment	|3|B|G
 * Instant
 * For any number of opponents, destroy target nonland permanent that player controls. */

/* Xantcha, Sleeper Agent	|1|B|R
 * Legendary Creature - Minion 5/5
 * As ~ enters the battlefield, an opponent of your choice gains control of it.
 * ~ attacks each combat if able and can't attack its owner or planeswalkers its owner controls.
 * |3: ~'s controller loses 2 life and you draw a card. Any player may activate this ability. */

/* Yennett, Cryptic Sovereign	|2|W|U|B
 * Legendary Creature - Sphinx 3/5
 * Flying, vigilance, menace
 * Whenever ~ attacks, reveal the top card of your library. If that card's converted mana cost is odd, you may cast it without paying its mana cost. Otherwise, draw a card. */

/* Yuriko, the Tiger's Shadow	|1|U|B
 * Legendary Creature - Human Ninja 1/3
 * Commander ninjutsu |U|B (|U|B, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand or the command zone tapped and attacking.)
 * Whenever a Ninja you control deals combat damage to a player, reveal the top card of your library and put that card into your hand. Each opponent loses life equal to that card's converted mana cost. */
