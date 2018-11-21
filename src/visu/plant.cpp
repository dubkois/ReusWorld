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

QRectF toRectF (const simu::Point &c, float l, float w) {
  return QRectF(toQPoint(c) + QPointF(0, -.5*w), QSizeF(l, w));
}

Plant::Plant(const simu::Plant &p) : _plant(p) {
  setPos(toQPoint(_plant.pos()));
  _boundingRect = QRectF();
  for (const auto &o: _plant.organs())
    _boundingRect = _boundingRect.united(QRectF(toQPoint(o->start), toQPoint(o->end)));
  qDebug() << "Bounding rect: " << boundingRect();
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();
  QPen pen = painter->pen();
  pen.setCapStyle(Qt::RoundCap);
//  qDebug() << "Bounding rect: " << boundingRect();
  for (const auto &o_ptr: _plant.organs()) {
    simu::Organ &o = *o_ptr;
    QPointF p = toQPoint(o.start);
    float r = -qRadiansToDegrees(o.rotation);
//    qDebug() << "applying(" << o.symbol << p << r << ")";
    painter->save();
    painter->translate(p);
    painter->rotate(r);
    painter->fillRect(toRectF({0,0}, o.length, o.width), color(o.symbol));\
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
