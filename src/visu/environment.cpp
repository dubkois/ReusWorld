#include <QPainter>

#include "environment.h"
#include "../simu/tiniestphysicsengine.h"
#include "qtconversions.hpp"

#include <QDebug>

namespace gui {

const QColor sky = QColor::fromRgbF(.13, .5, .7);
const QColor ground = QColor::fromRgbF(.5, .4, .3);

static constexpr int debugAABB = 0;
static constexpr int debugLeaves = 0;
static constexpr int debugSpores = 1;

void Environment::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->fillRect(-_object.xextent(), -_object.yextent(),
                    _object.width(), _object.yextent(), sky);

  painter->fillRect(-_object.xextent(), 0,
                    _object.width(), _object.yextent(), ground);

  painter->save();
  QPen pen = painter->pen();
  pen.setWidthF(0);
  painter->setPen(pen);

  if (debugAABB || debugLeaves) {
    using CData = simu::physics::CollisionData;
    using CObject = CData::CObject;

    painter->save();
    QPen dotPen = pen;
    dotPen.setStyle(Qt::DashLine);

    const CData &cd = _object.collisionData();
    for (const CObject &obj: cd.data()) {
      using ULItem = simu::physics::UpperLayer::Item;

      if (debugAABB) {
        painter->save();
        QRectF r = toQRect(obj.boundingRect);
        pen.setColor(Qt::red);
        painter->setPen(pen);
        painter->drawRect(r);
        painter->restore();
      }

      if (debugLeaves) {
        auto y = -1.05 * obj.plant->boundingRect().t();

        const auto &items = obj.layer.items;
        for (const ULItem &item: items) {

          if (debugLeaves & 1) {  // Draw upper layer segments
            painter->setPen(pen);
            painter->drawLine(QPointF(item.l, .975*y), QPointF(item.l, 1.025*y));
            painter->drawLine(QPointF(item.r, .975*y), QPointF(item.r, 1.025*y));
            painter->drawLine(QPointF(item.l, y), QPointF(item.r, y));
          }

          if (debugLeaves & 2) {  // Draw line to associated organ
            painter->setPen(dotPen);
            auto oy = toQRect(item.organ->globalCoordinates().boundingRect).top();
            painter->drawLine(QPointF(.5 * (item.l + item.r), y),
                              QPointF(.5 * (item.l + item.r), oy));
          }
        }

        if ((debugLeaves & 4) && !items.empty()) {  // Draw upper layer enveloppe
          painter->setPen(dotPen);
          ULItem prev = items[0];
          auto prevY = toQRect(prev.organ->globalCoordinates().boundingRect).top();
          painter->drawLine(QPointF(prev.l, prevY), QPointF(prev.r, prevY));
          for (uint i=1; i<items.size(); i++) {
            ULItem curr = items[i];
            auto oy = toQRect(curr.organ->globalCoordinates().boundingRect).top();
            painter->drawLine(QPointF(prev.r, prevY), QPointF(curr.l, oy));
            painter->drawLine(QPointF(curr.l, oy), QPointF(curr.r, oy));
            prev = curr;
            prevY = oy;
          }
        }
      }
    }
    painter->restore();
  }

  if (debugSpores) {
    painter->save();
    pen.setColor(Qt::blue);
    painter->setPen(pen);
    painter->setBrush(QColor::fromRgbF(0, 0, 1, .25));

    const auto& pistils = _object.collisionData().pistils();
    for (const simu::physics::Pistil &s: pistils)
      if (s.organ->fullness() > .5)
        painter->drawEllipse(toQPoint(s.boundingDisk.center),
                             s.boundingDisk.radius, s.boundingDisk.radius);

    painter->restore();
  }

  painter->restore();
}

} // end of namespace gui
