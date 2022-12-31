#include <vector>
#include <atomic>
#include <concepts>
#include <asm/svm.h>
#include <iostream>
#include "div/codegen.hh"
#include "div/jit.hh"
#include "div/util.hh"

using namespace std;
void InitializeModuleAndPassManager(void) {
  // Open a new context and module.
  ctx = std::make_unique<LLVMContext>();
  mod = std::make_unique<Module>("my cool jit", *ctx);
  mod->setDataLayout(jit->getDataLayout());

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*ctx);

  // Create a new pass manager attached to it.
  fpm = std::make_unique<legacy::FunctionPassManager>(mod.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  fpm->add(createInstructionCombiningPass());
  // Reassociate expressions.
  fpm->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  fpm->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  fpm->add(createCFGSimplificationPass());

  fpm->doInitialization();
}
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      ExitOnErr(jit->addModule(
          ThreadSafeModule(std::move(mod), std::move(ctx))));
      InitializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
    getNextTok();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExt()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      fnprotos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextTok();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      // Create a ResourceTracker to track JIT'd memory allocated to our
      // anonymous expression -- that way we can free it after executing.
      auto RT = jit->getMainJITDylib().createResourceTracker();

      auto TSM = ThreadSafeModule(std::move(mod), std::move(ctx));
      ExitOnErr(jit->addModule(std::move(TSM), RT));
      InitializeModuleAndPassManager();

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = ExitOnErr(jit->lookup("__anon_expr"));

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)ExprSymbol.getAddress();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      ExitOnErr(RT->remove());
    }
  } else {
    // Skip token for error recovery.
    getNextTok();
  }
}

static void HandleTopLevelExpr() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      // Create a ResourceTracker to track JIT'd memory allocated to our
      // anonymous expression -- that way we can free it after executing.
      auto RT = jit->getMainJITDylib().createResourceTracker();

      auto TSM = orc::ThreadSafeModule(std::move(mod), std::move(ctx));
      ExitOnErr(jit->addModule(std::move(TSM), RT));
      InitializeModuleAndPassManager();

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = ExitOnErr(jit->lookup("__anon_expr"));

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)ExprSymbol.getAddress();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      ExitOnErr(RT->remove());
    }
  } else {
    // Skip token for error recovery.
    getNextTok();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "\e[32;1m[DIV] \x1b[34;1m>\x1b[0m ");
    switch (curtok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextTok();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_ext:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}


void init() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  BinopPrec['<'] = 10;
  BinopPrec['+'] = 20;
  BinopPrec['-'] = 20;
  BinopPrec['*'] = 40; // highest.
}

int main(int argc, char* argv[]) {
  init();
  cout << "\e[32;1mDIV COMPILER\e[0m" << "\n";
  fprintf(stderr, "\e[32;1m[DIV] \x1b[34;1m>\x1b[0m ");
  getNextTok();
  jit = ExitOnErr(orc::DivJIT::Create());
  InitializeModuleAndPassManager();
  // InitializeModule();
  MainLoop();
  // mod->print(errs(), nullptr);
  return 0;
}
