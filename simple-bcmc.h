/*
 * Definitions shared across files for a simple billion-core Monte
 * Carlo simulation
 */

#ifndef _SIMPLE_BCMC_H
#define _SIMPLE_BCMC_H

#include "novapp.h"

#define TWO_PI (2*M_PI)

// Encapsulate machine state.
struct S1State {
  bool emulated;    // true=emulated; false=real hardware
  int trace_flags;  // Trace flags for emulator
  int chip_cols;    // Columns of chips
  int chip_rows;    // Rows of chips
  int ape_cols;     // APE columns per chip
  int ape_rows;     // APE rows per chip

  S1State() : emulated(false), trace_flags(0),
              chip_cols(1), chip_rows(1),
              ape_cols(44), ape_rows(48)
  {
  }
};

extern NovaExpr counter_3fry;  // RNG input: Loop counter
extern NovaExpr key_3fry;      // RNG input: Key (e.g., APE ID)

extern void emit_nova_code(S1State&, unsigned long long seed);
extern NovaExpr ape_min(const NovaExpr& a, const NovaExpr& b);
extern void assign_ape_coords(const S1State& s1, NovaExpr& ape_row, NovaExpr& ape_col);
extern void or_reduce_apes_to_cu(const S1State& s1, NovaExpr* cu_var, const NovaExpr& ape_var);
extern NovaExpr int_to_approx01(const NovaExpr& i_val);
extern NovaExpr cos_0_2pi(const NovaExpr& x);
extern NovaExpr sin_0_2pi(const NovaExpr& x);
extern NovaExpr get_random_int();
extern NovaExpr ln_of_int(const NovaExpr& r);

#endif
