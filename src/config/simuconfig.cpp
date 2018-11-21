#include "kgd/apt/core/ptreeconfig.h"

#include "simuconfig.h"

namespace config {
#define CFILE Simulation
DEFINE_SUBCONFIG(PTree, ptreeConfig)
DEFINE_SUBCONFIG(genotype::Plant::config_t, plantGenotypeConfig)
DEFINE_SUBCONFIG(genotype::Environment::config_t, environmentGenotypeConfig)
DEFINE_PARAMETER(uint, stepsPerDay, 1)
#undef CFILE

} // end of namespace config
