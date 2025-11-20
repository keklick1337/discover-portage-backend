cmake_minimum_required(VERSION 3.16)

project(discover-portage-backend VERSION 1.0.0)

set(QT_MIN_VERSION "6.5.0")
set(KF6_MIN_VERSION "6.0.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Xml
    Qml
    Quick
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    Config
    Auth
)

## Use system-installed Discover library (architecture-agnostic)
set(DISCOVER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../include")

# Try to find libDiscoverCommon in common library locations. First look under
# plasma-discover subfolders (e.g. /usr/lib64/plasma-discover/libDiscoverCommon.so),
# then fall back to default search paths.
find_library(DISCOVER_LIB
    NAMES DiscoverCommon libDiscoverCommon
    PATHS /usr/lib /usr/lib64 /usr/local/lib
    PATH_SUFFIXES plasma-discover
    NO_DEFAULT_PATH
)

if(NOT DISCOVER_LIB)
    # Second attempt: let the system default search find it (for non-standard layouts)
    find_library(DISCOVER_LIB NAMES DiscoverCommon libDiscoverCommon)
endif()

if(NOT DISCOVER_LIB)
    message(FATAL_ERROR "Discover library 'libDiscoverCommon' not found. Please install kde-plasma/discover.")
endif()

message(STATUS "Found Discover library: ${DISCOVER_LIB}")

# Extract plugin IID from the installed Discover library
execute_process(
    COMMAND sh -c "strings '${DISCOVER_LIB}' | grep 'org.kde.discover.*AbstractResourcesBackendFactory' | head -1"
    OUTPUT_VARIABLE DISCOVERED_PLUGIN_IID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT DISCOVERED_PLUGIN_IID)
    message(FATAL_ERROR "Could not extract DISCOVER_PLUGIN_IID from ${DISCOVER_LIB}")
endif()

message(STATUS "Extracted DISCOVER_PLUGIN_IID: ${DISCOVERED_PLUGIN_IID}")

# Extract version from PLUGIN_IID (e.g. "org.kde.discover.6.4.5.AbstractResourcesBackendFactory" -> "6.4.5")
# and construct NOTIFIER_IID with the same version
string(REGEX MATCH "org\\.kde\\.discover\\.([0-9.]+)\\." VERSION_MATCH "${DISCOVERED_PLUGIN_IID}")
if(CMAKE_MATCH_1)
    set(DISCOVER_VERSION "${CMAKE_MATCH_1}")
    set(DISCOVERED_NOTIFIER_IID "org.kde.discover.${DISCOVER_VERSION}.BackendNotifierModule")
    message(STATUS "Constructed DISCOVER_NOTIFIER_IID: ${DISCOVERED_NOTIFIER_IID}")
else()
    # Fallback if no version found in IID (shouldn't happen with modern Discover)
    set(DISCOVERED_NOTIFIER_IID "org.kde.discover.BackendNotifierModule")
    message(STATUS "Using version-agnostic DISCOVER_NOTIFIER_IID: ${DISCOVERED_NOTIFIER_IID}")
endif()

# Generate DiscoverConfig.h from template
configure_file(
    "${DISCOVER_INCLUDE_DIR}/libdiscover/DiscoverConfig.h.in"
    "${DISCOVER_INCLUDE_DIR}/libdiscover/DiscoverConfig.h"
    @ONLY
)

# Detect Discover version from installed package
execute_process(
    COMMAND sh -c "basename /var/db/pkg/kde-plasma/discover-* 2>/dev/null | cut -d'-' -f2- || echo '6.4.5'"
    OUTPUT_VARIABLE INSTALLED_DISCOVER_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Detected Discover version: ${INSTALLED_DISCOVER_VERSION}")

# Determine which Category.h to use based on version
# Version 6.4.90+ uses Category::Type enum, older versions use bool isAddons
if(INSTALLED_DISCOVER_VERSION VERSION_GREATER_EQUAL "6.4.90")
    message(STATUS "Using new Category.h (with Type enum) for Discover ${INSTALLED_DISCOVER_VERSION}")
    # Copy new version to standard location
    configure_file(
        "${DISCOVER_INCLUDE_DIR}/libdiscover/Category_new/Category.h"
        "${DISCOVER_INCLUDE_DIR}/libdiscover/Category/Category.h"
        COPYONLY
    )
    add_compile_definitions(DISCOVER_CATEGORY_HAS_TYPE_ENUM)
else()
    message(STATUS "Using old Category.h (with bool isAddons) for Discover ${INSTALLED_DISCOVER_VERSION}")
    # Copy old version to standard location
    configure_file(
        "${DISCOVER_INCLUDE_DIR}/libdiscover/Category_645/Category.h"
        "${DISCOVER_INCLUDE_DIR}/libdiscover/Category/Category.h"
        COPYONLY
    )
endif()

# Add include directories for bundled Discover headers
include_directories(${DISCOVER_INCLUDE_DIR}/libdiscover)
include_directories(${DISCOVER_INCLUDE_DIR})

# Create Discover::Common interface target pointing to discovered library
add_library(Discover::Common INTERFACE IMPORTED)
set_target_properties(Discover::Common PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${DISCOVER_INCLUDE_DIR}/libdiscover;${DISCOVER_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${DISCOVER_LIB}"
)

add_subdirectory(PortageBackend)

# Clang format
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES PortageBackend/*.cpp PortageBackend/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
