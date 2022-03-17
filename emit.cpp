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
void assign_ape_coords(S1State& s1_state, NovaExpr& ape_row, NovaExpr& ape_col)
{
  // Tell each APE its row number.
  ape_row = 0;
  NovaExpr rowNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(rowNum, 1, s1_state.ape_rows, 1,
                [&]() {
                  global_get(ape_row, ape_row, getNorth);
                  ++ape_row;
                });
  --ape_row;    // Use zero-based numbering.

  // Tell each APE its column number.
  ape_col = 0;
  NovaExpr colNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(colNum, 1, s1_state.ape_cols, 1,
                [&]() {
                  global_get(ape_col, ape_col, getWest);
                  ++ape_col;
                });
  --ape_col;    // Use zero-based numbering.
}


// OR-reduce a value from all APEs to the CU.
void or_reduce_apes_to_cu(S1State& s1_state, NovaExpr& cu_var, NovaExpr& ape_var)
{
  // Loop over all chips, ORing one value per chip into cu_var.
  cu_var = 0;
  NovaExpr chip_row(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(chip_row, 0, s1_state.chip_rows - 1, 1,
		[&]() {
		  NovaExpr chip_col(0, NovaExpr::NovaCUVar);
		  NovaCUForLoop(chip_col, 0, s1_state.chip_cols - 1, 1,
				[&]() {
				  // ReadOr all APEs' ape_var into CU data
				  // memory location 0.
				  eCUC(cuSet, cuRApeRow, _, -1);
				  eCUC(cuSet, cuRApeCol, _, -1);
				  eCUC(cuSetRWAddress, _, _, 0);
				  eControl(controlOpReserveApeReg, apeR0);
				  eApeX(apeSet, apeR0, _, ape_var.expr);
				  int propDelay = 4;  // This is plenty long.
				  eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory, (propDelay<<8)|apeR0);
				  eControl(controlOpReleaseApeReg, apeR0);

				  // OR the per-chip value read into cu_var.
				  DeclareCUMemAtAddress(chip_or, Int, 0);
				  CUIf(Eq(chip_or, IntConst(1)));
				  cu_var = 1;
				  CUFi();
				});
		});
}


// Emit the entire S1 program to a low-level kernel.
void emit_nova_code(S1State& s1_state)
{
  NovaExpr ape_row, ape_col;
  assign_ape_coords(s1_state, ape_row, ape_col);

  // Temporary
  TraceOneRegisterAllApes(ape_row.expr);
  TraceOneRegisterAllApes(ape_col.expr);

  // Temporary
  NovaExpr cu_var(0, NovaExpr::NovaCUVar);
  NovaExpr ape_var = ape_col;
  or_reduce_apes_to_cu(s1_state, cu_var, ape_var);
}
