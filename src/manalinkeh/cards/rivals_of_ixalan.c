#include "manalink.h"

/*********
 * White *
 *********/

/* Baffling End	|1|W
 * Enchantment
 * When ~ enters the battlefield, exile target creature an opponent controls with converted mana cost 3 or less.
 * When ~ leaves the battlefield, target opponent creates a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Bishop of Binding	|3|W
 * Creature - Vampire Cleric 1/1
 * When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield.
 * Whenever ~ attacks, target Vampire gets +X/+X until end of turn, where X is the power of the exiled card. */

/* Blazing Hope	|W
 * Instant
 * Exile target creature with power greater than or equal to your life total. */

/* Cleansing Ray	|1|W
 * Sorcery
 * Choose one -
 * * Destroy target Vampire.
 * * Destroy target enchantment. */

// Divine Verdict - reprint from Magic 2010

/* Everdawn Champion	|1|W|W
 * Creature - Human Soldier 2/2
 * Prevent all combat damage that would be dealt to ~. */

/* Exultant Skymarcher	|1|W|W
 * Creature - Vampire Soldier 2/3
 * Flying */

/* Famished Paladin	|1|W
 * Creature - Vampire Knight 3/3
 * ~ doesn't untap during your untap step.
 * Whenever you gain life, untap ~. */

/* Forerunner of the Legion	|2|W
 * Creature - Vampire Knight 2/2
 * When ~ enters the battlefield, you may search your library for a Vampire card, reveal it, then shuffle your library and put that card on top of it.
 * Whenever another Vampire enters the battlefield under your control, target creature gets +1/+1 until end of turn. */

/* Imperial Ceratops	|4|W
 * Creature - Dinosaur 3/5
 * Enrage - Whenever ~ is dealt damage, you gain 2 life. */

// Legion Conquistador - reprint from Ixalan

/* Luminous Bonds	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature can't attack or block. */

/* Majestic Heliopterus	|3|W
 * Creature - Dinosaur 2/2
 * Flying
 * Whenever ~ attacks, another target Dinosaur you control gains flying until end of turn. */

/* Martyr of Dusk	|1|W
 * Creature - Vampire Soldier 2/1
 * When ~ dies, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Moment of Triumph	|W
 * Instant
 * Target creature gets +2/+2 until end of turn. You gain 2 life. */

/* Paladin of Atonement	|1|W
 * Creature - Vampire Knight 1/1
 * At the beginning of each upkeep, if you lost life last turn, put a +1/+1 counter on ~.
 * When ~ dies, you gain life equal to its toughness. */

/* Pride of Conquerors	|1|W
 * Instant
 * Ascend
 * Creatures you control get +1/+1 until end of turn. If you have the city's blessing, those creatures get +2/+2 until end of turn instead. */

/* Radiant Destiny	|2|W
 * Enchantment
 * Ascend
 * As ~ enters the battlefield, choose a creature type.
 * Creatures you control of the chosen type get +1/+1. As long as you have the city's blessing, they also have vigilance. */

// Raptor Companion - reprint from Ixalan

/* Sanguine Glorifier	|3|W
 * Creature - Vampire Cleric 3/3
 * When ~ enters the battlefield, put a +1/+1 counter on another target Vampire you control. */

/* Skymarcher Aspirant	|W
 * Creature - Vampire Soldier 2/1
 * Ascend
 * ~ has flying as long as you have the city's blessing. */

/* Slaughter the Strong	|1|W|W
 * Sorcery
 * Each player chooses any number of creatures they control with total power 4 or less, then sacrifices all other creatures they control. */

/* Snubhorn Sentry	|W
 * Creature - Dinosaur 0/3
 * Ascend
 * ~ gets +3/+0 as long as you have the city's blessing. */

/* Sphinx's Decree	|1|W
 * Sorcery
 * Each opponent can't cast instant or sorcery spells during that player's next turn. */

/* Squire's Devotion	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has lifelink.
 * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Sun Sentinel	|1|W
 * Creature - Human Soldier 2/2
 * Vigilance */

/* Sun-Crested Pterodon	|4|W
 * Creature - Dinosaur 2/5
 * Flying
 * ~ has vigilance as long as you control another Dinosaur. */

/* Temple Altisaur	|4|W
 * Creature - Dinosaur 3/4
 * If a source would deal damage to another Dinosaur you control, prevent all but 1 of that damage. */

/* Trapjaw Tyrant	|3|W|W
 * Creature - Dinosaur 5/5
 * Enrage - Whenever ~ is dealt damage, exile target creature an opponent controls until ~ leaves the battlefield. */

/* Zetalpa, Primal Dawn	|6|W|W
 * Legendary Creature - Elder Dinosaur 4/8
 * Flying, double strike, vigilance, trample, indestructible */

/********
 * Blue *
 ********/

/* Admiral's Order	|1|U|U
 * Instant
 * Raid - If you attacked with a creature this turn, you may pay |U rather than pay this spell's mana cost.
 * Counter target spell. */

/* Aquatic Incursion	|3|U
 * Enchantment
 * When ~ enters the battlefield, create two 1/1 |Sblue Merfolk creature tokens with hexproof.
 * |3|U: Target Merfolk can't be blocked this turn. */

/* Crafty Cutpurse	|3|U
 * Creature - Human Pirate 2/2
 * Flash
 * When ~ enters the battlefield, each token that would be created under an opponent's control this turn is created under your control instead. */

/* Crashing Tide	|2|U
 * Sorcery
 * ~ has flash as long as you control a Merfolk.
 * Return target creature to its owner's hand.
 * Draw a card. */

/* Curious Obsession	|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has "Whenever this creature deals combat damage to a player, you may draw a card."
 * At the beginning of your end step, if you didn't attack with a creature this turn, sacrifice ~. */

/* Deadeye Rig-Hauler	|3|U
 * Creature - Human Pirate 3/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, you may return target creature to its owner's hand. */

/* Expel from Orazca	|1|U
 * Instant
 * Ascend
 * Return target nonland permanent to its owner's hand. If you have the city's blessing, you may put that permanent on top of its owner's library instead. */

/* Flood of Recollection	|U|U
 * Sorcery
 * Return target instant or sorcery card from your graveyard to your hand. Exile ~. */

/* Hornswoggle	|2|U
 * Instant
 * Counter target creature spell. You create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Induced Amnesia	|2|U
 * Enchantment
 * When ~ enters the battlefield, target player exiles all cards from their hand face down, then draws that many cards.
 * When ~ is put into a graveyard from the battlefield, return the exiled cards to their owner's hand. */

/* Kitesail Corsair	|1|U
 * Creature - Human Pirate 2/1
 * ~ has flying as long as it's attacking. */

/* Kumena's Awakening	|2|U|U
 * Enchantment
 * Ascend
 * At the beginning of your upkeep, each player draws a card. If you have the city's blessing, instead only you draw a card. */

/* Mist-Cloaked Herald	|U
 * Creature - Merfolk Warrior 1/1
 * ~ can't be blocked. */

// Negate - reprint from Morningtide

/* Nezahal, Primal Tide	|5|U|U
 * Legendary Creature - Elder Dinosaur 7/7
 * ~ can't be countered.
 * You have no maximum hand size.
 * Whenever an opponent casts a noncreature spell, draw a card.
 * Discard three cards: Exile ~. Return it to the battlefield tapped under its owner's control at the beginning of the next end step. */

/* Release to the Wind	|2|U
 * Instant
 * Exile target nonland permanent. For as long as that card remains exiled, its owner may cast it without paying its mana cost. */

/* River Darter	|2|U
 * Creature - Merfolk Warrior 2/3
 * ~ can't be blocked by Dinosaurs. */

/* Riverwise Augur	|3|U
 * Creature - Merfolk Wizard 2/2
 * When ~ enters the battlefield, draw three cards, then put two cards from your hand on top of your library in any order. */

// Sailor of Means - reprint from Ixalan

/* Sea Legs	|U
 * Enchantment - Aura
 * Flash
 * Enchant creature
 * Enchanted creature gets +0/+2 as long as it's a Pirate. Otherwise, it gets -2/-0. */

/* Seafloor Oracle	|2|U|U
 * Creature - Merfolk Wizard 2/3
 * Whenever a Merfolk you control deals combat damage to a player, draw a card. */

/* Secrets of the Golden City	|1|U|U
 * Sorcery
 * Ascend
 * Draw two cards. If you have the city's blessing, draw three cards instead. */

// Silvergill Adept - reprint from Lorwyn

/* Siren Reaver	|3|U
 * Creature - Siren Pirate 3/2
 * Raid - This spell costs |1 less to cast if you attacked with a creature this turn.
 * Flying */

/* Slippery Scoundrel	|2|U
 * Creature - Human Pirate 2/2
 * Ascend
 * As long as you have the city's blessing, ~ has hexproof and can't be blocked. */

/* Soul of the Rapids	|3|U|U
 * Creature - Elemental 3/2
 * Flying
 * Hexproof */

/* Spire Winder	|3|U
 * Creature - Snake 2/3
 * Flying
 * Ascend
 * ~ gets +1/+1 as long as you have the city's blessing. */

/* Sworn Guardian	|1|U
 * Creature - Merfolk Warrior 1/3 */

/* Timestream Navigator	|1|U
 * Creature - Human Pirate Wizard 1/1
 * Ascend
 * |2|U|U, |T, Put ~ on the bottom of its owner's library: Take an extra turn after this one. Activate this ability only if you have the city's blessing. */

/* Warkite Marauder	|1|U
 * Creature - Human Pirate 2/1
 * Flying
 * Whenever ~ attacks, target creature defending player controls loses all abilities and has base power and toughness 0/1 until end of turn. */

/* Waterknot	|1|U|U
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

/*********
 * Black *
 ********/

/* Arterial Flow	|1|B|B
 * Sorcery
 * Each opponent discards two cards. If you control a Vampire, each opponent loses 2 life and you gain 2 life. */

/* Canal Monitor	|4|B
 * Creature - Lizard 5/3 */

/* Champion of Dusk	|3|B|B
 * Creature - Vampire Knight 4/4
 * When ~ enters the battlefield, you draw X cards and you lose X life, where X is the number of Vampires you control. */

/* Dark Inquiry	|2|B
 * Sorcery
 * Target opponent reveals their hand. You choose a nonland card from it. That player discards that card. */

/* Dead Man's Chest	|1|B
 * Enchantment - Aura
 * Enchant creature an opponent controls
 * When enchanted creature dies, exile cards equal to its power from the top of its owner's library. You may cast nonland cards from among them for as long as they remain exiled, and you may spend mana as though it were mana of any type to cast those spells. */

/* Dinosaur Hunter	|1|B
 * Creature - Human Pirate 2/2
 * Whenever ~ deals damage to a Dinosaur, destroy that creature. */

/* Dire Fleet Poisoner	|1|B
 * Creature - Human Pirate 2/2
 * Flash
 * Deathtouch
 * When ~ enters the battlefield, target attacking Pirate you control gets +1/+1 and gains deathtouch until end of turn. */

/* Dusk Charger	|3|B
 * Creature - Horse 3/3
 * Ascend
 * ~ gets +2/+2 as long as you have the city's blessing. */

/* Dusk Legion Zealot	|1|B
 * Creature - Vampire Soldier 1/1
 * When ~ enters the battlefield, you draw a card and you lose 1 life. */

/* Fathom Fleet Boarder	|2|B
 * Creature - Orc Pirate 3/3
 * When ~ enters the battlefield, you lose 2 life unless you control another Pirate. */

/* Forerunner of the Coalition	|2|B
 * Creature - Human Pirate 2/2
 * When ~ enters the battlefield, you may search your library for a Pirate card, reveal it, then shuffle your library and put that card on top of it.
 * Whenever another Pirate enters the battlefield under your control, each opponent loses 1 life. */

/* Golden Demise	|1|B|B
 * Sorcery
 * Ascend
 * All creatures get -2/-2 until end of turn. If you have the city's blessing, instead only creatures your opponents control get -2/-2 until end of turn. */

/* Grasping Scoundrel	|B
 * Creature - Human Pirate 1/1
 * ~ gets +1/+0 as long as it's attacking. */

/* Gruesome Fate	|2|B
 * Sorcery
 * Each opponent loses 1 life for each creature you control. */

/* Impale	|2|B|B
 * Sorcery
 * Destroy target creature. */

/* Mastermind's Acquisition	|2|B|B
 * Sorcery
 * Choose one -
 * * Search your library for a card, put it into your hand, then shuffle your library.
 * * Choose a card you own from outside the game and put it into your hand. */

/* Mausoleum Harpy	|4|B
 * Creature - Harpy 3/3
 * Flying
 * Ascend
 * Whenever another creature you control dies, if you have the city's blessing, put a +1/+1 counter on ~. */

/* Moment of Craving	|1|B
 * Instant
 * Target creature gets -2/-2 until end of turn. You gain 2 life. */

/* Oathsworn Vampire	|1|B
 * Creature - Vampire Knight 2/2
 * ~ enters the battlefield tapped.
 * You may cast ~ from your graveyard if you gained life this turn. */

/* Pitiless Plunderer	|3|B
 * Creature - Human Pirate 1/4
 * Whenever another creature you control dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Ravenous Chupacabra	|2|B|B
 * Creature - Beast Horror 2/2
 * When ~ enters the battlefield, destroy target creature an opponent controls. */

/* Reaver Ambush	|2|B
 * Instant
 * Exile target creature with power 3 or less. */

// Recover - reprint from Invasion

/* Sadistic Skymarcher	|2|B
 * Creature - Vampire Soldier 2/2
 * As an additional cost to cast this spell, reveal a Vampire card from your hand or pay |1.
 * Flying, lifelink */

/* Tetzimoc, Primal Death	|4|B|B
 * Legendary Creature - Elder Dinosaur 6/6
 * Deathtouch
 * |B, Reveal ~ from your hand: Put a prey counter on target creature. Activate this ability only during your turn.
 * When ~ enters the battlefield, destroy each creature your opponents control with a prey counter on it. */

/* Tomb Robber	|2|B
 * Creature - Human Pirate 1/1
 * Menace
 * |1, Discard a card: ~ explores. */

/* Twilight Prophet	|2|B|B
 * Creature - Vampire Cleric 2/4
 * Flying
 * Ascend
 * At the beginning of your upkeep, if you have the city's blessing, reveal the top card of your library and put it into your hand. Each opponent loses X life and you gain X life, where X is that card's converted mana cost. */

/* Vampire Revenant	|3|B
 * Creature - Vampire Spirit 3/1
 * Flying */

/* Vona's Hunger	|2|B
 * Instant
 * Ascend
 * Each opponent sacrifices a creature. If you have the city's blessing, instead each opponent sacrifices half the creatures they control, rounded up. */

/* Voracious Vampire	|2|B
 * Creature - Vampire Knight 2/2
 * Menace
 * When ~ enters the battlefield, target Vampire you control gets +1/+1 and gains menace until end of turn. */

/*******
 * Red *
 *******/

/* Blood Sun	|2|R
 * Enchantment
 * When ~ enters the battlefield, draw a card.
 * All lands lose all abilities except mana abilities. */

/* Bombard	|2|R
 * Instant
 * ~ deals 4 damage to target creature. */

/* Brass's Bounty	|6|R
 * Sorcery
 * For each land you control, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Brazen Freebooter	|3|R
 * Creature - Human Pirate 3/3
 * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Buccaneer's Bravado	|1|R
 * Instant
 * Choose one -
 * * Target creature gets +1/+1 and gains first strike until end of turn.
 * * Target Pirate gets +1/+1 and gains double strike until end of turn. */

/* Charging Tuskodon	|3|R|R
 * Creature - Dinosaur 4/4
 * Trample
 * If ~ would deal combat damage to a player, it deals double that damage to that player instead. */

/* Daring Buccaneer	|R
 * Creature - Human Pirate 2/2
 * As an additional cost to cast this spell, reveal a Pirate card from your hand or pay |2. */

/* Dire Fleet Daredevil	|1|R
 * Creature - Human Pirate 2/1
 * First strike
 * When ~ enters the battlefield, exile target instant or sorcery card from an opponent's graveyard. You may cast that card this turn, and you may spend mana as though it were mana of any type to cast that spell. If that card would be put into a graveyard this turn, exile it instead. */

/* Etali, Primal Storm	|4|R|R
 * Legendary Creature - Elder Dinosaur 6/6
 * Whenever ~ attacks, exile the top card of each player's library, then you may cast any number of nonland cards exiled this way without paying their mana costs. */

/* Fanatical Firebrand	|R
 * Creature - Goblin Pirate 1/1
 * Haste
 * |T, Sacrifice ~: It deals 1 damage to any target. */

/* Forerunner of the Empire	|3|R
 * Creature - Human Soldier 1/3
 * When ~ enters the battlefield, you may search your library for a Dinosaur card, reveal it, then shuffle your library and put that card on top of it.
 * Whenever a Dinosaur enters the battlefield under your control, you may have ~ deal 1 damage to each creature. */

/* Form of the Dinosaur	|4|R|R
 * Enchantment
 * When ~ enters the battlefield, your life total becomes 15.
 * At the beginning of your upkeep, ~ deals 15 damage to target creature an opponent controls and that creature deals damage equal to its power to you. */

/* Frilled Deathspitter	|2|R
 * Creature - Dinosaur 3/2
 * Enrage - Whenever ~ is dealt damage, it deals 2 damage to target opponent or planeswalker. */

/* Goblin Trailblazer	|1|R
 * Creature - Goblin Pirate 2/1
 * Menace */

/* Mutiny	|R
 * Sorcery
 * Target creature an opponent controls deals damage equal to its power to another target creature that player controls. */

/* Needletooth Raptor	|3|R
 * Creature - Dinosaur 2/2
 * Enrage - Whenever ~ is dealt damage, it deals 5 damage to target creature an opponent controls. */

/* Orazca Raptor	|2|R|R
 * Creature - Dinosaur 3/4 */

/* Pirate's Pillage	|3|R
 * Sorcery
 * As an additional cost to cast this spell, discard a card.
 * Draw two cards and create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Reckless Rage	|R
 * Instant
 * ~ deals 4 damage to target creature you don't control and 2 damage to target creature you control. */

/* Rekindling Phoenix	|2|R|R
 * Creature - Phoenix 4/3
 * Flying
 * When ~ dies, create a 0/1 |Sred Elemental creature token with "At the beginning of your upkeep, sacrifice this creature and return target card named ~ from your graveyard to the battlefield. It gains haste until end of turn." */

/* See Red	|1|R
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has first strike.
 * At the beginning of your end step, if you didn't attack with a creature this turn, sacrifice ~. */

/* Shake the Foundations	|2|R
 * Instant
 * ~ deals 1 damage to each creature without flying.
 * Draw a card. */

// Shatter - reprint from Alpha

/* Silverclad Ferocidons	|5|R|R
 * Creature - Dinosaur 8/5
 * Enrage - Whenever ~ is dealt damage, each opponent sacrifices a permanent. */

/* Stampeding Horncrest	|4|R
 * Creature - Dinosaur 4/4
 * ~ has haste as long as you control another Dinosaur. */

/* Storm Fleet Swashbuckler	|1|R
 * Creature - Human Pirate 2/2
 * Ascend
 * ~ has double strike as long as you have the city's blessing. */

/* Sun-Collared Raptor	|1|R
 * Creature - Dinosaur 1/2
 * Trample
 * |2|R: ~ gets +3/+0 until end of turn. */

/* Swaggering Corsair	|2|R
 * Creature - Human Pirate 2/2
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Tilonalli's Crown	|1|R
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, it deals 1 damage to enchanted creature.
 * Enchanted creature gets +3/+0 and has trample. */

/* Tilonalli's Summoner	|1|R
 * Creature - Human Shaman 1/1
 * Ascend
 * Whenever ~ attacks, you may pay |X|R. If you do, create X 1/1 |Sred Elemental creature tokens that are tapped and attacking. At the beginning of the next end step, exile those tokens unless you have the city's blessing. */

/*********
 * Green *
 *********/

// Aggressive Urge - reprint from Invasion

/* Cacophodon	|3|G
 * Creature - Dinosaur 2/5
 * Enrage - Whenever ~ is dealt damage, untap target permanent. */

/* Cherished Hatchling	|1|G
 * Creature - Dinosaur 2/1
 * When ~ dies, you may cast Dinosaur spells this turn as though they had flash, and whenever you cast a Dinosaur spell this turn, it gains "When this creature enters the battlefield, you may have it fight another target creature." */

// Colossal Dreadmaw - reprint from Ixalan

/* Crested Herdcaller	|3|G|G
 * Creature - Dinosaur 3/3
 * Trample
 * When ~ enters the battlefield, create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Deeproot Elite	|1|G
 * Creature - Merfolk Warrior 1/1
 * Whenever another Merfolk enters the battlefield under your control, put a +1/+1 counter on target Merfolk you control. */

/* Enter the Unknown	|G
 * Sorcery
 * Target creature you control explores.
 * You may play an additional land this turn. */

/* Forerunner of the Heralds	|3|G
 * Creature - Merfolk Scout 3/2
 * When ~ enters the battlefield, you may search your library for a Merfolk card, reveal it, then shuffle your library and put that card on top of it.
 * Whenever another Merfolk enters the battlefield under your control, put a +1/+1 counter on ~. */

/* Ghalta, Primal Hunger	|10|G|G
 * Legendary Creature - Elder Dinosaur 12/12
 * ~ costs |X less to cast, where X is the total power of creatures you control.
 * Trample */

/* Giltgrove Stalker	|1|G
 * Creature - Merfolk Warrior 2/1
 * ~ can't be blocked by creatures with power 2 or less. */

/* Hardy Veteran	|1|G
 * Creature - Human Warrior 2/2
 * As long as it's your turn, ~ gets +0/+2. */

// Hunt the Weak - reprint from Magic 2014

/* Jade Bearer	|G
 * Creature - Merfolk Shaman 1/1
 * When ~ enters the battlefield, put a +1/+1 counter on another target Merfolk you control. */

/* Jadecraft Artisan	|3|G
 * Creature - Merfolk Shaman 3/3
 * When ~ enters the battlefield, target creature gets +2/+2 until end of turn. */

/* Jadelight Ranger	|1|G|G
 * Creature - Merfolk Scout 2/1
 * When ~ enters the battlefield, it explores, then it explores again. */

/* Jungleborn Pioneer	|2|G
 * Creature - Merfolk Scout 2/2
 * When ~ enters the battlefield, create a 1/1 |Sblue Merfolk creature token with hexproof. */

/* Knight of the Stampede	|3|G
 * Creature - Human Knight 2/4
 * Dinosaur spells you cast cost |2 less to cast. */

// Naturalize - reprint from Onslaught

/* Orazca Frillback	|2|G
 * Creature - Dinosaur 4/2 */

/* Overgrown Armasaur	|3|G|G
 * Creature - Dinosaur 4/4
 * Enrage - Whenever ~ is dealt damage, create a 1/1 |Sgreen Saproling creature token. */

/* Path of Discovery	|3|G
 * Enchantment
 * Whenever a creature enters the battlefield under your control, it explores. */

// Plummet - reprint from Magic 2011

/* Polyraptor	|6|G|G
 * Creature - Dinosaur 5/5
 * Enrage - Whenever ~ is dealt damage, create a token that's a copy of ~. */

/* Strength of the Pack	|4|G|G
 * Sorcery
 * Put two +1/+1 counters on each creature you control. */

/* Swift Warden	|1|G|G
 * Creature - Merfolk Warrior 3/3
 * Flash
 * When ~ enters the battlefield, target Merfolk you control gains hexproof until end of turn. */

/* Tendershoot Dryad	|4|G
 * Creature - Dryad 2/2
 * Ascend
 * At the beginning of each upkeep, create a 1/1 |Sgreen Saproling creature token.
 * Saprolings you control get +2/+2 as long as you have the city's blessing. */

/* Thrashing Brontodon	|1|G|G
 * Creature - Dinosaur 3/4
 * |1, Sacrifice ~: Destroy target artifact or enchantment. */

/* Thunderherd Migration	|1|G
 * Sorcery
 * As an additional cost to cast this spell, reveal a Dinosaur card from your hand or pay |1.
 * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

/* Wayward Swordtooth	|2|G
 * Creature - Dinosaur 5/5
 * Ascend
 * You may play an additional land on each of your turns.
 * ~ can't attack or block unless you have the city's blessing. */

/* World Shaper	|3|G
 * Creature - Merfolk Shaman 3/3
 * Whenever ~ attacks, you may put the top three cards of your library into your graveyard.
 * When ~ dies, put all land cards from your graveyard onto the battlefield tapped. */

/********
 * Gold *
 ********/

/* Angrath, the Flame-Chained	|3|B|R
 * Legendary Planeswalker - Angrath (4)
 * +1: Each opponent discards a card and loses 2 life.
 * -3: Gain control of target creature until end of turn. Untap it. It gains haste until end of turn. Sacrifice it at the beginning of the next end step if it has converted mana cost 3 or less.
 * -8: Each opponent loses life equal to the number of cards in their graveyard. */

/* Atzocan Seer	|1|G|W
 * Creature - Human Druid 2/3
 * |T: Add one mana of any color.
 * Sacrifice ~: Return target Dinosaur card from your graveyard to your hand. */

/* Azor, the Lawbringer	|2|W|W|U|U
 * Legendary Creature - Sphinx 6/6
 * Flying
 * When ~ enters the battlefield, each opponent can't cast instant or sorcery spells during that player's next turn.
 * Whenever ~ attacks, you may pay |X|W|U|U. If you do, you gain X life and draw X cards. */

/* Deadeye Brawler	|2|U|B
 * Creature - Human Pirate 2/4
 * Deathtouch
 * Ascend
 * Whenever ~ deals combat damage to a player, if you have the city's blessing, draw a card. */

/* Dire Fleet Neckbreaker	|2|B|R
 * Creature - Orc Pirate 3/2
 * Attacking Pirates you control get +2/+0. */

/* Elenda, the Dusk Rose	|2|W|B
 * Legendary Creature - Vampire Knight 1/1
 * Lifelink
 * Whenever another creature dies, put a +1/+1 counter on ~.
 * When ~ dies, create X 1/1 |Swhite Vampire creature tokens with lifelink, where X is ~'s power. */

/* Hadana's Climb	|1|G|U
 * Legendary Enchantment
 * At the beginning of combat on your turn, put a +1/+1 counter on target creature you control. Then if that creature has three or more +1/+1 counters on it, transform ~. */

/* Huatli, Radiant Champion	|2|G|W
 * Legendary Planeswalker - Huatli (3)
 * +1: Put a loyalty counter on ~ for each creature you control.
 * -1: Target creature gets +X/+X until end of turn, where X is the number of creatures you control.
 * -8: You get an emblem with "Whenever a creature enters the battlefield under your control, you may draw a card." */

/* Huatli, Radiant Champion Emblem	""
 * Emblem - Huatli
 * Whenever a creature enters the battlefield under your control, you may draw a card. */

/* Journey to Eternity	|1|B|G
 * Legendary Enchantment - Aura
 * Enchant creature you control
 * When enchanted creature dies, return it to the battlefield under your control, then return ~ to the battlefield transformed under your control. */

/* Jungle Creeper	|1|B|G
 * Creature - Elemental 3/3
 * |3|B|G: Return ~ from your graveyard to your hand. */

/* Kumena, Tyrant of Orazca	|1|G|U
 * Legendary Creature - Merfolk Shaman 2/4
 * Tap another untapped Merfolk you control: ~ can't be blocked this turn.
 * Tap three untapped Merfolk you control: Draw a card.
 * Tap five untapped Merfolk you control: Put a +1/+1 counter on each Merfolk you control. */

/* Legion Lieutenant	|W|B
 * Creature - Vampire Knight 2/2
 * Other Vampires you control get +1/+1. */

/* Merfolk Mistbinder	|G|U
 * Creature - Merfolk Shaman 2/2
 * Other Merfolk you control get +1/+1. */

/* Path of Mettle	|R|W
 * Legendary Enchantment
 * When ~ enters the battlefield, it deals 1 damage to each creature that doesn't have first strike, double strike, vigilance, or haste.
 * Whenever you attack with at least two creatures that have first strike, double strike, vigilance, and/or haste, transform ~. */

/* Profane Procession	|1|W|B
 * Legendary Enchantment
 * |3|W|B: Exile target creature. Then if there are three or more cards exiled with ~, transform it. */

/* Protean Raider	|1|U|R
 * Creature - Shapeshifter Pirate 2/2
 * Raid - If you attacked with a creature this turn, you may have ~ enter the battlefield as a copy of any creature on the battlefield. */

/* Raging Regisaur	|2|R|G
 * Creature - Dinosaur 4/4
 * Whenever ~ attacks, it deals 1 damage to any target. */

/* Relentless Raptor	|R|W
 * Creature - Dinosaur 3/3
 * Vigilance
 * ~ attacks or blocks each combat if able. */

/* Resplendent Griffin	|1|W|U
 * Creature - Griffin 2/2
 * Flying
 * Ascend
 * Whenever ~ attacks, if you have the city's blessing, put a +1/+1 counter on it. */

/* Siegehorn Ceratops	|G|W
 * Creature - Dinosaur 2/2
 * Enrage - Whenever ~ is dealt damage, put two +1/+1 counters on it. */

/* Storm Fleet Sprinter	|1|U|R
 * Creature - Human Pirate 2/2
 * Haste
 * ~ can't be blocked. */

/* Storm the Vault	|2|U|R
 * Legendary Enchantment
 * Whenever one or more creatures you control deal combat damage to a player, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * At the beginning of your end step, if you control five or more artifacts, transform ~. */

/* Zacama, Primal Calamity	|6|R|G|W
 * Legendary Creature - Elder Dinosaur 9/9
 * Vigilance, reach, trample
 * When ~ enters the battlefield, if you cast it, untap all lands you control.
 * |2|R: ~ deals 3 damage to target creature.
 * |2|G: Destroy target artifact or enchantment.
 * |2|W: You gain 3 life. */

/************
 * Artifact *
 ************/

/* Awakened Amalgam	|4
 * Artifact Creature - Golem 100/100
 * ~'s power and toughness are each equal to the number of differently named lands you control. */

/* Azor's Gateway	|2
 * Legendary Artifact
 * |1, |T: Draw a card, then exile a card from your hand. If cards with five or more different converted mana costs are exiled with ~, you gain 5 life, untap ~, and transform it. */

/* Captain's Hook	|3
 * Artifact - Equipment
 * Equipped creature gets +2/+0, has menace, and is a Pirate in addition to its other creature types.
 * Whenever ~ becomes unattached from a permanent, destroy that permanent.
 * Equip |1 */

/* Gleaming Barrier	|2
 * Artifact Creature - Wall 0/4
 * Defender
 * When ~ dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Golden Guardian	|4
 * Artifact Creature - Golem 4/4
 * Defender
 * |2: ~ fights another target creature you control. When ~ dies this turn, return it to the battlefield transformed under your control. */

/* The Immortal Sun	|6
 * Legendary Artifact
 * Players can't activate planeswalkers' loyalty abilities.
 * At the beginning of your draw step, draw an additional card.
 * Spells you cast cost |1 less to cast.
 * Creatures you control get +1/+1. */

/* Orazca Relic	|3
 * Artifact
 * Ascend
 * |T: Add |C.
 * |T, Sacrifice ~: You gain 3 life and draw a card. Activate this ability only if you have the city's blessing. */

/* Silent Gravestone	|1
 * Artifact
 * Cards in graveyards can't be the targets of spells or abilities.
 * |4, |T: Exile ~ and all cards from all graveyards. Draw a card. */

// Strider Harness - reprint from Scars of Mirrodin

// Traveler's Amulet - reprint from Innistrad

/********
 * Land *
 ********/

/* Arch of Orazca	""
 * Land
 * Ascend
 * |T: Add |C.
 * |5, |T: Draw a card. Activate this ability only if you have the city's blessing. */

// Evolving Wilds - reprint from Rise of the Eldrazi

// Forsaken Sanctuary - reprint from Shadows over Innistrad

// Foul Orchard - reprint from Shadows over Innistrad

// Highland Lake - reprint from Shadows over Innistrad

// Stone Quarry - reprint from Shadows over Innistrad

// Woodland Stream - reprint from Shadows over Innistrad

// Plains, Island, Swamp, Mountain, Forest

/**********************
 * Planeswalker Decks *
 **********************/

/* Vraska, Scheming Gorgon	|4|B|B
 * Legendary Planeswalker - Vraska (5)
 * +2: Creatures you control get +1/+0 until end of turn.
 * -3: Destroy target creature.
 * -10: Until end of turn, creatures you control gain deathtouch and "Whenever this creature deals damage to an opponent, that player loses the game." */

/* Vampire Champion	|3|B
 * Creature - Vampire Soldier 3/3
 * Deathtouch */

/* Vraska's Conquistador	|1|B
 * Creature - Vampire Soldier 2/1
 * Whenever ~ attacks or blocks, if you control a Vraska planeswalker, target opponent loses 2 life and you gain 2 life. */

/* Vraska's Scorn	|2|B|B
 * Sorcery
 * Target opponent loses 4 life. You may search your library and/or graveyard for a card named Vraska, Scheming Gorgon, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

/* Angrath, Minotaur Pirate	|4|B|R
 * Legendary Planeswalker - Angrath (5)
 * +2: ~ deals 1 damage to target opponent or planeswalker and each creature that player or that planeswalker's controller controls.
 * -3: Return target Pirate card from your graveyard to the battlefield.
 * -11: Destroy all creatures target opponent controls. ~ deals damage to that player equal to their total power. */

/* Angrath's Ambusher	|2|B
 * Creature - Orc Pirate 2/3
 * ~ gets +2/+0 as long as you control an Angrath planeswalker. */

/* Swab Goblin	|1|R
 * Creature - Goblin Pirate 2/2 */

/* Angrath's Fury	|3|B|R
 * Sorcery
 * Destroy target creature. ~ deals 3 damage to target player or planeswalker. You may search your library and/or graveyard for a card named Angrath, Minotaur Pirate, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

// Cinder Barrens - reprint from Oath of the Gatewatch
