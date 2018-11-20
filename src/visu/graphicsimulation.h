#ifndef GRAPHICSIMULATION_H
#define GRAPHICSIMULATION_H

#include "../simu/simulation.h"
#include "mainview.h"

namespace visu {

class GraphicSimulation : public simu::Simulation {
  gui::MainView *_view;
public:
  GraphicSimulation(QWidget *parent, const genotype::Ecosystem &e);

  auto view (void) {  return _view; }

private:
  void addPlant (const genotype::Plant &p, float x) override;
  void delPlant (float x) override;
};

} // end of namespace visu

#endif // GRAPHICSIMULATION_H
