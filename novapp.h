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
private:
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
        default:
          throw std::invalid_argument("invalid nova_t passed to NovaExpr");
      }
  }

public:
  // Specify the types of variable a NovaExpr object can represent.
  typedef enum {
        NovaInvalidType,  // Uninitialized
        NovaRegister,     // Hard-wired APE or CU register number
        NovaApeVar,
        NovaCUVar,
        NovaApeMem,
        NovaCUMem
  } nova_t;

  // Maintain a Nova expression and remember its type.
  scExpr expr = 0;    // Declare(expr); without the "static"
  nova_t expr_type;
  bool is_approx;     // true=Approx; false=Int

  // "Declare" a variable without "defining" it.
  NovaExpr() : expr_type(NovaInvalidType) { }

  // Initialize a Nova Approx from a double.
  NovaExpr(double d, nova_t type = NovaApeVar) {
    expr_type = type;
    is_approx = true;
    define_expr();
    Set(expr, AConst(d));
  }

  // Initialize a Nova Int from an int.
  NovaExpr(int i, nova_t type = NovaApeVar) {
    expr_type = type;
    is_approx = false;
    if (type == NovaRegister)
      expr = i;  // Special case for hard-wired registers
    else {
      define_expr();
      Set(expr, IntConst(i));
    }
  }

  // Return the address of a variable stored in either APE or CU memory.
  int operator&() {
    switch (expr_type) {
      case NovaApeMem:
      case NovaCUMem:
        return MemAddress(expr);
        break;
      default:
        throw std::domain_error("attempt to take the address of a non-memory variable");
        break;
    }
  }

  NovaExpr& operator=(const NovaExpr& rhs) {
    expr_type = rhs.expr_type;
    is_approx = rhs.is_approx;
    define_expr();
    Set(expr, rhs.expr);
    return *this;
  }

  NovaExpr& operator=(int rhs) {
    if (expr_type == NovaInvalidType) {
      // Default to an integer APE variable unless we're reassigning an
      // existing NovaExpr.
      expr_type = NovaApeVar;
      is_approx = false;
      define_expr();
    }
    Set(expr, IntConst(rhs));
    return *this;
  }

  NovaExpr& operator=(double rhs) {
    if (expr_type == NovaInvalidType) {
      // Default to an approx APE variable unless we're reassigning an
      // existing NovaExpr.
      expr_type = NovaApeVar;
      is_approx = true;
      define_expr();
    }
    Set(expr, AConst(rhs));
    return *this;
  }

  friend NovaExpr operator+(NovaExpr lhs,
                            const NovaExpr& rhs) {
    lhs.expr = Add(lhs.expr, rhs.expr);
    return lhs;
  }

  NovaExpr& operator+=(const NovaExpr& rhs) {
    Set(expr, Add(expr, rhs.expr));
    return *this;
  }

  friend NovaExpr operator-(NovaExpr lhs,
                            const NovaExpr& rhs) {
    lhs.expr = Sub(lhs.expr, rhs.expr);
    return lhs;
  }

  NovaExpr& operator-=(const NovaExpr& rhs) {
    Set(expr, Sub(expr, rhs.expr));
    return *this;
  }

  friend NovaExpr operator*(NovaExpr lhs,
                            const NovaExpr& rhs) {
    lhs.expr = Mul(lhs.expr, rhs.expr);
    return lhs;
  }

  NovaExpr& operator*=(const NovaExpr& rhs) {
    Set(expr, Mul(expr, rhs.expr));
    return *this;
  }

  friend NovaExpr operator/(NovaExpr lhs,
                            const NovaExpr& rhs) {
    lhs.expr = Div(lhs.expr, rhs.expr);
    return lhs;
  }

  NovaExpr& operator/=(const NovaExpr& rhs) {
    Set(expr, Div(expr, rhs.expr));
    return *this;
  }

  friend NovaExpr operator|(NovaExpr lhs,
                            const NovaExpr& rhs) {
    lhs.expr = Or(lhs.expr, rhs.expr);
    return lhs;
  }

  NovaExpr& operator|=(const NovaExpr& rhs) {
    Set(expr, Or(expr, rhs.expr));
    return *this;
  }

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
};


// Perform a for loop on the CU, taking the loop body as an argument.
void NovaCUForLoop(NovaExpr& var, int from, int to, int step,
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
