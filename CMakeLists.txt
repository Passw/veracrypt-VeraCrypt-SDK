cmake_minimum_required(VERSION 3.16)

# Project configuration
project(VeraCryptSDK
    VERSION 1.26.24
    DESCRIPTION "VeraCrypt SDK for creating encrypted volumes"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform and architecture detection
if(DEFINED CMAKE_GENERATOR_PLATFORM)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _platform)
    if(_platform STREQUAL "arm64")
        set(ARCH_DIR "arm64")
    elseif(_platform STREQUAL "x64" OR _platform STREQUAL "amd64")
        set(ARCH_DIR "x64")
    else()
        message(FATAL_ERROR "Unsupported generator platform: ${CMAKE_GENERATOR_PLATFORM}")
    endif()
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)
    if(_proc MATCHES "arm64" OR _proc MATCHES "aarch64")
        set(ARCH_DIR "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_DIR "x64")
    else()
        message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

# Verify we're on Windows
if(NOT WIN32)
    message(FATAL_ERROR "This SDK is currently only supported on Windows")
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For multi-config generators (Visual Studio)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
endforeach()

# SDK paths
set(SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SDK_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/${ARCH_DIR}")
set(SDK_BIN_DIR "${CMAKE_SOURCE_DIR}/bin/${ARCH_DIR}")

# Verify SDK files exist
if(NOT EXISTS "${SDK_INCLUDE_DIR}/VeraCryptFormatSDK.h")
    message(FATAL_ERROR "SDK header file not found: ${SDK_INCLUDE_DIR}/VeraCryptFormatSDK.h")
endif()

if(NOT EXISTS "${SDK_LIB_DIR}/VeraCryptFormat.lib")
    message(FATAL_ERROR "SDK library file not found: ${SDK_LIB_DIR}/VeraCryptFormat.lib")
endif()

if(NOT EXISTS "${SDK_BIN_DIR}/VeraCryptFormat.dll")
    message(FATAL_ERROR "SDK DLL file not found: ${SDK_BIN_DIR}/VeraCryptFormat.dll")
endif()

# Create imported target for VeraCrypt Format library
add_library(VeraCryptFormat SHARED IMPORTED)
set_target_properties(VeraCryptFormat PROPERTIES
    IMPORTED_LOCATION "${SDK_BIN_DIR}/VeraCryptFormat.dll"
    IMPORTED_IMPLIB "${SDK_LIB_DIR}/VeraCryptFormat.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SDK_INCLUDE_DIR}"
)

# Sample tool executable
add_executable(VeraCryptFormatTool
    VeraCryptFormatTool.cpp
)

# Link the sample tool with the SDK and required system libraries
target_link_libraries(VeraCryptFormatTool
    PRIVATE
        VeraCryptFormat
        shlwapi
)

# Set target properties
set_target_properties(VeraCryptFormatTool PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "VeraCryptFormatTool"
)

# Compiler-specific settings
if(MSVC)
    # Enable UTF-8 source code encoding
    target_compile_options(VeraCryptFormatTool PRIVATE /utf-8)
    
    # Set warning level
    target_compile_options(VeraCryptFormatTool PRIVATE /W3)
    
    # Define WIN32_LEAN_AND_MEAN to reduce Windows header bloat
    target_compile_definitions(VeraCryptFormatTool PRIVATE WIN32_LEAN_AND_MEAN)
    
    # Enable security features
    target_compile_options(VeraCryptFormatTool PRIVATE /GS)
    target_link_options(VeraCryptFormatTool PRIVATE /NXCOMPAT /DYNAMICBASE)
    
    # Set subsystem to console
    set_target_properties(VeraCryptFormatTool PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Copy DLL to output directory after build
add_custom_command(TARGET VeraCryptFormatTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDK_BIN_DIR}/VeraCryptFormat.dll"
        "$<TARGET_FILE_DIR:VeraCryptFormatTool>/VeraCryptFormat.dll"
    COMMENT "Copying VeraCryptFormat.dll to output directory"
)

# Install configuration
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

# Install the sample tool
install(TARGETS VeraCryptFormatTool
    RUNTIME DESTINATION bin
    COMPONENT Applications
)

# Install the DLL
install(FILES "${SDK_BIN_DIR}/VeraCryptFormat.dll"
    DESTINATION bin
    COMPONENT Runtime
)

# Install SDK files for development
install(FILES "${SDK_INCLUDE_DIR}/VeraCryptFormatSDK.h"
    DESTINATION include
    COMPONENT Development
)

install(FILES "${SDK_LIB_DIR}/VeraCryptFormat.lib"
    DESTINATION lib
    COMPONENT Development
)

# Install documentation
install(FILES 
    README.md
    LICENSE
    DESTINATION .
    COMPONENT Documentation
)

# Package configuration
set(CPACK_PACKAGE_NAME "VeraCryptSDK")
set(CPACK_PACKAGE_VENDOR "VeraCrypt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "VeraCrypt SDK for creating encrypted volumes")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VeraCryptSDK")

# Package components
set(CPACK_COMPONENTS_ALL Applications Runtime Development Documentation)
set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Sample Tool")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "VeraCryptFormatTool sample application")
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime Libraries")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Required DLL files")
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Headers and import libraries for development")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "README and license files")

# Set component dependencies
set(CPACK_COMPONENT_APPLICATIONS_DEPENDS Runtime)
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)

# Generator specific settings
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "VeraCrypt SDK")
    set(CPACK_NSIS_PACKAGE_NAME "VeraCrypt SDK")
    set(CPACK_NSIS_MODIFY_PATH ON)
endif()

# Set CPACK_SYSTEM_NAME to match target architecture for correct package naming
if(DEFINED CMAKE_GENERATOR_PLATFORM)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _platform)
    if(_platform STREQUAL "arm64")
        set(CPACK_SYSTEM_NAME "Windows-ARM64")
    elseif(_platform STREQUAL "x64" OR _platform STREQUAL "amd64")
        set(CPACK_SYSTEM_NAME "Windows-x64")
    endif()
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)
    if(_proc MATCHES "arm64" OR _proc MATCHES "aarch64")
        set(CPACK_SYSTEM_NAME "Windows-ARM64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_SYSTEM_NAME "Windows-x64")
    endif()
endif()

include(CPack)

# Print configuration summary
message(STATUS "VeraCrypt SDK Configuration Summary:")
message(STATUS "  Architecture: ${ARCH_DIR}")
message(STATUS "  SDK Include: ${SDK_INCLUDE_DIR}")
message(STATUS "  SDK Library: ${SDK_LIB_DIR}")
message(STATUS "  SDK DLL: ${SDK_BIN_DIR}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")

# Optional: Add testing support
enable_testing()

# Add a simple test to verify the tool can be executed
add_test(NAME VeraCryptFormatTool_Help
    COMMAND VeraCryptFormatTool --help
)

# Set test properties
set_tests_properties(VeraCryptFormatTool_Help PROPERTIES
    PASS_REGULAR_EXPRESSION "Usage: VeraCryptFormatTool"
)

# Print build instructions
message(STATUS "")
message(STATUS "Build Instructions:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  cmake --build . --config Release")
message(STATUS "")
message(STATUS "Install Instructions:")
message(STATUS "  cmake --install . --config Release")
message(STATUS "")
message(STATUS "Package Instructions:")
message(STATUS "  cpack -C Release")
