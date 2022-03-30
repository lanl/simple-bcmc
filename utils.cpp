/*
 * Miscellaneous utility functions for a simple billion-core Monte
 * Carlo simulation
 */

#include "simple-bcmc.h"
#include <cmath>

#define TWO_PI (2*M_PI)

// Convert an integer in [0, 65535] to an approx in [0, 1].
NovaExpr int_to_approx01(const NovaExpr& i_val)
{
  NovaExpr a_val(0.0);
  NovaExpr one(1);
  for (int i = 0; i < 16; ++i) {
    ApeIf(Eq(((i_val>>i)&1).expr, IntConst(1)));
    double f = 1.0/double(1<<(16 - i));  // ..., 1/8, 1/4, 1/2
    a_val += f;
    ApeFi();
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
