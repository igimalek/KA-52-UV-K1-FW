@echo off
cls
setlocal EnableDelayedExpansion

for /f %%a in ('echo prompt $E^| cmd') do set "ESC=%%a"
set "GREEN=!ESC![32m"
set "RED=!ESC![31m"
set "CYAN=!ESC![36m"
set "YELLOW=!ESC![33m"
set "WHITE=!ESC![97m"
set "RESET=!ESC![0m"

set /a FLASH_TOTAL=120832
set /a RAM_TOTAL=16384
set "IMAGE=uvk1-uvk5v3"

rem ── Читаем VERSION_STRING_1 из CMakePresets.json ──────────────────────────
powershell -NoProfile -Command "(Get-Content CMakePresets.json -Raw | ConvertFrom-Json).configurePresets[0].cacheVariables.VERSION_STRING_1.ToLower()" > "%TEMP%\ka52_ver.txt"
set /p FW_VER=<"%TEMP%\ka52_ver.txt"
del "%TEMP%\ka52_ver.txt" >nul 2>&1
set "FW_NAME=ka52.ouro.%FW_VER%.bin"

cls
echo.
echo !CYAN!=============================================!RESET!
echo !WHITE!        KA-52  Firmware  Builder!RESET!
echo !CYAN!        USB + UART  ^|  %FW_VER%  ^|  KA-52!RESET!
echo !CYAN!=============================================!RESET!
echo.
echo  Output: !WHITE!%FW_NAME%!RESET!
echo  Nav inversion: runtime toggle in menu  ^(BtnInv^)
echo.

docker info >nul 2>&1
if errorlevel 1 (
    echo !RED!ERROR: Docker is not running. Start Docker Desktop first.!RESET!
    echo.
    pause & exit /b 1
)

if not exist compiled-firmware mkdir compiled-firmware
del /Q compiled-firmware\*.bin >nul 2>&1

echo [1/3] Building Docker image...
docker build -t %IMAGE% . >nul 2>&1
if errorlevel 1 ( docker build -t %IMAGE% . & goto :error )
echo !GREEN!OK!RESET!
echo.

echo [2/3] Compiling  ka52.ouro...
if exist "build\KA52" rmdir /s /q "build\KA52"
docker run --rm -it -v "%CD%":/src -w /src %IMAGE% bash -c "cmake --preset 'KA52' && cmake --build --preset 'KA52' -j$(nproc)"
if errorlevel 1 goto :error

echo.
echo [3/3] Collecting ka52.ouro...
docker run --rm -v "%CD%":/src -w /src %IMAGE% sh -c "find 'build/KA52' -name '*.bin' -not -path '*/CMakeFiles/*' -exec cp {} /src/compiled-firmware/ \; 2>/dev/null; find 'build/KA52' -name '*.elf' -not -path '*/CMakeFiles/*' | head -1 | xargs -r arm-none-eabi-size 2>/dev/null | tail -1 > /src/compiled-firmware/size_KA52.txt"
if exist "compiled-firmware\ka52.ouro.bin" (
    ren "compiled-firmware\ka52.ouro.bin" "%FW_NAME%"
)
call :show_size "compiled-firmware\size_KA52.txt"
del /Q "compiled-firmware\size_KA52.txt" >nul 2>&1

echo.
echo !CYAN![Output files]!RESET!
dir /b compiled-firmware\*.bin

echo.
echo !GREEN!=============================================!RESET!
echo !GREEN!   Build complete  ^|  %DATE%  %TIME%!RESET!
echo !GREEN!=============================================!RESET!
echo.
pause
exit /b 0

:show_size
if not exist "%~1" ( echo   !YELLOW![size data not found]!RESET! & goto :eof )
set "SZLINE="
set /p SZLINE=<"%~1"
if "!SZLINE!"=="" goto :eof
for /f "tokens=1,2,3,4*" %%A in ("!SZLINE!") do (
    set /a FLASH_USED=%%A+%%B
    set /a RAM_USED=%%B+%%C
)
set /a FLASH_FREE=!FLASH_TOTAL!-!FLASH_USED!
set /a RAM_FREE=!RAM_TOTAL!-!RAM_USED!
set /a FLASH_PCT=!FLASH_USED!*100/!FLASH_TOTAL!
set /a RAM_PCT=!RAM_USED!*100/!RAM_TOTAL!
set "WF=" & set "WR="
if !FLASH_PCT! GEQ 95 set "WF=  !RED!^<^< FLASH NEARLY FULL!!RESET!"
if !RAM_PCT!   GEQ 90 set "WR=  !RED!^<^< RAM NEARLY FULL!!RESET!"
echo   !YELLOW!Flash : !FLASH_USED! / !FLASH_TOTAL! bytes  (!FLASH_PCT!%%)  --  !FLASH_FREE! free!RESET!!WF!
echo   !YELLOW!RAM   : !RAM_USED! / !RAM_TOTAL! bytes  (!RAM_PCT!%%)  --  !RAM_FREE! free!RESET!!WR!
echo   !GREEN!OK!RESET!
goto :eof

:error
echo.
echo !RED!=============================================!RESET!
echo !RED!   BUILD FAILED -- see log above!RESET!
echo !RED!=============================================!RESET!
echo.
pause
exit /b 1
