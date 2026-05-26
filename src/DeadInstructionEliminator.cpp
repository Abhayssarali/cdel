//===-- DeadInstructionEliminator.cpp - DIE Pass Implementation -----------===//
//
//  Dead Instruction Eliminator Using Use-Def Analysis
//  Compiler Design Course Project
//
//  ALGORITHM OVERVIEW
//  ==================
//  Phase 1 – Build Use-Def Chains
//    Walk every instruction I in the function.
//    For each operand V of I that is defined by another instruction D:
//      record D → {uses include I}.
//
//  Phase 2 – Worklist-based Dead Instruction Detection
//    Seed worklist: instructions with use_empty() AND isSafeToDelete().
//    While worklist not empty:
//      Pop I.  For each operand D (defining instruction):
//        Remove I from D's user set.
//        If D is now use-empty AND safe-to-delete → add D to worklist.
//      Mark I as dead.
//
//  Phase 3 – Elimination
//    Erase all dead instructions (in reverse post-order to respect dominance).
//    Report statistics.
//
//===----------------------------------------------------------------------===//

#include "DeadInstructionEliminator.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"

#include <queue>
#include <set>
#include <sstream>

using namespace llvm;

// ─── Statistics ──────────────────────────────────────────────────────────────
#define DEBUG_TYPE "dead-instruction-eliminator"
STATISTIC(NumDeadInst,   "Number of dead instructions eliminated");
STATISTIC(NumTotalInst,  "Number of instructions scanned");
STATISTIC(NumIterations, "Number of worklist iterations performed");

// ─────────────────────────────────────────────────────────────────────────────
// Helper: isSafeToDelete
// ─────────────────────────────────────────────────────────────────────────────

/// Instructions that have NO side effects and whose sole purpose is to produce
/// a value are safe to delete when that value is unused.
///
/// Instructions that are NEVER safe to delete (even when unused):
///   - Terminators  (ret, br, switch, unreachable, …)
///   - Store        (writes to memory → observable)
///   - Call / Invoke (may have arbitrary side effects)
///   - Fence, AtomicRMW, AtomicCmpXchg (memory model effects)
///   - LandingPad, CatchPad, CleanupPad (exception handling)
bool llvm::isSafeToDelete(const Instruction *I) {
  // Never remove terminators
  if (I->isTerminator())
    return false;

  // Stores always have side effects
  if (isa<StoreInst>(I))
    return false;

  // Calls may have side effects (conservative: skip all calls)
  if (isa<CallInst>(I) || isa<InvokeInst>(I))
    return false;

  // Atomic operations have memory-model observable effects
  if (isa<FenceInst>(I) || isa<AtomicRMWInst>(I) ||
      isa<AtomicCmpXchgInst>(I))
    return false;

  // Exception-handling pads
  if (isa<LandingPadInst>(I) || isa<CatchPadInst>(I) ||
      isa<CleanupPadInst>(I))
    return false;

  // Volatile loads have observable side effects
  if (const auto *LI = dyn_cast<LoadInst>(I))
    if (LI->isVolatile())
      return false;

  // Everything else (arithmetic, comparisons, casts, non-volatile loads,
  // alloca, GEP, phi, select, extractelement, …) is safe to delete when unused.
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: getInstCategory  (for pretty-printing the analysis report)
// ─────────────────────────────────────────────────────────────────────────────

std::string llvm::getInstCategory(const Instruction *I) {
  if (isa<BinaryOperator>(I))   return "BinaryOp";
  if (isa<UnaryOperator>(I))    return "UnaryOp";
  if (isa<ICmpInst>(I))         return "ICmp";
  if (isa<FCmpInst>(I))         return "FCmp";
  if (isa<AllocaInst>(I))       return "Alloca";
  if (isa<LoadInst>(I))         return "Load";
  if (isa<StoreInst>(I))        return "Store";
  if (isa<GetElementPtrInst>(I))return "GEP";
  if (isa<PHINode>(I))          return "PHI";
  if (isa<SelectInst>(I))       return "Select";
  if (isa<CastInst>(I))         return "Cast";
  if (isa<CallInst>(I))         return "Call";
  if (isa<ReturnInst>(I))       return "Return";
  if (isa<BranchInst>(I))       return "Branch";
  return "Other";
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 – Build Use-Def Chains
// ─────────────────────────────────────────────────────────────────────────────

UseDefChains
DeadInstructionEliminatorPass::buildUseDefChains(Function &F) {
  UseDefChains Chains;

  // 1a. Create an entry for every instruction
  for (auto &BB : F)
    for (auto &I : BB)
      Chains.try_emplace(&I, UseDefInfo(&I));

  // 1b. Populate the Users set: for each instruction I and each of its
  //     LLVM uses (i.e., instructions that read I's result), record that use.
  for (auto &BB : F) {
    for (auto &I : BB) {
      auto It = Chains.find(&I);
      if (It == Chains.end()) continue;

      for (User *U : I.users()) {
        if (auto *UserInst = dyn_cast<Instruction>(U)) {
          // UserInst USES the value defined by I  →  I defines for UserInst
          It->second.Users.insert(UserInst);
        }
      }
    }
  }

  return Chains;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 – Find Dead Instructions (worklist algorithm)
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Instruction *>
DeadInstructionEliminatorPass::findDeadInstructions(UseDefChains &Chains) {
  // Use a std::queue for BFS-style iteration (order does not affect
  // correctness, only the sequence in which instructions are marked).
  std::queue<Instruction *> Worklist;
  std::set<Instruction *> InWorklist; // track membership to avoid duplicates

  // ── Seed: instructions that are already obviously dead ───────────────────
  for (auto &[Inst, Info] : Chains) {
    if (isSafeToDelete(Inst) && Inst->use_empty()) {
      Worklist.push(Inst);
      InWorklist.insert(Inst);
    }
  }

  std::vector<Instruction *> Dead;

  // ── Worklist iteration ───────────────────────────────────────────────────
  while (!Worklist.empty()) {
    ++NumIterations;
    Instruction *I = Worklist.front();
    Worklist.pop();
    InWorklist.erase(I);

    auto It = Chains.find(I);
    if (It == Chains.end()) continue;
    if (It->second.IsDead)  continue; // already processed

    // Mark dead
    It->second.IsDead = true;
    Dead.push_back(I);

    // ── Propagate: for each operand that is defined by another instruction,
    //    remove I from that instruction's user set.  If the defining
    //    instruction becomes use-empty and is safe to delete → enqueue it.
    for (Use &Op : I->operands()) {
      auto *DefInst = dyn_cast<Instruction>(Op.get());
      if (!DefInst) continue; // constant / argument, skip

      auto DefIt = Chains.find(DefInst);
      if (DefIt == Chains.end()) continue;

      // Remove I from DefInst's user set
      DefIt->second.Users.erase(I);

      // If DefInst has no remaining users (within our chain map) and is safe,
      // also check LLVM's own use list for any uses we may have missed.
      // We use DefInst->use_empty() as the ground truth because LLVM keeps
      // this updated after we call eraseFromParent() later.
      // Here, during analysis (before any erasure), we rely on our custom
      // Users set being empty to detect cascading dead instructions.
      if (!DefIt->second.IsDead &&
          isSafeToDelete(DefInst) &&
          DefIt->second.Users.empty() &&
          !InWorklist.count(DefInst)) {
        Worklist.push(DefInst);
        InWorklist.insert(DefInst);
      }
    }
  }

  return Dead;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3 – Print Use-Def Chain Report
// ─────────────────────────────────────────────────────────────────────────────

void DeadInstructionEliminatorPass::printUseDefChains(
    const UseDefChains &Chains, const Function &F) {

  errs() << "\n";
  errs() << "╔══════════════════════════════════════════════════════════════╗\n";
  errs() << "║         USE-DEF CHAIN ANALYSIS REPORT                       ║\n";
  errs() << "║         Function: " << F.getName();
  // Padding
  int pad = 43 - (int)F.getName().size();
  for (int i = 0; i < pad; i++) errs() << " ";
  errs() << "║\n";
  errs() << "╚══════════════════════════════════════════════════════════════╝\n";
  errs() << "\n";

  unsigned instNum = 0;
  for (const auto &BB : F) {
    errs() << "  ┌─ Basic Block: " << BB.getName() << "\n";
    for (const auto &I : BB) {
      ++instNum;
      auto It = Chains.find(const_cast<Instruction*>(&I));
      bool isDead = (It != Chains.end()) && It->second.IsDead;

      errs() << "  │  [" << instNum << "] ";
      errs() << (isDead ? "✗ DEAD  " : "✓ LIVE  ");
      errs() << getInstCategory(&I) << "  ";
      I.print(errs());
      errs() << "\n";

      if (It != Chains.end() && !It->second.Users.empty()) {
        errs() << "  │       ↑ Used by:\n";
        for (const Instruction *U : It->second.Users) {
          errs() << "  │           → ";
          U->print(errs());
          errs() << "\n";
        }
      }
    }
    errs() << "  └──────────────────────────────────────────────────────\n";
  }
  errs() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// New Pass Manager – run()
// ─────────────────────────────────────────────────────────────────────────────

PreservedAnalyses
DeadInstructionEliminatorPass::run(Function &F,
                                   FunctionAnalysisManager &FAM) {
  errs() << "\n[DIE Pass] Running on function: " << F.getName() << "\n";

  // ── Phase 1: Build Use-Def Chains ──────────────────────────────────────
  errs() << "[DIE Pass] Phase 1: Building Use-Def chains...\n";
  UseDefChains Chains = buildUseDefChains(F);

  // Count total instructions scanned
  unsigned Total = 0;
  for (auto &BB : F) Total += BB.size();
  NumTotalInst += Total;
  errs() << "[DIE Pass]   → " << Total << " instructions scanned.\n";

  // ── Phase 2: Find Dead Instructions ────────────────────────────────────
  errs() << "[DIE Pass] Phase 2: Running worklist dead-instruction analysis...\n";
  std::vector<Instruction *> Dead = findDeadInstructions(Chains);
  errs() << "[DIE Pass]   → " << Dead.size() << " dead instruction(s) found.\n";

  // ── Print Use-Def Chain Report (before deletion) ───────────────────────
  printUseDefChains(Chains, F);

  // ── Phase 3: Eliminate Dead Instructions ───────────────────────────────
  errs() << "[DIE Pass] Phase 3: Eliminating dead instructions...\n";

  // Print a summary table
  errs() << "\n  ┌────────────────────────────────────────────────────────┐\n";
  errs() <<   "  │  DEAD INSTRUCTIONS TO BE REMOVED                       │\n";
  errs() <<   "  ├────────────────────────────────────────────────────────┤\n";

  for (Instruction *I : Dead) {
    errs() << "  │  [-] ";
    I->print(errs());
    errs() << "\n";
  }
  errs() <<   "  └────────────────────────────────────────────────────────┘\n\n";

  // Erase dead instructions.
  // Drop all references first so that use lists are cleared safely, then erase.
  for (Instruction *I : Dead) {
    I->dropAllReferences();
  }
  for (Instruction *I : Dead) {
    I->eraseFromParent();
    ++NumDeadInst;
  }

  if (Dead.empty()) {
    errs() << "[DIE Pass] No dead instructions found — IR unchanged.\n";
    return PreservedAnalyses::all();
  }

  errs() << "[DIE Pass] Done.  Eliminated " << Dead.size()
         << " dead instruction(s) from function '" << F.getName() << "'.\n\n";

  // We modified the IR; invalidate analyses that depend on instruction lists
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>(); // CFG structure unchanged
  return PA;
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy Pass Manager
// ─────────────────────────────────────────────────────────────────────────────

char DeadInstructionEliminatorLegacyPass::ID = 0;

bool DeadInstructionEliminatorLegacyPass::runOnFunction(Function &F) {
  DeadInstructionEliminatorPass NewPMPass;
  FunctionAnalysisManager FAM;
  auto PA = NewPMPass.run(F, FAM);
  return !PA.areAllPreserved(); // return true if IR was modified
}

void DeadInstructionEliminatorLegacyPass::getAnalysisUsage(
    AnalysisUsage &AU) const {
  // We don't require any analyses and we preserve the CFG
  AU.setPreservesCFG();
}

void DeadInstructionEliminatorLegacyPass::print(raw_ostream &OS,
                                                 const Module *) const {
  OS << "DeadInstructionEliminatorLegacyPass\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Plugin Registration (New Pass Manager)
// ─────────────────────────────────────────────────────────────────────────────

/// Called by LLVM's plugin infrastructure when the .so is loaded via
/// -load-pass-plugin.  We register our pass under the name
/// "dead-instruction-eliminator" so that:
///   opt -passes="dead-instruction-eliminator" ...
/// works out of the box.
extern "C" LLVM_ATTRIBUTE_WEAK
::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION,
    "DeadInstructionEliminator",
    LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      // Register as a Function pass
      PB.registerPipelineParsingCallback(
          [](StringRef Name, FunctionPassManager &FPM,
             ArrayRef<PassBuilder::PipelineElement>) {
            if (Name == "dead-instruction-eliminator") {
              FPM.addPass(DeadInstructionEliminatorPass());
              return true;
            }
            return false;
          });
    }
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy Pass Registration (for opt -load ./pass.so -die-pass)
// ─────────────────────────────────────────────────────────────────────────────

static RegisterPass<DeadInstructionEliminatorLegacyPass>
    X("die-pass",                          // command-line name
      "Dead Instruction Eliminator (DIE)", // description
      false,                               // CFG-only pass?  no
      false);                              // analysis pass?   no
