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

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/heat_method_distance.h"
#include "geometrycentral/surface/meshio.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/utilities/vector3.h"
#include "mem3dg/constants.h"
#include "mem3dg/meshops.h"
#include <cmath>
#include <stdexcept>
#include <vector>
#ifdef MEM3DG_WITH_NETCDF
#include "mem3dg/solver/trajfile.h"
#endif
#include "mem3dg/solver/system.h"

#include "polyscope/surface_mesh.h"
#include "polyscope/view.h"
#include <polyscope/polyscope.h>

#include <csignal>
#include <iomanip>

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

namespace mem3dg {
namespace solver {
#ifdef MEM3DG_WITH_NETCDF

void System::mapContinuationVariables(std::string trajFile, int startingFrame) {

  // Open netcdf file
  TrajFile fd = TrajFile::openReadOnly(trajFile);
  fd.getNcFrame(startingFrame);

  // Check consistent topology for continuation
  if (!(mesh->nFaces() == fd.getTopology().rows() &&
        mesh->nVertices() == fd.getCoords(startingFrame).rows())) {
    throw std::logic_error(
        "Topology for continuation parameters mapping is not consistent!");
  }

  // Map continuation variables
  time = fd.getTime(startingFrame);
  if (parameters.protein0.rows() == 1 && parameters.protein0[0] == -1) {
    proteinDensity.raw() = fd.getProteinDensity(startingFrame);
  } else {
    throw std::logic_error(
        "protein0 has to be disabled (=[-1]) for continuing simulations!");
  }
  toMatrix(velocity) = fd.getVelocity(startingFrame);
  // F.toMatrix(vel_protein) = fd.getProteinVelocity(startingFrame);
}

std::tuple<std::unique_ptr<gcs::ManifoldSurfaceMesh>,
           std::unique_ptr<gcs::VertexPositionGeometry>,
           std::unique_ptr<gcs::VertexPositionGeometry>>
System::readTrajFile(std::string trajFile, int startingFrame,
                     std::size_t nSub) {

  // Declare pointers to mesh / geometry objects
  std::unique_ptr<gcs::ManifoldSurfaceMesh> mesh;
  std::unique_ptr<gcs::VertexPositionGeometry> vpg;
  std::unique_ptr<gcs::ManifoldSurfaceMesh> referenceMesh;
  std::unique_ptr<gcs::VertexPositionGeometry> referenceVpg;
  std::unique_ptr<gcs::VertexPositionGeometry> refVpg;

  TrajFile fd = TrajFile::openReadOnly(trajFile);
  fd.getNcFrame(startingFrame);
  std::tie(mesh, vpg) = gcs::makeManifoldSurfaceMeshAndGeometry(
      fd.getCoords(startingFrame), fd.getTopology());
  std::cout << "Loaded input mesh from " << trajFile << " of frame "
            << startingFrame << std::endl;

  /// Load reference geometry ptrRefVpg onto ptrMesh object
  std::tie(referenceMesh, referenceVpg) =
      gcs::makeManifoldSurfaceMeshAndGeometry(fd.getRefcoordinate(),
                                              fd.getTopology());
  std::cout << "Loaded reference mesh" << std::endl;

  /// Subdivide the mesh and geometry objects
  if (nSub > 0) {
    mem3dg::loopSubdivide(mesh, vpg, nSub);
    mem3dg::loopSubdivide(referenceMesh, referenceVpg, nSub);
    std::cout << "Subdivided input and reference mesh " << nSub << " time(s)"
              << std::endl;
  }

  // Check consistent topology
  if ((mesh->nVertices() == referenceMesh->nVertices() &&
       mesh->nEdges() == referenceMesh->nEdges() &&
       mesh->nFaces() == referenceMesh->nFaces())) {
    // reinterpret referenceVpg to mesh instead of referenceMesh.
    refVpg = referenceVpg->reinterpretTo(*mesh);
    // throw std::logic_error(
    //     "Topology of input mesh and reference mesh is not consistent! If not
    //     " "referencing a mesh, please have the option isRefMesh on and have "
    //     "input mesh as the duplicated argument!");
  }

  return std::make_tuple(std::move(mesh), std::move(vpg), std::move(refVpg));
}
#endif

std::tuple<std::unique_ptr<gcs::ManifoldSurfaceMesh>,
           std::unique_ptr<gcs::VertexPositionGeometry>,
           std::unique_ptr<gcs::VertexPositionGeometry>>
System::readMeshes(std::string inputMesh, std::string refMesh,
                   std::size_t nSub) {

  // Declare pointers to mesh / geometry objects
  std::unique_ptr<gcs::ManifoldSurfaceMesh> mesh;
  std::unique_ptr<gcs::VertexPositionGeometry> vpg;
  std::unique_ptr<gcs::ManifoldSurfaceMesh> referenceMesh;
  std::unique_ptr<gcs::VertexPositionGeometry> referenceVpg;
  std::unique_ptr<gcs::VertexPositionGeometry> refVpg;

  // Load input mesh and geometry
  std::tie(mesh, vpg) = gcs::readManifoldSurfaceMesh(inputMesh);
  std::cout << "Loaded input mesh " << inputMesh << std::endl;

  // Load input reference mesh and geometry
  std::tie(referenceMesh, referenceVpg) = gcs::readManifoldSurfaceMesh(refMesh);
  std::cout << "Loaded reference mesh " << refMesh << std::endl;

  // Subdivide the mesh and geometry objects
  if (nSub > 0) {
    // mem3dg::subdivide(mesh, vpg, nSub);
    // mem3dg::subdivide(ptrRefMesh, ptrRefVpg, nSub);
    mem3dg::loopSubdivide(mesh, vpg, nSub);
    mem3dg::loopSubdivide(referenceMesh, referenceVpg, nSub);
    std::cout << "Subdivided input and reference mesh " << nSub << " time(s)"
              << std::endl;
  }

  // Check consistent topology
  if ((mesh->nVertices() == referenceMesh->nVertices() &&
       mesh->nEdges() == referenceMesh->nEdges() &&
       mesh->nFaces() == referenceMesh->nFaces())) {
    // reinterpret referenceVpg to mesh instead of referenceMesh.
    refVpg = referenceVpg->reinterpretTo(*mesh);
    // throw std::logic_error(
    //     "Topology of input mesh and reference mesh is not consistent! If not
    //     " "referencing a mesh, please have the option isRefMesh on and have "
    //     "input mesh as the duplicated argument!");
  }

  return std::make_tuple(std::move(mesh), std::move(vpg), std::move(refVpg));
}

std::tuple<std::unique_ptr<gcs::ManifoldSurfaceMesh>,
           std::unique_ptr<gcs::VertexPositionGeometry>,
           std::unique_ptr<gcs::VertexPositionGeometry>>
System::readMeshes(
    Eigen::Matrix<std::size_t, Eigen::Dynamic, 3> &topologyMatrix,
    Eigen::Matrix<double, Eigen::Dynamic, 3> &vertexMatrix,
    Eigen::Matrix<double, Eigen::Dynamic, 3> &refVertexMatrix,
    std::size_t nSub) {

  // Declare pointers to mesh / geometry objects
  std::unique_ptr<gcs::ManifoldSurfaceMesh> mesh;
  std::unique_ptr<gcs::VertexPositionGeometry> vpg;
  std::unique_ptr<gcs::ManifoldSurfaceMesh> referenceMesh;
  std::unique_ptr<gcs::VertexPositionGeometry> referenceVpg;
  std::unique_ptr<gcs::VertexPositionGeometry> refVpg;

  // Load input mesh and geometry
  std::tie(mesh, vpg) =
      gcs::makeManifoldSurfaceMeshAndGeometry(vertexMatrix, topologyMatrix);
  std::cout << "Loaded input mesh " << std::endl;

  // Load input reference mesh and geometry
  std::tie(referenceMesh, referenceVpg) =
      gcs::makeManifoldSurfaceMeshAndGeometry(refVertexMatrix, topologyMatrix);
  std::cout << "Loaded reference mesh " << std::endl;

  // Subdivide the mesh and geometry objects
  if (nSub > 0) {
    // mem3dg::subdivide(mesh, vpg, nSub);
    // mem3dg::subdivide(ptrRefMesh, ptrRefVpg, nSub);
    mem3dg::loopSubdivide(mesh, vpg, nSub);
    mem3dg::loopSubdivide(referenceMesh, referenceVpg, nSub);
    std::cout << "Subdivided input and reference mesh " << nSub << " time(s)"
              << std::endl;
  }

  // Check consistent topology
  if ((mesh->nVertices() == referenceMesh->nVertices() &&
       mesh->nEdges() == referenceMesh->nEdges() &&
       mesh->nFaces() == referenceMesh->nFaces())) {
    // reinterpret referenceVpg to mesh instead of referenceMesh.
    refVpg = referenceVpg->reinterpretTo(*mesh);
    // throw std::logic_error(
    //     "Topology of input mesh and reference mesh is not consistent! If not
    //     " "referencing a mesh, please have the option isRefMesh on and have "
    //     "input mesh as the duplicated argument!");
  }

  return std::make_tuple(std::move(mesh), std::move(vpg), std::move(refVpg));
}

void System::mapContinuationVariables(std::string plyFile) {
  std::unique_ptr<gcs::SurfaceMesh> ptrMesh_local;
  std::unique_ptr<gcs::RichSurfaceMeshData> ptrRichData_local;

  // Open mesh file
  std::tie(ptrMesh_local, ptrRichData_local) =
      gcs::RichSurfaceMeshData::readMeshAndData(plyFile);

  // Check consistent topology for continuation
  if (mesh->nFaces() != ptrMesh_local->nFaces() ||
      mesh->nVertices() != ptrMesh_local->nVertices()) {
    throw std::logic_error(
        "Topology for continuation parameters mapping is not consistent!");
  } else {
    // Map continuation variables
    if (parameters.protein0.rows() == 1 && parameters.protein0[0] == -1) {
      proteinDensity =
          ptrRichData_local->getVertexProperty<double>("protein_density")
              .reinterpretTo(*mesh);
      // vel_protein =
      //     ptrRichData_local->getVertexProperty<double>("protein_velocity")
      //         .reinterpretTo(*mesh);
      ;
    } else {
      throw std::logic_error(
          "protein0 has to be disabled (=[-1]) for continuing simulations!");
    }
  }
}

void System::saveRichData(std::string PathToSave, bool isJustGeometry) {

  if (isJustGeometry) {
    gcs::writeSurfaceMesh(*mesh, *vpg, PathToSave);
  } else {
    gcs::RichSurfaceMeshData richData(*mesh);
    richData.addMeshConnectivity();
    richData.addGeometry(*vpg);

    // write protein distribution
    richData.addVertexProperty("protein_density", proteinDensity);

    // write bool
    gcs::VertexData<double> msk(*mesh);
    msk.fromVector(toMatrix(forces.forceMask).rowwise().sum());
    richData.addVertexProperty("force_mask", msk);
    richData.addVertexProperty("protein_mask", forces.proteinMask);
    gcs::VertexData<double> smthingMsk(*mesh);
    smthingMsk.fromVector(smoothingMask.raw().cast<double>());
    richData.addVertexProperty("smoothing_mask", smthingMsk);
    gcs::VertexData<double> tkr(*mesh);
    tkr.fromVector(thePointTracker.raw().cast<double>());
    richData.addVertexProperty("the_point", tkr);

    // write geometry
    gcs::VertexData<double> meanCurv(*mesh);
    meanCurv.fromVector(vpg->vertexMeanCurvatures.raw().array() /
                        vpg->vertexDualAreas.raw().array());
    richData.addVertexProperty("mean_curvature", meanCurv);
    gcs::VertexData<double> gaussCurv(*mesh);
    gaussCurv.fromVector(vpg->vertexGaussianCurvatures.raw().array() /
                         vpg->vertexDualAreas.raw().array());
    richData.addVertexProperty("gauss_curvature", gaussCurv);
    richData.addVertexProperty("spon_curvature", H0);

    // write pressures
    richData.addVertexProperty("bending_force", forces.bendingForce);
    richData.addVertexProperty("capillary_force", forces.capillaryForce);
    richData.addVertexProperty("line_tension_force", forces.lineCapillaryForce);
    richData.addVertexProperty("osmotic_force", forces.osmoticForce);
    richData.addVertexProperty("external_force", forces.externalForce);
    richData.addVertexProperty("physical_force", forces.mechanicalForce);

    // write chemical potential
    richData.addVertexProperty("diffusion_potential",
                               forces.diffusionPotential);
    richData.addVertexProperty("bending_potential", forces.bendingPotential);
    richData.addVertexProperty("adsorption_potential",
                               forces.adsorptionPotential);
    richData.addVertexProperty("chemical_potential", forces.chemicalPotential);

    richData.write(PathToSave);
  }
}

void System::checkParametersAndOptions() {

  // check validity of parameters / options
  if (!parameters.variation.isShapeVariation) {
    if (parameters.tension.Ksg != 0) {
      mem3dg_runtime_error("Stretching modulus Ksg has to be zero for non "
                           "shape variation simulation!");
    }
    if (parameters.osmotic.Kv != 0) {
      mem3dg_runtime_error("Pressure-volume modulus Kv has to be zero for non "
                           "shape variation simulation!");
    }
    meshProcessor.meshMutator.summarizeStatus();
    if (meshProcessor.isMeshMutate) {
      mem3dg_runtime_error("Mesh mutation operation not allowed for non shape "
                           "variation simulation");
    }
    if (parameters.boundary.shapeBoundaryCondition != "none") {
      mem3dg_runtime_error("Shape boundary condition has to be none for non "
                           "shape variation simulation");
    }
  }

  // protein related
  if (parameters.variation.isProteinVariation != (parameters.Bc > 0)) {
    mem3dg_runtime_error("Binding constant Bc has to be consistent with the "
                         "protein variation option!");
  }
  if (parameters.variation.isProteinVariation && parameters.bending.Kbc != 0) {
    mem3dg_runtime_error(
        "Kbc != 0 is currently not expected for protein variation!");
  }
  if (parameters.protein0.rows() == 1 && parameters.protein0[0] == -1) {
    std::cout << "Disable protein init, expect continuation simulation."
              << std::endl;
  } else if (parameters.protein0.rows() == 1 && parameters.protein0[0] >= 0 &&
             parameters.protein0[0] <= 1) {
    if (parameters.variation.isProteinVariation) {
      if (parameters.protein0[0] == 0 || parameters.protein0[0] == 1) {
        mem3dg_runtime_error("{0<phi<1}");
      }
    } else {
      if (parameters.protein0[0] != 1 || parameters.bending.Kb != 0 ||
          parameters.dirichlet.eta != 0 || parameters.adsorption.epsilon != 0) {
        mem3dg_runtime_error(
            "For homogenous membrane simulation, good "
            "practice is "
            "to set protein0 = 1, Kb = 0, eta = 0, epsilon = 0 to "
            "avoid ambiguity & save computation!");
      }
    }
  } else if (parameters.protein0.rows() == 4 &&
             (parameters.protein0[2] >= 0 && parameters.protein0[2] <= 1) &&
             (parameters.protein0[3] >= 0 && parameters.protein0[3] <= 1) &&
             (parameters.protein0[0] > 0 && parameters.protein0[1] > 0)) {
    if (parameters.protein0[2] == parameters.protein0[3]) {
      mem3dg_runtime_error("Please switch to {phi} for homogeneous membrane!");
    }
    if (parameters.variation.isProteinVariation) {
      if (parameters.protein0[2] == 0 || parameters.protein0[2] == 1 ||
          parameters.protein0[3] == 0 || parameters.protein0[3] == 1) {
        mem3dg_runtime_error("{0<phi<1}");
      }
    }
  } else if (parameters.protein0.rows() == proteinDensity.raw().rows() &&
             (parameters.protein0.array() > 0).all() &&
             (parameters.protein0.array() < 1).all()) {
  } else {
    mem3dg_runtime_error("protein 0 can only be specified in three ways: 1. "
                         "length = 1, uniform {0<phi<1} 2. "
                         "length = 4, geodesic disk, {r1>0, r2>0, "
                         "0<phi_in<1, 0<phi_out<1} 3. length "
                         "= nVertices, user defined. To disable use {-1}");
  }

  // boundary related
  if (parameters.radius <= 0 && parameters.radius != -1) {
    mem3dg_runtime_error("Radius > 0 or radius = 1 to disable!");
  }
  isOpenMesh = mesh->hasBoundary();
  if (!isOpenMesh && mesh->genus() != 0) {
    mem3dg_runtime_error(
        "Do not support closed mesh with nonzero number of genus!")
  }
  if (isOpenMesh) {
    if (parameters.boundary.shapeBoundaryCondition != "roller" &&
        parameters.boundary.shapeBoundaryCondition != "pin" &&
        parameters.boundary.shapeBoundaryCondition != "fixed" &&
        parameters.variation.isShapeVariation) {
      std::cout << "Shape boundary condition type (roller, pin or fixed) "
                   "has not been specified for open boundary mesh!"
                << std::endl;
    }
    if (parameters.boundary.proteinBoundaryCondition != "pin" &&
        parameters.variation.isProteinVariation) {
      std::cout << "Protein boundary condition type (pin) "
                   "has not been specified for open boundary mesh!"
                << std::endl;
    }
  } else {
    if (parameters.tension.A_res != 0 || parameters.osmotic.V_res != 0) {
      mem3dg_runtime_error(
          "Closed mesh can not have area and volume reservior!");
    }
    if (parameters.boundary.shapeBoundaryCondition != "none") {
      mem3dg_runtime_error(
          "Shape boundary condition type should be disable (= \"none\") "
          "for closed boundary mesh!");
    }
    if (parameters.boundary.proteinBoundaryCondition != "none") {
      mem3dg_runtime_error(
          "Protein boundary condition type should be disable (= \"none\") "
          "for closed boundary mesh!");
    }
  }

  // regularization related
  meshProcessor.summarizeStatus();
  if (meshProcessor.isMeshRegularize &&
      ((mesh->nVertices() != refVpg->mesh.nVertices() ||
        mesh->nEdges() != refVpg->mesh.nEdges() ||
        mesh->nFaces() != refVpg->mesh.nFaces()))) {
    mem3dg_runtime_error("For topologically different reference mesh, mesh "
                         "regularization cannot be applied!");
  }

  // the vertex related
  if (parameters.point.pt.rows() > 3) {
    mem3dg_runtime_error(
        "Length of p.pt cannnot exceed 3! Instruction: (Length=1) => (vertex "
        "index); (Length=2) => ([x,y] coordinate); (Length=3) => ([x,y,z] "
        "coordinate)");
  }
  if (parameters.point.pt.rows() == 2 && !mesh->hasBoundary()) {
    std::cout << "\nWARNING: specifying x-y coordinate on closed surface may "
                 "lead to ambiguity! Please check by visualizing it first!\n"
              << std::endl;
  }
  if (parameters.point.isFloatVertex) {
    if (parameters.point.pt.rows() == 1) {
      mem3dg_runtime_error(
          "To have Floating vertex, one must specify vertex by coordinate!");
    }
    // if (P.pt.rows() == 3) {
    //   std::cout << "\nWARNING: float vertex using 3D position may lead to
    //   jump "
    //                "in geodesic sense!\n"
    //             << std::endl;
    // }
  }

  // Osmotic pressure related
  if (parameters.osmotic.isPreferredVolume) {
    if (parameters.osmotic.cam != -1) {
      mem3dg_runtime_error("ambient concentration cam has to be -1 for "
                           "preferred volume parametrized simulation!");
    }
    if (parameters.osmotic.isConstantOsmoticPressure) {
      mem3dg_runtime_error("preferred volume and constant osmotic pressure "
                           "cannot be simultaneously turned on!");
    }
  } else {
    if (parameters.osmotic.Vt != -1 &&
        !parameters.osmotic.isConstantOsmoticPressure) {
      mem3dg_runtime_error("preferred volume Vt has to be -1 for "
                           "ambient pressure parametrized simulation!");
    }
  }
  if (parameters.osmotic.isConstantOsmoticPressure) {
    if (parameters.osmotic.isPreferredVolume) {
      mem3dg_runtime_error("preferred volume and constant osmotic pressure "
                           "cannot be simultaneously turned on!");
    }
    if (parameters.osmotic.Vt != -1 || parameters.osmotic.V_res != 0 ||
        parameters.osmotic.cam != -1) {
      mem3dg_runtime_error(
          "Vt and cam have to be set to -1 and V_res to be 0 to "
          "enable constant omostic pressure! Note Kv is the "
          "pressure directly!");
    }
  }

  // surface tension related
  if (parameters.tension.isConstantSurfaceTension) {
    if (parameters.tension.A_res != 0) {
      mem3dg_runtime_error(
          "A_res has to be set to 0 to "
          "enable constant surface! Note Ksg is the surface tension directly!");
    }
  }

  // external force related
  if (parameters.external.Kf == 0) {
    if (parameters.external.conc != -1 || parameters.external.height != 0) {
      mem3dg_runtime_error("With no external force, its concentration "
                           "should be disabled (=-1) "
                           "and prescribed height should be set to 0!");
    }
  }
}

void System::pcg_test() {
  // Generate a normal distribution around that mean
  std::normal_distribution<> normal_dist(0, 2);

  // Make a copy of the RNG state to use later
  pcg32 rng_checkpoint = rng;

  // Produce histogram
  std::map<int, int> hist;
  for (int n = 0; n < 10000; ++n) {
    ++hist[std::round(normal_dist(rng))];
  }
  std::cout << "Normal distribution around " << 0 << ":\n";
  for (auto p : hist) {
    std::cout << std::fixed << std::setprecision(1) << std::setw(2) << p.first
              << ' ' << std::string(p.second / 30, '*') << '\n';
  }

  // Produce information about RNG usage
  std::cout << "Required " << (rng - rng_checkpoint) << " random numbers.\n";
}

void System::initConstants() {
  // Initialize random number generator
  pcg_extras::seed_seq_from<std::random_device> seed_source;
  rng = pcg32(seed_source);

  // define local vpg
  refVpg->requireEdgeLengths();
  refVpg->requireFaceAreas();

  // // Initialize V-E distribution matrix for line tension calculation
  // if (P.dirichlet.eta != 0) {
  //   D = localVpg->d0.transpose().cwiseAbs() / 2;
  //   // for (int k = 0; k < D.outerSize(); ++k) {
  //   //   for (Eigen::SparseMatrix<double>::InnerIterator it(D, k); it; ++it)
  //   {
  //   //     it.valueRef() = 0.5;
  //   //   }
  //   // }
  // }

  // Find "the" vertex
  findThePoint(*refVpg, geodesicDistanceFromPtInd, 1e18);

  // Initialize const geodesic distance
  gcs::HeatMethodDistanceSolver heatSolver(*vpg);
  geodesicDistanceFromPtInd = heatSolver.computeDistance(thePoint);

  // Initialize the constant mask based on distance from the point specified
  if (parameters.radius != -1) {
    if (parameters.radius > geodesicDistanceFromPtInd.raw().maxCoeff() ||
        parameters.radius < geodesicDistanceFromPtInd.raw().minCoeff()) {
      mem3dg_runtime_error("initConstants: either all vertices or none is "
                           "included within integration disk, "
                           "set radius = -1 to disable!");
    }
    for (gcs::Vertex v : mesh->vertices()) {
      forces.forceMask[v] = (geodesicDistanceFromPtInd[v] < parameters.radius)
                                ? gc::Vector3{1, 1, 1}
                                : gc::Vector3{0, 0, 0};
      forces.proteinMask[v] =
          (geodesicDistanceFromPtInd[v] < parameters.radius) ? 1 : 0;
    }
  }

  // Initialize protein density
  if (parameters.protein0.size() == 1) {
    proteinDensity.raw().setConstant(mesh->nVertices(), 1,
                                     parameters.protein0[0]);
  } else if (parameters.protein0.rows() == proteinDensity.raw().rows()) {
    proteinDensity.raw() = parameters.protein0;
  } else if (parameters.protein0.size() == 4) {
    std::vector<double> r_heter{parameters.protein0[0], parameters.protein0[1]};
    tanhDistribution(*vpg, proteinDensity.raw(),
                     geodesicDistanceFromPtInd.raw(), parameters.sharpness,
                     r_heter);
    proteinDensity.raw() *= parameters.protein0[2] - parameters.protein0[3];
    proteinDensity.raw().array() += parameters.protein0[3];
  }

  // Mask boundary element
  if (mesh->hasBoundary()) {
    boundaryForceMask(*mesh, forces.forceMask,
                      parameters.boundary.shapeBoundaryCondition);
    boundaryProteinMask(*mesh, forces.proteinMask,
                        parameters.boundary.proteinBoundaryCondition);
  }

  // Explicitly cached the reference face areas data
  refFaceAreas = refVpg->faceAreas;

  // Explicitly cached the reference edge length data
  refEdgeLengths = refVpg->edgeLengths;

  // Initialize the constant target surface (total mesh) area
  if (isOpenMesh) {
    refSurfaceArea = parameters.tension.A_res;
    for (gcs::BoundaryLoop bl : mesh->boundaryLoops()) {
      refSurfaceArea += computePolygonArea(bl, refVpg->inputVertexPositions);
    }
  } else {
    refSurfaceArea = refFaceAreas.raw().sum();
  }

  // initialize/update total surface area
  surfaceArea = vpg->faceAreas.raw().sum() + parameters.tension.A_res;
  std::cout << "area_init/area_ref = " << surfaceArea / refSurfaceArea
            << std::endl;

  // Initialize the constant target mean face area
  if (meshProcessor.meshMutator.isSplitEdge ||
      meshProcessor.meshMutator.isCollapseEdge) {
    meanTargetFaceArea = refFaceAreas.raw().sum() / mesh->nFaces();
    meshProcessor.meshMutator.targetFaceArea = meanTargetFaceArea;
  }

  // Initialize the constant target mean edge length
  meanTargetEdgeLength = refEdgeLengths.raw().sum() / mesh->nEdges();

  // Initialize the target constant cross length ration
  if (meshProcessor.meshRegularizer.Kst) {
    computeLengthCrossRatio(*refVpg, targetLcrs);
  }

  // Initialize the constant reference volume
  std::cout << "vol_ref = "
            << (isOpenMesh ? parameters.osmotic.V_res
                           : std::pow(refSurfaceArea / constants::PI / 4, 1.5) *
                                 (4 * constants::PI / 3))
            << std::endl;

  /// initialize/update enclosed volume
  volume = getMeshVolume(*mesh, *vpg, true) + parameters.osmotic.V_res;
  std::cout << "vol_init = " << volume << std::endl;
}

void System::updateVertexPositions(bool isUpdateGeodesics) {

  // refresh cached quantities after regularization
  vpg->refreshQuantities();

  // EigenMap commonly used matrices
  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg->vertexNormals);
  auto positions = gc::EigenMap<double, 3>(vpg->inputVertexPositions);

  // recompute floating "the vertex"
  if (parameters.point.isFloatVertex && isUpdateGeodesics) {
    findThePoint(
        *vpg, geodesicDistanceFromPtInd,
        3 * vpg->edgeLength(thePoint.nearestVertex().halfedge().edge()));
  }

  // update geodesic distance
  if (isUpdateGeodesics) {
    gcs::HeatMethodDistanceSolver heatSolver(*vpg);
    geodesicDistanceFromPtInd = heatSolver.computeDistance(thePoint);
  }

  // initialize/update external force
  if (parameters.external.Kf != 0 && isUpdateGeodesics) {
    computeExternalForce();
  }

  // update protein density
  if (parameters.protein0.rows() == 4 &&
      !parameters.variation.isProteinVariation) {
    if (isUpdateGeodesics) {
      std::vector<double> r_heter{parameters.protein0[0],
                                  parameters.protein0[1]};
      tanhDistribution(*vpg, proteinDensity.raw(),
                       geodesicDistanceFromPtInd.raw(), parameters.sharpness,
                       r_heter);
      proteinDensity.raw() *= parameters.protein0[2] - parameters.protein0[3];
      proteinDensity.raw().array() += parameters.protein0[3];
    }
  }

  // compute face gradient of spontaneous curvature
  if (parameters.dirichlet.eta != 0) {
    computeGradient(proteinDensity, proteinDensityGradient);
  }

  // Update protein density dependent quantities
  if (parameters.bending.relation == "linear") {
    H0.raw() = proteinDensity.raw() * parameters.bending.H0c;
    Kb.raw() = parameters.bending.Kb +
               parameters.bending.Kbc * proteinDensity.raw().array();
  } else if (parameters.bending.relation == "hill") {
    Eigen::Matrix<double, Eigen::Dynamic, 1> proteinDensitySq =
        (proteinDensity.raw().array() * proteinDensity.raw().array()).matrix();
    H0.raw() = (parameters.bending.H0c * proteinDensitySq.array() /
                (1 + proteinDensitySq.array()))
                   .matrix();
    Kb.raw() = (parameters.bending.Kb + parameters.bending.Kbc *
                                            proteinDensitySq.array() /
                                            (1 + proteinDensitySq.array()))
                   .matrix();
  } else {
    mem3dg_runtime_error("updateVertexPosition: P.relation is invalid option!");
  }

  /// initialize/update enclosed volume
  volume = getMeshVolume(*mesh, *vpg, true) + parameters.osmotic.V_res;

  // update global osmotic pressure
  if (parameters.osmotic.isPreferredVolume) {
    forces.osmoticPressure =
        -(parameters.osmotic.Kv * (volume - parameters.osmotic.Vt) /
              parameters.osmotic.Vt / parameters.osmotic.Vt +
          parameters.lambdaV);
  } else if (parameters.osmotic.isConstantOsmoticPressure) {
    forces.osmoticPressure = parameters.osmotic.Kv;
  } else {
    forces.osmoticPressure =
        mem3dg::constants::i * mem3dg::constants::R * parameters.temp *
        (parameters.osmotic.n / volume - parameters.osmotic.cam);
  }

  // initialize/update total surface area
  surfaceArea = vpg->faceAreas.raw().sum() + parameters.tension.A_res;

  // update global surface tension
  forces.surfaceTension = parameters.tension.isConstantSurfaceTension
                              ? parameters.tension.Ksg
                              : parameters.tension.Ksg *
                                        (surfaceArea - refSurfaceArea) /
                                        refSurfaceArea +
                                    parameters.lambdaSG;

  // initialize/update line tension (on dual edge)
  if (parameters.dirichlet.eta != 0 && false) {
    mem3dg_runtime_error(
        "updateVertexPosition: out of data implementation on line tension, "
        "shouldn't be called!");
    // scale the dH0 such that it is integrated over the edge
    // this is under the case where the resolution is low. This is where the
    // extra vpg->edgeLength comes from!!!
    // WIP The unit of line tension is in force*length (e.g. XXNewton)
    // F.lineTension.raw() = P.dirichlet.eta * vpg->edgeLengths.raw().array() *
    //                       (vpg->d0 * H0.raw()).cwiseAbs().array();
    // lineTension.raw() = P.dirichlet.eta * (vpg->d0 *
    // H0.raw()).cwiseAbs().array();
  }
}

void System::findThePoint(gcs::VertexPositionGeometry &vpg,
                          gcs::VertexData<double> &geodesicDistance,
                          double range) {
  bool isUpdated = false;
  if (parameters.point.isFloatVertex) {
    switch (parameters.point.pt.rows()) {
    case 1: {
      throw std::logic_error(
          "To have Floating vertex, one must specify vertex by coordinate!");
      break;
    }
    case 2: {
      // Find the cloest vertex to the point in the x-y plane
      gcs::Vertex closestVertex =
          closestVertexToPt(*mesh, vpg, parameters.point.pt, geodesicDistance, range);
      double shortestDistance = 1e18;
      // loop over every faces around the vertex
      for (gcs::Halfedge he : closestVertex.outgoingHalfedges()) {
        if (he.isInterior()) {
          // specify vertex coordinates and the target coordinate on the face
          gc::Vector2 v1{vpg.inputVertexPositions[he.vertex()].x,
                         vpg.inputVertexPositions[he.vertex()].y};
          gc::Vector2 v2{vpg.inputVertexPositions[he.next().vertex()].x,
                         vpg.inputVertexPositions[he.next().vertex()].y};
          gc::Vector2 v3{vpg.inputVertexPositions[he.next().next().vertex()].x,
                         vpg.inputVertexPositions[he.next().next().vertex()].y};
          gc::Vector2 v{parameters.point.pt[0], parameters.point.pt[1]};
          // find the inverse barycentric mapping based on the cartesian vertex
          // coordinates
          gc::Vector3 baryCoords_ = cartesianToBarycentric(v1, v2, v3, v);

          if (baryCoords_.x > 0 && baryCoords_.y > 0 &&
              baryCoords_.z > 0) { // A. set the surface point when the point
                                   // lays within the triangle
            thePoint = gcs::SurfacePoint(
                he.face(), correspondBarycentricCoordinates(baryCoords_, he));
            isUpdated = true;
            break;
          } else { // B. avoid the floating point comparision, find the best by
                   // looping over the whole fan
            baryCoords_ =
                gc::componentwiseMax(baryCoords_, gc::Vector3{0, 0, 0});
            baryCoords_ /= gc::sum(baryCoords_);
            gcs::SurfacePoint someSurfacePoint(
                he.face(), correspondBarycentricCoordinates(baryCoords_, he));
            double distance =
                (gc::Vector2{parameters.point.pt[0], parameters.point.pt[1]} -
                 gc::Vector2{
                     someSurfacePoint.interpolate(vpg.inputVertexPositions).x,
                     someSurfacePoint.interpolate(vpg.inputVertexPositions).y})
                    .norm();
            if (distance < shortestDistance) {
              thePoint = someSurfacePoint;
              shortestDistance = distance;
              isUpdated = true;
            }
          }
        }
      }
      break;
    }
    case 3: {
      // initialize embedded point and the closest vertex
      gc::Vector3 embeddedPoint{parameters.point.pt[0], parameters.point.pt[1],
                                parameters.point.pt[2]};
      gcs::Vertex closestVertex =
          closestVertexToPt(*mesh, vpg, parameters.point.pt, geodesicDistance, range);
      gc::Vector3 vertexToPoint =
          embeddedPoint - vpg.inputVertexPositions[closestVertex];
      // initialize the surface point as the closest vertex
      // thePoint = gc::SurfacePoint(closestVertex);
      // double shortestDistance = vertexToPoint.norm();
      double shortestDistance = 1e18;
      // loop over every faces around the vertex
      for (gcs::Halfedge he : closestVertex.outgoingHalfedges()) {
        if (he.isInterior()) {
          // project the embedded point onto the face
          auto faceNormal = vpg.faceNormal(he.face());
          gc::Vector3 projectedEmbeddedPoint =
              embeddedPoint - gc::dot(vertexToPoint, faceNormal) * faceNormal;
          // determine the choice of coordinates used for inverse
          // barycentric mapping based on orientation of the face
          gc::Vector2 v1, v2, v3, v;
          if (abs(faceNormal.z) > std::sqrt(3) / 3) {
            v1 = gc::Vector2{vpg.inputVertexPositions[he.vertex()].x,
                             vpg.inputVertexPositions[he.vertex()].y};
            v2 = gc::Vector2{vpg.inputVertexPositions[he.next().vertex()].x,
                             vpg.inputVertexPositions[he.next().vertex()].y};
            v3 = gc::Vector2{
                vpg.inputVertexPositions[he.next().next().vertex()].x,
                vpg.inputVertexPositions[he.next().next().vertex()].y};
            v = gc::Vector2{projectedEmbeddedPoint.x, projectedEmbeddedPoint.y};
          } else if (abs(faceNormal.x) > std::sqrt(3) / 3) {
            v1 = gc::Vector2{vpg.inputVertexPositions[he.vertex()].y,
                             vpg.inputVertexPositions[he.vertex()].z};
            v2 = gc::Vector2{vpg.inputVertexPositions[he.next().vertex()].y,
                             vpg.inputVertexPositions[he.next().vertex()].z};
            v3 = gc::Vector2{
                vpg.inputVertexPositions[he.next().next().vertex()].y,
                vpg.inputVertexPositions[he.next().next().vertex()].z};
            v = gc::Vector2{projectedEmbeddedPoint.y, projectedEmbeddedPoint.z};
          } else {
            v1 = gc::Vector2{vpg.inputVertexPositions[he.vertex()].z,
                             vpg.inputVertexPositions[he.vertex()].x};
            v2 = gc::Vector2{vpg.inputVertexPositions[he.next().vertex()].z,
                             vpg.inputVertexPositions[he.next().vertex()].x};
            v3 = gc::Vector2{
                vpg.inputVertexPositions[he.next().next().vertex()].z,
                vpg.inputVertexPositions[he.next().next().vertex()].x};
            v = gc::Vector2{projectedEmbeddedPoint.z, projectedEmbeddedPoint.x};
          }
          // find the inverse barycentric mapping based on the cartesian
          // vertex coordinates
          gc::Vector3 baryCoords_ = cartesianToBarycentric(v1, v2, v3, v);
          // since might not find the perfect reflecting face, best we could
          // do within each triangle
          baryCoords_ = gc::componentwiseMax(baryCoords_, gc::Vector3{0, 0, 0});
          baryCoords_ /= gc::sum(baryCoords_);
          gcs::SurfacePoint someSurfacePoint(
              he.face(), correspondBarycentricCoordinates(baryCoords_, he));
          // compute optimum distance and set surface point
          double distance = (embeddedPoint - someSurfacePoint.interpolate(
                                                 vpg.inputVertexPositions))
                                .norm();
          if (distance < shortestDistance) {
            thePoint = someSurfacePoint;
            shortestDistance = distance;
            isUpdated = true;
          }
        }
      }
      break;
    }
    }
    // mark three vertex on the face
    thePointTracker.fill(false);
    thePointTracker[thePoint.face.halfedge().vertex()] = true;
    thePointTracker[thePoint.face.halfedge().next().vertex()] = true;
    thePointTracker[thePoint.face.halfedge().next().next().vertex()] = true;
  } else {
    switch (parameters.point.pt.rows()) {
    case 1: {
      // Assign surface point as the indexed vertex
      thePoint = gc::SurfacePoint(mesh->vertex((std::size_t)parameters.point.pt[0]));
      isUpdated = true;
      break;
    }
    case 2: {
      // Find the cloest vertex to the point in the x-y plane
      thePoint = gc::SurfacePoint(closestVertexToPt(*mesh, vpg, parameters.point.pt,
                                                    geodesicDistance, range));
      isUpdated = true;
      break;
    }
    case 3: {
      thePoint = gc::SurfacePoint(closestVertexToPt(*mesh, vpg, parameters.point.pt,
                                                    geodesicDistance, range));
      isUpdated = true;
      break;
    }
    }
    thePointTracker[thePoint.vertex] = true;
  }

  if (!isUpdated) {
    mem3dg_runtime_error("Surface point is not updated!");
  }
}
} // namespace solver
} // namespace mem3dg
