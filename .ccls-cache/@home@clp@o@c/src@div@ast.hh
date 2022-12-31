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

class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual Value *codegen() = 0;
};

class NumExprAST : public ExprAST {
  double Val;

public:
  NumExprAST(double v) : Val(v) {}
  Value *codegen() override;
};


class VarExprAST : public ExprAST {
  string Name;

public:
  VarExprAST(const string &Name) : Name(Name) {}
  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;
public:
  BinaryExprAST(char op, unique_ptr<ExprAST> LHS, unique_ptr<ExprAST> RHS)
    : Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value *codegen() override;
};

class CallExprAST : public ExprAST {
  string Callee;
  vector<unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee, vector<unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
  Value *codegen() override;
};

class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
      : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
        Step(std::move(Step)), Body(std::move(Body)) {}

  Value *codegen() override;
};

class ProtoAST {
  string Name;
  vector<string> Args;

public:
  ProtoAST(const string &name, vector<string> args)
  : Name(name), Args(std::move(args)) {}

  Function *codegen();
  const string &getName() const { return Name; }
};

class FnAST {
  unique_ptr<ProtoAST> Proto;
  unique_ptr<ExprAST> Body;

public:
  FnAST(unique_ptr<ProtoAST> proto, unique_ptr<ExprAST> body)
  : Proto(std::move(proto)), Body(std::move(body)) {}
  Function *codegen();
};


class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
    : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  Value *codegen() override;
};
