#ifndef VISU_PLANT_H
#define VISU_PLANT_H

#include <QGraphicsItem>

#include "kgd/apt/core/tree/treetypes.h"
#include "../simu/plant.h"

namespace gui {

class Plant : public QGraphicsObject {
  Q_OBJECT

  using Species = phylogeny::SID;

  simu::Plant &_plant;
  Species _species;

  QRectF _boundingRect;
  bool _selected, _highlighted;

public:
  Plant(simu::Plant &p, Species s);

  QRectF boundingRect(void) const override {
    return _boundingRect;
  }

  const auto& plant (void) const {
    return _plant;
  }

  Species species (void) const {
    return _species;
  }

  void setSelected(bool selected) {
    _selected = selected;
  }
  bool isSelected(void) const {
    return _selected;
  }

  void setHighlighted (bool highlighted) {
    _highlighted = highlighted;
    update();
  }

  void updateGeometry(void);
  void updateTooltip(void);
  void updatePlantData(void);

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override;

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) override;

  void renderTo (const QString &filename, int width) const;

signals:
  void selected (Plant *me);

private:
  struct PlantMenu;
  static PlantMenu* contextMenu (void);
};

} // end of namespace gui

#endif // VISU_PLANT_H
