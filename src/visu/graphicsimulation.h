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

  bool init (void) override;
  void step (void) override;

private:
  void addPlant (const PGenome &p, float x, float biomass) override;
  void delPlant (float x) override;

signals:
  void initialized (bool ok);
};

} // end of namespace visu

#endif // GRAPHICSIMULATION_H
