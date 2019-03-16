#include "kgd/random/random_iterator.hpp"
#include "kgd/utils/functions.h"

#include "simulation.h"

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
#define CUSTOM_PLANTS 1
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


Simulation::Simulation (void) : _stats(), _aborted(false) {}

bool Simulation::init (const EGenome &env, const PGenome &plant) {
  _start = Stats::clock::now();

#ifdef INSTRUMENTALISE
    _env.init(instrumentaliseEnvGenome(env));
#else
    _env.init(env);
#endif

  _ptree.addGenome(plant);
  genotype::BOCData::setFirstID(plant.cdata.id);

  uint N = Config::initSeeds();
  float dx = .5; // m

#ifdef CUSTOM_PLANTS
  using SRule = genotype::grammar::Rule_t<genotype::LSystemType::SHOOT>;
  using RRule = genotype::grammar::Rule_t<genotype::LSystemType::ROOT>;
  std::vector<PGenome> genomes;

  PGenome modifiedPrimordialPlant = plant;

#define SRULE(X) SRule::fromString(X).toPair()
#define RRULE(X) RRule::fromString(X).toPair()

#if CUSTOM_PLANTS == 1
  N = 5;

  modifiedPrimordialPlant.dethklok = 15;
  modifiedPrimordialPlant.shoot.recursivity = 10;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone());

  genomes[0].shoot.rules = {
    SRULE("S -> s-s-s-A"),
    SRULE("A -> s[B]A"),
    SRULE("B -> [-l][+l]"),
  };
  genomes[0].root.rules = {
    RRULE("S -> A"),
    RRULE("A -> [+Bh][Bh][-Bh]"),
    RRULE("B -> tB"),
  };
  genomes[0].root.recursivity = 10;

  genomes[1].dethklok = 25;
  genomes[1].shoot.rules = {
    SRULE("S -> s[-Al][+Bl]"),
    SRULE("A -> -sA"),
    SRULE("B -> +sB"),
  };

  genomes[2].shoot.rules = genomes[1].shoot.rules;
  genomes[2].shoot.recursivity = 7;

  genomes[3].dethklok = 25;
  genomes[3].shoot.rules = genomes[1].shoot.rules;

  genomes[4].shoot.rules = {
    SRULE("S -> ss+s+s+A"),
    SRULE("A -> s[B]A"),
    SRULE("B -> [-l][+l]")
  };
  genomes[4].root = genomes[0].root;


#elif CUSTOM_PLANTS == 2
  N = 10;
  dx = .075;

  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone());

  genomes[0].shoot.rules = {
    SRULE("S -> A[fl][-A]f"),
    SRULE("A -> s")
  };

  genomes[1].shoot.rules = {
    SRULE("S -> s[+l][Al]f"),
    SRULE("A -> f")
  };

#elif CUSTOM_PLANTS == 3
  N = 1;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone());
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

  float x0 = - dx * int(N / 2);
  for (uint i=0; i<N; i++) {

#ifdef CUSTOM_PLANTS
    auto pg = genomes[i%genomes.size()];
#else
    auto pg = plant.clone();
#endif

    pg.cdata.sex = (i%2 ? Plant::Sex::MALE : Plant::Sex::FEMALE);
    float initBiomass = Plant::primordialPlantBaseBiomass(pg);

    _ptree.addGenome(pg);
    addPlant(pg, x0 + i * dx, initBiomass);
  }

  updateGenStats();
  if (Config::logGlobalStats()) logGlobalStats(true);

  return true;
}

void Simulation::destroy (void) {
  Plant::Seeds discardedSeeds;
  while (!_plants.empty())
    delPlant(_plants.begin()->first, discardedSeeds);
  _env.destroy();

  for (Plant::Seed &s: discardedSeeds)  _ptree.delGenome(s.genome);
}

bool Simulation::addPlant(const PGenome &g, float x, float biomass) {
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
      PStats *pstats = _ptree.getUserData(plant->id());
      plant->init(_env, biomass, pstats);

      if (debugPlantManagement)
        std::cerr << PlantID(plant) << " Added at " << plant->pos() << " with "
                  << biomass << " initial biomass" << std::endl;

      _stats.newPlants++;

    } else {
      insertionAborted = true;
      _plants.erase(pit);
    }
  }

  if (insertionAborted) _ptree.delGenome(g);

  return !insertionAborted;
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
        for (const auto &g: litter) {
          _ptree.addGenome(g);
          if (debugReproduction)  std::cerr << " " << g.cdata.id;
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

void Simulation::plantSeeds(Plant::Seeds &seeds) {
  using utils::gauss;

  static constexpr int debug = debugReproduction | debugTopology;

  static const bool& taurusWorld = Config::taurusWorld();
  static const float& dhsd = Config::heightPenaltyStddev();

  if (debugReproduction && seeds.size() > 0)
    std::cerr << "\tPlanting " << seeds.size() << " seeds" << std::endl;

  for (const Plant::Seed &seed: seeds) {
    if (seed.biomass <= 0) {
      _ptree.delGenome(seed.genome);
      continue;
    }

    float startH = _env.heightAt(seed.position.x), maxH = startH, h = startH;
    float avgDx = 1 + 5 * std::max(0.f, seed.position.y - h);
    float stdDx = avgDx / 3.;
    float dist = avgDx + 4 * stdDx;

    if (debug)
      std::cerr << "values = gauss(d," << avgDx << ", " << stdDx << ")"
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
      std::cerr << "Planted seed " << seed.genome.cdata.id << " at " << x
                << " (extracted from plant " << seed.genome.cdata.parent(genotype::BOCData::MOTHER)
                << " at position " << seed.position.x << ")" << std::endl;

    addPlant(seed.genome, x, seed.biomass);
  }
}

void Simulation::step (void) {
  static const auto CTSIZE = utils::CurrentTime::width();

  std::string ptime = _env.time().pretty();
  if (Config::verbosity() > 0)
    std::cout << std::string(22 + ptime.size() + 4 + CTSIZE, '#') << "\n"
              << "## Simulation step " << ptime
              << " at " << utils::CurrentTime{} << " ##"
              << std::endl;

  _stats = Stats{};
  _stats.start = std::chrono::high_resolution_clock::now();

  Plant::Seeds seeds;
  std::set<float> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    _stats.derivations += p->step(_env);
    if (p->isDead()) {
      if (debugDeath) p->autopsy();
      corpses.insert(p->pos().x);
    }
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

  for (const auto &p: _plants)
    if (p.second->hasUncollectedSeeds())
      p.second->collectCurrentStepSeeds(seeds);
  plantSeeds(seeds);

  if (!seeds.empty())
    _env.processNewObjects();

  _ptree.step(_env.time().toTimestamp(), _plants.begin(), _plants.end(),
              [] (const Plants::value_type &pair) {
    return pair.second->genome().cdata.id;
  });

  updateGenStats();
  if (Config::logGlobalStats()) logGlobalStats(false);

  if (_env.hasTopologyChanged()) {
    for (const auto &it: _plants) {
      const auto pos = it.second->pos();
      float h = _env.heightAt(pos.x);
      if (pos.y != h)
        updatePlantAltitude(*it.second, h);
    }
  }

  _env.step();

  if (_env.time().isStartOfYear() && (_env.time().year() % Config::saveEvery()) == 0)
    periodicSave();

  if (finished() && _env.startTime() < _env.time()) {
    // Update once more so that data goes from y0d0h0 to yLd0h0 with L = stopAtYear()
    _stats.start = Stats::clock::now();
    if (Config::logGlobalStats()) logGlobalStats(false);
    _ptree.step(_env.time().toTimestamp(), _plants.begin(), _plants.end(),
                [] (const Plants::value_type &pair) {
      return pair.second->genome().cdata.id;
    });

    _ptree.saveTo("phylogeny.ptree.json");

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
                << Stats::duration(_start) << " ms)" << std::endl;
    }
  }
}

void Simulation::updatePlantAltitude(Plant &p, float h) {
  p.updateAltitude(_env, h);
}

void Simulation::updateGenStats (void) {
  for (const auto &p: _plants) {
    const Plant &plant = *p.second;
    auto gen = plant.genome().cdata.generation;
    _stats.minGeneration = std::min(_stats.minGeneration, gen);
    _stats.maxGeneration = std::max(_stats.maxGeneration, gen);
  }
}

void Simulation::logGlobalStats(bool header) {
  stdfs::path path = "global.dat";
  if (header && stdfs::exists(path)) {
    stdfs::path backup = path;
    backup += "~";
    stdfs::copy(path, backup, stdfs::copy_options::update_existing);
  }

  std::ofstream ofs;
  std::ios_base::openmode mode = std::fstream::out;
  if (header)  mode |= std::fstream::trunc;
  else        mode |= std::fstream::app;
  ofs.open("global.dat", mode);

  if (header)
    ofs << "Date Time MinGen MaxGen Plants Seeds Females Males Biomass"
           " Derivations Organs Flowers Fruits Matings"
           " Reproductions dSeeds Births Deaths Trimmed AvgDist AvgCompat"
           " Species MinX MaxX\n";

  else {
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

    ofs << _env.time().pretty()
        << " " << Stats::duration(_stats.start)
        << " " << _stats.minGeneration << " " << _stats.maxGeneration
        << " " << _plants.size() << " " << seeds
        << " " << females << " " << males
        << " " << biomass

        << " " << _stats.derivations
        << " " << organs << " " << flowers << " " << fruits

        << " " << _stats.matings << " " << _stats.reproductions
        << " " << _stats.newSeeds << " " << _stats.newPlants
        << " " << _stats.deadPlants << " " << _stats.trimmed

        << " " << _stats.sumDistances / float(_stats.matings)
        << " " << _stats.sumCompatibilities / float(_stats.matings)

        << " " << _ptree.width()

        << " " << minx << " " << maxx
        << std::endl;
  }

//  debugPrintAll();
}

void Simulation::periodicSave (void) const {
  if (!stdfs::exists("autosaves"))  stdfs::create_directory("autosaves");
  if (_env.startTime() == _env.time()) {
    if (!stdfs::is_empty("autosaves")) {
      std::cerr << "WARNING: autosaves folder is not empty. Purge anyway ?"
                << std::flush;
      if(std::cin.get() == 'y') {
        stdfs::remove_all("autosaves");
        stdfs::create_directory("autosaves");
      }
      std::cerr << std::endl;
    }
  }

  save(periodicSaveName());
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

void Simulation::save (stdfs::path file) const {
  auto startTime = clock::now();

  json je, jp, jt;
  Environment::save(je, _env);
  for (const auto &p: _plants) {
    json jp_;
    Plant::save(jp_, *p.second);
    jp.push_back(jp_);
  }
  PTree::toJson(jt, _ptree, true);

  json j = { je, jp, jt };

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

  simu::save(file, v);

  if (debugSerialization)
    std::cerr << "Saving " << file << " (" << v.size() << " bytes) took "
              << duration(startTime) << " ms" << std::endl;
}

void Simulation::load (const stdfs::path &file, Simulation &s) {
  std::cout << "Loading " << file << "...\r" << std::flush;

  s._start = Stats::clock::now();
  auto startTime = clock::now();

  std::vector<uint8_t> v;
  simu::load(file, v);

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

  Environment::load(j[0], s._env);
  PTree::fromJson(j[2], s._ptree, true);
  Plant::ID lastID = Plant::ID(0);
  for (const auto &jp: j[1]) {
    Plant *p = Plant::load(jp);

    // Find biggest GID
    if (lastID < p->id()) lastID = p->id();
    for (const auto &fd: p->fruits())
      for (const genotype::Plant &g: fd.second.genomes)
        if (lastID < g.cdata.id) lastID = g.cdata.id;

    s._plants.insert({p->pos().x, Plant_ptr(p)});

    s._env.addCollisionData(p);

    PStats *pstats = s._ptree.getUserData(p->id());
    p->setPStatsPointer(pstats);

    s._env.updateCollisionDataFinal(p);
    if (p->sex() == Plant::Sex::FEMALE)
      for (Organ *o: p->pistils())
        s._env.disseminateGeneticMaterial(o);
  }

  genotype::BOCData::setFirstID(lastID);
  s._env.processNewObjects();

  if (debugSerialization)
    std::cerr << "Deserializing took "
              << duration(startTime) << " ms" << std::endl;

  if (Config::logGlobalStats()) s.logGlobalStats(true);

  std::cout << "Loaded " << file << "          " << std::endl;;
}

} // end of namespace simu
