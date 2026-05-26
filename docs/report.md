# Dead Instruction Eliminator Using Use-Def Analysis

## Algorithm Report — Compiler Design Course Project

---

## 1. Introduction

**Dead Code Elimination (DCE)** is a fundamental compiler optimization that removes instructions whose computed results are never used by any subsequent instruction. Such instructions are called *dead instructions*. Eliminating them:

- Reduces code size
- Reduces register pressure
- Speeds up execution
- Simplifies the IR for subsequent optimizations

This project implements a custom LLVM pass that performs DCE using **Use-Def chain analysis** — a data-flow technique that tracks, for every variable definition, all the points where that definition is used.

---

## 2. Background: Use-Def Chains

### 2.1 Definitions

| Term | Meaning |
|------|---------|
| **Definition** | An instruction that produces a value (e.g., `%x = add i32 %a, %b`) |
| **Use** | A reference to a previously defined value as an operand |
| **Use-Def Chain** | For a given *use*, the chain pointing back to its *definition* |
| **Def-Use Chain** | For a given *definition*, the set of all its *uses* |

In our analysis we primarily use the **Def-Use direction**: for each instruction `I`, we ask *"who uses the value I produce?"*

### 2.2 In LLVM

LLVM IR is in **Static Single Assignment (SSA)** form, which means:
- Every value is defined exactly once
- Use-Def chains are **trivially available** — each operand directly points to its defining instruction

LLVM maintains `use_begin()`/`use_end()` iterators and the `use_empty()` predicate on every `Value`, giving us O(1) access to the Def-Use chain.

---

## 3. Algorithm

### 3.1 Overview

```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   Phase 1: Build Use-Def Chain Map                      │
│   ─────────────────────────────────                     │
│   For each instruction I in function F:                 │
│     Create UseDefInfo(I)                                │
│     For each user U of I:                               │
│       Record I.Users ← { U }                            │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   Phase 2: Worklist Dead-Instruction Detection          │
│   ─────────────────────────────────────                  │
│   Seed worklist with:                                   │
│     { I | I.Users = ∅ ∧ isSafeToDelete(I) }            │
│                                                         │
│   while worklist ≠ ∅:                                   │
│     I ← worklist.dequeue()                              │
│     mark I as DEAD                                      │
│     for each operand DefInst of I:                      │
│       remove I from DefInst.Users                       │
│       if DefInst.Users = ∅ ∧ isSafeToDelete(DefInst):  │
│         worklist.enqueue(DefInst)                       │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   Phase 3: Elimination and Reporting                    │
│   ─────────────────────────────────                     │
│   for each DEAD instruction I:                          │
│     I.dropAllReferences()                               │
│     I.eraseFromParent()                                 │
│   Report statistics                                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 3.2 Phase 1: Building Use-Def Chains

We iterate through every instruction in the function and build a map:

```
Instruction*  →  UseDefInfo {
                    Inst:  the instruction itself
                    Users: set of instructions that use this value
                    IsDead: false (initially)
                  }
```

**Time complexity**: O(n × m) where n = number of instructions, m = average number of users per instruction. In practice m is very small (typically 1–3), so this is effectively O(n).

### 3.3 Phase 2: Worklist Algorithm

The key insight is that dead code elimination is **iterative**: removing one dead instruction may make other instructions dead (if they were the sole contributors to the now-deleted instruction).

We use a **BFS worklist** seeded with initially-dead instructions:

1. **Seed**: Instructions that have no users AND are safe to delete
2. **Process**: Dequeue an instruction, mark it dead, remove it from the user lists of its operands
3. **Cascade**: If an operand's defining instruction now has zero users and is safe to delete, enqueue it

This handles chains of arbitrary depth in a single pass through the worklist.

**Time complexity**: Each instruction is enqueued at most once → O(n).

### 3.4 Phase 3: Elimination

Dead instructions are erased from the IR:

1. `dropAllReferences()` — Clears all operand pointers (prevents dangling uses)
2. `eraseFromParent()` — Removes the instruction from its basic block

**Ordering**: We first drop all references, then erase, to avoid use-after-free issues when one dead instruction references another.

### 3.5 Side-Effect Classification

Not all unused instructions are dead. Instructions with **observable side effects** must be preserved:

| Instruction Type | Safe to Delete? | Reason |
|-----------------|----------------|--------|
| `add`, `sub`, `mul`, ... | ✅ Yes | Pure arithmetic, no side effects |
| `icmp`, `fcmp` | ✅ Yes | Pure comparison |
| `alloca` | ✅ Yes | Only allocates stack space |
| `load` (non-volatile) | ✅ Yes | Only reads memory |
| `getelementptr` | ✅ Yes | Address computation only |
| `bitcast`, `zext`, `sext` | ✅ Yes | Type conversion only |
| `phi`, `select` | ✅ Yes | Value selection only |
| `store` | ❌ No | Writes to memory (observable) |
| `call`, `invoke` | ❌ No | May have arbitrary side effects |
| `ret`, `br`, `switch` | ❌ No | Control flow terminators |
| `fence` | ❌ No | Memory ordering barrier |
| Volatile `load` | ❌ No | Hardware-observable read |

---

## 4. Correctness Argument

**Theorem**: The algorithm removes an instruction I if and only if:
1. I produces no observable side effects, AND
2. The value produced by I does not transitively reach any side-effecting instruction or function return

**Proof sketch**:
- *Soundness*: We only mark an instruction dead if `isSafeToDelete()` returns true (no side effects) AND it has zero users. The worklist propagation ensures we only cascade to instructions whose user count drops to zero *after* their sole user is marked dead.
- *Completeness*: The worklist processes every instruction that becomes dead during the analysis. Since each instruction is enqueued at most once, and the cascade follows Def-Use chains exhaustively, all transitively dead instructions are found.

---

## 5. Implementation Details

### 5.1 LLVM Pass Infrastructure

The pass is implemented for both pass manager interfaces:

| Interface | Class | Registration |
|-----------|-------|-------------|
| **New Pass Manager** | `DeadInstructionEliminatorPass` | `-passes="dead-instruction-eliminator"` |
| **Legacy Pass Manager** | `DeadInstructionEliminatorLegacyPass` | `-die-pass` |

### 5.2 Key Data Structures

| Data Structure | Purpose |
|---------------|---------|
| `UseDefInfo` | Per-instruction metadata: user set, dead flag |
| `UseDefChains` (DenseMap) | Maps Instruction* → UseDefInfo |
| `std::queue<Instruction*>` | BFS worklist for dead instruction discovery |
| `SmallPtrSet<Instruction*, 4>` | Efficient user set (stack-allocated for ≤4 users) |

### 5.3 LLVM Statistics

The pass reports via LLVM's `STATISTIC` macro:
- `NumDeadInst` — Number of dead instructions eliminated
- `NumTotalInst` — Number of instructions scanned
- `NumIterations` — Number of worklist iterations

---

## 6. Test Cases

| Test | Description | Expected Dead | Expected Live |
|------|-------------|---------------|--------------|
| `test1.ll` | Simple unused arithmetic | `%dead1`, `%dead2`, `%dead3` | `%live1`, `%live2`, `ret` |
| `test2.ll` | Cascading dead chain (5 deep) | `%a` through `%e` | `%result`, `ret` |
| `test3.ll` | Mixed: dead arith + live stores/calls | `%dead_arith1/2`, `%dead_cmp` | `store`, `call`, `load`, `ret` |
| `test4.ll` | Dead PHI nodes across CFG | `%dead_phi`, `%dead_left/right` | `%live_phi`, branches |
| `test5.ll` | All-live (negative test) | None | All instructions preserved |

---

## 7. Comparison with LLVM's Built-in DCE

| Feature | Our Pass | LLVM `-dce` | LLVM `-adce` |
|---------|----------|-------------|-------------|
| Use-Def analysis | ✅ Explicit chain building | ✅ Implicit (SSA) | ✅ Implicit (SSA) |
| Cascading elimination | ✅ Worklist | ✅ Worklist | ✅ Reverse post-order |
| Removes dead control flow | ❌ | ❌ | ✅ (Aggressive) |
| Removes dead stores | ❌ | ❌ | ❌ (needs DSE) |
| Educational / transparent | ✅ Detailed reports | ❌ | ❌ |

Our pass is functionally equivalent to LLVM's `-dce` pass but provides detailed diagnostic output for educational purposes.

---

## 8. References

1. Aho, Lam, Sethi, Ullman. *Compilers: Principles, Techniques, and Tools* (2nd ed.), Chapter 9: Machine-Independent Optimizations
2. Cooper, Torczon. *Engineering a Compiler* (2nd ed.), Chapter 10: Scalar Optimizations
3. LLVM Language Reference Manual — https://llvm.org/docs/LangRef.html
4. LLVM Writing an LLVM Pass — https://llvm.org/docs/WritingAnLLVMNewPMPass.html
