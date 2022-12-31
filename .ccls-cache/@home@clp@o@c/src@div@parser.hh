#include "parse.hh"

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
    }
  } else getNextTok();
}

static void HandleExt() {
  if (auto ProtoAST = ParseExt()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read ext: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
    }
  } else getNextTok();
}

static void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read top-level expression:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      FnIR->eraseFromParent();
    }
  } else getNextTok();
}

