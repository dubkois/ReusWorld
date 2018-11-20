#include <QPainter>
#include <qmath.h>

#include "plant.h"

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

}

void Plant::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
  painter->save();
  painter->translate(toQPoint(_plant.pos()));
  for (const simu::Organ &o: _plant.organs()) {
    painter->rotate(qRadiansToDegrees(o.rotation));
    painter->fillRect(toRectF(o.start, o.length, o.width), color(o.symbol));
  }
  painter->restore();
}

} // end of namespace gui
