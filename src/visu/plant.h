#ifndef VISU_PLANT_H
#define VISU_PLANT_H

#include <QGraphicsItem>

#include "../simu/plant.h"

namespace gui {

class Plant : public QGraphicsPathItem {
  const simu::Plant &_plant;
public:
  Plant(const simu::Plant &p);

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);
};

} // end of namespace gui

#endif // VISU_PLANT_H
