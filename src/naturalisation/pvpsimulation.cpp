#include "kgd/random/random_iterator.hpp"

#include "pvpsimulation.h"

namespace simu {
namespace naturalisation {

static constexpr bool debug = false;

PVESimulation::PVESimulation (void) {
  _counts.fill(0);
  _stableSteps = 0;
  _ptreeActive = false;
}

void PVESimulation::step (void) {
  Simulation::step();

  updateRatios();
  updateRanges();

  if (_env.time().isStartOfYear()) {
    float newratio = ratio();

    float dratio = newratio - _prevratio;
    if (std::fabs(dratio) <= _stabilityThreshold)
          _stableSteps++;
    else  _stableSteps = 0;

    std::cout << StatsHeader{} << "\n" << Stats{*this} << "\n";

    std::cout << "L/R ratio: " << _prevratio << " >> " << newratio << " (dr="
              << dratio << ")";
    if (_stableSteps) std::cout << " [" << _stableSteps << "]";
    std::cout << std::endl;

    _prevratio = newratio;
  }
}

Plant* PVESimulation::addPlant (const PGenome &g, float x, float biomass) {
  Plant *p = Simulation::addPlant(g, x, biomass);

  auto it = _pendingSeeds.find(g.id());
  if (it == _pendingSeeds.end())
    utils::doThrow<std::logic_error>("Could not find PVPTag for seed ", g.id());

  if (p) {
    auto tag = it->second;

    updateCounts(tag, +1);
  //  assert(counts(tag) < _plants.size());


    _populations[p] = tag;

    if (debug)
      std::cerr << "Registered " << PlantID(p) << " with " << tag << std::endl;

//    if (!initializing && _populations.size() != _plants.size())
//      utils::doThrow<std::logic_error>("Size mismatch: ", _populations.size(),
//                                       " != ", _plants.size());
  }

  _pendingSeeds.erase(it);

  return p;
}

void PVESimulation::delPlant (Plant &p, Plant::Seeds &seeds) {
  auto it = _populations.find(&p);
  if (it == _populations.end())
    utils::doThrow<std::logic_error>(
      "Could not find plant ", p.id(), " in population list");

  auto tag = it->second;
//  assert(0 < counts(tag));
//  assert(counts(tag) <= _plants.size());
  updateCounts(tag, -1);
  _populations.erase(it);

  if (debug)
    std::cerr << "Unregistered " << PlantID(&p) << " with " << tag << std::endl;

  Simulation::delPlant(p, seeds);
}

void PVESimulation::newSeed(const Plant *mother, const Plant *father, GID child) {
  auto mtag = _populations.at(mother), ftag = _populations.at(father),
       tag = Ratios::fromRatios(mtag, ftag);

  _pendingSeeds[child] = tag;

  if (debug)
    std::cerr << "Registered seed " << child << " as " << tag << std::endl;

  Simulation::newSeed(mother, father, child);
}

void PVESimulation::stillbornSeed(const Plant::Seed &seed) {
  _pendingSeeds.erase(seed.genome.id());
}

bool PVESimulation::finished(void) const {
  float r = ratio();
  bool stable = _stableSteps >= _stabilitySteps || r == 0 || r == 1;
  if (stable)
    std::cout << "l/R ratio reached stability. Stopping there" << std::endl;
  return stable || Simulation::finished();
}

float PVESimulation::ratio(void) const {
//  return counts(PVPTag::LHS) / float(counts(PVPTag::LHS) + counts(PVPTag::RHS));
//  return counts(PVPTag::LHS) / float(_plants.size());
  return _ratios[PVPTag::LHS];
}

PVPTag tagFromRatio (const Ratios &r) {
  if (r[PVPTag::LHS] == 1)      return PVPTag::LHS;
  else if (r[PVPTag::RHS] == 1) return PVPTag::RHS;
  else                        return PVPTag::HYB;
}

void PVESimulation::updateCounts(const Ratios &r, int dir) {
  using ut = EnumUtils<PVPTag>::underlying_t;
  _counts[ut(tagFromRatio(r))] += dir;
}

void PVESimulation::updateRatios (void) {
  _ratios.fill(0);
  for (const auto &pair: _populations)
    _ratios += pair.second;
  _ratios /= _populations.size();
  assert(std::fabs(_ratios[PVPTag::LHS] + _ratios[PVPTag::RHS] - 1) < 1e-3);
}

void PVESimulation::updateRanges(void) {
  _xmin.fill( _env.xextent());
  _xmax.fill(-_env.xextent());

  using ut = EnumUtils<PVPTag>::underlying_t;
  for (const auto &pair: _populations) {
    auto tag = ut(tagFromRatio(pair.second));
    float x = pair.first->pos().x;
    _xmin[tag] = std::min(_xmin[tag], x);
    _xmax[tag] = std::max(_xmax[tag], x);
  }

  for (auto t: EnumUtils<PVPTag>::iteratorUValues()) {
    if (_counts[t] == 0) {
      _xmin[t] = NAN;
      _xmax[t] = NAN;
    }
  }
}

struct TaggedPlant {
  Plant* plant;
  PVPTag tag;
};
using TaggedPlants = std::vector<TaggedPlant>;

void PVESimulation::commonInit(const Parameters &params) {
  _stabilityThreshold = params.stabilityThreshold;
  _stabilitySteps = params.stabilitySteps;
  _stableSteps = 0;
}

PVESimulation*
PVESimulation::artificialNaturalisation (const Parameters &params) {

  PVESimulation *s = new PVESimulation();
  PVESimulation &lhs = *s;

  Simulation::load(params.lhsSimulationFile, lhs,
                   params.loadConstraints, "!ptree");

  lhs.commonInit(params);

  TaggedPlants plants;

  static const auto extract =
    [] (PVESimulation &s, auto &plants, PVPTag tag) {

    Plant::Seeds newseeds;
    std::vector<Plant*> germinated;

    // Identify all germinated plants ...
    for (auto &pair: s._plants)
      if (!pair.second->isInSeedState())
        germinated.push_back(pair.second.get());

    // ... and remove then
    for (Plant *p: germinated)
      s.Simulation::delPlant(*p, newseeds);

    // Plant the collected seeds
    for (const Plant::Seed &seed: newseeds)
      s._pendingSeeds[seed.genome.id()] = Ratios::fromTag(tag);
    s.plantSeeds(newseeds);

    // Copy all remaining plants and delete from source
    s._pendingSeeds.clear();
    while (!s._plants.empty()) {
      Plant *p = s._plants.begin()->second.get();
      assert(p->isInSeedState());
      if (!p->starvedSeed())  plants.push_back({pclone(p), tag});
      s.Simulation::delPlant(*p, newseeds);
    }
  };

  std::cout<< "Extracting plants for LHS\r" << std::flush;
  extract(lhs, plants, PVPTag::LHS);
  uint lhsPC = plants.size();

  {
    PVESimulation rhs;
    Simulation::load(params.rhsSimulationFile, rhs,
                     params.loadConstraints, "!ptree");

    assert(lhs._env.width() == rhs._env.width());
    assert(lhs._env.height() == rhs._env.height());

    std::cout<< "Extracting plants for RHS\r" << std::flush;
    extract(rhs, plants, PVPTag::RHS);
  }

  uint rhsPC = plants.size() - lhsPC;
  std::cout << "Candidates: " << lhsPC << " (lhs), " << rhsPC << " (rhs)\n"
            << plants.size() << " plants pending" << std::endl;

  lhs._populations.clear();
  lhs._counts.fill(0);


  std::cout<< "Populating LHS\r" << std::flush;

  for (const TaggedPlant &tp: rng::randomIterator(plants, lhs._env.dice())) {
    if (lhs._plants.try_emplace(tp.plant->pos().x,
                                Plant_ptr(tp.plant)).second
        && lhs._env.addCollisionData(tp.plant)) {

      assert(!tp.plant->isDead());

      Ratios r = Ratios::fromTag(tp.tag);
      lhs._populations[tp.plant] = r;
      lhs.updateCounts(r, +1);
    }
  }

  assert(lhs.plants().size() == lhs.counts(PVPTag::LHS) + lhs.counts(PVPTag::RHS));

  lhs.updateRatios();
  lhs._prevratio = lhs.ratio();

  lhs.updateRanges();

  std::cout<< "Ready\r" << std::flush;

  return s;
}

PVESimulation*
PVESimulation::naturalNaturalisation (const Parameters &params) {
  PVESimulation *s = new PVESimulation();

  s->commonInit(params);

  TaggedPlants plants;
  Environment elhs, erhs;
  GID gidlhs, gidrhs;

  static const auto extract =
      [] (auto file, auto constraints, auto &plants,
      Environment &e, GID &gid, PVPTag tag) {

    PVESimulation tmp;
    Simulation::load(file, tmp, constraints, "!ptree");

    Plant::Seeds discardedseeds;
    while(!tmp._plants.empty()) {
      Plant *p = tmp._plants.begin()->second.get();
      plants.push_back({pclone(p), tag});
      tmp.Simulation::delPlant(*p, discardedseeds);
    }

    gid = GID(tmp._gidManager);
    e.clone(tmp._env, {}, {});
  };

  // Extract both environments and populations
  extract(params.lhsSimulationFile, params.loadConstraints, plants,
          elhs, gidlhs, PVPTag::LHS);

  extract(params.rhsSimulationFile, params.loadConstraints, plants,
          erhs, gidrhs, PVPTag::RHS);

  // Create two sided environment
  s->_env.initFrom({&elhs, &erhs}, params.noTopology);
  s->_gidManager.setNext(std::max(gidlhs, gidrhs));

  // Dump all plants at their (shifted) position
  for (auto &tp: plants) {
    float dx = 0;
    switch (tp.tag) {
    case PVPTag::LHS: dx = -elhs.xextent(); break;
    case PVPTag::RHS: dx =  erhs.xextent(); break;
    default:
      utils::doThrow<std::invalid_argument>("Invalid tag ", tp.tag);
    }

    tp.plant->updatePosition(tp.plant->pos().x + dx);

    if (s->_plants.find(tp.plant->pos().x) == s->_plants.end()
        && s->_env.addCollisionData(tp.plant)) {

      s->_plants.emplace(tp.plant->pos().x, Plant_ptr(tp.plant));
      if (params.noTopology)  tp.plant->updateAltitude(s->_env, 0);

      Ratios r = Ratios::fromTag(tp.tag);
      s->_populations[tp.plant] = r;
      s->updateCounts(r, +1);

      for (auto &f: tp.plant->fruits()) {
        for (auto &g: f.second.genomes) {
          g.gdata.self.gid = s->_gidManager();
          s->_pendingSeeds[g.id()] = Ratios::fromTag(tp.tag);
        }
      }
    }
  }

  s->updateRatios();
  s->_prevratio = s->ratio();

  s->updateRanges();

  return s;
}

} // end of namespace naturalisation
} // end of namespace simu
