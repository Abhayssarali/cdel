//===-- DeadInstructionEliminator.h - DIE Pass Declaration ----------------===//
//
//  Dead Instruction Eliminator Using Use-Def Analysis
//  Compiler Design Course Project
//
//  This header declares the LLVM Function Pass that performs Use-Def chain
//  analysis to identify and eliminate dead instructions — instructions whose
//  results are never used and which produce no observable side effects.
//
//===----------------------------------------------------------------------===//

#ifndef DEAD_INSTRUCTION_ELIMINATOR_H
#define DEAD_INSTRUCTION_ELIMINATOR_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include <vector>

namespace llvm {

//===----------------------------------------------------------------------===//
// UseDefInfo – Lightweight Use-Def chain information for a single instruction
//===----------------------------------------------------------------------===//

/// Stores Use-Def analysis data for one instruction:
///   - Pointer to the instruction itself
///   - Set of instructions that USE this instruction's result (its users)
///   - Flag indicating whether this instruction is dead
struct UseDefInfo {
  Instruction *Inst;
  SmallPtrSet<Instruction *, 4> Users; // instructions that use Inst's result
  bool IsDead = false;

  explicit UseDefInfo(Instruction *I) : Inst(I) {}
};

//===----------------------------------------------------------------------===//
// UseDefChains – Use-Def chain map for an entire function
//===----------------------------------------------------------------------===//

/// Maps each Instruction* to its UseDefInfo.
/// Built by DeadInstructionEliminatorPass before elimination.
using UseDefChains = DenseMap<Instruction *, UseDefInfo>;

//===----------------------------------------------------------------------===//
// Helper – Safe-to-delete predicate
//===----------------------------------------------------------------------===//

/// Returns true if the instruction can be safely deleted when it has no uses.
/// Instructions with observable side effects (stores, calls, terminators, etc.)
/// are NOT safe to delete even if their result is unused.
bool isSafeToDelete(const Instruction *I);

/// Returns a human-readable string categorising the instruction for reporting.
std::string getInstCategory(const Instruction *I);

//===----------------------------------------------------------------------===//
// DeadInstructionEliminatorPass – New Pass Manager interface
//===----------------------------------------------------------------------===//

/// New-PM pass.  Usage with opt:
///   opt -load-pass-plugin ./DeadInstructionEliminator.so \
///       -passes="dead-instruction-eliminator" -S input.ll
struct DeadInstructionEliminatorPass
    : public PassInfoMixin<DeadInstructionEliminatorPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);

  /// Build Use-Def chains for every instruction in F.
  static UseDefChains buildUseDefChains(Function &F);

  /// Iteratively find all dead instructions using the Use-Def chains.
  static std::vector<Instruction *>
  findDeadInstructions(UseDefChains &Chains);

  /// Print the Use-Def chain table to stderr (for debugging / reporting).
  static void printUseDefChains(const UseDefChains &Chains,
                                const Function &F);

  static bool isRequired() { return false; }
};

//===----------------------------------------------------------------------===//
// DeadInstructionEliminatorLegacyPass – Legacy Pass Manager interface
//===----------------------------------------------------------------------===//

/// Legacy-PM pass.  Usage:
///   opt -load ./DeadInstructionEliminator.so -die-pass -S input.ll
class DeadInstructionEliminatorLegacyPass : public FunctionPass {
public:
  static char ID;
  DeadInstructionEliminatorLegacyPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;
  void print(raw_ostream &OS, const Module *M) const override;
};

} // namespace llvm

#endif // DEAD_INSTRUCTION_ELIMINATOR_H
