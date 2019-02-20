#include "kgd/random/random_iterator.hpp"
#include "kgd/utils/functions.h"

#include "simulation.h"

namespace simu {

using Config = config::Simulation;

static constexpr bool debugPlantManagement = false;
static constexpr bool debugReproduction = false;
static constexpr bool debugDeath = false;
static constexpr int debugTopology = 0;
static constexpr bool debugSerialization = true;

static constexpr bool debug = false
  | debugPlantManagement | debugReproduction | debugTopology;

#ifndef NDEBUG
//#define INSTRUMENTALISE
//#define CUSTOM_PLANTS
#endif

#if !defined(NDEBUG) && defined(INSTRUMENTALISE)
auto instrumentaliseEnvGenome (genotype::Environment g) {
  std::cerr << __PRETTY_FUNCTION__ << " Bypassing environmental genomic values" << std::endl;

//  g.voxels = 10;
//  std::cerr << "genome.env.voxels = " << g.voxels << std::endl;

  return g;
}

auto instrumentalisePlantGenome (genotype::Plant g) {
  std::cerr << __PRETTY_FUNCTION__ << " Bypassing plant genomic values" << std::endl;

//  g.temperatureOptimal = 10;
//  std::cerr << "genome.temperatureOptimal = " << g.temperatureOptimal << std::endl;

//  g.temperatureRange = 15;
//  std::cerr << "genome.temperatureRange = " << g.temperatureRange << std::endl;

  return g;
}
#endif

Simulation::Simulation (void) : _stats(), _aborted(false) {}

bool Simulation::init (const EGenome &env, const PGenome &plant) {
#ifdef INSTRUMENTALISE
    _env.init(instrumentaliseEnvGenome(env));
    _primordialPlant(instrumentalisePlantGenome(plant)),
#else
  _env.init(env);
#endif

  _ptree.addGenome(plant);
  genotype::BOCData::setFirstID(plant.cdata.id);

  uint N = Config::initSeeds();
  float dx = .5; // m

#ifdef CUSTOM_PLANTS
  N = 5;
  std::vector<PGenome> genomes;

  using SRule = genotype::grammar::Rule_t<genotype::LSystemType::SHOOT>;
  using RRule = genotype::grammar::Rule_t<genotype::LSystemType::ROOT>;

  PGenome modifiedPrimordialPlant = _primordialPlant;
  modifiedPrimordialPlant.dethklok = 15;
  modifiedPrimordialPlant.shoot.recursivity = 10;
  for (uint i=0; i<N; i++)  genomes.push_back(modifiedPrimordialPlant.clone());

  genomes[0].shoot.rules = {
    SRule::fromString("S -> s-s-s-A").toPair(),
    SRule::fromString("A -> s[B]A").toPair(),
    SRule::fromString("B -> [-l][+l]").toPair(),
  };
  genomes[0].root.rules = {
    RRule::fromString("S -> A").toPair(),
    RRule::fromString("A -> [+Bh][Bh][-Bh]").toPair(),
    RRule::fromString("B -> tB").toPair(),
  };
  genomes[0].root.recursivity = 10;

  genomes[1].dethklok = 25;
  genomes[1].shoot.rules = {
    SRule::fromString("S -> s[-Al][+Bl]").toPair(),
    SRule::fromString("A -> -sA").toPair(),
    SRule::fromString("B -> +sB").toPair(),
  };

  genomes[2].shoot.rules = genomes[1].shoot.rules;
  genomes[2].shoot.recursivity = 7;

  genomes[3].dethklok = 25;
  genomes[3].shoot.rules = genomes[1].shoot.rules;

  genomes[4].shoot.rules = {
    SRule::fromString("S -> ss+s+s+A").toPair(),
    SRule::fromString("A -> s[B]A").toPair(),
    SRule::fromString("B -> [-l][+l]").toPair()
  };
  genomes[4].root = genomes[0].root;

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
  return true;
}

void Simulation::destroy(void) {
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
    auto y = _env.heightAt(x);
    auto pair = _plants.try_emplace(x, std::make_unique<Plant>(g, x, y));

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

  if (insertionAborted)  _ptree.delGenome(g);

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
        mother->updateGeometry(); /// TODO Probably multiple updates... not great
        mother->update(_env);

        stamen->accumulate(-stamen->biomass() + stamen->baseBiomass());

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
  std::string ptime = _env.time().pretty();
  if (Config::verbosity() > 0)
    std::cout << std::string(22 + ptime.size(), '#') << "\n"
              << "## Simulation step " << ptime << " ##"
              << std::endl;

  _stats = Stats{};
  _stats.start = std::chrono::high_resolution_clock::now();

  Plant::Seeds seeds;
  std::set<float> corpses;
  for (const auto &it: rng::randomIterator(_plants, _env.dice())) {
    const Plant_ptr &p = it.second;
    p->step(_env);
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
  if (Config::logGlobalStats())
    logGlobalStats();

  if (_env.hasTopologyChanged()) {
    for (const auto &it: _plants) {
      const auto pos = it.second->pos();
      float h = _env.heightAt(pos.x);
      if (pos.y != h)
        updatePlantAltitude(*it.second, h);
    }
  }

  if (_env.time().isStartOfYear() && (_env.time().year() % Config::saveEvery()) == 0)
    periodicSave();

  if (finished()) {
    _ptree.saveTo("phylogeny.ptree.json");

    if (Config::verbosity() > 0) {
      std::cout << "Simulation ";
      if (_aborted) std::cout << "canceled by user after";
      else          std::cout << "completed in";

      const Time &time = _env.time();
      std::cout << " " << time.year() << " year";
      if (time.year() > 1)  std::cout << "s";
      std::cout << " " << time.day() << " day";
      if (time.day() > 1)   std::cout << "s";
      std::cout << " " << time.hour() << " hour";
      if (time.hour() > 1)  std::cout << "s";
      std::cout << " (" << time.toTimestamp() << " steps)" << std::endl;
    }
  }

  _env.step();
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

void Simulation::logGlobalStats(void) {
  const auto &time = _env.time();
  bool first = time.isStartOf();

  std::ofstream ofs;
  std::ios_base::openmode mode = std::fstream::out;
  if (first)  mode |= std::fstream::trunc;
  else        mode |= std::fstream::app;
  ofs.open("global.dat", mode);

  if (first)
    ofs << "Date Time MinGen MaxGen Plants Seeds Females Males Biomass Organs"
           " Flowers Fruits Matings"
           " Reproductions dSeeds Births Deaths Trimmed AvgDist AvgCompat"
           " Species MinX MaxX\n";

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

  ofs << time.pretty()
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

      << " " << _ptree.width()

      << " " << minx << " " << maxx
      << std::endl;
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

void Simulation::periodicSave (void) const {
  auto startTime = clock::now();

  json je, jp, jt, j = { je, jp, jt };
  save(je, _env);
  for (const auto &p: _plants) {
    json jp_;
    save(jp_, *p.second);
    jp.push_back(jp_);
  }
  jt = _ptree;

  if (debugSerialization)
    std::cerr << "Serializing took " << duration(startTime) << " ms" << std::endl;

  std::ostringstream oss;
  oss << "last_run_y" << _env.time().year() << ".save";

  startTime = clock::now();

  std::vector<std::uint8_t> v_cbor = json::to_cbor(j);
  save(oss.str() + ".cbor", v_cbor);

  if (debugSerialization)
    std::cerr << "Saving " << oss.str() << ".cbor (" << v_cbor.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;

  startTime = clock::now();

  std::vector<std::uint8_t> v_msgpack = json::to_msgpack(j);
  save(oss.str() + ".msgpack", v_msgpack);

  if (debugSerialization)
    std::cerr << "Saving " << oss.str() << ".msgpack (" << v_msgpack.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;

  startTime = clock::now();

  std::vector<std::uint8_t> v_ubjson = json::to_ubjson(j);
  save(oss.str() + ".ubjson", v_ubjson);

  if (debugSerialization)
    std::cerr << "Saving " << oss.str() << ".ubjson (" << v_ubjson.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;
}

void Simulation::load (const std::string &file, Simulation &s) {
  auto startTime = clock::now();

  std::vector<uint8_t> v_cbor;
  simu::load(file + ".cbor", v_cbor);
  json j_from_cbor = json::from_cbor(v_cbor);

  if (debugSerialization)
    std::cerr << "Loading " << file << ".cbor (" << v_cbor.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;

  startTime = clock::now();

  std::vector<uint8_t> v_msgpack;
  simu::load(file + ".msgpack", v_msgpack);
  json j_from_msgpack = json::from_msgpack(v_msgpack);

  if (debugSerialization)
    std::cerr << "Loading " << file << ".msgpack (" << v_msgpack.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;

  startTime = clock::now();

  std::vector<uint8_t> v_ubjson;
  simu::load(file + ".ubjson", v_ubjson);
  json j_from_ubjson = json::from_ubjson(v_ubjson);

  if (debugSerialization)
    std::cerr << "Loading " << file << ".ubjson (" << v_ubjson.size()
              << " bytes) took " << duration(startTime)
              << " ms" << std::endl;

  startTime = clock::now();

  assert(j_from_cbor == j_from_msgpack);
  assert(j_from_cbor == j_from_ubjson);
  assert(j_from_msgpack == j_from_ubjson);

  const json &j = j_from_cbor;
  Environment::load(j[0], s._env);
  s._ptree = j[2];
  for (const auto &jp: j[1]) {
    Plant *p = Plant::load(jp);
    s._plants.insert({p->pos().x, Plant_ptr(p)});

    s._env.addCollisionData(p);

    PStats *pstats = s._ptree.getUserData(p->id());
    p->setPStatsPointer(pstats);

    _env.updateCollisionDataFinal(p);
    if (p->sex() == Plant::Sex::FEMALE)
      for (const Organ *o: p->flowers())
        _env.disseminateGeneticMaterial(o);
  }

  _env.processNewObjects();

  if (debugSerialization)
    std::cerr << "Deserializing took "
              << duration(startTime) << " ms" << std::endl;
}

} // end of namespace simu
