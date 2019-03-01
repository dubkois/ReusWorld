#include <QCoreApplication>
#include <QFileDialog>

#include "graphicsimulation.h"

#include "../config/visuconfig.h"
#include "controller.h"

namespace visu {

GraphicSimulation::GraphicSimulation (void)
  : _pviewer(nullptr, _ptree) {}

void GraphicSimulation::setController(visu::Controller *c) {
  _controller = c;
}

bool GraphicSimulation::addPlant(const PGenome &p, float x, float biomass) {
  bool added = Simulation::addPlant(p, x, biomass);
  if (added)
    _controller->view()->addPlantItem(*_plants.at(x),
                                      _ptree.getSpeciesID(p.cdata.id));
  return added;
}

void GraphicSimulation::delPlant(float x, simu::Plant::Seeds &seeds) {
  _controller->view()->delPlantItem(x);
  Simulation::delPlant(x, seeds);
}

void GraphicSimulation::updatePlantAltitude(simu::Plant &p, float h) {
  Simulation::updatePlantAltitude(p, h);
  _controller->view()->updatePlantItem(p);
}

bool GraphicSimulation::init(const EGenome &env, const PGenome &plant) {
  bool ok = Simulation::init(env, plant);
  emit initialized(ok);
  _controller->view()->updateEnvironment();

  connect(&_pviewer, &PViewer::onSpeciesHoverEvent,
          _controller->view(), &gui::MainView::speciesHovered);

  return ok;
}

void GraphicSimulation::graphicalStep (uint speed) {
  if (_env.time().isStartOf() && config::Visualization::withScreenshots())
    doScreenshot();

  for (uint i=0; i<speed; i++) {
    step();
    QCoreApplication::processEvents();  // A bit ugly but what the hell
  }

  _controller->view()->update();

  if (config::Visualization::withScreenshots())
    doScreenshot();

  if (finished()) emit completed();
}

void GraphicSimulation::saveAs(void) const {
  QFileDialog dialog (_controller->view());
  dialog.setWindowTitle("Save simulation as");
  dialog.setFileMode(QFileDialog::AnyFile);
  dialog.setNameFilter("Save file (*.save)");
  dialog.setViewMode(QFileDialog::Detail);
  dialog.selectFile(QString::fromStdString(periodicSaveName()));
  if (dialog.exec())
    save(dialog.selectedFiles()[0].toStdString());
}

void GraphicSimulation::savePhylogeny (void) const {
  QString file = QFileDialog::getSaveFileName(_controller->view(), "Save PTree as");
  if (!file.isEmpty())  _ptree.saveTo(file.toStdString(), false);
}

void GraphicSimulation::doScreenshot(void) const {
  static const std::string screenshotFolder = "screenshots/";
  static const QSize screenshotSize = config::Visualization::screenshotResolution();

  if (_env.time().isStartOf()) stdfs::remove_all(screenshotFolder);
  stdfs::create_directory(screenshotFolder);

  QPixmap p = _controller->view()->screenshot(screenshotSize);

  std::ostringstream oss;
  oss << screenshotFolder << "sc_" << _env.time().pretty() << ".png";
  p.save(QString::fromStdString(oss.str()));
}

void GraphicSimulation::load (const std::string &file, GraphicSimulation &s) {
  Simulation::load(file, s);
  s._controller->view()->updateEnvironment();
  for (const auto &p: s._plants)
    s._controller->view()->addPlantItem(*p.second,
                                        s._ptree.getSpeciesID(p.second->id()));
  s._pviewer.build();

  connect(&s._pviewer, &PViewer::onSpeciesHoverEvent,
          s._controller->view(), &gui::MainView::speciesHovered);

  emit s.initialized(true);
}

} // end of namespace visu
