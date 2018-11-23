#include <QPainter>
#include <qmath.h>

#include "plant.h"

#include <QDebug>

namespace gui {

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

QPointF toQPoint (const simu::Point &p) {
  return QPointF(p.x, -p.y);
}

QRectF toRect (const simu::Point &c, float l, float w) {
  return QRectF(toQPoint(c) + QPointF(0, -.5*w), QSizeF(l, w));
}

QRectF toRect (const simu::Rect &r) {
  return QRectF(toQPoint(r.ul), toQPoint(r.br));
}

Plant::Plant(const simu::Plant &p) : _plant(p) {
  setPos(toQPoint(_plant.pos()));
  updateGeometry();
  updateTooltip();
}

void Plant::updateGeometry(void) {
  prepareGeometryChange();
  _boundingRect = toRect(_plant.boundingRect());
  float w = _boundingRect.width(), h = _boundingRect.height();
//  _boundingRect.adjust(-.1 * w, -.1 * h, .1 * w, .1 * h);
  qDebug() << "Bounding rect: " << boundingRect();
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

void Plant::mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) {
  std::cout << _plant << std::endl;
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();

  QPen pen = painter->pen();
  pen.setWidthF(.5 * config::PlantGenome::ls_segmentWidth());
  pen.setColor(Qt::red);
  painter->setPen(pen);
  painter->drawRect(boundingRect());
  pen.setColor(Qt::blue);
  painter->setPen(pen);
  for (const simu::Organ *o: _plant.organs())
    painter->drawRect(toRect(o->boundingRect()));


//  qDebug() << "Bounding rect: " << boundingRect();
  for (const simu::Organ *o: _plant.organs()) {
    const auto &p = o->globalCoordinates();
    QPointF p0 = toQPoint(p.start);
    float r = -qRadiansToDegrees(p.rotation);
//    qDebug() << "applying(" << o.symbol << p << r << ")";
    painter->save();
    painter->translate(p0);
    painter->rotate(r);
    painter->fillRect(toRect({0,0}, o->length(), o->width()), color(o->symbol()));
    painter->restore();
  }

//  for (const auto &o_ptr: _plant.organs()) {
//    simu::Organ &o = *o_ptr;
//    QPointF p = toQPoint(o.start);
//    float r = -qRadiansToDegrees(o.rotation);
//    painter->save();
//    painter->translate(p);
//    painter->rotate(r);
//    painter->fillRect(QRectF(-.5*o.width,-.5*o.width, o.width, o.width), Qt::black);
//    painter->fillRect(QRectF(0, -.25*o.width, 4*o.width, .5*o.width), Qt::black);
//    painter->restore();
//  }

//  qDebug() << "";
  painter->restore();
}

} // end of namespace gui
