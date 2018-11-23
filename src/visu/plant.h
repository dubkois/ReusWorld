#ifndef VISU_PLANT_H
#define VISU_PLANT_H

#include <QGraphicsItem>

#include "../simu/plant.h"

namespace gui {

class Plant : public QGraphicsItem {
  const simu::Plant &_plant;
  QRectF _boundingRect;

public:
  Plant(const simu::Plant &p);

  QRectF boundingRect(void) const override {
    return _boundingRect;
  }

  void updateGeometry(void);
  void updateTooltip(void);
  void updatePlantData(void);

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);
};

} // end of namespace gui

#endif // VISU_PLANT_H
