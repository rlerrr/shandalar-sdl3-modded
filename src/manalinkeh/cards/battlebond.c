#include "manalink.h"

/* Arcane Artisan	|2|U
 * Creature - Human Wizard 0/3
 * |2|U, |T: Target player draws a card, then exiles a card from their hand. If a creature card is exiled this way, that player creates a token that's a copy of that card.
 * When ~ leaves the battlefield, exile all tokens created with it at the beginning of the next end step. */

/* Archfiend of Despair	|6|B|B
 * Creature - Demon 6/6
 * Flying
 * Your opponents can't gain life.
 * At the beginning of each end step, each opponent loses life equal to the life that player lost this turn. */

/* Archon of Valor's Reach	|4|G|W
 * Creature - Archon 5/6
 * Flying, vigilance, trample
 * As ~ enters the battlefield, choose artifact, enchantment, instant, sorcery, or planeswalker.
 * Players can't cast spells of the chosen type. */

/* Arena Rector	|3|W
 * Creature - Human Cleric 1/2
 * When ~ dies, you may exile it. If you do, search your library for a planeswalker card, put it onto the battlefield, then shuffle your library. */

/* Aurora Champion	|2|W
 * Creature - Elf Warrior 3/2
 * Whenever ~ attacks, if your team controls another Warrior, tap target creature. */

/* Azra Bladeseeker	|2|R
 * Creature - Azra Warrior 3/2
 * When ~ enters the battlefield, each player on your team may discard a card, then each player who discarded a card this way draws a card. */

/* Azra Oddsmaker	|1|B|R
 * Creature - Azra Warrior 3/3
 * At the beginning of combat on your turn, you may discard a card. If you do, choose a creature. Whenever that creature deals combat damage to a player this turn, you draw two cards. */

/* Blaring Captain	|3|B
 * Creature - Azra Warrior 2/2
 * Partner with Blaring Recruiter
 * Whenever ~ attacks, attacking Warriors get +1/+1 until end of turn. */

/* Blaring Recruiter	|3|W
 * Creature - Elf Warrior 2/2
 * Partner with Blaring Captain
 * |2|W: Create a 1/1 |Swhite Warrior creature token. */

/* Bloodborn Scoundrels	|5|B
 * Creature - Vampire Rogue 4/4
 * Assist
 * When ~ enters the battlefield, target opponent loses 2 life and you gain 2 life. */

/* Bonus Round	|1|R|R
 * Sorcery
 * Until end of turn, whenever a player casts an instant or sorcery spell, that player copies it and may choose new targets for the copy. */

/* Bountiful Promenade	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |G or |W. */

/* Bramble Sovereign	|2|G|G
 * Creature - Dryad 4/4
 * Whenever another nontoken creature enters the battlefield, you may pay |1|G. If you do, that creature's controller creates a token that's a copy of that creature. */

/* Brightling	|1|W|W
 * Creature - Shapeshifter 3/3
 * |W: ~ gains vigilance until end of turn.
 * |W: ~ gains lifelink until end of turn.
 * |W: Return ~ to its owner's hand.
 * |1: ~ gets +1/-1 or -1/+1 until end of turn. */

/* Bring Down	|3|W
 * Sorcery
 * Assist
 * Destroy target creature with power 4 or greater. */

/* Bull-Rush Bruiser	|3|R
 * Creature - Minotaur Warrior 4/3
 * Whenever ~ attacks, if your team controls another Warrior, ~ gains first strike until end of turn. */

/* Chakram Retriever	|4|U
 * Creature - Elemental Hound 2/4
 * Partner with Chakram Slinger
 * Whenever you cast a spell during your turn, untap target creature. */

/* Chakram Slinger	|4|R
 * Creature - Human Warrior 2/4
 * Partner with Chakram Retriever
 * |R, |T: ~ deals 2 damage to target player or planeswalker. */

/* Charging Binox	|7|G
 * Creature - Beast 7/5
 * Assist
 * Trample */

/* Cheering Fanatic	|1|R
 * Creature - Goblin 2/2
 * Whenever ~ attacks, choose a card name. Spells with the chosen name cost |1 less to cast this turn. */

/* Combo Attack	|2|G
 * Sorcery
 * Two target creatures your team controls each deal damage equal to their power to target creature. */

/* Decorated Champion	|1|G
 * Creature - Elf Warrior 2/2
 * Whenever another Warrior enters the battlefield under your team's control, put a +1/+1 counter on ~. */

/* Dwarven Lightsmith	|5|W
 * Creature - Dwarf Cleric 3/4
 * Assist
 * When ~ enters the battlefield, creatures your team controls get +1/+1 until end of turn. */

/* Fan Favorite	|3|B
 * Creature - Human Rogue 2/2
 * Assist
 * |2: ~ gets +1/+1 until end of turn. Any player may activate this ability. */

/* Fumble	|1|U
 * Instant
 * Return target creature to its owner's hand. Gain control of all Auras and Equipment that were attached to it, then attach them to another creature. */

/* Game Plan	|5|U
 * Sorcery
 * Assist
 * Each player shuffles their hand and graveyard into their library, then draws seven cards. Exile ~. */

/* Gang Up	|X|B
 * Instant
 * Assist
 * Destroy target creature with power X or less. */

/* Generous Patron	|2|G
 * Creature - Elf Advisor 1/4
 * When ~ enters the battlefield, support 2.
 * Whenever you put one or more counters on a creature you don't control, draw a card. */

/* Gorm the Great	|3|G
 * Legendary Creature - Giant Warrior 2/7
 * Partner with Virtus the Veiled
 * Vigilance
 * ~ must be blocked if able, and ~ must be blocked by two or more creatures if able. */

/* Grothama, All-Devouring	|3|G|G
 * Legendary Creature - Wurm 10/8
 * Other creatures have "Whenever this creature attacks, you may have it fight ~."
 * When ~ leaves the battlefield, each player draws cards equal to the amount of damage dealt to ~ this turn by sources they controlled. */

/* Huddle Up	|2|U
 * Sorcery
 * Assist
 * Two target players each draw a card. */

/* Impetuous Protege	|2|R
 * Creature - Human Warrior 0/4
 * Partner with Proud Mentor
 * Whenever ~ attacks, it gets +X/+0 until end of turn, where X is the greatest power among tapped creatures your opponents control. */

/* Inner Demon	|2|B|B
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2, has flying, and is a Demon in addition to its other types.
 * When ~ enters the battlefield, all non-Demon creatures get -2/-2 until end of turn. */

/* Jubilant Mascot	|2|W
 * Creature - Homunculus 1/1
 * At the beginning of combat on your turn, you may pay |3|W. If you do, support 2. */

/* Jungle Wayfinder	|2|G
 * Creature - Elf Warrior 3/3
 * When ~ enters the battlefield, each player may search their library for a basic land card, reveal it, put it into their hand, then shuffle their library. */

/* Khorvath Brightflame	|5|R
 * Legendary Creature - Dragon 3/4
 * Partner with Sylvia Brightspear
 * Flying, haste
 * Knights your team controls have flying and haste. */

/* Khorvath's Fury	|4|R
 * Sorcery
 * For each player, choose friend or foe. Each friend discards all cards from their hand, then draws that many cards plus one. ~ deals damage to each foe equal to the number of cards in their hand. */

/* Krav, the Unredeemed	|4|B
 * Legendary Creature - Demon 3/3
 * Partner with Regna, the Redeemer
 * |B, Sacrifice X creatures: Target player draws X cards and gains X life. Put X +1/+1 counters on ~. */

/* Last One Standing	|1|B|R
 * Sorcery
 * Choose a creature at random, then destroy the rest. */

/* Lava-Field Overlord	|7|R|R
 * Creature - Dragon 5/4
 * Assist
 * Flying
 * When ~ enters the battlefield, it deals 4 damage to target creature an opponent controls. */

/* Ley Weaver	|3|G
 * Creature - Human Druid 2/2
 * Partner with Lore Weaver
 * |T: Untap two target lands. */

/* Lore Weaver	|3|U
 * Creature - Human Wizard 2/2
 * Partner with Ley Weaver
 * |5|U|U: Target player draws two cards. */

/* Luxury Suite	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |B or |R. */

/* Magma Hellion	|6|R
 * Creature - Hellion 5/4
 * Assist
 * Trample, haste */

/* Mindblade Render	|1|B
 * Creature - Azra Warrior 1/3
 * Whenever your opponents are dealt combat damage, if any of that damage was dealt by a Warrior, you draw a card and you lose 1 life. */

/* Morphic Pool	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |U or |B. */

/* Najeela, the Blade-Blossom	|2|R
 * Legendary Creature - Human Warrior 3/2
 * Whenever a Warrior attacks, you may have its controller create a 1/1 |Swhite Warrior creature token that's tapped and attacking.
 * |W|U|B|R|G: Untap all attacking creatures. They gain trample, lifelink, and haste until end of turn. After this phase, there is an additional combat phase. Activate this ability only during combat. */

/* Nimbus Champion	|5|U
 * Creature - Avatar Warrior 4/4
 * Flying
 * Whenever ~ attacks, you may return target creature to its owner's hand if that creature's power is less than or equal to the number of Warriors your team controls. */

/* Okaun, Eye of Chaos	|4|R
 * Legendary Creature - Cyclops Berserker 3/3
 * Partner with Zndrsplt, Eye of Wisdom
 * At the beginning of combat on your turn, flip a coin until you lose a flip.
 * Whenever a player wins a coin flip, double ~'s power and toughness until end of turn. */

/* Out of Bounds	|3|U
 * Instant
 * Assist
 * Counter target spell. */

/* Pir's Whim	|3|G
 * Sorcery
 * For each player, choose friend or foe. Each friend searches their library for a land card, puts it onto the battlefield tapped, then shuffles their library. Each foe sacrifices an artifact or enchantment they control. */

/* Pir, Imaginative Rascal	|2|G
 * Legendary Creature - Human 1/1
 * Partner with Toothy, Imaginary Friend
 * If one or more counters would be put on a permanent your team controls, that many plus one of each of those kinds of counters are put on that permanent instead. */

/* Play of the Game	|6|W|W
 * Sorcery
 * Assist
 * Exile all nonland permanents. */

/* Proud Mentor	|2|W
 * Creature - Human Warrior 1/1
 * Partner with Impetuous Protege
 * |W, |T: Tap target creature. */

/* Regna's Sanction	|3|W
 * Sorcery
 * For each player, choose friend or foe. Each friend puts a +1/+1 counter on each creature they control. Each foe chooses one untapped creature they control, then taps the rest. */

/* Regna, the Redeemer	|5|W
 * Legendary Creature - Angel 4/4
 * Partner with Krav, the Unredeemed
 * Flying
 * At the beginning of each end step, if your team gained life this turn, create two 1/1 |Swhite Warrior creature tokens. */

/* Rowan Kenrith	|4|R|R
 * Legendary Planeswalker - Rowan (4)
 * +2: During target player's next turn, each creature that player controls attacks if able.
 * -2: ~ deals 3 damage to each tapped creature target player controls.
 * -8: Target player gets an emblem with "Whenever you activate an ability that isn't a mana ability, copy it. You may choose new targets for the copy."
 * Partner with Will Kenrith
 * ~ can be your commander. */

/* Rowan Kenrith Emblem	""
 * Emblem - Rowan
 * Whenever you activate an ability that isn't a mana ability, copy it. You may choose new targets for the copy. */

/* Rushblade Commander	|B|R
 * Creature - Azra Warrior 2/2
 * Warriors your team controls have haste. */

/* Saltwater Stalwart	|3|U
 * Creature - Merfolk Warrior 2/4
 * Whenever ~ deals damage to an opponent, target player draws a card. */

/* Sea of Clouds	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |W or |U. */

/* Sentinel Tower	|4
 * Artifact
 * Whenever an instant or sorcery spell is cast during your turn, ~ deals damage to any target equal to 1 plus the number of instant and sorcery spells cast before that spell this turn. */

/* Sickle Dancer	|2|B
 * Creature - Human Warrior 3/2
 * Whenever ~ attacks, if your team controls another Warrior, ~ gets +1/+1 until end of turn. */

/* Skystreamer	|4|W
 * Creature - Griffin 3/2
 * Assist
 * Flying
 * When ~ enters the battlefield, target player gains 4 life. */

/* Soaring Show-Off	|2|U
 * Creature - Bird Warrior 2/2
 * Flying
 * When ~ enters the battlefield, each player draws a card. */

/* Soulblade Corrupter	|4|B
 * Creature - Human Warrior 3/3
 * Partner with Soulblade Renewer
 * Deathtouch
 * Whenever a creature with a +1/+1 counter on it attacks one of your opponents, that creature gains deathtouch until end of turn. */

/* Soulblade Renewer	|4|G
 * Creature - Elf Warrior 2/2
 * Partner with Soulblade Corrupter
 * When ~ enters the battlefield, support 2. */

/* Spellseeker	|2|U
 * Creature - Human Wizard 1/1
 * When ~ enters the battlefield, you may search your library for an instant or sorcery card with converted mana cost 2 or less, reveal it, put it into your hand, then shuffle your library. */

/* Spellweaver Duo	|6|U
 * Creature - Human Wizard 4/4
 * Assist
 * When ~ enters the battlefield, you may return target tapped creature to its owner's hand. */

/* Spire Garden	""
 * Land
 * ~ enters the battlefield tapped unless you have two or more opponents.
 * |T: Add |R or |G. */

/* Stadium Vendors	|3|R
 * Creature - Goblin 3/3
 * When ~ enters the battlefield, choose a player. That player adds two mana of any one color they choose. */

/* Stolen Strategy	|4|R
 * Enchantment
 * At the beginning of your upkeep, exile the top card of each opponent's library. Until end of turn, you may cast nonland cards from among those exiled cards, and you may spend mana as though it were mana of any color to cast those spells. */

/* Stunning Reversal	|3|B
 * Instant
 * The next time you would lose the game this turn, instead draw seven cards and your life total becomes 1.
 * Exile ~. */

/* Sylvia Brightspear	|2|W
 * Legendary Creature - Human Knight 2/2
 * Partner with Khorvath Brightflame
 * Double strike
 * Dragons your team controls have double strike. */

/* The Crowd Goes Wild	|X|G
 * Sorcery
 * Assist
 * Support X.
 * Each creature with a +1/+1 counter on it gains trample until end of turn. */

/* Thrasher Brute	|3|B
 * Creature - Orc Warrior 4/3
 * Whenever ~ or another Warrior enters the battlefield under your team's control, target opponent loses 1 life and you gain 1 life. */

/* Thrilling Encore	|4|B
 * Instant
 * Put onto the battlefield under your control all creature cards in all graveyards that were put there from the battlefield this turn. */

/* Together Forever	|W|W
 * Enchantment
 * When ~ enters the battlefield, support 2.
 * |1: Choose target creature with a counter on it. When that creature dies this turn, return that card to its owner's hand. */

/* Toothy, Imaginary Friend	|3|U
 * Legendary Creature - Illusion 1/1
 * Partner with Pir, Imaginative Rascal
 * Whenever you draw a card, put a +1/+1 counter on ~.
 * When ~ leaves the battlefield, draw a card for each +1/+1 counter on it. */

/* Vampire Charmseeker	|6|U|B
 * Creature - Vampire Wizard 3/4
 * Assist
 * Flying
 * When ~ enters the battlefield, return target instant, sorcery, or creature card from a graveyard to its owner's hand. */

/* Victory Chimes	|3
 * Artifact
 * Untap ~ during each other player's untap step.
 * |T: A player of your choice adds |C. */

/* Virtus the Veiled	|2|B
 * Legendary Creature - Azra Assassin 1/1
 * Partner with Gorm the Great
 * Deathtouch
 * Whenever ~ deals combat damage to a player, that player loses half their life, rounded up. */

/* Virtus's Maneuver	|2|B
 * Sorcery
 * For each player, choose friend or foe. Each friend returns a creature card from their graveyard to their hand. Each foe sacrifices a creature they control. */

/* Will Kenrith	|4|U|U
 * Legendary Planeswalker - Will (4)
 * +2: Until your next turn, up to two target creatures each have base power and toughness 0/3 and lose all abilities.
 * -2: Target player draws two cards. Until your next turn, instant, sorcery, and planeswalker spells that player casts cost |2 less to cast.
 * -8: Target player gets an emblem with "Whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy."
 * Partner with Rowan Kenrith
 * ~ can be your commander. */

/* Will Kenrith Emblem	""
 * Emblem - Will
 * Whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy. */

/* Zndrsplt's Judgment	|4|U
 * Sorcery
 * For each player, choose friend or foe. Each friend creates a token that's a copy of a creature they control. Each foe returns a creature they control to its owner's hand. */

/* Zndrsplt, Eye of Wisdom	|4|U
 * Legendary Creature - Homunculus 1/4
 * Partner with Okaun, Eye of Chaos
 * At the beginning of combat on your turn, flip a coin until you lose a flip.
 * Whenever a player wins a coin flip, draw a card. */