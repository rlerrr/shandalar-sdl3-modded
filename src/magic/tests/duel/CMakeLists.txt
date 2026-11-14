include(FetchContent)
if(EXISTS "${CMAKE_SOURCE_DIR}/out/dependencies/googletest/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${CMAKE_SOURCE_DIR}/out/dependencies/googletest")
endif()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG 063de7e9578f82b369302001269680b4b1553359
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(duel_generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${duel_generated}")
option(SHANDALAR_DUEL_EXTENDED_TESTS "Register the full catalog smoke suite in CTest" OFF)
option(SHANDALAR_DUEL_AUTHORING_BUILD "Build a reusable engine without generating or registering the behavior catalog" OFF)
if(SHANDALAR_DUEL_AUTHORING_BUILD AND SHANDALAR_DUEL_EXTENDED_TESTS)
  message(FATAL_ERROR "SHANDALAR_DUEL_EXTENDED_TESTS requires a full catalog build; disable SHANDALAR_DUEL_AUTHORING_BUILD")
endif()
if(NOT SHANDALAR_DUEL_AUTHORING_BUILD)
file(GLOB_RECURSE duel_case_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/cases/*.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS mana_consequence_recipes.py mana_modes_recipes.py creature_conditions_recipes.py conditional_mana_land_recipes.py conditional_stats_recipes.py conditional_keyword_recipes.py duel_case_types.h)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS entry_stats_creature_recipes.py land_entry_mana_recipes.py land_search_recipes.py landcycling_recipes.py library_search_recipes.py data/library_search_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS removal_recipes.py data/removal_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  mana_spell_extension_recipes.py entry_value_extension_recipes.py static_effect_extension_recipes.py
  data/mana_effects_extension_witnesses.json data/entry_value_wave6_witnesses.json
  data/static_effect_extension_witnesses.json data/growth_counter_activation_wave6_witnesses.json
  data/removal_wave6_profiles.json data/library_search_wave6_profiles.json
  data/library_search_wave6_witnesses.json data/graveyard_recovery_wave6_witnesses.json
  data/token_creator_wave6_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  data/counterspell_wave7_witnesses.json data/prevention_wave7_witnesses.json
  data/edict_wave7_profiles.json data/graveyard_exile_wave7_profiles.json
  data/tap_spell_wave7_reviews.json data/equipment_wave7_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  combat_value_trigger_recipes.py data/combat_value_trigger_wave8_witnesses.json
  attack_value_trigger_recipes.py data/attack_value_trigger_wave8_witnesses.json
  death_value_trigger_recipes.py data/death_value_trigger_wave8_witnesses.json
  death_recurrence_recipes.py data/death_recurrence_wave8_witnesses.json
  cast_value_trigger_recipes.py data/cast_value_trigger_wave8_profiles.json
  entry_trigger_value_recipes.py data/entry_trigger_value_wave8_profiles.json
  data/entry_trigger_value_wave8_witnesses.json
  entry_trigger_value_handflow_recipes.py data/entry_trigger_value_wave8_handflow_profiles.json
  entry_trigger_value_selection_recipes.py data/entry_trigger_value_wave8_selection_profiles.json
  entry_trigger_value_gate_recipes.py data/entry_trigger_value_wave8_gate_profiles.json
  permanent_value_activation_recipes.py data/permanent_value_activation_wave8_witnesses.json
  activated_token_creation_recipes.py data/activated_token_creation_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  draw_value_trigger_recipes.py data/draw_value_trigger_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  death_counter_trigger_recipes.py data/death_counter_trigger_wave8_profiles.json
  battalion_trigger_recipes.py data/battalion_trigger_wave8_profiles.json
  entry_tutor_trigger_recipes.py data/entry_tutor_trigger_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  return_saboteur_trigger_recipes.py data/return_saboteur_trigger_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  charge_counter_activation_recipes.py data/charge_counter_activation_wave8_profiles.json
  death_counter_extended_recipes.py data/death_counter_extended_wave8_profiles.json
  entry_land_tutor_recipes.py data/entry_land_tutor_wave8_profiles.json
  cast_untap_trigger_recipes.py data/cast_untap_trigger_wave8_profiles.json
  mentor_attack_counter_recipes.py data/mentor_attack_counter_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  combat_library_prefix_recipes.py data/combat_library_prefix_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  attack_team_reward_recipes.py data/attack_team_reward_wave8_profiles.json
  combat_targeted_removal_recipes.py data/combat_targeted_removal_wave8_profiles.json
  cast_extort_trigger_recipes.py data/cast_extort_trigger_wave8_profiles.json
  permanent_plus1_activation_recipes.py data/permanent_plus1_activation_wave8_profiles.json
  entry_tap_trigger_recipes.py data/entry_tap_trigger_wave8_profiles.json
  death_self_destination_recipes.py data/death_self_destination_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  upkeep_value_trigger_recipes.py data/upkeep_value_trigger_wave8_profiles.json
  storm_effect_recipes.py data/storm_effect_wave8_profiles.json
  death_target_effect_recipes.py data/death_target_effect_wave8_profiles.json
  entry_linked_exile_recipes.py data/entry_linked_exile_wave8_profiles.json
  block_damage_destruction_recipes.py data/block_damage_destruction_wave8_profiles.json
  activated_temporary_pt_recipes.py data/activated_temporary_pt_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  upkeep_permanent_count_recipes.py data/upkeep_permanent_count_wave8_profiles.json
  death_player_value_recipes.py data/death_player_value_wave8_profiles.json
  entry_own_return_recipes.py data/entry_own_return_wave8_profiles.json
  cast_temporary_stat_trigger_recipes.py data/cast_temporary_stat_trigger_wave8_profiles.json
  activated_temporary_modifier_recipes.py data/activated_temporary_modifier_wave8_profiles.json
  block_stat_trigger_recipes.py data/block_stat_trigger_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  upkeep_hand_value_recipes.py data/upkeep_hand_value_wave8_profiles.json
  entry_aura_freeze_recipes.py data/entry_aura_freeze_wave8_profiles.json
  cast_counter_engine_recipes.py data/cast_counter_engine_wave8_profiles.json
  death_library_tutor_recipes.py data/death_library_tutor_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  cast_reload_tap_activation_recipes.py data/cast_reload_tap_activation_wave8_profiles.json
  upkeep_controlled_return_recipes.py data/upkeep_controlled_return_wave8_profiles.json
  aura_restriction_recipes.py data/aura_restriction_wave8_profiles.json
  death_paid_value_recipes.py data/death_paid_value_wave8_profiles.json
  conditional_blocking_legality_recipes.py data/conditional_blocking_legality_wave8_profiles.json
  printed_tap_mana_activation_recipes.py data/printed_tap_mana_activation_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  upkeep_echo_payment_recipes.py data/upkeep_echo_payment_wave8_profiles.json
  cast_conjunctive_base_stat_recipes.py data/cast_conjunctive_base_stat_wave8_profiles.json
  paid_unblockability_activation_recipes.py data/paid_unblockability_activation_wave8_profiles.json
  aura_mana_enhancement_recipes.py data/aura_mana_enhancement_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  counted_tap_mana_activation_recipes.py data/counted_tap_mana_activation_wave8_profiles.json
  death_modular_transfer_recipes.py data/death_modular_transfer_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  cascade_spell_effect_recipes.py data/cascade_spell_effect_wave8_profiles.json
  death_modular_composition_recipes.py data/death_modular_composition_wave8_profiles.json
  unblockability_composition_recipes.py data/unblockability_composition_wave8_profiles.json
  countdown_counter_lifecycle_recipes.py data/countdown_counter_lifecycle_wave8_profiles.json
  counter_condition_recurrence_recipes.py data/counter_condition_recurrence_wave8_profiles.json
  cumulative_upkeep_payment_recipes.py data/cumulative_upkeep_payment_wave8_profiles.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS hand_exchange_recipes.py data/hand_exchange_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS value_flow_recipes.py variable_value_recipes.py variable_damage_recipes.py case_archive.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS mass_removal_recipes.py data/mass_removal_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS removal_activation_recipes.py data/removal_activation_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS static_stats_recipes.py data/static_stats_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  etb_damage_creature_recipes.py data/etb_damage_witnesses.json
  death_damage_recipes.py data/death_damage_witnesses.json variable_sweep_recipes.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  growth_counter_spell_recipes.py
  counterspell_extension_recipes.py data/counterspell_extension_witnesses.json
  edict_recipes.py data/edict_witnesses.json
  growth_counter_activation_recipes.py data/growth_counter_activation_witnesses.json
  reanimation_recipes.py data/reanimation_witnesses.json
  token_trigger_creation_recipes.py token_trigger_creation_spec.py data/token_trigger_creation_witnesses.json
  prevention_recipes.py
  tap_spell_recipes.py data/tap_spell_reviews.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  token_spell_extension_recipes.py data/token_spell_extension_witnesses.json
  etb_removal_extension_recipes.py data/etb_removal_extension_witnesses.json
  graveyard_recovery_extension_recipes.py data/graveyard_recovery_extension_witnesses.json
  damage_spell_extension_recipes.py data/damage_spell_extension_witnesses.json
  discard_spell_extension_recipes.py data/discard_spell_extension_witnesses.json
  life_spell_extension_recipes.py data/life_spell_extension_witnesses.json
  aura_keyword_extension_recipes.py data/aura_keyword_extension_witnesses.json
  tribal_static_extension_recipes.py data/tribal_static_extension_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  graveyard_exile_recipes.py data/graveyard_exile_witnesses.json
  data/edict_extension_profiles.json data/edict_wave4_profiles.json data/edict_wave5_profiles.json data/prevention_extension_witnesses.json
  data/growth_counter_spell_extension_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  data/fixtures.json data/oracle.json data/oracle_aliases.json generate_cases.py case_files.py recipes.py destruction_recipes.py movement_recipes.py stat_recipes.py creature_recipes.py counter_recipes.py graveyard_recipes.py damage_recipes.py sweep_recipes.py grant_recipes.py pump_creature_recipes.py evasion_creature_recipes.py defensive_creature_recipes.py mana_creature_recipes.py regenerate_creature_recipes.py etb_value_creature_recipes.py blocking_creature_recipes.py tap_damage_creature_recipes.py combat_keyword_creature_recipes.py lord_creature_recipes.py entry_counter_creature_recipes.py cycling_recipes.py permanent_mana_recipes.py equipment_recipes.py
  value_activation_recipes.py mill_recipes.py mass_stats_recipes.py attack_pump_recipes.py trigger_pump_recipes.py hybrid_creature_recipes.py counter_damage_creature_recipes.py etb_removal_creature_recipes.py etb_recovery_creature_recipes.py death_value_creature_recipes.py passive_keyword_creature_recipes.py target_stats_activation_recipes.py target_keyword_activation_recipes.py self_keyword_activation_recipes.py token_creation_recipes.py tap_activation_recipes.py damage_activation_recipes.py ${duel_case_files}
  "${SHANDALAR_SRC_ROOT}/magic/src/carddata.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/tools/carddata_handler_overrides.json"
  "${CMAKE_SOURCE_DIR}/tools/import_carddata.py"
  "${CMAKE_SOURCE_DIR}/tools/import_duel_oracle.py"
  data/registry_kinds.json)
endif()
# Shared-engine rebuilds depend on compiled fixtures and registry tables only.
# Editing an author's card declarations or recipe module must not regenerate
# the entire catalog, or reconfigure another author's immutable engine.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  data/fixtures.json data/oracle.json data/oracle_aliases.json
  generate_cases.py case_files.py case_archive.py data/registry_kinds.json
  "${SHANDALAR_SRC_ROOT}/magic/src/carddata.c"
  "${CMAKE_SOURCE_DIR}/tools/carddata_handler_overrides.json"
  "${CMAKE_SOURCE_DIR}/tools/import_carddata.py"
  "${CMAKE_SOURCE_DIR}/tools/import_duel_oracle.py")
set(duel_generation_args)
if(SHANDALAR_DUEL_AUTHORING_BUILD)
  list(APPEND duel_generation_args --driver-only)
  message(STATUS "Duel authoring engine: generate worker archives separately and pass --duel-case-data at runtime")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_cases.py"
  --output "${duel_generated}" ${duel_generation_args} COMMAND_ERROR_IS_FATAL ANY)
set(duel_globals "${duel_generated}/duel_snapshot_globals.inc")
add_custom_command(OUTPUT "${duel_globals}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_snapshot_globals.py"
          "${SHANDALAR_SRC_ROOT}" "${duel_globals}"
  DEPENDS generate_snapshot_globals.py ../..//src/global_state.h ../../src/global_other.h
  VERBATIM)
include("${duel_generated}/duel_case_sources.cmake")
add_executable(duel_card_tests duel_card_tests.cpp duel_test_api.c headless_shell.c "${duel_globals}" ${duel_generated_case_sources})
set(SHANDALAR_DUEL_TEST_BINARY_DIR "" CACHE PATH "Optional output directory for large duel test binaries")
if(SHANDALAR_DUEL_TEST_BINARY_DIR)
  set_target_properties(duel_card_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SHANDALAR_DUEL_TEST_BINARY_DIR}")
endif()
target_include_directories(duel_card_tests PRIVATE "${duel_generated}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(duel_card_tests PRIVATE shandalar_duel_engine GTest::gtest shandalar_miniz)
if(MSVC)
  target_link_options(duel_card_tests PRIVATE /OPT:REF /errorlimit:0)
  target_compile_options(duel_card_tests PRIVATE "$<$<COMPILE_LANGUAGE:C>:/FI${SHANDALAR_SRC_ROOT}/manalinkeh/mok_manalink_redirects.h>")
else()
  target_compile_options(duel_card_tests PRIVATE "$<$<COMPILE_LANGUAGE:C>:-include;${SHANDALAR_SRC_ROOT}/manalinkeh/mok_manalink_redirects.h>")
endif()
if(EMSCRIPTEN)
  configure_file(node_case_data.js.in "${duel_generated}/node_case_data.js" @ONLY)
  target_compile_definitions(duel_card_tests PRIVATE DUEL_TEST_CASE_DATA="/test-cases/duel_cases.bin")
  set_target_properties(duel_card_tests PROPERTIES SUFFIX ".js")
  # Case bodies are loaded on demand; grow the engine arena when needed.
  target_link_options(duel_card_tests PRIVATE -O0 -sENVIRONMENT=node -sFETCH=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sASSERTIONS=1 -sFORCE_FILESYSTEM=1
    -sASYNCIFY=1 -lidbfs.js -lnodefs.js "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/node_results.js"
    "--pre-js=${duel_generated}/node_case_data.js")
  foreach(asset cards.dat Text.res uistrings.txt MP_UIStrings.txt prompts.txt)
    target_link_options(duel_card_tests PRIVATE "--preload-file=${SHANDALAR_MODDED_ASSET_DIR}/${asset}@/assets_modded/${asset}")
  endforeach()
  if(CMAKE_CROSSCOMPILING_EMULATOR)
    list(GET CMAKE_CROSSCOMPILING_EMULATOR 0 duel_node)
  else()
    find_program(duel_node NAMES node node.exe REQUIRED)
  endif()
  set(duel_node_args --node "${duel_node}")
  set(duel_batch_jobs 2)
else()
  set(duel_batch_jobs 4)
  target_compile_definitions(duel_card_tests PRIVATE DUEL_TEST_ASSETS="${SHANDALAR_MODDED_ASSET_DIR}"
    DUEL_TEST_CASE_DATA="${duel_generated}/duel_cases.bin")
endif()
if(SHANDALAR_DUEL_AUTHORING_BUILD)
  add_custom_target(duel_tests_batched
    COMMAND "${CMAKE_COMMAND}" -E echo "duel_tests_batched requires the full catalog. Generate a worker archive and run run_cases.py with --case-data and --manifest, or configure SHANDALAR_DUEL_AUTHORING_BUILD=OFF."
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM)
else()
include("${duel_generated}/cases.cmake")
add_custom_target(duel_tests_batched
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run_cases.py"
    --binary "$<TARGET_FILE:duel_card_tests>" ${duel_node_args}
    --manifest "${duel_generated}/manifest.json"
    --baseline "${CMAKE_CURRENT_SOURCE_DIR}/data/known_failures.json"
    --output "${CMAKE_CURRENT_BINARY_DIR}/batch-results"
    --batch-size 128 --jobs ${duel_batch_jobs} --timeout 30 --quiet
  DEPENDS duel_card_tests
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>"
  USES_TERMINAL VERBATIM)
endif()
if(TARGET shandalar_llvm_coverage)
  target_link_libraries(duel_card_tests PRIVATE shandalar_llvm_coverage)
endif()
if(NOT SHANDALAR_DUEL_AUTHORING_BUILD)
add_test(NAME duel.presentation_guard COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/verify_presentation_guard.py"
  --binary "$<TARGET_FILE:duel_card_tests>" ${duel_node_args})
set_tests_properties(duel.presentation_guard PROPERTIES LABELS "duel;harness" TIMEOUT 15)
endif()
add_test(NAME duel.case_files COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_case_files.py")
set_tests_properties(duel.case_files PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.case_archive COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_case_archive.py")
set_tests_properties(duel.case_archive PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.counter_observation COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_counter_observation.py")
set_tests_properties(duel.counter_observation PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.input_observations COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_input_observations.py")
set_tests_properties(duel.input_observations PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.recipes COMMAND "${Python3_EXECUTABLE}"
  -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*recipes.py")
set_tests_properties(duel.recipes PROPERTIES LABELS "duel;harness" TIMEOUT 180)
add_test(NAME duel.growth_counter_spell_witnesses COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_growth_counter_spell_witnesses.py")
set_tests_properties(duel.growth_counter_spell_witnesses PROPERTIES LABELS "duel;harness" TIMEOUT 15)

add_test(NAME duel.handler_overrides COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_SOURCE_DIR}/tools/test_import_carddata.py")
set_tests_properties(duel.handler_overrides PROPERTIES LABELS "duel;harness" TIMEOUT 15)

add_test(NAME duel.exe_carddata_import COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_SOURCE_DIR}/tools/test_import_carddata_exe.py")
set_tests_properties(duel.exe_carddata_import PROPERTIES LABELS "duel;harness" TIMEOUT 15)

add_test(NAME duel.runner_expectations COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_run_cases.py")
set_tests_properties(duel.runner_expectations PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.runner_batches COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_batch_cases.py")
set_tests_properties(duel.runner_batches PROPERTIES LABELS "duel;harness" TIMEOUT 30)
add_test(NAME duel.template_oracle COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_template_oracle.py")
set_tests_properties(duel.template_oracle PROPERTIES LABELS "duel;harness" TIMEOUT 15)

foreach(counter_case IN ITEMS
    RealChargeCostsAndSnapshotsPreserveCountsWithoutChangingQueryState
    RealLoyaltyEntryAndActivationCostsReplayForBothControllers)
  if(EMSCRIPTEN)
    add_test(NAME "duel.counter.${counter_case}" COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
      "--gtest_filter=CounterObservation.${counter_case}")
  else()
    add_test(NAME "duel.counter.${counter_case}" COMMAND duel_card_tests
      "--gtest_filter=CounterObservation.${counter_case}")
  endif()
  set_tests_properties("duel.counter.${counter_case}" PROPERTIES LABELS "duel;engine" TIMEOUT 30
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
endforeach()

foreach(input_case IN ITEMS
    TriggerSourceSelection.LiveSourceDefersAcrossEarlierWindowsAndReplaysWithoutLegacyAmbiguity
    OptionalTargetReceipt.RealCancellationAndMandatoryFallbackRetainPolicyAndSnapshotCounts
    ActivationModeIntent.MenusAutomaticValuesAndManaFallbacksStayDistinctAcrossSnapshots
    ActivationHeuristics.DisabledGraveyardModeDoesNotPromptBeforeTradingPostChoice)
  if(EMSCRIPTEN)
    add_test(NAME "duel.input.${input_case}" COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
      "--gtest_filter=${input_case}")
  else()
    add_test(NAME "duel.input.${input_case}" COMMAND duel_card_tests "--gtest_filter=${input_case}")
  endif()
  set_tests_properties("duel.input.${input_case}" PROPERTIES LABELS "duel;engine" TIMEOUT 30
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
endforeach()

if(NOT SHANDALAR_DUEL_AUTHORING_BUILD)
if(EMSCRIPTEN)
  add_test(NAME duel.summoning_sickness COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
    --gtest_filter=SummoningSickness.SuspendHasteEndsOnControlLoss)
  add_test(NAME duel.characteristics COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
    --gtest_filter=CharacteristicValues.*)
else()
  add_test(NAME duel.summoning_sickness COMMAND duel_card_tests
    --gtest_filter=SummoningSickness.SuspendHasteEndsOnControlLoss)
  add_test(NAME duel.characteristics COMMAND duel_card_tests --gtest_filter=CharacteristicValues.*)
endif()
set_tests_properties(duel.characteristics PROPERTIES LABELS "duel;engine" TIMEOUT 15
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
set_tests_properties(duel.summoning_sickness PROPERTIES LABELS "duel;engine" TIMEOUT 15
  WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
foreach(untap_case IN ITEMS
    RealPreparationAndSnapshotsPreserveTheOneStepBoundary
    FloatingPoolsAndPendingSpellsRejectWithoutChangingTheBoard
    TheFullWrapperHonorsARealSkippedUntap)
  if(EMSCRIPTEN)
    add_test(NAME "duel.untap_step.${untap_case}" COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
      "--gtest_filter=UntapStep.${untap_case}")
  else()
    add_test(NAME "duel.untap_step.${untap_case}" COMMAND duel_card_tests
      "--gtest_filter=UntapStep.${untap_case}")
  endif()
  set_tests_properties("duel.untap_step.${untap_case}" PROPERTIES LABELS "duel;engine" TIMEOUT 15
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
endforeach()
foreach(flashback_case IN ITEMS
    QueryPreservesCallerChoicesAndRealCastCompletesUnderSnapshotReplay
    CostTimingAndPendingStackDenialsLeaveCallerStateUntouched
    ActualMenuSelectsTheRequestedDuplicateAmongOtherLegalSpells)
  if(EMSCRIPTEN)
    add_test(NAME "duel.flashback.${flashback_case}" COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
      "--gtest_filter=Flashback.${flashback_case}")
  else()
    add_test(NAME "duel.flashback.${flashback_case}" COMMAND duel_card_tests
      "--gtest_filter=Flashback.${flashback_case}")
  endif()
  set_tests_properties("duel.flashback.${flashback_case}" PROPERTIES LABELS "duel;engine" TIMEOUT 15
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
endforeach()
foreach(observation_case IN ITEMS
    CastAnnouncement.RealHistoryAndCounterResolutionSurviveBothSnapshotBoundaries
    CardCallbackAbi.RegisteredHandlersUseThreeArgumentsForBothControllers
    FiresOfYavimaya.ExileNotificationUsesIidWhileSurvivorStillGrantsHaste
    FiresOfYavimaya.SacrificePumpKeepsCostsAndTemporaryEffect
    MysticDecree.CanonicalCallbackCastRemovesFlyingForBothControllers
    TokenCharacteristics.RealReplacementTokensExposeApeAndFrogLizard
    GraveyardProvenance.LookupIgnoresUnusedSlotsIncludingSourceZero
    GraveyardProvenance.LookupKeepsNewestLiveMatchAndIgnoresGenerationBits
    ShuffleObservation.ActualEntriesCountTheLibraryOwnerForBothPathsAndSmallDecks
    ShuffleObservation.TestAndBothAiSnapshotsRestoreCountsWithoutRefundingWork
    ShuffleObservation.RealTutorShufflesAfterResolutionAndReplaysUnderSnapshot
    TokenCharacteristics.GeneratedWolvesExposeWolfWithoutChangingOtherSubtypes)
  if(EMSCRIPTEN)
    add_test(NAME "duel.observation.${observation_case}" COMMAND "${duel_node}" "$<TARGET_FILE:duel_card_tests>"
      "--gtest_filter=${observation_case}")
  else()
    add_test(NAME "duel.observation.${observation_case}" COMMAND duel_card_tests
      "--gtest_filter=${observation_case}")
  endif()
  set_tests_properties("duel.observation.${observation_case}" PROPERTIES LABELS "duel;engine" TIMEOUT 15
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:duel_card_tests>")
endforeach()
endif()
