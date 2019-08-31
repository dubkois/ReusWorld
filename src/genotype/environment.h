#ifndef ECOSYSTEM_H
#define ECOSYSTEM_H

#include "plant.h"
#include "../cgp/minicgp.h"

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
  genotype::cgp, Inputs,
    D, // Time of year
    Y, // Time of simulation
    X, // X coordinate
    T, // Topology
    H, // Heat
    W, // Water
    G  // Grazing
)

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
  genotype::cgp, Outputs,
    T, // New topology
    H, // New heat
    W, // New water
    G  // New grazing
)

namespace genotype {

class Environment : public EDNA<Environment> {
  APT_EDNA()
public:
  uint rngSeed;

  float width, depth;
  float minT, maxT;
  uint voxels;

  float inertia;

  using CGP = ::cgp::CGP<cgp::Inputs, 100, cgp::Outputs>;
  CGP controller;

  std::string extension (void) const override {
    return ".env.json";
  }
};

DECLARE_GENOME_FIELD(Environment, uint, rngSeed)
DECLARE_GENOME_FIELD(Environment, float, width)
DECLARE_GENOME_FIELD(Environment, float, depth)
DECLARE_GENOME_FIELD(Environment, float, minT)
DECLARE_GENOME_FIELD(Environment, float, maxT)
DECLARE_GENOME_FIELD(Environment, uint, voxels)
DECLARE_GENOME_FIELD(Environment, float, inertia)
DECLARE_GENOME_FIELD(Environment, Environment::CGP, controller)

} // end of namespace genotype

namespace config {

template <>
struct EDNA_CONFIG_FILE(Environment) {
  using Bui = Bounds<uint>;
  using Bf = Bounds<float>;
  DECLARE_PARAMETER(Bui, rngSeedBounds)
  DECLARE_PARAMETER(Bf, widthBounds)
  DECLARE_PARAMETER(Bf, depthBounds)
  DECLARE_PARAMETER(Bf, minTBounds)
  DECLARE_PARAMETER(Bf, maxTBounds)
  DECLARE_PARAMETER(Bui, voxelsBounds)

  DECLARE_PARAMETER(Bf, inertiaBounds)

  using ActiveInputs = std::set<genotype::cgp::Inputs>;
  DECLARE_PARAMETER(ActiveInputs, cgpActiveInputs)
  static const utils::enumbitset<genotype::cgp::Inputs>& activeInputs (void);

  using ActiveOutputs = std::set<genotype::cgp::Outputs>;
  DECLARE_PARAMETER(ActiveOutputs, cgpActiveOutputs)
  static const utils::enumbitset<genotype::cgp::Outputs>& activeOutputs (void);

  DECLARE_SUBCONFIG(config::CGP, genotypeEnvCtrlConfig)

  DECLARE_PARAMETER(MutationRates, mutationRates)
};

} // end of namespace config

#endif // ECOSYSTEM_H
