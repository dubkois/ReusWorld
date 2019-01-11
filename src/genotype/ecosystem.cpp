#include "ecosystem.h"

using namespace genotype;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, rngSeed, "", 0u, std::numeric_limits<uint>::max())
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", 10.f, 1000.f, 1000.f, 10000.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, depth, "", 50, 50)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, voxels, "", 100, 100)

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(rngSeed, 0.f),
  MUTATION_RATE(  width, 0.f),
  MUTATION_RATE(  depth, 0.f),
  MUTATION_RATE( voxels, 0.f)
})
#undef GENOME

#define GENOME Ecosystem
DEFINE_GENOME_FIELD_AS_SUBGENOME(Environment, env, "")
DEFINE_GENOME_FIELD_AS_SUBGENOME(Plant, plant, "")

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(          plant, 1.f),
  MUTATION_RATE(            env, 0.f),
})

namespace config {
#define CFILE config::SAGConfigFile<genotype::Ecosystem>
DEFINE_SUBCONFIG(genotype::Environment::config_t, genotypeEnvironmentConfig)
#undef CFILE
}

#undef GENOME
