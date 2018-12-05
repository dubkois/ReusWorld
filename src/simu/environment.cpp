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

float Environment::waterAt(const Point &p) {
  uint v = p.x * _genome.voxels / _genome.width;
  float d = p.y / _genome.depth;
  return _layers[SHALLOW][v] * uint(d) + _layers[DEEP][v] * (1.f - uint(d));
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

void Environment::disseminateGeneticMaterial(Organ *f) {
  _collisionData->addPistil(f);
}

void Environment::updateGeneticMaterial(Organ *f, const Point &oldPos) {
  _collisionData->updatePistil(f, oldPos);
}

void Environment::removeGeneticMaterial(const Point &pos) {
  _collisionData->delPistil(pos);
}

physics::Pistil Environment::collectGeneticMaterial(Organ *f) {
  auto itP = _collisionData->sporesInRange(f);
  if (std::distance(itP.first, itP.second) >= 1)
    return *_dice(itP.first, itP.second);

  else
    return physics::Pistil();
}

} // end of namespace simu
