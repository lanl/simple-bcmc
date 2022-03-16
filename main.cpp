#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>
#include <getopt.h>

extern "C" {
#include "scAcceleratorAPI.h"
#include "scNova.h"
}

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

// Parse the command line into an s1_state.
S1State parse_command_line(int argc, char *argv[]) {
  S1State s1_state;
  struct option long_options[] = 
    {{"emulate", no_argument, nullptr, 'e'},
     {"trace", required_argument, nullptr, 't'},
     {"chips", required_argument, nullptr, 'c'},
     {"apes", required_argument, nullptr, 'a'},
     {"help", no_argument, nullptr, 'h'},
     {nullptr, 0, nullptr, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "h:f:c:a:h", long_options, nullptr)) != -1) {
    switch (c) {
      case 'e':
	s1_state.emulated = true;
	break;
	
      case 't':
	s1_state.trace_flags = std::stoi(optarg, nullptr, 0);
	break;
	
      case 'c':
	int cc, cr;
	if (sscanf(optarg, "%d x %d", &cc, &cr) != 2) {
	  std::cerr << argv[0] << ": --chips must be of the form <cols>x<rows>"
		    << std::endl;
	  std::exit(EXIT_FAILURE);
	}
	s1_state.chip_cols = cc;
	s1_state.chip_rows = cr;
	break;
	
      case 'a':
	int ac, ar;
	if (sscanf(optarg, "%d x %d", &ac, &ar) != 2) {
	  std::cerr << argv[0] << ": --apes must be of the form <cols>x<rows>"
		    << std::endl;
	  std::exit(EXIT_FAILURE);
	}
	s1_state.ape_cols = ac;
	s1_state.ape_rows = ar;
	break;

      case 'h':
	std::cout << "Usage: " << argv[0]
		  << "[--emulate] [--trace=<num>] [--chips=<cols>x<rows>] [--apes=<cols>x<rows>] [--help]"
		  << std::endl;
	std::exit(EXIT_SUCCESS);
	break;
	
      default:
	std::cerr << argv[0] << ": internal error parsing the command line"
		  << std::endl;
	break;
    }
  }
  return s1_state;
}

int main (int argc, char *argv[]) {
  // Parse the command line.
  S1State s1_state = parse_command_line(argc, argv);

  // Initialize the S1.
  initSingularArithmetic();
  scInitializeMachine(s1_state.emulated ? scEmulated : scRealMachine,
		      s1_state.chip_rows, s1_state.chip_cols,
		      s1_state.ape_rows, s1_state.ape_cols,
		      s1_state.trace_flags,
		      0, 0, 0);
		       
  return EXIT_SUCCESS;
}
