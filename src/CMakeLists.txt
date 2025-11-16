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

# Use Discover headers from submodule
set(DISCOVER_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../discover/libdiscover")
set(DISCOVER_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../discover/build")
include_directories(${DISCOVER_COMMON_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # For copied export headers

# Link to the built Discover library
link_directories(${DISCOVER_BUILD_DIR}/bin)

# Create Discover::Common interface target
add_library(Discover::Common INTERFACE IMPORTED)
set_target_properties(Discover::Common PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${DISCOVER_COMMON_DIR};${CMAKE_CURRENT_SOURCE_DIR}"
    INTERFACE_LINK_LIBRARIES "DiscoverCommon"
)

add_subdirectory(PortageBackend)

# Clang format
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES PortageBackend/*.cpp PortageBackend/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
