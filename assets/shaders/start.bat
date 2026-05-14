@echo off

if "%QTDIR%"=="" (
    if exist "F:\Qt\6.9.2\mingw_64" (
        set QTDIR=F:\Qt\6.9.2\mingw_64
    ) else (
        echo Error: QTDIR not set and default path not found
        pause
        exit /b 1
    )
)
set PATH=%QTDIR%\bin;%PATH%
echo Converting shaders ...

qsb --glsl 100,120,150 --hlsl 50 --msl 12 -o blur.frag.qsb blur.frag
if errorlevel 1 goto error

qsb --glsl 100,120,150 --hlsl 50 --msl 12 -o beautify.frag.qsb beautify.frag
if errorlevel 1 goto error

echo All shaders converted successfully!
pause
exit /b 0

:error
echo Failed to convert shader! Check qsb output above.
pause
exit /b 1