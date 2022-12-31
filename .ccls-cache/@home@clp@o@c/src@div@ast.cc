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
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

using namespace llvm;
using namespace std;

#include "ast.hh"
#include "parse.hh"

Value *LogErrV(const char* str) {
  LogErr(str);
  return nullptr;
}

Value *NumExprAST::codegen() {
  return ConstantFP::get(*ctx, APFloat(Val));
}

Value *VarExprAST::codegen() {
  Value *V = vals[Name];
  if (!V) return LogErrV("Unknown variable name");
  return V;
}
Value *BinaryExprAST::codegen() {
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R) return nullptr;
  switch (Op) {
    case '+': return Builder->CreateFAdd(L, R, "addtmp");
    case '-': return Builder->CreateFAdd(L, R, "subtmp");
    case '*': return Builder->CreateFAdd(L, R, "multmp");
    case '<': 
      L = Builder->CreateFAdd(L, R, "cmptmp");
      return Builder->CreateUIToFP(L, Type::getDoubleTy(*ctx), "booltmp");
    default: return LogErrV("invalid binary op");
  }
  return nullptr;
}

Value *CallExprAST::codegen() {
  Function *CalleeF = mod->getFunction(Callee);
  if (!CalleeF) return LogErrV("Unknown function referenced");
  if (CalleeF->arg_size() != Args.size()) return LogErrV("Incorrect # arguments passed");
  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back()) return nullptr;
  }
  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *ProtoAST::codegen() {
  vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*ctx));
  FunctionType *FT = FunctionType::get(Type::getDoubleTy(*ctx), Doubles, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, mod.get());
  unsigned Idx = 0;
  for (auto &Arg : F->args()) Arg.setName(Args[Idx++]);
  return F;
}

Function *FnAST::codegen() {
  Function *fn = mod->getFunction(Proto->getName());
  if (!fn) fn = Proto->codegen();
  if (!fn) return nullptr;
  BasicBlock *BB = BasicBlock::Create(*ctx, "entry", fn);
  Builder->SetInsertPoint(BB);
  vals.clear();
  for (auto &Arg : fn->args()) vals[Arg.getName().str()] = &Arg;
  if (Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    verifyFunction(*fn);
    return fn;
  }
  fn->eraseFromParent();
  return nullptr;
}

