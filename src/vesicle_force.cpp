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
#include <omp.h>
#include <sys/time.h>

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

void System::getVesicleForces() {

  /// 0. GENERAL
  // map the MeshData to eigen matrix XXX_e
  auto bendingPressure_e = gc::EigenMap<double, 3>(bendingPressure);
  auto insidePressure_e = gc::EigenMap<double, 3>(insidePressure);
  auto capillaryPressure_e = gc::EigenMap<double, 3>(capillaryPressure);
  auto lineTensionForce_e = gc::EigenMap<double, 3>(lineTensionPressure);
  auto positions = gc::EigenMap<double, 3>(vpg.inputVertexPositions);
  auto vertexAngleNormal_e = gc::EigenMap<double, 3>(vpg.vertexNormals);

  // Alias
  std::size_t n_vertices = (mesh.nVertices());

// #pragma omp parallel num_threads(3)
//   {
//     int ID = omp_get_thread_num();
//     if (ID == 0) {
//       /// A. BENDING PRESSURE
//       // calculate the Laplacian of mean curvature H
//       Eigen::Matrix<double, Eigen::Dynamic, 1> lap_H_integrated = L * (H - H0);

//       // initialize and calculate intermediary result scalarTerms_integrated
//       Eigen::Matrix<double, Eigen::Dynamic, 1> H_integrated = M * H;
//       Eigen::Matrix<double, Eigen::Dynamic, 1> scalarTerms_integrated =
//           M_inv * rowwiseProduct(M * H_integrated, M * H_integrated) +
//           rowwiseProduct(H_integrated, H0) - vpg.vertexGaussianCurvatures.raw();
//       Eigen::Matrix<double, Eigen::Dynamic, 1> zeroMatrix;
//       zeroMatrix.resize(n_vertices, 1);
//       zeroMatrix.setZero();
//       scalarTerms_integrated =
//           scalarTerms_integrated.array().max(zeroMatrix.array());

//       // initialize and calculate intermediary result productTerms_integrated
//       Eigen::Matrix<double, Eigen::Dynamic, 1> productTerms_integrated;
//       productTerms_integrated.resize(n_vertices, 1);
//       productTerms_integrated =
//           2.0 * rowwiseProduct(scalarTerms_integrated, H - H0);

//       bendingPressure_e =
//           -2.0 * P.Kb *
//           rowwiseScaling(M_inv * (productTerms_integrated + lap_H_integrated),
//                          vertexAngleNormal_e);
//     }
//     if (ID == 1) {
//       /// B. INSIDE EXCESS PRESSURE
//       insidePressure_e =
//           -(P.Kv * (volume - refVolume * P.Vt) / (refVolume * P.Vt) +
//             P.lambdaV) *
//           vertexAngleNormal_e;

//       /// C. CAPILLARY PRESSURE
//       capillaryPressure_e = rowwiseScaling(
//           -(P.Ksg * (surfaceArea - targetSurfaceArea) / targetSurfaceArea +
//             P.lambdaSG) *
//               2.0 * H,
//           vertexAngleNormal_e);
//     }
//     if (ID == 2) {
//       /// E. LOCAL REGULARIZATION
//       gcs::EdgeData<double> lcr(mesh);
//       getCrossLengthRatio(mesh, vpg, lcr);
//       if ((P.Ksl != 0) || (P.Kse != 0) || (P.eta != 0) || (P.Kst != 0)) {
//         // #pragma omp parallel for
//         // for (gcs::Vertex v : mesh.vertices()) {
//         for (int i = 0; i < mesh.nVertices(); i++) {
//           gcs::Vertex v = mesh.vertex(i);
//           // Calculate interfacial tension
//           if ((H0[v.getIndex()] > (0.1 * P.H0)) &&
//               (H0[v.getIndex()] < (0.9 * P.H0)) && (H[v.getIndex()] != 0)) {
//             gc::Vector3 gradient{0.0, 0.0, 0.0};
//             // Calculate gradient of spon curv
//             for (gcs::Halfedge he : v.outgoingHalfedges()) {
//               gradient += vecFromHalfedge(he, vpg).normalize() *
//                           (H0[he.next().vertex().getIndex()] -
//                            H0[he.vertex().getIndex()]) /
//                           vpg.edgeLengths[he.edge()];
//             }
//             gradient.normalize();
//             // Find angle between tangent & principal direction
//             gc::Vector3 tangentVector =
//                 gc::cross(gradient, vpg.vertexNormals[v]).normalize();
//             gc::Vector2 principalDirection1 =
//                 vpg.vertexPrincipalCurvatureDirections[v];
//             gc::Vector3 PD1InWorldCoords =
//                 vpg.vertexTangentBasis[v][0] * principalDirection1.x +
//                 vpg.vertexTangentBasis[v][1] * principalDirection1.y;
//             double cosT = gc::dot(tangentVector, PD1InWorldCoords.normalize());
//             // Deduce normal curvature
//             double K1 =
//                 (2 * H[v.getIndex()] + sqrt(principalDirection1.norm())) * 0.5;
//             double K2 =
//                 (2 * H[v.getIndex()] - sqrt(principalDirection1.norm())) * 0.5;
//             lineTensionPressure[v] = -P.eta * vpg.vertexNormals[v] *
//                                      (cosT * cosT * (K1 - K2) + K2) *
//                                      P.sharpness;
//           }

//           for (gcs::Halfedge he : v.outgoingHalfedges()) {
//             gcs::Halfedge base_he = he.next();

//             // Stretching forces
//             gc::Vector3 edgeGradient = -vecFromHalfedge(he, vpg).normalize();
//             gc::Vector3 base_vec = vecFromHalfedge(base_he, vpg);
//             gc::Vector3 localAreaGradient =
//                 -gc::cross(base_vec, vpg.faceNormals[he.face()]);
//             assert((gc::dot(localAreaGradient, vecFromHalfedge(he, vpg))) < 0);

//             // Conformal regularization
//             if (P.Kst != 0) {
//               gcs::Halfedge jl = he.next();
//               gcs::Halfedge li = jl.next();
//               gcs::Halfedge ik = he.twin().next();
//               gcs::Halfedge kj = ik.next();

//               gc::Vector3 grad_li = vecFromHalfedge(li, vpg).normalize();
//               gc::Vector3 grad_ik = vecFromHalfedge(ik.twin(), vpg).normalize();
//               regularizationForce[v] +=
//                   -P.Kst * (lcr[he.edge()] - targetLcr[he.edge()]) /
//                   targetLcr[he.edge()] *
//                   (vpg.edgeLengths[kj.edge()] / vpg.edgeLengths[jl.edge()]) *
//                   (grad_li * vpg.edgeLengths[ik.edge()] -
//                    grad_ik * vpg.edgeLengths[li.edge()]) /
//                   vpg.edgeLengths[ik.edge()] / vpg.edgeLengths[ik.edge()];
//               // regularizationForce[v] += -P.Kst * localAreaGradient;
//             }

//             if (P.Ksl != 0) {
//               regularizationForce[v] += -P.Ksl * localAreaGradient *
//                                         (vpg.faceAreas[base_he.face()] -
//                                          targetFaceAreas[base_he.face()]) /
//                                         targetFaceAreas[base_he.face()];
//             }

//             if (P.Kse != 0) {
//               regularizationForce[v] +=
//                   -P.Kse * edgeGradient *
//                   (vpg.edgeLengths[he.edge()] - targetEdgeLengths[he.edge()]) /
//                   targetEdgeLengths[he.edge()];
//             }
//           }
//         }
//       }
//     }
//   }

  /// A. BENDING PRESSURE

  // calculate the Laplacian of mean curvature H
  Eigen::Matrix<double, Eigen::Dynamic, 1> lap_H_integrated = L * (H - H0);

  // initialize and calculate intermediary result scalarTerms_integrated
  Eigen::Matrix<double, Eigen::Dynamic, 1> H_integrated = M * H;
  Eigen::Matrix<double, Eigen::Dynamic, 1> scalarTerms_integrated =
      M_inv * rowwiseProduct(M * H_integrated, M * H_integrated) +
      rowwiseProduct(H_integrated, H0) - vpg.vertexGaussianCurvatures.raw();
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
  insidePressure_e =
      -(P.Kv * (volume - refVolume * P.Vt) / (refVolume * P.Vt) + P.lambdaV)
      * vertexAngleNormal_e;

  /// C. CAPILLARY PRESSURE
  capillaryPressure_e = rowwiseScaling(
      -(P.Ksg * (surfaceArea - targetSurfaceArea) / targetSurfaceArea +
        P.lambdaSG) *
          2.0 * H,
      vertexAngleNormal_e);

  /// D. LINE TENSION FORCE

  /// E. LOCAL REGULARIZATION
  gcs::EdgeData<double> lcr(mesh);
  getCrossLengthRatio(mesh, vpg, lcr);

  // struct timeval start, end;
  // gettimeofday(&start, NULL);
  if ((P.Ksl != 0) || (P.Kse != 0) || (P.eta != 0) || (P.Kst != 0)) {
    // #pragma omp parallel for
    // for (gcs::Vertex v : mesh.vertices()) {
    for (int i = 0; i < mesh.nVertices(); i++) {
      gcs::Vertex v = mesh.vertex(i);
      // Calculate interfacial tension
      if ((H0[v.getIndex()] > (0.1 * P.H0)) &&
          (H0[v.getIndex()] < (0.9 * P.H0)) && (H[v.getIndex()] != 0)) {
        gc::Vector3 gradient{0.0, 0.0, 0.0};
        // Calculate gradient of spon curv
        for (gcs::Halfedge he : v.outgoingHalfedges()) {
          gradient +=
              vecFromHalfedge(he, vpg).normalize() *
              (H0[he.next().vertex().getIndex()] -
              H0[he.vertex().getIndex()]) / vpg.edgeLengths[he.edge()];
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
                                 (cosT * cosT * (K1 - K2) + K2) *
                                 P.sharpness;
      }

      for (gcs::Halfedge he : v.outgoingHalfedges()) {
        gcs::Halfedge base_he = he.next();

        // Stretching forces
        gc::Vector3 edgeGradient = -vecFromHalfedge(he, vpg).normalize();
        gc::Vector3 base_vec = vecFromHalfedge(base_he, vpg);
        gc::Vector3 localAreaGradient =
            -gc::cross(base_vec, vpg.faceNormals[he.face()]);
        assert((gc::dot(localAreaGradient, vecFromHalfedge(he, vpg))) < 0);

        // Conformal regularization
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
          // regularizationForce[v] += -P.Kst * localAreaGradient;
        }

        if (P.Ksl != 0) {
          regularizationForce[v] += -P.Ksl * localAreaGradient *
                                    (vpg.faceAreas[base_he.face()] -
                                     targetFaceAreas[base_he.face()]) /
                                    targetFaceAreas[base_he.face()];
        }

        if (P.Kse != 0) {
          regularizationForce[v] +=
              -P.Kse * edgeGradient *
              (vpg.edgeLengths[he.edge()] - targetEdgeLengths[he.edge()]) /
              targetEdgeLengths[he.edge()];
        }
      }
    }
  }
  // gettimeofday(&end, NULL);
  // double delta =
  //     ((end.tv_sec - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec)
  //     / 1.e6;
  // std::cout << "force time: " << delta << std::endl;
}
} // end namespace ddgsolver
