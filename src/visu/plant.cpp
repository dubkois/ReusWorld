#include <QPainter>
#include <qmath.h>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>

#include "plant.h"
#include "qtconversions.hpp"

#include <QDebug>

namespace gui {

struct Plant::PlantMenu : public QMenu {
  Plant *plantVisu;

  PlantMenu (void) {
    QAction *del = addAction("delete");

    connect(del, &QAction::triggered, [this] {
      plantVisu->_plant.kill();
      plantVisu->update();
    });
  }

  void popup(const QPoint &pos, Plant *visu) {
    qDebug() << "PlantMenu::popup(" << pos << ", " << uint(visu->plant().id()) << ")";
    plantVisu = visu;
    QMenu::popup(pos);
  }
};

auto segmentWidth (void) {
  return config::PlantGenome::ls_segmentWidth();
}

const auto& seedPath (void) {
  static const QPainterPath path = [] {
    QPainterPath seed;
    seed.addEllipse({0,0}, segmentWidth(), segmentWidth());
    return seed;
  }();
  return path;
}

const auto& pathForSymbol (char symbol) {
  static const auto defaultPath = [] {
    QPainterPath p;
    p.moveTo(0, -.5);
    p.lineTo(0, .5);
    p.lineTo(1, .5);
    p.lineTo(1, -.5);
    p.closeSubpath();
    return p;
  }();

  static const auto map = [] {
    std::map<char, QPainterPath> m;
    m['l'] = [] {
      QPainterPath p;
      p.lineTo(.5, .5);
      p.lineTo(1, 0);
      p.lineTo(.5, -.5);
      p.closeSubpath();
      return p;
    }();
    m['f'] = [] {
      float w = 2;
      QPainterPath p;
      p.quadTo(0, -.5*w,  1,  -.5*w);
      p.quadTo(1, -.25*w, .5, -.16*w);
      p.quadTo(1, -.16*w, 1,   0);
      p.quadTo(1,  .16*w, .5,  .16*w);
      p.quadTo(1,  .25*w, 1,   .5*w);
      p.quadTo(0,  .5*w,  0,   0);
      p.closeSubpath();
      return p;
    }();
    return m;
  }();

  auto it = map.find(symbol);
  if (it == map.end())
    return defaultPath;
  return it->second;
}

const auto& minBoundingBox (void) {
  static const auto l = segmentWidth();
  static const QRectF box = QRectF(-l, -l, 2*l, 2*l);
  return box;
}

const QColor seedColor = QColor::fromRgbF(.33,.42,.18);
const QColor& color (char symbol) {
  static const QColor black (Qt::black);
  static const std::map<char, QColor> others {
    { 's', QColor::fromRgbF(.55,.27,.07) },
    { 'l', QColor(Qt::green) },
    { 'f', QColor::fromRgbF(.75,0,0) },

    { 't', QColor::fromRgbF(.82,.71,.55) },
    { 'h', QColor(Qt::gray) },
  };
  if (std::isupper(symbol))
    return black;
  else
    return others.at(symbol);
}

void desaturate (QColor &c) {
  float s = c.saturationF();
  c = QColor::fromHsvF(c.hueF(), .25 * s, .5 * c.valueF());
}

Plant::Plant(simu::Plant &p) : _plant(p), _selected(false) {
  setPos(toQPoint(_plant.pos()));
  updateGeometry();
  updateTooltip();
}

void Plant::updateGeometry(void) {
  prepareGeometryChange();
  _boundingRect = toRect(_plant.boundingRect())
                    .united(minBoundingBox());
  float m = .1 * std::min(_boundingRect.width(), _boundingRect.height());
  _boundingRect.adjust(-m, -m, m, m);
//  qDebug() << "Bounding rect: " << boundingRect();
}

void Plant::updateTooltip(void) {
  std::ostringstream oss;
  oss << _plant.genome();
  setToolTip(QString::fromStdString(oss.str()));
}

void Plant::updatePlantData(void) {
  updateGeometry();
  updateTooltip();
  update();
}

void Plant::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
  qDebug() << "Plant::mouseDoubleClickEvent(" << e->button() << "," << e->buttons() << ")";
  if (e->button() == Qt::LeftButton) {
    setSelected(true);
    emit selected(this);
  }

  QGraphicsObject::mouseDoubleClickEvent(e);
}

void Plant::contextMenuEvent(QGraphicsSceneContextMenuEvent *e) {
  qDebug() << "Plant::contextMenuEvent()";
  contextMenu()->popup(e->screenPos(), this);
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  static constexpr bool drawOrganContour = true;
  static constexpr int drawSimuBoundingBox = 0;
  static constexpr bool drawQtBoundingBox = false;

  painter->save();
  QPen pen = painter->pen();
  pen.setWidth(0);
  painter->setPen(pen);

  if (drawSimuBoundingBox != 0) {
    painter->save();
      if (drawSimuBoundingBox & 1) {
        QRectF r = toRect(_plant.boundingRect());
        pen.setColor(Qt::blue);
        painter->setPen(pen);
        painter->drawRect(r);
      }
      if (drawSimuBoundingBox & 2) {
        pen.setColor(Qt::green);
        painter->setPen(pen);
        for (const simu::Organ *o: _plant.organs())
          painter->drawRect(toRect(o->boundingRect()));
      }
    painter->restore();
  }
  if (drawQtBoundingBox) {
    painter->save();
      QRectF r = boundingRect();
      pen.setColor(Qt::red);
      painter->setPen(pen);
      painter->drawRect(r);
    painter->restore();
  }

  bool doDrawOrganContour = drawOrganContour && isSelected();
  if (doDrawOrganContour) {
    pen.setColor(Qt::black);
    painter->setPen(pen);
  }

  for (const simu::Organ *o: _plant.organs()) {
    if (o->length() == 0) continue;
    const auto &p = o->globalCoordinates();
    QPointF p0 = toQPoint(p.start);
    float r = -qRadiansToDegrees(p.rotation);
    painter->save();
      painter->translate(p0);
      painter->rotate(r);

      painter->scale(o->length(), o->width());
      const QPainterPath &path = pathForSymbol(o->symbol());
      if (doDrawOrganContour) painter->drawPath(path);

      QColor c = color(o->symbol());
      if (_plant.isDead())  desaturate(c);
      painter->fillPath(path, c);

    painter->restore();
  }

//  qDebug() << "Drawing: " << uint(_plant.genome().cdata.id) << _plant.age();
  if (_plant.age() < 1) {
    const QPainterPath &path = seedPath();
    painter->fillPath(path, seedColor);
    if (doDrawOrganContour) painter->drawPath(path);
  }

  painter->restore();
}

Plant::PlantMenu* Plant::contextMenu(void) {
  static PlantMenu menu;
  return &menu;
}

} // end of namespace gui
