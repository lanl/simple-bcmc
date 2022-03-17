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
void assign_ape_coords(S1State& s1_state)
{
  // Tell each APE its row number.
  NovaExpr my_row(0);
  NovaExpr rowNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(rowNum, 1, s1_state.ape_rows, 1,
                [&]() {
                  global_get(my_row, my_row, getNorth);
                  ++my_row;
                });
  --my_row;    // Use zero-based numbering.

  // Tell each APE its column number.
  NovaExpr my_col(0);
  NovaExpr colNum(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(colNum, 1, s1_state.ape_cols, 1,
                [&]() {
                  global_get(my_col, my_col, getWest);
                  ++my_col;
                });
  --my_col;    // Use zero-based numbering.

  // Temporary
  TraceOneRegisterAllApes(my_row.expr);
  TraceOneRegisterAllApes(my_col.expr);
}


// Emit the entire S1 program to a low-level kernel.
void emit_nova_code(S1State& s1_state)
{
  assign_ape_coords(s1_state);
}
