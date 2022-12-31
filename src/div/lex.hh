#include <string>
#include <optional>

using namespace std;

enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_ext = -3,
  tok_ident = -4,
  tok_num = -5,
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,  
  tok_for = -9,
  tok_in = -10
};

static string IdentStr;
static double NumVal;

static int getspace(int c) {
  return -1;
}

static int gettok() {
  static int lastChar = ' ';
  while (isspace(lastChar)) 
    lastChar = getchar();
  if (isalpha(lastChar)) {
    while (isalnum((lastChar = getchar())))
      IdentStr += lastChar;
    if (IdentStr == "def") return tok_def;
    if (IdentStr == "ext") return tok_ext;
    if (IdentStr == "if") return tok_if;
    if (IdentStr == "then") return tok_then;
    if (IdentStr == "else") return tok_else;
    if (IdentStr == "for") return tok_for;
    if (IdentStr == "in") return tok_in;
    return tok_ident;
  }
  if (isdigit(lastChar) || lastChar == '.') {
    string NumStr;
    do {
      NumStr += lastChar;
      lastChar = getchar();
    } while (isdigit(lastChar) || lastChar == '.');
    NumVal = strtod(NumStr.c_str(), 0);
    return tok_num;
  }
  if (lastChar == '#') {
    do lastChar = getchar();
    while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');
    if (lastChar != EOF) return gettok();
  }
  if (lastChar == EOF) return tok_eof;
    int thisChar = lastChar;
    lastChar = getchar();
    return thisChar;
}

static int curtok;
static int getNextTok() {
  return curtok = gettok();
}



