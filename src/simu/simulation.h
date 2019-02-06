#ifndef SIMULATION_H
#define SIMULATION_H

#include "kgd/apt/core/tree/phylogenictree.hpp"

#include "../genotype/ecosystem.h"
#include "../config/simuconfig.h"

#include "environment.h"
#include "plant.h"

namespace simu {

class Simulation {
protected:
  using PGenome = genotype::Plant;
public:
  Simulation (const genotype::Ecosystem &genome);

  virtual ~Simulation (void) {
    destroy();
  }

  virtual bool init (void);
  virtual void destroy (void);
  virtual bool reset (void);

  virtual void step (void);

  void abort (void) { _aborted = true; }
  bool finished (void) const {
    return _aborted || _plants.empty()
        || _env.time().isEndOf()
        || _stats.minGeneration > config::Simulation::stopAtMinGen()
        || _stats.maxGeneration > config::Simulation::stopAtMaxGen();
  }

  const Environment& environment (void) const {
    return _env;
  }

  const auto& plants (void) const {
    return _plants;
  }

  const auto& time (void) const {
    return _env.time();
  }

protected:
  struct Stats {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;

    float sumDistances = 0;
    float sumCompatibilities = 0;
    uint matings = 0;
    uint reproductions = 0;
    uint newSeeds = 0;
    uint newPlants = 0;
    uint deadPlants = 0;
    uint trimmed = 0;

    uint minGeneration = std::numeric_limits<decltype(minGeneration)>::max();
    uint maxGeneration = 0;

  } _stats;

  Environment _env;
  const genotype::Plant _primordialPlant;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  phylogeny::PhylogenicTree<PGenome, PStats> _ptree;

  bool _aborted;

  virtual bool addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (float x, Plant::Seeds &seeds);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);

  virtual void updatePlantAltitude (Plant &p, float h);

  void updateGenStats (void);
  void logGlobalStats (void);
};

struct PTreeStats {
  enum Stat {

  };

  uint survivorSpecies;
  std::multiset<uint> survivalDistribution;
  std::multiset<uint> branchingDistribution;
};

} // end of namespace simu

#endif // SIMULATION_H
