#include "ecosystem.h"

using namespace genotype;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, rngSeed, "", 0u, std::numeric_limits<uint>::max())
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", 1000, 1000)
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
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, initSeeds, "", 100u, 100u)
DEFINE_GENOME_FIELD_AS_SUBGENOME(Environment, env, "")
DEFINE_GENOME_FIELD_AS_SUBGENOME(Plant, plant, "")
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, maxYearDuration, "", 1u, 100u, 100u, 1e6)

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(          plant, 1.f),
  MUTATION_RATE(      initSeeds, 0.f),
  MUTATION_RATE(            env, 0.f),
  MUTATION_RATE(maxYearDuration, 0.f)
})
#undef GENOME
