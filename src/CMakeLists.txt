set(SHANDALAR_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(platform)
add_subdirectory(rpbits)
add_subdirectory(cardartlib)
add_subdirectory(drawcardlib)
add_subdirectory(manalinkeh EXCLUDE_FROM_ALL)

if(SHANDALAR_BUILD_SHANDALAR OR SHANDALAR_BUILD_DECK)
  add_subdirectory(magsnd)
  add_subdirectory(deckdll)
endif()

if(SHANDALAR_BUILD_SHANDALAR OR SHANDALAR_BUILD_DECK)
  add_subdirectory(cardartlib-modded)
  add_subdirectory(drawcardlib-modded)
  add_subdirectory(deckdll-modded)
endif()

if(SHANDALAR_BUILD_FACEMAKER)
  add_subdirectory(facemaker)
endif()

if(SHANDALAR_BUILD_SHANDALAR)
  if(NOT SHANDALAR_BUILD_FACEMAKER)
    add_subdirectory(facemaker)
  endif()
  add_subdirectory(statwin)
  add_subdirectory(shandalar)
endif()

if(SHANDALAR_BUILD_DECK)
  add_subdirectory(deck)
endif()
