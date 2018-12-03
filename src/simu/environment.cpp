#include "environment.h"
#include "tiniestphysicsengine.h"

namespace simu {

using UL_EU = EnumUtils<UndergroundLayers>;

// =============================================================================

Environment::Environment(const Genome &g)
  : _genome(g), _collisionData(std::make_unique<physics::CollisionData>()) {

  for (UndergroundLayers l: UL_EU::iterator())
    _layers[l].resize(_genome.voxels, .5 * (1 + l));
}

Environment::~Environment(void) {}

void Environment::init (void) {
  _dice.reset(_genome.rngSeed);
}

void Environment::destroy (void) {
  _collisionData->reset();
}

void Environment::step (void) {}

float Environment::waterAt(const Position &p) {
  uint v0 = p.start.x * _genome.voxels / _genome.width,
       v1 = p.end.x * _genome.voxels / _genome.width;

  float d0 = p.start.y / _genome.depth,
        d1 = p.end.y / _genome.depth;

  return .5 * (
      _layers[SHALLOW][v0] * uint(d0) + _layers[DEEP][v0] * (1.f - uint(d0))
    + _layers[SHALLOW][v1] * uint(d1) + _layers[DEEP][v1] * (1.f - uint(d1))
  );
}

const physics::UpperLayer::Items& Environment::canopy(const Plant *p) const {
  return _collisionData->canopy(p);
}

void Environment::addCollisionData(Plant *p) {
  _collisionData->addCollisionData(p);
}

void Environment::updateCollisionData(Plant *p) {
  _collisionData->updateCollisions(p);
}

void Environment::updateCollisionDataFinal(Plant *p) {
  _collisionData->updateFinal(p);
}

void Environment::removeCollisionData(Plant *p) {
  _collisionData->removeCollisionData(p);
}

bool Environment::isCollisionFree (const Plant *p) const {
  return _collisionData->isCollisionFree(p);
}

void Environment::disseminateGeneticMaterial(Plant *p, Organ *f) {
  _collisionData->addSpore(p, f);
}

void Environment::removeGeneticMaterial(Plant *p, Organ *f) {
  _collisionData->delSpore(p, f);
}

physics::Spore Environment::collectGeneticMaterial(Plant *p, Organ *f) {
  auto itP = _collisionData->sporesInRange(p, f);
  if (std::distance(itP.first, itP.second) >= 1)
    return *_dice(itP.first, itP.second);

  else
    return physics::Spore();
}

} // end of namespace simu
