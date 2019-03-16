#include "environment.h"

using namespace genotype;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, rngSeed, "", 0u, std::numeric_limits<uint>::max())
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", 10.f, 1000.f, 1000.f, 10000.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, depth, "", 50, 50)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, minT, "", -20, -20)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, maxT, "", 40, 40)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, voxels, "", 100, 100)
DEFINE_GENOME_FIELD_AS_SUBGENOME(EnvCTRL, envCtrl, "ctrl")

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(rngSeed, .1f),
  MUTATION_RATE(  width, .1f),
  MUTATION_RATE(  depth, .1f),
  MUTATION_RATE(   minT, .1f),
  MUTATION_RATE(   maxT, .1f),
  MUTATION_RATE( voxels, .1f),
  MUTATION_RATE(envCtrl, 1.f)
})

namespace config {
#define CFILE config::EDNAConfigFile<genotype::Environment>
DEFINE_SUBCONFIG(genotype::EnvCTRL::config_t, genotypeEnvCtrlConfig)
#undef CFILE
}

#undef GENOME
