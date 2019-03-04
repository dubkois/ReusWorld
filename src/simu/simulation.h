#ifndef SIMULATION_H
#define SIMULATION_H

#include "kgd/apt/core/tree/phylogenictree.hpp"

#include "../genotype/environment.h"
#include "../config/simuconfig.h"

#include "environment.h"
#include "plant.h"

namespace simu {

class Simulation {
protected:
  using EGenome = genotype::Environment;
  using PGenome = genotype::Plant;
public:
  Simulation (void);

  virtual ~Simulation (void) {
    destroy();
  }

  virtual bool init (const EGenome &env, const PGenome &plant);
  virtual void destroy (void);

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

  const auto& phylogeny (void) const {
    return _ptree;
  }

  std::string periodicSaveName (void) const {
    std::ostringstream oss;
    oss << "autosaves/y" << _env.time().year() << ".save";
    return oss.str();
  }
  void periodicSave (void) const;

  void save (std::experimental::filesystem::__cxx11::path file) const;
  static void load (const stdfs::path &file, Simulation &s);

protected:
  struct Stats {
    using clock = std::chrono::high_resolution_clock;
    using duration_t = std::chrono::milliseconds;
    static constexpr auto duration = [] (clock::time_point start) {
      return std::chrono::duration_cast<duration_t>(
            clock::now() - start).count();
    };
    clock::time_point start;

    uint derivations = 0;
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

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  using PTree = phylogeny::PhylogenicTree<PGenome, PStats>;
  PTree _ptree;

  Stats::clock::time_point _start;
  bool _aborted;

  virtual bool addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (float x, Plant::Seeds &seeds);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);

  virtual void updatePlantAltitude (Plant &p, float h);

  void updateGenStats (void);
  void logGlobalStats (bool header);

  void debugPrintAll (void) const;
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
