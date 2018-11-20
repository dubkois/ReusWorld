#ifndef VISUCONFIG_H
#define VISUCONFIG_H

#include "simuconfig.h"

namespace config {

struct CONFIG_FILE(Visualization) {
  DECLARE_SUBCONFIG(Simulation, simuConfig)
};

} // end of namespace config

#endif // VISUCONFIG_H
