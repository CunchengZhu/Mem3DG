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

#pragma once

#include <cstddef>
#include <geometrycentral/surface/geometry.h>
#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/rich_surface_mesh_data.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

#include "Eigen/src/Core/util/Constants.h"
#include "mem3dg/meshops.h"
#include "mem3dg/solver/integrator/integrator.h"
#include "mem3dg/solver/system.h"

#include "mem3dg/meshops.h"
#include "mem3dg/solver/mutable_trajfile.h"
#include "mem3dg/solver/trajfile.h"

#include <csignal>
#include <stdexcept>

namespace mem3dg {
namespace solver {
namespace integrator {

// ==========================================================
// =============        Integrator             ==============
// ==========================================================
class DLL_PUBLIC Integrator {
public:
  // variables (read-only)
  /// initial maximum force
  double initialMaximumForce;
  /// ratio of time step to the squared mesh size
  double dt_size2_ratio;
  /// Flag of success of the simulation
  bool SUCCESS = true;
  /// Flag for terminating the simulation
  bool EXIT = false;
  /// time step
  double timeStep;
  /// System object to be integrated
  System &system;
  /// TrajFile
#ifdef MEM3DG_WITH_NETCDF
  MutableTrajFile mutableTrajFile;
#endif

  // key parameters (read/write)
  /// characterisitic time step
  double characteristicTimeStep;
  // total simulation time
  double totalTime;
  /// period of saving output data
  double savePeriod;
  /// tolerance for termination
  double tolerance;
  /// option to scale time step according to mesh size
  bool ifAdaptiveStep = true;
  /// path to the output directory
  std::string outputDirectory;

  // defaulted parameters (read/write)
  /// period of saving output data
  double updateGeodesicsPeriod;
  /// period of saving output data
  double processMeshPeriod;
  /// if just save geometry .ply file
  bool ifJustGeometryPly = false;
  /// if output netcdf traj file
  bool ifOutputTrajFile = false;
  /// if output .ply file
  bool ifOutputMeshFile = false;
  /// if print to console
  bool ifPrintToConsole = false;
  /// name of the trajectory file
  std::string trajFileName = "traj.nc";

  // ==========================================================
  // =============        Constructor            ==============
  // ==========================================================
  /**
   * @brief Construct a new integrator object
   * @param f, System object to be integrated
   * @param dt_, characteristic time step
   * @param total_time, total simulation time
   * @param tSave, period of saving output data
   * @param tolerance, tolerance for termination
   * @param outputDir, path to the output directory
   */
  Integrator(System &system_, double characteristicTimeStep_, double totalTime_,
             double savePeriod_, double tolerance_,
             std::string outputDirectory_)
      : system(system_), characteristicTimeStep(characteristicTimeStep_),
        totalTime(totalTime_), savePeriod(savePeriod_), tolerance(tolerance_),
        updateGeodesicsPeriod(totalTime_), processMeshPeriod(totalTime_),
        outputDirectory(outputDirectory_), timeStep(characteristicTimeStep_) {

    // Initialize the timestep-meshsize ratio
    dt_size2_ratio = characteristicTimeStep /
                     std::pow(system.vpg->edgeLengths.raw().minCoeff(), 2);

    // Initialize the initial maxForce
    system.computePhysicalForcing(timeStep);
    initialMaximumForce =
        system.parameters.variation.isShapeVariation
            ? toMatrix(system.forces.mechanicalForce).cwiseAbs().maxCoeff()
            : system.forces.chemicalPotential.raw().cwiseAbs().maxCoeff();
  }

  /**
   * @brief Destroy the Integrator
   *
   */
  ~Integrator() {}

  // ==========================================================
  // =================   Template functions    ================
  // ==========================================================
  virtual bool integrate() = 0;
  virtual void march() = 0;
  virtual void status() = 0;
  virtual void checkParameters() = 0;

  // ==========================================================
  // =================     Output Data         ================
  // ==========================================================
  /**
   * @brief Save trajectory, mesh and print to console
   */
  void saveData(bool ifTrajFile, bool ifMeshFile, bool ifPrint);

#ifdef MEM3DG_WITH_NETCDF
  /**
   * @brief Initialize netcdf traj file
   */
  void createMutableNetcdfFile(bool isContinue);

  /**
   * @brief close netcdf traj file
   */
  void closeMutableNetcdfFile();

  /**
   * @brief Save data to netcdf traj file
   */
  void saveMutableNetcdfData();

#endif

  // ==========================================================
  // =============     Helper functions          ==============
  // ==========================================================

  /**
   * @brief Backtracking algorithm that dynamically adjust step size based on
   * energy evaluation
   * @param positionDirection, direction of shape, most likely some function of
   * gradient
   * @param chemicalDirection, direction of protein density, most likely some
   * function of gradient
   * @param rho, discount factor
   * @param c1, constant for Wolfe condtion, between 0 to 1, usually ~ 1e-4
   * @return alpha, line search step size
   */
  double backtrack(Eigen::Matrix<double, Eigen::Dynamic, 3> &&positionDirection,
                   Eigen::Matrix<double, Eigen::Dynamic, 1> &&chemicalDirection,
                   double rho = 0.7, double c1 = 0.001);

  /**
   * @brief Backtracking algorithm that dynamically adjust step size based on
   * energy evaluation
   * @param positionDirection, direction of shape, most likely some function of
   * gradient
   * @param rho, discount factor
   * @param c1, constant for Wolfe condtion, between 0 to 1, usually ~ 1e-4
   * @return alpha, line search step size
   */
  double mechanicalBacktrack(
      Eigen::Matrix<double, Eigen::Dynamic, 3> &&positionDirection,
      double rho = 0.7, double c1 = 0.001);

  /**
   * @brief Backtracking algorithm that dynamically adjust step size based on
   * energy evaluation
   * @param chemicalDirection, direction of protein density, most likely some
   * function of gradient
   * @param rho, discount factor
   * @param c1, constant for Wolfe condtion, between 0 to 1, usually ~ 1e-4
   * @return alpha, line search step size
   */
  double chemicalBacktrack(
      Eigen::Matrix<double, Eigen::Dynamic, 1> &&chemicalDirection,
      double rho = 0.7, double c1 = 0.001);

  /**
   * @brief Check finiteness of simulation states and backtrack for error in
   * specific component
   * @return
   */
  void finitenessErrorBacktrace();

  /**
   * @brief Backtrack the line search failure by inspecting specific
   * energy-force relation
   * @return
   */
  void lineSearchErrorBacktrace(const double alpha,
                                const EigenVectorX3dr initial_pos,
                                const EigenVectorX1d init_proteinDensity,
                                const Energy previousE, bool runAll = false);

  /**
   * @brief get adaptive characteristic time step
   * @return
   */
  double getAdaptiveCharacteristicTimeStep();
};
} // namespace integrator
} // namespace solver
} // namespace mem3dg
