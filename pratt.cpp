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

using std::string, std::vector, std::array, std::cout, std::format;
using std::unique_ptr, std::make_unique, std::pair;

//== Tokenizer ====={{{
////== Op definition ====={{{
struct Op {
#define OP_LIST \
    X(Add, +, 1, 2) \
    X(Sub, -, 1, 2) \
    X(Mul, *, 3, 4) \
    X(Div, /, 3, 4) \
    X(Exp, ^, 5, 6)

  enum class Kind {
    None = 0,
    #define X(OpKind, _0, _1, _2) OpKind,
    OP_LIST
    #undef X
    NumOps,
  };

  Kind type = Kind::None;

  static constexpr array LeftBindingPowers {
    #define X(_0, _1, bp, _2) static_cast<u8>(bp),
    static_cast<u8>(0),
    OP_LIST
    #undef X
  };

  static constexpr array RightBindingPowers {
    #define X(_0, _1, _2, bp) static_cast<u8>(bp),
    static_cast<u8>(0),
    OP_LIST
    #undef X
  };

  pair<u8,u8> infix_binding_power() const {
    auto l_bp = this->LeftBindingPowers[static_cast<usize>(this->type)];
    auto r_bp = this->RightBindingPowers[static_cast<usize>(this->type)];
    return {l_bp, r_bp};
  }

  static constexpr array Names {
    #define X(OpKind, _0, _1, _2) #OpKind,
    "NoOp",
    OP_LIST
    #undef X
  };

  string name() const {
    return this->Names[static_cast<size_t>(this->type)];
  }

  static constexpr array Symbols {
    #define X(_0, OpSymbol, _1, _2) #OpSymbol,
    "<none>",
    OP_LIST
    #undef X
  };

  string symbol() const {
    return this->Symbols[static_cast<size_t>(this->type)];
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

    #define X(_Type, _Symbol, _1, _2) case (#_Symbol)[0]: { index++; return Op{Op::Kind::_Type}; }
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
    Expr,
  } kind = kind = Kind::None;

  union {
    i64 literal;
    struct {
      Op op;
      unique_ptr<Expr> left;
      unique_ptr<Expr> right;
    } expr;
  };

  string str();

  friend std::ostream& operator<< (std::ostream &os, const Expr& expr);

  Expr(): kind(Kind::None) {};
  Expr(i64 val): kind(Kind::Literal), literal(val) {}
  Expr(Op op, unique_ptr<Expr> left, unique_ptr<Expr> right) {
    kind = Kind::Expr;
    expr.op = {op};
    new (&expr.left) unique_ptr<Expr>(std::move(left));
    new (&expr.right) unique_ptr<Expr>(std::move(right));
  }
  ~Expr() {
    switch(kind) {
      case Kind::None:
      case Kind::Literal:
        break;
      case Kind::Expr:
        expr.left.~unique_ptr<Expr>();
        expr.right.~unique_ptr<Expr>();
        break;
    }
  }
  
  static unique_ptr<Expr> Literal(i64 val) {
    return make_unique<Expr>(val);
  }
};

string Expr::str() {
  switch(kind) {
    case Kind::None: return "<none>";
    case Kind::Literal: return format("{}", literal);
    case Kind::Expr: return format("({} {} {})", expr.op.symbol(), expr.left->str(), expr.right->str());
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

  Token expect(Token::Kind expected) {
    auto got = this->next();
    if (got.kind != expected) {
      throw std::runtime_error(
        format("Expected token of kind {}, got {}", 
               tokenkind_name(expected), tokenkind_name(got.kind))
      );
    }
    return got;
  }
  
  unique_ptr<Expr> parse_expr(u8 min_bp = 0) {
    auto lhs_val = this->expect(Token::Kind::Int);
    auto lhs = Expr::Literal(lhs_val.integer);

    while (true) {
      auto tok = peek();
      if (tok.kind == Token::Kind::None) break; // EOF
      if (tok.kind != Token::Kind::Op) throw std::runtime_error("Expected operator!");

      // token is op
      auto op = tok.op;
      auto [l_bp, r_bp] = op.infix_binding_power();

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
  if (argc == 1) throw std::runtime_error("No argument!");
  string stream = argv[1];
  Tokenizer tokenizer(stream);
  auto tokens = tokenizer.tokenize();

  cout << "#== Tokens ==\n";

  for (auto tok: tokens) {
    std::cout << tok.str() << std::endl;
  }

  cout << "\n#== AST =====\n";

  Parser p {.tokens = tokens};
  
  auto expr = p.parse_expr();

  std::cout << expr->str() << std::endl;

  return 0;
}
