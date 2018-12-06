#include <QGraphicsScene>
#include <QEvent>
#include <QMouseEvent>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"
#include "controller.h"

#include <QDebug>

namespace gui {

MainView::MainView(const simu::Environment &e, QWidget *parent)
  : QGraphicsView(parent), _scene(new QGraphicsScene(parent)),
    _env(new Environment(e)), _selection(nullptr), _zoom(1) {

  setScene(_scene);
  setDragMode(ScrollHandDrag);
  new Graphics_view_zoom(this);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  _scene->addItem(_env);
  setRenderHint(QPainter::Antialiasing, true);

  installEventFilter(this);
}

void MainView::setController(visu::Controller *c) {
  _controller = c;
}

void MainView::addPlantItem(simu::Plant &sp) {
  Plant *gp = new Plant(sp);
  _plants[sp.pos().x] = gp;
  _scene->addItem(gp);

  connect(gp, &Plant::selected, this, &MainView::updateSelection);
}

void MainView::updatePlantItem(float x) {
  Plant *p = _plants.value(x);
  assert(p);
  p->updatePlantData();
  if (p == _selection && _selection->isSelected())  focusOnSelection();
}

void MainView::delPlantItem(float x) {
  auto it = _plants.find(x);
  assert(it != _plants.end());
  Plant *p = *it;
  assert(p);
  if (p == _selection)  selectNextPlant();
  _plants.erase(it);
  _scene->removeItem(p);
  p->deleteLater();
}

bool MainView::eventFilter(QObject*, QEvent *event) {
  if (event->type() == QEvent::Resize)
    _viewportCenter = mapToScene(viewport()->geometry().center());
  return false;
}

void MainView::resizeEvent (QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  centerOn(_viewportCenter);
}

void MainView::mouseMoveEvent(QMouseEvent *e) {
  _controller->updateMousePosition(mapToScene(e->pos()));
  QGraphicsView::mouseMoveEvent(e);
}

void MainView::selectPreviousPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.last();
    else {
      auto it = _plants.find(_selection->pos().x());
      if (it != _plants.begin())
        p = *(--it);
      else
        p = _plants.last();
    }
    updateSelection(p);
  }
}

void MainView::selectNextPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.first();
    else {
      auto it = _plants.find(_selection->pos().x());
      if (it != _plants.end()-1)
        p = *(++it);
      else
        p = _plants.first();
    }
    updateSelection(p);
  }
}

void MainView::updateSelection(Plant *that) {
  if (_selection) _selection->setSelected(false);
  _selection = that;
  _selection->setSelected(true);
  focusOnSelection();
}

void MainView::focusOnSelection(void) {
  if (!_selection)  return;
  fitInView(_selection->boundingRect().translated(_selection->pos()),
            Qt::KeepAspectRatio);
  scale(_zoom, _zoom);
}

} // end of namespace gui
