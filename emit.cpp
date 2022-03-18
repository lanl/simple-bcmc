/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

#include <novapp.h>
#include "simple-bcmc.h"

// Perform a global Get operation.
void global_get(NovaExpr dest, NovaExpr src, int dir)
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
  NovaCUForLoop(rowNum, 1, s1.ape_rows, 1,
                [&]() {
                  global_get(ape_row, ape_row, getNorth);
                  ++ape_row;
                });
  --ape_row;    // Use zero-based numbering.

  // Tell each APE its column number.
  ape_col = 0;
  NovaExpr colNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(colNum, 1, s1.ape_cols, 1,
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
      eCUC(cuSet, active_ape_col.expr, _, -1);
      eCUC(cuSetRWAddress, _, _, &chip_or);
      int apeRChanged = apeR1;  // Register indicating ape_var is nonzero for some APE on the current chip.
      eControl(controlOpReserveApeReg, apeRChanged);
      eApeX(apeSet, apeRChanged, _, ape_var.expr);
      int propDelay = 4;  // This is plenty long.
      eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory, (propDelay<<8)|apeRChanged);
      eControl(controlOpReleaseApeReg, apeRChanged);

      // OR the per-chip value into cu_var.
      CUIf(Eq(chip_or.expr, IntConst(1)));
      cu_var = 1;
      CUFi();
      TraceMessage("  Checking ChipRow "); TraceRegisterCU(active_chip_row.expr);
      TraceMessage("           ChipCol "); TraceRegisterCU(active_chip_col.expr);
      TraceMessage("           Changed "); TraceRegisterCU(chip_or.expr);
    });
  });
}


// Temporary
void original_or(S1State& s1, NovaExpr& ape_var)
{
  // Readback from Apes whether anything changed, and if not then set NoChanges.
  // We use ReadOr to Or the changed flag in all the Apes in each chip,
  //   and read the chips sequentially and Or those results in the CU.
  DeclareCUVar(SomethingChangedInGrid,Int);
  Set(SomethingChangedInGrid,IntConst(0));
  DeclareCUMem(SomethingChangedInChip, Int);
  CUFor(cuRChipRow, IntConst(0), IntConst(s1.chip_rows - 1), IntConst(1));
  CUFor(cuRChipCol, IntConst(0), IntConst(s1.chip_cols - 1), IntConst(1));
  eCUC(cuSet, cuRApeCol, _, -1);
  eCUC(cuSetRWAddress, _, _, MemAddress(SomethingChangedInChip));
  int apeRChanged = apeR1;
  eControl(controlOpReserveApeReg,apeRChanged);
  eApeX(apeSet, apeRChanged, _, ape_var.expr);
  int propDelay = 4;  // this is plenty long
  eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory, (propDelay<<8)|apeRChanged);
  eControl(controlOpReleaseApeReg,apeRChanged);
  // Or the value read into SomethingChangedInGrid
  CUIf(Eq(SomethingChangedInChip,IntConst(1)));
  Set(SomethingChangedInGrid,IntConst(1));
  CUFi();
  TraceMessage("  Checking ChipRow "); TraceRegisterCU(cuRChipRow);
  TraceMessage("           ChipCol "); TraceRegisterCU(cuRChipCol);
  TraceMessage("           Changed "); TraceRegisterCU(SomethingChangedInChip);
  CUForEnd();
  CUForEnd();
}


// Emit the entire S1 program to a low-level kernel.
void emit_nova_code(S1State& s1)
{
  NovaExpr ape_row, ape_col;
  assign_ape_coords(s1, ape_row, ape_col);

  // Temporary
  TraceOneRegisterAllApes(ape_row.expr);
  TraceOneRegisterAllApes(ape_col.expr);

  // Temporary
  NovaExpr cu_var(0, NovaExpr::NovaCUVar);
  NovaExpr ape_var = ape_col;
  or_reduce_apes_to_cu(s1, cu_var, ape_var);
  //original_or(s1, ape_var);
}
