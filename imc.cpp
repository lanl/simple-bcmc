/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

#include "simple-bcmc.h"

// Sample a simple 2-D angle.  (The third dimension is not used for now.)
void get_angle(std::size_t r_idx, NovaExpr& x_angle, NovaExpr& y_angle)
{
  NovaExpr phi(int_to_approx01(get_random_int())*TWO_PI);
  NovaExpr mu(int_to_approx01(get_random_int())*2.0 - 1.0);
  NovaExpr eta(sqrt(NovaExpr(1.0) - mu*mu));
  x_angle = eta*cos_0_2pi(phi);
  y_angle = eta*sin_0_2pi(phi);
}

// Emit the entire S1 program to a low-level kernel.
void emit_nova_code(S1State& s1)
{
  NovaExpr ape_row, ape_col;
  assign_ape_coords(s1, ape_row, ape_col);

#ifdef XYZZY
  // Temporary
  TraceMessage("Row and column values\n");
  TraceOneRegisterAllApes(ape_row.expr);
  TraceOneRegisterAllApes(ape_col.expr);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Reduction to the CU\n");
  NovaExpr cu_var(0, NovaExpr::NovaCUVar);
  NovaExpr ape_var = ape_col;
  or_reduce_apes_to_cu(s1, cu_var, ape_var);
  TraceRegisterCU(cu_var.expr);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Integer to approx\n");
  NovaExpr some_int(0x4321);
  NovaExpr some_approx = int_to_approx01(some_int);
  TraceOneRegisterOneApe(some_int.expr, 0, 0);
  TraceOneRegisterOneApe(some_approx.expr, 0, 0);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Cosines\n");
  for (double a = 0.0; a <= 6.28318530718; a += 0.1) {
    NovaExpr angle(a);
    NovaExpr cos_val(cos_0_2pi(angle));
    TraceOneRegisterOneApe(angle.expr, 0, 0);
    TraceOneRegisterOneApe(cos_val.expr, 0, 0);
  }
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Threefry\n");
  int dummy_int;  // Hack needed to declare a vector.
  key_3fry = NovaExpr(&dummy_int, NovaExpr::NovaApeMemVector, 8);
  key_3fry[0] = ape_row;
  key_3fry[1] = ape_col;
  for (int i = 2; i < 8; ++i)
    key_3fry[i] = 0;
  counter_3fry = NovaExpr(&dummy_int, NovaExpr::NovaApeMemVector, 8);
  for (int i = 0; i < 8; ++i)
    counter_3fry[i] = 0;
  threefry4x32();
  TraceOneRegisterOneApe(random_3fry[0].expr, 0, 0);
  TraceOneRegisterOneApe(random_3fry[1].expr, 0, 0);
#endif

#ifndef XYZZY
  // Temporary
  TraceMessage("Square roots\n");
  NovaExpr x(4.0);
  x = sqrt(x);
  TraceOneRegisterOneApe(x.expr, 0, 0);
  x = 2.0;
  x = sqrt(x);
  TraceOneRegisterOneApe(x.expr, 0, 0);
#endif
}
