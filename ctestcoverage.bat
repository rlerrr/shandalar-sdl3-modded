@echo off
setlocal

REM LLVM source-based code coverage for the 32-bit duel/card ctest suite.
REM
REM Usage: ctestcoverage.bat [label] [test-regex]
REM   label      CTest label to run (default: behavior, same as the README's quick run)
REM   test-regex Optional ctest -R filter, e.g. ctestcoverage.bat behavior AbbeyGargoyles
REM
REM Requires a Clang toolchain that ships the 32-bit profile runtime
REM (clang_rt.profile-i386.lib). The stock 64-bit LLVM has none; this script
REM uses C:\bin\LLVM-21.1.8-win32 by default, override with SHANDALAR_LLVM_COV.
REM VsDevCmd (x86) is still needed for the MSVC headers/libs clang-cl targets.
REM
REM Reports land in out\coverage\llvmcov\ (report table on stdout,
REM html\index.html, coverage.profdata). The suite spawns thousands of test
REM processes; use the test-regex argument to iterate on a subset.

set "LABEL=%~1"
if "%LABEL%"=="" set "LABEL=behavior"
set "TESTREGEX=%~2"

REM --- 0. Locate the coverage toolchain (fail fast, before configuring) ---
if defined SHANDALAR_LLVM_COV (
  set "LLVMCOV=%SHANDALAR_LLVM_COV%"
) else (
  set "LLVMCOV=C:\bin\LLVM-21.1.8-win32"
)
if not exist "%LLVMCOV%\bin\clang-cl.exe" (
  echo clang-cl.exe not found under "%LLVMCOV%\bin".
  echo Install a 32-bit LLVM with clang_rt.profile-i386.lib there,
  echo or set SHANDALAR_LLVM_COV to its install root, then re-run.
  exit /b 1
)
if not exist "%LLVMCOV%\bin\llvm-profdata.exe" (
  echo llvm-profdata.exe not found under "%LLVMCOV%\bin"; need a full LLVM install.
  exit /b 1
)
if not exist "%LLVMCOV%\bin\llvm-cov.exe" (
  echo llvm-cov.exe not found under "%LLVMCOV%\bin"; need a full LLVM install.
  exit /b 1
)

if "%SHANDALAR_MODDED_ASSET_DIR%"=="" (
  if exist "%~dp0..\assets_modded\cards.dat" (
    set "SHANDALAR_MODDED_ASSET_DIR=%~dp0..\assets_modded"
  ) else (
    set "SHANDALAR_MODDED_ASSET_DIR=C:/dev/shandalar"
  )
)

call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

set "BUILD_DIR=out\build\shandalar-llvmcov-x86"
set "BIN_DIR=shandalar-llvmcov-x86"
set "COV_DIR=out\coverage\llvmcov"
set "TEST_EXE=%CD%\out\bin\%BIN_DIR%\duel_card_tests.exe"

REM --- 1. Configure the instrumented 32-bit build ---
cmake -S . -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_C_COMPILER="%LLVMCOV%/bin/clang-cl.exe" ^
  -DCMAKE_CXX_COMPILER="%LLVMCOV%/bin/clang-cl.exe" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%CD%/out/bin/%BIN_DIR%" ^
  -DCMAKE_C_COMPILER_TARGET=i686-pc-windows-msvc ^
  -DCMAKE_CXX_COMPILER_TARGET=i686-pc-windows-msvc ^
  -DCMAKE_EXE_LINKER_FLAGS=/machine:x86 ^
  -DCMAKE_SHARED_LINKER_FLAGS=/machine:x86 ^
  -DCMAKE_MODULE_LINKER_FLAGS=/machine:x86 ^
  -DCMAKE_STATIC_LINKER_FLAGS=/machine:x86 ^
  -DSHANDALAR_ASSET_DIR="C:/Magic/Program" ^
  -DSHANDALAR_MODDED_ASSET_DIR="%SHANDALAR_MODDED_ASSET_DIR%" ^
  -DSHANDALAR_BUILD_FACEMAKER=OFF ^
  -DSHANDALAR_BUILD_SHANDALAR=ON ^
  -DSHANDALAR_BUILD_DECK=OFF ^
  -DSHANDALAR_BUILD_TESTS=ON ^
  -DSHANDALAR_DUEL_EXTENDED_TESTS=ON ^
  -DSHANDALAR_DUEL_COVERAGE=ON
if errorlevel 1 exit /b %errorlevel%

REM --- 2. Build the duel test binary (deps follow automatically) ---
cmake --build "%BUILD_DIR%" --target duel_card_tests -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b %errorlevel%

REM --- 3. Run ctest with per-process raw profiles ---
if not exist "%CD%\%COV_DIR%" mkdir "%CD%\%COV_DIR%"
if exist "%CD%\%COV_DIR%\*.profraw" del /Q "%CD%\%COV_DIR%\*.profraw"
REM NOTE: doubled %% so cmd passes literal %p/%m through to the profile runtime.
set "LLVM_PROFILE_FILE=%CD%\%COV_DIR%\%%p-%%m.profraw"
set "CTEST_ARGS=--test-dir "%BUILD_DIR%" -L "%LABEL%" --output-on-failure -j10"
if not "%TESTREGEX%"=="" set "CTEST_ARGS=%CTEST_ARGS% -R "%TESTREGEX%""
echo Running: ctest %CTEST_ARGS%
ctest %CTEST_ARGS%
set "RC=%errorlevel%"
set "LLVM_PROFILE_FILE="

if not exist "%CD%\%COV_DIR%\*.profraw" (
  echo No .profraw files written; the test run above failed before coverage was collected.
  exit /b %RC%
)

REM --- 4. Merge and report (version-matched tools from the same toolchain) ---
"%LLVMCOV%\bin\llvm-profdata.exe" merge -sparse "%CD%\%COV_DIR%\*.profraw" -o "%CD%\%COV_DIR%\coverage.profdata"
if errorlevel 1 exit /b %errorlevel%
REM Ignore third-party headers, generated files, and the test harness itself.
set "IGNORE=.*(_deps|generated|duel_card_tests\.cpp|headless_shell\.c).*"
"%LLVMCOV%\bin\llvm-cov.exe" report "%TEST_EXE%" -instr-profile="%CD%\%COV_DIR%\coverage.profdata" --ignore-filename-regex="%IGNORE%"
"%LLVMCOV%\bin\llvm-cov.exe" show "%TEST_EXE%" -instr-profile="%CD%\%COV_DIR%\coverage.profdata" --ignore-filename-regex="%IGNORE%" -format=html -output-dir="%CD%\%COV_DIR%\html"
if errorlevel 1 exit /b %errorlevel%
echo HTML report: "%CD%\%COV_DIR%\html\index.html"

exit /b %RC%
