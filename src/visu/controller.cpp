#include <QApplication>
#include <QToolBar>
#include <QMenuBar>

#include "controller.h"
#include "tools/fastforwarddialog.h"

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
  QMenu *menuVisualisation = menuBar->addMenu("Visualisation");
  QMenu *menuPhylogeny = menuBar->addMenu("Phylogeny");

  QToolBar *controlToolbar = new QToolBar,
           *statusToolbar = new QToolBar;

  // == Simu ===================================================================

  QAction *save = buildAction(QStyle::SP_DialogSaveButton, "Save",
                                QKeySequence("Ctrl+S"));

  MultiAction *playPause = buildMultiAction(QKeySequence(" "),
                                            QStyle::SP_MediaPlay, "Play",
                                            QStyle::SP_MediaPause, "Pause");

  QAction *slower = buildAction(QStyle::SP_MediaSeekBackward, "Slow down",
                                QKeySequence("Ctrl+-"));

  QAction *faster = buildAction(QStyle::SP_MediaSeekForward, "Speed up",
                                QKeySequence("Ctrl++"));

  QAction *step = buildAction(QStyle::SP_MediaSkipForward, "Step",
                              QKeySequence("N"));

  QAction *jumpto = buildAction(QIcon::fromTheme("go-jump"), "Go to time",
                                QKeySequence("Ctrl+T"));

  QAction *nextPlant = buildAction(QStyle::SP_ArrowRight, "Next",
                                   QKeySequence::MoveToNextChar);

  QAction *prevPlant = buildAction(QStyle::SP_ArrowLeft, "Prev",
                                   QKeySequence::MoveToPreviousChar);

  // == Visu ===================================================================

  QAction *fullscreen = buildAction(QIcon::fromTheme("view-fullscreen"),
                                    "Fullscreen", QKeySequence("F11"));

  QAction *snapshot = buildAction(QIcon::fromTheme("camera-photo"),
                                  "Snapshot", QKeySequence("Ctrl+P"));

  QAction *zoomOut = buildAction(QIcon::fromTheme("zoom-out"), "Zoom out",
                                 QKeySequence("-"));

  QAction *zoomIn = buildAction(QIcon::fromTheme("zoom-in"), "Zoom in",
                                QKeySequence("+"));

  // == Phylogeny ==============================================================

  MultiAction *togglePTree = buildMultiAction(
                              QKeySequence("T"),
                              QStyle::SP_TitleBarUnshadeButton, "Show PTree",
                              QStyle::SP_TitleBarShadeButton, "Hide PTree");

  QAction *savePTree = buildAction(QStyle::SP_DialogSaveButton, "Save",
                                   QKeySequence("Ctrl+Shift+S"));

  QAction *saveMorphologies = buildAction(QStyle::SP_DialogSaveButton, "Save phenotypes",
                                          QKeySequence("Ctrl+Shift+M"));

  // ===========================================================================

  _window.setCentralWidget(_view);

  menuSimulation->addAction(save);
  menuSimulation->addSeparator();
  menuSimulation->addAction(playPause);
  menuSimulation->addAction(step);
  menuSimulation->addAction(jumpto);
  menuSimulation->addSeparator();
  menuSimulation->addAction(slower);
  menuSimulation->addAction(faster);
  menuSimulation->addSeparator();
  menuSimulation->addAction(prevPlant);
  menuSimulation->addAction(nextPlant);

  menuVisualisation->addAction(fullscreen);
  menuVisualisation->addAction(snapshot);
  menuVisualisation->addAction(zoomOut);
  menuVisualisation->addAction(zoomIn);

  menuPhylogeny->addAction(togglePTree);
  menuPhylogeny->addAction(savePTree);
  menuPhylogeny->addAction(saveMorphologies);

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

  connect(save, &QAction::triggered, [this] {
    _simulation.saveAs();
  });
  connect(playPause, &MultiAction::triggered, [this] (const QString&, uint index) {
    playInternal(index);
  });
  connect(this, &Controller::playStatusChanged,
          playPause, &MultiAction::trigger);
  connect(slower, &QAction::triggered, [this] {
    _speed = std::min(1.f, _speed / 2.f); play(_timer.isActive());
  });
  connect(faster, &QAction::triggered, [this] {
    _speed *= 2.f; play(_timer.isActive());
  });
  connect(step, &QAction::triggered, this, &Controller::step);
  connect(jumpto, &QAction::triggered, [this] {
    play(false);
    gui::FastForwardDialog ffd (_view, _simulation);
    ffd.exec();
    updateDisplays();
    _view->update();
  });
  connect(nextPlant, &QAction::triggered, _view,
          &gui::MainView::selectNextPlant);
  connect(prevPlant, &QAction::triggered, _view,
          &gui::MainView::selectPreviousPlant);

  connect(fullscreen, &QAction::triggered, [this] {
    if (!_window.isFullScreen())
      _window.showFullScreen();
    else
      _window.showNormal();
  });
  connect(snapshot, &QAction::triggered, [this] {
    QString filename =
      QFileDialog::getSaveFileName(_view, "Select filename", ".", "(*.png)");

    if (!filename.isEmpty()) {
      QPixmap p = _view->screenshot();
      p.save(filename);
      qDebug() << "Saved screenshot to " << filename;
    }
  });
  connect(zoomIn, &QAction::triggered, _view, &gui::MainView::zoomIn);
  connect(zoomOut, &QAction::triggered, _view, &gui::MainView::zoomOut);

  connect(togglePTree, &MultiAction::triggered,
          &_simulation, &GraphicSimulation::togglePViewer);
  connect(savePTree, &QAction::triggered,
          &_simulation, &GraphicSimulation::savePhylogeny);
  connect(saveMorphologies, &QAction::triggered,
          _view, &gui::MainView::saveMorphologiesInto);

  _speed = 1;
  connect(&_timer, &QTimer::timeout, this, &Controller::step);

  connect(&_simulation, &GraphicSimulation::initialized,
          this, &Controller::updateDisplays);

  _autoquit = false;
  connect(&_simulation, &GraphicSimulation::completed, [this] {
    play(false);
    if (_autoquit) QApplication::instance()->quit();
  });
}

QAction* Controller::buildAction(QStyle::StandardPixmap pixmap,
                                 const QString &name,
                                 const QKeySequence &shortcut) {

  return buildAction(style()->standardIcon(pixmap), name, shortcut);
}

QAction* Controller::buildAction(const QIcon &icon,
                                 const QString &name,
                                 const QKeySequence &shortcut) {

  QAction *action = new QAction(icon, name, _view);
  action->setShortcut(shortcut);
  return action;
}

void Controller::play(bool b) {
  if (_timer.isActive() != b) emit playStatusChanged(b);
  playInternal(b);
}

void Controller::playInternal(bool b) {
  if (b)  _timer.start(1000 / config::Simulation::stepsPerDay());
  else    _timer.stop();
  updateDisplays();
}

void Controller::step(void) {
  _simulation.graphicalStep(_speed);
  updateDisplays();
}

void Controller::updateMousePosition (const QPointF &pos) {
  QString string;
  QTextStream qss (&string);
  qss //<< qSetRealNumberPrecision(2)
      << pos.x() << " x " << -pos.y();
  _mdisplay->setText(string);
}

void Controller::updateDisplays(void) {
  QString string;
  QTextStream qss (&string);
  qss << "Time: " << QString::fromStdString(_simulation.time().pretty())
      << ", Speed: " << _speed << " dps";
  _ldisplay->setText(string);

  string.clear();
  const auto& plants = _simulation.plants();
  if (plants.size() > 0) {
    qss << plants.size();
    uint minGen = std::numeric_limits<uint>::max(), maxGen = 0;
    for (const auto &p: plants) {
      uint g = p.second->genealogy().generation;
      if (g < minGen) minGen = g;
      if (maxGen < g) maxGen = g;
    }
    qss << " plants, Gens: [" << minGen << ";" << maxGen << "]";
  } else
    qss << "population died out";
  _rdisplay->setText(string);
}

} // end of namespace visu
