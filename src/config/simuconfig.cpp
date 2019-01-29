#include "kgd/apt/core/ptreeconfig.h"

#include "simuconfig.h"

namespace config {
#define CFILE Simulation
DEFINE_SUBCONFIG(PTree, ptreeConfig)
DEFINE_SUBCONFIG(genotype::Plant::config_t, genotypePlantConfig)
DEFINE_SUBCONFIG(genotype::Ecosystem::config_t, genotypeEcosystemConfig)

DEFINE_PARAMETER(bool, logIndividualStats, false)
DEFINE_PARAMETER(bool, logGlobalStats, false)

DEFINE_PARAMETER(int, verbosity, 1)

DEFINE_PARAMETER(uint, initSeeds, 100)
DEFINE_PARAMETER(uint, stopAtYear, 100)
DEFINE_PARAMETER(uint, stopAtMinGen, -1)
DEFINE_PARAMETER(uint, stopAtMaxGen, -1)
DEFINE_PARAMETER(uint, stepsPerDay, 10)
DEFINE_PARAMETER(uint, daysPerYear, 100)
DEFINE_PARAMETER(bool, taurusWorld, false)

DEFINE_PARAMETER(bool, killSeedsEarly, true)
DEFINE_PARAMETER(float, trimmingProba, 0)
DEFINE_PARAMETER(float, maxPlantDensity, 10)

DEFINE_PARAMETER(float, assimilationRate, .01)
DEFINE_PARAMETER(float, baselineShallowWater, .5)
DEFINE_PARAMETER(float, baselineLight, 1)
DEFINE_PARAMETER(float, saturationRate, 1)
DEFINE_PARAMETER(float, lifeCost, .05)
DEFINE_PARAMETER(float, resourceCost, .5)
DEFINE_PARAMETER(float, floweringCost, 3)

#undef CFILE

} // end of namespace config
