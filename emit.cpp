/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

#include <novapp.h>

void emit_nova_code()
{
  // Temporary placeholder code
  NovaExpr A, B, C;
  A = NovaExpr(10);
  B = NovaExpr(20);
  C = A + B;
  TraceOneRegisterOneApe(A.expr, 0, 0);
  TraceOneRegisterOneApe(B.expr, 0, 0);
  TraceOneRegisterOneApe(C.expr, 0, 0);
  
  /*
  DeclareApeVar(A, Int);
  Set(A, IntConst(10));
  DeclareApeVar(B, Int);
  Set(B, IntConst(20));
  DeclareApeVar(C, Int);
  Set(C, Add(A, B));
  TraceOneRegisterOneApe(A, 0, 0);
  TraceOneRegisterOneApe(B, 0, 0);
  TraceOneRegisterOneApe(C, 0, 0);
  */
}
