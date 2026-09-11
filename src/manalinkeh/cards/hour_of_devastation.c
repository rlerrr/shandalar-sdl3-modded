#include "manalink.h"

/* Act of Heroism	|1|W
 * Instant
 * Untap target creature. It gets +2/+2 until end of turn and can block an additional creature this turn. */

/* Adorned Pouncer	|1|W
 * Creature - Cat 1/1
 * Double strike
 * Eternalize |3|W|W */

/* Angel of Condemnation	|2|W|W
 * Creature - Angel 3/3
 * Flying, vigilance
 * |2|W, |T: Exile another target creature. Return that card to the battlefield under its owner's control at the beginning of the next end step.
 * |2|W, |T, Exert ~: Exile another target creature until ~ leaves the battlefield. */

/* Angel of the God-Pharaoh	|4|W|W
 * Creature - Angel 4/4
 * Flying
 * Cycling |2 */

/* Aven of Enduring Hope	|4|W	=>portal_1_2_3k.c:card_angel_of_mercy 0x20056e2
 * Creature - Bird Cleric 3/3
 * Flying
 * When ~ enters the battlefield, you gain 3 life. */

/* Crested Sunmare	|3|W|W
 * Creature - Horse 5/5
 * Other Horses you control have indestructible.
 * At the beginning of each end step, if you gained life this turn, create a 5/5 |Swhite Horse creature token. */

/* Dauntless Aven	|2|W
 * Creature - Bird Warrior 2/1
 * Flying
 * Whenever ~ attacks, untap target creature you control. */

/* Desert's Hold	|2|W
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, you gain 3 life.
 * Enchanted creature can't attack or block, and its activated abilities can't be activated. */

/* Disposal Mummy	|2|W
 * Creature - Zombie Jackal 2/3
 * When ~ enters the battlefield, exile target card from an opponent's graveyard. */

/* Djeru, With Eyes Open	|3|W|W
 * Legendary Creature - Human Warrior 4/3
 * Vigilance
 * When ~ enters the battlefield, you may search your library for a planeswalker card, reveal it, put it into your hand, then shuffle your library.
 * If a source would deal damage to a planeswalker you control, prevent 1 of that damage. */

/* Djeru's Renunciation	|1|W
 * Instant
 * Tap up to two target creatures.
 * Cycling |W */

/* Dutiful Servants	|3|W	0x401000
 * Creature - Zombie 2/5 */

/* Gideon's Defeat	|W
 * Instant
 * Exile target |Swhite creature that's attacking or blocking. If it was a Gideon planeswalker, you gain 5 life. */

/* God-Pharaoh's Faithful	|W
 * Creature - Human Wizard 0/4
 * Whenever you cast a |Sblue, |Sblack, or |Sred spell, you gain 1 life. */

/* Hour of Revelation	|3|W|W|W
 * Sorcery
 * This spell costs |3 less to cast if there are ten or more nonland permanents on the battlefield.
 * Destroy all nonland permanents. */

/* Mummy Paramount	|1|W
 * Creature - Zombie 2/2
 * Whenever another Zombie enters the battlefield under your control, ~ gets +1/+1 until end of turn. */

/* Oketra's Avenger	|1|W
 * Creature - Human Warrior 3/1
 * You may exert ~ as it attacks. When you do, prevent all combat damage that would be dealt to it this turn. */

/* Oketra's Last Mercy	|1|W|W
 * Sorcery
 * Your life total becomes equal to your starting life total. Lands you control don't untap during your next untap step. */

/* Overwhelming Splendor	|6|W|W
 * Enchantment - Aura Curse
 * Enchant player
 * Creatures enchanted player controls lose all abilities and have base power and toughness 1/1.
 * Enchanted player can't activate abilities that aren't mana abilities or loyalty abilities. */

// Sandblast => reprint from FRF

/* Saving Grace	|1|W
 * Enchantment - Aura
 * Flash
 * Enchant creature you control
 * When ~ enters the battlefield, all damage that would be dealt this turn to you and permanents you control is dealt to enchanted creature instead.
 * Enchanted creature gets +0/+3. */

/* Solemnity	|2|W
 * Enchantment
 * Players can't get counters.
 * Counters can't be put on artifacts, creatures, enchantments, or lands. */

/* Solitary Camel	|2|W
 * Creature - Camel 3/2
 * ~ has lifelink as long as you control a Desert or there is a Desert card in your graveyard. */

/* Steadfast Sentinel	|3|W
 * Creature - Human Cleric 2/3
 * Vigilance
 * Eternalize |4|W|W */

/* Steward of Solidarity	|1|W
 * Creature - Human Warrior 2/2
 * |T, Exert ~: Create a 1/1 |Swhite Warrior creature token with vigilance. */

/* Sunscourge Champion	|2|W
 * Creature - Human Wizard 2/3
 * When ~ enters the battlefield, you gain life equal to its power.
 * Eternalize-|2|W|W, Discard a card. */

/* Unconventional Tactics	|2|W
 * Sorcery
 * Target creature gets +3/+3 and gains flying until end of turn.
 * Whenever a Zombie enters the battlefield under your control, you may pay |W. If you do, return ~ from your graveyard to your hand. */

/* Vizier of the True	|3|W
 * Creature - Human Cleric 3/2
 * You may exert ~ as it attacks.
 * Whenever you exert a creature, tap target creature an opponent controls. */

/* Aerial Guide	|2|U
 * Creature - Drake 2/2
 * Flying
 * Whenever ~ attacks, another target attacking creature gains flying until end of turn. */

/* Aven Reedstalker	|3|U	=>time_spiral.c:card_ashcoat_bear 0x200cb63
 * Creature - Bird Warrior 2/3
 * Flash
 * Flying */

/* Champion of Wits	|2|U
 * Creature - Naga Wizard 2/1
 * When ~ enters the battlefield, you may draw cards equal to its power. If you do, discard two cards.
 * Eternalize |5|U|U */

/* Countervailing Winds	|2|U
 * Instant
 * Counter target spell unless its controller pays |1 for each card in your graveyard.
 * Cycling |2 */

/* Cunning Survivor	|1|U
 * Creature - Human Warrior 1/3
 * Whenever you cycle or discard a card, ~ gets +1/+0 until end of turn and can't be blocked this turn. */

/* Eternal of Harsh Truths	|2|U
 * Creature - Zombie Cleric 1/3
 * Afflict 2
 * Whenever ~ attacks and isn't blocked, draw a card. */

/* Fraying Sanity	|2|U
 * Enchantment - Aura Curse
 * Enchant player
 * At the beginning of each end step, enchanted player puts the top X cards of their library into their graveyard, where X is the number of cards put into that graveyard from anywhere this turn. */

/* Hour of Eternity	|X|X|U|U|U
 * Sorcery
 * Exile X target creature cards from your graveyard. For each card exiled this way, create a token that's a copy of that card, except it's a 4/4 |Sblack Zombie. */

/* Imaginary Threats	|2|U|U
 * Instant
 * Creatures target opponent controls attack this turn if able. During that player's next untap step, creatures they control don't untap.
 * Cycling |2 */

/* Jace's Defeat	|1|U
 * Instant
 * Counter target |Sblue spell. If it was a Jace planeswalker spell, scry 2. */

/* Kefnet's Last Word	|2|U|U
 * Sorcery
 * Gain control of target artifact, creature, or enchantment. Lands you control don't untap during your next untap step. */

/* Nimble Obstructionist	|2|U
 * Creature - Bird Wizard 3/1
 * Flash
 * Flying
 * Cycling |2|U
 * When you cycle ~, counter target activated or triggered ability you don't control. */

/* Ominous Sphinx	|3|U|U
 * Creature - Sphinx 4/4
 * Flying
 * Whenever you cycle or discard a card, target creature an opponent controls gets -2/-0 until end of turn. */

/* Proven Combatant	|U
 * Creature - Human Warrior 1/1
 * Eternalize |4|U|U */

/* Riddleform	|1|U
 * Enchantment
 * Whenever you cast a noncreature spell, you may have ~ become a 3/3 Sphinx creature with flying in addition to its other types until end of turn.
 * |2|U: Scry 1. */

/* Seer of the Last Tomorrow	|2|U
 * Creature - Naga Cleric 1/4
 * |U, |T, Discard a card: Target player puts the top three cards of their library into their graveyard. */

/* Sinuous Striker	|2|U
 * Creature - Naga Warrior 2/2
 * |U: ~ gets +1/-1 until end of turn.
 * Eternalize-|3|U|U, Discard a card. */

/* Spellweaver Eternal	|1|U
 * Creature - Zombie Naga Wizard 2/1
 * Prowess
 * Afflict 2 */

// Strategic Planning => reprint from P3K

/* Striped Riverwinder	|6|U
 * Creature - Serpent 5/5
 * Hexproof
 * Cycling |U */

/* Supreme Will	|2|U
 * Instant
 * Choose one -
 * * Counter target spell unless its controller pays |3.
 * * Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in any order. */

/* Swarm Intelligence	|6|U
 * Enchantment
 * Whenever you cast an instant or sorcery spell, you may copy that spell. You may choose new targets for the copy. */

/* Tragic Lesson	|2|U
 * Instant
 * Draw two cards. Then discard a card unless you return a land you control to its owner's hand. */

/* Unesh, Criosphinx Sovereign	|4|U|U
 * Legendary Creature - Sphinx 4/4
 * Flying
 * Sphinx spells you cast cost |2 less to cast.
 * Whenever ~ or another Sphinx enters the battlefield under your control, reveal the top four cards of your library. An opponent separates those cards into two piles. Put one pile into your hand and the other into your graveyard. */

/* Unquenchable Thirst	|1|U
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

// Unsummon => reprint from LEA

/* Vizier of the Anointed	|3|U
 * Creature - Human Cleric 2/4
 * When ~ enters the battlefield, you may search your library for a creature card with eternalize or embalm, put that card into your graveyard, then shuffle your library.
 * Whenever you activate an eternalize or embalm ability, draw a card. */

/* Accursed Horde	|3|B
 * Creature - Zombie 3/3
 * |1|B: Target attacking Zombie gains indestructible until end of turn. */

/* Ammit Eternal	|2|B
 * Creature - Zombie Crocodile Demon 5/5
 * Afflict 3
 * Whenever an opponent casts a spell, put a -1/-1 counter on ~.
 * Whenever ~ deals combat damage to a player, remove all -1/-1 counters from it. */

/* Apocalypse Demon	|4|B|B
 * Creature - Demon 100/100
 * Flying
 * ~'s power and toughness are each equal to the number of cards in your graveyard.
 * At the beginning of your upkeep, tap ~ unless you sacrifice another creature. */

/* Banewhip Punisher	|2|B
 * Creature - Human Warrior 2/2
 * When ~ enters the battlefield, you may put a -1/-1 counter on target creature.
 * |B, Sacrifice ~: Destroy target creature that has a -1/-1 counter on it. */

/* Bontu's Last Reckoning	|1|B|B
 * Sorcery
 * Destroy all creatures. Lands you control don't untap during your next untap step. */

/* Carrion Screecher	|3|B	0x401000
 * Creature - Zombie Bird 3/1
 * Flying */

/* Doomfall	|2|B
 * Sorcery
 * Choose one -
 * * Target opponent exiles a creature they control.
 * * Target opponent reveals their hand. You choose a nonland card from it. Exile that card. */

/* Dreamstealer	|2|B
 * Creature - Human Wizard 1/2
 * Menace
 * Whenever ~ deals combat damage to a player, that player discards that many cards.
 * Eternalize |4|B|B */

/* Grisly Survivor	|2|B
 * Creature - Minotaur Warrior 2/3
 * Whenever you cycle or discard a card, ~ gets +2/+0 until end of turn. */

/* Hour of Glory	|3|B
 * Instant
 * Exile target creature. If that creature was a God, its controller reveals their hand and exiles all cards from it with the same name as that creature. */

/* Khenra Eternal	|1|B
 * Creature - Zombie Jackal Warrior 2/2
 * Afflict 1 */

/* Lethal Sting	|2|B
 * Sorcery
 * As an additional cost to cast this spell, put a -1/-1 counter on a creature you control.
 * Destroy target creature. */

/* Liliana's Defeat	|B
 * Sorcery
 * Destroy target |Sblack creature or |Sblack planeswalker. If that permanent was a Liliana planeswalker, her controller loses 3 life. */

/* Lurching Rotbeast	|3|B
 * Creature - Zombie Beast 4/2
 * Cycling |B */

/* Marauding Boneslasher	|2|B
 * Creature - Zombie Minotaur 3/3
 * ~ can't block unless you control another Zombie. */

/* Merciless Eternal	|2|B
 * Creature - Zombie Cleric 2/2
 * Afflict 2
 * |2|B, Discard a card: ~ gets +2/+2 until end of turn. */

/* Moaning Wall	|2|B
 * Creature - Zombie Wall 0/5
 * Defender
 * Cycling |2 */

/* Razaketh, the Foulblooded	|5|B|B|B
 * Legendary Creature - Demon 8/8
 * Flying, trample
 * Pay 2 life, Sacrifice another creature: Search your library for a card and put that card into your hand. Then shuffle your library. */

/* Razaketh's Rite	|3|B|B
 * Sorcery
 * Search your library for a card and put that card into your hand. Then shuffle your library.
 * Cycling |B */

/* Ruin Rat	|1|B
 * Creature - Rat 1/1
 * Deathtouch
 * When ~ dies, exile target card from an opponent's graveyard. */

/* Scrounger of Souls	|4|B	=>m10.c:card_child_of_night 0x200415c
 * Creature - Horror 3/4
 * Lifelink */

/* Torment of Hailfire	|X|B|B
 * Sorcery
 * Repeat the following process X times. Each opponent loses 3 life unless that player sacrifices a nonland permanent or discards a card. */

/* Torment of Scarabs	|3|B
 * Enchantment - Aura Curse
 * Enchant player
 * At the beginning of enchanted player's upkeep, that player loses 3 life unless they sacrifice a nonland permanent or discard a card. */

/* Torment of Venom	|2|B|B
 * Instant
 * Put three -1/-1 counters on target creature. Its controller loses 3 life unless they sacrifice another nonland permanent or discards a card. */

/* Vile Manifestation	|1|B
 * Creature - Horror 0/4
 * ~ gets +1/+0 for each card with cycling in your graveyard.
 * Cycling |2 */

/* Without Weakness	|1|B
 * Instant
 * Target creature you control gains indestructible until end of turn.
 * Cycling |2 */

/* Wretched Camel	|1|B
 * Creature - Zombie Camel 2/1
 * When ~ dies, if you control a Desert or there is a Desert card in your graveyard, target player discards a card. */

/* Abrade	|1|R
 * Instant
 * Choose one -
 * * ~ deals 3 damage to target creature.
 * * Destroy target artifact. */

/* Blur of Blades	|1|R
 * Instant
 * Put a -1/-1 counter on target creature. ~ deals 2 damage to that creature's controller. */

/* Burning-Fist Minotaur	|1|R
 * Creature - Minotaur Wizard 2/1
 * First strike
 * |1|R, Discard a card: ~ gets +2/+0 until end of turn. */

/* Chandra's Defeat	|R
 * Instant
 * ~ deals 5 damage to target |Sred creature or |Sred planeswalker. If that permanent is a Chandra planeswalker, you may discard a card. If you do, draw a card. */

/* Chaos Maw	|5|R|R
 * Creature - Hellion 6/6
 * When ~ enters the battlefield, it deals 3 damage to each other creature. */

/* Crash Through	|R
 * Sorcery
 * Creatures you control gain trample until end of turn.
 * Draw a card. */

/* Defiant Khenra	|1|R	0x401000
 * Creature - Jackal Warrior 2/2 */

/* Earthshaker Khenra	|1|R
 * Creature - Jackal Warrior 2/1
 * Haste
 * When ~ enters the battlefield, target creature with power less than or equal to ~'s power can't block this turn.
 * Eternalize |4|R|R */

/* Fervent Paincaster	|2|R
 * Creature - Human Wizard 3/1
 * |T: ~ deals 1 damage to target player or planeswalker.
 * |T, Exert ~: It deals 1 damage to target creature. */

/* Firebrand Archer	|1|R
 * Creature - Human Archer 2/1
 * Whenever you cast a noncreature spell, ~ deals 1 damage to each opponent. */

/* Frontline Devastator	|3|R
 * Creature - Zombie Minotaur Warrior 3/3
 * Afflict 2
 * |1|R: ~ gets +1/+0 until end of turn. */

/* Gilded Cerodon	|4|R
 * Creature - Beast 4/4
 * Whenever ~ attacks, if you control a Desert or there is a Desert card in your graveyard, target creature can't block this turn. */

/* Granitic Titan	|4|R|R
 * Creature - Elemental 5/4
 * Menace
 * Cycling |2 */

/* Hazoret's Undying Fury	|4|R|R
 * Sorcery
 * Shuffle your library, then exile the top four cards. You may cast any number of nonland cards with converted mana cost 5 or less from among them without paying their mana costs. Lands you control don't untap during your next untap step. */

/* Hour of Devastation	|3|R|R
 * Sorcery
 * All creatures lose indestructible until end of turn. ~ deals 5 damage to each creature and each non-Bolas planeswalker. */

/* Imminent Doom	|2|R
 * Enchantment
 * ~ enters the battlefield with a doom counter on it.
 * Whenever you cast a spell with converted mana cost equal to the number of doom counters on ~, ~ deals that much damage to any target. Then put a doom counter on ~. */

/* Inferno Jet	|5|R
 * Sorcery
 * ~ deals 6 damage to target opponent or planeswalker.
 * Cycling |2 */

/* Khenra Scrapper	|2|R
 * Creature - Jackal Warrior 2/3
 * Menace
 * You may exert ~ as it attacks. When you do, it gets +2/+0 until end of turn. */

// Kindled Fury => reprint from LOR

/* Magmaroth	|3|R
 * Creature - Elemental 5/5
 * At the beginning of your upkeep, put a -1/-1 counter on ~.
 * Whenever you cast a noncreature spell, remove a -1/-1 counter from ~. */

/* Manticore Eternal	|3|R|R
 * Creature - Zombie Manticore 5/4
 * Afflict 3
 * ~ attacks each combat if able. */

/* Neheb, the Eternal	|3|R|R
 * Legendary Creature - Zombie Minotaur Warrior 4/6
 * Afflict 3
 * At the beginning of your postcombat main phase, add |R for each 1 life your opponents have lost this turn. */

/* Open Fire	|2|R	=>unlimited.c:card_lightning_bolt 0x200c078
 * Instant
 * ~ deals 3 damage to any target. */

/* Puncturing Blow	|2|R|R
 * Sorcery
 * ~ deals 5 damage to target creature. If that creature would die this turn, exile it instead. */

/* Sand Strangler	|3|R
 * Creature - Beast 3/3
 * When ~ enters the battlefield, if you control a Desert or there is a Desert card in your graveyard, you may have ~ deal 3 damage to target creature. */

/* Thorned Moloch	|2|R
 * Creature - Lizard 2/2
 * Prowess
 * ~ has first strike as long as it's attacking. */

/* Wildfire Eternal	|3|R
 * Creature - Zombie Jackal Cleric 1/4
 * Afflict 4
 * Whenever ~ attacks and isn't blocked, you may cast an instant or sorcery card from your hand without paying its mana cost. */

/* Ambuscade	|2|G
 * Instant
 * Target creature you control gets +1/+0 until end of turn. It deals damage equal to its power to target creature an opponent controls. */

/* Beneath the Sands	|2|G
 * Sorcery
 * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library.
 * Cycling |2 */

/* Bitterbow Sharpshooters	|4|G	=>unlimited.c:card_serra_angel 0x200c122
 * Creature - Jackal Archer 4/4
 * Vigilance, reach */

/* Devotee of Strength	|2|G
 * Creature - Naga Wizard 3/2
 * |4|G: Target creature gets +2/+2 until end of turn. */

/* Dune Diviner	|2|G
 * Creature - Naga Cleric 2/3
 * |1, Tap an untapped Desert you control: You gain 1 life. */

/* Feral Prowler	|1|G	=>scars_of_mirrodin.c:card_darkslick_drake 0x2004201
 * Creature - Cat 1/3
 * When ~ dies, draw a card. */

/* Frilled Sandwalla	|G	=>torment.c:card_basking_rootwalla 0x2002983
 * Creature - Lizard 1/1
 * |1|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn. */

/* Gift of Strength	|1|G
 * Instant
 * Target creature gets +3/+3 and gains reach until end of turn. */

/* Harrier Naga	|2|G	0x401000
 * Creature - Naga Warrior 3/3 */

/* Hope Tender	|1|G
 * Creature - Human Druid 2/2
 * |1, |T: Untap target land.
 * |1, |T, Exert ~: Untap two target lands. */

/* Hour of Promise	|4|G
 * Sorcery
 * Search your library for up to two land cards, put them onto the battlefield tapped, then shuffle your library. Then if you control three or more Deserts, create two 2/2 |Sblack Zombie creature tokens. */

/* Life Goes On	|G
 * Instant
 * You gain 4 life. If a creature died this turn, you gain 8 life instead. */

/* Majestic Myriarch	|4|G
 * Creature - Chimera 100/100
 * ~'s power and toughness are each equal to twice the number of creatures you control.
 * At the beginning of each combat, ~ gains flying until end of turn if you control a creature with flying. The same is true for first strike, double strike, deathtouch, haste, hexproof, indestructible, lifelink, menace, reach, trample, and vigilance. */

/* Nissa's Defeat	|2|G
 * Sorcery
 * Destroy target |H2Forest, |Sgreen enchantment, or |Sgreen planeswalker. If that permanent was a Nissa planeswalker, draw a card. */

/* Oasis Ritualist	|3|G
 * Creature - Naga Druid 2/4
 * |T: Add one mana of any color.
 * |T, Exert ~: Add two mana of any one color. */

/* Overcome	|3|G|G
 * Sorcery
 * Creatures you control get +2/+2 and gain trample until end of turn. */

/* Pride Sovereign	|2|G
 * Creature - Cat 2/2
 * ~ gets +1/+1 for each other Cat you control.
 * |W, |T, Exert ~: Create two 1/1 |Swhite Cat creature tokens with lifelink. */

/* Quarry Beetle	|4|G
 * Creature - Insect 4/5
 * When ~ enters the battlefield, you may return target land card from your graveyard to the battlefield. */

/* Rampaging Hippo	|4|G|G
 * Creature - Hippo 5/6
 * Trample
 * Cycling |2 */

/* Ramunap Excavator	|2|G	=>fifth_dawn.c:card_crucible_of_worlds 0x2001628
 * Creature - Naga Cleric 2/3
 * You may play land cards from your graveyard. */

/* Ramunap Hydra	|3|G
 * Creature - Snake Hydra 3/3
 * Vigilance, reach, trample
 * ~ gets +1/+1 as long as you control a Desert.
 * ~ gets +1/+1 as long as there is a Desert card in your graveyard. */

/* Resilient Khenra	|1|G
 * Creature - Jackal Wizard 2/2
 * When ~ enters the battlefield, you may have target creature get +X/+X until end of turn, where X is ~'s power.
 * Eternalize |4|G|G */

/* Rhonas's Last Stand	|G|G
 * Sorcery
 * Create a 5/4 |Sgreen Snake creature token. Lands you control don't untap during your next untap step. */

/* Rhonas's Stalwart	|1|G
 * Creature - Human Warrior 2/2
 * You may exert ~ as it attacks. When you do, it gets +1/+1 until end of turn and can't be blocked by creatures with power 2 or less this turn. */

/* Sidewinder Naga	|2|G
 * Creature - Naga Warrior 3/2
 * As long as you control a Desert or there is a Desert card in your graveyard, ~ gets +1/+0 and has trample. */

/* Sifter Wurm	|5|G|G
 * Creature - Wurm 7/7
 * Trample
 * When ~ enters the battlefield, scry 3, then reveal the top card of your library. You gain life equal to that card's converted mana cost. */

/* Tenacious Hunter	|2|G|G
 * Creature - Crocodile 4/4
 * As long as a creature has a -1/-1 counter on it, ~ has vigilance and deathtouch. */

/* Uncage the Menagerie	|X|G|G
 * Sorcery
 * Search your library for up to X creature cards with different names that each have converted mana cost X, reveal them, put them into your hand, then shuffle your library. */

/* Bloodwater Entity	|1|U|R
 * Creature - Elemental 2/2
 * Flying
 * Prowess
 * When ~ enters the battlefield, you may put target instant or sorcery card from your graveyard on top of your library. */

/* The Locust God	|4|U|R
 * Legendary Creature - God 4/4
 * Flying
 * Whenever you draw a card, create a 1/1 |Sblue and |Sred Insect creature token with flying and haste.
 * |2|U|R: Draw a card, then discard a card.
 * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

/* Nicol Bolas, God-Pharaoh	|4|U|B|R
 * Legendary Planeswalker - Bolas (7)
 * +2: Target opponent exiles cards from the top of their library until they exile a nonland card. Until end of turn, you may cast that card without paying its mana cost.
 * +1: Each opponent exiles two cards from their hand.
 * -4: ~ deals 7 damage to target opponent, creature an opponent controls, or planeswalker an opponent controls.
 * -12: Exile each nonland permanent your opponents control. */

/* Obelisk Spider	|1|B|G
 * Creature - Spider 1/4
 * Reach
 * Whenever ~ deals combat damage to a creature, put a -1/-1 counter on that creature.
 * Whenever you put one or more -1/-1 counters on a creature, each opponent loses 1 life and you gain 1 life. */

/* Resolute Survivors	|1|R|W
 * Creature - Human Warrior 3/3
 * You may exert ~ as it attacks.
 * Whenever you exert a creature, ~ deals 1 damage to each opponent and you gain 1 life. */

/* River Hoopoe	|G|U
 * Creature - Bird 1/3
 * Flying
 * |3|G|U: You gain 2 life and draw a card. */

/* Samut, the Tested	|2|R|G
 * Legendary Planeswalker - Samut (4)
 * +1: Up to one target creature gains double strike until end of turn.
 * -2: ~ deals 2 damage divided as you choose among one or two targets.
 * -7: Search your library for up to two creature and/or planeswalker cards, put them onto the battlefield, then shuffle your library. */

/* The Scarab God	|3|U|B
 * Legendary Creature - God 5/5
 * At the beginning of your upkeep, each opponent loses X life and you scry X, where X is the number of Zombies you control.
 * |2|U|B: Exile target creature card from a graveyard. Create a token that's a copy of it, except it's a 4/4 |Sblack Zombie.
 * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

/* The Scorpion God	|3|B|R
 * Legendary Creature - God 6/5
 * Whenever a creature with a -1/-1 counter on it dies, draw a card.
 * |1|B|R: Put a -1/-1 counter on another target creature.
 * When ~ dies, return it to its owner's hand at the beginning of the next end step. */

/* Unraveling Mummy	|1|W|B
 * Creature - Zombie 2/3
 * |1|W: Target attacking Zombie gains lifelink until end of turn.
 * |1|B: Target attacking Zombie gains deathtouch until end of turn. */

/* Farm // Market	|2|W
 * Instant // Sorcery
 * Destroy target attacking or blocking creature.
 * //
 * Aftermath
 * Draw two cards, then discard two cards. */

/* Consign // Oblivion	|1|U
 * Instant // Sorcery
 * Return target nonland permanent to its owner's hand.
 * //
 * Aftermath
 * Target opponent discards two cards. */

/* Claim // Fame	|B
 * Sorcery // Sorcery
 * Return target creature card with converted mana cost 2 or less from your graveyard to the battlefield.
 * //
 * Aftermath
 * Target creature gets +2/+0 and gains haste until end of turn. */

/* Struggle // Survive	|2|R
 * Instant // Sorcery
 * Struggle deals damage to target creature equal to the number of lands you control.
 * //
 * Aftermath
 * Each player shuffles their graveyard into their library. */

/* Appeal // Authority	|G
 * Sorcery // Sorcery
 * Until end of turn, target creature gains trample and gets +X/+X, where X is the number of creatures you control.
 * //
 * Aftermath
 * Tap up to two target creatures your opponents control. Creatures you control gain vigilance until end of turn. */

/* Leave // Chance	|1|W
 * Instant // Sorcery
 * Return any number of target permanents you own to your hand.
 * //
 * Aftermath
 * Discard any number of cards, then draw that many cards. */

/* Reason // Believe	|U
 * Sorcery // Sorcery
 * Scry 3.
 * //
 * Aftermath
 * Look at the top card of your library. You may put it onto the battlefield if it's a creature card. If you don't, put it into your hand. */

/* Grind // Dust	|1|B
 * Sorcery // Sorcery
 * Put a -1/-1 counter on each of up to two target creatures.
 * //
 * Aftermath
 * Exile any number of target creatures that have -1/-1 counters on them. */

/* Refuse // Cooperate	|3|R
 * Instant // Instant
 * Refuse deals damage to target spell's controller equal to that spell's converted mana cost.
 * //
 * Aftermath
 * Copy target instant or sorcery spell. You may choose new targets for the copy. */

/* Driven // Despair	|1|G
 * Sorcery // Sorcery
 * Until end of turn, creatures you control gain trample and "Whenever this creature deals combat damage to a player, draw a card."
 * //
 * Aftermath
 * Until end of turn, creatures you control gain menace and "Whenever this creature deals combat damage to a player, that player discards a card." */

/* Abandoned Sarcophagus	|3
 * Artifact
 * You may cast nonland cards with cycling from your graveyard.
 * If a card with cycling would be put into your graveyard from anywhere and it wasn't cycled, exile it instead. */

/* Crook of Condemnation	|2
 * Artifact
 * |1, |T: Exile target card from a graveyard.
 * |1, Exile ~: Exile all cards from all graveyards. */

/* Dagger of the Worthy	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+0 and has afflict 1.
 * Equip |2 */

/* God-Pharaoh's Gift	|7
 * Artifact
 * At the beginning of combat on your turn, you may exile a creature card from your graveyard. If you do, create a token that's a copy of that card, except it's a 4/4 |Sblack Zombie. It gains haste until end of turn. */

/* Graven Abomination	|3
 * Artifact Creature - Horror 3/1
 * Whenever ~ attacks, exile target card from defending player's graveyard. */

/* Hollow One	|5
 * Artifact Creature - Golem 4/4
 * This spell costs |2 less to cast for each card you've cycled or discarded this turn.
 * Cycling |2 */

// Manalith => reprint from M12

/* Mirage Mirror	|3
 * Artifact
 * |2: ~ becomes a copy of target artifact, creature, enchantment, or land until end of turn. */

/* Sunset Pyramid	|2
 * Artifact
 * ~ enters the battlefield with three brick counters on it.
 * |2, |T, Remove a brick counter from ~: Draw a card.
 * |2, |T: Scry 1. */

// Traveler's Amulet => reprint from ISD

/* Wall of Forgotten Pharaohs	|2
 * Artifact Creature - Wall 0/4
 * Defender
 * |T: ~ deals 1 damage to target player or planeswalker. Activate this ability only if you control a Desert or there is a Desert card in your graveyard. */

/* Crypt of the Eternals	""
 * Land
 * When ~ enters the battlefield, you gain 1 life.
 * |T: Add |C.
 * |1, |T: Add |U, |B, or |R. */

/* Desert of the Fervent	""
 * Land - Desert
 * ~ enters the battlefield tapped.
 * |T: Add |R.
 * Cycling |1|R */

/* Desert of the Glorified	""
 * Land - Desert
 * ~ enters the battlefield tapped.
 * |T: Add |B.
 * Cycling |1|B */

/* Desert of the Indomitable	""
 * Land - Desert
 * ~ enters the battlefield tapped.
 * |T: Add |G.
 * Cycling |1|G */

/* Desert of the Mindful	""
 * Land - Desert
 * ~ enters the battlefield tapped.
 * |T: Add |U.
 * Cycling |1|U */

/* Desert of the True	""
 * Land - Desert
 * ~ enters the battlefield tapped.
 * |T: Add |W.
 * Cycling |1|W */

/* Dunes of the Dead	""
 * Land - Desert
 * |T: Add |C.
 * When ~ is put into a graveyard from the battlefield, create a 2/2 |Sblack Zombie creature token. */

/* Endless Sands	""
 * Land - Desert
 * |T: Add |C.
 * |2, |T: Exile target creature you control.
 * |4, |T, Sacrifice ~: Return each creature card exiled with ~ to the battlefield under its owner's control. */

/* Hashep Oasis	""
 * Land - Desert
 * |T: Add |C.
 * |T, Pay 1 life: Add |G.
 * |1|G|G, |T, Sacrifice a Desert: Target creature gets +3/+3 until end of turn. Activate this ability only any time you could cast a sorcery. */

/* Hostile Desert	""
 * Land - Desert
 * |T: Add |C.
 * |2, Exile a land card from your graveyard: ~ becomes a 3/4 Elemental creature until end of turn. It's still a land. */

/* Ifnir Deadlands	""
 * Land - Desert
 * |T: Add |C.
 * |T, Pay 1 life: Add |B.
 * |2|B|B, |T, Sacrifice a Desert: Put two -1/-1 counters on target creature an opponent controls. Activate this ability only any time you could cast a sorcery. */

/* Ipnu Rivulet	""
 * Land - Desert
 * |T: Add |C.
 * |T, Pay 1 life: Add |U.
 * |1|U, |T, Sacrifice a Desert: Target player puts the top four cards of their library into their graveyard. */

/* Ramunap Ruins	""
 * Land - Desert
 * |T: Add |C.
 * |T, Pay 1 life: Add |R.
 * |2|R|R, |T, Sacrifice a Desert: ~ deals 2 damage to each opponent. */

/* Scavenger Grounds	""
 * Land - Desert
 * |T: Add |C.
 * |2, |T, Sacrifice a Desert: Exile all cards from all graveyards. */

/* Shefet Dunes	""
 * Land - Desert
 * |T: Add |C.
 * |T, Pay 1 life: Add |W.
 * |2|W|W, |T, Sacrifice a Desert: Creatures you control get +1/+1 until end of turn. Activate this ability only any time you could cast a sorcery. */

/* Survivors' Encampment	""	=>oath_of_the_gatewatch.c:card_holdout_settlement 0x200e70b
 * Land - Desert
 * |T: Add |C.
 * |T, Tap an untapped creature you control: Add one mana of any color. */

/* Nissa, Genesis Mage	|5|G|G
 * Legendary Planeswalker - Nissa (5)
 * +2: Untap up to two target creatures and up to two target lands.
 * -3: Target creature gets +5/+5 until end of turn.
 * -10: Look at the top ten cards of your library. You may put any number of creature and/or land cards from among them onto the battlefield. Put the rest on the bottom of your library in a random order. */

/* Avid Reclaimer	|2|G
 * Creature - Human Druid 2/2
 * |T: Add |G or |U. If you control a Nissa planeswalker, you gain 2 life. */

/* Brambleweft Behemoth	|4|G|G	0x401000
 * Creature - Elemental 6/6
 * Trample */

/* Nissa's Encouragement	|4|G
 * Sorcery
 * Search your library and graveyard for a card named |H2Forest, a card named Brambleweft Behemoth, and a card named Nissa, Genesis Mage. Reveal those cards, put them into your hand, then shuffle your library. */

// Woodland Stream => reprint from SOI

/* Nicol Bolas, the Deceiver	|5|U|B|R
 * Legendary Planeswalker - Bolas (5)
 * +3: Each opponent loses 3 life unless that player sacrifices a nonland permanent or discards a card.
 * -3: Destroy target creature. Draw a card.
 * -11: ~ deals 7 damage to each opponent. You draw seven cards. */

/* Wasp of the Bitter End	|1|B
 * Creature - Insect Horror 2/1
 * Flying
 * Whenever you cast a Bolas planeswalker spell, you may sacrifice ~. If you do, destroy target creature. */

/* Zealot of the God-Pharaoh	|3|R
 * Creature - Minotaur Archer 4/3
 * |4|R: ~ deals 2 damage to target opponent or planeswalker. */

/* Visage of Bolas	|4
 * Artifact
 * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Nicol Bolas, the Deceiver, reveal it, and put it into your hand. If you search your library this way, shuffle it.
 * |T: Add |U, |B, or |R. */

// Cinder Barrens => reprint from OGW
