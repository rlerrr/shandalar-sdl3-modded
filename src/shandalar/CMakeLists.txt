include("${SHANDALAR_SRC_ROOT}/magic/duel_sources.cmake")

# The adventure/duel still uses the original bitmap, font, database, and sound
# helpers. Keep those utilities without pulling in the original deckbuilder
# or card renderers.
add_library(shandalar_ui_support STATIC
  ../deckdll/src/buttonclass.c
  ../deckdll/src/card_db.c
  ../deckdll/src/drm.c
  ../deckdll/src/magsnd_sdl_adapter.c
  ../deckdll/src/shared_resources.c
  ../drawcardlib/src/drawing.c
)
target_include_directories(shandalar_ui_support PUBLIC "${SHANDALAR_SRC_ROOT}")
target_compile_definitions(shandalar_ui_support PRIVATE
  MODERN_FIXES
  create_fonts=deckdll_create_fonts
  destroy_create_fonts_resources=deckdll_destroy_create_fonts_resources
  is_card_available_in_installed_expansion=shandalar_support_card_available
  card_coded=shandalar_support_card_coded
)
target_link_libraries(shandalar_ui_support PUBLIC drawcard_modded magsnd_static)

set(shandalar_version_header
  "${CMAKE_CURRENT_BINARY_DIR}/generated/shandalar_build_version.h"
)
add_custom_target(shandalar_version_header_target
  COMMAND "${CMAKE_COMMAND}"
          "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
          "-DOUTPUT_FILE=${shandalar_version_header}"
          -P "${CMAKE_SOURCE_DIR}/cmake/GenerateGitVersion.cmake"
  BYPRODUCTS "${shandalar_version_header}"
  VERBATIM
)

add_library(shandalar_duel_engine STATIC
  src/shandalar.c
  src/adventure_input.c
  src/adventure_main_menu.c
  src/adventure_new_game.c
  src/card_pool.c
  src/campaign_card_pool.c
  src/adventure_save.c
  src/adventure_sound.c
  src/adventure_ui.c
  src/world_generation.c
  src/opt57_shandalar_core.c
  src/opt57_shandalar_only.c
  src/opt57_shandalar_ui_fx.c
  src/openingMenu.c
  src/adventureWorldUi.c
  src/world_lair_monster_slots.c
  src/world_lair_monster_encounter.c
  src/visitLocation.c
  src/dungeonEncounter.c
  src/adventureStats.c
  src/mapScreen.c
  src/cityInfoScreen.c
  src/dungeonCluesScreen.c
  src/hintText.c
  src/adventureListPicker.c
  src/iniFile.c
  src/magic_runtime.c
  src/modded_card_rendering.c
  src/manalinkeh_host_abi.c
  src/manalinkeh_host_bridge.c
  src/shandalar_global_strings.c
  ${SHANDALAR_MAGIC_DUEL_SOURCES}
  ../magic/src/global_duel_ui_ids.c
  ../magic/src/global_other.c
  ../magic/src/global_state.c
  ../magic/src/global_strings.c
  ../magic/src/shared_startup.c
  ../magic/src/carddata.c
  ../magic/src/duel_host.c
)
add_executable(shandalar src/sdl_main.c)
target_link_libraries(shandalar PRIVATE shandalar_duel_engine)
if(WIN32 AND NOT EMSCRIPTEN)
  set(shandalar_icon_resource "${CMAKE_CURRENT_BINARY_DIR}/shandalar_icon.rc")
  file(GENERATE OUTPUT "${shandalar_icon_resource}"
    CONTENT "1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/shandalar.ico\"\n"
  )
  set(shandalar_icon_object "${CMAKE_CURRENT_BINARY_DIR}/shandalar_icon.res")
  add_custom_command(OUTPUT "${shandalar_icon_object}"
    COMMAND "${CMAKE_RC_COMPILER}" /nologo /fo "${shandalar_icon_object}" "${shandalar_icon_resource}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shandalar.ico"
    VERBATIM
  )
  add_custom_target(shandalar_icon DEPENDS "${shandalar_icon_object}")
  add_dependencies(shandalar shandalar_icon)
  set_source_files_properties("${shandalar_icon_object}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  target_sources(shandalar PRIVATE "${shandalar_icon_object}")
endif()
add_dependencies(shandalar_duel_engine shandalar_version_header_target)

target_compile_definitions(shandalar_duel_engine PUBLIC
  MODERN_FIXES
  SHANDALAR
  SHANDALAR_NATIVE_ASSET_ROOT="${SHANDALAR_MODDED_ASSET_DIR}"
  LoadPcxIntoPageNoPalette=ShandalarLoadPcxIntoPageNoPalette
)
if(MSVC)
  target_compile_options(shandalar_duel_engine PRIVATE
    /Gy
    "/FI${SHANDALAR_SRC_ROOT}/manalinkeh/mok_manalink_redirects.h"
  )
else()
  target_compile_options(shandalar_duel_engine PRIVATE
    "SHELL:-include ${SHANDALAR_SRC_ROOT}/manalinkeh/mok_manalink_redirects.h"
  )
endif()
# Non-Windows libcs lack MSVC's extras (_itoa, _stricmp, ...).
if(NOT WIN32)
  target_compile_options(shandalar_duel_engine PRIVATE
    "SHELL:-include platform/legacy_io.h"
  )
endif()
target_include_directories(shandalar_duel_engine PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_include_directories(shandalar_duel_engine BEFORE PUBLIC
  "${SHANDALAR_SRC_ROOT}/magic/compat"
)
target_link_libraries(shandalar_duel_engine PUBLIC
  manalinkeh_static
  deckdll_modded
  cardart_modded
  drawcard_modded
  shandalar_ui_support
  facemaker_legacy_ui
  deck_imgui_interaction
  legacy_win32_imgui_bridge
  magsnd_static
  statwin_static
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
)
if(TARGET shandalar_llvm_coverage)
  # PUBLIC so the profile runtime reaches the duel_card_tests/shandalar links.
  target_link_libraries(shandalar_duel_engine PUBLIC shandalar_llvm_coverage)
  target_link_libraries(shandalar_ui_support PRIVATE shandalar_llvm_coverage)
endif()
if(MSVC)
  target_link_options(shandalar PRIVATE /errorlimit:0 /OPT:REF)
endif()

if(EMSCRIPTEN)
  set_target_properties(shandalar PROPERTIES SUFFIX ".html")
  target_link_options(shandalar PRIVATE
    "-Wl,--fatal-warnings"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sMAXIMUM_MEMORY=1GB"
    "-sSTACK_SIZE=1MB"
    "-sASYNCIFY=1"
    # Experimentally, duel UI suspension needs this much Asyncify stack.
    "-sASYNCIFY_STACK_SIZE=262144"
    "-sEXIT_RUNTIME=1"
    "-sFETCH=1"
    "-lidbfs.js"
  )
  if(SHANDALAR_MODDED_ASSET_DIR)
    set(shandalar_opening_preload_assets
      advButtons.txt
      MENUBAK.PIC
      AdvStrings.txt
      FaceButtons.txt
      FaceData.txt   
      uistrings.txt
      advblocks.txt
      hints.txt
      concise.csv
      TT0127M_.TTF
      TT0298M_.TTF
      TT0300M_.TTF
      TT0530M_.TTF
      TT0085M_.TTF
      TT0299M_.TTF
      Magis___.TTF
      Magim___.TTF
      fonts.cv
      DuelArt/Duel.dat
      duelpalall.tr
      duel.plogpal
      cards.dat
      cardart/manasymbols.pic
      cardart/cardsets.pic
      cardart/cardback.pic
      sound/kwalkl.wav
      sound/kwalkr.wav
      sound/bwalkl.wav
      sound/bwalkr.wav
      sound/gwalkl.wav
      sound/gwalkr.wav
      sound/rwalkl.wav
      sound/rwalkr.wav
      sound/wwalkl.wav
      sound/wwalkr.wav
      sound/kbird1.wav
      sound/bbird1.wav
      sound/gbird1.wav
      sound/rbird1.wav
      sound/wbird1.wav
      todpal.tr
      advfac64.pic
      dbox.spr
      begin.spr
    )
    include("${SHANDALAR_SRC_ROOT}/deck/DeckModdedStartupAssets.cmake")
    if(EXISTS "${SHANDALAR_MODDED_ASSET_DIR}/Shandalar.ini")
      list(APPEND shandalar_opening_preload_assets Shandalar.ini)
    endif()
    list(APPEND shandalar_opening_preload_assets
      ${deck_modded_startup_assets}
      rarity.dat
      Deckbuilder.csv
      MENUS.TXT
      menusx1.txt
      CUECARDS.TXT
      MP_UIStrings.txt
      Legacy.csv
      Rarity.csv
      prompts.txt
      Text.res
      cardart/damage.pic
      cardart/cardcounters.pic
      cardart/abilities.pic
      cardart/manastripes.pic
      cardart/summon.pic
      cardart/dying.pic
      cardart/target.pic
      cardart/canttarget.pic
      cardart/willuntap.pic
      DuelSounds/DRAW.WAV
      DuelSounds/DISCARD.WAV
      DuelSounds/BUTTON.WAV
      DuelSounds/CANCEL.WAV
    )
    # Keep every eagerly loaded renderer frame in the shared deck manifest.
    # Adventure art, music, portraits, and resolution-specific sprites use
    # legacy_io's on-demand fetching rather than enlarging the startup bundle.
    # Windows source paths are case-insensitive; the browser VFS isn't. Avoid
    # packaging the same file twice under different directory/file spellings.
    set(shandalar_unique_preload_assets "")
    set(shandalar_preload_keys "")
    foreach(shandalar_asset IN LISTS shandalar_opening_preload_assets)
      string(TOLOWER "${shandalar_asset}" shandalar_asset_key)
      if(NOT shandalar_asset_key MATCHES "(^|/)(thumbs\\.db|desktop\\.ini)$|\\.(exe|dll|pdb|bat|cmd)$"
          AND NOT shandalar_asset_key IN_LIST shandalar_preload_keys)
        list(APPEND shandalar_preload_keys "${shandalar_asset_key}")
        list(APPEND shandalar_unique_preload_assets "${shandalar_asset}")
      endif()
    endforeach()
    set(shandalar_opening_preload_assets ${shandalar_unique_preload_assets})
    download_assets(assets_modded "${SHANDALAR_MODDED_ASSET_DIR}" ${shandalar_opening_preload_assets})
    set(shandalar_preload_response "")
    foreach(shandalar_preload_entry IN LISTS shandalar_opening_preload_assets)
      set_property(TARGET shandalar APPEND PROPERTY LINK_DEPENDS
        "${SHANDALAR_MODDED_ASSET_DIR}/${shandalar_preload_entry}")
      string(APPEND shandalar_preload_response
        "--preload-file\n\"${SHANDALAR_MODDED_ASSET_DIR}/${shandalar_preload_entry}@/assets_modded/${shandalar_preload_entry}\"\n"
      )
    endforeach()
    set(shandalar_preload_response_file "${CMAKE_CURRENT_BINARY_DIR}/shandalar-preload.rsp")
    file(GENERATE OUTPUT "${shandalar_preload_response_file}" CONTENT "${shandalar_preload_response}")
    set_property(TARGET shandalar APPEND PROPERTY LINK_DEPENDS "${shandalar_preload_response_file}")
    target_link_options(shandalar PRIVATE "@${shandalar_preload_response_file}")
  endif()
endif()

install(TARGETS shandalar RUNTIME DESTINATION .)

if(SHANDALAR_BUILD_TESTS AND SHANDALAR_BUILD_DUEL_TESTS)
  add_subdirectory(../magic/tests/duel "${CMAKE_CURRENT_BINARY_DIR}/duel_tests")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(mok_legacy_effect_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mok_legacy_effect.c")
add_custom_command(OUTPUT "${mok_legacy_effect_source}"
  COMMAND "${Python3_EXECUTABLE}" "${SHANDALAR_SRC_ROOT}/manalinkeh/tests/select_legacy_effect_functions.py"
          "${SHANDALAR_SRC_ROOT}" "${mok_legacy_effect_source}"
  DEPENDS ../manalinkeh/tests/select_legacy_effect_functions.py
          ../manalinkeh/functions/functions.c ../manalinkeh/cards/fifth_dawn.c
          src/manalinkeh_host_bridge.c ../magic/src/carddata.c
  VERBATIM
)
add_executable(mok_legacy_effect_test EXCLUDE_FROM_ALL
  ../manalinkeh/tests/mok_legacy_effect_test.c "${mok_legacy_effect_source}")
target_include_directories(mok_legacy_effect_test PRIVATE
  "${SHANDALAR_SRC_ROOT}/manalinkeh" "${SHANDALAR_SRC_ROOT}")
target_compile_definitions(mok_legacy_effect_test PRIVATE MANALINKEH_STATIC)
if(EMSCRIPTEN)
  target_include_directories(mok_legacy_effect_test BEFORE PRIVATE "${SHANDALAR_SRC_ROOT}/magic/compat")
  target_link_libraries(mok_legacy_effect_test PRIVATE shandalar_platform)
  set_target_properties(mok_legacy_effect_test PROPERTIES SUFFIX ".js")
  target_link_options(mok_legacy_effect_test PRIVATE -sENVIRONMENT=node -sEXIT_RUNTIME=1)
elseif(SHANDALAR_BUILD_TESTS)
  add_test(NAME mok_legacy_effect_test COMMAND mok_legacy_effect_test)
endif()
if(SHANDALAR_BUILD_TESTS)
  set_property(TARGET mok_legacy_effect_test PROPERTY EXCLUDE_FROM_ALL FALSE)
endif()

if(SHANDALAR_BUILD_TESTS AND WIN32)
  add_executable(card_pool_test tests/card_pool_test.c src/card_pool.c)
  target_include_directories(card_pool_test PRIVATE "${SHANDALAR_SRC_ROOT}")
  target_link_libraries(card_pool_test PRIVATE shandalar_platform)
  add_test(NAME card_pool_test COMMAND card_pool_test)
  set_tests_properties(card_pool_test PROPERTIES TIMEOUT 20)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(campaign_test_source "${CMAKE_CURRENT_BINARY_DIR}/generated/campaign_functions.c")
  add_custom_command(OUTPUT "${campaign_test_source}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/select_campaign_functions.py"
            "${SHANDALAR_SRC_ROOT}" "${campaign_test_source}"
    DEPENDS tests/select_campaign_functions.py src/adventure_new_game.c src/shandalar.c src/visitLocation.c
            ../magic/src/carddata.c ../magic/src/duel.c ../magic/src/duel_card_state.c
            ../magic/src/duel_engine_main_wndproc.c ../magic/src/shared_startup.c
            ../magic/src/duel_ai_support.c
            ../magic/src/cardlist.c
            ../magic/src/duel_stack.c
    VERBATIM
  )
  add_executable(campaign_new_game_test tests/campaign_new_game_test.c "${campaign_test_source}"
                 src/card_pool.c src/campaign_card_pool.c ../deckdll/src/card_db.c)
  target_include_directories(campaign_new_game_test PRIVATE "${SHANDALAR_SRC_ROOT}")
  target_include_directories(campaign_new_game_test BEFORE PRIVATE "${SHANDALAR_SRC_ROOT}/magic/compat")
  target_compile_definitions(campaign_new_game_test PRIVATE SHANDALAR MODERN_FIXES)
  target_link_libraries(campaign_new_game_test PRIVATE shandalar_platform)
  if(EXISTS "${SHANDALAR_MODDED_ASSET_DIR}/concise.csv")
    add_test(NAME campaign_new_game_test COMMAND campaign_new_game_test "${SHANDALAR_MODDED_ASSET_DIR}")
    set_tests_properties(campaign_new_game_test PROPERTIES TIMEOUT 20)
  endif()
  set(mok_test_engine_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mok_test_engine.c")
  add_custom_command(OUTPUT "${mok_test_engine_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SHANDALAR_SRC_ROOT}/magic/tests/select_engine_functions.py"
            "${SHANDALAR_SRC_ROOT}/magic/src" "${mok_test_engine_source}"
    DEPENDS ../magic/tests/select_engine_functions.py ../magic/src/duel.c
            ../magic/src/duel_card_state.c ../magic/src/duel_stack.c
            ../magic/src/duel_ai.c ../magic/src/duel_main_phase_combat_damage.c ../magic/src/duel_main_phase.c
            ../magic/src/dialog.c ../magic/src/global_state.c
            ../magic/src/duel_engine_lifeclass_wndproc.c
    VERBATIM
  )
  set(mok_ai_bridge_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mok_ai_bridge.c")
  add_custom_command(OUTPUT "${mok_ai_bridge_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SHANDALAR_SRC_ROOT}/manalinkeh/tests/select_ai_bridge_functions.py"
            "${SHANDALAR_SRC_ROOT}/manalinkeh/functions" "${mok_ai_bridge_source}"
    DEPENDS ../manalinkeh/tests/select_ai_bridge_functions.py
            ../manalinkeh/functions/ai.c ../manalinkeh/functions/targets.c
            ../manalinkeh/functions/events.c
    VERBATIM
  )
  set_source_files_properties("${mok_ai_bridge_source}" PROPERTIES
    COMPILE_DEFINITIONS MANALINKEH_STATIC
    INCLUDE_DIRECTORIES "${SHANDALAR_SRC_ROOT}/manalinkeh"
  )
  add_executable(mok_engine_test ../magic/tests/mok_engine_test.c "${mok_test_engine_source}"
                 "${mok_ai_bridge_source}")
  target_include_directories(mok_engine_test PRIVATE "${SHANDALAR_SRC_ROOT}")
  target_include_directories(mok_engine_test BEFORE PRIVATE "${SHANDALAR_SRC_ROOT}/magic/compat")
  target_compile_definitions(mok_engine_test PRIVATE SHANDALAR MODERN_FIXES)
  target_link_libraries(mok_engine_test PRIVATE shandalar_platform)
  target_compile_options(mok_engine_test PRIVATE
    "/FI${SHANDALAR_SRC_ROOT}/manalinkeh/mok_manalink_redirects.h")
  add_test(NAME mok_engine_test COMMAND mok_engine_test)

  set(mok_legacy_text_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mok_legacy_text.c")
  add_custom_command(OUTPUT "${mok_legacy_text_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SHANDALAR_SRC_ROOT}/manalinkeh/tests/select_legacy_text_functions.py"
            "${SHANDALAR_SRC_ROOT}/manalinkeh/functions/functions.c" "${mok_legacy_text_source}"
    DEPENDS ../manalinkeh/tests/select_legacy_text_functions.py ../manalinkeh/functions/functions.c
    VERBATIM
  )
  add_executable(mok_legacy_text_test ../manalinkeh/tests/mok_legacy_text_test.c "${mok_legacy_text_source}")
  target_include_directories(mok_legacy_text_test PRIVATE "${SHANDALAR_SRC_ROOT}/manalinkeh")
  target_compile_definitions(mok_legacy_text_test PRIVATE MANALINKEH_STATIC)
  add_test(NAME mok_legacy_text_test COMMAND mok_legacy_text_test)

  set(mok_ui_text_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mok_ui_text.c")
  add_custom_command(OUTPUT "${mok_ui_text_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SHANDALAR_SRC_ROOT}/manalinkeh/tests/select_ui_text_initializers.py"
            "${SHANDALAR_SRC_ROOT}/manalinkeh/functions/windows.c"
            "${SHANDALAR_SRC_ROOT}/manalinkeh/functions/has_creature_type.c" "${mok_ui_text_source}"
    DEPENDS ../manalinkeh/tests/select_ui_text_initializers.py ../manalinkeh/functions/windows.c
            ../manalinkeh/functions/has_creature_type.c
    VERBATIM
  )
  set_source_files_properties("${mok_ui_text_source}" PROPERTIES
    COMPILE_DEFINITIONS MANALINKEH_STATIC
    INCLUDE_DIRECTORIES "${SHANDALAR_SRC_ROOT}/manalinkeh"
  )
  add_executable(global_strings_test
    ../magic/tests/global_strings_test.c
    ../magic/src/global_strings.c
    "${mok_ui_text_source}"
  )
  target_link_libraries(global_strings_test PRIVATE shandalar_platform)
  add_test(NAME global_strings_test COMMAND global_strings_test)
endif()
