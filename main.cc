//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   main.cc
 * \author Alex Long
 * \date   January 17 2022
 * \brief  Simplified transport with approxes
 * \note   Copyright (C) 2022 Los Alamos National Security, LLC.
 *         All rights reserved
 */
//---------------------------------------------------------------------------//

#include <cstdlib>
#include <array>
#include <cmath>
#include <iostream>

#include "singem-sidecar.h"

using float_type = scFloatSidecar ;
//using float_type = double ;

// simple class to wrap C++'s basic random class, for now
template <typename T>
struct RNG {

  RNG(int seed) {
    std::srand(seed);
  }
  T random() {
    return T(static_cast<double>(rand()+1))/T(static_cast<double>(RAND_MAX) + 1.0);
  }
};

// sample a simple 2D angle (third dimension not used for now)
template <typename T>
std::array<T, 2> get_angle(RNG<T> &rng) {
  const T pi = T(3.1415926535);
  T phi = T(2.0) * pi * rng.random();
  T mu =  T(2.0) *  rng.random()- T(1.0);
  T eta = sqrt(T(1.0) - mu*mu);
  return  {eta*T(cos(phi)), eta*T(sin(phi))};

}

// sample position in 1x1 cell
template <typename T>
std::array<T, 2> sample_pos(RNG<T> &rng) {
  return  {rng.random(), rng.random()};
}

// get distane to boundary
template <typename T>
T get_distance_to_boundary(int &cross_face, const std::array<T, 2> &pos, const std::array<T,2> &angle,
                           int x_cell, int y_cell) {
    // initialize distance to each edge
    T min_distance = 1.0e16;
    cross_face = -1;
    const std::array<T, 4> vertices = {T(0.0) , T(1.0), T(0.0), T(1.0)};
    std::array<T,2> distances;
    for (int i=0; i<2; ++i) {
      int angle_sign = (angle[i] < T(-1.0e-10)) ? 0 : 1;
      distances[i] = (vertices[2*i + angle_sign] - pos[i])/angle[i];
      if (distances[i] < min_distance) {
        cross_face = 2*i + angle_sign;
        min_distance = distances[i];
      }
    }
    // in 2D make the cross face 4-7 to signify a double crossing
    if(distances[0] == distances[1]) {
      if (angle[0] > T(1.0e-19) && angle[1] > T(1.0e-19))
        cross_face=4;
      if (angle[0] > T(1.0e-19) && angle[1] < T(-1.0e-19))
        cross_face=5;
      if (angle[0] < T(-1.0e-19) && angle[1] > T(1.0e-19))
        cross_face=6;
      if (angle[0] < T(-1.0e-19) && angle[1] < T(-1.0e-19))
        cross_face=7;
      std::cout<<"Double cross: "<<cross_face<<std::endl;
    }
    return min_distance;
}

int main(void) {

  // constants
  const float_type c= float_type(299.792); // speed of light, in cm/shake

  // problem parameters
  const float_type dx = float_type(0.01);  // cell size, square, in cm
  const float_type dt = float_type(0.0005); // timestep size, in shakes (1e-8 seconds)
  const float_type mfp = float_type(.3); // average distance, in cm, between scattering events
  const float_type sig_s = float_type(1.0)/mfp; // scattering opacity
  const float_type ratio = dx; // converts real space to [0,1] space
  const int n_particles = 20;
  const int n_census = static_cast<int>(n_particles*1.0); // half particles are at time = 0.0
  constexpr int start_x = 10; // 10th x cell
  constexpr int start_y = 10; // 10th y cell
  constexpr int max_x_cell = 20;
  constexpr int max_y_cell = 20;
  RNG <float_type>rng(777);

  for (int i =0; i<n_particles; ++i) {

    //------- HOST SIDE COMPUTATION ---------//
    bool alive = true;
    bool census = false;
    std::array<float_type,2> pos = {float_type(0.5), float_type(0.5)};
    std::array<float_type,2> angle = get_angle<float_type>(rng);

    if (angle[0] != angle[0])
      std::cout<<"angle 0 is NaN!"<<std::endl;
    if (angle[1] != angle[1])
      std::cout<<"angle 1 is NaN!"<<std::endl;
    if (angle[0] > float_type(1.0) || angle[1] > float_type(1.0)) {
      std::cout<<"Warning angle over 1: "<<angle[0]<<" "<<angle[1]<<" Resampling..."<<std::endl;
      angle = get_angle<float_type>(rng);
    }

    float_type d_remain;
    // census particles are born at time 0, more poisson distribution
    if (i<n_census)
      d_remain = dt*c;
    // emission particles born throughout timestep, flatter distribution of event counts
    else
      d_remain = dt*c*rng.random();

    int event_count = 0;
    int x_cell = start_x;
    int y_cell = start_y;
    //----------- DEVICE SIDE COMPUTATION ----------//
    while(alive) {
      event_count=event_count+1;

      float_type d_scatter = ( float_type( -log(rng.random()))/sig_s)/ratio;
      int cross_face = -1;
      float_type d_boundary =get_distance_to_boundary(cross_face, pos, angle, x_cell, y_cell);
      float_type d_census = d_remain/ratio;
      float_type d_move = std::min(d_boundary, std::min(d_scatter, d_census));

      //std::cout<<"angle: "<<angle[0]<<", "<<angle[1]<<std::endl;
      //std::cout<<"x_cell, y_cell: "<<x_cell<<" ,"<<y_cell<<" x_pos, y_pos: "<<pos[0]<<", "<<pos[1]<<std::endl;
      //std::cout<<"x_cell, y_cell: "<<x_cell<<" ,"<<y_cell<<" x_pos, y_pos: "<<pos[0]<<", "<<pos[1]<<std::endl;
      //std::cout<<" d_scatter: "<<d_scatter<<" d_bound: "<<d_boundary<<" d_census: "<<d_census<<std::endl;
      // move, subtract distance remaining
      pos[0] = pos[0] + angle[0]*d_move;
      pos[1] = pos[1] + angle[1]*d_move;
      // reduce distace to census, use real distance
      d_remain = d_remain - d_move*ratio;

      // process census event
      if( d_move == d_census) {
        alive=false;
        census=true;
      }
      else if(d_move == d_scatter)
        angle = get_angle<float_type>(rng);
      else if(d_move == d_boundary) {
        if (cross_face == 0) {
          x_cell = x_cell - 1;
          pos[0] = 1.0;
        }
        if (cross_face == 1) {
          x_cell = x_cell + 1;
          pos[0] = 0.0;
        }
        if (cross_face == 2) {
          y_cell = y_cell - 1;
          pos[1] = 1.0;
        }
        if (cross_face == 3) {
          y_cell = y_cell + 1;
          pos[1] = 0.0;
        }
        // special event for double crossing: +x +y
        if (cross_face == 4) {
          x_cell = x_cell + 1;
          y_cell = y_cell + 1;
          pos[0] = 0.0;
          pos[1] = 0.0;
        }
        // special event for double crossing: +x, -y
        if (cross_face == 5) {
          x_cell = x_cell + 1;
          y_cell = y_cell - 1;
          pos[0] = 0.0;
          pos[1] = 1.0;
        }
        // special event for double crossing: -x, +y
        if (cross_face == 6) {
          x_cell = x_cell - 1;
          y_cell = y_cell + 1;
          pos[0] = 1.0;
          pos[1] = 0.0;
        }
        // special event for double crossing: -x, -y
        if (cross_face == 7) {
          x_cell = x_cell - 1;
          y_cell = y_cell - 1;
          pos[0] = 1.0;
          pos[1] = 1.0;
        }
        // check if particle exits domain
        if (x_cell > max_x_cell || x_cell < 0)
          alive= false;
        if (y_cell > max_y_cell || y_cell < 0)
          alive = false;
      } // if event == boundary
    } // while(alive)
    std::cout<<"Particle "<<i<<" , event count: "<<event_count<<" census: "<<census<<std::endl;
  } // for(int i=0;i<n_particles;++i)

  return 0;
}

