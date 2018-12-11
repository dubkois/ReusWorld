#ifndef VISUCONFIG_H
#define VISUCONFIG_H

#include <QSize>

#include "kgd/apt/core/ptreeconfig.h"

#include "simuconfig.h"

namespace config {

struct CONFIG_FILE(Visualization) {
  DECLARE_SUBCONFIG(Simulation, simuConfig)

  DECLARE_PARAMETER(bool, withScreenshots)

  DECLARE_PARAMETER(QSize, screenshotResolution)
};

} // end of namespace config

#endif // VISUCONFIG_H
