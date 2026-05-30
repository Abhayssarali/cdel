# Implementation Details

## LLVM Infrastructure
The pass is implemented in C++ using the modern LLVM 17 Pass Manager infrastructure (`llvm::PassInfoMixin`). It is compiled as a shared library plugin (`.so`) that can be loaded dynamically by `opt-17`.

## Core APIs Used
Our implementation heavily relies on LLVM's inherent SSA graph structures:
- `Instruction::users()`: Returns an iterable list of all instructions that consume the output value of the current instruction. Used in Phase 1 to construct the Use-Def chain map.
- `Instruction::operands()`: Returns the values (which can be cast to defining Instructions) that the current instruction consumes. Used in the Worklist phase to propagate deadness upward.
- `Instruction::use_empty()`: Returns true if the LLVM IR indicates zero downstream readers.

## Safety & Side Effects
A critical part of the implementation is the `isSafeToDelete(Instruction*)` logic.
We explicitly prevent the deletion of:
- `TerminatorInst` (e.g., `ret`, `br`) as they control Control Flow Graph (CFG) routing.
- `CallInst` / `InvokeInst` as function calls may modify global state, perform I/O, or throw exceptions.
- `StoreInst` EXCEPT for local unread allocas.

## Custom Dead Store Elimination
Clang compiles `-O0` C code by generating an `AllocaInst` (memory allocation) and a `StoreInst` for every variable. Standard LLVM passes consider memory stores to have side effects, thus blocking DCE. 

We implemented custom logic:
```cpp
if (const auto *Alloca = dyn_cast<AllocaInst>(SI->getPointerOperand())) {
  bool hasLoad = false;
  for (const User *U : Alloca->users()) {
    if (isa<LoadInst>(U)) hasLoad = true;
  }
  if (!hasLoad) return true; // Dead Store!
}
```
If a `StoreInst` writes to an `Alloca` that has NO `LoadInst` anywhere in the function's use list, our pass correctly flags the `StoreInst` as safe to delete. Removing the store cascades backwards to eliminate the mathematical operations that generated the stored value.
