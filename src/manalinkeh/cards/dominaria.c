#include "manalink.h"

/* Academy Drake	|2|U
 * Creature - Drake 2/2
 * Kicker |4
 * Flying
 * If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

/* Academy Journeymage	|4|U
 * Creature - Human Wizard 3/2
 * This spell costs |1 less to cast if you control a Wizard.
 * When ~ enters the battlefield, return target creature an opponent controls to its owner's hand. */

/* Adamant Will	|1|W
 * Instant
 * Target creature gets +2/+2 and gains indestructible until end of turn. */

/* Adeliz, the Cinder Wind	|1|U|R
 * Legendary Creature - Human Wizard 2/2
 * Flying, haste
 * Whenever you cast an instant or sorcery spell, Wizards you control get +1/+1 until end of turn. */

/* Adventurous Impulse	|G
 * Sorcery
 * Look at the top three cards of your library. You may reveal a creature or land card from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

/* Amaranthine Wall	|4
 * Artifact Creature - Wall 0/6
 * Defender
 * |2: ~ gains indestructible until end of turn. */

/* Ancient Animus	|1|G
 * Instant
 * Put a +1/+1 counter on target creature you control if it's legendary. Then it fights target creature an opponent controls. */

/* Arbor Armament	|G
 * Instant
 * Put a +1/+1 counter on target creature. That creature gains reach until end of turn. */

/* Arcane Flight	|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1 and has flying. */

/* Artificer's Assistant	|U
 * Creature - Bird 1/1
 * Flying
 * Whenever you cast a historic spell, scry 1. */

/* Arvad the Cursed	|3|W|B
 * Legendary Creature - Vampire Knight 3/3
 * Deathtouch, lifelink
 * Other legendary creatures you control get +2/+2. */

/* Aryel, Knight of Windgrace	|2|W|B
 * Legendary Creature - Human Knight 4/4
 * Vigilance
 * |2|W, |T: Create a 2/2 |Swhite Knight creature token with vigilance.
 * |B, |T, Tap X untapped Knights you control: Destroy target creature with power X or less. */

/* Aven Sentry	|3|W
 * Creature - Bird Soldier 3/2
 * Flying */

/* Baird, Steward of Argive	|2|W|W
 * Legendary Creature - Human Soldier 2/4
 * Vigilance
 * Creatures can't attack you or a planeswalker you control unless their controller pays |1 for each of those creatures. */

/* Baloth Gorger	|2|G|G
 * Creature - Beast 4/4
 * Kicker |4
 * If ~ was kicked, it enters the battlefield with three +1/+1 counters on it. */

/* Befuddle	|2|U
 * Instant
 * Target creature gets -4/-0 until end of turn.
 * Draw a card. */

/* Benalish Honor Guard	|1|W
 * Creature - Human Knight 2/2
 * ~ gets +1/+0 for each legendary creature you control. */

/* Benalish Marshal	|W|W|W
 * Creature - Human Knight 3/3
 * Other creatures you control get +1/+1. */

/* Blackblade Reforged	|2
 * Legendary Artifact - Equipment
 * Equipped creature gets +1/+1 for each land you control.
 * Equip legendary creature |3
 * Equip |7 */

/* Blessed Light	|4|W
 * Instant
 * Exile target creature or enchantment. */

/* Blessing of Belzenlok	|B
 * Instant
 * Target creature gets +2/+1 until end of turn. If it's legendary, it also gains lifelink until end of turn. */

/* Blink of an Eye	|1|U
 * Instant
 * Kicker |1|U
 * Return target nonland permanent to its owner's hand. If this spell was kicked, draw a card. */

/* Bloodstone Goblin	|1|R
 * Creature - Goblin Warrior 2/2
 * Whenever you cast a spell, if that spell was kicked, ~ gets +1/+1 and gains menace until end of turn. */

/* Bloodtallow Candle	|1
 * Artifact
 * |6, |T, Sacrifice ~: Target creature gets -5/-5 until end of turn. */

/* Board the Weatherlight	|1|W
 * Sorcery
 * Look at the top five cards of your library. You may reveal a historic card from among them and put it into your hand. Put the rest on the bottom of your library in a random order. */

/* Broken Bond	|1|G
 * Sorcery
 * Destroy target artifact or enchantment. You may put a land card from your hand onto the battlefield. */

/* Cabal Evangel	|1|B
 * Creature - Human Cleric 2/2 */

/* Cabal Paladin	|3|B
 * Creature - Human Knight 4/2
 * Whenever you cast a historic spell, ~ deals 2 damage to each opponent. */

/* Cabal Stronghold	""
 * Land
 * |T: Add |C.
 * |3, |T: Add |B for each basic |H2Swamp you control. */

/* Caligo Skin-Witch	|1|B
 * Creature - Human Wizard 1/3
 * Kicker |3|B
 * When ~ enters the battlefield, if it was kicked, each opponent discards two cards. */

/* Call the Cavalry	|3|W
 * Sorcery
 * Create two 2/2 |Swhite Knight creature tokens with vigilance. */

/* Cast Down	|1|B
 * Instant
 * Destroy target nonlegendary creature. */

/* Chainer's Torment	|3|B
 * Enchantment - Saga
 * I, II - ~ deals 2 damage to each opponent and you gain 2 life.
 * III - Create an X/X |Sblack Nightmare Horror creature token, where X is half your life total, rounded up. It deals X damage to you. */

/* Nightmare Horror	""(token)
 * Creature - Nightmare Horror 100/100 */

/* Champion of the Flame	|1|R
 * Creature - Human Warrior 1/1
 * Trample
 * ~ gets +2/+2 for each Aura and Equipment attached to it. */

/* Chandra's Outburst	|3|R|R
 * Sorcery
 * ~ deals 4 damage to target player or planeswalker.
 * Search your library and/or graveyard for a card named Chandra, Bold Pyromancer, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

/* Chandra, Bold Pyromancer	|4|R|R
 * Legendary Planeswalker - Chandra (5)
 * +1: Add |R|R. ~ deals 2 damage to target player.
 * -3: ~ deals 3 damage to target creature or planeswalker.
 * -7: ~ deals 10 damage to target player and each creature and planeswalker they control. */

/* Charge	|W
 * Instant
 * Creatures you control get +1/+1 until end of turn. */

/* Cloudreader Sphinx	|4|U
 * Creature - Sphinx 3/4
 * Flying
 * When ~ enters the battlefield, scry 2. */

/* Cold-Water Snapper	|5|U
 * Creature - Turtle 4/5
 * Hexproof */

/* Corrosive Ooze	|1|G
 * Creature - Ooze 2/2
 * Whenever ~ blocks or becomes blocked by an equipped creature, destroy all Equipment attached to that creature at end of combat. */

/* Curator's Ward	|2|U
 * Enchantment - Aura
 * Enchant permanent
 * Enchanted permanent has hexproof.
 * When enchanted permanent leaves the battlefield, if it was historic, draw two cards. */

/* D'Avenant Trapper	|2|W
 * Creature - Human Archer 3/2
 * Whenever you cast a historic spell, tap target creature an opponent controls. */

/* Damping Sphere	|2
 * Artifact
 * If a land is tapped for two or more mana, it produces |C instead of any other type and amount.
 * Each spell a player casts costs |1 more to cast for each other spell that player has cast this turn. */

/* Danitha Capashen, Paragon	|2|W
 * Legendary Creature - Human Knight 2/2
 * First strike, vigilance, lifelink
 * Aura and Equipment spells you cast cost |1 less to cast. */

/* Darigaaz Reincarnated	|4|B|R|G
 * Legendary Creature - Dragon 7/7
 * Flying, trample, haste
 * If ~ would die, instead exile it with three egg counters on it.
 * At the beginning of your upkeep, if ~ is exiled with an egg counter on it, remove an egg counter from it. Then if ~ has no egg counters on it, return it to the battlefield. */

/* Daring Archaeologist	|3|W
 * Creature - Human Artificer 3/3
 * When ~ enters the battlefield, you may return target artifact card from your graveyard to your hand.
 * Whenever you cast a historic spell, put a +1/+1 counter on ~. */

/* Dark Bargain	|3|B
 * Instant
 * Look at the top three cards of your library. Put two of them into your hand and the other into your graveyard. ~ deals 2 damage to you. */

/* Dauntless Bodyguard	|W
 * Creature - Human Knight 2/1
 * As ~ enters the battlefield, choose another creature you control.
 * Sacrifice ~: The chosen creature gains indestructible until end of turn. */

/* Deathbloom Thallid	|2|B
 * Creature - Fungus 3/2
 * When ~ dies, create a 1/1 |Sgreen Saproling creature token. */

/* Deep Freeze	|2|U
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has base power and toughness 0/4, has defender, loses all other abilities, and is a |Sblue Wall in addition to its other colors and types. */

/* Demonic Vigor	|B
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +1/+1.
 * When enchanted creature dies, return that card to its owner's hand. */

/* Demonlord Belzenlok	|4|B|B
 * Legendary Creature - Elder Demon 6/6
 * Flying, trample
 * When ~ enters the battlefield, exile cards from the top of your library until you exile a nonland card, then put that card into your hand. If the card's converted mana cost is 4 or greater, repeat this process. ~ deals 1 damage to you for each card put into your hand this way. */

/* Diligent Excavator	|1|U
 * Creature - Human Artificer 1/3
 * Whenever you cast a historic spell, target player puts the top two cards of their library into their graveyard. */

/* Divest	|B
 * Sorcery
 * Target player reveals their hand. You choose an artifact or creature card from it. That player discards that card. */

/* Dread Shade	|B|B|B
 * Creature - Shade 3/3
 * |B: ~ gets +1/+1 until end of turn. */

/* Drudge Sentinel	|2|B
 * Creature - Skeleton Warrior 2/1
 * |3: Tap ~. It gains indestructible until end of turn. */

/* Dub	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+2, has first strike, and is a Knight in addition to its other types. */

/* Elfhame Druid	|1|G
 * Creature - Elf Druid 0/2
 * |T: Add |G.
 * |T: Add |G|G. Spend this mana only to cast kicked spells. */

/* Eviscerate	|3|B
 * Sorcery
 * Destroy target creature. */

/* Evra, Halcyon Witness	|4|W|W
 * Legendary Creature - Avatar 4/4
 * Lifelink
 * |4: Exchange your life total with ~'s power. */

/* Excavation Elephant	|4|W
 * Creature - Elephant 3/5
 * Kicker |1|W
 * When ~ enters the battlefield, if it was kicked, return target artifact card from your graveyard to your hand. */

/* Fall of the Thran	|5|W
 * Enchantment - Saga
 * I - Destroy all lands.
 * II, III - Each player returns two land cards from their graveyard to the battlefield. */

/* Feral Abomination	|5|B
 * Creature - Thrull 5/5
 * Deathtouch */

/* Fervent Strike	|R
 * Instant
 * Target creature gets +1/+0 and gains first strike and haste until end of turn. */

/* Fiery Intervention	|4|R
 * Sorcery
 * Choose one -
 * * ~ deals 5 damage to target creature.
 * * Destroy target artifact. */

/* Fight with Fire	|2|R
 * Sorcery
 * Kicker |5|R
 * ~ deals 5 damage to target creature. If this spell was kicked, it deals 10 damage divided as you choose among any number of targets instead. */

/* Final Parting	|3|B|B
 * Sorcery
 * Search your library for two cards. Put one into your hand and the other into your graveyard. Then shuffle your library. */

/* Firefist Adept	|4|R
 * Creature - Human Wizard 3/3
 * When ~ enters the battlefield, it deals X damage to target creature an opponent controls, where X is the number of Wizards you control. */

/* Firesong and Sunspeaker	|4|R|W
 * Legendary Creature - Minotaur Cleric 4/6
 * |SRed instant and sorcery spells you control have lifelink.
 * Whenever a |Swhite instant or sorcery spell causes you to gain life, ~ deals 3 damage to target creature or player. */

/* Forebear's Blade	|3
 * Artifact - Equipment
 * Equipped creature gets +3/+0 and has vigilance and trample.
 * Whenever equipped creature dies, attach ~ to target creature you control.
 * Equip |3 */

/* Frenzied Rage	|1|R
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +2/+1 and has menace. */

/* Fungal Infection	|B
 * Instant
 * Target creature gets -1/-1 until end of turn. Create a 1/1 |Sgreen Saproling creature token. */

/* Fungal Plots	|1|G
 * Enchantment
 * |1|G, Exile a creature card from your graveyard: Create a 1/1 |Sgreen Saproling creature token.
 * Sacrifice two Saprolings: You gain 2 life and draw a card. */

/* Gaea's Protector	|3|G
 * Creature - Elemental Warrior 4/2
 * ~ must be blocked if able. */

/* Garna, the Bloodflame	|3|B|R
 * Legendary Creature - Human Warrior 3/3
 * Flash
 * When ~ enters the battlefield, return to your hand all creature cards in your graveyard that were put there from anywhere this turn.
 * Other creatures you control have haste. */

/* Ghitu Chronicler	|1|R
 * Creature - Human Wizard 1/3
 * Kicker |3|R
 * When ~ enters the battlefield, if it was kicked, return target instant or sorcery card from your graveyard to your hand. */

/* Ghitu Journeymage	|2|R
 * Creature - Human Wizard 3/2
 * When ~ enters the battlefield, if you control another Wizard, ~ deals 2 damage to each opponent. */

/* Ghitu Lavarunner	|R
 * Creature - Human Wizard 1/2
 * As long as there are two or more instant and/or sorcery cards in your graveyard, ~ gets +1/+0 and has haste. */

/* Gift of Growth	|1|G
 * Instant
 * Kicker |2
 * Untap target creature. It gets +2/+2 until end of turn. If this spell was kicked, that creature gets +4/+4 until end of turn instead. */

/* Goblin Barrage	|3|R
 * Sorcery
 * Kicker-Sacrifice an artifact or Goblin.
 * ~ deals 4 damage to target creature. If this spell was kicked, it also deals 4 damage to target player or planeswalker. */

/* Goblin Chainwhirler	|R|R|R
 * Creature - Goblin Warrior 3/3
 * First strike
 * When ~ enters the battlefield, it deals 1 damage to each opponent and each creature and planeswalker they control. */

/* Grand Warlord Radha	|2|R|G
 * Legendary Creature - Elf Warrior 3/4
 * Haste
 * Whenever one or more creatures you control attack, add that much mana in any combination of |R and/or |G. Until end of turn, you don't lose this mana as steps and phases end. */

/* Grow from the Ashes	|2|G
 * Sorcery
 * Kicker |2
 * Search your library for a basic land card, put it onto the battlefield, then shuffle your library. If this spell was kicked, instead search your library for two basic land cards, put them onto the battlefield, then shuffle your library. */

/* Grunn, the Lonely King	|4|G|G
 * Legendary Creature - Ape Warrior 5/5
 * Kicker |3
 * If ~ was kicked, it enters the battlefield with five +1/+1 counters on it.
 * Whenever ~ attacks alone, double its power and toughness until end of turn. */

/* Guardians of Koilos	|5
 * Artifact Creature - Construct 4/4
 * When ~ enters the battlefield, you may return another target historic permanent you control to its owner's hand. */

/* Hallar, the Firefletcher	|1|R|G
 * Legendary Creature - Elf Archer 3/3
 * Trample
 * Whenever you cast a spell, if that spell was kicked, put a +1/+1 counter on ~, then ~ deals damage equal to the number of +1/+1 counters on it to each opponent. */

/* Haphazard Bombardment	|5|R
 * Enchantment
 * When ~ enters the battlefield, choose four nonenchantment permanents you don't control and put an aim counter on each of them.
 * At the beginning of your end step, if two or more permanents you don't control have an aim counter on them, destroy one of those permanents at random. */

/* Healing Grace	|W
 * Instant
 * Prevent the next 3 damage that would be dealt to any target this turn by a source of your choice. You gain 3 life. */

/* Helm of the Host	|4
 * Legendary Artifact - Equipment
 * At the beginning of combat on your turn, create a token that's a copy of equipped creature, except the token isn't legendary if equipped creature is legendary. That token gains haste.
 * Equip |5 */

/* History of Benalia	|1|W|W
 * Enchantment - Saga
 * I, II - Create a 2/2 |Swhite Knight creature token with vigilance.
 * III - Knights you control get +2/+1 until end of turn. */

/* Homarid Explorer	|3|U
 * Creature - Homarid Scout 3/3
 * When ~ enters the battlefield, target player puts the top four cards of their library into their graveyard. */

/* Howling Golem	|3
 * Artifact Creature - Golem 2/3
 * Whenever ~ attacks or blocks, each player draws a card. */

/* In Bolas's Clutches	|4|U|U
 * Legendary Enchantment - Aura
 * Enchant permanent
 * You control enchanted permanent.
 * Enchanted permanent is legendary. */

/* Invoke the Divine	|2|W
 * Instant
 * Destroy target artifact or enchantment. You gain 4 life. */

/* Jaya Ballard	|2|R|R|R
 * Legendary Planeswalker - Jaya (5)
 * +1: Add |R|R|R. Spend this mana only to cast instant or sorcery spells.
 * +1: Discard up to three cards, then draw that many cards.
 * -8: You get an emblem with "You may cast instant and sorcery cards from your graveyard. If a card cast this way would be put into your graveyard, exile it instead." */

/* Jaya Ballard Emblem	""
 * Emblem - Jaya
 * You may cast instant and sorcery cards from your graveyard. If a card cast this way would be put into your graveyard, exile it instead. */

/* Jaya's Immolating Inferno	|X|R|R
 * Legendary Sorcery
 * ~ deals X damage to each of up to three targets. */

/* Jhoira's Familiar	|4
 * Artifact Creature - Bird 2/2
 * Flying
 * Historic spells you cast cost |1 less to cast. */

/* Jhoira, Weatherlight Captain	|2|U|R
 * Legendary Creature - Human Artificer 3/3
 * Whenever you cast a historic spell, draw a card. */

/* Jodah, Archmage Eternal	|1|U|R|W
 * Legendary Creature - Human Wizard 4/3
 * Flying
 * You may pay |W|U|B|R|G rather than pay the mana cost for spells that you cast. */

/* Josu Vess, Lich Knight	|2|B|B
 * Legendary Creature - Zombie Knight 4/5
 * Kicker |5|B
 * Menace
 * When ~ enters the battlefield, if it was kicked, create eight 2/2 |Sblack Zombie Knight creature tokens with menace. */

/* Zombie Knight	""(token)
 * Creature - Zombie Knight 2/2
 * Menace */

/* Jousting Lance	|2
 * Artifact - Equipment
 * Equipped creature gets +2/+0.
 * As long as it's your turn, equipped creature has first strike.
 * Equip |3 */

/* Kamahl's Druidic Vow	|X|G|G
 * Legendary Sorcery
 * Look at the top X cards of your library. You may put any number of land and/or legendary permanent cards with converted mana cost X or less from among them onto the battlefield. Put the rest into your graveyard. */

/* Karn's Temporal Sundering	|4|U|U
 * Legendary Sorcery
 * Target player takes an extra turn after this one. Return up to one target nonland permanent to its owner's hand. Exile ~. */

/* Karn, Scion of Urza	|4
 * Legendary Planeswalker - Karn (5)
 * +1: Reveal the top two cards of your library. An opponent chooses one of them. Put that card into your hand and exile the other with a silver counter on it.
 * -1: Put a card you own with a silver counter on it from exile into your hand.
 * -2: Create a 0/0 colorless Construct artifact creature token with "This creature gets +1/+1 for each artifact you control." */

/* Karplusan Hound	|3|R
 * Creature - Hound 3/3
 * Whenever ~ attacks, if you control a Chandra planeswalker, this creature deals 2 damage to any target. */

/* Kazarov, Sengir Pureblood	|5|B|B
 * Legendary Creature - Vampire 4/4
 * Flying
 * Whenever a creature an opponent controls is dealt damage, put a +1/+1 counter on ~.
 * |3|R: ~ deals 2 damage to target creature. */

/* Keldon Overseer	|2|R
 * Creature - Human Warrior 3/1
 * Kicker |3|R
 * Haste
 * When ~ enters the battlefield, if it was kicked, gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn. */

/* Keldon Raider	|2|R|R
 * Creature - Human Warrior 4/3
 * When ~ enters the battlefield, you may discard a card. If you do, draw a card. */

/* Keldon Warcaller	|1|R
 * Creature - Human Warrior 2/2
 * Whenever ~ attacks, put a lore counter on target Saga you control. */

/* Knight of Grace	|1|W
 * Creature - Human Knight 2/2
 * First strike
 * Hexproof from |Sblack
 * ~ gets +1/+0 as long as any player controls a |Sblack permanent. */

/* Knight of Malice	|1|B
 * Creature - Human Knight 2/2
 * First strike
 * Hexproof from |Swhite
 * ~ gets +1/+0 as long as any player controls a |Swhite permanent. */

/* Knight of New Benalia	|1|W
 * Creature - Human Knight 3/1 */

/* Krosan Druid	|2|G
 * Creature - Centaur Druid 2/3
 * Kicker |4|G
 * When ~ enters the battlefield, if it was kicked, you gain 10 life. */

/* Kwende, Pride of Femeref	|3|W
 * Legendary Creature - Human Knight 2/2
 * Double strike
 * Creatures you control with first strike have double strike. */

/* Lich's Mastery	|3|B|B|B
 * Legendary Enchantment
 * Hexproof
 * You can't lose the game.
 * Whenever you gain life, draw that many cards.
 * Whenever you lose life, for each 1 life you lost, exile a permanent you control or a card from your hand or graveyard.
 * When ~ leaves the battlefield, you lose the game. */

/* Lingering Phantom	|5|B
 * Creature - Spirit 5/4
 * Whenever you cast a historic spell, you may pay |B. If you do, return ~ from your graveyard to your hand. */

/* Llanowar Envoy	|2|G
 * Creature - Elf Scout 3/2
 * |1|G: Add one mana of any color. */

/* Llanowar Scout	|1|G
 * Creature - Elf Scout 1/3
 * |T: You may put a land card from your hand onto the battlefield. */

/* Lyra Dawnbringer	|3|W|W
 * Legendary Creature - Angel 5/5
 * Flying, first strike, lifelink
 * Other Angels you control get +1/+1 and have lifelink. */

/* Mammoth Spider	|4|G
 * Creature - Spider 3/5
 * Reach */

/* Marwyn, the Nurturer	|2|G
 * Legendary Creature - Elf Druid 1/1
 * Whenever another Elf enters the battlefield under your control, put a +1/+1 counter on ~.
 * |T: Add an amount of |G equal to ~'s power. */

/* Memorial to Folly	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |B.
 * |2|B, |T, Sacrifice ~: Return target creature card from your graveyard to your hand. */

/* Memorial to Genius	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |U.
 * |4|U, |T, Sacrifice ~: Draw two cards. */

/* Memorial to Glory	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |W.
 * |3|W, |T, Sacrifice ~: Create two 1/1 |Swhite Soldier creature tokens. */

/* Memorial to Unity	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |G.
 * |2|G, |T, Sacrifice ~: Look at the top five cards of your library. You may reveal a creature card from among them and put it into your hand. Then put the rest on the bottom of your library in a random order. */

/* Memorial to War	""
 * Land
 * ~ enters the battlefield tapped.
 * |T: Add |R.
 * |4|R, |T, Sacrifice ~: Destroy target land. */

/* Merfolk Trickster	|U|U
 * Creature - Merfolk Wizard 2/2
 * Flash
 * When ~ enters the battlefield, tap target creature an opponent controls. It loses all abilities until end of turn. */

/* Mesa Unicorn	|1|W
 * Creature - Unicorn 2/2
 * Lifelink */

/* Mishra's Self-Replicator	|5
 * Artifact Creature - Assembly-Worker 2/2
 * Whenever you cast a historic spell, you may pay |1. If you do, create a token that's a copy of ~. */

/* Mox Amber	|0
 * Legendary Artifact
 * |T: Add one mana of any color among legendary creatures and planeswalkers you control. */

/* Muldrotha, the Gravetide	|3|B|G|U
 * Legendary Creature - Elemental Avatar 6/6
 * During each of your turns, you may play up to one permanent card of each permanent type from your graveyard. */

/* Multani, Yavimaya's Avatar	|4|G|G
 * Legendary Creature - Elemental Avatar 0/0
 * Reach, trample
 * ~ gets +1/+1 for each land you control and each land card in your graveyard.
 * |1|G, Return two lands you control to their owner's hand: Return ~ from your graveyard to your hand. */

/* Naban, Dean of Iteration	|1|U
 * Legendary Creature - Human Wizard 2/1
 * If a Wizard entering the battlefield under your control causes a triggered ability of a permanent you control to trigger, that ability triggers an additional time. */

/* Naru Meha, Master Wizard	|2|U|U
 * Legendary Creature - Human Wizard 3/3
 * Flash
 * When ~ enters the battlefield, copy target instant or sorcery spell you control. You may choose new targets for the copy.
 * Other Wizards you control get +1/+1. */

/* Navigator's Compass	|1
 * Artifact
 * When ~ enters the battlefield, you gain 3 life.
 * |T: Until end of turn, target land you control becomes the basic land type of your choice in addition to its other types. */

/* Niambi, Faithful Healer	|1|W|U
 * Legendary Creature - Human Cleric 2/2
 * When ~ enters the battlefield, you may search your library and/or graveyard for a card named Teferi, Timebender, reveal it, and put it into your hand. If you search your library this way, shuffle it. */

/* Oath of Teferi	|3|W|U
 * Legendary Enchantment
 * When ~ enters the battlefield, exile another target permanent you control. Return it to the battlefield under its owner's control at the beginning of the next end step.
 * You may activate the loyalty abilities of planeswalkers you control twice each turn rather than only once. */

/* On Serra's Wings	|3|W
 * Legendary Enchantment - Aura
 * Enchant creature
 * Enchanted creature is legendary, gets +1/+1, and has flying, vigilance, and lifelink. */

/* Orcish Vandal	|1|R
 * Creature - Orc Warrior 1/1
 * |T, Sacrifice an artifact: ~ deals 2 damage to any target. */

/* Pardic Wanderer	|6
 * Artifact Creature - Golem 5/5
 * Trample */

/* Pegasus Courser	|2|W
 * Creature - Pegasus 1/3
 * Flying
 * Whenever ~ attacks, another target attacking creature gains flying until end of turn. */

/* Phyrexian Scriptures	|2|B|B
 * Enchantment - Saga
 * I - Put a +1/+1 counter on up to one target creature. That creature becomes an artifact in addition to its other types.
 * II - Destroy all nonartifact creatures.
 * III - Exile all cards from all opponents' graveyards. */

/* Pierce the Sky	|1|G
 * Instant
 * ~ deals 7 damage to target creature with flying. */

/* Powerstone Shard	|3
 * Artifact
 * |T: Add |C for each artifact you control named ~. */

/* Precognition Field	|3|U
 * Enchantment
 * You may look at the top card of your library.
 * You may cast the top card of your library if it's an instant or sorcery card.
 * |3: Exile the top card of your library. */

/* Primevals' Glorious Rebirth	|5|W|B
 * Legendary Sorcery
 * Return all legendary permanent cards from your graveyard to the battlefield. */

/* Primordial Wurm	|4|G|G
 * Creature - Wurm 7/6 */

/* Pyromantic Pilgrim	|2|R
 * Creature - Human Wizard 3/1
 * Haste */

/* Radiating Lightning	|3|R
 * Instant
 * ~ deals 3 damage to target player and 1 damage to each creature that player controls. */

/* Raff Capashen, Ship's Mage	|2|W|U
 * Legendary Creature - Human Wizard 3/3
 * Flash
 * Flying
 * You may cast historic spells as though they had flash. */

/* Rampaging Cyclops	|3|R
 * Creature - Cyclops 4/4
 * ~ gets -2/-0 as long as two or more creatures are blocking it. */

/* Rat Colony	|1|B
 * Creature - Rat 2/1
 * ~ gets +1/+0 for each other Rat you control.
 * A deck can have any number of cards named ~. */

/* Relic Runner	|1|U
 * Creature - Human Rogue 2/1
 * ~ can't be blocked if you've cast a historic spell this turn. */

/* Rite of Belzenlok	|2|B|B
 * Enchantment - Saga
 * I, II - Create two 0/1 |Sblack Cleric creature tokens.
 * III - Create a 6/6 |Sblack Demon creature token with flying, trample, and "At the beginning of your upkeep, sacrifice another creature. If you can't, this creature deals 6 damage to you." */

/* Rona, Disciple of Gix	|1|U|B
 * Legendary Creature - Human Artificer 2/2
 * When ~ enters the battlefield, you may exile target historic card from your graveyard.
 * You may cast nonland cards exiled with ~.
 * |4, |T: Exile the top card of your library. */

/* Run Amok	|1|R
 * Instant
 * Target attacking creature gets +3/+3 and gains trample until end of turn. */

/* Sanctum Spirit	|3|W
 * Creature - Spirit 3/2
 * Lifelink
 * Discard a historic card: ~ gains indestructible until end of turn. */

/* Saproling Migration	|1|G
 * Sorcery
 * Kicker |4
 * Create two 1/1 |Sgreen Saproling creature tokens. If this spell was kicked, create four of those tokens instead. */

/* Seal Away	|1|W
 * Enchantment
 * Flash
 * When ~ enters the battlefield, exile target tapped creature an opponent controls until ~ leaves the battlefield. */

/* Seismic Shift	|3|R
 * Sorcery
 * Destroy target land. Up to two target creatures can't block this turn. */

/* Sentinel of the Pearl Trident	|4|U
 * Creature - Merfolk Soldier 3/3
 * Flash
 * When ~ enters the battlefield, you may exile target historic permanent you control. If you do, return that card to the battlefield under its owner's control at the beginning of the next end step. */

/* Sergeant-at-Arms	|2|W
 * Creature - Human Soldier 2/3
 * Kicker |2|W
 * When ~ enters the battlefield, if it was kicked, create two 1/1 |Swhite Soldier creature tokens. */

/* Serra Disciple	|1|W
 * Creature - Bird Cleric 1/1
 * Flying, first strike
 * Whenever you cast a historic spell, ~ gets +1/+1 until end of turn. */

/* Settle the Score	|2|B|B
 * Sorcery
 * Exile target creature. Put two loyalty counters on a planeswalker you control. */

/* Shalai, Voice of Plenty	|3|W
 * Legendary Creature - Angel 3/4
 * Flying
 * You, planeswalkers you control, and other creatures you control have hexproof.
 * |4|G|G: Put a +1/+1 counter on each creature you control. */

/* Shanna, Sisay's Legacy	|G|W
 * Legendary Creature - Human Warrior 0/0
 * ~ can't be the target of abilities your opponents control.
 * ~ gets +1/+1 for each creature you control. */

/* Shield of the Realm	|2
 * Artifact - Equipment
 * If a source would deal damage to equipped creature, prevent 2 of that damage.
 * Equip |1 */

/* Shivan Fire	|R
 * Instant
 * Kicker |4
 * ~ deals 2 damage to target creature. If this spell was kicked, it deals 4 damage to that creature instead. */

/* Short Sword	|1
 * Artifact - Equipment
 * Equipped creature gets +1/+1.
 * Equip |1 */

/* Skittering Surveyor	|3
 * Artifact Creature - Construct 1/2
 * When ~ enters the battlefield, you may search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. */

/* Slimefoot, the Stowaway	|1|B|G
 * Legendary Creature - Fungus 2/3
 * Whenever a Saproling you control dies, ~ deals 1 damage to each opponent and you gain 1 life.
 * |4: Create a 1/1 |Sgreen Saproling creature token. */

/* Slinn Voda, the Rising Deep	|6|U|U
 * Legendary Creature - Leviathan 8/8
 * Kicker |1|U
 * When ~ enters the battlefield, if it was kicked, return all creatures to their owners' hands except for Merfolk, Krakens, Leviathans, Octopuses, and Serpents. */

/* Song of Freyalise	|1|G
 * Enchantment - Saga
 * I, II - Until your next turn, creatures you control gain "|T: Add one mana of any color."
 * III - Put a +1/+1 counter on each creature you control. Those creatures gain vigilance, trample, and indestructible until end of turn. */

/* Sorcerer's Wand	|1
 * Artifact - Equipment
 * Equipped creature has "|T: This creature deals 1 damage to target player or planeswalker. If this creature is a Wizard, it deals 2 damage to that player or planeswalker instead."
 * Equip |3 */

/* Soul Salvage	|2|B
 * Sorcery
 * Return up to two target creature cards from your graveyard to your hand. */

/* Sparring Construct	|1
 * Artifact Creature - Construct 1/1
 * When ~ dies, put a +1/+1 counter on target creature you control. */

/* Spore Swarm	|3|G
 * Instant
 * Create three 1/1 |Sgreen Saproling creature tokens. */

/* Sporecrown Thallid	|1|G
 * Creature - Fungus 2/2
 * Each other creature you control that's a Fungus or Saproling gets +1/+1. */

/* Squee, the Immortal	|1|R|R
 * Legendary Creature - Goblin 2/1
 * You may cast ~ from your graveyard or from exile. */

/* Steel Leaf Champion	|G|G|G
 * Creature - Elf Knight 5/4
 * ~ can't be blocked by creatures with power 2 or less. */

/* Stronghold Confessor	|B
 * Creature - Human Cleric 1/1
 * Kicker |3
 * Menace
 * If ~ was kicked, it enters the battlefield with two +1/+1 counters on it. */

/* Sylvan Awakening	|2|G
 * Sorcery
 * Until your next turn, all lands you control become 2/2 Elemental creatures with reach, indestructible, and haste. They're still lands. */

/* Tatyova, Benthic Druid	|3|G|U
 * Legendary Creature - Merfolk Druid 3/3
 * Whenever a land enters the battlefield under your control, you gain 1 life and draw a card. */

/* Teferi's Sentinel	|5
 * Artifact Creature - Golem 2/6
 * As long as you control a Teferi planeswalker, ~ gets +4/+0. */

/* Teferi, Hero of Dominaria	|3|W|U
 * Legendary Planeswalker - Teferi (4)
 * +1: Draw a card. At the beginning of the next end step, untap two lands.
 * -3: Put target nonland permanent into its owner's library third from the top.
 * -8: You get an emblem with "Whenever you draw a card, exile target permanent an opponent controls." */

/* Teferi, Hero of Dominaria Emblem	""
 * Emblem - Teferi
 * Whenever you draw a card, exile target permanent an opponent controls. */

/* Teferi, Timebender	|4|W|U
 * Legendary Planeswalker - Teferi (5)
 * +2: Untap up to one target artifact or creature.
 * -3: You gain 2 life and draw two cards.
 * -9: Take an extra turn after this one. */

/* Tempest Djinn	|U|U|U
 * Creature - Djinn 0/4
 * Flying
 * ~ gets +1/+0 for each basic |H2Island you control. */

/* Temporal Machinations	|2|U
 * Sorcery
 * Return target creature to its owner's hand. If you control an artifact, draw a card. */

/* Territorial Allosaurus	|2|G|G
 * Creature - Dinosaur 5/5
 * Kicker |2|G
 * When ~ enters the battlefield, if it was kicked, it fights another target creature. */

/* Teshar, Ancestor's Apostle	|3|W
 * Legendary Creature - Bird Cleric 2/2
 * Flying
 * Whenever you cast a historic spell, return target creature card with converted mana cost 3 or less from your graveyard to the battlefield. */

/* Tetsuko Umezawa, Fugitive	|1|U
 * Legendary Creature - Human Rogue 1/3
 * Creatures you control with power or toughness 1 or less can't be blocked. */

/* Thallid Omnivore	|3|B
 * Creature - Fungus 3/3
 * |1, Sacrifice another creature: ~ gets +2/+2 until end of turn. If a Saproling was sacrificed this way, you gain 2 life. */

/* Thallid Soothsayer	|3|B
 * Creature - Fungus 2/3
 * |2, Sacrifice a creature: Draw a card. */

/* The Antiquities War	|3|U
 * Enchantment - Saga
 * I, II - Look at the top five cards of your library. You may reveal an artifact card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
 * III - Artifacts you control become artifact creatures with base power and toughness 5/5 until end of turn. */

/* The Eldest Reborn	|4|B
 * Enchantment - Saga
 * I - Each opponent sacrifices a creature or planeswalker.
 * II - Each opponent discards a card.
 * III - Put target creature or planeswalker card from a graveyard onto the battlefield under your control. */

/* The First Eruption	|2|R
 * Enchantment - Saga
 * I - ~ deals 1 damage to each creature without flying.
 * II - Add |R|R.
 * III - Sacrifice |Ha Mountain. If you do, ~ deals 3 damage to each creature. */

/* The Flame of Keld	|1|R
 * Enchantment - Saga
 * I - Discard your hand.
 * II - Draw two cards.
 * III - If a |Sred source you control would deal damage to a permanent or player this turn, it deals that much damage plus 2 to that permanent or player instead. */

/* The Mending of Dominaria	|3|G|G
 * Enchantment - Saga
 * I, II - Put the top two cards of your library into your graveyard, then you may return a creature card from your graveyard to your hand.
 * III - Return all land cards from your graveyard to the battlefield, then shuffle your graveyard into your library. */

/* The Mirari Conjecture	|4|U
 * Enchantment - Saga
 * I - Return target instant card from your graveyard to your hand.
 * II - Return target sorcery card from your graveyard to your hand.
 * III - Until end of turn, whenever you cast an instant or sorcery spell, copy it. You may choose new targets for the copy. */

/* Thran Temporal Gateway	|4
 * Legendary Artifact
 * |4, |T: You may put a historic permanent card from your hand onto the battlefield. */

/* Tiana, Ship's Caretaker	|3|R|W
 * Legendary Creature - Angel Artificer 3/3
 * Flying, first strike
 * Whenever an Aura or Equipment you control is put into a graveyard from the battlefield, you may return that card to its owner's hand at the beginning of the next end step. */

/* Time of Ice	|3|U
 * Enchantment - Saga
 * I, II - Tap target creature an opponent controls. It doesn't untap during its controller's untap step for as long as you control ~.
 * III - Return all tapped creatures to their owners' hands. */

/* Tolarian Scholar	|2|U
 * Creature - Human Wizard 2/3 */

/* Torgaar, Famine Incarnate	|6|B|B
 * Legendary Creature - Avatar 7/6
 * As an additional cost to cast this spell, you may sacrifice any number of creatures. This spell costs |2 less to cast for each creature sacrificed this way.
 * When ~ enters the battlefield, up to one target player's life total becomes half their starting life total, rounded down. */

/* Traxos, Scourge of Kroog	|4
 * Legendary Artifact Creature - Construct 7/7
 * Trample
 * ~ enters the battlefield tapped and doesn't untap during your untap step.
 * Whenever you cast a historic spell, untap ~. */

/* Triumph of Gerrard	|1|W
 * Enchantment - Saga
 * I, II - Put a +1/+1 counter on target creature you control with the greatest power.
 * III - Target creature you control with the greatest power gains flying, first strike, and lifelink until end of turn. */

/* Two-Headed Giant	|2|R|R
 * Creature - Giant Warrior 4/4
 * Whenever ~ attacks, flip two coins. If both coins come up heads, ~ gains double strike until end of turn. If both coins come up tails, ~ gains menace until end of turn. */

/* Untamed Kavu	|1|G
 * Creature - Kavu 2/2
 * Kicker |3
 * Vigilance, trample
 * If ~ was kicked, it enters the battlefield with three +1/+1 counters on it. */

/* Unwind	|2|U
 * Instant
 * Counter target noncreature spell. Untap up to three lands. */

/* Urgoros, the Empty One	|4|B|B
 * Legendary Creature - Specter 4/3
 * Flying
 * Whenever ~ deals combat damage to a player, that player discards a card at random. If the player can't, you draw a card. */

/* Urza's Ruinous Blast	|4|W
 * Legendary Sorcery
 * Exile all nonland permanents that aren't legendary. */

/* Urza's Tome	|2
 * Artifact
 * |3, |T: Draw a card. Then discard a card unless you exile a historic card from your graveyard. */

/* Valduk, Keeper of the Flame	|2|R
 * Legendary Creature - Human Shaman 3/2
 * At the beginning of combat on your turn, for each Aura and Equipment attached to ~, create a 3/1 |Sred Elemental creature token with trample and haste. Exile those tokens at the beginning of the next end step. */

/* Verix Bladewing	|2|R|R
 * Legendary Creature - Dragon 4/4
 * Kicker |3
 * Flying
 * When ~ enters the battlefield, if it was kicked, create Karox Bladewing, a legendary 4/4 |Sred Dragon creature token with flying. */

/* Karox Bladewing	""(token)
 * Legendary Creature - Dragon 4/4
 * Flying */

/* Vicious Offering	|1|B
 * Instant
 * Kicker-Sacrifice a creature.
 * Target creature gets -2/-2 until end of turn. If this spell was kicked, that creature gets -5/-5 until end of turn instead. */

/* Vodalian Arcanist	|1|U
 * Creature - Merfolk Wizard 1/3
 * |T: Add |C. Spend this mana only to cast an instant or sorcery spell. */

/* Voltaic Servant	|2
 * Artifact Creature - Construct 1/3
 * At the beginning of your end step, untap target artifact. */

/* Warcry Phoenix	|3|R
 * Creature - Phoenix 2/2
 * Flying, haste
 * Whenever you attack with three or more creatures, you may pay |2|R. If you do, return ~ from your graveyard to the battlefield tapped and attacking. */

/* Warlord's Fury	|R
 * Sorcery
 * Creatures you control gain first strike until end of turn.
 * Draw a card. */

/* Weatherlight	|4
 * Legendary Artifact - Vehicle 4/5
 * Flying
 * Whenever ~ deals combat damage to a player, look at the top five cards of your library. You may reveal a historic card from among them and put it into your hand. Put the rest on the bottom of your library in a random order.
 * Crew 3 */

/* Weight of Memory	|3|U|U
 * Sorcery
 * Draw three cards. Target player puts the top three cards of their library into their graveyard. */

/* Whisper, Blood Liturgist	|3|B
 * Legendary Creature - Human Cleric 2/2
 * |T, Sacrifice two creatures: Return target creature card from your graveyard to the battlefield. */

/* Wild Onslaught	|3|G
 * Instant
 * Kicker |4
 * Put a +1/+1 counter on each creature you control. If this spell was kicked, put two +1/+1 counters on each creature you control instead. */

/* Windgrace Acolyte	|4|B
 * Creature - Cat Warrior 3/2
 * Flying
 * When ~ enters the battlefield, put the top three cards of your library into your graveyard and you gain 3 life. */

/* Wizard's Lightning	|2|R
 * Instant
 * This spell costs |2 less to cast if you control a Wizard.
 * ~ deals 3 damage to any target. */

/* Wizard's Retort	|1|U|U
 * Instant
 * This spell costs |1 less to cast if you control a Wizard.
 * Counter target spell. */

/* Yargle, Glutton of Urborg	|4|B
 * Legendary Creature - Frog Spirit 9/3 */

/* Yavimaya Sapherd	|2|G
 * Creature - Fungus 2/2
 * When ~ enters the battlefield, create a 1/1 |Sgreen Saproling creature token. */

/* Yawgmoth's Vile Offering	|4|B
 * Legendary Sorcery
 * Put up to one target creature or planeswalker card from a graveyard onto the battlefield under your control. Destroy up to one target creature or planeswalker. Exile ~. */

/* Zahid, Djinn of the Lamp	|4|U|U
 * Legendary Creature - Djinn 5/6
 * You may pay |3|U and tap an untapped artifact you control rather than pay this spell's mana cost.
 * Flying */

/* Zhalfirin Void	""
 * Land
 * When ~ enters the battlefield, scry 1.
 * |T: Add |C. */
