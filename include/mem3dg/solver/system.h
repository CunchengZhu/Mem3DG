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

// #include <cassert>

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/manifold_surface_mesh.h"
#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/heat_method_distance.h>
#include <geometrycentral/surface/intrinsic_geometry_interface.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/rich_surface_mesh_data.h>
#include <geometrycentral/surface/surface_mesh.h>
#include <geometrycentral/surface/vertex_position_geometry.h>

#include "geometrycentral/utilities/vector2.h"
#include "geometrycentral/utilities/vector3.h"
#include <geometrycentral/utilities/eigen_interop_helpers.h>

#include <Eigen/Core>
#include <Eigen/SparseLU>

#include <pcg_random.hpp>
#include <random>

#include <functional>
#include <iomanip>
#include <math.h>
#include <vector>

#include "mem3dg/constants.h"
#include "mem3dg/macros.h"
#include "mem3dg/mesh_io.h"
#include "mem3dg/meshops.h"
#include "mem3dg/type_utilities.h"

#include "mem3dg/solver/forces.h"
#include "mem3dg/solver/geometry.h"
#include "mem3dg/solver/mesh_process.h"
#include "mem3dg/solver/parameters.h"
#ifdef MEM3DG_WITH_NETCDF
#include "mem3dg/solver/mutable_trajfile.h"
#include "mem3dg/solver/trajfile.h"
#endif

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

namespace mem3dg {

namespace solver {

struct Energy {
  /// time
  double time = 0;
  /// total Energy of the system
  double totalEnergy = 0;
  /// kinetic energy of the membrane
  double kineticEnergy = 0;
  /// potential energy of the membrane
  double potentialEnergy = 0;
  /// spontaneous curvature energy of the membrane
  double spontaneousCurvatureEnergy = 0;
  /// deviatoric curvature energy of the membrane
  double deviatoricCurvatureEnergy = 0;
  /// area difference energy of the membrane
  double areaDifferenceEnergy = 0;
  /// stretching energy of the membrane
  double surfaceEnergy = 0;
  /// work of pressure within membrane
  double pressureEnergy = 0;
  /// adsorption energy of the membrane protein
  double adsorptionEnergy = 0;
  /// aggregation energy of the membrane protein
  double aggregationEnergy = 0;
  /// entropy energy of the membrane protein
  double entropyEnergy = 0;
  /// line tension energy of interface
  double dirichletEnergy = 0;
  /// work of external force
  double externalWork = 0;
  /// protein interior penalty energy
  double proteinInteriorPenalty = 0;
  /// membrane self-avoidance penalty energy
  double selfAvoidancePenalty = 0;
  /// mesh edge spring energy
  double edgeSpringEnergy = 0;
  /// mesh face spring energy
  double faceSpringEnergy = 0;
  /// mesh LCR spring energy
  double lcrSpringEnergy = 0;
};

class DLL_PUBLIC System {
protected:
  /// Random number engine
  pcg32 rng;
  std::normal_distribution<double> normal_dist;

public:
  /// Parameters
  Parameters parameters;
  /// Mesh processor
  MeshProcessor meshProcessor;
  /// Geometry
  Geometry &geometry;
  /// Energy
  Energy energy;
  /// Time
  double time;
  /// Forces of the system
  Forces forces;

  /// mechanical error norm
  double mechErrorNorm;
  /// chemical error norm
  double chemErrorNorm;
  /// Cached protein surface density
  gcs::VertexData<double> proteinDensity;
  /// Spontaneous curvature gradient of the mesh
  gcs::FaceData<gc::Vector3> proteinDensityGradient;
  /// Cached vertex velocity
  gcs::VertexData<gc::Vector3> velocity;
  /// Cached vertex protein rate of change
  gcs::VertexData<double> proteinRateOfChange;
  /// Spontaneous curvature of the mesh
  gcs::VertexData<double> H0;
  /// Bending rigidity of the membrane
  gcs::VertexData<double> Kb;
  /// deviatoric rigidity of the membrane
  gcs::VertexData<double> Kd;
  /// is Smooth
  bool isSmooth;
  /// if being mutated
  gcs::VertexData<bool> mutationMarker;
  /// projected time of collision
  double projectedCollideTime;

  // =======================================
  // =======       NetCDF Files     ========
  // =======================================
#ifdef MEM3DG_WITH_NETCDF
  System(Geometry &geometry_, std::string trajFile, int startingFrame,
         Parameters &p)
      : System(geometry_, readTrajFile(trajFile, startingFrame), p){};
  // System(std::string trajFile, int startingFrame, Parameters &p)
  //     : System(readTrajFile(trajFile, startingFrame), p){};
#endif

private:
  // =======================================
  // =======       Tuple            ========
  // =======================================
  //   /**
  //    * @brief Construct System
  //    * @param initialConditionTuple <geometry of the system, vertex protein
  //    * density, velocity of the vertex, time of the system>
  //    * @param p Parameters of the system
  //    * @return system instance
  //    *
  //    */
  //   System(std::tuple<Geometry &&, EigenVectorX1d &, EigenVectorX3dr &,
  //   double>
  //              initialConditionsTuple,
  //          Parameters &p)
  //       : System(std::get<0>(initialConditionsTuple),
  //                std::get<1>(initialConditionsTuple),
  //                std::get<2>(initialConditionsTuple), p,
  //                std::get<3>(initialConditionsTuple)){};
  //   /**
  //    * @brief Construct System
  //    * @param initialConditionTuple <geometry of the system, vertex protein
  //    * density, velocity of the vertex, time of the system>
  //    * @return system instance
  //    *
  //    */
  //   System(std::tuple<Geometry &&, EigenVectorX1d &&, EigenVectorX3dr &&,
  //   double>
  //              initialConditionsTuple)
  //       : System(std::get<0>(initialConditionsTuple),
  //                std::get<1>(initialConditionsTuple),
  //                std::get<2>(initialConditionsTuple),
  //                std::get<3>(initialConditionsTuple)){};

  // // below is temperary
  //   System(std::tuple<std::unique_ptr<Geometry>, EigenVectorX1d,
  //   EigenVectorX3dr,
  //                     double>
  //              initialConditionsTuple,
  //          Parameters &p)
  //       : System(*std::move(std::get<0>(initialConditionsTuple)),
  //                std::get<1>(initialConditionsTuple),
  //                std::get<2>(initialConditionsTuple), p,
  //                std::get<3>(initialConditionsTuple)){};

  //   System(std::unique_ptr<Geometry> geometryptr, EigenVectorX1d
  //   &proteinDensity_,
  //          EigenVectorX3dr &velocity_, Parameters &p, double time_ = 0)
  //       : System(*std::move(geometryptr).get(), proteinDensity_, velocity_,
  //       p,
  //                time_) {}

  System(Geometry &geometry_,
         std::tuple<EigenVectorX1d, EigenVectorX3dr, double> tuple,
         Parameters &p)
      : System(geometry_, std::get<0>(tuple), std::get<1>(tuple), p,
               std::get<2>(tuple)) {}

public:
  // =======================================
  // =======    Geometry Central    ========
  // =======================================

  /**
   * @brief Construct System
   * @param geometry_ geometry of the system
   * @param proteinDensity_ vertex protein density
   * @param velocity_ velocity of the vertex
   * @param p parameters struct of the system
   * @param time_ time of the system
   * @return system instance
   *
   */
  System(Geometry &geometry_, EigenVectorX1d &proteinDensity_,
         EigenVectorX3dr &velocity_, Parameters &p, double time_ = 0)
      : System(geometry_, proteinDensity_, velocity_, time_) {
    parameters = p;
  }

  /**
   * @brief Construct System
   * @param geometry_ geometry of the system
   * @param proteinDensity_ vertex protein density
   * @param velocity_ velocity of the vertex
   * @param time_ time of the system
   * @return system instance
   *
   */
  System(Geometry &geometry_, EigenVectorX1d &proteinDensity_,
         EigenVectorX3dr &velocity_, double time_ = 0)
      : System(geometry_, time_) {
    proteinDensity.raw() = proteinDensity_;
    toMatrix(velocity) = velocity_;
  }

  /**
   * @brief Construct System
   * @param geometry_ geometry of the system
   * @param p parameters struct of the system
   * @param time_ time of the system
   * @return system instance
   *
   */
  System(Geometry &geometry_, Parameters &p, double time_ = 0)
      : System(geometry_, time_) {
    parameters = p;
  }

  /**
   * @brief Construct System
   * @param geometry_ geometry of the system
   * @param time_ time of the system
   * @return system instance
   *
   */
  System(Geometry &geometry_, double time_ = 0)
      : geometry(geometry_), forces(geometry), time(time_) {
    energy = Energy({time, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    proteinDensity = gc::VertexData<double>(*geometry.mesh, 1);
    proteinDensityGradient =
        gcs::FaceData<gc::Vector3>(*geometry.mesh, {0, 0, 0});
    velocity = gcs::VertexData<gc::Vector3>(*geometry.mesh, {0, 0, 0});
    proteinRateOfChange = gcs::VertexData<double>(*geometry.mesh, 0);
    H0 = gcs::VertexData<double>(*geometry.mesh);
    Kb = gcs::VertexData<double>(*geometry.mesh);
    Kd = gcs::VertexData<double>(*geometry.mesh);

    chemErrorNorm = 0;
    mechErrorNorm = 0;

    isSmooth = true;
    mutationMarker = gc::VertexData<bool>(*geometry.mesh, false);
  }

public:
  /**
   * @brief Destroy the System
   *
   * Explicitly unrequire values required by the constructor. In case, there
   * is another pointer to the HalfEdgeMesh and VertexPositionGeometry
   * elsewhere, calculation of dependent quantities should be respected.
   */
  ~System() {}

  // ==========================================================
  // ================          io.cpp        ==================
  // ==========================================================

  /**
   * @brief Map the continuation variables
   *
   */
  void mapContinuationVariables(std::string plyFile);

  /**
   * @brief Save RichData to .ply file
   *
   */
  void saveRichData(std::string PathToSave, bool isJustGeometry = false);

#ifdef MEM3DG_WITH_NETCDF
  /**
   * @brief Construct a tuple of unique_ptrs from netcdf path
   *
   */
  // std::tuple<Geometry &&, EigenVectorX1d &, EigenVectorX3dr &, double>
  // readTrajFile(std::string trajFile, int startingFrame);
  std::tuple<EigenVectorX1d, EigenVectorX3dr, double>
  readTrajFile(std::string trajFile, int startingFrame);
#endif

  // ==========================================================
  // ================     init.cpp           ==================
  // ==========================================================
  /**
   * @brief Check all conflicting parameters and options
   *
   */
  void checkConfiguration();

  /**
   * @brief Initialize system
   *
   */
  void initialize(std::size_t nMutation = 0, bool ifMute = false);

  /**
   * @brief Initialize all constant values (on refVpg) needed for computation
   *
   */
  void initializeConstants(bool ifMute);

  /**
   * @brief Update the vertex position and recompute cached values
   * (all quantities that characterizes the current energy state)
   * Careful: 1. when using eigenMap: memory address may change after update!!
   */
  void updateConfigurations();

  // ==========================================================
  // ================        Force.cpp       ==================
  // ==========================================================
  /**
   * @brief Compute and update all conservative forces, update
   * mechanicalForce(Vec) with conservativeForce(Vec)
   */
  void computeConservativeForcing();

  /**
   * @brief Compute and append all non-conservative forces, update
   * mechanicalForce(Vec) and mechErrorNorm
   */
  void addNonconservativeForcing(double timeStep);

  /**
   * @brief Compute geometric forces, including
   * - spontaneous curvature force
   * - deviatoric curvature force
   * - area difference force
   * - capillary (surface tension) force
   * - osmotic force
   * - line capillary (line tension) force
   * - adsorption (area expansion) force
   * - aggregation (area expansion) force
   * - entropy (area expansion) force
   */
  void computeGeometricForces();
  void computeGeometricForces(size_t i);
  void computeGeometricForces(gcs::Vertex &v);

  /**
   * @brief Compute regularization pressure component of the system
   */
  void computeSpringForces();

  /**
   * @brief Compute Self Avoidance force
   */
  void computeSelfAvoidanceForce();

  /**
   * @brief Compute external force component of the system
   */
  EigenVectorX3dr prescribeExternalForce();

  /**
   * @brief Compute DPD forces of the system
   */
  void computeDPDForces(double dt);

  /**
   * @brief Compute damping forces of the system
   */
  gc::VertexData<gc::Vector3> computeDampingForce();

  /**
   * @brief Compute chemical potential of the system, including
   * - spontaneous curvature potential
   * - adsorption potential
   * - aggregation potential
   * - entropy potential
   * - deviatoric curvature potential
   * - dirichlet potential
   * - interior penalty potential
   */
  void computeChemicalPotentials();

  /**
   * @brief Compute in plane flux form on edge
   */
  EigenVectorX1d computeInPlaneFluxForm(EigenVectorX1d &chemicalPotential);

  // ==========================================================
  // ================        energy.cpp      ==================
  // ==========================================================
  /**
   * @brief Compute spontaneous curvature energy
   */
  void computeSpontaneousCurvatureEnergy();

  /**
   * @brief Compute deviatoric curvature energy
   */
  void computeDeviatoricCurvatureEnergy();

  /**
   * @brief Compute area difference energy
   */
  void computeAreaDifferenceEnergy();

  /**
   * @brief Compute surface energy
   */
  void computeSurfaceEnergy();

  /**
   * @brief Compute pressure work
   */
  void computePressureEnergy();

  /**
   * @brief Compute adsorption energy
   */
  void computeAdsorptionEnergy();

  /**
   * @brief Compute aggregation energy
   */
  void computeAggregationEnergy();

  /**
   * @brief Compute entropy penalty
   */
  void computeEntropyEnergy();

  /**
   * @brief Compute protein interior penalty
   */
  void computeProteinInteriorPenalty();

  /**
   * @brief Compute Dirichlet energy
   */
  void computeDirichletEnergy();

  /**
   * @brief Compute self-avoidance energy
   */
  void computeSelfAvoidanceEnergy();

  /**
   * @brief Compute edge spring energy
   */
  void computeEdgeSpringEnergy();

  /**
   * @brief Compute face spring energy
   */
  void computeFaceSpringEnergy();

  /**
   * @brief Compute LCR spring energy
   */
  void computeLcrSpringEnergy();

  /**
   * @brief Compute external work
   */
  double computeExternalWork(double currentTime, double dt);

  /**
   * @brief Compute kinetic energy
   */
  double computeKineticEnergy();

  /**
   * @brief Compute potential energy
   */
  double computePotentialEnergy();

  /**
   * @brief compute total energy
   */
  double computeTotalEnergy();

  /**
   * @brief Intermediate function to integrate the power
   */
  double computeIntegratedPower(double dt);
  double computeIntegratedPower(double dt, EigenVectorX3dr &&velocity);

  // ==========================================================
  // =============        regularization.cpp    ===============
  // ==========================================================
  /**
   * @brief Mesh mutation
   */
  void mutateMesh(size_t nMutation = 1);

  /**
   * @brief Apply vertex shift by moving the vertices chosen for integration to
   * the Barycenter of the it neighbors
   */
  void vertexShift();

  /**
   * @brief Edge flip if not Delaunay
   */
  bool edgeFlip();
  void flipEdge();

  /**
   * @brief Get regularization pressure component of the system
   */
  bool meshGrowth();
  bool growMesh();

  /**
   * @brief global smoothing after mutation of the mesh
   * @param initStep init guess of time step
   * @param target target reduce of force norm
   * @param maxIteration maximum number of iteration
   */
  Eigen::Matrix<bool, Eigen::Dynamic, 1>
  smoothenMesh(double initStep = 0.01, double target = 0.7,
               size_t maxIteration = 1000);
  /**
   * @brief pointwise smoothing after mutation of the mesh
   */
  void localSmoothing(const gcs::Vertex &v, std::size_t num = 10,
                      double stepSize = 0.01);
  void localSmoothing(const gcs::Halfedge &he, std::size_t num = 10,
                      double stepSize = 0.01);

  /**
   * @brief global update of quantities after mutation of the mesh
   */
  void globalUpdateAfterMutation();

  // ==========================================================
  // =============          misc.cpp            ===============
  // ==========================================================
  /**
   * @brief test conservative force computation by validating energy decrease
   * @return
   */
  bool testConservativeForcing(const double timeStep);

  /**
   * @brief backtrace energy increase from the system perturbation
   * @return
   */
  void backtraceEnergyGrowth(const double timeStep,
                             const Energy previousEnergy);

  /**
   * @brief Check finiteness of forcing and energy
   * @return whether the system is finite
   */
  bool checkFiniteness();

  /**
   * @brief testing of random number generator pcg
   *
   */
  void check_pcg();

  /**
   * @brief update cache of proteinDensity based on geodesicDistance and
   * parameters on profile type
   */
  EigenVectorX1d prescribeProteinDensityDistribution();

  /**
   * @brief prescribe mask based on geodesic disk
   */
  void prescribeGeodesicMasks();

  /**
   * @brief prescribe notable vertex based on parameter.point.form
   */
  Eigen::Matrix<bool, Eigen::Dynamic, 1> prescribeNotableVertex();
};
} // namespace solver
} // namespace mem3dg
