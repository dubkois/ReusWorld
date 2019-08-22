#ifndef SIMUCONFIG_H
#define SIMUCONFIG_H

#include "../genotype/environment.h"

namespace config {
struct PTree;
struct Dependencies;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(PTree, configPTree)

  DECLARE_SUBCONFIG(genotype::Plant::config_t, configGenotypePlant)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, configGenotypeEnvironment)
  DECLARE_SUBCONFIG(Dependencies, configDependencies)

  DECLARE_PARAMETER(int, verbosity)

  DECLARE_PARAMETER(uint, initSeeds)
  DECLARE_PARAMETER(uint, saveEvery)

  DECLARE_PARAMETER(uint, stepsPerDay)
  DECLARE_PARAMETER(uint, daysPerYear)
  DECLARE_PARAMETER(bool, taurusWorld)

  DECLARE_PARAMETER(bool, killSeedsEarly)

  DECLARE_PARAMETER(float, assimilationRate)
  DECLARE_PARAMETER(float, baselineShallowWater)
  DECLARE_PARAMETER(float, baselineLight)
  DECLARE_PARAMETER(float, saturationRate)
  DECLARE_PARAMETER(float, lifeCost)
  DECLARE_PARAMETER(float, photosynthesisCost)
  DECLARE_PARAMETER(float, resourceCost)
  DECLARE_PARAMETER(float, floweringCost) // Relative to base mass
  DECLARE_PARAMETER(float, temperatureMaxRange)
  DECLARE_PARAMETER(float, temperatureRangePenalty) // stddev

  DECLARE_PARAMETER(uint, updateTopologyEvery)  // In tics
  DECLARE_PARAMETER(float, heightPenaltyStddev)

  DECLARE_DEBUG_PARAMETER(bool, DEBUG_NO_METABOLISM, false)
};

} // end of namespace config

#endif // SIMUCONFIG_H
