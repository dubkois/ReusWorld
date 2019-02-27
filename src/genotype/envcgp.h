#ifndef ENVCGP_HPP
#define ENVCGP_HPP

#include "kgd/settings/prettyenums.hpp"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
  genotype::cgp, Inputs,
    DAY, YEAR, COORDINATE, ALTITUDE, TEMPERATURE, HYGROMETRY
)

DEFINE_NAMESPACE_PRETTY_ENUMERATION(
  genotype::cgp, Outputs,
    ALTITUDE_, TEMPERATURE_, HYGROMETRY_
)

#ifdef FULL_CGP
#include "../cgp/cgpwrapper.h"

namespace genotype {
  using EnvCGP = ::cgp::CGPWrapper<
                    EnumUtils<cgp::Inputs>::size(),
                    EnumUtils<cgp::Outputs>::size()>;

} // end of namespace genotype

namespace cgp {

//template <>
//const std::array<std::string, genotype::EnvCGP::INPUTS>
//genotype::EnvCGP::inputNames;

//template <>
//const std::array<std::string, genotype::EnvCGP::OUTPUTS>
//genotype::EnvCGP::outputNames;

//template <>
//void genotype::EnvCGP::demoCGP (const Inputs &/*inputs*/, Outputs &/*outputs*/);
} // end of namespace cgp
#else

#include "kgd/genotype/selfawaregenome.hpp"

namespace genotype {

class EnvCGP : public SelfAwareGenome<EnvCGP> {
  APT_SAG()

  float t_a0, t_m0, t_s0, t_a1, t_m1, t_s1; // Topology
  float h_a, h_f; // Heat
  float w_m, w_s; // Water

  float c0; // Heat decreases with altitude
  float c1; // Water decreases with temperature

public:
  using Inputs = std::array<float, EnumUtils<cgp::Inputs>::size()>;
  using Outputs = std::array<float, EnumUtils<cgp::Outputs>::size()>;

  void process (const Inputs &inputs, Outputs &outputs);
};

DECLARE_GENOME_FIELD(EnvCGP, float, t_a0)
DECLARE_GENOME_FIELD(EnvCGP, float, t_m0)
DECLARE_GENOME_FIELD(EnvCGP, float, t_s0)
DECLARE_GENOME_FIELD(EnvCGP, float, t_a1)
DECLARE_GENOME_FIELD(EnvCGP, float, t_m1)
DECLARE_GENOME_FIELD(EnvCGP, float, t_s1)
DECLARE_GENOME_FIELD(EnvCGP, float, h_a)
DECLARE_GENOME_FIELD(EnvCGP, float, h_f)
DECLARE_GENOME_FIELD(EnvCGP, float, w_m)
DECLARE_GENOME_FIELD(EnvCGP, float, w_s)
DECLARE_GENOME_FIELD(EnvCGP, float, c0)
DECLARE_GENOME_FIELD(EnvCGP, float, c1)

} // end of namespace genotype

namespace config {

template <>
struct SAG_CONFIG_FILE(EnvCGP) {
  using Bf = Bounds<float>;
  DECLARE_PARAMETER(Bf, t_a0Bounds)
  DECLARE_PARAMETER(Bf, t_m0Bounds)
  DECLARE_PARAMETER(Bf, t_s0Bounds)
  DECLARE_PARAMETER(Bf, t_a1Bounds)
  DECLARE_PARAMETER(Bf, t_m1Bounds)
  DECLARE_PARAMETER(Bf, t_s1Bounds)
  DECLARE_PARAMETER(Bf, h_aBounds)
  DECLARE_PARAMETER(Bf, h_fBounds)
  DECLARE_PARAMETER(Bf, w_mBounds)
  DECLARE_PARAMETER(Bf, w_sBounds)
  DECLARE_PARAMETER(Bf, c0Bounds)
  DECLARE_PARAMETER(Bf, c1Bounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif

#endif // ENVCGP_HPP
