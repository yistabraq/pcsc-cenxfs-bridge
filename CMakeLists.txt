cmake_minimum_required(VERSION 3.10)
project(PCSC-XFS-Bridge VERSION 1.0.0 LANGUAGES CXX)

# Définir les politiques CMake
cmake_policy(SET CMP0074 NEW)  # Utiliser les variables _ROOT
cmake_policy(SET CMP0144 NEW)  # Utiliser les variables en majuscules

# Forcer l'utilisation de vcpkg
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "VCPKG_ROOT environment variable is not set. Please install vcpkg and set VCPKG_ROOT.")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
endif()

# Configuration du standard C++
if(MSVC)
    # Pour MSVC, utiliser C++14 qui est rétrocompatible avec C++03 pour notre code
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # Désactiver les extensions Microsoft
    set(CMAKE_CXX_EXTENSIONS OFF)
    # Ajouter des flags spécifiques pour la compatibilité
    add_compile_options(/Zc:__cplusplus)
else()
    # Pour les autres compilateurs, utiliser C++03
    set(CMAKE_CXX_STANDARD 03)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Options de compilation
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_DEBUG "Enable debug build" ON)

# Définir les flags de compilation
if(MSVC)
    add_compile_options(/W4 /EHsc)
    if(ENABLE_DEBUG)
        add_compile_definitions(_DEBUG)
        # Activer le débogage CRT en mode Debug
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /D_DEBUG")
    else()
        # Utiliser la version release du CRT en mode Release
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    endif()
    add_compile_definitions(
        _WIN32_WINNT=0x0501
        WIN32_LEAN_AND_MEAN
    )
endif()

# Configuration Boost via vcpkg
find_package(Boost REQUIRED COMPONENTS chrono thread date_time)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Please ensure vcpkg is properly installed and run 'vcpkg install boost:x86-windows'")
endif()
message(STATUS "Found Boost ${Boost_VERSION} via vcpkg")

# Configuration XFS SDK
if(NOT DEFINED ENV{XFS_SDK})
    message(FATAL_ERROR "XFS_SDK environment variable is not set")
endif()
set(XFS_SDK $ENV{XFS_SDK})

# Vérifier que les fichiers XFS existent
if(NOT EXISTS "${XFS_SDK}/INCLUDE/XFSAPI.H")
    message(FATAL_ERROR "XFS SDK not found at ${XFS_SDK}. Please check XFS_SDK environment variable.")
endif()

# Inclure les répertoires
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
    "${XFS_SDK}/INCLUDE"
    "${XFS_SDK}/LIB"
)

# Ajouter des définitions pour le SDK XFS
add_definitions(
    -DWIN32
    -D_WINDOWS
    -D_X86_
    -DWINVER=0x0501
    -D_WIN32_WINNT=0x0501
    -DWFS_DYNAMIC_LOAD  # Indique que les fonctions XFS sont chargées dynamiquement
)

# Sources du projet
file(GLOB SOURCES 
    "*.cpp"
    "PCSC/*.cpp"
    "XFS/*.cpp"
    "Utils/*.cpp"
)

# Headers
file(GLOB HEADERS 
    "*.h"
    "PCSC/*.h"
    "XFS/*.h"
    "Utils/*.h"
)

# Créer la bibliothèque
add_library(PCSCspi SHARED ${SOURCES} ${HEADERS})

# Propriétés de la DLL
set_target_properties(PCSCspi PROPERTIES
    OUTPUT_NAME "PCSCspi"
    PREFIX ""
    SUFFIX ".dll"
)

# Lier les bibliothèques
target_link_libraries(PCSCspi PRIVATE
    ${Boost_LIBRARIES}
    winscard
    "${XFS_SDK}/LIB/msxfs.lib"
)

# Copier les DLLs XFS dans le répertoire de sortie
add_custom_command(TARGET PCSCspi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:PCSCspi>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${XFS_SDK}/LIB/msxfs.dll"
        "${XFS_SDK}/LIB/xfs_conf.dll"
        "${XFS_SDK}/LIB/xfs_supp.dll"
        $<TARGET_FILE_DIR:PCSCspi>
    COMMENT "Copying XFS DLLs to output directory"
)

# Installation
install(TARGETS PCSCspi
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Message de configuration
message(STATUS "Using vcpkg: ${USE_VCPKG}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")
message(STATUS "XFS_SDK: ${XFS_SDK}") 