add_library(drawcard_modded STATIC
  config.c
  drawcardlib.c
  drawcounters.c
  drawfullcard.c
  drawmanatext.c
  gdiplus_sdl.c
)

target_include_directories(drawcard_modded PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(drawcard_modded PRIVATE
  SHANDALAR_SDL_WIN32_COMPAT
  SHANDALAR_MODDED_LIBS
  DllMain=DrawCardModded_DllMain
)
target_link_libraries(drawcard_modded PUBLIC
  cardart_modded
  shandalar_platform
  SDL3_image::SDL3_image
)
