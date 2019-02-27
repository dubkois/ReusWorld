#ifndef GUI_ENVIRONMENT_H
#define GUI_ENVIRONMENT_H

#include <QGraphicsItem>

#include "../simu/environment.h"

namespace gui {

class Environment : public QGraphicsItem {
  const simu::Environment &_object;
public:
  Environment(const simu::Environment &e) : _object(e) {}

  void updateBounds (void) {
    prepareGeometryChange();
  }

  QRectF boundingRect (void) const override {
    return QRectF(-_object.xextent(), -_object.yextent(),
                  _object.width(), _object.height());
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
  QColor colorForTemperature(float t);
};

}

#endif // GUI_ENVIRONMENT_H
