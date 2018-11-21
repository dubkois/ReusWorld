#include "simulation.h"

namespace simu {

bool Simulation::init (void) {
  uint N = 1;//genome.initSeeds; TODO REMOVE
  float dx = 1. / N;
  for (uint i=0; i<N; i++)
    addPlant(_ecosystem.plant, (i%2?-1:1) * i * dx);
  return true;
}

void Simulation::addPlant(const genotype::Plant &p, float x) {
  _plants.try_emplace(x, std::make_unique<Plant>(p, x, 0));
}

void Simulation::delPlant(float x) {
  _plants.erase(x);
}

} // end of namespace simu
