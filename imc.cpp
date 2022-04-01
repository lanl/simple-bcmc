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
    distances[i] = (vertices[angle_sign + i + i] - pos[i])/angle[i];
    NovaApeIf(distances[i] < min_distance, [&]() {
      *cross_face = angle_sign + i + i;
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
  // The following were reduced from the original to fit in the S1's memory.
  const int start_x = 9; // 10th x cell
  const int start_y = 9; // 10th y cell
  const int max_x_cell = 19;
  const int max_y_cell = 19;

  // Allocate space for tallies, and initialize all tallies to zero.
  NovaExpr local_tally(0.0, NovaExpr::NovaApeMemArray, max_x_cell, max_y_cell);
 // x is the slow dimension
  NovaExpr global_tally(0.0, NovaExpr::NovaCUMemArray, max_x_cell, max_y_cell);
  NovaExpr x_iter(0, NovaExpr::NovaCUVar);
  NovaExpr y_iter(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(x_iter, 0, max_x_cell - 1, 1, [&]() {
    NovaCUForLoop(y_iter, 0, max_y_cell - 1, 1, [&]() {
      global_tally[x_iter][y_iter] = 0.0;
      local_tally[x_iter][y_iter] = 0.0;
    });
  });

  // Loop over the number of particles, split into two nested loops to work
  // around the 16-bit integer limitation.
  NovaExpr ci1(0, NovaExpr::NovaCUVar);
  NovaExpr ci2(0, NovaExpr::NovaCUVar);
  NovaCUForLoop(ci1, 0, n_particles_a - 1, 1, [&]() {
    NovaCUForLoop(ci2, 0, n_particles_b - 1, 1, [&]() {
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
      NovaCUForLoop(w_iter, 0, 1, 0, [&]() {  // while (alive) {...}
        // Compute the distance the particle will move.
        NovaExpr d_scatter(-ln_of_int(get_random_int())/sig_s/ratio);
        NovaExpr d_absorb(-ln_of_int(get_random_int())/sig_a/ratio);
        NovaExpr cross_face(-1);
        NovaExpr d_boundary =
          get_distance_to_boundary(&cross_face,
                                   pos, angle,
                                   x_cell, y_cell);
        NovaExpr d_census(d_remain/ratio);
        NovaExpr d_move = ape_min(d_boundary,
                                  ape_min(d_census,
                                          ape_min(d_scatter,
                                                  d_absorb)));

        // Move the particle, subtracting the distance remaining.
        pos[0] += angle[0]*d_move;
        pos[1] += angle[1]*d_move;

        // Reduce the distance to census, using the real distance.
        d_remain -= d_move*ratio;

        // Process the event.
        NovaApeIf (d_move == d_census, [&]() {
          alive = false;
        }, [&]() {
          NovaApeIf (d_move == d_absorb, [&]() {
            alive = false;
            local_tally[x_cell][y_cell] += weight;
          }, [&]() {
            NovaApeIf (d_move == d_scatter, [&]() {
              angle = get_angle();
            }, [&]() {
              NovaApeIf (d_move == d_boundary, [&]() {
                NovaApeIf (cross_face == 0, [&]() {
                  --x_cell;
                  pos[0] = 1.0;
                });
                NovaApeIf (cross_face == 1, [&]() {
                  ++x_cell;
                  pos[0] = 0.0;
                });
                NovaApeIf (cross_face == 2, [&]() {
                  --y_cell;
                  pos[1] = 1.0;
                });
                NovaApeIf (cross_face == 3, [&]() {
                  ++y_cell;
                  pos[1] = 1.0;
                });
                // Special event for double crossing: +x +y
                NovaApeIf (cross_face == 4, [&]() {
                  ++x_cell;
                  ++y_cell;
                  pos[0] = 0.0;
                  pos[1] = 0.0;
                });
                // Special event for double crossing: +x -y
                NovaApeIf (cross_face == 5, [&]() {
                  ++x_cell;
                  --y_cell;
                  pos[0] = 0.0;
                  pos[1] = 1.0;
                });
                // Special event for double crossing: -x, +y
                NovaApeIf (cross_face == 6, [&]() {
                  --x_cell;
                  ++y_cell;
                  pos[0] = 1.0;
                  pos[1] = 0.0;
                });
                // Special event for double crossing: -x, -y
                NovaApeIf (cross_face == 7, [&]() {
                  --x_cell;
                  --y_cell;
                  pos[0] = 1.0;
                  pos[1] = 1.0;
                });
                // Check if the particle exited the domain.
                NovaApeIf (x_cell >= max_x_cell || x_cell < 0, [&]() {
                  alive = false;
                });
                NovaApeIf (y_cell >= max_y_cell || y_cell < 0, [&]() {
                  alive = false;
                });
              });  // Event == boundary
            });  // Event == scatter
          });  // Event == absorb
        });  // Event == census

        // Determine if any APE is still alive.
        or_reduce_apes_to_cu(s1, &all_alive, alive);
        NovaApeIf (all_alive == 0, [&]() {
          // No APE is alive; exit the while loop.
          w_iter++;
        });
      });  // while (alive)
    });  // Loop over n_particles (part 2)
  });  // Loop over n_particles (part 1)

  // TODO: Accumulate all local tallies back into the CU's global tallies.
  NovaCUForLoop(x_iter, 0, max_x_cell - 1, 1, [&]() {
    NovaCUForLoop(y_iter, 0, max_y_cell - 1, 1, [&]() {
      TraceOneRegisterAllApes(local_tally[x_iter][y_iter].expr);
    });
  });
}
