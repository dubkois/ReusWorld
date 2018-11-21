#ifndef VISUCONFIG_H
#define VISUCONFIG_H

#include "kgd/apt/core/ptreeconfig.h"

#include "simuconfig.h"

namespace config {

struct CONFIG_FILE(Visualization) {
  DECLARE_SUBCONFIG(Simulation, simuConfig)
  DECLARE_SUBCONFIG(PTree, ptreeConfig)
};

} // end of namespace config

#endif // VISUCONFIG_H
