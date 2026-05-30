# Design Document

## Overall Approach
Our Dead Instruction Eliminator (DIE) is designed as an out-of-tree LLVM 17 pass. The primary goal is to strip mathematically pure or memory-isolated instructions whose outputs are never consumed, thereby reducing binary size and improving runtime execution speed.

The tool uses a **Worklist-based Iterative Algorithm**:
1. **Phase 1**: Map out the graph. We iterate over every LLVM Instruction and build a `UseDefChains` mapping of Definitions -> Users.
2. **Phase 2**: Seed a worklist with instructions that are "Safe to Delete" (no side effects) AND have zero users.
3. **Phase 3**: Iteratively pop instructions from the worklist. When an instruction is marked as dead, we traverse backward to its operands (its Definitions). We remove the dead instruction from the user list of its operands. If an operand suddenly drops to zero users, we cascade the deletion by pushing that operand onto the worklist.
4. **Phase 4**: Safely erase the dead instructions from the LLVM Basic Blocks.

## Alternatives Considered

### 1. Simple Linear Scan vs. Worklist
**Alternative**: A simple linear scan would just check `I->use_empty()` for every instruction. 
**Why Rejected**: A linear scan cannot handle cascading dead chains (e.g. `c = a + b; d = c * 2;` where `d` is dead). If we delete `d`, we must do another full pass over the entire function to realize `c` is now dead. The worklist algorithm handles cascading chains in $O(N)$ time efficiently.

### 2. Using LLVM's Built-in ADCE (Aggressive DCE)
**Alternative**: LLVM provides an Aggressive Dead Code Elimination pass out-of-the-box.
**Why Rejected**: Relying solely on LLVM's ADCE abstracts away the core Use-Def chain mechanics. By writing a custom out-of-tree pass, we gain fine-grained control over exactly what constitutes "dead" (such as our custom local Dead Store Elimination rules) and we can output detailed step-by-step terminal UI logging for educational visualization.

### 3. SSA form (mem2reg) vs Custom Dead Store Elimination
**Alternative**: Rely exclusively on `opt -passes=mem2reg` to lift memory `store`/`load` operations into SSA registers before running the pass.
**Why Chosen**: We opted to implement custom logic inside our `isSafeToDelete` function that analyzes `AllocaInst` memory. If an `alloca` memory space has zero `LoadInst` users, we consider any `StoreInst` writing to it as dead. This makes our tool much more robust when analyzing raw, unoptimized `-O0` Clang outputs without heavily relying on LLVM's preprocessing.
