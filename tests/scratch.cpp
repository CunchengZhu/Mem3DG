#include <iostream>
#ifdef MEM3DG_WITH_NETCDF
#include <netcdf>
#endif

#include "mem3dg/mem3dg"

#include <geometrycentral/surface/halfedge_factories.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/rich_surface_mesh_data.h>
#include <geometrycentral/surface/simple_polygon_mesh.h>
#include <geometrycentral/surface/surface_mesh.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

using EigenVectorX1d = Eigen::Matrix<double, Eigen::Dynamic, 1>;
template <typename T>
using EigenVectorX1_T = Eigen::Matrix<T, Eigen::Dynamic, 1>;
using EigenVectorX1i = Eigen::Matrix<int, Eigen::Dynamic, 1>;
using EigenVectorX3dr =
    Eigen::Matrix<double, Eigen::Dynamic, 3, Eigen::RowMajor>;
using EigenVectorX3ur =
    Eigen::Matrix<std::uint32_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using EigenVectorX3u = Eigen::Matrix<std::uint32_t, Eigen::Dynamic, 3>;
using EigenVectorX3sr =
    Eigen::Matrix<std::size_t, Eigen::Dynamic, 3, Eigen::RowMajor>;
using EigenVectorX3s = Eigen::Matrix<std::size_t, Eigen::Dynamic, 3>;

int main() {

  EigenVectorX3sr mesh;
  EigenVectorX3dr vpg;
  EigenVectorX3dr refVpg;
  mem3dg::solver::Parameters p;
  std::tie(mesh, vpg) = mem3dg::getCylinderMatrix(1, 16, 60, 7.5, 0);
  refVpg = vpg;
  // std::string inputMesh = "/home/cuzhu/Mem3DG/tests/frame9.ply";

  /// physical parameters
  p.proteinMobility = 0;
  p.temperature = 0;

  p.point.index = mem3dg::getVertexClosestToEmbeddedCoordinate(
      vpg, std::array<double, 3>{0, 0, 0},
      std::array<bool, 3>{true, true, false});
  p.protein.proteinInteriorPenalty = 0;

  p.boundary.shapeBoundaryCondition = "fixed";
  p.boundary.proteinBoundaryCondition = "none";

  p.variation.isProteinVariation = false;
  p.variation.isShapeVariation = true;
  p.variation.geodesicMask = -1;

  p.bending.Kb = 0;
  p.bending.Kbc = 2 * 8.22e-5;
  p.bending.H0c = -60;

  p.tension.isConstantSurfaceTension = false;
  p.tension.Ksg = 1;
  p.tension.A_res = 0;
  p.tension.At = 3.40904;
  p.tension.lambdaSG = 0;

  p.adsorption.epsilon = 0;

  p.aggregation.chi = 0;

  p.osmotic.isPreferredVolume = false;
  p.osmotic.isConstantOsmoticPressure = true;
  p.osmotic.Kv = 0.01;
  p.osmotic.V_res = 0;
  p.osmotic.n = 1;
  p.osmotic.Vt = -1;
  p.osmotic.cam = -1;
  p.osmotic.lambdaV = 0;

  p.dirichlet.eta = 0;

  p.selfAvoidance.d = 0.01;
  p.selfAvoidance.mu = 0;
  p.selfAvoidance.p = 0.1;

  p.dpd.gamma = 0;

  p.spring.Kse = 0.01;
  //   p.spring.Ksl = 0.01;
  //   p.spring.Kst = 0.01;

  // mem3dg::solver::System system(mesh, vpg, p, mP, 0);
  EigenVectorX1d phi = Eigen::MatrixXd::Constant(vpg.rows(), 1, 1);
  EigenVectorX3dr vel = Eigen::MatrixXd::Constant(vpg.rows(), 3, 0);

  mem3dg::solver::System system(mesh, vpg, refVpg, phi, vel, p, 0);
  system.initialize();
  //   system.testConservativeForcing(0.001);

  system.meshProcessor.meshMutator.isShiftVertex = true;
  system.meshProcessor.meshMutator.flipNonDelaunay = true;
  // system.meshProcessor.meshMutator.splitLarge = true;
  system.meshProcessor.meshMutator.splitFat = true;
  system.meshProcessor.meshMutator.splitSkinnyDelaunay = true;
  system.meshProcessor.meshMutator.splitCurved = true;
  system.meshProcessor.meshMutator.curvTol = 0.003;
  system.meshProcessor.meshMutator.collapseSkinny = true;

  const double dt = 0.01, T = 1000000, eps = 1e-4, tSave = 1;
  const std::string outputDir = "/tmp";

  mem3dg::solver::integrator::Euler integrator{system, dt,  T,
                                               tSave,  eps, outputDir};
  integrator.ifPrintToConsole = true;
  integrator.ifOutputMeshFile = false;
  integrator.ifOutputTrajFile = false;
  integrator.processMeshPeriod = 0.1;
  integrator.isBacktrack = true;
  integrator.ifAdaptiveStep = true;
  integrator.integrate();
  return 0;
}
