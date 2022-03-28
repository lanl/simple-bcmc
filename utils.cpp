/*
 * Miscellaneous utility functions for a simple billion-core Monte
 * Carlo simulation
 */

#include "simple-bcmc.h"

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
