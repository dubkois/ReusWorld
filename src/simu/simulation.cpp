#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

namespace simu {

static constexpr bool debugPlantManagement = false;
static constexpr bool debugReproduction = false;

static constexpr bool debug = false
  | debugPlantManagement | debugReproduction;

bool Simulation::init (void) {
  _step = 0;

  using SRule = genotype::LSystem<genotype::SHOOT>::Rule;
  using RRule = genotype::LSystem<genotype::ROOT>::Rule;

  // Unbalanced shoot/root rules
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> [-Al][+Al]l").toPair(),
//    SRule::fromString("A -> As[-l][+l]").toPair(),
//  };
//  _ecosystem.plant.shoot.recursivity = 5;
//  _ecosystem.plant.root.rules = {
//    RRule::fromString("S -> [-Ah][+Ah]").toPair(),
//    RRule::fromString("A -> hA[+Ah]").toPair(),
//  };
//  _ecosystem.plant.root.recursivity = 5;
//  _ecosystem.plant.dethklok = 101;

  // Interesting tree
//  _ecosystem.plant.shoot.rules = {
//    SRule::fromString("S -> AB[-Al][+Al]f").toPair(),
//    SRule::fromString("A -> AB[-ABl][+ABl]").toPair(),
//    SRule::fromString("B -> Bs").toPair(),
//  };
//  _ecosystem.plant.shoot.recursivity = 5;
//  _ecosystem.plant.root.rules = {
//    RRule::fromString("S -> [-Ah][+Ah][Ah]").toPair(),
//    RRule::fromString("A -> hA[+Ah]").toPair(),
//  };
//  _ecosystem.plant.root.recursivity = 2;
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

  uint N = _ecosystem.initSeeds;
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
  if (debugPlantManagement) _plants.at(x)->autopsy();
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
    p->update(_env);
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

  if (config::Simulation::logGlobalStats()) {
    std::ofstream ofs;
    std::ios_base::openmode mode = std::fstream::out;
    if (_step > 0)  mode |= std::fstream::app;
    else            mode |= std::fstream::trunc;
    ofs.open("global.dat", mode);

    if (_step == 0)
      ofs << "Plants Biomass FM_ratio Flowers Fruits\n";

    using decimal = Plant::decimal;
    decimal biomass = 0;
    uint flowers = 0, fruits = 0;
    uint females = 0, males = 0;
    for (const auto &p: _plants) {
      const Plant &plant = *p.second;
      biomass += plant.biomass();
      flowers += plant.flowers().size();
      fruits += plant.fruits().size();
      females += (plant.sex() == Plant::Sex::FEMALE);
      males += (plant.sex() == Plant::Sex::MALE);
    }
    ofs << _plants.size() << " " << biomass << " " << float(females) / float(males)
        << " " << flowers << " " << fruits << std::endl;
  }

  _step++;
  if (finished())
    std::cerr << "Simulation completed in " << _step << " steps"
              << std::endl;
}

} // end of namespace simu
