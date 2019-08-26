#ifndef SIMULATION_H
#define SIMULATION_H

#include "kgd/apt/core/tree/phylogenetictree.hpp"

#include "../config/simuconfig.h"

#include "environment.h"
#include "plant.h"

namespace simu {

class Simulation {
public:
  using EGenome = genotype::Environment;
  using PGenome = genotype::Plant;
  using PTree = phylogeny::PhylogeneticTree<PGenome, PStats>;

  using clock = std::chrono::high_resolution_clock;
  using duration_t = std::chrono::milliseconds;
  static constexpr auto duration = [] (clock::time_point start) {
    return std::chrono::duration_cast<duration_t>(
          clock::now() - start).count();
  };

  Simulation (void);

  Simulation (Simulation &&) {}

  virtual ~Simulation (void) {
    destroy();
  }

  virtual bool init (const EGenome &env, PGenome plant);
  virtual void destroy (void);

  virtual void step (void);
  void atEnd (void);

  bool extinct (void) const {
    return _plants.empty();
  }

  void abort (void) { _aborted = true; }
  bool finished (void) const {
    return _aborted || extinct() || _env.endTime() <= _env.time();
  }

  void printStepHeader (void) const;

  void setDuration (Environment::DurationSetType t, uint years) {
    _env.setDuration(t, years);
  }

  enum Overwrite : char {
    UNSPECIFIED = '\0',
    ABORT = 'a',
    PURGE = 'p'
  };
  void setDataFolder (const stdfs::path &path, Overwrite o = UNSPECIFIED);

  const stdfs::path dataFolder (void) const {
    return _dataFolder;
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

  const auto& gidManager (void) const {
    return _gidManager;
  }

  void mutateEnvController (rng::AbstractDice &dice) {
    _env.mutateController(dice);
  }

  void clone (const Simulation &s);

  auto wallTimeDuration (void) const {
    return duration(_start);
  }

  stdfs::path periodicSaveName (void) const {
    std::ostringstream oss;
    oss << "y" << _env.time().year() << ".save";
    return _dataFolder / oss.str();
  }

  void save (stdfs::path file) const;
  static void load (const stdfs::path &file, Simulation &s,
                    const std::string &constraints);

  friend void assertEqual (const Simulation &lhs, const Simulation &rhs,
                           bool deepcopy);

protected:
  struct Stats {
    clock::time_point start;

    uint derivations = 0;
    float sumDistances = 0;
    float sumCompatibilities = 0;
    uint matings = 0;
    uint reproductions = 0;
    uint newSeeds = 0;
    uint newPlants = 0;
    uint deadPlants = 0;

    uint minGeneration = std::numeric_limits<decltype(minGeneration)>::max();
    uint maxGeneration = 0;

  } _stats;

  Environment _env;

  phylogeny::GIDManager _gidManager;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  PTree _ptree;

  clock::time_point _start;
  bool _aborted;

  stdfs::path _dataFolder;
  std::ofstream _statsFile;
  std::array<std::ofstream,
             EnumUtils<genotype::env_controller::Outputs>::size()> _envFiles;

  virtual Plant* addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (Plant &p, Plant::Seeds &seeds);

  /// Check that all plants in \p newborns are collision free
  /// Remove those that are found wanting
  /// \note Traversal order is randomized
  void postInsertionCleanup (std::vector<Plant*> newborns);

  virtual void performReproductions (void);
  virtual void plantSeeds (const Plant::Seeds &seeds);

  virtual void updatePlantAltitude (Plant &p, float h);

  void updateGenStats (void);

  void logToFiles (void);
  void logGlobalStats (void);
  void logEnvState (void);

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
