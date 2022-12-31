#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "parse.hh"
#include "jit.hh"

using namespace llvm;
using namespace std;
using namespace llvm::orc;

static unique_ptr<LLVMContext> ctx;
static unique_ptr<Module> mod;
static unique_ptr<IRBuilder<>> Builder;
static std::map<string, Value *> vals;
static std::unique_ptr<llvm::orc::DivJIT> jit;
static std::unique_ptr<legacy::FunctionPassManager> fpm;
static std::map<string, unique_ptr<ProtoAST>> fnprotos;
static ExitOnError ExitOnErr;

static void InitializeModule() {
  ctx = std::make_unique<LLVMContext>();
  mod = std::make_unique<Module>("my cool jit", *ctx);
  Builder = std::make_unique<IRBuilder<>>(*ctx);
}
Function *getFunction(std::string Name) {
  if (auto *F = mod->getFunction(Name)) return F;
  auto FI = fnprotos.find(Name);
  if (FI != fnprotos.end()) return FI->second->codegen();
  return nullptr;
}
Value *LogErrV(const char *str) {
  LogErr(str);
  return nullptr;
}

Value *NumExprAST::codegen() {
  return ConstantFP::get(*ctx, APFloat(Val));
}
Value *VarExprAST::codegen() {
  // Look this variable up in the function.
  Value *V = vals[Name];
  if (!V)
    return LogErrV("Unknown variable name");
  return V;
}
Value *BinaryExprAST::codegen() {
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  switch (Op) {
  case '+':
    return Builder->CreateFAdd(L, R, "addtmp");
  case '-':
    return Builder->CreateFSub(L, R, "subtmp");
  case '*':
    return Builder->CreateFMul(L, R, "multmp");
  case '<':
    L = Builder->CreateFCmpULT(L, R, "cmptmp");
    // Convert bool 0/1 to double 0.0 or 1.0
    return Builder->CreateUIToFP(L, Type::getDoubleTy(*ctx), "booltmp");
  default:
    return LogErrV("invalid binary operator");
  }
}
Value *CallExprAST::codegen() {
  Function *CalleeF = getFunction(Callee);
  if (!CalleeF) return LogErrV("Unknown function referenced");
  if (CalleeF->arg_size() != Args.size())
    return LogErrV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}
Function *ProtoAST::codegen() {
  // Make the function type:  double(double,double) etc.
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*ctx));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*ctx), Doubles, false);

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, mod.get());

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

Function *FnAST::codegen() {
  // Transfer ownership of the prototype to the fnprotos map, but keep a
  // reference to it for use below.
  auto &P = *Proto;
  fnprotos[Proto->getName()] = std::move(Proto);
  Function *fn = getFunction(P.getName());
  if (!fn)
    return nullptr;

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(*ctx, "entry", fn);
  Builder->SetInsertPoint(BB);

  // Record the function arguments in the vals map.
  vals.clear();
  for (auto &Arg : fn->args())
    vals[std::string(Arg.getName())] = &Arg;

  if (Value *RetVal = Body->codegen()) {
    // Finish off the function.
    Builder->CreateRet(RetVal);

    // Validate the generated code, checking for consistency.
    verifyFunction(*fn);

    // Run the optimizer on the function.
    fpm->run(*fn);

    return fn;
  }

  // Error reading body, remove function.
  fn->eraseFromParent();
  return nullptr;
}
Value *IfExprAST::codegen() {
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  // Convert condition to a bool by comparing non-equal to 0.0.
  CondV = Builder->CreateFCmpONE(
      CondV, ConstantFP::get(*ctx, APFloat(0.0)), "ifcond");

  Function *fn = Builder->GetInsertBlock()->getParent();

  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  BasicBlock *ThenBB = BasicBlock::Create(*ctx, "then", fn);
  BasicBlock *ElseBB = BasicBlock::Create(*ctx, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*ctx, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit then value.
  Builder->SetInsertPoint(ThenBB);

  Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  ThenBB = Builder->GetInsertBlock();

  // Emit else block.
  ElseBB->insertInto(fn, ElseBB);
  Builder->SetInsertPoint(ElseBB);

  Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
  ElseBB = Builder->GetInsertBlock();

  // Emit merge block.
  MergeBB->insertInto(fn, MergeBB);
  Builder->SetInsertPoint(MergeBB);
  PHINode *PN = Builder->CreatePHI(Type::getDoubleTy(*ctx), 2, "iftmp");

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}
Value *ForExprAST::codegen() {
  // Emit the start code first, without 'variable' in scope.
  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  // Make the new basic block for the loop header, inserting after current
  // block.
  Function *fn = Builder->GetInsertBlock()->getParent();
  BasicBlock *PreheaderBB = Builder->GetInsertBlock();
  BasicBlock *LoopBB = BasicBlock::Create(*ctx, "loop", fn);

  // Insert an explicit fall through from the current block to the LoopBB.
  Builder->CreateBr(LoopBB);

  // Start insertion in LoopBB.
  Builder->SetInsertPoint(LoopBB);

  // Start the PHI node with an entry for Start.
  PHINode *Variable =
      Builder->CreatePHI(Type::getDoubleTy(*ctx), 2, VarName);
  Variable->addIncoming(StartVal, PreheaderBB);

  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  Value *OldVal = vals[VarName];
  vals[VarName] = Variable;

  // Emit the body of the loop.  This, like any other expr, can change the
  // current BB.  Note that we ignore the value computed by the body, but don't
  // allow an error.
  if (!Body->codegen())
    return nullptr;

  // Emit the step value.
  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    // If not specified, use 1.0.
    StepVal = ConstantFP::get(*ctx, APFloat(1.0));
  }

  Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

  // Compute the end condition.
  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  // Convert condition to a bool by comparing non-equal to 0.0.
  EndCond = Builder->CreateFCmpONE(
      EndCond, ConstantFP::get(*ctx, APFloat(0.0)), "loopcond");

  // Create the "after loop" block and insert it.
  BasicBlock *LoopEndBB = Builder->GetInsertBlock();
  BasicBlock *AfterBB =
      BasicBlock::Create(*ctx, "afterloop", fn);

  // Insert the conditional branch into the end of LoopEndBB.
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

  // Any new code will be inserted in AfterBB.
  Builder->SetInsertPoint(AfterBB);

  // Add a new entry to the PHI node for the backedge.
  Variable->addIncoming(NextVar, LoopEndBB);

  // Restore the unshadowed variable.
  if (OldVal)
    vals[VarName] = OldVal;
  else
    vals.erase(VarName);

  // for expr always returns 0.0.
  return Constant::getNullValue(Type::getDoubleTy(*ctx));
}
