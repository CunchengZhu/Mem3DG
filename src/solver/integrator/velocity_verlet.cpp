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
#include <csignal>
#include <iostream>
#include <math.h>
#include <pcg_random.hpp>

#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

#include "mem3dg/meshops.h"
#include "mem3dg/solver/integrator/integrator.h"
#include "mem3dg/solver/integrator/velocity_verlet.h"
#include "mem3dg/solver/mesh_process.h"
#include "mem3dg/solver/system.h"
#include "mem3dg/type_utilities.h"

namespace mem3dg {
namespace solver {
namespace integrator {
namespace gc = ::geometrycentral;

bool VelocityVerlet::integrate() {
  if (ifDisableIntegrate)
    mem3dg_runtime_error("integrate() is disabled for current construction!");

  signal(SIGINT, signalHandler);

  double initialTime = system.time, lastUpdateGeodesics = system.time,
         lastProcessMesh = system.time, lastComputeAvoidingForce = system.time,
         lastSave = system.time;

  // initialize netcdf traj file
#ifdef MEM3DG_WITH_NETCDF
  if (ifOutputTrajFile) {
    createMutableNetcdfFile(isContinuation);
    if (ifPrintToConsole)
      std::cout << "Initialized NetCDF file at "
                << outputDirectory + "/" + trajFileName << std::endl;
  }
#endif

  // time integration loop
  for (;;) {

    // Evaluate and threhold status data
    status();

    // Save files every tSave period and print some info
    if (system.time - lastSave >= savePeriod || system.time == initialTime ||
        EXIT) {
      lastSave = system.time;
      saveData(ifOutputTrajFile, ifOutputMeshFile, ifPrintToConsole);
    }

    // Process mesh every tProcessMesh period
    if (system.time - lastProcessMesh > processMeshPeriod) {
      lastProcessMesh = system.time;
      system.mutateMesh();
      system.updateConfigurations();
    }

    // update geodesics every tUpdateGeodesics period
    if (system.time - lastUpdateGeodesics > updateGeodesicsPeriod) {
      lastUpdateGeodesics = system.time;
      if (system.parameters.point.isFloatVertex)
        system.findFloatCenter(
            *system.vpg, system.geodesicDistance,
            3 * system.vpg->edgeLength(
                    system.center.nearestVertex().halfedge().edge()));
      system.updateGeodesicsDistance();
      if (system.parameters.protein.ifPrescribe)
        system.prescribeGeodesicProteinDensityDistribution();
      system.updateConfigurations();
    }

    // break loop if EXIT flag is on
    if (EXIT) {
      break;
    }

    // step forward
    if (system.time == lastProcessMesh || system.time == lastUpdateGeodesics) {
      system.time += 1e-10 * characteristicTimeStep;
    } else {
      march();
    }
  }

#ifdef MEM3DG_WITH_NETCDF
  if (ifOutputTrajFile) {
    closeMutableNetcdfFile();
    if (ifPrintToConsole)
      std::cout << "Closed NetCDF file" << std::endl;
  }
#endif

  // return if optimization is sucessful
  if (!SUCCESS && ifOutputTrajFile) {
    std::string filePath = outputDirectory;
    filePath.append("/");
    filePath.append(trajFileName);
    markFileName(filePath, "_failed", ".");
  }

  return SUCCESS;
}

void VelocityVerlet::checkParameters() {
  // system.meshProcessor.meshMutator.summarizeStatus();
  // if (system.meshProcessor.isMeshMutate) {
  //   mem3dg_runtime_error(
  //       "Mesh mutations are currently not supported for Velocity Verlet!");
  // }
}

void VelocityVerlet::status() {
  // exit if under error tol
  if (system.mechErrorNorm < tolerance && system.chemErrorNorm < tolerance) {
    if (ifPrintToConsole)
      std::cout << "\nError norm smaller than tol." << std::endl;
    EXIT = true;
  }

  // exit if reached time
  if (system.time > totalTime) {
    if (ifPrintToConsole)
      std::cout << "\nReached time." << std::endl;
    EXIT = true;
  }

  // compute the free energy of the system
  if (system.parameters.external.Kf != 0)
    system.computeExternalWork(system.time, timeStep);
  system.computeTotalEnergy();

  // check finiteness
  if (!std::isfinite(timeStep) || !system.checkFiniteness()) {
    EXIT = true;
    SUCCESS = false;
    if (!std::isfinite(timeStep))
      mem3dg_runtime_message("time step is not finite!");
  }

  // check energy increase
  if (isCapEnergy) {
    if (system.energy.totalEnergy - system.energy.proteinInteriorPenalty >
        1.05 * initialTotalEnergy) {
      if (ifPrintToConsole)
        std::cout << "\nVelocity Verlet: increasing system energy, simulation "
                     "stopped! E_total="
                  << system.energy.totalEnergy -
                         system.energy.proteinInteriorPenalty
                  << ", E_init=" << initialTotalEnergy << " (w/o inPE)"
                  << std::endl;
      EXIT = true;
      SUCCESS = false;
    }
  }
}

void VelocityVerlet::march() {
  // adjust time step if adopt adaptive time step based on mesh size
  if (ifAdaptiveStep) {
    characteristicTimeStep = getAdaptiveCharacteristicTimeStep();
    timeStep = characteristicTimeStep;
  }

  double hdt = 0.5 * timeStep, hdt2 = hdt * timeStep;

  // stepping on vertex position
  system.vpg->inputVertexPositions +=
      system.velocity * timeStep + hdt2 * pastMechanicalForceVec;

  // velocity predictor
  gc::VertexData<gc::Vector3> oldVelocity(*system.mesh);
  oldVelocity = system.velocity;
  system.velocity += hdt * pastMechanicalForceVec;

  // compute summerized forces
  system.computePhysicalForcing(timeStep);

  // stepping on velocity
  system.velocity =
      oldVelocity +
      (pastMechanicalForceVec + system.forces.mechanicalForceVec) * hdt;
  pastMechanicalForceVec = system.forces.mechanicalForceVec;

  // stepping on time
  system.time += timeStep;

  // time stepping on protein density
  if (system.parameters.variation.isProteinVariation) {
    system.proteinVelocity = system.parameters.proteinMobility *
                             system.forces.chemicalPotential /
                             system.vpg->vertexDualAreas;
    system.proteinDensity += system.proteinVelocity * timeStep;
  }

  // regularization
  if (system.meshProcessor.isMeshRegularize) {
    system.computeRegularizationForce();
    system.vpg->inputVertexPositions.raw() +=
        system.forces.regularizationForce.raw();
  }

  // recompute cached values
  system.updateConfigurations();
}
} // namespace integrator
} // namespace solver
} // namespace mem3dg
