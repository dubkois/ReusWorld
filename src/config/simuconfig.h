#ifndef SIMUCONFIG_H
#define SIMUCONFIG_H

#include "kgd/settings/configfile.h"
#include "../genotype/ecosystem.h"

namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(PTree, ptreeConfig)

  DECLARE_SUBCONFIG(genotype::Plant::config_t, plantGenotypeConfig)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, environmentGenotypeConfig)
};

} // end of namespace config

#endif // SIMUCONFIG_H
