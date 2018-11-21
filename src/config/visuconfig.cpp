#include "visuconfig.h"

namespace config {
#define CFILE Visualization
DEFINE_SUBCONFIG(Simulation, simuConfig)
DEFINE_SUBCONFIG(PTree, ptreeConfig)
#undef CFILE

} // end of namespace config
