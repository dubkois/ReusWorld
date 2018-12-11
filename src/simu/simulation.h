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
    : _stats(), _ecosystem(genome), _env(genome.env), _step(0) {}

  virtual ~Simulation (void) {}

  virtual bool init (void);
  virtual void destroy (void);
  virtual bool reset (void);

  virtual void step (void);

  bool finished (void) const {
    return _plants.empty()
        || _ecosystem.maxYearDuration < year();
  }

  const Environment& environment (void) const {
    return _env;
  }

  const auto& plants (void) const {
    return _plants;
  }

  uint step (void) const {
    return _step;
  }

  float dayCount (void) const {
    static const auto spd = config::Simulation::stepsPerDay();
    return float(_step) / spd;
  }

  float day (void) const {
    static const auto spd = config::Simulation::stepsPerDay();
    static const auto dpy = config::Simulation::daysPerYear();
    return std::fmod(float(_step) / spd, dpy);
  }

  float year (void) const {
    static const auto dpy = config::Simulation::daysPerYear();
    return dayCount() / dpy;
  }

protected:
  struct Stats {
    float sumDistances;
    float sumCompatibilities;
    uint matings;
    uint reproductions;
    uint newPlants;
  } _stats;

  genotype::Ecosystem _ecosystem;

  Environment _env;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  phylogeny::PhylogenicTree<PGenome> _ptree;

  uint _step;

  virtual void addPlant (const PGenome &g, float x, float biomass);
  virtual void delPlant (float x);

  virtual void performReproductions (void);
  virtual void plantSeeds (Plant::Seeds &seeds);
};

} // end of namespace simu

#endif // SIMULATION_H
