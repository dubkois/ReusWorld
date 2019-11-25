#include "kgd/random/random_iterator.hpp"

#include "pvesimulation.h"

namespace simu {
namespace naturalisation {

static constexpr bool debug = false;

PVESimulation::PVESimulation (void) {
  _stableSteps = 0;
  _ptreeActive = false;
}

void PVESimulation::step (void) {
  Simulation::step();

  updateRegions();

  if (_env.time().isStartOfYear()) {
    float newscore = score();

    float dscore = newscore - _prevscore;
    if (std::fabs(dscore) <= _stabilityThreshold)
          _stableSteps++;
    else  _stableSteps = 0;

    std::cout << StatsHeader{} << "\n" << Stats{*this} << "\n";

    std::cout << "L/R ratio: " << _prevscore << " >> " << newscore << " (dr="
              << dscore << ")";
    if (_stableSteps) std::cout << " [" << _stableSteps << "]";
    std::cout << std::endl;

    _prevscore = newscore;
  }
}

float PVESimulation::score (void) const {
  return 2 * (float(_regions.count()) / R - .5);
}

bool PVESimulation::finished(void) const {
  bool stable = _stableSteps >= _stabilitySteps;
  if (stable)
    std::cout << "Score reached stability. Stopping there" << std::endl;
  return stable || Simulation::finished();
}

void PVESimulation::updateRegions(void) {
  _regions.reset();
  float W = _env.width();
  for (const auto &p: _plants) {
    uint r = uint(R * ((p.second->pos().x / W) + .5));
    _regions.set(r);
  }
}

void PVESimulation::commonInit(const Parameters &params) {
  _stabilityThreshold = params.stabilityThreshold;
  _stabilitySteps = params.stabilitySteps;
  _stableSteps = 0;
}

PVESimulation* PVESimulation::type1PVE (const Parameters &params) {

  PVESimulation *s = new PVESimulation();

  PVESimulation lhs;
  Simulation::load(params.lhsSimulationFile, lhs,
                   params.loadConstraints, "!ptree");

  s->commonInit(params);

  std::vector<Plant*> plants;

  static const auto extract =
    [] (PVESimulation &s, auto &plants) {

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
    s.plantSeeds(newseeds);

    // Copy all remaining plants and delete from source
    while (!s._plants.empty()) {
      Plant *p = s._plants.begin()->second.get();
      assert(p->isInSeedState());
      if (!p->starvedSeed())  plants.push_back(pclone(p));
      s.delPlant(*p, newseeds);
    }
  };

  std::cout<< "Extracting plants for LHS\r" << std::flush;
  extract(lhs, plants);

  Environment erhs;
  erhs.init(genotype::Environment::fromFile(params.environmentFile));

  s->_env.initFrom({&lhs._env, &erhs}, params.noTopology, params.totalWidth);

  std::cout << "Populating LHS (" << plants.size() << " available seeds)\r"
            << std::flush;

  for (Plant *p: rng::randomIterator(plants, erhs.dice())) {
//    p->updatePosition(.5 * (p->pos().x + -lhs._env.xextent()));
    p->updatePosition(s->_env.xextent() * ((p->pos().x / lhs._env.width()) - .5));

    if (s->_plants.try_emplace(p->pos().x, Plant_ptr(p)).second
        && s->_env.addCollisionData(p)) {
      if (params.noTopology)  p->updateAltitude(s->_env, 0);
      assert(!p->isDead());
    }
  }

  s->updateRegions();
  s->_prevscore = s->score();

  std::cout<< "Ready\r" << std::flush;

  return s;
}

} // end of namespace naturalisation
} // end of namespace simu
