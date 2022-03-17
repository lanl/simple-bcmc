/*
 * Definitions shared across files for a simple billion-core Monte
 * Carlo simulation
 */

#ifndef _SIMPLE_BCMC_H
#define _SIMPLE_BCMC_H

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

#endif