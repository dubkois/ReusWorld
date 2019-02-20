#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "../genotype/ecosystem.h"
#include "physicstypes.hpp"
#include "types.h"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(simu, UndergroundLayers, SHALLOW = 0, DEEP = 1)

namespace simu {

namespace physics {
struct TinyPhysicsEngine;
} // end of namespace physics

struct Plant;

/// Manages everything related to the external conditions:
///  - abiotic inputs (sunlight, water, temperature, topology)
///  - biotic (inter-plant collisions)
class Environment {
  using Genome = genotype::Environment;
  Genome _genome;

  rng::FastDice _dice;

  using Voxels = std::vector<float>;
  using Layers = std::array<Voxels, EnumUtils<UndergroundLayers>::size()>;

  Voxels _topology;
  Voxels _temperature;
  Layers _hygrometry;

  bool _updatedTopology;

  Time _time;

  std::unique_ptr<physics::TinyPhysicsEngine> _physics;

public:
  Environment(void);
  ~Environment(void);

  void init (const Genome &g);
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

  auto insideXRange (float x) const {
    return -xextent() <= x && x <= xextent();
  }

  auto insideYRange (float y) const {
    return -yextent() <= y && y <= yextent();
  }

  auto inside (const Point &p) const {
    return insideXRange(p.x) && insideYRange(p.y);
  }

  auto minTemperature (void) const {
    return _genome.minT;
  }

  auto maxTemperature (void) const {
    return _genome.maxT;
  }

  auto& dice (void) {
    return _dice;
  }

  bool hasTopologyChanged (void) const {
    return _updatedTopology;
  }

  const auto& time (void) const {
    return _time;
  }

  auto voxelCount (void) const {
    return _genome.voxels;
  }

  const auto& topology (void) const  {
    return _topology;
  }

  const auto& temperature (void) const {
    return _temperature;
  }

  const auto& hygrometry (void) const {
    return _hygrometry;
  }

  void step (void);

  float heightAt (float x) const;
  float temperatureAt (float x) const;
  float waterAt (const Point &p) const;
  float lightAt (float x) const;

  const physics::UpperLayer::Items& canopy(const Plant *p) const;

  bool addCollisionData(Plant *p);
  void updateCollisionData (Plant *p);  ///< During plant step when testing for shape validity
  void updateCollisionDataFinal (Plant *p); ///< At the end of the step (no rollback allowed)
  void removeCollisionData (Plant *p);

  bool isCollisionFree (const Plant *p) const;

  void disseminateGeneticMaterial (Organ *f);
  void updateGeneticMaterial (Organ *f, const Point &oldPos);
  void removeGeneticMaterial(Organ *p);
  physics::Pistil collectGeneticMaterial (Organ *f);

  void processNewObjects (void);

  const auto& collisionData (void) const {
    return *_physics;
  }

  friend void save (nlohmann::json &j, const Environment &e);
  static void load (const nlohmann::json &j, Environment &e);

private:
  float interpolate (const Voxels &voxels, float x) const;

  void cgpStep (void);
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
