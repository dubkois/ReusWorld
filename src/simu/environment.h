#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "../genotype/ecosystem.h"
#include "physicstypes.hpp"
#include "types.h"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(simu, UndergroundLayers, SHALLOW = 0, DEEP = 1)

namespace simu {

namespace physics {
struct CollisionData;
} // end of namespace physics

struct Plant;

/// Manages everything related to the external conditions:
///  - abiotic inputs (sunlight, water, temperature, topology)
///  - biotic (inter-plant collisions)
class Environment {
  using Genome = genotype::Environment;
  const Genome &_genome;

  rng::FastDice _dice;

  using Layers = std::array<std::vector<float>, EnumUtils<UndergroundLayers>::size()>;
  Layers _layers;

public:
  Environment(const Genome &g);
  ~Environment(void);

  void init (void);
  void destroy (void);

  auto width (void) const {
    return _genome.width;
  }

  auto height (void) const {
    return 2. * _genome.depth;
  }

  auto xextent (void) const {
    return .5 * _genome.width;
  }

  auto yextent (void) const {
    return _genome.depth;
  }

  auto& dice (void) {
    return _dice;
  }

  void step (void);

  float waterAt(const Point &p);
  const physics::UpperLayer::Items& canopy(const Plant *p) const;

  bool addCollisionData(Plant *p);
  void updateCollisionData (Plant *p);  ///< During plant step when testing for shape validity
  void updateCollisionDataFinal (Plant *p); ///< At the end of the step (no rollback allowed)
  void removeCollisionData (Plant *p);

  bool isCollisionFree (const Plant *p) const;

  void disseminateGeneticMaterial (Organ *f);
  void updateGeneticMaterial (Organ *f, const Point &oldPos);
  void removeGeneticMaterial(const Point &pos);
  physics::Pistil collectGeneticMaterial (Organ *f);

  const auto& collisionData (void) const {
    return *_collisionData;
  }

private:
  std::unique_ptr<physics::CollisionData> _collisionData;
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
