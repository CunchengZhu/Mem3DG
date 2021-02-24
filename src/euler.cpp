// Membrane Dynamics in 3D using Discrete Differential Geometry (Mem3DG)
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// Copyright (c) 2020:
//     Laboratory for Computational Cellular Mechanobiology
//     Cuncheng Zhu (cuzhu@eng.ucsd.edu)
//     Christopher T. Lee (ctlee@ucsd.edu)
//     Ravi Ramamoorthi (ravir@cs.ucsd.edu)
//     Padmini Rangamani (prangamani@eng.ucsd.edu)
//

#include <Eigen/Core>
#include <iostream>
#include <math.h>
#include <pcg_random.hpp>

#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

#include "mem3dg/solver/integrator.h"
#include "mem3dg/solver/meshops.h"
#include "mem3dg/solver/system.h"

namespace mem3dg {
namespace gc = ::geometrycentral;

bool Euler::integrate() {

  signal(SIGINT, signalHandler);

#ifdef __linux__
  // start the timer
  struct timeval start;
  gettimeofday(&start, NULL);
#endif

  // time integration loop
  for (;;) {

    // Evaluate and threhold status data
    status();

    // Save files every tSave period and print some info
    static double lastSave;
    if (f.time - lastSave >= tSave - 1e-12 || f.time == init_time || EXIT) {
      lastSave = f.time;
      saveData();
    }

    // break loop if EXIT flag is on
    if (EXIT) {
      break;
    }

    // step forward
    march();
  }

  // return if optimization is sucessful
  if (!SUCCESS) {
    markFileName("_failed");
  }

  // stop the timer and report time spent
#ifdef __linux__
  double duration = getDuration(start);
  if (verbosity > 0) {
    std::cout << "\nTotal integration time: " << duration << " seconds"
              << std::endl;
  }
#endif

  return SUCCESS;
}

void Euler::checkParameters() {
  if (f.P.gamma != 0) {
    throw std::runtime_error("gamma has to be 0 for euler integration!");
  }
}

void Euler::status() {
  // map the raw eigen datatype for computation
  auto vel_e = gc::EigenMap<double, 3>(f.vel);
  auto pos_e = gc::EigenMap<double, 3>(f.vpg->inputVertexPositions);

  // recompute cached values
  f.updateVertexPositions();

  // compute summerized forces
  getForces();

  // compute velocity
  vel_e = f.M * (physicalPressure + DPDPressure) + regularizationForce;

  // compute the L1 error norm
  f.L1ErrorNorm =
      f.computeL1Norm(vel_e);

  // compute the area contraint error
  dArea = (f.P.Ksg != 0 && !f.mesh->hasBoundary())
              ? abs(f.surfaceArea / f.targetSurfaceArea - 1)
              : 0.0;

  if (f.isReducedVolume) {
    // compute volume constraint error
    dVP = (f.P.Kv != 0 && !f.mesh->hasBoundary())
              ? abs(f.volume / f.refVolume / f.P.Vt - 1)
              : 0.0;
  } else {
    // compute pressure constraint error
    dVP = (!f.mesh->hasBoundary()) ? abs(1.0 / f.volume / f.P.cam - 1) : 1.0;
  }

  // exit if under error tolerance
  if (f.L1ErrorNorm < tol) {
    std::cout << "\nL1 error norm smaller than tolerance." << std::endl;
    EXIT = true;
  }

  // exit if reached time
  if (f.time > total_time) {
    std::cout << "\nReached time." << std::endl;
    EXIT = true;
    SUCCESS = false;
  }

  // compute the free energy of the system
  f.computeFreeEnergy();
}

void Euler::march() {

  // map the raw eigen datatype for computation
  auto vel_e = gc::EigenMap<double, 3>(f.vel);
  auto pos_e = gc::EigenMap<double, 3>(f.vpg->inputVertexPositions);

  // adjust time step if adopt adaptive time step based on mesh size
  if (isAdaptiveStep) {
    double minMeshLength = f.vpg->edgeLengths.raw().minCoeff();
    dt = dt_size2_ratio * minMeshLength * minMeshLength;
  }

  // time stepping on vertex position
  if (isBacktrack) {
    backtrack(rho, c1, EXIT, SUCCESS, f.E.potE, vel_e, vel_e);
  } else {
    pos_e += vel_e * dt;
    f.time += dt;
  }

  // vertex shift for regularization
  if (f.isVertexShift) {
    f.vertexShift();
  }

  // time stepping on protein density
  if (f.isProtein) {
    f.proteinDensity.raw() += -f.P.Bc * f.chemicalPotential.raw() * dt;
  }
}
} // namespace mem3dg
