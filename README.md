# Dead Instruction Eliminator ⚡

An out-of-tree **LLVM 17** compiler optimization pass that performs **Dead Instruction Elimination (DIE)** using custom iterative **Use-Def Chain Analysis**, wrapped in a modern, interactive web frontend.

## Features
- **Custom Use-Def Analysis**: Iteratively tracks the operands and users of every instruction.
- **Worklist Algorithm**: Cascading elimination—when an instruction is deleted, it checks if the parent instructions are now dead and eliminates them too.
- **Automated C Compilation**: Web frontend allows writing raw C code, which is automatically compiled into LLVM IR via `clang` and lifted to SSA registers using `mem2reg` before optimization.
- **Visual Dashboard**: Modern glassmorphic UI displaying the unoptimized IR, the final optimized IR, and a detailed Use-Def analysis report showing exactly which instructions were marked `[✓ LIVE]` or `[✗ DEAD]`.

## Prerequisites
- **LLVM 17** (`clang-17`, `opt-17`, `llvm-17-dev`)
- **CMake** (v3.13.4 or higher)
- **Node.js** (for the web frontend)
- A Linux environment (or WSL on Windows)

## Build Instructions (Backend)

1. Clone the repository and navigate into it:
   ```bash
   mkdir build
   cd build
   ```
2. Run CMake and compile:
   ```bash
   cmake ..
   make
   ```
This will produce the standalone tool `die-tool` in the `build/` directory.

## Running the Web Server

The project includes a Node.js Express server that bridges the web UI with the WSL/Linux command-line compiler backend.

1. Install dependencies:
   ```bash
   npm install
   ```
2. Start the server:
   ```bash
   npm start
   ```
3. Open your browser and navigate to: **http://localhost:3000**

## Project Structure
- `src/` - Contains the C++ LLVM Pass logic (`DeadInstructionEliminator.cpp` & `main.cpp`).
- `public/` - Contains the HTML, CSS, and JS for the frontend (Monaco Editor integration).
- `server.js` - The Express backend that coordinates `clang-17`, `opt-17`, and `die-tool`.
- `tests/` - Example `.ll` test cases and temporary execution files.

## How It Works Under The Hood
1. The server saves the C code input and runs `clang-17 -O0` to generate unoptimized LLVM IR.
2. The server runs `opt-17 -passes=mem2reg` to promote memory `alloca` / `store` / `load` instructions into pure SSA virtual registers.
3. The server runs `./build/die-tool`. The pass builds a graph of all instruction uses.
4. An instruction is seeded into the worklist if it is "Safe to Delete" (has no side effects like memory stores or branches) AND has `0` uses.
5. The algorithm iterates until the worklist is empty, continuously stripping away dead mathematical operations and assignments.
