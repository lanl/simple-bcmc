/*
 * C++ wrappers for Singular Computing's Nova macros.
 *
 * Author: Scott Pakin <pakin@lanl.gov>
 */

#ifndef _NOVAPP_H_
#define _NOVAPP_H_

#include <stdexcept>
#include <functional>

extern "C" {
#include "scAcceleratorAPI.h"
#include "scNova.h"
}

// Wrap a Nova expression in a C++ class.
class NovaExpr {
public:
  // Specify the types of variable a NovaExpr object can represent.
  typedef enum {
        NovaInvalidType,  // Uninitialized
        NovaRegister,     // Hard-wired APE or CU register number
        NovaApeVar,
        NovaCUVar,
        NovaApeMem,
        NovaCUMem,
        NovaApeMemVector,
        NovaCUMemVector,
        NovaApeMemArray,
        NovaCUMemArray,
        NovaApeMemArrayPartial,  // Internal use only
        NovaCUMemArrayPartial    // Internal use only
  } nova_t;

private:
  // Store information about the expression that we can't easily access from an
  // scExpr without modifying Nova itself.
  nova_t expr_type;   // NovaApeVar, NovaCUVar, etc.
  bool is_approx;     // true=Approx; false=Int
  size_t rows;        // Number of rows in a vector or array
  size_t cols;        // Number of columns in an array
  scExpr row_idx;     // Row index in operator[][] array accesses

  // Invoke the correct {Ape,CU}{Var,Mem} function based on the current value
  // of expr_type and is_approx.
  void define_expr() {
    if (is_approx)
      // Approx
      switch (expr_type) {
        case NovaApeVar:
          ApeVar(expr, Approx);
          break;
        case NovaCUVar:
          CUVar(expr, Approx);
          break;
        case NovaApeMem:
          ApeMem(expr, Approx);
          break;
        case NovaCUMem:
          CUMem(expr, Approx);
          break;
        case NovaApeMemVector:
          ApeMemVector(expr, Approx, int(rows));
          break;
        case NovaCUMemVector:
          CUMemVector(expr, Approx, int(rows));
          break;
        case NovaApeMemArray:
          ApeMemArray(expr, Approx, int(rows), int(cols));
          break;
        case NovaCUMemArray:
          CUMemArray(expr, Approx, int(rows), int(cols));
          break;
        default:
          throw std::invalid_argument("invalid nova_t passed to NovaExpr");
      }
    else
      // Int
      switch (expr_type) {
        case NovaApeVar:
          ApeVar(expr, Int);
          break;
        case NovaCUVar:
          CUVar(expr, Int);
          break;
        case NovaApeMem:
          ApeMem(expr, Int);
          break;
        case NovaCUMem:
          CUMem(expr, Int);
          break;
        case NovaApeMemVector:
          ApeMemVector(expr, Int, int(rows));
          break;
        case NovaCUMemVector:
          CUMemVector(expr, Int, int(rows));
          break;
        case NovaApeMemArray:
          ApeMemArray(expr, Int, int(rows), int(cols));
          break;
        case NovaCUMemArray:
          CUMemArray(expr, Int, int(rows), int(cols));
          break;
        default:
          throw std::invalid_argument("invalid nova_t passed to NovaExpr");
      }
  }

  // Convert all APE types to NovaApeVar and all CU types to NovaCUVar.
  static nova_t convert_to_var(nova_t type) {
    nova_t result = NovaInvalidType;
    switch (type) {
      case NovaApeVar:
      case NovaApeMem:
      case NovaApeMemVector:
      case NovaApeMemArray:
        result = NovaApeVar;
        break;

      case NovaCUVar:
      case NovaCUMem:
      case NovaCUMemVector:
      case NovaCUMemArray:
      case NovaCUMemArrayPartial:
        result = NovaCUVar;
        break;

      default:
        throw std::invalid_argument("invalid nova_t passed to NovaExpr");
    }
    return result;
  }

public:
  // Maintain a Nova expression.
  scExpr expr = 0;    // Declare(expr); without the "static"

  // Return true if the NovaExpr has been assigned a value.
  bool has_value() { return expr_type != NovaInvalidType; }

  // ----- Constructors -----

  // "Declare" a variable without "defining" it.
  NovaExpr() : expr_type(NovaInvalidType), rows(0), cols(0) { }

  // Initialize a NovaExpr from another NovaExpr, optionally converting it
  // to a variable (if it were originally memory).
  NovaExpr(const NovaExpr& other, bool as_var=false) {
    expr_type = as_var ? convert_to_var(other.expr_type) : other.expr_type;
    is_approx = other.is_approx;
    rows = other.rows;
    cols = other.cols;
    define_expr();
    Set(expr, other.expr);
  }

  // Move a NovaExpr to another NovaExpr.
  NovaExpr(const NovaExpr&& other) noexcept {
    expr_type = other.expr_type;
    is_approx = other.is_approx;
    rows = other.rows;
    cols = other.cols;
    expr = other.expr;
  }

  // Initialize a Nova Approx from a double.  The double is currently
  // ignored for vector and array types.
  NovaExpr(double d, nova_t type = NovaApeVar, size_t rows=1, size_t cols=1) {
    expr_type = type;
    is_approx = true;
    this->rows = rows;
    this->cols = cols;
    define_expr();
    switch (expr_type) {
      case NovaApeMemVector:
      case NovaCUMemVector:
      case NovaApeMemArray:
      case NovaCUMemArray:
        // No data initialization for vectors or arrays.
        break;

      default:
        // Scalars are initialized to the given value.
        Set(expr, AConst(d));
        break;
    }
  }

  // Initialize a Nova Int from an int.  The int is currently ignored for
  // vector and array types.
  NovaExpr(int i, nova_t type = NovaApeVar, size_t rows=1, size_t cols=1) {
    expr_type = type;
    is_approx = false;
    this->rows = rows;
    this->cols = cols;
    switch (expr_type) {
      case NovaRegister:
        // Hard-wired registers use the register number as the expression
        // itself.
        expr = i;
        break;

      case NovaApeMemVector:
      case NovaCUMemVector:
      case NovaApeMemArray:
      case NovaCUMemArray:
        // No data initialization for vectors or arrays.
        define_expr();
        break;

      default:
        // Scalars are initialized to the given value.
        define_expr();
        Set(expr, IntConst(i));
        break;
    }
  }

  // ----- Assignment operators -----

  // Assign one NovaExpr to another using Nova's Set macro.
  NovaExpr& operator=(const NovaExpr& rhs) {
    expr_type = rhs.expr_type;
    is_approx = rhs.is_approx;
    define_expr();
    switch (expr_type) {
      case NovaApeMemVector:
      case NovaCUMemVector:
      case NovaApeMemArray:
      case NovaCUMemArray:
        // Store only a pointer for vector/array expressions.  (Set doesn't
        // work here.)
        expr = rhs.expr;
        break;

      default:
        // Copy scalar expressions.
        Set(expr, rhs.expr);
        break;
    }
    return *this;
  }

  // Assign an integer constant to a NovaExpr.
  NovaExpr& operator=(int rhs) {
    switch (expr_type) {
      case NovaInvalidType:
        // Default to an integer APE variable because we're not reassigning an
        // existing NovaExpr.
        expr_type = NovaApeVar;
        is_approx = false;
        define_expr();
        Set(expr, IntConst(rhs));
        break;
      case NovaRegister:
        // Use a low-level mechanism to assign a value to a CU or APE register.
        eCUC(cuSet, expr, _, rhs);
        break;
      default:
        // Normally we use Nova to assign an integer constant.
        Set(expr, IntConst(rhs));
        break;
    }
    return *this;
  }

  // Assign a floating-point constant to a NovaExpr.
  NovaExpr& operator=(double rhs) {
    if (expr_type == NovaInvalidType) {
      // Default to an approx APE variable because we're not reassigning an
      // existing NovaExpr.
      expr_type = NovaApeVar;
      is_approx = true;
      define_expr();
    }
    Set(expr, AConst(rhs));
    return *this;
  }

  // ----- Helper macros -----

  // A NOVA_OP defines both an arithmetic and an assignment operator
  // that accept a NovaExpr on the right-hand side.
#define NOVA_OP(OP, OP_EQ, NOVA)                                \
  friend NovaExpr operator OP(NovaExpr lhs,                     \
                              const NovaExpr& rhs) {            \
  NovaExpr result(lhs, true);                                   \
    Set(result.expr, NOVA(lhs.expr, rhs.expr));                 \
    return result;                                              \
  }                                                             \
                                                                \
  NovaExpr& operator OP_EQ(const NovaExpr& rhs) {               \
    Set(expr, NOVA(expr, rhs.expr));                            \
    return *this;                                               \
  }

  // An INTEGER_OP defines both an arithmetic and an assignment operator
  // that accept either a NovaExpr or an integer on the right-hand side.
#define INTEGER_OP(OP, OP_EQ, NOVA)                             \
  NOVA_OP(OP, OP_EQ, NOVA)                                      \
                                                                \
  friend NovaExpr operator OP(NovaExpr lhs, const int rhs) {    \
    NovaExpr result(lhs, true);                                 \
    Set(result.expr, NOVA(lhs.expr, IntConst(rhs)));            \
    return result;                                              \
  }                                                             \
                                                                \
  NovaExpr& operator OP_EQ(const int rhs) {                     \
    Set(expr, NOVA(expr, IntConst(rhs)));                       \
    return *this;                                               \
  }

  // An APPROX_OP defines both an arithmetic and an assignment operator
  // that accept either a NovaExpr or a double on the right-hand side.
#define APPROX_OP(OP, OP_EQ, NOVA)                              \
  NOVA_OP(OP, OP_EQ, NOVA)                                      \
                                                                \
  friend NovaExpr operator OP(NovaExpr lhs, const double rhs) { \
    NovaExpr result(lhs, true);                                 \
    Set(result.expr, NOVA(lhs.expr, AConst(rhs)));              \
    return result;                                              \
  }                                                             \
                                                                \
  NovaExpr& operator OP_EQ(const double rhs) {                  \
    Set(expr, NOVA(expr, AConst(rhs)));                         \
    return *this;                                               \
  }

  // A GENERAL_OP defines both an arithmetic and an assignment operator
  // that accept a NovaExpr, an integer, or a double on the right-hand
  // side.
#define GENERAL_OP(OP, OP_EQ, NOVA)                             \
  NOVA_OP(OP, OP_EQ, NOVA)                                      \
                                                                \
  friend NovaExpr operator OP(NovaExpr lhs, const int rhs) {    \
    NovaExpr result(lhs, true);                                 \
    Set(result.expr, NOVA(lhs.expr, IntConst(rhs)));            \
    return result;                                              \
  }                                                             \
                                                                \
  NovaExpr& operator OP_EQ(const int rhs) {                     \
    Set(expr, NOVA(expr, IntConst(rhs)));                       \
    return *this;                                               \
  }                                                             \
                                                                \
  friend NovaExpr operator OP(NovaExpr lhs, const double rhs) { \
    NovaExpr result(lhs, true);                                 \
    Set(result.expr, NOVA(lhs.expr, AConst(rhs)));              \
    return result;                                              \
  }                                                             \
                                                                \
  NovaExpr& operator OP_EQ(const double rhs) {                  \
    Set(expr, NOVA(expr, AConst(rhs)));                         \
    return *this;                                               \
  }

  // A GENERAL_REL defines a relational operator that accepts a NovaExpr,
  // an integer, or a double on the right-hand side.
#define GENERAL_REL(OP, NOVA)                                           \
  friend NovaExpr operator OP(const NovaExpr& lhs,                      \
                              const NovaExpr& rhs) {                    \
    NovaExpr result;                                                    \
    result.expr_type = convert_to_var(lhs.expr_type);                   \
    result.is_approx = false;                                           \
    result.expr = NOVA(lhs.expr, rhs.expr);                             \
    return result;                                                      \
  }                                                                     \
                                                                        \
  friend NovaExpr operator OP(const NovaExpr& lhs, int rhs) {           \
    NovaExpr result;                                                    \
    result.expr_type = convert_to_var(lhs.expr_type);                   \
    result.is_approx = false;                                           \
    result.expr = NOVA(lhs.expr, IntConst(rhs));                        \
    return result;                                                      \
  }                                                                     \
                                                                        \
  friend NovaExpr operator OP(const NovaExpr& lhs, double rhs) {        \
    NovaExpr result;                                                    \
    result.expr_type = convert_to_var(lhs.expr_type);                   \
    result.is_approx = false;                                           \
    result.expr = NOVA(lhs.expr, AConst(rhs));                          \
    return result;                                                      \
  }

  // ----- Basic arithmetic -----

  GENERAL_OP(+, +=, Add)
  GENERAL_OP(-, -=, Sub)
  APPROX_OP(*, *=, Mul)
  APPROX_OP(/, /=, Div)

  NovaExpr operator-() {
    NovaExpr result;
    result.expr_type = convert_to_var(expr_type);
    result.is_approx = is_approx;
    result.expr = Sub(is_approx ? AConst(0.0) : IntConst(0), expr);
    return result;
  }

  // ----- Bit manipulation -----

  INTEGER_OP(|, |=, Or)
  INTEGER_OP(&, &=, And)
  INTEGER_OP(^, ^=, Xor)
  INTEGER_OP(<<, <<=, Asl)
  INTEGER_OP(>>, >>=, Asr)

  // ----- Prefix and postfix operators -----

  NovaExpr& operator++() {
    Set(expr, Add(expr, IntConst(1)));
    return *this;
  }

  NovaExpr& operator++(int not_used) {
    Set(expr, Add(expr, IntConst(1)));
    return *this;
  }

  NovaExpr& operator--() {
    Set(expr, Sub(expr, IntConst(1)));
    return *this;
  }

  NovaExpr& operator--(int not_used) {
    Set(expr, Sub(expr, IntConst(1)));
    return *this;
  }

  // ----- Index operators -----

  NovaExpr operator[](std::size_t idx) const {
    NovaExpr val;
    val.is_approx = is_approx;
    switch (expr_type) {
      case NovaApeMemVector:
        val.expr_type = NovaApeVar;
        val.expr = IndexVector(expr, IntConst(idx));
        break;
      case NovaCUMemVector:
        val.expr_type = NovaCUVar;
        val.expr = IndexVector(expr, IntConst(idx));
        break;
      case NovaApeMemArray:
        val.expr_type = NovaApeMemArrayPartial;
        val.expr = expr;
        val.row_idx = IntConst(idx);
        break;
      case NovaCUMemArray:
        val.expr_type = NovaCUVar;
        val.expr = expr;
        val.row_idx = IntConst(idx);
        break;
      case NovaApeMemArrayPartial:
        val.expr_type = NovaApeVar;
        val.expr = IndexArray(expr, row_idx, IntConst(idx));
        break;
      case NovaCUMemArrayPartial:
        val.expr_type = NovaCUVar;
        val.expr = IndexArray(expr, row_idx, IntConst(idx));
        break;
      default:
        throw std::invalid_argument("operator[] applied to a scalar");
    }
    return val;
  }

  NovaExpr operator[](const NovaExpr& idx) const {
    NovaExpr val;
    val.is_approx = is_approx;
    switch (expr_type) {
      case NovaApeMemVector:
        val.expr_type = NovaApeVar;
        val.expr = IndexVector(expr, idx.expr);
        break;
      case NovaCUMemVector:
        val.expr_type = NovaCUVar;
        val.expr = IndexVector(expr, idx.expr);
        break;
      case NovaApeMemArray:
        val.expr_type = NovaApeMemArrayPartial;
        val.row_idx = idx.expr;
        break;
      case NovaCUMemArray:
        val.expr_type = NovaCUMemArrayPartial;
        val.row_idx = idx.expr;
        break;
      case NovaApeMemArrayPartial:
        val.expr_type = NovaApeVar;
        val.expr = IndexArray(expr, row_idx, idx.expr);
        break;
      case NovaCUMemArrayPartial:
        val.expr_type = NovaCUVar;
        val.expr = IndexArray(expr, row_idx, idx.expr);
        break;
      default:
        throw std::invalid_argument("operator[] applied to a scalar");
    }
    return val;
  }

  // ----- Square root -----

  friend NovaExpr sqrt(const NovaExpr& x) {
    NovaExpr s(x);
    Set(s.expr, Sqrt(x.expr));
    return s;
  }

  // ----- Conditionals -----

  GENERAL_REL(==, Eq)
  GENERAL_REL(!=, Ne)
  GENERAL_REL(<, Lt)
  GENERAL_REL(<=, Le)
  GENERAL_REL(>, Gt)
  GENERAL_REL(>=, Ge)

  // ----- Logical operators -----

  friend NovaExpr operator||(NovaExpr lhs, NovaExpr rhs) {
    NovaExpr result;
    result.expr_type = convert_to_var(lhs.expr_type);
    result.is_approx = false;
    result.expr = Or(lhs.expr, rhs.expr);
    return result;
  }

  friend NovaExpr operator&&(NovaExpr lhs, NovaExpr rhs) {
    NovaExpr result;
    result.expr_type = convert_to_var(lhs.expr_type);
    result.is_approx = false;
    result.expr = And(lhs.expr, rhs.expr);
    return result;
  }

  friend NovaExpr operator!(NovaExpr rhs) {
    NovaExpr result;
    result.expr_type = convert_to_var(rhs.expr_type);
    result.is_approx = false;
    result.expr = Not(rhs.expr);
    return result;
  }
};


// Perform an if statement on the APEs, taking the then and else clauses as
// arguments.
static void NovaApeIf(const NovaExpr& cond,
                      const std::function <void ()>& f_then,
                      const std::function <void ()>& f_else=nullptr)
{
  ApeIf(cond.expr);
  f_then();
  if (f_else != nullptr) {
    ApeElse();
    f_else();
  }
  ApeFi();
}

// Perform an if statement on the CU, taking the then and else clauses as
// arguments.
static void NovaCUIf(const NovaExpr& cond,
                     const std::function <void ()>& f_then,
                     const std::function <void ()>& f_else=nullptr)
{
  CUIf(cond.expr);
  f_then();
  CUFi();
  if (f_else != nullptr) {
    // There is no CUElse() macro in Nova.
    CUIf(Not(cond.expr));
    f_else();
    CUFi();
  }
}

// Perform a for loop on the CU, taking the loop body as an argument.
static void NovaCUForLoop(NovaExpr& var, int from, int to, int step,
                          const std::function <void ()>& f)
{
  CUFor(var.expr, IntConst(from), IntConst(to), IntConst(step));
  f();
  CUForEnd();
}

// Predefine wrappers for certain registers.
inline NovaExpr active_chip_row = NovaExpr(cuRChipRow, NovaExpr::NovaRegister);
inline NovaExpr active_chip_col = NovaExpr(cuRChipCol, NovaExpr::NovaRegister);
inline NovaExpr active_ape_row = NovaExpr(cuRApeRow, NovaExpr::NovaRegister);
inline NovaExpr active_ape_col = NovaExpr(cuRApeCol, NovaExpr::NovaRegister);

#endif
