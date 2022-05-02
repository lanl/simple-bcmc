IMC for the S1
==============

This work in progress represents an initial attempt to express a complete but extremely simple implicit Monte Carlo (IMC) application for [Singular Computing](https://www.singularcomputing.com/)'s S1 prototype hardware.  The S1 is a scalable SIMD system based on 16-bit approximate arithmetic and spatially aware computation.  At the time of this writing, the code is complete but does not appear to be returning correct results.

The main IMC logic can be found in [`imc.cpp`](imc.cpp), and a lot of interesting helper functions (e.g., transcendental computations and communication routines) are implemented in [`utils.cpp`](utils.cpp).  [`novapp.h`](novapp.h) ("Nova++") is a set of C++ wrappers for Singular Computing's Nova C preprocessor macros.  Its goal is to improve code readability by replacing bulky macro calls with overloaded operators.  For example, with Nova++ one can write `x[i] += a*b + c*d` instead of `Set(IndexVector(x, i), Add(IndexVector(x, i), Add(Mul(a, b), Mul(c, d))))`.

Installation
------------

The code requires Singular Computing's proprietary software environment, which includes the Nova macros and hardware emulator.  Singular Computing welcomes inquiries from parties interested in exploring its currently available hardware systems (contact@singularcomputing.com).

Edit the [`Makefile`](Makefile) to point `SCROOT` to the Singular Computing software directory then simply run `make` to produce a `simple-bcmc` executable.

Usage
-----

Run `./simple-bcmc --help` for a list of options.  To run on the emulator, it's recommended to scale down the number of APEs (SIMD cores) to reduce execution time.  For example, try
```console
$ ./simple-bcmc --emulate --apes=4x4
```

Legal statement
---------------

Copyright © 2021 Triad National Security, LLC.
All rights reserved.

This program was produced under U.S. Government contract 89233218CNA000001 for Los Alamos National Laboratory (LANL), which is operated by Triad National Security, LLC for the U.S.  Department of Energy/National Nuclear Security Administration. All rights in the program are reserved by Triad National Security, LLC, and the U.S. Department of Energy/National Nuclear Security Administration. The Government is granted for itself and others acting on its behalf a nonexclusive, paid-up, irrevocable worldwide license in this material to reproduce, prepare derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so.

This program is open source under the [BSD-3 License](LICENSE.md).  Its LANL-internal identifier is C21041.

Acknowledgments
---------------

Thanks to Joe Bates at Singular Computing for substantial help, guidance, and advice throughout the development of this application.

Authors
-------

* Alex R. Long, *along@lanl.gov*
* Scott Pakin, *pakin@lanl.gov*
