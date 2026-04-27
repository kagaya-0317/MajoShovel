@echo off
setlocal

set "ROOT=%~dp0"
set "EXE="
set "LOCAL_BUILD=%LOCALAPPDATA%\MajoShovel\build-nopch"

if exist "%LOCAL_BUILD%\Release\MajoShovel.exe" set "EXE=%LOCAL_BUILD%\Release\MajoShovel.exe"
if not defined EXE if exist "%LOCAL_BUILD%\Debug\MajoShovel.exe" set "EXE=%LOCAL_BUILD%\Debug\MajoShovel.exe"
if exist "%ROOT%build-nopch\Release\MajoShovel.exe" set "EXE=%ROOT%build-nopch\Release\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build\Release\MajoShovel.exe" set "EXE=%ROOT%build\Release\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build-nopch\Debug\MajoShovel.exe" set "EXE=%ROOT%build-nopch\Debug\MajoShovel.exe"
if not defined EXE if exist "%ROOT%build\Debug\MajoShovel.exe" set "EXE=%ROOT%build\Debug\MajoShovel.exe"

if not defined EXE (
    echo MajoShovel.exe was not found.
    echo Build first, for example:
    echo build_game.bat
    pause
    exit /b 1
)

echo Starting "%EXE%"
pushd "%~dp0"
start "" "%EXE%"
popd

endlocal
