#ifndef GRAPHICSIMULATION_H
#define GRAPHICSIMULATION_H

#include <QObject>

#include "../simu/simulation.h"

namespace visu {
struct Controller;

class GraphicSimulation : public QObject, public simu::Simulation {
  Q_OBJECT
  Controller *_controller;
public:
  GraphicSimulation(const genotype::Ecosystem &e) : Simulation(e) {}

  void setController (Controller *_controller);

  void step (void) override {
    Simulation::step();
  }

private:
  void addPlant (const genotype::Plant &p, float x) override;
  void delPlant (float x) override;
};

} // end of namespace visu

#endif // GRAPHICSIMULATION_H
