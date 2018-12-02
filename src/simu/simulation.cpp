#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

namespace simu {

bool Simulation::init (void) {
  _step = 0;

  using SRule = genotype::LSystem<genotype::SHOOT>::Rule;
  using RRule = genotype::LSystem<genotype::ROOT>::Rule;

  // Interesting tree
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> ABC+l").toPair(),
//    SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
//    SRule::fromString("B -> Bs").toPair(),
//    SRule::fromString("C -> [f]").toPair(),
//  };
//  _ecosystem.plant.shoot.recursivity = 5;
//  _ecosystem.plant.dethklok = 101;

  // Collision debugging genotype
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> [+Af][-Bf]").toPair(),
//    SRule::fromString("A -> s[+l][-l]A").toPair(),
//    SRule::fromString("B -> s[+l][-l]B").toPair(),
//  };
//  _ecosystem.plant.shoot.recursivity = 5;

  // Randomly generated strange genotype
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("B -> f").toPair(),
//    SRule::fromString("S -> ss-[+l][-l][A]").toPair()
//  };
//  _ecosystem.plant.shoot.recursivity = 5;
//  _ecosystem.plant.root.rules = {
//    RRule::fromString("A -> +++").toPair(),
//    RRule::fromString("B -> EE").toPair(),
//    RRule::fromString("C -> D").toPair(),
//    RRule::fromString("D -> -").toPair(),
//    RRule::fromString("E -> CC").toPair(),
//    RRule::fromString("F -> ++").toPair(),
//    RRule::fromString("S -> A[Bhh][Dh]").toPair()
//  };
//  _ecosystem.plant.root.recursivity = 5;

  _env.init();

  uint N = 3;//_ecosystem.initSeeds;
  float dx = 5 * genotype::Plant::config_t::ls_segmentLength();
  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {
    auto pg = _ecosystem.plant.clone();

    switch (i) {
    case 0:
      pg.shoot.rules = {
        SRule::fromString("S -> ABC+l").toPair(),
        SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
        SRule::fromString("B -> Bs").toPair(),
        SRule::fromString("C -> [f]").toPair(),
      };
      pg.shoot.recursivity = 5;
      pg.dethklok = 101;
      break;
    case 1:
      pg.shoot.rules = {
        SRule::fromString("S -> ABC+l").toPair(),
        SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
        SRule::fromString("B -> Bs").toPair(),
        SRule::fromString("C -> [f]").toPair(),
      };
      pg.shoot.recursivity = 5;
      pg.root.rules = {
        RRule::fromString("S -> AB[+h][-h]").toPair(),
        RRule::fromString("A -> AB[-ABh][+ABh]").toPair(),
        RRule::fromString("B -> Bt").toPair(),
      };
      pg.root.recursivity = 5;
      pg.dethklok = 101;
      break;
    case 2:
      pg.root.rules = {
        RRule::fromString("S -> AB[+h][-h]").toPair(),
        RRule::fromString("A -> AB[-ABh][+ABh]").toPair(),
        RRule::fromString("B -> Bt").toPair(),
      };
      pg.root.recursivity = 5;
      pg.dethklok = 101;
      break;
    }

//    for (uint j=0; j<100; j++)  pg.mutate(_env.dice());
    float initBiomass = Plant::primordialPlantBaseBiomass(pg);
    addPlant(pg, x0 + i * dx, initBiomass);
//    std::cerr << "genome " << i << ": " << pg << std::endl;
  }
  return true;
}

void Simulation::destroy(void) {
  while (!_plants.empty())
    delPlant(_plants.begin()->first);
  _env.destroy();
}

bool Simulation::reset (void) {
  destroy();
  return init();
}

void Simulation::addPlant(const PGenome &p, float x, float biomass) {
  auto pair = _plants.try_emplace(x, std::make_unique<Plant>(p, x, 0, biomass));
  if (pair.second) _env.addCollisionData(pair.first->second.get());
}

void Simulation::delPlant(float x) {
  _env.removeCollisionData(_plants.at(x).get());
  _plants.erase(x);
}

void Simulation::step (void) {
  std::cerr << "## Simulation step " << _step << " ##" << std::endl;

  _env.step();

  std::set<float> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    p->step(_env);
    if (p->isDead())
      corpses.insert(p->pos().x);
  }

  for (float x: corpses)
    delPlant(x);

  _step++;
}

} // end of namespace simu
