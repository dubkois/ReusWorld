#ifndef SIMULATION_H
#define SIMULATION_H

#include "../genotype/ecosystem.h"
#include "../config/simuconfig.h"
#include "plant.h"

namespace simu {

class Simulation {
public:
  Simulation (const genotype::Ecosystem &genome) : _ecosystem(genome) {}
  virtual ~Simulation (void) {}

  bool init (void);

protected:
  genotype::Ecosystem _ecosystem;

  using Plant_ptr = std::unique_ptr<Plant>;
  using Plants = std::map<float, Plant_ptr>;
  Plants _plants;

  virtual void addPlant (const genotype::Plant &p, float x);
  virtual void delPlant (float x);
};

} // end of namespace simu

#endif // SIMULATION_H
