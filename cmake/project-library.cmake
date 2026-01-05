# MIT License Copyright (c) 2024-2026 Tomáš Mark

# ==============================================================================
# LIBRARY-SPECIFIC CONFIGURATION
# ==============================================================================
include(${CMAKE_CURRENT_LIST_DIR}/project-common.cmake)

project(
    ${LIBRARY_NAME}
    VERSION 0.0.1
    LANGUAGES C CXX
    DESCRIPTION "template Copyright (c) 2024 TomasMark [at] digitalspace.name"
    HOMEPAGE_URL "https://github.com/tomasmark79/DotNameCpp")

# ==============================================================================
# Build guards
# ==============================================================================
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        WARNING
            "In-source builds. Please make a new directory (called a Build directory) and run CMake from there."
    )
endif()

# ==============================================================================
# Library installation attributes
# ==============================================================================
set(INSTALL_INCLUDE_DIR include/${LIBRARY_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${LIBRARY_NAME}/
        DESTINATION ${INSTALL_INCLUDE_DIR})

# ==============================================================================
# Library dependencies
# ==============================================================================
# Try to find dependencies via find_package (Conan/system), fallback to CPM

CPMAddPackage("gh:tomasmark79/PackageProject.cmake@1.13.0")

# find_package(fmt QUIET) # Disabled to always use CPM to avoid conflicts
if(NOT TARGET fmt::fmt)
    CPMAddPackage(
        GITHUB_REPOSITORY fmtlib/fmt
        GIT_TAG 12.1.0
        OPTIONS "FMT_INSTALL YES" "FMT_TEST NO" "FMT_DOC NO")
endif()

# find_package(nlohmann_json QUIET) # Disabled to always use CPM to avoid conflicts
if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
    CPMAddPackage("gh:nlohmann/json@3.12.0")
    if(TARGET nlohmann_json)
        install(TARGETS nlohmann_json EXPORT ${LIBRARY_NAME}Targets)
    endif()
endif()

CPMAddPackage(
    NAME EmojiModule
    GITHUB_REPOSITORY tomasmark79/EmojiModule
    GIT_TAG main
    # file(COPY ${EmojiModule_SOURCE_DIR}/assets/emoji-test.txt DESTINATION
    # ${CMAKE_CURRENT_SOURCE_DIR}/assets)
)

find_package(tinyxml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Opus REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpr REQUIRED)

# ccache help lots with dpp brainboxdotcc/DPP
# https://github.com/brainboxdotcc/DPP/blob/master/CMakeLists.txt important "CONAN_EXPORTED ON" has
# to be used together with Conan
CPMAddPackage(
    NAME dpp
    GITHUB_REPOSITORY brainboxdotcc/DPP
    GIT_TAG v10.1.4
    OPTIONS "BUILD_SHARED_LIBS ON" "DPP_BUILD_TEST OFF" "DPP_INSTALL OFF" "DPP_NO_CORO OFF"
            "CONAN_EXPORTED ON")
# equivalent to: "DPP_INSTALL ON" in CPM for DPP Target - kept for example
if(TARGET dpp)
    install(TARGETS dpp EXPORT ${LIBRARY_NAME}Targets)
    target_compile_options(dpp INTERFACE -Wno-unused-parameter -Wno-unused-variable)
endif()

CPMAddPackage("gh:tomasmark79/CPMLicenses.cmake@0.0.7")
cpm_licenses_create_disclaimer_target(
    write-licenses-${LIBRARY_NAME} "${CMAKE_CURRENT_BINARY_DIR}/${LIBRARY_NAME}_third_party.txt"
    "${CPM_PACKAGES}")

# ==============================================================================
# Library source files
# ==============================================================================
gather_sources(headers ${CMAKE_CURRENT_SOURCE_DIR}/include)
gather_sources(sources ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==============================================================================
# Create library target
# ==============================================================================
add_library(${LIBRARY_NAME})
target_sources(${LIBRARY_NAME} PRIVATE ${headers} ${sources})

# Apply common target settings
apply_common_target_settings(${LIBRARY_NAME})

# ==============================================================================
# Library-specific configuration
# ==============================================================================
# Emscripten handler
include(${CMAKE_CURRENT_LIST_DIR}/tmplt-emscripten.cmake)
emscripten(${LIBRARY_NAME} 0 1 "")

# Headers
target_include_directories(
    ${LIBRARY_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    PUBLIC $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>)

# Compile options
target_compile_options(
    ${LIBRARY_NAME}
    PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive-;/W4>"
    PUBLIC
        "$<$<AND:$<NOT:$<COMPILE_LANG_AND_ID:CXX,MSVC>>,$<NOT:$<PLATFORM_ID:Darwin>>,$<NOT:$<CXX_COMPILER_ID:Clang>>>:-Wall;-Wextra;-Wpedantic;-MMD;-MP>"
    PUBLIC
        "$<$<AND:$<NOT:$<COMPILE_LANG_AND_ID:CXX,MSVC>>,$<PLATFORM_ID:Darwin>>:-Wall;-Wextra;-Wpedantic>"
)

# C++ standard
target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Linking
target_link_libraries(
    ${LIBRARY_NAME}
    PUBLIC fmt::fmt
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC cpr::cpr
    PRIVATE Opus::opus
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PUBLIC tinyxml2::tinyxml2
    PUBLIC CURL::libcurl
    PUBLIC EmojiModuleLib
    PUBLIC dpp)

# ==============================================================================
# Package configuration
# ==============================================================================
packageProject(
    NAME ${LIBRARY_NAME}
    VERSION ${PROJECT_VERSION}
    BINARY_DIR ${PROJECT_BINARY_DIR}
    INCLUDE_DIR "/include"
    INCLUDE_DESTINATION "include"
    INCLUDE_HEADER_PATTERN "*.h;*.hpp;*.hh;*.hxx"
    DEPENDENCIES "fmt#12.1.0;nlohmann_json#3.12.0;CPMLicenses.cmake@0.0.7;dpp#10.1.4;EmojiModuleLib@main"
    VERSION_HEADER "${LIBRARY_NAME}/version.h"
    EXPORT_HEADER "${LIBRARY_NAME}/export.h"
    NAMESPACE ${LIBRARY_NAMESPACE}
    COMPATIBILITY AnyNewerVersion
    DISABLE_VERSION_SUFFIX YES
    ARCH_INDEPENDENT NO
    CPACK YES
    RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})

# Workaround: Ensure .dll files go to bin/ on Windows, not bin/LibraryName/
if(WIN32 AND BUILD_SHARED_LIBS)
    install(TARGETS ${LIBRARY_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)

    # Remove duplicate DLL created by packageProject
    install(
        CODE "
        file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${LIBRARY_NAME}\")
        message(STATUS \"Removed duplicate DLL directory: ${CMAKE_INSTALL_BINDIR}/${LIBRARY_NAME}\")
    ")
endif()
