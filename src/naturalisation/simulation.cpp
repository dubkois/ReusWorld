#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

namespace simu {
namespace naturalisation {

static constexpr bool debug = false;

NatSimulation::NatSimulation (void) {
  _counts.fill(0);
  _stable = false;
  _ptreeActive = false;
}

void NatSimulation::step (void) {
  Simulation::step();

  updateRatios();

  if (_env.time().isStartOfYear()) {
    float newratio = ratio();

    float dratio = _prevratio - newratio;
    _stable = (std::fabs(dratio) < 1e-3) || newratio == 1 || newratio == 0;

    std::cout << StatsHeader{} << "\n" << Stats{*this} << "\n";

    std::cout << "L/R ratio: " << _prevratio << " >> " << newratio << " (dr="
              << dratio << ")" << std::endl;

    _prevratio = newratio;
  }
}

Plant* NatSimulation::addPlant (const PGenome &g, float x, float biomass) {
  Plant *p = Simulation::addPlant(g, x, biomass);

  auto it = _pendingSeeds.find(g.id());
  if (it == _pendingSeeds.end())
    utils::doThrow<std::logic_error>("Could not find ntag for seed ", g.id());

  auto tag = it->second;

  updateCounts(tag, +1);
  assert(counts(tag) < _plants.size());

  _populations[p] = tag;
  _pendingSeeds.erase(it);

  if (debug)
    std::cerr << "Registered " << PlantID(p) << " with " << tag << std::endl;

  return p;
}

void NatSimulation::delPlant (Plant &p, Plant::Seeds &seeds) {
  auto it = _populations.find(&p);
  if (it == _populations.end())
    utils::doThrow<std::logic_error>(
      "Could not find plant ", p.id(), " in population list");

  auto tag = it->second;
  assert(0 < counts(tag));
  assert(counts(tag) <= _plants.size());
  updateCounts(tag, -1);
  _populations.erase(it);

  if (debug)
    std::cerr << "Unregistered " << PlantID(&p) << " with " << tag << std::endl;

  Simulation::delPlant(p, seeds);
}

void NatSimulation::newSeed(const Plant *mother, const Plant *father, GID child) {
  auto mtag = _populations.at(mother), ftag = _populations.at(father),
       tag = Ratios::fromRatios(mtag, ftag);

  _pendingSeeds[child] = tag;

  if (debug)
    std::cerr << "Registered seed " << child << " as " << tag << std::endl;

  Simulation::newSeed(mother, father, child);
}

bool NatSimulation::finished(void) const {
  if (_stable)
    std::cout << "l/R ratio reached stability. Stopping there" << std::endl;
  return _stable || Simulation::finished();
}

float NatSimulation::ratio(void) const {
//  return counts(NTag::LHS) / float(counts(NTag::LHS) + counts(NTag::RHS));
//  return counts(NTag::LHS) / float(_plants.size());
  return _ratios[NTag::LHS];
}

void NatSimulation::updateCounts(const Ratios &r, int dir) {
  using ut = EnumUtils<NTag>::underlying_t;
  uint *tgt = nullptr;
  if (r[NTag::LHS] == 1)      tgt = &_counts[ut(NTag::LHS)];
  else if (r[NTag::RHS] == 1) tgt = &_counts[ut(NTag::RHS)];
  else                        tgt = &_counts[ut(NTag::HYB)];
  *tgt += dir;
}

void NatSimulation::updateRatios (void) {
  _ratios.fill(0);
  for (const auto &pair: _populations)
    _ratios += pair.second;
  _ratios /= _populations.size();
  assert(std::fabs(_ratios[NTag::LHS] + _ratios[NTag::RHS] - 1) < 1e-3);
}

NatSimulation*
NatSimulation::artificialNaturalisation (Parameters &params) {

  NatSimulation *s = new NatSimulation();
  NatSimulation &lhs = *s;

  Simulation::load(params.lhsSimulationFile, lhs,
                   params.loadConstraints, "!ptree");

  struct TaggedPlant {
    Plant* plant;
    NTag tag;
  };
  std::vector<TaggedPlant> plants;

  static const auto clone = [] (const Plant *p) {
    std::map<const Plant*, std::map<const Organ*, Organ*>> olookups;
    return Plant::clone(*p, olookups);
  };

  static const auto extract =
    [] (NatSimulation &s, auto &plants, NTag tag) {

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

    s._pendingSeeds.clear();
    while (!s._plants.empty()) {
      Plant *p = s._plants.begin()->second.get();
      plants.push_back({clone(p), tag});
      s.Simulation::delPlant(*p, newseeds);
    }
  };

  std::cout<< "Extracting plants for LHS\r" << std::flush;
  extract(lhs, plants, NTag::LHS);
  uint lhsPC = plants.size();

  {
    NatSimulation rhs;
    Simulation::load(params.rhsSimulationFile, rhs,
                     params.loadConstraints, "!ptree,!env");

    std::cout<< "Extracting plants for RHS\r" << std::flush;
    extract(rhs, plants, NTag::RHS);
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

  assert(lhs.plants().size() == lhs.counts(NTag::LHS) + lhs.counts(NTag::RHS));

  lhs.updateRatios();
  lhs._prevratio = lhs.ratio();

  std::cout<< "Ready\r" << std::flush;

  return s;
}

NatSimulation*
NatSimulation::naturalNaturalisation (Parameters &/*params*/) {
  utils::doThrow<std::logic_error>("Natural naturalisation not implemented");
  return nullptr;
}

} // end of namespace naturalisation
} // end of namespace simu
