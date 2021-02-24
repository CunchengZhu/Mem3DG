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

#include <cassert>
#include <cmath>
#include <iostream>

#include <geometrycentral/numerical/linear_solvers.h>
#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/intrinsic_geometry_interface.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "mem3dg/solver/meshops.h"
#include "mem3dg/solver/system.h"
#include <Eigen/Core>
#include <pcg_random.hpp>

namespace mem3dg {

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

EigenVectorX3D System::computeBendingPressure() {

  // map the MeshData to eigen matrix XXX_e
  auto bendingPressure_e = gc::EigenMap<double, 3>(bendingPressure);
  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg->vertexNormals);
  auto positions = gc::EigenMap<double, 3>(vpg->inputVertexPositions);

  // Alias
  std::size_t n_vertices = (mesh->nVertices());

  /// A. non-optimized version
  // calculate the Laplacian of mean curvature H
  Eigen::Matrix<double, Eigen::Dynamic, 1> lap_H =
      M_inv * L * (H.raw() - H0.raw());

  // initialize and calculate intermediary result scalerTerms
  Eigen::Matrix<double, Eigen::Dynamic, 1> scalerTerms =
      rowwiseProduct(H.raw(), H.raw()) + rowwiseProduct(H.raw(), H0.raw()) -
      K.raw();
  // Eigen::Matrix<double, Eigen::Dynamic, 1> zeroMatrix;
  // zeroMatrix.resize(n_vertices, 1);
  // zeroMatrix.setZero();
  // scalerTerms = scalerTerms.array().max(zeroMatrix.array());

  // initialize and calculate intermediary result productTerms
  Eigen::Matrix<double, Eigen::Dynamic, 1> productTerms;
  productTerms.resize(n_vertices, 1);
  productTerms = 2.0 * rowwiseProduct(scalerTerms, H.raw() - H0.raw());

  // calculate bendingForce
  bendingPressure_e =
      -P.Kb * rowwiseScaling(productTerms + lap_H, vertexAngleNormal_e);

  return bendingPressure_e;

  // /// B. optimized version
  // // calculate the Laplacian of mean curvature H
  // Eigen::Matrix<double, Eigen::Dynamic, 1> lap_H_integrated = L * (H - H0);

  // // initialize and calculate intermediary result scalarTerms_integrated
  // Eigen::Matrix<double, Eigen::Dynamic, 1> H_integrated = M * H;
  // Eigen::Matrix<double, Eigen::Dynamic, 1> scalarTerms_integrated =
  //     M * rowwiseProduct(M_inv * H_integrated, M_inv * H_integrated) +
  //     rowwiseProduct(H_integrated, H0) - vpg->vertexGaussianCurvatures.raw();
  // Eigen::Matrix<double, Eigen::Dynamic, 1> zeroMatrix;
  // zeroMatrix.resize(n_vertices, 1);
  // zeroMatrix.setZero();
  // scalarTerms_integrated =
  //     scalarTerms_integrated.array().max(zeroMatrix.array());

  // // initialize and calculate intermediary result productTerms_integrated
  // Eigen::Matrix<double, Eigen::Dynamic, 1> productTerms_integrated;
  // productTerms_integrated.resize(n_vertices, 1);
  // productTerms_integrated =
  //     2.0 * rowwiseProduct(scalarTerms_integrated, H - H0);

  // bendingPressure_e =
  //     -2.0 * P.Kb *
  //     rowwiseScaling(M_inv * (productTerms_integrated + lap_H_integrated),
  //                    vertexAngleNormal_e);
}

EigenVectorX3D System::computeCapillaryPressure() {

  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg->vertexNormals);
  auto capillaryPressure_e = gc::EigenMap<double, 3>(capillaryPressure);

  /// Geometric implementation
  if (mesh->hasBoundary()) { // surface tension of patch
    surfaceTension = -P.Ksg;
  } else { // surface tension of vesicle
    surfaceTension =
        -(P.Ksg * (surfaceArea - targetSurfaceArea) / targetSurfaceArea +
          P.lambdaSG);
  }
  capillaryPressure_e =
      rowwiseScaling(surfaceTension * 2 * H.raw(), vertexAngleNormal_e);

  return capillaryPressure_e;

  // /// Nongeometric implementation
  // for (gcs::Vertex v : mesh->vertices()) {
  //   gc::Vector3 globalForce{0.0, 0.0, 0.0};
  //   for (gcs::Halfedge he : v.outgoingHalfedges()) {
  //     gc::Vector3 base_vec = vecFromHalfedge(he.next(), vpg);
  //     gc::Vector3 localAreaGradient =
  //         -gc::cross(base_vec, vpg->faceNormals[he.face()]);
  //     assert((gc::dot(localAreaGradient, vecFromHalfedge(he, vpg))) < 0);
  //     if (P.Ksg != 0) {
  //       capillaryPressure[v] += -P.Ksg * localAreaGradient *
  //                               (surfaceArea - targetSurfaceArea) /
  //                               targetSurfaceArea;
  //     }
  //   }
  //   capillaryPressure[v] /= vpg->vertexDualAreas[v];
  // }
}

double System::computeInsidePressure() {
  /// Geometric implementation
  if (mesh->hasBoundary()) {
    /// Inside excess pressure of patch
    insidePressure = P.Kv;
  } else if (isReducedVolume) {
    /// Inside excess pressure of vesicle
    insidePressure =
        -(P.Kv * (volume - refVolume * P.Vt) / (refVolume * P.Vt) + P.lambdaV);
  } else {
    insidePressure = P.Kv / volume - P.Kv * P.cam;
  }

  return insidePressure;

  // /// Nongeometric implementation
  // for (gcs::Vertex v : mesh->vertices()) {
  //   for (gcs::Halfedge he : v.outgoingHalfedges()) {
  //     gc::Vector3 p1 = vpg->inputVertexPositions[he.next().vertex()];
  //     gc::Vector3 p2 = vpg->inputVertexPositions[he.next().next().vertex()];
  //     gc::Vector3 dVdx = 0.5 * gc::cross(p1, p2) / 3.0;
  //     insidePressure[v] +=
  //         -P.Kv * (volume - refVolume * P.Vt) / (refVolume * P.Vt) * dVdx;
  //   }
  // }
}

EigenVectorX3D System::computeLineTensionPressure() {
  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg->vertexNormals);
  auto lineTensionPressure_e = gc::EigenMap<double, 3>(lineTensionPressure);
  // lineTension.raw() =
  //     vpg->hodge1Inverse * P.eta *
  //     ((vpg->hodge1 * (vpg->d0 * H0.raw()).cwiseAbs()).array() *
  //      vpg->edgeDihedralAngles.raw().array())
  //             .matrix();
  // lineTensionPressure_e = -rowwiseScaling(
  //     M_inv * D * lineTension.raw(),
  //     vertexAngleNormal_e);

  // normal curvature of the dual edges
  Eigen::Matrix<double, Eigen::Dynamic, 1> normalCurv =
      (vpg->edgeDihedralAngles.raw().array() /
       (vpg->hodge1 * vpg->edgeLengths.raw()).array())
          .matrix();
  lineTensionPressure_e = -rowwiseScaling(
      M_inv * D * vpg->hodge1Inverse *
          (lineTension.raw().array() * normalCurv.array()).matrix(),
      vertexAngleNormal_e);

  return lineTensionPressure_e;
}

EigenVectorX3D System::computeExternalPressure() {

  auto externalPressure_e = gc::EigenMap<double, 3>(externalPressure);
  Eigen::Matrix<double, Eigen::Dynamic, 1> externalPressureMagnitude;

  if (P.Kf != 0) {

    // a. FIND OUT THE CURRENT EXTERNAL PRESSURE MAGNITUDE BASED ON CURRENT
    // GEOMETRY

    // auto &dist_e = heatMethodDistance(vpg, mesh->vertex(P.ptInd)).raw();
    // double stdDev = dist_e.maxCoeff() / P.conc;
    // externalPressureMagnitude =
    //    P.Kf / (stdDev * pow(M_PI * 2, 0.5)) *
    //    (-dist_e.array() * dist_e.array() / (2 * stdDev * stdDev)).exp();

    // b. APPLY EXTERNAL PRESSURE NORMAL TO THE SURFACE

    // auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg->vertexNormals);
    // externalPressure_e = externalPressureMagnitude *
    // vertexAngleNormal_e.row(P.ptInd);

    // c. ALTERNATIVELY, PRESSURE BASED ON INITIAL GEOMETRY + ALONG A FIXED
    // DIRECTION, E.G. NEGATIVE Z DIRECTION

    // initialize/update the external pressure magnitude distribution
    gaussianDistribution(externalPressureMagnitude,
                         geodesicDistanceFromPtInd.raw(),
                         geodesicDistanceFromPtInd.raw().maxCoeff() / P.conc);
    externalPressureMagnitude *= P.Kf;

    Eigen::Matrix<double, 1, 3> zDir;
    zDir << 0.0, 0.0, -1.0;
    externalPressure_e = -externalPressureMagnitude * zDir *
                         (vpg->inputVertexPositions[theVertex].z - P.height);
  }
  return externalPressure_e;
}

EigenVectorX1D System::computeChemicalPotential() {

  Eigen::Matrix<double, Eigen::Dynamic, 1> proteinDensitySq =
      (proteinDensity.raw().array() * proteinDensity.raw().array()).matrix();

  Eigen::Matrix<double, Eigen::Dynamic, 1> dH0dphi =
      (2 * P.H0 * proteinDensity.raw().array() /
       ((1 + proteinDensitySq.array()) * (1 + proteinDensitySq.array())))
          .matrix();

  chemicalPotential.raw() =
      (P.epsilon - (2 * P.Kb * (H.raw() - H0.raw())).array() * dH0dphi.array())
          .matrix();

  return chemicalPotential.raw();
}

std::tuple<EigenVectorX3D, EigenVectorX3D> System::computeDPDForces() {
  // Reset forces to zero
  auto dampingForce_e = gc::EigenMap<double, 3>(dampingForce);
  auto stochasticForce_e = gc::EigenMap<double, 3>(stochasticForce);
  dampingForce_e.setZero();
  stochasticForce_e.setZero();

  // alias positions
  const auto &pos = vpg->inputVertexPositions;

  // std::default_random_engine random_generator;
  // gcs::EdgeData<double> random_var(mesh);
  std::normal_distribution<double> normal_dist(0, P.sigma);

  for (gcs::Edge e : mesh->edges()) {
    gcs::Halfedge he = e.halfedge();
    gcs::Vertex v1 = he.vertex();
    gcs::Vertex v2 = he.next().vertex();

    gc::Vector3 dVel12 = vel[v1] - vel[v2];
    gc::Vector3 dPos12_n = (pos[v1] - pos[v2]).normalize();

    if (P.gamma != 0) {
      gc::Vector3 df = P.gamma * (gc::dot(dVel12, dPos12_n) * dPos12_n);
      dampingForce[v1] -= df;
      dampingForce[v2] += df;
    }

    if (P.sigma != 0) {
      double noise = normal_dist(rng);
      stochasticForce[v1] += noise * dPos12_n;
      stochasticForce[v2] -= noise * dPos12_n;
    }

    // gc::Vector3 dVel21 = vel[v2] - vel[v1];
    // gc::Vector3 dPos21_n = (pos[v2] - pos[v1]).normalize();

    // std::cout << -gamma * (gc::dot(dVel12, dPos12_n) * dPos12_n)
    //           << " == " << -gamma * (gc::dot(-dVel12, -dPos12_n) * -dPos12_n)
    //           << " == " << -gamma * (gc::dot(dVel21, dPos21_n) * dPos21_n)
    //           << std::endl;
  }

  return std::tie(dampingForce_e, stochasticForce_e);
}

void System::computeAllForces() {

  // zero all forces
  gc::EigenMap<double, 3>(bendingPressure).setZero();
  gc::EigenMap<double, 3>(capillaryPressure).setZero();
  gc::EigenMap<double, 3>(lineTensionPressure).setZero();
  gc::EigenMap<double, 3>(externalPressure).setZero();
  gc::EigenMap<double, 3>(regularizationForce).setZero();
  gc::EigenMap<double, 3>(dampingForce).setZero();
  gc::EigenMap<double, 3>(stochasticForce).setZero();
  chemicalPotential.raw().setZero();
  insidePressure = 0;

  if (P.Kb != 0) {
    computeBendingPressure();
  }
  if (P.Kv != 0) {
    computeInsidePressure();
  }
  if (P.Ksg != 0) {
    computeCapillaryPressure();
  }
  if (P.eta != 0) {
    computeLineTensionPressure();
  }
  if ((P.Kse != 0) || (P.Ksl != 0) || (P.Kst != 0)) {
    getRegularizationForce();
  }
  if ((P.gamma != 0) || (P.sigma != 0)) {
    computeDPDForces();
  }
  if (isProtein) {
    computeChemicalPotential();
  }
  if (P.Kf != 0) {
    computeExternalPressure();
  }
}

} // namespace mem3dg