#!/bin/bash
# Script to build the Dead Instruction Eliminator pass

set -e

echo "=========================================="
echo " Building Dead Instruction Eliminator Tool"
echo "=========================================="

# Create build directory
mkdir -p build
cd build

# Run CMake
echo "-> Running CMake..."
cmake ..

# Compile
echo "-> Compiling..."
make -j$(nproc)

echo ""
echo "[✓] Build Complete! The tool is available at: ./build/die-tool"
echo "=========================================="
