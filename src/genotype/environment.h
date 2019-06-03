#ifndef ECOSYSTEM_H
#define ECOSYSTEM_H

#include "plant.h"
#include "../cgp/minicgp.h"
#include "envcgp.h"

namespace genotype {

class Environment : public EDNA<Environment> {
  APT_EDNA()
public:
  uint rngSeed;

  float width, depth;
  float minT, maxT;
  uint voxels;

  EnvCTRL envCtrl;

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
DECLARE_GENOME_FIELD(Environment, EnvCTRL, envCtrl)

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

  DECLARE_SUBCONFIG(genotype::EnvCTRL::config_t, genotypeEnvCtrlConfig)

  DECLARE_PARAMETER(MutationRates, mutationRates)
};

} // end of namespace config

#endif // ECOSYSTEM_H
