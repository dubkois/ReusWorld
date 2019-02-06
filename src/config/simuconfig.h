#ifndef SIMUCONFIG_H
#define SIMUCONFIG_H

#include "kgd/settings/configfile.h"
#include "../genotype/ecosystem.h"

namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(PTree, ptreeConfig)

  DECLARE_SUBCONFIG(genotype::Plant::config_t, genotypePlantConfig)
  DECLARE_SUBCONFIG(genotype::Ecosystem::config_t, genotypeEcosystemConfig)

  DECLARE_PARAMETER(bool, logIndividualStats)
  DECLARE_PARAMETER(bool, logGlobalStats)

  DECLARE_PARAMETER(int, verbosity)

  DECLARE_PARAMETER(uint, initSeeds)
  DECLARE_PARAMETER(uint, stopAtYear)
  DECLARE_PARAMETER(uint, stopAtMinGen)
  DECLARE_PARAMETER(uint, stopAtMaxGen)
  DECLARE_PARAMETER(uint, stepsPerDay)
  DECLARE_PARAMETER(uint, daysPerYear)
  DECLARE_PARAMETER(bool, taurusWorld)

  DECLARE_PARAMETER(bool, killSeedsEarly)
  DECLARE_PARAMETER(float, trimmingProba)
  DECLARE_PARAMETER(float, maxPlantDensity) // Per meter

  DECLARE_PARAMETER(float, assimilationRate)
  DECLARE_PARAMETER(float, baselineShallowWater)
  DECLARE_PARAMETER(float, baselineLight)
  DECLARE_PARAMETER(float, saturationRate)
  DECLARE_PARAMETER(float, lifeCost)
  DECLARE_PARAMETER(float, resourceCost)
  DECLARE_PARAMETER(float, floweringCost) // Relative to base mass

  DECLARE_PARAMETER(uint, updateTopologyEvery)
};

} // end of namespace config

#endif // SIMUCONFIG_H
