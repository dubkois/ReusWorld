#include "graphicsimulation.h"

#include "../config/visuconfig.h"
#include "controller.h"

namespace visu {

void GraphicSimulation::setController(visu::Controller *c) {
  _controller = c;
}

bool GraphicSimulation::addPlant(const PGenome &p, float x, float biomass) {
  bool added = Simulation::addPlant(p, x, biomass);
  if (added)  _controller->view()->addPlantItem(*_plants.at(x));
  return added;
}

void GraphicSimulation::delPlant(float x, simu::Plant::Seeds &seeds) {
  _controller->view()->delPlantItem(x);
  Simulation::delPlant(x, seeds);
}

bool GraphicSimulation::init(void) {
  bool ok = Simulation::init();
  emit initialized(ok);

  return ok;
}

void GraphicSimulation::graphicalStep (void) {
  if (_step == 0 && config::Visualization::withScreenshots())
    doScreenshot();

  step();

  _controller->view()->update();

  if (config::Visualization::withScreenshots())
    doScreenshot();

  if (finished()) emit completed();
}

void GraphicSimulation::doScreenshot(void) const {
  static const std::string screenshotFolder = "screenshots/";
  static const QSize screenshotSize = config::Visualization::screenshotResolution();

  if (_step == 0) stdfs::remove_all(screenshotFolder);
  stdfs::create_directory(screenshotFolder);

  QPixmap p = _controller->view()->screenshot(screenshotSize);

  std::ostringstream oss;
  oss << screenshotFolder << "step_" << _step << ".png";
  p.save(QString::fromStdString(oss.str()));
}

} // end of namespace visu
