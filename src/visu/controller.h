#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QMainWindow>
#include <QAction>
#include <QTimer>
#include <QStyle>
#include <QLabel>

#include "graphicsimulation.h"
#include "mainview.h"

namespace visu {

class MultiAction : public QAction {
  Q_OBJECT

  uint _index;
  struct State {
    QIcon icon;
    QString name;
  };
  QVector<State> _states;

  void addState(QStyle::StandardPixmap p, const QString &name);
  void addState(const QIcon &icon, const QString &name);

  template <typename I, typename... ARGS>
  void parseStates (const I &i, const QString &name, ARGS... args) {
    addState(i, name);
    parseStates(args...);
  }

  void parseStates(void) {}

  State& updateState (void);

public:
  template <typename... ARGS>
  MultiAction (QObject *parent, ARGS... args) : QAction(parent) {
    _index = 0;
    parseStates(args...);
    updateState();
    connect(this, &QAction::triggered, this, &MultiAction::signalForwarder);
  }

  void signalForwarder (void);

signals:
  void triggered (const QString &state, uint index);
};

class Controller : public QObject {
  Q_OBJECT

  GraphicSimulation &_simulation;

  QMainWindow &_window;
  gui::MainView *_view;

  QLabel *_ldisplay, *_mdisplay, *_rdisplay;

  QTimer _timer;
  uint _speed;
  bool _autoquit;

public:
  Controller(GraphicSimulation &s, QMainWindow &w, gui::MainView *v);

  auto& simulation (void) {
    return _simulation;
  }

  auto* view (void) {
    return _view;
  }

  void prevPlant (void) {
    _view->selectPreviousPlant();
  }

  void nextPlant (void) {
    _view->selectNextPlant();
  }

  void setSpeed (float s) {
    _speed = s;
  }

  void setAutoQuit (bool q) {
    _autoquit = q;
  }

  void updateMousePosition (const QPointF &pos);

  void step (void);

public slots:
  void play (bool b);

signals:
  void playStatusChanged (bool play);

private:
  QAction* buildAction(QStyle::StandardPixmap pixmap, const QString &name,
                       const QKeySequence &shortcut);

  QAction* buildAction(const QIcon &icon, const QString &name,
                       const QKeySequence &shortcut);

  template <typename... ARGS>
  MultiAction* buildMultiAction (const QKeySequence &shortcut, ARGS... args);

  void updateDisplays (void);

  void playInternal (bool b);
};

} // end of namespace visu

#endif // CONTROLLER_H
