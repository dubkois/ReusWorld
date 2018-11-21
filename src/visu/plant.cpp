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
  return QPointF(p.x, p.y);
}

QRectF toRectF (const simu::Point &c, float l, float w) {
  return QRectF(toQPoint(c) + QPointF(.5*w, l), QSize(l, w));
}

Plant::Plant(const simu::Plant &p) : _plant(p) {
  _boundingRect = QRectF();
  for (const auto &o: _plant.organs())
    _boundingRect = _boundingRect.united(QRectF(toQPoint(o->start), toQPoint(o->end)));
  qDebug() << "Bounding rect: " << boundingRect();
}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();
  qDebug() << "Bounding rect: " << boundingRect();
  painter->translate(toQPoint(_plant.pos()));
  for (const auto &o_ptr: _plant.organs()) {
    simu::Organ &o = *o_ptr;
    painter->rotate(qRadiansToDegrees(o.rotation));
    painter->fillRect(toRectF(o.start, o.length, o.width), color(o.symbol));
    qDebug() << "painted";
  }
  qDebug() << "";
  painter->restore();
}

} // end of namespace gui
