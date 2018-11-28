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

} // end of namespace simu
