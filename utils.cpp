/*
 * Miscellaneous utility functions for a simple billion-core Monte
 * Carlo simulation
 */

#include "simple-bcmc.h"
#include <cmath>

// Return the minimum of two APE expressions.
NovaExpr ape_min(const NovaExpr& a, const NovaExpr& b)
{
  NovaExpr result;
  NovaApeIf(a < b,
            [&]() { result = a; },
            [&]() { result = b; });
  return result;
}

// Perform a global Get operation.
void global_get(NovaExpr& dest, NovaExpr src, int dir)
{
  // Get 16 bits, one at a time.
  eApeC(apeGetGStart, _, _, dir);
  eApeC(apeGetGStartDone, 0, src.expr, 0);
  for (int i = 0; i < 16; ++i)
    eApeC(apeGetGMove, _, _, _);
  eApeC(apeGetGMoveDone, _, _, _);
  eApeC(apeGetGEnd, dest.expr, src.expr, dir);

}

// Tell each APE its row and column number.
void assign_ape_coords(const S1State& s1, NovaExpr& ape_row, NovaExpr& ape_col)
{
  // Tell each APE its row number.
  ape_row = 0;
  NovaExpr rowNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(rowNum, 1, s1.ape_rows*s1.chip_rows, 1,
                [&]() {
                  global_get(ape_row, ape_row, getNorth);
                  ++ape_row;
                });
  --ape_row;    // Use zero-based numbering.

  // Tell each APE its column number.
  ape_col = 0;
  NovaExpr colNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(colNum, 1, s1.ape_cols*s1.chip_cols, 1,
                [&]() {
                  global_get(ape_col, ape_col, getWest);
                  ++ape_col;
                });
  --ape_col;    // Use zero-based numbering.
}


// OR-reduce a value from all APEs to the CU.
void or_reduce_apes_to_cu(const S1State& s1, NovaExpr* cu_var, const NovaExpr& ape_var)
{
  // Loop over all chips, ORing one value per chip into cu_var.
  *cu_var = 0;
  DeclareCUVar(SomethingChangedInGrid,Int);
  Set(SomethingChangedInGrid,IntConst(0));
  NovaExpr chip_or(0, NovaExpr::NovaCUMem);   // Per-chip OR result
  NovaCUForLoop(active_chip_row, 0, s1.chip_rows - 1, 1, [&]() {
    NovaCUForLoop(active_chip_col, 0, s1.chip_cols - 1, 1, [&]() {
      // Compute an OR across all APEs on the current chip.
      // eCUC(cuSet, active_ape_col.expr, _, -1);
      active_ape_col = -1;
      eCUC(cuSetRWAddress, _, _, MemAddress(chip_or.expr));
      int apeRChanged = apeR1;  // Register indicating ape_var is nonzero for some APE on the current chip.
      eControl(controlOpReserveApeReg, apeRChanged);
      eApeX(apeSet, apeRChanged, _, ape_var.expr);
      int propDelay = 4;  // This is plenty long.
      eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory, (propDelay<<8)|apeRChanged);
      eControl(controlOpReleaseApeReg, apeRChanged);

      // OR the per-chip value into cu_var.
      CUIf(Ne(chip_or.expr, IntConst(0)));
      *cu_var = 1;
      CUFi();
    });
  });
}

// Convert an integer in [0, 65535] to an approx in [0, 1].
NovaExpr int_to_approx01(const NovaExpr& i_val)
{
  NovaExpr a_val(0.0);
  NovaExpr one(1);
  for (int i = 0; i < 16; ++i) {
    NovaApeIf((i_val>>i)&1 == 1, [&](){
      double f = 1.0/double(1<<(16 - i));  // ..., 1/8, 1/4, 1/2
      a_val += f;
    });
  }
  return a_val;
}

// Approximate cos(x) on [0, 2*pi] using 5 Chebyshev polynomials.
NovaExpr cos_0_2pi(const NovaExpr& x)
{
  // Scale num from [0, 2*pi] to [-1, 1].
  NovaExpr num((x*2.0 - TWO_PI)/TWO_PI);

  // Instantiate the Chebyshev polynomials.
  NovaExpr num2(num*2.0);
  NovaExpr t0(1.0);
  NovaExpr t1(num);
  NovaExpr t2(num2*t1 - t0);
  NovaExpr t3(num2*t2 - t1);
  NovaExpr t4(num2*t3 - t2);

  // Compute a linear combination of the Chebyshev polynomials.  We ignore
  // the t1 and t3 terms here because their coefficients were too small in
  // magnitude (<1e-6).
  NovaExpr sum(t0*0.30420407768492924161);
  sum += t4*-0.33194352475813920789;
  sum += t2*0.97226055289980561902;
  return sum;
}

// Approximate sin(x) on [0, 2*pi] using 6 Chebyshev polynomials.
NovaExpr sin_0_2pi(const NovaExpr& x)
{
  // Scale num from [0, 2*pi] to [-1, 1].
  NovaExpr num((x*2.0 - TWO_PI)/TWO_PI);

  // Instantiate the Chebyshev polynomials.
  NovaExpr num2(num*2.0);
  NovaExpr t0(1.0);
  NovaExpr t1(num);
  NovaExpr t2(num2*t1 - t0);
  NovaExpr t3(num2*t2 - t1);
  NovaExpr t4(num2*t3 - t2);
  NovaExpr t5(num2*t4 - t3);

  // Compute a linear combination of the Chebyshev polynomials.  We ignore
  // the t0, t2, and t4 terms here because their coefficients were too
  // small in magnitude (<1e-6).
  NovaExpr sum(t1*-0.56923064009501811444);
  sum += t3*0.66716913685894241315;
  sum += t5*-0.11112410957439385062;
  return sum;
}

// Compute ln(r) for r in [0, 65535].
NovaExpr ln_of_int(const NovaExpr& r)
{
  // Hard-wire the number of iterations to perform.
  const int n = 5;

  // Prepare the numerator and denominator.
  NovaExpr a[2];  // 32-bit big-endian version of the numerator (r)
  NovaExpr b[2];  // 32-bit big-endian version of the denominator (1)
  a[0] = 0;
  a[1] = r;
  b[0] = 0;
  b[1] = 1;
  NovaExpr lg(0.0);

  // Unroll the given number of iterations.  We do this on the host because
  // we need j in a floating-point expression.
  for (int j = 0; j < n; ++j) {
    // Unroll "while (a > b)" to a depth of 16.
    NovaExpr k(0, NovaExpr::NovaCUVar);
    NovaCUForLoop(k, 0, 15, 1, [&]() {
      NovaApeIf(a[0] > b[0] || (a[0] == b[0] && a[1] >= b[1]), [&]() {
        lg += std::log(1.0 + std::pow(2.0, -double(j)));

        // 32-bit a -= b
        NovaApeIf(a[1] < b[1], [&]() {
          --a[0];
        });
        a[0] -= b[0];
        a[1] -= b[1];

        // 32-bit a <<= j
        a[0] = (a[0]<<j) | (a[1]>>(16 - j));  // Logical shift right
        a[1] <<= j;

        // 32-bit b += b<<j
        NovaExpr bj[2];   // b<<j
        bj[0] = (b[0]<<j) | (b[1]>>(16 - j));  // Logical shift right
        bj[1] = b[1]<<j;
        b[0] += bj[0];
        NovaExpr b1(b[1] + bj[1]);
        NovaApeIf(b1 < b[1], [&]() {  // Carry
          ++b[0];
        });
        b[1] = b1;
      });
    });

    // 32-bit a <<= 1
    a[0] = (a[0]<<1) | ((a[1]>>15) & 1);
    a[1] <<= 1;
  }
  return lg - std::log(65535.0);
}
