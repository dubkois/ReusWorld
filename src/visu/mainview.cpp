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
  _scene->setSceneRect(_env->boundingRect());

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

void MainView::updatePlantItem (simu::Plant &sp) {
  auto it = _plants.find(sp.pos().x);
  assert(it != _plants.end());
  Plant *p = *it;
  assert(p);
  p->updateGeometry();
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

void MainView::update(void) {
  _env->update();
  for (Plant *p: _plants) {
    assert(p);
    p->updatePlantData();

    if (p == _selection && _selection->isSelected())  focusOnSelection();
  }
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
  updateSelection(nullptr);
  QGraphicsView::mouseDoubleClickEvent(e);
}

void MainView::selectPreviousPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.last();
    else {
      auto itBase = _plants.find(_selection->pos().x()), it = itBase;
      do {
        if (it != _plants.begin())
          --it;
        else
          it = std::prev(_plants.end());
        p = *it;
      } while (p->plant().isInSeedState() && it != itBase);
    }
    updateSelection(p);
  }
}

void MainView::selectNextPlant(void) {
  if (!_plants.empty()) {
    Plant *p = nullptr;
    if (!_selection)  p = _plants.first();
    else {
      auto itBase = _plants.find(_selection->pos().x()), it = itBase;
      do {
        ++it;
        if (it == _plants.end())  it = _plants.begin();
        p = *it;
      } while (p->plant().isInSeedState() && it != itBase);
    }
    updateSelection(p);
  }
}

void MainView::updateSelection(Plant *that) {
  if (_selection) {
    _selection->setSelected(false);
    _selection->update();
  }
  _selection = that;
  if (_selection) {
    _selection->setSelected(true);
    _selection->update();
  }
  focusOnSelection();
}

void MainView::focusOnSelection(void) {
  if (!_selection)  return;
  fitInView(_selection->boundingRect().translated(_selection->pos()),
            Qt::KeepAspectRatio);
  scale(_zoom, _zoom);
  _controller->updateMousePosition(mapToScene(_selection->pos().toPoint()));
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

QPixmap MainView::screenshot(QSize size) {
  if (!size.isValid() || size.isNull())
    utils::doThrow<std::invalid_argument>(
      "Invalid screenshots size: ", size.width(), "x", size.height());

  QRect src = viewport()->rect();
  if (size.width() > size.height())
    size.setHeight(size.width() * src.height() / src.width());
  else
    size.setWidth(size.height() * src.width() / src.height());

  QPixmap pixmap (size);
  QPainter painter (&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  render(&painter, pixmap.rect(), src);

  return pixmap;
}

} // end of namespace gui
