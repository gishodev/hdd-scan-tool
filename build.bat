@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Building HDD Surface Scanner (x86 Native Standalone)
echo ===================================================

:: Check if cl.exe is already available in the current environment
where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Compiler [cl.exe] is already in the PATH.
    goto compile
)

:: Check if g++.exe is already available in the current environment
where g++.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Compiler [g++.exe] GCC is already in the PATH.
    goto compile_gcc
)

:: 1. Try to find Visual Studio using vswhere with SystemDrive path
set "VSWHERE_PATH=%SystemDrive%\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE_PATH%" (
    set "VSWHERE_PATH=%SystemDrive%\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
)

if exist "%VSWHERE_PATH%" (
    echo Querying Visual Studio installation path via vswhere...
    for /f "usebackq tokens=*" %%i in (`^""%VSWHERE_PATH%" -latest -products * -property installationPath^"`) do (
        set "VS_INSTALL_PATH=%%i"
    )
)

if defined VS_INSTALL_PATH (
    set "VCVARS_BAT=!VS_INSTALL_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
    if exist "!VCVARS_BAT!" (
        echo Found MSVC build environment: "!VCVARS_BAT!"
        call "!VCVARS_BAT!" x86
        goto compile
    )
)

:: 2. Try common default paths if vswhere is not available or didn't find the tools
echo Automated lookup did not find the compiler. Checking default directories...
for %%v in (2022, 2019, 2017) do (
    for %%e in (Community, Professional, Enterprise, BuildTools) do (
        set "CHECK_PATH=%SystemDrive%\Program Files (x86)\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat"
        if exist "!CHECK_PATH!" (
            echo Found MSVC build environment: "!CHECK_PATH!"
            call "!CHECK_PATH!" x86
            goto compile
        )
        set "CHECK_PATH=%SystemDrive%\Program Files\Microsoft Visual Studio\%%v\%%e\VC\Auxiliary\Build\vcvarsall.bat"
        if exist "!CHECK_PATH!" (
            echo Found MSVC build environment: "!CHECK_PATH!"
            call "!CHECK_PATH!" x86
            goto compile
        )
    )
)

:: 3. Prompt user for path or instructions if it cannot be found
echo.
echo =====================================================================
echo ERROR: Could not auto-detect your Visual C++ Build environment.
echo =====================================================================
echo.
echo To compile this project, please:
echo 1. Open the "Developer Command Prompt for VS" from your Start menu.
echo 2. Navigate to this directory: "%~dp0"
echo 3. Run "build.bat" again.
echo.
echo Alternatively, you can enter the path to "vcvarsall.bat" manually.
set /p "USER_PATH=Enter path to vcvarsall.bat (or press Enter to exit): "

if not "%USER_PATH%"=="" (
    rem Strip any outer quotes the user might have pasted
    set "USER_PATH=%USER_PATH:"=%"
    if exist "!USER_PATH!" (
        echo Initializing custom MSVC environment...
        call "!USER_PATH!" x86
        goto compile
    ) else (
        echo Error: "!USER_PATH!" does not exist.
    )
)

echo.
echo Build aborted.
exit /b 1

:compile
echo.
echo Compiling Resources...
rc.exe resource.rc
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo Compiling and Linking Source Files...
cl.exe /utf-8 /EHsc /MT /O2 /DUNICODE /D_UNICODE main.cpp scanner.cpp resource.res /FeHddScanner.exe /link /SUBSYSTEM:WINDOWS
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilation or linking failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo BUILD SUCCESS: Produced HddScanner.exe (MSVC Build)
echo ===================================================
exit /b 0

:compile_gcc
echo.
echo Compiling Resources (GCC/windres)...
windres resource.rc -O coff -o resource.res
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo Compiling and Linking Source Files (GCC/g++)...
g++ -O2 -std=c++17 -mwindows -static -DUNICODE -D_UNICODE main.cpp scanner.cpp resource.res -o HddScanner.exe -luser32 -lgdi32 -lcomctl32 -lgdiplus
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilation or linking failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo BUILD SUCCESS: Produced HddScanner.exe (GCC Build)
echo ===================================================
exit /b 0
