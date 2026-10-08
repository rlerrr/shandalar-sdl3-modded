file(GLOB manalinkeh_card_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/*.c"
)
file(GLOB manalinkeh_function_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/functions/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/functions/*.cpp"
)

# These files had no corresponding object in the source dump and use APIs
# older than the accompanying headers.  Keep them as porting references, but
# defer them until the live veneer/card registry proves which definitions are
# still needed.
set(manalinkeh_deferred_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/artifact_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/artifacts.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/black_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/black_enchantments.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/black_oneshot.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/blue_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/blue_enchantments.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/blue_oneshot.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/green_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/green_enchantments.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/green_oneshot.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/jatill.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/lands.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/recoded_cards.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/red_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/red_enchantments.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/red_oneshot.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/white_creatures.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/white_enchantments.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/cards/white_oneshot.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/functions/ability_icons.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/functions/affect_me.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/functions/jatill.c"
)
list(REMOVE_ITEM manalinkeh_card_sources ${manalinkeh_deferred_sources})
list(REMOVE_ITEM manalinkeh_function_sources ${manalinkeh_deferred_sources})

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(manalinkeh_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(manalinkeh_veneer_source "${manalinkeh_generated_dir}/manalinkeh_veneer_table.c")
set(manalinkeh_veneer_header "${manalinkeh_generated_dir}/manalinkeh_veneer_table.h")
set(manalinkeh_namespace_header "${manalinkeh_generated_dir}/manalinkeh_symbol_namespace.h")
add_custom_command(
  OUTPUT "${manalinkeh_veneer_source}" "${manalinkeh_veneer_header}" "${manalinkeh_namespace_header}"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/generate_veneer_table.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/ManalinkEh.asm"
          "${manalinkeh_veneer_source}"
          "${manalinkeh_veneer_header}"
          "${manalinkeh_namespace_header}"
          "${CMAKE_CURRENT_SOURCE_DIR}/internal_symbols.txt"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ManalinkEh.asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_veneer_table.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal_symbols.txt"
  VERBATIM
)

add_library(manalinkeh_static STATIC EXCLUDE_FROM_ALL
  ${manalinkeh_card_sources}
  ${manalinkeh_function_sources}
  "${manalinkeh_veneer_source}"
  "${manalinkeh_namespace_header}"
)

target_include_directories(manalinkeh_static PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${manalinkeh_generated_dir}"
)
if(EMSCRIPTEN)
  target_include_directories(manalinkeh_static PRIVATE
    "${SHANDALAR_SRC_ROOT}"
  )
  target_include_directories(manalinkeh_static BEFORE PRIVATE
    "${SHANDALAR_SRC_ROOT}/magic/compat"
  )
  target_link_libraries(manalinkeh_static PRIVATE
    SDL3::Headers
  )
endif()

# This is initially a faithful source import.  The DLL veneer, injection
# patches, and absolute-address assembly helpers are intentionally excluded;
# their jobs are replaced by the static Shandalar bridge.
target_compile_definitions(manalinkeh_static PRIVATE
  MANALINKEH_STATIC
)

# An x86 stack tolerated incompatible callback prototypes; WASM traps on them.
# Typed target registrations expose these checks before a browser reaches them.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  target_compile_options(manalinkeh_static PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-function-pointer-types>"
    -Werror=cast-function-type-strict
  )
endif()

set(target_predicate_functions "${manalinkeh_generated_dir}/target_predicate_functions.inc")
add_custom_command(OUTPUT "${target_predicate_functions}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/select_target_predicate_functions.py"
          "${CMAKE_CURRENT_SOURCE_DIR}" "${target_predicate_functions}"
  DEPENDS tests/select_target_predicate_functions.py functions/targets.c cards/unlimited.c
  VERBATIM
)
add_executable(target_predicate_test EXCLUDE_FROM_ALL tests/target_predicate_test.c
               "${target_predicate_functions}")
if(SHANDALAR_BUILD_TESTS)
  set_property(TARGET target_predicate_test PROPERTY EXCLUDE_FROM_ALL FALSE)
endif()
target_include_directories(target_predicate_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${manalinkeh_generated_dir}")
if(EMSCRIPTEN)
  set_target_properties(target_predicate_test PROPERTIES SUFFIX ".js")
  target_link_options(target_predicate_test PRIVATE -sENVIRONMENT=node -sEXIT_RUNTIME=1)
endif()
if(SHANDALAR_BUILD_TESTS AND NOT EMSCRIPTEN)
  add_test(NAME target_predicate_test COMMAND target_predicate_test)
endif()

# Keep legacy translation units from retaining every unrelated function when
# the host requests only one of their MOK replacement entry points.
if(MSVC)
  target_compile_options(manalinkeh_static PRIVATE
    /Gy
    -fcommon
    "/FI${manalinkeh_namespace_header}"
  )
else()
  target_compile_options(manalinkeh_static PRIVATE
    -ffunction-sections
    -fdata-sections
    "-include" "${manalinkeh_namespace_header}"
  )
endif()

# Asset readers share the native/WASM stream overlay.
target_link_libraries(manalinkeh_static PRIVATE shandalar_platform)
if(TARGET shandalar_llvm_coverage)
  target_link_libraries(manalinkeh_static PRIVATE shandalar_llvm_coverage)
endif()
