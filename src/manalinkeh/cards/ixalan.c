#include "manalink.h"

/* Adanto Vanguard	|1|W
 * Creature - Vampire Soldier 1/1
 * As long as ~ is attacking, it gets +2/+0.
 * Pay 4 life: ~ gains indestructible until end of turn. */

/* Ashes of the Abhorrent	|1|W
 * Enchantment
 * Players can't cast spells from graveyards or activate abilities of cards in graveyards.
 * Whenever a creature dies, you gain 1 life. */

/* Axis of Mortality	|4|W|W
 * Enchantment
 * At the beginning of your upkeep, you may have two target players exchange life totals. */

/* Bellowing Aegisaur	|5|W
 * Creature - Dinosaur 3/5
 * Enrage - Whenever ~ is dealt damage, put a +1/+1 counter on each other creature you control. */

/* Bishop of Rebirth	|3|W|W
 * Creature - Vampire Cleric 3/4
 * Vigilance
 * Whenever ~ attacks, you may return target creature card with converted mana cost 3 or less from your graveyard to the battlefield. */

/* Bishop's Soldier	|1|W	=>m10.c:card_child_of_night 0x200415c
 * Creature - Vampire Soldier 2/2
 * Lifelink */

/* Bright Reprisal	|4|W
 * Instant
 * Destroy target attacking creature.
 * Draw a card. */

// Demystify: reprint from Onslaught

/* Duskborne Skymarcher	|W
 * Creature - Vampire Cleric 1/1
 * Flying
 * |W, |T: Target attacking Vampire gets +1/+1 until end of turn. */

/* Emissary of Sunrise	|2|W
 * Creature - Human Cleric 2/1
 * First strike
 * When ~ enters the battlefield, it explores. */

/* Encampment Keeper	|W
 * Creature - Hound 1/1
 * First strike
 * |7|W, |T, Sacrifice ~: Creatures you control get +2/+2 until end of turn. */

/* Glorifier of Dusk	|3|W|W
 * Creature - Vampire Soldier 4/4
 * Pay 2 life: ~ gains flying until end of turn.
 * Pay 2 life: ~ gains vigilance until end of turn. */

/* Goring Ceratops	|5|W|W
 * Creature - Dinosaur 3/3
 * Double strike
 * Whenever ~ attacks, other creatures you control gain double strike until end of turn. */

/* Imperial Aerosaur	|3|W
 * Creature - Dinosaur 3/3
 * Flying
 * When ~ enters the battlefield, another target creature you control gets +1/+1 and gains flying until end of turn. */

/* Imperial Lancer	|W
 * Creature - Human Knight 1/1
 * ~ has double strike as long as you control a Dinosaur. */

/* Inspiring Cleric	|2|W	=>rise_of_the_eldrazi.c:card_lone_missionary 0x20056dd
 * Creature - Vampire Cleric 3/2
 * When ~ enters the battlefield, you gain 4 life. */

/* Ixalan's Binding	|3|W
 * Enchantment
 * When ~ enters the battlefield, exile target nonland permanent an opponent controls until ~ leaves the battlefield.
 * Your opponents can't cast spells with the same name as the exiled card. */

/* Kinjalli's Caller	|W
 * Creature - Human Cleric 0/3
 * Dinosaur spells you cast cost |1 less to cast. */

/* Kinjalli's Sunwing	|2|W	=>m14.c:card_imposing_sovereign 0x200aa84
 * Creature - Dinosaur 2/3
 * Flying
 * Creatures your opponents control enter the battlefield tapped. */

/* Legion Conquistador	|2|W
 * Creature - Vampire Soldier 2/2
 * When ~ enters the battlefield, you may search your library for any number of cards named ~, reveal them, put them into your hand, then shuffle your library. */

/* Legion's Judgment	|2|W	=>innistrad.c:card_smite_the_monstrous 0x2004a44
 * Sorcery
 * Destroy target creature with power 4 or greater. */

/* Legion's Landing	|W
 * Legendary Enchantment
 * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink.
 * When you attack with three or more creatures, transform ~. */
/* Adanto, the First Fort	""
 * Legendary Land
 * |T: Add |W.
 * |2|W, |T: Create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Looming Altisaur	|3|W	0x401000
 * Creature - Dinosaur 1/7 */

/* Mavren Fein, Dusk Apostle	|2|W
 * Legendary Creature - Vampire Cleric 2/2
 * Whenever one or more nontoken Vampires you control attack, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Paladin of the Bloodstained	|3|W
 * Creature - Vampire Knight 3/2
 * When ~ enters the battlefield, create a 1/1 |Swhite Vampire creature token with lifelink. */

/* Pious Interdiction	|3|W
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, you gain 2 life.
 * Enchanted creature can't attack or block. */

/* Priest of the Wakening Sun	|W
 * Creature - Human Cleric 1/1
 * At the beginning of your upkeep, you may reveal a Dinosaur card from your hand. If you do, you gain 2 life.
 * |3|W|W, Sacrifice ~: Search your library for a Dinosaur card, reveal it, put it into your hand, then shuffle your library. */

/* Pterodon Knight	|3|W
 * Creature - Human Knight 3/3
 * ~ has flying as long as you control a Dinosaur. */

/* Queen's Commission	|2|W
 * Sorcery
 * Create two 1/1 |Swhite Vampire creature tokens with lifelink. */

/* Rallying Roar	|2|W
 * Instant
 * Creatures you control get +1/+1 until end of turn. Untap them. */

/* Raptor Companion	|1|W	0x401000
 * Creature - Dinosaur 3/1 */

/* Ritual of Rejuvenation	|2|W
 * Instant
 * You gain 4 life.
 * Draw a card. */

/* Sanguine Sacrament	|X|W|W
 * Instant
 * You gain twice X life. Put ~ on the bottom of its owner's library. */

/* Settle the Wreckage	|2|W|W
 * Instant
 * Exile all attacking creatures target player controls. That player may search their library for that many basic land cards, put those cards onto the battlefield tapped, then shuffle their library. */

/* Sheltering Light	|W
 * Instant
 * Target creature gains indestructible until end of turn. Scry 1. */

/* Shining Aerosaur	|4|W	0x401000
 * Creature - Dinosaur 3/4
 * Flying */

/* Skyblade of the Legion	|1|W	0x401000
 * Creature - Vampire Soldier 1/3
 * Flying */

/* Slash of Talons	|W	=>avacyn_restored.c:card_righteous_blow 0x2005cff
 * Instant
 * ~ deals 2 damage to target attacking or blocking creature. */

/* Steadfast Armasaur	|3|W
 * Creature - Dinosaur 2/3
 * Vigilance
 * |1|W, |T: ~ deals damage equal to its toughness to target creature blocking or blocked by it. */

/* Sunrise Seeker	|4|W
 * Creature - Human Scout 3/3
 * Vigilance
 * When ~ enters the battlefield, it explores. */

/* Territorial Hammerskull	|2|W
 * Creature - Dinosaur 2/3
 * Whenever ~ attacks, tap target creature an opponent controls. */

/* Tocatli Honor Guard	|1|W	=>0x401000; =CARD_ID_TORPOR_ORB
 * Creature - Human Soldier 1/3
 * Creatures entering the battlefield don't cause abilities to trigger. */

/* Vampire's Zeal	|W
 * Instant
 * Target creature gets +2/+2 until end of turn. If it's a Vampire, it gains first strike until end of turn. */

/* Wakening Sun's Avatar	|5|W|W|W
 * Creature - Dinosaur Avatar 7/7
 * When ~ enters the battlefield, if you cast it from your hand, destroy all non-Dinosaur creatures. */

// Air Elemental: reprint from Alpha

/* Arcane Adaptation	|2|U
 * Enchantment
 * As ~ enters the battlefield, choose a creature type.
 * Creatures you control are the chosen type in addition to their other types. The same is true for creature spells you control and creature cards you own that aren't on the battlefield. */

// Cancel: reprint from Time Spiral

/* Chart a Course	|1|U
 * Sorcery
 * Draw two cards. Then discard a card unless you attacked with a creature this turn. */

/* Daring Saboteur	|1|U
 * Creature - Human Pirate 2/1
 * |2|U: ~ can't be blocked this turn.
 * Whenever ~ deals combat damage to a player, you may draw a card. If you do, discard a card. */

/* Deadeye Quartermaster	|3|U
 * Creature - Human Pirate 2/2
 * When ~ enters the battlefield, you may search your library for an Equipment or Vehicle card, reveal it, put it into your hand, then shuffle your library. */

/* Deeproot Waters	|2|U
 * Enchantment
 * Whenever you cast a Merfolk spell, create a 1/1 |Sblue Merfolk creature token with hexproof. */

/* Depths of Desire	|2|U
 * Instant
 * Return target creature to its owner's hand. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dive Down	|U	=>dragons_of_tarkir.c:card_glint 0x200da27
 * Instant
 * Target creature you control gets +0/+3 and gains hexproof until end of turn. */

/* Dreamcaller Siren	|2|U|U
 * Creature - Siren Pirate 3/3
 * Flash
 * Flying
 * ~ can block only creatures with flying.
 * When ~ enters the battlefield, if you control another Pirate, tap up to two target nonland permanents. */

/* Entrancing Melody	|X|U|U
 * Sorcery
 * Gain control of target creature with converted mana cost X. */

// Favorable Winds: reprint from Avacyn Restored

/* Fleet Swallower	|5|U|U
 * Creature - Fish 6/6
 * Whenever ~ attacks, target player puts the top half of their library, rounded up, into their graveyard. */

/* Headwater Sentries	|3|U	0x401000
 * Creature - Merfolk Warrior 2/5 */

/* Herald of Secret Streams	|3|U
 * Creature - Merfolk Warrior 2/3
 * Creatures you control with +1/+1 counters on them can't be blocked. */

/* Jace, Cunning Castaway	|1|U|U
 * Legendary Planeswalker - Jace (3)
 * +1: Whenever one or more creatures you control deal combat damage to a player this turn, draw a card, then discard a card.
 * -2: Create a 2/2 |Sblue Illusion creature token with "When this creature becomes the target of a spell, sacrifice it."
 * -5: Create two tokens that are copies of ~, except they're not legendary. */

/* Kopala, Warden of Waves	|1|U|U
 * Legendary Creature - Merfolk Wizard 2/2
 * Spells your opponents cast that target a Merfolk you control cost |2 more to cast.
 * Abilities your opponents activate that target a Merfolk you control cost |2 more to activate. */

/* Lookout's Dispersal	|2|U
 * Instant
 * This spell costs |1 less to cast if you control a Pirate.
 * Counter target spell unless its controller pays |4. */

/* Navigator's Ruin	|2|U
 * Enchantment
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, target opponent puts the top four cards of their library into their graveyard. */

/* One With the Wind	|1|U	=>innistrad.c:card_spectral_flight 0x2004a4e
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2 and has flying. */

// Opt: reprint from Invasion

/* Overflowing Insight	|4|U|U|U
 * Sorcery
 * Target player draws seven cards. */

/* Perilous Voyage	|1|U
 * Instant
 * Return target nonland permanent you don't control to its owner's hand. If its converted mana cost was 2 or less, scry 2. */

/* Pirate's Prize	|3|U
 * Sorcery
 * Draw two cards. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Prosperous Pirates	|4|U
 * Creature - Human Pirate 3/4
 * When ~ enters the battlefield, create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* River Sneak	|1|U
 * Creature - Merfolk Warrior 1/1
 * ~ can't be blocked.
 * Whenever another Merfolk enters the battlefield under your control, ~ gets +1/+1 until end of turn. */

/* River's Rebuke	|4|U|U
 * Sorcery
 * Return all nonland permanents target player controls to their owner's hand. */

/* Run Aground	|3|U
 * Instant
 * Put target artifact or creature on top of its owner's library. */

/* Sailor of Means	|2|U
 * Creature - Human Pirate 1/4
 * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Search for Azcanta	|1|U
 * Legendary Enchantment
 * At the beginning of your upkeep, look at the top card of your library. You may put it into your graveyard. Then if you have seven or more cards in your graveyard, you may transform ~. */
/* Azcanta, the Sunken Ruin	""
 * Legendary Land
 * |T: Add |U.
 * |2|U, |T: Look at the top four cards of your library. You may reveal a noncreature, nonland card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

/* Shaper Apprentice	|1|U
 * Creature - Merfolk Wizard 2/1
 * ~ has flying as long as you control another Merfolk. */

/* Shipwreck Looter	|1|U
 * Creature - Human Pirate 2/1
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, you may draw a card. If you do, discard a card. */

/* Shore Keeper	|U
 * Creature - Trilobite 0/3
 * |7|U, |T, Sacrifice ~: Draw three cards. */

/* Siren Lookout	|2|U
 * Creature - Siren Pirate 1/2
 * Flying
 * When ~ enters the battlefield, it explores. */

/* Siren Stormtamer	|U
 * Creature - Siren Pirate Wizard 1/1
 * Flying
 * |U, Sacrifice ~: Counter target spell or ability that targets you or a creature you control. */

/* Siren's Ruse	|1|U
 * Instant
 * Exile target creature you control, then return that card to the battlefield under its owner's control. If a Pirate was exiled this way, draw a card. */

// Spell Pierce: reprint from Zendikar

/* Spell Swindle	|3|U|U
 * Instant
 * Counter target spell. Create X colorless Treasure artifact tokens, where X is that spell's converted mana cost. They have "|T, Sacrifice this artifact: Add one mana of any color." */

/* Storm Fleet Aerialist	|1|U
 * Creature - Human Pirate 1/2
 * Flying
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Storm Fleet Spy	|2|U
 * Creature - Human Pirate 2/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, draw a card. */

/* Storm Sculptor	|3|U
 * Creature - Merfolk Wizard 3/2
 * ~ can't be blocked.
 * When ~ enters the battlefield, return a creature you control to its owner's hand. */

/* Tempest Caller	|2|U|U
 * Creature - Merfolk Wizard 2/3
 * When ~ enters the battlefield, tap all creatures target opponent controls. */

/* Watertrap Weaver	|2|U	=>m15.c:card_frost_lynx 0x200c8c0
 * Creature - Merfolk Wizard 2/2
 * When ~ enters the battlefield, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step. */

/* Wind Strider	|4|U	=>time_spiral.c:card_ashcoat_bear 0x200cb63
 * Creature - Merfolk Wizard 3/3
 * Flash
 * Flying */

/* Anointed Deacon	|4|B
 * Creature - Vampire Cleric 3/3
 * At the beginning of combat on your turn, you may have target Vampire get +2/+0 until end of turn. */

/* Arguel's Blood Fast	|1|B
 * Legendary Enchantment
 * |1|B, Pay 2 life: Draw a card.
 * At the beginning of your upkeep, if you have 5 or less life, you may transform ~. */
/* Temple of Aclazotz	""
 * Legendary Land
 * |T: Add |B.
 * |T, Sacrifice a creature: You gain life equal to the sacrificed creature's toughness. */

/* Bishop of the Bloodstained	|3|B|B
 * Creature - Vampire Cleric 3/3
 * When ~ enters the battlefield, target opponent loses 1 life for each Vampire you control. */

/* Blight Keeper	|B
 * Creature - Bat Imp 1/1
 * Flying
 * |7|B, |T, Sacrifice ~: Target opponent loses 4 life and you gain 4 life. */

/* Bloodcrazed Paladin	|1|B
 * Creature - Vampire Knight 1/1
 * Flash
 * ~ enters the battlefield with a +1/+1 counter on it for each creature that died this turn. */

/* Boneyard Parley	|5|B|B
 * Sorcery
 * Exile up to five target creature cards from graveyards. An opponent separates those cards into two piles. Put all cards from the pile of your choice onto the battlefield under your control and the rest into their owners' graveyards. */

/* Contract Killing	|3|B|B
 * Sorcery
 * Destroy target creature. Create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Costly Plunder	|1|B
 * Instant
 * As an additional cost to cast this spell, sacrifice an artifact or creature.
 * Draw two cards. */

/* Dark Nourishment	|4|B	=>darksteel.c:card_essence_drain 0x2006768
 * Instant
 * ~ deals 3 damage to any target. You gain 3 life. */

/* Deadeye Tormentor	|2|B
 * Creature - Human Pirate 2/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, target opponent discards a card. */

/* Deadeye Tracker	|B
 * Creature - Human Pirate 1/1
 * |1|B, |T: Exile two target cards from an opponent's graveyard. ~ explores. */

/* Deathless Ancient	|4|B|B
 * Creature - Vampire Knight 4/4
 * Flying
 * Tap three untapped Vampires you control: Return ~ from your graveyard to your hand. */

/* Desperate Castaways	|1|B
 * Creature - Human Pirate 2/3
 * ~ can't attack unless you control an artifact. */

/* Dire Fleet Hoarder	|1|B
 * Creature - Human Pirate 2/1
 * When ~ dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dire Fleet Interloper	|3|B
 * Creature - Human Pirate 2/2
 * Menace
 * When ~ enters the battlefield, it explores. */

/* Dire Fleet Ravager	|3|B|B
 * Creature - Orc Pirate Wizard 4/4
 * Menace, deathtouch
 * When ~ enters the battlefield, each player loses a third of their life, rounded up. */

// Duress: reprint from Urza's Saga

/* Fathom Fleet Captain	|1|B
 * Creature - Human Pirate 2/1
 * Menace
 * Whenever ~ attacks, if you control another nontoken Pirate, you may pay |2. If you do, create a 2/2 |Sblack Pirate creature token with menace. */

/* Fathom Fleet Cutthroat	|3|B
 * Creature - Human Pirate 3/3
 * When ~ enters the battlefield, destroy target creature an opponent controls that was dealt damage this turn. */

/* Grim Captain's Call	|2|B
 * Sorcery
 * Return a Pirate card from your graveyard to your hand, then do the same for Vampire, Dinosaur, and Merfolk. */

/* Heartless Pillage	|2|B
 * Sorcery
 * Target opponent discards two cards.
 * Raid - If you attacked with a creature this turn, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Kitesail Freebooter	|1|B
 * Creature - Human Pirate 1/2
 * Flying
 * When ~ enters the battlefield, target opponent reveals their hand. You choose a noncreature, nonland card from it. Exile that card until ~ leaves the battlefield. */

/* Lurking Chupacabra	|3|B
 * Creature - Beast Horror 2/3
 * Whenever a creature you control explores, target creature an opponent controls gets -2/-2 until end of turn. */

/* March of the Drowned	|B
 * Sorcery
 * Choose one -
 * * Return target creature card from your graveyard to your hand.
 * * Return two target Pirate cards from your graveyard to your hand. */

// Mark of the Vampire: reprint from Magic 2013

/* Queen's Agent	|5|B
 * Creature - Vampire Scout 3/3
 * Lifelink
 * When ~ enters the battlefield, it explores. */

/* Queen's Bay Soldier	|1|B	0x401000
 * Creature - Vampire Soldier 2/2 */

/* Raiders' Wake	|3|B
 * Enchantment
 * Whenever an opponent discards a card, that player loses 2 life.
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, target opponent discards a card. */

/* Revel in Riches	|4|B
 * Enchantment
 * Whenever a creature an opponent controls dies, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * At the beginning of your upkeep, if you control ten or more Treasures, you win the game. */

/* Ruin Raider	|2|B
 * Creature - Orc Pirate 3/2
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, reveal the top card of your library and put that card into your hand. You lose life equal to the card's converted mana cost. */

/* Ruthless Knave	|2|B
 * Creature - Orc Pirate 3/2
 * |2|B, Sacrifice a creature: Create two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color."
 * Sacrifice three Treasures: Draw a card. */

/* Sanctum Seeker	|2|B|B
 * Creature - Vampire Knight 3/4
 * Whenever a Vampire you control attacks, each opponent loses 1 life and you gain 1 life. */

/* Seekers' Squire	|1|B
 * Creature - Human Scout 1/2
 * When ~ enters the battlefield, it explores. */

/* Skittering Heartstopper	|B	=>morningtide.c:card_moonglove_changeling 0x2007b63
 * Creature - Insect 1/2
 * |B: ~ gains deathtouch until end of turn. */

/* Skulduggery	|B
 * Instant
 * Until end of turn, target creature you control gets +1/+1 and target creature an opponent controls gets -1/-1. */

/* Skymarch Bloodletter	|2|B
 * Creature - Vampire Soldier 2/2
 * Flying
 * When ~ enters the battlefield, target opponent loses 1 life and you gain 1 life. */

/* Spreading Rot	|4|B
 * Sorcery
 * Destroy target land. Its controller loses 2 life. */

/* Sword-Point Diplomacy	|2|B
 * Sorcery
 * Reveal the top three cards of your library. For each of those cards, put that card into your hand unless any opponent pays 3 life. Then exile the rest. */

/* Vanquish the Weak	|2|B	=>origins.c:card_reave_soul 0x200dd01
 * Instant
 * Destroy target creature with power 3 or less. */

/* Vicious Conquistador	|B	=>worldwake.c:card_pulse_tracker 0x200292e
 * Creature - Vampire Soldier 1/2
 * Whenever ~ attacks, each opponent loses 1 life. */

/* Vraska's Contempt	|2|B|B
 * Instant
 * Exile target creature or planeswalker. You gain 2 life. */

/* Walk the Plank	|B|B
 * Sorcery
 * Destroy target non-Merfolk creature. */

/* Wanted Scoundrels	|1|B
 * Creature - Human Pirate 4/3
 * When ~ dies, target opponent creates two colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Angrath's Marauders	|5|R|R
 * Creature - Human Pirate 4/4
 * If a source you control would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */

/* Bonded Horncrest	|3|R
 * Creature - Dinosaur 5/5
 * ~ can't attack or block alone. */

/* Brazen Buccaneers	|3|R
 * Creature - Human Pirate 2/2
 * Haste
 * When ~ enters the battlefield, it explores. */

/* Burning Sun's Avatar	|3|R|R|R
 * Creature - Dinosaur Avatar 6/6
 * When ~ enters the battlefield, it deals 3 damage to target opponent or planeswalker and 3 damage to up to one target creature. */

/* Captain Lannery Storm	|2|R
 * Legendary Creature - Human Pirate 2/2
 * Haste
 * Whenever ~ attacks, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * Whenever you sacrifice a Treasure, ~ gets +1/+0 until end of turn. */

/* Captivating Crew	|3|R
 * Creature - Human Pirate 4/3
 * |3|R: Gain control of target creature an opponent controls until end of turn. Untap that creature. It gains haste until end of turn. Activate this ability only any time you could cast a sorcery. */

/* Charging Monstrosaur	|4|R	=>homelands.c:card_ambush_party	0x200bb46
 * Creature - Dinosaur 5/5
 * Trample, haste */

// Demolish: reprint from Odyssey

/* Dinosaur Stampede	|2|R
 * Instant
 * Attacking creatures get +2/+0 until end of turn. Dinosaurs you control gain trample until end of turn. */

// Dual Shot: reprint from Shadows over Innistrad

/* Fathom Fleet Firebrand	|1|R	=> card_wildfire_emissary 200bb0a
 * Creature - Human Pirate 2/2
 * |1|R: ~ gets +1/+0 until end of turn. */

/* Fiery Cannonade	|2|R
 * Instant
 * ~ deals 2 damage to each non-Pirate creature. */

/* Fire Shrine Keeper	|R
 * Creature - Elemental 1/1
 * Menace
 * |7|R, |T, Sacrifice ~: It deals 3 damage to each of up to two target creatures. */

/* Firecannon Blast	|1|R|R
 * Sorcery
 * ~ deals 3 damage to target creature.
 * Raid - ~ deals 6 damage to that creature instead if you attacked with a creature this turn. */

/* Frenzied Raptor	|2|R
 * Creature - Dinosaur 4/2 */

/* Headstrong Brute	|2|R
 * Creature - Orc Pirate 3/3
 * ~ can't block.
 * ~ has menace as long as you control another Pirate. */

// Hijack: reprint from Kaladesh

// Lightning Strike: reprint from Theros

/* Lightning-Rig Crew	|2|R
 * Creature - Goblin Pirate 0/5
 * |T: ~ deals 1 damage to each opponent.
 * Whenever you cast a Pirate spell, untap ~. */

/* Makeshift Munitions	|1|R
 * Enchantment
 * |1, Sacrifice an artifact or creature: ~ deals 1 damage to any target. */

/* Nest Robber	|1|R	=>homelands.c:card_ambush_party	0x200bb46
 * Creature - Dinosaur 2/1
 * Haste */

/* Otepec Huntmaster	|1|R
 * Creature - Human Shaman 1/2
 * Dinosaur spells you cast cost |1 less to cast.
 * |T: Target Dinosaur gains haste until end of turn. */

/* Rampaging Ferocidon	|2|R
 * Creature - Dinosaur 3/3
 * Menace
 * Players can't gain life.
 * Whenever another creature enters the battlefield, ~ deals 1 damage to that creature's controller. */

/* Raptor Hatchling	|1|R
 * Creature - Dinosaur 1/1
 * Enrage - Whenever ~ is dealt damage, create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Repeating Barrage	|1|R|R
 * Sorcery
 * ~ deals 3 damage to any target.
 * Raid - |3|R|R: Return ~ from your graveyard to your hand. Activate this ability only if you attacked with a creature this turn. */

/* Rigging Runner	|R
 * Creature - Goblin Pirate 1/1
 * First strike
 * Raid - ~ enters the battlefield with a +1/+1 counter on it if you attacked with a creature this turn. */

/* Rile	|R
 * Sorcery
 * ~ deals 1 damage to target creature you control. That creature gains trample until end of turn.
 * Draw a card. */

/* Rowdy Crew	|2|R|R
 * Creature - Human Pirate 3/3
 * Trample
 * When ~ enters the battlefield, draw three cards, then discard two cards at random. If two cards that share a card type are discarded this way, put two +1/+1 counters on ~. */

// Rummaging Goblin: reprint from Magic 2013

/* Star of Extinction	|5|R|R
 * Sorcery
 * Destroy target land. ~ deals 20 damage to each creature and each planeswalker. */

/* Storm Fleet Arsonist	|4|R
 * Creature - Orc Pirate 4/4
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, target opponent sacrifices a permanent. */

/* Storm Fleet Pyromancer	|4|R	=>khans_of_tarkir.c:card_mardu_heart_piercer 0x200cea6
 * Creature - Human Pirate Wizard 3/2
 * Raid - When ~ enters the battlefield, if you attacked with a creature this turn, ~ deals 2 damage to any target. */

/* Sun-Crowned Hunters	|4|R|R
 * Creature - Dinosaur 5/4
 * Enrage - Whenever ~ is dealt damage, it deals 3 damage to target opponent or planeswalker. */

/* Sunbird's Invocation	|5|R
 * Enchantment
 * Whenever you cast a spell from your hand, reveal the top X cards of your library, where X is that spell's converted mana cost. You may cast a card revealed this way with converted mana cost X or less without paying its mana cost. Put the rest on the bottom of your library in a random order. */

// Sure Strike: reprint from Battle for Zendikar

/* Swashbuckling	|1|R	=>zendikar.c:card_goblin_war_paint 0x2001b2d
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2 and has haste. */

/* Thrash of Raptors	|3|R
 * Creature - Dinosaur 3/3
 * As long as you control another Dinosaur, ~ gets +2/+0 and has trample. */

/* Tilonalli's Knight	|1|R
 * Creature - Human Knight 2/2
 * Whenever ~ attacks, if you control a Dinosaur, ~ gets +1/+1 until end of turn. */

/* Tilonalli's Skinshifter	|2|R
 * Creature - Human Shaman 0/1
 * Haste
 * Whenever ~ attacks, it becomes a copy of another target nonlegendary attacking creature until end of turn. */

/* Trove of Temptation	|3|R
 * Enchantment
 * Each opponent must attack you or a planeswalker you control with at least one creature each combat if able.
 * At the beginning of your end step, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Unfriendly Fire	|4|R	=>tempest.c:card_lightning_blast 0x200cce4
 * Instant
 * ~ deals 4 damage to any target. */

/* Vance's Blasting Cannons	|3|R
 * Legendary Enchantment
 * At the beginning of your upkeep, exile the top card of your library. If it's a nonland card, you may cast that card this turn.
 * Whenever you cast your third spell in a turn, you may transform ~. */
/* Spitfire Bastion	""
 * Legendary Land
 * |T: Add |R.
 * |2|R, |T: ~ deals 3 damage to any target. */

/* Wily Goblin	|R|R
 * Creature - Goblin Pirate 1/1
 * When ~ enters the battlefield, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Ancient Brontodon	|6|G|G	0x401000
 * Creature - Dinosaur 9/9 */

/* Atzocan Archer	|2|G
 * Creature - Human Archer 1/4
 * Reach
 * When ~ enters the battlefield, you may have it fight another target creature. */

/* Blinding Fog	|2|G
 * Instant
 * Prevent all damage that would be dealt to creatures this turn. Creatures you control gain hexproof until end of turn. */

/* Blossom Dryad	|2|G	=>unlimited.c:card_ley_druid 0x2003c39
 * Creature - Dryad 2/2
 * |T: Untap target land. */

/* Carnage Tyrant	|4|G|G
 * Creature - Dinosaur 7/6
 * This spell can't be countered.
 * Trample, hexproof */

/* Colossal Dreadmaw	|4|G|G	0x401000
 * Creature - Dinosaur 6/6
 * Trample */

/* Commune with Dinosaurs	|G
 * Sorcery
 * Look at the top five cards of your library. You may reveal a Dinosaur or land card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

/* Crash the Ramparts	|2|G	=>mirrodin.c:card_predators_strike 0x200532c
 * Instant
 * Target creature gets +3/+3 and gains trample until end of turn. */

/* Crushing Canopy	|2|G
 * Instant
 * Choose one -
 * * Destroy target creature with flying.
 * * Destroy target enchantment. */

/* Deathgorge Scavenger	|2|G
 * Creature - Dinosaur 3/2
 * Whenever ~ enters the battlefield or attacks, you may exile target card from a graveyard. If a creature card is exiled this way, you gain 2 life. If a noncreature card is exiled this way, ~ gets +1/+1 until end of turn. */

/* Deeproot Champion	|1|G
 * Creature - Merfolk Shaman 1/1
 * Whenever you cast a noncreature spell, put a +1/+1 counter on ~. */

/* Deeproot Warrior	|1|G
 * Creature - Merfolk Warrior 2/2
 * Whenever ~ becomes blocked, it gets +1/+1 until end of turn. */

/* Drover of the Mighty	|1|G
 * Creature - Human Druid 1/1
 * ~ gets +2/+2 as long as you control a Dinosaur.
 * |T: Add one mana of any color. */

/* Emergent Growth	|3|G
 * Sorcery
 * Target creature gets +5/+5 until end of turn and must be blocked this turn if able. */

/* Emperor's Vanguard	|3|G
 * Creature - Human Scout 4/3
 * Whenever ~ deals combat damage to a player, it explores. */

/* Grazing Whiptail	|2|G|G
 * Creature - Dinosaur 3/4
 * Reach */

/* Growing Rites of Itlimoc	|2|G
 * Legendary Enchantment
 * When ~ enters the battlefield, look at the top four cards of your library. You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in any order.
 * At the beginning of your end step, if you control four or more creatures, transform ~. */
/* Itlimoc, Cradle of the Sun	""
 * Legendary Land
 * |T: Add |G.
 * |T: Add |G for each creature you control. */

/* Ixalli's Diviner	|1|G
 * Creature - Human Druid 0/3
 * When ~ enters the battlefield, it explores. */

/* Ixalli's Keeper	|1|G
 * Creature - Human Shaman 2/2
 * |7|G, |T, Sacrifice ~: Target creature gets +5/+5 and gains trample until end of turn. */

/* Jade Guardian	|3|G
 * Creature - Merfolk Shaman 2/2
 * Hexproof
 * When ~ enters the battlefield, put a +1/+1 counter on target Merfolk you control. */

/* Jungle Delver	|G
 * Creature - Merfolk Warrior 1/1
 * |3|G: Put a +1/+1 counter on ~. */

/* Kumena's Speaker	|G
 * Creature - Merfolk Shaman 1/1
 * ~ gets +1/+1 as long as you control another Merfolk or |Han Island. */

/* Merfolk Branchwalker	|1|G
 * Creature - Merfolk Scout 2/1
 * When ~ enters the battlefield, it explores. */

/* New Horizons	|2|G
 * Enchantment - Aura
 * Enchant land
 * When ~ enters the battlefield, put a +1/+1 counter on target creature you control.
 * Enchanted land has "|T: Add two mana of any one color." */

/* Old-Growth Dryads	|G
 * Creature - Dryad 3/3
 * When ~ enters the battlefield, each opponent may search their library for a basic land card, put it onto the battlefield tapped, then shuffle their library. */

/* Pounce	|1|G	=>innistrad.c:card_prey_upon 0x20049db
 * Instant
 * Target creature you control fights target creature you don't control. */

/* Ranging Raptors	|2|G
 * Creature - Dinosaur 2/3
 * Enrage - Whenever ~ is dealt damage, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

/* Ravenous Daggertooth	|2|G
 * Creature - Dinosaur 3/2
 * Enrage - Whenever ~ is dealt damage, you gain 2 life. */

/* Ripjaw Raptor	|2|G|G
 * Creature - Dinosaur 4/5
 * Enrage - Whenever ~ is dealt damage, draw a card. */

/* River Heralds' Boon	|1|G
 * Instant
 * Put a +1/+1 counter on target creature and a +1/+1 counter on up to one target Merfolk. */

/* Savage Stomp	|2|G
 * Sorcery
 * This spell costs |2 less to cast if it targets a Dinosaur you control.
 * Put a +1/+1 counter on target creature you control. Then that creature fights target creature you don't control. */

/* Shapers' Sanctuary	|G
 * Enchantment
 * Whenever a creature you control becomes the target of a spell or ability an opponent controls, you may draw a card. */

// Slice in Twain: reprint from Scars of Mirrodin

/* Snapping Sailback	|4|G
 * Creature - Dinosaur 4/4
 * Flash
 * Enrage - Whenever ~ is dealt damage, put a +1/+1 counter on it. */

/* Spike-Tailed Ceratops	|4|G	=>unlimited.c:card_two_headed_giant_of_foriys 0x200b263
 * Creature - Dinosaur 4/4
 * ~ can block an additional creature each combat. */

/* Thundering Spineback	|5|G|G
 * Creature - Dinosaur 5/5
 * Other Dinosaurs you control get +1/+1.
 * |5|G: Create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Tishana's Wayfinder	|2|G
 * Creature - Merfolk Scout 2/2
 * When ~ enters the battlefield, it explores. */

/* Verdant Rebirth	|1|G
 * Instant
 * Until end of turn, target creature gains "When this creature dies, return it to its owner's hand."
 * Draw a card. */

/* Verdant Sun's Avatar	|5|G|G
 * Creature - Dinosaur Avatar 5/5
 * Whenever ~ or another creature enters the battlefield under your control, you gain life equal to that creature's toughness. */

/* Vineshaper Mystic	|2|G
 * Creature - Merfolk Shaman 1/3
 * When ~ enters the battlefield, put a +1/+1 counter on each of up to two target Merfolk you control. */

/* Waker of the Wilds	|2|G|G
 * Creature - Merfolk Shaman 3/3
 * |X|G|G: Put X +1/+1 counters on target land you control. That land becomes a 0/0 Elemental creature with haste. It's still a land. */

/* Wildgrowth Walker	|1|G
 * Creature - Elemental 1/3
 * Whenever a creature you control explores, put a +1/+1 counter on ~ and you gain 3 life. */

/* Admiral Beckett Brass	|1|U|B|R
 * Legendary Creature - Human Pirate 3/3
 * Other Pirates you control get +1/+1.
 * At the beginning of your end step, gain control of target nonland permanent controlled by a player who was dealt combat damage by three or more Pirates this turn. */

/* Belligerent Brontodon	|5|G|W
 * Creature - Dinosaur 4/6
 * Each creature you control assigns combat damage equal to its toughness rather than its power. */

/* Call to the Feast	|2|W|B
 * Sorcery
 * Create three 1/1 |Swhite Vampire creature tokens with lifelink. */

/* Deadeye Plunderers	|3|U|B
 * Creature - Human Pirate 3/3
 * ~ gets +1/+1 for each artifact you control.
 * |2|U|B: Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color." */

/* Dire Fleet Captain	|B|R
 * Creature - Orc Pirate 2/2
 * Whenever ~ attacks, it gets +1/+1 until end of turn for each other attacking Pirate. */

/* Gishath, Sun's Avatar	|5|R|G|W
 * Legendary Creature - Dinosaur Avatar 7/6
 * Vigilance, trample, haste
 * Whenever ~ deals combat damage to a player, reveal that many cards from the top of your library. Put any number of Dinosaur creature cards from among them onto the battlefield and the rest on the bottom of your library in a random order. */

/* Hostage Taker	|2|U|B
 * Creature - Human Pirate 2/3
 * When ~ enters the battlefield, exile another target creature or artifact until ~ leaves the battlefield. You may cast that card for as long as it remains exiled, and you may spend mana as though it were mana of any type to cast that spell. */

/* Huatli, Warrior Poet	|3|R|W
 * Legendary Planeswalker - Huatli (3)
 * +2: You gain life equal to the greatest power among creatures you control.
 * 0: Create a 3/3 |Sgreen Dinosaur creature token with trample.
 * -X: ~ deals X damage divided as you choose among any number of target creatures. Creatures dealt damage this way can't block this turn. */

/* Marauding Looter	|2|U|R
 * Creature - Human Pirate 4/3
 * Raid - At the beginning of your end step, if you attacked with a creature this turn, you may draw a card. If you do, discard a card. */

/* Raging Swordtooth	|3|R|G
 * Creature - Dinosaur 5/5
 * Trample
 * When ~ enters the battlefield, it deals 1 damage to each other creature. */

/* Regisaur Alpha	|3|R|G
 * Creature - Dinosaur 4/4
 * Other Dinosaurs you control have haste.
 * When ~ enters the battlefield, create a 3/3 |Sgreen Dinosaur creature token with trample. */

/* Shapers of Nature	|1|G|U
 * Creature - Merfolk Shaman 3/3
 * |3|G: Put a +1/+1 counter on target creature.
 * |2|U, Remove a +1/+1 counter from a creature you control: Draw a card. */

/* Sky Terror	|R|W
 * Creature - Dinosaur 2/2
 * Flying, menace */

/* Tishana, Voice of Thunder	|5|G|U
 * Legendary Creature - Merfolk Shaman 100/100
 * ~'s power and toughness are each equal to the number of cards in your hand.
 * You have no maximum hand size.
 * When ~ enters the battlefield, draw a card for each creature you control. */

/* Vona, Butcher of Magan	|3|W|B
 * Legendary Creature - Vampire Knight 4/4
 * Vigilance, lifelink
 * |T, Pay 7 life: Destroy target nonland permanent. Activate this ability only during your turn. */

/* Vraska, Relic Seeker	|4|B|G
 * Legendary Planeswalker - Vraska (6)
 * +2: Create a 2/2 |Sblack Pirate creature token with menace.
 * -3: Destroy target artifact, creature, or enchantment. Create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * -10: Target player's life total becomes 1. */

// Cobbled Wings: reprint from Innistrad

/* Conqueror's Galleon	|4
 * Artifact - Vehicle 2/10
 * When ~ attacks, exile it at end of combat, then return it to the battlefield transformed under your control.
 * Crew 4 */
/* Conqueror's Foothold	""
 * Land
 * |T: Add |C.
 * |2, |T: Draw a card, then discard a card.
 * |4, |T: Draw a card.
 * |6, |T: Return target card from your graveyard to your hand. */

/* Dowsing Dagger	|2
 * Artifact - Equipment
 * When ~ enters the battlefield, target opponent creates two 0/2 |Sgreen Plant creature tokens with defender.
 * Equipped creature gets +2/+1.
 * Whenever equipped creature deals combat damage to a player, you may transform ~.
 * Equip |2 */
/* Lost Vale	""
 * Land
 * |T: Add three mana of any one color. */

/* Dusk Legion Dreadnought	|5
 * Artifact - Vehicle 4/6
 * Vigilance
 * Crew 2 */

/* Elaborate Firecannon	|2
 * Artifact
 * ~ doesn't untap during your untap step.
 * |4, |T: ~ deals 2 damage to any target.
 * At the beginning of your upkeep, you may discard a card. If you do, untap ~. */

/* Fell Flagship	|3
 * Artifact - Vehicle 3/3
 * Pirates you control get +1/+0.
 * Whenever ~ deals combat damage to a player, that player discards a card.
 * Crew 3 */

/* Gilded Sentinel	|4	0x401000
 * Artifact Creature - Golem 3/3 */

/* Hierophant's Chalice	|3
 * Artifact
 * When ~ enters the battlefield, target opponent loses 1 life and you gain 1 life.
 * |T: Add |C. */

/* Pillar of Origins	|2
 * Artifact
 * As ~ enters the battlefield, choose a creature type.
 * |T: Add one mana of any color. Spend this mana only to cast a creature spell of the chosen type. */

/* Pirate's Cutlass	|3
 * Artifact - Equipment
 * When ~ enters the battlefield, attach it to target Pirate you control.
 * Equipped creature gets +2/+1.
 * Equip |2 */

/* Primal Amulet	|4
 * Artifact
 * Instant and sorcery spells you cast cost |1 less to cast.
 * Whenever you cast an instant or sorcery spell, put a charge counter on ~. Then if there are four or more charge counters on it, you may remove those counters and transform it. */
/* Primal Wellspring	""
 * Land
 * |T: Add one mana of any color. When that mana is spent to cast an instant or sorcery spell, copy that spell and you may choose new targets for the copy. */

/* Prying Blade	|1
 * Artifact - Equipment
 * Equipped creature gets +1/+0.
 * Whenever equipped creature deals combat damage to a player, create a colorless Treasure artifact token with "|T, Sacrifice this artifact: Add one mana of any color."
 * Equip |2 */

/* Sentinel Totem	|1
 * Artifact
 * When ~ enters the battlefield, scry 1.
 * |T, Exile ~: Exile all cards from all graveyards. */

/* Shadowed Caravel	|2
 * Artifact - Vehicle 2/2
 * Whenever a creature you control explores, put a +1/+1 counter on ~.
 * Crew 2 */

/* Sleek Schooner	|3	=>kaladesh.c:card_sky_stiff	0x200f4c1
 * Artifact - Vehicle 4/3
 * Crew 1 */

/* Sorcerous Spyglass	|2
 * Artifact
 * As ~ enters the battlefield, look at an opponent's hand, then choose any card name.
 * Activated abilities of sources with the chosen name can't be activated unless they're mana abilities. */

/* Thaumatic Compass	|2
 * Artifact
 * |3, |T: Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library.
 * At the beginning of your end step, if you control seven or more lands, transform ~. */
/* Spires of Orazca	""
 * Land
 * |T: Add |C.
 * |T: Untap target attacking creature an opponent controls and remove it from combat. */

/* Treasure Map	|2
 * Artifact
 * |1, |T: Scry 1. Put a landmark counter on ~. Then if there are three or more landmark counters on it, remove those counters, transform ~, and create three colorless Treasure artifact tokens with "|T, Sacrifice this artifact: Add one mana of any color." */
/* Treasure Cove	""
 * Land
 * |T: Add |C.
 * |T, Sacrifice a Treasure: Draw a card. */

/* Vanquisher's Banner	|5
 * Artifact
 * As ~ enters the battlefield, choose a creature type.
 * Creatures you control of the chosen type get +1/+1.
 * Whenever you cast a creature spell of the chosen type, draw a card. */

// Dragonskull Summit: reprint from Magic 2010

// Drowned Catacomb: reprint from Magic 2010

/* Field of Ruin	""
 * Land
 * |T: Add |C.
 * |2, |T, Sacrifice ~: Destroy target nonbasic land an opponent controls. Each player searches their library for a basic land card, puts it onto the battlefield, then shuffles their library. */

// Glacial Fortress: reprint from Magic 2010

// Rootbound Crag: reprint from Magic 2010

// Sunpetal Grove: reprint from Magic 2010

/* Unclaimed Territory	""
 * Land
 * As ~ enters the battlefield, choose a creature type.
 * |T: Add |C.
 * |T: Add one mana of any color. Spend this mana only to cast a creature spell of the chosen type. */

// Unknown Shores: reprint from Theros

// Plains
// Plains
// Plains
// Plains
// Island
// Island
// Island
// Island
// Swamp
// Swamp
// Swamp
// Swamp
// Mountain
// Mountain
// Mountain
// Mountain
// Forest
// Forest
// Forest
// Forest

/* Jace, Ingenious Mind-Mage	|4|U|U
 * Legendary Planeswalker - Jace (5)
 * +1: Draw a card.
 * +1: Untap all creatures you control.
 * -9: Gain control of up to three target creatures. */

/* Castaway's Despair	|3|U	=>innistrad.c:card_claustrophobia 0x200485f
 * Enchantment - Aura
 * Enchant creature
 * When ~ enters the battlefield, tap enchanted creature.
 * Enchanted creature doesn't untap during its controller's untap step. */

/* Grasping Current	|4|U
 * Sorcery
 * Return up to two target creatures to their owner's hand.
 * Search your library and/or graveyard for a card named Jace, Ingenious Mind-Mage, reveal it, and put it into your hand. If you searched your library this way, shuffle it. */

/* Jace's Sentinel	|1|U
 * Creature - Merfolk Warrior 1/3
 * As long as you control a Jace planeswalker, ~ gets +1/+0 and can't be blocked. */

// Woodland Stream: reprint from Shadows over Innistrad

/* Huatli, Dinosaur Knight	|4|R|W
 * Legendary Planeswalker - Huatli (4)
 * +2: Put two +1/+1 counters on up to one target Dinosaur you control.
 * -3: Target Dinosaur you control deals damage equal to its power to target creature you don't control.
 * -7: Dinosaurs you control get +4/+4 until end of turn. */

/* Huatli's Snubhorn	|1|W	=>unlimited.c:card_serra_angel 0x200c122
 * Creature - Dinosaur 2/2
 * Vigilance */

/* Huatli's Spurring	|R
 * Instant
 * Target creature gets +2/+0 until end of turn. If you control a Huatli planeswalker, that creature gets +4/+0 until end of turn instead. */

/* Sun-Blessed Mount	|3|R|W
 * Creature - Dinosaur 4/4
 * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Huatli, Dinosaur Knight, reveal it, then put it into your hand. If you searched your library this way, shuffle it. */

// Stone Quarry: reprint from Shadows over Innistrad
