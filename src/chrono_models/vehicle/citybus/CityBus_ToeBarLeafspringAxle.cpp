// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban, Justin Madsen, Daniel Melanz, Shuo He
// =============================================================================
//
// Front and Rear CityBus suspension subsystems
//
// These concrete suspension subsystems are defined with respect to right-handed
// frames with X pointing towards the front, Y to the left, and Z up.
//
// All point locations are provided for the left half of the suspension.
//
// =============================================================================

#include "chrono_models/vehicle/citybus/CityBus_ToeBarLeafspringAxle.h"

namespace chrono {
namespace vehicle {
namespace citybus {

// -----------------------------------------------------------------------------
// Static variables
// -----------------------------------------------------------------------------

const double CityBus_ToeBarLeafspringAxle::m_axleTubeMass = 124.0 * 4.1;
const double CityBus_ToeBarLeafspringAxle::m_spindleMass = 14.705 * 4.1;
const double CityBus_ToeBarLeafspringAxle::m_knuckleMass = 10.0 * 4.1;
const double CityBus_ToeBarLeafspringAxle::m_tierodMass = 5.0 * 4.1;
const double CityBus_ToeBarLeafspringAxle::m_draglinkMass = 5.0 * 4.1;

const double CityBus_ToeBarLeafspringAxle::m_axleTubeRadius = 0.0476;
const double CityBus_ToeBarLeafspringAxle::m_spindleRadius = 0.10;
const double CityBus_ToeBarLeafspringAxle::m_spindleWidth = 0.06;
const double CityBus_ToeBarLeafspringAxle::m_knuckleRadius = 0.05;
const double CityBus_ToeBarLeafspringAxle::m_tierodRadius = 0.02;
const double CityBus_ToeBarLeafspringAxle::m_draglinkRadius = 0.02;

const ChVector3d CityBus_ToeBarLeafspringAxle::m_axleTubeInertia(22.21 * 6.56, 0.0775 * 6.56, 22.21 * 6.56);
const ChVector3d CityBus_ToeBarLeafspringAxle::m_spindleInertia(0.04117 * 6.56, 0.07352 * 6.56, 0.04117 * 6.56);
const ChVector3d CityBus_ToeBarLeafspringAxle::m_knuckleInertia(0.1 * 6.56, 0.1 * 6.56, 0.1 * 6.56);
const ChVector3d CityBus_ToeBarLeafspringAxle::m_tierodInertia(1.0 * 6.56, 0.1 * 6.56, 1.0 * 6.56);
const ChVector3d CityBus_ToeBarLeafspringAxle::m_draglinkInertia(0.1 * 6.56, 1.0 * 6.56, 0.1 * 6.56);

const double CityBus_ToeBarLeafspringAxle::m_springDesignLength = 0.4;
const double CityBus_ToeBarLeafspringAxle::m_springCoefficient = 565480;
const double CityBus_ToeBarLeafspringAxle::m_springRestLength = m_springDesignLength + 0.0621225507207084;
const double CityBus_ToeBarLeafspringAxle::m_springMinLength = m_springDesignLength - 0.10;
const double CityBus_ToeBarLeafspringAxle::m_springMaxLength = m_springDesignLength + 0.10;
const double CityBus_ToeBarLeafspringAxle::m_damperCoefficient = 30276 * 2;
const double CityBus_ToeBarLeafspringAxle::m_damperDegressivityCompression = 3.0;
const double CityBus_ToeBarLeafspringAxle::m_damperDegressivityExpansion = 1.0;
const double CityBus_ToeBarLeafspringAxle::m_axleShaftInertia = 0.4 * 6.56;

// ---------------------------------------------------------------------------------------
// CityBus spring functor class - implements a linear spring + bump stop + rebound stop
// ---------------------------------------------------------------------------------------
class CityBus_SpringForceFront : public ChLinkTSDA::ForceFunctor {
  public:
    CityBus_SpringForceFront(double spring_constant, double min_length, double max_length);

    virtual double evaluate(double time,
                            double rest_length,
                            double length,
                            double vel,
                            const ChLinkTSDA& link) override;

  private:
    double m_spring_constant;
    double m_min_length;
    double m_max_length;

    ChFunctionInterp m_bump;
};

CityBus_SpringForceFront::CityBus_SpringForceFront(double spring_constant, double min_length, double max_length)
    : m_spring_constant(spring_constant), m_min_length(min_length), m_max_length(max_length) {
    // From ADAMS/Car
    m_bump.AddPoint(0.0, 0.0);
    m_bump.AddPoint(2.0e-3, 200.0);
    m_bump.AddPoint(4.0e-3, 400.0);
    m_bump.AddPoint(6.0e-3, 600.0);
    m_bump.AddPoint(8.0e-3, 800.0);
    m_bump.AddPoint(10.0e-3, 1000.0);
    m_bump.AddPoint(20.0e-3, 2500.0);
    m_bump.AddPoint(30.0e-3, 4500.0);
    m_bump.AddPoint(40.0e-3, 7500.0);
    m_bump.AddPoint(50.0e-3, 12500.0);
}

double CityBus_SpringForceFront::evaluate(double time,
                                          double rest_length,
                                          double length,
                                          double vel,
                                          const ChLinkTSDA& link) {
    double force = 0;

    double defl_spring = rest_length - length;
    double defl_bump = 0.0;
    double defl_rebound = 0.0;

    if (length < m_min_length) {
        defl_bump = m_min_length - length;
    }

    if (length > m_max_length) {
        defl_rebound = length - m_max_length;
    }

    force = defl_spring * m_spring_constant + m_bump.GetVal(defl_bump) - m_bump.GetVal(defl_rebound);

    return force;
}

// -----------------------------------------------------------------------------
// CityBus shock functor class - implements a nonlinear damper
// -----------------------------------------------------------------------------
class CityBus_ShockForceFront : public ChLinkTSDA::ForceFunctor {
  public:
    CityBus_ShockForceFront(double compression_slope,
                            double compression_degressivity,
                            double expansion_slope,
                            double expansion_degressivity);

    virtual double evaluate(double time,
                            double rest_length,
                            double length,
                            double vel,
                            const ChLinkTSDA& link) override;

  private:
    double m_slope_compr;
    double m_slope_expand;
    double m_degres_compr;
    double m_degres_expand;
};

CityBus_ShockForceFront::CityBus_ShockForceFront(double compression_slope,
                                                 double compression_degressivity,
                                                 double expansion_slope,
                                                 double expansion_degressivity)
    : m_slope_compr(compression_slope),
      m_degres_compr(compression_degressivity),
      m_slope_expand(expansion_slope),
      m_degres_expand(expansion_degressivity) {}

double CityBus_ShockForceFront::evaluate(double time,
                                         double rest_length,
                                         double length,
                                         double vel,
                                         const ChLinkTSDA& link) {
    // Simple model of a degressive damping characteristic
    double force = 0;

    // Calculate Damping Force
    if (vel >= 0) {
        force = -m_slope_expand / (1.0 + m_degres_expand * std::abs(vel)) * vel;
    } else {
        force = -m_slope_compr / (1.0 + m_degres_compr * std::abs(vel)) * vel;
    }

    return force;
}

CityBus_ToeBarLeafspringAxle::CityBus_ToeBarLeafspringAxle(const std::string& name) : ChToeBarLeafspringAxle(name) {
    m_springForceCB =
        chrono_types::make_shared<CityBus_SpringForceFront>(m_springCoefficient, m_springMinLength, m_springMaxLength);

    m_shockForceCB = chrono_types::make_shared<CityBus_ShockForceFront>(
        m_damperCoefficient, m_damperDegressivityCompression, m_damperCoefficient, m_damperDegressivityExpansion);
}

// -----------------------------------------------------------------------------
// Destructors
// -----------------------------------------------------------------------------
CityBus_ToeBarLeafspringAxle::~CityBus_ToeBarLeafspringAxle() {}

const ChVector3d CityBus_ToeBarLeafspringAxle::getLocation(PointId which) {
    switch (which) {
        case SPRING_A:
            return ChVector3d(0.0, 0.3824, m_axleTubeRadius);
        case SPRING_C:
            return ChVector3d(0.0, 0.3824, m_axleTubeRadius + m_springDesignLength - 0.1);
        case SHOCK_A:
            return ChVector3d(-0.125, 0.441, -0.0507);
        case SHOCK_C:
            return ChVector3d(-0.2, 0.4193, 0.5298 - 0.1);
        case SPINDLE:
            return ChVector3d(0.0, 0.7325 + 0.275, 0.0);
        case KNUCKLE_CM:
            return ChVector3d(0.0, 0.7325 + 0.08, 0.0);
        case KNUCKLE_L:
            return ChVector3d(0.0, 0.7325 + 0.08 + 0.0098058067569092, -0.1);
        case KNUCKLE_U:
            return ChVector3d(0.0, 0.7325 + 0.08 - 0.0098058067569092, 0.1);
        case KNUCKLE_DRL:
            return ChVector3d(0.02909228 * 2, 0.7325 + 0.08 - 0.19787278 * 1.5, 0.2);
        case TIEROD_K:
            return ChVector3d(-0.24777 * 2, 0.7325 + 0.08 - 0.033323 * 1.5, 0.0);
        case DRAGLINK_C:
            return ChVector3d(0.6 + 0.6 + 0.4, 0.7325 + 0.08 - 0.19787278 * 1.5, 0.1);
        default:
            return ChVector3d(0, 0, 0);
    }
}

}  // end namespace citybus
}  // end namespace vehicle
}  // end namespace chrono
