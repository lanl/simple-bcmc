/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

#include "simple-bcmc.h"

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
void assign_ape_coords(S1State& s1, NovaExpr& ape_row, NovaExpr& ape_col)
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
void or_reduce_apes_to_cu(S1State& s1, NovaExpr& cu_var, NovaExpr& ape_var)
{
  // Loop over all chips, ORing one value per chip into cu_var.
  cu_var = 0;
  DeclareCUVar(SomethingChangedInGrid,Int);
  Set(SomethingChangedInGrid,IntConst(0));
  NovaExpr chip_or(0, NovaExpr::NovaCUMem);   // Per-chip OR result
  NovaCUForLoop(active_chip_row, 0, s1.chip_rows - 1, 1, [&]() {
    NovaCUForLoop(active_chip_col, 0, s1.chip_cols - 1, 1, [&]() {
      // Compute an OR across all APEs on the current chip.
      // eCUC(cuSet, active_ape_col.expr, _, -1);
      active_ape_col = -1;
      eCUC(cuSetRWAddress, _, _, &chip_or);
      int apeRChanged = apeR1;  // Register indicating ape_var is nonzero for some APE on the current chip.
      eControl(controlOpReserveApeReg, apeRChanged);
      eApeX(apeSet, apeRChanged, _, ape_var.expr);
      int propDelay = 4;  // This is plenty long.
      eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory, (propDelay<<8)|apeRChanged);
      eControl(controlOpReleaseApeReg, apeRChanged);

      // OR the per-chip value into cu_var.
      CUIf(Ne(chip_or.expr, IntConst(0)));
      cu_var = 1;
      CUFi();
    });
  });
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

#ifndef XYZZY
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
#endif
}
