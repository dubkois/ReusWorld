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

  virtual ~Simulation (void) {}

  virtual bool init (void);
  virtual void destroy (void);
  virtual bool reset (void);

  virtual void step (void);

  void abort (void) { _aborted = true; }
  bool finished (void) const {
    return _aborted || _plants.empty()
        || _ecosystem.maxYearDuration < year();
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

  uint maxStep (void) const {
    static const auto spd = config::Simulation::stepsPerDay();
    static const auto dpy = config::Simulation::daysPerYear();
    return _ecosystem.maxYearDuration * dpy * spd;
  }

  static float dayCount (uint step) {
    static const auto spd = config::Simulation::stepsPerDay();
    return float(step) / spd;
  }
  float dayCount (void) const { return dayCount(_step); }

  static float day (uint step) {
    static const auto spd = config::Simulation::stepsPerDay();
    static const auto dpy = config::Simulation::daysPerYear();
    return std::fmod(float(step) / spd, dpy);
  }
  float day (void) const {  return day(_step);  }

  static float year (uint step) {
    static const auto dpy = config::Simulation::daysPerYear();
    return dayCount(step) / dpy;
  }
  float year (void) const { return year(_step); }

  static std::string prettyTime (uint step);
  std::string prettyTime (void) const {
    return prettyTime(_step);
  }

protected:
  struct Stats {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;

    float sumDistances;
    float sumCompatibilities;
    uint matings;
    uint reproductions;
    uint newSeeds;
    uint newPlants;
    uint deadPlants;
    uint trimmed;
  } _stats;

  genotype::Ecosystem _ecosystem;

  Environment _env;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  phylogeny::PhylogenicTree<PGenome> _ptree;

  uint _step;
  bool _aborted;

  virtual bool addPlant(const PGenome &g, float x, float biomass);
  virtual void delPlant (float x, Plant::Seeds &seeds);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);

  void logGlobalStats (void) const;
};

} // end of namespace simu

#endif // SIMULATION_H
