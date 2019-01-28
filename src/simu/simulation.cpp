#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

#ifndef NDEBUG  /// TODO REMOVE
#include "tiniestphysicsengine.h"
#endif

namespace simu {

using Config = config::Simulation;

static constexpr bool debugPlantManagement = false;
static constexpr bool debugReproduction = false;

static constexpr bool debug = false
  | debugPlantManagement | debugReproduction;

bool Simulation::init (void) {
  _step = 0;

#if 0
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
#endif

  _env.init();
  _ptree.addGenome(_ecosystem.plant);
  genotype::BOCData::setFirstID(_ecosystem.plant.cdata.id);

  uint N = Config::initSeeds();
  float dx = .5; // m
  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {
    auto pg = _ecosystem.plant.clone();
    pg.cdata.sex = (i%2 ? Plant::Sex::MALE : Plant::Sex::FEMALE);
    float initBiomass = Plant::primordialPlantBaseBiomass(pg);

    _ptree.addGenome(pg);
    addPlant(pg, x0 + i * dx, initBiomass);
  }

  updateGenStats();
  return true;
}

void Simulation::destroy(void) {
  Plant::Seeds discardedSeeds;
  while (!_plants.empty())
    delPlant(_plants.begin()->first, discardedSeeds);
  _env.destroy();

  for (Plant::Seed &s: discardedSeeds)  _ptree.delGenome(s.genome);
}

bool Simulation::reset (void) {
  destroy();
  return init();
}

bool Simulation::addPlant(const PGenome &g, float x, float biomass) {
  bool aborted = false;
  Plant *plant = nullptr;
  Plants::iterator pit = _plants.end();

  _stats.newSeeds++;

  // Is the coordinate inside the world ?
  if (!_env.insideXRange(x)) {
    if (Config::taurusWorld()) {  // Wrap around
      while (x < -_env.xextent())  x += _env.width();
      while (x >  _env.xextent())  x -= _env.width();

    } else  aborted = true; // Reject
  }

  if (!aborted) { // Is there room left in the main container? (should be)
    auto pair = _plants.try_emplace(x, std::make_unique<Plant>(g, x, 0));

    if (pair.second) {
      pit = pair.first;
      plant = pit->second.get();

    } else
      aborted = true;
  }

  // Is there room left in the environment?
  if (!aborted) {
    if (_env.addCollisionData(plant)) {
      PStats *pstats = _ptree.getUserData(plant->id());
      plant->init(_env, biomass, pstats);

      if (debugPlantManagement)
        std::cerr << PlantID(plant) << " Added at " << plant->pos() << " with "
                  << biomass << " initial biomass" << std::endl;

      _stats.newPlants++;

    } else {
      _aborted = true;
      _plants.erase(pit);
    }
  }

  if (aborted)  _ptree.delGenome(g);

  return !aborted;
}

void Simulation::delPlant(float x, Plant::Seeds &seeds) {
  Plant *p = _plants.at(x).get();

  p->destroy();
  p->collectCurrentStepSeeds(seeds);

  if (debugPlantManagement) p->autopsy();
  _env.removeCollisionData(p);

  _ptree.delGenome(p->genome());

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
        for (const auto &g: litter) _ptree.addGenome(g);

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
    if (seed.biomass <= 0) {
      _ptree.delGenome(seed.genome);
      continue;
    }

    float dx = 1 + 5 * seed.position.y;
    float x = seed.position.x
        + _env.dice().toss(1.f, -1.f) * _env.dice()(rng::ndist(dx, dx/3));
    addPlant(seed.genome, x, seed.biomass);
  }
}

void Simulation::step (void) {
  std::string ptime = prettyTime(_step);
  if (Config::verbosity() > 0)
    std::cout << std::string(22 + ptime.size(), '#') << "\n"
              << "## Simulation step " << ptime << " ##"
              << std::endl;

  _stats = Stats{};
  _stats.start = std::chrono::high_resolution_clock::now();

  _env.step();

  Plant::Seeds seeds;
  std::set<float> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    p->step(_env);
    if (p->isDead())
      corpses.insert(p->pos().x);
  }

  _stats.deadPlants = corpses.size();
  for (float x: corpses)
    delPlant(x, seeds);

  // Perfom soft trimming
  _stats.trimmed = 0;
  const uint softLimit = Config::maxPlantDensity() * _env.width();
  if (_env.dice()(Config::trimmingProba()) && _plants.size() > softLimit) {
    std::cerr << "Clearing out " << _plants.size() - softLimit << " out of "
              << _plants.size() << " totals" << std::endl;
    while (_plants.size() > softLimit) {
      auto it = _env.dice()(_plants);
      delPlant(it->second->pos().x, seeds);
      _stats.trimmed++;
    }
  }

  performReproductions();

  for (const auto &p: _plants)  p.second->collectCurrentStepSeeds(seeds);
  plantSeeds(seeds);

  _ptree.step(_step, _plants.begin(), _plants.end(),
              [] (const Plants::value_type &pair) {
    return pair.second->genome().cdata.id;
  });

  assert(_env.collisionData().data().size() <= _plants.size());

  updateGenStats();
  if (Config::logGlobalStats())
    logGlobalStats();

  if (finished()) {
    _ptree.saveTo("phylogeny.ptree.json");

    if (Config::verbosity() > 0) {
      std::cout << "Simulation ";
      if (_aborted) std::cout << "canceled by user after";
      else          std::cout << "completed in";
      std::cout << " " << _step << " steps"
                << std::endl;
    }
  }

  _step++;
}

void Simulation::updateGenStats (void) {
  for (const auto &p: _plants) {
    const Plant &plant = *p.second;
    auto gen = plant.genome().cdata.generation;
    _stats.minGeneration = std::min(_stats.minGeneration, gen);
    _stats.maxGeneration = std::max(_stats.maxGeneration, gen);
  }
}

std::string Simulation::prettyTime(uint step) {
  std::ostringstream oss;
  oss << "y" << std::fixed << int(year(step))
      << "d" << std::setw(4) << std::setfill('0') << std::setprecision(1) << day(step);
  return oss.str();
}

void Simulation::logGlobalStats(void) {
  std::ofstream ofs;
  std::ios_base::openmode mode = std::fstream::out;
  if (_step > 0)  mode |= std::fstream::app;
  else            mode |= std::fstream::trunc;
  ofs.open("global.dat", mode);

  if (_step == 0)
    ofs << "Date Time MinGen MaxGen Plants Seeds Females Males Biomass Organs Flowers Fruits Matings"
           " Reproductions dSeeds Births Deaths Trimmed AvgDist AvgCompat"
           " MinX MaxX\n";

  using decimal = Plant::decimal;
  decimal biomass = 0;
  uint seeds = 0;
  uint organs = 0, flowers = 0, fruits = 0;
  uint females = 0, males = 0;
  float minx = _env.xextent(), maxx = -_env.xextent();

  for (const auto &p: _plants) {
    const Plant &plant = *p.second;
    seeds += plant.isInSeedState();
    biomass += plant.biomass();

    organs += plant.organs().size();
    flowers += plant.flowers().size();
    fruits += plant.fruits().size();

    females += (plant.sex() == Plant::Sex::FEMALE);
    males += (plant.sex() == Plant::Sex::MALE);

    float x = plant.pos().x;
    minx = std::min(minx, x);
    maxx = std::max(maxx, x);
  }

  ofs << prettyTime(_step)
      << " " << std::chrono::duration_cast<std::chrono::milliseconds>(
           Stats::clock::now() - _stats.start).count()
      << " " << _stats.minGeneration << " " << _stats.maxGeneration
      << " " << _plants.size() << " " << seeds
      << " " << females << " " << males
      << " " << biomass

      << " " << organs << " " << flowers << " " << fruits

      << " " << _stats.matings << " " << _stats.reproductions
      << " " << _stats.newSeeds << " " << _stats.newPlants
      << " " << _stats.deadPlants << " " << _stats.trimmed

      << " " << _stats.sumDistances / float(_stats.matings)
      << " " << _stats.sumCompatibilities / float(_stats.matings)

      << " " << minx << " " << maxx
      << std::endl;
}

} // end of namespace simu
