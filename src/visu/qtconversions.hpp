#ifndef QTCONVERSIONS_HPP
#define QTCONVERSIONS_HPP

#include "../simu/types.h"

#include <QRectF>

namespace gui {

inline QPointF toQPoint (const simu::Point &p) {
  return QPointF(p.x, -p.y);
}

inline QRectF toRect (const simu::Point &c, float l, float w) {
  return QRectF(toQPoint(c) + QPointF(0, -.5*w), QSizeF(l, w));
}

inline QRectF toRect (const simu::Rect &r) {
  return QRectF(toQPoint(r.ul), toQPoint(r.br));
}

} // end of namespace gui

#endif // QTCONVERSIONS_HPP
