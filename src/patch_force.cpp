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
#include <iostream>
#include <math.h>

#include "geometrycentral/surface/simple_polygon_mesh.h"
#include "geometrycentral/surface/surface_mesh.h"
#include <geometrycentral/numerical/linear_solvers.h>
#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/intrinsic_geometry_interface.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>
#include <geometrycentral/utilities/vector3.h>

#include <Eigen/Core>

#include "mem3dg/solver/force.h"
#include "mem3dg/solver/meshops.h"
#include "mem3dg/solver/util.h"

namespace ddgsolver {
namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

void Force::getPatchForces() {

  /// 0. GENERAL
  // map the MeshData to eigen matrix XXX_e
  auto bendingPressure_e = gc::EigenMap<double, 3>(bendingPressure);
  auto insidePressure_e = gc::EigenMap<double, 3>(insidePressure);
  auto capillaryPressure_e = gc::EigenMap<double, 3>(capillaryPressure);
  auto lineTensionForce_e = gc::EigenMap<double, 3>(lineTensionPressure);
  auto positions = gc::EigenMap<double, 3>(vpg.inputVertexPositions);
  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg.vertexNormals);
  Eigen::Matrix<double, Eigen::Dynamic, 1> faceArea_e = vpg.faceAreas.raw();

  // Alias
  std::size_t n_vertices = (mesh.nVertices());
  const gcs::FaceData<gc::Vector3> &face_n = vpg.faceNormals;
  const gcs::FaceData<double> &face_a = vpg.faceAreas;

  /// A. BENDING PRESSURE
  // update the (tufted) mass and conformal Laplacian matrix
  if (isTuftedLaplacian) {
    getTuftedLaplacianAndMass(M, L, mesh, vpg, mollifyFactor);
  } else {
    M = vpg.vertexLumpedMassMatrix;
    L = vpg.cotanLaplacian;
  }
  // Cache the inverse mass matrix
  M_inv = (1 / (M.diagonal().array())).matrix().asDiagonal();

  // update distance
  geodesicDistanceFromAppliedForce =
      heatSolver.computeDistance(mesh.vertex(ptInd));
  if (P.H0 != 0) {
    if (isCircle) {
      tanhDistribution(H0, geodesicDistanceFromAppliedForce.raw(), P.sharpness,
                       P.r_H0[0]);
    } else {
      tanhDistribution(vpg, H0, geodesicDistanceFromAppliedForce.raw(),
                       P.sharpness, P.r_H0);
    }
    H0 *= P.H0;
  }

  // calculate mean curvature
  Eigen::Matrix<double, Eigen::Dynamic, 1> H_integrated =
      rowwiseDotProduct(L * positions / 2.0, vertexAngleNormal_e);
  H = M_inv * H_integrated;

  // Gaussian curvature
  Eigen::Matrix<double, Eigen::Dynamic, 1> &KG_integrated =
      vpg.vertexGaussianCurvatures.raw();

  // calculate the Laplacian of mean curvature H
  Eigen::Matrix<double, Eigen::Dynamic, 1> lap_H_integrated = L * (H - H0);

  // initialize and calculate intermediary result scalarTerms_integrated
  Eigen::Matrix<double, Eigen::Dynamic, 1> scalarTerms_integrated =
      M_inv * rowwiseProduct(H_integrated, H_integrated) +
      rowwiseProduct(H_integrated, H0) - KG_integrated;
  Eigen::Matrix<double, Eigen::Dynamic, 1> zeroMatrix;
  zeroMatrix.resize(n_vertices, 1);
  zeroMatrix.setZero();
  scalarTerms_integrated =
      scalarTerms_integrated.array().max(zeroMatrix.array());

  // initialize and calculate intermediary result productTerms_integrated
  Eigen::Matrix<double, Eigen::Dynamic, 1> productTerms_integrated;
  productTerms_integrated.resize(n_vertices, 1);
  productTerms_integrated =
      2.0 * rowwiseProduct(scalarTerms_integrated, H - H0);

  bendingPressure_e =
      -2.0 * P.Kb *
      rowwiseScaling(M_inv * (productTerms_integrated + lap_H_integrated),
                     vertexAngleNormal_e);

  /// B. INSIDE EXCESS PRESSURE
  volume = 0;
  for (gcs::Face f : mesh.faces()) {
    volume += signedVolumeFromFace(
        f, vpg, refVpg.inputVertexPositions[mesh.vertex(ptInd)]);
  }
  insidePressure_e = P.Kv * vertexAngleNormal_e;

  /// C. CAPILLARY PRESSURE
  surfaceArea = faceArea_e.sum();
  capillaryPressure_e = rowwiseScaling(-P.Ksg * 2.0 * H, vertexAngleNormal_e);

  /// D. LINE TENSION FORCE
  lineTensionPressure.fill({0.0, 0.0, 0.0});
  interArea = 0.0;

  /// E. LOCAL REGULARIZATION
  regularizationForce.fill({0.0, 0.0, 0.0});
  gcs::EdgeData<double> lcr(mesh);
  getCrossLengthRatio(mesh, vpg, lcr);

  if ((P.Kse != 0) || (P.eta != 0) || (P.Kst != 0)) {
    for (gcs::Vertex v : mesh.vertices()) {

      // Calculate interfacial tension
      if ((H0[v.getIndex()] > (0.1 * P.H0)) &&
          (H0[v.getIndex()] < (0.9 * P.H0)) && (H[v.getIndex()] != 0)) {
        gc::Vector3 gradient{0.0, 0.0, 0.0};
        // Calculate gradient of spon curv
        for (gcs::Halfedge he : v.outgoingHalfedges()) {
          gradient +=
              vecFromHalfedge(he, vpg).normalize() *
              (H0[he.next().vertex().getIndex()] - H0[he.vertex().getIndex()]) /
              vpg.edgeLengths[he.edge()];
        }
        gradient.normalize();
        // Find angle between tangent & principal direction
        gc::Vector3 tangentVector =
            gc::cross(gradient, vpg.vertexNormals[v]).normalize();
        gc::Vector2 principalDirection1 =
            vpg.vertexPrincipalCurvatureDirections[v];
        gc::Vector3 PD1InWorldCoords =
            vpg.vertexTangentBasis[v][0] * principalDirection1.x +
            vpg.vertexTangentBasis[v][1] * principalDirection1.y;
        double cosT = gc::dot(tangentVector, PD1InWorldCoords.normalize());
        // Deduce normal curvature
        double K1 =
            (2 * H[v.getIndex()] + sqrt(principalDirection1.norm())) * 0.5;
        double K2 =
            (2 * H[v.getIndex()] - sqrt(principalDirection1.norm())) * 0.5;
        lineTensionPressure[v] = -P.eta * vpg.vertexNormals[v] *
                                 (cosT * cosT * (K1 - K2) + K2) * P.sharpness;
        interArea += vpg.vertexDualAreas[v];
      }

      for (gcs::Halfedge he : v.outgoingHalfedges()) {
        gcs::Halfedge base_he = he.next();

        // Stretching forces
        gc::Vector3 edgeGradient = -vecFromHalfedge(he, vpg).normalize();
        gc::Vector3 base_vec = vecFromHalfedge(base_he, vpg);
        gc::Vector3 localAreaGradient = -gc::cross(base_vec, face_n[he.face()]);
        assert((gc::dot(localAreaGradient, vecFromHalfedge(he, vpg))) < 0);

        // conformal regularization
        if (P.Kst != 0) {
          gcs::Halfedge jl = he.next();
          gcs::Halfedge li = jl.next();
          gcs::Halfedge ik = he.twin().next();
          gcs::Halfedge kj = ik.next();

          gc::Vector3 grad_li = vecFromHalfedge(li, vpg).normalize();
          gc::Vector3 grad_ik = vecFromHalfedge(ik.twin(), vpg).normalize();
          regularizationForce[v] +=
              -P.Kst * (lcr[he.edge()] - targetLcr[he.edge()]) /
              targetLcr[he.edge()] *
              (vpg.edgeLengths[kj.edge()] / vpg.edgeLengths[jl.edge()]) *
              (grad_li * vpg.edgeLengths[ik.edge()] -
               grad_ik * vpg.edgeLengths[li.edge()]) /
              vpg.edgeLengths[ik.edge()] / vpg.edgeLengths[ik.edge()];
          // regularizationForce[v] += - P.Kst * localAreaGradient;
        }

        // the cubic penalty is for regularizing the mesh,
        // need better physical interpretation or alternative method
        if (P.Kse != 0) {
          double strain =
              (vpg.edgeLengths[he.edge()] - targetEdgeLengths[he.edge()]) /
              targetEdgeLengths[he.edge()];
          regularizationForce[v] +=
              -P.Kse * edgeGradient * strain * strain * strain;
        }
      }
    }
  }
}
} // end namespace ddgsolver
