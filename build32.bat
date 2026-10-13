@echo off
setlocal
cd /d "%~dp0"

set "OBJ=build\x32"
set "PLUGIN_FLAGS=/nologo /std:c++17 /O2 /MT /W1 /GR- /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX"
set "PLUGIN_LIBS=user32.lib gdi32.lib comdlg32.lib shell32.lib ole32.lib advapi32.lib comctl32.lib winmm.lib"
set "TEST_LIBS=user32.lib gdi32.lib ole32.lib"

if not defined VCINSTALLDIR call :find_tools
if errorlevel 1 exit /b 1

if not exist plugin\sdk\fp_plugclass.h (
    echo missing plugin\sdk: copy the Image-Line FL Studio plugin SDK headers there.
    exit /b 2
)
if not exist plugin\fsp_glyphs.cpp (
    echo missing plugin\fsp_glyphs.cpp: the editor image data is not part of this repository.
    exit /b 2
)
python plugin\make_regprobe.py
if errorlevel 1 exit /b 1

if not exist dist mkdir dist
if not exist "%OBJ%" mkdir "%OBJ%"

cl %PLUGIN_FLAGS% /Fo%OBJ%\ /LD plugin\fsp_*.cpp plugin\sdk\fp_plugclass.cpp engine\flspro32.c /Fe"dist\Fruity Soundfont Player.dll" /link /DEF:plugin\fsp.def /IMPLIB:%OBJ%\fsp.lib %PLUGIN_LIBS%
if errorlevel 1 exit /b 1

if not "%1"=="tests" exit /b 0

cl /nologo /std:c++17 /O1 /MT /W1 /D_CRT_SECURE_NO_WARNINGS /Fo%OBJ%\ tests\host_test.cpp /Fe"%OBJ%\host_test32.exe" /link %TEST_LIBS%
if errorlevel 1 exit /b 1
exit /b 0

:find_tools
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio was not found. Run this script from a developer command prompt.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
    echo No Visual Studio installation with the C++ tools was found.
    exit /b 1
)
call "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
exit /b %errorlevel%
