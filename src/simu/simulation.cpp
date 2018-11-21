#include "simulation.h"

namespace simu {

bool Simulation::init (void) {
  float dx = 3 * genotype::Plant::config_t::ls_segmentLength();
  for (uint i=0; i</*_ecosystem.initSeeds*/1; i++)
    addPlant(_ecosystem.plant, (i%2?-1:1) * i * dx);
  return true;
}

void Simulation::addPlant(const genotype::Plant &p, float x) {
  _plants.try_emplace(x, std::make_unique<Plant>(p, x, 0));
}

void Simulation::delPlant(float x) {
  _plants.erase(x);
}

void Simulation::step (void) {
  // step environment as well

  std::set<float> corpses;
  for (const auto &it: _plants) {
    const Plant_ptr &p = it.second;
    p->step();
    if (p->isDead())  corpses.insert(p->pos().x);
  }

  for (float x: corpses)
    _plants.erase(x);
}

} // end of namespace simu
