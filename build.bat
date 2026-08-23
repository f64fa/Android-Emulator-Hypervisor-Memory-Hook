@echo off
echo ===================================================
echo Building Standalone SimpleHook DLL...
echo ===================================================

where cl.exe >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [*] Initializing MSVC x64 Environment...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

del /f /q SimpleHook.dll 2>nul

cl.exe /LD /nologo /EHsc /O2 /std:c++17 /W3 ^
  main.cpp ^
  hook\src\hook.c hook\src\buffer.c hook\src\trampoline.c hook\src\HDE\hde32.c hook\src\HDE\hde64.c ^
  /I. /Imemory /Ihook\include ^
  /link /OUT:SimpleHook.dll ^
  user32.lib gdi32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [+] Build Succeeded: SimpleHook.dll created!
) else (
    echo.
    echo [-] Note: If SimpleHook.dll is locked by HD-Player.exe, close BlueStacks first.
)
