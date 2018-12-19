#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

#include "tiniestphysicsengine.h" /// TODO Remove

namespace simu {

using Config = config::Simulation;

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

bool Simulation::addPlant(const PGenome &g, float x, float biomass) {
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
      return true;

    } else
      _plants.erase(pair.first);
  }

  return false;
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
      std::vector<Plant::Genome> litter (mother->genome().seedsPerFruit);
      bool fecundated =
        genotype::bailOutCrossver(mother->genome(), father->genome(), litter,
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
        mother->replaceWithFruit(s.organ, litter, _env);
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
  auto started = std::chrono::high_resolution_clock::now();
  std::cout << std::string(22 + std::ceil(log10(_step+1)), '#') << "\n"
            << "## Simulation step " << prettyTime() << " ("
            << _step << ") ##" << std::endl;

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

  _stats.deadPlants = corpses.size();
  for (float x: corpses)
    delPlant(x);

  // Perfom soft trimming
  uint trimmed = 0;
  const uint softLimit = Config::maxPlantDensity() * _env.width();
  if (_env.dice()(Config::trimmingProba()) && _plants.size() > softLimit) {
    std::cerr << "Clearing out " << _plants.size() - softLimit << " out of "
              << _plants.size() << " totals" << std::endl;
    while (_plants.size() > softLimit) {
      auto it = _env.dice()(_plants);
      delPlant(it->second->pos().x);
      trimmed++;
    }
  }

  performReproductions();
  plantSeeds(seeds);

  assert(_env.collisionData().data().size() <= _plants.size());

  if (Config::logGlobalStats()) {
    std::ofstream ofs;
    std::ios_base::openmode mode = std::fstream::out;
    if (_step > 0)  mode |= std::fstream::app;
    else            mode |= std::fstream::trunc;
    ofs.open("global.dat", mode);

    if (_step == 0)
      ofs << "Date Time Plants Seeds Females Males Biomass Flowers Fruits Matings"
             " Reproductions dSeeds Births Deaths Trimmed AvgDist AvgCompat"
             " MinX MaxX\n";

    using decimal = Plant::decimal;
    decimal biomass = 0;
    uint seeds = 0;
    uint flowers = 0, fruits = 0;
    uint females = 0, males = 0;
    float minx = _env.xextent(), maxx = -_env.xextent();
    for (const auto &p: _plants) {
      const Plant &plant = *p.second;
      seeds += plant.isInSeedState();
      biomass += plant.biomass();
      flowers += plant.flowers().size();
      fruits += plant.fruits().size();
      females += (plant.sex() == Plant::Sex::FEMALE);
      males += (plant.sex() == Plant::Sex::MALE);
      minx = std::min(minx, plant.pos().x);
      maxx = std::max(maxx, plant.pos().x);
    }
    ofs << dayCount()
        << " " << std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::high_resolution_clock::now()-started).count()
        << " " << _plants.size() << " " << seeds << " " << females
        << " " << males << " " << biomass << " " << flowers << " " << fruits
        << " " << _stats.matings << " " << _stats.reproductions
        << " " << _stats.newSeeds << " " << _stats.newPlants
        << " " << _stats.deadPlants << " " << trimmed
        << " " << _stats.sumDistances / float(_stats.matings)
        << " " << _stats.sumCompatibilities / float(_stats.matings)
        << " " << minx << " " << maxx
        << std::endl;
  }

  _step++;
  if (finished()) {
    _ptree.saveTo("ptree.pt");
    std::cout << "Simulation completed in " << _step << " steps"
              << std::endl;
  }
}

std::string Simulation::prettyTime(uint step) {
  std::ostringstream oss;
  oss << "y" << std::fixed << int(year(step))
      << "d" << std::setprecision(1) << day(step);
  return oss.str();
}

} // end of namespace simu
