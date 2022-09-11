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
#include "mem3dg/solver/parameters.h"
#include "mem3dg/macros.h"
#include <cstddef>

namespace gc = ::geometrycentral;
namespace gcs = ::geometrycentral::surface;

namespace mem3dg {
namespace solver {

void Parameters::Tension::checkParameters() {
  if (isConstantSurfaceTension) {
    if (A_res != 0) {
      mem3dg_runtime_error(
          "A_res has to be set to 0 to "
          "enable constant surface! Note Ksg is the surface tension directly!");
    }
  }
  if (!(At > 0)) {
    if (Ksg > 0 && !isConstantSurfaceTension) {
      if (At == -1) {
        mem3dg_runtime_error("Target area At has to be specified!");
      } else {
        mem3dg_runtime_error("Target area At has to be greater than zero!");
      }
    }
  }
};

void Parameters::Bending::checkParameters() {
  if (alpha != 0) {
    if (D == 0)
      mem3dg_runtime_error("Membrane thickness D has to be larger than 0!");
  }
}

void Parameters::Osmotic::checkParameters() {
  if (isPreferredVolume && !isConstantOsmoticPressure) {
    if (cam != -1) {
      mem3dg_runtime_error("ambient concentration cam has to be -1 for "
                           "preferred volume parametrized simulation!");
    }
  } else if (!isPreferredVolume && !isConstantOsmoticPressure) {
    if (n == 0) {
      mem3dg_runtime_error("enclosed solute quantity n can not be 0 for "
                           "ambient pressure parametrized simulation!")
    }
    if (Vt != -1 && !isConstantOsmoticPressure) {
      mem3dg_runtime_error("preferred volume Vt has to be -1 for "
                           "ambient pressure parametrized simulation!");
    }
    if (Kv != 0 && !isConstantOsmoticPressure) {
      mem3dg_runtime_error(
          "Kv has to be 0 for ambient pressure parametrized simulation!");
    }

  } else if (!isPreferredVolume && isConstantOsmoticPressure) {
    if (Vt != -1 || V_res != 0 || cam != -1) {
      mem3dg_runtime_error(
          "Vt and cam have to be set to -1 and V_res to be 0 to "
          "enable constant omostic pressure! Note Kv is the "
          "pressure directly!");
    }
  } else {
    mem3dg_runtime_error("preferred volume and constant osmotic pressure "
                         "cannot be simultaneously turned on!");
  }
}

void Parameters::Protein::checkParameters(size_t nVertex) {
  ifPrescribe = (form != NULL);
}

void Parameters::External::checkParameters() { isActivated = (form != NULL); }

void Parameters::Boundary::checkParameters() {
  if (shapeBoundaryCondition != "roller" && shapeBoundaryCondition != "pin" &&
      shapeBoundaryCondition != "fixed" && shapeBoundaryCondition != "none") {
    mem3dg_runtime_error("Invalid option for shapeBoundaryCondition!");
  }
  if (proteinBoundaryCondition != "pin" && proteinBoundaryCondition != "none") {
    mem3dg_runtime_error("Invalid option for proteinBoundaryCondition!");
  }
}

void Parameters::Variation::checkParameters() {
  if (geodesicMask <= 0 && geodesicMask != -1) {
    mem3dg_runtime_error("Radius > 0 or radius = -1 to disable!");
  }
}

void Parameters::checkParameters(bool hasBoundary, size_t nVertex) {
  bending.checkParameters();
  tension.checkParameters();
  osmotic.checkParameters();
  variation.checkParameters();
  // point.checkParameters();
  external.checkParameters();
  protein.checkParameters(nVertex);

  // variation
  if (!variation.isShapeVariation) {
    if (tension.Ksg != 0) {
      mem3dg_runtime_error("Stretching modulus Ksg has to be zero for non "
                           "shape variation simulation!");
    }
    if (osmotic.Kv != 0) {
      mem3dg_runtime_error("Pressure-volume modulus Kv has to be zero for non "
                           "shape variation simulation!");
    }
    if (boundary.shapeBoundaryCondition != "none") {
      mem3dg_runtime_error("Shape boundary condition has to be none for non "
                           "shape variation simulation");
    }
  }

  if (variation.isProteinVariation != (proteinMobility > 0)) {
    mem3dg_runtime_error("proteinMobility value has to be consistent with the "
                         "protein variation option!");
  }

  if (variation.isProteinConservation) {
    if (adsorption.epsilon != 0)
      mem3dg_runtime_message(
          "protein adsorption has no effect when conserve protein!")
  }

  // boundary
  if (hasBoundary) {
    if (boundary.shapeBoundaryCondition == "none" &&
        variation.isShapeVariation) {
      mem3dg_runtime_message(
          "Shape boundary condition type (roller, pin or fixed) "
          "has not been specified for open boundary mesh! May result in "
          "unexpected behavior(e.g. osmotic force). ");
    }
    if (boundary.proteinBoundaryCondition != "pin" &&
        variation.isProteinVariation) {
      mem3dg_runtime_message("Protein boundary condition type (pin) "
                             "has not been specified for open boundary mesh!");
    }
  } else {
    if (tension.A_res != 0 || osmotic.V_res != 0) {
      mem3dg_runtime_error(
          "Closed mesh can not have area and volume reservior!");
    }
    if (boundary.shapeBoundaryCondition != "none") {
      mem3dg_runtime_error(
          "Shape boundary condition type should be disable (= \"none\") "
          "for closed boundary mesh!");
    }
    if (boundary.proteinBoundaryCondition != "none") {
      mem3dg_runtime_error(
          "Protein boundary condition type should be disable (= \"none\") "
          "for closed boundary mesh!");
    }
  }
}

} // namespace solver
} // namespace mem3dg
