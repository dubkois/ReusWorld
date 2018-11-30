#include "graphicsimulation.h"

#include "controller.h"

namespace visu {

void GraphicSimulation::setController(visu::Controller *c) {
  _controller = c;
}

void GraphicSimulation::addPlant(const PGenome &p, float x, const Reserves &r) {
  Simulation::addPlant(p, x, r);
  _controller->view()->addPlantItem(*_plants.at(x));
}

void GraphicSimulation::delPlant(float x) {
  _controller->view()->delPlantItem(x);
  Simulation::delPlant(x);
}

bool GraphicSimulation::init(void) {
  bool ok = Simulation::init();
  emit initialized(ok);
  return ok;
}

void GraphicSimulation::step (void) {
  Simulation::step();

  for (const auto& p: _plants)
    _controller->view()->updatePlantItem(p.first);
}

} // end of namespace visu
