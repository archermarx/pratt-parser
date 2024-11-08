#include <cassert>
#include <cstddef>
#include <format>
#include <iostream>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>

using u8 = uint8_t;
using i64 = int64_t;
using u64 = uint64_t;
using usize = size_t;
using isize = ptrdiff_t;
using f32 = float;
using f64 = double;

using std::string, std::vector, std::array, std::cout, std::format;
using std::unique_ptr, std::make_unique;

//== Tokenizer ====={{{
////== Op definition ====={{{
struct Op {
#define OP_LIST \
  X(Add, +) \
    X(Sub, -) \
    X(Mul, *) \
    X(Div, /)

  enum class Kind {
    None = 0,
    #define X(OpKind, _) OpKind,
    OP_LIST
    #undef X
    NumOps,
  };

  Kind type = Kind::None;

  static constexpr array Names {
    "NoOp",
#define X(OpKind, _) #OpKind,
    OP_LIST
    #undef X
  };

  string name() const {
    return this->Names[static_cast<size_t>(this->type)];
  }

  static constexpr array Symbols {
    "<none>",
#define X(_, OpSymbol) #OpSymbol,
    OP_LIST
    #undef X
  };

  string symbol() const {
    return this->Symbols[static_cast<size_t>(this->type)];
  }

  f64 eval (f64 a, f64 b) const {
    switch(this->type) {
#define X(_Op, _Symbol) case Kind::_Op: { return a _Symbol b; }
      OP_LIST
      #undef X
      default: assert(false);
    }
  }

  static Op classify(std::string str) {
    if (str == "") { return {}; }
#define X(_Op, _Symbol) else if (str == #_Symbol) { return {Kind::_Op}; }
    OP_LIST
    #undef X
    else { return {}; }
};
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
  usize index;

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

    switch(c) {
#define X(_Type, _Symbol) case (#_Symbol)[0]: { index++; return Op{Op::Kind::_Type}; }
      OP_LIST
      #undef X
      default:
        if (is_digit(c)) return read_number();
        // Unclassifiable -- abort
        Token tok = c;
        throw std::runtime_error(format("Unexpected token {} at byte {} of stream", tok.str(), index));
    }
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

  void expect(Token::Kind expected) const {
    auto got = peek().kind;
    if (got != expected) {
      throw std::runtime_error(
        format("Expected token of kind {}, got {}", 
               tokenkind_name(expected), tokenkind_name(got))
      );
    }
  }

  unique_ptr<Expr> parse() {
    expect(Token::Kind::Int);
    auto left = Expr::Literal(next().integer);
    switch(peek().kind) {
      case Token::Kind::None:
        return left;
      case Token::Kind::Op: {
        auto op = next().op;
        auto right = parse();
        return make_unique<Expr>(op, std::move(left), std::move(right));
      }
      default:
        expect(Token::Kind::Op);
        return unique_ptr<Expr>();
    }
  }
};


//== end parser }}}

int main() {
  string stream = "2*2 + 2 / 3 * 5";
  Tokenizer tokenizer {.stream = {stream.begin(), stream.end()}};
  auto tokens = tokenizer.tokenize();

  cout << "#== Tokens ==\n";

  for (auto tok: tokens) {
    std::cout << tok.str() << std::endl;
  }

  cout << "\n#== AST =====\n";

  Parser p {.tokens = tokens};
  
  auto expr = p.parse();

  std::cout << expr->str() << std::endl;

  return 0;
}
