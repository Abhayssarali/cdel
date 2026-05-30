#!/bin/bash
# Script to run the Dead Instruction Eliminator pass on a test case

set -e

if [ "$#" -ne 1 ]; then
    echo "Usage: ./run.sh <input file (.c or .ll)>"
    exit 1
fi

INPUT_FILE=$1
BASENAME=$(basename -- "$INPUT_FILE")
EXTENSION="${BASENAME##*.}"
FILENAME="${BASENAME%.*}"

mkdir -p tests/temp

# Check if the tool is built
if [ ! -f "./build/die-tool" ]; then
    echo "[!] Tool not found. Running ./build.sh first..."
    ./build.sh
fi

echo "=========================================="
echo " Running DIE Tool on $INPUT_FILE"
echo "=========================================="

# If input is a C file, compile it to LLVM IR first
if [ "$EXTENSION" = "c" ]; then
    TEMP_IR="tests/temp/${FILENAME}_unopt.ll"
    echo "-> Compiling C to LLVM IR (-O0) via clang-17..."
    clang-17 -S -emit-llvm -O0 -Xclang -disable-O0-optnone "$INPUT_FILE" -o "$TEMP_IR"
    
    echo "-> Promoting memory to registers via mem2reg..."
    opt-17 -passes=mem2reg -S "$TEMP_IR" -o "$TEMP_IR"
    
    # Run the pass on the generated IR
    echo "-> Running die-tool pass..."
    ./build/die-tool "$TEMP_IR" -v
else
    # Run directly on provided LLVM IR
    echo "-> Running die-tool pass..."
    ./build/die-tool "$INPUT_FILE" -v
fi

echo "=========================================="
