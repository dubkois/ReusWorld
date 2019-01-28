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
  Simulation (const genotype::Ecosystem &genome)
    : _stats(), _ecosystem(genome), _env(genome.env),
      _step(0), _aborted(false) {}

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
        || year(_step) > config::Simulation::stopAtYear()
        || _stats.minGeneration > config::Simulation::stopAtMinGen()
        || _stats.maxGeneration > config::Simulation::stopAtMaxGen();
  }

  const Environment& environment (void) const {
    return _env;
  }

  const auto& plants (void) const {
    return _plants;
  }

  uint currentStep (void) const {
    return _step;
  }

  static uint maxStep (void) {
    static const auto ms = config::Simulation::stopAtYear()
                         * config::Simulation::stepsPerDay()
                         * config::Simulation::daysPerYear();
    return ms;
  }

  static float dayCount (uint step) {
    static const auto spd = config::Simulation::stepsPerDay();
    return float(step) / spd;
  }

  static float day (uint step) {
    static const auto spd = config::Simulation::stepsPerDay();
    static const auto dpy = config::Simulation::daysPerYear();
    return std::fmod(float(step) / spd, dpy);
  }

  static float year (uint step) {
    static const auto dpy = config::Simulation::daysPerYear();
    return dayCount(step) / dpy;
  }

  static std::string prettyTime (uint step);
  auto prettyTime (void) const {
    return prettyTime(_step);
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

  genotype::Ecosystem _ecosystem;

  Environment _env;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  phylogeny::PhylogenicTree<PGenome, PStats> _ptree;

  uint _step;
  bool _aborted;

  virtual bool addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (float x, Plant::Seeds &seeds);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);

  void updateGenStats (void);
  void logGlobalStats (void);
};

} // end of namespace simu

#endif // SIMULATION_H
