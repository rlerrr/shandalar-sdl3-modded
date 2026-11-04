#include "manalink.h"

#ifndef SHANDALAR
typedef enum {
  XNONE = 0,
  XCOMMON = 1,
  XUNCOMMON = 2,
  XRARE = 3,
  XMYTHIC = 4,
  XLAND = 5,
  XSPECIAL = 6,
  TIMESHIFT = 6,
  COMMON_ARABIAN_NIGHTS = 7,
  UNCOMMON_ARABIAN_NIGHTS = 8,
  COMMON_ANTIQUITIES = 9,
  UNCOMMON_ANTIQUITIES = 10,
  COMMON_LEGENDS = 11,
  UNCOMMON_LEGENDS = 12,
  COMMON_THE_DARK = 13,
  UNCOMMON_THE_DARK = 14,
  COMMON_FALLEN_EMPIRES = 15,
  UNCOMMON_FALLEN_EMPIRES = 16,
  COMMON_CHRONICLES = 17,
  UNCOMMON_CHRONICLES = 18,
  COMMON_HOMELANDS = 19,
  UNCOMMON_HOMELANDS = 20,
  COMMON_ALLIANCES = 21,
  UNCOMMON_ALLIANCES = 22,
  RARE_ALLIANCES = 23,
  PLANESHIFT_COMMON = 24,
  PLANESHIFT_UNCOMMON = 25,
  PLANESHIFT_RARE = 26,
  DFC_ANY_RARITY = 27,
  DFC_C_OR_U = 28,
  DFC_R_OR_M = 29,
  DFC_COMMON = 30,
  CONTRAPTION = 31,
  ISLAND = 32,
  LAND_BASIC_OR_SNOW = 33,
  ANY_RARITY = 34,
  GUILDGATE_OR_SHOCKLAND = 35,
  GUILDGATE = 36,
  LEGENDARY_CREATURE = 37,
  NONLEGENDARY_RARE = 38,
  NONLEGENDARY_MYTHIC = 39,
  LESSON = 40,
  MYSTICAL_ARCHIVE = 41,
  NEO_LAND = 42,
  VAL_IFELSE = 43,
  MAX_RARITY_VAL_USED = 43,
} draftrarity_t;

typedef struct rarity_num_t { draftrarity_t rarity; int num; } rarity_num_t;
typedef struct booster_t { EXPANSION_t expansion; const rarity_num_t *booster; const rarity_num_t *starter; } booster_t;
typedef struct csvid_choices_t { int num; card_id_t *csvids; } csvid_choices_t;
_Static_assert(sizeof(rarity_num_t)==8 && sizeof(booster_t)==12 && sizeof(csvid_choices_t)==8, "native pack description layouts");

static const rarity_num_t no_boosters[1] = {
  {0, 0},
};

static const rarity_num_t no_starters[1] = {
  {0, 0},
};

static const rarity_num_t booster_15_cur[4] = {
  {1, 11},
  {2, 3},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_15_lcur[5] = {
  {5, 1},
  {1, 10},
  {2, 3},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_15_l2cur[5] = {
  {5, 2},
  {1, 9},
  {2, 3},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_15_lcurm71[7] = {
  {5, 1},
  {1, 10},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_15_lcurm74[7] = {
  {5, 1},
  {1, 10},
  {2, 3},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_15_curm71a[7] = {
  {1, 10},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {34, 1},
  {0, 0},
};

static const rarity_num_t starter_l22_cur[5] = {
  {5, 22},
  {1, 26},
  {2, 9},
  {3, 3},
  {0, 0},
};

static const rarity_num_t starter_l30_cur[5] = {
  {5, 30},
  {1, 32},
  {2, 10},
  {3, 3},
  {0, 0},
};

static const rarity_num_t starter_l30_curm[7] = {
  {5, 30},
  {1, 32},
  {2, 10},
  {43, 7929962},
  {3, 3},
  {4, 3},
  {0, 0},
};

static const rarity_num_t booster_alpha[10] = {
  {43, 7929903},
  {5, 11},
  {1, 11},
  {43, 7929882},
  {5, 3},
  {2, 3},
  {43, 7929861},
  {32, 1},
  {3, 1},
  {0, 0},
};

static const rarity_num_t starter_alpha[10] = {
  {43, 7929903},
  {5, 45},
  {1, 45},
  {43, 7929882},
  {5, 13},
  {2, 13},
  {43, 7929861},
  {32, 2},
  {3, 2},
  {0, 0},
};

static const rarity_num_t booster_beta[10] = {
  {43, 7929902},
  {5, 11},
  {1, 11},
  {43, 7929882},
  {5, 3},
  {2, 3},
  {43, 7929860},
  {32, 1},
  {3, 1},
  {0, 0},
};

static const rarity_num_t starter_beta[10] = {
  {43, 7929902},
  {5, 45},
  {1, 45},
  {43, 7929882},
  {5, 13},
  {2, 13},
  {43, 7929860},
  {32, 2},
  {3, 2},
  {0, 0},
};

static const rarity_num_t booster_arabian_nights[3] = {
  {7, 6},
  {8, 2},
  {0, 0},
};

static const rarity_num_t booster_antiquities[3] = {
  {9, 6},
  {10, 2},
  {0, 0},
};

static const rarity_num_t booster_revised[8] = {
  {43, 7929902},
  {5, 11},
  {1, 11},
  {43, 7929882},
  {5, 3},
  {2, 3},
  {3, 1},
  {0, 0},
};

static const rarity_num_t starter_revised[8] = {
  {43, 7929902},
  {5, 45},
  {1, 45},
  {43, 7929882},
  {5, 13},
  {2, 13},
  {3, 2},
  {0, 0},
};

static const rarity_num_t booster_legends[4] = {
  {11, 11},
  {12, 3},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_the_dark[3] = {
  {13, 6},
  {14, 2},
  {0, 0},
};

static const rarity_num_t booster_fallen_empires[3] = {
  {15, 6},
  {16, 2},
  {0, 0},
};

static const rarity_num_t booster_chronicles[3] = {
  {17, 9},
  {18, 3},
  {0, 0},
};

static const rarity_num_t booster_homelands[3] = {
  {19, 6},
  {20, 2},
  {0, 0},
};

static const rarity_num_t booster_alliances[4] = {
  {21, 8},
  {22, 3},
  {23, 1},
  {0, 0},
};

static const rarity_num_t starter_ice_age[5] = {
  {33, 22},
  {1, 26},
  {2, 9},
  {3, 3},
  {0, 0},
};

static const rarity_num_t booster_unglued[5] = {
  {5, 1},
  {1, 6},
  {2, 2},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_portal3k[5] = {
  {5, 2},
  {1, 5},
  {2, 2},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_time_spiral[5] = {
  {1, 10},
  {2, 3},
  {3, 1},
  {6, 1},
  {0, 0},
};

static const rarity_num_t starter_time_spiral[6] = {
  {5, 30},
  {1, 29},
  {2, 10},
  {3, 3},
  {6, 3},
  {0, 0},
};

static const rarity_num_t booster_planar_chaos[8] = {
  {1, 8},
  {24, 3},
  {2, 2},
  {43, 262147},
  {25, 1},
  {26, 1},
  {3, 1},
  {0, 0},
};

static const rarity_num_t booster_innistrad[10] = {
  {43, 262145},
  {5, 1},
  {5, 0},
  {1, 9},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {27, 1},
  {0, 0},
};

static const rarity_num_t booster_dragons_maze[7] = {
  {35, 1},
  {1, 10},
  {2, 3},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_conspiracy[6] = {
  {1, 10},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_shadows_over_innistrad[13] = {
  {43, 131073},
  {5, 1},
  {5, 0},
  {1, 8},
  {43, 524295},
  {1, 1},
  {29, 1},
  {28, 1},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_ixalan[7] = {
  {5, 1},
  {1, 10},
  {2, 3},
  {43, 8257647},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_unstable[8] = {
  {5, 1},
  {31, 2},
  {1, 8},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_guilds_of_ravnica[7] = {
  {36, 1},
  {1, 10},
  {2, 3},
  {43, 7929962},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_jumpstart[10] = {
  {5, 7},
  {43, 196610},
  {5, 1},
  {3, 1},
  {1, 8},
  {2, 3},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_commander_legends[8] = {
  {1, 13},
  {2, 3},
  {43, 4849728},
  {38, 1},
  {39, 1},
  {37, 2},
  {34, 1},
  {0, 0},
};

static const rarity_num_t booster_strixhaven[8] = {
  {1, 9},
  {2, 3},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {40, 1},
  {41, 1},
  {0, 0},
};

static const rarity_num_t booster_mid_vow[8] = {
  {5, 1},
  {1, 9},
  {30, 1},
  {2, 3},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {0, 0},
};

static const rarity_num_t booster_kamigawa_neo[8] = {
  {42, 1},
  {1, 9},
  {2, 3},
  {28, 1},
  {43, 4849728},
  {3, 1},
  {4, 1},
  {0, 0},
};

booster_t pack_contents[198] = {
  {0, no_boosters, no_starters},
  {1, no_boosters, no_starters},
  {2, booster_alpha, starter_alpha},
  {3, booster_beta, starter_beta},
  {4, booster_beta, starter_beta},
  {5, booster_arabian_nights, no_starters},
  {6, booster_antiquities, no_starters},
  {7, booster_revised, starter_revised},
  {8, booster_legends, no_starters},
  {9, booster_the_dark, no_starters},
  {10, booster_fallen_empires, no_starters},
  {11, booster_15_cur, starter_l22_cur},
  {12, booster_15_cur, starter_ice_age},
  {13, booster_chronicles, no_starters},
  {14, booster_homelands, no_starters},
  {15, booster_alliances, no_starters},
  {16, booster_15_cur, starter_l22_cur},
  {17, booster_15_cur, no_starters},
  {18, booster_15_cur, starter_l22_cur},
  {19, no_boosters, no_starters},
  {20, booster_15_l2cur, no_starters},
  {21, booster_15_cur, no_starters},
  {22, booster_15_cur, starter_l22_cur},
  {23, booster_15_cur, no_starters},
  {24, booster_15_cur, no_starters},
  {25, booster_15_l2cur, no_starters},
  {26, booster_unglued, no_starters},
  {27, booster_15_cur, starter_l30_cur},
  {28, booster_15_cur, no_starters},
  {29, booster_15_cur, starter_l30_cur},
  {30, booster_15_cur, no_starters},
  {31, booster_portal3k, no_starters},
  {32, booster_15_l2cur, no_starters},
  {33, booster_15_cur, starter_l30_cur},
  {34, no_boosters, no_starters},
  {35, booster_15_cur, no_starters},
  {36, no_boosters, no_starters},
  {37, booster_15_cur, no_starters},
  {38, booster_15_cur, starter_l30_cur},
  {39, no_boosters, no_starters},
  {40, booster_15_cur, no_starters},
  {41, booster_15_lcur, no_starters},
  {42, booster_15_cur, no_starters},
  {43, booster_15_cur, starter_l30_cur},
  {44, booster_15_cur, no_starters},
  {45, booster_15_cur, no_starters},
  {46, booster_15_cur, starter_l30_cur},
  {47, booster_15_cur, no_starters},
  {48, booster_15_cur, no_starters},
  {49, booster_15_lcur, no_starters},
  {50, booster_15_cur, starter_l30_cur},
  {51, booster_15_cur, no_starters},
  {52, booster_15_cur, no_starters},
  {53, booster_15_cur, starter_l30_cur},
  {54, booster_15_lcur, no_starters},
  {55, booster_15_cur, no_starters},
  {56, booster_15_cur, no_starters},
  {57, booster_15_lcur, no_starters},
  {58, booster_15_cur, starter_l30_cur},
  {59, booster_15_cur, no_starters},
  {60, booster_15_cur, no_starters},
  {61, booster_15_cur, no_starters},
  {62, booster_time_spiral, starter_time_spiral},
  {63, booster_planar_chaos, no_starters},
  {64, booster_15_cur, no_starters},
  {65, booster_15_lcur, no_starters},
  {66, booster_15_lcur, no_starters},
  {67, booster_15_cur, starter_l30_cur},
  {68, no_boosters, no_starters},
  {69, booster_15_cur, no_starters},
  {70, booster_15_cur, starter_l30_cur},
  {71, booster_15_cur, no_starters},
  {72, no_boosters, no_starters},
  {73, booster_15_lcur, no_starters},
  {74, booster_15_lcurm71, starter_l30_curm},
  {75, no_boosters, no_starters},
  {76, booster_15_lcurm71, no_starters},
  {77, no_boosters, no_starters},
  {78, booster_15_lcurm71, no_starters},
  {79, booster_15_lcurm71, no_starters},
  {80, no_boosters, no_starters},
  {81, no_boosters, no_starters},
  {82, booster_15_cur, no_starters},
  {83, booster_15_lcurm71, no_starters},
  {84, no_boosters, no_starters},
  {85, no_boosters, no_starters},
  {86, booster_15_lcurm71, no_starters},
  {87, no_boosters, no_starters},
  {88, booster_15_lcurm71, no_starters},
  {89, no_boosters, no_starters},
  {90, booster_15_lcurm71, no_starters},
  {91, no_boosters, no_starters},
  {92, no_boosters, no_starters},
  {93, booster_15_lcurm71, no_starters},
  {94, no_boosters, no_starters},
  {95, booster_15_lcur, no_starters},
  {96, booster_15_lcurm71, no_starters},
  {97, no_boosters, no_starters},
  {98, booster_15_lcurm71, no_starters},
  {99, no_boosters, no_starters},
  {100, booster_15_lcurm71, no_starters},
  {101, no_boosters, no_starters},
  {102, no_boosters, no_starters},
  {103, booster_innistrad, no_starters},
  {104, no_boosters, no_starters},
  {105, booster_innistrad, no_starters},
  {106, no_boosters, no_starters},
  {107, booster_15_lcurm71, no_starters},
  {108, no_boosters, no_starters},
  {109, booster_15_lcurm71, no_starters},
  {110, no_boosters, no_starters},
  {111, no_boosters, no_starters},
  {112, booster_15_lcurm71, no_starters},
  {113, no_boosters, no_starters},
  {114, booster_15_lcurm71, no_starters},
  {115, no_boosters, no_starters},
  {116, booster_dragons_maze, no_starters},
  {117, booster_15_curm71a, no_starters},
  {118, booster_15_lcurm71, no_starters},
  {119, no_boosters, no_starters},
  {120, no_boosters, no_starters},
  {121, booster_15_lcurm71, no_starters},
  {122, no_boosters, no_starters},
  {123, booster_15_lcurm71, no_starters},
  {124, no_boosters, no_starters},
  {125, booster_15_lcurm71, no_starters},
  {126, no_boosters, no_starters},
  {127, booster_conspiracy, no_starters},
  {128, booster_15_curm71a, no_starters},
  {129, booster_15_lcurm71, no_starters},
  {130, no_boosters, no_starters},
  {131, no_boosters, no_starters},
  {132, booster_15_lcurm71, no_starters},
  {133, no_boosters, no_starters},
  {134, booster_15_lcurm71, no_starters},
  {135, no_boosters, no_starters},
  {136, booster_15_lcurm71, no_starters},
  {137, booster_15_curm71a, no_starters},
  {138, booster_15_lcurm71, no_starters},
  {139, no_boosters, no_starters},
  {140, no_boosters, no_starters},
  {141, booster_15_lcurm71, no_starters},
  {142, no_boosters, no_starters},
  {143, booster_15_lcurm71, no_starters},
  {144, no_boosters, no_starters},
  {145, booster_shadows_over_innistrad, no_starters},
  {146, booster_15_curm71a, no_starters},
  {147, booster_shadows_over_innistrad, no_starters},
  {148, no_boosters, no_starters},
  {149, booster_conspiracy, no_starters},
  {150, no_boosters, no_starters},
  {151, booster_15_lcurm71, no_starters},
  {152, no_boosters, no_starters},
  {153, booster_15_lcurm71, no_starters},
  {154, booster_15_curm71a, no_starters},
  {155, no_boosters, no_starters},
  {156, booster_15_lcurm71, no_starters},
  {157, no_boosters, no_starters},
  {158, booster_15_lcurm71, no_starters},
  {159, no_boosters, no_starters},
  {160, booster_ixalan, no_starters},
  {161, booster_unstable, no_starters},
  {162, booster_15_lcurm71, no_starters},
  {163, booster_15_lcurm71, no_starters},
  {164, booster_15_lcurm71, no_starters},
  {165, no_boosters, no_starters},
  {166, booster_15_lcurm71, no_starters},
  {167, no_boosters, no_starters},
  {168, booster_guilds_of_ravnica, no_starters},
  {169, booster_guilds_of_ravnica, no_starters},
  {170, booster_15_lcurm71, no_starters},
  {171, booster_15_curm71a, no_starters},
  {172, booster_15_curm71a, no_starters},
  {173, no_boosters, no_starters},
  {174, booster_15_lcurm71, no_starters},
  {175, no_boosters, no_starters},
  {176, booster_15_lcurm71, no_starters},
  {177, booster_15_lcurm71, no_starters},
  {178, no_boosters, no_starters},
  {179, booster_15_lcurm71, no_starters},
  {180, booster_jumpstart, no_starters},
  {181, booster_15_lcurm74, no_starters},
  {182, booster_commander_legends, no_starters},
  {183, booster_15_lcurm74, no_starters},
  {184, no_boosters, no_starters},
  {185, booster_strixhaven, no_starters},
  {186, no_boosters, no_starters},
  {187, booster_15_lcurm74, no_starters},
  {188, booster_15_lcurm74, no_starters},
  {189, no_boosters, no_starters},
  {190, booster_mid_vow, no_starters},
  {191, no_boosters, no_starters},
  {192, booster_mid_vow, no_starters},
  {193, no_boosters, no_starters},
  {194, booster_kamigawa_neo, no_starters},
  {195, no_boosters, no_starters},
  {196, booster_15_lcurm74, no_starters},
  {197, no_boosters, no_starters},
};

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26eb550; original functions/packs.c:650. */
static void add_to_csvid_choices(csvid_choices_t *choices, int csvid)
{
  int result;
  card_id_t *csvids;
  result = choices->num;
  if (result < 0)
  {
    backtrace_and_abort("functions/packs.c:652\nchoices->num >= 0");
  }
  if (choices->csvids == NULL)
  {
    if (result != 0)
    {
      backtrace_and_abort("functions/packs.c:656\nchoices->num == 0");
    }
    csvids = malloc(800);
    choices->csvids = csvids;
    if (csvids == NULL)
    {
      fatal("could not malloc %d bytes for choices->csvids", 800);
    }
    result = choices->num;
  }
  if (((result == 200 || result == 400) || result == 800) || (result == 0x640))
  {
    csvids = realloc(choices->csvids, result << 3);
    choices->csvids = csvids;
    result = choices->num;
    if (csvids == NULL)
    {
      fatal("could not realloc %d bytes for choices->csvids", result << 3);
    }
  }
  if (result != 0xc80)
  {
    csvids = choices->csvids;
    choices->num = result + 1;
    csvids[result] = csvid;
    return;
  }
  fatal("Tried to insert 3200th card in csvid_t_choices_t");
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26eb660; original functions/packs.c:3042. */
static int add_card_to_pack(int *out_csvids, int pos, csvid_choices_t *choice, int num,
                            draftrarity_t rarity, EXPANSION_t expansion, int booster, int testing_mode)
{
  if (num < 1)
    return 0;
  int remaining = choice->num;
  if (remaining < 1)
  {
    if (testing_mode)
      fprintf(stderr, "Expansion #%d, booster=%d: no available cards of rarity %d\012", expansion,
              booster, rarity);
    return 0;
  }
  unsigned int bytes = remaining * 4U;
  unsigned int *local_choices = malloc(bytes);
  if (!local_choices)
    fatal("could not malloc %d bytes for local_choices.csvids", bytes);
  memcpy(local_choices, choice->csvids, bytes);
  int written = 0;
  do
  {
    int selected = internal_rand(remaining);
    unsigned int csvid = local_choices[selected];
    bool basic_land =
        csvid == CARD_ID_SWAMP || csvid == CARD_ID_MOUNTAIN || csvid == CARD_ID_FOREST || csvid == CARD_ID_ISLAND || csvid == CARD_ID_PLAINS;
    if (basic_land)
    {
      out_csvids[pos++] = csvid;
      ++written;
    }
    else
    {
      bool repeatable_snow_land = csvid - 0x834 < 5 && expansion == 0xc;
      bool duplicate = false;
      if (!repeatable_snow_land)
        for (int i = 0; i < pos; ++i)
          if (csvid == (unsigned int)out_csvids[i])
          {
            duplicate = true;
            break;
          }
      if (remaining == 1 && !repeatable_snow_land)
      {
        if (!duplicate)
        {
          out_csvids[pos++] = csvid;
          ++written;
        }
        if (written < num)
        {
          int *end = out_csvids + num + pos - written;
          for (int *entry = out_csvids + pos; entry != end; ++entry)
            *entry = local_choices[internal_rand(1)];
          if (testing_mode)
            fprintf(stderr, "Expansion #%d, booster=%d: added duplicate cards of rarity %d\012",
                    expansion, booster, rarity);
        }
        free(local_choices);
        return num;
      }
      if (!repeatable_snow_land)
      {
        --remaining;
        local_choices[selected] = local_choices[remaining];
      }
      if (!duplicate)
      {
        out_csvids[pos++] = csvid;
        ++written;
      }
    }
    if (written >= num)
    {
      free(local_choices);
      return num;
    }
  } while (true);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ec840; original functions/packs.c:3397. */
int is_valid_pack(EXPANSION_t expansion, int booster)
{
  const rarity_num_t *contents;

  if (((uint32_t)expansion < 198U) && (expansion != EXPANSION_UNHINGED))
  {
    switch (expansion)
    {
    case EXPANSION_UNSTABLE:
    case EXPANSION_CORE_SET_2021:
    case EXPANSION_ZENDIKAR_RISING:
    case EXPANSION_COMMANDER_LEGENDS:
    case EXPANSION_KALDHEIM:
    case EXPANSION_STRIXHAVEN_SCHOOL_OF_MAGES:
    case EXPANSION_MODERN_HORIZONS_2:
    case EXPANSION_ADVENTURES_IN_THE_FORGOTTEN_REALMS:
    case EXPANSION_INNISTRAD_MIDNIGHT_HUNT:
    case EXPANSION_INNISTRAD_CRIMSON_VOW:
    case EXPANSION_KAMIGAWA_NEON_DYNASTY:
    case EXPANSION_STREETS_OF_NEW_CAPENNA:
      break;
    default:
      if (booster == 0)
      {
        contents = pack_contents[expansion].starter;
      }
      else
      {
        contents = pack_contents[expansion].booster;
      }
      return (unsigned int)(contents->rarity != XNONE);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ec8a0; original functions/packs.c:2989. */
int pack_size(EXPANSION_t expansion, int booster)
{
  if (!is_valid_pack(expansion, booster))
    return 0;
  const rarity_num_t *contents =
      booster ? pack_contents[expansion].booster : pack_contents[expansion].starter;
  if (!booster && contents == no_starters)
    return 0;
  int total = 0;
  for (int index = 0; contents[index].rarity != XNONE;)
  {
    if (contents[index].rarity == MAX_RARITY_VAL_USED)
    {
      if (contents[index + 1].rarity == XNONE)
        backtrace_and_abort("functions/packs.c:3007\nrn[i + 1].rarity != NONE");
      if (contents[index + 2].rarity == XNONE)
        backtrace_and_abort("functions/packs.c:3008\nrn[i + 2].rarity != NONE");
      int second = contents[index + 2].num;
      int first = contents[index + 1].num;
      total += first < second ? second : first;
      index += 3;
    }
    else
      total += contents[index++].num;
  }
  return total;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ed870; original functions/packs.c:3770. */
void announce_expansion_choice(int player, int card, EXPANSION_t expansion)
{
  const char **expansion_names;
  int num_textlines;
  if (ai_is_speculating == 1)
  {
    return;
  }
  if (player != 0)
  {
    expansion_names = load_text_nocopy_from_file("Menus.txt", "EXPANSIONNAMES", &num_textlines);
    if ((int)(expansion + EXPANSION_ANTIQUITIES) < num_textlines)
    {
      dialog_impl(player, card, EVENT_ACTIVATE, DLG_HEADER(
                  expansion_names[expansion + EXPANSION_REVISED_EDITION]), DLG_NO_STORAGE, DLG_NO_CANCEL, 0,
                  0, 0, 0);
      return;
    }
  }
  return;
}

#endif

#ifndef SHANDALAR
typedef struct count_and_card_t { int count; card_id_t csvid; } count_and_card_t;
typedef struct varying_copies_entry_t { draftrarity_t rarity; const count_and_card_t *counts_and_cards; } varying_copies_entry_t;
_Static_assert(sizeof(count_and_card_t)==8 && sizeof(varying_copies_entry_t)==8, "native collation record widths");

static const count_and_card_t copies_common_arabian_nights[28] = {
  {4, 397},
  {4, 399},
  {5, 401},
  {4, 404},
  {4, 406},
  {11, 407},
  {4, 408},
  {5, 415},
  {4, 418},
  {5, 420},
  {4, 421},
  {4, 422},
  {5, 424},
  {4, 425},
  {5, 437},
  {4, 441},
  {5, 443},
  {1, 164},
  {5, 444},
  {4, 447},
  {4, 448},
  {4, 452},
  {4, 454},
  {4, 461},
  {5, 462},
  {4, 463},
  {5, 464},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_arabian_nights[52] = {
  {3, 391},
  {2, 392},
  {2, 393},
  {2, 396},
  {3, 394},
  {2, 395},
  {3, 398},
  {2, 400},
  {3, 930},
  {2, 402},
  {3, 403},
  {3, 405},
  {2, 931},
  {3, 409},
  {2, 410},
  {2, 411},
  {2, 412},
  {2, 414},
  {2, 413},
  {2, 416},
  {3, 417},
  {3, 419},
  {2, 423},
  {2, 426},
  {2, 427},
  {2, 428},
  {2, 429},
  {2, 430},
  {3, 431},
  {2, 432},
  {2, 433},
  {2, 434},
  {3, 435},
  {2, 436},
  {3, 438},
  {3, 439},
  {3, 440},
  {2, 442},
  {4, 445},
  {2, 446},
  {2, 449},
  {2, 450},
  {2, 451},
  {3, 453},
  {2, 455},
  {2, 456},
  {2, 457},
  {3, 458},
  {2, 459},
  {3, 460},
  {2, 465},
  {-1, 0},
};

static const count_and_card_t copies_common_antiquities[33] = {
  {4, 466},
  {4, 468},
  {4, 469},
  {4, 470},
  {4, 472},
  {4, 516},
  {4, 473},
  {4, 477},
  {4, 478},
  {4, 483},
  {4, 487},
  {1, 488},
  {4, 491},
  {4, 492},
  {1, 495},
  {4, 500},
  {1, 508},
  {4, 513},
  {4, 514},
  {4, 515},
  {4, 549},
  {4, 521},
  {4, 522},
  {4, 524},
  {4, 527},
  {1, 528},
  {4, 530},
  {4, 540},
  {6, 541},
  {6, 543},
  {6, 544},
  {4, 550},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_antiquities[56] = {
  {1, 467},
  {2, 471},
  {2, 474},
  {2, 475},
  {3, 476},
  {1, 479},
  {1, 480},
  {3, 481},
  {3, 482},
  {1, 484},
  {1, 485},
  {1, 486},
  {3, 489},
  {3, 490},
  {3, 493},
  {3, 494},
  {1, 496},
  {3, 497},
  {3, 498},
  {1, 499},
  {3, 501},
  {1, 502},
  {3, 503},
  {2, 504},
  {3, 505},
  {3, 506},
  {3, 507},
  {3, 508},
  {1, 509},
  {1, 510},
  {1, 511},
  {3, 512},
  {3, 518},
  {3, 517},
  {3, 519},
  {3, 520},
  {3, 523},
  {1, 525},
  {1, 526},
  {3, 528},
  {3, 529},
  {1, 531},
  {3, 532},
  {3, 533},
  {1, 534},
  {3, 535},
  {3, 536},
  {3, 537},
  {1, 538},
  {1, 539},
  {1, 542},
  {3, 545},
  {3, 546},
  {3, 547},
  {1, 548},
  {-1, 0},
};

static const count_and_card_t copies_common_the_dark[42] = {
  {3, 294},
  {3, 336},
  {3, 350},
  {3, 303},
  {3, 305},
  {3, 298},
  {3, 310},
  {3, 370},
  {3, 311},
  {3, 314},
  {3, 934},
  {3, 321},
  {3, 334},
  {3, 326},
  {3, 327},
  {3, 332},
  {3, 329},
  {3, 936},
  {3, 330},
  {3, 937},
  {3, 938},
  {3, 375},
  {3, 374},
  {3, 339},
  {3, 365},
  {3, 366},
  {3, 939},
  {1, 377},
  {3, 941},
  {3, 347},
  {3, 371},
  {3, 942},
  {3, 355},
  {3, 944},
  {3, 946},
  {3, 947},
  {3, 948},
  {3, 364},
  {3, 383},
  {3, 951},
  {3, 388},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_the_dark[79] = {
  {2, 292},
  {2, 293},
  {1, 300},
  {1, 295},
  {2, 296},
  {1, 297},
  {1, 299},
  {2, 301},
  {2, 302},
  {2, 353},
  {2, 304},
  {2, 306},
  {1, 307},
  {1, 367},
  {2, 308},
  {2, 932},
  {1, 309},
  {2, 369},
  {2, 376},
  {2, 312},
  {2, 933},
  {2, 313},
  {1, 315},
  {1, 316},
  {2, 317},
  {2, 318},
  {2, 319},
  {2, 320},
  {2, 935},
  {2, 324},
  {1, 325},
  {1, 331},
  {1, 333},
  {1, 335},
  {1, 337},
  {1, 338},
  {1, 340},
  {2, 341},
  {1, 342},
  {1, 343},
  {1, 344},
  {1, 345},
  {2, 940},
  {1, 346},
  {1, 378},
  {2, 348},
  {1, 373},
  {2, 349},
  {2, 323},
  {1, 351},
  {1, 352},
  {1, 943},
  {2, 354},
  {2, 356},
  {1, 357},
  {2, 358},
  {1, 945},
  {2, 322},
  {1, 359},
  {2, 360},
  {2, 361},
  {1, 381},
  {2, 328},
  {2, 362},
  {1, 363},
  {2, 949},
  {2, 372},
  {2, 950},
  {2, 368},
  {2, 379},
  {1, 380},
  {2, 382},
  {2, 384},
  {2, 385},
  {2, 386},
  {1, 387},
  {1, 390},
  {1, 389},
  {-1, 0},
};

static const count_and_card_t copies_common_fallen_empires[37] = {
  {4, 1750},
  {4, 1752},
  {4, 1754},
  {4, 1756},
  {1, 1759},
  {3, 1768},
  {4, 1771},
  {3, 1774},
  {3, 1775},
  {3, 1777},
  {3, 1781},
  {3, 1783},
  {4, 1785},
  {3, 1790},
  {4, 1792},
  {3, 1795},
  {4, 1456},
  {4, 1796},
  {3, 1797},
  {3, 1799},
  {4, 1802},
  {3, 1806},
  {4, 1807},
  {3, 1808},
  {3, 1809},
  {3, 1810},
  {3, 1104},
  {4, 1812},
  {3, 1555},
  {3, 1556},
  {3, 1823},
  {4, 1827},
  {4, 1833},
  {3, 1837},
  {3, 1841},
  {4, 1842},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_fallen_empires[67] = {
  {1, 1749},
  {1, 1751},
  {1, 1753},
  {3, 1755},
  {1, 1757},
  {3, 1758},
  {1, 1760},
  {1, 1761},
  {1, 1762},
  {1, 1763},
  {3, 1764},
  {1, 1765},
  {3, 1766},
  {2, 1767},
  {1, 1769},
  {2, 1770},
  {1, 1772},
  {1, 1773},
  {3, 1778},
  {3, 1776},
  {3, 1779},
  {1, 1780},
  {1, 1782},
  {3, 1784},
  {1, 1786},
  {1, 1787},
  {2, 1788},
  {3, 1789},
  {1, 1791},
  {1, 1793},
  {3, 1794},
  {1, 1798},
  {3, 1800},
  {3, 1801},
  {1, 1803},
  {1, 1804},
  {1, 1447},
  {1, 1805},
  {3, 1811},
  {1, 1813},
  {3, 1814},
  {1, 1815},
  {1, 1816},
  {1, 1817},
  {2, 1818},
  {1, 1819},
  {3, 1820},
  {3, 1821},
  {1, 1822},
  {3, 1824},
  {3, 1825},
  {2, 1826},
  {3, 1828},
  {3, 1831},
  {1, 1832},
  {3, 1829},
  {1, 1830},
  {1, 1834},
  {3, 1835},
  {3, 1836},
  {3, 1838},
  {3, 1839},
  {1, 1840},
  {1, 1573},
  {1, 1843},
  {1, 1844},
  {-1, 0},
};

static const count_and_card_t copies_common_chronicles[46] = {
  {3, 553},
  {3, 469},
  {2, 474},
  {2, 476},
  {3, 350},
  {3, 585},
  {3, 590},
  {3, 404},
  {3, 406},
  {3, 607},
  {3, 615},
  {3, 623},
  {2, 495},
  {3, 418},
  {3, 638},
  {2, 324},
  {3, 421},
  {3, 648},
  {3, 332},
  {3, 936},
  {3, 938},
  {3, 424},
  {3, 682},
  {3, 696},
  {1, 697},
  {2, 341},
  {1, 723},
  {3, 441},
  {3, 730},
  {3, 770},
  {3, 450},
  {2, 356},
  {3, 947},
  {1, 790},
  {1, 818},
  {2, 368},
  {1, 820},
  {3, 823},
  {4, 541},
  {4, 543},
  {4, 544},
  {3, 843},
  {3, 847},
  {3, 849},
  {3, 463},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_chronicles[72] = {
  {3, 391},
  {1, 558},
  {1, 392},
  {1, 564},
  {1, 568},
  {1, 569},
  {1, 571},
  {1, 572},
  {3, 573},
  {3, 296},
  {1, 297},
  {3, 579},
  {1, 299},
  {3, 301},
  {1, 353},
  {1, 588},
  {1, 595},
  {1, 403},
  {3, 598},
  {1, 599},
  {3, 601},
  {1, 405},
  {1, 608},
  {1, 309},
  {3, 625},
  {3, 416},
  {3, 631},
  {3, 320},
  {1, 644},
  {1, 646},
  {3, 498},
  {1, 670},
  {1, 673},
  {3, 686},
  {1, 504},
  {1, 431},
  {1, 691},
  {1, 693},
  {1, 711},
  {1, 731},
  {1, 733},
  {1, 511},
  {1, 737},
  {1, 741},
  {3, 747},
  {3, 751},
  {3, 755},
  {1, 520},
  {3, 764},
  {1, 772},
  {1, 952},
  {1, 357},
  {1, 783},
  {1, 784},
  {3, 786},
  {3, 787},
  {1, 791},
  {1, 797},
  {3, 798},
  {3, 807},
  {1, 809},
  {3, 372},
  {1, 815},
  {1, 824},
  {1, 834},
  {1, 838},
  {3, 845},
  {3, 850},
  {3, 387},
  {1, 858},
  {1, 548},
  {-1, 0},
};

static const count_and_card_t copies_common_homelands[42] = {
  {4, 2168},
  {4, 2169},
  {1, 2170},
  {4, 2171},
  {4, 2177},
  {4, 973},
  {4, 2181},
  {4, 2187},
  {4, 2189},
  {1, 2191},
  {1, 2192},
  {1, 2193},
  {1, 2194},
  {4, 2195},
  {4, 2197},
  {4, 2199},
  {1, 2200},
  {4, 2202},
  {4, 2204},
  {1, 2206},
  {4, 2208},
  {1, 2212},
  {4, 2213},
  {1, 2215},
  {1, 2216},
  {4, 2220},
  {1, 2221},
  {4, 1455},
  {1, 1084},
  {4, 2225},
  {4, 1646},
  {1, 2230},
  {1, 2235},
  {4, 2237},
  {4, 2240},
  {1, 1648},
  {1, 2244},
  {4, 2245},
  {4, 1651},
  {4, 2247},
  {4, 2250},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_homelands[75] = {
  {3, 1628},
  {3, 2179},
  {1, 2176},
  {1, 1629},
  {1, 2172},
  {3, 2173},
  {3, 2174},
  {1, 2175},
  {1, 2178},
  {1, 1630},
  {3, 2180},
  {1, 1631},
  {1, 2182},
  {1, 2183},
  {1, 1632},
  {1, 2184},
  {1, 2185},
  {1, 2186},
  {3, 2188},
  {1, 2190},
  {1, 1633},
  {1, 2196},
  {3, 1634},
  {1, 1635},
  {3, 1636},
  {1, 1637},
  {1, 2198},
  {3, 1638},
  {3, 2201},
  {1, 1639},
  {1, 2203},
  {1, 2205},
  {3, 2207},
  {3, 2209},
  {1, 1640},
  {1, 2210},
  {1, 2211},
  {1, 1641},
  {3, 1374},
  {3, 1642},
  {1, 2214},
  {1, 1643},
  {3, 2217},
  {1, 2218},
  {3, 2219},
  {1, 2222},
  {1, 2223},
  {1, 2224},
  {1, 2226},
  {1, 1644},
  {3, 2227},
  {1, 1645},
  {1, 2228},
  {3, 2229},
  {3, 2231},
  {1, 2232},
  {3, 2234},
  {3, 2233},
  {1, 2236},
  {3, 2238},
  {3, 2239},
  {1, 1647},
  {1, 2241},
  {1, 2242},
  {3, 2243},
  {1, 1649},
  {3, 1650},
  {1, 2246},
  {1, 2248},
  {1, 2249},
  {1, 1652},
  {1, 1654},
  {1, 2251},
  {3, 2252},
  {-1, 0},
};

static const count_and_card_t copies_common_alliances[51] = {
  {2, 2253},
  {2, 2254},
  {2, 2255},
  {2, 2257},
  {2, 2258},
  {2, 1685},
  {2, 2261},
  {3, 2262},
  {2, 2266},
  {2, 2267},
  {2, 1400},
  {2, 2277},
  {2, 2278},
  {3, 2283},
  {2, 2286},
  {2, 2287},
  {2, 2289},
  {3, 2290},
  {2, 2291},
  {2, 2293},
  {3, 1052},
  {2, 2299},
  {2, 2303},
  {2, 2306},
  {2, 2310},
  {2, 1407},
  {2, 2316},
  {2, 2324},
  {2, 2326},
  {2, 2329},
  {2, 2332},
  {2, 2335},
  {2, 2340},
  {2, 2348},
  {2, 2351},
  {3, 2352},
  {2, 2353},
  {2, 2354},
  {2, 2358},
  {3, 1162},
  {2, 1679},
  {3, 2365},
  {3, 2367},
  {2, 2372},
  {2, 2375},
  {2, 2377},
  {2, 2378},
  {2, 2381},
  {3, 1667},
  {2, 2384},
  {-1, 0},
};

static const count_and_card_t copies_uncommon_alliances[46] = {
  {2, 2259},
  {2, 2263},
  {2, 2264},
  {2, 2265},
  {2, 2269},
  {6, 1684},
  {2, 2270},
  {2, 2272},
  {2, 2274},
  {2, 2275},
  {2, 2276},
  {6, 2280},
  {6, 2282},
  {2, 2285},
  {5, 2292},
  {2, 2295},
  {2, 2298},
  {2, 2301},
  {2, 2304},
  {2, 2308},
  {2, 2312},
  {2, 2313},
  {2, 2318},
  {2, 2319},
  {2, 2320},
  {2, 2321},
  {2, 2322},
  {2, 1406},
  {2, 2333},
  {2, 2334},
  {6, 2336},
  {2, 2341},
  {2, 2342},
  {2, 2343},
  {2, 2344},
  {2, 2346},
  {2, 2355},
  {2, 2356},
  {2, 2360},
  {2, 2361},
  {2, 2362},
  {2, 2363},
  {2, 2373},
  {2, 2379},
  {2, 2385},
  {-1, 0},
};

static const count_and_card_t copies_rare_alliances[50] = {
  {2, 2256},
  {2, 1686},
  {2, 2260},
  {2, 2268},
  {2, 2271},
  {2, 2273},
  {2, 2279},
  {2, 2281},
  {2, 2284},
  {2, 2288},
  {2, 2294},
  {2, 2296},
  {2, 2297},
  {2, 2300},
  {2, 1481},
  {2, 2302},
  {2, 2305},
  {2, 2307},
  {2, 2309},
  {2, 2311},
  {2, 2314},
  {2, 2315},
  {2, 2317},
  {2, 2323},
  {2, 2325},
  {2, 2327},
  {2, 2328},
  {2, 2330},
  {2, 2331},
  {2, 2337},
  {2, 2338},
  {2, 2339},
  {2, 2345},
  {2, 2349},
  {2, 2350},
  {6, 2347},
  {2, 2357},
  {2, 2359},
  {2, 2364},
  {2, 2366},
  {2, 2368},
  {2, 2369},
  {2, 2370},
  {2, 2371},
  {6, 2374},
  {2, 2376},
  {2, 2380},
  {6, 2382},
  {2, 2383},
  {-1, 0},
};

static const count_and_card_t copies_guildgate_or_shockland[22] = {
  {65, 12662},
  {65, 12900},
  {65, 12926},
  {65, 12733},
  {65, 12964},
  {65, 12754},
  {65, 13025},
  {65, 12794},
  {65, 12815},
  {65, 13058},
  {7, 1714},
  {7, 1715},
  {7, 1711},
  {7, 1713},
  {7, 1707},
  {7, 1708},
  {7, 1712},
  {7, 1710},
  {7, 1498},
  {7, 1709},
  {6, 13177},
  {0, 0},
};

static const count_and_card_t copies_mystical_archive[64] = {
  {3, 16838},
  {3, 11422},
  {3, 30},
  {3, 11739},
  {3, 14532},
  {3, 10527},
  {3, 62},
  {3, 12220},
  {3, 6552},
  {3, 15531},
  {3, 2748},
  {3, 9237},
  {3, 5725},
  {3, 17331},
  {3, 1281},
  {6, 22951},
  {6, 1547},
  {6, 1549},
  {6, 48},
  {6, 55},
  {6, 19850},
  {6, 10408},
  {6, 1552},
  {6, 20094},
  {6, 12174},
  {6, 2905},
  {6, 13450},
  {6, 8566},
  {6, 19642},
  {6, 1243},
  {6, 10958},
  {6, 8592},
  {6, 145},
  {6, 1328},
  {6, 8803},
  {6, 1455},
  {6, 1506},
  {6, 202},
  {6, 10465},
  {6, 236},
  {6, 240},
  {6, 6589},
  {6, 11696},
  {6, 5173},
  {6, 20259},
  {25, 18341},
  {25, 20947},
  {25, 20551},
  {25, 11110},
  {25, 14251},
  {25, 22312},
  {25, 3755},
  {25, 21569},
  {25, 20355},
  {25, 9395},
  {25, 5051},
  {25, 18988},
  {25, 1147},
  {25, 22497},
  {25, 4255},
  {25, 20778},
  {25, 21701},
  {25, 21188},
  {-1, 0},
};

static const count_and_card_t copies_neo_land[16] = {
  {4, 188},
  {4, 126},
  {4, 239},
  {4, 164},
  {4, 91},
  {1, 14230},
  {1, 14234},
  {1, 14255},
  {1, 14301},
  {1, 14355},
  {1, 14371},
  {1, 14399},
  {1, 14406},
  {1, 14413},
  {1, 14432},
  {-1, 0},
};

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26eb8e0; original functions/packs.c:3158. */
static void get_csvid_choices(EXPANSION_t expansion, csvid_choices_t *choices);

static int make_pack_normal(EXPANSION_t expansion, int booster, int *out_csvids, int testing_mode)
{
  draftrarity_t recipe_rarity;
  int result, selected, value;
  const Rarities *rarities;
  const rarity_num_t *recipe_entry;
  unsigned int copies_or_probability, rarity_or_count;
  const rarity_num_t *contents =
      booster ? pack_contents[expansion].booster : pack_contents[expansion].starter;
  csvid_choices_t choices[44];
  for (int index = 0; index < 44; ++index)
  {
    choices[index].num = -1;
    choices[index].csvids = NULL;
  }
  for (const rarity_num_t *entry = contents; entry->rarity != XNONE; ++entry)
  {
    if (entry->rarity == MAX_RARITY_VAL_USED)
    {
      if (entry[1].rarity == XNONE || entry[1].rarity == MAX_RARITY_VAL_USED)
        backtrace_and_abort(
            "functions/packs.c:3175\nrn[i + 1].rarity != NONE && rn[i + 1].rarity != VAL_IFELSE");
      if (entry[2].rarity == XNONE || entry[2].rarity == MAX_RARITY_VAL_USED)
        backtrace_and_abort(
            "functions/packs.c:3176\nrn[i + 2].rarity != NONE && rn[i + 2].rarity != VAL_IFELSE");
    }
    else
    {
      if (entry->rarity > MAX_RARITY_VAL_USED)
        backtrace_and_abort("functions/packs.c:3180\nrn[i].rarity <= MAX_RARITY_VAL_USED");
      choices[entry->rarity].num = 0;
    }
  }
  get_csvid_choices(expansion, choices);
  bool no_late_duplicates = true;
  int attempts_left = ((testing_mode == 0) ? 100000U : 100U);
  unsigned int skip_dfc_check = expansion - EXPANSION_INNISTRAD_MIDNIGHT_HUNT & 0xfffffffd;
  bool accepted = false;
  do
  {
    recipe_rarity = contents->rarity;
    rarity_or_count = 0;
    if (recipe_rarity == XNONE)
    {
      if (skip_dfc_check)
      {
        accepted = true;
        break;
      }
    }
    else
    {
      value = 0;
      rarity_or_count = 0;
      recipe_entry = contents;
      do
      {
        copies_or_probability = recipe_entry->num;
        if (recipe_rarity == MAX_RARITY_VAL_USED)
        {
          if (contents[value + 1].num == contents[value + 2].num)
          {
            if (0 < contents[value + 1].num)
            {
              result = 0;
              do
              {
                selected = internal_rand(copies_or_probability >> 0x10);
                recipe_rarity =
                    contents[((int)(copies_or_probability & 0xffff) <= selected) + 1 + value]
                        .rarity;
                selected =
                    add_card_to_pack(out_csvids, rarity_or_count, &choices[recipe_rarity], 1,
                                     recipe_rarity, expansion, booster, testing_mode);
                rarity_or_count = (rarity_or_count + 1) - (unsigned int)(selected == 0);
                result = result + 1;
              } while (result < contents[value + 1].num);
            }
          }
          else
          {
            result = internal_rand(copies_or_probability >> 0x10);
            recipe_entry = contents + ((int)(copies_or_probability & 0xffff) <= result) + 1 + value;
            recipe_rarity = recipe_entry->rarity;
            result = add_card_to_pack(out_csvids, rarity_or_count, &choices[recipe_rarity], recipe_entry->num,
                             recipe_rarity, expansion, booster, testing_mode);
            if (result != 0)
            {
              rarity_or_count = rarity_or_count + recipe_entry->num;
            }
          }
          value = value + 2;
        }
        else
        {
          result = add_card_to_pack(out_csvids, rarity_or_count, &choices[recipe_rarity], copies_or_probability,
                           recipe_rarity, expansion, booster, testing_mode);
          if (result != 0)
          {
            rarity_or_count = rarity_or_count + recipe_entry->num;
          }
        }
        value = value + 1;
        recipe_entry = contents + value;
        recipe_rarity = recipe_entry->rarity;
      } while (recipe_rarity != XNONE);

      bool duplicate = false;
      for (int index = 0; index < (int)rarity_or_count - 1 && !duplicate; ++index)
      {
        int csvid = out_csvids[index];
        if (csvid == CARD_ID_SWAMP || csvid == CARD_ID_MOUNTAIN || csvid == CARD_ID_FOREST || csvid == CARD_ID_ISLAND || csvid == CARD_ID_PLAINS ||
            ((unsigned int)csvid - 0x834 < 5 && expansion == EXPANSION_ICE_AGE))
          continue;
        for (int other = index + 1; other < (int)rarity_or_count; ++other)
          if (csvid == out_csvids[other])
          {
            if (testing_mode)
            {
              fprintf(stderr,
                      "Expansion #%d, booster=%d: found a duplicate card late in "
                      "make_pack_normal()\012",
                      expansion, booster);
              no_late_duplicates = false;
            }
            duplicate = true;
            break;
          }
      }
      if (!duplicate)
      {
        if (skip_dfc_check)
        {
          accepted = true;
          break;
        }
        if ((int)rarity_or_count > 0)
        {
          int noncommon_dfcs = 0;
          for (int *entry = out_csvids; entry != out_csvids + rarity_or_count; ++entry)
          {
            rarities = get_rarity(*entry);
            if (rarities->dfc_symbol != 0xff)
              noncommon_dfcs += (unsigned char)(rarities->rarity_in_expansion[expansion] - 2) < 3;
          }
          if (noncommon_dfcs == 1)
          {
            accepted = true;
            break;
          }
        }
      }
    }
  } while (--attempts_left != 0);
  if (!accepted)
    rarity_or_count = 0;
  for (int index = 0; index < 44; ++index)
    if (choices[index].csvids)
      free(choices[index].csvids);
  if (testing_mode && !no_late_duplicates)
    rarity_or_count = 0;
  return rarity_or_count;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26ec9b0; original functions/packs.c:3292. */
int make_pack(EXPANSION_t expansion, int booster, int *out_csvids)
{
  int valid;
  int count;
  int *out_csvids_00;
  FILE *output;

  valid = is_valid_pack(expansion, booster);
  count = 0;
  if (valid != 0)
  {
    if (out_csvids != NULL)
    {
      return make_pack_normal(expansion, booster, out_csvids, 0);
    }
    count = pack_size(expansion, booster);
    out_csvids_00 = malloc(count * 4);
    if (out_csvids_00 == NULL)
    {
      fatal("could not malloc %d bytes to test out_csvids", count * 4);
    }
    count = make_pack_normal(expansion, booster, out_csvids_00, 1);
    free(out_csvids_00);
    if (count == 0)
    {
      output = stderr;
      fprintf(output, "Expansion #%d, booster=%d: could not create valid pack.\012", expansion,
              booster);
    }
  }
  return count;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26eca90; original functions/packs.c:3339. */
int test_packs(int run_time_test)
{
  static int once = 0;
  if (once)
    return 1;
  once = 1;
  int started = 0;
  if (run_time_test > 0)
    started = time_in_ms();
  int packs_made = 0;
  int all_valid = 1;
  for (EXPANSION_t expansion = EXPANSION_PROMO;
       expansion != (EXPANSION_STREETS_OF_NEW_CAPENNA | EXPANSION_LIMITED_EDITION_ALPHA);
       expansion += EXPANSION_PROMO)
  {
    EXPANSION_t recorded = pack_contents[expansion].expansion;
    if (recorded == expansion)
    {
      for (int booster = 0; booster < 2; ++booster)
        if (pack_size(expansion, booster))
        {
          int repetitions = run_time_test > 0 ? run_time_test : 1;
          int remaining = repetitions;
          do
          {
            if (!make_pack(expansion, booster, NULL))
              all_valid = 0;
          } while (--remaining != 0);
          packs_made += repetitions;
        }
    }
    else
      fprintf(stderr, "pack_contents[%d].expansion == %d\012", expansion, recorded);
  }
  if (run_time_test > 0)
  {
    int finished = time_in_ms();
    float elapsed = (float)(int32_t)((uint32_t)finished - (uint32_t)started) / 1000.0;
    fprintf(stderr, "Made %d packs in %f seconds (%f seconds each).\012", packs_made,
            (double)elapsed, (double)(elapsed / (float)packs_made));
  }
  fflush(stderr);
  return all_valid;
}

#endif

#ifndef SHANDALAR
#include <windows.h>
typedef struct pack_selection_t { EXPANSION_t expansion; int booster; } pack_selection_t;
typedef struct choose_a_pack_args_t {
  int num_packs, allow_boosters, allow_starters, allow_cancel;
  pack_selection_t results[6];
} choose_a_pack_args_t;
#define pack_path_duel_art global_duelart_path
#define pack_str_ok gs_ok_00924800
#define pack_str_cancel gs_localized_cancel
#define pack_main_hinstance g_app_instance
#define pack_hwnd_MainClass g_duel_window_hwnd
#define pack_dblbfr_hdc g_shared_offscreen_dc
#define pack_critical_section_for_painting &g_card_render_lock
#define pack_read_graphics_file load_pic
#define pack_set_palette_and_stretch_mode ApplyCardArtPaletteToDc
#define pack_blt_pic DrawBitmapToRect
#define pack_handle_palette_msgs handle_button_palette_message
#define pack_draw_button draw_owner_draw_button_centered
__attribute__((dllimport)) void blt_expansion_symbol(HDC hdc, const RECT *rect, int expansion, int rarity, int force_color);

#endif

#ifndef SHANDALAR
static int raw_has_subtype(csvid_t csvid, subtype_t subt)
{
  const card_ptr_t *cp = cards_ptr[csvid];
  for (int i = 0; i < 7; ++i)
    if (cp->types[i] == subt)
      return 1;
  return 0;
}

#endif

#ifndef SHANDALAR
static void get_csvid_choices(EXPANSION_t expansion, csvid_choices_t *choices)
{
  static const int contraption_copies[3] = {16, 7, 2};
  _Static_assert(sizeof(Rarities) == 5 && offsetof(Rarities, rarity_in_expansion) == 5,
                 "native rarity byte tail");
  _Static_assert(offsetof(card_ptr_t, types) == 132 &&
                     offsetof(card_ptr_t, mana_source_colors) == 146,
                 "native seven subtypes");
  uint8_t base_rarity;
  unsigned char symbol_or_index;
  bool more_copies;
  csvid_choices_t *pool;
  int16_t *subtype_ptr;
  int value;
  const Rarities *rarities;
  unsigned int copies_or_probability, current_csvid, pool_value, rarity_or_count,
      has_special_rarities;
  varying_copies_entry_t varying_copies[19] = {
      {7, copies_common_arabian_nights},
      {8, copies_uncommon_arabian_nights},
      {9, copies_common_antiquities},
      {10, copies_uncommon_antiquities},
      {13, copies_common_the_dark},
      {14, copies_uncommon_the_dark},
      {15, copies_common_fallen_empires},
      {16, copies_uncommon_fallen_empires},
      {17, copies_common_chronicles},
      {18, copies_uncommon_chronicles},
      {19, copies_common_homelands},
      {20, copies_uncommon_homelands},
      {21, copies_common_alliances},
      {22, copies_uncommon_alliances},
      {23, copies_rare_alliances},
      {35, copies_guildgate_or_shockland},
      {41, copies_mystical_archive},
      {42, copies_neo_land},
      {0, NULL},
  };
  if (((((expansion & ~EXPANSION_FUTURE_SIGHT) == EXPANSION_VINTAGE_MASTERS) ||
        (expansion == EXPANSION_LEGENDS)) ||
       (expansion == EXPANSION_PLANAR_CHAOS)) ||
      (expansion == EXPANSION_UNSTABLE))
  {
    has_special_rarities = 1;
  }
  else
  {
    has_special_rarities = 1;
    if (((expansion - EXPANSION_INNISTRAD & 0xfffffffd) != 0) &&
        ((expansion & ~EXPANSION_LIMITED_EDITION_ALPHA) != EXPANSION_SHADOWS_OVER_INNISTRAD))
    {
      has_special_rarities =
          (unsigned int)((expansion - EXPANSION_INNISTRAD_MIDNIGHT_HUNT & 0xfffffffb) == 0);
    }
  }

  if (available_slots > 1)
    for (current_csvid = 0; (int)current_csvid < available_slots - 1; ++current_csvid)
    {
      rarities = get_rarity(current_csvid);
      rarity_or_count = rarities->rarity_in_expansion[expansion];
      if (!rarity_or_count || !is_valid_card(current_csvid))
        continue;
      symbol_or_index = rarities->dfc_symbol;
      if (symbol_or_index != 0xff && (symbol_or_index & 1))
        continue;
      if (has_special_rarities)
      {
        if (expansion == EXPANSION_SHADOWS_OVER_INNISTRAD || expansion == EXPANSION_ELDRITCH_MOON)
        {
          if (symbol_or_index != 0xff)
          {
            if (rarity_or_count - 1 < 2)
              rarity_or_count = 0x1c;
            else if (rarity_or_count - 3 < 2)
              rarity_or_count = 0x1d;
          }
        }
        else if (expansion == EXPANSION_INNISTRAD_MIDNIGHT_HUNT ||
                 expansion == EXPANSION_INNISTRAD_CRIMSON_VOW)
        {
          if (rarity_or_count == 1 && symbol_or_index != 0xff)
            rarity_or_count = 0x1e;
        }
        else if (expansion == EXPANSION_KAMIGAWA_NEON_DYNASTY)
        {
          if (symbol_or_index != 0xff && rarity_or_count - 1 < 2)
            rarity_or_count = 0x1c;
        }
        else if (expansion == EXPANSION_UNSTABLE)
        {
          for (int index = 0; index < 7; ++index)
            if (cards_ptr[current_csvid]->types[index] == 0x3000)
            {
              rarity_or_count = 0x1f;
              break;
            }
        }
        else if (expansion == EXPANSION_INNISTRAD || expansion == EXPANSION_DARK_ASCENSION)
        {
          if (symbol_or_index != 0xff)
            rarity_or_count = 0x1b;
        }
        else if (expansion == EXPANSION_VINTAGE_MASTERS && rarity_or_count == 4)
        {
          if (((int)current_csvid < 0xaa &&
               ((int)current_csvid > 0xa4 || (current_csvid & 0xffffffef) == 1)) ||
              current_csvid - 0xf9 < 2)
            rarity_or_count = 6;
        }
        else if (expansion == EXPANSION_PLANAR_CHAOS && rarity_or_count - 1 < 3 &&
                 (rarities->flags & 1))
          rarity_or_count += 0x17;
        else if (expansion == EXPANSION_LEGENDS && rarity_or_count - 1 < 2)
        {
          copies_or_probability = has_special_rarities;
          bool default_weight = false;
          switch (current_csvid)
          {
          case 0x22d:
          case 0x236:
          case 0x23a:
          case 0x240:
          case 0x261:
          case 0x270:
          case 0x271:
          case 0x273:
          case 0x279:
          case 0x285:
          case 0x28b:
          case 0x28f:
          case 0x29b:
          case 0x29d:
          case 0x2a0:
          case 0x2a7:
          case 0x2e0:
          case 0x2ee:
          case 0x2f5:
          case 0x301:
          case 0x311:
          case 0x31b:
          case 800:
          case 0x32d:
          case 0x337:
          case 0x344:
          case 0x347:
          case 0x348:
          case 0x34b:
            copies_or_probability = 0;
            break;
          default:
            default_weight = true;
            if (rarity_or_count == 1)
            {
              value = 0x58;
            }
            else
            {
              value = 0x60;
              copies_or_probability = 0;
            }
            break;
          case 0x299:
          case 0x2b6:
          case 0x2c9:
          case 0x2e4:
          case 0x2ff:
          case 0x30c:
          case 0x333:
          case 0x33e:
          case 0x341:;
          }
          if (!default_weight)
            value = (unsigned int)(rarity_or_count != 1) * 8 + 0x58;
          draftrarity_t munged = value / sizeof(csvid_choices_t);
          if (choices[munged].num < 0 || choices[rarity_or_count].num >= 0)
            backtrace_and_abort(
                "functions/packs.c:2636\nchoices[munged].num >= 0 && choices[rar].num < 0");
          do
          {
            --copies_or_probability;
            add_to_csvid_choices(&choices[munged], current_csvid);
          } while (copies_or_probability != 0xffffffff);
          continue;
        }
      }
      for (unsigned int pool_index = 1; pool_index < 0x2b; ++pool_index)
      {
        pool_value = pool_index;
        pool = choices + pool_index;
        if (pool->num < 0)
          continue;
        switch (pool_value)
        {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 0x18:
        case 0x19:
        case 0x1a:
        case 0x1e:
          if (rarity_or_count == pool_value)
          {
            add_to_csvid_choices(pool, current_csvid);
            break;
          }
          break;
        case 0x1b:
          if (rarity_or_count == 0x1b)
          {
            pool_value = (unsigned int)rarities->rarity_in_expansion[expansion];
            if (expansion == EXPANSION_INNISTRAD)
            {
              value = 0;
              if (1 < pool_value - 3)
              {
                value = (unsigned int)(pool_value != 2) * 9 + 2;
              }
            }
            else if (pool_value == 4)
            {
              value = 2;
            }
            else if (pool_value == 3)
            {
              value = 0xd;
            }
            else
            {
              value = (pool_value == 2 ? 35U : 119U);
            }
            do
            {
              value = value + -1;
              add_to_csvid_choices(pool, current_csvid);
            } while (value != -1);
          }
          break;
        case 0x1c:
          if (rarity_or_count == 0x1c)
          {
            if (expansion == EXPANSION_SHADOWS_OVER_INNISTRAD)
            {
              pool_value = (rarities->rarity_in_expansion[expansion] == '\x01' ? 14 : 0);
            }
            else
            {
              pool_value = (unsigned int)(rarities->rarity_in_expansion[expansion] == '\x01');
              if (expansion == EXPANSION_ELDRITCH_MOON)
              {
                pool_value = (pool_value ? 14U : 1U);
              }
              else
              {
                pool_value = pool_value * 3;
              }
            }
            do
            {
              pool_value = pool_value - 1;
              add_to_csvid_choices(pool, current_csvid);
            } while (pool_value != 0xffffffff);
          }
          break;
        case 0x1d:
          if (rarity_or_count == 0x1d)
          {
            if (expansion == EXPANSION_SHADOWS_OVER_INNISTRAD)
            {
              value = (unsigned int)(rarities->rarity_in_expansion[expansion] == '\x03') * 5 + 1;
            }
            else
            {
              value = (unsigned int)(rarities->rarity_in_expansion[expansion] == '\x01') * 9 + 4;
            }
            do
            {
              value = value + -1;
              add_to_csvid_choices(pool, current_csvid);
            } while (value != -1);
          }
          break;
        case 0x1f:
          if (rarity_or_count == 0x1f)
          {
            symbol_or_index = rarities->rarity_in_expansion[expansion] - 2;
            if (symbol_or_index < 3)
            {
              if (contraption_copies[(unsigned int)symbol_or_index] < 1)
                break;
              value = contraption_copies[(unsigned int)symbol_or_index] + -1;
            }
            else
            {
              value = 0x2a;
            }
            do
            {
              add_to_csvid_choices(pool, current_csvid);
              more_copies = 0 < value;
              value = value + -1;
            } while (more_copies);
          }
          break;
        case 0x20:
          if (current_csvid == 0x7e)
          {
            add_to_csvid_choices(pool, 0x7e);
          }
          break;
        case 0x21:
          if (rarity_or_count == 5)
          {
            add_to_csvid_choices(pool, current_csvid);
            bool snow = false;
            subtype_ptr = cards_ptr[current_csvid]->types;
            do
            {
              if (*subtype_ptr == 0x2003)
              {
                snow = true;
                break;
              }
              ++subtype_ptr;
            } while (cards_ptr[current_csvid]->types + 7 != subtype_ptr);
            if (!snow)
            {
              add_to_csvid_choices(pool, current_csvid);
              add_to_csvid_choices(pool, current_csvid);
            }
          }
          break;
        case 0x22:
          add_to_csvid_choices(pool, current_csvid);
          break;
        case 0x24:
        {
          bool found_subtype = false;
          for (int subtype_index = 0; subtype_index < 7; ++subtype_index)
            if (cards_ptr[current_csvid]->types[subtype_index] == 0x500c)
            {
              found_subtype = true;
              break;
            }
          if (!found_subtype)
            break;
          if (current_csvid != 0x4bac)
          {
            add_to_csvid_choices(pool, current_csvid);
          }
          break;
        }
        case 0x25:
        {
          value = get_internal_card_id_from_csv_id(current_csvid);
          if ((cards_data[value].type & 6) != 0)
          {
            bool found_subtype = false;
            for (int subtype_index = 0; subtype_index < 7; ++subtype_index)
              if (cards_ptr[current_csvid]->types[subtype_index] == 0x2001)
              {
                found_subtype = true;
                break;
              }
            if (!found_subtype)
              break;
            base_rarity = rarities->rarity_in_expansion[expansion];
            value = 0;
            if (base_rarity != '\x04')
            {
              if (base_rarity == '\x03')
              {
                value = 7;
              }
              else
              {
                if (base_rarity == '\x06')
                  break;
                value = 0x13;
              }
            }
            do
            {
              value = value + -1;
              add_to_csvid_choices(pool, current_csvid);
            } while (value != -1);
          }
          break;
        }
        case 0x26:
        case 0x27:
          if (pool_value == 0x26)
          {
            if (rarity_or_count != 3)
              break;
          }
          else if (rarity_or_count != 4)
            break;
          bool legendary = false;
          for (int subtype_index = 0; subtype_index < 7; ++subtype_index)
            if (cards_ptr[current_csvid]->types[subtype_index] == 0x2001)
            {
              legendary = true;
              break;
            }
          if (legendary)
            break;
          value = get_internal_card_id_from_csv_id(current_csvid);
          if ((cards_data[value].type & 6) == 0)
            add_to_csvid_choices(pool, current_csvid);
          break;
        case 0x28:
        {
          bool found_subtype = false;
          for (int subtype_index = 0; subtype_index < 7; ++subtype_index)
            if (cards_ptr[current_csvid]->types[subtype_index] == 0x6003)
            {
              found_subtype = true;
              break;
            }
          if (!found_subtype)
            break;
          add_to_csvid_choices(pool, current_csvid);
          break;
        }
        }
      }
    }
  for (varying_copies_entry_t *entry = varying_copies; entry->counts_and_cards; ++entry)
  {
    draftrarity_t rarity = entry->rarity;
    const count_and_card_t *card = entry->counts_and_cards;
    if (choices[rarity].num >= 0 && card->count > 0)
      do
      {
        if (is_valid_card(card->csvid) && card->count > 0)
        {
          int count = 0;
          do
          {
            ++count;
            add_to_csvid_choices(&choices[rarity], card->csvid);
          } while (count < card->count);
        }
        ++card;
      } while (card->count > 0);
  }
  if ((choices[5].num == 0) &&
      ((((((expansion & ~EXPANSION_LIMITED_EDITION_ALPHA) == EXPANSION_CONFLUX ||
           (expansion == EXPANSION_WORLDWAKE)) ||
          (expansion == EXPANSION_DARK_ASCENSION)) ||
         (((expansion - EXPANSION_BORN_OF_THE_GODS & 0xfffffffd) == 0 ||
           (expansion == EXPANSION_GATECRASH)))) ||
        (((expansion - EXPANSION_OATH_OF_THE_GATEWATCH & 0xfffffffb) == 0 ||
          (expansion == EXPANSION_AETHER_REVOLT))))))
  {
    add_to_csvid_choices(&choices[5], 0xbc);
    add_to_csvid_choices(&choices[5], 0x7e);
    add_to_csvid_choices(&choices[5], 0xef);
    add_to_csvid_choices(&choices[5], 0xa4);
    add_to_csvid_choices(&choices[5], 0x5b);
  }
}

#endif

#ifndef SHANDALAR
static int allow_duplicates(EXPANSION_t expansion, csvid_t csvid)
{
  switch (csvid)
  {
    case CARD_ID_FOREST:
    case CARD_ID_ISLAND:
    case CARD_ID_MOUNTAIN:
    case CARD_ID_PLAINS:
    case CARD_ID_SWAMP:
      return 1;
    default:
      return (unsigned int)(csvid - CARD_ID_SNOW_COVERED_FOREST) <= 4U &&
             expansion == EXPANSION_ICE_AGE;
  }
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
/* ManalinkEh 2022: 0x26ed780; original functions/packs.c:3449. */
LPCDLGTEMPLATE generate_choose_a_pack_dlg_template(void)
{
  static unsigned char templ[294] __attribute__((aligned(4))) = {
    0x40, 0x08, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba, 0x00,
    0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x4d, 0x00, 0x53, 0x00, 0x20, 0x00,
    0x53, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x73, 0x00, 0x20, 0x00, 0x53, 0x00, 0x65, 0x00, 0x72, 0x00,
    0x69, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x23, 0x50, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x05, 0x00, 0xac, 0x00, 0x48, 0x00, 0x4d, 0x04, 0xff, 0xff, 0x85, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x23, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x14, 0x00,
    0xac, 0x00, 0x48, 0x00, 0x4e, 0x04, 0xff, 0xff, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x13, 0x02, 0x23, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x23, 0x00, 0xac, 0x00, 0x48, 0x00,
    0x4f, 0x04, 0xff, 0xff, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x23, 0x50,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x32, 0x00, 0xac, 0x00, 0x48, 0x00, 0x50, 0x04, 0xff, 0xff,
    0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x23, 0x50, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x41, 0x00, 0xac, 0x00, 0x48, 0x00, 0x51, 0x04, 0xff, 0xff, 0x85, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x23, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x50, 0x00,
    0xac, 0x00, 0x48, 0x00, 0x52, 0x04, 0xff, 0xff, 0x85, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x5f, 0x00, 0x32, 0x00, 0x0e, 0x00,
    0x01, 0x00, 0xff, 0xff, 0x80, 0x00, 0x4f, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x63, 0x00, 0x5f, 0x00, 0x32, 0x00, 0x0e, 0x00,
    0x02, 0x00, 0xff, 0xff, 0x80, 0x00, 0x43, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x63, 0x00, 0x65, 0x00,
    0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  return (LPCDLGTEMPLATE)templ;
}

/* ManalinkEh 2022: 0x26ecc50; original functions/packs.c:3506. */
static INT_PTR __stdcall dlgproc_choose_a_pack(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  static HGDIOBJ winbk_questn = NULL;
  static const char **expansion_names = NULL;
  static HBRUSH brush = NULL;
  static HPEN pen1 = NULL;
  static HPEN pen2 = NULL;
  static choose_a_pack_args_t *args = NULL;
  _Static_assert(sizeof(choose_a_pack_args_t) == 64 &&
                     offsetof(choose_a_pack_args_t, results) == 16,
                 "native six-pack argument record");
  _Static_assert(sizeof(DRAWITEMSTRUCT) == 48 && offsetof(DRAWITEMSTRUCT, hDC) == 24 &&
                     offsetof(DRAWITEMSTRUCT, rcItem) == 28 &&
                     offsetof(DRAWITEMSTRUCT, itemData) == 44,
                 "native owner-draw record");
  _Static_assert(sizeof(RECT) == 16, "drawing rectangle");
  if (msg == WM_COMMAND)
  {
    unsigned command = (uint16_t)wparam;
    if (command != 1 && (command != 2 || args->allow_cancel == 0))
      return 0;
    for (int i = 0; i < 6; ++i)
    {
      args->results[i].expansion = EXPANSION_0;
      args->results[i].booster = 0;
    }
    for (int i = 0; i < args->num_packs; ++i)
    {
      if (command == 1)
      {
        LRESULT selection = SendDlgItemMessageA(hwnd, i + 0x44d, 0x147, 0, 0);
        if (selection != -1)
        {
          LRESULT data = SendDlgItemMessageA(hwnd, i + 0x44d, 0x150, selection, 0);
          if (data != -1)
          {
            args->results[i].expansion = (uint32_t)data & 0xffff;
            args->results[i].booster = ((uint32_t)data & 0x10000) == 0;
          }
        }
      }
    }
    if (winbk_questn != NULL)
    {
      DeleteObject(winbk_questn);
      winbk_questn = NULL;
    }
    if (brush != NULL)
    {
      DeleteObject(brush);
      brush = NULL;
    }
    if (pen1 != NULL)
    {
      DeleteObject(pen1);
      pen1 = NULL;
    }
    if (pen2 != NULL)
    {
      DeleteObject(pen2);
      pen2 = NULL;
    }
    EndDialog(hwnd, command == 1);
    return 0;
  }
  if (msg == WM_INITDIALOG)
  {
    char path[279];
    sprintf(path, "%s\\WINBK_QuestN.pic", pack_path_duel_art);
    winbk_questn = pack_read_graphics_file(path);
    brush = CreateSolidBrush(0x100001c);
    pen1 = CreatePen(0, 0, 0x1000072);
    pen2 = CreatePen(0, 0, 0x10000ca);
    SetDlgItemTextA(hwnd, 1, pack_str_ok);
    SetDlgItemTextA(hwnd, 2, pack_str_cancel);
    args = (choose_a_pack_args_t *)lparam;
    if (args->num_packs > 6)
      args->num_packs = 6;
    else if (args->num_packs < 1)
      args->num_packs = 1;
    if (args->allow_cancel == 0)
      ShowWindow(GetDlgItem(hwnd, 2), 0);
    for (int i = args->num_packs; i < 6; ++i)
      ShowWindow(GetDlgItem(hwnd, i + 0x44d), 0);
    int num_textlines;
    expansion_names = load_text_nocopy_from_file("Menus.txt", "EXPANSIONNAMES", &num_textlines);
    for (int expansion = 1; expansion < 198 && num_textlines > expansion + 6; ++expansion)
    {
      char buf[300];
      if (args->allow_boosters != 0 && is_valid_pack(expansion, 1))
      {
        int size = pack_size(expansion, 1);
        const char *prefix = "14-";
        if ((((uint32_t)expansion - 103) & 0xfffffffd) != 0 &&
            ((uint32_t)expansion & 0xfffffffd) != 145)
          prefix = "";
        sprintf(buf, "%s (%s%d cards)", expansion_names[expansion + 7], prefix, size);
        for (int i = 0; i < args->num_packs; ++i)
        {
          LRESULT result = SendDlgItemMessageA(hwnd, i + 0x44d, 0x143, 0, (LPARAM)buf);
          if ((uint32_t)result + 2 > 1)
            SendDlgItemMessageA(hwnd, i + 0x44d, 0x151, result, expansion);
        }
      }
      if (args->allow_starters != 0 && is_valid_pack(expansion, 0))
      {
        int size = pack_size(expansion, 0);
        sprintf(buf, "%s %s deck (%d cards)", expansion_names[expansion + 7],
                expansion < 30 ? "starter" : "tournament", size);
        for (int i = 0; i < args->num_packs; ++i)
        {
          LRESULT result = SendDlgItemMessageA(hwnd, i + 0x44d, 0x143, 0, (LPARAM)buf);
          if ((uint32_t)result + 2 > 1)
            SendDlgItemMessageA(hwnd, i + 0x44d, 0x151, result, expansion | 0x10000);
        }
      }
    }
    for (int i = 0; i < args->num_packs; ++i)
      SendDlgItemMessageA(hwnd, i + 0x44d, 0x14e, 0, 0);
    SetFocus(GetDlgItem(hwnd, 0x44d));
    return 0;
  }
  if (msg == WM_DRAWITEM)
  {
    DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lparam;
    if (wparam < 3)
    {
      if (wparam == 0)
        return 0;
      COLORREF color = ((uint32_t)(dis->hwndItem == GetFocus()) - 1U & 0xffffffdbU) + 0x10000bfU;
      pack_draw_button(dis, brush, pen1, pen2, color, 0);
      return 1;
    }
    if ((uint32_t)wparam - 0x44d > 5 || dis->itemID == (UINT)-1)
      return 0;
    unsigned expansion = (uint16_t)dis->itemData;
    COLORREF old_fg = SetTextColor(dis->hDC, GetSysColor(8));
    COLORREF old_bk = SetBkColor(dis->hDC, GetSysColor(5));
    TEXTMETRICA tm;
    GetTextMetricsA(dis->hDC, &tm);
    int bottom = dis->rcItem.bottom;
    int top = dis->rcItem.top;
    char buf[300];
    SendMessageA(dis->hwndItem, 0x148, dis->itemID, (LPARAM)buf);
    int width = (bottom - top) * 2;
    ExtTextOutA(dis->hDC, width + 4, (bottom + top - tm.tmHeight) / 2, 6, &dis->rcItem, buf,
                strlen(buf), NULL);
    SetTextColor(dis->hDC, old_fg);
    SetBkColor(dis->hDC, old_bk);
    RECT rect;
    rect.left = dis->rcItem.left;
    rect.top = dis->rcItem.top;
    rect.right = rect.left + width;
    rect.bottom = dis->rcItem.bottom;
    FillRect(dis->hDC, &rect, GetSysColorBrush(5));
    blt_expansion_symbol(dis->hDC, &rect, expansion, (dis->itemState & 0x11) == 0 ? 1 : 4, -1);
    if ((dis->itemState & 0x11) != 0)
      DrawFocusRect(dis->hDC, &dis->rcItem);
    return 1;
  }
  if (msg == WM_ERASEBKGND && winbk_questn != NULL)
  {
    RECT rect;
    pack_set_palette_and_stretch_mode((HDC)wparam);
    GetClientRect(hwnd, &rect);
    EnterCriticalSection(pack_critical_section_for_painting);
    int savedc = SaveDC(pack_dblbfr_hdc);
    if (winbk_questn == NULL)
      FillRect(pack_dblbfr_hdc, &rect, GetStockObject(2));
    else
      pack_blt_pic(pack_dblbfr_hdc, &rect, winbk_questn);
    RestoreDC(pack_dblbfr_hdc, savedc);
    GetClientRect(hwnd, &rect);
    BitBlt((HDC)wparam, 0, 0, rect.right, rect.bottom, pack_dblbfr_hdc, 0, 0, 0xcc0020);
    LeaveCriticalSection(pack_critical_section_for_painting);
    return 1;
  }
  if (msg == WM_LBUTTONDOWN)
  {
    SendMessageA(hwnd, 0x112, 0xf012, 0);
    return 0;
  }
  if ((uint32_t)msg - 0x30f < 3)
    return pack_handle_palette_msgs(hwnd, msg, (HWND)wparam, lparam);
  return 0;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x26ed790; original functions/packs.c:3737. */
int choose_packs(int num_packs, pack_flags_t flags, EXPANSION_t *expansions, int *is_booster)
{
  static int idx = -1;
  choose_a_pack_args_t args;
  if (num_packs > 6)
    num_packs = 6;
  else if (num_packs < 1)
    num_packs = 1;
  idx = (int32_t)((uint32_t)idx + 1U);
  args.num_packs = num_packs;
  args.allow_boosters = flags & PACK_ALLOW_BOOSTER;
  args.allow_starters = flags & PACK_ALLOW_STARTER;
  args.allow_cancel = flags & PACK_ALLOW_CANCEL;
  for (int i = 0; i < 6; ++i)
  {
    args.results[i].expansion = EXPANSION_0;
    args.results[i].booster = 0;
  }
  INT_PTR result =
      DialogBoxIndirectParamA(pack_main_hinstance, generate_choose_a_pack_dlg_template(),
                              pack_hwnd_MainClass, dlgproc_choose_a_pack, (LPARAM)&args);
  if (result != 0)
  {
    for (int i = 0; i < num_packs; ++i)
    {
      expansions[i] = args.results[i].expansion;
      if (is_booster != NULL)
        is_booster[i] = args.results[i].booster;
    }
    return 1;
  }
  return 0;
}
#endif
