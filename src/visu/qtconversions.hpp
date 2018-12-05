#ifndef QTCONVERSIONS_HPP
#define QTCONVERSIONS_HPP

#include "../simu/types.h"

#include <QRectF>
#include <QColor>

namespace gui {

inline QPointF toQPoint (const simu::Point &p) {
  return QPointF(p.x, -p.y);
}

inline QRectF toQRect (const simu::Point &c, float l, float w) {
  return QRectF(toQPoint(c) + QPointF(0, -.5*w), QSizeF(l, w));
}

inline QRectF toQRect (const simu::Rect &r) {
  return QRectF(toQPoint(r.ul), toQPoint(r.br));
}

inline QColor mix (const QColor &lhs, const QColor &rhs, float r) {
  assert(0 <= r && r <= 1);
  return QColor::fromRgbF(
      r * lhs.redF() + (1-r) * rhs.redF(),
    r * lhs.greenF() + (1-r) * rhs.greenF(),
     r * lhs.blueF() + (1-r) * rhs.blueF()
  );
}

} // end of namespace gui

#endif // QTCONVERSIONS_HPP
