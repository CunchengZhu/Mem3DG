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

#include <geometrycentral/surface/halfedge_mesh.h>
#include <geometrycentral/surface/heat_method_distance.h>
#include <geometrycentral/surface/intrinsic_geometry_interface.h>
#include <geometrycentral/surface/meshio.h>
#include <geometrycentral/surface/rich_surface_mesh_data.h>
#include <geometrycentral/surface/surface_mesh.h>
#include <geometrycentral/surface/vertex_position_geometry.h>
#include <geometrycentral/utilities/eigen_interop_helpers.h>

#include <Eigen/Core>
#include <Eigen/SparseLU>

#include <pcg_random.hpp>
#include <random>

#include <math.h>
#include <vector>

#include "geometrycentral/surface/halfedge_element_types.h"
#include "geometrycentral/surface/manifold_surface_mesh.h"
#include "geometrycentral/utilities/vector2.h"
#include "geometrycentral/utilities/vector3.h"

#include "mem3dg/constants.h"
#include "mem3dg/macros.h"
#include "mem3dg/mesh_io.h"
#include "mem3dg/meshops.h"
#include "mem3dg/solver/forces.h"
#include "mem3dg/solver/mesh_process.h"
#include "mem3dg/type_utilities.h"

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

namespace mem3dg {

namespace solver {

struct Parameters {
  struct Bending {
    /// Bending modulus
    double Kb = 0;
    /// Constant of bending modulus vs protein density
    double Kbc = 0;
    /// Constant of Spontaneous curvature vs protein density
    double H0c = 0;
    /// type of relation between H0 and protein density, "linear" or "hill"
    std::string relation = "linear";
  };

  struct Tension {
    /// Global stretching modulus
    double Ksg = 0;
    /// Area reservior
    double A_res = 0;
  };

  struct Osmotic {
    /// pressure-volume modulus
    double Kv = 0;
    /// preferred volume
    double Vt = -1;
    /// Ambient Pressure
    double cam = 0;
    /// volume reservoir
    double V_res = 0;
    /// Enclosed solute (atto-mol)
    double n = 1;
  };

  struct Adsorption {
    /// binding energy per protein
    double epsilon = 0;
  };

  struct Dirichlet {
    /// Smooothing coefficients
    double eta = 0;
  };

  struct External {
    /// Magnitude of external force
    double Kf = 0;
    /// level of concentration of the external force
    double conc = -1;
    /// target height
    double height = 0;
  };

  struct DPD {
    /// Dissipation coefficient
    double gamma = 0;
  };

  Bending bending;
  Tension tension;
  Osmotic osmotic;
  Adsorption adsorption;
  Dirichlet dirichlet;
  External external;
  DPD dpd;

  /// (initial) protein density
  EigenVectorX1d protein0 = Eigen::MatrixXd::Constant(1, 1, 1);

  /// Vertex shifting constant
  double Kst = 0;
  /// Local stretching modulus
  double Ksl = 0;
  /// Edge spring constant
  double Kse = 0;

  /// The point
  EigenVectorX1d pt = Eigen::MatrixXd::Constant(1, 1, 0);

  /// mobility constant
  double Bc = 0;

  /// Temperature
  double temp = 0;

  /// domain of integration
  double radius = -1;
  /// augmented Lagrangian parameter for area
  double lambdaSG = 0;
  /// augmented Lagrangian parameter for volume
  double lambdaV = 0;
  /// interior point parameter for protein density
  double lambdaPhi = 1e-9;
  /// sharpness of tanh transition
  double sharpness = 20;
};

struct Options {
  /// Whether or not consider protein binding
  bool isProteinVariation = false;
  /// Whether or not consider shape evolution
  bool isShapeVariation = true;
  /// Whether or not do vertex shift
  bool isVertexShift = false;
  /// Whether adopt preferred volume parametrization
  bool isPreferredVolume = false;
  /// Whether adopt constant osmotic pressure
  bool isConstantOsmoticPressure = false;
  /// Whether adopt constant surface tension
  bool isConstantSurfaceTension = false;
  /// Whether edge flip
  bool isEdgeFlip = false;
  /// Whether split edge
  bool isSplitEdge = false;
  /// Whether collapse edge
  bool isCollapseEdge = false;
  /// Whether floating "the" vertex
  bool isFloatVertex = false;
  /// shape boundary condition: roller, pin, fixed, none
  std::string shapeBoundaryCondition = "none";
  /// protein boundary condition: pin
  std::string proteinBoundaryCondition = "none";
};

} // namespace solver
} // namespace mem3dg
