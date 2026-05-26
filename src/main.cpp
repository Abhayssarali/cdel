//===-- main.cpp - Standalone Dead Instruction Eliminator Tool ------------===//
//
//  A command-line utility that:
//    1. Reads an LLVM IR file (.ll or .bc)
//    2. Runs the Dead Instruction Eliminator pass on every function
//    3. Writes the optimized IR to stdout or a file
//
//  Usage:
//    die-tool input.ll                     # prints optimized IR to stdout
//    die-tool input.ll -o output.ll        # writes to output.ll
//
//===----------------------------------------------------------------------===//

#include "DeadInstructionEliminator.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// ─── Command-Line Options ────────────────────────────────────────────────────

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input .ll/.bc file>"),
                  cl::Required);

static cl::opt<std::string>
    OutputFilename("o", cl::desc("Output filename (default: stdout)"),
                   cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
    Verbose("v", cl::desc("Enable verbose Use-Def chain dump"),
            cl::init(false));

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv,
                               "Dead Instruction Eliminator Tool\n\n"
                               "  Eliminates dead instructions from LLVM IR\n"
                               "  using Use-Def chain analysis.\n");

  // ── Parse input IR ─────────────────────────────────────────────────────
  LLVMContext Ctx;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Ctx);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  errs() << "╔══════════════════════════════════════════════════════════════╗\n";
  errs() << "║       Dead Instruction Eliminator (DIE) Tool                ║\n";
  errs() << "╚══════════════════════════════════════════════════════════════╝\n";
  errs() << "  Input : " << InputFilename << "\n";
  errs() << "  Output: "
         << (OutputFilename.getValue() == "-" ? "<stdout>" : OutputFilename.getValue()) << "\n\n";

  // ── Run pass on each function ──────────────────────────────────────────
  unsigned totalDead = 0;
  unsigned totalInst = 0;
  unsigned totalFunctions = 0;

  for (Function &F : *M) {
    if (F.isDeclaration())
      continue; // skip declarations (e.g., printf)

    ++totalFunctions;

    // Count instructions before
    unsigned before = 0;
    for (auto &BB : F) before += BB.size();
    totalInst += before;

    // Phase 1: Build Use-Def Chains
    auto Chains = DeadInstructionEliminatorPass::buildUseDefChains(F);

    // Phase 2: Find Dead Instructions
    auto Dead = DeadInstructionEliminatorPass::findDeadInstructions(Chains);

    // Print Use-Def chains if verbose
    if (Verbose)
      DeadInstructionEliminatorPass::printUseDefChains(Chains, F);

    // Phase 3: Eliminate
    if (!Dead.empty()) {
      errs() << "  Function '" << F.getName() << "': "
             << Dead.size() << " dead instruction(s) eliminated.\n";

      for (Instruction *I : Dead) {
        if (Verbose) {
          errs() << "    [-] ";
          I->print(errs());
          errs() << "\n";
        }
      }

      for (Instruction *I : Dead)
        I->dropAllReferences();
      for (Instruction *I : Dead)
        I->eraseFromParent();

      totalDead += Dead.size();
    } else {
      errs() << "  Function '" << F.getName() << "': no dead instructions.\n";
    }
  }

  // ── Summary ────────────────────────────────────────────────────────────
  errs() << "\n";
  errs() << "┌─────────────────────────────────────────────────────────────┐\n";
  errs() << "│                      SUMMARY                               │\n";
  errs() << "├─────────────────────────────────────────────────────────────┤\n";
  errs() << "│  Functions processed : " << totalFunctions << "\n";
  errs() << "│  Instructions scanned: " << totalInst << "\n";
  errs() << "│  Dead eliminated     : " << totalDead << "\n";
  if (totalInst > 0) {
    double pct = 100.0 * totalDead / totalInst;
    errs() << "│  Reduction           : "
           << llvm::format("%.1f", pct) << "%\n";
  }
  errs() << "└─────────────────────────────────────────────────────────────┘\n";

  // ── Verify module ──────────────────────────────────────────────────────
  if (verifyModule(*M, &errs())) {
    errs() << "[ERROR] Module verification failed after optimization!\n";
    return 1;
  }

  // ── Write output ──────────────────────────────────────────────────────
  std::error_code EC;
  ToolOutputFile Out(OutputFilename, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "[ERROR] Cannot open output file '" << OutputFilename
           << "': " << EC.message() << "\n";
    return 1;
  }

  M->print(Out.os(), nullptr);
  Out.keep();

  return 0;
}
