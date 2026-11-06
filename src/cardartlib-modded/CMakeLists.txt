add_library(cardart_modded STATIC
  image_loader.c
  pic_assert.c
  src/main.cpp
  "${SHANDALAR_SRC_ROOT}/drawcardlib/src/pcxw.c"
  "${SHANDALAR_SRC_ROOT}/drawcardlib/src/pic.c"
)

target_include_directories(cardart_modded PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(cardart_modded PRIVATE
  SHANDALAR_SDL_WIN32_COMPAT
  SHANDALAR_MODDED_LIBS
  SHANDALAR_MODDED_PIC_LOADER
  DllMain=CardArtModded_DllMain
  assert=modded_pic_assert
  assert_noabort=modded_pic_assert_noabort
)
if(SHANDALAR_BUILD_SHANDALAR)
  # The shared PIC decoder must update the adventure's indexed-color palette.
  target_compile_definitions(cardart_modded PRIVATE SHANDALAR)
endif()
if(MSVC)
  target_compile_options(cardart_modded PRIVATE /Zc:__cplusplus)
endif()
target_link_libraries(cardart_modded PUBLIC
  shandalar_platform
  rpbits
  SDL3_image::SDL3_image
)
if(TARGET shandalar_llvm_coverage)
  target_link_libraries(cardart_modded PRIVATE shandalar_llvm_coverage)
endif()

# A tiny disk-format fixture catches pointer-size regressions without game assets.
add_executable(cardart_name_table_test EXCLUDE_FROM_ALL tests/name_table_test.cpp)
target_include_directories(cardart_name_table_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(cardart_name_table_test PRIVATE SHANDALAR_SDL_WIN32_COMPAT)
target_link_libraries(cardart_name_table_test PRIVATE shandalar_platform SDL3_image::SDL3_image)
if(SHANDALAR_BUILD_TESTS AND NOT EMSCRIPTEN)
  add_test(NAME cardart_name_table_test COMMAND cardart_name_table_test
    "${CMAKE_CURRENT_BINARY_DIR}/cardart-name-table-test.dat")
endif()
