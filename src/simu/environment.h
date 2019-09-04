#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "../genotype/environment.h"
#include "physicstypes.hpp"
#include "types.h"

DEFINE_NAMESPACE_PRETTY_ENUMERATION(simu, UndergroundLayers, SHALLOW = 0, DEEP = 1)

namespace simu {

namespace physics {
struct TinyPhysicsEngine;
} // end of namespace physics

struct Plant;
struct Branch;

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
  Voxels _grazing;

  bool _updatedTopology;

  Time _startTime, _currTime, _endTime;

  std::unique_ptr<physics::TinyPhysicsEngine> _physics;

public:
  enum DurationSetType : char {
    APPEND = '+', // Stop time is start + 'duration'
    SET = '=' // Stop time is exactly duration years
  };

  Environment(void);
  ~Environment(void);

  void init (const Genome &g);
  void destroy (void);

  const auto& genome (void) const {
    return _genome;
  }

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

  const auto& dice (void) const {
    return _dice;
  }

  bool hasTopologyChanged (void) const {
    return _updatedTopology;
  }

  const auto& startTime (void) const {
    return _startTime;
  }

  const auto& time (void) const {
    return _currTime;
  }

  const auto& endTime (void) const {
    return _endTime;
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

  const auto& grazing (void) const {
    return _grazing;
  }

  void stepStart (void);
  void stepEnd (void);

  float heightAt (float x) const;
  float temperatureAt (float x) const;
  float waterAt (const Point &p) const;
  float lightAt (float x) const;

  void setDuration (DurationSetType type, uint duration);
  void mutateController (rng::AbstractDice &dice) {
    _genome.controller.mutate(dice);
  }

  const physics::UpperLayer::Items& canopy(const Plant *p) const;

  bool addCollisionData(Plant *p);
  void updateCollisionData (Plant *p);  ///< During plant step when testing for shape validity
  void updateCollisionDataFinal (Plant *p); ///< At the end of the step (no rollback allowed)
  void removeCollisionData (Plant *p);

  physics::CollisionResult
  initialCollisionTest (const Plant *plant) const;

  physics::CollisionResult
  collisionTest (const Plant *plant,
                 const Branch &self, const Branch &branch,
                 const std::set<Organ*> &newOrgans) const;

  void disseminateGeneticMaterial (Organ *f);
  void updateGeneticMaterial (Organ *f, const Point &oldPos);
  void removeGeneticMaterial(Organ *p);
  physics::Pistil collectGeneticMaterial (Organ *f);

  void processNewObjects (void);

  const auto& collisionData (void) const {
    return *_physics;
  }

  void postLoad (void);

  void clone (const Environment &e,
              const std::map<const Plant*, Plant*> &plookup,
              const std::map<const Plant*,
                             std::map<const Organ*,Organ*>> &olookups);

  static void save (nlohmann::json &j, const Environment &e);
  static void load (const nlohmann::json &j, Environment &e);

  friend void assertEqual (const Environment &lhs, const Environment &rhs,
                           bool deepcopy);

  friend void swap (Environment &lhs, Environment &rhs) {
    using std::swap;
    swap(lhs._genome, rhs._genome);
    swap(lhs._dice, rhs._dice);
    swap(lhs._topology, rhs._topology);
    swap(lhs._temperature, rhs._temperature);
    swap(lhs._hygrometry, rhs._hygrometry);
    swap(lhs._grazing, rhs._grazing);
    swap(lhs._updatedTopology, rhs._updatedTopology);
    swap(lhs._startTime, rhs._startTime);
    swap(lhs._currTime, rhs._currTime);
    swap(lhs._endTime, rhs._endTime);
    swap(lhs._physics, rhs._physics);
  }

private:
  void updateVoxel (float &voxel, float newValue);

  float interpolate (const Voxels &voxels, float x) const;

  void cgpStep (void);

  void showVoxelsContents (void) const;
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
