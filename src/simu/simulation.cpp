#include "simulation.h"

namespace simu {

bool Simulation::init (void) {
  rng::FastDice dice (0);

  using SRule = genotype::LSystem<genotype::grammar::SHOOT>::Rule;

//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> BAf").toPair(),
//    SRule::fromString("A -> AB[-Cl][+Dl]").toPair(),
//    SRule::fromString("B -> BB").toPair(),
//    SRule::fromString("C -> C-[-l]C").toPair(),
//    SRule::fromString("D -> D+[+l]D").toPair(),
//  };
//  _ecosystem.plant.shoot.recursivity = 5;

  uint N = _ecosystem.initSeeds;
  float dx = 3 * genotype::Plant::config_t::ls_segmentLength();
  float x0 = - dx * N / 2;
  for (uint i=0; i<N; i++) {
    auto pg = _ecosystem.plant.clone();
    for (uint j=0; j<100; j++)  pg.mutate(dice);
    if (int(pg.cdata.id) != 48)  continue;
    addPlant(pg, x0 + i * dx);
    std::cerr << "genome " << i << ": " << pg << std::endl;
  }
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
