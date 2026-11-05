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
file(GLOB_RECURSE duel_case_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/cases/*.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS mana_consequence_recipes.py mana_modes_recipes.py creature_conditions_recipes.py conditional_mana_land_recipes.py conditional_stats_recipes.py conditional_keyword_recipes.py duel_case_types.h)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS entry_stats_creature_recipes.py land_entry_mana_recipes.py land_search_recipes.py landcycling_recipes.py library_search_recipes.py data/library_search_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS removal_recipes.py data/removal_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS hand_exchange_recipes.py data/hand_exchange_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS value_flow_recipes.py variable_value_recipes.py variable_damage_recipes.py case_archive.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS mass_removal_recipes.py data/mass_removal_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS removal_activation_recipes.py data/removal_activation_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS static_stats_recipes.py data/static_stats_witnesses.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  data/fixtures.json data/oracle.json data/oracle_aliases.json generate_cases.py case_files.py recipes.py destruction_recipes.py movement_recipes.py stat_recipes.py creature_recipes.py counter_recipes.py graveyard_recipes.py damage_recipes.py sweep_recipes.py grant_recipes.py pump_creature_recipes.py evasion_creature_recipes.py defensive_creature_recipes.py mana_creature_recipes.py regenerate_creature_recipes.py etb_value_creature_recipes.py blocking_creature_recipes.py tap_damage_creature_recipes.py combat_keyword_creature_recipes.py lord_creature_recipes.py entry_counter_creature_recipes.py cycling_recipes.py permanent_mana_recipes.py equipment_recipes.py
  value_activation_recipes.py mill_recipes.py mass_stats_recipes.py attack_pump_recipes.py trigger_pump_recipes.py hybrid_creature_recipes.py counter_damage_creature_recipes.py etb_removal_creature_recipes.py etb_recovery_creature_recipes.py death_value_creature_recipes.py passive_keyword_creature_recipes.py target_stats_activation_recipes.py target_keyword_activation_recipes.py self_keyword_activation_recipes.py token_creation_recipes.py tap_activation_recipes.py damage_activation_recipes.py ${duel_case_files}
  "${SHANDALAR_SRC_ROOT}/magic/src/carddata.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/tools/carddata_handler_overrides.json"
  "${CMAKE_SOURCE_DIR}/tools/import_carddata.py"
  "${CMAKE_SOURCE_DIR}/tools/import_duel_oracle.py"
  data/registry_kinds.json)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_cases.py"
  --output "${duel_generated}" COMMAND_ERROR_IS_FATAL ANY)
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
  # The generated scenario tables and engine state require a 1 GiB initial arena.
  target_link_options(duel_card_tests PRIVATE -O0 -sENVIRONMENT=node -sFETCH=1 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=256MB -sSTACK_SIZE=4MB -sASSERTIONS=1 -sFORCE_FILESYSTEM=1
    -lnodefs.js "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/node_results.js"
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
if(TARGET shandalar_llvm_coverage)
  target_link_libraries(duel_card_tests PRIVATE shandalar_llvm_coverage)
endif()
add_test(NAME duel.presentation_guard COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/verify_presentation_guard.py"
  --binary "$<TARGET_FILE:duel_card_tests>" ${duel_node_args})
set_tests_properties(duel.presentation_guard PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.case_files COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_case_files.py")
set_tests_properties(duel.case_files PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.case_archive COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/test_case_archive.py")
set_tests_properties(duel.case_archive PROPERTIES LABELS "duel;harness" TIMEOUT 15)
add_test(NAME duel.recipes COMMAND "${Python3_EXECUTABLE}"
  -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*recipes.py")
set_tests_properties(duel.recipes PROPERTIES LABELS "duel;harness" TIMEOUT 60)

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
