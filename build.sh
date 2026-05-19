#!/bin/bash
# ==============================================================================
# (c) Copyright, Real-Time Innovations, All rights reserved.
#
# Bash compilation script for 3d_shapes on macOS.
# ==============================================================================

# Exit immediately if any command fails
set -e

# 1. Set NDDSHOME to your macOS installation directory
export NDDSHOME="/Applications/rti_connext_dds-7.7.0"

echo "======================================================================"
echo " RTI Connext DDS 7.7 macOS Build System"
echo " SDK Path: $NDDSHOME"
echo "======================================================================"

# Verify that the directory exists before proceeding
if [ ! -d "$NDDSHOME" ]; then
    echo "ERROR: Could not find RTI Connext DDS at $NDDSHOME"
    echo "Please verify the installation directory and try again."
    exit 1
fi

# 2. Source the official RTI architecture environment script
# This populates critical variables like CONNEXTDDS_ARCH and library pathing
SPECIFIC_ENV_SCRIPT="$NDDSHOME/resource/scripts/rtisetenv_arm64Darwin23clang16.0.bash"

if [ -f "$SPECIFIC_ENV_SCRIPT" ]; then
    echo "-> Sourcing official RTI environment script..."
    echo "   File: $SPECIFIC_ENV_SCRIPT"
    # Source using '.' (portable equivalent to source)
    . "$SPECIFIC_ENV_SCRIPT"
else
    echo "-> Specific environment script not found at: $SPECIFIC_ENV_SCRIPT"
    echo "   Searching resource/scripts/ for any compatible fallback..."
    
    FALLBACK_SCRIPT=$(ls "$NDDSHOME"/resource/scripts/rtisetenv_*.bash 2>/dev/null | head -n 1 || true)
    if [ -n "$FALLBACK_SCRIPT" ]; then
        echo "-> Sourcing fallback environment script..."
        echo "   File: $FALLBACK_SCRIPT"
        . "$FALLBACK_SCRIPT"
    else
        echo "-> WARNING: No rtisetenv_*.bash scripts found. Proceeding with manual config."
    fi
fi

# Print out the current architecture to verify success
if [ -n "$CONNEXTDDS_ARCH" ]; then
    echo "-> Active DDS Architecture Target: $CONNEXTDDS_ARCH"
else
    # Fallback to manual selection if variables didn't export
    export CONNEXTDDS_ARCH="arm64Darwin23clang16.0"
    echo "-> Manually assigning default Architecture Target: $CONNEXTDDS_ARCH"
fi

# Export to make sure the nested subshells can read it
export CONNEXTDDS_ARCH

# 3. Detect library types (Shared vs Static) in the target architecture folder
echo "-> Inspecting library format in $CONNEXTDDS_ARCH..."
TARGET_LIB_DIR="$NDDSHOME/lib/$CONNEXTDDS_ARCH"
HAS_DYLIB=$(find "$TARGET_LIB_DIR" -name "*.dylib" | wc -l | tr -d ' ' || echo 0)
HAS_STATIC=$(find "$TARGET_LIB_DIR" -name "*.a" | wc -l | tr -d ' ' || echo 0)

EXTRA_CMAKE_ARGS=""
if [ "$HAS_DYLIB" -eq 0 ] && [ "$HAS_STATIC" -gt 0 ]; then
    echo "-> Note: Only static libraries (.a) detected. Configuring CMake for static link mode."
    EXTRA_CMAKE_ARGS="-DCONNEXTDDS_SHARED_LIBS=OFF"
elif [ "$HAS_DYLIB" -gt 0 ]; then
    echo "-> Note: Shared libraries (.dylib) detected. Configuring CMake for shared link mode."
    EXTRA_CMAKE_ARGS="-DCONNEXTDDS_SHARED_LIBS=ON"
else
    echo "-> Warning: Could not detect library files (.dylib or .a) in $TARGET_LIB_DIR."
    echo "   Will proceed with CMake defaults."
fi

# 4. Create and navigate into the transient build directory
mkdir -p build
cd build

# Remove existing CMake cache to force a clean, fresh configuration run
rm -f CMakeCache.txt

# 5. Configure the generator files via CMake passing the discovered architecture
# ==============================================================================
# SPECIAL FIXES APPLIED:
# - Passed -DCONNEXTDDS_DIR to bind the core SDK search root directly.
# - Enabled -DCONNEXTDDS_DEBUG to force internally verbose Find trace scripts.
# - Kept compiler & architecture bypass flags for unsupported AppleClang environments.
# ==============================================================================
echo "-> Running CMake configuration with diagnostic and compiler override rules..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCONNEXTDDS_DIR="$NDDSHOME" \
      -DCONNEXTDDS_ARCH=$CONNEXTDDS_ARCH \
      -DCONNEXTDDS_DEBUG=ON \
      -DCONNEXTDDS_ALLOW_UNSUPPORTED_COMPILER=ON \
      -DCONNEXTDDS_ALLOW_UNSUPPORTED_ARCH=ON \
      $EXTRA_CMAKE_ARGS \
      ..

# 6. Compile the binaries
echo "-> Compiling 3d_shapes target..."
cmake --build . --config Release

echo "======================================================================"
echo " BUILD SUCCESSFUL!"
echo " Execute the simulation using: ./3d_shapes"
echo "======================================================================"
 
