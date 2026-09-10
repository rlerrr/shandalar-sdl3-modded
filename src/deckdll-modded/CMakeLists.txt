add_library(deckdll_modded STATIC
  deckdll.cpp
  File.cpp
  "${SHANDALAR_SRC_ROOT}/deckdll/src/dialog_resources.c"
)

target_include_directories(deckdll_modded PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SHANDALAR_SRC_ROOT}/deckdll/src"
)
target_compile_definitions(deckdll_modded PRIVATE
  SHANDALAR_SDL_WIN32_COMPAT
  SHANDALAR_MODDED_LIBS
  SHANDALAR_MODDED_DECKDLL
)
if(MSVC)
  target_compile_options(deckdll_modded PRIVATE /Zc:__cplusplus)
endif()
target_link_libraries(deckdll_modded PUBLIC
  drawcard_modded
  magsnd_static
  shandalar_platform
)
