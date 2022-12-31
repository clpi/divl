#include <string>
#include <map>
#include <memory>

#include "ast.hh"
#include "lex.hh"

using namespace std;

// auto LHS = make_unique<VarExprAST>("x");
// auto RHS = make_unique<VarExprAST>("y");
// auto Res = make_unique<BinaryExprAST>('+', std::move(LHS), std::move(RHS));


unique_ptr<ExprAST> LogErr(const char *str) {
  fprintf(stderr, "LogErr: %s\n", str);
  return nullptr;
}

unique_ptr<ProtoAST> LogErrP(const char *str) {
  LogErr(str);
  return nullptr;
}

static unique_ptr<ExprAST> ParseNumExpr() {
  auto r = make_unique<NumExprAST>(NumVal);
  getNextTok();
  return std::move(r);
}

static map<char, int> BinopPrec;

static int GetTokPrecedence() {
  if (!isascii(curtok)) return -1;
  int tokPrec = BinopPrec[curtok];
  if (tokPrec <=0) return -1;
  return tokPrec;
}

static unique_ptr<ExprAST> ParseExpr();
static unique_ptr<ExprAST> ParseIfExpr();

static std::unique_ptr<ExprAST> ParseIdentExpr() {
  std::string IdName = IdentStr;
  getNextTok();
  if (curtok != '(') return make_unique<VarExprAST>(IdName);
  getNextTok();
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (curtok != ')') {
    while (1) {
      if (auto Arg = ParseExpr()) Args.push_back(std::move(Arg));
      else return nullptr;
      if (curtok == ')') break;
      if (curtok != ',') return LogErr("Expected ')' or ',' in argument list");
      getNextTok();
    }
  }
  getNextTok();
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}
static unique_ptr<ExprAST> ParseParenExpr() {
  getNextTok();
  auto v = ParseExpr();
  if (!v) return nullptr;
  if (curtok != ')') return LogErr("expected ')'");
  getNextTok();
  return v;
}
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (curtok) {
    default: return LogErr("unknown token when expecting an expression");
    case tok_ident: return ParseIdentExpr();
    case tok_num: return ParseNumExpr();
    case '(': return ParseParenExpr();
    case tok_if: return ParseIfExpr();
  }
}
static std::unique_ptr<ExprAST> ParseBinOpR(int ExprPrec, unique_ptr<ExprAST> LHS) {
  while (true) {
    int tokPrec = GetTokPrecedence();
    if (tokPrec < ExprPrec) return LHS;
    int binop = curtok;
    getNextTok();
    auto r = ParsePrimary();
    if (!r) return nullptr;
    int nextPrec = GetTokPrecedence();
    if (tokPrec < nextPrec) {
      r = ParseBinOpR(tokPrec + 1, std::move(r));
      if (!r) return nullptr;
    }
    LHS = std::make_unique<BinaryExprAST>(binop, std::move(LHS), std::move(r));
  }
}
static unique_ptr<ExprAST> ParseExpr() {
  auto l = ParsePrimary();
  if (!l) return nullptr;
  return ParseBinOpR(0, std::move(l));
}
static unique_ptr<ProtoAST> ParsePrototype() {
  if (curtok != tok_ident) return LogErrP("Expected function name in prototype");
  string FnName = IdentStr;
  getNextTok();
  if (curtok != '(') return LogErrP("Expected '(' in prototype");

  vector<string> ArgNames;
  while (getNextTok() == tok_ident)
    ArgNames.push_back(IdentStr);
  if (curtok != ')') return LogErrP("Expected ')' in prototype");
  getNextTok();
  return std::make_unique<ProtoAST>(FnName, std::move(ArgNames));
}

static std::unique_ptr<FnAST> ParseDefinition() {
  getNextTok(); // eat def.
  auto P = ParsePrototype();
  if (!P) return nullptr;
  if (auto E = ParseExpr()) return make_unique<FnAST>(std::move(P), std::move(E));
  return nullptr;
}

static unique_ptr<FnAST> ParseTopLevelExpr() {
  if (auto E = ParseExpr()) {
    auto pro = make_unique<ProtoAST>("__anon_expr", vector<string>());
    return make_unique<FnAST>(std::move(pro), std::move(E));
  }
  return nullptr;
}

static std::unique_ptr<ProtoAST> ParseExt() {
  getNextTok();
  return ParsePrototype();
}

static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextTok();  // eat the if.

  // condition.
  auto Cond = ParseExpr();
  if (!Cond)
    return nullptr;

  if (curtok != tok_then)
    return LogErr("expected then");
  getNextTok();  // eat the then

  auto Then = ParseExpr();
  if (!Then)
    return nullptr;

  if (curtok != tok_else)
    return LogErr("expected else");

  getNextTok();

  auto Else = ParseExpr();
  if (!Else)
    return nullptr;

  return make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                      std::move(Else));
}
/// forexpr ::= 'for' ident '=' expr ',' expr (',' expr)? 'in' expression
static std::unique_ptr<ExprAST> ParseForExpr() {
  getNextTok(); // eat the for.

  if (curtok != tok_ident)
    return LogErr("expected ident after for");

  std::string IdName = IdentStr;
  getNextTok(); // eat ident.

  if (curtok != '=')
    return LogErr("expected '=' after for");
  getNextTok(); // eat '='.

  auto Start = ParseExpr();
  if (!Start)
    return nullptr;
  if (curtok != ',')
    return LogErr("expected ',' after for start value");
  getNextTok();

  auto End = ParseExpr();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (curtok == ',') {
    getNextTok();
    Step = ParseExpr();
    if (!Step)
      return nullptr;
  }

  if (curtok != tok_in)
    return LogErr("expected 'in' after for");
  getNextTok(); // eat 'in'.

  auto Body = ParseExpr();
  if (!Body)
    return nullptr;

  return make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                       std::move(Step), std::move(Body));
}
