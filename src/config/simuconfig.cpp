#include "kgd/apt/core/ptreeconfig.h"

#include "simuconfig.h"
#include "dependencies.h"

namespace config {
#define CFILE Simulation
DEFINE_SUBCONFIG(PTree, configPTree)
DEFINE_SUBCONFIG(genotype::Plant::config_t, configGenotypePlant)
DEFINE_SUBCONFIG(genotype::Environment::config_t, configGenotypeEnvironment)
DEFINE_SUBCONFIG(Dependencies, configDependencies)

DEFINE_PARAMETER(int, verbosity, 1)

DEFINE_PARAMETER(uint, saveEvery, 1)

DEFINE_PARAMETER(uint, initSeeds, 100)
DEFINE_PARAMETER(uint, stepsPerDay, 10)
DEFINE_PARAMETER(uint, daysPerYear, 100)
DEFINE_PARAMETER(bool, taurusWorld, false)

DEFINE_PARAMETER(bool, killSeedsEarly, true)

DEFINE_PARAMETER(float, assimilationRate, .01)
DEFINE_PARAMETER(float, baselineShallowWater, .5)
DEFINE_PARAMETER(float, baselineLight, .5)
DEFINE_PARAMETER(float, saturationRate, 1)
DEFINE_PARAMETER(float, lifeCost, .05)
DEFINE_PARAMETER(float, photosynthesisCost, 1.5)
DEFINE_PARAMETER(float, resourceCost, .5)
DEFINE_PARAMETER(float, floweringCost, 3)
DEFINE_PARAMETER(float, temperatureMaxRange, 10)
DEFINE_PARAMETER(float, temperatureRangePenalty, 10)

DEFINE_PARAMETER(uint, updateTopologyEvery, 10)
DEFINE_PARAMETER(float, heightPenaltyStddev, 1)

DEFINE_DEBUG_PARAMETER(bool, DEBUG_NO_METABOLISM, false)

#undef CFILE

} // end of namespace config
