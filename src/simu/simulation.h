#ifndef SIMULATION_H
#define SIMULATION_H

#include "kgd/apt/core/tree/phylogenetictree.hpp"

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

  Simulation (Simulation &&) {}

  virtual ~Simulation (void) {
    destroy();
  }

  virtual bool init (const EGenome &env, PGenome plant);
  virtual void destroy (void);

  virtual void step (void);
  void atEnd (void);

  void abort (void) { _aborted = true; }
  bool finished (void) const {
    return _aborted || _plants.empty() || _env.endTime() <= _env.time();
  }

  void setDuration (Environment::DurationSetType t, uint years) {
    _env.setDuration(t, years);
  }

  void setDataFolder (const stdfs::path &path);

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

  void mutateEnvController (rng::AbstractDice &dice) {
    _env.mutateController(dice);
  }

  void clone (const Simulation &s);

  auto wallTimeDuration (void) const {
    return Stats::duration(_start);
  }

  stdfs::path periodicSaveName (void) const {
    std::ostringstream oss;
    oss << "y" << _env.time().year() << ".save";
    return _dataFolder / oss.str();
  }

  void save (stdfs::path file) const;
  static void load (const stdfs::path &file, Simulation &s,
                    const std::string &constraints);

  friend void assertEqual (const Simulation &lhs, const Simulation &rhs);

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

    uint minGeneration = std::numeric_limits<decltype(minGeneration)>::max();
    uint maxGeneration = 0;

  } _stats;

  Environment _env;

  phylogeny::GIDManager _gidManager;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  using PTree = phylogeny::PhylogeneticTree<PGenome, PStats>;
  PTree _ptree;

  Stats::clock::time_point _start;
  bool _aborted;

  stdfs::path _dataFolder, _statsFile, _ptreeFile;

  virtual bool addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (float x, Plant::Seeds &seeds);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);

  virtual void updatePlantAltitude (Plant &p, float h);

  void updateGenStats (void);
  void logGlobalStats (void);

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
