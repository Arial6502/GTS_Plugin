@echo off
setlocal

REM ---------------------------------------------------------------------------
REM clang-tidy sweep for GTSPlugin.
REM
REM   run-clang-tidy.bat                  analyse all of src\ (minus vendored)
REM   run-clang-tidy.bat Managers         analyse paths matching "Managers"
REM   run-clang-tidy.bat InitUtils        analyse one file
REM
REM Requires a compile database. Generate it once with:
REM   cmake -S . -B build\Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
REM (run from a Developer Command Prompt, or let this script's vcvars call do it)
REM
REM Results land in build\clang-tidy-report.txt.
REM ---------------------------------------------------------------------------

set "REPO=%~dp0.."
set "BUILDDIR=%REPO%\build\Debug"
set "REPORT=%REPO%\build\clang-tidy-report.txt"

REM clang-tidy invokes the compiler driver directly and needs INCLUDE/LIB set,
REM exactly as a normal MSVC build does.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if not exist "%BUILDDIR%\compile_commands.json" (
    echo ERROR: %BUILDDIR%\compile_commands.json not found.
    echo Run: cmake -S . -B build\Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    exit /b 1
)

REM Default to everything under src\, excluding vendored ImGui in src\UI\Lib.
set "FILTER=%~1"
if "%FILTER%"=="" set "FILTER=.*[\\/]src[\\/](?!UI[\\/]Lib).*"

REM Flag notes:
REM   /Y-                    MSVC .pch files are unreadable by clang. This drops
REM                          /Yu and /Fp while keeping the /FI force-include of
REM                          PCH.hpp, which sources depend on (none include it).
REM   -D_ThrowInfo=ThrowInfo works around ehdata_forceinclude.h in MSVC 14.51+.
REM   -Wno-everything        silences clang's own diagnostics. A handful of
REM                          MSVC-STL constructs still fail to parse under clang;
REM                          those errors are harmless here -- clang-tidy reports
REM                          its findings regardless. Do not chase them.

pushd "%REPO%"
python "C:\Program Files\LLVM\bin\run-clang-tidy" ^
    -p "%BUILDDIR%" ^
    -j %NUMBER_OF_PROCESSORS% ^
    -quiet ^
    -extra-arg-before=/Y- ^
    -extra-arg=-D_ThrowInfo=ThrowInfo ^
    -extra-arg=-Wno-everything ^
    "%FILTER%" > "%REPORT%" 2>&1
popd

echo.
echo Report written to %REPORT%
echo.

REM Summarise with python rather than findstr/for-loop pipelines, which deadlock
REM when this script is invoked from a non-interactive shell.
python -c "import re,sys,collections;t=open(sys.argv[1],errors='replace').read();f=re.findall(r'^(.*?):(\d+):\d+: warning: (.*?) \[([\w-]+)\]$',t,re.M);c=collections.Counter(x[3] for x in f);[print(f'  {n:5}  {k}') for k,n in c.most_common()];print();print(f'  {len(f)} finding(s) in {len({x[0] for x in f})} file(s)')" "%REPORT%"

endlocal
