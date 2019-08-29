#include "kgd/random/random_iterator.hpp"
#include "kgd/utils/functions.h"

#include "simulation.h"
#include "../config/dependencies.h"

/// TODO Remove
#include "kgd/utils/shell.hpp"

namespace simu {

using Config = config::Simulation;

static constexpr bool debugPlantManagement = false;
static constexpr int debugReproduction = 0;
static constexpr bool debugDeath = false;
static constexpr int debugTopology = 0;
static constexpr bool debugSerialization = false;

static constexpr bool debug = false
  | debugPlantManagement | debugReproduction | debugTopology;

#ifndef NDEBUG
//#define CUSTOM_ENVIRONMENT
//#define CUSTOM_PLANTS -1
//#define DISTANCE_TEST
#endif

#ifdef CUSTOM_ENVIRONMENT
auto instrumentaliseEnvGenome (genotype::Environment g) {
  std::cerr << __PRETTY_FUNCTION__ << " Bypassing environmental genomic values" << std::endl;

//  g.voxels = 10;
//  std::cerr << "genome.env.voxels = " << g.voxels << std::endl;

  return g;
}
#endif

static constexpr auto openMode = std::ofstream::out | std::ofstream::trunc;

std::map<genotype::env_controller::Outputs, stdfs::path> envPaths {
  { genotype::env_controller::T_, "topology.dat" },
  { genotype::env_controller::H_, "temperature.dat" },
  { genotype::env_controller::W_, "hygrometry.dat" }
};

Simulation::Simulation (void)
  : _stats(), _aborted(false), _dataFolder(".") {}

bool Simulation::init (const EGenome &env, PGenome plant) {
  _start = clock::now();

#ifdef INSTRUMENTALISE
    _env.init(instrumentaliseEnvGenome(env));
#else
    _env.init(env);
#endif

  _gidManager.setNext(plant.id());
  plant.gdata.setAsPrimordial(_gidManager);
  _ptree.addGenome(plant);

  uint N = Config::initSeeds();
  float dx = .5; // m

#ifdef CUSTOM_PLANTS
  using SRule = genotype::grammar::Rule_t<genotype::LSystemType::SHOOT>;
  using RRule = genotype::grammar::Rule_t<genotype::LSystemType::ROOT>;
  std::vector<PGenome> genomes;

  PGenome modifiedPrimordialPlant = plant;

  config::Simulation::DEBUG_NO_METABOLISM.ref() = true;

#define SRULE(X) SRule::fromString(X).toPair()
#define RRULE(X) RRule::fromString(X).toPair()

#if CUSTOM_PLANTS == 1
  N = 5;

  modifiedPrimordialPlant.dethklok = 15;
  modifiedPrimordialPlant.shoot.recursivity = 10;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));

  uint i=0;

  genomes[i].shoot.rules = {
    SRULE("S -> s-s-s-A"),
    SRULE("A -> s[B]A"),
    SRULE("B -> [-l][+l]"),
  };
  genomes[i].root.rules = {
    RRULE("S -> A"),
    RRULE("A -> [+Bh][Bh][-Bh]"),
    RRULE("B -> tB"),
  };
  genomes[i].root.recursivity = 10;
  i++;

  genomes[i].dethklok = 25;
  genomes[i].shoot.rules = {
    SRULE("S -> s[-Al][+Bl]"),
    SRULE("A -> -sA"),
    SRULE("B -> +sB"),
  };
  i++;

  genomes[i].shoot.rules = genomes[i-1].shoot.rules;
  genomes[i].shoot.recursivity = 7;
  i++;

  genomes[i].shoot.rules = genomes[i-1].shoot.rules;
  genomes[i].dethklok = 25;
  i++;

  genomes[i].shoot.rules = {
    SRULE("S -> ss+s+s+A"),
    SRULE("A -> s[B]A"),
    SRULE("B -> [-l][+l]")
  };
  genomes[i].root = genomes[0].root;
  i++;

#elif CUSTOM_PLANTS == 2  // Collisions detection
  dx = .5;

  modifiedPrimordialPlant.shoot.recursivity = 10;
  modifiedPrimordialPlant.root.rules = { RRULE("S -> A") };

//  // Left border
//  genomes[i++].shoot.rules = {
//    SRULE("S -> A[-f][+f]f"),
//    SRULE("A -> sss")
//  };

  // (C1) Overlap
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> s[l][l]")
  };

  // (C1) Diamond overlap
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> Af"),
    SRULE("A -> [-l++l][+l--l]"),
  };

  // (C2) Two step overlap
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> [Al][-l]"),
    SRULE("A -> -"),
  };

  // (C3) Diamond overlap (branch end)
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> A-l++l"),
    SRULE("A -> [+l--l]"),
  };

  // (C3) Diamond overlap (two rules)
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> [-Al][+Bl]"),
    SRULE("A -> l++"),
    SRULE("B -> l--"),
  };


  // Two step without overlap
  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes.back().shoot.rules = {
    SRULE("S -> [Al][-Al]"),
    SRULE("A -> -"),
  };

//  // Multi-layered flower
//  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
//  genomes.back().shoot.rules = {
//  genomes.back().shoot.recursivity = 5;
//    SRULE("S -> [f][A][B]"),
//    SRULE("A -> A-[l]"),
//    SRULE("B -> B+[l]"),
//  };
//  genomes[i].root.rules = {
//    RRULE("S -> h")
//  };
//  i++;

//  // A
//  genomes[i].shoot.rules = {
//    SRULE("S -> [-s----A][---f]"),
//    SRULE("A -> sB"),
//    SRULE("B -> s"),
//  };
//  genomes[i].root.rules = {
//    RRULE("S -> -t")
//  };
//  i++;

//  // K
//  genomes[i].shoot.rules = {
//    SRULE("S -> [s][-A]"),
//    SRULE("A -> s"),
//  };
//  genomes[i].root.rules = {
//    RRULE("S -> [t][+A]"),
//    RRULE("A -> t")
//  };
//  i++;

//  // G
//  genomes[i].shoot.rules = {
//    SRULE("S -> s---A"),
//    SRULE("A -> s"),
//  };
//  genomes[i].root.rules = {
//    RRULE("S -> t+++t+++A"),
//    RRULE("A -> t++++h")
//  };
//  i++;

//  // D
//  genomes[i].shoot.rules = {
//    SRULE("S -> s----A"),
//    SRULE("A -> s--B"),
//    SRULE("B -> s")
//  };
//  genomes[i].root.recursivity = 2;
//  genomes[i].root.rules = {
//    RRULE("S -> t++++A"),
//    RRULE("A -> t"),
//  };
//  i++;

  N = genomes.size();

#elif CUSTOM_PLANTS == 3  // Canopy exemple
  N = 3;
  uint m = N/2;
  dx = .1;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));

  genomes[0].shoot.rules = {
    SRULE("S -> -l")
  };
  genomes[0].root.rules = {
    RRULE("S -> h")
  };
  genomes[0].dethklok = -1;

  for (uint i=1; i<N; i++) {
    if (i != m) {
      genomes[i].shoot.rules = genomes[0].shoot.rules;
      genomes[i].root.rules = genomes[0].root.rules;
      genomes[i].dethklok = genomes[0].dethklok;
    }
  }

  genomes[m].shoot.rules = {
    SRULE("S -> s[-Al][+Bl]"),
    SRULE("A -> s[+++f]"),
    SRULE("B -> s[---f]")
  };
  genomes[m].root.rules = {
    RRULE("S -> h")
  };
  genomes[m].dethklok = -1;

#elif CUSTOM_PLANTS == 4  // Pistils detection
  N = 9;
  for (uint i=0; i<N; i++)
    genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));

  rng::FastDice dice (1);
  for (uint i=0; i<N; i++) {
    PGenome &g = genomes[i];
    g.cdata.sex = genotype::BOCData::Sex(i%2);

    std::string r = "S -> " + std::string(dice(0, 5), 's') + "f";
    g.shoot.rules = { SRULE(r) };
    g.root.rules = { RRULE("S -> A") };
  }


#else
  N = 1;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone(_gidManager));
  genomes[0].dethklok = -1;
#endif
#endif

#ifdef DISTANCE_TEST
  std::cerr << "Distance matrix:\n\t    ";
  for (uint i=0; i<N; i++)
    std::cerr << " " << std::setw(4) << i;
  std::cerr << "\n";
  for (uint i=0; i<N; i++) {
    std::cerr << "\t" << std::setw(4) << i;
    for (uint j=0; j<N; j++)
      std::cerr << " " << std::setprecision(2) << std::setw(4)
                << distance(genomes[i], genomes[j]);
    std::cerr << "\n";
  }
  std::cerr << std::endl;
  exit(255);
#endif

  std::vector<Plant*> newborns;
  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {

#ifdef CUSTOM_PLANTS
    auto pg = genomes[i%genomes.size()];
#else
    auto pg = plant.clone(_gidManager);
    pg.gdata.updateAfterCloning(_gidManager);
#endif

    pg.cdata.sex = (i%2 ? Plant::Sex::MALE : Plant::Sex::FEMALE);
    float initBiomass = Plant::primordialPlantBaseBiomass(pg);

    Plant *p = addPlant(pg, x0 + i * dx, initBiomass);
    if (p)  newborns.push_back(p);
  }

  postInsertionCleanup(newborns);
  updateGenStats();

  return true;
}

void Simulation::destroy (void) {
  Plant::Seeds discardedSeeds;
  while (!_plants.empty())
    delPlant(*_plants.begin()->second, discardedSeeds);
  _env.destroy();
}

Plant* Simulation::addPlant(const PGenome &g, float x, float biomass) {
  bool insertionAborted = false;
  Plant *plant = nullptr;
  Plants::iterator pit = _plants.end();

  _stats.newSeeds++;

  // Is the coordinate inside the world ?
  if (!_env.insideXRange(x)) {
    if (Config::taurusWorld()) {  // Wrap around
      while (x < -_env.xextent())  x += _env.width();
      while (x >  _env.xextent())  x -= _env.width();

    } else  insertionAborted = true; // Reject
  }

  if (!insertionAborted) { // Is there room left in the main container? (should be)
    Point pos {x, _env.heightAt(x)};
    auto pair = _plants.try_emplace(x, std::make_unique<Plant>(g, pos));

    if (pair.second) {
      pit = pair.first;
      plant = pit->second.get();

    } else
      insertionAborted = true;
  }

  // Is there room left in the environment?
  if (!insertionAborted) {
    if (_env.addCollisionData(plant)) {
      auto pd = _ptree.addGenome(plant->genome());
      plant->init(_env, biomass, pd);

      if (debugPlantManagement)
        std::cerr << PlantID(plant) << " Added at " << plant->pos() << " with "
                  << biomass << " initial biomass" << std::endl;

      _stats.newPlants++;

    } else {
      insertionAborted = true;
      _plants.erase(pit);
    }
  }

  return insertionAborted ? nullptr : plant;
}

void Simulation::delPlant(Plant &p, Plant::Seeds &seeds) {
  p.destroy();
  p.collectCurrentStepSeeds(seeds);

  if (debugPlantManagement) p.autopsy();
  _env.removeCollisionData(&p);

  _ptree.delGenome(p.genome());

  _plants.erase(p.pos().x);
}

void Simulation::postInsertionCleanup(std::vector<Plant*> newborns) {
  // Notify the physics engine
  _env.processNewObjects();

  // Shuffle the vector
  _env.dice().shuffle(newborns);

  // Now test that there is enough space
  Plant::Seeds discardedSeeds;
  for (Plant *p: newborns)
    if (!p->isInSeedState()
        && _env.initialCollisionTest(p) != physics::NO_COLLISION)
      delPlant(*p, discardedSeeds);
}

void Simulation::performReproductions(void) {
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

      if (debugReproduction > 1) {
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
        if (debugReproduction)  std::cerr << "\t\tOffsprings:";
        for (auto &g: litter) {
          g.genealogy().updateAfterCrossing(mother->genealogy(),
                                            father->genealogy(), _gidManager);
          _ptree.registerCandidate(g.genealogy());
          if (debugReproduction)  std::cerr << " " << g.id();
        }
        if (debugReproduction)  std::cerr << std::endl;

        mother->replaceWithFruit(s.organ, litter, _env);
        mother->updateGeometry(); /// TODO Probably multiple updates... not great
        mother->update(_env);

        father->resetStamen(stamen);

        _stats.reproductions++;
      }
    }
  }
}

void Simulation::plantSeeds(const Plant::Seeds &seeds) {
  using utils::gauss;

  static constexpr int debug = debugReproduction | debugTopology;

  static const bool& taurusWorld = Config::taurusWorld();
  static const float& dhsd = Config::heightPenaltyStddev();

  if (debugReproduction && seeds.size() > 0)
    std::cerr << "\tPlanting " << seeds.size() << " seeds" << std::endl;

  std::vector<Plant::Seed> unplanted;
  std::vector<Plant*> newborns;

  for (const Plant::Seed &seed: seeds) {
    if (seed.biomass <= 0) {
      unplanted.push_back(seed);
      continue;
    }

    float startH = _env.heightAt(seed.position.x), maxH = startH, h = startH;
    float avgDx = 1 + 5 * std::max(0.f, seed.position.y - h);
    float stdDx = avgDx / 3.;
    float dist = avgDx + 4 * stdDx;

    if (debug)
      std::cerr << "\tvalues = gauss(d," << avgDx << ", " << stdDx << ")"
                << " * gauss(dH,0," << dhsd << ")\n";
    if (debugTopology)  std::cerr << "Samples:\n";

    uint samplings = 101;
    uint hsamplings = samplings  / 2;
    std::vector<float> values (samplings, 0);

    // Sweep middle to left
    for (uint i=0; i<=hsamplings; i++) {
      uint i_ = hsamplings-i;
      float d = -2 * float(i) * dist / samplings;
      float x = seed.position.x + d;

      if (debugTopology)  std::cerr << /*i << " " << i_ << " " <<*/ x;

      if (!(taurusWorld || _env.insideXRange(x))) {
        values[i_] = 0;

        if (debugTopology)
          std::cerr << " " << NAN << " " << NAN
                    << " " << NAN
                    << " " << NAN
                    << " " << NAN << "\n";

      } else {
        h = _env.heightAt(seed.position.x + d);
        if (maxH < h) maxH = h;

        values[i_] = gauss(d, -avgDx, stdDx) * gauss(maxH - startH, 0.f, dhsd);

        if (debugTopology)
          std::cerr << " " << h << " " << maxH
                    << " " << gauss(d, -avgDx, stdDx)
                    << " " << gauss(maxH - startH, 0.f, dhsd)
                    << " " << values[i_] << "\n";
      }

    }

    // Sweep middle to right
    maxH = startH;
    for (uint i=hsamplings+1; i<samplings; i++) {
      float d = 2 * float(i - hsamplings) * dist / samplings;
      float x = seed.position.x + d;

      if (debugTopology)  std::cerr << /*i << " " << i << " " <<*/ x;

      if (!(taurusWorld || _env.insideXRange(x))) {
        values[i] = 0;

        if (debugTopology)
          std::cerr << " " << NAN << " " << NAN
                    << " " << NAN
                    << " " << NAN
                    << " " << NAN << "\n";

      } else {
        h = _env.heightAt(x);
        if (maxH < h) maxH = h;

        values[i] = gauss(d, avgDx, stdDx) * gauss(maxH - startH, 0.f, dhsd);

        if (debugTopology)
          std::cerr << " " << h << " " << maxH
                    << " " << gauss(d, avgDx, stdDx)
                    << " " << gauss(maxH - startH, 0.f, dhsd)
                    << " " << values[i] << "\n";
      }

    }
    if (debugTopology)  std::cerr << std::endl;

    rng::rdist rdist (values.begin(), values.end());
    uint voxel = _env.dice()(rdist);
    float noise = _env.dice()(-.5f, .5f);
    float x = seed.position.x
        + ((2 * voxel + noise) / samplings - 1) * dist;

    if (debug)
      std::cerr << "\tx = " << x << " = ((2 * (" << voxel << " + " << noise << ") / "
                << samplings << " - 1) * " << dist << std::endl;

    if (debug)
      std::cerr << "Planted seed " << seed.genome.id() << " at " << x
                << " (extracted from plant " << seed.genome.gdata.mother.gid
                << " at position " << seed.position.x << ")" << std::endl;

    // First insert regardless of space
    Plant *p = addPlant(seed.genome, x, seed.biomass);
    if (p)  newborns.push_back(p);
  }

  for (const Plant::Seed &seed: unplanted)
    _ptree.unregisterCandidate(seed.genome.genealogy());

  postInsertionCleanup(newborns);
}

void Simulation::printStepHeader (void) const {
  static const auto CTSIZE = utils::CurrentTime::width();

  std::string ptime = _env.time().pretty();
  std::cout << std::string(22 + ptime.size() + 4 + CTSIZE, '#') << "\n"
            << "## Simulation step " << ptime
            << " at " << utils::CurrentTime{} << " ##"
            << std::endl;
}

void Simulation::step (void) {
  if (Config::verbosity() > 0)
    printStepHeader();

  _stats = Stats{};
  _stats.start = std::chrono::high_resolution_clock::now();

  _env.stepStart();

  Plant::Seeds seeds;
  std::set<Plant*> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    _stats.derivations += p->step(_env);
    if (p->isDead()) {
      if (debugDeath) p->autopsy();
      corpses.insert(p.get());
    }
  }

  _stats.deadPlants = corpses.size();
  for (Plant *p: corpses)
    delPlant(*p, seeds);

#if !CUSTOM_PLANTS
  performReproductions();

  for (const auto &p: _plants)
    if (p.second->hasUncollectedSeeds())
      p.second->collectCurrentStepSeeds(seeds);
  if (!seeds.empty()) plantSeeds(seeds);
#endif

  _ptree.step(_env.time().toTimestamp(), _plants.begin(), _plants.end(),
              [] (const Plants::value_type &pair) {
    return pair.second->species();
  });

  updateGenStats();
  logToFiles();

  if (_env.hasTopologyChanged()) {
    for (const auto &it: _plants) {
      const auto pos = it.second->pos();
      float h = _env.heightAt(pos.x);
      if (pos.y != h)
        updatePlantAltitude(*it.second, h);
    }
  }

  _env.stepEnd();

  if (_env.time().isStartOfYear()
    && (_env.time().year() % Config::saveEvery()) == 0)
    save(periodicSaveName());
}

void Simulation::atEnd(void) {
  // Update once more so that data goes from y0d0h0 to yLd0h0 with L = stopAtYear()
  if (_env.startTime() < _env.time()) {
    _stats.start = clock::now();
    logToFiles();
  }

  _ptree.step(_env.time().toTimestamp(), _plants.begin(), _plants.end(),
              [] (const Plants::value_type &pair) {
    return pair.second->species();
  });

  stdfs::path ptreePath = dataFolder() / "phylogeny.ptree.json";
  std::ofstream ptreeFile (ptreePath, openMode);
  if (!ptreeFile.is_open())
    utils::doThrow<std::invalid_argument>(
      "Unable to open ptree file ", ptreePath);
  _ptree.saveTo(ptreeFile);

  if (Config::verbosity() > 0) {
    std::cout << "Simulation ";
    if (_aborted) std::cout << "canceled by user ";
    else          std::cout << "completed ";

    std::cout << "at " << _env.time() << " (simulated ";
    const Time &time = _env.time() - _env.startTime();
    std::cout << " " << time.year() << " year";
    if (time.year() > 1)  std::cout << "s";
    std::cout << " " << time.day() << " day";
    if (time.day() > 1)   std::cout << "s";
    std::cout << " " << time.hour() << " hour";
    if (time.hour() > 1)  std::cout << "s";
    std::cout << " = " << time.toTimestamp() << " steps, wall time = "
              << wallTimeDuration() << " ms)" << std::endl;
  }
}

void Simulation::updatePlantAltitude(Plant &p, float h) {
  p.updateAltitude(_env, h);
}

void Simulation::setDataFolder (const stdfs::path &path, Overwrite o) {
  _dataFolder = path;

  if (stdfs::exists(_dataFolder) && !stdfs::is_empty(_dataFolder)) {
    if (o == UNSPECIFIED) {
      std::cerr << "WARNING: data folder '" << _dataFolder << "' is not empty."
                   " What do you want to do ([a]bort, [p]urge)?"
                << std::flush;
      o = Overwrite(std::cin.get());
    }

    switch (o) {
    case PURGE: std::cerr << "Purging contents from " << _dataFolder
                          << std::endl;
                stdfs::remove_all(_dataFolder);
                break;

    case ABORT: _aborted = true;  break;

    default:  std::cerr << "Invalid overwrite option '" << o
                        << "' defaulting to abort." << std::endl;
              _aborted = true;
    }
  }
  if (_aborted) return;

  if (!stdfs::exists(_dataFolder))  stdfs::create_directories(_dataFolder);

  stdfs::path statsPath = path / "global.dat";
  if (_statsFile.is_open()) _statsFile.close();
  _statsFile.open(statsPath, openMode);
  if (!_statsFile.is_open())
    utils::doThrow<std::invalid_argument>(
      "Unable to open stats file ", statsPath);

  using O = genotype::env_controller::Outputs;
  for (O o: EnumUtils<O>::iterator()) {
    stdfs::path envPath = path / envPaths.at(o);
    if (_envFiles[o].is_open()) _envFiles[o].close();
    _envFiles[o].open(envPath, openMode);
    if (!_envFiles[o].is_open())
      utils::doThrow<std::invalid_argument>(
        "Unable to open voxel file ", o, " ", envPath);
  }
}

void Simulation::updateGenStats (void) {
  for (const auto &p: _plants) {
    const Plant &plant = *p.second;
    auto gen = plant.genome().gdata.generation;
    _stats.minGeneration = std::min(_stats.minGeneration, gen);
    _stats.maxGeneration = std::max(_stats.maxGeneration, gen);
  }
}

void Simulation::logToFiles (void) {
  if (_statsFile.is_open())  logGlobalStats();
  logEnvState();
}

void Simulation::logGlobalStats(void) {
  bool header = (_env.startTime() == _env.time());

  if (header)
    _statsFile << "Date Time MinGen MaxGen Plants Seeds Females Males Biomass"
                  " Derivations Organs Flowers Fruits Matings"
                  " Reproductions dSeeds Births Deaths AvgDist AvgCompat"
                  " ASpecies CSpecies MinX MaxX\n";

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

  _statsFile << _env.time().pretty()
             << " " << duration(_stats.start)
             << " " << _stats.minGeneration << " " << _stats.maxGeneration
             << " " << _plants.size() << " " << seeds
             << " " << females << " " << males
             << " " << biomass

             << " " << _stats.derivations
             << " " << organs << " " << flowers << " " << fruits

             << " " << _stats.matings << " " << _stats.reproductions
             << " " << _stats.newSeeds << " " << _stats.newPlants
             << " " << _stats.deadPlants

             << " " << _stats.sumDistances / float(_stats.matings)
             << " " << _stats.sumCompatibilities / float(_stats.matings)

             << " " << _ptree.width() << " " << _ptree.aliveSpecies().size()

             << " " << minx << " " << maxx
             << std::endl;

//  debugPrintAll();
}

void doLog (std::ostream &os, const simu::Time &time,
            const std::vector<float> &data) {
  os << time.pretty();
  for (float f: data) os << " " << f;
  os << "\n";
}

void Simulation::logEnvState(void) {
  using O = genotype::env_controller::Outputs;
  for (O o: EnumUtils<O>::iterator()) {
    std::ofstream &ofs = _envFiles[o];
    if (!ofs.is_open()) continue;

    switch (o) {
    case O::T_: doLog(ofs, _env.time(), _env.topology());  break;
    case O::H_: doLog(ofs, _env.time(), _env.temperature());  break;
    case O::W_: doLog(ofs, _env.time(), _env.hygrometry()[SHALLOW]);  break;
    }
  }
}

// Low level writing of a bytes array
bool save (const std::string &file, const std::vector<uint8_t> &bytes) {
  std::fstream ofs (file, std::ios::out | std::ios::binary);
  if (!ofs)
    utils::doThrow<std::invalid_argument>(
          "Unable to open '", file, "' for writing");

  ofs.write((char*)bytes.data(), bytes.size());
  ofs.close();

  return true;
}

// Low level loading of a bytes array
bool load (const std::string &file, std::vector<uint8_t> &bytes) {
  std::ifstream ifs(file, std::ios::binary | std::ios::ate);
  if (!ifs)
    utils::doThrow<std::invalid_argument>(
          "Unable to open '", file, "' for reading");

  std::ifstream::pos_type pos = ifs.tellg();

  bytes.resize(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read((char*)bytes.data(), pos);

  return true;
}


using json = nlohmann::json;
using CRC = utils::CRC32<json>;

void insertCRC (CRC::type crc, std::vector<std::uint8_t> &bytes) {
  for (int i=(CRC::bytes-1)*8; i>=0; i-=8)
    bytes.push_back((crc >> i) & 0xFF);
}

CRC::type extractCRC (std::vector<std::uint8_t> &bytes) {
  CRC::type crc = 0;
  for (uint i=0; i<CRC::bytes; i++) {
    CRC::type byte = bytes.back();
    bytes.pop_back();
    crc += byte << (i*8);
  }
  return crc;
}

using clock = std::chrono::high_resolution_clock;
const auto duration = [] (const clock::time_point &start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
};

void Simulation::debugPrintAll(void) const {
  std::ostringstream oss;
  oss << "simulation_" << _env.startTime().pretty() << "_to_"
      << _env.time().pretty() << ".state";
  std::ofstream ofs (oss.str());

  ofs << "Simulation:\n"
      << "\tEnvironment:\n"
      << "\t\tTime: " << _env.time().pretty() << "\n";

  {
    std::ostringstream doss;
    serialize(doss, _env.dice());
    ofs << "\t\tDice: " << doss.str() << "\n";
  }

  ofs
//      << "\t\tTopology: " << _env.topology() << "\n"
//      << "\t\tHygrometry: " << _env.hygrometry() << "\n"
//      << "\t\tTemperature: " << _env.temperature() << "\n"
      << "\tPlants:\n";

  for (const auto &pair: _plants) {
    Plant * p = pair.second.get();
    ofs << "\t\t" << PlantID(p) << "\n"
        << "\t\t\tGenome: " << p->genome() << "\n"
        << "\t\t\tPos: " << p->pos() << "\n"
        << "\t\t\tBiomasses: " << p->biomass() << "\n"
        << "\t\t\tOrgans:\n";

    std::vector<Organ*> organs (p->organs().begin(), p->organs().end());
    std::sort(organs.begin(), organs.end(),
              [] (const Organ *lhs, const Organ *rhs) { return lhs->id() < rhs->id(); });
    for (Organ *o: organs)
      ofs << "\t\t\t\t" << OrganID(o) << ":\n"
          << "\t\t\t\t\tRotation: " << o->localRotation() << "\n"
          << "\t\t\t\t\tBiomasses:\n"
          << "\t\t\t\t\t\tBase: " << o->baseBiomass() << "\n"
          << "\t\t\t\t\t\tAcc: " << o->accumulatedBiomass() << "\n"
          << "\t\t\t\t\t\tReq: " << o->requiredBiomass() << "\n";
  }

  ofs << "\tPTree:\n"
      << std::endl;
}

void Simulation::clone(const Simulation &s) {
  destroy();

  _stats = s._stats;

  _gidManager = s._gidManager;

  std::map<const Plant*, Plant*> plookup;
  std::map<const Plant*, std::map<const Organ*, Organ*>> olookups;
  std::vector<Plant*> rsetPlants;
  for (const auto &p: s._plants) {
     Plant *clone = Plant::clone(*p.second, olookups);
     _plants[clone->pos().x] = std::unique_ptr<Plant>(clone);
     plookup[p.second.get()] = clone;
     if (p.second->hasPStatsPointer())  rsetPlants.push_back(clone);
  }

  _env.clone(s._env, plookup, olookups);

  _ptree = s._ptree;
  for (Plant *p: rsetPlants) {
    p->setPStatsPointer(_ptree.getUserData(p->genealogy().self));
  }

  _start = clock::now();
  _aborted = false;

  _dataFolder = s._dataFolder;
}

std::string field (SimuFields f) {
  auto name = EnumUtils<SimuFields>::getName(f);
  transform(name.begin(), name.end(), name.begin(), ::tolower);
  return name;
}

void Simulation::save (stdfs::path file) const {
  auto startTime = clock::now();

  json jc, je, jp, jt;
  config::Simulation::serialize(jc);
  Environment::save(je, _env);
  for (const auto &p: _plants) {
    json jp_;
    Plant::save(jp_, *p.second);
    jp.push_back(jp_);
  }
  PTree::toJson(jt, _ptree);

  json j;
  j["config"] = jc;
  j[field(SimuFields::ENV)] = je;
  j[field(SimuFields::PLANTS)] = jp;
  j[field(SimuFields::PTREE)] = jt;
  j["nextID"] = Plant::ID(_gidManager);

  if (debugSerialization)
    std::cerr << "Serializing took " << duration(startTime) << " ms" << std::endl;

  startTime = clock::now();

  std::vector<std::uint8_t> v;

  const auto ext = file.extension();
  if (ext == ".cbor")         v = json::to_cbor(j);
  else if (ext == ".msgpack") v = json::to_msgpack(j);
  else {
    if (ext != ".ubjson") file += ".ubjson";
    v = json::to_ubjson(j);
  }

  CRC crcGenerator;
  auto crc = crcGenerator(v.begin(), v.end());
  insertCRC(crc, v);

  simu::save(file, v);

  if (debugSerialization)
    std::cerr << "Saving " << file << " (" << v.size() << " bytes) took "
              << duration(startTime) << " ms" << std::endl;

#if !defined(NDEBUG) && 0
  std::cerr << "Reloading for round-trip test" << std::endl;
  Simulation that;
  load(file, that, "");
  assertEqual(*this, that);
#endif
}

std::ostream& operator<< (std::ostream &os, const Simulation::LoadHelp&) {
  return os << "Not implemented yet\n";
}

void Simulation::load (const stdfs::path &file, Simulation &s,
                       const std::string &constraints,
                       const std::string &fields) {

  std::set<std::string> requestedFields;  // Default to all
  for (auto f: EnumUtils<SimuFields>::iterator())
    requestedFields.insert(field(f));

  for (std::string strf: utils::split(fields, ',')) {
    strf = utils::trim(strf);
    if (strf == "none") requestedFields.clear();
    else if (strf.empty() || strf == "all")
      continue;

    else if (strf.at(0) != '!')
      continue; // Ignore non removal requests

    else {
      strf = strf.substr(1);
      SimuFields f;
      std::istringstream (strf) >> f;

      if (!EnumUtils<SimuFields>::isValid(f)) {
        std::cerr << "'" << strf << "' is not a valid simulation field. "
                     "Ignoring." << std::endl;
        continue;
      }

      requestedFields.erase(field(f));
    }
  }

  if (!fields.empty())
    std::cout << "Requested fields: "
              << utils::join(requestedFields.begin(), requestedFields.end(), ",")
              << std::endl;

  std::cout << "Loading " << file << "...\r" << std::flush;

  s._start = clock::now();
  auto startTime = clock::now();

  std::vector<uint8_t> v;
  simu::load(file, v);

  CRC crcGenerator;
  auto crcStored = extractCRC(v);
  auto crcRecomputed = crcGenerator(v.begin(), v.end());
  if (crcStored != crcRecomputed)
    utils::doThrow<std::invalid_argument>(
      "CRC Check failed: ", std::hex, crcStored, " != ", crcRecomputed);

  std::cout << "Expanding " << file << "...\r" << std::flush;

  json j;
  const auto ext = file.extension();
  if (ext == ".cbor")         j = json::from_cbor(v);
  else if (ext == ".msgpack") j = json::from_msgpack(v);
  else if (ext == ".ubjson")  j = json::from_ubjson(v);
  else
    utils::doThrow<std::invalid_argument>(
      "Unkown save file type '", file, "' of extension '", ext, "'");

  if (debugSerialization)
    std::cerr << "Loading " << file << " (" << v.size() << " bytes) took "
              << duration(startTime) << " ms" << std::endl;

  startTime = clock::now();

  // Cannot work if containing ANY nan
//  assert(j_from_cbor == j_from_msgpack);
//  assert(j_from_cbor == j_from_ubjson);
//  assert(j_from_msgpack == j_from_ubjson);

  std::cout << "Deserializing " << file << "...\r" << std::flush;

  auto dependencies = config::Dependencies::saveState();
  config::Simulation::deserialize(j["config"]);
  if (!config::Dependencies::compareStates(dependencies, constraints))
    utils::doThrow<std::invalid_argument>(
      "Provided save has different build parameters than this one.\n"
      "See above for more details... (aborting)");

  auto loadf = [&requestedFields] (SimuFields f) {
    return requestedFields.find(field(f)) != requestedFields.end();
  };

  bool loadEnv = loadf(SimuFields::ENV);
  if (loadEnv)  Environment::load(j[field(SimuFields::ENV)], s._env);

  bool loadTree = loadf(SimuFields::PTREE);
  if (loadTree) PTree::fromJson(j[field(SimuFields::PTREE)], s._ptree);

  bool loadPlants = loadf(SimuFields::PLANTS);
  if (loadPlants) {
    for (const auto &jp: j[field(SimuFields::PLANTS)]) {
      Plant *p = Plant::load(jp);

      s._plants.insert({p->pos().x, Plant_ptr(p)});

      s._env.addCollisionData(p);

      if (loadTree) {
        PStats *pstats = s._ptree.getUserData(p->genealogy().self);
        p->setPStatsPointer(pstats);
      }

      s._env.updateCollisionDataFinal(p); /// FIXME Update after all plants have been inserted
      if (p->sex() == Plant::Sex::FEMALE)
        for (Organ *o: p->pistils())
          s._env.disseminateGeneticMaterial(o);
    }
  }

  s._gidManager.setNext(j["nextID"]);
  s._env.postLoad();  /// FIXME Should not be required

  if (debugSerialization)
    std::cerr << "Deserializing took "
              << duration(startTime) << " ms" << std::endl;

  s.logToFiles();

  std::cout << "Loaded " << file << "          " << std::endl;
}

void assertEqual(const Simulation &lhs, const Simulation &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs._plants, rhs._plants, deepcopy);
  assertEqual(lhs._ptree, rhs._ptree, deepcopy);
  assertEqual(lhs._env, rhs._env, deepcopy);
}

} // end of namespace simu
