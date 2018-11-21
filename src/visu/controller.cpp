#include <QApplication>
#include <QToolBar>
#include <QAction>
#include <QStyle>

#include "controller.h"

namespace visu {

QWidget* spacer (void) {
  QWidget *dummy = new QWidget();
  dummy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return dummy;
}

Controller::Controller(GraphicSimulation &s, QMainWindow &w, gui::MainView *v)
  : _simulation(s), _window(w), _view(v) {

  w.setWindowTitle("ReusWorld");

  QToolBar *toolbar = new QToolBar;
  QStyle *style = QApplication::style();
  QAction *step = new QAction(style->standardIcon(QStyle::SP_MediaSkipForward), "Step", _view);

  _window.setCentralWidget(_view);

  toolbar->setMovable(false);
  _window.addToolBar(Qt::BottomToolBarArea, toolbar);
    toolbar->addWidget(spacer());
    toolbar->addAction(step);
    toolbar->addWidget(spacer());

  _simulation.setController(this);
  _view->setController(this);

  connect(step, &QAction::triggered, &_simulation, &GraphicSimulation::step);
}


} // end of namespace visu
