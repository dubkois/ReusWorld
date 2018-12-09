#include <QGraphicsScene>
#include <QEvent>
#include <QMouseEvent>

#include <QScrollBar>

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

void MainView::mouseDoubleClickEvent(QMouseEvent *e) {
  if (_selection) {
    _selection->setSelected(false);
    _selection->update();
    _selection = nullptr;
  }
  QGraphicsView::mouseDoubleClickEvent(e);
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

void MainView::paintEvent(QPaintEvent *e) {
  QGraphicsView::paintEvent(e);

  static constexpr float M = 10;
  static constexpr float H = 10;

  QPainter painter (viewport());

  QPointF p0 = rect().bottomLeft();
  p0 += QPointF(M, - M - .5*H - horizontalScrollBar()->height());

  QPointF p1 = p0 + QPointF(.1*rect().width(), 0);

  painter.drawLine(p0, p1);
  painter.drawLine(p0 + QPointF(0, -.5*H), p0 + QPointF(0, .5*H));
  painter.drawLine(p1 + QPointF(0, -.5*H), p1 + QPointF(0, .5*H));

  qreal width = mapToScene(p1.toPoint()).x() - mapToScene(p0.toPoint()).x();
  QString unit = "m";

  if (width < 1) {
    width *= 100.;
    unit = "cm";
  }
  if (width < 1) {
    width *= 10.;
    unit = "mm";
  }

  painter.drawText(QRectF(p0.x(), p0.y() -1.5*H, p1.x() - p0.x(), 1.5*H),
                   Qt::AlignCenter, QString::number(width, 'f', 2) + unit);
}

} // end of namespace gui
