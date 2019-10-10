#include "kgd/random/random_iterator.hpp"

#include "simulation.h"

namespace simu {
namespace naturalisation {

static constexpr bool debug = false;

NatSimulation::NatSimulation (void) {
  _counts.fill(0);
}

Plant* NatSimulation::addPlant (const PGenome &g, float x, float biomass) {
  Plant *p = Simulation::addPlant(g, x, biomass);

  auto it = _pendingSeeds.find(g.id());
  if (it == _pendingSeeds.end())
    utils::doThrow<std::logic_error>("Could not find ntag for seed ", g.id());

  NTag tag = it->second;

  assert(0 <= counts(tag));
  assert(counts(tag) < _plants.size());
  counts(tag)++;

  _populations[p] = tag;
  _pendingSeeds.erase(it);

  if (debug)
    std::cerr << "Registered " << PlantID(p) << " as " << tag << std::endl;

  return p;
}

void NatSimulation::delPlant (Plant &p, Plant::Seeds &seeds) {
  auto it = _populations.find(&p);
  if (it == _populations.end())
    utils::doThrow<std::logic_error>(
      "Could not find plant ", p.id(), " in population list");

  NTag tag = it->second;
  assert(0 < counts(tag));
  assert(counts(tag) <= _plants.size());
  counts(tag)--;
  _populations.erase(it);

  if (debug)
    std::cerr << "Unregistered " << PlantID(&p) << " with " << tag << std::endl;

  Simulation::delPlant(p, seeds);
}

void NatSimulation::newSeed(const Plant *mother, const Plant *father, GID child) {
  NTag mtag = _populations.at(mother), ftag = _populations.at(father),
       tag = (mtag == ftag) ? mtag : NTag::HYB;

  _pendingSeeds[child] = tag;

  if (debug)
    std::cerr << "Registered seed " << child << " as " << tag << std::endl;

  Simulation::newSeed(mother, father, child);
}

NatSimulation*
NatSimulation::artificialNaturalisation (Parameters &params) {

  NatSimulation *s = new NatSimulation();
  NatSimulation &lhs = *s;

  Simulation::load(params.lhsSimulationFile, lhs, params.loadConstraints, "all");
  lhs._ptreeActive = false;

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

    for (auto &pair: s._plants)
      if (!pair.second->isInSeedState())
        germinated.push_back(pair.second.get());
    for (Plant *p: germinated)
      s.Simulation::delPlant(*p, newseeds);

    for (const Plant::Seed &seed: newseeds)
      s._pendingSeeds[seed.genome.id()] = tag;
    s.plantSeeds(newseeds);
    s._pendingSeeds.clear();

    while (!s._plants.empty()) {
      Plant *p = s._plants.begin()->second.get();
      plants.push_back({clone(p), tag});
      s.Simulation::delPlant(*p, newseeds);
    }
  };

  extract(lhs, plants, NTag::LHS);

  {
    NatSimulation rhs;
    Simulation::load(params.rhsSimulationFile, rhs,
                     params.loadConstraints, "plants");

    extract(rhs, plants, NTag::RHS);
  }

  std::cerr << plants.size() << " plants pending" << std::endl;

  for (const TaggedPlant &tp: rng::randomIterator(plants, lhs._env.dice())) {
    if (lhs._plants.try_emplace(tp.plant->pos().x,
                                Plant_ptr(tp.plant)).second
        && lhs._env.addCollisionData(tp.plant)) {

      lhs._populations[tp.plant] = tp.tag;
      lhs.counts(tp.tag)++;
    }
  }

  return s;
}

NatSimulation*
NatSimulation::naturalNaturalisation (Parameters &/*params*/) {
  utils::doThrow<std::logic_error>("Natural naturalisation not implemented");
  return nullptr;
}

} // end of namespace naturalisation
} // end of namespace simu
