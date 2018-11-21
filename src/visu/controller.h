#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QMainWindow>

#include "graphicsimulation.h"
#include "mainview.h"

namespace visu {

class Controller : public QObject {
  Q_OBJECT

  GraphicSimulation &_simulation;

  QMainWindow &_window;
  gui::MainView *_view;

public:
  Controller(GraphicSimulation &s, QMainWindow &w, gui::MainView *v);

  auto& simulation (void) {
    return _simulation;
  }

  auto* view (void) {
    return _view;
  }
};

} // end of namespace visu

#endif // CONTROLLER_H
