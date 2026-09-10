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
)
if(MSVC)
  target_compile_options(cardart_modded PRIVATE /Zc:__cplusplus)
endif()
target_link_libraries(cardart_modded PUBLIC
  shandalar_platform
  rpbits
  SDL3_image::SDL3_image
)
