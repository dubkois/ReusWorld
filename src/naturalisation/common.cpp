#include "common.h"

namespace simu {
namespace naturalisation {

Plant* pclone (const Plant *p) {
  std::map<const Plant*, std::map<const Organ*, Organ*>> olookups;
  return Plant::clone(*p, olookups);
}

} // end of namespace naturalisation
} // end of namespace simu
