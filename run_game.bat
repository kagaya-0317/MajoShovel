@echo off
setlocal

set "ROOT=%~dp0"
set "EXE="

if exist "%ROOT%build-nopch\Release\MajoShovel.exe" set "EXE=%ROOT%build-nopch\Release\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build\Release\MajoShovel.exe" set "EXE=%ROOT%build\Release\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build-nopch\Debug\MajoShovel.exe" set "EXE=%ROOT%build-nopch\Debug\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build\Debug\MajoShovel.exe" set "EXE=%ROOT%build\Debug\MajoShovel.exe"

if not defined EXE (
    echo MajoShovel.exe was not found.
    echo Build first, for example:
    echo cmake -S . -B build-nopch -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
    echo cmake --build build-nopch --config Release --target MajoShovel
    pause
    exit /b 1
)

echo Starting "%EXE%"
pushd "%~dp0"
start "" "%EXE%"
popd

endlocal
