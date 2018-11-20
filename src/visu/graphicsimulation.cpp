#include "graphicsimulation.h"

namespace visu {

GraphicSimulation::GraphicSimulation(QWidget *parent, const genotype::Ecosystem &e)
  : Simulation(e), _view(new gui::MainView(parent)) {

}

void GraphicSimulation::addPlant(const genotype::Plant &p, float x) {
  Simulation::addPlant(p, x);
  _view->addPlantItem(*_plants.at(x));
}

void GraphicSimulation::delPlant(float x) {
  _view->delPlantItem(x);
  Simulation::delPlant(x);
}

} // end of namespace visu
