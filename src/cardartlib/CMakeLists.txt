if(SHANDALAR_BUILD_SHANDALAR)
  set(cardart_facemaker_palette src/palette.c)
else()
  set(cardart_facemaker_palette src/palette_facemaker.c)
endif()
add_library(cardart_facemaker STATIC
  src/assert.c
  ${cardart_facemaker_palette}
)

target_include_directories(cardart_facemaker PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(cardart_facemaker PUBLIC shandalar_platform)
if(SHANDALAR_BUILD_SHANDALAR)
  target_compile_definitions(cardart_facemaker PRIVATE CARDARTLIB MODERN_FIXES)
else()
  target_compile_definitions(cardart_facemaker PRIVATE FACEMAKER)
endif()

add_library(cardart_static STATIC
  src/assert.c
  src/catalog.c
  src/haar.c
  src/huffman.c
  src/main.c
  src/palette.c
)

target_include_directories(cardart_static PUBLIC
  "${SHANDALAR_SRC_ROOT}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(cardart_static PUBLIC shandalar_platform)
target_compile_definitions(cardart_static PRIVATE CARDARTLIB MODERN_FIXES)
