#include <limits.h>

#include "manalink.h"

typedef struct
{
  counter_t type;
  int num;
} counter_snapshot_t;

static int snapshot_counters(card_instance_t *instance, counter_snapshot_t counters[9])
{
  const uint8_t *labels = (const uint8_t *)&instance->targets[18].player;
  const uint8_t *overflow = (const uint8_t *)&instance->targets[18].card;
  const uint8_t quantities[9] = {instance->special_counters_byte, instance->counters2,
      instance->counters3, instance->counters4, instance->counters5, instance->counters_byte,
      instance->counters_m1m1, overflow[0], overflow[1]};
  const counter_t types[9] = {instance->special_counter_type, labels[0], labels[1],
      labels[2], labels[3], COUNTER_P1_P1, COUNTER_M1_M1, overflow[2], overflow[3]};
  int count = 0;
  for (int i = 0; i < 9; ++i)
    if (quantities[i] != 0 && types[i] != COUNTER_invalid)
    {
      counters[count].type = types[i];
      counters[count].num = quantities[i];
      ++count;
    }
  return count;
}

static void add_or_remove_counters_impl(int player,int card,int t_player,int t_card,counter_t type,int num);

/* Canonical recovered helper declarations. */
static int get_updated_counters_number_as_cost(int player, int card, counter_t type, int number);
int get_updated_counters_number(int player, int card, counter_t type, int number);
static void add_or_remove_counters_impl(int player, int card, int t_player, int t_card,
                                                    counter_t type, int num);
void add_counters(int player, int card, int t_player, int t_card, counter_t type,
                              int num);
void remove_counters(int player, int card, int t_player, int t_card, counter_t type,
                                 int num);
void remove_all_counters(int player, int card, int t_player, int t_card, counter_t type);
void copy_counters(int player, int card, int t_player, int t_card, int src_player,
                               int src_card, int raw);
void add_counters_predoubled(int player, int card, int t_player, int t_card,
                                         counter_t type, int num);
void move_counters(int player, int card, int t_player, int t_card, int src_player,
                               int src_card, counter_t counter_type, int number);

int hack_silent_counters = 0;
int counters_added = 0;	// backed up and saved

/* Relevant data in card_instance_t:
 * special_counters: number of "special" counters (preferred type determined by card csvid; stored in special_counter_type)
 * counters2: drawn in upper right of lower section.  Originally -0/-2 counters for Spirit Shackle; now arbitrary.
 * counters3: drawn in upper center of lower section.  Originally -1/-1 counters for Unstable Mutation; now arbitrary.
 * counters4: drawn in upper left of lower section.  Originally -0/-1 counters for Orcish Catapult; now arbitrary.
 * counters: drawn lower left of lower section.  Always +1/+1 counters (originally using text from Dwarven Weaponsmith).
 * counters5: drawn in lower right of lower section.  Originally +1/+1 counters from Ashnod's Transmogrant; now arbitray.
 * counters_m1m1: drawn in lower left of lower section with +1/+1 counters.  Originally unused.  Always -1/-1 counters.
 * targets[18].player: byte0: type of counters2.  byte1: type of counters3.  byte2: type of counters4.  byte3: type of counters5.
 * targets[18].card: byte0: non-displayed counters slot A.  byte1: non-displayed counters slot B.  byte2: type of byte0.  byte3: type of byte1.
 */

static void play_counters_sound(wav_t wav){
	if (hack_silent_counters <= 0){
		play_sound_effect(wav);
	}
}

int get_counter_type_by_id(int csvid)
{
  switch (csvid)
  {
  case CARD_ID_ARNJLOTS_ASCENT:
  case CARD_ID_BALDUVIAN_SHAMAN:
  case CARD_ID_BLIZZARD:
  case CARD_ID_BRAND_OF_ILL_OMEN:
  case CARD_ID_BREATH_OF_DREAMS:
  case CARD_ID_COLD_SNAP:
  case CARD_ID_ENERGY_STORM:
  case CARD_ID_FLOW_OF_MAGGOTS:
  case CARD_ID_FYNDHORN_POLLEN:
  case CARD_ID_GLACIAL_CHASM:
  case CARD_ID_HALLS_OF_MIST:
  case CARD_ID_ILLUSIONARY_FORCES:
  case CARD_ID_ILLUSIONARY_PRESENCE:
  case CARD_ID_ILLUSIONARY_TERRAIN:
  case CARD_ID_ILLUSIONARY_WALL:
  case CARD_ID_ILLUSIONS_OF_GRANDEUR:
  case CARD_ID_INFERNAL_DARKNESS:
  case CARD_ID_MADDENING_WIND:
  case CARD_ID_MESMERIC_TRANCE:
  case CARD_ID_MUSICIAN:
  case CARD_ID_MYSTIC_MIGHT:
  case CARD_ID_MYSTIC_REMORA:
  case CARD_ID_NAKED_SINGULARITY:
  case CARD_ID_POLAR_KRAKEN:
  case CARD_ID_REALITY_TWIST:
  case CARD_ID_RITUAL_OF_SUBDUAL:
  case CARD_ID_SNOWFALL:
  case CARD_ID_SOLDEVI_SIMULACRUM:
  case CARD_ID_DYSTOPIA:
  case CARD_ID_ROYAL_DECREE:
  case CARD_ID_SUSTAINING_SPIRIT:
  case CARD_ID_THOUGHT_LASH:
  case CARD_ID_TIDAL_CONTROL:
  case CARD_ID_TORNADO:
  case CARD_ID_VARCHILDS_WAR_RIDERS:
  case CARD_ID_YAVIMAYA_ANTS:
  case CARD_ID_DECOMPOSITION:
  case CARD_ID_MALIGNANT_GROWTH:
  case CARD_ID_MIND_HARNESS:
  case CARD_ID_PRISMATIC_CIRCLE:
  case CARD_ID_TOMBSTONE_STAIRWELL:
  case CARD_ID_CORROSION:
  case CARD_ID_ELEPHANT_GRASS:
  case CARD_ID_FIRESTORM_HELLKITE:
  case CARD_ID_HEAT_WAVE:
  case CARD_ID_JUJU_BUBBLE:
  case CARD_ID_ABOROTH:
  case CARD_ID_ANCESTRAL_KNOWLEDGE:
  case CARD_ID_ARCTIC_WOLVES:
  case CARD_ID_GALLOWBRAID:
  case CARD_ID_HEART_OF_BOGARDAN:
  case CARD_ID_INNER_SANCTUM:
  case CARD_ID_MANA_CHAINS:
  case CARD_ID_MORINFEN:
  case CARD_ID_MWONVULI_OOZE:
  case CARD_ID_PSYCHIC_VORTEX:
  case CARD_ID_REVERED_UNICORN:
  case CARD_ID_UKTABI_EFREET:
  case CARD_ID_VOLUNTEER_RESERVES:
  case CARD_ID_WAVE_OF_TERROR:
  case CARD_ID_ARCTIC_NISHOBA:
  case CARD_ID_BALDUVIAN_FALLEN:
  case CARD_ID_BRAID_OF_FIRE:
  case CARD_ID_COVER_OF_WINTER:
  case CARD_ID_EARTHEN_GOO:
  case CARD_ID_FREYALISES_RADIANCE:
  case CARD_ID_GLACIAL_PLATING:
  case CARD_ID_HERALD_OF_LESHRAC:
  case CARD_ID_HIBERNATIONS_END:
  case CARD_ID_JOTUN_GRUNT:
  case CARD_ID_JOTUN_OWL_KEEPER:
  case CARD_ID_KARPLUSAN_MINOTAUR:
  case CARD_ID_KJELDORAN_JAVELINEER:
  case CARD_ID_KROVIKAN_WHISPERS:
  case CARD_ID_MAGMATIC_CORE:
  case CARD_ID_PHOBIAN_PHANTASM:
  case CARD_ID_PHYREXIAN_ETCHINGS:
  case CARD_ID_PHYREXIAN_SOULGORGER:
  case CARD_ID_RONOM_HULK:
  case CARD_ID_SHELTERING_ANCIENT:
  case CARD_ID_SURVIVOR_OF_THE_UNSEEN:
  case CARD_ID_VEXING_SPHINX:
  case CARD_ID_WALL_OF_SHARDS:
  case CARD_ID_SPLINTER_TOKEN:
    return COUNTER_AGE;
  case CARD_ID_HANKYU:
    return COUNTER_AIM;
  case CARD_ID_ARCHERY_TRAINING:
    return COUNTER_ARROW;
  case CARD_ID_SERRATED_ARROWS:
    return COUNTER_ARROWHEAD;
  case CARD_ID_FIVE_ALARM_FIRE:
    return COUNTER_BLAZE;
  case CARD_ID_BLOODLETTER_QUILL:
  case CARD_ID_RAKDOS_RITEKNIFE:
    return COUNTER_BLOOD;
  case CARD_ID_BOUNTY_HUNTER:
    return COUNTER_BOUNTY;
  case CARD_ID_GWAFA_HAZID_PROFITEER:
    return COUNTER_BRIBERY;
  case CARD_ID_OSAI_VULTURES:
    return COUNTER_CARRION;
  case CARD_ID_SCAVENGING_GHOUL:
    return COUNTER_CORPSE;
  case CARD_ID_ICATIAN_MONEYCHANGER:
    return COUNTER_CREDIT;
  case CARD_ID_DELIFS_CUBE:
    return COUNTER_CUBE;
  case CARD_ID_TRADE_CARAVAN:
    return COUNTER_CURRENCY;
  case CARD_ID_BOGARDAN_PHOENIX:
  case CARD_ID_DEADBOX:
    return COUNTER_DEATH;
  case CARD_ID_ERTAIS_MEDDLING:
  case CARD_ID_DELAYING_SHIELD:
    return COUNTER_DELAY;
  case CARD_ID_LAND_CAP:
  case CARD_ID_LAVA_TUBES:
  case CARD_ID_RIVER_DELTA:
  case CARD_ID_TIMBERLINE_RIDGE:
  case CARD_ID_VELDT:
  case CARD_ID_HICKORY_WOODLOT:
  case CARD_ID_PEAT_BOG:
  case CARD_ID_REMOTE_FARM:
  case CARD_ID_SANDSTONE_NEEDLE:
  case CARD_ID_SAPRAZZAN_SKERRY:
  case CARD_ID_DECREE_OF_SILENCE:
  case CARD_ID_FORCE_BUBBLE:
    return COUNTER_DEPLETION;
  case CARD_ID_DESCENT_INTO_MADNESS:
    return COUNTER_DESPAIR;
  case CARD_ID_BLOODTHIRSTY_OGRE:
  case CARD_ID_PIOUS_KITSUNE:
    return COUNTER_DEVOTION;
  case CARD_ID_MYOJIN_OF_CLEANSING_FIRE:
  case CARD_ID_MYOJIN_OF_INFINITE_RAGE:
  case CARD_ID_MYOJIN_OF_LIFES_WEB:
  case CARD_ID_MYOJIN_OF_NIGHTS_REACH:
  case CARD_ID_MYOJIN_OF_SEEING_WINDS:
    return COUNTER_DIVINITY;
  case CARD_ID_ARMAGEDDON_CLOCK:
  case CARD_ID_YOUR_INESCAPABLE_DOOM:
  case CARD_ID_IMMINENT_DOOM:
    return COUNTER_DOOM;
  case CARD_ID_RASPUTIN_DREAMWEAVER:
    return COUNTER_DREAM;
  case CARD_ID_SOUL_ECHO:
    return COUNTER_ECHO;
  case CARD_ID_ESSENCE_BOTTLE:
    return COUNTER_ELIXIR;
  case CARD_ID_ENERGY_VORTEX:
  case CARD_ID_ELDER_DRAGON_HIGHLANDER:
  case CARD_ID_PLANECHASE:
  case CARD_ID_SPAT:
  case CARD_ID_RULES_ENGINE:
    return COUNTER_VORTEX;
  case CARD_ID_MAGOSI_THE_WATERVEIL:
    return COUNTER_EON;
  case CARD_ID_JAR_OF_EYEBALLS:
    return COUNTER_EYEBALL;
  case CARD_ID_ANCIENT_HYDRA:
  case CARD_ID_BLASTODERM:
  case CARD_ID_CLOUDSKATE:
  case CARD_ID_DEFENDER_EN_VEC:
  case CARD_ID_JOLTING_MERFOLK:
  case CARD_ID_PARALLAX_DEMENTIA:
  case CARD_ID_PARALLAX_NEXUS:
  case CARD_ID_PARALLAX_TIDE:
  case CARD_ID_PARALLAX_WAVE:
  case CARD_ID_PHYREXIAN_PROWLER:
  case CARD_ID_REJUVENATION_CHAMBER:
  case CARD_ID_RUSTING_GOLEM:
  case CARD_ID_SAPROLING_BURST:
  case CARD_ID_SKYSHROUD_BEHEMOTH:
  case CARD_ID_SKYSHROUD_RIDGEBACK:
  case CARD_ID_TANGLE_WIRE:
  case CARD_ID_WOODRIPPER:
    return COUNTER_FADE;
  case CARD_ID_KANGEE_AERIE_KEEPER:
  case CARD_ID_SOULCATCHERS_AERIE:
  case CARD_ID_AVEN_MIMEOMANCER:
    return COUNTER_FEATHER;
  case CARD_ID_AZORS_ELOCUTORS:
    return COUNTER_FILIBUSTER;
  case CARD_ID_NAAR_ISLE:
    return COUNTER_FLAME;
  case CARD_ID_BOUNTY_OF_THE_LUXA:
    return COUNTER_FLOOD;
  case CARD_ID_SPOROGENESIS:
    return COUNTER_FUNGUS;
  case CARD_ID_TEMP_OF_THE_DAMNED:
    return COUNTER_FUNK;
  case CARD_ID_GOBLIN_BOMB:
  case CARD_ID_INCENDIARY:
  case CARD_ID_POWDER_KEG:
    return COUNTER_FUSE;
  case CARD_ID_DRAGONS_HOARD:
    return COUNTER_GOLD;
  case CARD_ID_MOMENTUM:
  case CARD_ID_SIMIC_ASCENDANCY:
    return COUNTER_GROWTH;
  case CARD_ID_TRIASSIC_EGG:
  case CARD_ID_LUDEVICS_TEST_SUBJECT:
    return COUNTER_HATCHLING;
  case CARD_ID_FYLGJA:
  case CARD_ID_URSINE_FYLGJA:
    return COUNTER_HEALING;
  case CARD_ID_HOOFPRINTS_OF_THE_STAG:
    return COUNTER_HOOFPRINT;
  case CARD_ID_TEMPORAL_DISTORTION:
    return COUNTER_HOURGLASS;
  case CARD_ID_FASTING:
    return COUNTER_HUNGER;
  case CARD_ID_NECROPOLIS_OF_AZAR:
    return COUNTER_HUSK;
  case CARD_ID_ICEBERG:
  case CARD_ID_DARK_DEPTHS:
  case CARD_ID_WOOLLY_RAZORBACK:
    return COUNTER_ICE;
  case CARD_ID_DISEASED_VERMIN:
  case CARD_ID_FESTERING_WOUND:
    return COUNTER_INFECTION;
  case CARD_ID_DIVINE_INTERVENTION:
    return COUNTER_INTERVENTION;
  case CARD_ID_ICATIAN_JAVELINEERS:
    return COUNTER_JAVELIN;
  case CARD_ID_BAKU_ALTAR:
  case CARD_ID_BLADEMANE_BAKU:
  case CARD_ID_BUDOKA_PUPIL:
  case CARD_ID_ICHIGA_WHO_TOPPLES_OAKS:
  case CARD_ID_CALLOW_JUSHI:
  case CARD_ID_JARAKU_THE_INTERLOPER:
  case CARD_ID_CUNNING_BANDIT:
  case CARD_ID_AZAMUKI_TREACHERY_INCARNATE:
  case CARD_ID_FAITHFUL_SQUIRE:
  case CARD_ID_KAISO_MEMORY_OF_LOYALTY:
  case CARD_ID_HIRED_MUSCLE:
  case CARD_ID_SCARMAKER:
  case CARD_ID_PETALMANE_BAKU:
  case CARD_ID_QUILLMANE_BAKU:
  case CARD_ID_SKULLMANE_BAKU:
  case CARD_ID_WAXMANE_BAKU:
    return COUNTER_KI;
  case CARD_ID_BEASTBREAKER_OF_BALA_GED:
  case CARD_ID_BRIMSTONE_MAGE:
  case CARD_ID_CARAVAN_ESCORT:
  case CARD_ID_CORALHELM_COMMANDER:
  case CARD_ID_ECHO_MAGE:
  case CARD_ID_ENCLAVE_CRYPTOLOGIST:
  case CARD_ID_GUUL_DRAZ_ASSASSIN:
  case CARD_ID_HADA_SPY_PATROL:
  case CARD_ID_HALIMAR_WAVEWATCH:
  case CARD_ID_HEDRON_FIELD_PURISTS:
  case CARD_ID_IKIRAL_OUTRIDER:
  case CARD_ID_JORAGA_TREESPEAKER:
  case CARD_ID_KABIRA_VINDICATOR:
  case CARD_ID_KARGAN_DRAGONLORD:
  case CARD_ID_KAZANDU_TUSKCALLER:
  case CARD_ID_KNIGHT_OF_CLIFFHAVEN:
  case CARD_ID_LIGHTHOUSE_CHRONOLOGIST:
  case CARD_ID_LORD_OF_SHATTERSKULL_PASS:
  case CARD_ID_NIRKANA_CUTTHROAT:
  case CARD_ID_NULL_CHAMPION:
  case CARD_ID_SKYWATCHER_ADEPT:
  case CARD_ID_STUDENT_OF_WARFARE:
  case CARD_ID_TRANSCENDENT_MASTER:
  case CARD_ID_ZULAPORT_ENFORCER:
    return COUNTER_LEVEL;
  case CARD_ID_MIND_UNBOUND:
  case CARD_ID_SCROLL_OF_THE_MASTERS:
  case CARD_ID_MYTH_REALIZED:
  case CARD_ID_CHAINERS_TORMENT:
  case CARD_ID_FALL_OF_THE_THRAN:
  case CARD_ID_HISTORY_OF_BENALIA:
  case CARD_ID_PHYREXIAN_SCRIPTURES:
  case CARD_ID_RITE_OF_BELZENLOK:
  case CARD_ID_SONG_OF_FREYALISE:
  case CARD_ID_THE_ANTIQUITIES_WAR:
  case CARD_ID_THE_ELDEST_REBORN:
  case CARD_ID_THE_FIRST_ERUPTION:
  case CARD_ID_THE_FLAME_OF_KELD:
  case CARD_ID_THE_MENDING_OF_DOMINARIA:
  case CARD_ID_THE_MIRARI_CONJECTURE:
  case CARD_ID_TIME_OF_ICE:
  case CARD_ID_TRIUMPH_OF_GERRARD:
  case CARD_ID_ELSPETH_CONQUERS_DEATH:
  case CARD_ID_ELSPETHS_NIGHTMARE:
  case CARD_ID_KIORA_BESTS_THE_SEA_GOD:
  case CARD_ID_MEDOMAIS_PROPHECY:
  case CARD_ID_THE_AKROAN_WAR:
  case CARD_ID_THE_BINDING_OF_THE_TITANS:
  case CARD_ID_THE_BIRTH_OF_MELETIS:
  case CARD_ID_THE_FIRST_IROAN_GAMES:
  case CARD_ID_THE_TRIUMPH_OF_ANAX:
  case CARD_ID_TYMARET_CALLS_THE_DEAD:
    return COUNTER_LORE;
  case CARD_ID_AJANI_GOLDMANE:
  case CARD_ID_CHANDRA_NALAAR:
  case CARD_ID_GARRUK_WILDSPEAKER:
  case CARD_ID_JACE_BELEREN:
  case CARD_ID_LILIANA_VESS:
  case CARD_ID_AJANI_VENGEANT:
  case CARD_ID_ELSPETH_KNIGHT_ERRANT:
  case CARD_ID_SARKHAN_VOL:
  case CARD_ID_TEZZERET_THE_SEEKER:
  case CARD_ID_NICOL_BOLAS_PLANESWALKER:
  case CARD_ID_CHANDRA_ABLAZE:
  case CARD_ID_NISSA_REVANE:
  case CARD_ID_SORIN_MARKOV:
  case CARD_ID_JACE_THE_MIND_SCULPTOR:
  case CARD_ID_GIDEON_JURA:
  case CARD_ID_GIDEON_JURA_ANIMATED:
  case CARD_ID_SARKHAN_THE_MAD:
  case CARD_ID_ELSPETH_TIREL:
  case CARD_ID_KOTH_OF_THE_HAMMER:
  case CARD_ID_VENSER_THE_SOJOURNER:
  case CARD_ID_TEZZERET_AGENT_OF_BOLAS:
  case CARD_ID_KARN_LIBERATED:
  case CARD_ID_CHANDRA_THE_FIREBRAND:
  case CARD_ID_GARRUK_PRIMAL_HUNTER:
  case CARD_ID_JACE_MEMORY_ADEPT:
  case CARD_ID_GARRUK_RELENTLESS:
  case CARD_ID_GARRUK_THE_VEIL_CURSED:
  case CARD_ID_LILIANA_OF_THE_VEIL:
  case CARD_ID_SORIN_LORD_OF_INNISTRAD:
  case CARD_ID_TAMIYO_THE_MOON_SAGE:
  case CARD_ID_TIBALT_THE_FIEND_BLOODED:
  case CARD_ID_AJANI_CALLER_OF_THE_PRIDE:
  case CARD_ID_LILIANA_OF_THE_DARK_REALMS:
  case CARD_ID_JACE_ARCHITECT_OF_THOUGHT:
  case CARD_ID_VRASKA_THE_UNSEEN:
  case CARD_ID_DOMRI_RADE:
  case CARD_ID_GIDEON_CHAMPION_OF_JUSTICE:
  case CARD_ID_GIDEON_CHAMPION_ANIMATED:
  case CARD_ID_RAL_ZAREK:
  case CARD_ID_CHANDRA_PYROMASTER:
  case CARD_ID_GARRUK_CALLER_OF_BEASTS:
  case CARD_ID_ASHIOK_NIGHTMARE_WEAVER:
  case CARD_ID_ELSPETH_SUNS_CHAMPION:
  case CARD_ID_XENAGOS_THE_REVELER:
  case CARD_ID_KIORA_THE_CRASHING_WAVE:
  case CARD_ID_AJANI_MENTOR_OF_HEROES:
  case CARD_ID_DACK_FAYDEN:
  case CARD_ID_AJANI_STEADFAST:
  case CARD_ID_GARRUK_APEX_PREDATOR:
  case CARD_ID_JACE_THE_LIVING_GUILDPACT:
  case CARD_ID_NISSA_WORLDWAKER:
  case CARD_ID_SARKHAN_THE_DRAGONSPEAKER:
  case CARD_ID_SORIN_SOLEMN_VISITOR:
  case CARD_ID_DARETTI_SCRAP_SAVANT:
  case CARD_ID_FREYALISE_LLANOWARS_FURY:
  case CARD_ID_NAHIRI_THE_LITHOMANCER:
  case CARD_ID_OB_NIXILIS_OF_THE_BLACK_OATH:
  case CARD_ID_TEFERI_TEMPORAL_ARCHMAGE:
  case CARD_ID_UGIN_THE_SPIRIT_DRAGON:
  case CARD_ID_NARSET_TRANSCENDENT:
  case CARD_ID_SARKHAN_UNBROKEN:
  case CARD_ID_CHANDRA_ROARING_FLAME:
  case CARD_ID_JACE_TELEPATH_UNBOUND:
  case CARD_ID_GIDEON_BATTLE_FORGED:
  case CARD_ID_LILIANA_DEFIANT_NECROMANCER:
  case CARD_ID_NISSA_SAGE_ANIMIST:
  case CARD_ID_GIDEON_ALLY_OF_ZENDIKAR:
  case CARD_ID_KIORA_MASTER_OF_THE_DEPTHS:
  case CARD_ID_OB_NIXILIS_REIGNITED:
  case CARD_ID_CHANDRA_FLAMECALLER:
  case CARD_ID_NISSA_VOICE_OF_ZENDIKAR:
  case CARD_ID_ARLINN_KORD:
  case CARD_ID_ARLINN_EMBRACED_BY_THE_MOON:
  case CARD_ID_JACE_UNRAVELER_OF_SECRETS:
  case CARD_ID_NAHIRI_THE_HARBINGER:
  case CARD_ID_SORIN_GRIM_NEMESIS:
  case CARD_ID_LILIANA_THE_LAST_HOPE:
  case CARD_ID_TAMIYO_FIELD_RESEARCHER:
  case CARD_ID_CHANDRA_PYROGENIUS:
  case CARD_ID_CHANDRA_TORCH_OF_DEFIANCE:
  case CARD_ID_DARETTI_INGENIOUS_ICONOCLAST:
  case CARD_ID_DOVIN_BAAN:
  case CARD_ID_KAYA_GHOST_ASSASSIN:
  case CARD_ID_NISSA_NATURES_ARTISAN:
  case CARD_ID_NISSA_VITAL_FORCE:
  case CARD_ID_SAHEELI_RAI:
  case CARD_ID_AJANI_UNYIELDING:
  case CARD_ID_AJANI_VALIANT_PROTECTOR:
  case CARD_ID_TEZZERET_THE_SCHEMER:
  case CARD_ID_TEZZERET_MASTER_OF_METAL:
  case CARD_ID_GIDEON_OF_THE_TRIALS:
  case CARD_ID_GIDEON_MARTIAL_PARAGON:
  case CARD_ID_LILIANA_DEATH_WIELDER:
  case CARD_ID_LILIANA_DEATHS_MAJESTY:
  case CARD_ID_NISSA_STEWARD_OF_ELEMENTS:
  case CARD_ID_NICOL_BOLAS_GOD_PHARAOH:
  case CARD_ID_SAMUT_THE_TESTED:
  case CARD_ID_HUATLI_WARRIOR_POET:
  case CARD_ID_VRASKA_RELIC_SEEKER:
  case CARD_ID_ANGRATH_MINOTAUR_PIRATE:
  case CARD_ID_ANGRATH_THE_FLAME_CHAINED:
  case CARD_ID_HUATLI_RADIANT_CHAMPION:
  case CARD_ID_KARN_SCION_OF_URZA:
  case CARD_ID_TEFERI_HERO_OF_DOMINARIA:
  case CARD_ID_TEFERI_TIMEBENDER:
  case CARD_ID_AJANI_ADVERSARY_OF_TYRANTS:
  case CARD_ID_LILIANA_UNTOUCHED_BY_DEATH:
  case CARD_ID_NICOL_BOLAS_THE_ARISEN:
  case CARD_ID_TEZZERET_ARTIFICE_MASTER:
  case CARD_ID_VIVIEN_REID:
  case CARD_ID_AMINATOU_THE_FATESHIFTER:
  case CARD_ID_ESTRID_THE_MASKED:
  case CARD_ID_LORD_WINDGRACE:
  case CARD_ID_SAHEELI_THE_GIFTED:
  case CARD_ID_RAL_IZZET_VICEROY:
  case CARD_ID_VRASKA_GOLGARI_QUEEN:
  case CARD_ID_DOVIN_GRAND_ARBITER:
  case CARD_ID_AJANI_THE_GREATHEARTED:
  case CARD_ID_ANGRATH_CAPTAIN_OF_CHAOS:
  case CARD_ID_ARLINN_VOICE_OF_THE_PACK:
  case CARD_ID_ASHIOK_DREAM_RENDER:
  case CARD_ID_CHANDRA_FIRE_ARTISAN:
  case CARD_ID_DAVRIEL_ROGUE_SHADOWMAGE:
  case CARD_ID_DOMRI_ANARCH_OF_BOLAS:
  case CARD_ID_DOVIN_HAND_OF_CONTROL:
  case CARD_ID_GIDEON_BLACKBLADE:
  case CARD_ID_HUATLI_THE_SUNS_HEART:
  case CARD_ID_JACE_WIELDER_OF_MYSTERIES:
  case CARD_ID_JAYA_VENERATED_FIREMAGE:
  case CARD_ID_JIANG_YANGGU_WILDCRAFTER:
  case CARD_ID_KARN_THE_GREAT_CREATOR:
  case CARD_ID_KAYA_BANE_OF_THE_DEAD:
  case CARD_ID_KIORA_BEHEMOTH_BECKONER:
  case CARD_ID_LILIANA_DREADHORDE_GENERAL:
  case CARD_ID_NAHIRI_STORM_OF_STONE:
  case CARD_ID_NARSET_PARTER_OF_VEILS:
  case CARD_ID_NICOL_BOLAS_DRAGON_GOD:
  case CARD_ID_NISSA_WHO_SHAKES_THE_WORLD:
  case CARD_ID_OB_NIXILIS_THE_HATE_TWISTED:
  case CARD_ID_RAL_STORM_CONDUIT:
  case CARD_ID_SAHEELI_SUBLIME_ARTIFICER:
  case CARD_ID_SAMUT_TYRANT_SMASHER:
  case CARD_ID_SARKHAN_THE_MASTERLESS:
  case CARD_ID_SORIN_VENGEFUL_BLOODLORD:
  case CARD_ID_TAMIYO_COLLECTOR_OF_TALES:
  case CARD_ID_TEFERI_TIME_RAVELER:
  case CARD_ID_TEYO_THE_SHIELDMAGE:
  case CARD_ID_TEZZERET_MASTER_OF_THE_BRIDGE:
  case CARD_ID_THE_WANDERER:
  case CARD_ID_TIBALT_RAKISH_INSTIGATOR:
  case CARD_ID_UGIN_THE_INEFFABLE:
  case CARD_ID_VIVIEN_CHAMPION_OF_THE_WILDS:
  case CARD_ID_VRASKA_SWARMS_EMINENCE:
  case CARD_ID_SERRA_THE_BENEVOLENT:
  case CARD_ID_WRENN_AND_SIX:
  case CARD_ID_AJANI_STRENGTH_OF_THE_PRIDE:
  case CARD_ID_CHANDRA_ACOLYTE_OF_FLAME:
  case CARD_ID_CHANDRA_AWAKENED_INFERNO:
  case CARD_ID_CHANDRA_NOVICE_PYROMANCER:
  case CARD_ID_MU_YANLING_SKY_DANCER:
  case CARD_ID_SORIN_IMPERIOUS_BLOODLORD:
  case CARD_ID_VIVIEN_ARKBOW_RANGER:
  case CARD_ID_GARRUK_CURSED_HUNTSMAN:
  case CARD_ID_OKO_THIEF_OF_CROWNS:
  case CARD_ID_ASHIOK_NIGHTMARE_MUSE:
  case CARD_ID_CALIX_DESTINYS_HAND:
  case CARD_ID_ELSPETH_SUNS_NEMESIS:
  case CARD_ID_LUKKA_COPPERCOAT_OUTCAST:
  case CARD_ID_VIVIEN_MONSTERS_ADVOCATE:
  case CARD_ID_JACE_MIRROR_MAGE:
  case CARD_ID_NAHIRI_HEIR_OF_THE_ANCIENTS:
  case CARD_ID_NISSA_OF_SHADOWED_BOUGHS:
  case CARD_ID_JESKA_THRICE_REBORN:
  case CARD_ID_TEVESH_SZAT_DOOM_OF_FOOLS:
  case CARD_ID_KASMINA_ENIGMA_SAGE:
  case CARD_ID_LUKKA_WAYWARD_BONDER:
  case CARD_ID_PROFESSOR_ONYX:
  case CARD_ID_ROWAN_SCHOLAR_OF_SPARKS:
  case CARD_ID_WILL_SCHOLAR_OF_FROST:
  case CARD_ID_DAKKON_SHADOW_SLAYER:
  case CARD_ID_GEYADRONE_DIHADA:
  case CARD_ID_GRIST_THE_HUNGER_TIDE:
    return COUNTER_LOYALTY;
  case CARD_ID_CHANCE_ENCOUNTER:
  case CARD_ID_GEMSTONE_CAVERNS:
    return COUNTER_LUCK;
  case CARD_ID_MAGNETIC_WEB:
    return COUNTER_MAGNET;
  case CARD_ID_MINE_LAYER:
    return COUNTER_MINE;
  case CARD_ID_GEMSTONE_MINE:
    return COUNTER_MINING;
  case CARD_ID_CYCLOPEAN_TOMB:
    return COUNTER_MIRE;
  case CARD_ID_ASSEMBLE_THE_LEGION:
    return COUNTER_MUSTER;
  case CARD_ID_MERSEINE:
    return COUNTER_NET;
  case CARD_ID_CELESTIAL_CONVERGENCE:
    return COUNTER_OMEN;
  case CARD_ID_ORCISH_MINE:
    return COUNTER_ORE;
  case CARD_ID_BARRINS_CODEX:
  case CARD_ID_PRIVATE_RESEARCH:
    return COUNTER_PAGE;
  case CARD_ID_TORTURE_CHAMBER:
    return COUNTER_PAIN;
  case CARD_ID_DREAD_WIGHT:
    return COUNTER_PARALYZATION;
  case CARD_ID_LOTUS_BLOSSOM:
    return COUNTER_PETAL;
  case CARD_ID_VOODOO_DOLL:
    return COUNTER_PIN;
  case CARD_ID_TRAVELING_PLAGUE:
  case CARD_ID_WITHERING_HEX:
  case CARD_ID_PLAGUE_BOILER:
    return COUNTER_PLAGUE;
  case CARD_ID_CORAL_REEF:
    return COUNTER_POLYP;
  case CARD_ID_MAGMA_MINE:
  case CARD_ID_HELLION_CRUCIBLE:
  case CARD_ID_MOUNT_KERALIA:
    return COUNTER_PRESSURE;
  case CARD_ID_COCOON:
    return COUNTER_PUPA;
  case CARD_ID_ARCHMAGE_ASCENSION:
  case CARD_ID_BEASTMASTER_ASCENSION:
  case CARD_ID_BLOODCHIEF_ASCENSION:
  case CARD_ID_IOR_RUIN_EXPEDITION:
  case CARD_ID_KHALNI_HEART_EXPEDITION:
  case CARD_ID_LUMINARCH_ASCENSION:
  case CARD_ID_PYROMANCER_ASCENSION:
  case CARD_ID_QUEST_FOR_ANCIENT_SECRETS:
  case CARD_ID_QUEST_FOR_PURE_FLAME:
  case CARD_ID_QUEST_FOR_THE_GEMBLADES:
  case CARD_ID_QUEST_FOR_THE_GRAVELORD:
  case CARD_ID_QUEST_FOR_THE_HOLY_RELIC:
  case CARD_ID_SOUL_STAIR_EXPEDITION:
  case CARD_ID_SUNSPRING_EXPEDITION:
  case CARD_ID_ZEKTAR_SHRINE_EXPEDITION:
  case CARD_ID_QUEST_FOR_RENEWAL:
  case CARD_ID_QUEST_FOR_ULAS_TEMPLE:
  case CARD_ID_QUEST_FOR_THE_GOBLIN_LORD:
  case CARD_ID_QUEST_FOR_THE_NIHIL_STONE:
    return COUNTER_QUEST;
  case CARD_ID_ALL_HALLOWS_EVE:
  case CARD_ID_ENDLESS_SCREAM:
    return COUNTER_SCREAM;
  case CARD_ID_ARETOPOLIS:
    return COUNTER_SCROLL;
  case CARD_ID_ROC_HATCHLING:
    return COUNTER_SHELL;
  case CARD_ID_PALLIATION_ACCORD:
    return COUNTER_PALLIATION;
  case CARD_ID_CEPHALID_VANDAL:
    return COUNTER_SHRED;
  case CARD_ID_CHROMATIC_ARMOR:
    return COUNTER_SLEIGHT;
  case CARD_ID_GUTTER_GRIME:
    return COUNTER_SLIME;
  case CARD_ID_SMOKESTACK:
    return COUNTER_SOOT;
  case CARD_ID_ELVISH_FARMER:
  case CARD_ID_FERAL_THALLID:
  case CARD_ID_SPORE_FLOWER:
  case CARD_ID_THALLID:
  case CARD_ID_THALLID_DEVOURER:
  case CARD_ID_THORN_THALLID:
  case CARD_ID_DEATHSPORE_THALLID:
  case CARD_ID_SAVAGE_THALLID:
  case CARD_ID_SPORESOWER_THALLID:
  case CARD_ID_THALLID_GERMINATOR:
  case CARD_ID_THALLID_SHELL_DWELLER:
  case CARD_ID_MYCOLOGIST:
  case CARD_ID_PALLID_MYCODERM:
  case CARD_ID_PSYCHOTROPE_THALLID:
  case CARD_ID_VITASPORE_THALLID:
  case CARD_ID_SPOROLOTH_ANCIENT:
  case CARD_ID_UTOPIA_MYCON:
    return COUNTER_SPORE;
  case CARD_ID_CITY_OF_SHADOWS:
  case CARD_ID_BOTTOMLESS_VAULT:
  case CARD_ID_DWARVEN_HOLD:
  case CARD_ID_HOLLOW_TREES:
  case CARD_ID_ICATIAN_STORE:
  case CARD_ID_SAND_SILOS:
  case CARD_ID_FOUNTAIN_OF_CHO:
  case CARD_ID_MERCADIAN_BAZAAR:
  case CARD_ID_RUSHWOOD_GROVE:
  case CARD_ID_SAPRAZZAN_COVE:
  case CARD_ID_SUBTERRANEAN_HANGAR:
  case CARD_ID_CALCIFORM_POOLS:
  case CARD_ID_DREADSHIP_REEF:
  case CARD_ID_FUNGAL_REACHES:
  case CARD_ID_MOLTEN_SLAGHEAP:
  case CARD_ID_SALTCRUSTED_STEPPE:
  case CARD_ID_CRUCIBLE_OF_THE_SPIRIT_DRAGON:
  case CARD_ID_MAGE_RING_NETWORK:
    return COUNTER_STORAGE;
  case CARD_ID_CRESCENDO_OF_WAR:
    return COUNTER_STRIFE;
  case CARD_ID_PURSUIT_OF_KNOWLEDGE:
  case CARD_ID_GRIMOIRE_OF_THE_DEAD:
    return COUNTER_STUDY;
  case CARD_ID_NIGHT_DEALINGS:
    return COUNTER_THEFT;
  case CARD_ID_HOMARID:
  case CARD_ID_TIDAL_INFLUENCE:
    return COUNTER_TIDE;
  case CARD_ID_TIME_VAULT:
  case CARD_ID_TOURACHS_GATE:
  case CARD_ID_INFINITE_HOURGLASS:
  case CARD_ID_TIME_BOMB:
  case CARD_ID_ANCESTRAL_VISION:
  case CARD_ID_CORPULENT_CORPSE:
  case CARD_ID_CURSE_OF_THE_CABAL:
  case CARD_ID_DEEP_SEA_KRAKEN:
  case CARD_ID_DIVINE_CONGREGATION:
  case CARD_ID_DURKWOOD_BALOTH:
  case CARD_ID_DUSKRIDER_PEREGRINE:
  case CARD_ID_ERRANT_EPHEMERON:
  case CARD_ID_GREATER_GARGADON:
  case CARD_ID_GREATER_GARGADON_SUSPENDED:
  case CARD_ID_HYPERGENESIS:
  case CARD_ID_ITH_HIGH_ARCANIST:
  case CARD_ID_IVORY_GIANT:
  case CARD_ID_KELDON_HALBERDIER:
  case CARD_ID_LIVING_END:
  case CARD_ID_LOTUS_BLOOM:
  case CARD_ID_MINDSTAB:
  case CARD_ID_NANTUKO_SHAMAN:
  case CARD_ID_PARDIC_DRAGON:
  case CARD_ID_PHTHISIS:
  case CARD_ID_PLUNDER:
  case CARD_ID_RESTORE_BALANCE:
  case CARD_ID_RIFT_BOLT:
  case CARD_ID_RIFTWING_CLOUDSKATE:
  case CARD_ID_SEARCH_FOR_TOMORROW:
  case CARD_ID_VISCERID_DEEPWALKER:
  case CARD_ID_WHEEL_OF_FATE:
  case CARD_ID_AEON_CHRONICLER:
  case CARD_ID_AVEN_RIFTWATCHER:
  case CARD_ID_BENALISH_COMMANDER:
  case CARD_ID_CALCIDERM:
  case CARD_ID_CHRONOZOA:
  case CARD_ID_DEADLY_GRUB:
  case CARD_ID_DEADWOOD_TREEFOLK:
  case CARD_ID_DETRITIVORE:
  case CARD_ID_DICHOTOMANCY:
  case CARD_ID_FUNGAL_BEHEMOTH:
  case CARD_ID_GIANT_DUSTWASP:
  case CARD_ID_HEROES_REMEMBERED:
  case CARD_ID_KELDON_MARAUDERS:
  case CARD_ID_LAVACORE_ELEMENTAL:
  case CARD_ID_REALITY_ACID:
  case CARD_ID_RIFTMARKED_KNIGHT:
  case CARD_ID_ROILING_HORROR:
  case CARD_ID_SHADE_OF_TROKAIR:
  case CARD_ID_SHIVAN_METEOR:
  case CARD_ID_TIDEWALKER:
  case CARD_ID_TIMEBENDER:
  case CARD_ID_TIMECRAFTING:
  case CARD_ID_VEILING_ODDITY:
  case CARD_ID_WANING_WURM:
  case CARD_ID_ARC_BLADE:
  case CARD_ID_CHRONOMANTIC_ESCAPE:
  case CARD_ID_CYCLICAL_EVOLUTION:
  case CARD_ID_DUST_OF_MOMENTS:
  case CARD_ID_EPOCHRASITE:
  case CARD_ID_FESTERING_MARCH:
  case CARD_ID_INFILTRATOR_IL_KOR:
  case CARD_ID_KNIGHT_OF_SURSI:
  case CARD_ID_LOST_AURAMANCERS:
  case CARD_ID_MAELSTROM_DJINN:
  case CARD_ID_NIHILITH:
  case CARD_ID_PETRIFIED_PLATING:
  case CARD_ID_RAVAGING_RIFTWURM:
  case CARD_ID_REALITY_STROBE:
  case CARD_ID_SHIVAN_SAND_MAGE:
  case CARD_ID_SOULTETHER_GOLEM:
  case CARD_ID_AS_FORETOLD:
  case CARD_ID_MOX_TANTALITE:
    return COUNTER_TIME;
  case CARD_ID_HELIX_PINNACLE:
    return COUNTER_TOWER;
  case CARD_ID_TRAP_DIGGER:
    return COUNTER_TRAP;
  case CARD_ID_LEGACYS_ALLURE:
    return COUNTER_TREASURE;
  case CARD_ID_DISCORDANT_DIRGE:
  case CARD_ID_LILTING_REFRAIN:
  case CARD_ID_MIDSUMMER_REVEL:
  case CARD_ID_RECANTATION:
  case CARD_ID_RUMBLING_CRESCENDO:
  case CARD_ID_SERRAS_HYMN:
  case CARD_ID_SERRAS_LITURGY:
  case CARD_ID_TORCH_SONG:
  case CARD_ID_VILE_REQUIEM:
  case CARD_ID_WAR_DANCE:
  case CARD_ID_YISAN_THE_WANDERER_BARD:
  case CARD_ID_ARIA_OF_FLAME:
    return COUNTER_VERSE;
  case CARD_ID_LIVING_ARTIFACT:
    return COUNTER_VITALITY;
  case CARD_ID_ROGUE_SKYCAPTAIN:
    return COUNTER_WAGE;
  case CARD_ID_MERCADIAN_LIFT:
    return COUNTER_WINCH;
  case CARD_ID_CYCLONE:
  case CARD_ID_FREYALISES_WINDS:
    return COUNTER_WIND;
  case CARD_ID_DJINN_OF_WISHES:
  case CARD_ID_RING_OF_THREE_WISHES:
  case CARD_ID_WISHCLAW_TALISMAN:
    return COUNTER_WISH;
  case CARD_ID_BLACK_MANA_BATTERY:
  case CARD_ID_BLUE_MANA_BATTERY:
  case CARD_ID_GREEN_MANA_BATTERY:
  case CARD_ID_RED_MANA_BATTERY:
  case CARD_ID_WHITE_MANA_BATTERY:
  case CARD_ID_ICE_CAULDRON:
  case CARD_ID_JEWELED_AMULET:
  case CARD_ID_VENTIFACT_BOTTLE:
  case CARD_ID_BLACK_MARKET:
  case CARD_ID_KYREN_TOY:
  case CARD_ID_MAGISTRATES_SCEPTER:
  case CARD_ID_MANA_CACHE:
  case CARD_ID_RIPTIDE_REPLICATOR:
  case CARD_ID_ALTAR_OF_SHADOWS:
  case CARD_ID_BANSHEES_BLADE:
  case CARD_ID_CHALICE_OF_THE_VOID:
  case CARD_ID_JINXED_CHOKER:
  case CARD_ID_LIGHTNING_COILS:
  case CARD_ID_SERUM_TANK:
  case CARD_ID_SUN_DROPLET:
  case CARD_ID_ARCANE_SPYGLASS:
  case CARD_ID_AETHER_VIAL:
  case CARD_ID_CHIMERIC_EGG:
  case CARD_ID_DARKSTEEL_REACTOR:
  case CARD_ID_MIRRODINS_CORE:
  case CARD_ID_SPAWNING_PIT:
  case CARD_ID_TALON_OF_PAIN:
  case CARD_ID_BATON_OF_COURAGE:
  case CARD_ID_CLEARWATER_GOBLET:
  case CARD_ID_ENGINEERED_EXPLOSIVES:
  case CARD_ID_GEMSTONE_ARRAY:
  case CARD_ID_HELIOPHIAL:
  case CARD_ID_INFUSED_ARROWS:
  case CARD_ID_OPALINE_BRACERS:
  case CARD_ID_PENTAD_PRISM:
  case CARD_ID_OROCHI_HATCHERY:
  case CARD_ID_TENDO_ICE_BRIDGE:
  case CARD_ID_UMEZAWAS_JITTE:
  case CARD_ID_LIGHTNING_STORM:
  case CARD_ID_COALITION_RELIC:
  case CARD_ID_VIVID_CRAG:
  case CARD_ID_VIVID_CREEK:
  case CARD_ID_VIVID_GROVE:
  case CARD_ID_VIVID_MARSH:
  case CARD_ID_VIVID_MEADOW:
  case CARD_ID_DOOR_OF_DESTINIES:
  case CARD_ID_SIGIL_OF_DISTINCTION:
  case CARD_ID_LIGHTNING_REAVER:
  case CARD_ID_ETERNITY_VESSEL:
  case CARD_ID_EVERFLOWING_CHALICE:
  case CARD_ID_ANGELHEART_VIAL:
  case CARD_ID_SPHINX_BONE_WAND:
  case CARD_ID_SURRAKAR_SPELLBLADE:
  case CARD_ID_CHIMERIC_MASS:
  case CARD_ID_CULLING_DAIS:
  case CARD_ID_GOLDEN_URN:
  case CARD_ID_GOLEM_FOUNDRY:
  case CARD_ID_GRINDCLOCK:
  case CARD_ID_LUX_CANNON:
  case CARD_ID_NECROGEN_CENSER:
  case CARD_ID_RATCHET_BOMB:
  case CARD_ID_TRIGON_OF_CORRUPTION:
  case CARD_ID_TRIGON_OF_INFESTATION:
  case CARD_ID_TRIGON_OF_MENDING:
  case CARD_ID_TRIGON_OF_RAGE:
  case CARD_ID_TRIGON_OF_THOUGHT:
  case CARD_ID_TUMBLE_MAGNET:
  case CARD_ID_SHRIEKHORN:
  case CARD_ID_SPHERE_OF_THE_SUNS:
  case CARD_ID_TITAN_FORGE:
  case CARD_ID_CONVERSION_CHAMBER:
  case CARD_ID_SHRINE_OF_BOUNDLESS_GROWTH:
  case CARD_ID_SHRINE_OF_BURNING_RAGE:
  case CARD_ID_SHRINE_OF_LIMITLESS_POWER:
  case CARD_ID_SHRINE_OF_LOYAL_LEGIONS:
  case CARD_ID_SHRINE_OF_PIERCING_VISION:
  case CARD_ID_SURGE_NODE:
  case CARD_ID_DRUIDS_REPOSITORY:
  case CARD_ID_OTHERWORLD_ATLAS:
  case CARD_ID_MANA_BLOOM:
  case CARD_ID_ASTRAL_CORNUCOPIA:
  case CARD_ID_KILNSPIRE_DISTRICT:
  case CARD_ID_PRIMAL_AMULET:
  case CARD_ID_TRANSMOGRIFYING_WAND:
  case CARD_ID_FIREMINDS_RESEARCH:
    return COUNTER_CHARGE;
  case CARD_ID_PRISM_ARRAY:
    return COUNTER_CRYSTAL;
  case CARD_ID_BRIBERS_PURSE:
    return COUNTER_GEM;
  case CARD_ID_QUARANTINE_FIELD:
    return COUNTER_ISOLATION;
  case CARD_ID_CALL_FOR_UNITY:
    return COUNTER_UNITY;
  case CARD_ID_EDIFICE_OF_AUTHORITY:
  case CARD_ID_LUXA_RIVER_SHRINE:
  case CARD_ID_ORACLES_VAULT:
  case CARD_ID_PYRAMID_OF_THE_PANTHEON:
  case CARD_ID_SUNSET_PYRAMID:
    return COUNTER_BRICK;
  case CARD_ID_TREASURE_MAP:
    return COUNTER_LANDMARK;
  case CARD_ID_TETZIMOC_PRIMAL_DEATH:
    return COUNTER_PREY;
  case CARD_ID_ARIXMETHES_SLUMBERING_ISLE:
    return COUNTER_SLUMBER;
  case CARD_ID_THE_MAGIC_MIRROR:
    return COUNTER_KNOWLEDGE;
  case CARD_ID_HELIODS_PUNISHMENT:
    return COUNTER_TASK;
  case CARD_ID_OMINOUS_SEAS:
    return COUNTER_FORESHADOW;
  case CARD_ID_NINE_LIVES:
    return COUNTER_INCARNATION;
  case CARD_ID_NETHERBORN_ALTAR:
  case CARD_ID_MALEFIC_SCYTHE:
    return COUNTER_SOUL;
  default:
    return -1;
  }
}

int get_counter_type_by_instance(card_instance_t* instance)
{
  int iid = instance->internal_card_id;

  if (iid == -1)
	iid = instance->backup_internal_card_id;

  if (iid == activation_card)
	iid = instance->original_internal_card_id;

  int csvid;
  if (iid == LEGACY_EFFECT_CUSTOM)
	{
	  if (instance->info_slot == (intptr_t)generic_suspend_legacy)
		return COUNTER_TIME;
	  else
		csvid = instance->display_pic_csv_id;
	}
  else
	csvid = cards_data[iid].id;

  return get_counter_type_by_id(csvid);
}

void get_counter_name_by_type(char *dest, counter_t counter_type, int num)
{
  if (num <= 0)
    return;
  if ((unsigned int)counter_type > COUNTER_end)
    counter_type = COUNTER_end;
  if (num == 256)
    scnprintf(dest, 100, "%s", counter_names[counter_type]);
  else
    scnprintf(dest, 100, "%s: %d", counter_names[counter_type], num);
}

void get_special_counters_name(char *dest, int csvid, int num)
{
  if (num > 0)
    get_counter_name_by_type(dest, get_counter_type_by_id(csvid), num);
}

// num must be > 0, or no effect

/* num must be > 0, or no effect.  Assumes caller has already called get_updated_counters_number().  This still accounts for putting counters on
 * Melira's Keepers, but not onto effect cards.  Do not call for the latter. */

// unaffected by doubling effects; num may be positive or negative
void add_counters_as_cost(int player, int card, counter_t type, int num)
{
  if (num > 0)
    num = get_updated_counters_number_as_cost(player, card, type, num);
  if (num != 0)
    add_or_remove_counters_impl(player, card, player, card, type, num);
}

// num must be > 0, or no effect

/* For each counter on {src_player, src_card}, add one of that type to {t_player, t_card}.  If raw is set, add the exact amount (even for Melira's Keepers or
 * Melira, Sylvok Outcast, or if Doubling Season etc. are present). */

/* Moves number of counter_type from {src_player, src_card} to {t_player, t_card}.  Moves all counters of all types if counter_type is -1; moves all of
 * counter_type if number is -1. */

static int count_counters_impl(card_instance_t* instance, counter_t type)
{
  if (type == COUNTER_P1_P1)
	return instance->counters_byte;	// Dedicated slot for +1/+1 counters

  if (type == COUNTER_M1_M1)
	return instance->counters_m1m1;	// Dedicated slot for -1/-1 counters

  uint8_t* t18 = (void*)(&instance->targets[18].player);

  int tot;
  if ((unsigned char)type == COUNTER_invalid)	// Total count of all counters
	{
	  tot = (instance->special_counters_byte		// dedicated to a type determined by card's csvid
			 + instance->counters_byte			// dedicated to +1/+1 counters
			 + instance->counters_m1m1);	// dedicated to -1/-1 counters

	  // Dynamically-assigned counter types.
	  // Four visible
	  if (t18[0] != COUNTER_invalid)
		tot += instance->counters2;
	  if (t18[1] != COUNTER_invalid)
		tot += instance->counters3;
	  if (t18[2] != COUNTER_invalid)
		tot += instance->counters4;
	  if (t18[3] != COUNTER_invalid)
		tot += instance->counters5;
	  // Two more invisible
	  if (t18[6] != COUNTER_invalid)
		tot += t18[4];
	  if (t18[7] != COUNTER_invalid)
		tot += t18[5];

	  return tot;
	}

  /* General case.  Be a little extra paranoid in case a card has gotten the same counter type both in special_counters (type determined by csvid) and an
   * assignable counter slot, presumeably by an uncontrolled copy effect. */

  tot = 0;

  if (instance->special_counter_type == type)
	tot += instance->special_counters_byte;

  // Four visible
  if (t18[0] == type)
	tot += instance->counters2;
  if (t18[1] == type)
	tot += instance->counters3;
  if (t18[2] == type)
	tot += instance->counters4;
  if (t18[3] == type)
	tot += instance->counters5;
  // Two more invisible
  if (t18[6] == type)
	tot += t18[4];
  if (t18[7] == type)
	tot += t18[5];

  return tot;
}

// use -1 to get total count
int count_counters(int player, int card, counter_t type)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == activation_card)
	instance = get_card_instance(instance->parent_controller, instance->parent_card);

  return count_counters_impl(instance, type);
}

/* Just like count_counters(), but if called for an activation_card, return the counters that were on th parent card as it was activated, not how many are on it
 * now. */
int count_counters_no_parent(int player, int card, counter_t type)
{
  return count_counters_impl(get_card_instance(player, card), type);
}

// use -1 to remove all of all types

// May use ANYBODY for player and/or -1 for counter_type to indicate any player's cards or any kind of counter.  Returns a total count.
int count_counters_by_counter_and_card_type(int player, counter_t counter_type, type_t type)
{
  int p, c, result = 0;
  for (p = 0; p <= 1; ++p)
	if (p == player || player == ANYBODY)
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, type))
			result += count_counters(p, c, counter_type);

  return result;
}

// May use ANYBODY for player and/or -1 for counter_type to indicate any player's cards or any kind of counter.  Stops looking after the first match.
int has_any_counters(int player, counter_t counter_type, type_t type)
{
  int p, c;
  for (p = 0; p <= 1; ++p)
	if (p == player || player == ANYBODY)
	  for (c = 0; c <= active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, type) && count_counters(p, c, counter_type))
		  return 1;

  return 0;
}

/* The return values here, which should all be between -10000 and 10000, indicate how much the AI values a counter type on a card it controls.  They're inverted
 * if the opponent controls the card.
 *
 * If adding is nonzero, the AI is considering adding another counter to the card (there will always be at least one counter of that type already there);
 * otherwise, it's considering removing a counter of that type, possibly the last.
 *
 * So, for example, COUNTER_EON returns 0 for a non-native card, else 4 if adding, else 9001: this means, the AI doesn't care one way or the other about Eon
 * counters on a Grizzly Bears; is very slightly in favor of adding a second (or third, etc.) Eon counter to a Magosi, the Waterveil it controls; is very
 * slightly against adding a second (or third, etc.) Eon counter to a Magosi, the Waterveil the human player controls; is very, very much against removing an
 * Eon counter from its own Magosi, the Waterveil; and is very, very much in favor of removing an Eon counter from the human player's Magosi, the Waterveil. */
int raw_counter_value(int player, int t_player, int t_card, counter_t counter_type,
                             int adding)
{
  card_instance_t *instance;
  int result;
  int count;
  int limit;
  unsigned int csvid;
  int value;
  int iid;

  instance = get_card_instance(t_player, t_card);
  iid = instance->internal_card_id;
  if (iid < 0)
    iid = instance->backup_internal_card_id;
  csvid = cards_data[iid].id;
  if (adding == 2)
  {
    result = count_counters(t_player, t_card, counter_type);
    adding = (int)(0 < result);
  }
  switch (counter_type)
  {
  case COUNTER_P1_P1:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    switch (csvid)
    {
    case CARD_ID_TETRAVUS:
    case CARD_ID_TRISKELION:
    case CARD_ID_SPIKE_FEEDER:
    case CARD_ID_SPIKE_WEAVER:
    case CARD_ID_WORKHORSE:
    case CARD_ID_MOLTEN_HYDRA:
    case CARD_ID_CANOPY_CRAWLER:
    case CARD_ID_PENTAVUS:
    case CARD_ID_ETCHED_ORACLE:
    case CARD_ID_TWILIGHT_DROVER:
    case CARD_ID_ULASHT_THE_HATE_SEED:
    case CARD_ID_TRISKELAVUS:
    case CARD_ID_ASHLING_THE_PILGRIM:
    case CARD_ID_FERTILID:
    case CARD_ID_FESTERCREEP:
    case CARD_ID_SHINEWEND:
    case CARD_ID_MYCOLOTH:
    case CARD_ID_APOCALYPSE_HYDRA:
    case CARD_ID_DEATHBRINGER_THOCTAR:
    case CARD_ID_JORAGA_WARCALLER:
    case CARD_ID_THOUGHT_GORGER:
    case CARD_ID_ANIMAR_SOUL_OF_ELEMENTS:
    case CARD_ID_VISH_KAL_BLOOD_ARBITER:
    case CARD_ID_MIKAEUS_THE_LUNARCH:
    case CARD_ID_HERALD_OF_WAR:
    case CARD_ID_GYRE_SAGE:
      return 7300;
    default:
      break;
    }
    result = check_special_flags(t_player, t_card, 0x8000);
    value = (result == 0) ? 1200 : 7300;
    break;
  case COUNTER_M1_M1:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -3200;
    break;
  case COUNTER_P0_P1:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 150;
    break;
  case COUNTER_P0_P2:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 300;
    break;
  case COUNTER_P1_P0:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 600;
    break;
  case COUNTER_P1_P2:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 1800;
    break;
  case COUNTER_P2_P0:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 2400;
    break;
  case COUNTER_P2_P2:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 4800;
    break;
  case COUNTER_M0_M1:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -1600;
    break;
  case COUNTER_M0_M2:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -7200;
    break;
  case COUNTER_M1_M0:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -1563;
    break;
  case COUNTER_M2_M1:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -4800;
    break;
  case COUNTER_M2_M2:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = -9600;
    break;
  case COUNTER_AGE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_AGE)
    {
      return 0;
    }
    value = -1000;
    break;
  case COUNTER_AIM:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_AIM)
    {
      return 0;
    }
    value = 125;
    break;
  case COUNTER_ARROW:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ARROW)
    {
      return 0;
    }
    value = 440;
    break;
  case COUNTER_ARROWHEAD:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ARROWHEAD)
    {
      return 0;
    }
    value = 490;
    break;
  case COUNTER_AWAKENING:
    result = is_what(t_player, t_card, TYPE_LAND);
    if (result == 0)
    {
      return 0;
    }
    value = 6;
    break;
  case COUNTER_BLAZE:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_BLAZE)
    {
      value = 52;
    }
    else
    {
      result = is_what(t_player, t_card, TYPE_LAND);
      if (result == 0)
      {
        return 0;
      }
      value = (adding == 0) ? -401 : -4;
    }
    break;
  case COUNTER_BLOOD:
    if (csvid == CARD_ID_BLOODLETTER_QUILL)
    {
      value = -160;
    }
    else
    {
      value = (csvid != CARD_ID_RAKDOS_RITEKNIFE) ? 0 : 8800;
    }
    break;
  case COUNTER_BOUNTY:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    return (adding == 0) ? -601 : -6;
  case COUNTER_BRIBERY:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -701 : -7;
    break;
  case COUNTER_CARRION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CARRION)
    {
      return 0;
    }
    value = 40;
    break;
  case COUNTER_CHIP:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CHIP)
    {
      return 0;
    }
    value = 36;
    break;
  case COUNTER_CORPSE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CORPSE)
    {
      return 0;
    }
    value = 430;
    break;
  case COUNTER_CREDIT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CREDIT)
    {
      return 0;
    }
    value = 38;
    break;
  case COUNTER_CUBE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CUBE)
    {
      return 0;
    }
    value = 47;
    break;
  case COUNTER_CURRENCY:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CURRENCY)
    {
      return 0;
    }
    value = 41;
    break;
  case COUNTER_DEATH:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_DEATH)
    {
      return 0;
    }
    value = (adding == 0) ? -5001 : -5;
    break;
  case COUNTER_DELAY:
    if (csvid == CARD_ID_DELAYING_SHIELD)
    {
      value = -850;
    }
    else
    {
      if (LEGACY_EFFECT_CUSTOM != iid)
      {
        return 0;
      }
      if (instance->display_pic_csv_id != CARD_ID_ERTAIS_MEDDLING)
      {
        return 0;
      }
      value = -850;
    }
    break;
  case COUNTER_DEPLETION:
    if (get_counter_type_by_instance(instance) != COUNTER_DEPLETION)
      return 0;
    switch (csvid)
    {
    case CARD_ID_HICKORY_WOODLOT:
    case CARD_ID_PEAT_BOG:
    case CARD_ID_REMOTE_FARM:
    case CARD_ID_SANDSTONE_NEEDLE:
    case CARD_ID_SAPRAZZAN_SKERRY:
      return 480;
    case CARD_ID_DECREE_OF_SILENCE:
      return -9999;
    case CARD_ID_FORCE_BUBBLE:
      return -540;
    default:
      return -350;
    }
  case COUNTER_DESPAIR:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_DESPAIR)
    {
      return 0;
    }
    value = 16;
    break;
  case COUNTER_DEVOTION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_DEVOTION)
    {
      return 0;
    }
    value = 420;
    break;
  case COUNTER_DIVINITY:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_DIVINITY)
    {
      value = 9600;
    }
    else
    {
      value = (adding == 0) ? 8001 : 5;
    }
    break;
  case COUNTER_DOOM:
    if ((csvid == CARD_ID_ARMAGEDDON_CLOCK) && (life[1 - player] < life[player]))
    {
      value = 13;
    }
    else
    {
      value = (adding == 0) ? -801 : -8;
    }
    break;
  case COUNTER_DREAM:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_DREAM)
    {
      return 0;
    }
    if (((csvid == CARD_ID_RASPUTIN_DREAMWEAVER) && (adding != 0)) &&
        (result = count_counters(t_player, t_card, COUNTER_DREAM), 6 < result))
    {
      return 0;
    }
    value = 410;
    break;
  case COUNTER_ECHO:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ECHO)
    {
      return 0;
    }
    value = 65;
    break;
  case COUNTER_ELIXIR:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ELIXIR)
    {
      return 0;
    }
    value = 44;
    break;
  case COUNTER_VORTEX:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_VORTEX)
    {
      return 0;
    }
    value = 380;
    break;
  case COUNTER_EON:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_EON)
    {
      return 0;
    }
    value = (adding == 0) ? 9001 : 4;
    break;
  case COUNTER_EYEBALL:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_EYEBALL)
    {
      return 0;
    }
    value = 70;
    break;
  case COUNTER_FADE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FADE)
    {
      return 0;
    }
    if (csvid == CARD_ID_PARALLAX_DEMENTIA)
    {
      value = 9200;
      if ((int)instance->damage_target_player == 1 - player)
      {
        value = (t_player != player) ? 9997 : -9997;
      }
    }
    else
    {
      value = 9200;
    }
    break;
  case COUNTER_FATE:
    value = 801;
    if (adding != 0)
      return 0;
    break;
  case COUNTER_FEATHER:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_FEATHER)
    {
      value = 8600;
    }
    else
    {
      value = (adding == 0) ? 701 : 7;
    }
    break;
  case COUNTER_FILIBUSTER:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FILIBUSTER)
    {
      return 0;
    }
    value = 10000;
    break;
  case COUNTER_FLAME:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FLAME)
    {
      return 0;
    }
    value = (t_player != player) ? 650 : -650;
    break;
  case COUNTER_FLOOD:
    if (adding != 0)
    {
      return 0;
    }
    value = -301;
    break;
  case COUNTER_FUNGUS:
    value = -190;
    break;
  case COUNTER_FUNK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FUNK)
    {
      return 0;
    }
    value = 9000;
    break;
  case COUNTER_FUSE:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_FUSE)
    {
      value = 400;
    }
    else
    {
      result = is_what(t_player, t_card, TYPE_CREATURE);
      if (result == 0)
      {
        return 0;
      }
      value = -800;
    }
    break;
  case COUNTER_GLYPH:
    value = -210;
    break;
  case COUNTER_GOLD:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -6001 : -3;
    break;
  case COUNTER_GROWTH:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_GROWTH)
    {
      return 0;
    }
    value = (csvid == CARD_ID_MOMENTUM) ? 480 : 15;
    break;
  case COUNTER_HATCHLING:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HATCHLING)
    {
      return 0;
    }
    value = 7900;
    break;
  case COUNTER_HEALING:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HEALING)
    {
      return 0;
    }
    value = 39;
    break;
  case COUNTER_HOOFPRINT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HOOFPRINT)
    {
      return 0;
    }
    value = 8000;
    break;
  case COUNTER_HOURGLASS:
    if (adding == 0)
    {
      result = is_tapped(t_player, t_card);
      value = (result == 0) ? -9 : -2001;
    }
    else
    {
      value = -9;
    }
    break;
  case COUNTER_HUNGER:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HUNGER)
    {
      return 0;
    }
    value = 510;
    break;
  case COUNTER_HUSK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HUSK)
    {
      return 0;
    }
    value = 8100;
    break;
  case COUNTER_ICE:
    if (csvid == CARD_ID_DARK_DEPTHS)
    {
      value = -330;
    }
    else if (csvid == CARD_ID_WOOLLY_RAZORBACK)
    {
      value = -230;
    }
    else
    {
      value = (csvid == CARD_ID_ICEBERG) ? 80 : -2;
    }
    break;
  case COUNTER_INFECTION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_INFECTION)
    {
      return 0;
    }
    value = 7800;
    break;
  case COUNTER_INTERVENTION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_INTERVENTION)
    {
      return 0;
    }
    value = 24;
    break;
  case COUNTER_JAVELIN:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_JAVELIN)
    {
      return 0;
    }
    value = 701;
    break;
  case COUNTER_KI:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_KI)
    {
      return 0;
    }
    value = 511;
    break;
  case COUNTER_LEVEL:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_LEVEL)
      return 0;
    switch (csvid)
    {
    case CARD_ID_HADA_SPY_PATROL:
    case CARD_ID_NIRKANA_CUTTHROAT:
    case CARD_ID_SKYWATCHER_ADEPT:
    case CARD_ID_ZULAPORT_ENFORCER:
    case CARD_ID_BRIMSTONE_MAGE:
      result = 7500;
      limit = 3;
      break;
    case CARD_ID_IKIRAL_OUTRIDER:
    case CARD_ID_KNIGHT_OF_CLIFFHAVEN:
    case CARD_ID_NULL_CHAMPION:
    case CARD_ID_BEASTBREAKER_OF_BALA_GED:
      result = 7500;
      limit = 4;
      break;
    case CARD_ID_HEDRON_FIELD_PURISTS:
    case CARD_ID_KABIRA_VINDICATOR:
      result = 7500;
      limit = 5;
      break;
    case CARD_ID_ENCLAVE_CRYPTOLOGIST:
      result = 500;
      limit = 3;
      break;
    case CARD_ID_GUUL_DRAZ_ASSASSIN:
    case CARD_ID_CORALHELM_COMMANDER:
    case CARD_ID_ECHO_MAGE:
      result = 500;
      limit = 4;
      break;
    case CARD_ID_HALIMAR_WAVEWATCH:
    case CARD_ID_JORAGA_TREESPEAKER:
    case CARD_ID_CARAVAN_ESCORT:
      result = 500;
      limit = 5;
      break;
    case CARD_ID_KAZANDU_TUSKCALLER:
    case CARD_ID_LORD_OF_SHATTERSKULL_PASS:
      result = 500;
      limit = 6;
      break;
    case CARD_ID_LIGHTHOUSE_CHRONOLOGIST:
    case CARD_ID_STUDENT_OF_WARFARE:
      result = 500;
      limit = 7;
      break;
    case CARD_ID_KARGAN_DRAGONLORD:
      result = 500;
      limit = 8;
      break;
    case CARD_ID_TRANSCENDENT_MASTER:
      result = 500;
      limit = 12;
      break;
    default:
      return 0;
    }
    if (adding == 0)
      return result;
    count = count_counters(t_player, t_card, COUNTER_LEVEL);
    if (limit <= count)
      return 0;
    return result;
  case COUNTER_LORE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_LORE)
    {
      return 0;
    }
    value = 8300;
    break;
  case COUNTER_LOYALTY:
    result = is_what(t_player, t_card, TARGET_TYPE_PLANESWALKER);
    if (result == 0)
    {
      return 0;
    }
    value = 8500;
    break;
  case COUNTER_LUCK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_LUCK)
    {
      return 0;
    }
    if (csvid == CARD_ID_CHANCE_ENCOUNTER)
    {
      value = 9998;
    }
    else
    {
      value = (adding == 0) ? 301 : 3;
    }
    break;
  case COUNTER_MAGNET:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -1001 : -10;
    break;
  default:
    value = 0;
    break;
  case COUNTER_MANNEQUIN:
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -7001 : -11;
    break;
  case COUNTER_MATRIX:
    value = 46;
    break;
  case COUNTER_MINE:
    result = is_what(t_player, t_card, TYPE_LAND);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -7002 : -12;
    break;
  case COUNTER_MINING:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_MINING)
    {
      return 0;
    }
    value = 470;
    break;
  case COUNTER_MIRE:
    result = is_what(t_player, t_card, TYPE_LAND);
    if (result == 0)
    {
      return 0;
    }
    value = (adding == 0) ? -101 : -1;
    break;
  case COUNTER_MUSIC:
    value = -700;
    break;
  case COUNTER_MUSTER:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_MUSTER)
    {
      return 0;
    }
    value = 130;
    break;
  case COUNTER_NET:
    if (csvid != CARD_ID_MERSEINE)
    {
      return 0;
    }
    if ((int)instance->damage_target_player == 1 - player)
    {
      value = (t_player == player) ? 8 : -8;
    }
    else
    {
      if (instance->damage_target_player != player)
      {
        return 0;
      }
      value = (t_player != player) ? 400 : -400;
    }
    break;
  case COUNTER_OMEN:
    if (csvid != CARD_ID_CELESTIAL_CONVERGENCE)
    {
      return 0;
    }
    if (life[1 - player] < life[player])
    {
      value = (t_player == player) ? 9997 : -9997;
    }
    else
    {
      if (life[1 - player] <= life[player])
      {
        return 0;
      }
      value = (t_player != player) ? 10000 : -10000;
    }
    break;
  case COUNTER_ORE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ORE)
    {
      return 0;
    }
    value = -530;
    break;
  case COUNTER_PAGE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PAGE)
    {
      return 0;
    }
    value = 370;
    break;
  case COUNTER_PAIN:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PAIN)
    {
      return 0;
    }
    value = 60;
    break;
  case COUNTER_PARALYZATION:
    value = -250;
    break;
  case COUNTER_PETAL:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PETAL)
    {
      return 0;
    }
    value = 81;
    break;
  case COUNTER_PHYLACTERY:
    value = (adding == 0) ? 8001 : 2;
    break;
  case COUNTER_PIN:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PIN)
    {
      return 0;
    }
    value = -50;
    break;
  case COUNTER_PLAGUE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PLAGUE)
    {
      return 0;
    }
    if (instance->damage_target_player == player)
    {
      value = (t_player != player) ? 100 : -100;
    }
    else if ((1 - player != (int)instance->damage_target_player) ||
             (value = -100, t_player == player))
    {
      value = 100;
    }
    break;
  case COUNTER_POLYP:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_POLYP)
    {
      return 0;
    }
    value = 110;
    break;
  case COUNTER_POP:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_POP)
    {
      return 0;
    }
    value = 35;
    break;
  case COUNTER_PRESSURE:
    if (csvid == CARD_ID_MOUNT_KERALIA)
    {
      value = (t_player == player) ? 360 : -360;
    }
    else
    {
      result = get_counter_type_by_instance(instance);
      if (result != COUNTER_PRESSURE)
      {
        return 0;
      }
      value = 360;
    }
    break;
  case COUNTER_PUPA:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PUPA)
    {
      return 0;
    }
    value = -260;
    break;
  case COUNTER_QUEST:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_QUEST)
      return 0;
    switch (csvid)
    {
    case CARD_ID_IOR_RUIN_EXPEDITION:
    case CARD_ID_KHALNI_HEART_EXPEDITION:
    case CARD_ID_QUEST_FOR_THE_GRAVELORD:
    case CARD_ID_SUNSPRING_EXPEDITION:
    case CARD_ID_ZEKTAR_SHRINE_EXPEDITION:
    case CARD_ID_SOUL_STAIR_EXPEDITION:
      result = 350;
      limit = 3;
      break;
    case CARD_ID_QUEST_FOR_PURE_FLAME:
    case CARD_ID_QUEST_FOR_RENEWAL:
      result = 350;
      limit = 4;
      break;
    case CARD_ID_QUEST_FOR_THE_GEMBLADES:
      result = 350;
      limit = 0;
      break;
    case CARD_ID_QUEST_FOR_THE_HOLY_RELIC:
      result = 350;
      limit = 5;
      break;
    case CARD_ID_QUEST_FOR_THE_NIHIL_STONE:
      result = 350;
      limit = 2;
      break;
    case CARD_ID_BLOODCHIEF_ASCENSION:
    case CARD_ID_QUEST_FOR_ULAS_TEMPLE:
      result = 6000;
      limit = 3;
      break;
    case CARD_ID_LUMINARCH_ASCENSION:
      result = 6000;
      limit = 4;
      break;
    case CARD_ID_PYROMANCER_ASCENSION:
      result = 6000;
      limit = 2;
      break;
    case CARD_ID_BEASTMASTER_ASCENSION:
      result = 6000;
      limit = 7;
      break;
    case CARD_ID_ARCHMAGE_ASCENSION:
      result = 6000;
      limit = 6;
      break;
    case CARD_ID_QUEST_FOR_THE_GOBLIN_LORD:
      result = 6000;
      limit = 5;
      break;
    case CARD_ID_QUEST_FOR_ANCIENT_SECRETS:
      result = 10;
      limit = 5;
      break;
    default:
      return 0;
    }
    if (adding == 0)
      return result;
    /* The native quest-priority path queries level counters. */
    count = count_counters(t_player, t_card, COUNTER_LEVEL);
    if (limit <= count)
      return 0;
    return result;
  case COUNTER_RUST:
    result = is_what(t_player, t_card, TYPE_ARTIFACT);
    if (result == 0)
    {
      return 0;
    }
    value = -600;
    break;
  case COUNTER_SCREAM:
    if ((LEGACY_EFFECT_CUSTOM == iid) && (instance->display_pic_csv_id == CARD_ID_ALL_HALLOWS_EVE))
    {
      value = -500;
    }
    else
    {
      if (csvid != CARD_ID_ENDLESS_SCREAM)
      {
        return 0;
      }
      if (instance->damage_target_player == player)
      {
        value = (t_player == player) ? 120 : -120;
      }
      else
      {
        if ((int)instance->damage_target_player != 1 - player)
        {
          return 0;
        }
        value = (t_player != player) ? 120 : -120;
      }
    }
    break;
  case COUNTER_SCROLL:
    if (csvid != CARD_ID_ARETOPOLIS)
    {
      return 0;
    }
    value = (t_player == player) ? 34 : -34;
    break;
  case COUNTER_SHELL:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SHELL)
    {
      return 0;
    }
    value = -280;
    break;
  case COUNTER_PALLIATION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PALLIATION)
    {
      return 0;
    }
    value = 50;
    break;
  case COUNTER_SHOE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SHOE)
    {
      return 0;
    }
    value = 1400;
    break;
  case COUNTER_SHRED:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SHRED)
    {
      return 0;
    }
    value = 7400;
    break;
  case COUNTER_SLEEP:
    value = -261;
    break;
  case COUNTER_SLEIGHT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SLEIGHT)
    {
      return 0;
    }
    value = -290;
    break;
  case COUNTER_SLIME:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SLIME)
    {
      return 0;
    }
    value = 7700;
    break;
  case COUNTER_SOOT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SOOT)
    {
      return 0;
    }
    value = 14;
    break;
  case COUNTER_SPORE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SPORE)
    {
      return 0;
    }
    value = 550;
    break;
  case COUNTER_STORAGE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_STORAGE)
    {
      return 0;
    }
    value = 560;
    break;
  case COUNTER_STRIFE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_STRIFE)
    {
      return 0;
    }
    value = 320;
    break;
  case COUNTER_STUDY:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_STUDY)
    {
      return 0;
    }
    value = 7600;
    break;
  case COUNTER_THEFT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_THEFT)
    {
      return 0;
    }
    value = 140;
    break;
  case COUNTER_THIRD_DEGREE_BURN:
    if (adding == 0)
    {
      return -0xf;
    }
    return 0;
  case COUNTER_TIDE:
    result = get_counter_type_by_instance(instance);
    if (result == COUNTER_TIDE)
    {
      result = count_counters(t_player, t_card, COUNTER_P1_P1);
      return (result == 0) ? 28 : -300;
    }
    return 0;
  case COUNTER_TIME:
    if (LEGACY_EFFECT_CUSTOM == iid && instance->info_slot == (intptr_t)generic_suspend_legacy)
      return -901;
    if (get_counter_type_by_instance(instance) != COUNTER_TIME)
      return 0;
    switch (csvid)
    {
    case CARD_ID_CHRONOZOA:
    case CARD_ID_DEADLY_GRUB:
    case CARD_ID_REALITY_ACID:
    case CARD_ID_LOST_AURAMANCERS:
      return -150;
    default:
      return 9100;
    }
  case COUNTER_TOWER:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_TOWER)
    {
      return 0;
    }
    value = 30;
    break;
  case COUNTER_TRAP:
    result = is_what(t_player, t_card, TYPE_LAND);
    if (result != 0)
    {
      return (adding == 0) ? 101 : 1;
    }
    return 0;
  case COUNTER_TREASURE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_TREASURE)
    {
      return 0;
    }
    value = 5000;
    break;
  case COUNTER_VELOCITY:
    if (csvid != CARD_ID_TORNADO)
    {
      return 0;
    }
    value = -550;
    break;
  case COUNTER_VERSE:
    if (get_counter_type_by_instance(instance) != COUNTER_VERSE)
      return 0;
    switch (csvid)
    {
    case CARD_ID_MIDSUMMER_REVEL:
    case CARD_ID_RECANTATION:
    case CARD_ID_SERRAS_LITURGY:
    case CARD_ID_VILE_REQUIEM:
      return 7000;
    case CARD_ID_DISCORDANT_DIRGE:
      result = hand_count[1 - t_player];
      limit = count_counters(t_player, t_card, COUNTER_VERSE);
      return result < (int)(limit + (unsigned int)(adding == 0)) ? 7000 : 530;
    default:
      return 530;
    }
  case COUNTER_VITALITY:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_VITALITY)
    {
      return 0;
    }
    value = 37;
    break;
  case COUNTER_WAGE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_WAGE)
    {
      return 0;
    }
    value = -520;
    break;
  case COUNTER_WINCH:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_WINCH)
    {
      return 0;
    }
    value = 310;
    break;
  case COUNTER_WIND:
    if (csvid == CARD_ID_CYCLONE)
    {
      if (life[1 - player] < life[player])
      {
        value = (t_player == player) ? 12 : -12;
      }
      else
      {
        value = (t_player != player) ? 13 : -13;
      }
    }
    else
    {
      value = -13;
    }
    break;
  case COUNTER_WISH:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_WISH)
    {
      return 0;
    }
    value = 540;
    break;
  case COUNTER_CHARGE:
    switch (csvid)
    {
    case CARD_ID_MAGISTRATES_SCEPTER:
    case CARD_ID_RIPTIDE_REPLICATOR:
    case CARD_ID_OROCHI_HATCHERY:
    case CARD_ID_UMEZAWAS_JITTE:
    case CARD_ID_DOOR_OF_DESTINIES:
    case CARD_ID_LIGHTNING_REAVER:
    case CARD_ID_SPHINX_BONE_WAND:
    case CARD_ID_CULLING_DAIS:
    case CARD_ID_GOLEM_FOUNDRY:
    case CARD_ID_LUX_CANNON:
    case CARD_ID_TITAN_FORGE:
    case CARD_ID_CONVERSION_CHAMBER:
    case CARD_ID_SHRINE_OF_LOYAL_LEGIONS:
    case CARD_ID_OTHERWORLD_ATLAS:
      return 6500;
    case CARD_ID_DARKSTEEL_REACTOR:
      return 9999;
    case CARD_ID_KILNSPIRE_DISTRICT:
      return t_player == player ? 450 : -450;
    default:
      if (get_counter_type_by_instance(instance) != COUNTER_CHARGE)
        return 0;
      return 450;
    }
  case COUNTER_CRYSTAL:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_CRYSTAL)
    {
      return 0;
    }
    value = 441;
    break;
  case COUNTER_GEM:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_GEM)
    {
      return 0;
    }
    value = 442;
    break;
  case COUNTER_PLOT:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_PLOT)
    {
      return 0;
    }
    value = 5002;
    break;
  case COUNTER_HOUR:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_HOUR)
    {
      return 0;
    }
    value = 5003;
    break;
  case COUNTER_FURY:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FURY)
    {
      return 0;
    }
    value = 1201;
    break;
  case COUNTER_SPITE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SPITE)
    {
      return 0;
    }
    value = 402;
    break;
  case COUNTER_UNITY:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_UNITY)
    {
      return 0;
    }
    value = 3101;
    break;
  case COUNTER_BRICK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_BRICK)
    {
      return 0;
    }
    value = 316;
    break;
  case COUNTER_GLOBE:
    value = -281;
    break;
  case COUNTER_LANDMARK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_LANDMARK)
    {
      return 0;
    }
    value = 1500;
    break;
  case COUNTER_PREY:
    if (adding != 0)
    {
      return 0;
    }
    value = -3001;
    break;
  case COUNTER_SLUMBER:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_SLUMBER)
    {
      return 0;
    }
    value = -331;
    break;
  case COUNTER_KNOWLEDGE:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_KNOWLEDGE)
    {
      return 0;
    }
    value = 7701;
    break;
  case COUNTER_COIN:
    if (adding != 0)
    {
      return 0;
    }
    if (t_player == player)
    {
      result = is_what(t_player, t_card, TYPE_CREATURE);
      if (result == 0)
      {
        return 0;
      }
      value = 4300;
    }
    else
    {
      result = is_what(t_player, t_card, TYPE_CREATURE);
      if (result == 0)
      {
        return 0;
      }
      value = -4300;
    }
    break;
  case COUNTER_TASK:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_TASK)
    {
      return 0;
    }
    value = 1301;
    break;
  case COUNTER_DEATHTOUCH:
    value = 4107;
    if (adding != 0)
      return 0;
    break;
  case COUNTER_DOUBLE_STRIKE:
    if (adding != 0)
    {
      return 0;
    }
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 4108;
    break;
  case COUNTER_FIRST_STRIKE:
    if (adding != 0)
    {
      return 0;
    }
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 2109;
    break;
  case COUNTER_FLYING:
    if (adding != 0)
    {
      return 0;
    }
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 1808;
    break;
  case COUNTER_HEXPROOF:
    value = 4110;
    if (adding != 0)
      return 0;
    break;
  case COUNTER_INDESTRUCTIBLE:
    value = 6110;
    if (adding != 0)
    {
      return 0;
    }
    break;
  case COUNTER_LIFELINK:
    result = 0x83e;
    if (adding != 0)
      return 0;
    return result;
  case COUNTER_MENACE:
    result = 0x758;
    if (adding != 0)
    {
      return 0;
    }
    return result;
  case COUNTER_REACH:
    if (adding != 0)
    {
      return 0;
    }
    result = is_what(t_player, t_card, TYPE_CREATURE);
    if (result == 0)
    {
      return 0;
    }
    value = 908;
    break;
  case COUNTER_TRAMPLE:
    if (adding == 0)
    {
      result = is_what(t_player, t_card, TYPE_CREATURE);
      if (result != 0)
      {
        return 2107;
      }
      return 0;
    }
    return 0;
  case COUNTER_VIGILANCE:
    if (adding == 0)
    {
      result = is_what(t_player, t_card, TYPE_CREATURE);
      if (result != 0)
      {
        return 2111;
      }
      return 0;
    }
    value = 0;
    break;
  case COUNTER_FORESHADOW:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_FORESHADOW)
    {
      return 0;
    }
    value = 1338;
    break;
  case COUNTER_INCARNATION:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_INCARNATION)
    {
      return 0;
    }
    value = -3338;
    break;
  case COUNTER_SOUL:
    if (csvid == CARD_ID_NETHERBORN_ALTAR)
    {
      value = -3707;
    }
    else
    {
      value = (csvid != CARD_ID_MALEFIC_SCYTHE) ? 0 : 1303;
    }
    break;
  case COUNTER_ACORN:
    result = get_counter_type_by_instance(instance);
    if (result != COUNTER_ACORN)
    {
      return 0;
    }
    value = 7300;
    break;
  case COUNTER_BOOK:
  case COUNTER_COMPONENT:
    value = 5000;
    break;
  case COUNTER_CORRUPTION:
  case COUNTER_HONE:
  case COUNTER_NIGHT:
  case COUNTER_VOW:
    value = -5000;
    break;
  case COUNTER_ENLIGHTENED:
  case COUNTER_HARMONY:
  case COUNTER_VALOR:
  case COUNTER_SHIELD:
    value = 7500;
    break;
  case COUNTER_GHOSTFORM:
    value = 500;
    break;
  case COUNTER_POINT:
    value = 9000;
    break;
  case COUNTER_VOID:
    value = 2000;
    break;
  case COUNTER_VOYAGE:
  case COUNTER_EMBER:
  case COUNTER_SUSPECT:
    value = 4500;
    break;
  case COUNTER_AEGIS:
    value = 1000;
    break;
  case COUNTER_RITUAL:
    value = 3500;
    break;
  case COUNTER_BLOODLINE:
    value = -4500;
    break;
  case COUNTER_CROAK:
  case COUNTER_HATCHING:
    value = 1000;
    break;
  case COUNTER_INVITATION:
    value = -2500;
    break;
  case COUNTER_JUDGMENT:
    value = 2500;
    break;
  case COUNTER_COLLECTION:
    value = 5000;
    break;
  case COUNTER_CONTESTED:
  case COUNTER_STASH:
    value = 3000;
  }
  return value;
}

// Returns COUNTER_invalid if cancelled, or if there are no counters.
counter_t choose_existing_counter_type(int who_chooses, int src_player, int src_card, int t_player, int t_card, cect_t mode, int smallcard_player, int smallcard_card)
{
  STATIC_ASSERT(sizeof(counter_t) >= 2, counter_t_must_be_at_least_two_bytes_wide);

  char choices[10][100];
  int i;
  for (i = 0; i < 10; ++i)
	choices[i][0] = 0;
  counter_t counter_types[10];
  int priorities[10];

  int num_types = 0;

  card_instance_t* instance = get_card_instance(t_player, t_card);
  uint8_t* t18 = (void*)(&instance->targets[18].player);

  struct
  {
	uint8_t num;
	uint8_t typ;
  } counter_data[9] =
	  {
		{ instance->special_counters_byte, instance->special_counter_type },	// Top
		{ instance->counters2, t18[0] },	// Upper right
		{ instance->counters3, t18[1] },	// Upper center
		{ instance->counters4, t18[2] },	// Upper left
		{ instance->counters5, t18[3] },	// Lower right
		{ instance->counters_byte, COUNTER_P1_P1 },		// Lower left
		{ instance->counters_m1m1, COUNTER_M1_M1 },	// Also lower left
		{ t18[4], t18[6] },	// undisplayed 1
		{ t18[5], t18[7] }	// undisplayed 2
	  };

  int add_remove_mode = mode & (CECT_REMOVE | CECT_ADD_OR_REMOVE | CECT_MOVE);
  ASSERT(num_bits_set(add_remove_mode) <= 1);
#define CECT_ADD ((cect_t)(1<<8))
  if (!add_remove_mode)
	add_remove_mode |= CECT_ADD;

  for (i = 0; i < 9; ++i)
	if (counter_data[i].num > 0 && counter_data[i].typ != COUNTER_invalid)
	  {
		counter_types[num_types] = counter_data[i].typ;
		scnprintf(choices[num_types], 100, "%s: %d", counter_names[counter_data[i].typ], counter_data[i].num);

		int pri_add = INT_MIN, pri_remove = INT_MIN;
		if (add_remove_mode & (CECT_ADD | CECT_ADD_OR_REMOVE))	// consider adding a counter to {t_player,t_card}
		  {
			pri_add = raw_counter_value(who_chooses, t_player, t_card, counter_data[i].typ, 1);
			if (who_chooses != t_player)
			  pri_add = -pri_add;
		  }
		if (add_remove_mode & CECT_MOVE)	// consider adding a counter to {smallcard_player,smallcard_card}
		  {
			pri_add = raw_counter_value(who_chooses, smallcard_player, smallcard_card, counter_data[i].typ, 2);
			if (who_chooses != smallcard_player)
			  pri_add = -pri_add;
		  }
		if (add_remove_mode & (CECT_REMOVE | CECT_ADD_OR_REMOVE | CECT_MOVE))	// consider removing a counter from {t_player,t_card}
		  {
			pri_remove = -raw_counter_value(who_chooses, t_player, t_card, counter_data[i].typ, 0);
			if (who_chooses != t_player)
			  pri_remove = -pri_remove;
		  }

		switch (add_remove_mode)
		  {
			case CECT_REMOVE:
			  priorities[num_types] = pri_remove;
			  break;

			case CECT_ADD_OR_REMOVE:
			  if (pri_remove > pri_add)
				{
				  priorities[num_types] = pri_remove;
				  counter_types[num_types] |= (1<<9);
				}
			  else
				priorities[num_types] = pri_add;
			  break;

			case CECT_MOVE:
			  priorities[num_types] = (pri_add - pri_remove) / 2;
			  break;

			default:	// add
			  priorities[num_types] = pri_add;
			  break;
		  }

		if (mode & CECT_CONSIDER_ALL)
		  {
			int pri = priorities[num_types];

			if (pri < 0)
			  pri = (pri / 3) + 3334;	// map raw values of [-10,000..-1] to [1..3,334]
			else if (pri > 0)
			  pri = (99 * pri) + 9999;	// map raw values of [1..10,000] to [10098..999999] (the maximum priority DIALOG() can take and still treat normally)
			else
			  pri = 5775;				// map raw value of 0 to 5,775 == sqrt(3) times as likely as raw value of -1, and 1/sqrt(3) times as likely as raw value of 1

			priorities[num_types] = pri;
		  }

		++num_types;
	  }

  if (num_types == 0)
	{
	  if (!(mode & CECT_AI_CAN_CANCEL))
		cancel = 1;
	  return COUNTER_invalid;
	}

  if (mode & CECT_AUTOCHOOSE_BEST)
	{
	  int best_pri = INT_MIN;
	  counter_t best = COUNTER_invalid;
	  for (i = 0; i < num_types; ++i)
		if (priorities[i] > best_pri)
		  {
			best_pri = priorities[i];
			best = i;
		  }

	  if (best_pri <= 0 && (mode & (IS_AI(who_chooses) ? CECT_HUMAN_CAN_CANCEL : CECT_AI_CAN_CANCEL)))
		{
		  if (!(mode & CECT_AI_CAN_CANCEL))
			cancel = 1;
		  return COUNTER_invalid;
		}
	  else
		return counter_types[best];
	}

  if (mode & CECT_AI_CAN_CANCEL)
	{
	  mode &= ~CECT_HUMAN_CAN_CANCEL;	// We'll be displaying a cancel option with this, so suppress the automatic one
	  strcpy(choices[num_types], gs_localized_cancel);	// localized Cancel
	  counter_types[num_types] = COUNTER_invalid;

	  if (mode & CECT_CONSIDER_ALL)
		priorities[num_types] = 5775;	// same as a raw value of 0 would get
	  else
		priorities[num_types] = 1;
	}

#undef ADD_TYPE

  const char* prompt;
  if (mode & CECT_MOVE)
	prompt = "Move a counter";
  else if (mode & CECT_ADD_OR_REMOVE)
	prompt = "Add or remove a counter";
  else if (mode & CECT_REMOVE)
	prompt = "Remove a counter";
  else
	prompt = "Add a counter";

  int choice;

#define ADD_CHOICE(x)	choices[x][0] ? choices[x] : NULL, 1, priorities[x]
#define ADD_CHOICES		ADD_CHOICE(0), ADD_CHOICE(1), ADD_CHOICE(2), ADD_CHOICE(3), ADD_CHOICE(4), ADD_CHOICE(5), ADD_CHOICE(6), ADD_CHOICE(7), ADD_CHOICE(8), ADD_CHOICE(9)

  // We avoid DLG_OMIT_ILLEGAL by making sure we pass NULL when we run out of choices, and not having any valid choices after invalid ones.
  choice = DIALOG(src_player, src_card, EVENT_ACTIVATE,
				  DLG_FULLCARD(t_player, t_card), DLG_SMALLCARD(smallcard_player, smallcard_card),
				  DLG_HEADER(prompt), DLG_WHO_CHOOSES(who_chooses), DLG_NO_STORAGE, DLG_RANDOM,
				  (mode & CECT_HUMAN_CAN_CANCEL ? DLG_NO_OP : DLG_NO_CANCEL),
				  (mode & CECT_AI_NO_DIALOG ? DLG_NO_DISPLAY_FOR_AI : DLG_NO_OP),
				  ADD_CHOICES);

#undef ADD_CHOICE
#undef ADD_CHOICES

  if (choice == 0 || counter_types[choice - 1] == COUNTER_invalid)
	{
	  if (!(mode & CECT_AI_CAN_CANCEL))
		cancel = 1;
	  return COUNTER_invalid;
	}
  else
	return counter_types[choice - 1];
}

void poison(int player, int number_of_counters)
{
  if (number_of_counters <= 0)
    return;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *instance = in_play(p, c);
      if (!instance)
        continue;
      int csvid = cards_data[instance->internal_card_id].id;
      if (csvid == CARD_ID_WINDING_CONSTRICTOR)
        number_of_counters += player == p;
      else if (csvid == CARD_ID_SOLEMNITY)
      {
        return;
      }
    }
  int total = POISON_COUNTERS(player) + number_of_counters;
  if (total > 255)
    total = 255;
  SET_POISON_COUNTERS(player, total);
  if (number_of_counters > 0)
    play_sound_effect(WAV_POISON);
}

void raw_set_poison(int player, uint8_t new_number_of_counters)
{
  //if (ai_is_speculating == 1 && new_number_of_counters > player_counters[player].poison_)
  //  {
  //    player_counters[player].poison_ = new_number_of_counters;
  //    check_loss_of_game(false);
  //  }
  //else
  SET_POISON_COUNTERS(player, new_number_of_counters);
}

void add_experience(int player, int number_of_counters)
{
  if (number_of_counters <= 0)
    return;
  int total;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *instance = in_play(p, c);
      if (!instance)
        continue;
      int csvid = cards_data[instance->internal_card_id].id;
      if (csvid == CARD_ID_WINDING_CONSTRICTOR)
        number_of_counters += player == p;
      else if (csvid == CARD_ID_SOLEMNITY)
      {
        number_of_counters = 0;
        goto AFTER_SCAN;
      }
    }
AFTER_SCAN:
  total = player_counters[player].experience_ + number_of_counters;
  if (total > 255)
    total = 255;
  player_counters[player].experience_ = total;
  if (ai_is_speculating != 1)
    play_sound_effect(WAV_COUNTER);
}

void raw_set_experience(int player, uint8_t new_number_of_counters)
{
  player_counters[player].experience_ = new_number_of_counters;
}

void add_energy(int player, int number_of_counters)
{
  if (number_of_counters <= 0)
    return;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *instance = in_play(p, c);
      if (!instance)
        continue;
      int csvid = cards_data[instance->internal_card_id].id;
      if (csvid == CARD_ID_WINDING_CONSTRICTOR)
        number_of_counters += player == p;
      else if (csvid == CARD_ID_SOLEMNITY)
      {
        return;
      }
    }
  int total = player_counters[player].energy_ + number_of_counters;
  if (total > 255)
    total = 255;
  player_counters[player].energy_ = total;
  if (number_of_counters > 0)
  {
    play_sound_effect(WAV_ENERGY_PLUS);
    if (enable_xtrigger_flags & XTRIGGER_COUNTER_ENERGY)
    {
      int old_counters_added = counters_added;
      counters_added = number_of_counters;
      dispatch_xtrigger2(player, XTRIGGER_COUNTER_ENERGY,
                         number_of_counters > 1 ? "energy counters" : "energy counter", 0, player,
                         -1);
      counters_added = old_counters_added;
    }
  }
}

void remove_energy(int player, int number_of_counters)
{
  if (number_of_counters <= 0 || player_counters[player].energy_ == 0)
    return;
  int total = player_counters[player].energy_ - number_of_counters;
  if (total < 0)
    total = 0;
  player_counters[player].energy_ = total;
  play_sound_effect(WAV_ENERGY_MINUS);
}

int count_player_counters(int player, player_counter_names_t type){
	switch( type ){
			case PCOUNTER_ENERGY:
				return ENERGY_COUNTERS(player);
			case PCOUNTER_POISON:
				return POISON_COUNTERS(player);
			case PCOUNTER_EXPERIENCE:
				return EXPERIENCE_COUNTERS(player);
			case PCOUNTER_invalid:
				return ENERGY_COUNTERS(player)+POISON_COUNTERS(player)+EXPERIENCE_COUNTERS(player);
	}
	return 0;
}

void add_player_counters(int player, player_counter_names_t type, int amount){
	switch( type ){
			case PCOUNTER_ENERGY:
				add_energy(player, amount);
				break;
			case PCOUNTER_POISON:
				poison(player, amount);
				break;
			case PCOUNTER_EXPERIENCE:
				add_experience(player, amount);
				break;
			case PCOUNTER_invalid:
				ASSERT( 0 && "PCOUNTER_invalid not handled by 'add_player_counters'.");
				break;
	}
}

void remove_player_counters(int player, player_counter_names_t type, int amount){
	switch( type ){
			case PCOUNTER_ENERGY:
				remove_energy(player, amount);
				break;
			case PCOUNTER_POISON:
				ASSERT( 0 && "you can't remove poison counters via 'remove_player_counters'");
				break;
			case PCOUNTER_EXPERIENCE:
				ASSERT( 0 && "you can't remove experience counters via 'remove_player_counters'");
				break;
			case PCOUNTER_invalid:
				ASSERT( 0 && "PCOUNTER_invalid not handled by 'remove_player_counters'.");
				break;
	}
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int can_have_counters_placed_on_it(int player, int card);

/* ManalinkEh 2022: 0x26d44d0; original functions/counters.c:2691. */
int can_have_counters_placed_on_it(int player, int card)
{
  if (card < 0)
    return 1;
  switch (get_id(player, card))
  {
  case CARD_ID_TATTERKITE:
  case CARD_ID_MELIRAS_KEEPERS:
    return is_humiliated(player, card, AL_STATIC) != 0;
  default:
    return 1;
  }
}

/* Counter operations carry source and recipient pairs and use the shared
 * native trigger IDs and enable masks. */
int counters_added_type = -1; /* Target .data 0x02721d68, counters.c:7. */

/* ManalinkEh 2022: 0x26d4540; original functions/counters.c:919. */
static int get_updated_counters_number_as_cost(int player, int card, counter_t type, int number)
{
  if (!can_have_counters_placed_on_it(player, card))
    return 0;
  if (type != COUNTER_M1_M1 && type != COUNTER_P1_P1)
    return number;
  if (card >= 0 && !is_what(player, card, TYPE_CREATURE))
    return number;
  int blocker_controller = type == COUNTER_M1_M1 ? player : 1 - player;
  int blocker_id = type == COUNTER_M1_M1 ? CARD_ID_MELIRA_SYLVOK_OUTCAST : CARD_ID_BLIGHTBEETLE;
  return check_battlefield_for_id(blocker_controller, blocker_id) ? 0 : number;
}

/* ManalinkEh 2022: 0x26d45f0; original functions/counters.c:933. */
static int get_updated_counters_number_impl(int player, int card, counter_t type, int number)
{
  number = get_updated_counters_number_as_cost(player, card, type, number);
  if (number == 0 || (card >= 0 && is_what(player, card, TYPE_EFFECT)))
    return number;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
    {
      card_instance_t *instance = in_play(p, c);
      if (instance == NULL)
        continue;
      int csvid = cards_data[instance->internal_card_id].id;
      switch (csvid)
      {
      case CARD_ID_WINDING_CONSTRICTOR:
        if (player == p)
          number += is_what(player, card, TYPE_CREATURE | TYPE_ARTIFACT) != 0;
        break;
      case CARD_ID_PRIMAL_VIGOR:
        if (type == COUNTER_P1_P1)
          number *= 2;
        break;
      case CARD_ID_DOUBLING_SEASON:
        if (player == p)
          number *= 2;
        break;
      case CARD_ID_CORPSEJACK_MENACE:
      case CARD_ID_BRANCHING_EVOLUTION:
        if (player == p && is_what(player, card, TYPE_CREATURE))
          if (type == COUNTER_P1_P1)
            number *= 2;
        break;
      case CARD_ID_HARDENED_SCALES:
        if (player == p && type == COUNTER_P1_P1)
          ++number;
        break;
      case CARD_ID_SELESNYA_LOFT_GARDENS:
        number *= 2;
        break;
      case CARD_ID_PIR_IMAGINATIVE_RASCAL:
        number += player == p;
        break;
      case CARD_ID_VIZIER_OF_REMEDIES:
        if (player == p && type == COUNTER_M1_M1)
          number -= is_what(player, card, TYPE_CREATURE) != 0;
        break;
      case CARD_ID_SOLEMNITY:
        if (is_what(p, card, TYPE_PERMANENT))
          number -= 99;
        break;
      case CARD_ID_MOWU_LOYAL_COMPANION:
        if (card == c && player == p && type == COUNTER_P1_P1)
          ++number;
        break;
      }
    }
  return number;
}

/* ManalinkEh 2022: 0x26d4810; original functions/counters.c:933. */
int get_updated_counters_number(int player, int card, counter_t type, int number)
{
  if (number > 0)
    return get_updated_counters_number_impl(player, card, type, number);
  return number;
}

/* ManalinkEh 2022: 0x26d0e90; original functions/counters.c:1012. */
static void add_or_remove_counters_impl(int player, int card, int t_player, int t_card,
    counter_t type, int num)
{
  int saved_added = counters_added;
  int saved_type = counters_added_type;
  if (num > 0 && ((uint8_t)enable_xtrigger_flags & ENABLE_XTRIGGER_COUNTERS))
  {
    counters_added = num;
    counters_added_type = type;
    dispatch_xtrigger2(t_player, XTRIGGER_REPLACE_COUNTERS,
        num == 1 ? "counter" : "counters", 0, t_player, t_card);
  }
  counters_added_type = saved_type;
  counters_added = saved_added;
  card_instance_t *instance = get_card_instance(t_player, t_card);
  uint8_t *labels = (uint8_t *)&instance->targets[18].player;
  uint8_t *overflow = (uint8_t *)&instance->targets[18].card;
  uint8_t *position = NULL;
  uint8_t *label = NULL;
  int slot = -1;
  int power = 0;
  int toughness = 0;
  switch (type)
  {
  case COUNTER_P1_P1: position = &instance->counters_byte; power = toughness = 1; break;
  case COUNTER_M1_M1: position = &instance->counters_m1m1; power = toughness = -1; break;
  case COUNTER_P0_P1: toughness = 1; break;
  case COUNTER_P0_P2: toughness = 2; break;
  case COUNTER_P1_P0: power = 1; break;
  case COUNTER_P1_P2: power = 1; toughness = 2; break;
  case COUNTER_P2_P0: power = 2; break;
  case COUNTER_P2_P2: power = toughness = 2; break;
  case COUNTER_M0_M1: toughness = -1; break;
  case COUNTER_M0_M2: toughness = -2; break;
  case COUNTER_M1_M0: power = -1; break;
  case COUNTER_M2_M1: power = -2; toughness = -1; break;
  case COUNTER_M2_M2: power = toughness = -2; break;
  default: break;
  }
  if (position == NULL)
  {
    uint8_t *counts[7] = {&instance->special_counters_byte, &instance->counters2,
        &instance->counters3, &instance->counters4, &instance->counters5,
        &overflow[0], &overflow[1]};
    uint8_t *types[7] = {&instance->special_counter_type, &labels[0], &labels[1],
        &labels[2], &labels[3], &overflow[2], &overflow[3]};
    for (int i = 0; i < 7; ++i)
      if (type == *types[i])
      {
        if (*counts[i] != 0)
        {
          slot = i;
          break;
        }
        *types[i] = COUNTER_invalid;
      }
    if (slot == -1)
    {
      if (num < 0)
        return;
      int special_count = instance->special_counters_byte;
      if (special_count == 0 && type == get_counter_type_by_instance(instance))
        slot = 0;
      else
      {
        for (int i = 1; i < 5; ++i)
          if (*counts[i] == 0)
          {
            slot = i;
            break;
          }
        if (slot == -1)
        {
          if (special_count == 0)
            slot = 0;
          else if (overflow[2] == COUNTER_invalid)
            slot = 5;
          else if (overflow[3] == COUNTER_invalid)
            slot = 6;
        }
      }
    }
    if (slot != -1)
    {
      position = counts[slot];
      label = types[slot];
      if (*label == COUNTER_invalid)
        *position = 0;
    }
  }
  if (position != NULL)
  {
    unsigned int count = *position;
    int32_t sum = (int32_t)(count + (uint32_t)num);
    if (num > 0)
    {
      if (sum >= 256)
        num = 255 - count;
      *position = (uint8_t)(count + (uint32_t)num);
      if (label != NULL)
        *label = (uint8_t)type;
    }
    else
    {
      if (num < 0 && sum < 0)
        num = -(int)count;
      *position = (uint8_t)(count + (uint32_t)num);
      if (label != NULL && *position == 0)
      {
        *label = COUNTER_invalid;
        if (slot < 5)
        {
          if (overflow[0] != 0 && overflow[2] != COUNTER_invalid)
          {
            *position = overflow[0];
            *label = overflow[2];
            overflow[0] = 0;
            overflow[2] = COUNTER_invalid;
          }
          else if (overflow[1] != 0 && overflow[3] != COUNTER_invalid)
          {
            *position = overflow[1];
            *label = overflow[3];
            overflow[1] = 0;
            overflow[3] = COUNTER_invalid;
          }
        }
      }
    }
  }
  if (num != 0)
  {
    if (power != 0)
    {
      instance->counter_power += (int16_t)power * (int16_t)num;
      instance->regen_status |= KEYWORD_RECALC_POWER;
    }
    if (toughness != 0)
    {
      uint16_t old_toughness = instance->counter_toughness;
      instance->regen_status |= KEYWORD_RECALC_TOUGHNESS;
      instance->counter_toughness = old_toughness + (int16_t)toughness * (int16_t)num;
    }
  }
  if (num > 0 && hack_silent_counters < 1)
    play_sound_effect(WAV_COUNTER);
  saved_added = counters_added;
  saved_type = counters_added_type;
  counters_added_type = type;
  counters_added = num;
  dispatch_event_with_attacker(t_player, t_card, EVENT_COUNTERS, player, card);
  counters_added_type = saved_type;
  counters_added = saved_added;
  if (num > 0 && type == COUNTER_P1_P1)
  {
    if (is_what(t_player, t_card, TYPE_PERMANENT))
      set_trap_condition(t_player, TRAP_P1_P1_COUNTER_PLACED, 1);
    saved_added = counters_added;
    if (!((uint8_t)enable_xtrigger_flags & ENABLE_XTRIGGER_1_1_COUNTERS))
      return;
    counters_added = num;
    dispatch_xtrigger2(t_player, XTRIGGER_1_1_COUNTERS,
        num == 1 ? "+1/+1 counter" : "+1/+1 counters", 0, t_player, t_card);
    counters_added = saved_added;
    return;
  }
  saved_added = counters_added;
  if (num > 0 && type == COUNTER_M1_M1 &&
      ((uint8_t)enable_xtrigger_flags & ENABLE_XTRIGGER_M1_M1_COUNTERS))
  {
    counters_added = num;
    dispatch_xtrigger2(t_player, XTRIGGER_M1_M1_COUNTERS,
        num == 1 ? "-1/-1 counter" : "-1/-1 counters", 0, t_player, t_card);
  }
  counters_added = saved_added;
}

/* ManalinkEh 2022: 0x26d4840; original functions/counters.c:1266. */
void add_counters(int player, int card, int t_player, int t_card, counter_t type, int num)
{
  card_instance_t *instance = get_card_instance(t_player, t_card);
  if (instance->internal_card_id == activation_card)
  {
    t_player = instance->parent_controller;
    t_card = instance->parent_card;
  }
  if (num > 0)
  {
    num = get_updated_counters_number(t_player, t_card, type, num);
    if (num > 0)
      add_or_remove_counters_impl(player, card, t_player, t_card, type, num);
  }
}

/* Recovered October 2022 source-aware counter removal. */

/* ManalinkEh 2022: 0x26d5970; original functions/counters.c:1281. */
void add_counters_predoubled(int player, int card, int t_player, int t_card, counter_t type, int num)
{
  if (is_what(t_player, t_card, TYPE_EFFECT))
    backtrace_and_abort("functions/counters.c:1283\n!is_what(t_player, t_card, TYPE_EFFECT)");
  if (can_have_counters_placed_on_it(t_player, t_card) && num != 0)
    add_or_remove_counters_impl(player, card, t_player, t_card, type, num);
}

/* ManalinkEh 2022: 0x26d48d0; original functions/counters.c:1304. */
static void remove_counters_impl(int player, int card, int t_player, int t_card,
    counter_t type, int num)
{
  card_instance_t *instance = get_card_instance(t_player, t_card);
  target_definition_t td;
  token_generation_t token;
  if (instance->internal_card_id == activation_card)
  {
    t_player = instance->parent_controller;
    t_card = instance->parent_card;
  }
  int removed = count_counters(t_player, t_card, type);
  add_or_remove_counters_impl(player, card, t_player, t_card, type, -num);
  if (type != COUNTER_TIME || !is_suspend_legacy(t_player, t_card))
    return;
  if (num < removed)
    removed = num;
  int suspended_card = instance->targets[9].card;
  if (suspended_card == CARD_ID_AEON_CHRONICLER)
  {
    draw_cards(t_player, removed);
    suspended_card = instance->targets[9].card;
  }
  if (suspended_card == CARD_ID_BENALISH_COMMANDER)
  {
    default_token_definition(t_player, t_card, CARD_ID_SOLDIER, &token);
    token.qty = (uint8_t)removed;
    generate_token(&token);
    suspended_card = instance->targets[9].card;
  }
  if (suspended_card == CARD_ID_DETRITIVORE)
  {
    default_target_definition(t_player, t_card, &td, TYPE_LAND);
    td.required_subtype = SUBTYPE_BASIC;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
    td.allow_cancel = 0;
    for (int i = 0; i < removed; ++i)
    {
      instance->number_of_targets = 0;
      if (can_target(&td) && new_pick_target(&td, "Select target nonbasic land.", 0, 0x80))
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    suspended_card = instance->targets[9].card;
  }
  if (suspended_card == CARD_ID_FUNGAL_BEHEMOTH)
  {
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = player;
    for (int i = 0; i < removed; ++i)
    {
      instance->number_of_targets = 0;
      if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
        add_counters(t_player, t_card, instance->targets[0].player,
            instance->targets[0].card, COUNTER_P1_P1, 1);
    }
    suspended_card = instance->targets[9].card;
  }
  if (suspended_card == CARD_ID_ROILING_HORROR)
  {
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.zone = TARGET_ZONE_PLAYERS;
    td.allow_cancel = 0;
    for (int i = 0; i < removed; ++i)
    {
      instance->number_of_targets = 0;
      if (can_target(&td) && pick_target(&td, "TARGET_PLAYER"))
      {
        lose_life(instance->targets[0].player, 1);
        gain_life(player, 1);
      }
    }
  }
}

/* ManalinkEh 2022: 0x26d4c10; original functions/counters.c:1304. */
void remove_counters(int player, int card, int t_player, int t_card, counter_t type, int num)
{
  if (num > 0)
    remove_counters_impl(player, card, t_player, t_card, type, num);
}

/* ManalinkEh 2022: 0x26d4ca0; original functions/counters.c:1385. */
void copy_counters(int player, int card, int t_player, int t_card, int src_player,
    int src_card, int raw)
{
  card_instance_t *instance = get_card_instance(src_player, src_card);
  if (instance->internal_card_id == activation_card)
    instance = get_card_instance(instance->parent_controller, instance->parent_card);
  card_instance_t *target = get_card_instance(t_player, t_card);
  if (target->internal_card_id == activation_card)
  {
    t_player = target->parent_controller;
    t_card = target->parent_card;
  }
  counter_snapshot_t counters[9];
  int count = snapshot_counters(instance, counters);
  for (int i = 0; i < count; ++i)
    if (raw)
      add_or_remove_counters_impl(player, card, t_player, t_card, counters[i].type, counters[i].num);
    else
      add_counters(player, card, t_player, t_card, counters[i].type, counters[i].num);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void quarry_hauler_helper(int player, int card, int t_player, int t_card);

/* ManalinkEh 2022: 0x26ceef0; original functions/counters.c:1630. */
static void clear_all_counters(int player, int card, int t_player, int t_card, counter_t type)
{
  card_instance_t *instance = get_card_instance(t_player, t_card);
  unsigned int previous_status = instance->regen_status;
  instance->special_counter_type = COUNTER_invalid;
  instance->special_counters_byte = 0;
  instance->counters2 = 0;
  instance->counters3 = 0;
  instance->counters4 = 0;
  instance->counters_byte = 0;
  instance->counters5 = 0;
  instance->counters_m1m1 = 0;
  instance->targets[18].player = -1;
  /* Two empty hidden slots, each with an invalid counter-type byte. */
  instance->targets[18].card = (int32_t)0xffff0000U;
  instance->counter_power = 0;
  instance->counter_toughness = 0;
  instance->regen_status = previous_status | KEYWORD_RECALC_POWER | KEYWORD_RECALC_TOUGHNESS;
}

void copy_counters(int player, int card, int t_player, int t_card, int src_player,
                               int src_card, int raw);

/* ManalinkEh 2022: 0x26d5a00; original functions/counters.c:1437. */
void move_counters(int player, int card, int t_player, int t_card, int src_player,
    int src_card, counter_t counter_type, int number)
{
  if ((t_player == src_player && t_card == src_card) ||
      !can_have_counters_placed_on_it(t_player, t_card))
    return;
  if ((uint8_t)counter_type == COUNTER_invalid)
  {
    if (check_battlefield_for_id(t_player, CARD_ID_MELIRA_SYLVOK_OUTCAST))
    {
      card_instance_t *instance = get_card_instance(src_player, src_card);
      uint8_t minus_counters = instance->counters_m1m1;
      instance->counters_m1m1 = 0;
      ++hack_silent_counters;
      copy_counters(player, card, t_player, t_card, src_player, src_card, 1);
      clear_all_counters(player, card, src_player, src_card, (counter_t)-1);
      --hack_silent_counters;
      instance->counters_m1m1 = minus_counters;
    }
    if (!check_battlefield_for_id(1 - t_player, CARD_ID_BLIGHTBEETLE))
    {
      ++hack_silent_counters;
      copy_counters(player, card, t_player, t_card, src_player, src_card, 1);
      clear_all_counters(player, card, src_player, src_card, (counter_t)-1);
      --hack_silent_counters;
    }
    else
    {
      int plus_counters = count_counters(src_player, src_card, COUNTER_P1_P1);
      remove_counters_impl(player, card, src_player, src_card, COUNTER_P1_P1, 255);
      ++hack_silent_counters;
      copy_counters(player, card, t_player, t_card, src_player, src_card, 1);
      clear_all_counters(player, card, src_player, src_card, (counter_t)-1);
      add_counters(player, card, src_player, src_card, COUNTER_P1_P1, plus_counters);
      --hack_silent_counters;
    }
    return;
  }
  int melira = check_battlefield_for_id(t_player, CARD_ID_MELIRA_SYLVOK_OUTCAST);
  if (counter_type == COUNTER_M1_M1 && melira)
    return;
  int blightbeetle = check_battlefield_for_id(1 - t_player, CARD_ID_BLIGHTBEETLE);
  if (counter_type == COUNTER_P1_P1 && blightbeetle)
    return;
  if (number < 0)
    number = 255;
  int available = count_counters(src_player, src_card, counter_type);
  if (available < number)
    number = available;
  if (number > 0)
    remove_counters_impl(player, card, src_player, src_card, counter_type, number);
  add_counters_predoubled(player, card, t_player, t_card, counter_type, number);
}

/* ManalinkEh 2022: 0x26d4c50; original functions/counters.c:1630. */
void remove_all_counters(int player, int card, int t_player, int t_card, counter_t type)
{
  if ((uint8_t)type == COUNTER_invalid)
    clear_all_counters(player, card, t_player, t_card, type);
  else
    remove_counters_impl(player, card, t_player, t_card, type, 255);
}

/* ManalinkEh 2022: 0x26d51e0; original functions/counters.c:2412. */
void quarry_hauler_helper(int player, int card, int t_player, int t_card)
{
  enum
  {
    CHOICE_ADD = 1,
    CHOICE_REMOVE = 2
  };

  card_instance_t *instance = get_card_instance(t_player, t_card);
  if (instance->internal_card_id == activation_card)
    instance = get_card_instance(instance->parent_controller, instance->parent_card);
  const uint8_t *t18 = (const uint8_t *)&instance->targets[18].player;
  const uint8_t *overflow = (const uint8_t *)&instance->targets[18].card;
  const uint8_t quantities[9] = {instance->special_counters_byte, instance->counters2,
      instance->counters3, instance->counters4, instance->counters5, instance->counters_byte,
      instance->counters_m1m1, overflow[0], overflow[1]};
  const counter_t types[9] = {instance->special_counter_type, t18[0], t18[1],
      t18[2], t18[3], COUNTER_P1_P1, COUNTER_M1_M1, overflow[2], overflow[3]};
  counter_t counter_types[9];
  int num_types = 0;
  for (int slot = 0; slot < 9; ++slot)
    if (quantities[slot] != 0)
    {
      counter_types[num_types] = types[slot];
      if (types[slot] != COUNTER_invalid)
        ++num_types;
    }
  if (num_types == 0)
    return;
  int choices[9] = {0};
  int to_choose = num_types;
  int i;
  char ctr_names[9][100];
  for (i = 0; i < num_types; ++i)
    get_counter_name_by_type(ctr_names[i], counter_types[i], 256);
  while (to_choose > 0)
  {
    int choice1 = DIALOG(player, card, EVENT_RESOLVE_ACTIVATION,
        DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL,
        DLG_FULLCARD(t_player, t_card), DLG_NO_DISPLAY_FOR_AI,
        ctr_names[0], num_types > 0 && choices[0] == 0, 1,
        ctr_names[1], num_types > 1 && choices[1] == 0, 1,
        ctr_names[2], num_types > 2 && choices[2] == 0, 1,
        ctr_names[3], num_types > 3 && choices[3] == 0, 1,
        ctr_names[4], num_types > 4 && choices[4] == 0, 1,
        ctr_names[5], num_types > 5 && choices[5] == 0, 1,
        ctr_names[6], num_types > 6 && choices[6] == 0, 1,
        ctr_names[7], num_types > 7 && choices[7] == 0, 1,
        ctr_names[8], num_types > 8 && choices[8] == 0, 1);
    int priority[2] = {1, 1};
    if (player == AI)
    {
      // Native code reuses the name-loop index, then the priority-loop index.
      // At index 9 its adjacent stack word is choices[0].  Shorter lists can
      // still leave this native AI lookup uninitialized; retain that quirk.
      counter_t priority_type = i == 9 ? (counter_t)choices[0] : counter_types[i];
      int raw_pri[2] = {
          raw_counter_value(AI, t_player, t_card, priority_type, 1),
          raw_counter_value(AI, t_player, t_card, priority_type, 0)};
      for (i = 0; i < 2; ++i)
        priority[i] = raw_pri[i] < 0 ? raw_pri[i] / 3 + 3334 :
            (raw_pri[i] == 0 ? 5775 : raw_pri[i] * 99 + 9999);
    }
    int choice2 = DIALOG(player, card, EVENT_RESOLVE_ACTIVATION,
        DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_FULLCARD(t_player, t_card),
        DLG_HEADER(ctr_names[choice1 - 1]), DLG_NO_DISPLAY_FOR_AI,
        "Add", 1, priority[0], "Remove", 1, priority[1]);
    choices[choice1 - 1] = choice2;
    --to_choose;
  }
  for (i = 0; i < num_types; ++i)
    if (choices[i] == CHOICE_ADD)
      add_counters(player, card, t_player, t_card, counter_types[i], 1);
    else if (choices[i] == CHOICE_REMOVE)
      remove_counters_impl(player, card, t_player, t_card, counter_types[i], 1);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26d4f30; original functions/counters.c:1506. */
void increment_all_counters(int src_player, int src_card, int t_player, int t_card)
{
  if ((unsigned int)t_player > 1 || t_card > 150)
    return;
  if (t_card < 0)
  {
    if (ENERGY_COUNTERS(t_player))
      add_energy(t_player, 1);
    if (POISON_COUNTERS(t_player))
      poison(t_player, 1);
    if (EXPERIENCE_COUNTERS(t_player))
      add_experience(t_player, 1);
    return;
  }
  card_instance_t *instance = get_card_instance(t_player, t_card);
  if (instance->internal_card_id == activation_card)
    instance = get_card_instance(instance->parent_controller, instance->parent_card);
  counter_snapshot_t counters[9];
  int count = snapshot_counters(instance, counters);
  for (int i = 0; i < count; ++i)
    add_counters(src_player, src_card, t_player, t_card, counters[i].type, 1);
}

#endif
