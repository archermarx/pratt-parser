#include <cassert>
#include <cstddef>
#include <format>
#include <iostream>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

using u8 = uint8_t;
using i64 = int64_t;
using u64 = uint64_t;
using usize = size_t;
using isize = ptrdiff_t;
using f32 = float;
using f64 = double;

using std::string, std::vector, std::array, std::cout, std::endl, std::format;
using std::unique_ptr, std::make_unique, std::pair;

constexpr i64 powu(i64 x, usize p) {
  // special casing
  switch(p) {
    case (0): return 1;
    case (1): return x;
    case (2): return x*x;
    case (3): return x*x*x;
    case (4): return x*x*x*x;
    default:
      break;
  }

  // power by repeated squaring
  i64 res = x;
  i64 p_cur = 1;
  while (2*p_cur < p) {
    res = res*res;
    p_cur *= 2;
  }

  i64 remainder = p - p_cur;
  return res * powu(x, remainder);
}

i64 powi(i64 x, i64 p) {
  if (p < 0) throw std::runtime_error("Integer cannot be raised to negative power");
  return powu(x, (usize)p);
}

i64 factorial_unchecked(i64 x) {
  if (x == 0 || x == 1) {
    return 1;
  }
  return x * factorial_unchecked(x-1);
}

i64 factorial(i64 x) {
  if (x > 21) {
    throw std::runtime_error(format("{}! will overflow!", x));
  }
  if (x < 0) {
    throw std::runtime_error(format("Factorial of negative integer {} is not defined", x));
  }
  return factorial_unchecked(x);
}

//== Tokenizer ====={{{
////== Op definition ====={{{
struct Op {
    // Name, symbol, left infix bp, right infix bp, prefix bp, postfix bp
    // zero bp means the operator is invalid in that position
#define OP_LIST \
    X(Add, +, 1, 2, 5, 0) \
    X(Sub, -, 1, 2, 5, 0) \
    X(Mul, *, 3, 4, 0, 0) \
    X(Div, /, 3, 4, 0, 0) \
    X(Exp, ^, 9, 10, 0, 0) \
    X(Fact, !, 0, 0, 0, 7)

  enum class Kind {
    #define X(OpKind, _0, _1, _2, _3, _4) OpKind,
    OP_LIST
    #undef X
  };

  Kind kind;

  Op(Kind kind): kind(kind) {}

  pair<u8, u8> infix_binding_power() const {
    #define X(op, _0, l_bp, r_bp, _1, _2) case Kind::op: return {l_bp, r_bp};
    switch(kind) {
      OP_LIST 
    }
    #undef X
  }

  pair<u8, u8> prefix_binding_power() const {
    #define X(op, _0, _1, _2, bp, _3) case Kind::op: return {0, bp}; 
    switch(kind) {
      OP_LIST
    }
    #undef X
  }

  pair<u8, u8> postfix_binding_power() const {
    #define X(op, _0, _1, _2, _3, bp) case Kind::op: return {bp, 0};
    switch(kind) {
      OP_LIST
    }
    #undef X
  }

  string name() {
    #define X(op, _0, _1, _2, _3, _4) case Kind::op: return #op;
    switch(kind) {
      OP_LIST
    }
    #undef X
  } 

  string symbol() {
    #define X(op, sym, _0, _1, _2, _3) case Kind::op: return #sym;
    switch(kind) {
      OP_LIST
    }
    #undef X
  }

  i64 eval(i64 x)  {
    switch(this->kind) {
      case Kind::Add: return x;
      case Kind::Sub: return -x;
      case Kind::Fact: return factorial(x);
      default: throw std::runtime_error(format("Invalid unary operator '{}'. This should be unreachable.", this->symbol()));
    }
  }

  i64 eval(i64 left, i64 right) {
    switch(this->kind){
      case Kind::Add:  return left + right;
      case Kind::Sub:  return left - right;
      case Kind::Mul:  return left * right;
      case Kind::Div:  return left / right;
      case Kind::Exp:  return powi(left, right);
      default: throw std::runtime_error(format("Invalid infix operator '{}'. This should be unreachable", this->symbol()));
    }

  }
};
////== end op definition }}}

///== Token definition==={{{

struct Token {
  enum class Kind  {
    None, Int, Op,
  };
  Kind kind;
  union {
    u8 byte;
    i64 integer;
    Op op;
  };
  Token(): kind(Kind::None), byte('\0') {}
  Token(u8 byte): kind(Kind::None), byte(byte) {}
  Token(i64 integer): kind(Kind::Int), integer(integer) {}
  Token(Op op): kind(Kind::Op), op(op) {}

  string str() {
    switch(kind) {
      case Kind::Op: return format("Op: {}", op.symbol());
      case Kind::Int: return format("Int: {}", integer);
      default: return format("<'{:c}'(0x{:2x})>", byte, byte);
    }
  }
};

std::string tokenkind_name(Token::Kind kind) {
  switch(kind) {
    case(Token::Kind::None): return "<none>";
    case(Token::Kind::Int): return "Int";
    case(Token::Kind::Op): return "Op";
  }
}

////== end token definition }}}

bool is_space(u8 c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool is_digit(u8 c) {
  return c >= '0' && c <= '9';
}

bool is_hexdigit(u8 c) {
  return (c >= '0' && c <= '9') || (c >= 'a' || c <= 'f') || (c >= 'A' || c <= 'F');
}

struct Tokenizer {
  std::vector<u8> stream;
  usize index = 0;

  Tokenizer(string s): stream(s.begin(), s.end()) {}
  Tokenizer(vector<u8> stream): stream(stream) {}

  u8 peek(i64 n = 0) {
    if (index + n >= stream.size()) {
      return '\0';
    } else {
      return stream[index + n];
    }
  };

  u8 advance(i64 n = 1) {
    u8 c = stream[index];
    if (index <= stream.size()) {
      index += 1;
    }
    return c;
  };

  Token read_number() {
    u8 c = '\0';
    i64 acc = 0;
    while (is_digit(c = peek()) || c == '_') {
      index++;
      if (c == '_') continue;
      acc = 10 * acc + (c - '0');
    }
    return acc;
  }

  Token next() {
    // Consume whitespace
    while (is_space(peek())) {
      index++;
    }

    u8 c = peek();

    #define X(op, sym, _0, _1, _2, _3) case (#sym)[0]: { index++; return {Op::Kind::op}; }
    switch(c) {
      OP_LIST
      default:
        if (is_digit(c)) return read_number();
        // Unclassifiable -- abort
        Token tok = c;
        throw std::runtime_error(format("Unexpected token {} at byte {} of stream", tok.str(), index));
    }
    #undef X
  }

  vector<Token> tokenize() {
    vector<Token> tokens{};
    while (index < stream.size()) {
      tokens.emplace_back(next());
    }
    return tokens;
  }
};
//== end tokenizer }}}

//== Parser ===== {{{

////== Expr definition ===== {{{
struct Expr{
  enum class Kind {
    None,
    Literal,
    Unary,
    Binary,
  } kind = kind = Kind::None;

  union {
    i64 literal;
    struct {
      Op op;
      unique_ptr<Expr> expr;
    } unary;
    struct {
      Op op;
      unique_ptr<Expr> left;
      unique_ptr<Expr> right;
    } binary;
  };

  string str();

  friend std::ostream& operator<< (std::ostream &os, const Expr& expr);

  Expr(): kind(Kind::None) {};
  Expr(i64 val): kind(Kind::Literal), literal(val) {}
  Expr(Op op, unique_ptr<Expr> expr): kind(Kind::Unary) {
    unary.op = op;
    new (&unary.expr) unique_ptr<Expr>(std::move(expr));
  }
  Expr(Op op, unique_ptr<Expr> left, unique_ptr<Expr> right): kind(Kind::Binary) {
    binary.op = {op};
    new (&binary.left) unique_ptr<Expr>(std::move(left));
    new (&binary.right) unique_ptr<Expr>(std::move(right));
  }
  ~Expr() {
    switch(kind) {
      case Kind::None:
      case Kind::Literal:
        break;
      case Kind::Unary:
        unary.expr.~unique_ptr<Expr>();
      case Kind::Binary:
        binary.left.~unique_ptr<Expr>();
        binary.right.~unique_ptr<Expr>();
        break;
    }
  }
  
  static unique_ptr<Expr> Literal(i64 val) {
    return make_unique<Expr>(val);
  }

  i64 eval() {
    switch(this->kind) {
      case Kind::None: throw std::runtime_error("Attempt to eval expr of type None");
      case Kind::Literal: return this->literal;
      case Kind::Unary:   return this->unary.op.eval(this->unary.expr->eval());
      case Kind::Binary:  return this->binary.op.eval(this->binary.left->eval(), this->binary.right->eval());
    } 
  }
};

string Expr::str() {
  switch(kind) {
    case Kind::None: return "<none>";
    case Kind::Literal: return format("{}", literal);
    case Kind::Unary: return format("({} {})", unary.op.symbol(), unary.expr->str(), literal);
    case Kind::Binary: return format("({} {} {})", binary.op.symbol(), binary.left->str(), binary.right->str());
  }
}
////== end expr struct definition }}}

struct Parser {
  vector<Token> tokens {};
  i64 index = 0;
  
  Token peek() const {
    if (index >= tokens.size()) return {};
    return tokens[index];
  }

  Token next() {
    Token tok = peek();
    if (index <= tokens.size()) index++;
    return tok;
  }

  unique_ptr<Expr> parse_expr(u8 min_bp = 0) {
    auto lhs_tok = this->next();
    //cout << lhs_tok.str() << endl;
    unique_ptr<Expr> lhs;
    switch (lhs_tok.kind) {
      case Token::Kind::Int:{
        lhs = Expr::Literal(lhs_tok.integer);
        break;
      }
      case Token::Kind::Op: {  
        // prefix operator
        auto op = lhs_tok.op;
        auto [_, bp] = op.prefix_binding_power();
        if (bp == 0) throw std::runtime_error(format("Invalid unary operator '{}'", op.symbol()));
        auto rhs = parse_expr(bp);
        lhs = make_unique<Expr>(op, std::move(rhs));
        break;
      }
      default: throw std::runtime_error(format("Expected operator or number, got '{}'", lhs_tok.str()));
    }

    // parse binary operators
    while (true) {
      auto tok = peek();
      if (tok.kind == Token::Kind::None) break; // EOF
      if (tok.kind != Token::Kind::Op) throw std::runtime_error("Expected operator!");

      // token is op
      auto op = tok.op;

      auto [l_bp, r_bp] = op.infix_binding_power();

      // check if this op is a postfix op and parse if so
      if (l_bp == 0 && r_bp == 0) {
        auto [bp, _] = op.postfix_binding_power();
        if (bp < min_bp) break;
        next();
        lhs = make_unique<Expr>(op, std::move(lhs));
        continue;
      }

      if (l_bp < min_bp) break;

      next();
      auto rhs = parse_expr(r_bp);
      lhs = make_unique<Expr>(op, std::move(lhs), std::move(rhs));
    }

    return lhs;
  }
};


//== end parser }}}

int main(int argc, char **argv) {
  // TODO: too much validation at all stages, leading to exceptions peppered throughout
  // possibly remove things like None from many stages
  // also, return errors instead of panicking, perhaps
  
  if (argc == 1) throw std::runtime_error("No argument!");

  string stream = "";
  bool print_tokens = true;
  bool print_ast = true;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (stream != "") {
      throw std::runtime_error("Too many arguments");
    }
    stream = arg;
  }

  Tokenizer tokenizer(stream);
  auto tokens = tokenizer.tokenize();

  if (print_tokens) {
    cout << "#== Tokens ==\n";

    for (auto tok: tokens) {
      std::cout << tok.str() << "\n";
    }

    cout << "\n";
  }

  Parser p {.tokens = tokens};
  auto expr = p.parse_expr();

  if (print_ast) {
    cout << "#== AST =====\n";
    std::cout << expr->str() << "\n\n";
  }

  auto result = expr->eval();

  std::cout << result << std::endl;

  return 0;
}
