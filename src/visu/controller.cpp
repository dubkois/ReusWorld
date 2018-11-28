#include <QApplication>
#include <QToolBar>
#include <QMenuBar>

#include "controller.h"

#include <QDebug>

namespace visu {

auto style (void) {
  static QStyle *style = QApplication::style();
  return style;
}

void MultiAction::addState (QStyle::StandardPixmap p, const QString &name) {
  addState(style()->standardIcon(p), name);
}

void MultiAction::addState(const QIcon &icon, const QString &name) {
  _states.append({icon, name});
}

MultiAction::State& MultiAction::updateState(void) {
  State &s = _states[_index];
  setIcon(s.icon);
  setText(s.name);
  return s;
}

void MultiAction::signalForwarder(void) {
  _index = (_index + 1) % _states.size();
  State &s = updateState();
  emit triggered(s.name, _index);
}

QWidget* spacer (void) {
  QWidget *dummy = new QWidget();
  dummy->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return dummy;
}

template <typename... ARGS>
MultiAction* Controller::buildMultiAction (const QKeySequence &shortcut, ARGS... args) {

  MultiAction *action = new MultiAction(_view, args...);
  action->setShortcut(shortcut);
  return action;
}

Controller::Controller(GraphicSimulation &s, QMainWindow &w, gui::MainView *v)
  : _simulation(s), _window(w), _view(v) {

  w.setWindowTitle("ReusWorld");

  _ldisplay = new QLabel, _mdisplay = new QLabel, _rdisplay = new QLabel;

  QMenuBar *menuBar = w.menuBar();
  QMenu *menuSimulation = menuBar->addMenu("Simulation");

  menuBar->addMenu(menuSimulation);

  QToolBar *controlToolbar = new QToolBar,
           *statusToolbar = new QToolBar;

  QAction *reset = buildAction(QStyle::SP_BrowserReload, "Reset",
                               QKeySequence("Ctrl+R"));

  MultiAction *playPause = buildMultiAction(QKeySequence(" "),
                                            QStyle::SP_MediaPlay, "Play",
                                            QStyle::SP_MediaPause, "Pause");

  QAction *slower = buildAction(QStyle::SP_MediaSeekBackward, "Slow down",
                                QKeySequence("-"));

  QAction *faster = buildAction(QStyle::SP_MediaSeekForward, "Speed up",
                                QKeySequence("+"));

  QAction *step = buildAction(QStyle::SP_MediaSkipForward, "Step",
                              QKeySequence("N"));

  QAction *nextPlant = buildAction(QStyle::SP_ArrowRight, "Next",
                                   QKeySequence::MoveToNextChar);

  QAction *prevPlant = buildAction(QStyle::SP_ArrowLeft, "Prev",
                                   QKeySequence::MoveToPreviousChar);

  _window.setCentralWidget(_view);

  menuSimulation->addAction(reset);
  menuSimulation->addAction(playPause);
  menuSimulation->addAction(step);
  menuSimulation->addSeparator();
  menuSimulation->addAction(prevPlant);
  menuSimulation->addAction(nextPlant);

  statusToolbar->setMovable(false);
  _window.addToolBar(Qt::BottomToolBarArea, statusToolbar);
    statusToolbar->addWidget(_ldisplay);
    statusToolbar->addWidget(spacer());
    statusToolbar->addWidget(_mdisplay);
    statusToolbar->addWidget(spacer());
    statusToolbar->addWidget(_rdisplay);

  controlToolbar->setMovable(false);
  _window.addToolBarBreak(Qt::BottomToolBarArea);
  _window.addToolBar(Qt::BottomToolBarArea, controlToolbar);
    controlToolbar->addWidget(spacer());
    controlToolbar->addAction(reset);
    controlToolbar->addAction(playPause);
    controlToolbar->addAction(slower);
    controlToolbar->addAction(faster);
    controlToolbar->addAction(step);
    controlToolbar->addSeparator();
    controlToolbar->addAction(prevPlant);
    controlToolbar->addAction(nextPlant);
    controlToolbar->addWidget(spacer());

  _simulation.setController(this);
  _view->setController(this);

  connect(reset, &QAction::triggered, [this] {
    play(false);
    _simulation.reset();
  });
  connect(playPause, &MultiAction::triggered, [this] (const QString&, uint index) {
    play(index);
  });
  connect(slower, &QAction::triggered, [this] {
    _speed /= 2.f; play(_timer.isActive());
  });
  connect(faster, &QAction::triggered, [this] {
    _speed *= 2.f; play(_timer.isActive());
  });
  connect(step, &QAction::triggered, this, &Controller::step);
  connect(nextPlant, &QAction::triggered, _view, &gui::MainView::selectNextPlant);
  connect(prevPlant, &QAction::triggered, _view, &gui::MainView::selectPreviousPlant);

  _speed = 1;
  connect(&_timer, &QTimer::timeout, this, &Controller::step);

  connect(&_simulation, &GraphicSimulation::initialized,
          this, &Controller::updateDisplays);
}

QAction* Controller::buildAction(QStyle::StandardPixmap pixmap,
                                 const QString &name,
                                 const QKeySequence &shortcut) {

  QAction *action = new QAction(style()->standardIcon(pixmap), name, _view);
  action->setShortcut(shortcut);
  return action;
}

void Controller::play(bool b) {
  if (b)  _timer.start(1000 / (_speed * config::Simulation::stepsPerDay()));
  else    _timer.stop();
  updateDisplays();
}

void Controller::step(void) {
  _simulation.step();
  updateDisplays();
}

void Controller::updateMousePosition (const QPointF &pos) {
  QString string;
  QTextStream qss (&string);
  qss //<< qSetRealNumberPrecision(2)
      << pos.x() << " x " << pos.y();
  _mdisplay->setText(string);
}

void Controller::updateDisplays(void) {
  QString string;
  QTextStream qss (&string);
  qss << "Time: " << _simulation.day() << ", Speed: " << _speed << " dps";
  _ldisplay->setText(string);

  string.clear();
  const auto& plants = _simulation.plants();
  qss << plants.size();
  uint minGen = std::numeric_limits<uint>::max(), maxGen = 0;
  for (const auto &p: plants) {
    uint g = p.second->genome().cdata.generation;
    if (g < minGen) minGen = g;
    if (maxGen < g) maxGen = g;
  }
  qss << " plants, Gens: [" << minGen << ";" << maxGen << "]";
  _rdisplay->setText(string);
}

} // end of namespace visu
