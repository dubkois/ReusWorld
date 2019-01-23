#include "environment.h"
#include "tiniestphysicsengine.h"

#include "../config/simuconfig.h"

namespace simu {

using UL_EU = EnumUtils<UndergroundLayers>;

// =============================================================================

Environment::Environment(const Genome &g)
  : _genome(g), _collisionData(std::make_unique<physics::CollisionData>()) {

  _layers[UndergroundLayers::SHALLOW].resize(_genome.voxels, config::Simulation::baselineShallowWater());
  _layers[UndergroundLayers::DEEP].resize(_genome.voxels, 1.f);
}

Environment::~Environment(void) {}

void Environment::init (void) {
  _dice.reset(_genome.rngSeed);
}

void Environment::destroy (void) {
  _collisionData->reset();
}

void Environment::step (void) {
#ifndef NDEBUG
  _collisionData->debug();
#endif
}

float Environment::waterAt(const Point &p) {
  // No water outside the boundaries
  if (p.x < -xextent()) return 0;
  if (xextent() < p.x)  return 0;
  if (p.y < -yextent()) return 0;
  if (0 < p.y)          return 0;

  uint v = (p.x + xextent()) * float(_genome.voxels) / width();
  assert(v < _genome.voxels);
  float d = - p.y / yextent();
  assert(0 <= d && d <= 1);
  return _layers[SHALLOW][v] * uint(d) + _layers[DEEP][v] * (1.f - uint(d));
}

float Environment::lightAt(const Point &p) {
  return config::Simulation::baselineLight();
}

const physics::UpperLayer::Items& Environment::canopy(const Plant *p) const {
  return _collisionData->canopy(p);
}

bool Environment::addCollisionData(Plant *p) {
  return _collisionData->addCollisionData(p);
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

void Environment::removeGeneticMaterial(Organ *f) {
  _collisionData->delPistil(f);
}

physics::Pistil Environment::collectGeneticMaterial(Organ *f) {
  auto itP = _collisionData->sporesInRange(f);

  if (f->plant()->id() == Plant::ID(152182)) {
    std::cerr << OrganID(f) << " found spores: ";
    for (auto it=itP.first; it != itP.second; ++it)
      std::cerr << " " << OrganID(it->organ);
    std::cerr << std::endl;
  }



  if (std::distance(itP.first, itP.second) >= 1)
    return *dice()(itP.first, itP.second);

  else
    return physics::Pistil();
}

} // end of namespace simu
