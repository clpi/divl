#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

#include "ast.hh"

using namespace llvm;


unique_ptr<ExprAST> LogErr(const char *str) {
  fprintf(stderr, "LogErr: %s\n", str);
  return nullptr;
}

unique_ptr<ProtoAST> LogErrP(const char *str) {
  LogErr(str);
  return nullptr;
}

Value *LogErrV(const char* str) {
  LogErr(str);
  return nullptr;
}


