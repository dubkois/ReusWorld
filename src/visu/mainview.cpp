#include <QGraphicsScene>
#include <QEvent>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"

#include <QDebug>

namespace gui {

MainView::MainView(const simu::Environment &e, QWidget *parent)
  : QGraphicsView(parent), _scene(new QGraphicsScene(parent)),
    _env(new Environment(e)) {

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

void MainView::addPlantItem(const simu::Plant &sp) {
  Plant *gp = new Plant(sp);
  _map[sp.pos().x] = gp;
  _scene->addItem(gp);
  viewPlant(gp);
}

void MainView::updatePlantItem(float x) {
  Plant *p = _map.value(x);
  assert(p);
  p->updatePlantData();
}

void MainView::delPlantItem(float x) {
  Plant *p = _map.value(x);
  assert(p);
  _scene->removeItem(p);
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

void MainView::viewPlant(Plant *p) {
  fitInView(p->boundingRect(), Qt::KeepAspectRatio);
}

} // end of namespace gui
