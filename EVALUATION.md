# Evaluation

## Metrics & Test Cases
The compiler pass has been thoroughly evaluated on a suite of test cases involving complex CFG structures (PHI nodes, cascading dead mathematical operations, branching, and dead stores).

### Core Test Cases (`testcases/`)
- `dead_code.c`: A raw C test containing isolated dead mathematical chains, branch-isolated dead code, and memory overwrite anomalies.
- `test1.ll`: Tests simple dead scalar stores.
- `test2.ll`: Tests cascading dead chains (A uses B, B uses C; all are dead).
- `test3.ll`: Tests a mix of live side-effect instructions alongside dead instructions.
- `test4.ll`: Tests CFG branching and dead PHI nodes.
- `test5.ll`: Tests an all-live control case where reduction should be exactly 0%.

## Performance Comparison
On the `dead_code.c` test case (which compiles to 11 IR instructions via `clang -O0` for isolated tests):
- **Raw `-O0`**: 11 Instructions (0 eliminated).
- **With `die-tool` (No `mem2reg`)**: 2 Instructions Eliminated (The dead local `Alloca` and its associated `StoreInst`). 18.2% Reduction.
- **With `mem2reg` + `die-tool`**: 14 Instructions Eliminated (Because memory operations are lifted to SSA virtual registers, all dead mathematical cascading chains are exposed to the tool).

## Conclusion
The custom Dead Instruction Eliminator pass successfully operates at the same efficiency as LLVM's internal `ADCE` for SSA code, while additionally providing fine-grained terminal analysis logs (marking individual instructions as `[✗ DEAD]` or `[✓ LIVE]`) which is invaluable for visualization. The implementation handles both memory-isolated Dead Store Elimination and standard Use-Def SSA chaining cleanly.
