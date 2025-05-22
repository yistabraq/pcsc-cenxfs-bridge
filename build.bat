@echo off
setlocal enabledelayedexpansion

REM Vérifier si vcpkg est installé
if not defined VCPKG_ROOT (
    echo Error: VCPKG_ROOT environment variable is not set
    echo Please install vcpkg and set VCPKG_ROOT environment variable
    exit /b 1
)

REM Vérifier si les dépendances sont installées
if not exist "%VCPKG_ROOT%\installed\x86-windows\include\boost\version.hpp" (
    echo Installing dependencies via vcpkg...
    call "%VCPKG_ROOT%\vcpkg" install --triplet x86-windows
    if errorlevel 1 (
        echo Error: Failed to install dependencies
        exit /b 1
    )
)

REM Nettoyer le cache CMake
if exist build (
    echo Cleaning CMake cache...
    rmdir /s /q build
)
mkdir build
cd build

REM Configurer avec CMake
echo Configuring with CMake...
cmake .. -A Win32 ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET=x86-windows

if errorlevel 1 (
    echo Error: CMake configuration failed
    exit /b 1
)

REM Construire le projet en mode Debug
echo Building project (Debug)...
cmake --build . --config Debug

if errorlevel 1 (
    echo Error: Debug build failed
    exit /b 1
)

echo Build completed successfully in Debug configuration.
cd .. 