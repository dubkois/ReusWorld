#ifndef SIMULATION_H
#define SIMULATION_H

#include "../genotype/ecosystem.h"
#include "../config/simuconfig.h"

#include "environment.h"
#include "plant.h"

namespace simu {

class Simulation {
public:
  Simulation (const genotype::Ecosystem &genome)
    : _ecosystem(genome), _env(genome.env), _step(0) {}

  virtual ~Simulation (void) {}

  virtual bool init (void);
  virtual void destroy (void);
  virtual bool reset (void);

  virtual void step (void);

  const Environment& environment (void) const {
    return _env;
  }

  const auto& plants (void) const {
    return _plants;
  }

  uint step (void) const {
    return _step;
  }

  float day (void) const {
    static const auto spd = config::Simulation::stepsPerDay();
    return float(_step) / spd;
  }

protected:
  genotype::Ecosystem _ecosystem;

  Environment _env;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  uint _step;

  virtual void addPlant (const genotype::Plant &p, float x);
  virtual void delPlant (float x);
};

} // end of namespace simu

#endif // SIMULATION_H
