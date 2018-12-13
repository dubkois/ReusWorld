#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

#include "tiniestphysicsengine.h" /// TODO Remove

namespace simu {

static constexpr bool debugPlantManagement = false;
static constexpr bool debugReproduction = false;

static constexpr bool debug = true
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
  _ptree.addGenome(_ecosystem.plant);

  uint N = _ecosystem.initSeeds;
  float dx = .5; // m
  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {
    auto pg = _ecosystem.plant.clone();
    pg.cdata.sex = (i%2 ? Plant::Sex::MALE : Plant::Sex::FEMALE);
    float initBiomass = Plant::primordialPlantBaseBiomass(pg);

    addPlant(pg, x0 + i * dx, initBiomass);
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

void Simulation::addPlant(const PGenome &g, float x, float biomass) {
  auto pair = _plants.try_emplace(x, std::make_unique<Plant>(g, x, 0));
  _stats.newSeeds++;

  if (pair.second) {
    Plant *p = pair.first->second.get();
    if (_env.addCollisionData(p)) {

      pair.first->second->init(_env, biomass);

      if (debugPlantManagement) {
        std::cerr << PlantID(p) << " Added at " << p->pos() << " with "
                  << biomass << " initial biomass" << std::endl;
      }

      _stats.newPlants++;

    } else
      _plants.erase(pair.first);
  }
}

void Simulation::delPlant(float x) {
  Plant *p = _plants.at(x).get();
  if (debugPlantManagement) p->autopsy();
  _env.removeCollisionData(p);
//  _ptree.delGenome(p->genome());
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

      float distance, compatibility;
      Plant *mother = s.organ->plant();
      Plant::Genome child;
      bool fecundated =
        genotype::bailOutCrossver(mother->genome(), father->genome(), child,
                                  _env.dice(), &distance, &compatibility);

      if (debugReproduction) {
        std::cerr << "\tMating " << mother->id() << " with " << father->id()
                  << " (dist=" << distance << ", compat=" << compatibility
                  << ")? " << fecundated << std::endl;
      }

      _stats.sumDistances += distance;
      _stats.sumCompatibilities += compatibility;
      _stats.matings++;

      if (fecundated) {
//        _ptree.addGenome(p->genome());
        mother->replaceWithFruit(s.organ, child, _env);
        stamen->accumulate(-stamen->biomass() + stamen->baseBiomass());
        modifiedPlants.push_back(mother);

        _stats.reproductions++;
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
  std::cout << std::string(22 + std::ceil(log10(_step+1)), '#') << "\n"
            << "## Simulation step " << _step << " ##" << std::endl;

  _stats = Stats{};

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

  assert(_env.collisionData().data().size() <= _plants.size());

  if (config::Simulation::logGlobalStats()) {
    std::ofstream ofs;
    std::ios_base::openmode mode = std::fstream::out;
    if (_step > 0)  mode |= std::fstream::app;
    else            mode |= std::fstream::trunc;
    ofs.open("global.dat", mode);

    if (_step == 0)
      ofs << "Time Plants Females Males Biomass Flowers Fruits Matings Reproductions"
             " dPlants AvgDist AvgCompat\n";

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
    ofs << dayCount() << " " << _plants.size() << " " << females << " " << males
        << " " << biomass << " " << flowers << " " << fruits
        << " " << _stats.matings << " " << _stats.reproductions
        << " " << _stats.newPlants
        << " " << _stats.sumDistances / float(_stats.matings)
        << " " << _stats.sumCompatibilities / float(_stats.matings)
        << std::endl;
  }

  _step++;
  if (finished()) {
    _ptree.saveTo("ptree.pt");
    std::cout << "Simulation completed in " << _step << " steps"
              << std::endl;
  }
}

} // end of namespace simu
