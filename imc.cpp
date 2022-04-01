/*
 * Use Nova to emit code for a simple billion-core Monte Carlo simulation.
 */

#include "simple-bcmc.h"
#include <cassert>

// Sample a simple 2-D angle.  (The third dimension is not used for now.)
NovaExpr get_angle()
{
  NovaExpr phi(int_to_approx01(get_random_int())*TWO_PI);
  NovaExpr mu(int_to_approx01(get_random_int())*2.0 - 1.0);
  NovaExpr eta(sqrt(NovaExpr(1.0) - mu*mu));
  NovaExpr angle(0.0, NovaExpr::NovaApeMemVector, 2);
  angle[0] = eta*cos_0_2pi(phi);
  angle[1] = eta*sin_0_2pi(phi);
  return angle;
}


// Return the distance to a boundary.
NovaExpr get_distance_to_boundary(NovaExpr* cross_face,
                                  const NovaExpr& pos,
                                  const NovaExpr& angle,
                                  const NovaExpr& x_cell,
                                  const NovaExpr& y_cell) {
  // Initialize the distance to each edge.
  NovaExpr min_distance(1e-6);
  *cross_face = -1;
  NovaExpr vertices(0.0, NovaExpr::NovaApeMemVector, 4);
  vertices[0] = 0.0;
  vertices[1] = 1.0;
  vertices[2] = 0.0;
  vertices[3] = 1.0;
  NovaExpr distances(0.0, NovaExpr::NovaApeMemVector, 2);
  NovaExpr i(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(i, 0, 1, 1, [&]() {
    NovaExpr angle_sign;
    NovaApeIf(angle[i] < -1.0e-10, [&]() {
      angle_sign = 0;
    }, [&]() {
         angle_sign = 1;
    });
    distances[i] = (vertices[i*2 + angle_sign] - pos[i])/angle[i];
    NovaApeIf(distances[i] < min_distance, [&]() {
      *cross_face = i*2 + angle_sign;
      min_distance = distances[i];
    });
  });

  // In 2D make the cross face 4-7 to signify a double crossing.
  NovaApeIf(distances[0] == distances[1], [&]() {
    NovaApeIf(angle[0] > 1.0e-19 && angle[1] > 1.0e-19, [&]() {
      *cross_face = 4;
    });
    NovaApeIf(angle[0] > 1.0e-19 && angle[1] < 1.0e-19, [&]() {
      *cross_face = 5;
    });
    NovaApeIf(angle[0] < 1.0e-19 && angle[1] > 1.0e-19, [&]() {
      *cross_face = 6;
    });
    NovaApeIf(angle[0] < 1.0e-19 && angle[1] < 1.0e-19, [&]() {
      *cross_face = 6;
    });
  });
  return min_distance;
}

// Emit the entire S1 program to a low-level kernel.
void emit_nova_code(S1State& s1, unsigned long long seed)
{
  // Tell each APE its row and column.
  NovaExpr ape_row, ape_col;
  assign_ape_coords(s1, ape_row, ape_col);

  // Initialize the random-number generator.
  NovaExpr ci(0, NovaExpr::NovaCUVar);      // CU loop variable
  counter_3fry = NovaExpr(0, NovaExpr::NovaApeMemVector, 8);
  NovaCUForLoop(ci, 0, 7, 1,
                [&]() {
                  counter_3fry[ci] = 0;
                });
  key_3fry = NovaExpr(0, NovaExpr::NovaApeMemVector, 8);
  key_3fry[0] = ape_row;
  key_3fry[1] = ape_col;
  for (int i = 2; i < 7; ++i) {
    key_3fry[i] = int(seed&0xFFFF);
    seed >>= 16;
  }

#ifdef XYZZY
  // Temporary
  TraceMessage("Row and column values\n");
  TraceOneRegisterAllApes(ape_row.expr);
  TraceOneRegisterAllApes(ape_col.expr);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Reduction to the CU\n");
  NovaExpr cu_var(0, NovaExpr::NovaCUVar);
  NovaExpr ape_var = ape_col;
  or_reduce_apes_to_cu(s1, cu_var, ape_var);
  TraceRegisterCU(cu_var.expr);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Integer to approx\n");
  NovaExpr some_int(0x4321);
  NovaExpr some_approx = int_to_approx01(some_int);
  TraceOneRegisterOneApe(some_int.expr, 0, 0);
  TraceOneRegisterOneApe(some_approx.expr, 0, 0);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Cosines\n");
  for (double a = 0.0; a <= 6.28318530718; a += 0.1) {
    NovaExpr angle(a);
    NovaExpr cos_val(cos_0_2pi(angle));
    TraceOneRegisterOneApe(angle.expr, 0, 0);
    TraceOneRegisterOneApe(cos_val.expr, 0, 0);
  }
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Threefry\n");
  for (int i = 0; i < 16; ++i) {
    NovaExpr r(get_random_int());
    TraceOneRegisterOneApe(r.expr, 0, 0);
  }
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Square roots\n");
  NovaExpr x(4.0);
  x = sqrt(x);
  TraceOneRegisterOneApe(x.expr, 0, 0);
  x = 2.0;
  x = sqrt(x);
  TraceOneRegisterOneApe(x.expr, 0, 0);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("get_angle()\n");
  NovaExpr angle(0.0, NovaExpr::NovaApeMemVector, 2);
  get_angle(angle);
  TraceOneRegisterOneApe(angle[0].expr, 0, 0);
  TraceOneRegisterOneApe(angle[1].expr, 0, 0);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Arrays\n");
  NovaExpr my_array(0, NovaExpr::NovaApeMemArray, 2, 2);
  my_array[0][0] = ape_row;
  my_array[0][1] = 10;
  my_array[1][0] = 20;
  my_array[1][1] = ape_col;
  TraceOneRegisterOneApe(my_array[0][0].expr, 0, 0);
  TraceOneRegisterOneApe(my_array[0][1].expr, 0, 0);
  TraceOneRegisterOneApe(my_array[1][0].expr, 0, 0);
  TraceOneRegisterOneApe(my_array[1][1].expr, 0, 0);
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Conditionals\n");
  {
    NovaExpr a(2);
    NovaExpr b(3);
    NovaExpr c(4);
    NovaExpr d(5);
    NovaExpr thing;
    NovaApeIf(a < b && c > d, [&]() {
      thing = 123;
    }, [&]() {
      thing = 456;
    });
    TraceOneRegisterOneApe(thing.expr, 0, 0);
  }
#endif

#ifdef XYZZY
  // Temporary
  TraceMessage("Natural log of an integer/65535\n");
  NovaExpr p64k(12345);
  NovaExpr lg = ln_of_int(p64k);
  TraceOneRegisterOneApe(lg.expr, 0, 0);
#endif


  // Define the number of particles.  Because the value is larger than
  // 65535, we split it into A and B such that A*B equals the desired total.
  const int n_particles = 1000;  // Temporary -- should be 1000000;
  const int n_particles_a = 1000;
  const int n_particles_b = n_particles/n_particles_a;
  assert(n_particles_a*n_particles_b == n_particles);
  const double start_weight = 1.0/n_particles; // Starting energy weight of each particle

  // Define various other constants and parameters.
  const double c = 299.792; // speed of light, in cm/shake
  const double dx = 0.01;  // cell size, square, in cm
  const double dt = 0.001; // timestep size, in shakes (1e-8 seconds)
  const double mfp = 0.3; // average distance, in cm, between scattering events
  const double sig_s = 1.0/mfp; // scattering opacity
  const double sig_a = 10.0; // absorption opacity
  const double ratio = dx; // converts real space to [0,1] space
  const int start_x = 10; // 11th x cell
  const int start_y = 10; // 11th y cell
  const int max_x_cell = 21;
  const int max_y_cell = 21;

  // Allocate space for tallies.
  NovaExpr local_tally(0.0, NovaExpr::NovaApeMemArray, max_x_cell, max_y_cell);  // x is the slow dimension
  NovaExpr global_tally(0.0, NovaExpr::NovaCUMemArray, max_x_cell, max_y_cell);

  // Loop over the number of particles, split into two nested loops to work
  // around the 16-bit integer limitation.
  NovaExpr ci1(0, NovaExpr::NovaCUVar);
  NovaExpr ci2(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(ci1, 0, n_particles_a - 1, 1,
    [&]() {
      NovaCUForLoop(ci2, 0, n_particles_b - 1, 1,
        [&]() {
          // Initialize the per-particle work.
          NovaExpr weight(start_weight);
          NovaExpr d_remain(dt*c);  // TODO: Multiply by a random number after census.
          NovaExpr x_cell(start_x);
          NovaExpr y_cell(start_y);
          NovaExpr alive(1);   // Is the current APE alive?
          NovaExpr all_alive(1, NovaExpr::NovaCUVar);  // Are all APEs alive?
          NovaExpr pos(0.0, NovaExpr::NovaApeMemVector, 2);  // Particle position
          pos[0] = 0.5;
          pos[1] = 0.5;
          NovaExpr angle = get_angle();  // Particle angle

          // Iterate until no more particles are alive.
          NovaExpr w_iter(0, NovaExpr::NovaCUVar);
          NovaCUForLoop(w_iter, 0, 1, 1,  // TODO: Loop from 0 to 1 by 0.
            [&]() {
              // Compute the distance the particle will move.
              NovaExpr d_scatter(-ln_of_int(get_random_int())/sig_s/ratio);
              NovaExpr d_absorb(-ln_of_int(get_random_int())/sig_a/ratio);
              NovaExpr cross_face(-1);
	      /*
	      NovaExpr d_boundary =
		get_distance_to_boundary(&cross_face,
					 pos, angle,
					 x_cell, y_cell);
	      */
            });
        });
    });
}
