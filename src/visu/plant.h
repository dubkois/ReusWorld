#ifndef VISU_PLANT_H
#define VISU_PLANT_H

#include <QGraphicsItem>

#include "../simu/plant.h"

namespace gui {

class Plant : public QGraphicsObject {
  Q_OBJECT

  simu::Plant &_plant;
  QRectF _boundingRect;
  bool _selected;

public:
  Plant(simu::Plant &p);

  QRectF boundingRect(void) const override {
    return _boundingRect;
  }

  const auto& plant (void) const {
    return _plant;
  }

  void setSelected(bool selected) {
    _selected = selected;
  }
  bool isSelected(void) const {
    return _selected;
  }

  void updateGeometry(void);
  void updateTooltip(void);
  void updatePlantData(void);

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e);
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *e);

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);

signals:
  void selected (Plant *me);

private:
  struct PlantMenu;
  static PlantMenu* contextMenu (void);
};

} // end of namespace gui

#endif // VISU_PLANT_H
