#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "../genotype/ecosystem.h"

namespace simu {
class Environment {
  using Genome = genotype::Environment;
  const Genome &_genome;
public:
  Environment(const Genome &g) : _genome(g) {}

  float width (void) const {
    return _genome.width;
  }

  float height (void) const {
    return 2. * _genome.depth;
  }

  float xextent (void) const {
    return .5 * _genome.width;
  }

  float yextent (void) const {
    return _genome.depth;
  }
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
