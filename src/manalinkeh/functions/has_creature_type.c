#include "manalink.h"

static int add_force_a_subtype_until_eot_legacy(int player, int card, event_t event);

/* Canonical recovered helper declarations. */
void force_a_subtype(int player, int card, subtype_t subtype);
void add_a_subtype(int player, int card, subtype_t subtype);
void get_subtypes_by_csvid(int player, int csvid, reported_subtypes_t *my_subtypes);
int has_subtype_by_id(int player, int id, subtype_t subtype);

/* October 2022 .bss subtype table @ 0x2797020. */
subtypes_not_in_play_t added_subtypes_not_in_play[2][16];
#define NUM_SUBTYPES 26

static const subtype_t arr_from_hardcoded_subtype_to_subtype[] =
{
  0,
  SUBTYPE_INCARNATION,	// HARDCODED_SUBTYPE_INCARNATION
  SUBTYPE_ARTIFICER,	// HARDCODED_SUBTYPE_ARTIFICER
  SUBTYPE_ADVISOR,		// HARDCODED_SUBTYPE_ADVISOR
  SUBTYPE_PEST,			// HARDCODED_SUBTYPE_PEST
  SUBTYPE_ANGEL,		// HARDCODED_SUBTYPE_ANGEL
  SUBTYPE_SNOW,			// HARDCODED_SUBTYPE_SNOW
  SUBTYPE_APE,			// HARDCODED_SUBTYPE_APE
  SUBTYPE_NIGHTSTALKER,	// HARDCODED_SUBTYPE_NIGHTSTALKER
  SUBTYPE_ARCHER,		// HARDCODED_SUBTYPE_ARCHER
  SUBTYPE_DRYAD,		// HARDCODED_SUBTYPE_DRYAD
  SUBTYPE_AURA_ARTIFACT,// HARDCODED_SUBTYPE_ARTIFACT
  SUBTYPE_ANTELOPE,		// HARDCODED_SUBTYPE_ANTELOPE
  SUBTYPE_ASSASSIN,		// HARDCODED_SUBTYPE_ASSASSIN
  SUBTYPE_ATOG,			// HARDCODED_SUBTYPE_ATOG
  SUBTYPE_AVATAR,		// HARDCODED_SUBTYPE_AVATAR
  SUBTYPE_ARCHON,		// HARDCODED_SUBTYPE_ARCHON
  SUBTYPE_PLANT,		// HARDCODED_SUBTYPE_PLANT
  SUBTYPE_BADGER,		// HARDCODED_SUBTYPE_BADGER
  SUBTYPE_AUROCHS,		// HARDCODED_SUBTYPE_AUROCHS
  SUBTYPE_LOCUS,		// HARDCODED_SUBTYPE_LOCUS
  SUBTYPE_BLINKMOTH,	// HARDCODED_SUBTYPE_BLINKMOTH
  SUBTYPE_BASILISK,		// HARDCODED_SUBTYPE_BASILISK
  SUBTYPE_BAT,			// HARDCODED_SUBTYPE_BAT
  SUBTYPE_BEAR,			// HARDCODED_SUBTYPE_BEAR
  SUBTYPE_BEAST,		// HARDCODED_SUBTYPE_BEAST
  SUBTYPE_BRINGER,		// HARDCODED_SUBTYPE_BRINGER
  SUBTYPE_SLIVER,		// HARDCODED_SUBTYPE_SLIVER
  SUBTYPE_BERSERKER,	// HARDCODED_SUBTYPE_BERSERKER
  SUBTYPE_MONK,			// HARDCODED_SUBTYPE_MONK
  SUBTYPE_BOAR,			// HARDCODED_SUBTYPE_BOAR
  SUBTYPE_TRAP,			// HARDCODED_SUBTYPE_TRAP
  SUBTYPE_ALLY,			// HARDCODED_SUBTYPE_ALLY
  SUBTYPE_OX,			// HARDCODED_SUBTYPE_OX
  SUBTYPE_CYCLOPS,		// HARDCODED_SUBTYPE_CYCLOPS
  SUBTYPE_CAMEL,		// HARDCODED_SUBTYPE_CAMEL
  SUBTYPE_EQUIPMENT,	// HARDCODED_SUBTYPE_EQUIPMENT
  SUBTYPE_CEPHALID,		// HARDCODED_SUBTYPE_CEPHALID
  SUBTYPE_CHIMERA,		// HARDCODED_SUBTYPE_CHIMERA
  SUBTYPE_CENTAUR,		// HARDCODED_SUBTYPE_CENTAUR
  SUBTYPE_CLERIC,		// HARDCODED_SUBTYPE_CLERIC
  SUBTYPE_SHAPESHIFTER,	// HARDCODED_SUBTYPE_SHAPESHIFTER
  SUBTYPE_CONSTRUCT,	// HARDCODED_SUBTYPE_CONSTRUCT
  SUBTYPE_COCKATRICE,	// HARDCODED_SUBTYPE_COCKATRICE
  SUBTYPE_AURA_CREATURE,// HARDCODED_SUBTYPE_ARTIFACT_CREATURE_OR_AURA_MOSTLY_WITH_ENCHANT_CREATURE
  SUBTYPE_CURSE,		// HARDCODED_SUBTYPE_CURSE
  SUBTYPE_DEMON,		// HARDCODED_SUBTYPE_DEMON
  SUBTYPE_ELDRAZI,		// HARDCODED_SUBTYPE_ELDRAZI
  SUBTYPE_DEVIL,		// HARDCODED_SUBTYPE_DEVIL
  SUBTYPE_CRAB,			// HARDCODED_SUBTYPE_CRAB
  SUBTYPE_DJINN,		// HARDCODED_SUBTYPE_DJINN
  SUBTYPE_SHAMAN,		// HARDCODED_SUBTYPE_SHAMAN
  SUBTYPE_DRAGON,		// HARDCODED_SUBTYPE_DRAGON
  SUBTYPE_DAUTHI,		// HARDCODED_SUBTYPE_DAUTHI
  SUBTYPE_DRAKE,		// HARDCODED_SUBTYPE_DRAKE
  SUBTYPE_DESERTER,		// HARDCODED_SUBTYPE_DESERTER
  SUBTYPE_DRUID,		// HARDCODED_SUBTYPE_DRUID
  SUBTYPE_DWARF,		// HARDCODED_SUBTYPE_DWARF
  SUBTYPE_EYE,			// HARDCODED_SUBTYPE_EYE
  SUBTYPE_DREADNOUGHT,	// HARDCODED_SUBTYPE_DREADNOUGHT
  SUBTYPE_DRONE,		// HARDCODED_SUBTYPE_DRONE
  SUBTYPE_EFREET,		// HARDCODED_SUBTYPE_EFREET
  SUBTYPE_BIRD,			// HARDCODED_SUBTYPE_EGG	// Best fit
  SUBTYPE_ELK,			// HARDCODED_SUBTYPE_ELK
  SUBTYPE_ELDER,		// HARDCODED_SUBTYPE_ELDER
  SUBTYPE_ELEMENTAL,	// HARDCODED_SUBTYPE_ELEMENTAL
  SUBTYPE_ELEPHANT,		// HARDCODED_SUBTYPE_ELEPHANT
  SUBTYPE_ELF,			// HARDCODED_SUBTYPE_ELF
  SUBTYPE_AURA_ENCHANTMENT,	// HARDCODED_SUBTYPE_ENCHANTMENT
  SUBTYPE_NONBASIC,		// HARDCODED_SUBTYPE_NONBASIC_LAND
  SUBTYPE_FLAGBEARER,	// HARDCODED_SUBTYPE_FLAGBEARER
  SUBTYPE_MINION,		// HARDCODED_SUBTYPE_MINION
  SUBTYPE_FOX,			// HARDCODED_SUBTYPE_FOX
  SUBTYPE_FAERIE,		// HARDCODED_SUBTYPE_FAERIE
  SUBTYPE_HOMUNCULUS,	// HARDCODED_SUBTYPE_HOMUNCULUS
  SUBTYPE_FROG,			// HARDCODED_SUBTYPE_FROG
  SUBTYPE_GOAT,			// HARDCODED_SUBTYPE_GOAT
  SUBTYPE_GRAVEBORN,	// HARDCODED_SUBTYPE_GRAVEBORN
  SUBTYPE_HARPY,		// HARDCODED_SUBTYPE_HARPY
  SUBTYPE_FUNGUS,		// HARDCODED_SUBTYPE_FUNGUS
  SUBTYPE_HELLION,		// HARDCODED_SUBTYPE_HELLION
  SUBTYPE_GARGOYLE,		// HARDCODED_SUBTYPE_GARGOYLE
  SUBTYPE_HIPPO,		// HARDCODED_SUBTYPE_HIPPO
  SUBTYPE_HOMARID,		// HARDCODED_SUBTYPE_HOMARID
  SUBTYPE_GIANT,		// HARDCODED_SUBTYPE_GIANT
  SUBTYPE_GNOME,		// HARDCODED_SUBTYPE_GNOME
  SUBTYPE_GOBLIN,		// HARDCODED_SUBTYPE_GOBLIN
  SUBTYPE_LIZARD,		// HARDCODED_SUBTYPE_LIZARD
  SUBTYPE_HYENA,		// HARDCODED_SUBTYPE_HYENA
  SUBTYPE_JELLYFISH,	// HARDCODED_SUBTYPE_JELLYFISH
  SUBTYPE_SOLDIER,		// HARDCODED_SUBTYPE_SOLDIER
  SUBTYPE_GORGON,		// HARDCODED_SUBTYPE_GORGON
  SUBTYPE_HAG,			// HARDCODED_SUBTYPE_HAG
  SUBTYPE_HUMAN,		// HARDCODED_SUBTYPE_HUMAN
  SUBTYPE_HORROR,		// HARDCODED_SUBTYPE_HORROR
  SUBTYPE_HORSE,		// HARDCODED_SUBTYPE_HORSE
  SUBTYPE_KAVU,			// HARDCODED_SUBTYPE_KAVU
  SUBTYPE_HYDRA,		// HARDCODED_SUBTYPE_HYDRA
  SUBTYPE_IMP,			// HARDCODED_SUBTYPE_IMP
  SUBTYPE_KIRIN,		// HARDCODED_SUBTYPE_KIRIN
  SUBTYPE_KOR,			// HARDCODED_SUBTYPE_KOR
  SUBTYPE_JUGGERNAUT,	// HARDCODED_SUBTYPE_JUGGERNAUT
  SUBTYPE_KRAKEN,		// HARDCODED_SUBTYPE_KRAKEN
  SUBTYPE_LAMMASU,		// HARDCODED_SUBTYPE_LAMMASU
  SUBTYPE_KITHKIN,		// HARDCODED_SUBTYPE_KITHKIN
  SUBTYPE_KNIGHT,		// HARDCODED_SUBTYPE_KNIGHT
  SUBTYPE_GOLEM,		// HARDCODED_SUBTYPE_GOLEM
  SUBTYPE_KOBOLD,		// HARDCODED_SUBTYPE_KOBOLD
  SUBTYPE_AURA_LAND,	// HARDCODED_SUBTYPE_LAND
  SUBTYPE_LEECH,		// HARDCODED_SUBTYPE_LEECH
  0,					// HARDCODED_SUBTYPE_UNUSED
  SUBTYPE_MOUNTAIN,		// HARDCODED_SUBTYPE_MOUNTAIN
  SUBTYPE_LICID,		// HARDCODED_SUBTYPE_LICID
  SUBTYPE_METATHRAN,	// HARDCODED_SUBTYPE_METATHRAN
  SUBTYPE_LEVIATHAN,	// HARDCODED_SUBTYPE_LEVIATHAN
  SUBTYPE_MONGER,		// HARDCODED_SUBTYPE_MONGER
  SUBTYPE_MONGOOSE,		// HARDCODED_SUBTYPE_MONGOOSE
  SUBTYPE_DESERT,		// HARDCODED_SUBTYPE_DESERT
  SUBTYPE_MOONFOLK,		// HARDCODED_SUBTYPE_MOONFOLK
  SUBTYPE_MUTANT,		// HARDCODED_SUBTYPE_MUTANT
  SUBTYPE_MYSTIC,		// HARDCODED_SUBTYPE_MYSTIC
  SUBTYPE_BIRD,			// HARDCODED_SUBTYPE_BIRD
  SUBTYPE_MANTICORE,	// HARDCODED_SUBTYPE_MANTICORE
  SUBTYPE_NAUTILUS,		// HARDCODED_SUBTYPE_NAUTILUS
  SUBTYPE_NEPHILIM,		// HARDCODED_SUBTYPE_NEPHILIM
  SUBTYPE_MERFOLK,		// HARDCODED_SUBTYPE_MERFOLK
  SUBTYPE_MINOTAUR,		// HARDCODED_SUBTYPE_MINOTAUR
  SUBTYPE_NINJA,		// HARDCODED_SUBTYPE_NINJA
  SUBTYPE_MASTICORE,	// HARDCODED_SUBTYPE_MASTICORE
  SUBTYPE_OCTOPUS,		// HARDCODED_SUBTYPE_OCTOPUS
  SUBTYPE_MYR,			// HARDCODED_SUBTYPE_MYR
  SUBTYPE_ORB,			// HARDCODED_SUBTYPE_ORB
  SUBTYPE_ORGG,			// HARDCODED_SUBTYPE_ORGG
  SUBTYPE_OYSTER,		// HARDCODED_SUBTYPE_OYSTER
  SUBTYPE_OUPHE,		// HARDCODED_SUBTYPE_OUPHE
  SUBTYPE_NIGHTMARE,	// HARDCODED_SUBTYPE_NIGHTMARE
  SUBTYPE_NOMAD,		// HARDCODED_SUBTYPE_NOMAD
  SUBTYPE_ISLAND,		// HARDCODED_SUBTYPE_ISLAND
  SUBTYPE_OGRE,			// HARDCODED_SUBTYPE_OGRE
  SUBTYPE_PENTAVITE,	// HARDCODED_SUBTYPE_PENTAVITE
  SUBTYPE_OOZE,			// HARDCODED_SUBTYPE_OOZE
  SUBTYPE_ORC,			// HARDCODED_SUBTYPE_ORC
  SUBTYPE_FOREST,		// HARDCODED_SUBTYPE_FOREST
  SUBTYPE_PEGASUS,		// HARDCODED_SUBTYPE_PEGASUS
  SUBTYPE_SWAMP,		// HARDCODED_SUBTYPE_SWAMP
  SUBTYPE_PHELDDAGRIF,	// HARDCODED_SUBTYPE_PHELDDAGRIF
  SUBTYPE_PHOENIX,		// HARDCODED_SUBTYPE_PHOENIX
  SUBTYPE_PINCHER,		// HARDCODED_SUBTYPE_PINCHER
  SUBTYPE_PLAINS,		// HARDCODED_SUBTYPE_PLAINS
  SUBTYPE_PRISM,		// HARDCODED_SUBTYPE_PRISM
  SUBTYPE_RABBIT,		// HARDCODED_SUBTYPE_RABBIT
  SUBTYPE_REFLECTION,	// HARDCODED_SUBTYPE_REFLECTION
  SUBTYPE_BARBARIAN,	// HARDCODED_SUBTYPE_BARBARIAN
  SUBTYPE_RHINO,		// HARDCODED_SUBTYPE_RHINO
  SUBTYPE_RAT,			// HARDCODED_SUBTYPE_RAT
  SUBTYPE_CAT,			// HARDCODED_SUBTYPE_CAT
  SUBTYPE_ROGUE,		// HARDCODED_SUBTYPE_ROGUE
  SUBTYPE_RIGGER,		// HARDCODED_SUBTYPE_RIGGER
  SUBTYPE_SALAMANDER,	// HARDCODED_SUBTYPE_SALAMANDER
  SUBTYPE_SCOUT,		// HARDCODED_SUBTYPE_SCOUT
  SUBTYPE_SATYR,		// HARDCODED_SUBTYPE_SATYR
  SUBTYPE_SCARECROW,	// HARDCODED_SUBTYPE_SCARECROW
  SUBTYPE_SCORPION,		// HARDCODED_SUBTYPE_SCORPION
  SUBTYPE_SERPENT,		// HARDCODED_SUBTYPE_SERPENT
  SUBTYPE_SHADE,		// HARDCODED_SUBTYPE_SHADE
  SUBTYPE_AURA_PERMANENT,	// HARDCODED_SUBTYPE_PERMANENT
  SUBTYPE_FISH,			// HARDCODED_SUBTYPE_FISH
  SUBTYPE_PIRATE,		// HARDCODED_SUBTYPE_PIRATE
  SUBTYPE_SAMURAI,		// HARDCODED_SUBTYPE_SAMURAI
  SUBTYPE_SAPROLING,	// HARDCODED_SUBTYPE_SAPROLING
  SUBTYPE_SERF,			// HARDCODED_SUBTYPE_SERF
  SUBTYPE_SKELETON,		// HARDCODED_SUBTYPE_SKELETON
  SUBTYPE_SLUG,			// HARDCODED_SUBTYPE_SLUG
  SUBTYPE_SHEEP,		// HARDCODED_SUBTYPE_SHEEP
  SUBTYPE_SLITH,		// HARDCODED_SUBTYPE_SLITH
  SUBTYPE_SPAWN,		// HARDCODED_SUBTYPE_SPAWN
  SUBTYPE_SPECTER,		// HARDCODED_SUBTYPE_SPECTER
  SUBTYPE_SPHINX,		// HARDCODED_SUBTYPE_SPHINX
  SUBTYPE_SPIDER,		// HARDCODED_SUBTYPE_SPIDER
  SUBTYPE_SPIRIT,		// HARDCODED_SUBTYPE_SPIRIT
  SUBTYPE_SOLTARI,		// HARDCODED_SUBTYPE_SOLTARI
  SUBTYPE_SPELLSHAPER,	// HARDCODED_SUBTYPE_SPELLSHAPER
  SUBTYPE_GRIFFIN,		// HARDCODED_SUBTYPE_GRIFFIN
  SUBTYPE_SPIKE,		// HARDCODED_SUBTYPE_SPIKE
  SUBTYPE_TURTLE,		// HARDCODED_SUBTYPE_TURTLE
  SUBTYPE_THOPTER,		// HARDCODED_SUBTYPE_THOPTER
  SUBTYPE_TREEFOLK,		// HARDCODED_SUBTYPE_TREEFOLK
  SUBTYPE_TROLL,		// HARDCODED_SUBTYPE_TROLL
  SUBTYPE_BASIC,		// HARDCODED_SUBTYPE_BASIC_LAND
  SUBTYPE_WEREWOLF,		// HARDCODED_SUBTYPE_WEREWOLF
  SUBTYPE_UNICORN,		// HARDCODED_SUBTYPE_UNICORN
  SUBTYPE_VAMPIRE,		// HARDCODED_SUBTYPE_VAMPIRE
  SUBTYPE_SPONGE,		// HARDCODED_SUBTYPE_SPONGE
  SUBTYPE_SQUID,		// HARDCODED_SUBTYPE_SQUID
  SUBTYPE_SQUIRREL,		// HARDCODED_SUBTYPE_SQUIRREL
  SUBTYPE_STARFISH,		// HARDCODED_SUBTYPE_STARFISH
  SUBTYPE_WARRIOR,		// HARDCODED_SUBTYPE_WARRIOR
  SUBTYPE_WALL,			// HARDCODED_SUBTYPE_WALL
  SUBTYPE_PRAETOR,		// HARDCODED_SUBTYPE_PRAETOR
  SUBTYPE_THALAKOS,		// HARDCODED_SUBTYPE_THALAKOS
  SUBTYPE_THRULL,		// HARDCODED_SUBTYPE_THRULL
  SUBTYPE_WIZARD,		// HARDCODED_SUBTYPE_WIZARD
  SUBTYPE_WOLVERINE,	// HARDCODED_SUBTYPE_WOLVERINE
  SUBTYPE_WOLF,			// HARDCODED_SUBTYPE_WOLF
  SUBTYPE_WOMBAT,		// HARDCODED_SUBTYPE_WOMBAT
  SUBTYPE_WORLD,		// HARDCODED_SUBTYPE_WORLD
  SUBTYPE_WRAITH,		// HARDCODED_SUBTYPE_WRAITH
  SUBTYPE_TRISKELAVITE,	// HARDCODED_SUBTYPE_TRISKELAVITE
  SUBTYPE_WURM,			// HARDCODED_SUBTYPE_WURM
  SUBTYPE_YETI,			// HARDCODED_SUBTYPE_YETI
  SUBTYPE_ZOMBIE,		// HARDCODED_SUBTYPE_ZOMBIE
  0,					// HARDCODED_SUBTYPE_NONE	// deliberately not SUBTYPE_NONE
  SUBTYPE_CROCODILE,	// HARDCODED_SUBTYPE_CROCODILE
  SUBTYPE_ILLUSION,		// HARDCODED_SUBTYPE_ILLUSION
  SUBTYPE_INSECT,		// HARDCODED_SUBTYPE_INSECT
  SUBTYPE_VEDALKEN,		// HARDCODED_SUBTYPE_VEDALKEN
  SUBTYPE_VOLVER,		// HARDCODED_SUBTYPE_VOLVER
  SUBTYPE_WEIRD,		// HARDCODED_SUBTYPE_WEIRD
  SUBTYPE_SNAKE,		// HARDCODED_SUBTYPE_SNAKE
  SUBTYPE_SAND,			// HARDCODED_SUBTYPE_SAND
  SUBTYPE_TETRAVITE,	// HARDCODED_SUBTYPE_TETRAVITE
  SUBTYPE_WHALE,		// HARDCODED_SUBTYPE_WHALE
  SUBTYPE_WORM,			// HARDCODED_SUBTYPE_WORM
  SUBTYPE_ZUBERA,		// HARDCODED_SUBTYPE_ZUBERA
  SUBTYPE_ASSEMBLY_WORKER,	// HARDCODED_SUBTYPE_ASSEMBLY_WORKER
  SUBTYPE_DOG,		// HARDCODED_SUBTYPE_HOUND
  SUBTYPE_LHURGOYF,		// HARDCODED_SUBTYPE_LHURGOYF
  SUBTYPE_TRIBAL,		// HARDCODED_SUBTYPE_TRIBAL
  SUBTYPE_SHRINE,		// HARDCODED_SUBTYPE_SHRINE
  SUBTYPE_CARRIER,		// HARDCODED_SUBTYPE_CARRIER
  SUBTYPE_MERCENARY,	// HARDCODED_SUBTYPE_MERCENARY
  SUBTYPE_REBEL,		// HARDCODED_SUBTYPE_REBEL
  SUBTYPE_VIASHINO,		// HARDCODED_SUBTYPE_VIASHINO
};
STATIC_ASSERT(sizeof(arr_from_hardcoded_subtype_to_subtype)/sizeof(subtype_t) == HARDCODED_SUBTYPE_VIASHINO + 1,
			  indices_in__arr_from_hardcoded_subtype_to_subtype__are_mismatched);

/* Only needed for engine.c:effect_asterisk(), field ASTERISK_SUBTYPE_OF_INFO_SLOT.  I'm reasonably certain that only gets called from card_swarm_of_rats() at
 * 0x4140f0, for HARDCODED_SUBTYPE_RAT; that happens to have the same value as SUBTYPE_RAT.  But no reason to risk it. */
subtype_t from_hardcodedsubtype_to_subtype(hardcoded_subtype_t hcs)
{
  if (hcs <= HARDCODED_SUBTYPE_VIASHINO)
	return arr_from_hardcoded_subtype_to_subtype[hcs];
  else
	return hcs;
}

/*
static int is_basic_land_subtype(int subtype){
	if( subtype == SUBTYPE_MOUNTAIN ||
		subtype == SUBTYPE_ISLAND ||
		subtype == SUBTYPE_PLAINS ||
		subtype == SUBTYPE_SWAMP ||
		subtype == SUBTYPE_FOREST
	  ){
		return 1;
	}
	return 0;
}
*/

int is_creature_type(int new_type){
	return new_type >= SUBTYPE_MIN_CREATURE_SUBTYPE && new_type <= SUBTYPE_MAX_CREATURE_SUBTYPE;
}

static int is_type_type(int new_type){
	return new_type >= SUBTYPE_MIN_TYPE && new_type <= SUBTYPE_MAX_TYPE;
}

static int is_supertype_type(int new_type){
	return new_type >= SUBTYPE_MIN_SUPERTYPE && new_type <= SUBTYPE_MAX_SUPERTYPE;
}

static int has_new_types(int id, int subtype){

	if( subtype == -1 ){
		return 0;
	}

	card_ptr_t* c = cards_ptr[id];

	if( c->types[0] == subtype ||
		c->types[1] == subtype ||
		c->types[2] == subtype ||
		c->types[3] == subtype ||
		c->types[4] == subtype ||
		c->types[5] == subtype
	){
		return 1;
	}

	return 0;
}

static int is_changeling(int player, int card){
	card_instance_t* instance;
	if( player != -1 && (instance = in_play(player, card)) ){
		uint32_t stash_keywords = instance->regen_status;
		int cur_keywords = get_abilities(player, card, EVENT_ABILITIES, -1);
		instance->regen_status = stash_keywords;
		if (cur_keywords & KEYWORD_PROT_INTERRUPTS ){
			return 1;
		}
	}
	else{
		int iid = card;
		if( player > -1 && in_hand(player, card) ){
			iid = get_original_internal_card_id(player, card);
		}
		if( cards_data[iid].static_ability & KEYWORD_PROT_INTERRUPTS ){
			return 1;
		}
	}
	return 0;
}

enum{
	ADD_SUBTYPE = 1,
	FORCE_SUBTYPE = 2,
};

static void add_force_subtype_impl(int player, int card, subtype_t subtype, int mode){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[15].player == -1 ){
		instance->targets[15].player = 0;
	}
	if( mode == ADD_SUBTYPE ){
		instance->targets[15].player &= ~0x0000FFFF;	// removes any previously-added subtype, but leaves any forced one in place
		instance->targets[15].player |= subtype & 0xFFFF;
	}
	if( mode == FORCE_SUBTYPE ){
		instance->targets[15].player = 0;	// removes any previously-added subtype
		instance->targets[15].player |= ((unsigned)(subtype & 0xFFFF) << 16);
	}
}

void force_a_subtype(int player, int card, subtype_t subtype)
{
  card_instance_t *instance = get_card_instance(player, card);
  update_alter_remove_creature_subtypes(instance);
  update_alter_add_subtype(subtype, instance);
}

void add_a_subtype(int player, int card, subtype_t subtype)
{
  update_alter_add_subtype(subtype, get_card_instance(player, card));
}

int get_added_subtype(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if (instance->targets[15].player == -1){
		return 0;
	} else {
		return LOWORD(instance->targets[15].player);
	}
}

int force_a_subtype_until_eot(int player, int card, int t_player, int t_card, subtype_t subtype)
{
  int legacy = create_targetted_legacy_effect(player, card, add_force_a_subtype_until_eot_legacy,
                                              t_player, t_card);
  get_card_instance(player, legacy)->targets[1].player = 2;
  get_card_instance(player, legacy)->targets[1].card = subtype;
  return legacy;
}

static int get_forced_subtype(int player, int card){
	card_instance_t *instance = get_card_instance(player, card);
	if (instance->targets[15].player == -1){
		return 0;
	} else {
		return HIWORD(instance->targets[15].player);
	}
}

int has_forced_subtype(int player, int card){
	return get_forced_subtype(player, card) > 0;
}

int has_added_subtype(int player, int card){
	return get_added_subtype(player, card) > 0;
}

void get_subtypes(int player, int card, reported_subtypes_t *my_subtypes)
{
  if (player == -1 || card == -1)
  {
    backtrace_and_abort("functions/has_creature_type.c:683\nplayer != -1 && card != -1");
    return;
  }
  card_instance_t *instance = get_card_instance(player, card);
  memset(my_subtypes, 0, sizeof(*my_subtypes));
  card_ext_t *extension = instance->ext;
  for (int i = 0; i < 12; ++i)
    my_subtypes->reported_subtypes[i] = extension->subtype[i];

  int count = 12;
  if (check_special_flags3(player, card, SF3_EMBALMED_OR_ETERNALIZED))
    my_subtypes->reported_subtypes[count++] = SUBTYPE_ZOMBIE;
  if (is_what(player, card, TYPE_CREATURE) && (in_hand(player, card) || on_stack(player, card)))
  {
    // The release scans only slots 0..14 of each 16-record row.
    for (int index = 0; index < 15; ++index)
    {
      const subtypes_not_in_play_t *modifier = &added_subtypes_not_in_play[player][index];
      unsigned int subtype = modifier->subtype;
      if (subtype - 1U > 0xffeU)
        return;
      if (modifier->added_forced == 2)
      {
        memset(my_subtypes, 0, sizeof(*my_subtypes));
        count = 0;
      }
      my_subtypes->reported_subtypes[count++] = subtype;
    }
  }
}

int has_subtype(int player, int card, subtype_t subtype)
{
  if (subtype == SUBTYPE_INVALID)
    return 1;
  if (subtype == SUBTYPE_IMPOSSIBLE)
    return 0;
  if ((unsigned int)player >= 2 || (unsigned int)card >= 0x97 ||
      get_card_instance(player, card) == NULL)
    backtrace_and_abort("functions/has_creature_type.c:780\nplayer >= 0 && card >= 0 && player <=1 "
                        "&& card <= 150 && get_card_instance(player, card) != NULL");
  if ((unsigned int)(subtype - SUBTYPE_ALLY) < 0xfffU)
  {
    if (!is_what(player, card, TYPE_CREATURE) && !has_subtype(player, card, SUBTYPE_TRIBAL))
      return 0;
    if (is_changeling(player, card))
      return 1;
  }
  reported_subtypes_t stypes;
  get_subtypes(player, card, &stypes);
  for (int i = 0; i < 26; ++i)
    if (stypes.reported_subtypes[i] == subtype)
      return 1;
  card_instance_t *instance = get_card_instance(player, card);
  int old_event_result = event_result;
  int result = 0;
  static int depth;
  if (depth + 1 < 10 && instance->internal_card_id >= 0)
  {
    event_result = subtype;
    ++depth;
    call_card_function_i(instance, player, card, EVENT_SUBTYPE);
    --depth;
    result = event_result == 0;
  }
  event_result = old_event_result;
  return result;
}

int has_creature_type(int player, int card, subtype_t subtype){

  if( ! is_what(player, card, TYPE_CREATURE) ){
	 return 0;
  }

  return has_subtype(player, card, subtype);
}

int is_tribal(int player, int card){
	return has_subtype(player, card, SUBTYPE_TRIBAL);
}

// Returns true if {p0,c0} shares at least one creature subtype with {p1,c1}.
int shares_creature_subtype(int p0, int c0, int p1, int c1)
{
  int p[2] = {p0, p1};
  int c[2] = {c0, c1};
  for (int i = 0; i < 2; ++i)
    if (!is_what(p[i], c[i], TYPE_CREATURE) && !is_tribal(p[i], c[i]))
      return 0;
  int subtypes[2][26] = {{0}};
  int changeling[2] = {0};
  reported_subtypes_t stypes;
  for (int i = 0; i < 2; ++i)
  {
    if (is_changeling(p[i], c[i]))
      changeling[i] = 1;
    else
    {
      get_subtypes(p[i], c[i], &stypes);
      for (int j = 0; j < 26; ++j)
        subtypes[i][j] = stypes.reported_subtypes[j];
    }
  }
  if (changeling[0])
  {
    if (changeling[1])
      return 1;
    for (int j = 0; j < 26; ++j)
      if (is_creature_type(subtypes[1][j]))
        return 1;
  }
  else if (changeling[1])
  {
    for (int j = 0; j < 26; ++j)
      if (is_creature_type(subtypes[0][j]))
        return 1;
  }
  else
    for (int i = 0; i < 26; ++i)
      if (is_creature_type(subtypes[0][i]))
        for (int j = 0; j < 26; ++j)
          if (subtypes[0][i] == subtypes[1][j])
            return 1;
  return 0;
}

// Finds creature subtypes only (by design)
static int get_most_common_subtype_in_zone(const int *zone){
	if( zone == NULL ){
		return -1;
	}
	int subs2[SUBTYPE_MAX_USED_CREATURE_SUBTYPE + 1] = {0};
	int i, count;
	for (count = 0; zone[count] != -1; ++count){
			if( is_what(-1, zone[count], TYPE_CREATURE) ){
				int id = cards_data[zone[count]].id;
				if( !is_changeling(-1, zone[count]) ){
					card_ptr_t* c = cards_ptr[id];
					for (i = 1; i < 6; ++i){	// [0] is always either a type or supertype; [6] is unused
						// deliberately omit SUBTYPE_NONE and SUBTYPE_ALL_CREATURES
						if (c->types[i] >= SUBTYPE_MIN_CREATURE_SUBTYPE && c->types[i] <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE){
							subs2[c->types[i]]++;
						}
					}
				}
			}
	}
	int par = 0, mcsid = -1;
	for (i = 0; i <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE; i++){
		if( subs2[i] > par ){
			par = subs2[i];
			mcsid = i;
		}
	}
	return mcsid;
}

/*
static int create_select_subtype_legacy(int player, int card, subtype_t subtype, int show_legacy)
{
	int legacy = -1;
	if( show_legacy && subtype > 0 && subtype != SUBTYPE_NONE ){
		if( is_what(player, card, TYPE_PERMANENT) && show_legacy ){
			legacy = create_subtype_name_legacy(player, card, subtype);
			if( show_legacy == 2 ){
				get_card_instance(player, legacy)->targets[0].card = 2;
			}
		}
	}
	return legacy;
}
*/

static int select_a_subtype_impl(int player, int card, int who_chooses, int t_player,
                                 const int *zone, int show_legacy)
{
  // Actually, is "select a creature type"

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.illegal_abilities = 0;
  td.allow_cancel = player == HUMAN;
  td.who_chooses = who_chooses;
  td.preferred_controller = t_player;
  td.zone = TARGET_ZONE_IN_PLAY | TARGET_ZONE_HAND;

  card_instance_t *instance = get_card_instance(player, card);

  enum
  {
    CHOICE_AUTO = 1,
    CHOICE_CARD_LIST,
    CHOICE_PICK_CREATURE,
    CHOICE_UNPRINTED
  };

restart:;
  reported_subtypes_t stypes;
  int result = -1;
  int csvid = -1;
  int subs[NUM_SUBTYPES] = {0};
  int choice =
      DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, "Auto", 1,
             5, "Choose from a card list", 1, 1, "Pick it from a creature in play", can_target(&td),
             10, "Token-only subtypes", 1, 1);

  if (choice == CHOICE_AUTO)
  {
    if (zone == NULL)
    {
      zone = deck_ptr[t_player];
      result = get_most_common_subtype_in_zone(zone);
      if (result < 1)
      {
        zone = get_grave(t_player);
        result = get_most_common_subtype_in_zone(zone);
      }
      if (result < 1)
      {
        zone = rfg_ptr[t_player];
        result = get_most_common_subtype_in_zone(zone);
      }
      if (result == -1)
      {
        result = internal_rand(SUBTYPE_MAX_USED_CREATURE_SUBTYPE - SUBTYPE_MIN_CREATURE_SUBTYPE) +
                 SUBTYPE_MIN_CREATURE_SUBTYPE;
      }
    }
    else
    {
      result = get_most_common_subtype_in_zone(zone);
    }
  }

  if (choice == CHOICE_CARD_LIST)
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_CREATURE, "Choose a creature card name.");
    int iid = new_card_from_list(player, 5, &test);
    if (is_changeling(-1, iid))
    {
      goto restart;
    }
    csvid = cards_data[iid].id;
  }

  if (choice == CHOICE_PICK_CREATURE)
  {
    int prev_num_targs = instance->number_of_targets;
    target_t prev_targ0 = instance->targets[0];
    instance->number_of_targets = 0;
    if (!pick_target(&td, "TARGET_CREATURE"))
    {
      goto restart;
    }
    if (is_changeling(instance->targets[0].player, instance->targets[0].card))
    {
      goto restart;
    }
    get_subtypes(instance->targets[0].player, instance->targets[0].card, &stypes);
    int i;
    int sc = 0;
    for (i = 0; i < 14; ++i)
    {
      if (is_creature_type(stypes.reported_subtypes[i]))
      {
        subs[sc] = stypes.reported_subtypes[i];
        sc++;
      }
    }
    instance->number_of_targets = prev_num_targs;
    instance->targets[0] = prev_targ0;
  }

  if (choice == CHOICE_UNPRINTED)
  {
    int token_choice = DIALOG(
        player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, "Army", 1, 1,
        "Blinkmoth", 1, 1, "Camarid", 1, 1, "Caribou", 1, 1, "Coward", 1, 1, "Deserter", 1, 1,
        "Germ", 1, 1, "Graveborn", 1, 1, "Hamster", 1, 1, "Inkling", 1, 1, "Pentavite", 1, 1,
        "Pest", 1, 1, "Pincher", 1, 1, "Prism", 1, 1, "Reflection", 1, 1, "Continue >>", 1, 1);
    switch (token_choice)
    {
    case 1:
      result = SUBTYPE_ARMY;
      break;
    case 2:
      result = SUBTYPE_BLINKMOTH;
      break;
    case 3:
      result = SUBTYPE_CAMARID;
      break;
    case 4:
      result = SUBTYPE_CARIBOU;
      break;
    case 5:
      result = SUBTYPE_COWARD;
      break;
    case 6:
      result = SUBTYPE_DESERTER;
      break;
    case 7:
      result = SUBTYPE_GERM;
      break;
    case 8:
      result = SUBTYPE_GRAVEBORN;
      break;
    case 9:
      result = SUBTYPE_HAMSTER;
      break;
    case 10:
      result = SUBTYPE_INKLING;
      break;
    case 11:
      result = SUBTYPE_PENTAVITE;
      break;
    case 12:
      result = SUBTYPE_PEST;
      break;
    case 13:
      result = SUBTYPE_PINCHER;
      break;
    case 14:
      result = SUBTYPE_PRISM;
      break;
    case 15:
      result = SUBTYPE_REFLECTION;
      break;
    case 16:
    {
      static const subtype_t more_types[9] = {
          SUBTYPE_SAND,     SUBTYPE_SAPROLING, SUBTYPE_SCULPTURE,
          SUBTYPE_SERF,     SUBTYPE_SERVO,     SUBTYPE_SPLINTER,
          SUBTYPE_TENTACLE, SUBTYPE_TETRAVITE, SUBTYPE_TRISKELAVITE};
      token_choice =
          DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_RANDOM, DLG_NO_CANCEL, "Sand",
                 1, 1, "Saproling", 1, 3, "Sculpture", 1, 1, "Serf", 1, 1, "Servo", 1, 1,
                 "Splinter", 1, 1, "Tentacle", 1, 1, "Tetravite", 1, 1, "Triskelavite", 1, 1);
      if ((unsigned int)(token_choice - 1) >= 9)
        goto restart;
      result = more_types[token_choice - 1];
      break;
    }
    default:
      goto restart;
    }
  }

  if (choice != CHOICE_AUTO && choice != CHOICE_UNPRINTED)
  {
    int sc = 0;
    if (csvid < 0)
    {
      csvid = CARD_ID_AIR_ELEMENTAL;
    }
    if (choice == CHOICE_CARD_LIST)
    {
      get_subtypes_by_csvid(-1, csvid, &stypes);
      int i;
      for (i = 0; i < NUM_SUBTYPES; ++i)
      {
        if (is_creature_type(stypes.reported_subtypes[i]))
        {
          subs[sc] = stypes.reported_subtypes[i];
          sc++;
        }
      }
    }

#define OPTION(x)                                                                                  \
  (subs[x] > 0 && subs[x] <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE) ? raw_get_subtype_text(subs[x])    \
                                                                : "",                              \
      (subs[x] > 0 && subs[x] <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE), 1

    int choice2 = DIALOG(player, card, EVENT_ACTIVATE, DLG_WHO_CHOOSES(who_chooses), DLG_NO_STORAGE,
                         DLG_OMIT_ILLEGAL, DLG_NO_CANCEL, OPTION(0), OPTION(1), OPTION(2),
                         OPTION(3), OPTION(4), OPTION(5), OPTION(6), OPTION(7), OPTION(8),
                         OPTION(9), OPTION(10), OPTION(11), OPTION(12), OPTION(13));
#undef OPTION

    if (choice2 == 0)
    {
      goto restart;
    }
    else
    {
      result = subs[choice2 - 1];
    }
  }

  if (result > 0 && result != SUBTYPE_NONE)
  {
    if (is_what(player, card, TYPE_PERMANENT) && show_legacy)
    {
      int legacy = create_subtype_name_legacy(player, card, result);
      if (show_legacy == 2)
      {
        get_card_instance(player, legacy)->targets[0].card = 2;
      }
    }
  }

  return result;
}

int select_a_subtype(int player, int card){

	int t_player = player;
	if( get_id(player, card) == CARD_ID_ENGINEERED_PLAGUE || get_id(player, card) == CARD_ID_TSABOS_DECREE || get_id(player, card) == CARD_ID_AN_ZERRIN_RUINS){
		if( player == AI ){
			t_player = 1-player;
		}
	}

	return select_a_subtype_impl(player, card, player, t_player, NULL, 1);
}

int select_a_subtype_full_choice(int player, int card, int t_player, const int *zone, int show_legacy){
	return select_a_subtype_impl(player, card, t_player, t_player, zone, show_legacy);
}

int target_player_sacrifices_a_subtype(int player, int card, int req_type, int req_subtype, int cannot_cancel){
	return sacrifice(player, card, player, cannot_cancel, req_type, 0, req_subtype, 0, 0, 0, 0, 0, -1, 0);
}

int count_untapped_nonsick_subtype(int what_player, int type, int subtype){
  // what_player = 2 -> count the permanents of both players
  // subtype = -1 -> count any nontapped, nonsick permanent of chosen type

  int result = 0;

  if( what_player == 2 ){
	 int i;
	 for(i = 0; i < 2; i++){
		 int count = active_cards_count[i]-1;
		 while( count > -1 ){
			   if( in_play(i, count) && is_what(i, count, type) &&  ! is_tapped(i, count) && ! is_animated_and_sick(i, count) ){
				   if( subtype > 0 ){
					   if( has_subtype(i, count, subtype) ){
						   result++;
					   }
				   }
				   else{
						result++;
				   }

			   }
			   count--;
		 }
	 }
  }

  else if ( what_player == 1 || what_player == 0){
		   int count = active_cards_count[what_player]-1;
		   while( count > -1 ){
				 if( in_play(what_player, count) && is_what(what_player, count, type) &&  ! is_tapped(what_player, count) &&
					 ! is_animated_and_sick(what_player, count)
				  ){
					if( subtype > 0 ){
						if( has_subtype(what_player, count, subtype) ){
							result++;
						}
					}
					else{
						 result++;
					}
				 }
				 count--;
		   }
  }

  return result;
}

int check_for_untapped_nonsick_subtype(int what_player, int type, int subtype){
  // what_player = 2 -> check the permanents of both players
  // subtype = -1 -> check if there's a nontapped, nonsick permanent of chosen type


  if( what_player == 2 ){
	 int i;
	 for(i = 0; i < 2; i++){
		 int count = active_cards_count[i]-1;
		 while( count > -1 ){
			   if( in_play(i, count) && is_what(i, count, type) &&  ! is_tapped(i, count) && ! is_animated_and_sick(i, count) ){
				   if( subtype > 0 ){
					   if( has_subtype(i, count, subtype) ){
						   return 1;
					   }
				   }
				   else{
						return 1;
				   }

			   }
			   count--;
		 }
	 }
  }

  else if ( what_player == 1 || what_player == 0){
		   int count = active_cards_count[what_player]-1;
		   while( count > -1 ){
				 if( in_play(what_player, count) && is_what(what_player, count, type) &&  ! is_tapped(what_player, count) &&
					 ! is_animated_and_sick(what_player, count)
				  ){
					if( subtype > 0 ){
						if( has_subtype(what_player, count, subtype) ){
							return 1;
						}
					}
					else{
						 return 1;
					}
				 }
				 count--;
		   }
  }

  return 0;
}

int count_untapped_subtype(int what_player, int type, int subtype){
  // what_player = 2 -> count the permanents of both players
  // subtype = -1 -> count any nontapped permanent of chosen type

  int result = 0;

  if( what_player == 2 ){
	 int i;
	 for(i = 0; i < 2; i++){
		 int count = active_cards_count[i]-1;
		 while( count > -1 ){
			   if( in_play(i, count) && is_what(i, count, type) && ! is_tapped(i, count)
				   && (subtype <= 0 || has_subtype(i, count, subtype)) ){
				   result++;
			   }
			   count--;
		 }
	 }
  }

  else if ( what_player == 1 || what_player == 0){
		   int count = active_cards_count[what_player]-1;
		   while( count > -1 ){
				 if( in_play(what_player, count) && is_what(what_player, count, type) && ! is_tapped(what_player, count)
					 && (subtype <= 0 || has_subtype(what_player, count, subtype)) ){
					 result++;
				 }
				 count--;
		   }
  }

  return result;
}

int is_subtype_in_hand(int player, int subtype){

  int i;

  for(i=0; i < active_cards_count[player]; i++){
	  if( in_hand(player, i) && has_subtype(player, i, subtype) ){
		 return 1;
	  }
  }

  return 0;
}

int is_subtype_dead(int player, int subtype1, int subtype2, int mode){
		int p;
		for(p=0;p<2;p++){
			if( p == player || player == 2 ){
				int c=0;
				for(c=0;c< active_cards_count[p];c++){
					card_data_t* card_d = get_card_data(p, c);
					if((card_d->type & TYPE_PERMANENT) && in_play(p, c) ){
						int good = 0;
						if( subtype1 > 0 && has_subtype(p, c, subtype1) ){
							good = 1 ;
						}
						if( subtype2 > 0 && has_subtype(p, c, subtype2) ){
							good = 1 ;
						}
						if( subtype2 == -1 && subtype1 == -1 ){
							good = 1 ;
						}
						if( good == 1 ){
							card_instance_t *creature = get_card_instance(p, c);
							if( creature->kill_code > 0 ){
								if( mode == 0 && creature->kill_code == KILL_DESTROY ){
									return 0x63;
								}
								if( mode == 1 && creature->kill_code < KILL_SACRIFICE ){
									return 0x63;
								}
							}
						}
					}
				}
			}
		}
		return 0;
}

int subtype_deals_damage(int player, int card, event_t event, int controller, int subtype, int mode)
{
  // subtype = -1 --> Any creature

  card_instance_t *instance = get_card_instance(player, card);

  ASSERT(!(mode & DDBM_REPORT_DAMAGE_DEALT) && "unimplemented for subtype_deals_damage");
  ASSERT(!(mode & DDBM_TRACE_DAMAGED_PLAYERS) && "unimplemented for subtype_deals_damage");
  ASSERT(!(mode & DDBM_STORE_IN_TARGETS_9) && "unimplemented for subtype_deals_damage");

  if (event == EVENT_DEAL_DAMAGE)
  {
    card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card &&
        (subtype == -1 || subtype == 0 ||
         (subtype != SUBTYPE_IMPOSSIBLE &&
          has_subtype(damage->damage_source_player, damage->damage_source_card, subtype))))
    {
      int good = 0;
      if (damage->damage_source_player == controller || controller == 2)
      {
        if (damage->info_slot > 0)
        {
          good = 1;
        }
        else
        {
          card_instance_t *trg =
              get_card_instance(damage->damage_source_player, damage->damage_source_card);
          if (trg->targets[16].player > 0)
          {
            good = 1;
          }
        }
      }

      if (mode &
          (DDBM_MUST_DAMAGE_PLAYER | DDBM_MAY_DAMAGE_PLANESWALKER | DDBM_MUST_DAMAGE_CREATURE))
      {
        if (damage_is_to_planeswalker(damage))
        {
          if (!(mode & DDBM_MAY_DAMAGE_PLANESWALKER))
          {
            good = 0;
          }
        }
        else if (damage->damage_target_card != -1)
        {
          if (!(mode & DDBM_MUST_DAMAGE_CREATURE))
          {
            good = 0;
          }
        }
        else
        {
          if (!(mode & DDBM_MUST_DAMAGE_PLAYER))
          {
            good = 0;
          }
        }
      }

      if ((mode & DDBM_MUST_DAMAGE_OPPONENT) &&
          (damage->damage_target_card != -1 || damage->damage_target_player != 1 - player ||
           damage_is_to_planeswalker(damage)))
      {
        good = 0;
      }
      if ((mode & DDBM_MUST_BE_COMBAT_DAMAGE) &&
          !(damage->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)))
      {
        good = 0;
      }
      if ((mode & DDBM_NOT_ME) && damage->damage_source_player == player &&
          damage->damage_source_card == card)
      {
        good = 0;
      }
      if (good == 1)
      {
        if (instance->targets[1].player < 2)
        {
          instance->targets[1].player = 2;
        }
        if (mode & DDBM_TRACE_DAMAGED_CREATURES)
        {
          int pos = instance->targets[1].player;
          if (pos < 10 && in_play(damage->damage_target_player, damage->damage_target_card))
          {
            instance->targets[pos].player = damage->damage_target_player;
            instance->targets[pos].card = damage->damage_target_card;
          }
        }
        instance->targets[1].player++;
      }
    }
  }

  if (instance->targets[1].player > 2 && trigger_condition == TRIGGER_DEAL_DAMAGE &&
      affect_me(player, card) && reason_for_trigger_controller == player)
  {
    if (event == EVENT_TRIGGER)
    {
      int trg = 2;
      if (mode & DDBM_TRIGGER_OPTIONAL)
      {
        trg = 1 + player;
      }
      event_result |= trg;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance->targets[1].card = instance->targets[1].player - 2;
      instance->targets[1].player = 2;
      return 1;
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[1].player = 2;
    }
  }

  return 0;
}

/* These all rely on SUBTYPE_MIN_* all being even multiples of 0x1000 (except SUBTYPE_MIN_USED_CREATURE_SUBTYPE), and SUBTYPE_MAX_USED_* all being
 * SUBTYPE_MIN_* + 299 or less.  (load_text() can't load more entries than that.) */
static const char* strs_creature_type[		(SUBTYPE_MAX_USED_CREATURE_SUBTYPE			& 0xFFF) + 1] = {0};
static char* strs_type_type[			(SUBTYPE_MAX_USED_TYPE						& 0xFFF) + 1] = {0};
static char* strs_supertype_type[		(SUBTYPE_MAX_USED_SUPERTYPE					& 0xFFF) + 1] = {0};
static char* strs_artifact_type[		(SUBTYPE_MAX_USED_ARTIFACT_SUBTYPE			& 0xFFF) + 1] = {0};
static char* strs_enchantment_type[		(SUBTYPE_MAX_USED_ENCHANTMENT_SUBTYPE		& 0xFFF) + 1] = {0};
static char* strs_land_type[			(SUBTYPE_MAX_USED_LAND_SUBTYPE				& 0xFFF) + 1] = {0};
static char* strs_instant_sorcery_type[	(SUBTYPE_MAX_USED_INSTANT_SORCERY_SUBTYPE	& 0xFFF) + 1] = {0};
static char* strs_planeswalker_type[	(SUBTYPE_MAX_USED_PLANESWALKER_SUBTYPE		& 0xFFF) + 1] = {0};
static char* strs_plane_type[			(SUBTYPE_MAX_USED_PLANE_SUBTYPE				& 0xFFF) + 1] = {0};

void init_subtype_text(void)
{
  if (strs_creature_type[0])
    return;

  ASSERT(ai_is_speculating != 1);
  int num_lines;
  const char **lines = load_text_nocopy("CREATURE_TYPE", &num_lines);
  ASSERT(num_lines >= (SUBTYPE_MAX_USED_CREATURE_SUBTYPE & 0xfff));
  int i;
  for (i = 0; i <= (SUBTYPE_MAX_USED_CREATURE_SUBTYPE & 0xfff); ++i)
    strs_creature_type[i] = lines[i];

  load_text(0, "TYPE_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_TYPE & 0xFFF); ++i)
    strs_type_type[i] = strdup(text_lines[i]);

  load_text(0, "SUPERTYPE_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_SUPERTYPE & 0xFFF); ++i)
    strs_supertype_type[i] = strdup(text_lines[i]);

  load_text(0, "ARTIFACT_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_ARTIFACT_SUBTYPE & 0xFFF); ++i)
    strs_artifact_type[i] = strdup(text_lines[i]);

  load_text(0, "ENCHANTMENT_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_ENCHANTMENT_SUBTYPE & 0xFFF); ++i)
    strs_enchantment_type[i] = strdup(text_lines[i]);

  load_text(0, "LAND_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_LAND_SUBTYPE & 0xFFF); ++i)
    strs_land_type[i] = strdup(text_lines[i]);

  load_text(0, "INSTANT_SORCERY_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_INSTANT_SORCERY_SUBTYPE & 0xFFF); ++i)
    strs_instant_sorcery_type[i] = strdup(text_lines[i]);

  load_text(0, "PLANESWALKER_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_PLANESWALKER_SUBTYPE & 0xFFF); ++i)
    strs_planeswalker_type[i] = strdup(text_lines[i]);

  load_text(0, "PLANE_TYPE");
  for (i = 0; i <= (SUBTYPE_MAX_USED_PLANE_SUBTYPE & 0xFFF); ++i)
    strs_plane_type[i] = strdup(text_lines[i]);
}

// Returns text corresponding to subtype.  Safely usable in display functions and targeting, since it doesn't call load_text().
const char* raw_get_subtype_text(subtype_t subtype)
{
  // Makes the same assumptions as init_subtype_text() above.

  switch (subtype & 0xF000)
	{
	  case 0:	// Creature subtypes - note SUBTYPE_MIN_CREATURE_SUBTYPE is 1, not 0, unlike the other MINs
		if (subtype <= SUBTYPE_MAX_USED_CREATURE_SUBTYPE)
		  return strs_creature_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_TYPE:
		if (subtype <= SUBTYPE_MAX_USED_TYPE)
		  return strs_type_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_SUPERTYPE:
		if (subtype <= SUBTYPE_MAX_USED_SUPERTYPE)
		  return strs_supertype_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_ARTIFACT_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_ARTIFACT_SUBTYPE)
		  return strs_artifact_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_ENCHANTMENT_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_ENCHANTMENT_SUBTYPE)
		  return strs_enchantment_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_LAND_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_LAND_SUBTYPE)
		  return strs_land_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_INSTANT_SORCERY_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_INSTANT_SORCERY_SUBTYPE)
		  return strs_instant_sorcery_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_PLANESWALKER_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_PLANESWALKER_SUBTYPE)
		  return strs_planeswalker_type[subtype & 0xFFF];
		break;

	  case SUBTYPE_MIN_PLANE_SUBTYPE:
		if (subtype <= SUBTYPE_MAX_USED_PLANE_SUBTYPE)
		  return strs_plane_type[subtype & 0xFFF];
		break;
	}

  return strs_creature_type[0];	// "None"
}

/* Any occurrence of %s or %a in fmt will be replaced by raw_get_subtype_text(subtype).  %a will also insert a "a " or "an " before the subtype's text as
 * appropriate.  Returns a pointer to a static buffer. */
const char* get_subtype_text(const char* fmt, subtype_t subtype)
{
  static char buf[500];
  if (ai_is_speculating == 1)
	{
	  *buf = 0;
	  return buf;
	}

  const char* raw_txt = NULL;
  int raw_len_minus_1 = 0;

  const char* p = fmt;
  char* q = buf;
  for (; (*q = *p); ++p, ++q)
	if (*p == '%' && (*(p + 1) == 's' || *(p + 1) == 'a'))
	  {
		if (!raw_txt)
		  {
			raw_txt = raw_get_subtype_text(subtype);
			raw_len_minus_1 = strlen(raw_txt) - 1;
		  }

		++p;
		if (*p == 'a')
		  {
			*q++ = 'a';

			if (strchr("aeiouAEIOU", raw_txt[0]))
			  *q++ = 'n';

			*q++ = ' ';
		  }

		strcpy(q, raw_txt);
		q += raw_len_minus_1;
	  }

  return buf;
}

static int fx_change_types(int player, int card, event_t event)
{
  /* Local data:
   * targets[4].player: keywords
   * targets[4].card: sp_keywords
   * targets[5].player: base power
   * targets[5].card: base toughness
   * targets[6].player: types to add
   * targets[6].card: types to remove (done before adding types)
   * targets[7].player: last-seen iid
   * targets[7].card: initial color, if non-zero
   * dummy3: iid to change type to
   */

  card_instance_t *inst = get_card_instance(player, card);
  card_instance_t *ench;

  if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card))
  {
    int was_attached_to_player = inst->damage_target_player,
        was_attached_to_card = inst->damage_target_card;
    inst->damage_target_player = inst->damage_target_card = -1;
    if (was_attached_to_card != -1)
    {
      get_card_instance(was_attached_to_player, was_attached_to_card)->regen_status |=
          KEYWORD_RECALC_CHANGE_TYPE;
      get_abilities(was_attached_to_player, was_attached_to_card, EVENT_CHANGE_TYPE, -1);
    }
    return 0;
  }

  if ((event == EVENT_EFFECT_CREATED && inst->targets[6].player != -1 // dispatched too soon?
       && (ench = get_card_instance(inst->damage_target_player, inst->damage_target_card))) ||
      (event == EVENT_CHANGE_TYPE &&
       !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS) && affected_card != -1 &&
       affect_me(inst->damage_target_player, inst->damage_target_card) &&
       (ench = in_play(affected_card_controller, affected_card))))
  {
    if ((int)inst->targets[9].card >= 0)
      update_alter_add_subtype(inst->targets[9].card, get_card_instance(inst->damage_target_player,
                                                                        inst->damage_target_card));
    int change_to, iid = (event == EVENT_CHANGE_TYPE    ? event_result
                          : ench->internal_card_id >= 0 ? ench->internal_card_id
                                                        : ench->backup_internal_card_id);

    type_t cur_typ = cards_data[iid].type;
    type_t new_typ = cur_typ;
    new_typ &= ~inst->targets[6].card;
    new_typ |= inst->targets[6].player;
    if (cur_typ == new_typ // already has the right types
        && (cards_data[iid].static_ability & inst->targets[4].player) ==
               (unsigned)inst->targets[4].player // has all keywords that would be granted
        && !inst->targets[5].player              // not granting sp_keywords
        && (!(new_typ & TYPE_CREATURE)           // either not turning into a creature,
            || (inst->targets[5].player == -1 &&
                inst->targets[5].card == -1) // or not setting power or toughness,
            || (cards_data[iid].power == inst->targets[5].player &&
                cards_data[iid].toughness == inst->targets[5].card))) // or already has right p/t
      return 0;

    if (iid == inst->targets[7].player && cards_data[inst->dummy3].type == new_typ)
      change_to = inst->dummy3;
    else
    {
      change_to = create_a_card_type(iid);
      if (change_to == -1)
        return 0;

      card_data_t *cd = &cards_data[change_to];
      cd->type = new_typ;

      if ((new_typ & TYPE_CREATURE) &&
          !(inst->targets[5].player == -1 && inst->targets[5].card == -1))
      {
        cd->power = inst->targets[5].player;
        cd->toughness = inst->targets[5].card;
      }

      cd->static_ability |= inst->targets[4].player;

      inst->targets[7].player = iid;
      inst->dummy3 = change_to;
      ench->regen_status |= KEYWORD_RECALC_ALL & ~KEYWORD_RECALC_CHANGE_TYPE;
    }

    if (event == EVENT_CHANGE_TYPE)
      event_result = change_to;
    else
    {
      ench->internal_card_id = change_to;
      ench->regen_status |= KEYWORD_RECALC_ALL;
    }

    if (inst->damage_source_card == inst->damage_target_card &&
        inst->damage_source_player == inst->damage_target_player &&
        (inst->targets[6].player & TYPE_CREATURE))
      ench->token_status |= STATUS_LEGACY_TYPECHANGE;
  }

  // Nutty that this has to be called for events besides EVENT_ABILITIES.
  if (inst->damage_target_card != -1 && inst->targets[4].card)
    special_abilities(inst->damage_target_player, inst->damage_target_card, event,
                      inst->targets[4].card, player, card);

  if (event == EVENT_SET_COLOR && affect_me(inst->damage_target_player, inst->damage_target_card) &&
      inst->targets[7].card > 0)
    event_result = inst->targets[7].card & COLOR_TEST_ANY_COLORED;

  if (event == EVENT_CLEANUP && !(inst->token_status & STATUS_PERMANENT))
    kill_card(player, card, KILL_EXILE);

  return 0;
}

static int
add_or_remove_types_legacy_impl(int src_player, int src_card,
								int tgt_player, int tgt_card,
								type_t types_to_remove, type_t types_to_add,
								int pow, int tgh,
								keyword_t kws_to_add, sp_keyword_t sp_kws_to_add,
								color_test_t color, int permanent)
{
  card_instance_t* inst = get_card_instance(tgt_player, tgt_card);
  if (inst->internal_card_id == activation_card)
	{
	  tgt_player = inst->parent_controller;
	  tgt_card = inst->parent_card;
	}

  if (!in_play(tgt_player, tgt_card))
	return -1;

  int leg_card = create_targetted_legacy_effect(src_player, src_card, fx_change_types, tgt_player, tgt_card);
  card_instance_t* legacy = get_card_instance(src_player, leg_card);
  legacy->targets[4].player = kws_to_add;
  legacy->targets[4].card = sp_kws_to_add;
  legacy->targets[5].player = pow;
  legacy->targets[5].card = tgh;
  legacy->targets[6].player = types_to_add;
  legacy->targets[6].card = types_to_remove;
  legacy->targets[7].player = -1;
  legacy->targets[7].card = color;
  if (permanent)
    legacy->token_status |= STATUS_PERMANENT;
  play_sound_effect(WAV_CHANGET);
  call_card_function_i(legacy, src_player, leg_card, EVENT_EFFECT_CREATED);

  return leg_card;
}

/* Animates {player,card} by adding TYPE_CREATURE, with characteristic power, toughness, and keywords; continuously-added sp_keywords; and
 * continuously-overriden color (unless color is 0).  Lasts until end of turn unless permanent is nonzero.  The animated card will have token_status &
 * STATUS_LEGACY_TYPECHANGE set while animated by this effect.  Returns a legacy effect index, but you shouldn't do anything with it except maybe give it
 * alternate text. */
int
animate_self(int player, int card, int pow, int tgh, keyword_t kws_to_add, sp_keyword_t sp_kws_to_add, color_test_t color, int permanent)
{
  return add_or_remove_types_legacy_impl(player, card, player, card, TYPE_NONE, TYPE_CREATURE, pow, tgh, kws_to_add, sp_kws_to_add,
										 get_sleighted_color_test(player, card, color), permanent);
}

// As per animate_self(), but works on a different card than the effect source and doesn't set STATUS_LEGACY_TYPECHANGE.
int
animate_other(int src_player, int src_card, int tgt_player, int tgt_card, int pow, int tgh, keyword_t kws_to_add, sp_keyword_t sp_kws_to_add, color_test_t color, int permanent)
{
  return add_or_remove_types_legacy_impl(src_player, src_card, tgt_player, tgt_card, TYPE_NONE, TYPE_CREATURE, pow, tgh, kws_to_add, sp_kws_to_add,
										 get_sleighted_color_test(src_player, src_card, color), permanent);
}

// {player,card} has an additional subtype subt if it animated itself via animate_self().
void
has_subtype_if_animated_self(int player, int card, event_t event, subtype_t subt)
{
  if (event == EVENT_SUBTYPE && (unsigned)event_result == subt)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (inst->token_status & STATUS_LEGACY_TYPECHANGE)
		event_result = 0;
	}
}

// {player,card} has two additional subtypes subt1 and subt2 if it animated itself via animate_self().
void
has_subtypes_if_animated_self(int player, int card, event_t event, subtype_t subt1, subtype_t subt2)
{
  if (event == EVENT_SUBTYPE && ((unsigned)event_result == subt1 || (unsigned)event_result == subt2))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (inst->token_status & STATUS_LEGACY_TYPECHANGE)
		event_result = 0;
	}
}

int count_all_other_creatures_that_shares_a_creature_type_with_me(int player, int card, int t_player){
	int result = 0;
	int p;
	for(p=0; p<2; p++){
		if( p == t_player || t_player == ANYBODY ){
			int c;
			for(c=0; c<active_cards_count[p]; c++){
				if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && !(p == player && c == card) && 
					shares_creature_subtype(player, card, p, c) )
				{
					result++;
				}
			}
		}
	}
	return result;
}

#undef NUM_SUBTYPES

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int add_a_subtype_until_eot(int player, int card, int t_player, int t_card, subtype_t subtype);
int add_a_subtype_permanently(int player, int card, int t_player, int t_card, subtype_t subtype);
static int add_force_a_subtype_until_eot_legacy(int player, int card, event_t event);
static int add_force_a_subtype_permanently_legacy(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2672900; original functions/has_creature_type.c:404. */
card_instance_t *update_alter_add_subtype(subtype_t subt, card_instance_t *inst)
{
  card_ext_t *extension;
  uint16_t *subtype_cursor;
  int i;

  if (inst == NULL)
  {
    inst = get_card_instance(affected_card_controller, affected_card);
  }
  extension = inst->ext;
  subtype_cursor = extension->subtype;
  do
  {
    if (*subtype_cursor == subt)
    {
      return inst;
    }
    subtype_cursor = subtype_cursor + 1;
  } while (subtype_cursor != extension->subtype + 12);
  i = 0;
  do
  {
    if (extension->subtype[i] == 0)
    {
      extension->subtype[i] = (uint16_t)subt;
      return inst;
    }
    i = i + 1;
  } while (i != 0xc);
  return inst;
}

/* ManalinkEh 2022: 0x2672d90; original functions/has_creature_type.c:433. */
card_instance_t *update_alter_remove_subtype(subtype_t subt, card_instance_t *inst)
{
  card_ext_t *extension;
  int subtype_index;
  int next_index;
  subtype_t shifted_subtype;
  int shift_index;

  if (inst == NULL)
  {
    inst = get_card_instance(affected_card_controller, affected_card);
  }
  extension = inst->ext;
  subtype_index = 0;
  do
  {
    while (extension->subtype[subtype_index] != subt)
    {
      subtype_index = subtype_index + 1;
      if (subtype_index == 0xc)
      {
        return inst;
      }
    }
    do
    {
      extension->subtype[subtype_index] = 0;
      shift_index = subtype_index;
      if (subtype_index < 0xc)
      {
        do
        {
          next_index = shift_index + 1;
          if ((shift_index == 11 ? extension->inst_info_data[0].id
                                 : extension->subtype[shift_index + 1]) == 0)
            break;
          extension->subtype[shift_index] =
              (shift_index == 11 ? extension->inst_info_data[0].id
                                 : extension->subtype[shift_index + 1]);
          shift_index = next_index;
        } while (next_index != 0xc);
        shifted_subtype = (subtype_t)extension->subtype[subtype_index];
      }
      else
      {
        shifted_subtype = SUBTYPE_INVALID;
      }
    } while (subt == shifted_subtype);
    subtype_index = subtype_index + 1;
    if (subtype_index == 0xc)
    {
      return inst;
    }
  } while (true);
}

/* ManalinkEh 2022: 0x2672eb0; original functions/has_creature_type.c:458. */
card_instance_t *update_alter_remove_creature_subtypes(card_instance_t *inst)
{
  card_ext_t *extension;
  int next_index;
  int i;
  int shift_index;

  if (inst == NULL)
  {
    inst = get_card_instance(affected_card_controller, affected_card);
  }
  extension = inst->ext;
  i = 0;
  do
  {
    while ((0xffeU < (uint32_t)extension->subtype[i] - 1U ||
            (extension->subtype[i] = 0, shift_index = i, 0xb < i)))
    {
      i = i + 1;
      if (i == 0xc)
      {
        return inst;
      }
    }
    do
    {
      next_index = shift_index + 1;
      if ((shift_index == 11 ? extension->inst_info_data[0].id
                             : extension->subtype[shift_index + 1]) == 0)
        break;
      extension->subtype[shift_index] = (shift_index == 11 ? extension->inst_info_data[0].id
                                                           : extension->subtype[shift_index + 1]);
      shift_index = next_index;
    } while (next_index != 0xc);
  } while (true);
}

/* ManalinkEh 2022: 0x2672fa0; original functions/has_creature_type.c:500. */
void remove_a_subtype(int player, int card, subtype_t subtype)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  update_alter_remove_subtype(subtype, instance);
  return;
}

/* ManalinkEh 2022: 0x2672360; original functions/has_creature_type.c:505. */
static int add_force_a_subtype_until_eot_legacy(int player, int card, event_t event)
{
  subtype_t subtype;
  int result;
  card_instance_t *instance;

  if (event == EVENT_CHANGE_TYPE)
  {
    instance = in_play(player, card);
    if ((((instance != NULL) && (instance->damage_target_card == affected_card)) &&
         (instance->damage_target_player == affected_card_controller)) &&
        (subtype = instance->targets[1].card, subtype - SUBTYPE_ALLY < 0xfff))
    {

      if ((land_can_be_played & 0x20000U) == 0)
      {
        land_can_be_played = land_can_be_played | 0x10000;
      }
      result = instance->targets[1].player;
      if (result == 2)
      {
        attached_is_forced_subtype(player, card, EVENT_CHANGE_TYPE, subtype);
      }
      else if (result == 1)
      {
        attached_is_additional_subtype(player, card, EVENT_CHANGE_TYPE, subtype);
      }
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int animate_self_remove_type(int player, int card, int pow, int tgh, type_t types_to_remove,
                             keyword_t kws_to_add, sp_keyword_t sp_kws_to_add, color_test_t color,
                             int permanent);

/* ManalinkEh 2022: 0x2673040; original functions/has_creature_type.c:534. */
int add_a_subtype_until_eot(int player, int card, int t_player, int t_card, subtype_t subtype)
{
  int legacy = create_targetted_legacy_effect(player, card, add_force_a_subtype_until_eot_legacy,
                                              t_player, t_card);
  get_card_instance(player, legacy)->targets[1].player = 1;
  get_card_instance(player, legacy)->targets[1].card = subtype;
  return legacy;
}

/* ManalinkEh 2022: 0x26726f0; original functions/has_creature_type.c:538. */
static int add_force_a_subtype_permanently_legacy(int player, int card, event_t event)
{
  subtype_t subtype;
  int result;
  card_instance_t *instance;
  int32_t *additional_subtype_cursor;

  if (event == EVENT_CHANGE_TYPE)
  {

    if ((land_can_be_played & 0x20000U) == 0)
    {
      land_can_be_played = land_can_be_played | 0x10000;
    }

    instance = get_card_instance(player, card);
    subtype = instance->targets[1].card;
    if (subtype - SUBTYPE_ALLY < 0xfff)
    {

      result = instance->targets[1].player;
      if (result == 1)
      {
        additional_subtype_cursor = &instance->targets[2].card;
        attached_is_additional_subtype(player, card, EVENT_CHANGE_TYPE, subtype);
        do
        {
          if ((uint32_t)*additional_subtype_cursor - SUBTYPE_ALLY < 0xfffU)
          {
            attached_is_additional_subtype(player, card, EVENT_CHANGE_TYPE,
                                           *additional_subtype_cursor);
          }
          additional_subtype_cursor = additional_subtype_cursor + 2;
        } while (additional_subtype_cursor != &instance->targets[0xc].card);
      }
      else if (result == 2)
      {
        additional_subtype_cursor = &instance->targets[2].card;
        attached_is_forced_subtype(player, card, EVENT_CHANGE_TYPE, subtype);
        do
        {
          if ((uint32_t)*additional_subtype_cursor - SUBTYPE_ALLY < 0xfffU)
          {
            attached_is_additional_subtype(player, card, EVENT_CHANGE_TYPE,
                                           *additional_subtype_cursor);
          }
          additional_subtype_cursor = additional_subtype_cursor + 2;
        } while (additional_subtype_cursor != &instance->targets[0xc].card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26730b0; original functions/has_creature_type.c:578. */
int add_a_subtype_permanently(int player, int card, int t_player, int t_card, subtype_t subtype)
{
  int legacy_card;
  card_instance_t *instance;

  legacy_card = create_targetted_legacy_effect(player, card, add_force_a_subtype_permanently_legacy,
                                           t_player, t_card);
  instance = get_card_instance(player, legacy_card);
  instance->targets[1].player = 1;
  instance->targets[1].card = subtype;
  instance->token_status = 0x10000;
  return legacy_card;
}

/* ManalinkEh 2022: 0x2673110; original functions/has_creature_type.c:582. */
void purge_add_force_a_subtype_permanently_legacy(int player, int card, int mode)
{
  card_instance_t *instance;
  int card_00;
  int player_00;

  player_00 = 0;
  do
  {
    card_00 = 0;
    if (0 < active_cards_count[player_00])
    {
      do
      {
        instance = in_play(player_00, card_00);
        if ((((instance != NULL) && (instance->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
             (instance->info_slot_fn == add_force_a_subtype_permanently_legacy)) &&
            (((instance->damage_target_player == player &&
               (instance->damage_target_card == card)) &&
              ((mode == -1 || (instance = get_card_instance(player_00, card_00),
                               instance->targets[1].player == mode))))))
        {
          kill_card(player_00, card_00, KILL_EXILE);
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[player_00]);
    }
    player_00 = player_00 + 1;
  } while (player_00 != 2);
  return;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x2672e30; original functions/has_creature_type.c:597. */
static int remove_a_subtype_permanently_legacy(int player, int card, event_t event)
{
  card_instance_t *effect;
  card_instance_t *inst;

  if (event == EVENT_CHANGE_TYPE)
  {

    if ((land_can_be_played & 0x20000U) == 0)
    {
      land_can_be_played = land_can_be_played | 0x10000;
    }

    effect = get_card_instance(player, card);
    if ((effect->damage_target_card == affected_card) &&
        (effect->damage_target_player == affected_card_controller))
    {
      inst = get_card_instance((int)effect->damage_target_player, effect->damage_target_card);
      update_alter_remove_subtype(effect->targets[1].card, inst);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26731c0; original functions/has_creature_type.c:610. */
int remove_a_subtype_permanently(int player, int card, int t_player, int t_card, subtype_t subtype)
{
  int legacy;
  card_instance_t *instance;

  legacy = create_targetted_legacy_effect(player, card, remove_a_subtype_permanently_legacy,
                                           t_player, t_card);
  instance = get_card_instance(player, legacy);
  instance->targets[1].card = subtype;
  instance->token_status = 0x10000;
  return legacy;
}

/* ManalinkEh 2022: 0x2673220; original functions/has_creature_type.c:636. */
int add_a_subtype_not_in_play(int player, event_t event, subtype_t subtype)
{
  if (event != EVENT_CHANGE_TYPE)
    return 0;
  if (!(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
  {
    land_can_be_played |= LCBP_NEED_EVENT_CHANGE_TYPE_SECOND_PASS;
    return 0;
  }
  for (int index = 0; index < 15; ++index)
  {
    subtypes_not_in_play_t *modifier = &added_subtypes_not_in_play[player][index];
    if (modifier->subtype == SUBTYPE_INVALID)
    {
      modifier->subtype = subtype;
      modifier->added_forced = 1;
      return 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x26732a0; original functions/has_creature_type.c:640. */
int force_a_subtype_not_in_play(int player, event_t event, subtype_t subtype)
{
  if (event != EVENT_CHANGE_TYPE)
    return 0;
  if (!(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS))
  {
    land_can_be_played |= LCBP_NEED_EVENT_CHANGE_TYPE_SECOND_PASS;
    return 0;
  }
  for (int index = 0; index < 15; ++index)
  {
    subtypes_not_in_play_t *modifier = &added_subtypes_not_in_play[player][index];
    if (modifier->subtype == SUBTYPE_INVALID)
    {
      modifier->subtype = subtype;
      modifier->added_forced = 2;
      return 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2673320; original functions/has_creature_type.c:644. */
void get_subtypes_by_csvid(int player, int csvid, reported_subtypes_t *my_subtypes)
{
  memset(my_subtypes, 0, sizeof(*my_subtypes));
  card_ptr_t *cp = cards_ptr[csvid];
  for (int i = 0; i < 7; ++i)
    my_subtypes->reported_subtypes[i] = (int)cp->types[i];
  int count = 7;
  if (player >= 0 && is_what(-1, get_internal_card_id_from_csv_id(csvid), TYPE_CREATURE))
  {
    for (int index = 0; index < 15; ++index)
    {
      const subtypes_not_in_play_t *modifier = &added_subtypes_not_in_play[player][index];
      unsigned int subtype = modifier->subtype;
      if (subtype - 1U > 0xffeU)
        break;
      if (modifier->added_forced == 2)
      {
        memset(my_subtypes, 0, sizeof(*my_subtypes));
        count = 0;
      }
      my_subtypes->reported_subtypes[count++] = subtype;
    }
  }
  if (csvid == CARD_ID_GRIST_THE_HUNGER_TIDE)
    my_subtypes->reported_subtypes[count] = SUBTYPE_INSECT;
}

/* ManalinkEh 2022: 0x26736e0; original functions/has_creature_type.c:721. */
int has_subtype_by_id(int player, int id, subtype_t subtype)
{
  int creature = is_what(-1, get_internal_card_id_from_csv_id(id), TYPE_CREATURE);
  int creature_subtype = (unsigned int)subtype - SUBTYPE_ALLY <= 0xffeU;
  if (creature && creature_subtype && player >= 0)
  {
    reported_subtypes_t types;
    get_subtypes_by_csvid(player, id, &types);
    for (int index = 0; index < 15; ++index)
    {
      const subtypes_not_in_play_t *modifier = &added_subtypes_not_in_play[player][index];
      unsigned int added_subtype = modifier->subtype;
      if (added_subtype - 1U > 0xffeU)
        break;
      if (modifier->added_forced == 2)
        memset(&types, 0, sizeof(types));
      // Preserve the release's second modifier pass and its growing search bound.
      for (int slot = 0; slot <= index + 5; ++slot)
        if (types.reported_subtypes[slot] == 0)
        {
          types.reported_subtypes[slot] = added_subtype;
          break;
        }
    }
    // The native query inspects twenty entries, not all twenty-six.
    for (int index = 0; index < 20; ++index)
      if (types.reported_subtypes[index] == SUBTYPE_ALL_CREATURES ||
          types.reported_subtypes[index] == subtype)
        return 1;
  }
  else if (subtype == SUBTYPE_LEGEND)
  {
    int iid = get_internal_card_id_from_csv_id(id);
    if ((unsigned char)(cards_data[iid].subtype - 0xe) < 2)
      return 1;
    return has_new_types(id, subtype);
  }
  else if (!creature_subtype)
  {
    if (subtype == SUBTYPE_AURA_CREATURE)
      return cards_ptr[id]->subtype1 == 0x2c && has_new_types(id, SUBTYPE_AURA) != 0;
    return has_new_types(id, subtype);
  }

  int iid = get_internal_card_id_from_csv_id(id);
  if (cards_data[iid].static_ability & KEYWORD_CHANGELING)
    return 1;
  if (id == CARD_ID_GRIST_THE_HUNGER_TIDE && subtype == SUBTYPE_INSECT)
    return 1;
  return has_new_types(id, subtype);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void remove_a_subtype(int player, int card, subtype_t subtype);
static int remove_a_subtype_permanently_legacy(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2673b90; original functions/has_creature_type.c:913. */
int shares_creature_subtype_by_iid(int p0, int c0, int p1, int iid)
{
  bool first_is_changeling;
  int result;
  int *second_subtype_cursor;
  int *first_subtype_cursor;
  reported_subtypes_t stypes;
  int subtypes[2][26];

  result = is_what(p0, c0, 2);
  if (((result != 0) || (result = has_subtype(p0, c0, SUBTYPE_TRIBAL), result != 0)) &&
      ((result = is_what(-1, iid, 2),
        result != 0 ||
            (result = has_subtype_by_id(p1, (unsigned int)cards_data[iid].id, SUBTYPE_TRIBAL),
             result != 0))))
  {
    first_subtype_cursor = subtypes[0];
    memset(subtypes, 0, sizeof(subtypes));
    result = is_changeling(p0, c0);
    if (result == 0)
    {
      get_subtypes(p0, c0, &stypes);
      result = 0;
      do
      {
        first_subtype_cursor[result] = stypes.reported_subtypes[result];
        result = result + 1;
      } while (result != 0x1a);
      first_is_changeling = false;
      if ((cards_data[iid].static_ability & 0x80000) != 0)
      {
        do
        {
          if (*first_subtype_cursor - 1U < 0xfff)
          {
            return 1;
          }
          first_subtype_cursor = first_subtype_cursor + 1;
        } while ((int (*)[26])first_subtype_cursor != subtypes + 1);
        return 0;
      }
    }
    else
    {
      if ((cards_data[iid].static_ability & 0x80000) != 0)
      {
        return 1;
      }
      first_is_changeling = true;
    }
    get_subtypes_by_csvid(p1, (unsigned int)cards_data[iid].id, &stypes);
    result = 0;
    do
    {
      subtypes[1][result] = stypes.reported_subtypes[result];
      result = result + 1;
    } while (result != 0x1a);
    if (first_is_changeling)
    {
      first_subtype_cursor = subtypes[1];
      do
      {
        if (*first_subtype_cursor - 1U < 0xfff)
        {
          return 1;
        }
        first_subtype_cursor = first_subtype_cursor + 1;
      } while (first_subtype_cursor != (subtypes[1] + 26));
    }
    else
    {
      do
      {
        second_subtype_cursor = subtypes[1];
        if (*first_subtype_cursor - 1U < 0xfff)
        {
          do
          {
            if (*first_subtype_cursor == *second_subtype_cursor)
            {
              return 1;
            }
            second_subtype_cursor = second_subtype_cursor + 1;
          } while (second_subtype_cursor != (subtypes[1] + 26));
        }
        first_subtype_cursor = first_subtype_cursor + 1;
      } while ((int (*)[26])first_subtype_cursor != subtypes + 1);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2675a90; original functions/has_creature_type.c:1848. */
int animate_self_to_artifact_creature(int player, int card, int pow, int tgh, keyword_t kws_to_add,
                                      sp_keyword_t sp_kws_to_add, color_test_t color, int permanent)
{
  color_test_t color_00;
  int result;

  color_00 = get_sleighted_color_test(player, card, color);
  result = add_or_remove_types_legacy_impl(player, card, player, card, TYPE_NONE,
                                           TYPE_ARTIFACT | TYPE_CREATURE, pow, tgh, kws_to_add,
                                           sp_kws_to_add, color_00, permanent);
  return result;
}

/* ManalinkEh 2022: 0x2675b90; original functions/has_creature_type.c:1863. */
int animate_self_remove_type(int player, int card, int pow, int tgh, type_t types_to_remove,
                             keyword_t kws_to_add, sp_keyword_t sp_kws_to_add, color_test_t color,
                             int permanent)
{
  color_test_t color_00;
  int result;

  color_00 = get_sleighted_color_test(player, card, color);
  result =
      add_or_remove_types_legacy_impl(player, card, player, card, types_to_remove, TYPE_CREATURE,
                                      pow, tgh, kws_to_add, sp_kws_to_add, color_00, permanent);
  return result;
}

#ifndef SHANDALAR
/* Recovered October 2022 pump effects and cleanup helpers. */

/* ManalinkEh 2022: 0x2672f30; original functions/has_creature_type.c:492. */

/* ManalinkEh 2022: 0x2672f70; original functions/has_creature_type.c:496. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2672860; original functions/has_creature_type.c:323. */
void reset_subtypes_from_instance(card_instance_t *inst, iid_t iid)
{
  card_ptr_t *cp = cards_ptr[cards_data[iid].id];
  for (int i = 0; i < 7; ++i)
  {
    int16_t st = cp->types[i];
    if (st < 0)
      st = 0;
    if ((st >= 0x1000 && st < 0x100a && ((1u << (st & 31)) & 0x35fu)) || st == 0x2005)
      st = 0;
    inst->ext->subtype[i] = st;
  }
  for (int i = 7; i < 12; ++i)
    inst->ext->subtype[i] = 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
static int add_force_a_subtype_until_eot_impl(int player, int card, int t_player, int t_card,
                                             subtype_t subtype, int mode)
{
  int legacy = create_targetted_legacy_effect(player, card, add_force_a_subtype_until_eot_legacy,
                                             t_player, t_card);
  get_card_instance(player, legacy)->targets[1].player = mode;
  get_card_instance(player, legacy)->targets[1].card = subtype;
  return legacy;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
static int add_force_a_subtype_permanently(int player, int card, int t_player, int t_card,
                                            subtype_t subtype, int mode)
{
  int legacy = create_targetted_legacy_effect(player, card, add_force_a_subtype_permanently_legacy,
                                             t_player, t_card);
  card_instance_t *instance = get_card_instance(player, legacy);
  instance->targets[1].player = mode;
  instance->targets[1].card = subtype;
  instance->token_status = 0x10000;
  return legacy;
}

static int is_tribal_by_id(int player, int csvid)
{
  return has_subtype_by_id(player, csvid, SUBTYPE_TRIBAL);
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline mechanics; native callers retain their expansions. */

#endif
