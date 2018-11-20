#include "ecosystem.h"

using namespace genotype;
#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, seed, "", 0u, std::numeric_limits<uint>::max())

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(seed, 0.f),
})
#undef GENOME

#define GENOME Ecosystem
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, initSeeds, "", 100u, 100u)
DEFINE_GENOME_FIELD_AS_SUBGENOME(Environment, env, "")
DEFINE_GENOME_FIELD_AS_SUBGENOME(Plant, plant, "")

DEFINE_GENOME_MUTATION_RATES({
  MUTATION_RATE(    plant, 1.f),
  MUTATION_RATE(initSeeds, 0.f),
  MUTATION_RATE(      env, 0.f)
})
#undef GENOME
