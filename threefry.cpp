/*
 * Implement the Threefry PRNG for Singular Computing's S1 system.
 */

#include <novapp.h>
#include "simple-bcmc.h"

// Each of the following represent four 32-bit numbers stored as eight Ints.
Declare(counter_3fry);  // Input: Loop counter
Declare(key_3fry);      // Input: Key (e.g., APE ID)
Declare(random_3fry);   // Output: Random numbers
Declare(scratch_3fry);  // Internal: Scratch space

// Define the list of Threefry 32x4 rotation constants.
const int rot_32x4[] = {
  10, 26, 11, 21, 13, 27, 23,  5,  6, 20, 17, 11, 25, 10, 18, 20
};

// Emit code to add two 32-bit numbers.
void Add32Bits(scExpr sum_hi, scExpr sum_lo,
               scExpr a_hi, scExpr a_lo,
               scExpr b_hi, scExpr b_lo)
{
  // Copy all arguments to variables to perform all arithmetic (constant
  // construction, vector indexing, etc.) up front.
  DeclareApeVar(a_lo_var, Int);
  DeclareApeVar(a_hi_var, Int);
  DeclareApeVar(b_lo_var, Int);
  DeclareApeVar(b_hi_var, Int);
  DeclareApeVar(sum_lo_var, Int);
  DeclareApeVar(sum_hi_var, Int);
  Set(a_lo_var, a_lo);
  Set(a_hi_var, a_hi);
  Set(b_lo_var, b_lo);
  Set(b_hi_var, b_hi);

  // Because we take scExprs as inputs but will be working directly with
  // registers, we need to stage our data from scExpr --> variable -->
  // register.  We reserve two registers for this.
  eControl(controlOpReserveApeReg, apeR0);
  eControl(controlOpReserveApeReg, apeR1);

  // Add the low-order words.
  eApeX(apeSet, apeR0, _, a_lo_var);
  eApeX(apeSet, apeR1, _, b_lo_var);
  eApeR(apeAdd, sum_lo_var, apeR0, apeR1);

  // Add the high-order words with carry.
  eApeX(apeSet, apeR0, _, a_hi_var);
  eApeX(apeSet, apeR1, _, b_hi_var);
  eApeR(apeAddL, sum_hi_var, apeR0, apeR1);

  // Release the reserved registers.
  eControl(controlOpReleaseApeReg, apeR0);
  eControl(controlOpReleaseApeReg, apeR1);

  // Copy the low-order and high-order words to their final destination.
  Set(sum_lo, sum_lo_var);
  Set(sum_hi, sum_hi_var);
}

/* Add two 32-bit integers, each represented as a vector of two 16-bit Ints.
 * The arguments alternate a base name (Nova vector) and an index, pretending
 * this is indexing N 32-bit elements rather than N*2 16-bit elements. */
#define ADD32(OUT, OUT_IDX, IN1, IN1_IDX, IN2, IN2_IDX)         \
  do {                                                          \
    Add32Bits(IndexVector(OUT, IntConst(2*(OUT_IDX))),          \
              IndexVector(OUT, IntConst(2*(OUT_IDX) + 1)),      \
              IndexVector(IN1, IntConst(2*(IN1_IDX))),          \
              IndexVector(IN1, IntConst(2*(IN1_IDX) + 1)),      \
              IndexVector(IN2, IntConst(2*(IN2_IDX))),          \
              IndexVector(IN2, IntConst(2*(IN2_IDX) + 1)));     \
  }                                                             \
  while (0)

// Key injection for round/4.
void inject_key(int r)
{
  int i;

  for (i = 0; i < 4; i++)
    ADD32(random_3fry, i, random_3fry, i, scratch_3fry, (r + i)%5);
  Add32Bits(IndexVector(random_3fry, IntConst(3*2)),
            IndexVector(random_3fry, IntConst(3*2 + 1)),
            IndexVector(random_3fry, IntConst(3*2)),
            IndexVector(random_3fry, IntConst(3*2 + 1)),
            IntConst(0),
            IntConst(r));
}

// Mixer operation.
void mix(int a, int b, int ridx)
{
  int rot = rot_32x4[ridx];  // Number of bits by which to left-rotate

  // Increment random_3fry[a] by random_3fry[b].
  ADD32(random_3fry, a, random_3fry, a, random_3fry, b);

  // Left-rotate random_3fry[b] by rot.
  DeclareApeVar(hi, Int);
  DeclareApeVar(lo, Int);
  if (rot >= 16) {
    // To rotate by rot >= 16, swap the high and low Ints then prepare to
    // rotate by rot - 16.
    Set(hi, IndexVector(random_3fry, IntConst(b*2)));
    Set(lo, IndexVector(random_3fry, IntConst(b*2 + 1)));
    Set(IndexVector(random_3fry, IntConst(b*2 + 1)), hi);
    Set(IndexVector(random_3fry, IntConst(b*2)), lo);
    rot -= 16;
  }
  if (rot != 0) {
    Set(hi, Asl(IndexVector(random_3fry, IntConst(b*2)),
                IntConst(rot)));
    Set(lo, Asl(IndexVector(random_3fry, IntConst(b*2 + 1)),
                IntConst(rot)));
    DeclareApeVar(mask, Int);
    Set(mask, IntConst((1<<rot) - 1));
    Set(hi,
        Or(hi,
           And(Asr(IndexVector(random_3fry, IntConst(b*2 + 1)),
                   IntConst(16 - rot)),
               mask)));
    Set(lo,
        Or(lo,
           And(Asr(IndexVector(random_3fry, IntConst(b*2)),
                   IntConst(16 - rot)),
               mask)));
    Set(IndexVector(random_3fry, IntConst(b*2)), hi);
    Set(IndexVector(random_3fry, IntConst(b*2 + 1)), lo);
  }

  // Xor the new random_3fry[b] by random_3fry[a].
  Set(IndexVector(random_3fry, IntConst(b*2)),
      Xor(IndexVector(random_3fry, IntConst(b*2)),
          IndexVector(random_3fry, IntConst(a*2))));
  Set(IndexVector(random_3fry, IntConst(b*2 + 1)),
      Xor(IndexVector(random_3fry, IntConst(b*2 + 1)),
          IndexVector(random_3fry, IntConst(a*2 + 1))));
}

/* Use counter_3fry and key_3fry to generate random numbers random_3fry. */
void threefry4x32()
{
  int r;  /* Round */
  int i;

  /* Initialize both the internal and output state. */
  ApeMemVector(random_3fry, Int, 8);
  ApeMemVector(scratch_3fry, Int, 10);
  Set(IndexVector(scratch_3fry, IntConst(8)), IntConst(0x1BD1));
  Set(IndexVector(scratch_3fry, IntConst(9)), IntConst(0x1BDA));

  for (i = 0; i < 4; i++) {
    DeclareApeVar(hi, Int);
    DeclareApeVar(lo, Int);
    Set(hi, IntConst(i*2));
    Set(lo, IntConst(i*2 + 1));
    Set(IndexVector(scratch_3fry, hi), IndexVector(key_3fry, hi));
    Set(IndexVector(scratch_3fry, lo), IndexVector(key_3fry, lo));
    Set(IndexVector(random_3fry, hi), IndexVector(counter_3fry, hi));
    Set(IndexVector(random_3fry, lo), IndexVector(counter_3fry, lo));
    Set(IndexVector(scratch_3fry, IntConst(8)),
        Xor(IndexVector(scratch_3fry, IntConst(8)),
            IndexVector(key_3fry, hi)));
    Set(IndexVector(scratch_3fry, IntConst(9)),
        Xor(IndexVector(scratch_3fry, IntConst(9)),
            IndexVector(key_3fry, lo)));
  }
  for (i = 0; i < 4; i++)
    ADD32(random_3fry, i, random_3fry, i, scratch_3fry, i);

  // Perform 20 rounds of mixing.
  for (r = 0; r < 20; r++) {
    /* Inject */
    if (r%4 == 0 && r > 0)
      inject_key(r/4);

    /* Mix */
    if (r%2 == 0) {
      mix(0, 1, (2*r)%16);
      mix(2, 3, (2*r + 1)%16);
    }
    else {
      mix(0, 3, (2*r)%16);
      mix(2, 1, (2*r + 1)%16);
    }
  }
  inject_key(20/4);
}