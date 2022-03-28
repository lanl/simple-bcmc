#! /usr/bin/env python

#######################################
# Generate code for approximating a   #
# function with Chebyshev polynomials #
#                                     #
# By Scott Pakin <pakin@lanl.gov>     #
#######################################

# Shout out to https://www.embeddedrelated.com/showarticle/152.php

import argparse
from math import *
import sys

# Parse the command line.
parser = argparse.ArgumentParser(description='Generate code for function approximation.')
parser.add_argument('function', metavar='FUNC',
                    help='Function of x to approximate, as a Python expression')
parser.add_argument('--name', default='approximate',
                    help='Name of the approximation function to use in generated code')
parser.add_argument('--from', dest='lb', type=float, default=0.0,
                    help='Lower bound of the expected input range')
parser.add_argument('--to', dest='ub', type=float, default=1.0,
                    help='Upper bound of the expected input range')
parser.add_argument('-n', '--num-polys', type=int, default=5,
                    help='Number of Chebyshev polynomials to consider (i.e., approximation accuracy)')
parser.add_argument('-o', '--output', default=sys.stdout,
                    type=argparse.FileType('w', encoding='utf-8'),
                    help='File to which to write the generated code')
parser.add_argument('--generate', default='c', choices=['c', 'c++', 'nova'],
                    help='Type of code to generate (C, C++, or Nova)')
parser.add_argument('--datatype', default='double',
                    help='C numeric datatype to use when generating C code')
cl_args = parser.parse_args()
fname = cl_args.name
func = eval('lambda x: %s' % cl_args.function)
n = cl_args.num_polys
a = cl_args.lb
b = cl_args.ub
dtype = cl_args.datatype

# Find the Chebyshev nodes.
us = []
for i in range(1, n + 1):
    us.append(cos(pi*(2*i - 1)/(2*n)))
us.sort()
xs = [u*(b - a)/2 + (b + a)/2 for u in us]
ys = [func(x) for x in xs]

def cheb_poly(i, x):
    return cos(i*acos(x))

# Find the Chebyshev coefficients.
cs = []
cs.append(sum(ys)/n)
uys = list(zip(us, ys))
for i in range(1, n):
    cs.append(2*sum([cheb_poly(i, u)*y for u, y in uys])/n)

def generate_c(w, cs):
    'Generate C code using a given datatype.'
    make_num = lambda x: '%s(%.20g)' % (dtype, x)
    w.write('// Approximate %s on [%.5g, %.5g] using %d Chebyshev polynomials.\n' %
            (cl_args.function, a, b, n))
    w.write('%s %s(%s num)\n' % (dtype, fname, dtype))
    w.write('{\n')
    w.write('  // Scale num from [%.5g, %.5g] to [-1, 1].\n' % (a, b))
    w.write('  num = (%s*num - %s - %s)/(%s - %s);\n' %
            (make_num(2), make_num(a), make_num(b), make_num(b), make_num(a)))
    w.write('\n')
    w.write('  // Instantiate the Chebyshev polynomials.\n')
    w.write('  %s num2 = num*%s;\n' % (dtype, make_num(2)))
    for i in range(n):
        if i == 0:
            w.write('  %s t0 = %s;\n' % (dtype, make_num(1)))
        elif i == 1:
            w.write('  %s t1 = num;\n' % dtype)
        else:
            w.write('  %s t%d = num2*t%d - t%d;\n' % (dtype, i, i - 1, i - 2))
    w.write('\n')
    w.write('  // Compute a linear combination of the Chebyshev polynomials.\n')
    w.write('  %s sum = %s*t0;\n' % (dtype, make_num(cs[0])))
    for i in range(1, n):
        w.write('  sum += %s*t%d;\n' % (make_num(cs[i]), i))
    w.write('  return sum;\n')
    w.write('}\n')

def generate_cpp(w, cs):
    'Generate C++ code using a given datatype.'
    make_num = lambda x: '%s(%.20g)' % (dtype, x)
    w.write('// Approximate %s on [%.5g, %.5g] using %d Chebyshev polynomials.\n' %
            (cl_args.function, a, b, n))
    w.write('%s %s(%s num)\n' % (dtype, fname, dtype))
    w.write('{\n')
    w.write('  // Scale num from [%.5g, %.5g] to [-1, 1].\n' % (a, b))
    w.write('  num = (%s*num - %s - %s)/(%s - %s);\n' %
            (make_num(2), make_num(a), make_num(b), make_num(b), make_num(a)))
    w.write('\n')
    w.write('  // Instantiate the Chebyshev polynomials.\n')
    w.write('  %s num2(num*%s);\n' % (dtype, make_num(2)))
    for i in range(n):
        if i == 0:
            w.write('  %s t0(%s);\n' % (dtype, make_num(1)))
        elif i == 1:
            w.write('  %s t1(num);\n' % dtype)
        else:
            w.write('  %s t%d(num2*t%d - t%d);\n' % (dtype, i, i - 1, i - 2))
    w.write('\n')
    w.write('  // Compute a linear combination of the Chebyshev polynomials.\n')
    w.write('  %s sum(t0*%s);\n' % (dtype, make_num(cs[0])))
    for i in range(1, n):
        w.write('  sum += t%d*%s;\n' % (i, make_num(cs[i])))
    w.write('  return sum;\n')
    w.write('}\n')

def generate_nova(w, cs):
    'Generate Nova code using approxes as the datatype.'
    make_num = lambda x: 'AConst(%.20g)' % x
    w.write('// Approximate %s on [%.5g, %.5g] using %d Chebyshev polynomials.\n' %
            (cl_args.function, a, b, n))
    w.write('void %s(scExpr x, scExpr result)\n' % fname)
    w.write('{\n')
    w.write('  // Scale num from [%.5g, %.5g] to [-1, 1].\n' % (a, b))
    w.write('  DeclareApeVar(num, Approx);\n')
    w.write('  Set(num, Div(Sub(Mul(%s, x), %s), %s))\n' %
            (make_num(2), make_num(a + b), make_num(b - a)))
    w.write('\n')
    w.write('  // Instantiate the Chebyshev polynomials.\n')
    w.write('  DeclareApeVar(num2, Approx);\n')
    w.write('  Set(num2, Mul(num, %s));\n' % make_num(2))
    t = [make_num(1), 'num'] + ['t%d' % i for i in range(2, n)]
    for i in range(2, n):
        w.write('  DeclareApeVar(t%d, Approx);\n' % i)
        w.write('  Set(%s, Sub(Mul(num2, %s), %s));\n' %
                (t[i], t[i - 1], t[i - 2]))
    w.write('\n')
    w.write('  // Compute a linear combination of the Chebyshev polynomials.\n')
    w.write('  DeclareApeVar(sum, Approx);\n')
    w.write('  Set(sum, %s);\n' % make_num(cs[0]))
    for i in range(1, n):
        w.write('  Set(sum, Add(sum, Mul(%s, %s)));\n' % (make_num(cs[i]), t[i]))
    w.write('  Set(result, sum);\n')
    w.write('}\n')

# Write a function to a file.
if cl_args.generate == 'c':
    generate_c(cl_args.output, cs)
elif cl_args.generate == 'c++':
    generate_cpp(cl_args.output, cs)
elif cl_args.generate == 'nova':
    generate_nova(cl_args.output, cs)
else:
    sys.exit('Unexpected generation type %s' % repr(cl_args.generate))
