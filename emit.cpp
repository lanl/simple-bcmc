/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

extern "C" {
#include <scAcceleratorAPI.h>
#include <scNova.h>
}

void emit_nova_code()
{
  // Temporary placeholder code
  DeclareApeVar(A, Int);
  Set(A, IntConst(10));
  DeclareApeVar(B, Int);
  Set(B, IntConst(20));
  DeclareApeVar(C, Int);
  Set(C, Add(A, B));
  TraceOneRegisterOneApe(C, 0, 0);
}
