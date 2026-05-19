cmake_minimum_required(VERSION 3.20)
project(Rti3dShapesWorkspace LANGUAGES CXX)

# Force modern C++ standards conforming to RTI Connext DDS 7.7 requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# 0. macOS Homebrew Support (Resolve dependencies installed via brew)
# ==============================================================================
if(APPLE)
    # Ensure CMake searches inside Homebrew prefixes on Apple Silicon and Intel Macs
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
    message(STATUS "macOS detected: Adding Homebrew prefix paths to CMAKE_PREFIX_PATH")
endif()

# ==============================================================================
# 1. Environment Verification & RTI Connext DDS SDK Resolution
# ==============================================================================

# Ensure NDDSHOME is defined (either via environment variable or cmake parameter)
if(NOT DEFINED ENV{NDDSHOME})
    message(FATAL_ERROR 
        "NDDSHOME environment variable is not set!\n"
        "Please set NDDSHOME to point to your RTI Connext DDS 7.7 installation directory.\n"
        "Example: export NDDSHOME=/opt/rti_connext_dds-7.7.0"
    )
endif()

set(RTI_NDDS_HOME "$ENV{NDDSHOME}")
message(STATUS "Using RTI Connext DDS SDK located at: ${RTI_NDDS_HOME}")

# Add RTI CMake module path to find package configurations
list(APPEND CMAKE_MODULE_PATH "${RTI_NDDS_HOME}/resource/cmake")

# Find RTI Connext DDS 7.7 core components (Modern C++ is mapped inside core)
find_package(RTIConnextDDS 7.7 REQUIRED COMPONENTS core)

# ==============================================================================
# 2. Graphics Dependencies Resolution (GLFW3, OpenGL)
# ==============================================================================

# Find OpenGL framework
find_package(OpenGL REQUIRED)

# --- Multi-Tier Fallback Pipeline to locate GLFW3 on macOS ---
message(STATUS "Searching for GLFW3 dependencies...")

# Tier 1: Try finding GLFW3 CMake config files
find_package(glfw3 CONFIG QUIET)

if(NOT glfw3_FOUND)
    # Check if registered under 'glfw' instead of 'glfw3'
    find_package(glfw CONFIG QUIET)
endif()

if(NOT glfw3_FOUND AND NOT glfw_FOUND)
    # Tier 2: Try finding via PkgConfig (Highly reliable under Homebrew)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GLFW QUIET glfw3)
    endif()
endif()

# Tier 3: Manual Direct Filesystem Scan (Absolute Fallback)
if(NOT glfw3_FOUND AND NOT glfw_FOUND AND NOT GLFW_FOUND)
    message(STATUS "GLFW CMake configs not resolved. Falling back to direct filesystem scanning...")
    
    find_path(GLFW_INCLUDE_DIR
        NAMES GLFW/glfw3.h
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )
    
    find_library(GLFW_LIBRARY
        NAMES glfw glfw3
        PATHS
            /opt/homebrew/lib
            /usr/local/lib
            /usr/lib
    )
    
    if(GLFW_INCLUDE_DIR AND GLFW_LIBRARY)
        set(GLFW_FOUND TRUE)
        message(STATUS "Found GLFW manually:")
        message(STATUS "  - Headers: ${GLFW_INCLUDE_DIR}")
        message(STATUS "  - Library: ${GLFW_LIBRARY}")
    else()
        message(FATAL_ERROR 
            "Could NOT find GLFW library (tried CMake package configurations, PkgConfig, and manual scan).\n"
            "Please verify that GLFW is installed.\n"
            "On macOS, run: brew install glfw"
        )
    endif()
endif()

# Standardize include path and target name definitions based on discovery method
if(TARGET glfw)
    set(GLFW_LINK_LIBRARY glfw)
    message(STATUS "Using GLFW Target: glfw")
elseif(TARGET glfw3)
    set(GLFW_LINK_LIBRARY glfw3)
    message(STATUS "Using GLFW Target: glfw3")
elseif(glfw3_FOUND)
    set(GLFW_LINK_LIBRARY glfw)
elseif(GLFW_FOUND)
    if(GLFW_LIBRARIES)
        set(GLFW_LINK_LIBRARY ${GLFW_LIBRARIES})
    else()
        set(GLFW_LINK_LIBRARY ${GLFW_LIBRARY})
    endif()
    
    if(GLFW_INCLUDE_DIRS)
        set(GLFW_INCLUDE_PATH ${GLFW_INCLUDE_DIRS})
    else()
        set(GLFW_INCLUDE_PATH ${GLFW_INCLUDE_DIR})
    endif()
else()
    # Ultimate linker fallback
    set(GLFW_LINK_LIBRARY glfw)
endif()

# ==============================================================================
# 3. Automatic rtiddsgen Compilation Pipeline (Extensible Topic Generator)
# ==============================================================================

# Resolve paths to the rtiddsgen executable based on the operating system
if(WIN32)
    set(RTIDDSGEN_EXE "${RTI_NDDS_HOME}/bin/rtiddsgen.bat")
else()
    set(RTIDDSGEN_EXE "${RTI_NDDS_HOME}/bin/rtiddsgen")
endif()

# Check that the code generator executable physically exists
if(NOT EXISTS "${RTIDDSGEN_EXE}")
    message(FATAL_ERROR "Could not find rtiddsgen utility at: ${RTIDDSGEN_EXE}")
endif()

# Define paths for IDL schemas and generated files
set(IDL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/idl/ShapeType.idl")
set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated")

# Modern C++11 mapping outputs both the main type and the serialization Plugin implementation.
set(GENERATED_SOURCES
    "${GEN_DIR}/ShapeType.cxx"
    "${GEN_DIR}/ShapeTypePlugin.cxx"
)
set(GENERATED_HEADERS
    "${GEN_DIR}/ShapeType.hpp"
    "${GEN_DIR}/ShapeTypePlugin.hpp"
)

# Custom build command to compile IDL to C++ when the IDL changes
add_custom_command(
    OUTPUT ${GENERATED_SOURCES} ${GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
    COMMAND ${RTIDDSGEN_EXE} -language C++11 -replace -d ${GEN_DIR} ${IDL_FILE}
    DEPENDS ${IDL_FILE}
    COMMENT "RTI rtiddsgen: Compiling ShapeType.idl to modern C++ bindings..."
    VERBATIM
)

# ==============================================================================
# 3.5. Dear ImGui FetchContent Integration
# ==============================================================================
include(FetchContent)
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
)
FetchContent_MakeAvailable(imgui)

# ImGui lacks a native CMakeLists.txt, so we compile it manually into a static library
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Use SYSTEM to treat as third-party headers and suppress header warnings during compile
target_include_directories(imgui_lib SYSTEM PUBLIC 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)
# ImGui backends require GLFW and OpenGL definitions
target_link_libraries(imgui_lib PRIVATE ${GLFW_LINK_LIBRARY} OpenGL::GL)

# Disable all compiler warnings for the ImGui source files to avoid Clang noise
if(MSVC)
    target_compile_options(imgui_lib PRIVATE /w)
else()
    target_compile_options(imgui_lib PRIVATE -w)
endif()

# ==============================================================================
# 4. Target Orchestration & Include Configurations
# ==============================================================================

# Executable target grouping manually created source files and generated types
add_executable(3d_shapes
    src/main.cpp
    src/dds/dds_manager.cpp
    src/graphics/graphics_engine.cpp
    ${GENERATED_SOURCES}
)

# Set compile flags for code warnings & optimizations
if(MSVC)
    target_compile_options(3d_shapes PRIVATE /W4 /WX /Ot)
else()
    target_compile_options(3d_shapes PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

# Silence OpenGL API deprecation warnings on macOS
if(APPLE)
    target_compile_definitions(3d_shapes PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Map include directories
target_include_directories(3d_shapes PRIVATE
    src
    src/dds
    src/graphics
    src/idl
    ${GEN_DIR}
    ${GLFW_INCLUDE_PATH} # Standardized GLFW header directory
)

# ==============================================================================
# 5. Link Libraries Framework (DDS, Threads, Graphics)
# ==============================================================================

# Search and configure local system Thread dependencies (pthreads)
find_package(Threads REQUIRED)

target_link_libraries(3d_shapes PRIVATE
    RTIConnextDDS::cpp2_api        # Official RTI Connext Modern C++ API target
    imgui_lib                      # Dear ImGui library + Backends
    ${GLFW_LINK_LIBRARY}           # Standardized GLFW variable target
    OpenGL::GL                     # OpenGL Framework Context
    Threads::Threads               # System Pthread support
)

message(STATUS "CMake Configuration complete. Run 'cmake --build build' to compile target.")
