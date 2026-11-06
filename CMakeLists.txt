cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(shandalar_sdl3 LANGUAGES C CXX)

if(WIN32 AND NOT EMSCRIPTEN)
  enable_language(RC)
endif()

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EMSCRIPTEN)
  # Emscripten's libc strlcpy reads past the terminator in word-sized
  # chunks, which ASan catches on short strings. Use SDL's bounded-copy
  # fallback instead, including when reconfiguring an existing build.
  set(LIBC_HAS_STRLCPY OFF)
  # Keep the regular Debug build symbol-enabled without the ASan overhead.
  add_compile_options("$<$<CONFIG:Debug>:-g>")
  # Instrument dependencies too: ASan must be enabled when compiling every
  # library as well as when linking the final WebAssembly application.
  # Unoptimized ASan instrumentation can exceed browser limits on Wasm
  # locals in SDL's macro-heavy software rendering functions.
  add_compile_options("$<$<CONFIG:DebugAsan>:-O1;-g;-fsanitize=address;-fsanitize-recover=address;-mllvm;-asan-instrument-reads=false>")
  # ASan's shadow memory and instrumented globals must fit at link time;
  # runtime memory growth cannot satisfy wasm-ld's initial-memory minimum.
  add_link_options("$<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:DebugAsan>>,-g,--profiling-funcs>"
                   "$<$<CONFIG:DebugAsan>:-O1;-fsanitize=address;-asan-instrument-reads=false;-sASSERTIONS=1;-sINITIAL_MEMORY=256MB>")
endif()

option(SHANDALAR_BUILD_FACEMAKER "Build the SDL3 Facemaker application" ON)
option(SHANDALAR_BUILD_SHANDALAR "Build the SDL3 Shandalar application" OFF)
option(SHANDALAR_BUILD_DECK "Build the SDL3 Deck Builder application" OFF)
option(SHANDALAR_BUILD_TESTS "Build unit tests" ON)
option(SHANDALAR_BUILD_DUEL_TESTS "Build headless real-engine duel regression tests" ON)
option(SHANDALAR_DUEL_COVERAGE "Instrument the real duel/card runtime with LLVM coverage" OFF)
if(SHANDALAR_DUEL_COVERAGE)
  # LLVM source-based coverage (-fprofile-instr-generate -fcoverage-mapping).
  # Only Clang toolchains with a matching profile runtime can use this.
  if(EMSCRIPTEN)
    message(FATAL_ERROR "SHANDALAR_DUEL_COVERAGE is not supported for Emscripten builds. Use ctestcoverage.bat (OpenCppCoverage) for native Windows coverage instead.")
  endif()
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "SHANDALAR_DUEL_COVERAGE requires a Clang compiler (C=${CMAKE_C_COMPILER_ID}, CXX=${CMAKE_CXX_COMPILER_ID}). Use ctestcoverage.bat (OpenCppCoverage) for non-Clang Windows coverage instead.")
  endif()
  if(WIN32)
    # The profile runtime ships inside the compiler install, matched to that
    # compiler version. Current 64-bit-only LLVM distributions have no i686
    # runtime at all, while e.g. C:\bin\LLVM-21.1.8-win32 still ships
    # lib/clang/21/lib/windows/clang_rt.profile-i386.lib. Fail fast when the
    # active compiler cannot provide it instead of failing at link time.
    execute_process(
      COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
      OUTPUT_VARIABLE SHANDALAR_CLANG_RESOURCE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    unset(SHANDALAR_PROFILE_RT_LIB CACHE)
    foreach(_profile_arch IN ITEMS i386 i686 x86)
      set(_profile_rt_candidate
        "${SHANDALAR_CLANG_RESOURCE_DIR}/lib/windows/clang_rt.profile-${_profile_arch}.lib")
      if(EXISTS "${_profile_rt_candidate}")
        set(SHANDALAR_PROFILE_RT_LIB "${_profile_rt_candidate}" CACHE FILEPATH
          "LLVM profile runtime for SHANDALAR_DUEL_COVERAGE")
        break()
      endif()
    endforeach()
    if(NOT SHANDALAR_PROFILE_RT_LIB)
      message(FATAL_ERROR "SHANDALAR_DUEL_COVERAGE cannot link with ${CMAKE_C_COMPILER}: no clang_rt.profile i386/i686 runtime under ${SHANDALAR_CLANG_RESOURCE_DIR}/lib/windows. Use a toolchain that ships one (e.g. C:\\bin\\LLVM-21.1.8-win32, see ctestcoverage.bat).")
    endif()
  endif()
  add_library(shandalar_llvm_coverage INTERFACE)
  target_compile_options(shandalar_llvm_coverage INTERFACE
    "$<$<COMPILE_LANGUAGE:C,CXX>:-fprofile-instr-generate;-fcoverage-mapping>")
  target_link_options(shandalar_llvm_coverage INTERFACE
    "-fprofile-instr-generate;-fcoverage-mapping")
endif()
set(SHANDALAR_ASSET_DIR "" CACHE PATH "Directory containing the Magic game data")
get_filename_component(shandalar_sibling_modded_assets "${CMAKE_SOURCE_DIR}/../assets_modded" ABSOLUTE)
if(EXISTS "${shandalar_sibling_modded_assets}")
  set(shandalar_default_modded_assets "${shandalar_sibling_modded_assets}")
else()
  set(shandalar_default_modded_assets "C:/dev/shandalar")
endif()
set(SHANDALAR_MODDED_ASSET_DIR "${shandalar_default_modded_assets}" CACHE PATH "Directory containing the modded Magic game data")

if(SHANDALAR_BUILD_TESTS)
  enable_testing()
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
  # Threadless SDL is only supported by upstream on Emscripten (and N-Gage);
  # native builds (Windows/macOS) require threads. Our own code still avoids
  # spawning threads and pumps UI via Asyncify yields on Emscripten.
  set(SDL_PTHREADS OFF CACHE BOOL "" FORCE)
endif()
if(SHANDALAR_BUILD_DECK OR SHANDALAR_BUILD_SHANDALAR)
  set(SDL_AUDIO ON CACHE BOOL "" FORCE)
else()
  set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
endif()
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.10
  GIT_SHALLOW TRUE
  OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(SDL3)
if(NOT TARGET SDL3::Headers)
  add_library(SDL3::Headers ALIAS SDL3_Headers)
endif()
if(NOT TARGET SDL3::SDL3)
  add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()

if(SHANDALAR_BUILD_SHANDALAR OR SHANDALAR_BUILD_DECK)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_BACKEND_STB OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_ANI OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_JPG ON CACHE BOOL "" FORCE)
  set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
  set(SDLIMAGE_PNG_LIBPNG ON CACHE BOOL "" FORCE)
  set(SDLIMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
  if(SHANDALAR_BUILD_SHANDALAR)
    set(SDLIMAGE_WEBP ON CACHE BOOL "" FORCE)
  else()
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
  endif()
  set(SDLIMAGE_WEBP_SAVE OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_WEBP_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.4.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(SDL3_image)
endif()

set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_TESTS OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3_ttf)

if(SHANDALAR_BUILD_DECK OR SHANDALAR_BUILD_SHANDALAR)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(imgui)
endif()

if(MSVC)
  add_compile_options(/W3 /wd4996)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast)
elseif(MSVC)
  add_compile_options(/we4302 /we4311 /we4312)
endif()

include(cmake/DownloadAssets.cmake)

if(SHANDALAR_BUILD_FACEMAKER OR SHANDALAR_BUILD_SHANDALAR OR SHANDALAR_BUILD_DECK)
  # Keep miniz's generic CMake options and install rules out of our build.
  FetchContent_Declare(shandalar_miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.1.2
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR shandalar-unused
  )
  FetchContent_MakeAvailable(shandalar_miniz)
  file(CONFIGURE OUTPUT "${shandalar_miniz_BINARY_DIR}/miniz_export.h" CONTENT "#define MINIZ_EXPORT\n" @ONLY)
  add_library(shandalar_miniz STATIC
    "${shandalar_miniz_SOURCE_DIR}/miniz.c"
    "${shandalar_miniz_SOURCE_DIR}/miniz_tinfl.c"
    "${shandalar_miniz_SOURCE_DIR}/miniz_tdef.c"
    "${shandalar_miniz_SOURCE_DIR}/miniz_zip.c"
  )
  target_include_directories(shandalar_miniz PUBLIC
    "${shandalar_miniz_SOURCE_DIR}" "${shandalar_miniz_BINARY_DIR}")
  target_compile_definitions(shandalar_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
  add_subdirectory(src)
endif()
