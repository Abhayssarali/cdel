# Dead Instruction Eliminator ⚡

An out-of-tree **LLVM 17** compiler optimization pass that performs **Dead Instruction Elimination (DIE)** using custom iterative **Use-Def Chain Analysis**, wrapped in a modern, interactive web frontend.

## What it does
The compiler pass analyzes LLVM Intermediate Representation (IR) to identify and safely remove instructions whose results are never used (dead code). It also implements local **Dead Store Elimination** by identifying memory allocations (`alloca`) that are stored into but never loaded from.

## Prerequisites
- **LLVM 17** (`clang-17`, `opt-17`, `llvm-17-dev`)
- **CMake** (v3.13.4 or higher)
- **Node.js** (for the web frontend)
- A Linux environment (or WSL on Windows)

## How to Build & Run (Command Line)

You can use the included scripts to build the C++ pass:
```bash
./build.sh
```

To run the tool on a test case:
```bash
./run.sh testcases/dead_code.c
```
*(This script will automatically use `clang-17` to compile the C code to LLVM IR, and then run the `die-tool` pass on it).*

## How to Run (Web Interface)

The project includes an interactive Node.js Express server that bridges the web UI with the backend compiler.

1. Install dependencies:
   ```bash
   npm install
   ```
2. Start the server:
   ```bash
   npm start
   ```
3. Open your browser and navigate to: **http://localhost:3000**
