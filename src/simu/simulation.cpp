#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

namespace simu {

static constexpr bool debugPlantManagement = true;
static constexpr bool debugReproduction = true;

static constexpr bool debug = false
  | debugPlantManagement | debugReproduction;

bool Simulation::init (void) {
  _step = 0;

  using SRule = genotype::LSystem<genotype::SHOOT>::Rule;
  using RRule = genotype::LSystem<genotype::ROOT>::Rule;

  // Interesting tree
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> AB[Cf]+l").toPair(),
//    SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
//    SRule::fromString("B -> Bs").toPair(),
//    SRule::fromString("C -> C+s").toPair(),
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

  uint N = 5;//_ecosystem.initSeeds;
  float dx = .5; // m
  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {
    auto pg = _ecosystem.plant.clone();
    pg.cdata.sex = (i%2 ? Plant::Sex::MALE : Plant::Sex::FEMALE);

//    switch (i) {
//    case 0:
//      pg.shoot.rules = {
//        SRule::fromString("S -> ABC+l").toPair(),
//        SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
//        SRule::fromString("B -> Bs").toPair(),
//        SRule::fromString("C -> [f]").toPair(),
//      };
//      pg.shoot.recursivity = 5;
//      pg.dethklok = 101;
//      break;
//    case 1:
//      pg.shoot.rules = {
//        SRule::fromString("S -> ABC+l").toPair(),
//        SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
//        SRule::fromString("B -> Bs").toPair(),
//        SRule::fromString("C -> [f]").toPair(),
//      };
//      pg.shoot.recursivity = 5;
//      pg.root.rules = {
//        RRule::fromString("S -> AB[+h][-h]").toPair(),
//        RRule::fromString("A -> AB[-ABh][+ABh]").toPair(),
//        RRule::fromString("B -> Bt").toPair(),
//      };
//      pg.root.recursivity = 5;
//      pg.dethklok = 101;
//      break;
//    case 2:
//      pg.root.rules = {
//        RRule::fromString("S -> AB[+h][-h]").toPair(),
//        RRule::fromString("A -> AB[-ABh][+ABh]").toPair(),
//        RRule::fromString("B -> Bt").toPair(),
//      };
//      pg.root.recursivity = 5;
//      pg.dethklok = 101;
//      break;
//    }

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
  auto pair = _plants.try_emplace(x, std::make_unique<Plant>(p, x, 0));
  if (pair.second) {
    pair.first->second->init(_env, biomass);
    _env.addCollisionData(pair.first->second.get());

    if (debugPlantManagement) {
      Plant *p = pair.first->second.get();
      std::cerr << PlantID(p) << " Added at " << p->pos() << " with "
                << biomass << " initial biomass" << std::endl;
    }
  }
}

void Simulation::delPlant(float x) {
  if (debugPlantManagement) {
    Plant *p = _plants.at(x).get();
    bool spontaneous = p->spontaneousDeath();
    std::cerr << PlantID(p) << " Deleted."
              << " Death was " << (spontaneous ? "spontaneous" : "forced");
    if (spontaneous) {
      std::cerr << "\n\tReserves:";

      using PL = EnumUtils<Plant::Layer>;
      using PE = EnumUtils<Plant::Element>;
      for (auto l: PL::iterator())
        for (auto e: PE::iterator())
          std::cerr << " {" << PL::getName(l)[0] << PE::getName(e)[0]
                    << ": " << 100 * p->concentration(l, e) << "%}";

      std::cerr << "\n\tAge: " << p->age() << " / " << p->genome().dethklok
                << "\n\tOrgans: " << p->organs().size()
                << std::endl;
    }
  }

  _env.removeCollisionData(_plants.at(x).get());
  _plants.erase(x);
}

void Simulation::performReproductions(void) {
  std::vector<Plant*> modifiedPlants;
  if (debugReproduction)
    std::cerr << "Performing reproduction(s)" << std::endl;

  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    Plant *father = it.second.get();
    if (father->sex() == Plant::Sex::FEMALE) continue;

    bool fecundated = false;
    auto &stamens = father->stamens();
    for (auto it = stamens.begin(); it != stamens.end();
         it = fecundated ? stamens.erase(it) : ++it, fecundated = false) {

      Organ *stamen = *it;

      if (stamen->requiredBiomass() > 0)  continue;
      physics::Pistil s = _env.collectGeneticMaterial(stamen);

      if (debugReproduction) {
        if (s.isValid())
          std::cerr << "\t" << OrganID(stamen) << " Got a spore: "
                    << OrganID(s.organ) << std::endl;
        else
          std::cerr << "\t" << OrganID(stamen) << " Could not find a spore"
                    << std::endl;
      }

      if (!s.isValid()) continue;
      if (s.organ->requiredBiomass() > 0)  continue;

      Plant *mother = s.organ->plant();
      Plant::Genome child;
      bool fecundated = genotype::bailOutCrossver(mother->genome(),
                                                  father->genome(),
                                                  child, _env.dice());

      if (debugReproduction) {
        double d = distance(mother->genome(), father->genome());
        double c = mother->genome().compatibility(d);
        assert(c > 0);
        std::cerr << "\tMating " << mother->id() << " with " << father->id()
                  << " (dist=" << d << ", compat=" << c << ")? "
                  << fecundated << std::endl;
      }

      if (fecundated) {
        mother->replaceWithFruit(s.organ, child, _env);
        stamen->accumulate(-stamen->biomass() + stamen->baseBiomass());
        modifiedPlants.push_back(mother);
      }
    }
  }

  for (Plant *p: modifiedPlants)
    _env.updateCollisionDataFinal(p);
}

void Simulation::plantSeeds(Plant::Seeds &seeds) {
  if (debugReproduction && seeds.size() > 0)
    std::cerr << "\tPlanting " << seeds.size() << " seeds" << std::endl;

  for (const Plant::Seed &seed: seeds) {
    float dx = 1 + 5 * seed.position.y;
    float x = seed.position.x
        + _env.dice().toss(1.f, -1.f) * _env.dice()(rng::ndist(dx, dx/3));
    addPlant(seed.genome, x, seed.biomass);
  }
}

void Simulation::step (void) {
  if (debug)
    std::cerr << "## Simulation step " << _step << " ##" << std::endl;

  _env.step();

  Plant::Seeds seeds;
  std::set<float> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    p->step(_env, seeds);
    if (p->isDead())
      corpses.insert(p->pos().x);
  }

  for (float x: corpses)
    delPlant(x);

  performReproductions();
  plantSeeds(seeds);

  _step++;
}

} // end of namespace simu
