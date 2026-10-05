add_library(shandalar_platform STATIC
  asset_manager.c
  cheat_options.c
  file_io.c
  stream_helpers.c
  file_list.c
  gdi_compat.c
  legacy_io.c
  mod_overlay.c
  dictionary.c
  iniparser.c
  winbase_compat.c
  wincontrols_compat.c
  windialog_compat.c
  winmenu_compat.c
  winuser_compat.c
  winuser_runtime.c
  winuser_sdl_compositor.c
  winreg_compat.c
  winscroll_compat.c
)

if(SHANDALAR_BUILD_SHANDALAR)
  target_sources(shandalar_platform PRIVATE vfw_compat.c)
endif()

target_include_directories(shandalar_platform PUBLIC
  "${SHANDALAR_SRC_ROOT}"
)
target_link_libraries(shandalar_platform PUBLIC
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
)
target_link_libraries(shandalar_platform PRIVATE shandalar_miniz)
if(SHANDALAR_BUILD_SHANDALAR)
  target_link_libraries(shandalar_platform PUBLIC SDL3_image::SDL3_image)
endif()

if(EMSCRIPTEN)
  target_link_options(shandalar_platform INTERFACE
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/mod_overlay.js")
  set_property(TARGET shandalar_platform APPEND PROPERTY INTERFACE_LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/mod_overlay.js")
endif()

if(SHANDALAR_BUILD_DECK OR SHANDALAR_BUILD_SHANDALAR)
  add_library(legacy_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
  )
  target_include_directories(legacy_imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
  )
  target_link_libraries(legacy_imgui PUBLIC SDL3::SDL3)

  add_library(legacy_win32_imgui_bridge STATIC
    winuser_imgui_bridge.cpp
  )
  target_include_directories(legacy_win32_imgui_bridge PUBLIC
    "${SHANDALAR_SRC_ROOT}"
    "${imgui_SOURCE_DIR}"
  )
  target_link_libraries(legacy_win32_imgui_bridge PUBLIC
    legacy_imgui
    shandalar_platform
  )
endif()

if(SHANDALAR_BUILD_TESTS)
  if(SHANDALAR_BUILD_DECK OR SHANDALAR_BUILD_SHANDALAR)
    add_executable(asset_stream_test
      tests/asset_stream_test.c
      ../magic/src/global_strings.c
      ../shandalar/src/card_pool.c
      ../drawcardlib/src/pic.c
      ../drawcardlib/src/pcxw.c
      ../cardartlib-modded/image_loader.c
      ../cardartlib-modded/pic_assert.c
    )
    target_compile_definitions(asset_stream_test PRIVATE
      SHANDALAR_MODDED_PIC_LOADER assert=modded_pic_assert assert_noabort=modded_pic_assert_noabort)
    target_link_libraries(asset_stream_test PRIVATE shandalar_platform shandalar_miniz rpbits SDL3_image::SDL3_image)
    if(EMSCRIPTEN)
      target_link_options(asset_stream_test PRIVATE "-sASYNCIFY=1" "-sFORCE_FILESYSTEM=1" "-sFETCH=1" "-sALLOW_MEMORY_GROWTH=1" "-lidbfs.js")
      add_test(NAME asset_stream_test COMMAND node "$<TARGET_FILE:asset_stream_test>")
    else()
      add_test(NAME asset_stream_test COMMAND asset_stream_test)
    endif()
    set_tests_properties(asset_stream_test PROPERTIES TIMEOUT 30)
  endif()

  add_executable(mod_overlay_test tests/mod_overlay_test.c)
  target_link_libraries(mod_overlay_test PRIVATE shandalar_platform shandalar_miniz)
  if(EMSCRIPTEN)
    target_link_options(mod_overlay_test PRIVATE "-sASYNCIFY=1" "-sFORCE_FILESYSTEM=1" "-sFETCH=1" "-lidbfs.js")
    add_test(NAME mod_overlay_test COMMAND node "$<TARGET_FILE:mod_overlay_test>")
    foreach(mod_host_mode bytes blob url env)
      add_test(NAME mod_overlay_host_${mod_host_mode}
        COMMAND node "${CMAKE_CURRENT_SOURCE_DIR}/tests/mod_overlay_host_test.cjs"
                     "$<TARGET_FILE:mod_overlay_test>" "${mod_host_mode}")
      set_tests_properties(mod_overlay_host_${mod_host_mode} PROPERTIES
        PASS_REGULAR_EXPRESSION "WebAssembly host mod ZIP startup passed")
    endforeach()
  else()
    add_test(NAME mod_overlay_test COMMAND mod_overlay_test)
  endif()

  if(EMSCRIPTEN)
    add_executable(user_data_namespace_test tests/user_data_namespace_test.c)
    target_link_libraries(user_data_namespace_test PRIVATE shandalar_platform)
    target_link_options(user_data_namespace_test PRIVATE "-sASYNCIFY=1" "-sFORCE_FILESYSTEM=1" "-sFETCH=1" "-sEXIT_RUNTIME=1" "-lidbfs.js")
    foreach(user_data_namespace base jamuraa barrins-folly)
      if(user_data_namespace STREQUAL "base")
        set(user_data_test_root "/userdata")
      else()
        set(user_data_test_root "/userdata/mods/${user_data_namespace}")
      endif()
      add_test(NAME user_data_namespace_${user_data_namespace}
        COMMAND node "${CMAKE_CURRENT_SOURCE_DIR}/tests/user_data_namespace_host_test.cjs"
                     "$<TARGET_FILE:user_data_namespace_test>" "${user_data_test_root}")
      set_tests_properties(user_data_namespace_${user_data_namespace} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(legacy_io_path_test tests/legacy_io_path_test.c legacy_io.c)
    target_include_directories(legacy_io_path_test PRIVATE "${SHANDALAR_SRC_ROOT}")
    target_link_options(legacy_io_path_test PRIVATE "-sASYNCIFY=1" "-sFORCE_FILESYSTEM=1")
    add_test(NAME legacy_io_path_test COMMAND node "$<TARGET_FILE:legacy_io_path_test>")
  endif()

  add_executable(gdi_compat_test tests/gdi_compat_test.c)
  target_link_libraries(gdi_compat_test PRIVATE shandalar_platform)
  add_test(NAME gdi_compat_test COMMAND gdi_compat_test)

  add_executable(winuser_compat_test tests/winuser_compat_test.c)
  target_link_libraries(winuser_compat_test PRIVATE shandalar_platform)
  add_test(NAME winuser_compat_test COMMAND winuser_compat_test)

  add_executable(winuser_caption_test tests/winuser_caption_test.c)
  target_link_libraries(winuser_caption_test PRIVATE shandalar_platform)
  add_test(NAME winuser_caption_test COMMAND winuser_caption_test)
endif()
