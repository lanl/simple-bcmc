/*
 * Top-level code for a simple billion-core Monte Carlo simulation
 */

#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <getopt.h>
#include "simple-bcmc.h"

// Parse the command line into an S1State plus a random-number seed.
S1State parse_command_line(int argc, char *argv[], unsigned long long* seed) {
  S1State s1;
  struct option long_options[] =
    {{"emulate", no_argument, nullptr, 'e'},
     {"trace", required_argument, nullptr, 't'},
     {"chips", required_argument, nullptr, 'c'},
     {"apes", required_argument, nullptr, 'a'},
     {"seed", required_argument, nullptr, 's'},
     {"help", no_argument, nullptr, 'h'},
     {nullptr, 0, nullptr, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "h:f:c:a:s:h", long_options, nullptr)) != -1) {
    switch (c) {
      case 'e':
        s1.emulated = true;
        break;

      case 't':
        s1.trace_flags = std::stoi(optarg, nullptr, 0);
        break;

      case 'c':
        int cc, cr;
        if (sscanf(optarg, "%d x %d", &cc, &cr) != 2) {
          std::cerr << argv[0] << ": --chips must be of the form <cols>x<rows>"
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
        s1.chip_cols = cc;
        s1.chip_rows = cr;
        break;

      case 'a':
        int ac, ar;
        if (sscanf(optarg, "%d x %d", &ac, &ar) != 2) {
          std::cerr << argv[0] << ": --apes must be of the form <cols>x<rows>"
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
        s1.ape_cols = ac;
        s1.ape_rows = ar;
        break;

      case 's':
        char *endptr;
        errno = 0;
        *seed = std::strtoull(optarg, &endptr, 0);
        if (*endptr != '\0' || errno != 0) {
          std::cerr << argv[0] << ": --seed requires an integer argument"
                    << std::endl;
          std::exit(EXIT_FAILURE);
        }
        break;

      case 'h':
        std::cout << "Usage: " << argv[0]
                  << "[--emulate] [--trace=<num>] [--chips=<cols>x<rows>] [--apes=<cols>x<rows>] [--seed=<num>] [--help]"
                  << std::endl;
        std::exit(EXIT_SUCCESS);
        break;

      default:
        std::exit(EXIT_FAILURE);
        break;
    }
  }
  return s1;
}

int main (int argc, char *argv[]) {
  // Parse the command line.
  unsigned long long seed = 0ULL;
  S1State s1 = parse_command_line(argc, argv, &seed);

  // Initialize the S1.
  initSingularArithmetic();
  scInitializeMachine(s1.emulated ? scEmulated : scRealMachine,
                      s1.chip_rows, s1.chip_cols,
                      s1.ape_rows, s1.ape_cols,
                      s1.trace_flags,
                      0, 0, 0);

  // Compile the entire S1 program to a kernel.
  scNovaInit();
  scEmitLLKernelCreate();
  eCUC(cuSetMaskMode, _, _, 1);
  eCUC(cuSetGroupMode, _, _, 0);
  eApeC(apeSetMask, _, _, 0);
  emit_nova_code(s1, seed);
  eCUC(cuHalt, _, _, _);
  scKernelTranslate();

  // Launch the S1 program and wait for it to finish.
  extern LLKernel *llKernel;
  scLLKernelLoad(llKernel, 0);
  scLLKernelExecute(0);
  scLLKernelWaitSignal();

  // Shut down the S1 and the program.
  scTerminateMachine();
  return EXIT_SUCCESS;
}
