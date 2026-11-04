@echo off
rem Builds build\dinput8.dll and dist\swbf2_uncap.zip with MSVC (x64). Needs Visual Studio 2019+ with C++ tools.
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio with C++ tools & exit /b 1)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (echo No Visual Studio with C++ x64 tools found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

if not exist build mkdir build
cl /nologo /O2 /MT /EHa /W3 /LD /DUNICODE /D_UNICODE src\swbf2_uncap.cpp /Fo:build\ /Fe:build\dinput8.dll /link /DEF:src\dinput8.def || exit /b 1

if not exist dist mkdir dist
del /q dist\swbf2_uncap.zip 2>nul
powershell -NoProfile -Command "Compress-Archive -Path 'build\dinput8.dll','swbf2_uncap.ini' -DestinationPath 'dist\swbf2_uncap.zip'" || exit /b 1
echo Built build\dinput8.dll and dist\swbf2_uncap.zip
