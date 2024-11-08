#include <cassert>
#include <cstddef>
#include <format>
#include <iostream>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <variant>
#include <vector>

using u8 = uint8_t;
using i64 = int64_t;
using u64 = uint64_t;
using usize = size_t;
using isize = ptrdiff_t;
using f32 = float;
using f64 = double;

using std::string, std::vector, std::array, std::cout, std::format;
using std::variant, std::holds_alternative, std::get_if, std::get;

struct Op {
#define OP_LIST \
  X(Add, +) \
    X(Sub, -) \
    X(Mul, *) \
    X(Div, /)

  enum class Type {
    None = 0,
    #define X(OpType, _) OpType,
    OP_LIST
    #undef X
    NumOps,
  };

  Type type = Type::None;

  static constexpr array Names {
    "NoOp",
#define X(OpType, _) #OpType,
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
#define X(_Op, _Symbol) case Type::_Op: { return a _Symbol b; }
      OP_LIST
      #undef X
      default: assert(false);
    }
  }

  static Op classify(std::string str) {
    if (str == "") { return {}; }
#define X(_Op, _Symbol) else if (str == #_Symbol) { return {Type::_Op}; }
    OP_LIST
    #undef X
    else { return {}; }
};
};

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

  friend std::ostream& operator<< (std::ostream& os, Token tok);
};

std::ostream& operator<< (std::ostream &os, Token tok) { return os << tok.str(); }

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
#define X(_Type, _Symbol) case (#_Symbol)[0]: { index++; return Op{Op::Type::_Type}; }
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

struct ExprNode;

using Expr = variant<ExprNode, i64>;

struct ExprNode {
  Op op;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
};


int main() {
  string stream = "2 + 2 / 52";
  Tokenizer tokenizer {.stream = {stream.begin(), stream.end()}};
  auto tokens = tokenizer.tokenize();


  for (auto tok: tokens) {
    std::cout << tok << std::endl;
  }

  return 0;
}
